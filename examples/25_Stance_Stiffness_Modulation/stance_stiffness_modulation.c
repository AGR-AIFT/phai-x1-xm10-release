/**
 ******************************************************************************
 * @file    stance_stiffness_modulation.c
 * @author  HyundoKim
 * @brief   [고급] 입각기 가변 강성 제어 (Stance-Phase Stiffness Modulation)
 * @details
 * 보행 주기를 입각기(Stance)와 유각기(Swing)로 구분하여 각 구간에 최적화된
 * 제어 전략을 독립적으로 적용합니다.
 *
 * [제어 이론 — 가변 강성]
 * 입각기 (footContact == true):
 *   τ = K_s·(θ_eq - θ) - B_s·θ̇    (가상 스프링-댐퍼, 체중 지지 보조)
 *
 * 유각기 (footContact == false):
 *   τ = -B_sw·θ̇                    (최소 감쇠만, 자유 운동 허용)
 *
 * 게인 전이 (Smooth Blending, 1st-order LPF):
 *   K_active[k+1] = K_active[k] + α·(K_target - K_active[k])
 *   α = 0.05 → 시정수 ≈ 20ms (급격한 토크 점프 방지)
 *
 * Collins et al. (2015) Nature 논문에서 입각기 탄성 에너지 저장·방출이
 * 보행 대사 비용 감소의 핵심임을 실증하였습니다.
 *
 * [⚠️ Body Data 전제조건 — 필수]
 * isFootContact, gaitCycle은 H10 CM 보행 분석(1kHz) 출력입니다.
 * XM_SendUserBodyData() 미설정 시:
 *   - footContact 미검출 → 입각/유각 구분 불가 → 제어 전략 완전 무효화
 * 이 예제는 Body Data 없이 정상 동작하지 않습니다.
 *
 * [논문 레퍼런스]
 * - Collins, S.H. et al. (2015) "Reducing the energy cost of human walking using
 *   an unpowered exoskeleton" Nature, 522, 212-215.
 * - Mooney, L.M. et al. (2014) "Autonomous exoskeleton reduces metabolic cost
 *   of human walking during load carriage" J. NeuroEngineering Rehab, 11(80).
 *
 * [버튼 조작]
 * - BTN1: K_stance 순환 (0.2→0.4→0.6→0.8 Nm/deg)
 * - BTN2: θ_eq 평형 각도 조절 (−5→0→+5→+10 deg)
 * - BTN3: 입각기 전용 모드 ↔ 항시 모드 전환
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

/* --- 입각기 게인 --- */
#define K_STANCE_DEFAULT    0.4f    /* 기본 입각 강성 (Nm/deg) */
#define B_STANCE_DEFAULT    0.015f  /* 입각 감쇠 (Nm·s/deg) */

/* --- 유각기 게인 --- */
#define B_SWING_DEFAULT     0.005f  /* 유각 최소 감쇠 (Nm·s/deg) */

/* --- 게인 전이 계수 (LPF α) --- */
#define BLEND_ALPHA         0.05f   /* α = 0.05 → 시정수 ≈ 20ms @ 1kHz */

/* --- 안전 한계 --- */
#define MAX_TORQUE_NM       5.0f
#define CONTROL_DT          0.001f
#define USB_DEBUG_PERIOD_MS 500U

/* K_stance 프리셋 */
#define K_PRESET_COUNT      4U
static const float k_stance_presets[K_PRESET_COUNT] = {0.2f, 0.4f, 0.6f, 0.8f};

/* 평형 각도 프리셋 (deg) */
#define EQ_PRESET_COUNT     4U
static const float k_eq_presets[EQ_PRESET_COUNT] = {-5.0f, 0.0f, 5.0f, 10.0f};

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float stance_r;     /* 우측 입각 여부 (1.0=입각, 0.0=유각) */
    float stance_l;     /* 좌측 입각 여부 */
    float torque_rh;    /* 우측 토크 (Nm) */
    float torque_lh;    /* 좌측 토크 (Nm) */
} StanceStreamData_t;

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

/* 파라미터 */
static float        s_k_stance      = K_STANCE_DEFAULT;
static float        s_theta_eq      = 0.0f;
static uint8_t      s_k_idx         = 1U;
static uint8_t      s_eq_idx        = 1U;
static bool         s_stance_only   = true;     /* true = 입각기만 활성화 */

/* 블렌딩된 실시간 게인 (좌/우 독립) */
static float        s_k_blend_r     = 0.0f;
static float        s_k_blend_l     = 0.0f;

/* 각속도 계산용 이전 각도 */
static float        s_prev_angle_r  = 0.0f;
static float        s_prev_angle_l  = 0.0f;

/* 출력 토크 */
static float        s_torque_rh     = 0.0f;
static float        s_torque_lh     = 0.0f;

static uint32_t     s_usb_debug_timer = 0U;
static StanceStreamData_t s_stream_data;

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

