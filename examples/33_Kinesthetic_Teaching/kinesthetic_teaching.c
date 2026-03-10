/**
 ******************************************************************************
 * @file    kinesthetic_teaching.c
 * @author  HyundoKim
 * @brief   [고급] 운동감각 교시 + 재생 — 전문가 스킬 캡처 (Stage 5)
 *          (Kinesthetic Teaching & Replay — Expert Skill Data Collection)
 * @details
 * Stage 5: 전문가가 로봇을 직접 손으로 이끌어(guided motion) 동작을 교시하고,
 * 로봇이 그 궤적을 그대로 재생하는 "장인 스킬 캡처(Expert Skill Capture)"를 구현합니다.
 * 이것이 Physical AI 데이터 파이프라인의 핵심 원시(primitive)입니다.
 *
 * [운동감각 교시 개요]
 * 운동감각 교시(Kinesthetic Teaching, KT)는 로봇 공학의 고전적인 기술로,
 * 사람이 로봇 팔/다리를 직접 움직여 동작을 "가르치는" 방식입니다.
 * 외골격에서의 핵심 특징:
 *   → 교시 중: 로봇이 투명(transparent)해야 함 (Ex.21 중력+마찰 보상 적용)
 *   → 인간이 컨트롤러 (Human IS the Controller)
 *   → 100Hz(10ms)로 궤적 기록 → 최대 20초 (2000포인트)
 *   → 재생: PD 위치 추적으로 동일 궤적 재연
 *
 * [Physical AI 연결 고리 — 장인 스킬 → AI 학습]
 * 이 예제가 수집하는 데이터:
 *   (θ_rh[t], θ_lh[t]) — 전문가의 운동 궤적
 *
 * 다음 단계 (PhAI Studio 연동):
 *   1. XM_SetUsbLogSource로 SD카드에 저장 (Ex.10b 참조)
 *   2. PhAI Studio로 데이터 전송 및 라벨링
 *   3. π0 스타일 VLA(Vision-Language-Action) 모델 학습
 *   → 이 데이터가 π0 스타일 VLA 모델의 학습 데이터가 됩니다
 *
 * [상태 전이]
 * IDLE → RECORD: BTN1 (교시 시작 — 인간이 로봇을 이끔)
 * RECORD → RECORDED: BTN1 재클릭 또는 버퍼 만료
 * RECORDED → REPLAY: BTN2 (재생 시작)
 * REPLAY → RECORDED: 재생 완료 (자동 루프 복귀)
 * 어떤 상태 → IDLE: BTN3 (리셋 — 버퍼 클리어)
 *
 * [PD 재생 제어]
 * 단순한 PD 위치 추적기로 저장된 궤적을 재연합니다.
 * τ = Kp·(θ_target - θ_actual) + Kd·Δerror/dt
 * → 고급 구현에서는 feedforward(중력 보상) 추가로 정밀도 향상 가능
 *
 * [링 버퍼 vs 선형 버퍼]
 * 이 예제는 선형 버퍼(linear buffer)를 사용합니다.
 * → 단순, 인덱스 관리 직관적, 오버런 명확
 * → 연속 교시가 필요한 경우: 링 버퍼로 교체 (Ex.10b 패턴 참조)
 *
 * @see     Billard, A. et al. (2008) "Robot Programming by Demonstration"
 *          in Springer Handbook of Robotics, pp.1371-1394
 * @see     Chi, C. et al. (2023) "Diffusion Policy: Visuomotor Policy Learning
 *          via Action Diffusion" RSS 2023 (π0의 전신 개념)
 * @see     Ex.21 gravity_compensation.c (교시 중 투명 모드 기반)
 * @see     Ex.10b msc_custom_struct.c (SD카드 로깅 연동)
 * @see     docs/api-reference/XM_Control.md
 * @version 1.0
 * @date    Mar 10, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// --- 교시 버퍼 설정 ---
// 20초 × 100Hz = 2000포인트
// 메모리: 2000 × 8bytes(2×float) = 16KB → STM32H743 SRAM 여유 충분
#define MAX_TEACH_POINTS        2000        // 최대 교시 포인트 수 (20초 @ 100Hz)
#define RECORD_DOWNSAMPLE       10          // 다운샘플: 1kHz 루프에서 10ms마다 1회 기록

// --- PD 재생 제어 파라미터 ---
// Kp: 1.5 Nm/deg → 1도 오차 시 1.5Nm 토크 (적당한 추적, 충격 적음)
// Kd: 0.05        → 각속도 감쇠 (진동 억제)
// 고급 튜닝 방향: 중력 보상 feedforward 추가 → 정상 상태 오차 감소
//
// [구동기 속도 스펙]
// 정격 속도: 50 rpm → 관절 50/18.75 ≈ 2.67 rpm ≈ 16.0 deg/s  (연속 운전 권장 범위)
// 무부하 최대: 85 rpm → 관절 85/18.75 ≈ 4.53 rpm ≈ 27.2 deg/s
// 재생 궤적의 관절 각속도가 16 deg/s를 초과하지 않도록 교시 속도를 조절하세요.
//
// [구동기 안전 한계]
// MD 전류: 14A 초과 시 보호 동작 (경고 후 전원 OFF) — KT_JOINT=1.594 Nm/A 기준 약 22 Nm
// MD 온도: 110°C 이상 휴식 안내, 120°C 이상 전원 OFF
// 정격 토크: 10 Nm, 예제 가이드: 8 Nm 이하 (처음 사용 시 3~5 Nm 권장)
#define KP_REPLAY               1.5f        // PD 재생 위치 이득 (Nm/deg)
#define KD_REPLAY               0.05f       // PD 재생 미분 이득
#define REPLAY_TORQUE_MAX       8.0f        // 재생 토크 포화 한계 (Nm) — 정격 10 Nm 대비 보수적 설정

// --- 투명 모드 파라미터 (교시 중, Ex.21과 동일) ---
#define G_ACC                   9.81f       // 중력 가속도 (m/s²)
#define MGL_EFF                 17.16f      // M·g·L_eff (70kg × 9.81 × 0.25m)
#define B_COULOMB_NM            0.3f        // 쿨롱 마찰 계수 (Nm)
#define B_VISCOUS_NMS           0.01f       // 점성 마찰 계수 (Nm·s/rad)
#define VEL_DZ_RADS             0.01f       // 속도 데드존 (rad/s)

// --- 제어 루프 타이밍 ---
#define CONTROL_DT              0.001f      // 제어 주기 (1ms = 1kHz)
#define RECORD_DT               0.010f      // 기록 주기 (10ms = 100Hz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS     500         // USB CDC 디버그 메시지 출력 주기 (ms)

// --- 각도 변환 매크로 ---
#define DEG_TO_RAD(d)           ((d) * 0.017453292f)    // π / 180

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 교시 상태 열거형
 * @details 스트리밍 데이터의 teach_mode 필드에 float으로 전송됩니다.
 */
