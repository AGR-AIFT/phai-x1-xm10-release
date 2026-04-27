/**
 ******************************************************************************
 * @file    torque_ramp_profile.c
 * @author  JIMIN
 * @brief   [중급] 시간 기반 사다리꼴 토크 프로파일 제어
 * @details
 * BTN1을 누르면 사다리꼴(Trapezoidal) 전류 프로파일을 반복 생성합니다.
 *
 * [프로파일 구간]
 *   Phase 0 — Ramp Up  : 0A → 1A (1초, 선형 증가)
 *   Phase 1 — Hold     : 1A 유지  (1초)
 *   Phase 2 — Ramp Down: 1A → 0A (1초, 선형 감소)
 *   → Phase 0으로 복귀하여 무한 반복
 *
 * [동작 방식]
 *   - User_Loop()가 1ms 주기로 호출됨 (1kHz)
 *   - BTN1 클릭: 프로파일 시작/정지 토글
 *   - BTN2 클릭: 목표 전류 0.5A 단위로 증가 (0.5 ~ 3.0A, 래핑)
 *   - ASSIST 모드 해제 시 안전 정지
 *
 * @see     docs/api-reference/XM_Control.md
 * @version 1.0
 * @date    Apr 03, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// --- 프로파일 파라미터 ---
#define RAMP_UP_MS          1000        // Ramp Up 구간 (ms)
#define HOLD_MS             1000        // Hold 구간 (ms)
#define RAMP_DOWN_MS        1000        // Ramp Down 구간 (ms)
#define PROFILE_PERIOD_MS   (RAMP_UP_MS + HOLD_MS + RAMP_DOWN_MS)  // 전체 주기 (3000ms)

#define TARGET_CURRENT_A    1.0f        // 초기 목표 전류 (A)
#define MAX_CURRENT_A       5.0f        // 전류 포화 한계 (A)

// --- 목표 전류 조절 파라미터 ---
#define CURRENT_STEP_A      0.5f        // BTN2 클릭 시 증가량 (A)
#define CURRENT_MAX_A       3.0f        // 최대 목표 전류 (A)
#define CURRENT_MIN_A       0.5f        // 래핑 후 최소 목표 전류 (A)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS 500         // USB CDC 디버그 메시지 출력 주기 (ms)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 프로파일 구간 열거형 */
typedef enum {
    PHASE_RAMP_UP   = 0,    // 0A → target (선형 증가)
    PHASE_HOLD      = 1,    // target 유지
    PHASE_RAMP_DOWN = 2     // target → 0A (선형 감소)
} ProfilePhase_e;

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details 토크 프로파일 핵심 변수를 실시간 스트리밍합니다.
 */
typedef struct {
    float target_current;   // 목표 전류 (A)
    float output_current;   // 현재 출력 전류 (A)
    float phase;            // 현재 구간 (0=RampUp, 1=Hold, 2=RampDown)
    float running;          // 동작 상태 (1.0=동작중, 0.0=정지)
} ProfileStreamData_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

// --- Task State Machine ---
static XmTsmHandle_t s_tsm;

// --- 프로파일 상태 변수 ---
static bool     s_profile_running = false;  // 프로파일 동작 여부
static uint32_t s_phase_tick      = 0;      // 현재 구간 내 경과 tick (ms 단위, 1ms마다 +1)
static ProfilePhase_e s_phase     = PHASE_RAMP_UP;  // 현재 구간

// --- 토크 출력 ---
static float s_target_current  = TARGET_CURRENT_A;  // 목표 전류 크기 (A)
static float s_output_current  = 0.0f;              // 현재 출력 전류 (A)

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer = 0;

// --- USB 스트리밍 데이터 ---
static ProfileStreamData_t s_stream_data;

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

// --- 프로파일 생성 함수 ---
static float _ComputeProfileOutput(void);
static void  _ResetProfile(void);

