/**
 ******************************************************************************
 * @file    quick_start.c
 * @author  HyundoKim
 * @brief   [입문] XM10 보드 동작 확인 — LED + 버튼 + USB 출력
 * @details
 * XM10 보드가 정상적으로 동작하는지 확인하는 가장 첫 번째 예제입니다.
 * 외부 하드웨어 연결 없이 보드 단독으로 실행할 수 있습니다.
 *
 * [학습 목표]
 * - User_Setup() / User_Loop() 진입점 구조 이해
 * - Task State Machine(TSM) 기본 생성 및 실행
 * - LED, 버튼, USB CDC의 기초 동작 확인
 *
 * [동작 순서]
 * 1. 부팅 시: LED 1~3 순차 점등 (시각적 부팅 확인)
 * 2. 대기 상태: LED 1 Heartbeat (정상 동작 표시)
 * 3. BTN 1 클릭: USB CDC로 "Hello XM10!" 메시지 전송
 * 4. BTN 2 클릭: LED 2 토글 (ON/OFF 상태 유지)
 *
 * @see     docs/api-reference/01-task-state-machine.md  (TSM API)
 * @see     docs/api-reference/03-led-btn-control.md     (LED/버튼 API)
 * @see     docs/api-reference/05-usb-connectivity.md    (USB CDC API)
 *
 * @version 1.0
 * @date    Mar 09, 2026
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* 부팅 LED 시퀀스 설정 */
#define BOOT_LED_INTERVAL_MS    200   /* 각 LED 점등 간격 (ms) */
#define BOOT_LED_HOLD_MS        500   /* 전체 점등 유지 시간 (ms) */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/* 부팅 시퀀스 단계 */
typedef enum {
    BOOT_STEP_LED1,         /* LED 1 점등 */
    BOOT_STEP_LED2,         /* LED 2 점등 */
    BOOT_STEP_LED3,         /* LED 3 점등 */
    BOOT_STEP_HOLD,         /* 전체 유지 */
    BOOT_STEP_CLEAR,        /* 전체 소등 */
    BOOT_STEP_DONE          /* 완료 */
} BootStep_t;

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

/* 부팅 시퀀스 */
static BootStep_t s_boot_step = BOOT_STEP_LED1;
static uint32_t   s_boot_timer = 0;

/* LED 2 토글 상태 */
static bool s_is_led2_on = false;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Run_Entry(void);
static void Run_Loop(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief  시스템 초기화 — 부팅 시 1회 호출
 * @details TSM 생성 및 상태 등록. 외부 하드웨어 설정 없음.
 */
void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);

    XmStateConfig_t conf = {
        .id       = XM_STATE_USER_START,
        .on_entry = Run_Entry,
        .on_loop  = Run_Loop
    };
    XM_TSM_AddState(s_tsm, &conf);
}

/**
 * @brief  메인 루프 — 1ms 주기로 반복 호출
 */
void User_Loop(void)
{
    XM_TSM_Run(s_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief  상태 진입 — 부팅 시퀀스 시작 및 USB 메시지 전송
 */
static void Run_Entry(void)
{
    s_boot_step  = BOOT_STEP_LED1;
    s_boot_timer = XM_GetTick();

    /* 부팅 완료 메시지 (PC 터미널에서 확인) */
    XM_SendUsbDebugMessage("[QuickStart] XM10 보드 준비 완료!\r\n");
}

/**
 * @brief  메인 로직 — 부팅 시퀀스 + 버튼 이벤트 처리
 *
 * [부팅 시퀀스]
 * LED 1 → LED 2 → LED 3 → 전체 유지 → 전체 소등 → Heartbeat 시작
 *
 * [버튼 이벤트]
 * - BTN 1 클릭: USB CDC로 인사 메시지 전송
 * - BTN 2 클릭: LED 2 토글 (ON ↔ OFF)
 */
static void Run_Loop(void)
{
    uint32_t now = XM_GetTick();

    /* ===== 1단계: 부팅 LED 시퀀스 ===== */
    if (s_boot_step != BOOT_STEP_DONE) {
        switch (s_boot_step) {
            case BOOT_STEP_LED1:
                XM_SetLedState(XM_LED_1, XM_ON);
                if (now - s_boot_timer >= BOOT_LED_INTERVAL_MS) {
                    s_boot_step  = BOOT_STEP_LED2;
                    s_boot_timer = now;
                }
                break;

            case BOOT_STEP_LED2:
                XM_SetLedState(XM_LED_2, XM_ON);
                if (now - s_boot_timer >= BOOT_LED_INTERVAL_MS) {
                    s_boot_step  = BOOT_STEP_LED3;
                    s_boot_timer = now;
                }
                break;

            case BOOT_STEP_LED3:
                XM_SetLedState(XM_LED_3, XM_ON);
                if (now - s_boot_timer >= BOOT_LED_INTERVAL_MS) {
                    s_boot_step  = BOOT_STEP_HOLD;
                    s_boot_timer = now;
                }
                break;

            case BOOT_STEP_HOLD:
                /* 전체 LED 유지 */
                if (now - s_boot_timer >= BOOT_LED_HOLD_MS) {
                    s_boot_step  = BOOT_STEP_CLEAR;
                    s_boot_timer = now;
                }
                break;

            case BOOT_STEP_CLEAR:
                /* 전체 소등 후 Heartbeat 시작 */
                XM_SetLedState(XM_LED_1, XM_OFF);
                XM_SetLedState(XM_LED_2, XM_OFF);
                XM_SetLedState(XM_LED_3, XM_OFF);
                XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
                s_boot_step = BOOT_STEP_DONE;
                XM_SendUsbDebugMessage("[QuickStart] 부팅 시퀀스 완료. 버튼을 눌러보세요!\r\n");
                break;

            default:
                break;
        }
        return;  /* 부팅 중에는 버튼 이벤트 무시 */
    }

    /* ===== 2단계: 버튼 이벤트 처리 ===== */

    /* BTN 1: USB CDC 메시지 전송 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_SendUsbDebugMessage("Hello XM10! 버튼 1이 클릭되었습니다.\r\n");
        XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, 200);
    }

    /* BTN 2: LED 2 토글 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_is_led2_on = !s_is_led2_on;
        XM_SetLedState(XM_LED_2, s_is_led2_on ? XM_ON : XM_OFF);
    }
}
