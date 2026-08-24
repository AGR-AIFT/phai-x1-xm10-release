/**
 ******************************************************************************
 * @file    Final_Encoder_ex01.c
 * @brief   Bilateral encoder-angle-triggered assist-pulse backbone.
 * @details
 * ============================================================================
 * 1. Purpose
 * ============================================================================
 * This file is a student project backbone for designing encoder-angle-based
 * assistance. It reads the left and right hip motor encoder angles, applies a
 * simple angle-trigger rule independently to each side, and sends an optional
 * short assist-torque pulse.
 *
 * It also sends selected experiment variables to a PC through USB CDC so that
 * students can monitor the signals live and save the received data as CSV for
 * offline analysis. Before an experiment, select only the variables needed for
 * the project in the "STUDENT CDC STREAM CONFIGURATION" block below.
 *
 * Students should modify only the high-level parameters and assist rule
 * described in section 6. Keep the safety gate, torque limit, and low-level
 * XM API calls intact unless an instructor directs otherwise.
 *
 * ============================================================================
 * 2. Encoder Signal Definition
 * ============================================================================
 * Raw encoder signals:
 *   XM.status.h10.leftHipMotorAngle
 *   XM.status.h10.rightHipMotorAngle
 *
 * The motor encoder angle may require an offset and sign correction depending
 * on the installed mechanism and sensor convention. This example calculates:
 *
 *   control_angle = encoder_sign * (raw_encoder_angle - encoder_offset)
 *
 * Validate the sign and offset on a bench setup before enabling torque output.
 *
 * ============================================================================
 * 3. Default Assist Rule
 * ============================================================================
 * The default high-level rule is intentionally simple:
 *
 *   if control_angle crosses assist_angle_threshold_deg while armed:
 *       command requested torque for assist_pulse_duration_ms
 *   when control_angle <= assist_rearm_angle_deg:
 *       arm the next pulse
 *
 * The left and right sides are evaluated independently. Holding an angle above
 * the threshold does not continuously apply torque and does not repeatedly
 * trigger pulses. This is a software-generated torque pulse using
 * XM_SetAssistTorqueLH/RH(), not the lower-level FVector_t command.
 *
 * ============================================================================
 * 4. Required Operating Procedure
 * ============================================================================
 * 1) Validate encoder signs, offsets, and torque directions on a bench setup.
 * 2) Switch the H10 device to ASSIST mode.
 * 3) Confirm assist_mode_active == 1 in STM32CubeIDE Live Expressions.
 * 4) Press BTN1 once to request torque control.
 * 5) Confirm control_ON == 1 and assist_enable == 1.
 * 6) Move each side and observe encoder angle, pulse state, and torque.
 * 7) Press BTN1 again to disable torque output.
 *
 * BTN3 disables torque output. BTN2 is reserved for a student extension.
 *
 * ============================================================================
 * 5. Important Live Expressions Variables
 * ============================================================================
 * Write from debugger:
 *   control_ON                    : 0 = disabled, 1 = request torque control
 *   encoder_offset_lh_deg         : left encoder neutral-position offset
 *   encoder_offset_rh_deg         : right encoder neutral-position offset
 *   encoder_sign_lh               : left angle sign; normally +1 or -1
 *   encoder_sign_rh               : right angle sign; normally +1 or -1
 *   assist_angle_threshold_deg    : pulse trigger threshold; default 20 deg
 *   assist_rearm_angle_deg        : next-pulse rearm threshold; default 15 deg
 *   assist_pulse_duration_ms      : torque pulse duration; default 120 ms
 *   left_requested_torque_nm      : requested left torque; start low
 *   right_requested_torque_nm     : requested right torque; start low
 *   assist_torque_limit_nm        : project torque limit; increase gradually
 *   cdc_stream_enable             : 0 = custom CDC off, 1 = custom CDC on
 *   cdc_stream_period_ms          : custom CDC period; default 10 ms = 100 Hz
 *
 * Observe only:
 *   assist_enable                 : 1 only while torque mode is actually active
 *   assist_mode_active            : 1 while H10 ASSIST mode is active
 *   btn1_state .. btn3_state      : physical button states
 *   btn1_last_event .. btn3_last_event
 *   left_encoder_angle_deg        : raw left hip motor encoder angle
 *   right_encoder_angle_deg       : raw right hip motor encoder angle
 *   left_control_angle_deg        : corrected left angle used by assist rule
 *   right_control_angle_deg       : corrected right angle used by assist rule
 *   left_threshold_active         : 1 while left angle meets the threshold
 *   right_threshold_active        : 1 while right angle meets the threshold
 *   left_pulse_active             : 1 while the left torque pulse is active
 *   right_pulse_active            : 1 while the right torque pulse is active
 *   left_trigger_armed            : 1 when a left pulse may be triggered
 *   right_trigger_armed           : 1 when a right pulse may be triggered
 *   left_assist_torque_nm         : torque currently commanded to left side
 *   right_assist_torque_nm        : torque currently commanded to right side
 *   left_motor_current_a          : measured left motor current from H10
 *   right_motor_current_a         : measured right motor current from H10
 *
 * USB CDC custom stream:
 *   Module ID 0xF0 sends 14 float channels at 100 Hz. The large system Total
 *   Data stream is disabled in this backbone to keep the CDC load low.
 *
 *   Default Module ID 0xF0 CSV channel order:
 *    1) L Encoder    = left_encoder_angle_deg
 *    2) R Encoder    = right_encoder_angle_deg
 *    3) L Angle      = left_control_angle_deg
 *    4) R Angle      = right_control_angle_deg
 *    5) L Pulse      = left_pulse_active
 *    6) R Pulse      = right_pulse_active
 *    7) L Torque     = left_assist_torque_nm
 *    8) R Torque     = right_assist_torque_nm
 *    9) Threshold    = assist_angle_threshold_deg
 *   10) Control Req  = control_ON
 *   11) Assist On    = assist_enable
 *   12) H10 Assist   = assist_mode_active
 *   13) L Current    = left_motor_current_a
 *   14) R Current    = right_motor_current_a
 *
 * CDC stream customization:
 *   1) Find the "STUDENT CDC STREAM CONFIGURATION" block below.
 *   2) Keep the first CDC_STREAM_FIRST() row and add, remove, or reorder only
 *      the CDC_STREAM_NEXT() rows to choose the variables sent to the PC.
 *   3) Each row defines: field_name, display_name, unit, source_expression.
 *   4) Use cdc_stream_enable and cdc_stream_period_ms in Live Expressions to
 *      turn the custom stream on/off or change its period without rebuilding.
 *   5) Keep the metadata JSON below 512 bytes. The build checks this limit.
 *   6) Run a PC-side CDC receiver/logger and save Module ID 0xF0 data as CSV.
 *
 * ============================================================================
 * 6. Recommended Student Design Tasks (High-Level Only)
 * ============================================================================
 * Example A - Tune the default threshold rule:
 *   Change assist_angle_threshold_deg, assist_rearm_angle_deg, and
 *   assist_pulse_duration_ms through Live Expressions while observing the
 *   corrected angles, pulse states, and commanded torques.
 *
 * Example B - Design a different high-level assist rule:
 *   Modify the trigger and rearm comparisons in _UpdateAssistTorque(). Use the
 *   CDC stream to compare the behavior before and after the change.
 *
 * Example C - Design asymmetric assistance:
 *   Tune left_requested_torque_nm and right_requested_torque_nm independently,
 *   or add independent left/right thresholds.
 *
 * Example D - Compare encoder and joint-level angles:
 *   Add XM.status.h10.leftHipAngle and XM.status.h10.rightHipAngle to the CDC
 *   configuration. Compare them with the raw motor encoder angles.
 *
 * ============================================================================
 * 7. Safety Gate
 * ============================================================================
 * Torque output is permitted only when all conditions below are true:
 *   control_ON == 1
 *   H10 mode == ASSIST
 * The H10 built-in assist algorithm remains disabled in this experiment so
 * that measured assistance comes only from this user algorithm.
 *
 * Torque amplitude has two limits:
 *   assist_torque_limit_nm      : student-adjustable project limit
 *   HARD_MAX_ASSIST_TORQUE_NM   : absolute instructor safety limit
 *
 * BTN1 toggles control_ON. BTN3 forces control_ON back to zero. The final
 * command cannot exceed either torque limit. Start with 0.5 Nm or less and
 * verify torque directions on a bench setup before any wearable test.
 *
 * ============================================================================
 * 8. Areas Students Should Not Modify
 * ============================================================================
 * Do not remove the safety gate at the beginning of _UpdateAssistTorque(), the
 * HARD_MAX_ASSIST_TORQUE_NM clamp, or the XM_SetControlMode() and
 * XM_SetAssistTorqueLH/RH() calls without instructor review.
 *
 * ============================================================================
 * 9. Quick Code Map (Approximate Line Numbers)
 * ============================================================================
 * Line numbers may move slightly after edits. Search the function name first.
 *   approx. 248-271 : student tuning parameters and Live Expressions controls
 *   approx. 345-357 : _SampleEncoder() raw and corrected encoder angles
 *   approx. 386-466 : _UpdateAssistTorque() safety gate and pulse rule
 *   approx. 414-452 : threshold trigger, pulse duration, and rearm condition
 *   approx. 486-499 : _SendStream() CDC data update and transmission
 ******************************************************************************
 */

