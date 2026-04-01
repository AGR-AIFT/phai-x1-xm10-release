/**
 ******************************************************************************
 * @file    inverted_pendulum_control.c
 * @author  HyundoKim
 * @brief   [고급] 역진자 모델 기반 고관절 보조 제어
 * @details
 * H10 고관절 보조 로봇을 역진자(inverted pendulum) 모델로 근사하여
 * 중력 보상 + PD 안정화 제어를 수행합니다.
 *
 * [역진자 모델 — 이론적 배경]
 * 인체 보행 시 골반(trunk)은 하지를 축으로 한 역진자처럼 동작합니다.
 * 단일 지지기(single support phase)에서 인체 질량 중심(CoM)은
 * 지면 접촉점을 피벗으로 하여 불안정한 역진자 운동을 합니다.
 *   참고: "The inverted pendulum model of bipedal walking"
 *         — Kuo, A.D., Lecture Notes, 2007
 *
 * [동역학 방정식]
 * 역진자의 운동 방정식 (비선형):
 *   I * theta_ddot = M * g * L * sin(theta)
 * 여기서:
 *   M = 체중 (kg), g = 중력가속도 (m/s²), L = 다리 길이 (m)
 *   theta = 골반 기울기 (rad), I = 관성 모멘트 ≈ M * L²
 *
 * [제어 법칙]
 * 1. 중력 보상 토크 (Gravity Compensation):
 *    tau_gravity = M * g * L * sin(theta)
 *    → 역진자의 중력 모멘트를 상쇄하여 에너지 균형 유지
 *
 * 2. PD 안정화 토크 (Stabilization):
 *    tau_pd = Kp * (theta_ref - theta) + Kd * (0 - theta_dot)
 *    → 기준 각도 부근에서 위치+속도 피드백으로 안정화
 *
 * 3. 총 토크 = tau_gravity + tau_pd
 *    → 좌/우 고관절에 50:50 분배
 *
 * [에너지 관점 안정성]
 * 중력 보상은 역진자의 위치 에너지(potential energy)를 제거하고,
 * PD 항의 Kd 성분이 운동 에너지(kinetic energy)를 소산(dissipate)합니다.
 * 이를 통해 리아프노프(Lyapunov) 안정성을 확보합니다.
 *
 * @see     docs/api-reference/XM_Control.md
 * @version 1.0
 * @date    Mar 09, 2026
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

// --- 역진자 물리 모델 파라미터 ---
#define M_BODY_KG           70.0f   // 체중 (kg) — 피험자에 맞게 조절
#define L_LEG_M             0.9f    // 유효 다리 길이 (m) — 고관절~발목
#define G_ACC               9.81f   // 중력 가속도 (m/s²)

// --- PD 안정화 게인 ---
#define IP_KP_GAIN          15.0f   // 비례 게인 (Nm/rad) — 자세 복원 강성
#define IP_KD_GAIN          3.0f    // 미분 게인 (Nm·s/rad) — 각속도 감쇠

// --- 안전 한계 ---
#define MAX_TORQUE_NM       8.0f    // 토크 포화 한계 (Nm) — 모터/인체 보호
#define ANGLE_LIMIT_DEG     30.0f   // 비상 정지 각도 한계 (deg) — 초과 시 ERROR

// --- 보조 레벨 스케일 ---
#define NUM_GAIN_LEVELS     4       // 게인 레벨 수 (0.0, 0.3, 0.6, 1.0)

// --- 제어 루프 타이밍 ---
#define CONTROL_DT          0.001f  // 제어 주기 (1ms = 1kHz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS 500     // USB CDC 디버그 메시지 출력 주기 (ms)

// --- 각도 변환 매크로 ---
#define DEG_TO_RAD(d)       ((d) * 0.017453292f)   // pi / 180
#define RAD_TO_DEG(r)       ((r) * 57.29577951f)   // 180 / pi

// --- Homing 설정 값 ---
#define HOMING_SPEED            150     // 초당 이동 속도 (deg/s, x10 스케일)
#define HOMING_ACCEL_S0         4       // 초기 가속도 (deg/s²)
#define HOMING_ACCEL_SD         4       // 말기 가속도 (deg/s²)
#define HOMING_TRANSITION_MS    50      // Homing 완료 후 안정화 지연 (ms)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Homing(원점 복귀) 절차의 내부 동작 상태
 */