// --- 유틸리티 함수 ---
static float _ClampFloat(float value, float min_val, float max_val);
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 사용자 초기 설정 - TSM 생성 및 상태 등록
 */
void User_Setup(void)
{
    // TSM 생성 (초기 상태: OFF - CM 연결 대기)
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

    // [상태 3] ACTIVE: 토크 프로파일 제어 실행
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드)
    XM_SetUsbCustomMeta(0xF1,
        "[{\"name\":\"Target Current\",\"unit\":\"A\"},"
        "{\"name\":\"Output Current\",\"unit\":\"A\"},"
        "{\"name\":\"Phase\",\"unit\":\"-\"},"
        "{\"name\":\"Running\",\"unit\":\"-\"}]");

    // 초기 제어 모드: 모니터링 (토크 미인가)
    XM_SetControlMode(XM_CTRL_MONITOR);
}

/**
 * @brief 메인 루프 - 1ms 주기로 호출됨
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
 * @brief OFF 상태 - CM 연결 대기
 */
static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SendUsbDebugMessage("[Profile] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태 - H10 ASSIST 모드 진입 대기
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[Profile] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 - 토크 제어 모드 설정 및 변수 초기화
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // 프로파일 초기화 (정지 상태로 시작)
    _ResetProfile();
    s_profile_running = false;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED: ACTIVE 상태 표시
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);  // LED2: 프로파일 동작 시 ON

    XM_SendUsbDebugMessage("[Profile] ACTIVE 진입 — BTN1으로 시작\r\n");
}

/**
 * @brief ACTIVE 루프 - 토크 프로파일 생성 (1ms 주기)
 * @details
 * 매 루프마다 다음을 수행합니다:
 * 1. 버튼 입력 처리 (시작/정지, 목표 전류 변경)
 * 2. 프로파일 출력 계산 (동작 중일 때만)
 * 3. 토크 명령 전송
 * 4. USB 디버그/스트리밍 갱신
 */
static void Active_Loop(void)
{
    // H10이 ASSIST 해제 시 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[Profile] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    // --- 1. 버튼 입력 처리 ---
    _HandleButtonInput();

    // --- 2. 프로파일 출력 계산 ---
    if (s_profile_running) {
        s_output_current = _ComputeProfileOutput();
    } else {
        s_output_current = 0.0f;
    }

    // --- 3. 토크 명령 전송 (좌/우 대칭) ---
    XM_SetAssistTorqueRH(s_output_current);
    XM_SetAssistTorqueLH(s_output_current);

    // --- 4. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug();
    _UpdateStreamData();
}

/**
 * @brief ACTIVE 탈출 - 안전 정지 절차
 */
static void Active_Exit(void)
{
    // 토크 0으로 안전 해제
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    // 모니터링 모드로 복귀
    XM_SetControlMode(XM_CTRL_MONITOR);

    // 프로파일 초기화
    _ResetProfile();
    s_profile_running = false;
    s_output_current  = 0.0f;

    // LED 복원
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);

    XM_SendUsbDebugMessage("[Profile] 제어 종료 — 토크 해제\r\n");
}

// ==================== 프로파일 생성 함수 ====================

/**
 * @brief 사다리꼴 토크 프로파일 출력 계산 (1ms마다 호출)
 * @return 현재 시점의 출력 전류 (A)
 *
 * @details
 * 1ms마다 s_phase_tick을 +1 증가시키고, 현재 구간(phase)에 따라
 * 출력 전류를 선형 보간(ramp) 또는 고정값(hold)으로 계산합니다.
 *
 * [타이밍]
 *   Phase 0 (Ramp Up)  : tick 0 ~ 999  → output = target * (tick / 1000)
 *   Phase 1 (Hold)     : tick 0 ~ 999  → output = target
 *   Phase 2 (Ramp Down): tick 0 ~ 999  → output = target * (1 - tick / 1000)
 *   → Phase 0으로 복귀
 */
