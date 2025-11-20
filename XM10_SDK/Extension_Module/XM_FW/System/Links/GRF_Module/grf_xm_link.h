/**
 ******************************************************************************
 * @file    grf_xm_link.h
 * @author  HyundoKim
 * @brief   [System/Links] GRF(FSR) 센서 데이터 링크 (Lock-Free Double Buffering)
 * @details 
 * UART Rx Task(Writer)와 User Task(Reader) 간의 데이터 공유를 담당합니다.
 * Atomic 연산을 이용한 이중 버퍼링을 적용하여, 
 * 인터럽트를 끄지 않고도(Zero Latency) 데이터 무결성을 보장합니다.
 * @version 0.1
 * @date    Nov 6, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_LINKS_GRF_MODULE_GRF_XM_LINK_H_
#define SYSTEM_LINKS_GRF_MODULE_GRF_XM_LINK_H_

#include "link_interface.h"
#include "mdaf-25-6850.h" // MarvelDex_packet_t 타입을 직접 사용

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define SENSOR_SPACE_LEFT       (1)
#define SENSOR_SPACE_RIGHT      (2)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief GRF-XM 링크 모듈의 LinkModule_t 인터페이스를 반환합니다.
 * @details PnP 매니저가 이 모듈을 등록하고 제어할 수 있도록 합니다.
 * @return 이 모듈의 LinkModule_t 구조체 포인터.
 */
LinkModule_t* GRF_XM_Link_GetModule(void);

/**
 * @brief (Facade API용) 지정된 GRF 센서의 가상 PnP 연결 상태를 반환합니다.
 * @param[in] sensorSpace 1=왼발, 2=오른발
 * @return 데이터 스트림이 활성화(OPERATIONAL) 상태이면 true.
 */
bool GRF_XM_Link_IsConnected(uint8_t sensorSpace);

/**
 * @brief [Reader] 최신 데이터 스냅샷 가져오기
 * @note  User Task(Core Process)에서 호출됩니다.
 * 현재 유효한 버퍼 인덱스를 원자적으로 읽어 데이터를 복사합니다.
 * @param out_L 왼쪽 발 데이터 저장 포인터 (NULL 가능)
 * @param out_R 오른쪽 발 데이터 저장 포인터 (NULL 가능)
 * @return true (항상 성공)
 */
bool GRF_XM_Link_GetLatest(MarvelDex_packet_t* out_L, MarvelDex_packet_t* out_R);

/**
 * @brief [Writer] 수신된 패킷으로 내부 캐시 업데이트
 * @note  UartRxTask(1kHz)에서 호출됩니다. 
 * Lock-Free 방식이므로 FDCAN/USB 인터럽트를 지연시키지 않습니다.
 * @param packet 수신된 파싱 패킷 포인터
 */
void GRF_XM_Link_UpdateData(const MarvelDex_packet_t* packet);

#endif /* SYSTEM_LINKS_GRF_MODULE_GRF_XM_LINK_H_ */