typedef enum {
    TEACH_STATE_IDLE     = 0,   // 대기 중 (BTN1 교시 시작 대기)
    TEACH_STATE_RECORD   = 1,   // 교시 중 (인간이 컨트롤러 — transparent mode)
    TEACH_STATE_RECORDED = 2,   // 교시 완료 (재생 가능 상태)
    TEACH_STATE_REPLAY   = 3    // 재생 중 (PD 위치 추적)
} TeachState_t;

/**
 * @brief 교시 포인트 구조체
 * @details 100Hz로 샘플링된 전문가 궤적의 단일 프레임
 */
typedef struct {
    float theta_rh;     // 우측 고관절 각도 (deg)
    float theta_lh;     // 좌측 고관절 각도 (deg)
} TeachPoint_t;

/**
 * @brief USB 스트리밍용 데이터 구조체
 */
typedef struct {
    float teach_mode;       // 현재 상태 (0=IDLE, 1=RECORD, 2=RECORDED, 3=REPLAY)
    float teach_count;      // 기록된 포인트 수 (개)
    float theta_target;     // 재생 목표 각도 (deg) — REPLAY 중에만 유효
    float theta_actual;     // 현재 실제 각도 — 우측 기준 (deg)
} TeachStreamData_t;

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

// --- 교시 버퍼 ---
// [메모리 주의] 2000×8bytes = 16KB — SRAM에 정적 할당
// 더 큰 버퍼 필요 시: XM PSRAM(8MB) 활용 → Ex.10b의 PSRAM 패턴 참조
static TeachPoint_t s_teach_buf[MAX_TEACH_POINTS];
static uint32_t     s_teach_count   = 0;    // 현재 기록된 포인트 수
static uint32_t     s_replay_idx    = 0;    // 재생 인덱스 (0 ~ s_teach_count-1)
static uint32_t     s_rec_downcnt   = 0;    // 다운샘플 카운터 (0~9)

