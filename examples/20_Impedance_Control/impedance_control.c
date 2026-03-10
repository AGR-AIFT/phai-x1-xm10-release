/**
 ******************************************************************************
 * @file    impedance_control.c
 * @author  HyundoKim
 * @brief   [고급] 임피던스 제어 기반 고관절 외골격 상호작용 제어
 * @details
 * H10 고관절 외골격 로봇에 임피던스 제어(Impedance Control)를 적용하여
 * 착용자와 로봇 사이의 상호작용 역학(interaction dynamics)을 정의합니다.
 *
 * [임피던스 제어 vs PD 제어 — 본질적 차이]
 * PD 제어는 "목표 위치를 추종하라"는 순수 위치 추적(position tracking) 기법입니다.
 * 반면 임피던스 제어는 "환경(착용자)과의 상호작용 관계를 정의하라"는 상호작용 포트
 * 행동(interaction port behavior) 기법입니다.
 *
 * PD 제어기는 외란(사용자 힘)을 "오차"로 인식하여 억제하려 합니다.
 * 임피던스 제어기는 외란을 "상호작용"으로 인식하여 가상 스프링-댐퍼의 물리적
 * 응답을 제공합니다. 착용자가 로봇을 밀면, 변위에 비례한 순응적(compliant)
 * 반력을 느끼게 됩니다 — 이것이 임피던스 제어의 핵심 개념입니다.
 *
 * [가상 스프링-댐퍼 모델의 물리적 의미]
 * 임피던스 제어는 로봇을 가상의 기계적 임피던스로 만듭니다:
 *   τ = K·(θ_d - θ) + B·(θ̇_d - θ̇)
 * - K (가상 강성, Virtual Stiffness): 평형 위치로부터의 변위에 비례하는 복원력.
 *   스프링 상수와 동일하며, K가 클수록 로봇이 "뻣뻣"하게 느껴집니다.
 * - B (가상 감쇠, Virtual Damping): 속도에 비례하는 에너지 소산력.
 *   댐퍼 계수와 동일하며, B가 클수록 움직임이 "느릿느릿"합니다.
 * - θ_d (평형 위치): 가상 스프링의 자연 길이에 해당하는 기준 각도.
 *
 * [Hogan (1985) 논문 핵심 기여]
 * Neville Hogan은 로봇 매니퓰레이터의 제어 문제를 "운동 제어"가 아닌
 * "역학적 상호작용의 제어"로 재정의하였습니다. 핵심 통찰:
 * (1) 접촉 환경에서 위치와 힘을 동시에 독립 제어할 수 없다
 * (2) 대신 위치-힘 사이의 "관계"(임피던스)를 제어해야 한다
 * (3) 이 임피던스를 가상 스프링-댐퍼-관성으로 매개변수화하면
 *     안정적이고 직관적인 상호작용 행동을 구현할 수 있다
 * 이 프레임워크는 이후 재활 로봇, 외골격, 협동 로봇의 근간이 되었습니다.
 *
 * [제어 법칙]
 *   τ = K·(θ_d - θ) + B·(θ̇_d - θ̇)
 *   여기서 θ̇_d = 0 (정적 평형), θ̇ ≈ (θ[k] - θ[k-1]) / dt (후향 차분)
 *
 * [버튼 조작]
 * - BTN1 클릭: 강성 프리셋 순환 (Soft → Medium → Stiff)
 * - BTN2 클릭: 감쇠 프리셋 순환 (Light → Medium → Heavy)
 * - BTN3 클릭: 평형 위치 ±5도 이동
 *
 * @see     Hogan, N. (1985) "Impedance Control: An Approach to Manipulation"
 *          ASME J. Dynamic Systems, Measurement, and Control, 107(1), 1-24.
 * @see     Keemink, A.Q.L. et al. (2018) "Admittance control for physical
 *          human-robot interaction" IJRR, 37(11), 1421-1444.
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

// --- 임피던스 제어 파라미터 프리셋 ---
// 강성 프리셋 (Nm/deg): Soft → Medium → Stiff
#define NUM_STIFFNESS_PRESETS    3
// 감쇠 프리셋 (Nm·s/deg): Light → Medium → Heavy
#define NUM_DAMPING_PRESETS      3

// --- 평형 위치 조절 ---
#define EQUILIBRIUM_STEP_DEG    5.0f    // BTN3 클릭 시 평형 위치 이동량 (deg)
#define EQUILIBRIUM_MAX_DEG     25.0f   // 평형 위치 최대 한계 (deg)
#define EQUILIBRIUM_MIN_DEG     (-25.0f) // 평형 위치 최소 한계 (deg)

// --- 안전 한계 ---
#define MAX_TORQUE_NM           5.0f    // 토크 포화 한계 (Nm) — 모터/인체 보호

// --- 제어 루프 타이밍 ---
#define CONTROL_DT              0.001f  // 제어 주기 (1ms = 1kHz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS     500     // USB CDC 디버그 메시지 출력 주기 (ms)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 강성 프리셋 레벨
 */
