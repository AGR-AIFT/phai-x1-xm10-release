/**
 ******************************************************************************
 * @file    Final_Compensation_AddOn.c
 * @brief   Encoder-based gravity and friction compensation add-on backbone.
 * @details
 * ============================================================================
 * 1. Purpose
 * ============================================================================
 * This file is a student project backbone for adding a small feedforward
 * compensation torque to another assistance algorithm. It uses only the H10
 * built-in left and right hip motor encoders. No external sensor is required
 * to test this add-on by itself.
 *
 * The final command is intentionally separated into two parts:
 *
 *   final_assist_torque = base_assist_torque + compensation_torque
 *
 * Students may later replace base_assist_torque with an EMG-, FSR-, encoder-,
 * or state-estimation-based command while reusing the compensation functions.
 * This file does not prescribe an assistance scenario or gait-event rule.
 *
 * It also sends selected experiment variables to a PC through USB CDC so that
 * students can monitor the signals live and save the received data as CSV for
 * offline analysis. Before an experiment, select only the variables needed for
 * the project in the "STUDENT CDC STREAM CONFIGURATION" block below.
 *
 * ============================================================================
 * 2. Built-In Encoder Signal Definition
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
 * The corrected angle is used as an input to the compensation model. Confirm
 * that the corrected sign and zero position match the actual robot before
 * enabling torque output.
 *
 * ============================================================================
 * 3. Default Compensation Rule
 * ============================================================================
 * The default add-on uses a simple feedforward model:
 *
 *   gravity_torque = gravity_mgl_nm * sin(control_angle)
 *   friction_torque =
 *       coulomb_friction_nm * sign(angular_velocity)
 *       + viscous_friction_nms * angular_velocity
 *   compensation_torque =
 *       compensation_scale * (gravity_torque + friction_torque)
 *   final_assist_torque =
 *       clamp(base_assist_torque + compensation_torque)
 *
 * `gravity_mgl_nm` represents M * g * L_eff in Nm. This example deliberately
 * starts with a small model value and compensation scale. The correct value
 * depends on the mechanism and must be identified experimentally.
 *
 * Friction compensation is optional and defaults to OFF because numerical
 * differentiation amplifies encoder noise. Enable it only after observing the
 * filtered angular velocity and validating the torque direction.
 *
 * ============================================================================
 * 4. Required Operating Procedure
 * ============================================================================
 * 1) Validate encoder signs, offsets, and torque directions on a bench setup.
 * 2) Switch the H10 device to ASSIST mode.
 * 3) Confirm assist_mode_active == 1 in STM32CubeIDE Live Expressions.
 * 4) Press BTN1 once to request torque control.
 * 5) Confirm control_ON == 1 and assist_enable == 1.
 * 6) Press BTN2 to toggle compensation_ON.
 * 7) Move each side slowly and observe encoder angle and compensation torque.
 * 8) Press BTN3 at any time to disable torque output.
 *
 * BTN1 toggles total torque output. BTN2 toggles the compensation add-on.
 * BTN3 forces total torque output OFF. friction_compensation_ON is adjusted
 * separately in Live Expressions so it cannot be enabled accidentally.
 *
 * ============================================================================
 * 5. Important Live Expressions Variables
 * ============================================================================
 * Write from debugger:
 *   control_ON                    : 0 = output disabled, 1 = torque requested
 *   compensation_ON               : 0 = add-on disabled, 1 = add-on enabled
 *   friction_compensation_ON      : 0 = gravity only, 1 = gravity + friction
 *   encoder_offset_lh_deg         : left encoder neutral-position offset
 *   encoder_offset_rh_deg         : right encoder neutral-position offset
 *   encoder_sign_lh               : left angle sign; normally +1 or -1
 *   encoder_sign_rh               : right angle sign; normally +1 or -1
 *   left_base_assist_torque_nm    : left torque from another algorithm
 *   right_base_assist_torque_nm   : right torque from another algorithm
 *   gravity_mgl_nm                : M * g * L_eff model value; start low
 *   compensation_scale           : add-on scale; increase gradually
 *   coulomb_friction_nm           : constant friction model magnitude
 *   viscous_friction_nms          : velocity-proportional friction coefficient
 *   velocity_lpf_cutoff_hz        : encoder-velocity low-pass cutoff
 *   velocity_deadzone_rads        : friction model deadzone
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
 *   left_control_angle_deg        : corrected left angle used by the model
 *   right_control_angle_deg       : corrected right angle used by the model
 *   left_angular_velocity_rads    : filtered left encoder angular velocity
 *   right_angular_velocity_rads   : filtered right encoder angular velocity
 *   left_gravity_torque_nm        : left gravity-model torque before scale
 *   right_gravity_torque_nm       : right gravity-model torque before scale
 *   left_friction_torque_nm       : left friction-model torque before scale
 *   right_friction_torque_nm      : right friction-model torque before scale
 *   left_compensation_torque_nm   : scaled left add-on torque
 *   right_compensation_torque_nm  : scaled right add-on torque
 *   left_assist_torque_nm         : final left torque command after clamp
 *   right_assist_torque_nm        : final right torque command after clamp
 *
 * USB CDC custom stream:
 *   Module ID 0xF0 sends 16 float channels at 100 Hz. The large system Total
 *   Data stream is disabled in this backbone to keep the CDC load low.
 *
 *   Default Module ID 0xF0 CSV channel order:
 *    1) L Angle       = left_control_angle_deg
 *    2) R Angle       = right_control_angle_deg
 *    3) L Vel         = left_angular_velocity_rads
 *    4) R Vel         = right_angular_velocity_rads
 *    5) L Base        = left_base_assist_torque_nm
 *    6) R Base        = right_base_assist_torque_nm
 *    7) L Gravity     = left_gravity_torque_nm
 *    8) R Gravity     = right_gravity_torque_nm
 *    9) L Fric        = left_friction_torque_nm
 *   10) R Fric        = right_friction_torque_nm
 *   11) L Add-on      = left_compensation_torque_nm
 *   12) R Add-on      = right_compensation_torque_nm
 *   13) L Torque      = left_assist_torque_nm
 *   14) R Torque      = right_assist_torque_nm
 *   15) Control       = control_ON
 *   16) Comp On       = compensation_ON
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
 * Example A - Identify a gravity-compensation scale:
 *   Keep friction_compensation_ON == 0. Increase compensation_scale gradually
 *   while observing control angle, compensation torque, and user comfort.
 *
 * Example B - Add compensation to another assist rule:
 *   Replace left_base_assist_torque_nm and right_base_assist_torque_nm with
 *   another algorithm's torque commands. Preserve the final clamp in
 *   _UpdateAssistTorque().
 *
 * Example C - Evaluate friction compensation:
 *   After validating gravity compensation, set friction_compensation_ON = 1.
 *   Tune velocity_lpf_cutoff_hz, velocity_deadzone_rads, and the friction
 *   coefficients while observing encoder velocity noise and commanded torque.
 *
 * Example D - Design an evaluation metric:
 *   Compare compensation_ON == 0 and compensation_ON == 1. Evaluate commanded
 *   torque smoothness, angular velocity, EMG effort, or perceived comfort.
 *
 * ============================================================================
 * 7. Safety Gate
 * ============================================================================
 * Torque output is permitted only when all conditions below are true:
 *   control_ON == 1
 *   H10 mode == ASSIST
 *
 * The H10 built-in assist algorithm remains disabled in this experiment so
 * that measured assistance comes only from this user algorithm.
 *
 * Torque amplitude has two limits:
 *   assist_torque_limit_nm      : student-adjustable project limit
 *   HARD_MAX_ASSIST_TORQUE_NM   : absolute instructor safety limit
 *
 * BTN3 forces control_ON back to zero. The final command cannot exceed either
 * torque limit. Start with low values and verify torque directions on a bench
 * setup before any wearable test.
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
 *   approx. 281-309 : student model parameters and ON/OFF controls
 *   approx. 389-438 : _SampleEncoder() angle and filtered velocity calculation
 *   approx. 468-503 : _UpdateCompensation() gravity and friction model
 *   approx. 504-553 : _UpdateAssistTorque() safety gate and torque composition
 *   approx. 532-542 : base assist torque + compensation add-on final clamp
 *   approx. 560-573 : _SendStream() CDC data update and transmission
 ******************************************************************************
 */

