/**
 ******************************************************************************
 * @file    iterative_learning_control.c
 * @author  HyundoKim
 * @brief   [고급] 반복 학습 제어 (Iterative Learning Control, ILC)
 * @details
 * 매 보행 주기마다 추적 오차를 기록하고, 다음 주기의 토크 프로파일을
 * 점진적으로 개선하는 반복 학습 제어(ILC)를 구현합니다.
 *
 * [제어 이론 — P-type ILC]
 * 보행 주기 k에서의 토크 프로파일: τ_k(i), i = 배열 인덱스 (0~99)
 * 추적 오차: e_k(i) = θ_d(i) - θ_k(i)
 *
 * 학습 업데이트 법칙 (P-type ILC):
 *   τ_{k+1}(i) = τ_k(i) + L·e_k(i)
 *   L: 학습 게인 (0 < L < 2, 수렴 조건)
 *
 * 수렴 보장: |1 - L·G| < 1 (G: 플랜트 DC 게인)
 *   → L이 너무 크면 발산, 너무 작으면 학습 느림
 *   → L = 0.1~0.3 권장 (초기 값)
 *
 * 참조 궤도 (단순 정현파):
 *   θ_d(i) = A_ref·sin(π·(i/99))  (보행 굴곡 참조, deg)
 *
 * 위상 인덱싱:
 *   i = gaitCycle / 100.0 * (ILC_SAMPLES - 1)  (gaitCycle → 배열 인덱스)
 *   새 주기 감지: s_prev_gait > 80 && gait_cycle < 20
 *
 * [⚠️ Body Data 전제조건 — 필수]
 * _EstimateGaitCycle()이 footContact 기반으로 보행 위상(0~100%)을 추정하며,
 * 이것이 ILC 위상 인덱스의 유일한 소스입니다.
 * XM_SendUserBodyData() 미설정 시 footContact 항상 0 → ILC 학습 붕괴.
 * 이 예제는 Body Data 없이 정상 동작하지 않습니다.
 *
 * [논문 레퍼런스]
 * - Emken, J.L. et al. (2007) "Robotic movement training as an optimization
 *   problem: designing a controller that assists only as needed" IEEE ICORR, 307-312.
 * - Bristow, D.A. et al. (2006) "A survey of iterative learning control"
 *   IEEE Control Systems Magazine, 26(3), 96-114.
 *
 * [버튼 조작]
 * - BTN1: 학습 게인 L 순환 (0.05→0.1→0.2→0.3, 4단계)
 * - BTN2: 토크 프로파일 리셋 (배열 전체 0으로 초기화)
 * - BTN3: 학습 ON/OFF 토글 (현재 프로파일 동결)
 *
 * @see     docs/api-reference/02-h10-control-n-data.md
 * @version 1.0
 * @date    Mar 10, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>
#include <string.h>

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

#define ILC_SAMPLES         100U    /* 보행 주기당 샘플 수 (gaitCycle 0~100%) */
#define REF_AMPLITUDE_DEG   10.0f   /* 참조 궤도 진폭 (deg) */
#define MAX_TORQUE_NM       3.0f    /* 포화 한계 (Nm, 학습 중 보수적 설정) */
#define CONTROL_DT          0.001f
#define USB_DEBUG_PERIOD_MS 500U

/* 학습 게인 프리셋 */
#define L_PRESET_COUNT      4U
static const float k_l_presets[L_PRESET_COUNT] = {0.05f, 0.10f, 0.20f, 0.30f};

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float gait_phase_pct;   /* 보행 위상 (%) */
    float theta_d;          /* 참조 각도 (deg) */
    float theta_actual;     /* 실제 각도 (deg) */
    float ilc_torque;       /* ILC 출력 토크 (Nm) */
} IlcStreamData_t;

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

