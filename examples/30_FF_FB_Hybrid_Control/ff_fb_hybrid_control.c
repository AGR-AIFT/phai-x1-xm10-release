/**
 ******************************************************************************
 * @file    ff_fb_hybrid_control.c
 * @author  HyundoKim
 * @brief   [고급] 피드포워드 + 피드백 혼합 제어 (Feedforward + Feedback Hybrid)
 * @details
 * 모델 기반 피드포워드(FF)와 PD 피드백(FB)을 결합하는 계산 토크(Computed Torque)
 * 방식의 혼합 제어기를 구현합니다.
 *
 * [제어 이론 — FF+FB 혼합 제어]
 * 기존 접근의 한계:
 *   PD (Ex.14): 피드백만 사용 → 중력 영향으로 정상상태 오차 발생
 *   중력 보상 (Ex.21): 피드포워드만 사용 → 모델 불확실성에 민감
 *
 * 혼합 제어의 장점 (두 접근의 상호보완):
 *   τ_total = τ_ff + τ_fb
 *   τ_ff: 모델 기반 피드포워드 (중력 + 마찰 사전 보상, 정상상태 오차 제거)
 *   τ_fb: PD 피드백 (모델 불확실성 및 외란 보정)
 *
 * [피드포워드 — 역동역학 근사 (저속 보행 가정)]
 * 고속 동작의 관성항(M·θ̈)은 보행 속도(<1 Hz)에서 작으므로 생략:
 *   τ_ff ≈ M·g·L_eff·cos(θ) + B_f·θ̇   (중력 + 점성 마찰)
 *   DEG_TO_RAD 변환 필수 (cos 함수 입력은 rad)
 *
 * [피드백 — PD 제어기]
 *   θ_d(t) = A·sin(2π·f·t)              (정현파 참조 궤도)
 *   e = θ_d - θ
 *   τ_fb = Kp·e + Kd·(θ̇_d - θ̇)
 *
 * [BTN3 토글 활용]
 * FF 활성화/비활성화로 순수 PD와의 성능 직접 비교 가능
 *
 * [Body Data 여부]
 * 이 예제는 Body Data 없이 동작합니다.
 * 신체 파라미터를 매크로로 직접 설정합니다.
 *
 * [논문 레퍼런스]
 * - Slotine, J.J.E. & Li, W. (1991) Applied Nonlinear Control,
 *   Prentice-Hall, Ch.6 (Computed Torque 방법론)
 * - Vukobratovic, M. & Borovac, B. (2004) "Zero-moment point — thirty five years
 *   of its life" IJHR, 1(1), 157-173.
 *
 * [버튼 조작]
 * - BTN1: Kp 순환 (0.1→0.2→0.4→0.6 Nm/deg)
 * - BTN2: 참조 진폭 순환 (5→10→15→20 deg)
 * - BTN3: FF 활성화/비활성화 토글 (순수 PD 성능 비교용)
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

/* --- 신체/로봇 파라미터 (피드포워드 모델) --- */
#define M_BODY_KG           70.0f   /* 체중 (kg) — 실측 또는 추정값으로 교체 */
#define L_EFF_M             0.25f   /* 유효 레버 길이 (m, 고관절→CoM 거리) */
#define G_ACC               9.81f   /* 중력 가속도 (m/s²) */
#define B_FRICTION          0.3f    /* 점성 마찰 계수 (Nm·s/deg) */

/* --- 피드포워드 토크 계수 --- */
/* M·g·L_eff 사전 계산 (실행 시 반복 곱셈 방지) */
#define MGL_EFF             (M_BODY_KG * G_ACC * L_EFF_M)

/* --- 피드백 게인 --- */
#define KP_FB_DEFAULT       0.4f    /* 기본 비례 게인 (Nm/deg) */
#define KD_FB               0.015f  /* 미분 게인 (Nm·s/deg) */

/* --- 참조 궤도 --- */
#define A_REF_DEFAULT       10.0f   /* 기본 진폭 (deg) */
#define FREQ_HZ             0.5f    /* 참조 주파수 (Hz, 보행 0.5Hz ≈ 1초 주기) */

