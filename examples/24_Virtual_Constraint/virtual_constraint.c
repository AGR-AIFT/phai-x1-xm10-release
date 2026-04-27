/**
 ******************************************************************************
 * @file    virtual_constraint.c
 * @author  HyundoKim
 * @brief   [고급] 가상 구속(Virtual Constraint) / HZD 보행 위상 기반 궤도 추종
 * @details
 * Hybrid Zero Dynamics(HZD) 프레임워크의 핵심인 가상 구속(Virtual Constraint)을
 * 구현합니다. 시간 기반 궤도 추종(PD, Ex.14)과의 근본적 차이:
 *
 *   PD (Ex.14): θ_d = θ_d(t) — 시간이 독립 변수 (보행 속도 변화에 취약)
 *   VC  (이 예제): θ_d = θ_d(s), s = gaitCycle/100 — 보행 위상이 독립 변수
 *                  → 보행 속도가 변해도 s는 측정값이므로 자동 적응
 *
 * [제어 이론 — 가상 구속 + Bézier 다항식]
 * 위상 변수: s = gaitCycle / 100.0f  (0~1)
 *
 * 목표 각도 (5차 Bézier 다항식):
 *   θ_d(s) = Σ_{k=0}^{5} C(5,k)·s^k·(1-s)^(5-k)·α_k
 *   C(5,k) = {1, 5, 10, 10, 5, 1}   (이항 계수)
 *   α_k: 제어점 (deg) — 보행 최적 궤도 파라미터
 *
 * 토크 출력 (PD 추종):
 *   e = θ_d(s) - θ
 *   τ = Kp·e + Kd·(θ̇_d - θ̇)   (가상 구속 이행 토크)
 *
 * [⚠️ Body Data 전제조건 — 필수]
 * s = _EstimateGaitCycle()/100이 이 예제의 유일한 위상 기준입니다.
 * footContact 기반 보행 위상 추정(heel strike detection)을 사용합니다.
 * XM_SendUserBodyData() 미설정 시 footContact 항상 0 → 가상 구속 무효화.
 * 이 예제는 Body Data 없이 정상 동작하지 않습니다.
 *
 * [논문 레퍼런스]
 * - Westervelt, E.R. et al. (2003) "Hybrid zero dynamics of planar biped walkers"
 *   IEEE Transactions on Automatic Control, 48(1), 42-56.
 * - Agrawal, A. et al. (2017) "First steps towards translating HZD control of
 *   bipedal robots to decentralized control of exoskeletons" IEEE Access, 5, 9919-9934.
 *
 * [버튼 조작]
 * - BTN1: Kp 순환 (0.2→0.5→1.0→2.0 Nm/deg)
 * - BTN2: Kd 순환 (0.01→0.03→0.05 Nm·s/deg)
 * - BTN3: Bézier 프로파일 프리셋 전환 (일반 보행 A / 빠른 보행 B / 최소 보조 C)
 *
 * @see     docs/api-reference/02-h10-control-n-data.md
 * @version 1.0
 * @date    Mar 10, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>

/* ===================================================================
 * Gait Cycle Estimator — Heel Strike Detection + Linear Interpolation
 *
 * [⚠️ Body Data 필수]
 * footContact는 H10의 실시간 동작 분석 알고리즘에 의해 추정됩니다.
 * XM_SendUserBodyData()로 인체 정보(체중, 키, 다리 길이)를 반드시 설정하세요.
 * 미설정 시 footContact가 항상 0 → 보행 위상 추정 불가.
 *
 * [알고리즘]
 * 1. forwardVelocity < 임계값 → 정지 (gaitCycle = 0)
 * 2. leftFootContact rising edge → Heel Strike → stride 주기 갱신
 * 3. Heel Strike 간 경과 시간으로 선형 보간 (0~100%)
 *
 * [정확도] ±2-3% (steady-state walking, Body Data 설정 시)
 * =================================================================== */
