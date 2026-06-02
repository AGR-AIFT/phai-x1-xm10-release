/**
 ******************************************************************************
 * @file    Final_FSR.c
 * @author  HyundoKim
 * @brief   FSR proportional assist torque-control project backbone.
 * @details
 * ============================================================================
 * 1. Purpose
 * ============================================================================
 * This file is a project backbone for pressure-proportional hip assistance.
 * It reads four FSR channels through external ADC inputs, calibrates each
 * channel, converts normalized pressure into bounded torque commands, and
 * sends selected signals to a PC through USB CDC for live monitoring and CSV
 * logging.
 *
 * This is NOT a fuzzy-logic gait-phase detector. Torque magnitude increases
 * continuously with normalized FSR load. Use Final_FSR_Fuzzy_Logic.c when the
 * project requires left/right gait-event detection and phase-triggered pulses.
 *
 * ============================================================================
 * 2. Sensor Mapping and Torque Routing
 * ============================================================================
 * ADC input mapping:
 *   PF3 = EXT_ADC_5 / DIO_1
 *   PF4 = EXT_ADC_6 / DIO_2
 *   PF5 = EXT_ADC_7 / DIO_3
 *   PF6 = EXT_ADC_8 / DIO_4
 *
 * torque_input_pair selects the sensor pair routed to the H10:
 *   0 = PF3 -> right hip, PF4 -> left hip
 *   1 = PF5 -> right hip, PF6 -> left hip
 *
 * Signal-processing path:
 *   ADC voltage -> 20 Hz low-pass filter -> calibration normalization
 *   -> load clamp -> contact hysteresis -> proportional torque clamp
 *
 * ============================================================================
 * 3. Required Operating Procedure
 * ============================================================================
 * 1) Put on the shoes and switch the H10 device to ASSIST mode.
 * 2) Keep all FSRs unloaded, click BTN1, and remain still for 3 seconds.
 * 3) Apply a representative full load, click BTN2, and hold it for 3 seconds.
 * 4) Select torque_input_pair in STM32CubeIDE Live Expressions.
 * 5) Validate the torque direction and amplitude on a bench setup.
 * 6) Set control_ON = 1 only after validation.
 *
 * BTN3 restores default calibration values.
 *
 * Important: this legacy proportional example does not enforce BTN1-before-BTN2
 * ordering and does not expose a calibration_ready safety gate. Complete both
 * calibration steps manually before setting control_ON = 1.
 *
 * ============================================================================
 * 4. Important Live Expressions Variables
 * ============================================================================
 * Write from debugger:
 *   control_ON          : 0 = torque disabled, 1 = proportional torque enabled
 *   torque_input_pair   : 0 = PF3/PF4 pair, 1 = PF5/PF6 pair
 *
 * Observe only:
 *   pf3_volt .. pf6_volt         : raw sensor voltages
 *   pf3_volt_cal .. pf6_volt_cal : filtered voltages after bias removal
 *   pf3_load .. pf6_load         : calibrated normalized loads
 *   pf3_contact .. pf6_contact   : hysteresis-based contact states
 *   pf3_torque_nm .. pf6_torque_nm : calculated proportional torques
 *
 * ============================================================================
 * 5. USB CDC Data Logging
 * ============================================================================
 * Module ID 0xF0 sends four float channels every control loop for PC-side CSV
 * logging and live plotting:
 *   1) PF3 Load = pf3_load
 *   2) PF4 Load = pf4_load
 *   3) PF5 Load = pf5_load
 *   4) PF6 Load = pf6_load
 *
 * The current implementation sends CDC data at the 1 kHz control-loop rate.
 * For a student project, reduce the send rate before adding more channels.
 *
 * ============================================================================
 * 6. Recommended Student Design Tasks (High-Level Only)
 * ============================================================================
 * Example A - Change proportional gain:
 *   Adjust FSR_MAX_TORQUE_NM conservatively. Start low and validate direction
 *   on a bench setup before a wearable test.
 *
 * Example B - Change the torque curve:
 *   Modify _LoadToTorque() to compare linear, dead-zone, or smooth nonlinear
 *   pressure-to-torque mappings. Preserve the final torque clamp.
 *
 * Example C - Tune contact detection:
 *   Adjust FSR_ON_THRESHOLD and FSR_OFF_THRESHOLD while observing
 *   pf3_contact .. pf6_contact. Keep ON_THRESHOLD above OFF_THRESHOLD.
 *
 * Example D - Select data for logging:
 *   Update RampStreamData_t, the XM_SetUsbCustomMeta() channel list, and
 *   _UpdateStreamData() together. Keep the same field order in all three.
 *
 * ============================================================================
 * 7. Areas Students Should Not Modify
 * ============================================================================
 * Do not remove torque clearing in Active_Exit(), the ASSIST-mode exit check,
 * or the final clamp in _LoadToTorque(). Do not modify low-level ADC, USB, or
 * XM_SetAssistTorqueRH/LH() APIs without instructor review.
 *
 * @see     docs/api-reference/XM_Control.md
 * @version 1.0
 * @date    Mar 09, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 *
 * ============================================================================
 * 8. Quick Code Map (Approximate Line Numbers)
 * ============================================================================
 * Line numbers may move slightly after edits. Search the function name first.
 *   approx. 203-230 : public FSR values, torque values, and control_ON
 *   approx. 414-459 : Active_Loop() safety checks and torque command routing
 *   approx. 534-559 : _SampleAdcChannels() raw ADC sampling and low-pass filter
 *   approx. 560-652 : _UpdateCalibration() BTN1/BTN2 calibration sequence
 *   approx. 742-753 : _LoadToTorque() proportional mapping and final clamp
 ******************************************************************************
 */