#include "xm_api.h"

#include <stdbool.h>

#define USB_MODULE_ID             0xF0U
#define HARD_MAX_ASSIST_TORQUE_NM 2.5f

/*
 * ============================================================================
 * STUDENT CDC STREAM CONFIGURATION (100Hz)
 * ============================================================================
 * Keep the CDC_STREAM_FIRST() row. Add, remove, or reorder CDC_STREAM_NEXT()
 * rows to select the variables sent to the PC. The metadata and payload are
 * generated from this single list, so no other CDC code needs to be edited.
 *
 * Format:
 *   CDC_STREAM_NEXT(field_name, "Display Name", "unit", source_expression)
 */
#define CDC_STREAM_CHANNELS(CDC_STREAM_FIRST, CDC_STREAM_NEXT)                  \
    CDC_STREAM_FIRST(left_encoder,  "L Encoder",   "deg",  left_encoder_angle_deg) \
    CDC_STREAM_NEXT (right_encoder, "R Encoder",   "deg",  right_encoder_angle_deg) \
    CDC_STREAM_NEXT (left_angle,    "L Angle",     "deg",  left_control_angle_deg) \
    CDC_STREAM_NEXT (right_angle,   "R Angle",     "deg",  right_control_angle_deg) \
    CDC_STREAM_NEXT (left_active,   "L Pulse",     "bool", left_pulse_active) \
    CDC_STREAM_NEXT (right_active,  "R Pulse",     "bool", right_pulse_active) \
    CDC_STREAM_NEXT (left_torque,   "L Torque",    "Nm",   left_assist_torque_nm) \
    CDC_STREAM_NEXT (right_torque,  "R Torque",    "Nm",   right_assist_torque_nm) \
    CDC_STREAM_NEXT (threshold,     "Threshold",   "deg",  assist_angle_threshold_deg) \
    CDC_STREAM_NEXT (control_req,   "Control Req", "bool", control_ON)       \
    CDC_STREAM_NEXT (assist_on,     "Assist On",   "bool", assist_enable)    \
    CDC_STREAM_NEXT (h10_assist,    "H10 Assist",  "bool", assist_mode_active) \
    CDC_STREAM_NEXT (left_current,  "L Current",   "A",    left_motor_current_a) \
    CDC_STREAM_NEXT (right_current, "R Current",   "A",    right_motor_current_a)