#define GAIT_EST_VELOCITY_THRESHOLD  0.1f   /* m/s — 이하이면 정지 판정 */
#define GAIT_EST_MIN_STRIDE_MS       400U   /* 최소 stride 주기 (잡음 방지) */
#define GAIT_EST_MAX_STRIDE_MS       3000U  /* 최대 stride 주기 (타임아웃) */

static uint8_t _EstimateGaitCycle(void)
{
    static bool     s_prev_left_contact = false;
    static uint32_t s_last_hs_tick      = 0;
    static uint32_t s_stride_period_ms  = 1000;  /* 초기값 ~1Hz 보행 가정 */
    static bool     s_has_first_hs      = false;

    /* 정지 판정 */
    if (XM.status.h10.forwardVelocity < GAIT_EST_VELOCITY_THRESHOLD) {
        s_has_first_hs = false;
        return 0;
    }

    uint32_t now = XM_GetTick();
    bool left_contact = XM.status.h10.isLeftFootContact;

    /* Heel Strike 감지: left foot contact rising edge */
    if (left_contact && !s_prev_left_contact) {
        uint32_t elapsed = now - s_last_hs_tick;
        if (s_has_first_hs && elapsed >= GAIT_EST_MIN_STRIDE_MS && elapsed <= GAIT_EST_MAX_STRIDE_MS) {
            s_stride_period_ms = elapsed;
        }
        s_last_hs_tick = now;
        s_has_first_hs = true;
    }
    s_prev_left_contact = left_contact;

    /* 선형 보간 */
    if (!s_has_first_hs || s_stride_period_ms == 0) {
        return 0;
    }

    uint32_t since_hs = now - s_last_hs_tick;
    uint32_t pct = (since_hs * 100U) / s_stride_period_ms;
    return (pct > 100U) ? 100U : (uint8_t)pct;
}

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define KP_DEFAULT          0.5f        /* 기본 비례 게인 (Nm/deg) */
#define KD_DEFAULT          0.02f       /* 기본 미분 게인 (Nm·s/deg) */
#define MAX_TORQUE_NM       5.0f        /* 토크 포화 한계 (Nm) */
#define CONTROL_DT          0.001f      /* 제어 주기 (1ms) */
#define USB_DEBUG_PERIOD_MS 500U

/* Bézier 차수 (5차 = 6개 제어점) */
#define BEZIER_ORDER        5
#define BEZIER_CTRL_PTS     6

/* 정지 판정: gaitCycle이 이 시간(ms) 동안 변화 없으면 토크=0 */
#define STALL_TIMEOUT_MS    3000U

/* Kp 프리셋 */
#define KP_PRESET_COUNT     4U
static const float k_kp_presets[KP_PRESET_COUNT] = {0.2f, 0.5f, 1.0f, 2.0f};

/* Kd 프리셋 */
#define KD_PRESET_COUNT     3U
static const float k_kd_presets[KD_PRESET_COUNT] = {0.01f, 0.03f, 0.05f};

/* Bézier 제어점 프리셋 (3종) — 단위: deg */
#define BEZIER_PROFILE_COUNT 3U
static const float k_bezier_profiles[BEZIER_PROFILE_COUNT][BEZIER_CTRL_PTS] = {
    {-5.0f, -8.0f, -12.0f, -8.0f,  2.0f,  8.0f},  /* A: 일반 보행 */
    {-8.0f, -12.0f, -18.0f, -10.0f, 3.0f, 10.0f}, /* B: 빠른 보행 */
    {-2.0f,  -4.0f,  -6.0f,  -4.0f, 1.0f,  4.0f}, /* C: 최소 보조 */
};

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float gait_phase_s; /* 정규화 위상 s (0~1) */
    float theta_d;      /* 목표 각도 (deg) */
    float theta;        /* 실제 각도 (deg) */
    float torque;       /* 출력 토크 (Nm) */
} VcStreamData_t;

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
static float         s_kp              = KP_DEFAULT;
static float         s_kd              = KD_DEFAULT;
static uint8_t       s_kp_idx          = 1U;           /* 기본 Kp=0.5 */
static uint8_t       s_kd_idx          = 1U;           /* 기본 Kd=0.03 */
static uint8_t       s_profile_idx     = 0U;           /* 기본 프로파일 A */
static float         s_prev_angle      = 0.0f;         /* 이전 각도 (속도 계산용) */
static float         s_torque_cmd      = 0.0f;
static uint8_t       s_prev_gait_cycle = 0U;           /* 정지 판정용 */
static uint32_t      s_last_gait_tick  = 0U;           /* 정지 타임아웃 타이머 */
static bool          s_is_stalled      = false;        /* 정지 상태 플래그 */
static uint32_t      s_usb_debug_timer = 0U;
static VcStreamData_t s_stream_data;

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

