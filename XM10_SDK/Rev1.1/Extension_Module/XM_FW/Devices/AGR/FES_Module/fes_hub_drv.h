/**
 ******************************************************************************
 * @file    fes_hub_drv.h
 * @author  HyundoKim
 * @brief   [Device Layer] XM10 <-> FES Hub 통신 드라이버 (Master, DOP V3 ES-vector)
 * @version 2.0
 * @date    2026-04-09
 *
 * @details
 * XM10(Master)이 FES Hub(Slave, Node 0x0C)와 DOP/PnP로 통신하기 위한 드라이버.
 * IMU Hub 드라이버 패턴을 기반으로, FES ES-vector 모델에 맞게 리팩토링.
 *
 * [V2.0 변경 — ES-vector 모델 (2026-04-09)]
 * - 기존 14B RPDO 모델 (FesHub_Cmd_t / RpdoPayload_t / SendRPDO1) 완전 폐기
 * - ES-vector 6B: 비주기 SDO Download (0x6300, atomic 단일 프레임)
 * - Master Command 1B: SDO Download (0x6310, 가상 ISI EXT 트리거)
 * - TPDO1 (0x18C, 37B): FES Feedback (Legacy 16B + KHJ 21B: FSM/ISI/target/impedance)
 * - Pre-Op: TPDO1 Mapping + NMT START (RPDO mapping step 제거)
 *
 * [통신 구조]
 * - FES Hub Node ID: 0x0C (AGR_NODE_ID_FES_HUB, Node ID v3.0)
 * - TPDO1 (0x18C): FES Feedback (37B = Legacy 16B + KHJ 21B, Slave -> Master, 10ms)
 * - SDO Request  (0x60C): ES-vector Write + Master Cmd + OD 설정
 * - SDO Response (0x58C): OD 기반 응답
 * - Heartbeat (0x70C): PnP Master가 처리
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_AGR_FES_MODULE_FES_HUB_DRV_H_
#define DEVICES_AGR_FES_MODULE_FES_HUB_DRV_H_

#include <stdint.h>
#include <stdbool.h>
#include "agr_dop_types.h"
#include "Transport/CAN_FD/agr_dop_canfd.h"
#include "agr_nmt.h"
#include "agr_pnp_master.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief FES 채널 수 */
#define FESHUB_CH_COUNT             2

/**
 *-----------------------------------------------------------
 * SCALING — int16 <-> float 변환
 *-----------------------------------------------------------
 */
#define FESHUB_SCALE_AMPLITUDE      10.0f   /**< x10: mA, 0.1mA precision */
#define FESHUB_SCALE_FREQUENCY      10.0f   /**< x10: Hz, 0.1Hz precision */
#define FESHUB_SCALE_VOLTAGE        100.0f  /**< x100: V, 0.01V precision */
#define FESHUB_SCALE_IMPEDANCE      10.0f   /**< x10: ohm, 0.1ohm precision */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief FES 채널 상태 (FES Hub -> XM, TPDO)
 */
typedef enum {
    FESHUB_STATE_IDLE        = 0,
    FESHUB_STATE_READY       = 1,
    FESHUB_STATE_STIMULATING = 2,
    FESHUB_STATE_FAULT       = 3,
} FesHub_ChState_t;

/**
 * @brief ES-vector 페이로드 (XM -> FES Hub, PPT 충실)
 * @details 6 bytes, SDO Download to 0x6300:00 (atomic 단일 CAN-FD 프레임)
 */
typedef struct __attribute__((packed)) {
    uint8_t  ch_select;      /**< [1,2,3] 1=CH1, 2=CH2, 3=Both */
    uint8_t  amplitude_mA;   /**< [0..80] 전류 진폭 (mA) */
    uint8_t  duty_x1000;     /**< [0..100] 듀티비 x0.001 */
    uint8_t  frequency_Hz;   /**< [10..100] 펄스 주파수 (Hz) */
    uint16_t burst_ms;       /**< LE, [0..60000, 65535=infinite] */
} FesHub_ESVector_t;  /* 6 bytes */

/**
 * @brief Master Command (XM -> FES Hub, 가상 ISI EXT 트리거)
 * @details SDO Download to 0x6310:00 (UINT8, 1-shot)
 */
