/**
 ******************************************************************************
 * @file    pd_realtime_control.c
 * @author  HyundoKim
 * @brief   [중급] 계단식 램프 프로파일 토크 제어
 * @details
 * ramp_time과 hold_time 두 파라미터를 설정하여 계단식 토크 프로파일을
 * 생성하고 XM_SetAssistTorque API로 H10 고관절 모터에 인가합니다.
 *
 * [토크 프로파일 — 계단식 램프]
 *   0→1 (1s ramp) → 1 유지 (2s) → 1→2 (1s) → 2 유지 (2s) → ... → 10 유지 (2s)
 *   → 10→9 (1s) → 9 유지 (2s) → ... → 1→0 (1s) → 0 유지 (2s) → 종료
 *
 * @see     docs/api-reference/XM_Control.md
 * @version 1.0
 * @date    Mar 09, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// --- 계단식 램프 프로파일 파라미터 ---
#define RAMP_TIME_MS        1000    // 각 단계 램프 시간 (ms)
#define HOLD_TIME_MS        2000    // 각 단계 유지 시간 (ms)
#define TORQUE_STEP_NM      1.0f    // 단계당 토크 증가량 (Nm)
#define MAX_LEVEL           10      // 최대 레벨 (0→10→0, 총 20단계)

// --- 제어 루프 타이밍 ---
#define CONTROL_DT          0.001f  // 제어 주기 (1ms = 1kHz)

// --- Fail-safe 토크 한계 ---
//     control_ON 활성 시 raw ADC 전압이 토크 명령으로 직접 인가되므로,
//     런어웨이/센서 고장 대비 안전 상한으로 클램프한다.
//     (EMG 예제의 EMG_MAX_TORQUE_NM=2.5f 와 동일 기준)
#define ASSIST_TORQUE_LIMIT_NM  2.5f

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS 500     // USB CDC 디버그 메시지 출력 주기 (ms)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 각 단계 내 위상
 */
typedef enum {
    PHASE_RAMP,     // 램프 구간 (from → to)
    PHASE_HOLD,     // 유지 구간
    PHASE_DONE      // 전체 프로파일 완료 (0 유지)
} StepPhase_t;

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details PF3~PF6 ADC 전압을 실시간 스트리밍합니다.
 *          PC monitor에서 4채널 float로 표시됩니다.
 */
typedef struct {
    float pf3_volt;         // PF3 raw voltage (V)
    float pf4_volt;         // PF4 raw voltage (V)
    float pf5_volt;         // PF5 raw voltage (V)
    float pf6_volt;         // PF6 raw voltage (V)
} RampStreamData_t;

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

// --- 계단식 램프 프로파일 상태 변수 ---
static int s_step               = 0;                // 현재 단계 (0~19: 상승10 + 하강10)
static StepPhase_t s_phase      = PHASE_RAMP;       // 단계 내 위상
static uint32_t s_phase_tick    = 0;                // 현재 위상 시작 시각 (ms)
static float s_torque_cmd       = 0.0f;             // 최종 토크 명령 (Nm)

// --- ADC 채널 정의 (PF3~PF6, 모두 ADC3 16-bit) ---
//     DIO_1~DIO_4 (PF3~PF6) 핀을 ADC3로 전환해서 사용 (Control_Setup에서 전환)
#define ADC_CH_COUNT    4
typedef enum {
    CH_PF3 = 0,    // EXT_ADC_5 (PF3 / DIO_1, ADC3, 16-bit)
    CH_PF4 = 1,    // EXT_ADC_6 (PF4 / DIO_2, ADC3, 16-bit)
    CH_PF5 = 2,    // EXT_ADC_7 (PF5 / DIO_3, ADC3, 16-bit)
    CH_PF6 = 3     // EXT_ADC_8 (PF6 / DIO_4, ADC3, 16-bit)
} AdcChIdx_t;

static const XmAdcPin_t s_adc_pins[ADC_CH_COUNT] = {
    XM_EXT_ADC_5,   // CH_PF3 (DIO_1 → ADC3)
    XM_EXT_ADC_6,   // CH_PF4 (DIO_2 → ADC3)
    XM_EXT_ADC_7,   // CH_PF5 (DIO_3 → ADC3)
    XM_EXT_ADC_8    // CH_PF6 (DIO_4 → ADC3)
};

// 환산 전압 (V) — calibration 미적용
float pf3_volt;
float pf4_volt;
float pf5_volt;
float pf6_volt;