static float _BezierBasis(uint8_t k, float s);
static float _ComputeDesiredAngle(float s);
static bool  _CheckStall(uint8_t gait_cycle);
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(float s, float theta_d, float theta);
static void  _UpdateStreamData(float s, float theta_d, float theta);
static float _ClampFloat(float val, float min_val, float max_val);
static void  _SetupBodyData(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* ⚠️ Body Data 필수 — footContact 기반 보행 위상 추정이 위상 변수 s의 소스
     * 미설정 시 footContact 항상 0 → 가상 구속 제어 무효화 */
    _SetupBodyData();

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
        "[{\"name\":\"Gait Phase s\",\"unit\":\"-\"},"
        "{\"name\":\"Target Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Actual Angle\",\"unit\":\"deg\"},"
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
        XM_SendUsbDebugMessage("[VC] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[VC] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);

    s_prev_angle      = XM.status.h10.rightHipMotorAngle;
    s_torque_cmd      = 0.0f;
    s_is_stalled      = false;
    s_last_gait_tick  = XM_GetTick();
    s_prev_gait_cycle = _EstimateGaitCycle();
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[VC] ACTIVE — 가상 구속 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — 가상 구속 PD 제어 (1ms)
 * @details
 * 1. 정지 감지 (gaitCycle 미변화 → 토크=0)
 * 2. gaitCycle → s 변환
 * 3. Bézier 다항식으로 θ_d(s) 계산
 * 4. PD 추종 토크 계산
 * 5. 포화 후 전송
 * 6. LED: 주기 완료(s: 0.95→0.05) 및 추종 오차 표시
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[VC] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    uint8_t gait_cycle = _EstimateGaitCycle();

    /* 정지 판정 — gaitCycle이 변화 없으면 토크 출력 안전 차단 */
    s_is_stalled = _CheckStall(gait_cycle);
    if (s_is_stalled) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        s_torque_cmd = 0.0f;
        _UpdateStreamData(0.0f, 0.0f, XM.status.h10.rightHipMotorAngle);
        return;
    }

    /* 위상 변수 s 계산 */
    float s       = (float)gait_cycle / 100.0f;

    /* 현재 각도 및 속도 (후향 차분) — 우측 기준, 좌측은 _RunActiveLoop에서 동일 적용 */
    float theta   = XM.status.h10.rightHipMotorAngle;
    float theta_dot = (theta - s_prev_angle) / CONTROL_DT;
    s_prev_angle  = theta;

    /* Bézier 다항식으로 목표 각도 계산 */
    float theta_d = _ComputeDesiredAngle(s);

    /* PD 추종 토크 (θ̇_d ≈ 0 단순화) */
    float error   = theta_d - theta;
    float torque_raw = s_kp * error + s_kd * (0.0f - theta_dot);
    s_torque_cmd  = _ClampFloat(torque_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    /* 좌우 동일 적용 */
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);

    /* LED2: 주기 완료 시 토글 (s: 0.95→0.05 전이 감지) */
    static bool s_led2_state = false;
    if (s_prev_gait_cycle > 85U && gait_cycle < 15U) {
        s_led2_state = !s_led2_state;
        XM_SetLedState(XM_LED_2, s_led2_state ? XM_ON : XM_OFF);
    }

    /* LED3: 추종 오차 < 2 deg 이면 ON (추종 양호) */
    XM_SetLedState(XM_LED_3, (fabsf(error) < 2.0f) ? XM_ON : XM_OFF);

    s_prev_gait_cycle = gait_cycle;

    _UpdateUsbDebug(s, theta_d, theta);
    _UpdateStreamData(s, theta_d, theta);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    s_torque_cmd = 0.0f;
    s_is_stalled = false;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[VC] 제어 종료 — 토크 해제\r\n");
}

