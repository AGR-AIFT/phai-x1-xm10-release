/**
 ******************************************************************************
 * @file    fes_hub_drv.h
 * @author  HyundoKim
 * @brief   [Device Layer] XM10 ↔ FES Hub 통신 드라이버 (Master, CANopen 표준)
 * @version 1.0
 * @date    2026-03-21
 *
 * @details
 * XM10(Master)이 FES Hub(Slave, Node 0x0A)와 DOP/PnP로 통신하기 위한 드라이버.
 * IMU Hub 드라이버 패턴을 기반으로, FES 모듈에 맞게 단순화.
 *
 * [통신 구조]
 * - FES Hub Node ID: 0x0A
 * - RPDO1 (0x20A): FES Command  (2ch × 7B = 14B, Master → Slave)
 * - TPDO1 (0x18A): FES Feedback (16B, Slave → Master)
 * - SDO Request  (0x60A): OD 읽기/쓰기 (PDO Mapping 설정)
 * - SDO Response (0x58A): OD 기반 응답
 * - Heartbeat (0x70A): PnP Master가 처리
 *
 * [메시지 처리 흐름]
 * canfd_rx_handler.c 에서 라우팅:
 *     ├─ 0x700: Boot-up/Heartbeat → AGR_PnP_Master_ProcessMessage() [PnP Master]
 *     ├─ 0x580: SDO Response → FesHub_Drv_ProcessCANMessage() [Device Driver]
 *     └─ 0x180: TPDO1 → FesHub_Drv_ProcessCANMessage() [Device Driver]
 *
 * [RPDO1 전송]
 * Application Task에서 FesHub_Drv_SendRPDO1() 호출 → FES Hub로 명령 전송
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
 * SCALING — int16 ↔ float 변환
 *-----------------------------------------------------------
 * - Amplitude: int16 / 10.0f → float mA (0.1mA 정밀도)
 * - Frequency: uint16 / 10.0f → float Hz (0.1Hz 정밀도)
 * - Voltage:   uint16 / 100.0f → float V (0.01V 정밀도)
 */
#define FESHUB_SCALE_AMPLITUDE      10.0f
#define FESHUB_SCALE_FREQUENCY      10.0f
#define FESHUB_SCALE_VOLTAGE        100.0f

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief FES 채널 명령 코드 (XM → FES Hub, RPDO)
 */
typedef enum {
    FESHUB_CMD_NOP         = 0x00,  /**< No operation */
    FESHUB_CMD_SET_PARAM   = 0x01,  /**< 파라미터 설정 (IDLE → READY) */
    FESHUB_CMD_START       = 0x02,  /**< 자극 시작 (READY → STIMULATING) */
    FESHUB_CMD_STOP        = 0x03,  /**< 자극 정지 (→ RAMP_DOWN → IDLE) */
    FESHUB_CMD_RESET_FAULT = 0x04,  /**< Fault 리셋 (FAULT → IDLE) */
} FesHub_Cmd_t;

/**
 * @brief FES 채널 상태 (FES Hub → XM, TPDO)
 */
typedef enum {
    FESHUB_STATE_IDLE        = 0,
    FESHUB_STATE_READY       = 1,
    FESHUB_STATE_STIMULATING = 2,
    FESHUB_STATE_FAULT       = 3,
} FesHub_ChState_t;

/**
 * @brief FES 채널별 명령 구조체 (RPDO1 서브 필드, 7B)
 */
typedef struct __attribute__((packed)) {
    uint8_t  command;            /**< FesHub_Cmd_t */
    int16_t  amplitude_x10;     /**< mA × 10 (0.1mA 정밀도) */
    uint16_t frequency_x10;     /**< Hz × 10 (0.1Hz 정밀도) */
    uint16_t pulse_width_us;    /**< 펄스 폭 (μs) */
} FesHub_ChCmd_t;  /* 7 bytes */

/**
 * @brief RPDO1 전송 페이로드 (XM → FES Hub, 14B)
 */
typedef struct __attribute__((packed)) {
    FesHub_ChCmd_t ch[FESHUB_CH_COUNT];  /**< 2ch × 7B = 14B */
} FesHub_RpdoPayload_t;  /* 14 bytes */

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

    /* Timestamp (24-bit ms) */
    uint32_t timestamp;

    /* Error Register */
    uint8_t error_register;
} FesHub_RxData_t;

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief FES Hub 드라이버 초기화 (XM Master)
 * @param tx_func     CAN 전송 함수
 * @param master_pnp  Master PnP 인스턴스 (pnp_task.c에서 제공)
 * @return 0=성공, <0=에러
 */
int FesHub_Drv_Init(AGR_TxFunc_t tx_func, AGR_PnP_Master_t* master_pnp);

/**
 * @brief CAN 메시지 처리 (TPDO/SDO만, Heartbeat 제외)
 * @param can_id CAN ID
 * @param data   수신 데이터
 * @param len    데이터 길이
 * @note canfd_rx_handler.c에서 호출 (0x58A, 0x18A)
 */
void FesHub_Drv_ProcessCANMessage(uint16_t can_id, uint8_t* data, uint8_t len);

/**
 * @brief 최신 FES Hub 데이터 읽기 (Lock-Free Double Buffer)
 * @param[out] rx_data 수신 데이터
 * @return true=유효 데이터 있음
 */
bool FesHub_Drv_GetRxData(FesHub_RxData_t* rx_data);

/**
 * @brief 데이터 준비 여부 확인
 * @return true=최소 1회 TPDO 수신 완료
 */
bool FesHub_Drv_IsDataReady(void);

/**
 * @brief FES Hub 연결 상태 확인
 * @return true=연결됨 (OPERATIONAL + Heartbeat OK)
 */
bool FesHub_Drv_IsConnected(void);

/**
 * @brief FES Hub NMT 상태 조회
 */
AGR_NMT_State_t FesHub_Drv_GetNmtState(void);

/**
 * @brief RPDO1 전송 (FES 명령, XM → FES Hub)
 * @param payload RPDO1 페이로드 (14B, 2ch 명령)
 * @return 0=성공, <0=에러
 */
int FesHub_Drv_SendRPDO1(const FesHub_RpdoPayload_t* payload);

/**
 * @brief 주기 실행 (Pre-Op SM + Timeout/Retry)
 * @note pnp_task.c에서 100ms 주기로 호출
 */
void FesHub_Drv_RunPeriodic(void);

#endif /* DEVICES_AGR_FES_MODULE_FES_HUB_DRV_H_ */