#define CDC_DECLARE_FIELD(field, name, unit, source) float field;
typedef struct {
    CDC_STREAM_CHANNELS(CDC_DECLARE_FIELD, CDC_DECLARE_FIELD)
} EncoderStreamData_t;
#undef CDC_DECLARE_FIELD

#define CDC_META_FIRST(field, name, unit, source) "[{\"name\":\"" name "\",\"unit\":\"" unit "\"}"
#define CDC_META_NEXT(field, name, unit, source)  ",{\"name\":\"" name "\",\"unit\":\"" unit "\"}"
static const char s_cdc_stream_meta[] =
    CDC_STREAM_CHANNELS(CDC_META_FIRST, CDC_META_NEXT) "]";
#undef CDC_META_FIRST
#undef CDC_META_NEXT

_Static_assert(sizeof(s_cdc_stream_meta) <= 513U,
               "CDC metadata exceeds the XM_SetUsbCustomMeta 512-byte limit");

static uint32_t s_stream_tick;
static uint32_t s_left_pulse_start_tick;
static uint32_t s_right_pulse_start_tick;
static bool s_torque_mode_active;
static bool s_assist_session_active;
static EncoderStreamData_t s_stream;

/*
 * STUDENT ASSIST-TORQUE AND CDC CONFIGURATION
 *
 * Adjust these values directly or through STM32CubeIDE Live Expressions.
 * Start requested torque at 0.5 Nm or less.
 */
