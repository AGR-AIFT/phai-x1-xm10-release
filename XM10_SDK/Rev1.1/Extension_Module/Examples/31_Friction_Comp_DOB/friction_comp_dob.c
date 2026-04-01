/**
 ******************************************************************************
 * @file    friction_comp_dob.c
 * @author  HyundoKim
 * @brief   [고급] 외란 관측기 기반 마찰 보상 투명 모드 Stage 1 완성
 *          (Disturbance Observer for Physical Transparency — Stage 1 Completion)
 * @details
 * Ex.21 (중력+마찰 보상)에 외란 관측기(DOB)를 추가하여 Stage 1을 완성합니다.
 * DOB는 공칭 모델(nominal model: 중력+마찰)로 설명되지 않는 잔류 외란을
 * 실시간으로 추정하여 보상합니다. 이로써 진정한 액추에이터 투명성을 달성합니다.
 *
 * [투명 모드 Stage 진행도]
 * Stage 1a (Ex.21): 중력 보상 + 마찰 보상 (명목 모델)
 * Stage 1b (이 예제): + DOB → 잔류 외란 보상 → 진정한 투명성 달성 ★
 * Stage 2  (Ex.32): 발 접촉 이벤트 기반 보행 의도 감지 (Intent Sensing)
 * Stage 3+: Adaptive Assist, Physical AI...
 *
 * [외란 관측기(DOB) 원리]
 * 공칭 모델: τ_model = τ_grav + τ_fric
 *   → 중력 토크 + 마찰 토크의 합산 (Ex.21과 동일)
 *
 * 실측 토크: τ_meas = Kt_joint · i_meas
 *   → 모터 전류 × 관절 등가 토크 상수로부터 관절 토크 추정
 *   → Kt_joint = Kt_motor × gear_ratio = 0.085 × 18.75 ≈ 1.594 Nm/A (데이터시트 기준)
 *
 * 외란 추정: τ_ext = LPF(τ_meas - τ_model)
 *   → 실측 - 공칭 = 모델이 설명 못한 나머지 (= 외란)
 *   → LPF(Q-filter): 고주파 노이즈 제거, 5Hz 차단 주파수
 *   → 이 신호가 바로 "인간 의도 힘" τ_human을 포함하는 신호!
 *
 * DOB 업데이트 방정식 (1차 IIR, Euler discretization):
 *   d_hat[k] = α_q · d_hat[k-1] + (1 - α_q) · (τ_meas - τ_model)
 *   α_q = 1 - ω_c · dt,  ω_c = 2π · f_c
 *   → f_c = 5Hz일 때: α_q = 1 - (2π·5·0.001) ≈ 0.9686
 *
 * 총 출력 토크: τ_out = τ_model + τ_dob
 *   → 공칭 보상 + 잔류 외란 보상 = 완전한 투명 모드
 *
 * [핵심 인사이트 — Stage 2 연결 고리]
 * τ_ext_est ≈ 인간 의도 힘 (human intent force)
 * → 이 신호를 임계값(threshold)으로 분류하면 "의도 감지"가 됩니다
 * → Stage 2 (Ex.32)에서는 이 신호를 이벤트 트리거로 활용합니다
 * → Physical AI 관점: 이 신호가 VLA 모델의 핵심 입력이 됩니다
 *
 * [에너지 관점]
 * 이상적 투명 모드: 로봇의 임피던스 = 0
 *   Z_effective = Z_human + Z_robot → Z_robot → 0 (DOB 완성 시)
 *   → 착용자는 로봇 없이 움직이는 것과 동일한 에너지 소비
 *
 * @see     Ohnishi, K. et al. (1996) "Microprocessor-controlled DC motor for
 *          load-insensitive position servo system" IEEE Trans. IE, 33(1)
 * @see     Sariyildiz, E. & Ohnishi, K. (2015) "Stability and robustness of
 *          disturbance-observer-based motion control systems" IEEE Trans. IE, 62(1)
 * @see     Ex.21 gravity_compensation.c (공칭 모델 기반)
 * @see     docs/api-reference/XM_Control.md
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

// --- 물리 상수 ---
#define G_ACC                   9.81f       // 중력 가속도 (m/s²)

// --- 공칭 모델 파라미터 (Ex.21과 동일) ---
#define MGL_EFF                 17.16f      // M·g·L = 70kg × 9.81 × 0.25m (≈ 대표값)
                                            // 체중 70kg, 유효 팔 길이 0.25m 기준
#define B_COULOMB_NM            0.3f        // 쿨롱 마찰 계수 (Nm) — 감속기 정마찰
#define B_VISCOUS_NMS           0.01f       // 점성 마찰 계수 (Nm·s/rad) — 속도 비례 마찰

// --- 구동기 사양 (데이터시트 기준) ---
// rightHipTorque / leftHipTorque 필드는 실제로 모터 전류(A)를 담고 있습니다.
// 관절 토크 추정: τ_joint [Nm] = Kt_motor [Nm/A] × gear_ratio × i_motor [A]
#define GEAR_RATIO              18.75f      // 감속비 (양쪽 동일)
#define KT_MOTOR_NM_PER_A       0.085f      // 모터 토크 상수 (데이터시트, Nm/A)
#define KT_JOINT_NM_PER_A       (KT_MOTOR_NM_PER_A * GEAR_RATIO)  // ≈ 1.594 Nm/A (관절 등가 토크 상수)

// --- 안전 한계 ---
// [구동기 스펙] 하드웨어 최대 18.3 Nm, 정격 10 Nm, MD 전류 보호 14A (1.594 Nm/A × 14A ≈ 22.3 Nm)
// [예제 가이드] 정격(10 Nm) 대비 안전 마진을 고려하여 8 Nm으로 제한
//              처음 사용 시 더 낮은 값(예: 3~5 Nm)부터 시작 권장
#define MAX_TORQUE_NM           8.0f        // 토크 포화 한계 (Nm) — 정격 10 Nm 대비 보수적 설정

// --- DOB Q-filter 파라미터 ---
// Q-filter: 외란 추정치의 고주파 노이즈를 제거하는 1차 저역통과 필터
// α_q = 1 - ω_c·dt,  ω_c = 2π·f_c
// f_c = 5Hz일 때: α_q = 1 - (2π·5·0.001) ≈ 0.9686
// f_c가 낮을수록: 노이즈 억제 강, 응답 느림 (추적 지연 증가)
// f_c가 높을수록: 응답 빠름, 노이즈에 민감 (채터링 위험)
#define DOB_CUTOFF_HZ           5.0f        // Q-filter 차단 주파수 기본값 (Hz)
#define PI_VALUE                3.14159265f // π
#define ALPHA_Q                 (1.0f - (2.0f * PI_VALUE * DOB_CUTOFF_HZ * CONTROL_DT))

// --- Q-filter 차단 주파수 프리셋 ---
#define NUM_QF_PRESETS          4           // 프리셋 수: 1Hz, 5Hz, 10Hz, 20Hz

// --- 안전 한계 ---
#define MAX_TORQUE_NM           5.0f        // 토크 포화 한계 (Nm) — 모터/인체 보호

// --- 제어 루프 타이밍 ---
#define CONTROL_DT              0.001f      // 제어 주기 (1ms = 1kHz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS     500         // USB CDC 디버그 메시지 출력 주기 (ms)

// --- 각도 변환 매크로 ---
#define DEG_TO_RAD(d)           ((d) * 0.017453292f)    // π / 180

// --- 속도 데드존 (마찰 보상 시 노이즈 방어) ---
#define VEL_DZ_RADS             0.01f       // 이 속도 미만에서는 마찰 보상 비활성 (rad/s)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details DOB 제어 핵심 변수 4개를 실시간 스트리밍합니다.
 *          PhAI Studio에서 실시간 시각화 가능.
 */
typedef struct {
    float tau_model;    // 공칭 모델 토크 (중력+마찰 합산) [Nm]
    float tau_dob;      // DOB 보상 토크 [Nm]
    float tau_out;      // 총 출력 토크 (model + DOB) [Nm]
    float tau_ext_est;  // 추정된 외부 토크 (≈ 인간 의도 힘) [Nm]
} DobStreamData_t;

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

// --- Task State Machine ---
static XmTsmHandle_t s_tsm;

// --- DOB 상태 변수 ---
static float   s_d_hat_r           = 0.0f;   // 우측 외란 추정치 (Nm)
static float   s_d_hat_l           = 0.0f;   // 좌측 외란 추정치 (Nm)
static bool    s_is_dob_active     = true;   // DOB 활성화 여부 (BTN1 토글)

// --- Q-filter 파라미터 (런타임 변경 가능) ---
// 차단 주파수 프리셋: 낮을수록 부드럽고 지연, 높을수록 빠르고 노이즈 민감
static const float s_qf_presets[NUM_QF_PRESETS] = { 1.0f, 5.0f, 10.0f, 20.0f };
static uint8_t     s_qf_preset_idx              = 1;    // 기본: 5Hz
static float       s_alpha_q                    = ALPHA_Q;  // Q-filter 극점 (IIR 계수)