#include "xm_api.h"

#include <math.h>
#include <stdbool.h>

#define USB_MODULE_ID             0xF0U
#define CONTROL_DT_S              0.001f
#define DEG_TO_RAD                0.01745329252f
#define TWO_PI                    6.28318530718f
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
#define CDC_STREAM_CHANNELS(CDC_STREAM_FIRST, CDC_STREAM_NEXT)                    \
    CDC_STREAM_FIRST(left_angle,     "L Angle",    "deg",  left_control_angle_deg) \
    CDC_STREAM_NEXT (right_angle,    "R Angle",    "deg",  right_control_angle_deg) \
    CDC_STREAM_NEXT (left_velocity,  "L Vel",      "rad/s", left_angular_velocity_rads) \
    CDC_STREAM_NEXT (right_velocity, "R Vel",      "rad/s", right_angular_velocity_rads) \
    CDC_STREAM_NEXT (left_base,      "L Base",     "Nm",   left_base_assist_torque_nm) \
    CDC_STREAM_NEXT (right_base,     "R Base",     "Nm",   right_base_assist_torque_nm) \
    CDC_STREAM_NEXT (left_gravity,   "L Gravity",  "Nm",   left_gravity_torque_nm) \
    CDC_STREAM_NEXT (right_gravity,  "R Gravity",  "Nm",   right_gravity_torque_nm) \
    CDC_STREAM_NEXT (left_friction,  "L Fric",     "Nm",   left_friction_torque_nm) \
    CDC_STREAM_NEXT (right_friction, "R Fric",     "Nm",   right_friction_torque_nm) \
    CDC_STREAM_NEXT (left_addon,     "L Add-on",   "Nm",   left_compensation_torque_nm) \
    CDC_STREAM_NEXT (right_addon,    "R Add-on",   "Nm",   right_compensation_torque_nm) \
    CDC_STREAM_NEXT (left_torque,    "L Torque",   "Nm",   left_assist_torque_nm) \
    CDC_STREAM_NEXT (right_torque,   "R Torque",   "Nm",   right_assist_torque_nm) \
    CDC_STREAM_NEXT (control_req,    "Control",    "bool", control_ON)       \
    CDC_STREAM_NEXT (comp_on,        "Comp On",    "bool", compensation_ON)

