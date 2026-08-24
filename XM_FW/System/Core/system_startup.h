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
 * @details CM Bus (DOP V1) 접근용.
 * @return FDCAN1의 IOIF_FDCANx_t 핸들.
 */
IOIF_FDCANx_t System_GetFDCAN1_Id(void);

/**
 * @brief 초기화된 FDCAN2의 IOIF 핸들(ID)을 반환합니다.
 * @details Sensor Hub Bus (DOP V2) 접근용.
 * @return FDCAN2의 IOIF_FDCANx_t 핸들.
 */
IOIF_FDCANx_t System_GetFDCAN2_Id(void);

/**
 * @brief FDCAN1 채널을 통해 CAN 메시지를 전송합니다 (CM Bus, DOP V1).
 * @param[in] can_id CAN ID (11-bit 또는 29-bit).
 * @param[in] data  전송할 데이터의 포인터 (const).
 * @param[in] len   전송할 데이터의 길이 (0~64 bytes).
 * @return 0 on success, <0 on error.
 */
int System_Fdcan1_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief FDCAN2 채널을 통해 CAN 메시지를 전송합니다 (Sensor Hub Bus, DOP V2).
 * @param[in] can_id CAN ID (11-bit 또는 29-bit).
 * @param[in] data  전송할 데이터의 포인터 (const).
 * @param[in] len   전송할 데이터의 길이 (0~64 bytes).
 * @return 0 on success, <0 on error.
 */
int System_Fdcan2_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief CiA 301 SYNC 메시지를 FDCAN1(Ch1: XM↔CM)으로 전송합니다.
 * @details UserTask에서 _FetchAllInputs() 직후 호출.
 *          Read↔SYNC가 같은 태스크에서 실행되어 phase drift 제거.
 * @note HAL 직접 호출 (단일 Tx 경로, Mutex 불필요)
 */
void System_SendSync_Ch1(void);

/**
 * @brief Returns the IOIF GPIO handle for EXT_PWR_EN(PE4).
 * @return Initialized IOIF GPIO handle.
 */
IOIF_GPIOx_t System_GetExtPwrEnGpioId(void);

/**
 * @brief [신규] Extension Port(PA0, PA1)를 ADC 모드에서 UART(IMU) 모드로 동적 전환합니다.
 * @return 성공 시 true, 실패 시 false
 */
bool System_Switch_To_IMU_Mode(void);

/**
 * @brief [RTOS 태스크] "강한(strong)" 정의의 StartupTask 구현부.
 * @details main.c에서 생성된 __weak StartStartupTask를 덮어씁니다.
 * 시스템 초기화를 총괄하고, 완료되면 다른 태스크를 깨운 뒤 자신을 삭제합니다.
 */
void StartStartupTask(void *argument);

/* --- ISR Wrappers (For stm32h7xx_it.c) XM10-XSENS IMU 연결시 사용 --- */
void System_ISR_DMA_UART4_RX_Manual(void);
void System_ISR_DMA_UART4_TX_Manual(void);
void System_ISR_UART4_Manual(void);

#endif /* SYSTEM_CORE_INC_SYSTEM_STARTUP_H_ */
