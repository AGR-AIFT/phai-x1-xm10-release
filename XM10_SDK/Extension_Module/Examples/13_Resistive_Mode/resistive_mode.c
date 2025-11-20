/**
 ******************************************************************************
 * @file    resistive_mode.c
 * @author  HyundoKim
 * @brief   [응용 예제] H10 내부 저항 모드 제어 (Resistive Mode)
 * @details 
 * 사용자의 허벅지 움직임 속도에 비례하여 반대 방향으로 토크를 생성합니다.
 * 마치 물속에서 걷는 듯한 저항감을 주어 근력 운동 효과를 냅니다.
 * XM10에서 직접 토크를 계산하지 않고, KIT H10에 내장된 저항 제어 로직을 활용합니다.
 * 사용자가 슈트 버튼으로 '보조 레벨(Assist Level)'을 변경하면,
 * 그에 맞춰 적절한 '저항 강도(Gain)'를 계산하여 H10에게 전달합니다.
 * * [제어 로직]
 * Torque(허벅지 목표 토크) = -Gain * Velocity(허벅지 각속도)
 * @version 0.1
 * @date    Nov 18, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"           // XM API

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* 저항 강도 (Gain) */
// 값이 클수록 더 뻑뻑하게 느껴집니다. (-0.1 ~ -5.0 권장)
#define RESISTIVE_GAIN          (-6.0f)  

/* 속도 데드존 (Threshold) */
// 이 속도(deg/s) 이상으로 움직일 때만 저항을 겁니다. (노이즈 방지)
#define VELOCITY_THRESHOLD      (5.0f)  

/* 안전을 위한 최대 토크 제한 (Nm) */
#define MAX_RESISTIVE_TORQUE    (10.0f)

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
// 변경 감지용 이전 레벨 저장 변수 (초기값 255로 설정하여 첫 실행 시 무조건 전송 유도)
static uint8_t s_prev_assist_level = 255;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Off_Entry(void);
static void Off_Loop(void);

static void Standby_Entry(void);
static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t states[] = {
        { .id = XM_STATE_OFF,     .on_entry = Off_Entry,    .on_loop = Off_Loop },
        { .id = XM_STATE_STANDBY, .on_entry = Standby_Entry,.on_loop = Standby_Loop },
        { .id = XM_STATE_ACTIVE,  .on_entry = Active_Entry, .on_loop = Active_Loop, .on_exit = Active_Exit }
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

/* --- OFF State --- */
static void Off_Entry(void)
{
    XM_SetLedState(XM_LED_1, XM_OFF);
    // 안전을 위해 게인 0으로 리셋
    XM_SetResistiveCompGain(SYS_NODE_ID_LH, 0.0f);
    XM_SetResistiveCompGain(SYS_NODE_ID_RH, 0.0f);
}

static void Off_Loop(void)
{
    // 버튼 1 클릭 -> STANDBY
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/* --- STANDBY State --- */
static void Standby_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    
    // 다시 진입할 때 강제 업데이트를 위해 초기화
    s_prev_assist_level = 255;
}

static void Standby_Loop(void)
{
    // 버튼 2 클릭 -> ACTIVE (운동 시작)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
    // 버튼 1 롱프레스 -> OFF
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_LONG_PRESS) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }
}

/* --- ACTIVE State (저항 모드 동작) --- */
static void Active_Entry(void)
{
    XM_SetLedState(XM_LED_1, XM_ON);
    
    // 진입 시 강제 업데이트 예약
    s_prev_assist_level = 255;
}

static void Active_Loop(void)
{
    // 1. 종료 조건 (버튼 2)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    // 2. [Input] 현재 H10 슈트의 보조 레벨 읽어오기 (1~9단계)
    // (core_process가 수신하여 XM.status에 넣어둠)
    uint8_t current_level = XM.status.h10.h10AssistLevel;

    // 3. [Logic] 레벨이 변경되었을 때만 게인값 전송 (Event-driven Update)
    if (current_level != s_prev_assist_level) {
        // 게인 계산: Level * 0.1 * -6.0
        float gain = (float)current_level * 0.1f * RESISTIVE_GAIN;
        
        // [Output] API를 통해 H10으로 게인 설정 명령 전송
        // (변경된 경우에만 core_process가 Flush 수행)
        XM_SetResistiveCompGain(SYS_NODE_ID_LH, gain);
        XM_SetResistiveCompGain(SYS_NODE_ID_RH, gain);
        
        // 디버깅 메시지
        char msg[64];
        sprintf(msg, "Level Changed: %d -> Gain: %.2f\r\n", current_level, gain);
        XM_SendUsbDebugMessage(msg);

        // 현재 상태 저장
        s_prev_assist_level = current_level;
    }
}

static void Active_Exit(void)
{
    // 나갈 때는 저항을 풀어줌 (안전)
    XM_SetResistiveCompGain(SYS_NODE_ID_LH, 0.0f);
    XM_SetResistiveCompGain(SYS_NODE_ID_RH, 0.0f);
}