static float _ComputeLegTorque(float angle, float prev_angle,
                                bool in_stance, float k_blend);
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(bool stance_r, bool stance_l);
static float _ClampFloat(float val, float min_val, float max_val);
static void  _SetupBodyData(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* ⚠️ Body Data 필수 — footContact 미설정 시 입각/유각 구분 불가 */
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
        "[{\"name\":\"Stance R\",\"unit\":\"bool\"},"
        "{\"name\":\"Stance L\",\"unit\":\"bool\"},"
        "{\"name\":\"RH Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"LH Torque\",\"unit\":\"Nm\"}]");
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
        XM_SendUsbDebugMessage("[STANCE] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[STANCE] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);

    /* 블렌딩 게인 초기화 (0에서 부드럽게 시작) */
    s_k_blend_r     = 0.0f;
    s_k_blend_l     = 0.0f;
    s_prev_angle_r  = XM.status.h10.rightHipMotorAngle;
    s_prev_angle_l  = XM.status.h10.leftHipMotorAngle;
    s_torque_rh     = 0.0f;
    s_torque_lh     = 0.0f;
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[STANCE] ACTIVE — 가변 강성 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — 입각/유각 가변 강성 (1ms)
 * @details
 * 1. 좌/우 발 접촉 상태 읽기
 * 2. 목표 게인 결정 (입각=K_stance, 유각=0)
 * 3. LPF 블렌딩 (급격한 게인 전환 방지)
 * 4. 각 다리 독립 토크 계산 및 전송
 * 5. LED: 입각 상태 표시
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[STANCE] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    bool stance_r = XM.status.h10.isRightFootContact;
    bool stance_l = XM.status.h10.isLeftFootContact;

    /* 목표 강성 결정 (입각기: K_stance, 유각기: 0) */
    float k_target_r = (stance_r || !s_stance_only) ? s_k_stance : 0.0f;
    float k_target_l = (stance_l || !s_stance_only) ? s_k_stance : 0.0f;

    /* LPF 블렌딩으로 부드러운 게인 전이 */
    s_k_blend_r += BLEND_ALPHA * (k_target_r - s_k_blend_r);
    s_k_blend_l += BLEND_ALPHA * (k_target_l - s_k_blend_l);

    /* 각 다리 독립 토크 계산 */
    float angle_r = XM.status.h10.rightHipMotorAngle;
    float angle_l = XM.status.h10.leftHipMotorAngle;

    s_torque_rh = _ComputeLegTorque(angle_r, s_prev_angle_r, stance_r, s_k_blend_r);
    s_torque_lh = _ComputeLegTorque(angle_l, s_prev_angle_l, stance_l, s_k_blend_l);

    s_prev_angle_r = angle_r;
    s_prev_angle_l = angle_l;

    /* 포화 후 전송 */
    s_torque_rh = _ClampFloat(s_torque_rh, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    s_torque_lh = _ClampFloat(s_torque_lh, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(s_torque_rh);
    XM_SetAssistTorqueLH(s_torque_lh);

    /* LED2: 우측 입각 표시, LED3: 좌측 입각 표시 */
    XM_SetLedState(XM_LED_2, stance_r ? XM_ON : XM_OFF);
    XM_SetLedState(XM_LED_3, stance_l ? XM_ON : XM_OFF);

    _UpdateUsbDebug();
    _UpdateStreamData(stance_r, stance_l);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);

    s_k_blend_r = 0.0f;
    s_k_blend_l = 0.0f;
    s_torque_rh = 0.0f;
    s_torque_lh = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[STANCE] 제어 종료 — 토크 해제\r\n");
}

/**
 * @brief 단일 다리 토크 계산 (입각기 스프링-댐퍼 / 유각기 최소 감쇠)
 * @param angle       현재 관절각 (deg)
 * @param prev_angle  이전 관절각 (deg, 속도 계산용)
 * @param in_stance   입각 여부
 * @param k_blend     현재 블렌딩된 강성 게인 (Nm/deg)
 */
static float _ComputeLegTorque(float angle, float prev_angle,
                                bool in_stance, float k_blend)
{
    /* 각속도 추정 (후향 차분) */
    float ang_vel = (angle - prev_angle) / CONTROL_DT;

    float tau = 0.0f;
    if (in_stance) {
        /* 입각기: 가상 스프링-댐퍼 (체중 지지 보조) */
        tau = k_blend * (s_theta_eq - angle) - B_STANCE_DEFAULT * ang_vel;
    } else {
        /* 유각기: 최소 감쇠만 (자유 운동 허용) */
        tau = -B_SWING_DEFAULT * ang_vel;
    }
    return tau;
}

static void _HandleButtonInput(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_k_idx   = (s_k_idx + 1U) % K_PRESET_COUNT;
        s_k_stance = k_stance_presets[s_k_idx];
        XM_SendUsbDebugMessage("[STANCE] BTN1: K_stance 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_eq_idx   = (s_eq_idx + 1U) % EQ_PRESET_COUNT;
        s_theta_eq = k_eq_presets[s_eq_idx];
        XM_SendUsbDebugMessage("[STANCE] BTN2: 평형 각도 변경\r\n");
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_stance_only = !s_stance_only;
        XM_SendUsbDebugMessage("[STANCE] BTN3: 모드 전환\r\n");
    }
}

static void _UpdateUsbDebug(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    char buf[96];
    snprintf(buf, sizeof(buf),
             "STANCE | K:%.2f θeq:%.1f τRH:%.2f τLH:%.2f SR:%d SL:%d\r\n",
             s_k_stance, s_theta_eq, s_torque_rh, s_torque_lh,
             (int)XM.status.h10.isRightFootContact,
             (int)XM.status.h10.isLeftFootContact);
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(bool stance_r, bool stance_l)
{
    s_stream_data.stance_r  = stance_r ? 1.0f : 0.0f;
    s_stream_data.stance_l  = stance_l ? 1.0f : 0.0f;
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
