/**
 ******************************************************************************
 * @file    xm_api_led_btn.c
 * @author  HyundoKim
 * @brief   XM10 LED 및 버튼 API 구현부
 * @details 
 * 이 모듈은 단순한 GPIO 제어를 넘어, 시간 기반의 효과(Blink, Long-press)를 제공합니다.
 * 이를 위해 XM_IO_Update() 함수가 User Task 루프에서 주기적으로 호출되고 있습니다. (core process 내부)
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api_led_btn.h"
#include "led_manager.h"
#include "button_manager.h"
#include "ioif_agrb_tim.h" // GetTick

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/* --- LED Implementation --- */
void XM_SetLedState(XmLedId_t led_idx, XmState_t state)
{
    // XM_ON/OFF -> LED_MODE_SOLID/OFF
    LedMode_t target_mode = (state == XM_ON) ? LED_MODE_SOLID : LED_MODE_OFF;

    // XmLedId_t(1,2,3) -> uint8_t(1,2,3)
    // led_manager는 내부적으로 (id - 1)을 수행하여 배열 인덱스(0,1,2)를 찾습니다.
    LedManager_SetUserLedMode((uint8_t)led_idx, target_mode, 0);
}

void XM_SetLedEffect(XmLedId_t led_idx, XmLedMode_t mode, uint32_t period_ms)
{
    // API Enum을 Manager Enum으로 캐스팅 (값 호환됨)
    // XM_LED_BLINK(2) -> LED_MODE_BLINK(2)
    LedManager_SetUserLedMode((uint8_t)led_idx, (LedMode_t)mode, period_ms);
}


/* --- Button Implementation --- */
XmBtnState_t XM_GetButtonState(XmBtnId_t btn_idx)
{
    // button_manager는 1(Pressed), 0(Released)을 반환함
    uint8_t raw_state = ButtonManager_GetState((uint8_t)btn_idx);
    // 1 -> XM_PRESSED, 0 -> XM_RELEASED
    return (XmBtnState_t)raw_state;
}

XmBtnEvent_t XM_GetButtonEvent(XmBtnId_t btn_idx)
{
    // Button Manager에서 이벤트를 Pop 해옴 (읽으면 사라짐)
    // API Enum과 Manager Enum 값이 1:1 매핑되므로 캐스팅 사용
    // button_manager의 이벤트를 그대로 캐스팅하여 전달
    return (XmBtnEvent_t)ButtonManager_PopEvent((uint8_t)btn_idx);
}

/* --- Common Implementation --- */
void XM_IO_Update(void)
{
    // 현재 시스템 시간(Tick)을 가져옴
    uint32_t current_tick = IOIF_GetTick();
    
    // 1. LED 매니저에게 시간을 알려줌 -> 깜빡임 계산
    LedManager_Update(current_tick);
    
    // 2. 버튼 매니저에게 시간을 알려줌 -> 디바운싱/롱프레스 계산
    ButtonManager_Update(current_tick);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */
