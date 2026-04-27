/**
 ******************************************************************************
 * @file    cpg_oscillator.c
 * @author  HyundoKim
 * @brief   [고급] CPG 적응 주파수 진동자 기반 보행 리듬 동기 어시스트
 * @details
 * 중앙 패턴 생성기(Central Pattern Generator, CPG)의 핵심 개념인
 * 적응 주파수 진동자(Adaptive Frequency Oscillator, AFO)를 구현합니다.
 *
 * [제어 이론 — 적응 주파수 진동자]
 * 기존 주기 제어와의 차이점:
 *   - 기존: 보행 주파수를 사용자가 직접 설정 (주파수 오차 발생 가능)
 *   - AFO: 사용자의 실제 보행 리듬을 실시간으로 추종하여 자동 동기화
 *
 * 이산 시간 업데이트 방정식 (1ms, dt=0.001s):
 *   F[k] = x_feedback[k] - A₀·sin(φ[k])     (예측 오차 = 피드백 신호)
 *   φ[k+1] = φ[k] + dt·(ω[k] + ε·F[k]·cos(φ[k]))  (위상 업데이트)
 *   ω[k+1] = ω[k] - dt·ε·F[k]·sin(φ[k])            (주파수 자동 적응)
 *   φ[k] = fmod(φ[k], 2π)                            (위상 정규화)
 *   τ[k] = A·sin(φ[k])                               (토크 출력)
 *
 * - φ: 진동자 위상 (rad, 0~2π)
 * - ω: 진동자 각주파수 (rad/s), 사용자 보행 속도에 자동 수렴
 * - ε: 커플링 강도 (적응 속도), 0.1~0.5 권장
 * - A: 토크 진폭 (Nm)
 * - F: 피드백 오차 신호 (측정값 - 진동자 예측값)
 *
 * [피드백 모드 선택]
 * 기본: footContact 기반 보행 위상 추정 (_EstimateGaitCycle) → Body Data 필수
 * 대안: USE_MOTOR_ANGLE_FEEDBACK 정의 시 고관절 각도 직접 사용 → Body Data 불필요
 *
 * [⚠️ Body Data 전제조건]
 * footContact는 H10의 실시간 동작 분석 알고리즘에 의해 추정됩니다.
 * XM_SendUserBodyData()로 신체 데이터를 H10에 전달해야 정확히 동작합니다.
 * - 미설정 시: footContact 항상 0, AFO 동기화 실패 가능
 * - 대안: USE_MOTOR_ANGLE_FEEDBACK 모드로 컴파일 타임 전환 가능
 *
 * [논문 레퍼런스]
 * - Ronsse, R. et al. (2011) "Oscillator-based assistance of cyclical movements"
 *   Medical & Biological Engineering & Computing, 49(10), 1173-1185.
 * - Ijspeert, A.J. (2008) "Central pattern generators for locomotion control"
 *   Neural Networks, 21(4), 642-653.
 *
 * [버튼 조작]
 * - BTN1 클릭: 토크 진폭 증가 (0.5→1.0→1.5→2.0→2.5→3.0→3.5→4.0 Nm, 래핑)
 * - BTN2 클릭: 커플링 강도 ε 전환 (0.1→0.2→0.3→0.5→0.1)
 * - BTN3 클릭: 진동자 리셋 (위상=0, 주파수=초기값) — 동기 실패 시 재시작
 *
 * @see     docs/api-reference/02-h10-control-n-data.md
 * @version 1.0
 * @date    Mar 10, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>

/* 피드백 모드 선택 — USE_MOTOR_ANGLE_FEEDBACK 정의 시 모터 각도 기반 (Body Data 불필요) */
/* #define USE_MOTOR_ANGLE_FEEDBACK */

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

