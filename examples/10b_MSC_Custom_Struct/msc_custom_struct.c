/**
 ******************************************************************************
 * @file    msc_custom_struct.c
 * @author  HyundoKim
 * @brief   [중급] 사용자 정의 구조체 + 수동 타임스탬프 + 상세 메타데이터
 * @details
 * [학습 목표]
 *   - 다양한 타입을 혼합한 구조체 설계 (uint32_t, float, uint8_t, bool)
 *   - 자동 타임스탬프를 비활성화하고 직접 tick을 관리하는 방법
 *   - 메타데이터를 상세하게 기술하는 방법 (Python 디코더가 파싱 가능)
 *
 * [바이너리 레코드 포맷]
 *   auto_timestamp=OFF이므로:
 *   [Header:4][user_data:26] = 30 bytes/record
 *
 * [구조체 설계 팁]
 *   - 필드를 4-byte 정렬로 배치하면 패딩 없이 최적 크기
 *   - uint32_t/float는 4바이트, uint8_t/bool은 1바이트
 *   - 아래 구조체는 packed 없이 설계하여 자연 정렬 유지
 *
 * @version 1.1
 * @date    Mar 09, 2026
 * @see     docs/api-reference/05-usb-connectivity.md
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef struct {
    uint32_t tick_ms;
    float    hip_angle_L;
    float    hip_angle_R;
    float    accel_x;
    float    accel_y;
    float    accel_z;
    uint8_t  gait_phase;
    uint8_t  _pad[3];
} SensorSnapshot_t;  /* 28 bytes (4-byte aligned, no padding issues) */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static SensorSnapshot_t s_snap;
static bool s_is_logging = false;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    XM_SetUsbLogSource(&s_snap, sizeof(SensorSnapshot_t));

    /* 구조체에 tick_ms를 직접 포함하므로 자동 타임스탬프 비활성화 */
    XM_SetUsbLogAutoTimestamp(false);
}

void User_Loop(void)
{
    if (!s_is_logging && XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogReady()) {
            /*
             * 메타데이터: 각 필드의 타입과 오프셋을 명시하면
             * Python 디코더가 STRUCT_FMT를 자동 생성할 수 있습니다.
             *
             * 포맷: "필드명(타입), ..." — 한 줄로 작성
             * 타입: uint32_t, float, uint8_t, bool, int16_t 등
             */
            s_is_logging = XM_StartUsbDataLog(
                "SensorCapture",
                "tick_ms(uint32_t), "
                "hip_angle_L(float), hip_angle_R(float), "
                "accel_x(float), accel_y(float), accel_z(float), "
                "gait_phase(uint8_t), _pad(3bytes)"
            );
            if (s_is_logging) {
                XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
            }
        }
    }

    if (s_is_logging && XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_StopUsbDataLog();
        s_is_logging = false;
        XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0);
    }

    if (s_is_logging) {
        s_snap.tick_ms     = XM_GetTick();
        s_snap.hip_angle_L = XM.status.h10.leftHipAngle;
        s_snap.hip_angle_R = XM.status.h10.rightHipAngle;
        s_snap.accel_x     = XM.status.h10.leftHipImuGlobalAccX;
        s_snap.accel_y     = XM.status.h10.leftHipImuGlobalAccY;
        s_snap.accel_z     = XM.status.h10.leftHipImuGlobalAccZ;
        s_snap.gait_phase  = 0;
    }
}