#include "xm_api.h"

#include <stdbool.h>
#include <stdio.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// --- 제어 루프 타이밍 ---
#define CONTROL_DT          0.001f  // 제어 주기 (1ms = 1kHz)

// --- USB 디버그 출력 주기 ---
#define USB_DEBUG_PERIOD_MS 500     // USB CDC 디버그 메시지 출력 주기 (ms)

// --- FSR signal processing ---
#define FSR_CAL_DURATION_MS 3000U
#define FSR_LPF_CUTOFF_HZ   20.0f
#define FSR_MIN_SPAN_V      0.050f
#define FSR_ON_THRESHOLD    0.35f
#define FSR_OFF_THRESHOLD   0.20f
#define FSR_MAX_TORQUE_NM   2.5f
#define FSR_MIN_TORQUE_NM   0.0f

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB 스트리밍용 데이터 구조체
 * @details PF3~PF6 normalized load를 실시간 스트리밍합니다.
 */
typedef struct {
    float pf3_load;         // PF3 normalized load
    float pf4_load;         // PF4 normalized load
    float pf5_load;         // PF5 normalized load
    float pf6_load;         // PF6 normalized load
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
float pf3_load;
float pf4_load;
float pf5_load;
float pf6_load;
float pf3_torque_nm;
float pf4_torque_nm;
float pf5_torque_nm;
float pf6_torque_nm;
uint16_t pf3_contact;
uint16_t pf4_contact;
uint16_t pf5_contact;
uint16_t pf6_contact;
uint16_t control_ON;
uint16_t torque_input_pair;  // 0: PF3/PF4 -> RH/LH, 1: PF5/PF6 -> RH/LH

// --- Calibration 상태 ---
typedef enum {
    CAL_IDLE,           // 평상시 (bias 적용)
    CAL_OFF_RUNNING,    // 3초간 unloaded/off 샘플 누적 중
    CAL_ON_RUNNING,     // 3초간 loaded/on 샘플 누적 중
    CAL_DONE            // 1회 이상 완료
} CalState_t;

static CalState_t s_cal_state    = CAL_IDLE;
static uint32_t   s_cal_tick     = 0;
static uint32_t   s_cal_count    = 0;
static double     s_cal_sum[ADC_CH_COUNT] = {0};   // 누적 (double로 정밀도 확보)
static float      s_cal_off_v[ADC_CH_COUNT] = {0}; // unloaded/off voltage (V)
static float      s_cal_on_v[ADC_CH_COUNT] = {1.0f, 1.0f, 1.0f, 1.0f}; // loaded/on voltage (V)
static float      s_lpf_v[ADC_CH_COUNT] = {0};
static float      s_load[ADC_CH_COUNT] = {0};
static float      s_torque_nm[ADC_CH_COUNT] = {0};
static bool       s_contact[ADC_CH_COUNT] = {false};
static bool       s_filter_initialized = false;

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

static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(void);

// --- ADC / Calibration ---
static float _ReadAdcVoltage(XmAdcPin_t pin);
static void  _SampleAdcChannels(void);
static void  _UpdateCalibration(void);
static void  _StartOffCalibration(void);
static void  _StartOnCalibration(void);
static void  _ResetCalibrationDefaults(void);
static void  _ProcessFsrSignals(void);
static void  _UpdatePublicSignals(void);
static float _LowPassUpdate(float prev, float input, float cutoff_hz);
static float _ClampFloat(float x, float min_value, float max_value);
static float _LoadToTorque(float load);

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

    // [상태 3] ACTIVE: FSR 기반 토크 제어 실행
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    // USB 스트리밍 설정 (User Custom 모드)
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"PF3 Load\",\"unit\":\"-\"},"
        "{\"name\":\"PF4 Load\",\"unit\":\"-\"},"
        "{\"name\":\"PF5 Load\",\"unit\":\"-\"},"
        "{\"name\":\"PF6 Load\",\"unit\":\"-\"}]");

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
    // 호출하지 않으면 XM_GetButtonEvent() 가 항상 NONE → BTN1/2/3 캘리브레이션 동작 불가.
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
 * @details H10이 ASSIST 모드로 전환되면 FSR 제어를 시작합니다.
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[FSR] ASSIST mode detected -> ACTIVE\r\n");
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

    s_filter_initialized = false;

    // USB 디버그 타이머 초기화
    s_usb_debug_timer = XM_GetTick();

    // LED: ACTIVE 상태 표시 (빠른 깜빡임)
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);

    XM_SendUsbDebugMessage("[FSR] ACTIVE start, calibrated proportional torque enabled\r\n");
}

