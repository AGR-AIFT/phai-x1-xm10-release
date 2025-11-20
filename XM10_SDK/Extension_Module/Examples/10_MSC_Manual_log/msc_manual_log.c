/**
 ******************************************************************************
 * @file    msc_manual_log.c
 * @author  HyundoKim
 * @brief   [초급, 중급, 고급] 버튼으로 로깅 시작/정지 제어, 사용자 정의 구조체 및 
 * 상태 머신과 연동된 자동 실험 로깅
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

// 저장할 데이터 최적화 (CSV 컬럼이 됨)
typedef struct {
    uint32_t tick_ms;
    float    cmd_torque;
    float    res_angle;
} MiniLog_t;

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

    // ACTIVE 상태에 Entry/Exit 함수 연결
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    /* 내가 만든 구조체만 저장하도록 등록 */
    XM_SetUsbLogSource(&myLog, sizeof(MiniLog_t));
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
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) { // 버튼 1: 녹화 시작
        if (XM_IsUsbLogReady()) {
            // "/LOGS/TestRun_001" 폴더를 만들고 "metadata.txt"를 생성함
            // C언어 문자열 연결 기능을 사용하여 깔끔하게 작성
            // 각 줄 끝에 공백이나 쉼표가 빠지지 않도록 주의하세요.
            // meta data를 저장하면서 log status를 LOG_STATUS_LOGGING으로 변경하여 데이터 저장을 수행할 수 있음.
            log_start = XM_StartUsbDataLog("TestRun_001", "time_ms, command_torque, result_angle");
            if (log_start) {
                XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
            } else {
                // 실패 (USB 없음 등) -> 빨간불
                XM_SetLedEffect(XM_LED_2, XM_LED_HEARTBEAT, 200); 
            }
        } else {
            // 실패 (USB 없음 등) -> 빨간불
            XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 200); 
        }
    }
}

/* --- ACTIVE State (실험 구간) --- */
static void Active_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500); // 녹화 중 표시
}

static void Active_Loop(void)
{
    // ... 실험 및 제어 로직 수행 ...
    // (데이터는 core_process가 2ms마다 자동으로 myLog 구조체를 저장함)
    // 데이터 채우기
    myLog.tick_ms    = XM_GetTick();
    myLog.cmd_torque = XM.command.assist_torque_rh;
    myLog.res_angle  = XM.status.h10.rightHipAngle;
    
    // 버튼 2: 녹화 종료 (저장)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Active_Exit(void)
{
    // 상태를 나갈 때 무조건 저장 및 파일 닫기
    if (XM_GetUsbLogStatus() == XM_LOG_STATUS_LOGGING) {
        XM_StopUsbDataLog();
        XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0); // 대기 상태 표시
    }
}