typedef enum {
    HOMING_ENTRY,               // 1. 진입 — 임피던스 최대값 설정
    HOMING_SET_IMPEDANCE,       // 2. 위치 제어용 임피던스 설정
    HOMING_START_MOTION,        // 3. 0도 위치로 이동 시작
    HOMING_WAIT_FOR_DONE,       // 4. 이동 완료 대기
    HOMING_FINALIZE_DELAY,      // 5. 안정화 지연
    HOMING_FINALIZE_CLEANUP     // 6. 설정 정리 및 ACTIVE 전환
} HomingState_t;

/**
 * @brief ACTIVE 상태의 내부 단계
 * @details Homing 완료 후 실시간 제어로 전환됩니다.
 */
typedef enum {
    ACTIVE_PHASE_HOMING,        // Homing 절차 진행 중
    ACTIVE_PHASE_CONTROL        // 역진자 실시간 제어 중
} ActivePhase_t;

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details 역진자 제어 핵심 변수 5개를 실시간 스트리밍합니다.
 */
typedef struct {
    float theta;            // 골반 기울기 (rad)
    float theta_dot;        // 골반 각속도 (rad/s)
    float tau_gravity;      // 중력 보상 토크 (Nm)
    float tau_pd;           // PD 안정화 토크 (Nm)
    float tau_total;        // 총 토크 (Nm)
} IpStreamData_t;

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

// --- ACTIVE 내부 단계 ---
static ActivePhase_t s_active_phase = ACTIVE_PHASE_HOMING;

// --- Homing 상태 ---
static HomingState_t s_homing_state = HOMING_ENTRY;
static uint32_t s_homing_timer = 0;

// --- 역진자 제어 변수 ---
static float s_theta_ref    = 0.0f;    // 기준 골반 각도 (rad) — BTN2로 리셋 가능
static float s_tau_gravity  = 0.0f;    // 중력 보상 토크 (Nm)
static float s_tau_pd       = 0.0f;    // PD 안정화 토크 (Nm)
static float s_tau_total    = 0.0f;    // 총 토크 (Nm)

// --- 보조 레벨 (게인 스케일링) ---
static const float s_gain_levels[NUM_GAIN_LEVELS] = { 0.0f, 0.3f, 0.6f, 1.0f };
static uint8_t s_gain_level_idx = 0;   // 현재 게인 레벨 인덱스
static float s_gain_scale   = 0.0f;    // 현재 게인 스케일 (0.0 ~ 1.0)

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer = 0;

// --- USB 스트리밍 데이터 ---
static IpStreamData_t s_stream_data;

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
static void Error_Entry(void);
static void Error_Loop(void);

// --- Homing 절차 ---
static void _RunHomingSequence(void);

// --- 역진자 제어 ---
static void _RunInvertedPendulumControl(void);