// --- 교시 상태 ---
static TeachState_t s_teach_state   = TEACH_STATE_IDLE;

// --- PD 재생 이전 오차 (미분 항 계산용) ---
static float s_prev_error_r         = 0.0f;
static float s_prev_error_l         = 0.0f;

// --- 재생 주기 타이머 (10ms마다 인덱스 전진) ---
static uint32_t s_replay_tick_last  = 0;    // 마지막 인덱스 전진 시각

// --- 각속도 추정용 (투명 모드 — 교시 중) ---
static float s_prev_angle_r_rad     = 0.0f;
static float s_prev_angle_l_rad     = 0.0f;
static bool  s_is_prev_valid        = false;

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer   = 0;

// --- USB 스트리밍 데이터 ---
static TeachStreamData_t s_stream_data;

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

// --- 교시 제어 ---
static void  _RunIdleMode(void);
static void  _RunRecordMode(void);
static void  _RunReplayMode(void);
static float _ComputeTransparentTorque(float angle_rad, float vel_rads);
static void  _ResetTeachBuffer(void);

// --- 유틸리티 ---
static float _ClampFloat(float value, float min_val, float max_val);
static float _SignFloat(float value);
static void  _HandleButtonInput(void);
static void  _UpdateLedIndicators(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(float theta_target_r, float theta_actual_r);

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

    // [상태 3] ACTIVE: 운동감각 교시 + 재생
    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드 0xF3)
    XM_SetUsbCustomMeta(0xF3,
        "[{\"name\":\"Teach Mode\",\"unit\":\"-\"},"
        "{\"name\":\"Teach Count\",\"unit\":\"pts\"},"
        "{\"name\":\"Theta Target\",\"unit\":\"deg\"},"
        "{\"name\":\"Theta Actual\",\"unit\":\"deg\"}]");

    // 초기 제어 모드: 모니터링
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
        XM_SendUsbDebugMessage("[KT] CM 연결됨 -> STANDBY\r\n");
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
        XM_SendUsbDebugMessage("[KT] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — 토크 제어 모드 전환 및 교시 시스템 초기화
 * @details
 * 초기 상태는 IDLE입니다.
 * BTN1을 눌러야 교시(RECORD)가 시작됩니다.
 * 교시 전 안내 메시지를 출력합니다.
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // 교시 상태 초기화
    s_teach_state = TEACH_STATE_IDLE;
    _ResetTeachBuffer();

    // PD 이전 오차 초기화
    s_prev_error_r = 0.0f;
    s_prev_error_l = 0.0f;

    // 각속도 추정 초기화
    s_prev_angle_r_rad = DEG_TO_RAD(XM.status.h10.rightHipMotorAngle);
    s_prev_angle_l_rad = DEG_TO_RAD(XM.status.h10.leftHipMotorAngle);
    s_is_prev_valid    = false;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED 초기 표시
    _UpdateLedIndicators();

    XM_SendUsbDebugMessage("[KT] 운동감각 교시 시스템 준비 완료\r\n");
    XM_SendUsbDebugMessage("[KT] BTN1: 교시 시작  BTN2: 재생  BTN3: 리셋\r\n");
    XM_SendUsbDebugMessage("[KT] 최대 교시 시간: 20초 (2000포인트 @ 100Hz)\r\n");
}

