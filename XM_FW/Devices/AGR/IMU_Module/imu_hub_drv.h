/**
 ******************************************************************************
 * @file    imu_hub_drv.h
 * @author  HyundoKim
 * @brief   [Device Layer] XM10 ↔ IMU Hub 통신 드라이버 (Master, CANopen 표준)
 * @version 4.0 (PnP V2 리팩토링 - 새 AGR_PnP_Master API)
 * @date    2026-02-10
 *
 * @details
 * [V4.0 변경사항 - PnP V2 리팩토링] (.cursorrules Phase 2)
 * - AS-IS: AGR_PnP_Inst_t (구 API), 0x700 자체 처리
 * - TO-BE: AGR_PnP_Master_t (신 API), 0x700 → PnP Master가 처리
 *
 * [역할 분리]
 * - PnP Master (pnp_task.c): Heartbeat 전송, Boot-up/HB 수신, NMT Timeout
 * - Device Driver (이 파일): TPDO/SDO 처리, Pre-Op State Machine
 *
 * [메시지 처리 흐름 (V4.0)]
 * canfd_rx_handler.c 에서 라우팅:
 *     ├─ 0x700: Boot-up/Heartbeat → AGR_PnP_Master_ProcessMessage() [PnP Master]
 *     ├─ 0x580: SDO Response → ImuHub_Drv_ProcessCANMessage() [Device Driver]
 *     └─ 0x180/0x280: TPDO → ImuHub_Drv_ProcessCANMessage() [Device Driver]
 * 
 * [콜백 구조]
 * PnP Master에서 Boot-up 수신 → on_slave_bootup 콜백 → Pre-Op SM 시작
 * PnP Master에서 Timeout 감지 → on_slave_offline 콜백 → Pre-Op 초기화
 * 
 * [통신 구조]
 * - IMU Hub Node ID: 0x08
 * - TPDO1 (0x188): Group A (Metadata + IMU 0,1,2)
 * - TPDO2 (0x288): Group B (Metadata + IMU 3,4,5)
 * - SDO Request (0x608): TPDO Mapping (0x1A00/0x1A01)
 * - SDO Response (0x588): IMU Connected Mask (0x2000)
 * - Heartbeat (0x708): PnP Master가 처리
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_AGR_IMU_MODULE_IMU_HUB_DRV_H_
#define DEVICES_AGR_IMU_MODULE_IMU_HUB_DRV_H_

#include <stdint.h>
#include <stdbool.h>
#include "agr_dop_types.h"
#include "Transport/CAN_FD/agr_dop_canfd.h"
#include "agr_nmt.h"
#include "agr_pnp_master.h"  /* ✅ V4.0: 새 AGR_PnP_Master API */

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief IMU 센서 개수 */
#define IMUHUB_IMU_COUNT            6

/** @brief Group A IMU 개수 (IMU 0, 1, 2) */
#define IMUHUB_GROUP_A_COUNT        3

/** @brief Group B IMU 개수 (IMU 3, 4, 5) */
#define IMUHUB_GROUP_B_COUNT        3

/**
 *-----------------------------------------------------------
 * SCALING FACTORS (int16 ↔ float 변환, XM10 → User)
 *-----------------------------------------------------------
 * @details IMU Hub에서 받은 int16 데이터를 float으로 복원
 * 
 * [복원 공식]
 * - Quaternion: int16 / 10000.0f → float (-1.0 ~ 1.0)
 * - Euler: int16 / 100.0f → float (-180° ~ 180°)
 * - Accel: int16 / 100.0f → float (±16g)
 * - Gyro: int16 / 10.0f → float (±2000 deg/s)
 * - Mag: int16 / 1.0f → float (±4800 uT)
 */

#define IMUHUB_SCALE_INT16_TO_QUAT      10000.0f
#define IMUHUB_SCALE_INT16_TO_EULER     100.0f
#define IMUHUB_SCALE_INT16_TO_ACC       100.0f
#define IMUHUB_SCALE_INT16_TO_GYRO      10.0f
#define IMUHUB_SCALE_INT16_TO_MAG       1.0f

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief IMU 센서 데이터 구조체 (int16 기반, IMU Hub → XM10)
 * @details 32 bytes per IMU
 */
/**
 * @brief IMU Hub 센서 데이터 (XM10용, PDO 최적화)
 * @details SubIndex 0x60 ({q,a,g} 20B)에 맞춰진 구조체
 */