typedef enum {
    STIFFNESS_SOFT   = 0,   // 부드러움 (0.1 Nm/deg)
    STIFFNESS_MEDIUM = 1,   // 중간 (0.3 Nm/deg)
    STIFFNESS_STIFF  = 2    // 단단함 (0.8 Nm/deg)
} StiffnessLevel_t;

/**
 * @brief 감쇠 프리셋 레벨
 */
typedef enum {
    DAMPING_LIGHT  = 0,     // 가벼움 (0.005 Nm·s/deg)
    DAMPING_MEDIUM = 1,     // 중간 (0.02 Nm·s/deg)
    DAMPING_HEAVY  = 2      // 무거움 (0.05 Nm·s/deg)
} DampingLevel_t;

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details 임피던스 제어 핵심 변수 4개를 실시간 스트리밍합니다.
 *          PhAI Studio User Custom 모드에서 4채널 float로 표시됩니다.
 */
typedef struct {
    float equilibrium;      // 평형 위치 θ_d (deg)
    float current_angle;    // 현재 각도 θ (deg)
    float velocity;         // 현재 각속도 θ̇ (deg/s)
    float torque;           // 출력 토크 τ (Nm)
} ImpedanceStreamData_t;

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

// --- 임피던스 제어 파라미터 ---
// 강성 프리셋 테이블 (Nm/deg)
static const float s_stiffness_presets[NUM_STIFFNESS_PRESETS] = {
    0.1f,   // Soft  — 착용자가 쉽게 움직일 수 있음
    0.3f,   // Medium — 적절한 저항감
    0.8f    // Stiff — 강한 복원력 (재활 훈련용)
};

// 감쇠 프리셋 테이블 (Nm·s/deg)
static const float s_damping_presets[NUM_DAMPING_PRESETS] = {
    0.005f, // Light  — 최소 감쇠 (자유로운 움직임)
    0.02f,  // Medium — 적절한 점성 저항
    0.05f   // Heavy  — 강한 감쇠 (느린 동작 유도)
};

// --- 현재 선택된 프리셋 인덱스 ---
static uint8_t s_stiffness_idx = 0;         // 강성 프리셋 인덱스
static uint8_t s_damping_idx   = 0;         // 감쇠 프리셋 인덱스

// --- 임피던스 제어 상태 변수 ---
static float s_stiffness_k     = 0.1f;      // 현재 가상 강성 K (Nm/deg)
static float s_damping_b       = 0.005f;    // 현재 가상 감쇠 B (Nm·s/deg)
static float s_equilibrium_deg = 0.0f;      // 평형 위치 θ_d (deg)
static float s_eq_direction    = 1.0f;      // 평형 이동 방향 (+1 또는 -1)
static float s_prev_angle      = 0.0f;      // 이전 루프 각도 (각속도 수치 미분용)
static float s_torque_cmd      = 0.0f;      // 최종 토크 명령 (Nm)

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer = 0;

// --- USB 스트리밍 데이터 ---
static ImpedanceStreamData_t s_stream_data;

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

