/**
 ******************************************************************************
 * @file    grf_gait_intent.c
 * @author  HyundoKim
 * @brief   [고급] 발 접촉 이벤트 기반 실시간 보행 의도 감지 (Stage 2 — Intent Sensing)
 * @details
 * Stage 2: 발 접촉 센서(GRF — Ground Reaction Force) 이벤트를 이용하여
 * 보행 위상(Gait Phase)을 실시간으로 추정하고, 이에 동기화된 보조 토크를 생성합니다.
 * 이 예제는 원시 센서 신호가 어떻게 "의도(intent)"가 되는지를 보여줍니다.
 *
 * [보행 의도 감지 원리 — 신호 흐름]
 * 발 접촉 센서 (isRightFootContact / isLeftFootContact)
 *   ↓ 상태 전이 감지
 * 보행 이벤트 추출 (Heel Strike / Toe Off)
 *   ↓ 이벤트 타이밍
 * 보행 주기 추정 (Period Estimation via HS-to-HS 시간 측정)
 *   ↓ 위상 계산
 * 보행 위상 추정 (Phase: 0.0 ~ 1.0)
 *   ↓ 위상 기반 토크 프로파일
 * 보조 토크 생성 (Sinusoidal / Stance-assist / Swing-assist)
 *
 * [보행 이벤트 정의]
 * Heel Strike (발뒤꿈치 착지):
 *   was_contact = false → is_contact = true
 *   → 입각기(Stance Phase) 시작 신호
 *   → 이 순간 보행 위상(phase)을 0으로 리셋하고 주기 갱신
 *
 * Toe Off (발끝 이탈):
 *   was_contact = true → is_contact = false
 *   → 유각기(Swing Phase) 시작 신호
 *   → 보조 전략 전환에 활용 가능
 *
 * [보행 위상 추정 (Phase Estimator)]
 * phase[k] = phase[k-1] + dt / T_estimated
 *   → phase는 0.0 (Heel Strike)에서 1.0 (다음 Heel Strike 직전)으로 선형 증가
 *   → T_estimated: 직전 두 Heel Strike 사이의 시간 (연속 갱신)
 *   → 부드러운 위상 추정 — 이벤트 사이에서도 연속 신호 유지
 *
 * [토크 프로파일]
 * PUSH (입각기 보조): τ = A · sin(phase · π)
 *   → phase=0.5(입각기 중간)에서 최대 보조
 *   → 지면 반발력이 가장 클 때 보조 — 에너지 효율 최적
 *
 * PULL (유각기 보조): τ = A · sin((phase - 0.5) · π) [phase > 0.5]
 *   → 유각기 진입 시 굴곡 보조 (스윙 레그 가속)
 *   → 보행 속도 향상에 기여
 *
 * [Body Data 의존성 경고]
 * isRightFootContact / isLeftFootContact는 H10 Body Data 패킷에 포함됩니다.
 * H10 소프트웨어 설정에서 Body Data 전송이 활성화되어 있어야 합니다.
 * 비활성화 시: 항상 false → Heel Strike 이벤트 미발생 → phase 추정 불가
 *
 * @see     Winter, D.A. (2009) "Biomechanics and Motor Control of Human Movement"
 *          4th ed., Wiley-Blackwell (보행 위상 정의 기준)
 * @see     Gervasi, A. et al. (2020) "Exoskeleton gait assistance based on
 *          continuous gait phase estimation" IROS 2020
 * @see     Ex.31 friction_comp_dob.c (Stage 1 — 투명 모드 기반)
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

// --- 수학 상수 ---
#define PI_VALUE                3.14159265f

// --- 보행 위상 추정 파라미터 ---
// 보행 주기 초기값: 1.2초 (정상 성인 보행 평균 약 1.1~1.3초)
// Heel Strike가 감지될 때까지 이 값으로 초기화됩니다.
#define GAIT_PERIOD_INIT_S      1.2f

// 보행 주기 클램핑: 비정상적 값 방어
// 너무 짧으면 → phase 급증 → 과보조 위험
// 너무 길면  → phase 정체 → 보조 타이밍 지연
#define GAIT_PERIOD_MIN_S       0.4f        // 최소 주기 (≈ 빠른 달리기)
#define GAIT_PERIOD_MAX_S       3.0f        // 최대 주기 (≈ 느린 보행)

// --- 보조 토크 파라미터 ---
#define NUM_TORQUE_PRESETS      4           // 토크 프리셋 수
#define TORQUE_AMP_INIT_NM      1.0f        // 초기 토크 진폭 (Nm) — 두 번째 프리셋

// --- 제어 루프 타이밍 ---
#define CONTROL_DT              0.001f      // 제어 주기 (1ms = 1kHz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS     200         // USB CDC 디버그 메시지 출력 주기 (ms)
                                            // 보행 이벤트 특성상 200ms로 단축

// --- 각도 변환 매크로 ---
#define DEG_TO_RAD(d)           ((d) * 0.017453292f)    // π / 180

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 보조 전략 종류
 * @details
 * PUSH: 입각기(Stance) 보조 — 지면 반발 구간에서 신전 토크 인가
 * PULL: 유각기(Swing) 보조 — 스윙 레그 가속을 위한 굴곡 토크 인가
 */
