/**
 ******************************************************************************
 * @file    ext_io_basic.c
 * @author  HyundoKim
 * @brief   [초급] 외부 스위치와 LED를 연결하여 직접 제어하기
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
    // [설정] 핀 모드 초기화
    // Pin 3: 스위치 입력 (풀업 저항 활성화 -> 별도 저항 불필요!)
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP);

    // Pin 4: LED 제어용 출력
    XM_SetPinMode(XM_EXT_DIO_4, XM_EXT_DIO_MODE_OUTPUT);

    // TSM 생성 및 등록
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = {
    	.id = XM_STATE_USER_START,
		.on_loop = Run_Loop
    };
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
    // 1. 외부 스위치 상태 읽기 (Pin 3)
    // 내부 풀업(Input Pullup)을 사용했으므로, 스위치를 누르면 GND(LOW)와 연결됩니다.
    XmLogicLevel_t switch_state = XM_DigitalRead(XM_EXT_DIO_3);

    // 2. 상태에 따라 외부 LED 제어 (Pin 4)
    if (switch_state == XM_LOW) {
        // 스위치가 눌림 -> LED 켜기 (3.3V 출력)
        XM_DigitalWrite(XM_EXT_DIO_4, XM_HIGH);
    } else {
        // 스위치가 떨어짐 -> LED 끄기 (0V 출력)
        XM_DigitalWrite(XM_EXT_DIO_4, XM_LOW);
    }
}
