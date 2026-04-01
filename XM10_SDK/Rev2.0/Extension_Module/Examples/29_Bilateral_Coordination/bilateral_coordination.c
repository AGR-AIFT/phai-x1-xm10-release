/**
 ******************************************************************************
 * @file    bilateral_coordination.c
 * @author  HyundoKim
 * @brief   [고급] 좌우 협응 제어 (Bilateral Coordination Control)
 * @details
 * 좌우 고관절을 커플링하여 대칭적 보행 패턴을 유도합니다.
 * 편마비(hemiplegia) 재활에서 약측을 건측의 움직임에 동조시키는 핵심 기법입니다.
 *
 * [제어 이론 — 대칭 커플링]
 * 정상 보행에서 좌우 고관절은 대략 역위상(antiphase): θ_R ≈ -θ_L
 *
 * 대칭 이탈 오차:
 *   Δθ_R = θ_R - (-θ_L) = θ_R + θ_L   (대칭 기준에서의 편차)
 *   Δθ_L = θ_L - (-θ_R) = θ_L + θ_R   (대칭 기준에서의 편차)
 *
 * 커플링 복원 토크 (스프링-댐퍼):
 *   τ_R = -K_c·Δθ_R - B_c·(θ̇_R - (-θ̇_L))
 *   τ_L = -K_c·Δθ_L - B_c·(θ̇_L - (-θ̇_R))
 *
 * 약측 강화 모드:
 *   약측 게인 = K_c_strong (더 강한 복원력으로 건측 패턴으로 유도)
 *   건측 게인 = K_c (표준 커플링 유지)
 *
 * [Body Data 선택적 권장]
 * isFootContact, gaitCycle: Body Data 설정 시 비대칭 판단 정확도 향상.
 * 미설정 시에도 순수 각도 기반 커플링은 정상 동작합니다.
 *
 * [논문 레퍼런스]
 * - Duschau-Wicke, A. et al. (2010) "Path control: a method for patient-cooperative
 *   robot-aided gait rehabilitation" IEEE TNSRE, 18(1), 38-48.
 * - Neckel, N. et al. (2008) "Abnormal joint torque patterns exhibited by chronic
 *   stroke subjects while walking" J. NeuroEngineering Rehab, 5(19).
 *
 * [버튼 조작]
 * - BTN1: K_coupling 순환 (0.1→0.2→0.3→0.5 Nm/deg)
 * - BTN2: B_coupling 순환 (0.005→0.01→0.02 Nm·s/deg)
 * - BTN3: 제어 모드 순환 (대칭↔우측 강화↔좌측 강화)
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

#define K_COUPLING_DEFAULT  0.3f    /* 기본 커플링 강성 (Nm/deg) */
#define B_COUPLING_DEFAULT  0.01f   /* 기본 커플링 감쇠 (Nm·s/deg) */
#define K_COUPLING_STRONG   0.5f    /* 약측 강화 게인 (Nm/deg) */
#define MAX_TORQUE_NM       5.0f
#define CONTROL_DT          0.001f
#define USB_DEBUG_PERIOD_MS 500U

/* K_c 프리셋 */
#define K_PRESET_COUNT      4U
static const float k_kc_presets[K_PRESET_COUNT] = {0.1f, 0.2f, 0.3f, 0.5f};

/* B_c 프리셋 */
#define B_PRESET_COUNT      3U
static const float k_bc_presets[B_PRESET_COUNT] = {0.005f, 0.01f, 0.02f};

/* 비대칭 경고 임계값 */
#define ASYMMETRY_THRESHOLD 5.0f    /* |θ_R + θ_L| > 이 값이면 비대칭 경고 */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 제어 모드 */
typedef enum {
    COORD_MODE_SYMMETRIC   = 0, /* 좌우 동일 커플링 */
    COORD_MODE_ASSIST_RIGHT,    /* 우측 강화 (편마비 우측 약측 재활) */
    COORD_MODE_ASSIST_LEFT,     /* 좌측 강화 (편마비 좌측 약측 재활) */
} CoordMode_t;

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float angle_r;      /* 우측 각도 (deg) */
    float angle_l;      /* 좌측 각도 (deg) */
    float torque_rh;    /* 우측 토크 (Nm) */
    float torque_lh;    /* 좌측 토크 (Nm) */
} CoordStreamData_t;

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
static float            s_k_coupling    = K_COUPLING_DEFAULT;
static float            s_b_coupling    = B_COUPLING_DEFAULT;
static uint8_t          s_k_idx         = 2U;  /* 기본 0.3 */
static uint8_t          s_b_idx         = 1U;  /* 기본 0.01 */
static CoordMode_t      s_coord_mode    = COORD_MODE_SYMMETRIC;

/* 각속도 계산용 이전 각도 */
static float            s_prev_angle_r  = 0.0f;
static float            s_prev_angle_l  = 0.0f;

/* 출력 */
static float            s_torque_rh     = 0.0f;
static float            s_torque_lh     = 0.0f;