typedef enum {
    FESHUB_MCMD_NOOP         = 0,  /**< No operation */
    FESHUB_MCMD_TOGGLE_CH1   = 1,  /**< CH1 토글 (가상 ISI EXT7) */
    FESHUB_MCMD_TOGGLE_CH2   = 2,  /**< CH2 토글 (가상 ISI EXT8) */
    FESHUB_MCMD_TOGGLE_BOTH  = 3,  /**< 양채널 (EXT7 + EXT8) */
} FesHub_MasterCmd_t;

/**
 * @brief FES Hub 수신 데이터 (TPDO1 디코딩 결과)
 * @details float 변환 완료된 최종 데이터
 */
typedef struct {
    /* 채널 상태 */
    FesHub_ChState_t ch_state[FESHUB_CH_COUNT];

    /* 전류 피드백 (mA) */
    float ch_current_mA[FESHUB_CH_COUNT];

    /* Fault 코드 */
    uint8_t ch_fault_code[FESHUB_CH_COUNT];

    /* HV 전압 (V) */
    float hv_voltage_V;

    /* Digipot 위치 (0~127) */
    uint8_t digipot_pos;

    /* ES-vector state: [3:0]=CH0, [7:4]=CH1 (ESState_t) */
    uint8_t es_state_packed;

    /* Timestamp (24-bit ms) */
    uint32_t timestamp;

    /* Error Register */
    uint8_t error_register;

    /* KHJ Current Control + ES-vector telemetry */
    uint8_t  fsm_state;                              /**< FSM state_curr */
    uint8_t  fsm_state_prev;                         /**< FSM state_prev */
    uint8_t  isi_packed;                             /**< bit[N] = ISI[N] */
    uint8_t  ch_es_error_lo[FESHUB_CH_COUNT];        /**< ES-vector error_code low byte */
    float    ch_target_amplitude_mA[FESHUB_CH_COUNT]; /**< target amplitude (mA) */
    float    ch_impedance[FESHUB_CH_COUNT];           /**< filtered impedance */
    uint16_t ch_pulse_cnt[FESHUB_CH_COUNT];          /**< burst pulse counter */
    float    ch_voltage_diff_V[FESHUB_CH_COUNT];     /**< differential voltage (V) */
} FesHub_RxData_t;

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief FES Hub 드라이버 초기화 (XM Master)
 * @param tx_func     CAN 전송 함수
 * @param master_pnp  Master PnP 인스턴스
 * @return 0=성공, <0=에러
 */
int FesHub_Drv_Init(AGR_TxFunc_t tx_func, AGR_PnP_Master_t* master_pnp);

/**
 * @brief CAN 메시지 처리 (TPDO/SDO만, Heartbeat 제외)
 * @param can_id CAN ID
 * @param data   수신 데이터
 * @param len    데이터 길이
 */
void FesHub_Drv_ProcessCANMessage(uint16_t can_id, uint8_t* data, uint8_t len);

/**
 * @brief 최신 FES Hub 데이터 읽기 (Mutex + Snapshot)
 * @param[out] rx_data 수신 데이터
 * @return true=유효 데이터 있음
 */
bool FesHub_Drv_GetRxData(FesHub_RxData_t* rx_data);

/**
 * @brief 데이터 준비 여부 확인
 */
bool FesHub_Drv_IsDataReady(void);

/**
 * @brief FES Hub 연결 상태 확인
 */
bool FesHub_Drv_IsConnected(void);

/**
 * @brief FES Hub NMT 상태 조회
 */
AGR_NMT_State_t FesHub_Drv_GetNmtState(void);

/**
 * @brief ES-vector 전송 (XM -> FES Hub, SDO 0x6300:00)
 * @param esv  ES-vector 페이로드 (6B)
 * @return 0=성공, <0=에러
 * @note CAN-FD 단일 프레임 atomic 전송 (AGR_CANFD_SendSDOWrite)
 */
int FesHub_Drv_SendESVector(const FesHub_ESVector_t* esv);

/**
 * @brief Master Command 전송 (XM -> FES Hub, SDO 0x6310:00)
 * @param cmd  Master Command (1B)
 * @return 0=성공, <0=에러
 */
int FesHub_Drv_SendMasterCommand(FesHub_MasterCmd_t cmd);

/**
 * @brief 주기 실행 (Pre-Op SM + Timeout/Retry)
 */
void FesHub_Drv_RunPeriodic(void);

#endif /* DEVICES_AGR_FES_MODULE_FES_HUB_DRV_H_ */
