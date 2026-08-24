/**
 ******************************************************************************
 * @file    emg_hub_drv.h
 * @author  HyundoKim
 * @brief   [Device Layer] XM10 ↔ EMG Hub 통신 드라이버 (Master, CANopen 표준)
 * @version 1.0
 * @date    2026-03-20
 *
 * @details
 * [역할 분리]
 * - PnP Master (pnp_task.c): Heartbeat 전송, Boot-up/HB 수신, NMT Timeout
 * - Device Driver (이 파일): TPDO/SDO 처리, Pre-Op State Machine
 *
 * [메시지 처리 흐름]
 * canfd_rx_handler.c 에서 라우팅:
 *     ├─ 0x700: Boot-up/Heartbeat → AGR_PnP_Master_ProcessMessage() [PnP Master]
 *     ├─ 0x580: SDO Response → EmgHub_Drv_ProcessCANMessage() [Device Driver]
 *     └─ 0x180: TPDO1 → EmgHub_Drv_ProcessCANMessage() [Device Driver]
 *
 * [통신 구조]
 * - EMG Hub Node ID: 0x0A
 * - TPDO1 (0x18A): Metadata(4B) + EMG Data(10B) = 14B
 * - SDO Request (0x60A): TPDO Mapping (0x2110)
 * - SDO Response (0x58A): 설정 확인
 * - Heartbeat (0x70A): PnP Master가 처리
 *
 * [PDO 데이터 구조 (EMG Hub xm_drv.h 기준)]
 * Metadata (4B): timestamp_low, timestamp_mid, timestamp_high, status_flags
 * EMG Data SubIndex 0x60 (10B):
 *   raw_adc(2B), voltage_uv_x10(2B), rms_uv_x10(2B),
 *   envelope_uv_x10(2B), mvc_percent(1B), is_active(1B)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_AGR_EMG_MODULE_EMG_HUB_DRV_H_
#define DEVICES_AGR_EMG_MODULE_EMG_HUB_DRV_H_

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

/**
 * @brief EMG 데이터 스케일링 (int16 ↔ float 변환, XM10 → User)
 * @details EMG Hub에서 받은 int16 데이터를 float으로 복원
 *
 * [복원 공식]
 * - Voltage/RMS/Envelope: int16 / 10.0f → float (µV)
 * - MVC Percent: uint8 직접 (0~100%)
 * - Frequency: uint16 / 10.0f → float (Hz)
 */
#define EMGHUB_SCALE_UV_X10            10.0f   /**< µV × 10 → µV */
#define EMGHUB_SCALE_FREQ_X10          10.0f   /**< Hz × 10 → Hz */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief EMG Hub PDO 수신 데이터 (최종 처리 완료, int16 기반)
 * @details TPDO1 Metadata + EMG Data SubIndex 0x60 해석 결과
 * @note Timestamp는 24-bit 값이지만 uint32_t에 저장 (상위 8bit = 0)
 */
typedef struct __attribute__((packed)) {
    /* PDO Metadata (4B) */
    uint32_t timestamp;           /**< Frame Timestamp (24-bit, ms) */
    uint8_t  status_flags;        /**< Status Flags (bit0: ADC_OK, bit1: IS_ACTIVE, bit2: SATURATED) */

    /* EMG Sensor Data — SubIndex 0x60 (10B) */
    uint16_t raw_adc;             /**< ADC 원시값 (12-bit, HW OVS 16×) */
    int16_t  voltage_uv_x10;     /**< EMG 전압 (µV × 10) */
    int16_t  rms_uv_x10;         /**< RMS 값 (µV × 10) */
    int16_t  envelope_uv_x10;    /**< Envelope 값 (µV × 10) */
    uint8_t  mvc_percent;         /**< MVC 정규화 (0~100%) */
    uint8_t  is_active;           /**< 근수축 감지 (0/1) */
} EmgHub_RxData_t;

/** @brief EMG Hub Status Flag Bits (EMG Hub xm_drv.h와 동일) */
#define EMGHUB_STATUS_ADC_OK       (1 << 0)
#define EMGHUB_STATUS_IS_ACTIVE    (1 << 1)
#define EMGHUB_STATUS_SATURATED    (1 << 2)

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief EMG Hub 드라이버 초기화 (XM Master)
 *
 * @param tx_func     CAN 전송 함수 (IOIF Layer에서 제공)
 * @param master_pnp  Master PnP 인스턴스 (pnp_task.c에서 제공)
 * @return 0=성공, <0=에러
 *
 * @note pnp_task.c의 PnP_Task_Init() 이후에 호출해야 합니다.
 */
int EmgHub_Drv_Init(AGR_TxFunc_t tx_func, AGR_PnP_Master_t* master_pnp);

/**
 * @brief CAN 메시지 처리 (TPDO/SDO만, Heartbeat 제외)
 *
 * @param can_id CAN ID
 * @param data   수신 데이터
 * @param len    데이터 길이
 *
 * @note canfd_rx_handler.c의 FDCAN_Rx_Task에서 호출됩니다.
 */
void EmgHub_Drv_ProcessCANMessage(uint16_t can_id, uint8_t* data, uint8_t len);

/**
 * @brief 최신 EMG 데이터 읽기 (Mutex + Snapshot)
 * @param rx_data 수신 데이터를 저장할 포인터
 * @return true: 유효한 데이터 있음, false: 데이터 없음
 */
bool EmgHub_Drv_GetRxData(EmgHub_RxData_t* rx_data);

/**
 * @brief 데이터 준비 여부 확인
 * @return true: 최소 1회 이상 TPDO 수신 완료
 */
bool EmgHub_Drv_IsDataReady(void);

/**
 * @brief EMG Hub 연결 상태 확인
 * @return true: 연결됨 (OPERATIONAL + Heartbeat OK)
 */
bool EmgHub_Drv_IsConnected(void);

/**
 * @brief EMG Hub NMT 상태 조회
 * @return NMT 상태
 */
AGR_NMT_State_t EmgHub_Drv_GetNmtState(void);

/**
 * @brief 주기 실행 (Pre-Op SM Timeout/Retry 체크)
 * @details pnp_task.c의 PnP Task에서 100ms 주기로 호출합니다.
 */
void EmgHub_Drv_RunPeriodic(void);

#endif /* DEVICES_AGR_EMG_MODULE_EMG_HUB_DRV_H_ */
