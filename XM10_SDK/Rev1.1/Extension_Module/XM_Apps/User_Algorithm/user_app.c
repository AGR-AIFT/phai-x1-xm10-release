/**
 ******************************************************************************
 * @file    user_app.c
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 18, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"      // 통합 API 헤더

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

/* --- 전역 변수 및 핸들 선언 --- */
static XmTsmHandle_t s_user_tsm;

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

/* =================================================================
 * [필수 구현 1] 초기화 함수 예시 (User_Setup)
 * - 전원 인가 후 딱 한 번 실행됩니다.
 * - TSM 생성, 변수 초기화, LED/Button 초기 설정 등을 수행합니다.
 * - 함수 이름을 수정하거나 삭제하면 동작하지 않습니다.
 * - 함수 이름을 수정하거나 삭제하면 동작하지 않습니다.
 * ================================================================= */
void User_Setup(void)
{
    // 1. TSM 생성 (초기 상태: OFF)
    s_user_tsm = XM_TSM_Create(XM_STATE_OFF);

    // 2. 상태 등록
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
 * [필수 구현 2] 반복 루프 함수 예시 (User_Loop)
 * - 2ms(500Hz) 주기로 계속 호출됩니다.
 * - 제어 알고리즘, TSM 실행 로직을 여기에 작성합니다.
 * - 내부 IPO(Input-Process-Output)모델이 적용되어 있습니다.
 * - xm_api_data.h를 통해 RxData에 대해 확인할 수 있습니다.
 * - XM_SetControlMode함수를 Setup에서 실시간 제어 모드 / 모니터링 모드를 선택해야 합니다. (기본 모니터링 모드)
 * - 모니터링 모드에서는 H10으로 제어 명령을 전송하지 않습니다. 
 * - 실시간 제어 모드에서는 H10으로 Torque input을 2ms 주기로 전송합니다.
 * - 함수 이름을 수정하거나 삭제하면 동작하지 않습니다.
 * ================================================================= */
void User_Loop(void)
{
    // TSM 실행 (현재 상태에 맞는 Loop 함수가 실행됨)
    XM_TSM_Run(s_user_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* --- 상태별 동작 함수 정의 예시 --- */
static void Off_Entry(void)
{

}

static void Off_Loop(void)
{

}

static void Standby_Entry(void)
{

}

static void Standby_Loop(void)
{
    // ... 대기 로직 ...
}

static void Active_Entry(void)
{

}

static void Active_Loop(void)
{

}

static void Active_Exit(void)
{

}
