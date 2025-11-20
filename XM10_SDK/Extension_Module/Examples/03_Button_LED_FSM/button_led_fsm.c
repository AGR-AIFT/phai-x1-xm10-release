/**
 ******************************************************************************
 * @file    button_led_fsm.c
 * @author  HyundoKim
 * @brief   [고급] 롱 프레스와 Task State Machine을 이용한 모드 전환
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

static void Standby_Entry(void);
static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    // TSM 생성 (초기 상태: STANDBY)
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);

    // 1. STANDBY 상태 등록
    XmStateConfig_t sb_conf = {
        .id = XM_STATE_STANDBY,
        .on_entry = Standby_Entry,
        .on_loop  = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    // 2. ACTIVE 상태 등록
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop
    };
    XM_TSM_AddState(s_tsm, &act_conf);
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

/* ====================================================
 * State 1: STANDBY (대기 모드)
 * - 진입 시: LED 심장박동 효과 설정
 * - 루프 시: 롱 프레스 감지하면 ACTIVE로 전환
 * ==================================================== */
static void Standby_Entry(void)
{
    // 두근-두근 효과 (1초 주기)
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000); 
}

static void Standby_Loop(void)
{
    // 버튼 1을 1초 이상 꾹 누르면 모드 변경
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_LONG_PRESS) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
        XM_SendUsbDebugMessage("[Mode] STANDBY\r\n");
    }
}

/* ====================================================
 * State 2: ACTIVE (동작 모드)
 * - 진입 시: LED 빠른 깜빡임 설정
 * - 루프 시: 롱 프레스 감지하면 STANDBY로 복귀
 * ==================================================== */
static void Active_Entry(void)
{
    // 경고등처럼 빠르게 깜빡임 (200ms 주기)
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SendUsbDebugMessage("[Mode] ACTIVE !!\r\n");
}

static void Active_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_LONG_PRESS) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
    
    // (여기에 모터 제어 등 실제 동작 코드가 들어감)
}