#define CDC_DECLARE_FIELD(field, name, unit, source) float field;
typedef struct {
    CDC_STREAM_CHANNELS(CDC_DECLARE_FIELD, CDC_DECLARE_FIELD)
} CompensationStreamData_t;
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
static bool s_torque_mode_active;
static bool s_assist_session_active;
static bool s_previous_angle_valid;
static float s_previous_left_angle_rad;
static float s_previous_right_angle_rad;
static CompensationStreamData_t s_stream;

/*
 * STUDENT COMPENSATION AND CDC CONFIGURATION
 *
 * Adjust these values directly or through STM32CubeIDE Live Expressions.
 * Keep base torque at 0 Nm while validating the compensation add-on alone.
 */
float encoder_offset_lh_deg = 0.0f;
float encoder_offset_rh_deg = 0.0f;
float encoder_sign_lh = 1.0f;
float encoder_sign_rh = 1.0f;
float left_base_assist_torque_nm = 0.0f;
float right_base_assist_torque_nm = 0.0f;
float gravity_mgl_nm = 1.0f;
float compensation_scale = 0.20f;
float coulomb_friction_nm = 0.10f;
float viscous_friction_nms = 0.01f;
float velocity_lpf_cutoff_hz = 5.0f;
float velocity_deadzone_rads = 0.10f;
float assist_torque_limit_nm = 1.0f;
uint16_t cdc_stream_enable = 1U;
uint16_t cdc_stream_period_ms = 10U;