/* --- 안전 한계 --- */
#define MAX_TORQUE_NM       5.0f
#define CONTROL_DT          0.001f
#define USB_DEBUG_PERIOD_MS 500U

/* --- 단위 변환 --- */
#define DEG_TO_RAD(x)       ((x) * 0.017453292f)

/* Kp 프리셋 */
#define KP_PRESET_COUNT     4U
static const float k_kp_presets[KP_PRESET_COUNT] = {0.1f, 0.2f, 0.4f, 0.6f};

/* 진폭 프리셋 (deg) */
#define AMP_PRESET_COUNT    4U
static const float k_amp_presets[AMP_PRESET_COUNT] = {5.0f, 10.0f, 15.0f, 20.0f};

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float theta_d;      /* 목표 각도 (deg) */
    float theta;        /* 실제 각도 (deg) */
    float tau_ff;       /* 피드포워드 토크 (Nm) */
    float tau_fb;       /* 피드백 토크 (Nm) */
} FfFbStreamData_t;

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

static XmTsmHandle_t    s_tsm;
static float            s_kp            = KP_FB_DEFAULT;
static float            s_a_ref         = A_REF_DEFAULT;
static uint8_t          s_kp_idx        = 2U;   /* 기본 0.4 */
static uint8_t          s_amp_idx       = 1U;   /* 기본 10 deg */
static bool             s_ff_enabled    = true; /* 피드포워드 활성화 여부 */

static float            s_prev_angle    = 0.0f;
static float            s_tau_ff        = 0.0f;
static float            s_tau_fb        = 0.0f;
static float            s_torque_cmd    = 0.0f;

static uint32_t         s_usb_debug_timer = 0U;
static FfFbStreamData_t s_stream_data;

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

static float _ComputeFeedforward(float theta_deg, float theta_dot_deg);
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(float theta_d, float theta);
static void  _UpdateStreamData(float theta_d, float theta);
static float _ClampFloat(float val, float min_val, float max_val);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* Body Data: 이 예제는 Body Data 없이 동작합니다.
     * 신체 파라미터(M_BODY_KG, L_EFF_M)를 매크로에서 직접 설정하세요. */

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
        "[{\"name\":\"Target Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Actual Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"FF Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"FB Torque\",\"unit\":\"Nm\"}]");
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
        XM_SendUsbDebugMessage("[FF+FB] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[FF+FB] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);

    s_prev_angle      = XM.status.h10.rightHipMotorAngle;
    s_tau_ff          = 0.0f;
    s_tau_fb          = 0.0f;
    s_torque_cmd      = 0.0f;
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, s_ff_enabled ? XM_ON : XM_OFF);

    XM_SendUsbDebugMessage("[FF+FB] ACTIVE — 혼합 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — FF+FB 혼합 제어 (1ms)
 * @details
 * [혼합 제어 계산 순서]
 * Step 1: 시간 기반 참조 궤도 계산
 *   θ_d(t) = A·sin(2π·f·t)
 *   θ̇_d(t) = A·2π·f·cos(2π·f·t)
 *
 * Step 2: 피드포워드 토크 계산 (역동역학 근사)
 *   τ_ff = M·g·L·cos(θ) + B_f·θ̇   (중력 + 마찰 사전 보상)
 *
 * Step 3: 피드백 PD 토크 계산
 *   τ_fb = Kp·(θ_d - θ) + Kd·(θ̇_d - θ̇)
 *
 * Step 4: 혼합 및 포화
 *   τ = τ_ff + τ_fb  (FF 비활성화 시 τ = τ_fb만)
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[FF+FB] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    /* Step 1: 참조 궤도 계산 */
    float t_sec  = (float)XM_GetTick() * 0.001f;
    float omega  = 2.0f * (float)M_PI * FREQ_HZ;
    float theta_d     = s_a_ref * sinf(omega * t_sec);
    float theta_dot_d = s_a_ref * omega * cosf(omega * t_sec);

    /* 현재 상태 읽기 */
    float theta     = XM.status.h10.rightHipMotorAngle;
    float theta_dot = (theta - s_prev_angle) / CONTROL_DT;
    s_prev_angle    = theta;

    /* Step 2: 피드포워드 토크 */
    s_tau_ff = s_ff_enabled ? _ComputeFeedforward(theta, theta_dot) : 0.0f;

    /* Step 3: 피드백 PD 토크 */
    float error  = theta_d - theta;
    float d_err  = theta_dot_d - theta_dot;
    s_tau_fb = s_kp * error + KD_FB * d_err;

    /* Step 4: 혼합 및 포화 */
    float torque_raw = s_tau_ff + s_tau_fb;
    s_torque_cmd = _ClampFloat(torque_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);

    /* LED2: 참조 궤도 영교차 시 토글 (시각적 주파수 표시) */
    static bool s_led2_state = false;
    static float s_prev_theta_d = 0.0f;
    if (s_prev_theta_d < 0.0f && theta_d >= 0.0f) {
        s_led2_state = !s_led2_state;
        XM_SetLedState(XM_LED_2, s_led2_state ? XM_ON : XM_OFF);
    }
    s_prev_theta_d = theta_d;

    /* LED3: FF 활성화 상태 표시 */
    XM_SetLedState(XM_LED_3, s_ff_enabled ? XM_ON : XM_OFF);

    _UpdateUsbDebug(theta_d, theta);
    _UpdateStreamData(theta_d, theta);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);

    s_tau_ff     = 0.0f;
    s_tau_fb     = 0.0f;
    s_torque_cmd = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[FF+FB] 제어 종료 — 토크 해제\r\n");
}

