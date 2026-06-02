/**
 ******************************************************************************
 * @file    Final_FSR_Fuzzy_Logic.c
 * @brief   Bilateral 2-FSR fuzzy gait phase monitor and assist-control backbone.
 * @details
 * ============================================================================
 * 1. Purpose
 * ============================================================================
 * This file is a project backbone for designing gait-event-based assistance.
 * It measures two FSRs per foot, identifies each foot's gait phase with fuzzy
 * logic, and applies an optional torque pulse according to a high-level rule.
 *
 * Students should modify only the high-level parameters and assist rules
 * described in section 6. Keep the calibration sequence, safety gate, torque
 * limit, and low-level XM API calls intact unless an instructor directs
 * otherwise.
 *
 * ============================================================================
 * 2. Sensor Mapping
 * ============================================================================
 *   PF3 = left toe, PF4 = left heel, PF5 = right toe, PF6 = right heel.
 *
 * Each foot is classified independently with tanh membership functions,
 * product T-norm rules, normalization, and max-membership defuzzification.
 * The reference Python monitor uses four sensors per foot for six phases.
 * This reduced detector uses the available heel/toe pair for four phases:
 *   HEEL_STRIKE -> LOAD_RESPONSE -> TERMINAL_STANCE -> SWING (toe off).
 *
 * Live Expressions phase values:
 *   0 = HEEL_STRIKE
 *   1 = LOAD_RESPONSE
 *   2 = TERMINAL_STANCE
 *   3 = SWING (toe off)
 *
 * ============================================================================
 * 3. Required Operating Procedure
 * ============================================================================
 * 1) Put on the shoes and switch the H10 device to ASSIST mode.
 * 2) Keep the feet unloaded, press BTN1, and remain still for 1 second.
 * 3) Apply a representative full load, press BTN2, and hold it for 1 second.
 * 4) Confirm calibration_ready == 1 in STM32CubeIDE Live Expressions.
 * 5) Validate torque direction and amplitude on a bench setup.
 * 6) Set control_ON = 1 in Live Expressions to request torque control.
 * 7) Confirm assist_enable == 1. If it remains 0, a safety condition is not met.
 *
 * BTN3 resets calibration. Repeat BTN1 and BTN2 calibration after a reset or
 * after leaving and re-entering H10 ASSIST mode.
 *
 * ============================================================================
 * 4. Important Live Expressions Variables
 * ============================================================================
 * Write from debugger:
 *   control_ON                 : 0 = torque disabled, 1 = request torque control
 *   fuzzy_heel_threshold       : normalized heel-load decision boundary
 *   fuzzy_toe_threshold        : normalized toe-load decision boundary
 *   fuzzy_sensitivity          : tanh slope; larger values make transitions sharp
 *   left_push_off_torque_nm    : left assist pulse amplitude in Nm
 *   right_push_off_torque_nm   : right assist pulse amplitude in Nm
 *   push_off_pulse_ms          : pulse duration in milliseconds
 *
 * Observe only:
 *   assist_enable              : 1 only while torque mode is actually active
 *   calibration_zero_done      : BTN1 calibration completion flag
 *   calibration_full_load_done : BTN2 calibration completion flag
 *   calibration_ready          : 1 only after both calibration steps complete
 *   assist_mode_active         : 1 while H10 ASSIST mode is active
 *   pf3_volt .. pf6_volt       : raw sensor voltages
 *   fsr_lt_load .. fsr_rh_load : calibrated and normalized sensor loads
 *   L_gait_event, R_gait_event : current left and right gait phases (0 .. 3)
 *   left_assist_torque_nm      : torque currently commanded to the left side
 *   right_assist_torque_nm     : torque currently commanded to the right side
 *
 * ============================================================================
 * 5. Safety Gate
 * ============================================================================
 * Torque output is permitted only when all conditions below are true:
 *   control_ON == 1
 *   calibration_zero_done == 1
 *   calibration_full_load_done == 1
 *   H10 mode == ASSIST
 *
 * The final command is also limited by MAX_ASSIST_TORQUE_NM. Do not remove
 * these checks for a student project. Start with a low amplitude and verify
 * torque direction before any wearable test.
 *
 * ============================================================================
 * 6. Recommended Student Design Tasks (High-Level Only)
 * ============================================================================
 * Example A - Change when assistance starts:
 *   In User_Loop(), modify the phase-transition condition that reloads
 *   s_left_push_off_remaining_ms or s_right_push_off_remaining_ms.
 *   Default: apply assistance when TERMINAL_STANCE changes to SWING.
 *   Possible project: compare push-off assistance at TERMINAL_STANCE entry
 *   against assistance at the TERMINAL_STANCE -> SWING transition.
 *
 * Example B - Change the assist torque profile:
 *   In _UpdateAssistTorque(), replace the constant torque during the remaining
 *   pulse time with a high-level profile such as a ramp, triangle, or smooth
 *   half-sine curve. Preserve the safety gate and MAX_ASSIST_TORQUE_NM clamp.
 *
 * Example C - Tune gait detection:
 *   Adjust fuzzy_heel_threshold, fuzzy_toe_threshold, and fuzzy_sensitivity
 *   through Live Expressions while observing L_gait_event and R_gait_event.
 *   Lower threshold values detect contact earlier. Higher sensitivity values
 *   make the phase decision sharper but can increase phase chatter.
 *
 * Example D - Design asymmetric assistance:
 *   Tune left_push_off_torque_nm and right_push_off_torque_nm independently,
 *   or apply different high-level conditions for the left and right pulse
 *   reload rules in User_Loop(). Preserve independent left/right gait events.
 *
 * ============================================================================
 * 7. Areas Students Should Not Modify
 * ============================================================================
 * Do not redesign _UpdateCalibration(), _ResetCalibration(), the safety gate
 * at the beginning of _UpdateAssistTorque(), or the XM_SetControlMode() and
 * XM_SetAssistTorqueLH/RH() calls without instructor review.
 ******************************************************************************
 */