/**
 * @brief 5차 Bézier 기저 함수 B_{k,5}(s) 계산
 * @param k  제어점 인덱스 (0~5)
 * @param s  위상 변수 (0~1)
 */
static float _BezierBasis(uint8_t k, float s)
{
    /* 이항 계수 C(5,k) = {1, 5, 10, 10, 5, 1} */
    const float binom[BEZIER_CTRL_PTS] = {1.0f, 5.0f, 10.0f, 10.0f, 5.0f, 1.0f};
    return binom[k] * powf(s, (float)k) * powf(1.0f - s, (float)(BEZIER_ORDER - k));
}

/**
 * @brief 5차 Bézier 다항식으로 목표 각도 계산
 * @param s 정규화 위상 (0~1)
 * @return 목표 각도 (deg)
 */
static float _ComputeDesiredAngle(float s)
{
    /* s 범위 제한 (수치 안정성) */
    s = _ClampFloat(s, 0.0f, 1.0f);

    float result = 0.0f;
    const float *alpha = k_bezier_profiles[s_profile_idx];

    for (uint8_t k = 0U; k <= (uint8_t)BEZIER_ORDER; k++) {
        result += _BezierBasis(k, s) * alpha[k];
    }
    return result;
}

/**
 * @brief 정지 판정 — gaitCycle이 STALL_TIMEOUT_MS 이상 변화 없으면 정지
 * @return true = 정지 상태
 */
static bool _CheckStall(uint8_t gait_cycle)
{
    if (gait_cycle != s_prev_gait_cycle) {
        s_last_gait_tick = XM_GetTick();   /* 변화 감지 시 타이머 리셋 */
    }
    return ((XM_GetTick() - s_last_gait_tick) >= STALL_TIMEOUT_MS);
}

static void _HandleButtonInput(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_kp_idx = (s_kp_idx + 1U) % KP_PRESET_COUNT;
        s_kp     = k_kp_presets[s_kp_idx];
        XM_SendUsbDebugMessage("[VC] BTN1: Kp 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_kd_idx = (s_kd_idx + 1U) % KD_PRESET_COUNT;
        s_kd     = k_kd_presets[s_kd_idx];
        XM_SendUsbDebugMessage("[VC] BTN2: Kd 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_profile_idx = (s_profile_idx + 1U) % BEZIER_PROFILE_COUNT;
        XM_SendUsbDebugMessage("[VC] BTN3: 프로파일 전환\r\n");
    }
}

static void _UpdateUsbDebug(float s, float theta_d, float theta)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    char buf[96];
    snprintf(buf, sizeof(buf),
             "VC | s:%.2f θd:%.1f θ:%.1f e:%.1f τ:%.2f Kp:%.1f\r\n",
             s, theta_d, theta, theta_d - theta, s_torque_cmd, s_kp);
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(float s, float theta_d, float theta)
{
    s_stream_data.gait_phase_s = s;
    s_stream_data.theta_d      = theta_d;
    s_stream_data.theta        = theta;
    s_stream_data.torque       = s_torque_cmd;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

static float _ClampFloat(float val, float min_val, float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}

static void _SetupBodyData(void)
{
    uint32_t body_data[8] = {
        70000U, 1750U, 450U, 450U, 420U, 420U, 60U, 60U
    };
    XM_SendUserBodyData(body_data);
}
