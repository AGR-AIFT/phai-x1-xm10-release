/**
 ******************************************************************************
 * @file    control_task.c
 * @author  HyundoKim
 * @brief   사용자 제어 알고리즘 진입점 — Control_Setup() / Control_Loop()
 * @details
 * 본 파일은 사용자가 직접 수정하는 영역입니다.
 *   - Control_Setup() : 부팅 직후 1회 실행 (TSM 생성, 변수 초기화 등)
 *   - Control_Loop()  : 1 ms (1 kHz) 주기로 반복 호출
 *
 * 명명 안내 (2026-05-15):
 *   기존 User_Setup() / User_Loop() 는 Control_Setup() / Control_Loop() 로
 *   통일되었습니다. 기존 자산이 있다면 함수명만 바꾸시면 동일하게 동작합니다.
 *   1 릴리즈 동안은 weak shim (XM_FW/System/Core/user_compat.c) 이 자동
 *   호환을 제공하지만, 다음 major 릴리즈에서 제거 예정이니 가능한 빨리
 *   마이그레이션 권장.
 *
 * @version 0.2
 * @date    2026-05-15
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"      // 통합 API 헤더

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES
 *-----------------------------------------------------------
 */

static XmTsmHandle_t s_user_tsm;

/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void Off_Entry(void);
static void Off_Loop(void);

static void Standby_Entry(void);
static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

/* =================================================================
 * [필수 구현 1] 초기화 함수 (Control_Setup)
 * - 전원 인가 후 딱 한 번 실행됩니다.
 * - TSM 생성, 변수 초기화, LED/Button 초기 설정 등을 수행합니다.
 * - 함수 이름을 수정하거나 삭제하면 동작하지 않습니다.
 * ================================================================= */
void Control_Setup(void)
{
    s_user_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = {
        .id = XM_STATE_OFF,
        .on_entry = Off_Entry,
        .on_loop  = Off_Loop
    };
    XM_TSM_AddState(s_user_tsm, &off_conf);

    XmStateConfig_t sb_conf = {
        .id = XM_STATE_STANDBY,
        .on_entry = Standby_Entry,
        .on_loop  = Standby_Loop
    };
    XM_TSM_AddState(s_user_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit,
    };
    XM_TSM_AddState(s_user_tsm, &act_conf);
}

/* =================================================================
 * [필수 구현 2] 반복 루프 함수 (Control_Loop)
 * - 1 ms (1 kHz) 주기로 계속 호출됩니다.
 * - 함수 이름을 수정하거나 삭제하면 동작하지 않습니다.
 * - 한 사이클 안에서 너무 무거운 계산은 피하세요 (1 ms 이내 권장).
 * ================================================================= */
void Control_Loop(void)
{
    XM_TSM_Run(s_user_tsm);
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — 상태별 동작 함수
 *-----------------------------------------------------------
 */

static void Off_Entry(void) { }
static void Off_Loop(void) { }

static void Standby_Entry(void) { }
static void Standby_Loop(void) { }

static void Active_Entry(void) { }
static void Active_Loop(void) { }
static void Active_Exit(void) { }
