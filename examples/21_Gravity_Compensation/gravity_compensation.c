/**
 ******************************************************************************
 * @file    gravity_compensation.c
 * @author  HyundoKim
 * @brief   [고급] 중력 보상 기반 투명 모드 (Gravity Compensation for Transparency)
 * @details
 * H10 고관절 외골격 로봇의 "투명 모드(Transparency Mode)"를 구현합니다.
 * 투명 모드의 목표는 로봇을 "보이지 않는 존재"로 만드는 것입니다:
 * 로봇이 발생시키는 중력 토크와 마찰 토크를 정확히 상쇄하여,
 * 사용자가 로봇을 착용하지 않은 것처럼 자유롭게 움직일 수 있게 합니다.
 *
 * [투명 모드 vs 역진자 안정화 (Ex.15) — 핵심 차이]
 * Ex.15 (역진자): 특정 평형점으로 복원하는 것이 목적 (PD 안정화)
 *   → 위치 추적 + 복원력 존재 → 로봇이 사용자를 "잡아끔"
 * Ex.21 (투명): 모든 로봇 기인 힘을 상쇄하는 것이 목적 (중력+마찰 보상)
 *   → 위치 추적 없음, 복원력 없음 → 로봇이 "사라짐"
 *
 * [제어 법칙]
 * 1. 중력 토크 상쇄 (Gravity Compensation):
 *    τ_gravity = M · g · L_eff · sin(θ)
 *    → 로봇+다리 질량으로 인한 중력 토크를 정확히 상쇄
 *    → θ: 수직 기준 고관절 각도 (rad)
 *    → L_eff: 질량 중심까지의 유효 길이 (고관절~대퇴부 CoM)
 *
 * 2. 마찰 보상 (Friction Compensation):
 *    τ_friction = B_f · sign(θ̇) + B_v · θ̇
 *    → B_f: 쿨롱(Coulomb) 마찰 — 속도 방향과 무관한 일정 크기 마찰
 *    → B_v: 점성(Viscous) 마찰 — 속도에 비례하는 마찰
 *    → 감속기/베어링의 기계적 마찰을 상쇄하여 백드라이버빌리티 향상
 *
 * 3. 총 보상 토크:
 *    τ = α · (τ_gravity + τ_friction)
 *    → α: 보상 비율 (0.0~1.0), BTN1으로 0.1 단위 조절
 *    → 안전을 위해 50%에서 시작, 점진적 증가 권장
 *
 * [에너지 관점]
 * 중력 보상은 위치 에너지(potential energy)를 제거하고,
 * 마찰 보상은 에너지 소산(dissipation)을 제거합니다.
 * 이상적으로 α=1.0에서 로봇은 에너지 중립(energy-neutral) 상태가 됩니다.
 *
 * @see     Just, F. et al. (2018) "Human arm weight compensation in
 *          rehabilitation robotics" J. NeuroEngineering and Rehabilitation, 15(28)
 * @see     Vallery, H. et al. (2008) "Compliant actuation of rehabilitation
 *          robots" IEEE RAM, 15(3), 60-69
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

// --- 신체/로봇 모델 파라미터 ---
#define M_BODY_DEFAULT_KG       70.0f       // 기본 체중 (kg) — BTN2로 프리셋 순환
#define L_EFF_M                 0.25f       // 유효 다리 길이: 고관절~대퇴부 CoM (m)
#define NUM_MASS_PRESETS        5           // 체중 프리셋 수 (50, 60, 70, 80, 90 kg)

// --- 마찰 보상 파라미터 ---
#define B_COULOMB_NM            0.3f        // 쿨롱 마찰 계수 (Nm) — 감속기 정마찰
#define B_VISCOUS_NMS           0.01f       // 점성 마찰 계수 (Nm·s/rad) — 속도 비례 마찰

// --- 보상 비율 (Compensation Ratio) ---
#define ALPHA_INIT              0.5f        // 초기 보상 비율 (50%) — 안전 마진
#define ALPHA_STEP              0.1f        // BTN1 클릭당 증가량
#define ALPHA_MAX               1.0f        // 최대 보상 비율 (100%)
#define NUM_ALPHA_STEPS         11          // 0.0, 0.1, ..., 1.0 → 11단계

// --- 안전 한계 ---
#define MAX_TORQUE_NM           5.0f        // 토크 포화 한계 (Nm) — 모터/인체 보호

// --- 제어 루프 타이밍 ---
#define CONTROL_DT              0.001f      // 제어 주기 (1ms = 1kHz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS     500         // USB CDC 디버그 메시지 출력 주기 (ms)

// --- 각도 변환 매크로 ---
#define DEG_TO_RAD(d)           ((d) * 0.017453292f)    // π / 180

// --- 속도 데드존 (마찰 보상 시 노이즈 방어) ---
#define VELOCITY_DEADZONE_RADS  0.01f       // 이 속도 미만에서는 마찰 보상 비활성 (rad/s)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details 중력 보상 제어 핵심 변수 4개를 실시간 스트리밍합니다.
 */