// --- 각속도 추정용 이전 각도 ---
static float   s_prev_angle_r_rad  = 0.0f;
static float   s_prev_angle_l_rad  = 0.0f;
static bool    s_is_prev_valid     = false;

// --- 제어 변수 (디버그/스트리밍용) ---
static float   s_tau_model_r       = 0.0f;   // 공칭 모델 토크 — 우측 (Nm)
static float   s_tau_dob_r         = 0.0f;   // DOB 보상 토크 — 우측 (Nm)
static float   s_tau_out_r         = 0.0f;   // 총 출력 토크 — 우측 (Nm)

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer  = 0;

// --- USB 스트리밍 데이터 ---
static DobStreamData_t s_stream_data;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

// --- State Machine 콜백 함수 ---
static void Off_Loop(void);
static void Standby_Loop(void);
static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

// --- DOB 제어 ---
static void  _RunDobCompensation(void);
static float _ComputeNominalModel(float angle_rad, float vel_rads);
static float _UpdateDobFilter(float d_hat_prev, float tau_meas, float tau_model);

// --- 유틸리티 ---
static float _ClampFloat(float value, float min_val, float max_val);
static float _SignFloat(float value);
static void  _HandleButtonInput(void);
static void  _UpdateLedIndicators(void);
static void  _UpdateUsbDebug(float angle_r_deg, float angle_l_deg);
static void  _UpdateStreamData(void);
static void  _RecalcAlphaQ(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 사용자 초기 설정 — TSM 생성 및 상태 등록
 */
void User_Setup(void)
{
    // TSM 생성 (초기 상태: OFF — CM 연결 대기)
    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    // [상태 1] OFF: CM 연결 대기
    XmStateConfig_t off_conf = {
        .id      = XM_STATE_OFF,
        .on_loop = Off_Loop
    };
    XM_TSM_AddState(s_tsm, &off_conf);

    // [상태 2] STANDBY: H10 ASSIST 모드 대기
    XmStateConfig_t sb_conf = {
        .id      = XM_STATE_STANDBY,
        .on_loop = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    // [상태 3] ACTIVE: DOB 기반 투명 모드
    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드 0xF1)
    // PhAI Studio에서 채널별 이름과 단위를 표시합니다
    XM_SetUsbCustomMeta(0xF1,
        "[{\"name\":\"Model Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"DOB Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Output Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Ext Est\",\"unit\":\"Nm\"}]");

    // 초기 제어 모드: 모니터링 (토크 미인가)
    XM_SetControlMode(XM_CTRL_MONITOR);
}

/**
 * @brief 메인 루프 — 1ms 주기로 호출됨
 */
void User_Loop(void)
{
    // CM 연결 끊김 시 OFF 상태로 강제 전환 (안전 우선)
    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }

    XM_TSM_Run(s_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

// ==================== Task State Machine 콜백 ====================

/**
 * @brief OFF 상태 — CM 연결 대기
 */
static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SendUsbDebugMessage("[DOB] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태 — H10 ASSIST 모드 진입 대기
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[DOB] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — 토크 제어 모드 전환 및 변수 초기화
 * @details
 * DOB는 이전 추정치(d_hat)를 기억하는 적분기 성격을 가집니다.
 * 상태 진입 시 반드시 0으로 초기화하여 안전한 시작을 보장합니다.
 * (초기값이 크면 순간적인 큰 토크 출력으로 이어질 수 있음)
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // DOB 상태 초기화 (안전 — 초기 큰 외란 추정치 방지)
    s_d_hat_r = 0.0f;
    s_d_hat_l = 0.0f;
    s_is_dob_active = true;

    // Q-filter 기본 파라미터 (5Hz)
    s_qf_preset_idx = 1;
    _RecalcAlphaQ();

    // 제어 변수 초기화
    s_tau_model_r = 0.0f;
    s_tau_dob_r   = 0.0f;
    s_tau_out_r   = 0.0f;

    // 각속도 추정 초기화
    s_prev_angle_r_rad = DEG_TO_RAD(XM.status.h10.rightHipMotorAngle);
    s_prev_angle_l_rad = DEG_TO_RAD(XM.status.h10.leftHipMotorAngle);
    s_is_prev_valid    = false;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED 초기 표시
    _UpdateLedIndicators();

    XM_SendUsbDebugMessage("[DOB] ACTIVE 진입 — DOB ON, Q-filter=5Hz\r\n");
}

/**
 * @brief ACTIVE 루프 — DOB 기반 투명 모드 실시간 제어
 */
static void Active_Loop(void)
{
    // H10이 ASSIST 해제 시 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[DOB] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();
    _RunDobCompensation();
}

/**
 * @brief ACTIVE 탈출 — 안전 정지 절차
 * @details
 * 제어 해제 순서: 토크 0 → 모드 복귀 → 상태 초기화
 * DOB 추정치도 초기화하여 재진입 시 안전 보장
 */
static void Active_Exit(void)
{
    // 토크 즉시 해제
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    // 모니터링 모드로 복귀
    XM_SetControlMode(XM_CTRL_MONITOR);

    // 상태 초기화
    s_d_hat_r         = 0.0f;
    s_d_hat_l         = 0.0f;
    s_tau_model_r     = 0.0f;
    s_tau_dob_r       = 0.0f;
    s_tau_out_r       = 0.0f;
    s_is_prev_valid   = false;

    // LED: STANDBY 표시
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[DOB] 제어 종료 — 토크 해제\r\n");
}

// ==================== DOB 보상 제어 ====================

/**
 * @brief 공칭 모델 토크 계산 (중력 + 마찰)
 * @details
 * τ_model = τ_grav + τ_fric
 *   τ_grav = M·g·L_eff · sin(θ)  (중력 보상 — Ex.21 참조)
 *   τ_fric = B_f·sign(θ̇) + B_v·θ̇  (마찰 보상)
 *
 * 이 함수는 DOB의 "내부 모델(Internal Model)" 역할을 합니다.
 * 내부 모델이 정확할수록 d_hat이 순수 인간 의도 신호에 가까워집니다.
 *
 * @param angle_rad  고관절 각도 (rad)
 * @param vel_rads   고관절 각속도 (rad/s)
 * @return 공칭 모델 토크 (Nm)
 */
static float _ComputeNominalModel(float angle_rad, float vel_rads)
{
    // 중력 토크: M·g·L_eff · sin(θ)
    // MGL_EFF = M · g · L_eff (상수로 사전 계산)
    float tau_grav = MGL_EFF * sinf(angle_rad);

    // 마찰 토크: B_f·sign(θ̇) + B_v·θ̇
    float tau_fric = 0.0f;
    if (fabsf(vel_rads) > VEL_DZ_RADS) {
        // 데드존 외부에서만 마찰 보상 활성화 (정지 시 노이즈 방어)
        tau_fric = B_COULOMB_NM * _SignFloat(vel_rads)
                 + B_VISCOUS_NMS * vel_rads;
    }

    return tau_grav + tau_fric;
}

/**
 * @brief DOB Q-filter 업데이트 (1차 IIR)
 * @details
 * Q-filter는 외란 추정치의 고주파 성분을 제거하는 저역통과 필터입니다.
 *
 * 연속 시간 전달 함수: Q(s) = ω_c / (s + ω_c)
 * Euler 이산화:       d_hat[k] = α_q · d_hat[k-1] + (1-α_q) · (τ_meas - τ_model)
 * 여기서:             α_q = 1 - ω_c·dt = 1 - 2π·f_c·dt
 *
 * 물리적 의미:
 *   (τ_meas - τ_model) = 모델 미설명 잔류 = 외란 + 노이즈
 *   Q-filter 통과 후  = 외란 (저주파 성분) ≈ τ_human (인간 의도)
 *
 * @param d_hat_prev  이전 루프의 외란 추정치 (Nm)
 * @param tau_meas    실측 토크 = Kt · i_meas (Nm)
 * @param tau_model   공칭 모델 토크 (Nm)
 * @return 갱신된 외란 추정치 (Nm)
 */
static float _UpdateDobFilter(float d_hat_prev, float tau_meas, float tau_model)
{
    // 잔류 외란: 실측 - 모델 = 모델이 설명 못한 나머지
    float residual = tau_meas - tau_model;

    // 1차 IIR (Q-filter) 업데이트
    // α_q에 가까울수록 이전 값 유지 → 느린 추적, 노이즈 억제
    // (1-α_q)에 가까울수록 현재 잔류에 빠른 반응 → 빠른 추적
    float d_hat_new = s_alpha_q * d_hat_prev + (1.0f - s_alpha_q) * residual;

    return d_hat_new;
}

/**
 * @brief DOB 기반 투명 모드 실시간 토크 제어 (좌/우 독립)
 * @details
 * [1단계] 센서 데이터 획득 (각도, 전류)
 * [2단계] 각속도 추정 (backward difference)
 * [3단계] 공칭 모델 계산 (τ_model = τ_grav + τ_fric)
 * [4단계] 실측 토크 추정 (τ_meas = Kt × i_meas)
 * [5단계] DOB Q-filter 업데이트 (d_hat 갱신)
 * [6단계] 총 출력 토크 계산 + 포화
 *         τ_out = τ_model + (DOB ON ? d_hat : 0)
 * [7단계] 좌/우 독립 토크 인가
 * [8단계] USB 디버그 및 스트리밍 갱신
 */
static void _RunDobCompensation(void)
{
    // --- 1. 센서 데이터 획득 ---
    float angle_r_deg = XM.status.h10.rightHipMotorAngle;
    float angle_l_deg = XM.status.h10.leftHipMotorAngle;
    float angle_r_rad = DEG_TO_RAD(angle_r_deg);
    float angle_l_rad = DEG_TO_RAD(angle_l_deg);

    // 모터 전류 획득 후 관절 토크로 변환
    // rightHipTorque / leftHipTorque 필드는 실제 단위가 모터 전류(A)입니다.
    // τ_joint [Nm] = Kt_motor [Nm/A] × gear_ratio × i_motor [A]
    //             = KT_JOINT_NM_PER_A × i_motor  (≈ 1.594 × i)
    float current_r_a = XM.status.h10.rightHipTorque;  // 모터 전류 (A), 필드명 주의
    float current_l_a = XM.status.h10.leftHipTorque;
    float tau_meas_r  = KT_JOINT_NM_PER_A * current_r_a;  // 관절 토크 추정 (Nm)
    float tau_meas_l  = KT_JOINT_NM_PER_A * current_l_a;

    // --- 2. 각속도 추정: θ̇ ≈ (θ[k] - θ[k-1]) / dt ---
    float vel_r_rads = 0.0f;
    float vel_l_rads = 0.0f;

    if (s_is_prev_valid) {
        vel_r_rads = (angle_r_rad - s_prev_angle_r_rad) / CONTROL_DT;
        vel_l_rads = (angle_l_rad - s_prev_angle_l_rad) / CONTROL_DT;
    } else {
        // 첫 루프: 속도 추정 불가 — 0으로 시작 (안전)
        s_is_prev_valid = true;
    }

    s_prev_angle_r_rad = angle_r_rad;
    s_prev_angle_l_rad = angle_l_rad;

    // --- 3. 공칭 모델 계산: τ_model = τ_grav + τ_fric ---
    float tau_model_r = _ComputeNominalModel(angle_r_rad, vel_r_rads);
    float tau_model_l = _ComputeNominalModel(angle_l_rad, vel_l_rads);

    // --- 4 & 5. DOB Q-filter 업데이트 ---
    // 매 1ms마다 잔류 외란을 추정하고 IIR 필터로 스무딩
    s_d_hat_r = _UpdateDobFilter(s_d_hat_r, tau_meas_r, tau_model_r);
    s_d_hat_l = _UpdateDobFilter(s_d_hat_l, tau_meas_l, tau_model_l);

    // --- 6. 총 출력 토크: τ_out = τ_model + (DOB ON ? d_hat : 0) ---
    // DOB를 비활성화하면 Ex.21 (공칭 모델만) 동작과 동일
    float tau_dob_r = s_is_dob_active ? s_d_hat_r : 0.0f;
    float tau_dob_l = s_is_dob_active ? s_d_hat_l : 0.0f;

    float tau_raw_r = tau_model_r + tau_dob_r;
    float tau_raw_l = tau_model_l + tau_dob_l;

    // 토크 포화 (Actuator Saturation — 모터 및 인체 보호)
    float tau_out_r = _ClampFloat(tau_raw_r, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    float tau_out_l = _ClampFloat(tau_raw_l, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    // --- 7. 좌/우 독립 토크 인가 ---
    XM_SetAssistTorqueRH(tau_out_r);
    XM_SetAssistTorqueLH(tau_out_l);

    // --- 디버깅용 변수 저장 (우측 기준) ---
    s_tau_model_r = tau_model_r;
    s_tau_dob_r   = tau_dob_r;
    s_tau_out_r   = tau_out_r;

    // --- 8. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug(angle_r_deg, angle_l_deg);
    _UpdateStreamData();
}

// ==================== 유틸리티 함수 ====================

/**
 * @brief float 값을 [min, max] 범위로 클램핑합니다.
 * @param value    입력 값
 * @param min_val  하한
 * @param max_val  상한
 * @return 클램핑된 값
 */
static float _ClampFloat(float value, float min_val, float max_val)
{
    if (value < min_val) {
        return min_val;
    }
    if (value > max_val) {
        return max_val;
    }
    return value;
}

/**
 * @brief float 부호 함수 (signum)
 * @param value  입력 값
 * @return +1.0f (양), -1.0f (음), 0.0f (영)
 */
static float _SignFloat(float value)
{
    if (value > 0.0f) {
        return 1.0f;
    }
    if (value < 0.0f) {
        return -1.0f;
    }
    return 0.0f;
}

/**
 * @brief Q-filter α_q 재계산
 * @details
 * Q-filter 차단 주파수 변경 시 α_q를 즉시 재계산합니다.
 * α_q = 1 - 2π·f_c·dt
 * α_q 범위 보호: [0.0, 1.0] — 음수 또는 1 초과 시 불안정
 */
static void _RecalcAlphaQ(void)
{
    float fc = s_qf_presets[s_qf_preset_idx];
    float alpha = 1.0f - (2.0f * PI_VALUE * fc * CONTROL_DT);
    // α_q는 [0, 1) 범위에서만 안정적인 IIR 필터를 보장
    s_alpha_q = _ClampFloat(alpha, 0.0f, 0.999f);
}

/**
 * @brief 버튼 입력 처리
 * @details
 * - BTN1 클릭: DOB ON/OFF 토글
 *   → OFF 시: 공칭 모델(중력+마찰)만 인가 (Ex.21과 동일)
 *   → ON  시: 공칭 + DOB 보상 (완전한 투명 모드)
 *   → 비교를 통해 DOB의 효과를 체감 가능
 *
 * - BTN2 클릭: Q-filter 차단 주파수 순환 (1→5→10→20→1 Hz)
 *   → 낮은 주파수: 느린 응답, 강한 노이즈 억제
 *   → 높은 주파수: 빠른 응답, 채터링 위험
 *   → 실험을 통해 최적값 탐색
 *
 * - BTN3 클릭: DOB 상태 리셋 (d_hat → 0)
 *   → 외란 추정치가 드리프트(drift)된 경우 초기화
 *   → 토크 급변 없이 점진적 0 수렴 (Q-filter가 완충)
 */
static void _HandleButtonInput(void)
{
    // BTN1: DOB ON/OFF 토글
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_is_dob_active = !s_is_dob_active;

        if (s_is_dob_active) {
            XM_SendUsbDebugMessage("[DOB] BTN1: DOB ON — 완전 투명 모드\r\n");
        } else {
            XM_SendUsbDebugMessage("[DOB] BTN1: DOB OFF — 공칭 모델만 (Ex.21 동작)\r\n");
        }

        _UpdateLedIndicators();
    }

    // BTN2: Q-filter 차단 주파수 순환
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_qf_preset_idx = (s_qf_preset_idx + 1) % NUM_QF_PRESETS;
        _RecalcAlphaQ();

        char buf[72];
        snprintf(buf, sizeof(buf),
                 "[DOB] BTN2: Q-filter f_c=%.0fHz, alpha_q=%.4f\r\n",
                 (double)s_qf_presets[s_qf_preset_idx],
                 (double)s_alpha_q);
        XM_SendUsbDebugMessage(buf);
    }

    // BTN3: DOB 상태 리셋 (d_hat 초기화)
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
        XM_SendUsbDebugMessage("[DOB] BTN3: 외란 추정치 초기화 (d_hat=0)\r\n");
    }
}

/**
 * @brief LED 상태 갱신
 * @details
 * LED1: CM 연결 상태 / DOB 활성 표시 (빠른 깜빡임)
 * LED2: DOB 활성 ON/OFF 표시
 * LED3: Q-filter 프리셋 레벨 표시 (높을수록 빠른 깜빡임)
 */
static void _UpdateLedIndicators(void)
{
    // LED1: ACTIVE 상태 — 빠른 깜빡임 (200ms)
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);

    // LED2: DOB 활성 여부
    if (s_is_dob_active) {
        XM_SetLedState(XM_LED_2, XM_ON);    // DOB ON: 상시 점등
    } else {
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 500);  // DOB OFF: 느린 깜빡임
    }

    // LED3: Q-filter 주파수 레벨 (인덱스가 높을수록 빠른 깜빡임)
    // 1Hz→1000ms, 5Hz→500ms, 10Hz→250ms, 20Hz→125ms
    static const uint16_t led3_periods[NUM_QF_PRESETS] = { 1000, 500, 250, 125 };
    XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, led3_periods[s_qf_preset_idx]);
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 * @details
 * 핵심 출력:
 *   tau_ext_est ≈ 인간 의도 힘 — 이 신호가 Stage 2에서 이벤트 트리거가 됩니다.
 *   DOB ON/OFF 비교: 같은 동작에서 tau_out이 달라지는 것을 관찰하세요.
 *
 * @param angle_r_deg  우측 고관절 각도 (deg)
 * @param angle_l_deg  좌측 고관절 각도 (deg)
 */