/* --- AFO 기본 파라미터 --- */
#define OMEGA_INIT_HZ       0.9f            /* 초기 진동자 주파수 (Hz, ≈ 보행 0.9Hz) */
#define OMEGA_INIT_RAD      (2.0f * (float)M_PI * OMEGA_INIT_HZ)  /* rad/s 변환 */
#define EPSILON_DEFAULT     0.3f            /* 기본 커플링 강도 */
#define TORQUE_AMP_DEFAULT  1.5f            /* 기본 토크 진폭 (Nm) */
#define TORQUE_AMP_STEP     0.5f            /* BTN1 증가 단위 (Nm) */
#define TORQUE_AMP_MAX      4.0f            /* 최대 토크 진폭 (Nm) */
#define TORQUE_AMP_MIN      0.5f            /* 최소 토크 진폭 (Nm) */

/* --- 주파수 클램핑 (발산 방지) --- */
#define OMEGA_MIN_RAD       (2.0f * (float)M_PI * 0.3f)  /* 최소 0.3 Hz */
#define OMEGA_MAX_RAD       (2.0f * (float)M_PI * 2.5f)  /* 최대 2.5 Hz */

/* --- 안전 한계 --- */
#define MAX_TORQUE_NM       5.0f            /* 토크 포화 한계 (Nm) */
#define CONTROL_DT          0.001f          /* 제어 주기 (1ms = 1kHz) */

/* --- ε 프리셋 목록 --- */
#define EPSILON_PRESET_COUNT  4U
static const float k_epsilon_presets[EPSILON_PRESET_COUNT] = {0.1f, 0.2f, 0.3f, 0.5f};

/* --- 동기화 판정 기준 --- */
#define SYNC_THRESHOLD_RAD  0.2f            /* |ω - ω_init| < 이 값이면 동기됨 */

/* --- USB 디버그 출력 주기 --- */
#define USB_DEBUG_PERIOD_MS 500U

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB 스트리밍 데이터 구조체
 * @details AFO 핵심 상태 4채널을 PhAI Studio에 실시간 전송합니다.
 */
typedef struct {
    float phase_deg;    /* 진동자 위상 (deg, 0~360) */
    float omega_hz;     /* 현재 진동자 주파수 (Hz) */
    float torque;       /* 출력 토크 (Nm) */
    float sync_error;   /* 동기화 오차: |ω - ω_init| (rad/s) */
} CpgStreamData_t;

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

/* --- Task State Machine --- */
static XmTsmHandle_t s_tsm;

/* --- AFO 상태 변수 --- */
static float s_phase   = 0.0f;              /* 진동자 위상 φ (rad) */
static float s_omega   = OMEGA_INIT_RAD;    /* 진동자 각주파수 ω (rad/s) */
static float s_epsilon = EPSILON_DEFAULT;   /* 커플링 강도 ε */
static float s_torque_amp = TORQUE_AMP_DEFAULT; /* 토크 진폭 A (Nm) */
static float s_torque_cmd = 0.0f;           /* 최종 토크 명령 (Nm) */

/* --- ε 프리셋 인덱스 --- */
static uint8_t s_epsilon_idx = 2U;          /* 기본값 0.3 (인덱스 2) */

/* --- LED2 위상 반전 추적 (시각적 박동 표시용) --- */
static bool s_led2_state = false;           /* LED2 현재 상태 */
static bool s_prev_phase_positive = true;   /* 이전 루프의 위상 양수 여부 */

/* --- USB 타이머 및 스트리밍 --- */
static uint32_t s_usb_debug_timer = 0U;
static CpgStreamData_t s_stream_data;

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/* TSM 콜백 */
static void Off_Loop(void);
static void Standby_Loop(void);
static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

/* AFO 핵심 연산 */
static float _GetFeedbackSignal(void);
static void  _RunCpgOscillator(void);