float encoder_offset_lh_deg = 0.0f;
float encoder_offset_rh_deg = 0.0f;
float encoder_sign_lh = 1.0f;
float encoder_sign_rh = 1.0f;
float assist_angle_threshold_deg = 20.0f;
float assist_rearm_angle_deg = 15.0f;
uint16_t assist_pulse_duration_ms = 120U;
float left_requested_torque_nm = 1.0f;
float right_requested_torque_nm = 1.0f;
float assist_torque_limit_nm = 1.0f;
uint16_t cdc_stream_enable = 1U;
uint16_t cdc_stream_period_ms = 10U;

/*
 * Optional assist controls. BTN1 toggles control_ON. assist_enable reports
 * whether torque control is actually active after the safety gate is checked.
 */
uint16_t assist_enable = 0U;
uint16_t control_ON = 0U;

/* Public runtime signals for STM32CubeIDE Live Expressions. */
uint16_t assist_mode_active;
uint16_t btn1_state;
uint16_t btn2_state;
uint16_t btn3_state;
uint16_t btn1_last_event;
uint16_t btn2_last_event;
uint16_t btn3_last_event;
float left_encoder_angle_deg;
float right_encoder_angle_deg;
float left_control_angle_deg;
float right_control_angle_deg;
uint16_t left_threshold_active;
uint16_t right_threshold_active;
uint16_t left_pulse_active;
uint16_t right_pulse_active;
uint16_t left_trigger_armed = 1U;
uint16_t right_trigger_armed = 1U;
float left_assist_torque_nm;
float right_assist_torque_nm;
float left_motor_current_a;
float right_motor_current_a;

static void _SampleEncoder(void);
static void _UpdateButtons(void);
static void _UpdateAssistTorque(void);
static void _UpdatePublicSignals(void);
static void _SendStream(void);
static void _ResetAssistPulse(void);
static float _ClampFloat(float value, float min_value, float max_value);

void User_Setup(void)
{
    XM_SetControlMode(XM_CTRL_MONITOR);
    XM_SetH10AssistExistingMode(false);
    XM_SetUsbTotalDataStream(false);
    XM_SetUsbCustomMeta(USB_MODULE_ID, s_cdc_stream_meta);

    _SampleEncoder();
    s_stream_tick = XM_GetTick();
    XM_SendUsbDebugMessage("[ENCODER ASSIST] press BTN1 in H10 ASSIST mode\r\n");
}

void User_Loop(void)
{
    bool is_assist_mode = (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST);

    _SampleEncoder();

    if (!is_assist_mode) {
        if (s_assist_session_active) {
            control_ON = 0U;
            s_assist_session_active = false;
        }
        _UpdateAssistTorque();
        _UpdatePublicSignals();
        return;
    }

    if (!s_assist_session_active) {
        control_ON = 0U;
        XM_SetH10AssistExistingMode(false);
        s_assist_session_active = true;
        XM_SendUsbDebugMessage("[ENCODER ASSIST] H10 ASSIST mode ready; press BTN1\r\n");
    }

    _UpdateButtons();
    _UpdateAssistTorque();
    _UpdatePublicSignals();
    _SendStream();
}

static void _SampleEncoder(void)
{
    left_encoder_angle_deg = XM.status.h10.leftHipMotorAngle;
    right_encoder_angle_deg = XM.status.h10.rightHipMotorAngle;
    left_motor_current_a = XM.status.h10.leftHipTorque;
    right_motor_current_a = XM.status.h10.rightHipTorque;

    left_control_angle_deg =
        encoder_sign_lh * (left_encoder_angle_deg - encoder_offset_lh_deg);
    right_control_angle_deg =
        encoder_sign_rh * (right_encoder_angle_deg - encoder_offset_rh_deg);
}

static void _UpdateButtons(void)
{
    XmBtnEvent_t btn1 = XM_GetButtonEvent(XM_BTN_1);
    XmBtnEvent_t btn2 = XM_GetButtonEvent(XM_BTN_2);
    XmBtnEvent_t btn3 = XM_GetButtonEvent(XM_BTN_3);

    btn1_state = XM_GetButtonState(XM_BTN_1);
    btn2_state = XM_GetButtonState(XM_BTN_2);
    btn3_state = XM_GetButtonState(XM_BTN_3);
    btn1_last_event = btn1;
    btn2_last_event = btn2;
    btn3_last_event = btn3;

    if (btn3 == XM_BTN_PRESSED) {
        control_ON = 0U;
        XM_SendUsbDebugMessage("[ENCODER ASSIST] BTN3 output disabled\r\n");
        return;
    }

    if (btn1 == XM_BTN_PRESSED &&
        XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        control_ON = (control_ON == 0U) ? 1U : 0U;
        XM_SendUsbDebugMessage((control_ON == 1U)
            ? "[ENCODER ASSIST] BTN1 output enabled\r\n"
            : "[ENCODER ASSIST] BTN1 output disabled\r\n");
    }
}