/**
 * @brief ACTIVE 루프 — FSR proportional torque 실행 (1ms 주기)
 */
static void Active_Loop(void)
{
    // H10이 STANDBY로 전환되면 제어 종료
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        XM_SendUsbDebugMessage("[FSR] ASSIST released -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    // 4채널 ADC 샘플링 (PF3/PF4/PF5/PF6, ADC3 16-bit)
    _SampleAdcChannels();

    // Calibration 트리거 / 진행 / 완료 처리
    _UpdateCalibration();

    if (s_cal_state == CAL_OFF_RUNNING || s_cal_state == CAL_ON_RUNNING) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        _UpdatePublicSignals();
        _UpdateUsbDebug();
        _UpdateStreamData();
        return;
    }

    _ProcessFsrSignals();
    _UpdatePublicSignals();

    // --- 3. USB 디버그 및 스트리밍 갱신 ---
    _UpdateUsbDebug();
    _UpdateStreamData();

    if (control_ON == 1)  {
        if (torque_input_pair == 1) {
            XM_SetAssistTorqueRH(s_torque_nm[CH_PF5]);
            XM_SetAssistTorqueLH(s_torque_nm[CH_PF6]);
        } else {
            XM_SetAssistTorqueRH(s_torque_nm[CH_PF3]);
            XM_SetAssistTorqueLH(s_torque_nm[CH_PF4]);
        }
    } else {
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

    // LED: STANDBY 표시 (심장박동)
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);

    XM_SendUsbDebugMessage("[FSR] control stopped, torque cleared\r\n");
}

