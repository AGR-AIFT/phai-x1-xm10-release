/**
 ******************************************************************************
 * @file    button_led_basic.c
 * @author  HyundoKim
 * @brief   [초급] 버튼 상태를 LED에 직접 반영하기
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

/* --- 전역 핸들 --- */
static XmTsmHandle_t s_tsm;

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

/* --- Setup & Loop --- */
void User_Setup(void)
{
    // 기본 태스크 생성 (단일 상태)
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

/* --- State Logic --- */
static void Run_Loop(void)
{
    // 1. 버튼 1의 현재 물리적 상태 확인
    XmBtnState_t state = XM_GetButtonState(XM_BTN_1);

    // "만약 눌려 있다면(PRESSED)" -> 0인지 1인지 몰라도 됨!
    if (state == XM_PRESSED) {
        // "LED 1을 켜라(ON)"
        XM_SetLedState(XM_LED_1, XM_ON);
    } else {
        XM_SetLedState(XM_LED_1, XM_OFF);
    }
    
    // (참고) 한 줄로 작성 가능: SetLedState(1, GetButtonState(1));
}