#include "xm_api.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define FSR_COUNT                 4
#define CONTROL_DT_S              0.001f
#define LPF_CUTOFF_HZ             8.0f
#define MINIMUM_SPAN_V            0.05f
#define CALIBRATION_DURATION_MS   1000U
#define STREAM_PERIOD_MS          2U
#define DEBUG_PERIOD_MS           500U
#define USB_MODULE_ID             0xF0U
#define FUZZY_PHASE_COUNT         4
#define MAX_ASSIST_TORQUE_NM      2.5f

typedef enum {
    FSR_LT = 0,
    FSR_LH,
    FSR_RT,
    FSR_RH
} FsrIndex_t;

typedef enum {
    CAL_IDLE = 0,
    CAL_OFF_RUNNING,
    CAL_ON_RUNNING,
    CAL_DONE
} CalibrationState_t;

typedef enum {
    GAIT_PHASE_HEEL_STRIKE = 0,
    GAIT_PHASE_LOAD_RESPONSE,
    GAIT_PHASE_TERMINAL_STANCE,
    GAIT_PHASE_SWING
} FootGaitPhase_t;

typedef enum {
    GAIT_EVENT_NONE          = 0U,
    GAIT_EVENT_HEEL_STRIKE   = 1U << 0,
    GAIT_EVENT_LOAD_RESPONSE = 1U << 1,
    GAIT_EVENT_TERMINAL_STANCE = 1U << 2,
    GAIT_EVENT_TOE_OFF       = 1U << 3
} FootGaitEventFlags_t;

typedef struct {
    float mu[FUZZY_PHASE_COUNT];
    FootGaitPhase_t phase;
    FootGaitPhase_t prev_phase;
    bool initialized;
} FootFuzzyDetector_t;

typedef struct {
    float lt_load;
    float lh_load;
    float rt_load;
    float rh_load;
    float left_phase;
    float right_phase;
    float left_events;
    float right_events;
    float left_mu_hs;
    float left_mu_lr;
    float left_mu_ts;
    float left_mu_swing;
    float right_mu_hs;
    float right_mu_lr;
    float right_mu_ts;
    float right_mu_swing;
} GaitStreamData_t;

static const XmAdcPin_t s_adc_pins[FSR_COUNT] = {
    XM_EXT_ADC_5, XM_EXT_ADC_6, XM_EXT_ADC_7, XM_EXT_ADC_8
};

static float s_raw_v[FSR_COUNT];
static float s_lpf_v[FSR_COUNT];
static float s_off_v[FSR_COUNT];
static float s_on_v[FSR_COUNT] = {1.0f, 1.0f, 1.0f, 1.0f};
static float s_load[FSR_COUNT];
static bool s_filter_initialized;

