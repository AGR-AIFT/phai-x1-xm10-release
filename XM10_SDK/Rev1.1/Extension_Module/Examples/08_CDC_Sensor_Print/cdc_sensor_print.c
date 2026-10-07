/**
 ******************************************************************************
 * @file    cdc_sensor_print.c
 * @author  HyundoKim
 * @brief   [중급] snprintf를 활용한 센서 데이터 모니터링
 * @note    텍스트 기반 디버깅 예제입니다. 터미널/콘솔 확인용으로 적합합니다.
 *          기본 설정에서는 PC 프로그램이 포트를 열면 센서 데이터(Total Data)도 자동으로
 *          함께 전송되어, 일반 시리얼 터미널에 알아볼 수 없는 글자가 섞여 보입니다.
 *          텍스트만 보려면 Control_Setup 에서 XM_SetUsbAutoStream(false) 를 호출하세요.
 *          구조체 실시간 그래프(xm10 도구)는 09_CDC_Stream 을 참조하세요.
 * @warning PC 프로그램(PhAI Studio, xm10 도구, PuTTY 등)은 한 번에 하나만 연결하세요.
 * @version 1.2
 * @date    Mar 10, 2026
 *
 * @see     docs/api-reference/05-usb-connectivity.md
 * @see     docs/api-reference/02-h10-control-n-data.md
 * @see     Extension_Module/Examples/09_CDC_Stream/cdc_stream.c
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <stdio.h> /* snprintf 사용 (버퍼 오버런 방지) */

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

void Control_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
    XM_TSM_AddState(s_tsm, &conf);
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

static void Run_Loop(void)
{
    static uint32_t last_print_time = 0;
    uint32_t now = XM_GetTick();

    /* 500ms마다 실행 (논블로킹 타이머) */
    if (now - last_print_time >= 500) {
        last_print_time = now;

        float angle_rh = XM.status.h10.rightHipAngle;
        float angle_lh = XM.status.h10.leftHipAngle;

        /* 문자열 포맷팅 (실수형 출력) — snprintf 로 버퍼 오버런 방지 */
        char buf[64];
        snprintf(buf, sizeof(buf), "Hip Angles -> RH: %.2f, LH: %.2f\r\n", angle_rh, angle_lh);

        /* 전송 */
        XM_SendUsbDebugMessage(buf);
    }
}
