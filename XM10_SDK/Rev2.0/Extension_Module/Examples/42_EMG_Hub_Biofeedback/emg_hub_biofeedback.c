/**
 ******************************************************************************
 * @file    emg_hub_biofeedback.c
 * @author  HyundoKim
 * @brief   [고급] EMG Hub 근활성도 바이오피드백 — MVC 캘리브 + 실시간 활성도 피드백
 * @details
 * EMG Hub Module(sEMG 1채널, FDCAN2 DOP V3)에서 **허브가 신호처리를 마친** 근활성도
 * 데이터를 받아 실시간 바이오피드백을 제공합니다. "지금 근육을 내 최대 대비 얼마나 세게
 * 쓰고 있는가"를 LED 와 PhAI Studio 로 되먹임하는 재활/트레이닝용 예제입니다.
 *
 * ============================================================
 *  Ex.40 과의 차이 (역할 분리)
 * ============================================================
 *  - Ex.40 EMG Proportional Assist : XM 외부 ADC 로 raw EMG 를 직접 받아 DSP → 모터 보조 토크.
 *  - Ex.42 (본 예제)                : **EMG Hub Module** 이 HPF→정류→RMS→Envelope→MVC 까지 처리한
 *                                     결과(XM.status.emg_hub)를 받아 **바이오피드백**(모터 구동 없음, 안전).
 *
 * ============================================================
 *  사용 절차 (MVC 정규화)
 * ============================================================
 *  1) EMG Hub 연결 → LED1 점등(READY). 미연결 시 LED1 느린 대기 점멸.
 *  2) [BTN1] 근육 이완 상태에서 클릭 → offset 캘리브(허브가 4000샘플 자동 누적).
 *  3) [BTN2] 최대 수축(MVC) 유지하며 클릭 → 현재 RMS 를 100% 기준으로 캡처.
 *     → 이후 mvc_percent(0~100%)가 유효(status CALIB_VALID)해집니다.
 *  4) 근육을 쓰면:
 *     - LED1 : mvc_percent 에 비례해 빠르게 점멸(셀수록 빠름). 캘리브 전엔 heartbeat.
 *     - LED2 : 근수축 감지(is_active, Schmitt) 시 점등.
 *     - PhAI Studio 0xF0 채널로 envelope/mvc%/활성/캘리브상태 스트리밍.
 *
 * @note EMG Hub 데이터 원본은 FDCAN2 TPDO → core_process 가 XM.status.emg_hub 를 자동 갱신하므로
 *       읽기만 하면 됩니다. 캘리브 명령만 EmgHub_Drv_SendCalCommand() 로 허브에 전송합니다.
 * @warning **본 예제는 XM10 Rev 2.0 전용입니다.** EMG Hub Module 은 FDCAN2 센서허브(Rev 2.0)로
 *          연결됩니다. Rev 1.1 은 FDCAN2 센서허브를 지원하지 않아 데이터가 수신되지 않습니다.
 *          참고: docs/hardware/README.md (보드 리비전 비교)
 *
 * @see     Ex.09 cdc_stream.c (User Custom 0xF0 스트리밍)
 * @see     Ex.40 emg_proportional_assist.c (외부 ADC EMG → 모터 보조, 역할 대비)
 * @version 1.0
 * @date    2026-07-15
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include "emg_hub_drv.h"   /* EmgHub_Drv_SendCalCommand, EMGHUB_CAL_CMD_*, EMGHUB_STATUS_CALIB_VALID */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define EMG_STREAM_PERIOD_MS   20U    /* 스트리밍 주기 (50Hz) */

/* LED1 점멸 주기 매핑 (mvc% 0→느림 500ms, 100→빠름 100ms) */
#define EMG_LED_PERIOD_MIN_MS  100U   /* mvc 100% */
#define EMG_LED_PERIOD_MAX_MS  500U   /* mvc 0%   */

