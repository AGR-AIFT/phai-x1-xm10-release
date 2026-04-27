/**
 ******************************************************************************
 * @file    admittance_control.c
 * @author  HyundoKim
 * @brief   [고급] 어드미턴스 제어 (Admittance Control)
 * @details
 * 임피던스 제어(Ex.20)의 쌍대(dual)인 어드미턴스 제어를 구현합니다.
 *
 * [임피던스 vs 어드미턴스 — 근본적 차이]
 * 임피던스 (Ex.20): 위치 편차 입력 → 힘(토크) 출력  (위치 제어기 내장)
 * 어드미턴스 (이 예제): 힘(토크) 입력 → 위치 출력  (힘 센서 → 가상 동역학 → 위치 제어기)
 *
 * 어드미턴스는 강성 구동기(stiff actuator)에 더 적합합니다.
 * 사용자가 밀면 로봇이 자연스럽게 따라가는 "유순한" 동작을 구현합니다.
 *
 * [제어 이론 — 어드미턴스 루프]
 * 외력 입력: τ_ext = 측정된 고관절 토크 (XM.status.h10.rightHipTorque)
 *
 * 가상 질량-스프링-댐퍼 응답 (오일러 적분):
 *   M_v · Δθ̈ + B_v · Δθ̇ + K_v · Δθ = τ_ext
 *
 * 이산화 (Forward Euler, 1ms):
 *   vel[k+1] = vel[k] + dt·(τ_ext - K_v·pos[k] - B_v·vel[k]) / M_v
 *   pos[k+1] = pos[k] + dt·vel[k+1]
 *
 * 위치 참조 (평형점 + 편차):
 *   θ_ref = θ_eq + pos[k]
 *
 * 내부 위치 제어기 (PD):
 *   τ_cmd = Kp_pos·(θ_ref - θ) + Kd_pos·(θ̇_ref - θ̇)
 *
 * [Body Data 여부]
 * 이 예제는 Body Data 없이 동작합니다.
 * 측정 토크(rightHipTorque, leftHipTorque)만으로 외력을 추정합니다.
 *
 * [논문 레퍼런스]
 * - Keemink, A.Q.L. et al. (2018) "Admittance control for physical human-robot
 *   interaction" The International Journal of Robotics Research, 37(11), 1421-1444.
 * - Hogan, N. (1985) "Impedance Control: An Approach to Manipulation"
 *   ASME Journal of Dynamic Systems, Measurement, and Control, 107(1), 1-24.
 *
 * [버튼 조작]
 * - BTN1: 가상 관성 M_v 순환 (0.2→0.5→1.0→2.0 Nm·s²/rad)
 * - BTN2: 가상 감쇠 B_v 순환 (1.0→2.0→4.0 Nm·s/rad)
 * - BTN3: 어드미턴스 상태 리셋 (pos=0, vel=0)
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

/* --- 가상 동역학 파라미터 --- */
#define M_VIRTUAL_DEFAULT   0.5f    /* 기본 가상 관성 (Nm·s²/rad) */
#define B_VIRTUAL_DEFAULT   2.0f    /* 기본 가상 감쇠 (Nm·s/rad) */
#define K_VIRTUAL           5.0f    /* 평형 복원 강성 (Nm/rad) */

/* --- 내부 위치 제어기 게인 --- */
#define KP_POS              0.3f    /* 위치 비례 게인 (Nm/deg) */
#define KD_POS              0.01f   /* 위치 미분 게인 (Nm·s/deg) */

/* --- 어드미턴스 편차 안전 한계 --- */
#define MAX_POS_DEV_DEG     20.0f   /* 최대 허용 위치 편차 (deg) */
#define MAX_TORQUE_NM       5.0f    /* 토크 포화 한계 (Nm) */

/* --- 단위 변환 (deg ↔ rad) --- */
#define DEG_TO_RAD(x)       ((x) * 0.017453292f)
#define RAD_TO_DEG(x)       ((x) * 57.295779513f)

#define CONTROL_DT          0.001f
#define USB_DEBUG_PERIOD_MS 500U

/* M_v 프리셋 */
#define M_PRESET_COUNT      4U
static const float k_m_presets[M_PRESET_COUNT] = {0.2f, 0.5f, 1.0f, 2.0f};