static CalibrationState_t s_cal_state;
static double s_cal_sum[FSR_COUNT];
static uint32_t s_cal_count;
static uint32_t s_cal_start_tick;
static bool s_zero_captured;
static bool s_full_load_captured;

static FootFuzzyDetector_t s_left;
static FootFuzzyDetector_t s_right;
static uint16_t s_stream_left_events;
static uint16_t s_stream_right_events;
static uint32_t s_stream_tick;
static uint32_t s_debug_tick;
static uint32_t s_left_push_off_remaining_ms;
static uint32_t s_right_push_off_remaining_ms;
static bool s_torque_mode_active;
static bool s_assist_session_active;
static GaitStreamData_t s_stream;

/*
 * Public tuning parameters for STM32CubeIDE Live Expressions.
 * Loads are normalized to approximately 0..1 before tanh evaluation.
 */
float fuzzy_heel_threshold = 0.35f;
float fuzzy_toe_threshold = 0.35f;
float fuzzy_sensitivity = 12.0f;

/*
 * Optional assist controls. Set control_ON to 1 only after torque direction and
 * amplitude have been validated on a bench setup. assist_enable reports whether
 * torque control is actually active after all safety conditions are satisfied.
 */
uint16_t assist_enable = 0U;
uint16_t control_ON = 0U;
uint16_t push_off_pulse_ms = 100U;
float left_push_off_torque_nm = 1.0f;
float right_push_off_torque_nm = 1.0f;

/* Public runtime signals for STM32CubeIDE Live Expressions. */
float pf3_volt;
float pf4_volt;
float pf5_volt;
float pf6_volt;
float fsr_lt_load;
float fsr_lh_load;
float fsr_rt_load;
float fsr_rh_load;
uint16_t calibration_zero_done;
uint16_t calibration_full_load_done;
uint16_t calibration_ready;
uint16_t assist_mode_active;
uint16_t btn1_state;
uint16_t btn2_state;
uint16_t btn3_state;
uint16_t btn1_last_event;
uint16_t btn2_last_event;
uint16_t btn3_last_event;
uint16_t L_gait_event;
uint16_t R_gait_event;
uint16_t left_gait_phase;
uint16_t right_gait_phase;
uint16_t left_gait_event_flags;
uint16_t right_gait_event_flags;
float left_assist_torque_nm;
float right_assist_torque_nm;

static void _SampleFsr(void);
static void _UpdateCalibration(void);
static void _StartCalibration(CalibrationState_t state);
static void _ResetCalibration(void);
static void _ResetDetectors(void);
static void _UpdateLoads(void);
static uint16_t _UpdateFootDetector(FootFuzzyDetector_t *detector,
                                    float heel_load, float toe_load);
static void _UpdateAssistTorque(void);
static void _UpdatePublicSignals(void);
static void _SendStream(void);
static void _SendDebug(void);
static float _ClampFloat(float value, float min_value, float max_value);
static float _MembershipLarge(float value, float threshold, float sensitivity);

void User_Setup(void)
{
    XM_SetExtPowerVoltage(XM_EXT_PWR_5V);
    XM_SwitchDioToAdc(XM_EXT_DIO_1);
    XM_SwitchDioToAdc(XM_EXT_DIO_2);
    XM_SwitchDioToAdc(XM_EXT_DIO_3);
    XM_SwitchDioToAdc(XM_EXT_DIO_4);

    XM_SetControlMode(XM_CTRL_MONITOR);
    XM_SetH10AssistExistingMode(true);

    XM_SetUsbCustomMeta(USB_MODULE_ID,
        "[{\"name\":\"LT Load\",\"unit\":\"-\"},"
        "{\"name\":\"LH Load\",\"unit\":\"-\"},"
        "{\"name\":\"RT Load\",\"unit\":\"-\"},"
        "{\"name\":\"RH Load\",\"unit\":\"-\"},"
        "{\"name\":\"L Phase\",\"unit\":\"id\"},"
        "{\"name\":\"R Phase\",\"unit\":\"id\"},"
        "{\"name\":\"L Events\",\"unit\":\"bits\"},"
        "{\"name\":\"R Events\",\"unit\":\"bits\"},"
        "{\"name\":\"L mu HS\",\"unit\":\"-\"},"
        "{\"name\":\"L mu LR\",\"unit\":\"-\"},"
        "{\"name\":\"L mu TS\",\"unit\":\"-\"},"
        "{\"name\":\"L mu SW\",\"unit\":\"-\"},"
        "{\"name\":\"R mu HS\",\"unit\":\"-\"},"
        "{\"name\":\"R mu LR\",\"unit\":\"-\"},"
        "{\"name\":\"R mu TS\",\"unit\":\"-\"},"
        "{\"name\":\"R mu SW\",\"unit\":\"-\"}]");

    _ResetCalibration();
    s_stream_tick = XM_GetTick();
    s_debug_tick = XM_GetTick();
    XM_SendUsbDebugMessage("[FUZZY GAIT] bilateral heel/toe monitor ready\r\n");
}