typedef struct {
    float tau_gravity;      // 중력 보상 토크 (Nm)
    float tau_friction;     // 마찰 보상 토크 (Nm)
    float tau_total;        // 총 보상 토크 (Nm)
    float alpha;            // 현재 보상 비율 (0.0~1.0)
} GcStreamData_t;

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

// --- 신체 모델 파라미터 ---
static const float s_mass_presets[NUM_MASS_PRESETS] = { 50.0f, 60.0f, 70.0f, 80.0f, 90.0f };
static uint8_t s_mass_preset_idx    = 2;            // 기본값 인덱스: 70kg
static float s_body_mass_kg         = M_BODY_DEFAULT_KG;

// --- 보상 비율 ---
static float s_alpha                = ALPHA_INIT;   // 보상 비율 (0.0~1.0)
static uint8_t s_alpha_step_idx     = 5;            // 0.5 = 5번째 스텝

// --- 마찰 보상 토글 ---
static bool s_is_friction_comp_on   = true;         // 마찰 보상 활성화 여부

// --- 제어 변수 ---
static float s_tau_gravity          = 0.0f;         // 중력 보상 토크 (Nm)
static float s_tau_friction         = 0.0f;         // 마찰 보상 토크 (Nm)
static float s_tau_total            = 0.0f;         // 총 보상 토크 (Nm)

// --- 속도 추정용 이전 각도 (backward difference) ---
static float s_prev_angle_r_rad     = 0.0f;         // 이전 루프 우측 고관절 각도 (rad)
static float s_prev_angle_l_rad     = 0.0f;         // 이전 루프 좌측 고관절 각도 (rad)
static bool s_is_prev_angle_valid   = false;        // 첫 루프 마킹 (초기값 유효성)

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer   = 0;

// --- USB 스트리밍 데이터 ---
static GcStreamData_t s_stream_data;

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

// --- 중력 보상 제어 ---
static void _RunGravityCompensation(void);

// --- 유틸리티 함수 ---
static float _ClampFloat(float value, float min_val, float max_val);
static float _SignFloat(float value);
static void _HandleButtonInput(void);
static void _UpdateLedIndicators(void);
static void _UpdateUsbDebug(float angle_r_deg, float angle_l_deg,
                             float vel_r_rads, float vel_l_rads);
static void _UpdateStreamData(void);

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
        .id = XM_STATE_OFF,
        .on_loop = Off_Loop
    };
    XM_TSM_AddState(s_tsm, &off_conf);

    // [상태 2] STANDBY: H10 ASSIST 모드 대기
    XmStateConfig_t sb_conf = {
        .id = XM_STATE_STANDBY,
        .on_loop = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    // [상태 3] ACTIVE: 중력 보상 투명 모드
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드)
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Gravity Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Friction Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Total Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Alpha\",\"unit\":\"-\"}]");

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
        XM_SendUsbDebugMessage("[GC] CM 연결됨 -> STANDBY\r\n");
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
        XM_SendUsbDebugMessage("[GC] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — 토크 제어 모드 전환 및 변수 초기화
 * @details
 * 투명 모드는 Homing이 불필요합니다 (위치 추적을 하지 않으므로).
 * 즉시 토크 제어 모드로 전환하여 중력/마찰 보상을 시작합니다.
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // 보상 비율 초기화 (50% — 안전 시작)
    s_alpha_step_idx = 5;
    s_alpha = ALPHA_INIT;

    // 체중 프리셋 초기화 (70kg)
    s_mass_preset_idx = 2;
    s_body_mass_kg = s_mass_presets[s_mass_preset_idx];

    // 마찰 보상 기본 활성화
    s_is_friction_comp_on = true;

    // 제어 변수 초기화
    s_tau_gravity  = 0.0f;
    s_tau_friction = 0.0f;
    s_tau_total    = 0.0f;

    // 속도 추정용 이전 각도 초기화
    s_prev_angle_r_rad = DEG_TO_RAD(XM.status.h10.rightHipMotorAngle);
    s_prev_angle_l_rad = DEG_TO_RAD(XM.status.h10.leftHipMotorAngle);
    s_is_prev_angle_valid = false;  // 첫 루프에서는 속도 추정 불가

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED 초기 표시
    _UpdateLedIndicators();

    XM_SendUsbDebugMessage("[GC] ACTIVE 진입 — 중력 보상 시작 (alpha=0.5)\r\n");
}