// Calibration 후 전압 (V) — bias 제거됨
float pf3_volt_cal;
float pf4_volt_cal;
float pf5_volt_cal;
float pf6_volt_cal;
uint16_t control_ON       = 0U;  // 0: 토크 출력 OFF(기본), 1: 출력 ON — 벤치에서 명시적으로 1로 설정
uint16_t torque_input_pair = 0U; // 0: PF3/PF4 -> RH/LH, 1: PF5/PF6 -> RH/LH

// --- Calibration 상태 ---
typedef enum {
    CAL_IDLE,           // 평상시 (bias 적용)
    CAL_RUNNING,        // 3초간 샘플 누적 중
    CAL_DONE            // 1회 이상 완료
} CalState_t;

#define CAL_DURATION_MS     3000

static CalState_t s_cal_state    = CAL_IDLE;
static uint32_t   s_cal_tick     = 0;
static uint32_t   s_cal_count    = 0;
static double     s_cal_sum[ADC_CH_COUNT] = {0};   // 누적 (double로 정밀도 확보)
static float      s_cal_bias[ADC_CH_COUNT] = {0};  // 최종 bias (V)

// --- USB 디버그 타이머 ---
static uint32_t s_usb_debug_timer = 0;

// --- USB 스트리밍 데이터 ---
static RampStreamData_t s_stream_data;

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
static void  _UpdateRampProfile(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(void);

// --- ADC / Calibration ---
static float _ReadAdcVoltage(XmAdcPin_t pin);
static void  _SampleAdcChannels(void);
static void  _UpdateCalibration(void);
static void  _StartCalibration(void);

// --- 유틸 ---
static float _ClampFloat(float x, float min_value, float max_value);

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
    XM_SetExtPowerVoltage(XM_EXT_PWR_5V);

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

    // [상태 3] ACTIVE: 계단식 램프 토크 제어 실행
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드)
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"PF3\",\"unit\":\"V\"},"
        "{\"name\":\"PF4\",\"unit\":\"V\"},"
        "{\"name\":\"PF5\",\"unit\":\"V\"},"
        "{\"name\":\"PF6\",\"unit\":\"V\"}]");

    // DIO_1~DIO_4 (PF3~PF6)를 ADC3 모드로 전환 — 16-bit 4채널 ADC 입력
    XM_SwitchDioToAdc(XM_EXT_DIO_1);
    XM_SwitchDioToAdc(XM_EXT_DIO_2);
    XM_SwitchDioToAdc(XM_EXT_DIO_3);
    XM_SwitchDioToAdc(XM_EXT_DIO_4);

    // 초기 제어 모드: 모니터링 (토크 미인가)
    XM_SetControlMode(XM_CTRL_MONITOR);
}

/**
 * @brief 메인 루프 — 1ms 주기로 호출됨
 */
