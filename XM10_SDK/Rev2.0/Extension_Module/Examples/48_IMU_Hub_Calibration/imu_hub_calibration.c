/**
 ******************************************************************************
 * @file    imu_hub_calibration.c
 * @author  HyundoKim
 * @brief   [고급] IMU Hub 센서 제자리 보정 — 버튼으로 자이로 영점 · 지자기 보정
 * @details
 * CAN-FD 로 연결된 IMU Hub 의 센서를 케이스에 넣은 채로 보정합니다. 함수는 기다리지 않으므로
 * Control_Loop 에서 요청하고 상태만 살펴봅니다.
 *
 * ============================================================
 *  동작
 * ============================================================
 *  버튼 1  연결된 모든 센서의 자이로 영점 — 센서를 가만히 두세요
 *  버튼 2  지자기 보정 시작. 센서를 장착한 그대로 돌린 뒤 한 번 더 누르면 끝냅니다
 *  버튼 3  지자기 보정 중단 — 저장하지 않습니다
 *
 *  LED1  느린 점멸 = IMU Hub 없음 · 켜짐 = 연결됨
 *  LED2  빠른 점멸 = 보정 진행 중 · 느린 점멸 = 센서를 움직일 단계 · 켜짐 = 모두 끝남
 *  LED3  켜짐 = 실패한 채널이 있음(PC 에서 이유를 보세요)
 *  PC    xm10 도구 — 채널마다 상태가 바뀔 때 "[IMU] ..." 문장
 *
 * ============================================================
 *  알아 둘 것
 * ============================================================
 *  - 제어 모드에서는 시작되지 않습니다. 이 예제는 XM_CTRL_MONITOR(기본값)로 둡니다.
 *  - 보정 중인 채널은 XM.status.imu_hub.connected_mask 비트가 꺼집니다. 끝난 뒤 새 데이터가 들어오면 다시 켜집니다.
 *  - 시작 요청은 버튼을 누를 때 한 번만 보냅니다. 앞 요청을 처리하는 중이면(XM_IMU_CALIB_REQ_BUSY)
 *    요청을 기억해 두었다가 다음 주기에 다시 보냅니다.
 *  - EBIMU 의 지자기 보정을 중단하면 모든 채널의 전원이 약 2 초 꺼졌다 켜집니다. 중단 결과까지 약 17 초가 걸릴 수 있습니다.
 *
 * @note IMU Hub 는 내부에서 개발 중인 모듈입니다. 사용하려면 https://huphailab.com/contact 로 문의해 주세요.
 * @warning **본 예제는 XM10 Rev 2.0 전용입니다.** 참고: docs/hardware/README.md (보드 리비전 비교)
 *
 * @see     xm_api_imu_calib.h (보정 함수와 결과 값)
 * @see     Ex.41 imu_hub_dashboard.c (IMU Hub 데이터 보기)
 * @version 1.0
 * @date    2026-11-05
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <stdio.h>   /* snprintf — 상태를 문장으로 만들 때 */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define STATUS_PERIOD_MS   100U   /* 상태를 살피는 주기 */
#define MSG_SIZE           64U
#define LED_FAST_MS        200U
#define LED_SLOW_MS        1000U
#define ALL_CHANNELS       ((uint8_t)((1U << XM_IMU_HUB_SENSOR_COUNT) - 1U))

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 아직 받아들여지지 않은 요청 */
typedef enum {
    REQ_NONE = 0,
    REQ_START_GYRO,
    REQ_START_MAG,
    REQ_FINISH,
    REQ_ABORT,
} PendingRequest_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static PendingRequest_t   s_pending;
static uint8_t            s_pending_mask;
static XmImuCalibStatus_t s_status[XM_IMU_HUB_SENSOR_COUNT];
static uint32_t           s_last_status_tick;
static bool               s_supported_asked;
static bool               s_supported_shown;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void    _ShowSupportedOnce(void);
static void    _HandleButtons(uint8_t connected_mask);
static void    _SendPending(void);
static void    _UpdateStatus(void);
static uint8_t _ChannelsInState(XmImuCalibState_t state);
static void    _ShowLeds(void);
static const char* _StateName(XmImuCalibState_t state);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void Control_Setup(void)
{
    s_last_status_tick = XM_GetTick();
}