typedef enum {
    ASSIST_MODE_PUSH = 0,   // 입각기 보조 (Stance Phase Assist)
    ASSIST_MODE_PULL = 1,   // 유각기 보조 (Swing Phase Assist)
    ASSIST_MODE_COUNT = 2
} AssistMode_t;

/**
 * @brief 단일 다리의 보행 위상 추정기 상태
 * @details 좌/우 독립적으로 보행 위상을 추정하기 위한 구조체입니다.
 */
typedef struct {
    float    phase;             // 현재 보행 위상 (0.0 ~ 1.0)
    float    period_s;          // 추정된 보행 주기 (초)
    uint32_t last_hs_tick;      // 마지막 Heel Strike 시각 (ms)
    bool     has_first_hs;      // 첫 Heel Strike 발생 여부 (주기 추정 유효성)
    bool     prev_contact;      // 이전 루프의 발 접촉 상태 (이벤트 감지용)
} GaitEstimator_t;

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details 보행 위상 및 보조 토크를 실시간 스트리밍합니다.
 */
typedef struct {
    float phase_rh;     // 우측 보행 위상 (0.0 ~ 1.0) [-]
    float phase_lh;     // 좌측 보행 위상 (0.0 ~ 1.0) [-]
    float torque_rh;    // 우측 보조 토크 (Nm)
    float torque_lh;    // 좌측 보조 토크 (Nm)
} GrfStreamData_t;

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

// --- 보행 위상 추정기 (좌/우 독립) ---
static GaitEstimator_t s_gait_r;   // 우측 보행 추정기
static GaitEstimator_t s_gait_l;   // 좌측 보행 추정기

// --- 보조 파라미터 ---
static const float s_torque_presets[NUM_TORQUE_PRESETS] = { 0.5f, 1.0f, 2.0f, 3.0f };
static uint8_t s_torque_preset_idx = 1;             // 기본: 1.0 Nm
static float   s_torque_amp_nm     = TORQUE_AMP_INIT_NM;
static AssistMode_t s_assist_mode  = ASSIST_MODE_PUSH;

// --- 실시간 토크 (디버그/스트리밍) ---
static float s_tau_out_r            = 0.0f;
static float s_tau_out_l            = 0.0f;

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer   = 0;

// --- USB 스트리밍 데이터 ---
static GrfStreamData_t s_stream_data;

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

// --- 보행 위상 추정 및 토크 생성 ---
static void  _RunGaitIntentControl(void);
static void  _UpdateGaitEstimator(GaitEstimator_t *est, bool is_contact, uint32_t tick);
static float _ComputeAssistTorque(float phase, AssistMode_t mode, float amplitude);
static void  _ResetGaitEstimator(GaitEstimator_t *est);