#define EMG_STREAM_MODULE_ID   0xF0U
#define EMG_STREAM_CH_COUNT    4U     /* envelope, mvc%, active, calib_valid */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static float    s_stream[EMG_STREAM_CH_COUNT];
static uint32_t s_last_stream_tick;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void     _HandleCalibrationButtons(bool connected);
static uint32_t _MvcToBlinkPeriod(uint8_t mvc_percent);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void Control_Setup(void)
{
    XM_SetUsbCustomMeta(EMG_STREAM_MODULE_ID,
        "[{\"name\":\"EMG Envelope\",\"unit\":\"uV\"},"
        "{\"name\":\"MVC\",\"unit\":\"%\"},"
        "{\"name\":\"Active\",\"unit\":\"bool\"},"
        "{\"name\":\"Calib Valid\",\"unit\":\"bool\"}]");

    s_last_stream_tick = XM_GetTick();
}

void Control_Loop(void)
{
    const XmEmgHubData_t* emg = &XM.status.emg_hub;
    bool connected = emg->is_connected;

    /* --- 미연결: 대기 표시 후 종료 --- */
    if (!connected) {
        /* 미연결 중 눌린 버튼이 재연결 직후 stale 캘리브로 발화하지 않도록 latch 를 비운다 */
        (void)XM_GetButtonEvent(XM_BTN_1);
        (void)XM_GetButtonEvent(XM_BTN_2);
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 1000);   /* 느린 대기 점멸 */
        XM_SetLedState(XM_LED_2, XM_OFF);
        return;
    }

    /* --- 캘리브 버튼 (BTN1=offset, BTN2=MVC) --- */
    _HandleCalibrationButtons(connected);

    bool calib_valid = (emg->status_flags & EMGHUB_STATUS_CALIB_VALID) != 0U;

    /* --- LED1: 근활성도 피드백 --- */
    if (calib_valid) {
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, _MvcToBlinkPeriod(emg->mvc_percent));
    } else {
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);  /* 캘리브 전 — MVC 기준 없음 */
    }

    /* --- LED2: 근수축(Schmitt) on/off --- */
    XM_SetLedState(XM_LED_2, emg->is_active ? XM_ON : XM_OFF);

    /* --- 50Hz 스로틀 스트리밍 --- */
    uint32_t now = XM_GetTick();
    if ((now - s_last_stream_tick) < EMG_STREAM_PERIOD_MS) {
        return;
    }
    s_last_stream_tick = now;

    s_stream[0] = emg->envelope_uv;
    s_stream[1] = (float)emg->mvc_percent;
    s_stream[2] = emg->is_active ? 1.0f : 0.0f;
    s_stream[3] = calib_valid ? 1.0f : 0.0f;
    XM_SendUsbDataWithId(s_stream, sizeof(s_stream), EMG_STREAM_MODULE_ID);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/** @brief BTN1=offset 캘리브 / BTN2=MVC 캡처 명령을 EMG Hub 로 전송. */
static void _HandleCalibrationButtons(bool connected)
{
    if (!connected) {
        return;
    }

    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        /* 근육 이완 상태에서 offset 캘리브 (허브가 4000샘플 누적 후 자동 완료) */
        EmgHub_Drv_SendCalCommand(EMGHUB_CAL_CMD_OFFSET);
        XM_SendUsbDebugMessage("[EMG] Offset 캘리브 시작 — 근육을 이완하세요\r\n");
    }

    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        /* 최대 수축(MVC) 유지 상태에서 현재 RMS 를 100% 기준으로 캡처 */
        EmgHub_Drv_SendCalCommand(EMGHUB_CAL_CMD_MVC);
        XM_SendUsbDebugMessage("[EMG] MVC 캡처 — 최대로 수축하세요\r\n");
    }
}

/**
 * @brief mvc_percent(0~100+%) → LED 점멸 주기(ms). 활성도 높을수록 빠른 점멸.
 *        100% 초과값은 100 으로 포화.
 */
static uint32_t _MvcToBlinkPeriod(uint8_t mvc_percent)
{
    uint32_t p = (mvc_percent > 100U) ? 100U : (uint32_t)mvc_percent;
    /* 선형 매핑: 0% → MAX(500ms), 100% → MIN(100ms) */
    uint32_t span = EMG_LED_PERIOD_MAX_MS - EMG_LED_PERIOD_MIN_MS;  /* 400 */
    return EMG_LED_PERIOD_MAX_MS - (span * p) / 100U;
}
