/**
 ******************************************************************************
 * @file    stair_ascent_hip_flexion_assist.c
 * @brief   [Example] Hip-flexion angle triggered stair-ascent torque assist
 * @details
 * This example combines the TSM and safe torque shutdown pattern from
 * examples/14_PD_Realtime_Control with the independent left/right leg FSM
 * pattern from examples/17_FSM_Gait_Intent.
 *
 * The controller does not force a position trajectory. When the user starts
 * hip flexion and passes the trigger angle, it applies a smooth flexion-assist
 * torque profile. Each leg is controlled independently.
 *
 * BTN1 toggles algorithm output while H10 is in ASSIST mode.
 *
 * IMPORTANT:
 * - Verify the angle and torque signs on a bench before wearing the device.
 * - Start with a low torque limit.
 * - If USE_MOTOR_ENCODER_ANGLE is enabled, calibrate the motor-angle offset,
 *   scale, and sign for the installed transmission before use.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* Hip angle source:
 * 0: H10 joint-level hip angle (recommended initial setting)
 * 1: raw motor encoder angle (requires transmission calibration) */
#define USE_MOTOR_ENCODER_ANGLE          0

/* Adjust after checking the sign convention on the actual device. */
#define HIP_FLEXION_SIGN_RH              1.0f
#define HIP_FLEXION_SIGN_LH              1.0f
#define HIP_FLEXION_TORQUE_SIGN_RH       1.0f
#define HIP_FLEXION_TORQUE_SIGN_LH       1.0f

/* Trigger hysteresis and intent detection. */
#define FLEXION_TRIGGER_ANGLE_DEG        20.0f
#define FLEXION_RELEASE_ANGLE_DEG        12.0f
#define FLEXION_VELOCITY_TRIGGER_DPS     10.0f
#define VELOCITY_LPF_ALPHA               0.10f

/* Smooth torque profile. */
#define ASSIST_TORQUE_MAX_NM             1.0f
#define RAMP_UP_DURATION_MS              180U
#define HOLD_MAX_DURATION_MS             350U
#define RAMP_DOWN_DURATION_MS            220U

/* Set to 1 after validating foot-contact data if stricter intent gating is
 * desired. The opposite foot must be on the ground before assist starts. */
#define REQUIRE_OPPOSITE_FOOT_CONTACT    0

#define CONTROL_DT_S                     0.001f

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef enum {
    STAIR_WAIT_FOR_FLEXION = 0,
    STAIR_RAMP_UP,
    STAIR_HOLD,
    STAIR_RAMP_DOWN,
    STAIR_WAIT_FOR_REARM
} StairAssistState_t;

typedef struct {
    StairAssistState_t state;
    uint32_t            state_start_tick;
    float               prev_angle_deg;
    float               velocity_dps;
    float               torque_cmd_nm;
    float               ramp_down_start_torque_nm;
} StairLegFsm_t;

typedef struct __attribute__((packed)) {
    float rh_state;
    float lh_state;
    float rh_hip_angle_deg;
    float lh_hip_angle_deg;
    float rh_velocity_dps;
    float lh_velocity_dps;
    float rh_torque_nm;
    float lh_torque_nm;
} StairStreamData_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t    s_tsm;
static StairLegFsm_t    s_leg_rh;
static StairLegFsm_t    s_leg_lh;
static StairStreamData_t s_stream;
static bool             s_assist_enabled;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Off_Loop(void);
static void Standby_Entry(void);
static void Standby_Loop(void);
static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

static void _ResetLeg(StairLegFsm_t *leg, float angle_deg);
static float _UpdateLeg(StairLegFsm_t *leg, float angle_deg,
                        bool opposite_foot_contact, float torque_sign);