void User_Loop(void)
{
    bool is_assist_mode = (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST);
    if (!is_assist_mode) {
        if (s_assist_session_active) {
            _ResetCalibration();
            s_assist_session_active = false;
        }
        _UpdateAssistTorque();
        _UpdatePublicSignals();
        return;
    }

    if (!s_assist_session_active) {
        _ResetCalibration();
        s_assist_session_active = true;
    }

    _SampleFsr();
    _UpdateCalibration();

    if (s_cal_state != CAL_OFF_RUNNING && s_cal_state != CAL_ON_RUNNING) {
        _UpdateLoads();
        left_gait_event_flags = _UpdateFootDetector(&s_left,
                                                    s_load[FSR_LH],
                                                    s_load[FSR_LT]);
        right_gait_event_flags = _UpdateFootDetector(&s_right,
                                                     s_load[FSR_RH],
                                                     s_load[FSR_RT]);
        if (s_left.prev_phase == GAIT_PHASE_TERMINAL_STANCE &&
            s_left.phase == GAIT_PHASE_SWING) {
            s_left_push_off_remaining_ms = push_off_pulse_ms;
        }
        if (s_right.prev_phase == GAIT_PHASE_TERMINAL_STANCE &&
            s_right.phase == GAIT_PHASE_SWING) {
            s_right_push_off_remaining_ms = push_off_pulse_ms;
        }
        s_stream_left_events |= left_gait_event_flags;
        s_stream_right_events |= right_gait_event_flags;
    } else {
        memset(s_load, 0, sizeof(s_load));
        _ResetDetectors();
    }

    _UpdateAssistTorque();
    _UpdatePublicSignals();
    _SendStream();
    _SendDebug();
}

static void _SampleFsr(void)
{
    const float rc = 1.0f / (6.28318530718f * LPF_CUTOFF_HZ);
    const float alpha = CONTROL_DT_S / (rc + CONTROL_DT_S);

    for (int i = 0; i < FSR_COUNT; i++) {
        s_raw_v[i] = (float)XM_AnalogReadMillivolts(s_adc_pins[i]) * 0.001f;
        if (!s_filter_initialized) {
            s_lpf_v[i] = s_raw_v[i];
        } else {
            s_lpf_v[i] += alpha * (s_raw_v[i] - s_lpf_v[i]);
        }
    }
    s_filter_initialized = true;
    pf3_volt = s_raw_v[FSR_LT];
    pf4_volt = s_raw_v[FSR_LH];
    pf5_volt = s_raw_v[FSR_RT];
    pf6_volt = s_raw_v[FSR_RH];

    if (s_cal_state == CAL_OFF_RUNNING || s_cal_state == CAL_ON_RUNNING) {
        for (int i = 0; i < FSR_COUNT; i++) {
            s_cal_sum[i] += s_lpf_v[i];
        }
        s_cal_count++;
    }
}