// --- 유틸리티 ---
static float _ClampFloat(float value, float min_val, float max_val);
static void  _HandleButtonInput(void);
static void  _UpdateLedIndicators(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 사용자 초기 설정 — TSM 생성 및 상태 등록
 *
 * @warning Body Data 의존성:
 *   isRightFootContact / isLeftFootContact는 H10 Body Data 패킷에 의존합니다.
 *   H10 소프트웨어에서 Body Data 전송이 비활성화된 경우,
 *   발 접촉 이벤트가 전혀 발생하지 않아 보행 위상 추정이 불가능합니다.
 *   H10 설정을 반드시 확인하세요.
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

    // [상태 3] ACTIVE: GRF 이벤트 기반 보행 의도 감지 및 보조
    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드 0xF2)
    XM_SetUsbCustomMeta(0xF2,
        "[{\"name\":\"Phase RH\",\"unit\":\"-\"},"
        "{\"name\":\"Phase LH\",\"unit\":\"-\"},"
        "{\"name\":\"Torque RH\",\"unit\":\"Nm\"},"
        "{\"name\":\"Torque LH\",\"unit\":\"Nm\"}]");

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
        XM_SendUsbDebugMessage("[GRF] CM 연결됨 -> STANDBY\r\n");
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
        XM_SendUsbDebugMessage("[GRF] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — 토크 제어 모드 전환 및 보행 추정기 초기화
 * @details
 * 보행 추정기는 첫 Heel Strike가 발생할 때까지 초기 주기값으로 동작합니다.
 * 두 번째 Heel Strike부터 실제 보행 주기로 갱신됩니다.
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // 보행 추정기 초기화 (좌/우 독립)
    _ResetGaitEstimator(&s_gait_r);
    _ResetGaitEstimator(&s_gait_l);

    // 보조 파라미터 기본값 설정
    s_torque_preset_idx = 1;
    s_torque_amp_nm     = s_torque_presets[s_torque_preset_idx];
    s_assist_mode       = ASSIST_MODE_PUSH;

    // 제어 변수 초기화
    s_tau_out_r = 0.0f;
    s_tau_out_l = 0.0f;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED 초기 표시
    _UpdateLedIndicators();

    XM_SendUsbDebugMessage("[GRF] ACTIVE 진입 — 보행 의도 감지 시작\r\n");
    XM_SendUsbDebugMessage("[GRF] 주의: H10 Body Data 전송 활성화 필요\r\n");
    XM_SendUsbDebugMessage("[GRF] 걷기 시작하면 Heel Strike 이벤트 감지 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — GRF 이벤트 기반 보행 의도 감지 및 보조 토크 생성
 */
static void Active_Loop(void)
{
    // H10이 ASSIST 해제 시 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[GRF] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();
    _RunGaitIntentControl();
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

    // 제어 변수 초기화
    s_tau_out_r = 0.0f;
    s_tau_out_l = 0.0f;

    // LED: STANDBY 표시
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[GRF] 제어 종료 — 토크 해제\r\n");
}

// ==================== 보행 위상 추정 및 토크 생성 ====================

/**
 * @brief 보행 추정기 초기화
 * @details
 * 추정기를 안전한 초기 상태로 설정합니다.
 * phase = 0.0, period = 초기값(1.2초), 이벤트 이력 클리어
 *
 * @param est  초기화할 보행 추정기 포인터
 */
static void _ResetGaitEstimator(GaitEstimator_t *est)
{
    if (est == NULL) {
        return;
    }

    est->phase        = 0.0f;
    est->period_s     = GAIT_PERIOD_INIT_S;
    est->last_hs_tick = 0;
    est->has_first_hs = false;
    est->prev_contact = false;
}

/**
 * @brief 보행 위상 추정기 업데이트 (매 1ms 호출)
 * @details
 * [이벤트 감지]
 * Heel Strike: prev=false, cur=true
 *   → phase 리셋 (0.0)
 *   → 보행 주기 갱신: T = (현재 시각 - 마지막 HS 시각) / 1000.0
 *   → 주기 클램핑: [GAIT_PERIOD_MIN, GAIT_PERIOD_MAX]
 *
 * Toe Off: prev=true, cur=false
 *   → 이벤트 기록 (현재는 로그만; 고급 구현에서 위상 보정 가능)
 *
 * [위상 증분]
 * phase[k] += dt / T_estimated
 *   → 항상 0.0~1.0 범위로 래핑(wrapping)
 *   → 보행 이벤트 없이도 선형 증가 (연속 신호)
 *
 * @param est        보행 위상 추정기 포인터
 * @param is_contact 현재 루프의 발 접촉 상태
 * @param tick       현재 시각 (ms, XM_GetTick() 반환값)
 */
static void _UpdateGaitEstimator(GaitEstimator_t *est, bool is_contact, uint32_t tick)
{
    if (est == NULL) {
        return;
    }

    bool was_contact = est->prev_contact;
    est->prev_contact = is_contact;

    // --- Heel Strike 이벤트 감지: false → true 전이 ---
    if (!was_contact && is_contact) {
        if (est->has_first_hs) {
            // 두 번째 이후 Heel Strike: 주기 갱신
            uint32_t elapsed_ms = tick - est->last_hs_tick;
            float period_new    = (float)elapsed_ms * 0.001f;   // ms → s

            // 비정상적 주기 방어: 너무 짧거나 긴 경우 클램핑
            est->period_s = _ClampFloat(period_new,
                                        GAIT_PERIOD_MIN_S,
                                        GAIT_PERIOD_MAX_S);
        } else {
            // 첫 번째 Heel Strike: 주기 추정 시작
            est->has_first_hs = true;
        }

        // 위상 리셋 (Heel Strike = 보행 주기 시작)
        est->phase        = 0.0f;
        est->last_hs_tick = tick;
    }

    // --- Toe Off 이벤트 감지: true → false 전이 ---
    // 현재는 이벤트 발생 기록 (USB 디버그에서 확인 가능)
    // 고급 구현: 유각기 보조 전략 전환 트리거로 활용 가능
    // (Toe Off 시점의 phase ≈ 0.4~0.6 — 보행 패턴에 따라 다름)

    // --- 위상 선형 증분: phase += dt / T_estimated ---
    est->phase += CONTROL_DT / est->period_s;

    // 위상 래핑: 1.0 초과 시 0.0으로 롤오버
    // 이상적으로는 Heel Strike에서 정확히 0으로 리셋되지만,
    // 주기 추정 오차 보정을 위해 래핑도 병행 적용
    if (est->phase >= 1.0f) {
        est->phase -= 1.0f;
    }
}

/**
 * @brief 보행 위상 기반 보조 토크 계산
 * @details
 * PUSH 모드 (입각기 보조):
 *   τ = A · sin(phase · π)
 *   → phase=0 (HS)에서 0, phase=0.5 (입각기 중간)에서 최대, phase=1.0에서 0
 *   → 입각기 전반에 걸쳐 지속적 보조 — 보행 효율 향상
 *
 * PULL 모드 (유각기 보조):
 *   τ = A · sin(max(0, phase - 0.5) · 2π)  [phase > 0.5]
 *   → phase < 0.5: 0 (입각기 — 무간섭)
 *   → phase > 0.5: 유각기 굴곡 보조 (스윙 레그 가속)
 *   → 보행 속도 향상, 발 들기 지원
 *
 * @param phase     현재 보행 위상 (0.0 ~ 1.0)
 * @param mode      보조 전략 (PUSH / PULL)
 * @param amplitude 보조 토크 진폭 (Nm)
 * @return 보조 토크 (Nm), 포화 없음 (호출부에서 처리)
 */
static float _ComputeAssistTorque(float phase, AssistMode_t mode, float amplitude)
{
    float torque = 0.0f;

    switch (mode) {
        case ASSIST_MODE_PUSH:
            // 입각기 보조: sin 반파 (0 ~ π)
            // 에너지 효율 최적: 지면 반발력이 가장 클 때 보조
            torque = amplitude * sinf(phase * PI_VALUE);
            break;

        case ASSIST_MODE_PULL:
            // 유각기 보조: 위상 후반부 (0.5 ~ 1.0)에서만 활성
            if (phase > 0.5f) {
                // (phase - 0.5)를 [0, 0.5] 범위로 정규화 후 sin 적용
                torque = amplitude * sinf((phase - 0.5f) * 2.0f * PI_VALUE);
            }
            break;

        default:
            // 방어적 프로그래밍: 알 수 없는 모드는 0 출력
            torque = 0.0f;
            break;
    }

    return torque;
}

/**
 * @brief GRF 기반 보행 의도 감지 및 보조 토크 생성 메인 함수
 * @details
 * [1단계] 발 접촉 상태 읽기 (H10 Body Data)
 * [2단계] 좌/우 보행 위상 추정기 갱신
 * [3단계] 위상 기반 보조 토크 계산
 * [4단계] 토크 인가
 * [5단계] 디버그 및 스트리밍 갱신
 *
 * @warning isRightFootContact / isLeftFootContact는 H10 Body Data 필요
 *          Body Data가 오지 않으면 항상 false → 보조 없음
 */
static void _RunGaitIntentControl(void)
{
    uint32_t tick = XM_GetTick();

    // --- 1. 발 접촉 상태 읽기 ---
    // [Body Data 의존] H10 Body Data 전송이 활성화되어 있어야 합니다.
    // 활성화 방법: H10 소프트웨어 설정 → Body Data → Enable
    bool contact_r = XM.status.h10.isRightFootContact;
    bool contact_l = XM.status.h10.isLeftFootContact;

    // --- 2. 보행 위상 추정기 갱신 (좌/우 독립) ---
    _UpdateGaitEstimator(&s_gait_r, contact_r, tick);
    _UpdateGaitEstimator(&s_gait_l, contact_l, tick);

    // --- 3. 위상 기반 보조 토크 계산 ---
    float tau_r = _ComputeAssistTorque(s_gait_r.phase, s_assist_mode, s_torque_amp_nm);
    float tau_l = _ComputeAssistTorque(s_gait_l.phase, s_assist_mode, s_torque_amp_nm);

    // 첫 Heel Strike 이전: 보조 토크 인가 금지 (주기 불확실)
    // 보행이 시작되기 전에 sin 위상이 임의의 값을 가질 수 있음
    if (!s_gait_r.has_first_hs) {
        tau_r = 0.0f;
    }
    if (!s_gait_l.has_first_hs) {
        tau_l = 0.0f;
    }

    // --- 4. 좌/우 독립 토크 인가 ---
    XM_SetAssistTorqueRH(tau_r);
    XM_SetAssistTorqueLH(tau_l);

    // 디버그용 저장
    s_tau_out_r = tau_r;
    s_tau_out_l = tau_l;

    // --- 5. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug();
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
 * @brief 버튼 입력 처리
 * @details
 * - BTN1 클릭: 토크 진폭 순환 (0.5→1.0→2.0→3.0 Nm)
 *   → 착용자 체력/목표 보조 수준에 따라 조절
 *   → 처음 사용 시 0.5Nm에서 시작하여 점진적 증가 권장
 *
 * - BTN2 클릭: 보조 모드 순환 (PUSH ↔ PULL)
 *   → PUSH: 입각기 보조 (대부분의 보행 보조에 적합)
 *   → PULL: 유각기 보조 (발 들기 어려운 환자에게 유용)
 *
 * - BTN3 클릭: 보행 위상 추정기 리셋
 *   → 걷다 멈출 때, 또는 추정기가 올바르지 않을 때 초기화
 *   → 리셋 후 다시 걸어야 주기 재추정 시작
 */
static void _HandleButtonInput(void)
{
    // BTN1: 토크 진폭 순환
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_torque_preset_idx = (s_torque_preset_idx + 1) % NUM_TORQUE_PRESETS;
        s_torque_amp_nm     = s_torque_presets[s_torque_preset_idx];

        char buf[64];
        snprintf(buf, sizeof(buf),
                 "[GRF] BTN1: 토크 진폭 = %.1fNm\r\n",
                 (double)s_torque_amp_nm);
        XM_SendUsbDebugMessage(buf);

        _UpdateLedIndicators();
    }

    // BTN2: 보조 모드 순환 (PUSH ↔ PULL)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_assist_mode = (AssistMode_t)((s_assist_mode + 1) % ASSIST_MODE_COUNT);

        if (s_assist_mode == ASSIST_MODE_PUSH) {
            XM_SendUsbDebugMessage("[GRF] BTN2: PUSH 모드 (입각기 보조)\r\n");
        } else {
            XM_SendUsbDebugMessage("[GRF] BTN2: PULL 모드 (유각기 보조)\r\n");
        }

        _UpdateLedIndicators();
    }

    // BTN3: 보행 위상 추정기 리셋
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        _ResetGaitEstimator(&s_gait_r);
        _ResetGaitEstimator(&s_gait_l);
        XM_SendUsbDebugMessage("[GRF] BTN3: 보행 위상 추정기 리셋\r\n");
    }
}