// --- 유틸리티 함수 ---
static float _ClampFloat(float value, float min_val, float max_val);
static void _HandleButtonInput(void);
static void _UpdateUsbDebug(float theta, float theta_dot);
static void _UpdateStreamData(float theta, float theta_dot);
static void _SafetyShutdown(void);

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

    // [상태 3] ACTIVE: Homing → 역진자 제어
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // [상태 4] ERROR: 비상 정지
    XmStateConfig_t err_conf = {
        .id = XM_STATE_ERROR,
        .on_entry = Error_Entry,
        .on_loop  = Error_Loop
    };
    XM_TSM_AddState(s_tsm, &err_conf);

    // USB 스트리밍 설정 (User Custom 모드)
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Angle\",\"unit\":\"rad\"},"
        "{\"name\":\"Ang Velocity\",\"unit\":\"rad/s\"},"
        "{\"name\":\"Gravity Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"PD Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Total Torque\",\"unit\":\"Nm\"}]");

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
        XM_SendUsbDebugMessage("[IP] CM 연결됨 -> STANDBY\r\n");
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
        XM_SendUsbDebugMessage("[IP] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — Homing 절차 시작 및 변수 초기화
 * @details
 * Homing이 완료된 후 역진자 실시간 제어가 시작됩니다.
 * Homing은 IVector(임피던스) + PVector(궤적)를 사용하여
 * 모터를 0도 위치로 안전하게 이동시킵니다.
 */
static void Active_Entry(void)
{
    // Homing 단계로 시작
    s_active_phase = ACTIVE_PHASE_HOMING;
    s_homing_state = HOMING_ENTRY;

    // 역진자 제어 변수 초기화
    s_theta_ref   = 0.0f;
    s_tau_gravity = 0.0f;
    s_tau_pd      = 0.0f;
    s_tau_total   = 0.0f;

    // 보조 레벨 초기화 (0% — 처음에는 토크 미인가)
    s_gain_level_idx = 0;
    s_gain_scale = s_gain_levels[s_gain_level_idx];

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED: ACTIVE 상태 표시
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);

    XM_SendUsbDebugMessage("[IP] ACTIVE 진입 — Homing 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — Homing 또는 역진자 제어 실행
 * @details
 * ACTIVE 상태는 두 단계로 구성됩니다:
 * 1. HOMING: 모터를 0도 위치로 안전하게 이동
 * 2. CONTROL: 역진자 모델 기반 실시간 토크 제어
 */
static void Active_Loop(void)
{
    // H10이 STANDBY로 전환되면 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[IP] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    switch (s_active_phase) {
        case ACTIVE_PHASE_HOMING:
            _RunHomingSequence();
            break;

        case ACTIVE_PHASE_CONTROL:
            _HandleButtonInput();
            _RunInvertedPendulumControl();
            break;

        default:
            s_active_phase = ACTIVE_PHASE_HOMING;
            break;
    }
}

/**
 * @brief ACTIVE 탈출 — 안전 정지 절차
 * @details
 * 토크를 0으로 리셋하고 임피던스를 해제한 후
 * 모니터링 모드로 복귀합니다.
 */
static void Active_Exit(void)
{
    _SafetyShutdown();

    // LED: STANDBY 표시
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[IP] 제어 종료 — 토크 해제\r\n");
}

/**
 * @brief ERROR 진입 — 비상 정지 (골반 각도 한계 초과)
 * @details
 * 골반 기울기가 ±ANGLE_LIMIT_DEG를 초과하면 즉시 토크를 해제하고
 * ERROR 상태로 전환합니다. 착용자의 안전이 최우선입니다.
 */
static void Error_Entry(void)
{
    _SafetyShutdown();

    // LED: ERROR 표시 (빠른 깜빡임)
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 100);
    XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 100);
    XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 100);

    XM_SendUsbDebugMessage("[IP] ERROR — 각도 한계 초과! 비상 정지\r\n");
}

/**
 * @brief ERROR 루프 — H10이 STANDBY로 전환되면 복귀
 */