/**
 * @brief ACTIVE 루프 — 중력+마찰 보상 실시간 제어
 */
static void Active_Loop(void)
{
    // H10이 ASSIST 해제 시 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[GC] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();
    _RunGravityCompensation();
}

/**
 * @brief ACTIVE 탈출 — 안전 정지 절차
 * @details
 * 토크를 0으로 리셋하고 모니터링 모드로 복귀합니다.
 * 투명 모드는 임피던스를 사용하지 않으므로 IVector 해제 불필요.
 */
static void Active_Exit(void)
{
    // 토크 즉시 해제
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    // 모니터링 모드로 복귀
    XM_SetControlMode(XM_CTRL_MONITOR);

    // 제어 변수 초기화
    s_tau_gravity  = 0.0f;
    s_tau_friction = 0.0f;
    s_tau_total    = 0.0f;
    s_is_prev_angle_valid = false;

    // LED: STANDBY 표시
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[GC] 제어 종료 — 토크 해제\r\n");
}

// ==================== 중력 보상 제어 ====================

/**
 * @brief 중력+마찰 보상 실시간 토크 제어 (좌/우 독립)
 * @details
 * 매 1ms 루프마다 좌/우 고관절 각각에 대해 다음을 수행합니다:
 *
 * [1단계] 센서 데이터 획득
 *   θ = 고관절 모터 각도 (deg → rad 변환)
 *   수직 기준으로 양(+)이 굴곡(flexion), 음(-)이 신전(extension)
 *
 * [2단계] 각속도 추정 (backward difference)
 *   θ̇ ≈ (θ[k] - θ[k-1]) / dt
 *   → 센서 각속도가 없으므로 수치 미분으로 추정
 *   → dt = 1ms, 1차 후방 차분법 (first-order backward difference)
 *
 * [3단계] 중력 토크 상쇄
 *   τ_gravity = M · g · L_eff · sin(θ)
 *   → M: 로봇+다리 합산 질량 (체중의 일부, 프리셋으로 조절)
 *   → L_eff: 고관절에서 대퇴부 질량 중심(CoM)까지의 거리
 *   → sin(θ): 중력 성분의 레버암 비율
 *   → 부호: 중력 방향과 반대로 토크를 인가하여 상쇄
 *
 * [4단계] 마찰 토크 상쇄 (옵션, BTN3 토글)
 *   τ_friction = B_f · sign(θ̇) + B_v · θ̇
 *   → 쿨롱 마찰: 운동 방향의 일정 크기 저항 (정마찰)
 *   → 점성 마찰: 속도에 비례하는 저항 (동마찰)
 *   → 데드존 적용: |θ̇| < 임계값이면 마찰 보상 비활성 (노이즈 방어)
 *
 * [5단계] 총 보상 토크 계산 + 포화
 *   τ = α · (τ_gravity + τ_friction)
 *   τ_out = clamp(τ, -MAX_TORQUE, +MAX_TORQUE)
 */
