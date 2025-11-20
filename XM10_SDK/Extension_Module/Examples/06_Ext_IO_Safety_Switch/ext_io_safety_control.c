/**
 ******************************************************************************
 * @file    ext_io_safety_control.c
 * @author  HyundoKim
 * @brief   [고급] 외부 리미트 스위치를 활용한 안전 상태 머신 구현
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

static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);

static void Error_Entry(void);
static void Error_Loop(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    // 핀 설정
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP); // 시작 버튼
    XM_SetPinMode(XM_EXT_DIO_4, XM_EXT_DIO_MODE_INPUT);        // 리미트 스위치

    // TSM 설정
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);

    XmStateConfig_t states[] = {
        { .id = XM_STATE_STANDBY, .on_loop = Standby_Loop },
        { .id = XM_STATE_ACTIVE,  .on_entry = Active_Entry, .on_loop = Active_Loop },
        { .id = XM_STATE_ERROR,   .on_entry = Error_Entry,  .on_loop = Error_Loop }
    };

    for(int i=0; i<3; i++) XM_TSM_AddState(s_tsm, &states[i]);
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
 * 1. STANDBY (대기)
 * ==================================================== */
static void Standby_Loop(void)
{
    // 외부 시작 버튼(Pin 3) 감지
    if (XM_DigitalRead(XM_EXT_DIO_3) == XM_LOW) { // 버튼 눌림 (Active Low)
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/* ====================================================
 * 2. ACTIVE (동작 중)
 * ==================================================== */
static void Active_Entry(void)
{
    XM_SetLedState(XM_LED_2, XM_ON);     // 동작 표시 LED
    XM_SetControlMode(XM_CTRL_TORQUE); // 토크 제어 시작
}

static void Active_Loop(void)
{
    // [안전 장치] 외부 리미트 스위치(Pin 4) 감지
    // 기구물이 한계에 도달하여 스위치가 눌리면 즉시 정지!
    if (XM_DigitalRead(XM_EXT_DIO_4) == XM_HIGH) { // 스위치 눌림 (Active High 가정)
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ERROR);
        return;
    }

    // 정상 제어 로직 (예: 천천히 토크 증가)
    static float torque = 0;
    torque += 0.01f;
    if (torque > 2.0f) torque = 0;
    XM_SetAssistTorque(torque, torque);
}

/* ====================================================
 * 3. ERROR (비상 정지)
 * ==================================================== */
static void Error_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 100); // 빨간불 빠르게 깜빡임
    XM_SetControlMode(XM_CTRL_MONITOR);        // 제어 중단 (토크 0)
}

static void Error_Loop(void)
{
    // 사용자가 내부 버튼 1을 눌러 확인해야만 해제
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}