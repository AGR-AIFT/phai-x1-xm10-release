/**
 ******************************************************************************
 * @file    cdc_stream.c
 * @author  HyundoKim
 * @brief   [예제] PhAI V2 프로토콜 USB-CDC 스트리밍
 * @details
 * PC(PhAI Studio 또는 Python)로 센서 데이터를 실시간 스트리밍합니다.
 *
 * [핵심 개념]
 * - User는 전송할 데이터 구조체(payload)만 정의합니다.
 * - SOF / SEQ_ID / MODULE_ID / CRC8은 System이 자동으로 래핑합니다.
 * - PhAI Studio는 USB 연결 시 자동으로 데이터를 수신합니다 (Auto-Stream).
 *
 * [사용법]
 * 1. MyStreamData_t 구조체를 원하는 float 필드로 정의
 * 2. XM_SetUsbStreamSource()로 등록
 * 3. User_Loop()에서 데이터 갱신 → XM_SendUsbData()로 전송
 *
 * @version 2.1  (PhAI V2 프로토콜 적용)
 * @date    Mar 09, 2026
 *
 * @see     docs/api-reference/05-usb-connectivity.md
 * @see     docs/api-reference/02-h10-control-n-data.md
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
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

/*
 * 전송할 데이터 구조체 (User가 자유롭게 정의)
 *
 * PhAI Studio COMBINED 모드(MODULE_ID=0x10)와 호환하려면
 * 10개 float (Accel XYZ, Gyro XYZ, Motor Angle L/R, Motor Torque L/R) 순서로 배치.
 *
 * User Custom 모드(MODULE_ID=0xF0~0xFE)에서는 어떤 float 배열이든 가능.
 */
typedef struct {
    float accel[3];        /* Accelerometer X, Y, Z (m/s²)  */
    float gyro[3];         /* Gyroscope X, Y, Z (rad/s)     */
    float motor_angle[2];  /* Motor Angle Left, Right (deg)  */
    float motor_torque[2]; /* Motor Torque Left, Right (Nm)  */
} PhAI_CombinedData_t;    /* 40 bytes = 10 × float32        */

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

static PhAI_CombinedData_t s_streamData;
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

    /* PhAI V2: 데이터 소스 등록 (Auto-Stream 시 매 루프 자동 전송) */
    XM_SetUsbStreamSource(&s_streamData, sizeof(s_streamData));

    /* Module ID 설정 (COMBINED = PhAI Studio 기본 10ch 모드) */
    XM_SetUsbStreamModuleId(PHAI_MODULE_COMBINED);

    /*
     * [선택] Auto-Stream 비활성화 시 (Legacy Python 호환):
     * XM_SetUsbAutoStream(false);
     * → 이 경우 PC에서 "AGRB MON START" 전송 후에만 스트리밍 시작
     */
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
    /*
     * XM.status에서 실제 센서 데이터를 조합하여 스트리밍 구조체에 매핑.
     * 연결되지 않은 모듈의 데이터는 0.0f (이전 값 유지 대신 명시적 초기화).
     */

    /* IMU (from H10) */
    if (XM.status.h10.is_connected) {
        s_streamData.accel[0] = XM.status.h10.leftHipImuGlobalAccX;
        s_streamData.accel[1] = XM.status.h10.leftHipImuGlobalAccY;
        s_streamData.accel[2] = XM.status.h10.leftHipImuGlobalAccZ;

        s_streamData.gyro[0] = XM.status.h10.leftHipImuGlobalGyrX;
        s_streamData.gyro[1] = XM.status.h10.leftHipImuGlobalGyrY;
        s_streamData.gyro[2] = XM.status.h10.leftHipImuGlobalGyrZ;

        s_streamData.motor_angle[0]  = XM.status.h10.leftHipMotorAngle;
        s_streamData.motor_angle[1]  = XM.status.h10.rightHipMotorAngle;
        s_streamData.motor_torque[0] = XM.status.h10.leftHipTorque;
        s_streamData.motor_torque[1] = XM.status.h10.rightHipTorque;
    }

    /*
     * 명시적 전송 (선택사항)
     * Auto-Stream이 켜져 있으면 XM_USB_ProcessPeriodic()에서 자동 전송되므로
     * 아래 호출은 불필요하지만, 수동 제어가 필요한 경우 사용 가능.
     *
     * XM_SendUsbData(&s_streamData, sizeof(s_streamData));
     */
}