static void _UpdateAssistTorque(void)
{
    float torque_limit_nm = _ClampFloat(assist_torque_limit_nm,
                                        0.0f, HARD_MAX_ASSIST_TORQUE_NM);
    bool assist_requested = (control_ON == 1U) &&
                            (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST);

    if (!assist_requested) {
        assist_enable = 0U;
        _ResetAssistPulse();
        XM_SetAssistTorqueLH(0.0f);
        XM_SetAssistTorqueRH(0.0f);
        if (s_torque_mode_active) {
            XM_SetControlMode(XM_CTRL_MONITOR);
            XM_SetH10AssistExistingMode(false);
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

    /*
     * STUDENT HIGH-LEVEL ASSIST RULE: modify this block for project work.
     * A pulse is triggered only once while an angle stays above the threshold.
     * The angle must return below the rearm angle before another trigger.
     */
    uint32_t now = XM_GetTick();
    left_threshold_active =
        (left_control_angle_deg >= assist_angle_threshold_deg) ? 1U : 0U;
    right_threshold_active =
        (right_control_angle_deg >= assist_angle_threshold_deg) ? 1U : 0U;

    if (left_trigger_armed == 0U &&
        left_control_angle_deg <= assist_rearm_angle_deg) {
        left_trigger_armed = 1U;
    }
    if (right_trigger_armed == 0U &&
        right_control_angle_deg <= assist_rearm_angle_deg) {
        right_trigger_armed = 1U;
    }

    if (left_trigger_armed == 1U && left_threshold_active == 1U) {
        s_left_pulse_start_tick = now;
        left_pulse_active = 1U;
        left_trigger_armed = 0U;
    }
    if (right_trigger_armed == 1U && right_threshold_active == 1U) {
        s_right_pulse_start_tick = now;
        right_pulse_active = 1U;
        right_trigger_armed = 0U;
    }

    if (left_pulse_active == 1U &&
        (now - s_left_pulse_start_tick) >= assist_pulse_duration_ms) {
        left_pulse_active = 0U;
    }
    if (right_pulse_active == 1U &&
        (now - s_right_pulse_start_tick) >= assist_pulse_duration_ms) {
        right_pulse_active = 0U;
    }

    left_assist_torque_nm =
        (left_pulse_active == 1U)
            ? _ClampFloat(left_requested_torque_nm,
                          -torque_limit_nm, torque_limit_nm)
            : 0.0f;
    right_assist_torque_nm =
        (right_pulse_active == 1U)
            ? _ClampFloat(right_requested_torque_nm,
                          -torque_limit_nm, torque_limit_nm)
            : 0.0f;

    XM_SetAssistTorqueLH(left_assist_torque_nm);
    XM_SetAssistTorqueRH(right_assist_torque_nm);
}

static void _ResetAssistPulse(void)
{
    left_threshold_active = 0U;
    right_threshold_active = 0U;
    left_pulse_active = 0U;
    right_pulse_active = 0U;
    left_trigger_armed = 1U;
    right_trigger_armed = 1U;
    left_assist_torque_nm = 0.0f;
    right_assist_torque_nm = 0.0f;
}

static void _UpdatePublicSignals(void)
{
    assist_mode_active =
        (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) ? 1U : 0U;
}

static void _SendStream(void)
{
    uint32_t now = XM_GetTick();
    if (cdc_stream_enable != 1U || cdc_stream_period_ms == 0U ||
        (now - s_stream_tick) < cdc_stream_period_ms) {
        return;
    }
    s_stream_tick = now;

#define CDC_ASSIGN_FIELD(field, name, unit, source) s_stream.field = (float)(source);
    CDC_STREAM_CHANNELS(CDC_ASSIGN_FIELD, CDC_ASSIGN_FIELD)
#undef CDC_ASSIGN_FIELD

    XM_SendUsbDataWithId(&s_stream, sizeof(s_stream), USB_MODULE_ID);
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
