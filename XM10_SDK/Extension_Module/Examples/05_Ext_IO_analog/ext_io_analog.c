/**
 ******************************************************************************
 * @file    ext_io_analog.c
 * @author  HyundoKim
 * @brief   [중급] 아날로그 센서 전압 측정 및 임계치(Threshold) 반응
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
    // 1. 아날로그 전압 읽기 (0.0V ~ 3.3V)
    // 복잡한 ADC Raw 값(0~65535) 대신 직관적인 전압(V)을 사용합니다.
    float raw = XM_AnalogRead(XM_EXT_ADC_1);
    float voltage = 3.3f * ((float)raw / 65535.0f); // 16 bit resolution 기준: 3.3v * (raw/65535.0)

    // 2. 임계치 판단 (Threshold Logic)
    if (voltage > 2.0f) {
        // 2.0V 초과 시 -> 내부 LED 1번 켜기 (경고)
        XM_SetLedState(XM_LED_1, XM_ON);
    } else {
        // 정상 범위 -> 끄기
        XM_SetLedState(XM_LED_1, XM_OFF);
    }
}