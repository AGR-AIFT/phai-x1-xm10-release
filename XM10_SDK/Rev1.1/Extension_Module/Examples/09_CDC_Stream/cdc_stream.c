/**
 ******************************************************************************
 * @file    cdc_stream.c
 * @author  HyundoKim
 * @brief   [예제] USB-CDC 실시간 데이터 스트리밍 (Total Data + 사용자 구조체)
 * @details
 * USB-CDC를 통해 PC 로 센서 데이터(Total Data)와 알고리즘 출력(사용자 구조체)을 전송합니다.
 * PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 xm10 도구로 보고 저장하세요.
 * xm10 PC 도구 (docs/getting-started/04-pc-data-tool.md)
 *
 * [USB-CDC 스트림 구조]
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Module ID 0x20  │ Total Data Packet  │ XM10 자동 (1ms 주기)   │
 * │  Module ID 0xEF  │ User Meta (JSON)   │ XM10 — 접속 시 1회     │
 * │  Module ID 0xF0  │ User Custom Data   │ Control_Loop에서 호출  │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * [Total Data Packet (0x20)]
 * - 365B 구조체(H10 관절각/토크, GRF, IMU Hub, External IO 등)를 1kHz 자동 전송
 * - 사용자 코드 불필요 — PC 프로그램(PhAI Studio, xm10 도구)이 포트를 열면 자동 수신됨
 *
 * [User Custom (0xF0~0xFE)]
 * - 알고리즘 디버그 채널을 추가하고 싶을 때 사용
 * - Control_Setup에서 채널 메타데이터(이름/단위) JSON 등록
 * - Control_Loop에서 XM_SendUsbDataWithId()로 float[] 전송
 *
 * @warning PC 프로그램(PhAI Studio, xm10 도구, PuTTY 등)은 한 번에 하나만 연결하세요.
 *          이 예제는 xm10 도구 단독 실행을 전제로 합니다.
 *
 * @version 3.0  (Total Data Packet + User Custom API 적용)
 * @date    Mar 10, 2026
 *
 * @see     docs/api-reference/05-usb-connectivity.md
 * @see     docs/getting-started/04-pc-data-tool.md  (xm10 PC 도구)
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

void Control_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
    XM_TSM_AddState(s_tsm, &conf);

    /*
     * [1] Total Data Packet (Module ID 0x20) — 사용자 코드 불필요
     *
     * XM10 이 H10 데이터(관절각/토크/IMU), GRF, IMU Hub, External IO 등
     * 365B를 PC 프로그램이 포트를 열면 자동으로 1kHz 스트리밍합니다.
     * PhAI Studio에서 0x20 채널을 선택하면 즉시 모니터링 가능합니다.
     *
     * → 아무 코드도 필요 없음.
     */

    /*
     * [2] User Custom Data (Module ID 0xF0) — 선택적 추가 채널
     *
     * Total Data에 없는 알고리즘 변수를 추가로 전송할 때 사용합니다.
     * Control_Setup에서 채널 이름/단위를 JSON으로 등록하면 xm10 도구에 채널 이름으로 표시됩니다.
     * 채널 이름은 프로그램이 연결될 때 한 번 전달됩니다. 이름이 안 보이면 USB 케이블을 다시 꽂고 연결하세요.
     * (JSON 은 512바이트까지, 등록은 Module ID 1개만 가능)
     * PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 xm10 도구로 보고 저장하세요.
     */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"H10 Connected\",\"unit\":\"bool\"},"
        "{\"name\":\"Left Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Right Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Forward Velocity\",\"unit\":\"m/s\"}]");
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
    /*
     * [User Custom 채널 전송 예시]
     *
     * 알고리즘 내부 변수를 float[] 배열에 담아 전송합니다.
     * Total Data(0x20)는 XM10 이 처리하므로 여기서 별도 전송 불필요.
     *
     * 주의:
     *   - XM_SendUsbDataWithId()는 non-blocking입니다.
     *   - 버퍼 풀이 가득 차면 false를 반환하며 해당 tick은 드롭됩니다.
     *   - 매 tick 호출 불필요 — 필요 시에만 호출해도 됩니다.
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
     * 채널 이름(XM_SetUsbCustomMeta)은 슬롯이 하나뿐이라 Module ID 1개(위 예제의 0xF0)만 등록됩니다.
     * 0xF1 데이터는 xm10 도구에 이름 없이 ch0, ch1 … 로 표시됩니다.
     */
}
