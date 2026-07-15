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
 * @details 1-byte rolling counter payload. UserTask(1kHz)에서 호출.
 *          ISR-Safe: IOIF Tx Mutex 우회, HAL 직접 호출.
 *          PnP Operational 진입 후에만 호출할 것 (CM_Drv_IsConnected() 체크).
 */
void System_SendSync_Ch1(void);

/**
 * @brief CiA 301 SYNC를 FDCAN2(Ch2, XM↔SM)으로 전송합니다.
 * @details 1-byte rolling counter payload. UserTask(1kHz)에서 호출.
 *          IOIF Tx 사용 (Ch2는 PnP/SDO 등 다중 Tx 소스, Mutex 보호).
 *          최소 1개 SM이 Online일 때만 호출할 것.
 */
void System_SendSync_Ch2(void);

/**
 * @brief Notify FDCAN2 bus manager that a sensor-module bootup/heartbeat was seen.
 * @details Called from the FDCAN Rx routing task. This is non-blocking and only
 *          marks Tx cleanup for the next task-context FDCAN2 transmit.
 */
void System_Fdcan2_NotifySensorHeartbeat(uint8_t node_id, uint8_t nmt_state);

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

/**
 * @brief HWREV[2:0] strap 값을 읽어 반환합니다.
 * @return bit2=HWREV_2, bit1=HWREV_1, bit0=HWREV_0. 미초기화 시 0.
 */
uint8_t System_GetHwRevisionStrap(void);

/**
 * @brief GRF 포트 전원 폴트 자동 복구 주기 실행 (grf-pwr-watchdog).
 * @details 로드스위치 #FAULT(low-active)가 디바운스(연속 3샘플) 확정될 때만 해당
 *          포트 EN 을 off(500ms)→on 토글하고, 재인가 후 1s blanking 동안은 소프트스타트
 *          인러시를 폴트로 오판하지 않는다. 재시도는 지수 백오프(2s→30s), 연속 8회 미해소
 *          시 hands-off(EN 유지 = 워치독 도입 전 거동, 스위치 자체 보호에 위임). 핫플러그/
 *          부팅 인러시 트립으로 GRF 레일이 영구 사망하던 문제의 자동 복구선.
 *          ⚠️ 초판(off 100ms/2s 고정/무제한)은 트립 즉시재발 시 2s 주기 무한 콜드리셋으로
 *          dead-window 를 영속화해 폐기됨(2026-07-13). 관측: g_dbg_grf_power_retry_count[L/R],
 *          g_dbg_grf_power_fault_mask.
 * @note Call context: PnP Task 주기 루프 (~100ms, Task 전용 — 단일 태스크 소유, 락 불필요).
 */
void System_GrfPowerRunPeriodic(void);

/**
 * @brief [RTOS 태스크] "강한(strong)" 정의의 StartupTask 구현부.
 * @details main.c에서 생성된 __weak StartStartupTask를 덮어씁니다.
 * 시스템 초기화를 총괄하고, 완료되면 다른 태스크를 깨운 뒤 자신을 삭제합니다.
 */
void StartStartupTask(void *argument);

#endif /* SYSTEM_CORE_INC_SYSTEM_STARTUP_H_ */
