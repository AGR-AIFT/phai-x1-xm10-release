/**
 ******************************************************************************
 * @file    am_drv.h
 * @author  HyundoKim
 * @brief   Application Module (Jetson Orin) Device Driver — UDP/AGR_DOP
 * @version 1.0
 * @date    Mar 2, 2026
 *
 * @details
 * Application Module (AM, Jetson Orin AGX/NX) 디바이스 드라이버.
 * cm_drv (Control Module, CAN-FD)와 대칭 구조:
 *   - Writer: AGR_DOP UDP 콜백에서 호출 (tcpip_thread context)
 *   - Reader: Main Task 1kHz (core_process.c)에서 호출
 *   - Mutex + Snapshot 패턴으로 Thread-Safe 보장
 *
 * [의존성]
 *   - agr_dop_udp.h: UDP Transport (RX 콜백 등록)
 *   - FreeRTOS: Mutex (xSemaphoreCreateMutex)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AM_DRV_H
#define AM_DRV_H

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * DATA TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief AM → XM 수신 데이터 구조체
 * @note  초기 버전은 raw PDO 저장. 향후 Jetson ↔ XM 프로토콜 확정 시 구조체 확장.
 */
typedef struct {
    uint32_t timestamp;         /**< 마지막 수신 시각 (tick) */
    uint8_t  raw_pdo[64];      /**< Raw PDO payload (최대 64 bytes) */
    uint8_t  raw_len;           /**< Raw PDO 길이 */
} AM_RxData_t;

/**
 *-----------------------------------------------------------
 * INITIALIZATION
 *-----------------------------------------------------------
 */

/**
 * @brief AM 드라이버 초기화
 * @param am_node_id AM(Jetson) Node ID
 * @return 0=성공, <0=에러
 * @note  AGR_UDP_Init() 이전 또는 이후 호출 가능.
 *        AGR_UDP_RegisterPdoCallback(AM_Drv_ProcessPdo) 등록은 main.c에서 수행.
 */
int AM_Drv_Init(uint8_t am_node_id);

/**
 *-----------------------------------------------------------
 * WRITER (AGR_DOP UDP 콜백 — tcpip_thread context)
 *-----------------------------------------------------------
 */

/**
 * @brief PDO 수신 처리 (AGR_UDP_PdoCallback_t 시그니처 호환)
 * @param pdo_type PDO 타입 (TPDO1~4, RPDO1~4)
 * @param src_id   송신자 Node ID
 * @param data     PDO payload
 * @param len      payload 길이
 */
void AM_Drv_ProcessPdo(uint8_t pdo_type, uint8_t src_id,
                        const uint8_t* data, uint8_t len);

/**
 * @brief SDO 수신 처리 (AGR_UDP_SdoCallback_t 시그니처 호환)
 * @param msg_type SDO 타입 (REQ or RSP)
 * @param src_id   송신자 Node ID
 * @param data     SDO payload
 * @param len      payload 길이
 */
void AM_Drv_ProcessSdo(uint8_t msg_type, uint8_t src_id,
                        const uint8_t* data, uint8_t len);

/**
 *-----------------------------------------------------------
 * READER (Main Task 1kHz — core_process.c)
 *-----------------------------------------------------------
 */

/**
 * @brief 최신 수신 데이터 Snapshot 복사
 * @param out 출력 버퍼 (호출자 소유)
 * @return true=성공, false=Mutex timeout (이전 값 사용 권장)
 */
bool AM_Drv_GetRxData(AM_RxData_t* out);

/**
 * @brief AM 연결 상태 확인 (Heartbeat/Timeout 기반)
 * @return true=연결됨 (3초 이내 수신 있음), false=연결 끊김
 */
bool AM_Drv_IsConnected(void);

#endif /* AM_DRV_H */
