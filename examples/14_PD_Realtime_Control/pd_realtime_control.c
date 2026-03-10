/**
 ******************************************************************************
 * @file    pd_realtime_control.c
 * @author  HyundoKim
 * @brief   [중급] PD 비례-미분 실시간 토크 제어
 * @details
 * 목표 각도(target_angle)를 설정하고 PD 제어기로 실시간 토크를 계산하여
 * XM_SetAssistTorque API를 통해 H10 고관절 모터에 직접 토크를 인가합니다.
 *
 * [제어 이론 — PD 제어기]
 * PD(Proportional-Derivative) 제어기는 가장 기본적인 피드백 제어 기법입니다.
 *   tau = Kp * e(t) + Kd * de(t)/dt
 *   - e(t) = theta_ref - theta(t)   : 위치 오차 (비례항의 입력)
 *   - de(t)/dt ≈ (e[k] - e[k-1])/dt : 이산 미분 근사 (후향 차분법)
 *   - Kp: 비례 게인 — 오차에 비례하는 복원력 (스프링 상수와 유사)
 *   - Kd: 미분 게인 — 오차 변화율에 비례하는 감쇠력 (댐퍼와 유사)
 *
 * [포화(Saturation)]
 * 실제 모터는 출력 토크 한계가 존재하므로, 계산된 토크를
 * [-MAX_TORQUE, +MAX_TORQUE] 범위로 클램핑하여 안전성을 확보합니다.
 * 이는 actuator saturation 모델에 해당합니다.
 *
 * [버튼 조작]
 * - BTN1 클릭: 목표 각도 부호 반전 (양 ↔ 음)
 * - BTN2 클릭: 목표 각도 5도씩 증가 (최대 25도, 이후 5도로 래핑)
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

// --- PD 제어기 튜닝 파라미터 ---
#define KP_GAIN             0.5f    // 비례 게인 (Nm/deg) — 오차 1도당 0.5 Nm 복원력
#define KD_GAIN             0.02f   // 미분 게인 (Nm·s/deg) — 오차 변화율 감쇠
#define TARGET_ANGLE_DEG    10.0f   // 초기 목표 각도 (deg)
#define MAX_TORQUE_NM       5.0f    // 토크 포화 한계 (Nm) — 모터 보호

// --- 목표 각도 조절 파라미터 ---
#define TARGET_ANGLE_STEP   5.0f    // BTN2 클릭 시 증가량 (deg)
#define TARGET_ANGLE_MAX    25.0f   // 최대 목표 각도 (deg)
#define TARGET_ANGLE_MIN    5.0f    // 래핑 후 최소 목표 각도 (deg)

// --- 제어 루프 타이밍 ---
#define CONTROL_DT          0.001f  // 제어 주기 (1ms = 1kHz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS 500     // USB CDC 디버그 메시지 출력 주기 (ms)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details PD 제어 핵심 변수 4개를 실시간 스트리밍합니다.
 *          PhAI Studio User Custom 모드에서 4채널 float로 표시됩니다.
 */
typedef struct {
    float target_angle;     // 목표 각도 (deg)
    float current_angle;    // 현재 각도 (deg)
    float error;            // 위치 오차 (deg)
    float torque;           // 출력 토크 (Nm)
} PdStreamData_t;

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

// --- PD 제어기 상태 변수 ---
static float s_target_angle = TARGET_ANGLE_DEG;   // 현재 목표 각도 (deg)
static float s_prev_error   = 0.0f;               // 이전 루프의 오차 (이산 미분용)
static float s_torque_cmd   = 0.0f;               // 최종 토크 명령 (Nm)

// --- 목표 각도 부호 (양/음 방향) ---
static float s_target_sign  = 1.0f;               // +1.0 또는 -1.0
static float s_target_magnitude = TARGET_ANGLE_DEG; // 절대값 크기

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer = 0;