static void Error_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_STANDBY) {
        XM_SendUsbDebugMessage("[IP] ERROR 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

// ==================== Homing 절차 ====================

/**
 * @brief Homing 상태 머신 — 모터를 0도 위치로 안전하게 이동
 * @details
 * Passive Mode 예제와 동일한 패턴:
 * IVector로 임피던스(강성)를 설정 → PVector로 0도 궤적 전송 → 완료 대기
 * Homing 완료 후 토크 직접 제어 모드로 전환합니다.
 */
static void _RunHomingSequence(void)
{
    switch (s_homing_state) {
        case HOMING_ENTRY:
            // 임피던스 최대값 설정 (안전한 위치 제어를 위해)
            XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 1);
            XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6, 1);
            s_homing_state = HOMING_SET_IMPEDANCE;
            break;

        case HOMING_SET_IMPEDANCE: {
            // 위치 제어용 임피던스 설정 (Kp 80%, Kd 1%)
            IVector_t stiff = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
            XM_SendIVector(SYS_NODE_ID_RH, &stiff);
            XM_SendIVector(SYS_NODE_ID_LH, &stiff);
            s_homing_state = HOMING_START_MOTION;
            break;
        }

        case HOMING_START_MOTION: {
            // 현재 각도에서 0도로 이동하는 P-Vector 전송
            int16_t target_angle = 0;
            int16_t current_rh = (int16_t)round(XM.status.h10.rightHipMotorAngle * 10.0f);
            int16_t current_lh = (int16_t)round(XM.status.h10.leftHipMotorAngle * 10.0f);

            int16_t dist_rh = abs(target_angle - current_rh);
            int16_t dist_lh = abs(target_angle - current_lh);
            uint16_t dur_rh = (uint16_t)(((float)dist_rh / (float)HOMING_SPEED) * 1000.0f);
            uint16_t dur_lh = (uint16_t)(((float)dist_lh / (float)HOMING_SPEED) * 1000.0f);

            PVector_t pvec_rh = { .yd = target_angle, .L = dur_rh, .s0 = HOMING_ACCEL_S0, .sd = HOMING_ACCEL_SD };
            PVector_t pvec_lh = { .yd = target_angle, .L = dur_lh, .s0 = HOMING_ACCEL_S0, .sd = HOMING_ACCEL_SD };
            XM_SendPVector(SYS_NODE_ID_RH, &pvec_rh);
            XM_SendPVector(SYS_NODE_ID_LH, &pvec_lh);

            s_homing_state = HOMING_WAIT_FOR_DONE;
            break;
        }

        case HOMING_WAIT_FOR_DONE:
            // 양쪽 모터 모두 이동 완료될 때까지 대기
            if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                s_homing_timer = XM_GetTick();
                s_homing_state = HOMING_FINALIZE_DELAY;
            }
            break;

        case HOMING_FINALIZE_DELAY:
            // 안정화 지연
            if (XM_GetTick() - s_homing_timer >= HOMING_TRANSITION_MS) {
                s_homing_state = HOMING_FINALIZE_CLEANUP;
            }
            break;

        case HOMING_FINALIZE_CLEANUP:
            // Homing 완료 — 토크 직접 제어 모드로 전환
            XM_SetControlMode(XM_CTRL_TORQUE);

            // 현재 골반 각도를 기준점으로 설정
            s_theta_ref = DEG_TO_RAD(XM.status.h10.pelvicAngle);

            // 제어 단계로 전환
            s_active_phase = ACTIVE_PHASE_CONTROL;

            // LED: 제어 중 표시 (빠른 깜빡임)
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);

            // Homing 상태 초기화 (다음 사용을 위해)
            s_homing_state = HOMING_ENTRY;

            XM_SendUsbDebugMessage("[IP] Homing 완료 — 역진자 제어 시작\r\n");
            break;
    }
}

// ==================== 역진자 제어 ====================

/**
 * @brief 역진자 모델 기반 실시간 토크 제어
 * @details
 * 매 1ms 루프마다 다음을 수행합니다:
 *
 * [1단계] 센서 데이터 획득
 *   theta     = pelvicAngle (deg → rad 변환)
 *   theta_dot = (L_gyroY + R_gyroY) / 2  (골반 각속도 추정, deg/s → rad/s 변환)
 *
 * [2단계] 중력 보상 토크 계산
 *   tau_gravity = M * g * L * sin(theta)
 *   → 역진자가 기울어진 방향으로 작용하는 중력 모멘트를 상쇄
 *   → sin(theta) ≈ theta (소각 근사, |theta| < 15도에서 오차 < 1%)
 *   → 본 구현에서는 sin() 함수를 사용하여 대각도에서도 정확한 보상 수행
 *
 * [3단계] PD 안정화 토크 계산
 *   tau_pd = Kp * (theta_ref - theta) + Kd * (0 - theta_dot)
 *   → theta_ref: 기준 각도 (BTN2로 리셋 가능)
 *   → 목표 각속도는 0 (정지 자세 안정화)
 *
 * [4단계] 게인 스케일링 + 포화
 *   tau_total = gain_scale * (tau_gravity + tau_pd)
 *   tau_out = clamp(tau_total, -MAX, +MAX)
 *
 * [5단계] 좌/우 분배
 *   tau_rh = tau_out * 0.5
 *   tau_lh = tau_out * 0.5
 */