static float _ComputeProfileOutput(void)
{
    float output = 0.0f;

    switch (s_phase) {
    case PHASE_RAMP_UP:
        // 선형 증가: 0A → target_current over RAMP_UP_MS
        output = s_target_current * ((float)s_phase_tick / (float)RAMP_UP_MS);
        s_phase_tick++;
        if (s_phase_tick >= RAMP_UP_MS) {
            s_phase = PHASE_HOLD;
            s_phase_tick = 0;
        }
        break;

    case PHASE_HOLD:
        // 고정 출력: target_current 유지
        output = s_target_current;
        s_phase_tick++;
        if (s_phase_tick >= HOLD_MS) {
            s_phase = PHASE_RAMP_DOWN;
            s_phase_tick = 0;
        }
        break;

    case PHASE_RAMP_DOWN:
        // 선형 감소: target_current → 0A over RAMP_DOWN_MS
        output = s_target_current * (1.0f - (float)s_phase_tick / (float)RAMP_DOWN_MS);
        s_phase_tick++;
        if (s_phase_tick >= RAMP_DOWN_MS) {
            s_phase = PHASE_RAMP_UP;
            s_phase_tick = 0;
        }
        break;
    }

    // 안전 클램핑
    return _ClampFloat(output, 0.0f, MAX_CURRENT_A);
}

/**
 * @brief 프로파일 상태 초기화
 */
static void _ResetProfile(void)
{
    s_phase      = PHASE_RAMP_UP;
    s_phase_tick = 0;
}

// ==================== 유틸리티 함수 ====================

/**
 * @brief float 값을 [min, max] 범위로 클램핑합니다.
 */
static float _ClampFloat(float value, float min_val, float max_val)
{
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * @brief 버튼 입력 처리
 * @details
 * - BTN1 클릭: 프로파일 시작/정지 토글
 * - BTN2 클릭: 목표 전류 0.5A 단위 증가 (0.5 ~ 3.0A 래핑)
 */
static void _HandleButtonInput(void)
{
    // BTN1: 프로파일 시작/정지 토글
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_profile_running = !s_profile_running;

        if (s_profile_running) {
            _ResetProfile();
            XM_SetLedState(XM_LED_2, XM_ON);
            XM_SendUsbDebugMessage("[Profile] BTN1: 프로파일 시작\r\n");
        } else {
            s_output_current = 0.0f;
            XM_SetAssistTorqueRH(0.0f);
            XM_SetAssistTorqueLH(0.0f);
            XM_SetLedState(XM_LED_2, XM_OFF);
            XM_SendUsbDebugMessage("[Profile] BTN1: 프로파일 정지\r\n");
        }
    }

    // BTN2: 목표 전류 증가 (래핑)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_target_current += CURRENT_STEP_A;
        if (s_target_current > CURRENT_MAX_A) {
            s_target_current = CURRENT_MIN_A;
        }

        char buf[60];
        snprintf(buf, sizeof(buf),
                 "[Profile] BTN2: 목표 전류 = %.1fA\r\n", s_target_current);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 */
static void _UpdateUsbDebug(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        const char *phase_str;
        switch (s_phase) {
        case PHASE_RAMP_UP:   phase_str = "RampUp";   break;
        case PHASE_HOLD:      phase_str = "Hold";      break;
        case PHASE_RAMP_DOWN: phase_str = "RampDn";    break;
        default:              phase_str = "???";        break;
        }

        char buf[100];
        snprintf(buf, sizeof(buf),
                 "Profile | Tgt:%.1fA Out:%.2fA Phase:%s Run:%d\r\n",
                 s_target_current, s_output_current,
                 phase_str, (int)s_profile_running);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신
 */
static void _UpdateStreamData(void)
{
    s_stream_data.target_current = s_target_current;
    s_stream_data.output_current = s_output_current;
    s_stream_data.phase          = (float)s_phase;
    s_stream_data.running        = s_profile_running ? 1.0f : 0.0f;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF1);
}
