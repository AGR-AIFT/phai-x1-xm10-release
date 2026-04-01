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

void User_Setup(void)
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

void User_Loop(void)
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
    bool log_start = false;
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) { /* 버튼 1: 녹화 시작 */
        if (XM_IsUsbLogReady()) {
            /*
             * "/LOGS/TestRun_001" 폴더를 만들고 "metadata.txt"를 생성함
             * C언어 문자열 연결 기능을 사용하여 깔끔하게 작성
             * 각 줄 끝에 공백이나 쉼표가 빠지지 않도록 주의하세요.
             * metadata를 저장하면서 log status를 LOG_STATUS_LOGGING으로 변경하여 데이터 저장을 수행할 수 있음.
             */
            log_start = XM_StartUsbDataLog("TestRun_001", "command_torque(float), result_angle(float)");
            if (log_start) {
                XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
            } else {
                /* 실패 (USB 없음 등) -> 빨간불 */
                XM_SetLedEffect(XM_LED_2, XM_LED_HEARTBEAT, 200);
            }
        } else {
            /* 실패 (USB 없음 등) -> 빨간불 */
            XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 200);
        }
    }
}

/* --- ACTIVE 상태 (실험 구간) --- */
static void Active_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500); /* 녹화 중 표시 */
}

static void Active_Loop(void)
{
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