static void _RunInvertedPendulumControl(void)
{
    // --- 1. 센서 데이터 획득 (deg → rad 변환) ---
    float theta_deg = XM.status.h10.pelvicAngle;
    float theta     = DEG_TO_RAD(theta_deg);
    /* 골반 각속도 추정: 좌/우 고관절 IMU gyroY 평균 */
    float pelvic_vel_y = (XM.status.h10.leftHipImuGlobalGyrY
                        + XM.status.h10.rightHipImuGlobalGyrY) / 2.0f;
    float theta_dot = DEG_TO_RAD(pelvic_vel_y);

    // --- 안전 검사: 골반 각도 한계 초과 시 비상 정지 ---
    if (fabsf(theta_deg) > ANGLE_LIMIT_DEG) {
        XM_SendUsbDebugMessage("[IP] 각도 한계 초과! -> ERROR\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ERROR);
        return;
    }

    // --- 2. 중력 보상 토크 ---
    // tau_gravity = M * g * L * sin(theta)
    // 역진자의 중력 모멘트를 정확히 상쇄하여
    // 제어 관점에서 역진자를 "무중력" 상태로 만듦
    s_tau_gravity = M_BODY_KG * G_ACC * L_LEG_M * sinf(theta);

    // --- 3. PD 안정화 토크 ---
    // 기준 각도(theta_ref) 부근에서 위치+속도 피드백
    // Kp: 자세 복원력 (rad 단위이므로 Nm/rad)
    // Kd: 각속도 감쇠력 (진동 억제)
    float theta_error = s_theta_ref - theta;
    s_tau_pd = (IP_KP_GAIN * theta_error) + (IP_KD_GAIN * (0.0f - theta_dot));

    // --- 4. 총 토크 계산 (게인 스케일링 적용) ---
    // gain_scale: 0.0(비활성) ~ 1.0(최대) 범위의 보조 레벨
    // 착용자가 BTN1로 단계별 조절 가능 → 점진적 적응 지원
    float tau_raw = s_gain_scale * (s_tau_gravity + s_tau_pd);

    // --- 토크 포화 (Actuator Saturation) ---
    s_tau_total = _ClampFloat(tau_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    // --- 5. 좌/우 대칭 분배 (50:50) ---
    float tau_rh = s_tau_total * 0.5f;
    float tau_lh = s_tau_total * 0.5f;

    XM_SetAssistTorqueRH(tau_rh);
    XM_SetAssistTorqueLH(tau_lh);

    // --- 6. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug(theta, theta_dot);
    _UpdateStreamData(theta, theta_dot);
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
 * @brief 버튼 입력 처리
 * @details
 * - BTN1 클릭: 보조 레벨 단계별 조절 (0.0 → 0.3 → 0.6 → 1.0 → 0.0 순환)
 *   → 착용자가 점진적으로 보조 강도를 높이며 적응할 수 있음
 *   → LED2/LED3로 현재 레벨을 시각적으로 표시
 * - BTN2 클릭: 골반 기준 각도(theta_ref) 리셋
 *   → 현재 골반 각도를 새로운 0점으로 설정
 *   → 자세 보정이나 초기 정렬에 유용
 */
static void _HandleButtonInput(void)
{
    // BTN1: 보조 레벨 순환
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_gain_level_idx = (s_gain_level_idx + 1) % NUM_GAIN_LEVELS;
        s_gain_scale = s_gain_levels[s_gain_level_idx];

        // LED로 보조 레벨 표시
        // Level 0 (0.0): LED2=OFF, LED3=OFF
        // Level 1 (0.3): LED2=ON,  LED3=OFF
        // Level 2 (0.6): LED2=ON,  LED3=ON
        // Level 3 (1.0): LED2=BLINK, LED3=BLINK
        switch (s_gain_level_idx) {
            case 0:
                XM_SetLedState(XM_LED_2, XM_OFF);
                XM_SetLedState(XM_LED_3, XM_OFF);
                break;
            case 1:
                XM_SetLedState(XM_LED_2, XM_ON);
                XM_SetLedState(XM_LED_3, XM_OFF);
                break;
            case 2:
                XM_SetLedState(XM_LED_2, XM_ON);
                XM_SetLedState(XM_LED_3, XM_ON);
                break;
            case 3:
                XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 300);
                XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 300);
                break;
            default:
                break;
        }

        XM_SendUsbDebugMessage("[IP] BTN1: 보조 레벨 변경\r\n");
    }

    // BTN2: 기준 각도 리셋 (현재 골반 각도를 0점으로)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_theta_ref = DEG_TO_RAD(XM.status.h10.pelvicAngle);
        XM_SendUsbDebugMessage("[IP] BTN2: 기준 각도 리셋\r\n");
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 * @param theta     골반 기울기 (rad)
 * @param theta_dot 골반 각속도 (rad/s)
 */
static void _UpdateUsbDebug(float theta, float theta_dot)
{
    (void)theta_dot;  // 현재 디버그 출력에서는 미사용

    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        // 고정소수점 변환 (snprintf float 대체 — 임베디드 안전)
        int th_int  = (int)(theta * 100.0f);
        int tg_int  = (int)(s_tau_gravity * 100.0f);
        int tp_int  = (int)(s_tau_pd * 100.0f);
        int tt_int  = (int)(s_tau_total * 100.0f);

        char buf[80];
        snprintf(buf, sizeof(buf),
                 "IP | th:%d.%02d tg:%d.%02d tp:%d.%02d tau:%d.%02d\r\n",
                 th_int / 100, abs(th_int) % 100,
                 tg_int / 100, abs(tg_int) % 100,
                 tp_int / 100, abs(tp_int) % 100,
                 tt_int / 100, abs(tt_int) % 100);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신
 * @param theta     골반 기울기 (rad)
 * @param theta_dot 골반 각속도 (rad/s)
 */
static void _UpdateStreamData(float theta, float theta_dot)
{
    s_stream_data.theta       = theta;
    s_stream_data.theta_dot   = theta_dot;
    s_stream_data.tau_gravity = s_tau_gravity;
    s_stream_data.tau_pd      = s_tau_pd;
    s_stream_data.tau_total   = s_tau_total;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

/**
 * @brief 안전 정지 절차 — 토크 해제 및 임피던스 초기화
 * @details
 * ERROR 또는 ACTIVE 탈출 시 공통으로 호출됩니다.
 * 1. 토크 즉시 0으로 해제
 * 2. 임피던스(강성) 해제 — 모터 자유 회전
 * 3. 모니터링 모드 복귀
 */
static void _SafetyShutdown(void)
{
    // 토크 즉시 해제
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    // 임피던스 해제 (Kp=0, Kd=0 → 모터 자유 회전)
    IVector_t release = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
    XM_SendIVector(SYS_NODE_ID_RH, &release);
    XM_SendIVector(SYS_NODE_ID_LH, &release);

    // 모니터링 모드로 복귀
    XM_SetControlMode(XM_CTRL_MONITOR);

    // 제어 변수 초기화
    s_tau_gravity = 0.0f;
    s_tau_pd      = 0.0f;
    s_tau_total   = 0.0f;
    s_gain_level_idx = 0;
    s_gain_scale  = 0.0f;
}