/*
 * Optional assist controls. BTN1 toggles control_ON. BTN2 toggles the add-on.
 * assist_enable reports whether torque mode is active after safety checks.
 */
uint16_t assist_enable = 0U;
uint16_t control_ON = 0U;
uint16_t compensation_ON = 0U;
uint16_t friction_compensation_ON = 0U;

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
float left_angular_velocity_rads;
float right_angular_velocity_rads;
float left_gravity_torque_nm;
float right_gravity_torque_nm;
float left_friction_torque_nm;
float right_friction_torque_nm;
float left_compensation_torque_nm;
float right_compensation_torque_nm;
float left_assist_torque_nm;
float right_assist_torque_nm;
float left_motor_current_a;
float right_motor_current_a;

static void _SampleEncoder(void);
static void _UpdateButtons(void);
static void _UpdateCompensation(void);
static void _UpdateAssistTorque(void);
static void _UpdatePublicSignals(void);
static void _ResetTorqueSignals(void);
static void _SendStream(void);
static float _ClampFloat(float value, float min_value, float max_value);
static float _SignFloat(float value);

void User_Setup(void)
{
    XM_SetControlMode(XM_CTRL_MONITOR);
    XM_SetH10AssistExistingMode(false);
    XM_SetUsbTotalDataStream(false);
    XM_SetUsbCustomMeta(USB_MODULE_ID, s_cdc_stream_meta);

    _SampleEncoder();
    s_stream_tick = XM_GetTick();
    XM_SendUsbDebugMessage("[COMP ADD-ON] press BTN1 in H10 ASSIST mode\r\n");
}

void User_Loop(void)
{
    bool is_assist_mode = (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST);

    _SampleEncoder();

    if (!is_assist_mode) {
        if (s_assist_session_active) {
            control_ON = 0U;
            compensation_ON = 0U;
            s_assist_session_active = false;
        }
        _UpdateAssistTorque();
        _UpdatePublicSignals();
        return;
    }

    if (!s_assist_session_active) {
        control_ON = 0U;
        compensation_ON = 0U;
        XM_SetH10AssistExistingMode(false);
        s_assist_session_active = true;
        XM_SendUsbDebugMessage("[COMP ADD-ON] H10 ASSIST mode ready; press BTN1\r\n");
    }

    _UpdateButtons();
    _UpdateCompensation();
    _UpdateAssistTorque();
    _UpdatePublicSignals();
    _SendStream();
}

static void _SampleEncoder(void)
{
    float left_angle_rad;
    float right_angle_rad;
    float left_raw_velocity_rads = 0.0f;
    float right_raw_velocity_rads = 0.0f;
    float cutoff_hz = _ClampFloat(velocity_lpf_cutoff_hz, 0.0f, 100.0f);
    float alpha = (cutoff_hz > 0.0f)
                    ? (TWO_PI * cutoff_hz * CONTROL_DT_S) /
                      (1.0f + TWO_PI * cutoff_hz * CONTROL_DT_S)
                    : 0.0f;

    left_encoder_angle_deg = XM.status.h10.leftHipMotorAngle;
    right_encoder_angle_deg = XM.status.h10.rightHipMotorAngle;
    left_motor_current_a = XM.status.h10.leftHipTorque;
    right_motor_current_a = XM.status.h10.rightHipTorque;

    left_control_angle_deg =
        encoder_sign_lh * (left_encoder_angle_deg - encoder_offset_lh_deg);
    right_control_angle_deg =
        encoder_sign_rh * (right_encoder_angle_deg - encoder_offset_rh_deg);

    left_angle_rad = left_control_angle_deg * DEG_TO_RAD;
    right_angle_rad = right_control_angle_deg * DEG_TO_RAD;

    if (s_previous_angle_valid) {
        left_raw_velocity_rads =
            (left_angle_rad - s_previous_left_angle_rad) / CONTROL_DT_S;
        right_raw_velocity_rads =
            (right_angle_rad - s_previous_right_angle_rad) / CONTROL_DT_S;
    } else {
        s_previous_angle_valid = true;
    }

    s_previous_left_angle_rad = left_angle_rad;
    s_previous_right_angle_rad = right_angle_rad;
    left_angular_velocity_rads +=
        alpha * (left_raw_velocity_rads - left_angular_velocity_rads);
    right_angular_velocity_rads +=
        alpha * (right_raw_velocity_rads - right_angular_velocity_rads);
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
        compensation_ON = 0U;
        XM_SendUsbDebugMessage("[COMP ADD-ON] BTN3 output disabled\r\n");
        return;
    }

    if (btn1 == XM_BTN_PRESSED &&
        XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        control_ON = (control_ON == 0U) ? 1U : 0U;
        XM_SendUsbDebugMessage((control_ON == 1U)
            ? "[COMP ADD-ON] BTN1 output enabled\r\n"
            : "[COMP ADD-ON] BTN1 output disabled\r\n");
    }

    if (btn2 == XM_BTN_PRESSED &&
        XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        compensation_ON = (compensation_ON == 0U) ? 1U : 0U;
        XM_SendUsbDebugMessage((compensation_ON == 1U)
            ? "[COMP ADD-ON] BTN2 compensation enabled\r\n"
            : "[COMP ADD-ON] BTN2 compensation disabled\r\n");
    }
}