static void _UpdateCalibration(void)
{
    XmBtnEvent_t btn1 = XM_GetButtonEvent(XM_BTN_1);
    XmBtnEvent_t btn2 = XM_GetButtonEvent(XM_BTN_2);
    XmBtnEvent_t btn3 = XM_GetButtonEvent(XM_BTN_3);
    bool busy = (s_cal_state == CAL_OFF_RUNNING || s_cal_state == CAL_ON_RUNNING);

    btn1_state = XM_GetButtonState(XM_BTN_1);
    btn2_state = XM_GetButtonState(XM_BTN_2);
    btn3_state = XM_GetButtonState(XM_BTN_3);
    btn1_last_event = btn1;
    btn2_last_event = btn2;
    btn3_last_event = btn3;

    if (btn3 == XM_BTN_PRESSED && !busy) {
        _ResetCalibration();
        XM_SendUsbDebugMessage("[FUZZY CAL] reset to defaults\r\n");
        return;
    }
    if (btn1 == XM_BTN_PRESSED && !busy) {
        _StartCalibration(CAL_OFF_RUNNING);
        XM_SendUsbDebugMessage("[FUZZY CAL] shoe-on zero: stand unloaded for 1 second\r\n");
        return;
    }
    if (btn2 == XM_BTN_PRESSED && !busy && s_zero_captured) {
        _StartCalibration(CAL_ON_RUNNING);
        XM_SendUsbDebugMessage("[FUZZY CAL] normalization: apply full load for 1 second\r\n");
        return;
    }
    if (btn2 == XM_BTN_PRESSED && !busy) {
        XM_SendUsbDebugMessage("[FUZZY CAL] run BTN1 shoe-on zero before BTN2\r\n");
        return;
    }

    if (busy && (XM_GetTick() - s_cal_start_tick) >= CALIBRATION_DURATION_MS) {
        CalibrationState_t completed_state = s_cal_state;
        if (s_cal_count > 0U) {
            for (int i = 0; i < FSR_COUNT; i++) {
                float average = (float)(s_cal_sum[i] / (double)s_cal_count);
                if (completed_state == CAL_OFF_RUNNING) {
                    s_off_v[i] = average;
                } else {
                    s_on_v[i] = average;
                }
            }
            if (completed_state == CAL_OFF_RUNNING) {
                s_zero_captured = true;
            } else {
                s_full_load_captured = true;
            }
        }
        s_cal_state = (s_zero_captured && s_full_load_captured)
                        ? CAL_DONE
                        : CAL_IDLE;
        _ResetDetectors();
        XM_SetLedEffect(XM_LED_1, XM_LED_OFF, 0);
        XM_SetLedEffect(XM_LED_2, XM_LED_OFF, 0);
        XM_SetLedEffect(XM_LED_1, XM_LED_ONESHOT, 500);
        XM_SendUsbDebugMessage("[FUZZY CAL] capture complete\r\n");
    }
}

static void _StartCalibration(CalibrationState_t state)
{
    memset(s_cal_sum, 0, sizeof(s_cal_sum));
    s_cal_count = 0U;
    s_cal_start_tick = XM_GetTick();
    s_cal_state = state;
    if (state == CAL_OFF_RUNNING) {
        s_zero_captured = false;
        s_full_load_captured = false;
    } else {
        s_full_load_captured = false;
    }
    s_stream_left_events = GAIT_EVENT_NONE;
    s_stream_right_events = GAIT_EVENT_NONE;
    XM_SetLedEffect((state == CAL_OFF_RUNNING) ? XM_LED_1 : XM_LED_2,
                    XM_LED_BLINK, 100);
}

static void _ResetCalibration(void)
{
    memset(s_raw_v, 0, sizeof(s_raw_v));
    memset(s_lpf_v, 0, sizeof(s_lpf_v));
    memset(s_off_v, 0, sizeof(s_off_v));
    memset(s_load, 0, sizeof(s_load));
    for (int i = 0; i < FSR_COUNT; i++) {
        s_on_v[i] = 1.0f;
    }
    s_cal_state = CAL_IDLE;
    s_filter_initialized = false;
    s_zero_captured = false;
    s_full_load_captured = false;
    calibration_zero_done = 0U;
    calibration_full_load_done = 0U;
    calibration_ready = 0U;
    _ResetDetectors();
    XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, 500);
}

static void _ResetDetectors(void)
{
    memset(&s_left, 0, sizeof(s_left));
    memset(&s_right, 0, sizeof(s_right));
    s_left.phase = GAIT_PHASE_SWING;
    s_left.prev_phase = GAIT_PHASE_SWING;
    s_right.phase = GAIT_PHASE_SWING;
    s_right.prev_phase = GAIT_PHASE_SWING;
    left_gait_phase = GAIT_PHASE_SWING;
    right_gait_phase = GAIT_PHASE_SWING;
    left_gait_event_flags = GAIT_EVENT_NONE;
    right_gait_event_flags = GAIT_EVENT_NONE;
    s_stream_left_events = GAIT_EVENT_NONE;
    s_stream_right_events = GAIT_EVENT_NONE;
    s_left_push_off_remaining_ms = 0U;
    s_right_push_off_remaining_ms = 0U;
}

