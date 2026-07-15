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
 * - EMG Hub Node ID: 0x0F (AGR_NODE_ID_EMG_HUB)
 * - TPDO1 (0x18F): Metadata(4B) + EMG Data(10B) = 14B
 * - SDO Request (0x60F): TPDO Mapping (0x2110)
 * - SDO Response (0x58F): 설정 확인
 * - Heartbeat (0x70F): PnP Master가 처리
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
 */
#define EMGHUB_SCALE_UV_X10            10.0f   /**< µV × 10 → µV */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief EMG Hub PDO 수신 데이터 (TPDO v2.1, 최종 처리 완료, int16 기반)
 * @details TPDO v2.1 wire = ctrl_tick_ms(4B) + status(1B) + EMG Processed Set 0x6010:0x60(10B) = 15B.
 * @note ctrl_tick_ms 는 Slave 제어 루프 32-bit ms tick (wrap 49.7 일) — 송신 1kHz 시점 ground
 *       truth, 연속 수신 delta!=1 → gap. (구 v1 의 24-bit timestamp 는 제거됨.)
 *       MDF/MNF(mean/median_freq) 는 EMG/XM 에서 산출 안 함 — raw_adc 로깅 후 PC/MATLAB 오프라인 FFT.
 */
typedef struct __attribute__((packed)) {
    /* Slave 제어 틱 — OD 0x6050 (4B, LE) — TPDO v2.1 선두 */
    uint32_t ctrl_tick_ms;        /**< Slave Control Task ms tick (송신 1kHz 시점). delta!=1 → gap */

    /* Status — OD 0x3000 (1B) */
    uint8_t  status_flags;        /**< bit0 ADC_OK / bit1 IS_ACTIVE / bit2 SATURATED / bit3 CALIB_VALID */

    /* EMG Processed Set — OD 0x6010:0x60 (10B, Little-Endian) */
    uint16_t raw_adc;             /**< ADC 원시값 (12-bit, HW OVS 16×) */
    int16_t  voltage_uv_x10;     /**< EMG 전압 (µV × 10) */
    int16_t  rms_uv_x10;         /**< RMS 값 (µV × 10) */
    int16_t  envelope_uv_x10;    /**< Envelope 값 (µV × 10) — 제어 1차 입력 */
    uint8_t  mvc_percent;         /**< MVC 정규화 (0~100+%, calib_valid=1 시 유효) */
    uint8_t  is_active;           /**< 근수축 감지 (0/1) */
} EmgHub_RxData_t;

/** @brief EMG Hub Status Flag Bits (EMG Hub xm_drv.h와 동일) */
#define EMGHUB_STATUS_ADC_OK       (1 << 0)
#define EMGHUB_STATUS_IS_ACTIVE    (1 << 1)
#define EMGHUB_STATUS_SATURATED    (1 << 2)
#define EMGHUB_STATUS_CALIB_VALID  (1 << 3)   /**< MVC 보정 완료 — mvc_percent 신뢰 가능. 0=envelope_uv fallback */

/** @brief EMG 캘리브 명령값 (OD 0x2103, EMG_CMD_* 미러) */
#define EMGHUB_CAL_CMD_OFFSET      2   /**< offset 캘리브 시작 (EMG 가 4000샘플 누적 후 자동 완료) */
#define EMGHUB_CAL_CMD_MVC         3   /**< 현재 RMS 를 MVC 기준으로 즉시 캡처 */

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
 * @brief EMG 캘리브레이션 명령 전송 (XM master → EMG SDO Write 0x2103)
 * @param cmd EMGHUB_CAL_CMD_OFFSET(2) / EMGHUB_CAL_CMD_MVC(3)
 * @return 0=성공, <0=SDO Write 실패
 * @note OPERATIONAL/Pre-Op 무관, EMG 가 SDO 수신 가능한 상태면 동작. offset 은
 *       EMG 가 ~2초 누적 후 자동 완료, mvc 는 즉시 현재 RMS 캡처.
 */
int EmgHub_Drv_SendCalCommand(uint8_t cmd);

/**
 * @brief EMG MVC 기준값 직접 주입 (XM master → EMG SDO Write 0x2104, float32 µV)
 * @param mvc_uv host/XM 가 산출한 MVC 기준 RMS (µV, >0)
 * @return 0=성공, <0=SDO Write 실패
 */
int EmgHub_Drv_SetMvcValue(float mvc_uv);

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
 * @brief IDENTIFY 불일치 상태 여부 (NMT START 영구 보류 중 — fail loud)
 * @return true: IDENTITY_MISMATCH (잘못된 디바이스/계약버전 연결)
 * @details pnp_task.c 가 CH_LED_EMG 를 CH_DEV_WRONG_DEVICE 로 표시하는 데 사용 (FES 정합).
 */
bool EmgHub_Drv_IsIdentityMismatch(void);

/**
 * @brief 주기 실행 (Pre-Op SM Timeout/Retry 체크)
 * @details pnp_task.c의 PnP Task에서 100ms 주기로 호출합니다.
 */
void EmgHub_Drv_RunPeriodic(void);

#endif /* DEVICES_AGR_EMG_MODULE_EMG_HUB_DRV_H_ */