static float _GetHipAngleRh(void);
static float _GetHipAngleLh(void);
static float _Clamp01(float value);
static float _SmoothStep(float value);
static void _UpdateStream(float angle_rh, float angle_lh,
                          float torque_rh, float torque_lh);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = {
        .id = XM_STATE_OFF,
        .on_loop = Off_Loop
    };
    XM_TSM_AddState(s_tsm, &off_conf);

    XmStateConfig_t standby_conf = {
        .id = XM_STATE_STANDBY,
        .on_entry = Standby_Entry,
        .on_loop = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &standby_conf);

    XmStateConfig_t active_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop = Active_Loop,
        .on_exit = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &active_conf);

    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"RH State\",\"unit\":\"-\"},"
        "{\"name\":\"LH State\",\"unit\":\"-\"},"
        "{\"name\":\"RH Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"LH Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"RH Hip Velocity\",\"unit\":\"deg/s\"},"
        "{\"name\":\"LH Hip Velocity\",\"unit\":\"deg/s\"},"
        "{\"name\":\"RH Assist Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"LH Assist Torque\",\"unit\":\"Nm\"}]");

    XM_SetControlMode(XM_CTRL_MONITOR);
}

void User_Loop(void)
{
    if (!s_tsm) {
        return;
    }

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

static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Entry(void)
{
    XM_SetAssistTorque(0.0f, 0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);
    s_assist_enabled = false;
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    float angle_rh = _GetHipAngleRh();
    float angle_lh = _GetHipAngleLh();

    _ResetLeg(&s_leg_rh, angle_rh);
    _ResetLeg(&s_leg_lh, angle_lh);
    s_assist_enabled = false;

    XM_SetAssistTorque(0.0f, 0.0f);
    XM_SetControlMode(XM_CTRL_TORQUE);
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);
    XM_SendUsbDebugMessage("[STAIR] ACTIVE: press BTN1 to enable output\r\n");
}

static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_assist_enabled = !s_assist_enabled;
        _ResetLeg(&s_leg_rh, _GetHipAngleRh());
        _ResetLeg(&s_leg_lh, _GetHipAngleLh());
        XM_SetLedState(XM_LED_2, s_assist_enabled ? XM_ON : XM_OFF);
        XM_SendUsbDebugMessage(s_assist_enabled
            ? "[STAIR] output enabled\r\n"
            : "[STAIR] output disabled\r\n");
    }

    float angle_rh = _GetHipAngleRh();
    float angle_lh = _GetHipAngleLh();
    float torque_rh = 0.0f;
    float torque_lh = 0.0f;

    if (s_assist_enabled) {
        torque_rh = _UpdateLeg(&s_leg_rh, angle_rh,
                               XM.status.h10.isLeftFootContact,
                               HIP_FLEXION_TORQUE_SIGN_RH);
        torque_lh = _UpdateLeg(&s_leg_lh, angle_lh,
                               XM.status.h10.isRightFootContact,
                               HIP_FLEXION_TORQUE_SIGN_LH);

        float level_scale = (float)XM.status.h10.h10AssistLevel / 10.0f;
        torque_rh *= level_scale;
        torque_lh *= level_scale;
    } else {
        _ResetLeg(&s_leg_rh, angle_rh);
        _ResetLeg(&s_leg_lh, angle_lh);
    }

    XM_SetAssistTorque(torque_rh, torque_lh);
    XM_SetLedState(XM_LED_3,
        (torque_rh != 0.0f || torque_lh != 0.0f) ? XM_ON : XM_OFF);
    _UpdateStream(angle_rh, angle_lh, torque_rh, torque_lh);
}

static void Active_Exit(void)
{
    XM_SetAssistTorque(0.0f, 0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);
    s_assist_enabled = false;
    XM_SendUsbDebugMessage("[STAIR] output stopped\r\n");
}

static void _ResetLeg(StairLegFsm_t *leg, float angle_deg)
{
    leg->state = STAIR_WAIT_FOR_FLEXION;
    leg->state_start_tick = XM_GetTick();
    leg->prev_angle_deg = angle_deg;
    leg->velocity_dps = 0.0f;
    leg->torque_cmd_nm = 0.0f;
    leg->ramp_down_start_torque_nm = 0.0f;
}