/**
 * @brief ACTIVE 루프 — 교시 상태에 따른 제어 분기
 * @details
 * 교시 상태기(TeachState_t)는 TSM 내부에서 s_teach_state로 관리됩니다.
 * TSM의 상태(OFF/STANDBY/ACTIVE)와 교시 상태는 계층적으로 분리됩니다:
 *   TSM: 시스템 레벨 (CM 연결, ASSIST 모드)
 *   TeachState: 교시 레벨 (IDLE/RECORD/RECORDED/REPLAY)
 */
static void Active_Loop(void)
{
    // H10이 ASSIST 해제 시 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[KT] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    // 교시 상태에 따른 제어 분기
    switch (s_teach_state) {
        case TEACH_STATE_IDLE:
            _RunIdleMode();
            break;

        case TEACH_STATE_RECORD:
            _RunRecordMode();
            break;

        case TEACH_STATE_RECORDED:
            // RECORDED 상태: 토크 인가 없음 (BTN2 대기)
            XM_SetAssistTorqueRH(0.0f);
            XM_SetAssistTorqueLH(0.0f);
            break;

        case TEACH_STATE_REPLAY:
            _RunReplayMode();
            break;

        default:
            // 방어적 프로그래밍: 알 수 없는 상태 — 안전 정지
            XM_SetAssistTorqueRH(0.0f);
            XM_SetAssistTorqueLH(0.0f);
            s_teach_state = TEACH_STATE_IDLE;
            break;
    }

    // USB 디버그 및 스트리밍 갱신
    float theta_actual_r = XM.status.h10.rightHipMotorAngle;
    float theta_target_r = (s_teach_state == TEACH_STATE_REPLAY && s_teach_count > 0)
                           ? s_teach_buf[s_replay_idx].theta_rh
                           : theta_actual_r;
    _UpdateUsbDebug();
    _UpdateStreamData(theta_target_r, theta_actual_r);
}

/**
 * @brief ACTIVE 탈출 — 안전 정지 절차
 */
static void Active_Exit(void)
{
    // 토크 즉시 해제
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    // 모니터링 모드로 복귀
    XM_SetControlMode(XM_CTRL_MONITOR);

    // 상태 초기화
    s_teach_state     = TEACH_STATE_IDLE;
    s_is_prev_valid   = false;

    // LED: STANDBY 표시
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[KT] 제어 종료 — 토크 해제\r\n");
}

// ==================== 교시 제어 함수 ====================

/**
 * @brief 교시 버퍼 초기화
 * @details
 * memset으로 전체 버퍼 클리어 + 카운터/인덱스 리셋
 */
static void _ResetTeachBuffer(void)
{
    memset(s_teach_buf, 0, sizeof(s_teach_buf));
    s_teach_count  = 0;
    s_replay_idx   = 0;
    s_rec_downcnt  = 0;
}

/**
 * @brief 투명 모드 토크 계산 (교시 중 인가 — Ex.21과 동일)
 * @details
 * 이것이 운동감각 교시의 핵심입니다:
 * 로봇이 중력과 마찰을 상쇄하여 "보이지 않는 존재"가 됩니다.
 * 인간 전문가가 로봇을 움직일 때 느끼는 저항이 최소화됩니다.
 * 이 상태에서: Human IS the Controller
 *
 * @param angle_rad  고관절 각도 (rad)
 * @param vel_rads   고관절 각속도 (rad/s)
 * @return 투명 모드 토크 (Nm)
 */
