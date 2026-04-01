/**
 ******************************************************************************
 * @file    mrac_adaptive_control.c
 * @author  HyundoKim
 * @brief   [고급] 모델 참조 적응 제어 (Model Reference Adaptive Control, MRAC)
 * @details
 * 참조 모델의 응답 특성을 추종하도록 제어기 파라미터를 실시간으로 적응시킵니다.
 * 사용자-로봇 상호작용 파라미터(체중, 근력, 강성)를 온라인으로 추정하여
 * 개인화된 어시스트를 제공합니다.
 *
 * [제어 이론 — MRAC (MIT Rule 기반 이산 시간)]
 * 참조 모델 (1차 이산 LPF):
 *   x_m[k+1] = a_m·x_m[k] + b_m·r[k]
 *   a_m = 0.99 (극점 ≈ 100ms 응답 시정수 at 1kHz)
 *   b_m = 0.01 (입력 게인, a_m + b_m = 1 for DC unity gain)
 *
 * 추종 오차:
 *   e[k] = x_m[k] - x[k]   (참조 모델 상태 - 실제 각도)
 *
 * 적응 제어 입력 (선형 파라미터화):
 *   τ[k] = θ̂₁[k]·x[k] + θ̂₂[k]·r[k]
 *
 * MIT Rule 적응 법칙 (Gradient Descent):
 *   θ̂₁[k+1] = θ̂₁[k] + γ₁·e[k]·x[k]
 *   θ̂₂[k+1] = θ̂₂[k] + γ₂·e[k]·r[k]
 *
 * 파라미터 드리프트 방지 (Projection):
 *   θ̂₁ ∈ [-1.0, +1.0],  θ̂₂ ∈ [0.0, 2.0]
 *
 * [Body Data 여부]
 * 이 예제는 Body Data 없이 동작합니다.
 * 고관절 모터 각도만으로 피드백을 구성합니다.
 * XM_SendUserBodyData() 설정 시 보행 상태 감지 정확도가 향상됩니다.
 *
 * [논문 레퍼런스]
 * - Slotine, J.J.E. & Li, W. (1991) Applied Nonlinear Control,
 *   Prentice-Hall, Ch.8 (MRAC 이론 교과서)
 * - Sharifi, M. et al. (2014) "Nonlinear model reference adaptive impedance
 *   control for human-robot interactions" Control Engineering Practice, 32, 9-27.
 *
 * [버튼 조작]
 * - BTN1: 참조 입력 r 증가 (5→10→15→20 deg, 래핑)
 * - BTN2: 적응 속도 γ 순환 (0.001→0.005→0.01→0.05)
 * - BTN3: 적응 파라미터 리셋 (θ̂₁=0, θ̂₂=0.5)
 *
 * @see     docs/api-reference/02-h10-control-n-data.md
 * @version 1.0
 * @date    Mar 10, 2026
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

/* --- 참조 모델 파라미터 (1차 이산 LPF) --- */
#define AM_REF          0.99f   /* 참조 모델 극점 (응답 시정수 ≈ 100ms) */
#define BM_REF          0.01f   /* 참조 모델 입력 게인 */

/* --- 초기 적응 파라미터 --- */
#define THETA1_INIT     0.0f    /* 초기 상태 피드백 게인 추정값 */
#define THETA2_INIT     0.5f    /* 초기 입력 게인 추정값 */

/* --- 파라미터 범위 제한 (드리프트 방지) --- */
#define THETA1_MIN     -1.0f
#define THETA1_MAX      1.0f
#define THETA2_MIN      0.0f    /* 입력 게인은 양수 유지 */
#define THETA2_MAX      2.0f

/* --- 기본 적응 속도 --- */
#define GAMMA_DEFAULT   0.005f

/* --- 참조 입력 --- */
#define R_REF_DEFAULT   10.0f   /* 기본 목표 각도 (deg) */
#define R_REF_STEP      5.0f
#define R_REF_MAX       20.0f
#define R_REF_MIN       5.0f

/* --- 안전 한계 --- */
#define MAX_TORQUE_NM   5.0f
#define CONTROL_DT      0.001f
#define USB_DEBUG_PERIOD_MS 500U

/* 적응 속도 프리셋 */
#define GAMMA_PRESET_COUNT 4U
static const float k_gamma_presets[GAMMA_PRESET_COUNT] = {0.001f, 0.005f, 0.01f, 0.05f};

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float ref_model_state;  /* 참조 모델 상태 x_m (deg) */
    float actual_angle;     /* 실제 각도 (deg) */
    float mrac_error;       /* 추종 오차 e (deg) */
    float torque;           /* 출력 토크 (Nm) */
} MracStreamData_t;

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *-----------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;

/* 참조 모델 상태 */
static float s_x_ref   = 0.0f;     /* 참조 모델 출력 x_m (deg) */

/* 적응 파라미터 */
static float s_theta1  = THETA1_INIT;
static float s_theta2  = THETA2_INIT;
static float s_gamma   = GAMMA_DEFAULT;
static uint8_t s_gamma_idx = 1U;

/* 참조 입력 */
static float s_r_ref   = R_REF_DEFAULT;

/* 출력 */
static float s_torque_cmd = 0.0f;

static uint32_t      s_usb_debug_timer = 0U;
static MracStreamData_t s_stream_data;

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void  Off_Loop(void);
static void  Standby_Loop(void);
static void  Active_Entry(void);
static void  Active_Loop(void);
static void  Active_Exit(void);