static void _RunGravityCompensation(void)
{
    // --- 1. 센서 데이터 획득 (deg → rad) ---
    float angle_r_deg = XM.status.h10.rightHipMotorAngle;
    float angle_l_deg = XM.status.h10.leftHipMotorAngle;
    float angle_r_rad = DEG_TO_RAD(angle_r_deg);
    float angle_l_rad = DEG_TO_RAD(angle_l_deg);

    // --- 2. 각속도 추정: θ̇ ≈ (θ[k] - θ[k-1]) / dt ---
    float vel_r_rads = 0.0f;
    float vel_l_rads = 0.0f;

    if (s_is_prev_angle_valid) {
        // backward difference (1차 후방 차분)
        vel_r_rads = (angle_r_rad - s_prev_angle_r_rad) / CONTROL_DT;
        vel_l_rads = (angle_l_rad - s_prev_angle_l_rad) / CONTROL_DT;
    } else {
        // 첫 루프: 속도 추정 불가 — 0으로 시작 (안전)
        s_is_prev_angle_valid = true;
    }

    // 이전 각도 갱신 (다음 루프의 미분에 사용)
    s_prev_angle_r_rad = angle_r_rad;
    s_prev_angle_l_rad = angle_l_rad;

    // --- 3. 중력 토크 상쇄: τ_gravity = M · g · L_eff · sin(θ) ---
    // 부호 약속: sin(θ) > 0 (굴곡)이면 중력이 신전 방향으로 작용
    //           → 굴곡 방향(양)으로 토크를 인가하여 상쇄
    float tau_grav_r = s_body_mass_kg * G_ACC * L_EFF_M * sinf(angle_r_rad);
    float tau_grav_l = s_body_mass_kg * G_ACC * L_EFF_M * sinf(angle_l_rad);

    // --- 4. 마찰 토크 상쇄: τ_friction = B_f · sign(θ̇) + B_v · θ̇ ---
    float tau_fric_r = 0.0f;
    float tau_fric_l = 0.0f;

    if (s_is_friction_comp_on) {
        // 우측 마찰 보상 (데드존 적용 — 정지 시 노이즈 방어)
        if (fabsf(vel_r_rads) > VELOCITY_DEADZONE_RADS) {
            // 쿨롱 마찰: 속도 부호 방향의 일정 토크
            // 점성 마찰: 속도에 비례하는 토크
            tau_fric_r = B_COULOMB_NM * _SignFloat(vel_r_rads)
                       + B_VISCOUS_NMS * vel_r_rads;
        }

        // 좌측 마찰 보상 (동일 로직)
        if (fabsf(vel_l_rads) > VELOCITY_DEADZONE_RADS) {
            tau_fric_l = B_COULOMB_NM * _SignFloat(vel_l_rads)
                       + B_VISCOUS_NMS * vel_l_rads;
        }
    }

    // --- 5. 총 보상 토크: τ = α · (τ_gravity + τ_friction) ---
    float tau_raw_r = s_alpha * (tau_grav_r + tau_fric_r);
    float tau_raw_l = s_alpha * (tau_grav_l + tau_fric_l);

    // --- 토크 포화 (Actuator Saturation) ---
    float tau_out_r = _ClampFloat(tau_raw_r, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    float tau_out_l = _ClampFloat(tau_raw_l, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    // --- 6. 좌/우 독립 토크 인가 ---
    XM_SetAssistTorqueRH(tau_out_r);
    XM_SetAssistTorqueLH(tau_out_l);

    // --- 디버깅용 변수 저장 (우측 기준 — 스트리밍/디버그에 사용) ---
    s_tau_gravity  = tau_grav_r;
    s_tau_friction = tau_fric_r;
    s_tau_total    = tau_out_r;

    // --- 7. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug(angle_r_deg, angle_l_deg, vel_r_rads, vel_l_rads);
    _UpdateStreamData();
}

// ==================== 유틸리티 함수 ====================

/**
 * @brief float 값을 [min, max] 범위로 클램핑합니다.
 * @param value     입력 값
 * @param min_val   하한
 * @param max_val   상한
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
 * @brief 버튼 입력 처리
 * @details
 * - BTN1 클릭: 보상 비율 α를 0.1 단위로 증가 (0.0→0.1→...→1.0→0.0 순환)
 *   → 착용자가 점진적으로 보상 강도를 높이며 적응
 *   → 투명 모드 튜닝의 핵심 파라미터
 *
 * - BTN2 클릭: 체중 프리셋 순환 (50→60→70→80→90→50 kg)
 *   → 착용자 체중에 따라 중력 보상 크기가 달라짐
 *   → M이 클수록 보상 토크 증가 (τ ∝ M)
 *
 * - BTN3 클릭: 마찰 보상 ON/OFF 토글
 *   → 마찰 파라미터 튜닝 시 중력 보상만 단독 테스트 가능
 *   → 마찰 보상 효과를 체감으로 비교할 수 있음
 */
static void _HandleButtonInput(void)
{
    // BTN1: 보상 비율 α 증가 (0.0 → 0.1 → ... → 1.0 → 0.0)
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_alpha_step_idx = (s_alpha_step_idx + 1) % NUM_ALPHA_STEPS;
        s_alpha = (float)s_alpha_step_idx * ALPHA_STEP;

        char buf[64];
        snprintf(buf, sizeof(buf), "[GC] BTN1: alpha=%.1f\r\n", (double)s_alpha);
        XM_SendUsbDebugMessage(buf);

        _UpdateLedIndicators();
    }

    // BTN2: 체중 프리셋 순환 (50→60→70→80→90→50)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_mass_preset_idx = (s_mass_preset_idx + 1) % NUM_MASS_PRESETS;
        s_body_mass_kg = s_mass_presets[s_mass_preset_idx];

        char buf[64];
        snprintf(buf, sizeof(buf), "[GC] BTN2: mass=%.0fkg\r\n", (double)s_body_mass_kg);
        XM_SendUsbDebugMessage(buf);
    }

    // BTN3: 마찰 보상 ON/OFF 토글
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_is_friction_comp_on = !s_is_friction_comp_on;

        if (s_is_friction_comp_on) {
            XM_SendUsbDebugMessage("[GC] BTN3: 마찰 보상 ON\r\n");
        } else {
            XM_SendUsbDebugMessage("[GC] BTN3: 마찰 보상 OFF\r\n");
        }

        _UpdateLedIndicators();
    }
}

