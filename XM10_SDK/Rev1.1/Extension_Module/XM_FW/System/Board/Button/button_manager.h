/**
 ******************************************************************************
 * @file    button_manager.h
 * @author  HyundoKim
 * @brief   [System Layer] 온보드 기능 버튼 관리 (디바운싱)
 * @details 10ms 주기로 버튼 핀을 폴링하여 디바운싱을 처리합니다.
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_BOARD_BUTTON_BUTTON_MANAGER_H_
#define SYSTEM_BOARD_BUTTON_BUTTON_MANAGER_H_

#include "ioif_agrb_gpio.h"
#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* --- Button Events --- */
typedef enum {
    BTN_EVENT_NONE        = 0,
    BTN_EVENT_PRESSED     = 1, // 누르는 순간 (Rising Edge)
    BTN_EVENT_RELEASED    = 2, // 떼는 순간 (Falling Edge)
    BTN_EVENT_CLICK       = 3, // 짧게 눌렀다 뗌
    BTN_EVENT_LONG_PRESS  = 4  // 길게 누름 (설정 시간 초과)
} ButtonEvent_t;

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
 * @brief  버튼 관리자 초기화
 * @param[in] btn1_id IOIF 핸들 (PC10)
 * @param[in] btn2_id IOIF 핸들 (PC11)
 * @param[in] btn3_id IOIF 핸들 (PC12)
 */
void ButtonManager_Init(IOIF_GPIOx_t btn1_id, IOIF_GPIOx_t btn2_id, IOIF_GPIOx_t btn3_id);

/**
 * @brief  버튼 상태 업데이트 (Tick 기반)
 * @note   XM_Update() 내부에서 호출됨
 */
void ButtonManager_Update(uint32_t timestamp_ms);

/**
 * @brief  현재 버튼의 물리적 상태 반환 (0: 뗌, 1: 눌림)
 * @note   단순 상태 확인용
 */
uint8_t ButtonManager_GetState(uint8_t btn_index);

/**
 * @brief  버튼 이벤트 큐에서 최신 이벤트를 가져옴 (Read & Clear)
 * @note   이벤트를 한 번 읽으면 NONE으로 초기화됨 (One-shot)
 */
ButtonEvent_t ButtonManager_PopEvent(uint8_t btn_index);

#endif /* SYSTEM_BOARD_BUTTON_BUTTON_MANAGER_H_ */