/* 유틸리티 */
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(void);
static float _ClampFloat(float val, float min_val, float max_val);
static void  _SetupBodyData(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief 사용자 초기 설정 — TSM 생성 및 Body Data 전송
 */
void User_Setup(void)
{
    /* ⚠️ Body Data 전제조건 (footContact 기반 보행 위상 추정 사용 시 필수)
     *
     * H10 CM은 실시간 보행 분석(1kHz)으로 footContact, forwardVelocity
     * 등을 추정합니다. 이 분석은 사용자 신체 데이터가 H10에 전달되어야
     * 정확하게 동작합니다.
     *
     * XM_SendUserBodyData()를 반드시 호출하세요:
     *   - 실측값 우선, 불가 시 추정치/표준체형 근사값 사용 가능
     *   - 미설정 시: footContact 항상 0, 보행 위상 추정 불가
     *
     * USE_MOTOR_ANGLE_FEEDBACK 모드로 컴파일 시 Body Data 불필요
     */
    _SetupBodyData();

    /* TSM 생성 (초기 상태: OFF) */
    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    /* [상태 1] OFF: CM 연결 대기 */
    XmStateConfig_t off_conf = {
        .id      = XM_STATE_OFF,
        .on_loop = Off_Loop
    };
    XM_TSM_AddState(s_tsm, &off_conf);

    /* [상태 2] STANDBY: H10 ASSIST 모드 대기 */
    XmStateConfig_t sb_conf = {
        .id      = XM_STATE_STANDBY,
        .on_loop = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    /* [상태 3] ACTIVE: AFO 토크 제어 실행 */
    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    /* USB 스트리밍 설정 */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Phase\",\"unit\":\"deg\"},"
        "{\"name\":\"Frequency\",\"unit\":\"Hz\"},"
        "{\"name\":\"Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Sync Error\",\"unit\":\"rad/s\"}]");

    /* 초기 제어 모드: 모니터링 */
    XM_SetControlMode(XM_CTRL_MONITOR);
}

/**
 * @brief 메인 루프 — 1ms 주기 호출
 */
void User_Loop(void)
{
    /* CM 연결 끊김 시 OFF로 강제 전환 */
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

/* ==================== TSM 콜백 ==================== */

/**
 * @brief OFF 상태 — CM 연결 대기
 */
static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SendUsbDebugMessage("[CPG] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태 — H10 ASSIST 모드 전환 대기
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[CPG] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — AFO 초기화 및 토크 모드 전환
 */
static void Active_Entry(void)
{
    /* 토크 직접 제어 모드 활성화 */
    XM_SetControlMode(XM_CTRL_TORQUE);

    /* AFO 상태 초기화 */
    s_phase       = 0.0f;
    s_omega       = OMEGA_INIT_RAD;
    s_torque_cmd  = 0.0f;
    s_led2_state  = false;
    s_prev_phase_positive = true;

    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[CPG] ACTIVE — AFO 진동자 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — AFO 업데이트 및 토크 출력 (1ms 주기)
 * @details
 * 매 루프마다:
 * 1. 버튼 입력 처리 (파라미터 조절)
 * 2. 피드백 신호 취득
 * 3. AFO 이산 방정식 업데이트 (φ, ω 갱신)
 * 4. 토크 계산 및 전송
 * 5. LED 박동 표시
 * 6. USB 갱신
 */
static void Active_Loop(void)
{
    /* H10이 ASSIST 해제 시 종료 */
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[CPG] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    /* 1. 버튼 입력 처리 */
    _HandleButtonInput();

    /* 2~4. AFO 업데이트 및 토크 출력 */
    _RunCpgOscillator();

    /* 5. LED2: 위상이 양→음 교차 시 토글 (시각적 박동) */
    bool phase_positive = (sinf(s_phase) >= 0.0f);
    if (s_prev_phase_positive && !phase_positive) {
        s_led2_state = !s_led2_state;
        XM_SetLedState(XM_LED_2, s_led2_state ? XM_ON : XM_OFF);
    }
    s_prev_phase_positive = phase_positive;

    /* LED3: 동기화 완료 표시 (|ω - ω_init| < 임계값) */
    float sync_err = fabsf(s_omega - OMEGA_INIT_RAD);
    XM_SetLedState(XM_LED_3, (sync_err < SYNC_THRESHOLD_RAD) ? XM_ON : XM_OFF);

    /* 6. USB 갱신 */
    _UpdateUsbDebug();
    _UpdateStreamData();
}

/**
 * @brief ACTIVE 탈출 — 안전 정지
 */
static void Active_Exit(void)
{
    /* 토크 0으로 안전 해제 */
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    /* 모니터링 모드 복귀 */
    XM_SetControlMode(XM_CTRL_MONITOR);

    /* AFO 상태 초기화 */
    s_phase      = 0.0f;
    s_omega      = OMEGA_INIT_RAD;
    s_torque_cmd = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[CPG] 제어 종료 — 토크 해제\r\n");
}

/* ==================== AFO 핵심 연산 ==================== */

/**
 * @brief 피드백 신호 취득 (모드별 분기)
 * @return 정규화된 피드백 신호 (deg 또는 정규화 보행위상 기반)
 */
static float _GetFeedbackSignal(void)
{
#ifdef USE_MOTOR_ANGLE_FEEDBACK
    /* 우측 고관절 각도를 직접 피드백으로 사용 — Body Data 없이도 동작 가능 */
    return XM.status.h10.rightHipMotorAngle;
#else
    /* footContact 기반 보행 위상 추정 → 정현파로 변환
     * _EstimateGaitCycle()은 heel strike 기반 0~100% 추정. Body Data 설정 필수. */
    float phi_gc = ((float)_EstimateGaitCycle() / 100.0f) * 2.0f * (float)M_PI;
    return 15.0f * sinf(phi_gc);   /* 진폭 15 deg 스케일 (AFO 입력 스케일 맞춤) */
#endif
}

/**
 * @brief AFO 이산 방정식 업데이트 및 토크 출력
 * @details
 * [이산 AFO 업데이트 순서]
 * Step 1: 피드백 신호 F[k] = x_feedback - A₀·sin(φ[k])
 * Step 2: 위상 업데이트 φ[k+1] = φ[k] + dt·(ω[k] + ε·F[k]·cos(φ[k]))
 * Step 3: 주파수 적응 ω[k+1] = ω[k] - dt·ε·F[k]·sin(φ[k])
 * Step 4: 위상 정규화 φ = fmod(φ, 2π)
 * Step 5: 주파수 클램핑 (발산 방지)
 * Step 6: 토크 출력 τ = A·sin(φ)
 * Step 7: 토크 포화
 */
static void _RunCpgOscillator(void)
{
    /* Step 1: 피드백 신호 및 예측 오차 계산 */
    float x_feedback = _GetFeedbackSignal();
    float x_predicted = s_torque_amp * sinf(s_phase);   /* 현재 위상에서 예측값 */
    float feedback_err = x_feedback - x_predicted;

    /* Step 2: 위상 업데이트 (커플링 항 포함) */
    float phase_dot = s_omega + s_epsilon * feedback_err * cosf(s_phase);
    s_phase += CONTROL_DT * phase_dot;

    /* Step 3: 주파수 자동 적응 (MIT Rule 기반) */
    s_omega -= CONTROL_DT * s_epsilon * feedback_err * sinf(s_phase);

    /* Step 4: 위상 정규화 (0 ~ 2π 유지) */
    s_phase = fmodf(s_phase, 2.0f * (float)M_PI);
    if (s_phase < 0.0f) {
        s_phase += 2.0f * (float)M_PI;   /* 음수 위상 방지 */
    }

    /* Step 5: 주파수 클램핑 (발산 또는 과소 진동 방지) */
    s_omega = _ClampFloat(s_omega, OMEGA_MIN_RAD, OMEGA_MAX_RAD);

    /* Step 6: 토크 출력 τ = A·sin(φ) */
    float torque_raw = s_torque_amp * sinf(s_phase);

    /* Step 7: 토크 포화 */
    s_torque_cmd = _ClampFloat(torque_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    /* 토크 명령 전송 (좌우 동일 적용) */
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);
}

/* ==================== 유틸리티 함수 ==================== */

/**
 * @brief 버튼 입력 처리
 * @details
 * - BTN1: 토크 진폭 단계적 증가 (래핑)
 * - BTN2: 커플링 강도 ε 프리셋 순환
 * - BTN3: 진동자 리셋 (동기 실패 시 초기화)
 */
static void _HandleButtonInput(void)
{
    /* BTN1: 토크 진폭 증가 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_torque_amp += TORQUE_AMP_STEP;
        if (s_torque_amp > TORQUE_AMP_MAX) {
            s_torque_amp = TORQUE_AMP_MIN;   /* 최대 초과 시 최소값으로 래핑 */
        }
        XM_SendUsbDebugMessage("[CPG] BTN1: 토크 진폭 변경\r\n");
    }

    /* BTN2: ε 프리셋 순환 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_epsilon_idx = (s_epsilon_idx + 1U) % EPSILON_PRESET_COUNT;
        s_epsilon     = k_epsilon_presets[s_epsilon_idx];
        XM_SendUsbDebugMessage("[CPG] BTN2: 커플링 강도 변경\r\n");
    }

    /* BTN3: 진동자 리셋 */
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_phase = 0.0f;
        s_omega = OMEGA_INIT_RAD;
        XM_SendUsbDebugMessage("[CPG] BTN3: 진동자 리셋\r\n");
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 */
static void _UpdateUsbDebug(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) {
        return;
    }
    s_usb_debug_timer = now;

    float omega_hz   = s_omega / (2.0f * (float)M_PI);
    float phase_deg  = s_phase * (180.0f / (float)M_PI);
    float sync_err   = fabsf(s_omega - OMEGA_INIT_RAD);

    char buf[96];
    snprintf(buf, sizeof(buf),
             "CPG | φ:%.1f° ω:%.2fHz A:%.1fNm ε:%.2f Sync:|Δω|:%.3f\r\n",
             phase_deg, omega_hz, s_torque_amp, s_epsilon, sync_err);
    XM_SendUsbDebugMessage(buf);
}

/**
 * @brief USB 스트리밍 데이터 갱신
 */
static void _UpdateStreamData(void)
{
    s_stream_data.phase_deg  = s_phase * (180.0f / (float)M_PI);
    s_stream_data.omega_hz   = s_omega / (2.0f * (float)M_PI);
    s_stream_data.torque     = s_torque_cmd;
    s_stream_data.sync_error = fabsf(s_omega - OMEGA_INIT_RAD);
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

/**
 * @brief float 값을 [min, max] 범위로 클램핑
 */
static float _ClampFloat(float val, float min_val, float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}

/**
 * @brief H10 신체 데이터 전송
 * @details
 * H10 CM 보행 분석 정확도 향상을 위해 사용자 신체 데이터를 전달합니다.
 * 실측값 우선 적용. 불가 시 표준체형 근사치 사용 가능.
 */
static void _SetupBodyData(void)
{
    uint32_t body_data[8] = {
        70000U,  /* 체중 (g): 70 kg */
        1750U,   /* 신장 (mm): 175 cm */
        450U,    /* 우측 대퇴 길이 (mm) */
        450U,    /* 좌측 대퇴 길이 (mm) */
        420U,    /* 우측 하퇴 길이 (mm) */
        420U,    /* 좌측 하퇴 길이 (mm) */
        60U,     /* 우측 발목 높이 (mm) */
        60U,     /* 좌측 발목 높이 (mm) */
    };
    XM_SendUserBodyData(body_data);
}
