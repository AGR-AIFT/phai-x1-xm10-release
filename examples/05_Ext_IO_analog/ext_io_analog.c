/**
 ******************************************************************************
 * @file    ext_io_analog.c
 * @author  HyundoKim
 * @brief   [초급] 아날로그 전압 측정 — XM ADC 입문
 * @details
 * [학습 목표]
 *   - 고정 아날로그 핀(ADC_1~4)에서 전압을 읽는 가장 기본적인 방법
 *   - XM_AnalogReadMillivolts()로 밀리볼트 단위 직관적 전압 읽기
 *   - 임계치(Threshold) 기반 LED 제어
 *
 * [하드웨어 연결]
 *   - ADC_1 (PA0): 가변저항 또는 센서 출력 연결
 *     3.3V ── 가변저항 ── ADC_1 핀
 *                  └── GND
 *
 * [시리즈 안내]
 *   이 예제는 XM ADC 튜토리얼 시리즈의 첫 번째입니다:
 *   - 05   (본 예제) : 고정 ADC 핀 전압 읽기 (기초)
 *   - 05a  : DIO→ADC 전환으로 FSR 센서 읽기
 *   - 05b  : FSR 8채널 일괄 전환 + Resolution 설정
 *   - 05c  : 고정 ADC + DIO→ADC 혼합 사용 (12핀 전체)
 *   - 05d  : DIO/ADC 혼합 모드 — 버튼 + LED + FSR 실전 시나리오
 *
 * @version 1.1
 * @date    Mar 09, 2026
 * @see docs/api-reference/04-external-io.md
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

void User_Setup(void)
{
    // TSM 설정...
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
    uint16_t mv = XM_AnalogReadMillivolts(XM_EXT_ADC_1);

    if (mv > 2000) {
        XM_SetLedState(XM_LED_1, XM_ON);
    } else {
        XM_SetLedState(XM_LED_1, XM_OFF);
    }
}