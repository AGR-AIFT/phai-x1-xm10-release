/**
 ******************************************************************************
 * @file    cdc_sensor_print.c
 * @author  HyundoKim
 * @brief   [중급] sprintf를 활용한 센서 데이터 모니터링
 * @version 0.1
 * @date    Nov 18, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <stdio.h> // sprintf 사용

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

static void Run_Loop(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
    XM_TSM_AddState(s_tsm, &conf);
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

static void Run_Loop(void)
{
    static uint32_t last_print_time = 0;
    uint32_t now = XM_GetTick();

    // 500ms마다 실행 (Non-blocking Timer)
    if (now - last_print_time >= 500) {
        last_print_time = now;

        float angle_rh = XM.status.h10.rightHipAngle;
        float angle_lh = XM.status.h10.leftHipAngle;

        // 문자열 포맷팅 (실수형 출력)
        char buf[64];
        sprintf(buf, "Hip Angles -> RH: %.2f, LH: %.2f\r\n", angle_rh, angle_lh);
        
        // 전송
        XM_SendUsbDebugMessage(buf);
    }
}
