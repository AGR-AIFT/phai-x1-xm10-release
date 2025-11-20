/**
 ******************************************************************************
 * @file    pnp_manager.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Oct 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_LINKS_PNP_MNGR_INC_PNP_MANAGER_H_
#define SYSTEM_LINKS_PNP_MNGR_INC_PNP_MANAGER_H_

#include "link_interface.h" // LinkModule_t를 사용하기 위해 포함

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
 * @brief PnP 매니저를 초기화하고 전체 PnP 관리자 태스크를 생성합니다.
 * @details 시스템 시작 시 startup.c에서 단 한 번만 호출되어야 합니다.
 * 내부적으로 모든 링크 모듈을 등록하고 초기화한 뒤,
 * 이들을 주기적으로 관리하는 메인 PnP 태스크를 시작합니다.
 */
void PnPManager_Init(void);

/**
 * @brief CAN 라우터로부터 수신된 메시지를 적절한 링크 모듈로 분배합니다.
 * @details canfd_rx_router.c의 CAN 라우터 태스크에서 호출됩니다.
 * @param canId 수신된 메시지의 전체 CAN ID.
 * @param data  수신된 데이터 버퍼 포인터.
 * @param len   수신된 데이터의 길이.
 */
void PnPManager_RouteMessage(uint16_t canId, uint8_t* data, uint8_t len);

#endif /* SYSTEM_LINKS_PNP_MNGR_INC_PNP_MANAGER_H_ */