/**
 * @brief LED 상태 갱신
 * @details
 * LED1: ACTIVE 표시 (빠른 깜빡임)
 * LED2: 보조 모드 표시 (PUSH=상시, PULL=느린 깜빡임)
 * LED3: Heel Strike 이벤트 표시 (첫 HS 수신 여부)
 */
static void _UpdateLedIndicators(void)
{
    // LED1: ACTIVE 상태 — 빠른 깜빡임
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);

    // LED2: 보조 모드 표시
    if (s_assist_mode == ASSIST_MODE_PUSH) {
        XM_SetLedState(XM_LED_2, XM_ON);           // PUSH: 상시 점등
    } else {
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 400);  // PULL: 중간 깜빡임
    }

    // LED3: 토크 레벨 표시 (인덱스가 높을수록 빠른 깜빡임)
    // 0.5Nm→1000ms, 1.0Nm→500ms, 2.0Nm→250ms, 3.0Nm→125ms
    static const uint16_t led3_periods[NUM_TORQUE_PRESETS] = { 1000, 500, 250, 125 };
    XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, led3_periods[s_torque_preset_idx]);
}

/**
 * @brief USB CDC 디버그 메시지 출력 (200ms 주기)
 * @details
 * 보행 위상, 추정 주기, 보조 토크, 발 접촉 상태를 출력합니다.
 * 보행 이벤트 특성상 200ms 주기로 단축하여 이벤트 포착 용이.
 */
static void _UpdateUsbDebug(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "GRF|R:ph=%.2f T=%.1fs|L:ph=%.2f T=%.1fs|tR=%.2f tL=%.2f\r\n",
                 (double)s_gait_r.phase,
                 (double)s_gait_r.period_s,
                 (double)s_gait_l.phase,
                 (double)s_gait_l.period_s,
                 (double)s_tau_out_r,
                 (double)s_tau_out_l);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신 (매 루프)
 */
static void _UpdateStreamData(void)
{
    s_stream_data.phase_rh  = s_gait_r.phase;
    s_stream_data.phase_lh  = s_gait_l.phase;
    s_stream_data.torque_rh = s_tau_out_r;
    s_stream_data.torque_lh = s_tau_out_l;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF2);
}