/**
 * @brief LED 상태 갱신
 * @details
 * LED1: 심박수 패턴 (STANDBY) / 빠른 깜빡임 (ACTIVE)
 *   → α에 따라 깜빡임 속도 변화: α가 높을수록 빠르게
 * LED2: 보상 레벨 표시 (α 수준에 따라 깜빡임 주기 변화)
 *   → α=0: OFF, α>0: 깜빡임 (높을수록 빠름)
 * LED3: 마찰 보상 ON/OFF 표시
 */
static void _UpdateLedIndicators(void)
{
    // LED1: ACTIVE 상태 — α에 따라 깜빡임 속도 변화
    // α=0: 느린 깜빡임 (1000ms), α=1.0: 빠른 깜빡임 (200ms)
    uint16_t led1_period = (uint16_t)(1000.0f - 800.0f * s_alpha);
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, led1_period);

    // LED2: 보상 레벨 표시
    if (s_alpha_step_idx == 0) {
        // α=0.0: LED OFF (보상 비활성)
        XM_SetLedState(XM_LED_2, XM_OFF);
    } else {
        // α>0: 깜빡임 (α가 높을수록 빠르게: 1000ms→100ms)
        uint16_t led2_period = (uint16_t)(1100.0f - 1000.0f * s_alpha);
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, led2_period);
    }

    // LED3: 마찰 보상 활성 여부
    if (s_is_friction_comp_on) {
        XM_SetLedState(XM_LED_3, XM_ON);
    } else {
        XM_SetLedState(XM_LED_3, XM_OFF);
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 * @param angle_r_deg   우측 고관절 각도 (deg)
 * @param angle_l_deg   좌측 고관절 각도 (deg)
 * @param vel_r_rads    우측 고관절 각속도 (rad/s)
 * @param vel_l_rads    좌측 고관절 각속도 (rad/s)
 */
static void _UpdateUsbDebug(float angle_r_deg, float angle_l_deg,
                             float vel_r_rads, float vel_l_rads)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "GC|a=%.1f M=%.0f R:%.1fd %.1fNm L:%.1fd %.1fNm f=%d\r\n",
                 (double)s_alpha,
                 (double)s_body_mass_kg,
                 (double)angle_r_deg,
                 (double)s_tau_total,
                 (double)angle_l_deg,
                 (double)(_ClampFloat(
                     s_alpha * (s_body_mass_kg * G_ACC * L_EFF_M
                                * sinf(DEG_TO_RAD(angle_l_deg))
                                + (s_is_friction_comp_on
                                   ? B_COULOMB_NM * _SignFloat(vel_l_rads)
                                     + B_VISCOUS_NMS * vel_l_rads
                                   : 0.0f)),
                     -MAX_TORQUE_NM, MAX_TORQUE_NM)),
                 (int)s_is_friction_comp_on);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신 (우측 기준)
 */
static void _UpdateStreamData(void)
{
    s_stream_data.tau_gravity  = s_tau_gravity;
    s_stream_data.tau_friction = s_tau_friction;
    s_stream_data.tau_total    = s_tau_total;
    s_stream_data.alpha        = s_alpha;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}
