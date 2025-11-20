/**
 ******************************************************************************
 * @file    cdc_stream.c
 * @author  HyundoKim
 * @brief   [고급] 자동화된 바이너리 스트리밍 (그래프 시각화용)
 * @version 0.1
 * @date    Nov 18, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>

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

// 전송할 데이터 구조체 (바이너리 패킷)
typedef struct {
    float time_sec;
    float sine_wave;
    float real_angle;
} MyGraphData_t;

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

static MyGraphData_t graphData;
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

    /* 데이터 소스 등록 */
    XM_SetUsbStreamSource(&graphData, sizeof(MyGraphData_t));
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
    // 1. 데이터 업데이트 (2ms마다 호출됨)
    float t = (float)XM_GetTick() / 1000.0f;
    
    graphData.time_sec  = t;
    graphData.sine_wave = 10.0f * sinf(2.0f * 3.14159f * t); // 가상의 사인파
    graphData.real_angle = XM.status.h10.rightHipAngle;      // 실제 센서값
    
    XM_SendUsbData(&graphData, sizeof(graphData));
}
