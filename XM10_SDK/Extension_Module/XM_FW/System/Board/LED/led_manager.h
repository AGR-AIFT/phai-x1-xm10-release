/**
 ******************************************************************************
 * @file    led_manager.h
 * @author  HyundoKim
 * @brief   [System Layer] 온보드 RGB LED 관리
 * @details 
 * - PnP (NMT) 상태에 따라 RGB LED의 색상을 제어하는 정책을 관리합니다.
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_BOARD_LED_LED_MANAGER_H_
#define SYSTEM_BOARD_LED_LED_MANAGER_H_

#include "link_interface.h" // LinkNmtState_t
#include "ioif_agrb_gpio.h" // IOIF_GPIOx_t
#include "cm_xm_link.h" // LinkNmtState_t

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

/* --- 1. System Link Status LED (RGB) --- */
typedef enum {
    LINK_STATE_INITIALISING,   // White Solid
    LINK_STATE_PRE_OPERATIONAL,// Blue Blinking (연결 대기)
    LINK_STATE_OPERATIONAL,    // Blue Solid (연결 완료)
    //LINK_STATE_ERROR,          // Red Blinking
    LINK_STATE_STOPPED,        // Red Solid
} LinkState_t;

/* --- 2. User Function LED (Single Color) --- */
typedef enum {
    LED_MODE_OFF,       // 꺼짐
    LED_MODE_SOLID,     // 계속 켜짐
    LED_MODE_BLINK,     // 일정 주기로 깜빡임 (50% Duty)
    LED_MODE_HEARTBEAT, // 두 번 빠르게 깜빡이고 쉼 (두근-두근)
    LED_MODE_ONE_SHOT,  // 한 번만 켜졌다 꺼짐 (이벤트 알림용)
} LedMode_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief  [System] Link Status LED(RGB) 초기화
 * @details system_startup에서 IOIF 핸들을 주입받습니다.
 * @param[in] r_id IOIF 핸들 (PC7)
 * @param[in] g_id IOIF 핸들 (PC8)
 * @param[in] b_id IOIF 핸들 (PC9)
 */
void LedManager_InitLinkStatusLeds(IOIF_GPIOx_t r_id, IOIF_GPIOx_t g_id, IOIF_GPIOx_t b_id);

/**
 * @brief  [System] 현재 Link(NMT) 상태에 맞춰 LED 패턴 자동 설정
 * @details cm_xm_link 모듈이 NMT 상태 변경 시 호출합니다.
 * @param[in] nmtState 새로운 NMT 상태
 * @note   PnP Task에서 호출
 */
void LedManager_SetLinkState(LinkState_t state);

/**
 * @brief  [User] Function LED 초기화
 * @param[in] led1_id IOIF 핸들 (PB8)
 * @param[in] led2_id IOIF 핸들 (PB9)
 * @param[in] led3_id IOIF 핸들 (PB10)
 */
void LedManager_InitUserLeds(IOIF_GPIOx_t led1_id, IOIF_GPIOx_t led2_id, IOIF_GPIOx_t led3_id);

/**
 * @brief  [User] Function LED 모드 설정
 * @param  led_index : 1, 2, 3
 * @param  mode : 동작 모드 (Solid, Blink, Heartbeat 등)
 * @param  period_ms : 주기 (ms). Solid/Off 모드에서는 무시됨.
 */
void LedManager_SetUserLedMode(uint8_t led_index, LedMode_t mode, uint32_t period_ms);

/**
 * @brief  [Common] 모든 LED의 시간 기반 효과 업데이트 (Tick 구동)
 * @param  timestamp_ms : 현재 시스템 시간 (GetTick())
 * @note   User Task 또는 System Task 루프에서 주기적으로 호출해야 함
 */
void LedManager_Update(uint32_t timestamp_ms);

#endif /* SYSTEM_BOARD_LED_LED_MANAGER_H_ */
