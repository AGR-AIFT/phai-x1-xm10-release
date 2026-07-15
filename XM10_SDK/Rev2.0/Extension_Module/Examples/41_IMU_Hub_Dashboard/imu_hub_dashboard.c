/**
 ******************************************************************************
 * @file    imu_hub_dashboard.c
 * @author  HyundoKim
 * @brief   [고급] IMU Hub 6축 자세 대시보드 — 쿼터니언→오일러 변환 후 실시간 스트리밍
 * @details
 * IMU Hub Module(EBIMU-9DOFV6 × 6, FDCAN2 DOP V3)에서 최대 6개 IMU 의 방위(쿼터니언)를
 * 받아, 사람이 직관적으로 읽는 **오일러 각(roll/pitch/yaw)** 으로 on-device 변환해
 * PhAI Studio User Custom(0xF0) 채널로 스트리밍합니다.
 *
 * ============================================================
 *  왜 쿼터니언에서 계산하는가 (0x20 Total Data 와의 차이)
 * ============================================================
 * IMU Hub 가 XM 으로 보내는 것은 각 센서의 **쿼터니언(q_w,q_x,q_y,q_z)** 이며
 * (Total Data 0x20 에도 쿼터니언 형태로 자동 포함됨), roll/pitch/yaw 형태로는 오지 않습니다.
 * 따라서 본 예제는 XM.status.imu_hub.sensor[i] 의 **live 쿼터니언에서 오일러 각을 직접 계산**해
 * 스트리밍합니다 — 쿼터니언→오일러 변환은 자세 표시의 기본 기술입니다. (센서/모터 불필요, 관찰형)
 *
 * ============================================================
 *  동작
 * ============================================================
 *  - connected_mask(bit0~5)로 0~6개 IMU 연결을 자동 감지. 미연결 슬롯은 0 으로 전송.
 *  - 0xF0 : IMU0~5 의 roll/pitch/yaw = 18채널 (센서 6 × 3축)
 *    ※ XM_SetUsbCustomMeta 는 USB 연결당 Module ID 1개만 라벨 등록 가능하므로
 *      6센서를 하나의 0xF0 채널 그룹으로 묶어 전송합니다.
 *  - LED1 : 연결된 IMU 수가 많을수록 빠르게 점멸 (0개 = 느린 대기 점멸)
 *  - 스트리밍은 50Hz(20ms)로 스로틀 — 자세 관찰에 충분하고 USB 대역을 절약
 *
 * @note IMU Hub 데이터 원본은 FDCAN2 TPDO → core_process 가 XM.status.imu_hub 의 쿼터니언을
 *       자동 갱신하므로, 사용자 코드는 XM.status.imu_hub 를 읽기만 하면 됩니다 (드라이버 직접 호출 불필요).
 * @warning **본 예제는 XM10 Rev 2.0 전용입니다.** IMU Hub Module 은 FDCAN2 센서허브(Rev 2.0)로
 *          연결됩니다. Rev 1.1 은 FDCAN2 센서허브를 지원하지 않아 XM.status.imu_hub.is_connected 가
 *          항상 false 이며 데이터가 수신되지 않습니다. 참고: docs/hardware/README.md (보드 리비전 비교)
 *
 * @see     Ex.09 cdc_stream.c (User Custom 0xF0 스트리밍)
 * @see     Ex.16 TinyAI_Sensor_Fusion (IMU Hub 데이터를 제어/AI 에 활용)
 * @version 1.0
 * @date    2026-07-15
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>   /* atan2f, asinf */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define IMU_STREAM_PERIOD_MS   20U   /* 스트리밍 주기 (50Hz) — 자세 관찰용 */
#define IMU_AXES_PER_SENSOR    3U    /* roll, pitch, yaw */
#define IMU_TOTAL_CH           (XM_IMU_HUB_SENSOR_COUNT * IMU_AXES_PER_SENSOR)  /* 6 × 3 = 18 */
#define IMU_MODULE_ID          0xF0U