/* ILC 학습 배열 — 우측 기준, 좌측은 미러링 */
static float s_ilc_torque[ILC_SAMPLES];     /* 현재 주기 토크 프로파일 (Nm) */
static float s_ilc_error[ILC_SAMPLES];      /* 현재 주기 오차 누적 (deg) */

/* 학습 파라미터 */
static float    s_learn_gain    = 0.10f;    /* 학습 게인 L */
static uint8_t  s_l_idx         = 1U;       /* 프리셋 인덱스 (기본 0.1) */
static bool     s_is_learning   = true;     /* 학습 활성화 여부 */
static uint32_t s_cycle_count   = 0U;       /* 완료된 보행 주기 수 */

/* 보행 주기 감지 */
static uint8_t  s_prev_gait_cycle = 0U;

/* 출력 */
static float    s_torque_cmd    = 0.0f;

static uint32_t         s_usb_debug_timer = 0U;
static IlcStreamData_t  s_stream_data;

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

static float _ComputeRefAngle(float gait_phase_norm);
static void  _UpdateIlcProfile(void);
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(float phi, float theta_d, float theta);
static void  _UpdateStreamData(float phi, float theta_d, float theta);
static float _ClampFloat(float val, float min_val, float max_val);
static void  _SetupBodyData(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* ⚠️ Body Data 필수 — footContact 기반 보행 위상 추정이 ILC 위상 인덱스 소스 */
    _SetupBodyData();

    /* ILC 배열 초기화 */
    memset(s_ilc_torque, 0, sizeof(s_ilc_torque));
    memset(s_ilc_error,  0, sizeof(s_ilc_error));

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
        "[{\"name\":\"Gait Phase\",\"unit\":\"%\"},"
        "{\"name\":\"Target Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Actual Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"ILC Torque\",\"unit\":\"Nm\"}]");
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
        XM_SendUsbDebugMessage("[ILC] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[ILC] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);
    s_torque_cmd  = 0.0f;
    s_cycle_count = 0U;
    s_prev_gait_cycle = _EstimateGaitCycle();
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, s_is_learning ? XM_ON : XM_OFF);

    XM_SendUsbDebugMessage("[ILC] ACTIVE — 반복 학습 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — ILC 실행 (1ms)
 * @details
 * 1. 새 보행 주기 감지 (gaitCycle 리셋)
 *    → 새 주기 시작 시 이전 주기 오차로 토크 프로파일 업데이트
 * 2. gaitCycle → 배열 인덱스 i 계산
 * 3. 참조 각도 θ_d(i) 계산
 * 4. 오차 e[i] 누적
 * 5. ILC 토크 프로파일 τ[i] 출력
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[ILC] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    uint8_t gait_cycle = _EstimateGaitCycle();

    /* 새 보행 주기 감지 (고치→저치 전환: 80%↑ → 20%↓) */
    if (s_prev_gait_cycle > 80U && gait_cycle < 20U) {
        /* 이전 주기 오차로 ILC 프로파일 업데이트 */
        if (s_is_learning) {
            _UpdateIlcProfile();
            s_cycle_count++;
        }
        /* LED2: 주기 완료 시 토글 */
        static bool s_led2_state = false;
        s_led2_state = !s_led2_state;
        XM_SetLedState(XM_LED_2, s_led2_state ? XM_ON : XM_OFF);

        /* 오차 배열 초기화 (새 주기 준비) */
        memset(s_ilc_error, 0, sizeof(s_ilc_error));
    }

    /* 배열 인덱스 i 계산 (gaitCycle 0~100 → 인덱스 0~99) */
    uint8_t idx = (uint8_t)((float)gait_cycle / 100.0f * (float)(ILC_SAMPLES - 1U));
    if (idx >= ILC_SAMPLES) { idx = ILC_SAMPLES - 1U; }

    /* 현재 각도 및 참조 각도 (gait_cycle은 이미 _EstimateGaitCycle() 결과) */
    float theta   = XM.status.h10.rightHipMotorAngle;
    float phi_norm = (float)gait_cycle / 100.0f;
    float theta_d  = _ComputeRefAngle(phi_norm);

    /* 오차 누적 (이번 주기 각 위상에서의 추적 오차) */
    s_ilc_error[idx] = theta_d - theta;

    /* ILC 토크 출력 (학습된 프로파일 적용) */
    float torque_raw = s_ilc_torque[idx];
    s_torque_cmd = _ClampFloat(torque_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    /* 우측 기준, 좌측 미러링 (동일 적용) */
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);

    s_prev_gait_cycle = gait_cycle;

    _UpdateUsbDebug(phi_norm, theta_d, theta);
    _UpdateStreamData(phi_norm, theta_d, theta);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    s_torque_cmd = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[ILC] 제어 종료 — 토크 해제\r\n");
}