/* B_v 프리셋 */
#define B_PRESET_COUNT      3U
static const float k_b_presets[B_PRESET_COUNT] = {1.0f, 2.0f, 4.0f};

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 단일 다리 어드미턴스 상태 */
typedef struct {
    float pos;      /* 어드미턴스 위치 편차 Δθ (deg) */
    float vel;      /* 어드미턴스 속도 편차 Δθ̇ (deg/s) */
    float prev_ang; /* 이전 관절각 (속도 계산용, deg) */
} AdmState_t;

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float tau_ext;      /* 외력 토크 (Nm) */
    float delta_theta;  /* 위치 편차 (deg) */
    float theta_ref;    /* 위치 참조 (deg) */
    float tau_cmd;      /* 출력 토크 (Nm) */
} AdmStreamData_t;

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

static XmTsmHandle_t  s_tsm;

/* 파라미터 */
static float    s_m_virtual = M_VIRTUAL_DEFAULT;
static float    s_b_virtual = B_VIRTUAL_DEFAULT;
static uint8_t  s_m_idx     = 1U;
static uint8_t  s_b_idx     = 1U;

/* 좌/우 독립 어드미턴스 상태 */
static AdmState_t s_adm_r;
static AdmState_t s_adm_l;

/* 평형 각도 (진입 시 기준점으로 설정) */
static float s_theta_eq_r = 0.0f;
static float s_theta_eq_l = 0.0f;

/* 출력 */
static float s_torque_rh  = 0.0f;
static float s_torque_lh  = 0.0f;

static uint32_t       s_usb_debug_timer = 0U;
static AdmStreamData_t s_stream_data;

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

static float _ComputeAdmittanceTorque(AdmState_t *state, float tau_ext,
                                       float theta, float theta_eq);
static void  _ResetAdmState(AdmState_t *state, float current_angle);
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(float tau_ext_r, float tau_cmd_r);
static float _ClampFloat(float val, float min_val, float max_val);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* Body Data: 이 예제는 Body Data 없이 동작 가능
     * 측정 토크(rightHipTorque, leftHipTorque)를 외력으로 사용 */

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
        "[{\"name\":\"Ext Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Delta Theta\",\"unit\":\"deg\"},"
        "{\"name\":\"Theta Ref\",\"unit\":\"deg\"},"
        "{\"name\":\"Cmd Torque\",\"unit\":\"Nm\"}]");
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
        XM_SendUsbDebugMessage("[ADM] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[ADM] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);

    /* 현재 각도를 평형점으로 설정 */
    s_theta_eq_r = XM.status.h10.rightHipMotorAngle;
    s_theta_eq_l = XM.status.h10.leftHipMotorAngle;

    /* 어드미턴스 상태 초기화 */
    _ResetAdmState(&s_adm_r, s_theta_eq_r);
    _ResetAdmState(&s_adm_l, s_theta_eq_l);

    s_torque_rh       = 0.0f;
    s_torque_lh       = 0.0f;
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[ADM] ACTIVE — 어드미턴스 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — 어드미턴스 제어 (1ms)
 * @details
 * [어드미턴스 루프 실행 순서]
 * 1. 외력(τ_ext) = 측정 토크 읽기
 * 2. 가상 동역학 업데이트 (M_v, B_v, K_v)
 * 3. 위치 참조 θ_ref = θ_eq + Δθ 계산
 * 4. 내부 PD 위치 제어기로 τ_cmd 계산
 * 5. 포화 후 전송
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[ADM] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    /* 외력 입력 (측정 토크) */
    float tau_ext_r = XM.status.h10.rightHipTorque;
    float tau_ext_l = XM.status.h10.leftHipTorque;

    float theta_r = XM.status.h10.rightHipMotorAngle;
    float theta_l = XM.status.h10.leftHipMotorAngle;

    /* 좌/우 독립 어드미턴스 토크 계산 */
    s_torque_rh = _ComputeAdmittanceTorque(&s_adm_r, tau_ext_r, theta_r, s_theta_eq_r);
    s_torque_lh = _ComputeAdmittanceTorque(&s_adm_l, tau_ext_l, theta_l, s_theta_eq_l);

    /* LED2: 외력 감지 시 ON (|τ_ext| > 0.5 Nm) */
    XM_SetLedState(XM_LED_2, (fabsf(tau_ext_r) > 0.5f) ? XM_ON : XM_OFF);

    /* LED3: 위치 편차 표시 (|Δθ| > 5 deg → 빠른 점멸) */
    if (fabsf(s_adm_r.pos) > 5.0f) {
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);
    } else {
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 800);
    }

    _UpdateUsbDebug();
    _UpdateStreamData(tau_ext_r, s_torque_rh);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);

    /* 어드미턴스 상태 리셋 */
    s_adm_r.pos = 0.0f; s_adm_r.vel = 0.0f;
    s_adm_l.pos = 0.0f; s_adm_l.vel = 0.0f;
    s_torque_rh = 0.0f;
    s_torque_lh = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedEffect(XM_LED_2, XM_LED_OFF, 0);
    XM_SetLedEffect(XM_LED_3, XM_LED_OFF, 0);

    XM_SendUsbDebugMessage("[ADM] 제어 종료 — 토크 해제\r\n");
}