// ==================== 유틸리티 함수 ====================

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
                 "FSR | pair:%u on:%u cal:%d load RH/LH:%.2f/%.2f tau RH/LH:%.2f/%.2f Nm\r\n",
                 (unsigned int)torque_input_pair,
                 (unsigned int)control_ON,
                 (int)s_cal_state,
                 (torque_input_pair == 1U) ? s_load[CH_PF5] : s_load[CH_PF3],
                 (torque_input_pair == 1U) ? s_load[CH_PF6] : s_load[CH_PF4],
                 (torque_input_pair == 1U) ? s_torque_nm[CH_PF5] : s_torque_nm[CH_PF3],
                 (torque_input_pair == 1U) ? s_torque_nm[CH_PF6] : s_torque_nm[CH_PF4]);
        XM_SendUsbDebugMessage(buf);
    }
}

/**
 * @brief USB 스트리밍 데이터 갱신
 */
static void _UpdateStreamData(void)
{
    s_stream_data.pf3_load = s_load[CH_PF3];
    s_stream_data.pf4_load = s_load[CH_PF4];
    s_stream_data.pf5_load = s_load[CH_PF5];
    s_stream_data.pf6_load = s_load[CH_PF6];
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

    // filtered 값은 processing 단계에서 갱신. calibration 중에는 raw를 누적.

    if (s_cal_state == CAL_OFF_RUNNING || s_cal_state == CAL_ON_RUNNING) {
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
    XmBtnEvent_t btn1 = XM_GetButtonEvent(XM_BTN_1);
    XmBtnEvent_t btn2 = XM_GetButtonEvent(XM_BTN_2);
    XmBtnEvent_t btn3 = XM_GetButtonEvent(XM_BTN_3);

    bool cal_busy = (s_cal_state == CAL_OFF_RUNNING || s_cal_state == CAL_ON_RUNNING);

    if (btn3 == XM_BTN_CLICK && !cal_busy) {
        _ResetCalibrationDefaults();
        return;
    }

    if (btn1 == XM_BTN_CLICK && !cal_busy) {
        _StartOffCalibration();
        return;
    }

    if (btn2 == XM_BTN_CLICK && !cal_busy) {
        _StartOnCalibration();
        return;
    }

    if (s_cal_state == CAL_OFF_RUNNING || s_cal_state == CAL_ON_RUNNING) {
        uint32_t elapsed = XM_GetTick() - s_cal_tick;
        if (elapsed >= FSR_CAL_DURATION_MS) {
            if (s_cal_count > 0) {
                for (int i = 0; i < ADC_CH_COUNT; i++) {
                    float avg = (float)(s_cal_sum[i] / (double)s_cal_count);
                    if (s_cal_state == CAL_OFF_RUNNING) {
                        s_cal_off_v[i] = avg;
                    } else {
                        s_cal_on_v[i] = avg;
                    }
                }
            }

            s_cal_state = CAL_DONE;
            s_filter_initialized = false;
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);  // ACTIVE LED 복귀

            char buf[180];
            snprintf(buf, sizeof(buf),
                     "[FSR CAL] done n=%lu | off %.3f/%.3f/%.3f/%.3f on %.3f/%.3f/%.3f/%.3f V\r\n",
                     (unsigned long)s_cal_count,
                     s_cal_off_v[CH_PF3], s_cal_off_v[CH_PF4],
                     s_cal_off_v[CH_PF5], s_cal_off_v[CH_PF6],
                     s_cal_on_v[CH_PF3], s_cal_on_v[CH_PF4],
                     s_cal_on_v[CH_PF5], s_cal_on_v[CH_PF6]);
            XM_SendUsbDebugMessage(buf);
        }
    }
}

/**
 * @brief Calibration 시작 — 누적 버퍼 리셋, LED 빠르게 깜빡
 */
static void _StartOffCalibration(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_cal_sum[i] = 0.0;
    }
    s_cal_count = 0;
    s_cal_tick  = XM_GetTick();
    s_cal_state = CAL_OFF_RUNNING;

    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 50);  // 50ms — 매우 빠른 깜빡임
    XM_SendUsbDebugMessage("[FSR CAL] off start, unload all FSRs for 3 seconds\r\n");
}