#define RAD_TO_DEG             57.2957795130823f   /* 180 / π */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/* [IMU0 r,p,y, IMU1 r,p,y, ... IMU5 r,p,y] */
static float    s_posture[IMU_TOTAL_CH];
static uint32_t s_last_stream_tick;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static uint8_t _CountConnected(uint8_t mask);
static void    _QuatToEulerDeg(const XmImuHubSensor_t* s, float* roll, float* pitch, float* yaw);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void Control_Setup(void)
{
    /* PhAI Studio 그래프 라벨 등록 (18채널) — 문자열 리터럴이라 수명 내내 유효.
     * XM_SetUsbCustomMeta 는 USB 연결당 1개 Module ID 만 등록되므로 6센서를 0xF0 하나로 묶는다. */
    XM_SetUsbCustomMeta(IMU_MODULE_ID,
        "[{\"name\":\"IMU0 Roll\",\"unit\":\"deg\"},{\"name\":\"IMU0 Pitch\",\"unit\":\"deg\"},{\"name\":\"IMU0 Yaw\",\"unit\":\"deg\"},"
        "{\"name\":\"IMU1 Roll\",\"unit\":\"deg\"},{\"name\":\"IMU1 Pitch\",\"unit\":\"deg\"},{\"name\":\"IMU1 Yaw\",\"unit\":\"deg\"},"
        "{\"name\":\"IMU2 Roll\",\"unit\":\"deg\"},{\"name\":\"IMU2 Pitch\",\"unit\":\"deg\"},{\"name\":\"IMU2 Yaw\",\"unit\":\"deg\"},"
        "{\"name\":\"IMU3 Roll\",\"unit\":\"deg\"},{\"name\":\"IMU3 Pitch\",\"unit\":\"deg\"},{\"name\":\"IMU3 Yaw\",\"unit\":\"deg\"},"
        "{\"name\":\"IMU4 Roll\",\"unit\":\"deg\"},{\"name\":\"IMU4 Pitch\",\"unit\":\"deg\"},{\"name\":\"IMU4 Yaw\",\"unit\":\"deg\"},"
        "{\"name\":\"IMU5 Roll\",\"unit\":\"deg\"},{\"name\":\"IMU5 Pitch\",\"unit\":\"deg\"},{\"name\":\"IMU5 Yaw\",\"unit\":\"deg\"}]");

    s_last_stream_tick = XM_GetTick();
}

void Control_Loop(void)
{
    const XmImuHubData_t* hub = &XM.status.imu_hub;

    /* --- LED1: 연결된 IMU 개수를 점멸 속도로 표시 --- */
    if (!hub->is_connected) {
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 1000);   /* 허브 미연결 → 느린 대기 */
    } else {
        uint8_t n = _CountConnected(hub->connected_mask);
        /* 1개=520ms ... 6개=120ms (많을수록 빠르게) */
        uint32_t period = (n > 0U) ? (600U - (uint32_t)n * 80U) : 1000U;
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, period);
    }

    /* --- 50Hz 스로틀 스트리밍 --- */
    uint32_t now = XM_GetTick();
    if ((now - s_last_stream_tick) < IMU_STREAM_PERIOD_MS) {
        return;
    }
    s_last_stream_tick = now;

    /* 각 IMU 쿼터니언 → 오일러(deg). 미연결 슬롯은 0 으로 채워 그래프가 평평. */
    for (uint8_t i = 0U; i < XM_IMU_HUB_SENSOR_COUNT; i++) {
        float* slot = &s_posture[i * IMU_AXES_PER_SENSOR];
        if (hub->connected_mask & (uint8_t)(1U << i)) {
            _QuatToEulerDeg(&hub->sensor[i], &slot[0], &slot[1], &slot[2]);
        } else {
            slot[0] = 0.0f;
            slot[1] = 0.0f;
            slot[2] = 0.0f;
        }
    }

    XM_SendUsbDataWithId(s_posture, sizeof(s_posture), IMU_MODULE_ID);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/** @brief connected_mask(bit0~5)에서 연결된 IMU 개수를 센다. */
static uint8_t _CountConnected(uint8_t mask)
{
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < XM_IMU_HUB_SENSOR_COUNT; i++) {
        if (mask & (uint8_t)(1U << i)) {
            n++;
        }
    }
    return n;
}

/**
 * @brief 단위 쿼터니언(w,x,y,z) → 오일러 각(deg). Aerospace ZYX (roll-pitch-yaw).
 *        pitch 는 asin 정의역([-1,1])으로 포화시켜 gimbal 근처에서도 NaN 을 방지.
 */
static void _QuatToEulerDeg(const XmImuHubSensor_t* s, float* roll, float* pitch, float* yaw)
{
    float w = s->q_w, x = s->q_x, y = s->q_y, z = s->q_z;

    /* roll (x축 회전) */
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    *roll = atan2f(sinr_cosp, cosr_cosp) * RAD_TO_DEG;

    /* pitch (y축 회전) — asin 정의역 포화 */
    float sinp = 2.0f * (w * y - z * x);
    if (sinp >  1.0f) sinp =  1.0f;
    if (sinp < -1.0f) sinp = -1.0f;
    *pitch = asinf(sinp) * RAD_TO_DEG;

    /* yaw (z축 회전) */
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    *yaw = atan2f(siny_cosp, cosy_cosp) * RAD_TO_DEG;
}
