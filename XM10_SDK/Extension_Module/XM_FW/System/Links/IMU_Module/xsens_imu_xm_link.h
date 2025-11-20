/**
 ******************************************************************************
 * @file    xsens_imu_xm_link.h
 * @author  HyundoKim
 * @brief   [System/Links] Xsens IMU 모듈과의 '가상 PnP' 링크 관리
 * @details uart_rx_handler로부터 IMU 패킷을 받아 타임아웃을 감시하고,
 * Facade Layer(xm_api)에 최신 데이터를 제공합니다.
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_LINKS_IMU_MODULE_XSENS_IMU_XM_LINK_H_
#define SYSTEM_LINKS_IMU_MODULE_XSENS_IMU_XM_LINK_H_

#include "link_interface.h"
#include "mti-630.h"  // XsensMTi_packet_t 타입을 사용

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


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
 * @brief Xsens-XM 링크 모듈의 LinkModule_t 인터페이스를 반환합니다.
 * @details PnP 매니저가 이 모듈을 등록하고 제어할 수 있도록 합니다.
 * @return 이 모듈의 LinkModule_t 구조체 포인터.
 */
LinkModule_t* XsensIMU_XM_Link_GetModule(void);

/**
 * @brief (Facade API용) IMU 센서의 가상 PnP 연결 상태를 반환합니다.
 * @return 데이터 스트림이 활성화(OPERATIONAL) 상태이면 true.
 */
bool XsensIMU_XM_Link_IsConnected(void);

/**
 * @brief [Writer] 수신된 패킷으로 내부 버퍼 업데이트 (From Rx Task)
 * @note  Lock-Free 방식: 인터럽트를 끄지 않고 쓰기 버퍼에 기록 후 인덱스만 교체
 */
void XsensIMU_XM_Link_UpdateData(const XsensMTi_packet_t* packet);

/**
 * @brief [Reader] 최신 데이터 사본 가져오기 (From Core Process)
 * @note  Lock-Free 방식: 현재 유효한 인덱스의 버퍼를 읽음
 * @param out_packet 데이터를 복사받을 구조체 포인터
 * @return true 성공 (항상 성공)
 */
bool XsensIMU_XM_Link_GetLatest(XsensMTi_packet_t* out_packet);

#endif /* SYSTEM_LINKS_IMU_MODULE_XSENS_IMU_XM_LINK_H_ */
