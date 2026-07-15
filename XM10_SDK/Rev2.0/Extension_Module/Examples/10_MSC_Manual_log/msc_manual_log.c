/**
 ******************************************************************************
 * @file    msc_manual_log.c
 * @author  HyundoKim
 * @brief   [레거시] TSM 연동 로깅 예제 — 새 튜토리얼은 10a/10b/10c 참조
 * @details
 * 이 예제는 기존 호환을 위해 유지됩니다.
 * 단계별 학습은 아래 예제를 참고하세요:
 *   - 10a_MSC_Basic_Log       : [초급] 최소한 구조체, 버튼 Start/Stop
 *   - 10b_MSC_Custom_Struct   : [중급] 사용자 정의 구조체, 수동 타임스탬프
 *   - 10c_MSC_Advanced_Log    : [고급] TSM + 에러 핸들링 + 파일 롤링
 * @version 1.1
 * @date    Mar 09, 2026
 *
 * @see     docs/api-reference/05-usb-connectivity.md
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

/* 저장할 데이터 (tick_ms는 System이 자동 삽입하므로 User payload만 정의) */
typedef struct {
    float    cmd_torque;
    float    res_angle;
} MiniLog_t;

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

static MiniLog_t myLog;
static XmTsmHandle_t s_tsm;
static bool s_log_start_failed = false;  /* 로그 시작 실패 → Active_Loop 에서 STANDBY 복귀 */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Standby_loop(void);

static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void Control_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);
    
    XmStateConfig_t sb_conf = { 
        .id = XM_STATE_STANDBY, 
        .on_loop = Standby_loop 
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    /* (1) User payload 등록 — tick_ms는 System이 자동 삽입 (기본 ON) */
    XM_SetUsbLogSource(&myLog, sizeof(MiniLog_t));
    
    /* (2) 옵션: 자동 타임스탬프 비활성화 (User 구조체에 tick을 직접 포함하는 경우) */
    // XM_SetUsbLogAutoTimestamp(false);
    
    /* (3) 옵션: 파일 롤링 크기 변경 (기본 10MB) */
    // XM_SetUsbLogRollingSize(20);
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

static void Standby_loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) { /* 버튼 1: 녹화 시작 */
        if (XM_IsUsbLogReady()) {
            /* 실제 로그 시작(폴더/metadata 생성, 최대 ~100ms 블로킹)은 ACTIVE 진입 시
             * Active_Entry(on_entry)에서 수행 — 블로킹 호출을 실시간 on_loop 밖으로 분리. */
            XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
        } else {
            /* USB 미준비 -> 빨간불 */
            XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 200);
        }
    }
}

/* --- ACTIVE 상태 (실험 구간) --- */
static void Active_Entry(void)
{
    /* 로그 시작은 상태 진입 시 1회 (on_entry) — 블로킹 호출을 1kHz on_loop 밖으로 분리 */
    bool ok = XM_StartUsbDataLog("TestRun_001", "command_torque(float), result_angle(float)");
    if (ok) {
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500); /* 녹화 중 표시 */
    } else {
        /* 시작 실패 — on_entry 전이는 TSM 엔진이 무시하므로 플래그로 다음 tick 복귀 */
        s_log_start_failed = true;
        XM_SetLedEffect(XM_LED_2, XM_LED_HEARTBEAT, 200);
    }
}

static void Active_Loop(void)
{
    /* 로그 시작 실패 폴백 (on_entry 에서 이월) */
    if (s_log_start_failed) {
        s_log_start_failed = false;
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    /* User payload만 채우면 됨 (tick_ms는 System이 자동 삽입) */
    myLog.cmd_torque = XM.command.assist_torque_rh;
    myLog.res_angle  = XM.status.h10.rightHipAngle;

    /* 버튼 2: 녹화 종료 (저장) */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Active_Exit(void)
{
    /* 상태를 나갈 때 무조건 저장 및 파일 닫기 */
    if (XM_GetUsbLogStatus() == XM_LOG_STATUS_LOGGING) {
        XM_StopUsbDataLog();
        XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0); /* 대기 상태 표시 */
    }
}