/**
 * @brief 정현파 참조 궤도 계산
 * @param gait_phase_norm 정규화 보행 위상 (0~1)
 * @return 참조 각도 (deg)
 */
static float _ComputeRefAngle(float gait_phase_norm)
{
    /* 단순 정현파: 보행 굴곡 반주기 (입각기 최적 근사) */
    return REF_AMPLITUDE_DEG * sinf((float)M_PI * gait_phase_norm);
}

/**
 * @brief ILC 프로파일 업데이트 (주기 종료 시 호출)
 * @details
 * P-type ILC 업데이트: τ_{k+1}(i) = τ_k(i) + L·e_k(i)
 * 포화 적용으로 발산 방지
 */
static void _UpdateIlcProfile(void)
{
    for (uint8_t i = 0U; i < ILC_SAMPLES; i++) {
        s_ilc_torque[i] += s_learn_gain * s_ilc_error[i];
        /* 토크 프로파일 포화 (안전 한계 유지) */
        s_ilc_torque[i] = _ClampFloat(s_ilc_torque[i], -MAX_TORQUE_NM, MAX_TORQUE_NM);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "[ILC] 주기 %lu 학습 완료 (L=%.2f)\r\n",
             s_cycle_count, s_learn_gain);
    XM_SendUsbDebugMessage(buf);
}

static void _HandleButtonInput(void)
{
    /* BTN1: 학습 게인 순환 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_l_idx      = (s_l_idx + 1U) % L_PRESET_COUNT;
        s_learn_gain = k_l_presets[s_l_idx];
        XM_SendUsbDebugMessage("[ILC] BTN1: 학습 게인 변경\r\n");
    }
    /* BTN2: 프로파일 리셋 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        memset(s_ilc_torque, 0, sizeof(s_ilc_torque));
        memset(s_ilc_error,  0, sizeof(s_ilc_error));
        s_cycle_count = 0U;
        XM_SendUsbDebugMessage("[ILC] BTN2: 프로파일 리셋\r\n");
    }
    /* BTN3: 학습 ON/OFF 토글 */
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_is_learning = !s_is_learning;
        XM_SetLedState(XM_LED_3, s_is_learning ? XM_ON : XM_OFF);
        XM_SendUsbDebugMessage(s_is_learning ?
            "[ILC] BTN3: 학습 활성화\r\n" : "[ILC] BTN3: 학습 동결\r\n");
    }
}

static void _UpdateUsbDebug(float phi, float theta_d, float theta)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    char buf[96];
    snprintf(buf, sizeof(buf),
             "ILC | φ:%.2f θd:%.1f θ:%.1f τ:%.2f 주기:%lu L:%.2f\r\n",
             phi, theta_d, theta, s_torque_cmd, s_cycle_count, s_learn_gain);
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(float phi, float theta_d, float theta)
{
    s_stream_data.gait_phase_pct = phi * 100.0f;
    s_stream_data.theta_d        = theta_d;
    s_stream_data.theta_actual   = theta;
    s_stream_data.ilc_torque     = s_torque_cmd;
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