// --- 유틸리티 함수 ---
static float _ClampFloat(float value, float min_val, float max_val);
static void _HandleButtonInput(void);
static void _UpdateStiffnessLed(void);
static void _UpdateDampingLed(void);
static void _UpdateUsbDebug(float current_angle, float velocity);
static void _UpdateStreamData(float current_angle, float velocity);

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

    // [상태 3] ACTIVE: 임피던스 토크 제어 실행
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드)
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Equilibrium\",\"unit\":\"deg\"},"
        "{\"name\":\"Current Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Velocity\",\"unit\":\"deg/s\"},"
        "{\"name\":\"Torque\",\"unit\":\"Nm\"}]");

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
 * @details CM과 CAN-FD 통신이 수립될 때까지 대기합니다.
 */
static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SendUsbDebugMessage("[IMP] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태 — H10 ASSIST 모드 진입 대기
 * @details H10이 ASSIST 모드로 전환되면 임피던스 제어를 시작합니다.
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[IMP] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — 토크 제어 모드 설정 및 임피던스 파라미터 초기화
 * @details
 * XM_CTRL_TORQUE 모드를 활성화하여 임피던스 제어 법칙에 따라
 * 직접 토크 명령을 인가할 수 있도록 합니다.
 * 초기 강성/감쇠는 가장 부드러운 프리셋(Soft/Light)으로 시작합니다.
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // 임피던스 파라미터 초기화 (가장 부드러운 설정으로 시작)
    s_stiffness_idx = 0;
    s_damping_idx   = 0;
    s_stiffness_k   = s_stiffness_presets[0];   // Soft: 0.1 Nm/deg
    s_damping_b     = s_damping_presets[0];      // Light: 0.005 Nm·s/deg

    // 평형 위치: 현재 각도를 기준으로 설정
    s_equilibrium_deg = XM.status.h10.rightHipMotorAngle;
    s_eq_direction    = 1.0f;

    // 제어 상태 초기화
    s_prev_angle = XM.status.h10.rightHipMotorAngle;
    s_torque_cmd = 0.0f;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED1: ACTIVE 상태 표시 (빠른 깜빡임)
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    // LED2: 강성 레벨 표시 (초기: Soft → OFF)
    _UpdateStiffnessLed();
    // LED3: 감쇠 레벨 표시 (초기: Light → OFF)
    _UpdateDampingLed();

    XM_SendUsbDebugMessage("[IMP] ACTIVE 진입 — 임피던스 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — 임피던스 제어기 실행 (1ms 주기)
 * @details
 * 매 루프마다 다음을 수행합니다:
 * 1. 버튼 입력 처리 (강성/감쇠/평형 위치 변경)
 * 2. 임피던스 제어 연산
 * 3. 토크 포화 (클램핑)
 * 4. 토크 명령 전송
 * 5. USB 디버그/스트리밍 갱신
 *
 * [임피던스 제어 법칙 — 이산 구현]
 *   θ̇[k] ≈ (θ[k] - θ[k-1]) / dt       ← 후향 차분법으로 각속도 추정
 *   τ[k] = K·(θ_d - θ[k]) + B·(0 - θ̇[k])
 *        = K·(θ_d - θ[k]) - B·θ̇[k]
 *
 * 이 법칙의 물리적 의미:
 * - K·(θ_d - θ): 평형 위치로부터 벗어난 만큼 복원력 발생 (가상 스프링)
 * - -B·θ̇: 움직이는 속도에 비례하여 저항력 발생 (가상 댐퍼)
 * → 착용자는 마치 스프링-댐퍼 시스템과 상호작용하는 것처럼 느낌
 *
 * [PD 제어와의 수식 비교]
 * PD:  τ = Kp·(θ_ref - θ) + Kd·(ė)      ← "오차"를 줄이려는 목적
 * IMP: τ = K·(θ_d - θ) + B·(θ̇_d - θ̇)   ← "관계"를 정의하는 목적
 * 수식 형태는 유사하나, 설계 철학이 근본적으로 다릅니다.
 * PD는 추적 성능(tracking performance)을 최적화하고,
 * 임피던스 제어는 상호작용 품질(interaction quality)을 최적화합니다.
 */
static void Active_Loop(void)
{
    // H10이 STANDBY로 전환되면 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[IMP] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    // --- 1. 버튼 입력 처리 ---
    _HandleButtonInput();

    // --- 2. 임피던스 제어 연산 ---
    // 현재 우측 고관절 모터 각도 읽기 (deg)
    float current_angle = XM.status.h10.rightHipMotorAngle;

    // 각속도 추정: θ̇ ≈ (θ[k] - θ[k-1]) / dt (후향 차분법)
    // 1kHz 샘플링에서 충분한 정밀도를 제공하지만, 노이즈에 민감할 수 있음
    float velocity = (current_angle - s_prev_angle) / CONTROL_DT;

    // 임피던스 제어 법칙:
    //   τ = K·(θ_d - θ) + B·(θ̇_d - θ̇)
    // 여기서 θ̇_d = 0 (정적 평형 — 원하는 속도 없음)
    float displacement = s_equilibrium_deg - current_angle;  // (θ_d - θ)
    float velocity_error = 0.0f - velocity;                  // (θ̇_d - θ̇)

    float torque_raw = (s_stiffness_k * displacement)        // 스프링 항: 복원력
                     + (s_damping_b * velocity_error);        // 댐퍼 항: 감쇠력

    // --- 3. 토크 포화 (Actuator Saturation) ---
    // 모터 보호를 위해 출력 토크를 안전 범위로 제한
    s_torque_cmd = _ClampFloat(torque_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    // --- 4. 토크 명령 전송 ---
    // 좌/우 동일 토크 인가 (대칭 제어)
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);

    // --- 5. 이전 각도 저장 (다음 루프의 속도 추정용) ---
    s_prev_angle = current_angle;

    // --- 6. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug(current_angle, velocity);
    _UpdateStreamData(current_angle, velocity);
}

/**
 * @brief ACTIVE 탈출 — 안전 정지 절차
 * @details
 * 토크를 0으로 리셋하고 모니터링 모드로 복귀합니다.
 * 이는 제어 종료 시 잔류 토크로 인한 사고를 방지하는 필수 안전 절차입니다.
 */
static void Active_Exit(void)
{
    // 토크 0으로 안전 해제
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    // 모니터링 모드로 복귀 (토크 명령 비활성화)
    XM_SetControlMode(XM_CTRL_MONITOR);

    // 제어 상태 초기화
    s_prev_angle = 0.0f;
    s_torque_cmd = 0.0f;

    // LED: STANDBY 표시 (심장박동)
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[IMP] 제어 종료 — 토크 해제\r\n");
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
 * - BTN1 클릭: 강성 프리셋 순환 (Soft → Medium → Stiff → Soft)
 *   → 가상 스프링 상수 변경. 높을수록 평형 위치에 강하게 묶임
 * - BTN2 클릭: 감쇠 프리셋 순환 (Light → Medium → Heavy → Light)
 *   → 가상 댐퍼 계수 변경. 높을수록 움직임에 더 큰 저항
 * - BTN3 클릭: 평형 위치 ±5도 이동 (한계 도달 시 방향 반전)
 *   → 가상 스프링의 자연 길이(rest position) 변경
 */
static void _HandleButtonInput(void)
{
    // BTN1: 강성 프리셋 순환 (Soft → Medium → Stiff)
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_stiffness_idx = (s_stiffness_idx + 1) % NUM_STIFFNESS_PRESETS;
        s_stiffness_k = s_stiffness_presets[s_stiffness_idx];

        // LED2로 강성 레벨 시각적 표시
        _UpdateStiffnessLed();

        char buf[64];
        snprintf(buf, sizeof(buf),
                 "[IMP] BTN1: K=%.2f Nm/deg (%s)\r\n",
                 s_stiffness_k,
                 (s_stiffness_idx == 0) ? "Soft" :
                 (s_stiffness_idx == 1) ? "Medium" : "Stiff");
        XM_SendUsbDebugMessage(buf);
    }

    // BTN2: 감쇠 프리셋 순환 (Light → Medium → Heavy)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_damping_idx = (s_damping_idx + 1) % NUM_DAMPING_PRESETS;
        s_damping_b = s_damping_presets[s_damping_idx];

        // LED3로 감쇠 레벨 시각적 표시
        _UpdateDampingLed();

        char buf[64];
        snprintf(buf, sizeof(buf),
                 "[IMP] BTN2: B=%.3f Nm·s/deg (%s)\r\n",
                 s_damping_b,
                 (s_damping_idx == 0) ? "Light" :
                 (s_damping_idx == 1) ? "Medium" : "Heavy");
        XM_SendUsbDebugMessage(buf);
    }

    // BTN3: 평형 위치 ±5도 이동 (한계 도달 시 방향 반전)
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_equilibrium_deg += s_eq_direction * EQUILIBRIUM_STEP_DEG;

        // 한계 도달 시 방향 반전 (핑퐁 방식)
        if (s_equilibrium_deg >= EQUILIBRIUM_MAX_DEG) {
            s_equilibrium_deg = EQUILIBRIUM_MAX_DEG;
            s_eq_direction = -1.0f;
        } else if (s_equilibrium_deg <= EQUILIBRIUM_MIN_DEG) {
            s_equilibrium_deg = EQUILIBRIUM_MIN_DEG;
            s_eq_direction = 1.0f;
        }

        char buf[64];
        snprintf(buf, sizeof(buf),
                 "[IMP] BTN3: θ_d=%.1f deg\r\n",
                 s_equilibrium_deg);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief LED2 강성 레벨 표시 갱신
 * @details Soft=OFF, Medium=Blink, Stiff=ON
 */
static void _UpdateStiffnessLed(void)
{
    switch (s_stiffness_idx) {
        case STIFFNESS_SOFT:
            XM_SetLedState(XM_LED_2, XM_OFF);
            break;
        case STIFFNESS_MEDIUM:
            XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 500);
            break;
        case STIFFNESS_STIFF:
            XM_SetLedState(XM_LED_2, XM_ON);
            break;
        default:
            break;
    }
}

/**
 * @brief LED3 감쇠 레벨 표시 갱신
 * @details Light=OFF, Medium=Blink, Heavy=ON
 */
static void _UpdateDampingLed(void)
{
    switch (s_damping_idx) {
        case DAMPING_LIGHT:
            XM_SetLedState(XM_LED_3, XM_OFF);
            break;
        case DAMPING_MEDIUM:
            XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 500);
            break;
        case DAMPING_HEAVY:
            XM_SetLedState(XM_LED_3, XM_ON);
            break;
        default:
            break;
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 * @param current_angle 현재 모터 각도 (deg)
 * @param velocity      현재 각속도 (deg/s)
 */
static void _UpdateUsbDebug(float current_angle, float velocity)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        /*
         * snprintf + %.xf 사용 — ARM GCC의 nano printf에서 float을
         * 지원하려면 링커 옵션 -u _printf_float이 필요합니다.
         * 본 프로젝트는 이미 활성화되어 있으므로 snprintf로 직접 출력합니다.
         */
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "IMP | Eq:%.1f Cur:%.1f Vel:%.1f K:%.2f B:%.3f Tau:%.2f\r\n",
                 s_equilibrium_deg, current_angle, velocity,
                 s_stiffness_k, s_damping_b, s_torque_cmd);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신
 * @param current_angle 현재 모터 각도 (deg)
 * @param velocity      현재 각속도 (deg/s)
 */
static void _UpdateStreamData(float current_angle, float velocity)
{
    s_stream_data.equilibrium   = s_equilibrium_deg;
    s_stream_data.current_angle = current_angle;
    s_stream_data.velocity      = velocity;
    s_stream_data.torque        = s_torque_cmd;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}