static uint32_t         s_usb_debug_timer = 0U;
static CoordStreamData_t s_stream_data;

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
static void  _UpdateUsbDebug(float angle_r, float angle_l, float asym);
static void  _UpdateStreamData(float angle_r, float angle_l);
static float _ClampFloat(float val, float min_val, float max_val);
static void  _SetupBodyData(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* Body Data: 권장 (footContact 정확도 향상)
     * 미설정 시에도 순수 각도 기반 커플링은 동작 */
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
        "[{\"name\":\"Angle R\",\"unit\":\"deg\"},"
        "{\"name\":\"Angle L\",\"unit\":\"deg\"},"
        "{\"name\":\"Torque RH\",\"unit\":\"Nm\"},"
        "{\"name\":\"Torque LH\",\"unit\":\"Nm\"}]");
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
        XM_SendUsbDebugMessage("[COORD] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[COORD] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);

    s_prev_angle_r    = XM.status.h10.rightHipMotorAngle;
    s_prev_angle_l    = XM.status.h10.leftHipMotorAngle;
    s_torque_rh       = 0.0f;
    s_torque_lh       = 0.0f;
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[COORD] ACTIVE — 좌우 협응 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — 좌우 협응 토크 (1ms)
 * @details
 * [협응 토크 계산 순서]
 * 1. 좌/우 각도 및 각속도 읽기
 * 2. 대칭 이탈 오차 계산 (Δθ_R = θ_R + θ_L)
 * 3. 모드별 게인 결정 (대칭/우측강화/좌측강화)
 * 4. 커플링 토크 계산
 * 5. 포화 후 전송
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[COORD] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    /* 현재 각도 읽기 */
    float angle_r = XM.status.h10.rightHipMotorAngle;
    float angle_l = XM.status.h10.leftHipMotorAngle;

    /* 각속도 추정 (후향 차분) */
    float vel_r = (angle_r - s_prev_angle_r) / CONTROL_DT;
    float vel_l = (angle_l - s_prev_angle_l) / CONTROL_DT;
    s_prev_angle_r = angle_r;
    s_prev_angle_l = angle_l;

    /* 대칭 이탈 오차 (역위상 기준: θ_R ≈ -θ_L) */
    float asym_r = angle_r + angle_l;   /* Δθ_R = θ_R - (-θ_L) */
    float asym_l = angle_l + angle_r;   /* Δθ_L = θ_L - (-θ_R) */

    /* 모드별 게인 결정 */
    float k_r = s_k_coupling;
    float k_l = s_k_coupling;
    switch (s_coord_mode) {
        case COORD_MODE_ASSIST_RIGHT:
            k_r = K_COUPLING_STRONG;    /* 우측 약측 강화 */
            break;
        case COORD_MODE_ASSIST_LEFT:
            k_l = K_COUPLING_STRONG;    /* 좌측 약측 강화 */
            break;
        default:
            break;   /* 대칭 모드: 동일 게인 */
    }

    /* 커플링 토크 계산 */
    float vel_diff_r = vel_r - (-vel_l);   /* 상대 속도 오차 */
    float vel_diff_l = vel_l - (-vel_r);
    float tau_r_raw = -k_r * asym_r - s_b_coupling * vel_diff_r;
    float tau_l_raw = -k_l * asym_l - s_b_coupling * vel_diff_l;

    /* 포화 후 전송 */
    s_torque_rh = _ClampFloat(tau_r_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    s_torque_lh = _ClampFloat(tau_l_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(s_torque_rh);
    XM_SetAssistTorqueLH(s_torque_lh);

    /* LED2: 모드 표시 */
    switch (s_coord_mode) {
        case COORD_MODE_SYMMETRIC:    XM_SetLedState(XM_LED_2, XM_OFF); break;
        case COORD_MODE_ASSIST_RIGHT: XM_SetLedState(XM_LED_2, XM_ON);  break;
        case COORD_MODE_ASSIST_LEFT:  XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 300); break;
        default: break;
    }

    /* LED3: 비대칭 경고 (|θ_R + θ_L| > 임계값) */
    XM_SetLedState(XM_LED_3, (fabsf(asym_r) > ASYMMETRY_THRESHOLD) ? XM_ON : XM_OFF);

    _UpdateUsbDebug(angle_r, angle_l, asym_r);
    _UpdateStreamData(angle_r, angle_l);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    s_torque_rh = 0.0f;
    s_torque_lh = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[COORD] 제어 종료 — 토크 해제\r\n");
}

static void _HandleButtonInput(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_k_idx      = (s_k_idx + 1U) % K_PRESET_COUNT;
        s_k_coupling = k_kc_presets[s_k_idx];
        XM_SendUsbDebugMessage("[COORD] BTN1: K_c 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_b_idx      = (s_b_idx + 1U) % B_PRESET_COUNT;
        s_b_coupling = k_bc_presets[s_b_idx];
        XM_SendUsbDebugMessage("[COORD] BTN2: B_c 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_coord_mode = (CoordMode_t)(((uint8_t)s_coord_mode + 1U) % 3U);
        XM_SendUsbDebugMessage("[COORD] BTN3: 모드 전환\r\n");
    }
}

static void _UpdateUsbDebug(float angle_r, float angle_l, float asym)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    char buf[96];
    snprintf(buf, sizeof(buf),
             "COORD | θR:%.1f θL:%.1f Asym:%.1f τRH:%.2f τLH:%.2f\r\n",
             angle_r, angle_l, asym, s_torque_rh, s_torque_lh);
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(float angle_r, float angle_l)
{
    s_stream_data.angle_r   = angle_r;
    s_stream_data.angle_l   = angle_l;
    s_stream_data.torque_rh = s_torque_rh;
    s_stream_data.torque_lh = s_torque_lh;
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