void Control_Loop(void)
{
    const XmImuHubData_t* hub = &XM.status.imu_hub;
    const uint32_t now = XM_GetTick();

    /* 연결이 끊겨도 상태는 봅니다 — 보정 중에 끊기면 XM_IMU_CALIB_RESULT_LINK_LOST 로 끝납니다 */
    if ((now - s_last_status_tick) >= STATUS_PERIOD_MS) {
        s_last_status_tick = now;
        _UpdateStatus();
        _ShowLeds();
    }

    if (!hub->is_connected) {
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, LED_SLOW_MS);
        s_pending = REQ_NONE;
        s_supported_asked = false;
        s_supported_shown = false;
        return;
    }
    XM_SetLedState(XM_LED_1, XM_ON);

    _ShowSupportedOnce();
    _HandleButtons(hub->connected_mask);
    _SendPending();
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* 어떤 항목을 지원하는지 연결될 때 한 번 묻고 보여 줍니다 */
static void _ShowSupportedOnce(void)
{
    char msg[MSG_SIZE];
    uint8_t mask = 0U;

    if (!s_supported_asked) {
        s_supported_asked = (XM_ImuCalibQuerySupported() == XM_IMU_CALIB_REQ_OK);
        return;
    }
    if (s_supported_shown) {
        return;
    }
    const XmImuCalibResult_t result = XM_ImuCalibGetSupported(0U, &mask);
    if (result == XM_IMU_CALIB_RESULT_NONE) {
        return;                       /* 아직 답이 없습니다 */
    }
    s_supported_shown = true;
    if (result != XM_IMU_CALIB_RESULT_OK) {
        snprintf(msg, sizeof(msg), "[IMU] calibration not available (result %d)\r\n", (int)result);
        XM_SendUsbDebugMessage(msg);
        return;
    }
    for (uint8_t ch = 0U; ch < XM_IMU_HUB_SENSOR_COUNT; ch++) {
        if (XM_ImuCalibGetSupported(ch, &mask) == XM_IMU_CALIB_RESULT_OK && mask != 0U) {
            snprintf(msg, sizeof(msg), "[IMU] ch%u supports 0x%02X\r\n", (unsigned)ch, (unsigned)mask);
            XM_SendUsbDebugMessage(msg);
        }
    }
}

static void _HandleButtons(uint8_t connected_mask)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_pending = REQ_START_GYRO;
        s_pending_mask = connected_mask;
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        const uint8_t waiting = _ChannelsInState(XM_IMU_CALIB_WAIT_USER);
        s_pending = (waiting != 0U) ? REQ_FINISH : REQ_START_MAG;
        s_pending_mask = (waiting != 0U) ? waiting : connected_mask;
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_pending = REQ_ABORT;
        s_pending_mask = ALL_CHANNELS;
    }
}

/* 버튼으로 생긴 요청을 보냅니다. 앞 요청을 처리하는 중이면 다음 주기에 다시 */
static void _SendPending(void)
{
    XmImuCalibRequest_t result = XM_IMU_CALIB_REQ_OK;

    switch (s_pending) {
    case REQ_START_GYRO:
        result = XM_ImuCalibStart(s_pending_mask, XM_IMU_CALIB_OP_GYRO);
        break;
    case REQ_START_MAG:
        result = XM_ImuCalibStart(s_pending_mask, XM_IMU_CALIB_OP_MAG);
        break;
    case REQ_FINISH:
        result = XM_ImuCalibFinish(s_pending_mask);
        break;
    case REQ_ABORT:
        result = XM_ImuCalibAbort(s_pending_mask);
        break;
    default:
        return;
    }

    if (result == XM_IMU_CALIB_REQ_BUSY) {
        return;
    }
    s_pending = REQ_NONE;
    if (result != XM_IMU_CALIB_REQ_OK) {
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), "[IMU] request refused (%d)\r\n", (int)result);
        XM_SendUsbDebugMessage(msg);
    }
}

/* 채널 상태를 읽고, 바뀐 채널만 PC 로 알립니다 */
static void _UpdateStatus(void)
{
    for (uint8_t ch = 0U; ch < XM_IMU_HUB_SENSOR_COUNT; ch++) {
        XmImuCalibStatus_t st;
        if (!XM_ImuCalibGetStatus(ch, &st)) {
            continue;                 /* 이번에는 못 읽었습니다 — 다음 주기에 */
        }
        if (st.state != s_status[ch].state || st.result != s_status[ch].result) {
            char msg[MSG_SIZE];
            snprintf(msg, sizeof(msg), "[IMU] ch%u %s result=%d info=%u\r\n",
                     (unsigned)ch, _StateName(st.state), (int)st.result, (unsigned)st.info);
            XM_SendUsbDebugMessage(msg);
        }
        s_status[ch] = st;
    }
}

static uint8_t _ChannelsInState(XmImuCalibState_t state)
{
    uint8_t mask = 0U;
    for (uint8_t ch = 0U; ch < XM_IMU_HUB_SENSOR_COUNT; ch++) {
        if (s_status[ch].state == state) {
            mask |= (uint8_t)(1U << ch);
        }
    }
    return mask;
}

static void _ShowLeds(void)
{
    const uint8_t running = (uint8_t)(_ChannelsInState(XM_IMU_CALIB_RUNNING) | _ChannelsInState(XM_IMU_CALIB_PENDING));

    if (running != 0U) {
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, LED_FAST_MS);
    } else if (_ChannelsInState(XM_IMU_CALIB_WAIT_USER) != 0U) {
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, LED_SLOW_MS);
    } else if (_ChannelsInState(XM_IMU_CALIB_DONE) != 0U) {
        XM_SetLedState(XM_LED_2, XM_ON);
    } else {
        XM_SetLedState(XM_LED_2, XM_OFF);
    }
    XM_SetLedState(XM_LED_3, (_ChannelsInState(XM_IMU_CALIB_FAILED) != 0U) ? XM_ON : XM_OFF);
}

static const char* _StateName(XmImuCalibState_t state)
{
    switch (state) {
    case XM_IMU_CALIB_IDLE:      return "idle";
    case XM_IMU_CALIB_PENDING:   return "pending";
    case XM_IMU_CALIB_RUNNING:   return "running";
    case XM_IMU_CALIB_WAIT_USER: return "move-sensor";
    case XM_IMU_CALIB_DONE:      return "done";
    case XM_IMU_CALIB_FAILED:    return "failed";
    default:                     return "?";
    }
}