static float _ComputeTransparentTorque(float angle_rad, float vel_rads)
{
    // 중력 토크 상쇄: τ_grav = M·g·L_eff · sin(θ)
    float tau_grav = MGL_EFF * sinf(angle_rad);

    // 마찰 토크 상쇄: τ_fric = B_f·sign(θ̇) + B_v·θ̇
    float tau_fric = 0.0f;
    if (fabsf(vel_rads) > VEL_DZ_RADS) {
        tau_fric = B_COULOMB_NM * _SignFloat(vel_rads)
                 + B_VISCOUS_NMS * vel_rads;
    }

    // α=1.0 (완전 투명 모드 — 교시 정확도 최대화)
    return tau_grav + tau_fric;
}

/**
 * @brief IDLE 모드 — 토크 없음, 교시 시작 대기
 * @details
 * IDLE 상태에서는 중력+마찰 보상을 인가하지 않습니다.
 * 착용자가 스스로 균형을 잡고 있는 상태 (H10 자체 지지)
 */
static void _RunIdleMode(void)
{
    // IDLE: 토크 인가 없음 (H10 자체 지지 상태)
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
}

/**
 * @brief RECORD 모드 — 투명 모드 + 100Hz 궤적 기록
 * @details
 * 이것이 운동감각 교시의 본질입니다:
 * "This is kinesthetic teaching — human IS the controller"
 *
 * [기록 로직]
 * 1kHz 제어 루프에서 매 10ms마다 (다운샘플 카운터 기반) 좌/우 각도를 기록
 * 버퍼가 가득 차면 자동으로 RECORDED 상태로 전환
 *
 * [투명 모드]
 * α=1.0의 완전 투명 모드로 동작하여 전문가가 최소 저항으로 교시 가능
 * 로봇이 무겁게 느껴지면 교시 동작의 정확도가 떨어집니다
 */
static void _RunRecordMode(void)
{
    // --- 1. 현재 각도 획득 ---
    float angle_r_deg = XM.status.h10.rightHipMotorAngle;
    float angle_l_deg = XM.status.h10.leftHipMotorAngle;
    float angle_r_rad = DEG_TO_RAD(angle_r_deg);
    float angle_l_rad = DEG_TO_RAD(angle_l_deg);

    // --- 2. 각속도 추정 (투명 모드 마찰 보상에 사용) ---
    float vel_r_rads = 0.0f;
    float vel_l_rads = 0.0f;

    if (s_is_prev_valid) {
        vel_r_rads = (angle_r_rad - s_prev_angle_r_rad) / CONTROL_DT;
        vel_l_rads = (angle_l_rad - s_prev_angle_l_rad) / CONTROL_DT;
    } else {
        s_is_prev_valid = true;
    }

    s_prev_angle_r_rad = angle_r_rad;
    s_prev_angle_l_rad = angle_l_rad;

    // --- 3. 투명 모드 토크 인가 (α=1.0 — 완전 투명) ---
    // This is kinesthetic teaching — human IS the controller
    // 로봇이 중력과 마찰을 상쇄하여 전문가가 자유롭게 동작을 표현
    float tau_r = _ComputeTransparentTorque(angle_r_rad, vel_r_rads);
    float tau_l = _ComputeTransparentTorque(angle_l_rad, vel_l_rads);

    float tau_out_r = _ClampFloat(tau_r, -REPLAY_TORQUE_MAX, REPLAY_TORQUE_MAX);
    float tau_out_l = _ClampFloat(tau_l, -REPLAY_TORQUE_MAX, REPLAY_TORQUE_MAX);

    XM_SetAssistTorqueRH(tau_out_r);
    XM_SetAssistTorqueLH(tau_out_l);

    // --- 4. 100Hz 다운샘플 기록 ---
    s_rec_downcnt++;
    if (s_rec_downcnt >= RECORD_DOWNSAMPLE) {
        s_rec_downcnt = 0;

        // 버퍼 오버런 방어
        if (s_teach_count < MAX_TEACH_POINTS) {
            s_teach_buf[s_teach_count].theta_rh = angle_r_deg;
            s_teach_buf[s_teach_count].theta_lh = angle_l_deg;
            s_teach_count++;
        } else {
            // 버퍼 만료 → RECORDED 상태로 자동 전환
            s_teach_state = TEACH_STATE_RECORDED;

            // 전문가 데이터 캡처 완료 메시지
            // XM_SetUsbLogSource로 SD카드에도 저장 가능 (Ex.10b 참조)
            XM_SendUsbDebugMessage(
                "[KT] 전문가 데이터 캡처 완료 — "
                "PhAI Studio로 전송하여 AI 학습에 활용 가능\r\n");

            char buf[80];
            snprintf(buf, sizeof(buf),
                     "[KT] 기록 완료: %lu포인트 (%.1f초)\r\n",
                     (unsigned long)s_teach_count,
                     (double)((float)s_teach_count / 100.0f));
            XM_SendUsbDebugMessage(buf);

            // 이 데이터가 π0 스타일 VLA 모델의 학습 데이터가 됩니다
            XM_SendUsbDebugMessage(
                "[KT] 이 궤적 데이터로 VLA 모델 학습 → BTN2로 재생 확인\r\n");

            _UpdateLedIndicators();
        }
    }
}

