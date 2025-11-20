/**
 ******************************************************************************
 * @file    button_led_event.c
 * @author  HyundoKim
 * @brief   [중급] 클릭 이벤트 감지 및 LED 특수 효과 사용
 * @version 0.1
 * @date    Nov 18, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

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

static XmTsmHandle_t s_tsm;
static bool s_led1_state = false; // LED 1 토글 상태 저장용

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Run_Loop(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    
    XmStateConfig_t conf = {
        .id = XM_STATE_USER_START,
        .on_loop = Run_Loop
    };
    XM_TSM_AddState(s_tsm, &conf);
}

void User_Loop(void)
{
    XM_TSM_Run(s_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

static void Run_Loop(void)
{
    /* Case 1: 버튼 1 (토글 스위치 구현) */
    // GetButtonEvent는 이벤트를 한 번 읽으면 자동으로 초기화됩니다(Read-Clear).
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_led1_state = !s_led1_state; // 상태 반전
        XM_SetLedState(XM_LED_1, s_led1_state);
    }

    /* Case 2: 버튼 2 (알림등 구현) */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        // LED 2를 2000ms 동안만 켰다가 자동으로 끔 (타이머 구현 불필요)
        XM_SetLedEffect(XM_LED_2, XM_LED_ONESHOT, 2000);
    }
}
