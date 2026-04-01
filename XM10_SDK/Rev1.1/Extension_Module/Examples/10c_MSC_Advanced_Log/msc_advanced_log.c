/**
 ******************************************************************************
 * @file    msc_advanced_log.c
 * @author  HyundoKim
 * @brief   [고급] 상태 머신(TSM) + 에러 모니터링 + 파일 롤링 + LED 피드백
 * @details
 * [학습 목표]
 *   - 상태 머신(TSM)과 로깅을 연동하는 패턴
 *   - 로깅 상태(WARNING, ERROR)를 모니터링하여 LED로 표시하는 방법
 *   - 파일 롤링 크기를 런타임에 설정하는 방법
 *   - 에러 발생 시 자동 복구 (재시작) 전략
 *
 * [상태 전이]
 *   STANDBY ──(BTN1 클릭)──▶ ACTIVE ──(BTN2 클릭)──▶ STANDBY
 *                             │                          ▲
 *                             └──(ERROR 감지)────────────┘
 *
 * [LED 피드백]
 *   LED1 OFF    : STANDBY (대기)
 *   LED1 BLINK  : ACTIVE (로깅 중)
 *   LED2 BLINK  : WARNING (버퍼 90%+, 쓰기 지연 발생)
 *   LED2 SOLID  : WARNING (디스크 잔여 50MB 미만)
 *   LED3 SOLID  : ERROR (로깅 중단, 자동 복귀)
 *
 * @version 1.1
 * @date    Mar 09, 2026
 * @see     docs/api-reference/05-usb-connectivity.md
 * @see     docs/api-reference/01-task-state-machine.md
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef struct {
    float hip_L;
    float hip_R;
    float knee_L;
    float knee_R;
    float torque_L;
    float torque_R;
} GaitLog_t;  /* 24 bytes */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static GaitLog_t s_log;
static XmTsmHandle_t s_tsm;
static uint32_t s_session_counter = 0;
static char s_session_name[32];

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

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
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);

    XmStateConfig_t sb = { .id = XM_STATE_STANDBY, .on_loop = Standby_Loop };
    XM_TSM_AddState(s_tsm, &sb);

    XmStateConfig_t act = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act);

    XM_SetUsbLogSource(&s_log, sizeof(GaitLog_t));

    /* 파일 롤링: 5MB마다 분할 (작은 USB에서 테스트 시 유용) */
    XM_SetUsbLogRollingSize(5);
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

static void Standby_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogReady()) {
            XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
        } else {
            XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 200);
        }
    }
}

static void Active_Entry(void)
{
    snprintf(s_session_name, sizeof(s_session_name), "Gait_%03lu", 
             (unsigned long)s_session_counter++);

    bool ok = XM_StartUsbDataLog(
        s_session_name,
        "hip_L(float), hip_R(float), "
        "knee_L(float), knee_R(float), "
        "torque_L(float), torque_R(float)"
    );

    if (ok) {
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);

        /* 디스크 용량 확인 */
        uint32_t free_mb = XM_GetUsbDiskFreeMB();
        uint32_t total_mb = XM_GetUsbDiskTotalMB();
        (void)free_mb; (void)total_mb; /* 디버그용 */

        /* 세션 시작 마커 삽입 (type=1: session start) */
        XM_InsertUsbLogMarker(1, (uint16_t)s_session_counter);
    } else {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Active_Loop(void)
{
    /* 1. 데이터 갱신 */
    s_log.hip_L    = XM.status.h10.leftHipAngle;
    s_log.hip_R    = XM.status.h10.rightHipAngle;
    s_log.knee_L   = XM.status.h10.leftKneeAngle;
    s_log.knee_R   = XM.status.h10.rightKneeAngle;
    s_log.torque_L = XM.command.assist_torque_lh;
    s_log.torque_R = XM.command.assist_torque_rh;

    /* 2. 로깅 상태 모니터링 */
    XmLogStatus_e status = XM_GetUsbLogStatus();

    switch (status) {
    case XM_LOG_STATUS_WARNING_QUEUE_FULL:
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 200);
        break;
    case XM_LOG_STATUS_WARNING_DISK_LOW:
        XM_SetLedEffect(XM_LED_2, XM_LED_SOLID, 0);
        break;
    case XM_LOG_STATUS_ERROR_STOPPED:
        XM_SetLedEffect(XM_LED_3, XM_LED_SOLID, 0);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    default:
        XM_SetLedEffect(XM_LED_2, XM_LED_SOLID, 0);
        break;
    }

    /* 3. 수동 정지 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        /* 수동 정지 마커 삽입 (type=2: manual stop) */
        XM_InsertUsbLogMarker(2, 0);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Active_Exit(void)
{
    if (XM_GetUsbLogStatus() != XM_LOG_STATUS_IDLE) {
        XM_StopUsbDataLog();
    }
    XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0);
    XM_SetLedEffect(XM_LED_2, XM_LED_SOLID, 0);
    XM_SetLedEffect(XM_LED_3, XM_LED_SOLID, 0);
}