/**
 * @brief REPLAY 모드 — PD 위치 추적으로 저장 궤적 재생
 * @details
 * [재생 타이밍]
 * 기록과 동일한 10ms 주기로 인덱스를 전진합니다.
 * 이를 통해 원래 교시 속도와 동일한 속도로 재생됩니다.
 *
 * [PD 제어]
 * τ = Kp·e + Kd·(e - e_prev)/dt
 *   e = θ_target - θ_actual (위치 오차, deg 단위)
 *
 * [재생 완료]
 * 마지막 포인트 도달 시 → RECORDED 상태로 복귀 (무한 루프 가능)
 * 루프 재생 원하면: s_replay_idx = 0 (인덱스 초기화)
 */
static void _RunReplayMode(void)
{
    // 빈 버퍼 방어 — 교시 없이 재생 시도 방어
    if (s_teach_count == 0) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        s_teach_state = TEACH_STATE_IDLE;
        return;
    }

    // 인덱스 범위 방어
    if (s_replay_idx >= s_teach_count) {
        s_replay_idx = s_teach_count - 1;
    }

    // --- 목표 각도 (기록된 궤적) ---
    float target_r = s_teach_buf[s_replay_idx].theta_rh;
    float target_l = s_teach_buf[s_replay_idx].theta_lh;

    // --- 현재 실제 각도 ---
    float actual_r = XM.status.h10.rightHipMotorAngle;
    float actual_l = XM.status.h10.leftHipMotorAngle;

    // --- PD 위치 추적 제어: τ = Kp·e + Kd·Δe/dt ---
    float error_r = target_r - actual_r;
    float error_l = target_l - actual_l;

    float tau_r = KP_REPLAY * error_r
                + KD_REPLAY * (error_r - s_prev_error_r) / CONTROL_DT;
    float tau_l = KP_REPLAY * error_l
                + KD_REPLAY * (error_l - s_prev_error_l) / CONTROL_DT;

    s_prev_error_r = error_r;
    s_prev_error_l = error_l;

    // 토크 포화
    float tau_out_r = _ClampFloat(tau_r, -REPLAY_TORQUE_MAX, REPLAY_TORQUE_MAX);
    float tau_out_l = _ClampFloat(tau_l, -REPLAY_TORQUE_MAX, REPLAY_TORQUE_MAX);

    XM_SetAssistTorqueRH(tau_out_r);
    XM_SetAssistTorqueLH(tau_out_l);

    // --- 재생 인덱스 전진 (10ms마다 — 기록 주기와 동일) ---
    uint32_t now = XM_GetTick();
    if (now - s_replay_tick_last >= (uint32_t)(RECORD_DOWNSAMPLE)) {
        s_replay_tick_last = now;
        s_replay_idx++;

        // 재생 완료 시 RECORDED 상태로 복귀
        if (s_replay_idx >= s_teach_count) {
            s_replay_idx  = 0;
            s_teach_state = TEACH_STATE_RECORDED;
            s_prev_error_r = 0.0f;
            s_prev_error_l = 0.0f;

            XM_SendUsbDebugMessage("[KT] 재생 완료 — RECORDED 복귀 (BTN2로 재재생)\r\n");
            _UpdateLedIndicators();
        }
    }
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
 * @brief 버튼 입력 처리
 * @details
 * - BTN1 클릭:
 *   IDLE     → RECORD: 교시 시작 (투명 모드 + 기록)
 *   RECORD   → RECORDED: 교시 중단 (버퍼 보존)
 *   기타 상태: 무시
 *
 * - BTN2 클릭:
 *   RECORDED → REPLAY: 재생 시작
 *   기타 상태: 무시
 *
 * - BTN3 클릭:
 *   모든 상태 → IDLE: 교시 버퍼 클리어 + 완전 리셋
 *   재생 중 비상 정지로도 사용
 */