static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(float x_m, float x, float e);
static void  _UpdateStreamData(float x_m, float x, float e);
static float _ClampFloat(float val, float min_val, float max_val);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* Body Data: 이 예제는 Body Data 없이 동작 가능
     * 보행 상태 감지 향상을 원하면 아래 주석 해제 후 값 설정 */
    /*
    uint32_t body_data[8] = {70000U, 1750U, 450U, 450U, 420U, 420U, 60U, 60U};
    XM_SendUserBodyData(body_data);
    */

    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = { .id = XM_STATE_OFF, .on_loop = Off_Loop };
    XM_TSM_AddState(s_tsm, &off_conf);

    XmStateConfig_t sb_conf  = { .id = XM_STATE_STANDBY, .on_loop = Standby_Loop };
    XM_TSM_AddState(s_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Ref Model State\",\"unit\":\"deg\"},"
        "{\"name\":\"Actual Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"MRAC Error\",\"unit\":\"deg\"},"
        "{\"name\":\"Torque\",\"unit\":\"Nm\"}]");
    XM_SetControlMode(XM_CTRL_MONITOR);
}

void User_Loop(void)
{
    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }
    XM_TSM_Run(s_tsm);
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS
 *-----------------------------------------------------------
 */

static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SendUsbDebugMessage("[MRAC] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[MRAC] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);

    /* 참조 모델을 현재 각도로 초기화 (부드러운 시작) */
    s_x_ref      = XM.status.h10.rightHipMotorAngle;
    s_theta1     = THETA1_INIT;
    s_theta2     = THETA2_INIT;
    s_torque_cmd = 0.0f;
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[MRAC] ACTIVE — 적응 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — MRAC 실행 (1ms)
 * @details
 * [이산 시간 MRAC 업데이트 순서]
 * Step 1: 참조 모델 갱신 x_m[k+1] = a_m·x_m[k] + b_m·r[k]
 * Step 2: 추종 오차 e[k] = x_m[k] - x[k]
 * Step 3: 적응 제어 입력 τ[k] = θ̂₁·x[k] + θ̂₂·r[k]
 * Step 4: MIT Rule 파라미터 업데이트
 * Step 5: 파라미터 범위 제한 (드리프트 방지)
 * Step 6: 토크 포화 후 전송
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[MRAC] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    /* 현재 상태 읽기 */
    float x = XM.status.h10.rightHipMotorAngle;   /* 실제 각도 (deg) */
    float r = s_r_ref;                             /* 참조 입력 (deg) */

    /* Step 1: 참조 모델 갱신 (1차 이산 LPF) */
    s_x_ref = AM_REF * s_x_ref + BM_REF * r;

    /* Step 2: 추종 오차 */
    float e = s_x_ref - x;

    /* Step 3: 적응 제어 입력 */
    float torque_raw = s_theta1 * x + s_theta2 * r;

    /* Step 4: MIT Rule 파라미터 업데이트 */
    s_theta1 += s_gamma * e * x;
    s_theta2 += s_gamma * e * r;

    /* Step 5: 파라미터 범위 제한 (projection 연산자) */
    s_theta1 = _ClampFloat(s_theta1, THETA1_MIN, THETA1_MAX);
    s_theta2 = _ClampFloat(s_theta2, THETA2_MIN, THETA2_MAX);

    /* Step 6: 토크 포화 후 전송 (좌우 동일 적용) */
    s_torque_cmd = _ClampFloat(torque_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);

    /* LED2: 오차 크기에 비례한 점멸 (|e| > 3deg → 빠른 점멸) */
    if (fabsf(e) > 3.0f) {
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 100);
    } else {
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 500);
    }

    /* LED3: 추종 양호 시 ON (|e| < 1 deg) */
    XM_SetLedState(XM_LED_3, (fabsf(e) < 1.0f) ? XM_ON : XM_OFF);

    _UpdateUsbDebug(s_x_ref, x, e);
    _UpdateStreamData(s_x_ref, x, e);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);

    /* 적응 파라미터 초기값으로 복귀 */
    s_theta1     = THETA1_INIT;
    s_theta2     = THETA2_INIT;
    s_torque_cmd = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedEffect(XM_LED_2, XM_LED_OFF, 0);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[MRAC] 제어 종료 — 토크 해제\r\n");
}

static void _HandleButtonInput(void)
{
    /* BTN1: 참조 입력 증가 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_r_ref += R_REF_STEP;
        if (s_r_ref > R_REF_MAX) { s_r_ref = R_REF_MIN; }
        XM_SendUsbDebugMessage("[MRAC] BTN1: 참조 입력 변경\r\n");
    }
    /* BTN2: 적응 속도 순환 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_gamma_idx = (s_gamma_idx + 1U) % GAMMA_PRESET_COUNT;
        s_gamma     = k_gamma_presets[s_gamma_idx];
        XM_SendUsbDebugMessage("[MRAC] BTN2: 적응 속도 변경\r\n");
    }
    /* BTN3: 적응 파라미터 리셋 */
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_theta1 = THETA1_INIT;
        s_theta2 = THETA2_INIT;
        s_x_ref  = XM.status.h10.rightHipMotorAngle;
        XM_SendUsbDebugMessage("[MRAC] BTN3: 파라미터 리셋\r\n");
    }
}

static void _UpdateUsbDebug(float x_m, float x, float e)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    char buf[96];
    snprintf(buf, sizeof(buf),
             "MRAC | xm:%.1f x:%.1f e:%.2f τ:%.2f θ1:%.3f θ2:%.3f\r\n",
             x_m, x, e, s_torque_cmd, s_theta1, s_theta2);
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(float x_m, float x, float e)
{
    s_stream_data.ref_model_state = x_m;
    s_stream_data.actual_angle    = x;
    s_stream_data.mrac_error      = e;
    s_stream_data.torque          = s_torque_cmd;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

static float _ClampFloat(float val, float min_val, float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}