typedef struct __attribute__((packed)) {
    int16_t  q[4];       /**< 쿼터니언 (w, x, y, z) - 8 bytes */
    int16_t  a[3];       /**< 가속도 (x, y, z) - 6 bytes */
    int16_t  g[3];       /**< 자이로 (x, y, z) - 6 bytes */
} ImuHub_ImuData_t;      /* Total: 20 bytes */

/**
 * @brief IMU Hub 수신 데이터 (최종 처리 완료)
 * @note Timestamp는 24-bit 값이지만 uint32_t에 저장 (상위 8bit = 0)
 */
typedef struct {
	uint32_t         timestamp;               /**< Frame Timestamp (24-bit, 0~16777215 ms, 약 4.6시간 순환) */
	uint8_t          connected_mask;          /**< bit0~5: IMU 0~5 연결 상태 (1=connected, 0=disconnected) */
    ImuHub_ImuData_t imu[IMUHUB_IMU_COUNT];  /**< 6개 IMU 데이터 {q[4], a[3], g[3]} */
} ImuHub_RxData_t;

/* Legacy 콜백 구조체 제거 (V3.0에서 사용 안 함, AGR_PnP가 처리) */

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief IMU Hub 드라이버 초기화 (XM Master)
 *
 * @param tx_func     CAN 전송 함수 (IOIF Layer에서 제공)
 * @param master_pnp  Master PnP 인스턴스 (pnp_task.c에서 제공)
 * @return 0=성공, <0=에러
 *
 * @details
 * [V4.0] 새 AGR_PnP_Master API 적용
 * - AGR_PnP_Master_AddSlave()로 IMU Hub를 Slave로 등록
 * - 콜백: on_slave_bootup → Pre-Op SM 시작
 * - 콜백: on_slave_offline → Pre-Op 초기화
 *
 * @note pnp_task.c의 PnP_Task_Init() 이후에 호출해야 합니다.
 */
int ImuHub_Drv_Init(AGR_TxFunc_t tx_func, AGR_PnP_Master_t* master_pnp);

/**
 * @brief CAN 메시지 처리 (TPDO/SDO만, Heartbeat 제외)
 *
 * @param can_id CAN ID
 * @param data   수신 데이터
 * @param len    데이터 길이
 *
 * @details
 * [V4.0] Heartbeat/Boot-up (0x700) 처리 제거
 * - 0x700 메시지는 canfd_rx_handler.c → AGR_PnP_Master_ProcessMessage()로 라우팅
 * - 이 함수는 SDO Response (0x580)와 TPDO (0x180/0x280)만 처리
 *
 * @note canfd_rx_handler.c의 FDCAN_Rx_Task에서 호출됩니다.
 */
void ImuHub_Drv_ProcessCANMessage(uint16_t can_id, uint8_t* data, uint8_t len);

/**
 * @brief 최신 IMU 데이터 읽기 (Lock-Free Double Buffer)
 * @param rx_data 수신 데이터를 저장할 포인터
 * @return true: 유효한 데이터 있음, false: 데이터 없음
 */
bool ImuHub_Drv_GetRxData(ImuHub_RxData_t* rx_data);

/**
 * @brief 데이터 준비 여부 확인
 * @return true: 최소 1회 이상 TPDO 수신 완료
 */
bool ImuHub_Drv_IsDataReady(void);

/**
 * @brief IMU Hub 연결 상태 확인
 * @return true: 연결됨 (OPERATIONAL + Heartbeat OK)
 */
bool ImuHub_Drv_IsConnected(void);

/**
 * @brief IMU Hub NMT 상태 조회
 * @return NMT 상태
 */
AGR_NMT_State_t ImuHub_Drv_GetNmtState(void);

/**
 * @brief 주기 실행 (Pre-Op SM Timeout/Retry 체크)
 *
 * @details
 * pnp_task.c의 PnP Task에서 100ms 주기로 호출합니다.
 *
 * [처리 항목]
 * 1. Pre-Op SDO Timeout 체크 (5초)
 * 2. SDO Retry (최대 3회)
 * 3. Pre-Op State Machine 구동 (Send 상태 처리)
 *
 * @note Heartbeat/NMT Timeout은 PnP Master가 처리합니다.
 */
void ImuHub_Drv_RunPeriodic(void);

/**
 * @brief IMU Connected Mask 조회
 * @return IMU Connected Mask (bit0: IMU0, ..., bit5: IMU5)
 * @details Pre-Op 단계에서 IMU Hub로부터 SDO로 조회한 값
 */
uint8_t ImuHub_Drv_GetImuConnectedMask(void);

#endif /* DEVICES_AGR_IMU_MODULE_IMU_HUB_DRV_H_ */

