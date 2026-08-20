/**
 ******************************************************************************
 * @file    ext_io_safety_control.c
 * @author  HyundoKim
 * @brief   [고급] 외부 리미트 스위치를 활용한 안전 상태 머신 구현
 * @version 1.1
 * @date    Mar 09, 2026
 *
 * @see docs/api-reference/04-external-io.md
 * @see docs/api-reference/01-task-state-machine.md
 * @see docs/api-reference/02-h10-control-n-data.md
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
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
 * PUBLIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;

/* 데모 토크 파형 상태 — Active_Entry 에서 반드시 리셋 (트립 당시 크기로 재개 금지) */
static float  s_demo_torque = 0.0f;
static int8_t s_demo_dir    = 1;   /* +1 = 증가, -1 = 감소 (삼각파) */

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

void Control_Setup(void)
{
    // 핀 설정
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP); // 시작 버튼 (Active-Low, 눌림=LOW)
    /* [W-P2-18] 리미트 스위치는 NC(Normally-Closed) 배선 + 내부 Pull-UP 을 사용합니다:
     *   배선: NC 스위치 한쪽 = GND, 반대쪽 = XM_EXT_DIO_4
     *     평상시(스위치 닫힘)   = GND 도통 → LOW  = 정상
     *     트립(스위치 열림)     = Pull-UP  → HIGH = 정지
     *     배선 단선/커넥터 이탈 = Pull-UP  → HIGH = 정지  ← fail-safe 핵심
     *   NO(Normally-Open)+Pull-DOWN 배선은 단선 시 '정상(LOW)'과 구분되지 않아
     *   (fail-open) 안전 스위치에 부적합합니다. 실제 사용 HW 의 NO/NC 극성을
     *   반드시 확인 후 배선하세요 — NO 스위치를 이 코드에 그대로 연결하면
     *   반대로 상시 트립됩니다. */
    XM_SetPinMode(XM_EXT_DIO_4, XM_EXT_DIO_MODE_INPUT_PULLUP); // 리미트 스위치 (NC: 평상시 LOW, 트립/단선 = HIGH)

    // TSM 설정
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);

    XmStateConfig_t states[] = {
        { .id = XM_STATE_STANDBY, .on_loop = Standby_Loop },
        { .id = XM_STATE_ACTIVE,  .on_entry = Active_Entry, .on_loop = Active_Loop },
        { .id = XM_STATE_ERROR,   .on_entry = Error_Entry,  .on_loop = Error_Loop }
    };

    for(int i=0; i<3; i++) XM_TSM_AddState(s_tsm, &states[i]);
}

void Control_Loop(void)
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
    XM_SetControlMode(XM_CTRL_CONTROL);  // 제어 출력 시작
    s_demo_torque = 0.0f;   // 소프트스타트 — 이전 트립 당시 크기에서 재개 금지
    s_demo_dir    = 1;
}

static void Active_Loop(void)
{
    // [안전 장치] 외부 리미트 스위치(Pin 4) 감지
    // 기구물이 한계에 도달해 스위치가 열리거나(트립), 배선이 끊기면 즉시 정지!
    if (XM_DigitalRead(XM_EXT_DIO_4) == XM_HIGH) { // NC 개방 = 트립 또는 단선 (fail-safe)
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ERROR);
        return;
    }

    // 정상 제어 로직 (데모: 0 ↔ 2.0Nm 연속 삼각파 — 불연속 점프 없음)
    s_demo_torque += 0.01f * (float)s_demo_dir;
    if (s_demo_torque >= 2.0f)      { s_demo_torque = 2.0f; s_demo_dir = -1; }
    else if (s_demo_torque <= 0.0f) { s_demo_torque = 0.0f; s_demo_dir = 1; }
    XM_SetAssistTorque(s_demo_torque, s_demo_torque);
}

/* ====================================================
 * 3. ERROR (비상 정지)
 * ==================================================== */
static void Error_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 100); // 빨간불 빠르게 깜빡임
    /* [P1-03] 안전 스위치 트립 = 비상 정지. XM_SetControlMode(MONITOR)의
     * 지수 램프다운(0.3~0.5초간 감쇠 토크 계속 전송)이 아니라, 즉시 0 토크
     * 확정 + 벡터 해제로 끊는 전용 API 를 사용합니다. */
    XM_EmergencyDisengage();
}

static void Error_Loop(void)
{
    // 사용자가 내부 버튼 1을 눌러 확인해야만 해제
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}