static void _UpdateLoads(void)
{
    for (int i = 0; i < FSR_COUNT; i++) {
        float span = s_on_v[i] - s_off_v[i];
        if (span < MINIMUM_SPAN_V) {
            span = MINIMUM_SPAN_V;
        }
        s_load[i] = _ClampFloat((s_lpf_v[i] - s_off_v[i]) / span, 0.0f, 1.5f);
    }
}

static uint16_t _UpdateFootDetector(FootFuzzyDetector_t *detector,
                                    float heel_load, float toe_load)
{
    float heel_large = _MembershipLarge(heel_load, fuzzy_heel_threshold,
                                        fuzzy_sensitivity);
    float toe_large = _MembershipLarge(toe_load, fuzzy_toe_threshold,
                                       fuzzy_sensitivity);
    float heel_small = 1.0f - heel_large;
    float toe_small = 1.0f - toe_large;

    /* Reduced form of the reference Larsen product implication rules. */
    detector->mu[GAIT_PHASE_HEEL_STRIKE] = heel_large * toe_small;
    detector->mu[GAIT_PHASE_LOAD_RESPONSE] = heel_large * toe_large;
    detector->mu[GAIT_PHASE_TERMINAL_STANCE] = heel_small * toe_large;
    detector->mu[GAIT_PHASE_SWING] = heel_small * toe_small;

    float sum = 0.0f;
    for (int i = 0; i < FUZZY_PHASE_COUNT; i++) {
        sum += detector->mu[i];
    }
    if (sum > 1.0e-9f) {
        for (int i = 0; i < FUZZY_PHASE_COUNT; i++) {
            detector->mu[i] /= sum;
        }
    }

    FootGaitPhase_t next_phase = GAIT_PHASE_HEEL_STRIKE;
    for (int i = 1; i < FUZZY_PHASE_COUNT; i++) {
        if (detector->mu[i] > detector->mu[next_phase]) {
            next_phase = (FootGaitPhase_t)i;
        }
    }

    uint16_t events = GAIT_EVENT_NONE;
    if (detector->initialized && next_phase != detector->phase) {
        switch (next_phase) {
        case GAIT_PHASE_HEEL_STRIKE:
            events |= GAIT_EVENT_HEEL_STRIKE;
            break;
        case GAIT_PHASE_LOAD_RESPONSE:
            events |= GAIT_EVENT_LOAD_RESPONSE;
            break;
        case GAIT_PHASE_TERMINAL_STANCE:
            events |= GAIT_EVENT_TERMINAL_STANCE;
            break;
        case GAIT_PHASE_SWING:
            events |= GAIT_EVENT_TOE_OFF;
            break;
        default:
            break;
        }
    }

    detector->prev_phase = detector->phase;
    detector->phase = next_phase;
    detector->initialized = true;
    return events;
}

static void _UpdateAssistTorque(void)
{
    bool assist_requested = (control_ON == 1U) &&
                            s_zero_captured &&
                            s_full_load_captured &&
                            (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST);

    if (!assist_requested) {
        assist_enable = 0U;
        left_assist_torque_nm = 0.0f;
        right_assist_torque_nm = 0.0f;
        s_left_push_off_remaining_ms = 0U;
        s_right_push_off_remaining_ms = 0U;
        XM_SetAssistTorqueLH(0.0f);
        XM_SetAssistTorqueRH(0.0f);
        if (s_torque_mode_active) {
            XM_SetControlMode(XM_CTRL_MONITOR);
            XM_SetH10AssistExistingMode(true);
            s_torque_mode_active = false;
        }
        return;
    }

    if (!s_torque_mode_active) {
        XM_SetH10AssistExistingMode(false);
        XM_SetControlMode(XM_CTRL_TORQUE);
        s_torque_mode_active = true;
    }
    assist_enable = 1U;

    left_assist_torque_nm =
        (s_left_push_off_remaining_ms > 0U)
            ? _ClampFloat(left_push_off_torque_nm,
                          -MAX_ASSIST_TORQUE_NM, MAX_ASSIST_TORQUE_NM)
            : 0.0f;
    right_assist_torque_nm =
        (s_right_push_off_remaining_ms > 0U)
            ? _ClampFloat(right_push_off_torque_nm,
                          -MAX_ASSIST_TORQUE_NM, MAX_ASSIST_TORQUE_NM)
            : 0.0f;

    XM_SetAssistTorqueLH(left_assist_torque_nm);
    XM_SetAssistTorqueRH(right_assist_torque_nm);

    if (s_left_push_off_remaining_ms > 0U) {
        s_left_push_off_remaining_ms--;
    }
    if (s_right_push_off_remaining_ms > 0U) {
        s_right_push_off_remaining_ms--;
    }
}