static void _HandleButtonInput(void)
{
    // BTN1: 교시 시작 / 중단
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (s_teach_state == TEACH_STATE_IDLE) {
            // IDLE → RECORD: 교시 시작
            s_teach_state     = TEACH_STATE_RECORD;
            s_teach_count     = 0;
            s_rec_downcnt     = 0;
            s_is_prev_valid   = false;

            XM_SendUsbDebugMessage("[KT] BTN1: 교시 시작 — 전문가가 로봇을 이끌어 동작 교시\r\n");
            XM_SendUsbDebugMessage("[KT] This is kinesthetic teaching — human IS the controller\r\n");
            _UpdateLedIndicators();

        } else if (s_teach_state == TEACH_STATE_RECORD) {
            // RECORD → RECORDED: 교시 중단 (버퍼 보존)
            s_teach_state = TEACH_STATE_RECORDED;

            char buf[80];
            snprintf(buf, sizeof(buf),
                     "[KT] BTN1: 교시 완료 — %lu포인트 (%.1f초) 기록됨\r\n",
                     (unsigned long)s_teach_count,
                     (double)((float)s_teach_count / 100.0f));
            XM_SendUsbDebugMessage(buf);

            // 전문가 데이터 캡처 완료
            // XM_SetUsbLogSource로 SD카드에도 저장 가능 (Ex.10b 참조)
            XM_SendUsbDebugMessage(
                "[KT] 전문가 데이터 캡처 완료 — "
                "PhAI Studio로 전송하여 AI 학습에 활용 가능\r\n");

            _UpdateLedIndicators();
        }
        // RECORDED, REPLAY 상태에서는 BTN1 무시 (BTN3로 리셋)
    }

    // BTN2: 재생 시작
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        if (s_teach_state == TEACH_STATE_RECORDED && s_teach_count > 0) {
            // RECORDED → REPLAY: 재생 시작
            s_teach_state     = TEACH_STATE_REPLAY;
            s_replay_idx      = 0;
            s_prev_error_r    = 0.0f;
            s_prev_error_l    = 0.0f;
            s_replay_tick_last = XM_GetTick();

            char buf[80];
            snprintf(buf, sizeof(buf),
                     "[KT] BTN2: 재생 시작 — %lu포인트 재생 (%.1f초)\r\n",
                     (unsigned long)s_teach_count,
                     (double)((float)s_teach_count / 100.0f));
            XM_SendUsbDebugMessage(buf);

            _UpdateLedIndicators();
        } else if (s_teach_count == 0) {
            XM_SendUsbDebugMessage("[KT] BTN2: 교시 데이터 없음 — BTN1로 먼저 교시하세요\r\n");
        }
    }

    // BTN3: 완전 리셋 (어떤 상태에서도 IDLE로)
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        // 토크 즉시 해제 (재생 중 비상 정지 대응)
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);

        s_teach_state   = TEACH_STATE_IDLE;
        s_is_prev_valid = false;
        _ResetTeachBuffer();

        XM_SendUsbDebugMessage("[KT] BTN3: 리셋 — 교시 버퍼 클리어, IDLE 복귀\r\n");
        _UpdateLedIndicators();
    }
}