static void _UpdateCompensation(void)
{
    float left_angle_rad = left_control_angle_deg * DEG_TO_RAD;
    float right_angle_rad = right_control_angle_deg * DEG_TO_RAD;
    float deadzone_rads = _ClampFloat(velocity_deadzone_rads, 0.0f, 10.0f);
    float scale = _ClampFloat(compensation_scale, 0.0f, 1.0f);

    left_gravity_torque_nm = gravity_mgl_nm * sinf(left_angle_rad);
    right_gravity_torque_nm = gravity_mgl_nm * sinf(right_angle_rad);
    left_friction_torque_nm = 0.0f;
    right_friction_torque_nm = 0.0f;

    if (friction_compensation_ON == 1U) {
        if (fabsf(left_angular_velocity_rads) > deadzone_rads) {
            left_friction_torque_nm =
                coulomb_friction_nm * _SignFloat(left_angular_velocity_rads) +
                viscous_friction_nms * left_angular_velocity_rads;
        }
        if (fabsf(right_angular_velocity_rads) > deadzone_rads) {
            right_friction_torque_nm =
                coulomb_friction_nm * _SignFloat(right_angular_velocity_rads) +
                viscous_friction_nms * right_angular_velocity_rads;
        }
    }

    if (compensation_ON == 1U) {
        left_compensation_torque_nm =
            scale * (left_gravity_torque_nm + left_friction_torque_nm);
        right_compensation_torque_nm =
            scale * (right_gravity_torque_nm + right_friction_torque_nm);
    } else {
        left_compensation_torque_nm = 0.0f;
        right_compensation_torque_nm = 0.0f;
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
        _ResetTorqueSignals();
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
     * STUDENT TORQUE-COMPOSITION RULE: modify the base command source for
     * project work. Preserve the final clamp and low-level XM API calls.
     */
    left_assist_torque_nm =
        _ClampFloat(left_base_assist_torque_nm + left_compensation_torque_nm,
                    -torque_limit_nm, torque_limit_nm);
    right_assist_torque_nm =
        _ClampFloat(right_base_assist_torque_nm + right_compensation_torque_nm,
                    -torque_limit_nm, torque_limit_nm);

    XM_SetAssistTorqueLH(left_assist_torque_nm);
    XM_SetAssistTorqueRH(right_assist_torque_nm);
}

static void _UpdatePublicSignals(void)
{
    assist_mode_active =
        (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) ? 1U : 0U;
}

static void _ResetTorqueSignals(void)
{
    left_compensation_torque_nm = 0.0f;
    right_compensation_torque_nm = 0.0f;
    left_assist_torque_nm = 0.0f;
    right_assist_torque_nm = 0.0f;
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
