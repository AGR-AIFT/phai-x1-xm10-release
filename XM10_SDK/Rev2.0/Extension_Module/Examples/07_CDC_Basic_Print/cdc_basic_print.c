/**
 ******************************************************************************
 * @file    cdc_basic_print.c
 * @author  HyundoKim
 * @brief   [초급] 버튼 이벤트 발생 시 텍스트 메시지 전송
 * @note    텍스트 디버깅 전용 예제입니다. Control_Setup 에서 호스트 프로파일을
 *          TERMINAL 로 지정하여 1kHz Total Data auto-pump 를 끄고, 일반 시리얼
 *          터미널(Tera Term · VS Code Serial Monitor · PuTTY 등)에 깨끗한 텍스트만
 *          출력합니다. 실시간 구조체 그래프 모니터링은 09_CDC_Stream(PhAI Studio)을
 *          참조하세요.
 * @warning USB-CDC 포트는 단일 점유 자원입니다. 다른 시리얼 클라이언트
 *          (PhAI Studio, PuTTY, RealTerm 등)와 동시에 열지 마십시오 —
 *          같은 COM 포트 충돌로 접속 실패 또는 데이터 손실이 발생합니다.
 *          실시간 그래프 모니터링이 필요하면 PhAI Studio 만 단독 실행하세요.
 * @version 1.2
 * @date    Mar 10, 2026
 *
 * @see     docs/api-reference/05-usb-connectivity.md
 * @see     Extension_Module/Examples/09_CDC_Stream/cdc_stream.c
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
    /* 이 예제는 일반 시리얼 터미널(Tera Term · VS Code Serial Monitor · PuTTY 등)로
     * 텍스트를 확인하는 예제입니다. 호스트 프로파일을 TERMINAL 로 지정하면 1kHz
     * Total Data auto-pump 가 꺼져 터미널에 깨끗한 텍스트만 출력됩니다.
     * (미지정 시 기본 PhAI Studio 스트리밍 → 터미널에 바이너리가 섞여 보임) */
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
    /* 버튼 1 클릭 이벤트 감지 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        /* 단순 문자열 전송 */
        XM_SendUsbDebugMessage("Hello! Button 1 was clicked.\r\n");

        /* LED 깜빡임으로 반응 확인 */
        XM_SetLedEffect(XM_LED_1, XM_LED_ONESHOT, 100);
    }
}