/**
 * @brief LED 상태 갱신
 * @details
 * LED1: ACTIVE 표시 / 교시 상태별 차별화
 *   IDLE     → 중간 깜빡임 (500ms)
 *   RECORD   → 빠른 깜빡임 (100ms) — 기록 중 시각적 경고
 *   RECORDED → 상시 점등
 *   REPLAY   → 빠른 깜빡임 (200ms)
 * LED2: RECORD 상태 표시 (기록 중: 점등, 아니면: 소등)
 * LED3: REPLAY 상태 표시 (재생 중: 점등, 아니면: 소등)
 */
static void _UpdateLedIndicators(void)
{
    switch (s_teach_state) {
        case TEACH_STATE_IDLE:
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
            XM_SetLedState(XM_LED_2, XM_OFF);
            XM_SetLedState(XM_LED_3, XM_OFF);
            break;

        case TEACH_STATE_RECORD:
            // 기록 중: LED1 빠른 깜빡임 + LED2 점등 (주의 신호)
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 100);
            XM_SetLedState(XM_LED_2, XM_ON);
            XM_SetLedState(XM_LED_3, XM_OFF);
            break;

        case TEACH_STATE_RECORDED:
            // 기록 완료: LED1 상시 점등 (대기)
            XM_SetLedState(XM_LED_1, XM_ON);
            XM_SetLedState(XM_LED_2, XM_OFF);
            XM_SetLedState(XM_LED_3, XM_OFF);
            break;

        case TEACH_STATE_REPLAY:
            // 재생 중: LED1 중간 깜빡임 + LED3 점등
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
            XM_SetLedState(XM_LED_2, XM_OFF);
            XM_SetLedState(XM_LED_3, XM_ON);
            break;

        default:
            XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
            XM_SetLedState(XM_LED_2, XM_OFF);
            XM_SetLedState(XM_LED_3, XM_OFF);
            break;
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 * @details
 * 교시 상태, 기록 포인트 수, 재생 진행도를 출력합니다.
 */
static void _UpdateUsbDebug(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        // 상태 이름 문자열 (가독성)
        static const char * const state_names[] = {
            "IDLE", "RECORD", "RECORDED", "REPLAY"
        };
        const char *state_str = (s_teach_state < 4)
                                 ? state_names[s_teach_state]
                                 : "UNKNOWN";

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "KT|%s|cnt=%lu|rh=%.1f|lh=%.1f|replay=%lu/%lu\r\n",
                 state_str,
                 (unsigned long)s_teach_count,
                 (double)XM.status.h10.rightHipMotorAngle,
                 (double)XM.status.h10.leftHipMotorAngle,
                 (unsigned long)s_replay_idx,
                 (unsigned long)s_teach_count);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신 (매 루프)
 * @param theta_target_r  재생 목표 각도 (deg) — REPLAY 중에만 유효
 * @param theta_actual_r  현재 실제 각도 — 우측 기준 (deg)
 */
static void _UpdateStreamData(float theta_target_r, float theta_actual_r)
{
    s_stream_data.teach_mode  = (float)s_teach_state;
    s_stream_data.teach_count = (float)s_teach_count;
    s_stream_data.theta_target = theta_target_r;
    s_stream_data.theta_actual = theta_actual_r;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF3);
}
