/**
 ******************************************************************************
 * @file    cdc_sensor_print.c
 * @author  HyundoKim
 * @brief   [중급] snprintf를 활용한 센서 데이터 모니터링
 * @note    텍스트 기반 디버깅 예제입니다. Control_Setup 에서 호스트 프로파일을
 *          TERMINAL 로 지정하여 1kHz Total Data auto-pump 를 끄고, 일반 시리얼
 *          터미널/콘솔(Tera Term · VS Code Serial Monitor · PuTTY 등)에 깨끗한
 *          텍스트만 출력합니다. PhAI Studio 실시간 그래프는 09_CDC_Stream 을 참조하세요.
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
    /* 텍스트 터미널 확인 예제 — 호스트 프로파일 TERMINAL 지정으로 1kHz Total Data
     * auto-pump 를 꺼 깨끗한 텍스트만 출력합니다. (미지정 시 기본 PhAI Studio 스트리밍) */
    XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL);

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