/**
 * @brief 단일 다리 어드미턴스 토크 계산
 * @param state     어드미턴스 상태 (pos, vel, prev_ang)
 * @param tau_ext   외력 토크 입력 (Nm)
 * @param theta     현재 각도 (deg)
 * @param theta_eq  평형 각도 (deg)
 * @return 출력 토크 (Nm)
 */
static float _ComputeAdmittanceTorque(AdmState_t *state, float tau_ext,
                                       float theta, float theta_eq)
{
    /* Step 1: 가상 동역학 업데이트 (Forward Euler, 1ms)
     *   M_v · Δθ̈ = τ_ext - K_v·Δθ - B_v·Δθ̇ */
    float acc = (tau_ext
                 - K_VIRTUAL  * DEG_TO_RAD(state->pos)
                 - s_b_virtual * DEG_TO_RAD(state->vel))
                / s_m_virtual;  /* 단위: rad/s² */

    state->vel += CONTROL_DT * RAD_TO_DEG(acc);  /* deg/s 단위 유지 */
    state->pos += CONTROL_DT * state->vel;

    /* Step 2: 위치 편차 범위 제한 (안전) */
    state->pos = _ClampFloat(state->pos, -MAX_POS_DEV_DEG, MAX_POS_DEV_DEG);

    /* Step 3: 위치 참조 계산 */
    float theta_ref = theta_eq + state->pos;

    /* Step 4: 내부 PD 위치 제어기 */
    float pos_error = theta_ref - theta;
    float theta_dot = (theta - state->prev_ang) / CONTROL_DT;
    float tau_raw   = KP_POS * pos_error + KD_POS * (0.0f - theta_dot);

    state->prev_ang = theta;

    /* Step 5: 포화 후 전송 */
    float tau_out = _ClampFloat(tau_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(s_torque_rh); /* 실제 전송은 호출자에서 */
    return tau_out;
}

static void _ResetAdmState(AdmState_t *state, float current_angle)
{
    state->pos      = 0.0f;
    state->vel      = 0.0f;
    state->prev_ang = current_angle;
}

static void _HandleButtonInput(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_m_idx     = (s_m_idx + 1U) % M_PRESET_COUNT;
        s_m_virtual = k_m_presets[s_m_idx];
        XM_SendUsbDebugMessage("[ADM] BTN1: M_v 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_b_idx     = (s_b_idx + 1U) % B_PRESET_COUNT;
        s_b_virtual = k_b_presets[s_b_idx];
        XM_SendUsbDebugMessage("[ADM] BTN2: B_v 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        /* 어드미턴스 상태 리셋 */
        _ResetAdmState(&s_adm_r, XM.status.h10.rightHipMotorAngle);
        _ResetAdmState(&s_adm_l, XM.status.h10.leftHipMotorAngle);
        XM_SendUsbDebugMessage("[ADM] BTN3: 어드미턴스 리셋\r\n");
    }
}

static void _UpdateUsbDebug(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    char buf[96];
    snprintf(buf, sizeof(buf),
             "ADM | τext:%.2f Δθ:%.1f τRH:%.2f τLH:%.2f Mv:%.1f Bv:%.1f\r\n",
             XM.status.h10.rightHipTorque, s_adm_r.pos,
             s_torque_rh, s_torque_lh, s_m_virtual, s_b_virtual);
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(float tau_ext_r, float tau_cmd_r)
{
    s_stream_data.tau_ext     = tau_ext_r;
    s_stream_data.delta_theta = s_adm_r.pos;
    s_stream_data.theta_ref   = s_theta_eq_r + s_adm_r.pos;
    s_stream_data.tau_cmd     = tau_cmd_r;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

static float _ClampFloat(float val, float min_val, float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}