void User_Loop(void)
{
    // TSM 핸들 생성 실패 시 안전 정지 (NULL 역참조 HardFault 방지)
    if (!s_tsm) {
        return;
    }

    // CM 연결 끊김 시 OFF 상태로 강제 전환 (안전 우선)
    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }

    XM_TSM_Run(s_tsm);

    // [필수] 버튼 이벤트 디바운싱 + LED 효과 타이머 틱 (xm_api_led_btn.h).
    // 호출하지 않으면 XM_GetButtonEvent() 가 항상 NONE → BTN1 캘리브레이션 동작 불가.
    XM_IO_Update();
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
        XM_SendUsbDebugMessage("[RAMP] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태 — H10 ASSIST 모드 진입 대기
 * @details H10이 ASSIST 모드로 전환되면 램프 제어를 시작합니다.
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[RAMP] ASSIST 모드 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 진입 — 토크 제어 모드 설정 및 변수 초기화
 */
static void Active_Entry(void)
{
    // 토크 직접 제어 모드로 전환
    XM_SetControlMode(XM_CTRL_TORQUE);

    // 계단식 램프 프로파일 초기화
    s_step = 0;
    s_phase = PHASE_RAMP;
    s_phase_tick = XM_GetTick();
    s_torque_cmd = 0.0f;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED: ACTIVE 상태 표시 (빠른 깜빡임)
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);

    XM_SendUsbDebugMessage("[RAMP] ACTIVE 진입 — 계단식 램프 토크 제어 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — 계단식 램프 토크 프로파일 실행 (1ms 주기)
 * @details
 * 총 20단계 (상승 10 + 하강 10):
 *   step 0~9  : 0→1→2→...→10 (각 1s ramp + 2s hold)
 *   step 10~19: 10→9→8→...→0 (각 1s ramp + 2s hold)
 *   완료 후 0 유지
 */
static void Active_Loop(void)
{
    // H10이 STANDBY로 전환되면 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        XM_SendUsbDebugMessage("[RAMP] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    // --- 1. 램프 프로파일 갱신 ---
    _UpdateRampProfile();

    // --- 2. 토크 명령 전송 ---
//    XM_SetAssistTorqueRH(s_torque_cmd);
//    XM_SetAssistTorqueLH(0);
    // 4채널 ADC 샘플링 (PF3/PF4/PF5/PF6, ADC3 16-bit)
    _SampleAdcChannels();

    // Calibration 트리거 / 진행 / 완료 처리
    _UpdateCalibration();

    // --- 3. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug();
    _UpdateStreamData();

    if (control_ON == 1)  {
        float rh_cmd, lh_cmd;
        if (torque_input_pair == 1) {
            rh_cmd = pf5_volt_cal;
            lh_cmd = pf6_volt_cal;
        } else {
            rh_cmd = pf3_volt_cal;
            lh_cmd = pf4_volt_cal;
        }

        // Fail-safe: 안전 한계 [-LIMIT, +LIMIT] 로 클램프 (런어웨이/센서 고장 방지)
        rh_cmd = _ClampFloat(rh_cmd, -ASSIST_TORQUE_LIMIT_NM, ASSIST_TORQUE_LIMIT_NM);
        lh_cmd = _ClampFloat(lh_cmd, -ASSIST_TORQUE_LIMIT_NM, ASSIST_TORQUE_LIMIT_NM);

        XM_SetAssistTorqueRH(rh_cmd);
        XM_SetAssistTorqueLH(lh_cmd);
    } else {
        // 출력 OFF — 명시적으로 0 토크 (이전 명령 잔류 방지)
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
    }

}

/**
 * @brief ACTIVE 탈출 — 안전 정지 절차
 */
static void Active_Exit(void)
{
    // 토크 0으로 안전 해제
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    // 모니터링 모드로 복귀 (토크 명령 비활성화)
    XM_SetControlMode(XM_CTRL_MONITOR);

    // 램프 상태 초기화
    s_torque_cmd = 0.0f;

    // LED: STANDBY 표시 (심장박동)
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);

    XM_SendUsbDebugMessage("[RAMP] 제어 종료 — 토크 해제\r\n");
}

// ==================== 유틸리티 함수 ====================

/**
 * @brief 계단식 램프 프로파일 — 위상 전이 및 토크 계산
 * @details
 * 총 20단계 (상승 10 + 하강 10):
 *   step 0~9  (상승): from=step, to=step+1  (0→1, 1→2, ..., 9→10)
 *   step 10~19(하강): from=20-step, to=19-step (10→9, 9→8, ..., 1→0)
 * 각 단계: RAMP(1s) → HOLD(2s) → 다음 단계
 * 20단계 완료 후 DONE (0 유지)
 */
static void _UpdateRampProfile(void)
{
    if (s_phase == PHASE_DONE) {
        s_torque_cmd = 0.0f;
        return;
    }

    // 현재 단계의 시작/끝 토크 레벨 계산
    float from_level, to_level;
    if (s_step < MAX_LEVEL) {
        // 상승: 0→1, 1→2, ..., 9→10
        from_level = (float)s_step * TORQUE_STEP_NM;
        to_level   = (float)(s_step + 1) * TORQUE_STEP_NM;
    } else {
        // 하강: 10→9, 9→8, ..., 1→0
        from_level = (float)(2 * MAX_LEVEL - s_step) * TORQUE_STEP_NM;
        to_level   = (float)(2 * MAX_LEVEL - s_step - 1) * TORQUE_STEP_NM;
    }

    uint32_t elapsed = XM_GetTick() - s_phase_tick;

    switch (s_phase) {
    case PHASE_RAMP:
        if (elapsed >= RAMP_TIME_MS) {
            s_phase = PHASE_HOLD;
            s_phase_tick = XM_GetTick();
            s_torque_cmd = to_level;
        } else {
            float ratio = (float)elapsed / (float)RAMP_TIME_MS;
            s_torque_cmd = from_level + (to_level - from_level) * ratio;
        }
        break;

    case PHASE_HOLD:
        s_torque_cmd = to_level;
        if (elapsed >= HOLD_TIME_MS) {
            s_step++;
            if (s_step >= 2 * MAX_LEVEL) {
                // 전체 프로파일 완료
                s_phase = PHASE_DONE;
                s_torque_cmd = 0.0f;
            } else {
                s_phase = PHASE_RAMP;
                s_phase_tick = XM_GetTick();
            }
        }
        break;

    default:
        break;
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

        char buf[80];
        snprintf(buf, sizeof(buf),
                 "RAMP | Step:%d/%d Ph:%d Tau:%.2f\r\n",
                 s_step, 2 * MAX_LEVEL, (int)s_phase, s_torque_cmd);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신
 */
static void _UpdateStreamData(void)
{
    s_stream_data.pf3_volt = pf3_volt;
    s_stream_data.pf4_volt = pf4_volt;
    s_stream_data.pf5_volt = pf5_volt;
    s_stream_data.pf6_volt = pf6_volt;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

// ==================== ADC / Calibration ====================

/**
 * @brief 단일 ADC 핀에서 전압(V)을 읽어옵니다.
 * @details
 * XM_AnalogReadMillivolts()는 핀별 native resolution을 자동으로 반영해서
 * mV 단위 환산값을 돌려줍니다 — PF3~PF6(ADC3, 16-bit) 모두
 * 0~3300 mV 범위로 정확히 환산됩니다.
 */
static float _ReadAdcVoltage(XmAdcPin_t pin)
{
    return (float)XM_AnalogReadMillivolts(pin) * 0.001f;
}

/**
 * @brief 4채널(PF3/PF4/PF5/PF6) 동시 샘플링 + bias 보정
 */
static void _SampleAdcChannels(void)
{
    float v[ADC_CH_COUNT];
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        v[i] = _ReadAdcVoltage(s_adc_pins[i]);
    }

    // 원시 환산값 (calibration 무관)
    pf3_volt = v[CH_PF3];
    pf4_volt = v[CH_PF4];
    pf5_volt = v[CH_PF5];
    pf6_volt = v[CH_PF6];

    // calibrated 값 — bias가 잡혀있으면 빼서 출력, 아니면 원시값 그대로
    pf3_volt_cal = v[CH_PF3] - s_cal_bias[CH_PF3];
    pf4_volt_cal = v[CH_PF4] - s_cal_bias[CH_PF4];
    pf5_volt_cal = v[CH_PF5] - s_cal_bias[CH_PF5];
    pf6_volt_cal = v[CH_PF6] - s_cal_bias[CH_PF6];

    // CALIBRATING 중이면 누적 (raw 4채널)
    if (s_cal_state == CAL_RUNNING) {
        for (int i = 0; i < ADC_CH_COUNT; i++) {
            s_cal_sum[i] += v[i];
        }
        s_cal_count++;
    }
}

/**
 * @brief BTN_1 클릭 → 3초 calibration 진행 → bias 확정
 */
static void _UpdateCalibration(void)
{
    // BTN_1 클릭 이벤트로 calibration 시작
    XmBtnEvent_t evt = XM_GetButtonEvent(XM_BTN_1);
    if (evt == XM_BTN_CLICK && s_cal_state != CAL_RUNNING) {
        _StartCalibration();
        return;
    }

    // 진행 중이면 3초 경과 체크
    if (s_cal_state == CAL_RUNNING) {
        uint32_t elapsed = XM_GetTick() - s_cal_tick;
        if (elapsed >= CAL_DURATION_MS) {
            // 평균 = bias
            if (s_cal_count > 0) {
                for (int i = 0; i < ADC_CH_COUNT; i++) {
                    s_cal_bias[i] = (float)(s_cal_sum[i] / (double)s_cal_count);
                }
            }
            s_cal_state = CAL_DONE;
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);  // ACTIVE LED 복귀

            char buf[120];
            snprintf(buf, sizeof(buf),
                     "[CAL] 완료 (n=%lu) | PF3:%.3fV PF4:%.3fV PF5:%.3fV PF6:%.3fV\r\n",
                     (unsigned long)s_cal_count,
                     s_cal_bias[CH_PF3], s_cal_bias[CH_PF4],
                     s_cal_bias[CH_PF5], s_cal_bias[CH_PF6]);
            XM_SendUsbDebugMessage(buf);
        }
    }
}

/**
 * @brief Calibration 시작 — 누적 버퍼 리셋, LED 빠르게 깜빡
 */
static void _StartCalibration(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_cal_sum[i] = 0.0;
    }
    s_cal_count = 0;
    s_cal_tick  = XM_GetTick();
    s_cal_state = CAL_RUNNING;

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 50);  // 50ms — 매우 빠른 깜빡임
    XM_SendUsbDebugMessage("[CAL] 시작 — 3초간 ADC bias 측정\r\n");
}

/**
 * @brief float 값을 [min, max] 범위로 클램핑합니다.
 */
static float _ClampFloat(float x, float min_value, float max_value)
{
    if (x < min_value) {
        return min_value;
    }
    if (x > max_value) {
        return max_value;
    }
    return x;
}