static void _UpdateUsbDebug(float angle_r_deg, float angle_l_deg)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        // tau_ext_est ≈ 인간 의도 힘
        // → 이 신호가 Stage 2 의도 감지의 핵심 입력!
        float tau_ext_est = s_d_hat_r;

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "DOB|R:%.1fd|mdl=%.2f|dob=%.2f|out=%.2f|ext=%.2f|fc=%.0f\r\n",
                 (double)angle_r_deg,
                 (double)s_tau_model_r,
                 (double)s_tau_dob_r,
                 (double)s_tau_out_r,
                 (double)tau_ext_est,
                 (double)s_qf_presets[s_qf_preset_idx]);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신 (매 루프, 우측 기준)
 * @details
 * tau_ext_est ≈ 인간 의도 힘 (human intent force)
 * → Stage 2 (grf_gait_intent.c)에서 이 신호를 이벤트 트리거로 활용
 * → Physical AI: 이 신호가 π0 스타일 VLA 모델의 핵심 입력
 */
static void _UpdateStreamData(void)
{
    s_stream_data.tau_model    = s_tau_model_r;
    s_stream_data.tau_dob      = s_tau_dob_r;
    s_stream_data.tau_out      = s_tau_out_r;
    // tau_ext_est ≈ 인간 의도 힘 — 이 신호가 Stage 2가 감지하는 신호입니다
    s_stream_data.tau_ext_est  = s_d_hat_r;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF1);
}