static float _UpdateLeg(StairLegFsm_t *leg, float angle_deg,
                        bool opposite_foot_contact, float torque_sign)
{
    uint32_t now = XM_GetTick();
    uint32_t elapsed_ms = now - leg->state_start_tick;
    float raw_velocity_dps = (angle_deg - leg->prev_angle_deg) / CONTROL_DT_S;

    leg->prev_angle_deg = angle_deg;
    leg->velocity_dps += VELOCITY_LPF_ALPHA
                       * (raw_velocity_dps - leg->velocity_dps);

    switch (leg->state) {
        case STAIR_WAIT_FOR_FLEXION: {
            bool contact_ok = !REQUIRE_OPPOSITE_FOOT_CONTACT
                           || opposite_foot_contact;
            bool angle_ok = angle_deg >= FLEXION_TRIGGER_ANGLE_DEG;
            bool velocity_ok = leg->velocity_dps >= FLEXION_VELOCITY_TRIGGER_DPS;

            leg->torque_cmd_nm = 0.0f;
            if (contact_ok && angle_ok && velocity_ok) {
                leg->state = STAIR_RAMP_UP;
                leg->state_start_tick = now;
            }
            break;
        }

        case STAIR_RAMP_UP: {
            float ratio = (float)elapsed_ms / (float)RAMP_UP_DURATION_MS;
            leg->torque_cmd_nm = torque_sign * ASSIST_TORQUE_MAX_NM
                               * _SmoothStep(_Clamp01(ratio));
            if (elapsed_ms >= RAMP_UP_DURATION_MS) {
                leg->state = STAIR_HOLD;
                leg->state_start_tick = now;
            }
            break;
        }

        case STAIR_HOLD:
            leg->torque_cmd_nm = torque_sign * ASSIST_TORQUE_MAX_NM;
            if (angle_deg <= FLEXION_RELEASE_ANGLE_DEG
                || elapsed_ms >= HOLD_MAX_DURATION_MS) {
                leg->ramp_down_start_torque_nm = leg->torque_cmd_nm;
                leg->state = STAIR_RAMP_DOWN;
                leg->state_start_tick = now;
            }
            break;

        case STAIR_RAMP_DOWN: {
            float ratio = (float)elapsed_ms / (float)RAMP_DOWN_DURATION_MS;
            leg->torque_cmd_nm = leg->ramp_down_start_torque_nm
                               * (1.0f - _SmoothStep(_Clamp01(ratio)));
            if (elapsed_ms >= RAMP_DOWN_DURATION_MS) {
                leg->torque_cmd_nm = 0.0f;
                leg->state = STAIR_WAIT_FOR_REARM;
                leg->state_start_tick = now;
            }
            break;
        }

        case STAIR_WAIT_FOR_REARM:
            leg->torque_cmd_nm = 0.0f;
            if (angle_deg <= FLEXION_RELEASE_ANGLE_DEG) {
                leg->state = STAIR_WAIT_FOR_FLEXION;
                leg->state_start_tick = now;
            }
            break;

        default:
            _ResetLeg(leg, angle_deg);
            break;
    }

    return leg->torque_cmd_nm;
}

static float _GetHipAngleRh(void)
{
#if USE_MOTOR_ENCODER_ANGLE
    return HIP_FLEXION_SIGN_RH * XM.status.h10.rightHipMotorAngle;
#else
    return HIP_FLEXION_SIGN_RH * XM.status.h10.rightHipAngle;
#endif
}

static float _GetHipAngleLh(void)
{
#if USE_MOTOR_ENCODER_ANGLE
    return HIP_FLEXION_SIGN_LH * XM.status.h10.leftHipMotorAngle;
#else
    return HIP_FLEXION_SIGN_LH * XM.status.h10.leftHipAngle;
#endif
}

static float _Clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float _SmoothStep(float value)
{
    return value * value * (3.0f - 2.0f * value);
}

static void _UpdateStream(float angle_rh, float angle_lh,
                          float torque_rh, float torque_lh)
{
    s_stream.rh_state = (float)s_leg_rh.state;
    s_stream.lh_state = (float)s_leg_lh.state;
    s_stream.rh_hip_angle_deg = angle_rh;
    s_stream.lh_hip_angle_deg = angle_lh;
    s_stream.rh_velocity_dps = s_leg_rh.velocity_dps;
    s_stream.lh_velocity_dps = s_leg_lh.velocity_dps;
    s_stream.rh_torque_nm = torque_rh;
    s_stream.lh_torque_nm = torque_lh;
    XM_SendUsbDataWithId(&s_stream, sizeof(s_stream), 0xF0);
}