// --- USB 스트리밍 데이터 ---
static PdStreamData_t s_stream_data;

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
static void _UpdateUsbDebug(float current_angle, float error);
static void _UpdateStreamData(float current_angle, float error);

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

    // [상태 3] ACTIVE: PD 토크 제어 실행
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드)
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Target Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Current Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Error\",\"unit\":\"deg\"},"
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
        XM_SendUsbDebugMessage("[PD] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태 — H10 ASSIST 모드 진입 대기
 * @details H10이 ASSIST 모드로 전환되면 PD 제어를 시작합니다.
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[PD] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — 토크 제어 모드 설정 및 변수 초기화
 * @details
 * XM_CTRL_TORQUE 모드를 활성화하여 XM_SetAssistTorque API를 통해
 * 직접 토크 명령을 인가할 수 있도록 합니다.
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // PD 제어기 상태 초기화
    s_prev_error = 0.0f;
    s_torque_cmd = 0.0f;

    // 목표 각도 초기화
    s_target_sign = 1.0f;
    s_target_magnitude = TARGET_ANGLE_DEG;
    s_target_angle = s_target_sign * s_target_magnitude;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED: ACTIVE 상태 표시 (빠른 깜빡임)
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    // LED2: 목표 각도 양방향 표시 (ON = 양, OFF = 음)
    XM_SetLedState(XM_LED_2, XM_ON);

    XM_SendUsbDebugMessage("[PD] ACTIVE 진입 — 토크 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — PD 제어기 실행 (1ms 주기)
 * @details
 * 매 루프마다 다음을 수행합니다:
 * 1. 버튼 입력 처리 (목표 각도 변경)
 * 2. PD 제어 연산
 * 3. 토크 포화 (클램핑)
 * 4. 토크 명령 전송
 * 5. USB 디버그/스트리밍 갱신
 *
 * [이산 PD 제어 수식]
 *   e[k] = theta_ref - theta[k]
 *   de[k] = (e[k] - e[k-1]) / dt     ← 후향 차분법 (Backward Difference)
 *   tau[k] = Kp * e[k] + Kd * de[k]
 *   tau_out = clamp(tau[k], -MAX, +MAX)
 *
 * 후향 차분법은 가장 단순한 이산 미분 근사로, 노이즈에 민감할 수 있으나
 * 1ms 제어 주기에서는 충분한 정확도를 제공합니다.
 */
static void Active_Loop(void)
{
    // H10이 STANDBY로 전환되면 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[PD] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    // --- 1. 버튼 입력 처리 ---
    _HandleButtonInput();

    // --- 2. PD 제어 연산 ---
    // 현재 우측 고관절 모터 각도 읽기
    float current_angle = XM.status.h10.rightHipMotorAngle;

    // 위치 오차 계산: e[k] = theta_ref - theta[k]
    float error = s_target_angle - current_angle;

    // 이산 미분 근사 (후향 차분): de[k] = (e[k] - e[k-1]) / dt
    float derivative = (error - s_prev_error) / CONTROL_DT;

    // PD 제어 법칙: tau = Kp * e + Kd * de/dt
    float torque_raw = (KP_GAIN * error) + (KD_GAIN * derivative);

    // --- 3. 토크 포화 (Actuator Saturation) ---
    // 모터 보호를 위해 출력 토크를 안전 범위로 제한
    s_torque_cmd = _ClampFloat(torque_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    // --- 4. 토크 명령 전송 ---
    // 좌/우 동일 토크 인가 (대칭 제어)
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);

    // --- 5. 이전 오차 저장 (다음 루프의 미분 계산용) ---
    s_prev_error = error;

    // --- 6. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug(current_angle, error);
    _UpdateStreamData(current_angle, error);
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

    // PD 제어기 상태 초기화
    s_prev_error = 0.0f;
    s_torque_cmd = 0.0f;

    // LED: STANDBY 표시 (심장박동)
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);

    XM_SendUsbDebugMessage("[PD] 제어 종료 — 토크 해제\r\n");
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
 * - BTN1 클릭: 목표 각도 부호 반전 (양 ↔ 음)
 *   → 관절 굴곡/신전 방향 전환 테스트에 유용
 * - BTN2 클릭: 목표 각도 5도씩 증가 (5→10→15→20→25→5 래핑)
 *   → 다양한 진폭에서 PD 응답 특성 관찰에 유용
 */
static void _HandleButtonInput(void)
{
    // BTN1: 목표 각도 방향 반전
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_target_sign *= -1.0f;
        s_target_angle = s_target_sign * s_target_magnitude;

        // LED2로 방향 표시: ON = 양(+), OFF = 음(-)
        if (s_target_sign > 0.0f) {
            XM_SetLedState(XM_LED_2, XM_ON);
        } else {
            XM_SetLedState(XM_LED_2, XM_OFF);
        }

        XM_SendUsbDebugMessage("[PD] BTN1: 방향 반전\r\n");
    }

    // BTN2: 목표 각도 증가 (래핑)
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_target_magnitude += TARGET_ANGLE_STEP;
        if (s_target_magnitude > TARGET_ANGLE_MAX) {
            s_target_magnitude = TARGET_ANGLE_MIN;
        }
        s_target_angle = s_target_sign * s_target_magnitude;

        XM_SendUsbDebugMessage("[PD] BTN2: 목표 각도 변경\r\n");
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 * @param current_angle 현재 모터 각도 (deg)
 * @param error         위치 오차 (deg)
 */
static void _UpdateUsbDebug(float current_angle, float error)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        /*
         * snprintf + %.1f 사용 — ARM GCC의 nano printf에서 float을
         * 지원하려면 링커 옵션 -u _printf_float이 필요합니다.
         * 본 프로젝트는 이미 활성화되어 있으므로 snprintf로 직접 출력합니다.
         */
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "PD | Tgt:%.1f Cur:%.1f Err:%.1f Tau:%.2f\r\n",
                 s_target_angle, current_angle, error, s_torque_cmd);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신
 * @param current_angle 현재 모터 각도 (deg)
 * @param error         위치 오차 (deg)
 */
static void _UpdateStreamData(float current_angle, float error)
{
    s_stream_data.target_angle  = s_target_angle;
    s_stream_data.current_angle = current_angle;
    s_stream_data.error         = error;
    s_stream_data.torque        = s_torque_cmd;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}
