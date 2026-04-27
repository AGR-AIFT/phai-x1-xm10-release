/**
 ******************************************************************************
 * @file    cdc_stream.c
 * @author  HyundoKim
 * @brief   [예제] PhAI Studio 실시간 데이터 스트리밍
 * @details
 * USB-CDC를 통해 PhAI Studio로 센서 데이터와 알고리즘 출력을 전송합니다.
 *
 * [USB-CDC 스트림 구조]
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Module ID 0x20  │ Total Data Packet  │ System 자동 (1ms 주기) │
 * │  Module ID 0xEF  │ User Meta (JSON)   │ System — 연결 시 1회   │
 * │  Module ID 0xF0  │ User Custom Data   │ User_Loop에서 호출     │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * [Total Data Packet (0x20)]
 * - 425B 구조체(H10 PDO, GRF, IMU Hub, External IO 등)를 1kHz 자동 전송
 * - 사용자 코드 불필요 — PhAI Studio 연결만 하면 자동 수신됨
 *
 * [User Custom (0xF0~0xFE)]
 * - 알고리즘 디버그 채널을 추가하고 싶을 때 사용
 * - User_Setup에서 채널 메타데이터(이름/단위) JSON 등록
 * - User_Loop에서 XM_SendUsbDataWithId()로 float[] 전송
 *
 * @version 3.0  (Total Data Packet + User Custom API 적용)
 * @date    Mar 10, 2026
 *
 * @see     docs/api-reference/05-usb-connectivity.md
 * @see     docs/total_data_packet/02_User_Custom_API_Guide.md
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* User Custom 채널 수 (float 기준, 권장 최대 10개) */
#define USER_CH_COUNT   4U

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief User Custom 알고리즘 디버그 채널
 *
 * Total Data(0x20)에 없는 알고리즘 내부 변수를 추가 모니터링.
 * 여기서는 H10 연결 상태, 좌우 고관절 각도, 보행 위상을 표시합니다.
 */
typedef struct {
    float is_connected;     /* H10 연결 여부 (1.0=연결, 0.0=미연결)   */
    float left_hip_angle;   /* 좌측 고관절 각도 (deg)                  */
    float right_hip_angle;  /* 우측 고관절 각도 (deg)                  */
    float forward_velocity; /* 전방 보행 속도 (m/s)                    */
} UserDebugData_t;          /* 16 bytes = 4 × float32                  */

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

static UserDebugData_t s_debug;
static XmTsmHandle_t   s_tsm;

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

    /*
     * [1] Total Data Packet (Module ID 0x20) — 사용자 코드 불필요
     *
     * System이 H10 PDO(관절각/토크/IMU), GRF, IMU Hub, External IO 등
     * 425B를 USB 연결 시 자동으로 1kHz 스트리밍합니다.
     * PhAI Studio에서 0x20 채널을 선택하면 즉시 모니터링 가능합니다.
     *
     * → 아무 코드도 필요 없음.
     */

    /*
     * [2] User Custom Data (Module ID 0xF0) — 선택적 추가 채널
     *
     * Total Data에 없는 알고리즘 변수를 추가로 전송할 때 사용합니다.
     * User_Setup에서 채널 이름/단위를 JSON으로 등록하면
     * PhAI Studio에 "User Custom" 그룹으로 자동 표시됩니다.
     */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"H10 Connected\",\"unit\":\"bool\"},"
        "{\"name\":\"Left Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Right Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Forward Velocity\",\"unit\":\"m/s\"}]");
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
     * [User Custom 채널 전송 예시]
     *
     * 알고리즘 내부 변수를 float[] 배열에 담아 전송합니다.
     * Total Data(0x20)는 System이 처리하므로 여기서 별도 전송 불필요.
     *
     * 주의:
     *   - XM_SendUsbDataWithId()는 non-blocking입니다.
     *   - 버퍼 풀이 가득 차면 false를 반환하며 해당 tick은 드롭됩니다.
     *   - 매 tick 호출 불필수 — 필요 시에만 호출해도 됩니다.
     */

    /* H10 연결 시 최신 값 업데이트 */
    s_debug.is_connected    = XM.status.h10.is_connected ? 1.0f : 0.0f;
    s_debug.left_hip_angle  = XM.status.h10.leftHipAngle;
    s_debug.right_hip_angle = XM.status.h10.rightHipAngle;
    s_debug.forward_velocity = XM.status.h10.forwardVelocity;

    /* Module ID 0xF0으로 User Custom 데이터 전송 */
    XM_SendUsbDataWithId(&s_debug, sizeof(s_debug), 0xF0);

    /*
     * [다중 채널 예시]
     * 여러 알고리즘 모듈 데이터를 독립 Module ID로 분리 전송 가능:
     *
     *   float control_data[2] = { kp_output, kd_output };
     *   XM_SendUsbDataWithId(control_data, sizeof(control_data), 0xF1);
     *
     * 단, Module ID가 늘어날수록 USB 대역폭이 추가 소모됩니다.
     * 2~3개 이상 사용 시 드롭 여부를 모니터링하세요.
     */
}
