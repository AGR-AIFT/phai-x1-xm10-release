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

#include <stdint.h>

#include "ioif_agrb_fdcan.h"
#include "ioif_agrb_gpio.h"
#include "ioif_agrb_spi.h"
#include "ioif_agrb_uart.h"
#include "canfd_tx.h"           /* Ch1 · Ch2 송신 경로 — 선언이 옮겨 갔다 (2026-10-29) */

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
 * @brief EXT_PWR_SEL_5V(PE3)의 IOIF GPIO 핸들을 반환합니다.
 * @return IOIF_GPIOx_t 핸들.
 */
IOIF_GPIOx_t System_GetExtPwrSelGpioId(void);

/**
 * @brief External UART(USART2, PD5/PD6)의 IOIF 핸들을 반환합니다.
 * @details Application Layer Facade(예: XM_AttachXsensMTi630)에서 사용.
 *          향후 범용 Serial API에서도 동일 핸들을 활용.
 * @return IOIF_UARTx_t 핸들.
 */
IOIF_UARTx_t System_GetExternalUartId(void);

/**
 * @brief SPI2 (MCP79510 RTC, SW NSS)의 IOIF 핸들을 반환합니다.
 * @details Rev2.0 전용. SI 검증 stimulus / 진단 도구에서 직접 SPI 토글이 필요할 때.
 *          평시 driver path 는 MCP79510_Read/Write 사용.
 * @return IOIF_SPIx_t 핸들 (미초기화 시 IOIF_SPI_ID_NOT_ALLOCATED).
 */
IOIF_SPIx_t System_GetSpi2RtcId(void);

/**
 * @brief SPI5 (PCA9957 LED Driver, SW NSS)의 IOIF 핸들을 반환합니다.
 * @details Rev2.0 전용. SI 검증 stimulus 용. 평시는 PCA9957_* driver path.
 * @return IOIF_SPIx_t 핸들 (미초기화 시 IOIF_SPI_ID_NOT_ALLOCATED).
 */
IOIF_SPIx_t System_GetSpi5LedDrvId(void);

/**
 * @brief Power On LED(PC6)의 IOIF GPIO 핸들을 반환합니다.
 * @details 부팅 완료 직후 상시 Solid ON(led_manager 관리 대상 아님). 생산검사
 *          LED 일괄 점등(OD 0x7EA0) 이 5종 GPIO 를 함께 재확인할 때 사용.
 * @return IOIF_GPIOx_t 핸들 (미초기화 시 IOIF_GPIO_NOT_INITIALIZED).
 */
IOIF_GPIOx_t System_GetPowerLedGpioId(void);

/** @brief Startup 결과 비트맵 — bit=1 이면 해당 서브시스템 init 실패, 0 = 전부 정상 */
uint32_t System_GetStartupResultBitmap(void);
/** @brief 하나라도 init 실패가 기록되면 true (degraded 동작 중) */
bool System_HasStartupDegraded(void);

/**
 * @brief HWREV[2:0] strap 값을 읽어 반환합니다.
 * @return bit2=HWREV_2, bit1=HWREV_1, bit0=HWREV_0. 미초기화 시 0.
 */
uint8_t System_GetHwRevisionStrap(void);

/**
 * @brief [RTOS 태스크] "강한(strong)" 정의의 StartupTask 구현부.
 * @details main.c에서 생성된 __weak StartStartupTask를 덮어씁니다.
 * 시스템 초기화를 총괄하고, 완료되면 다른 태스크를 깨운 뒤 자신을 삭제합니다.
 */
void StartStartupTask(void *argument);

#endif /* SYSTEM_CORE_INC_SYSTEM_STARTUP_H_ */