static void _StartOnCalibration(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_cal_sum[i] = 0.0;
    }
    s_cal_count = 0;
    s_cal_tick  = XM_GetTick();
    s_cal_state = CAL_ON_RUNNING;

    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 50);
    XM_SendUsbDebugMessage("[FSR CAL] on start, load target FSRs for 3 seconds\r\n");
}

static void _ResetCalibrationDefaults(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_cal_off_v[i] = 0.0f;
        s_cal_on_v[i] = 1.0f;
        s_lpf_v[i] = 0.0f;
        s_load[i] = 0.0f;
        s_torque_nm[i] = 0.0f;
        s_contact[i] = false;
    }

    s_cal_state = CAL_IDLE;
    s_filter_initialized = false;
    XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, 1000);
    XM_SendUsbDebugMessage("[FSR CAL] reset to defaults\r\n");
}

static void _ProcessFsrSignals(void)
{
    float raw[ADC_CH_COUNT] = { pf3_volt, pf4_volt, pf5_volt, pf6_volt };

    for (int i = 0; i < ADC_CH_COUNT; i++) {
        if (!s_filter_initialized) {
            s_lpf_v[i] = raw[i];
        }
        s_lpf_v[i] = _LowPassUpdate(s_lpf_v[i], raw[i], FSR_LPF_CUTOFF_HZ);

        float span = s_cal_on_v[i] - s_cal_off_v[i];
        if (span < FSR_MIN_SPAN_V) {
            span = FSR_MIN_SPAN_V;
        }

        float load = (s_lpf_v[i] - s_cal_off_v[i]) / span;
        s_load[i] = _ClampFloat(load, 0.0f, 1.5f);

        if (s_contact[i]) {
            if (s_load[i] <= FSR_OFF_THRESHOLD) {
                s_contact[i] = false;
            }
        } else {
            if (s_load[i] >= FSR_ON_THRESHOLD) {
                s_contact[i] = true;
            }
        }

        s_torque_nm[i] = _LoadToTorque(s_load[i]);
    }
    s_filter_initialized = true;

    pf3_volt_cal = s_lpf_v[CH_PF3] - s_cal_off_v[CH_PF3];
    pf4_volt_cal = s_lpf_v[CH_PF4] - s_cal_off_v[CH_PF4];
    pf5_volt_cal = s_lpf_v[CH_PF5] - s_cal_off_v[CH_PF5];
    pf6_volt_cal = s_lpf_v[CH_PF6] - s_cal_off_v[CH_PF6];
}

static void _UpdatePublicSignals(void)
{
    pf3_load = s_load[CH_PF3];
    pf4_load = s_load[CH_PF4];
    pf5_load = s_load[CH_PF5];
    pf6_load = s_load[CH_PF6];

    pf3_torque_nm = s_torque_nm[CH_PF3];
    pf4_torque_nm = s_torque_nm[CH_PF4];
    pf5_torque_nm = s_torque_nm[CH_PF5];
    pf6_torque_nm = s_torque_nm[CH_PF6];

    pf3_contact = s_contact[CH_PF3] ? 1U : 0U;
    pf4_contact = s_contact[CH_PF4] ? 1U : 0U;
    pf5_contact = s_contact[CH_PF5] ? 1U : 0U;
    pf6_contact = s_contact[CH_PF6] ? 1U : 0U;
}

static float _LowPassUpdate(float prev, float input, float cutoff_hz)
{
    const float two_pi = 6.28318530718f;
    float rc = 1.0f / (two_pi * cutoff_hz);
    float alpha = CONTROL_DT / (rc + CONTROL_DT);

    return prev + alpha * (input - prev);
}

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

static float _LoadToTorque(float load)
{
    float torque = _ClampFloat(load, 0.0f, 1.0f) * FSR_MAX_TORQUE_NM;
    return _ClampFloat(torque, FSR_MIN_TORQUE_NM, FSR_MAX_TORQUE_NM);
}