/**
 * @brief 피드포워드 토크 계산 (역동역학 근사)
 * @param theta_deg     현재 관절각 (deg)
 * @param theta_dot_deg 관절 각속도 (deg/s)
 * @return 피드포워드 토크 (Nm)
 * @details
 * 저속 보행(<1Hz) 가정 하에 관성항 M·θ̈를 생략:
 *   τ_ff ≈ M·g·L·cos(θ) + B_f·θ̇
 *
 * cos(θ): 고관절 각도에 따른 중력 토크 변화 반영
 *   θ=0  (직립): 최대 중력 토크
 *   θ=90 (수평): 중력 토크=0 (지지력과 수직)
 */
static float _ComputeFeedforward(float theta_deg, float theta_dot_deg)
{
    /* 중력 보상 토크 (단위: deg→rad 변환 필수) */
    float theta_rad = DEG_TO_RAD(theta_deg);
    float tau_gravity = MGL_EFF * cosf(theta_rad);   /* N·m */

    /* 점성 마찰 보상 */
    float tau_friction = B_FRICTION * theta_dot_deg;

    return tau_gravity + tau_friction;
}

static void _HandleButtonInput(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_kp_idx = (s_kp_idx + 1U) % KP_PRESET_COUNT;
        s_kp     = k_kp_presets[s_kp_idx];
        XM_SendUsbDebugMessage("[FF+FB] BTN1: Kp 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_amp_idx = (s_amp_idx + 1U) % AMP_PRESET_COUNT;
        s_a_ref   = k_amp_presets[s_amp_idx];
        XM_SendUsbDebugMessage("[FF+FB] BTN2: 진폭 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_ff_enabled = !s_ff_enabled;
        XM_SendUsbDebugMessage(s_ff_enabled ?
            "[FF+FB] BTN3: FF 활성화\r\n" : "[FF+FB] BTN3: FF 비활성화 (순수 PD)\r\n");
    }
}

static void _UpdateUsbDebug(float theta_d, float theta)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    char buf[96];
    snprintf(buf, sizeof(buf),
             "FF+FB | θd:%.1f θ:%.1f τff:%.2f τfb:%.2f τ:%.2f FF:%s\r\n",
             theta_d, theta, s_tau_ff, s_tau_fb, s_torque_cmd,
             s_ff_enabled ? "ON" : "OFF");
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(float theta_d, float theta)
{
    s_stream_data.theta_d = theta_d;
    s_stream_data.theta   = theta;
    s_stream_data.tau_ff  = s_tau_ff;
    s_stream_data.tau_fb  = s_tau_fb;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

static float _ClampFloat(float val, float min_val, float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}
