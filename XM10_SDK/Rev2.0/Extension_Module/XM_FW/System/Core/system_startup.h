/**
 ******************************************************************************
 * @file    system_startup.h
 * @author  HyundoKim
 * @brief   시스템 초기화 및 부팅 시퀀스 관리
 * @version 0.1
 * @date    Oct 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_CORE_INC_SYSTEM_STARTUP_H_
#define SYSTEM_CORE_INC_SYSTEM_STARTUP_H_

#include "ioif_agrb_fdcan.h"
#include "ioif_agrb_gpio.h"

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
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */



/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief XM10 시스템의 모든 기반 서비스를 초기화하고 시작합니다.
 * @details StartupTask에 의해 RTOS 스케줄러가 시작된 후 단 한 번만 호출됩니다.
 * 이 함수는 IOIF 계층을 '배선(Wiring)'하고 시스템 서비스(PnP, CAN 핸들러)를 생성합니다.
 */
void System_Startup(void);

/**
 * @brief 초기화된 FDCAN1의 IOIF 핸들(ID)을 반환합니다.
 * @details 다른 시스템 모듈(예: canfd_rx_handler)이 IOIF 드라이버에 접근하기 위해 사용합니다.
 * @return FDCAN1의 IOIF_FDCANx_t 핸들.
 */
IOIF_FDCANx_t System_GetFDCAN1_Id(void);

/**
 * @brief 초기화된 FDCAN2의 IOIF 핸들(ID)을 반환합니다.
 * @details Rev2.0 Ch2 (XM↔Sensor Module, DOP V2) 전용.
 * @return FDCAN2의 IOIF_FDCANx_t 핸들.
 */
IOIF_FDCANx_t System_GetFDCAN2_Id(void);

/**
 * @brief FDCAN1 채널(Ch1)을 통해 CAN 메시지를 전송하는 래퍼 함수.
 * @note  CM(DOP V1) 전용 채널. AGR_TxFunc_t / CM_TxFunc_t 타입과 호환.
 * @param[in] can_id CAN ID (11-bit 또는 29-bit).
 * @param[in] data  전송할 데이터의 포인터 (const).
 * @param[in] len   전송할 데이터의 길이 (0~64 bytes).
 * @return 0 on success, <0 on error.
 */
int System_Fdcan1_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief FDCAN2 채널(Ch2)을 통해 CAN 메시지를 전송하는 래퍼 함수.
 * @note  Sensor Module(DOP V2) 전용 채널. AGR_TxFunc_t 타입과 호환.
 * @param[in] can_id CAN ID (11-bit 또는 29-bit).
 * @param[in] data  전송할 데이터의 포인터 (const).
 * @param[in] len   전송할 데이터의 길이 (0~64 bytes).
 * @return 0 on success, <0 on error.
 */
int System_Fdcan2_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief CiA 301 SYNC를 FDCAN1(Ch1, XM↔CM)으로 전송합니다.
 * @details 1-byte rolling counter payload. TIM6 ISR(NVIC 4, 1kHz)에서 호출.
 *          ISR-Safe: IOIF Tx Mutex 우회, HAL 직접 호출.
 *          PnP Operational 진입 후에만 호출할 것 (CM_Drv_IsConnected() 체크).
 */
void System_SendSync_Ch1(void);

/**
 * @brief EXT_PWR_SEL_5V(PE3)의 IOIF GPIO 핸들을 반환합니다.
 * @return IOIF_GPIOx_t 핸들.
 */
IOIF_GPIOx_t System_GetExtPwrSelGpioId(void);

/**
 * @brief [RTOS 태스크] "강한(strong)" 정의의 StartupTask 구현부.
 * @details main.c에서 생성된 __weak StartStartupTask를 덮어씁니다.
 * 시스템 초기화를 총괄하고, 완료되면 다른 태스크를 깨운 뒤 자신을 삭제합니다.
 */
void StartStartupTask(void *argument);

#endif /* SYSTEM_CORE_INC_SYSTEM_STARTUP_H_ */
