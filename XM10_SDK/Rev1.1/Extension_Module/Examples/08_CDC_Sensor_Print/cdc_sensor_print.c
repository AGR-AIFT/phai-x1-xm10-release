/**
 ******************************************************************************
 * @file    cdc_sensor_print.c
 * @author  HyundoKim
 * @brief   [중급] sprintf를 활용한 센서 데이터 모니터링
 * @note    텍스트 기반 디버깅 예제입니다. 터미널/콘솔 확인용으로 적합합니다.
 *          PhAI Studio 실시간 그래프 모니터링은 09_CDC_Stream 예제를 참조하세요.
 *          Total Data Packet(0x20)은 System이 자동 전송하므로 별도 코드 불필요.
 * @warning USB-CDC 포트는 단일 점유 자원입니다. 다른 시리얼 클라이언트
 *          (PhAI Studio, PuTTY, RealTerm 등)와 동시에 열지 마십시오 —
 *          같은 COM 포트 충돌로 접속 실패 또는 데이터 손실이 발생합니다.
 *          실시간 그래프 모니터링이 필요하면 PhAI Studio 만 단독 실행하세요.
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
#include <stdio.h> /* sprintf 사용 */

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