static void _UpdatePublicSignals(void)
{
    fsr_lt_load = s_load[FSR_LT];
    fsr_lh_load = s_load[FSR_LH];
    fsr_rt_load = s_load[FSR_RT];
    fsr_rh_load = s_load[FSR_RH];
    calibration_zero_done = s_zero_captured ? 1U : 0U;
    calibration_full_load_done = s_full_load_captured ? 1U : 0U;
    calibration_ready = (s_zero_captured && s_full_load_captured) ? 1U : 0U;
    assist_mode_active = (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) ? 1U : 0U;
    left_gait_phase = s_left.phase;
    right_gait_phase = s_right.phase;
    L_gait_event = left_gait_phase;
    R_gait_event = right_gait_phase;
}

static void _SendStream(void)
{
    uint32_t now = XM_GetTick();
    if ((now - s_stream_tick) < STREAM_PERIOD_MS) {
        return;
    }
    s_stream_tick = now;

    s_stream.lt_load = fsr_lt_load;
    s_stream.lh_load = fsr_lh_load;
    s_stream.rt_load = fsr_rt_load;
    s_stream.rh_load = fsr_rh_load;
    s_stream.left_phase = (float)left_gait_phase;
    s_stream.right_phase = (float)right_gait_phase;
    s_stream.left_events = (float)s_stream_left_events;
    s_stream.right_events = (float)s_stream_right_events;
    s_stream.left_mu_hs = s_left.mu[GAIT_PHASE_HEEL_STRIKE];
    s_stream.left_mu_lr = s_left.mu[GAIT_PHASE_LOAD_RESPONSE];
    s_stream.left_mu_ts = s_left.mu[GAIT_PHASE_TERMINAL_STANCE];
    s_stream.left_mu_swing = s_left.mu[GAIT_PHASE_SWING];
    s_stream.right_mu_hs = s_right.mu[GAIT_PHASE_HEEL_STRIKE];
    s_stream.right_mu_lr = s_right.mu[GAIT_PHASE_LOAD_RESPONSE];
    s_stream.right_mu_ts = s_right.mu[GAIT_PHASE_TERMINAL_STANCE];
    s_stream.right_mu_swing = s_right.mu[GAIT_PHASE_SWING];
    XM_SendUsbDataWithId(&s_stream, sizeof(s_stream), USB_MODULE_ID);
    s_stream_left_events = GAIT_EVENT_NONE;
    s_stream_right_events = GAIT_EVENT_NONE;
}

static void _SendDebug(void)
{
    uint32_t now = XM_GetTick();
    if ((now - s_debug_tick) < DEBUG_PERIOD_MS) {
        return;
    }
    s_debug_tick = now;

    char buf[160];
    snprintf(buf, sizeof(buf),
             "FUZZY | LT/LH %.2f/%.2f RT/RH %.2f/%.2f | L ph:%u ev:%02X R ph:%u ev:%02X\r\n",
             fsr_lt_load, fsr_lh_load, fsr_rt_load, fsr_rh_load,
             (unsigned int)left_gait_phase, (unsigned int)left_gait_event_flags,
             (unsigned int)right_gait_phase, (unsigned int)right_gait_event_flags);
    XM_SendUsbDebugMessage(buf);
}

static float _ClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float _MembershipLarge(float value, float threshold, float sensitivity)
{
    return 0.5f * (tanhf(sensitivity * (value - threshold)) + 1.0f);
}
