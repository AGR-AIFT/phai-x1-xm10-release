/**
 ******************************************************************************
 * @file    cdc_basic_print.c
 * @author  HyundoKim
 * @brief   [초급] 버튼 이벤트 발생 시 텍스트 메시지 전송
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
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
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
    // 버튼 1 클릭 이벤트 감지
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        // 단순 문자열 전송
        XM_SendUsbDebugMessage("Hello! Button 1 was clicked.\r\n");
        
        // LED 깜빡임으로 반응 확인
        XM_SetLedEffect(XM_LED_1, XM_LED_ONESHOT, 100);
    }
}