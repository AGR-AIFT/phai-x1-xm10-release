/**
 ******************************************************************************
 * @file    Final_EMG.c
 * @brief   EMG proportional assist torque control using external ADC inputs.
 * @details
 * ============================================================================
 * 1. Purpose
 * ============================================================================
 * This file is a project backbone for designing EMG-based hip assistance.
 * It reads up to four external analog EMG signals, extracts a smoothed muscle
 * activation envelope, and converts the selected right/left EMG pair into
 * proportional hip assist-torque commands.
 *
 * The default algorithm is intentionally simple so that students can observe
 * each processing step and tune the response through STM32CubeIDE Live
 * Expressions. Validate the signal quality and torque direction on a bench
 * setup before wearing the device.
 *
 * ============================================================================
 * 2. Sensor Mapping
 * ============================================================================
 * Four external ADC inputs are available:
 *   PF3 = XM_EXT_ADC_5
 *   PF4 = XM_EXT_ADC_6
 *   PF5 = XM_EXT_ADC_7
 *   PF6 = XM_EXT_ADC_8
 *
 * Select the EMG pair with torque_input_pair in Live Expressions:
 *   torque_input_pair = 0: PF3 -> right hip, PF4 -> left hip (default)
 *   torque_input_pair = 1: PF5 -> right hip, PF6 -> left hip
 *
 * The default sensor assumption is a 0.0~3.3 V biased raw-EMG output with a
 * resting center voltage near 1.65 V. If the connected sensor already outputs
 * a processed envelope, simplify the processing chain before using this file.
 *
 * ============================================================================
 * 3. EMG Signal Processing Pipeline
 * ============================================================================
 * Each channel is processed independently:
 *
 *   raw ADC voltage
 *   -> resting-bias removal
 *   -> pre-rectification low-pass filter
 *   -> full-wave rectification
 *   -> envelope low-pass filter
 *   -> deadband removal
 *   -> active-effort normalization
 *   -> proportional torque gain and saturation
 *
 * Default settings:
 *   EMG_RAW_LPF_CUTOFF_HZ   = 80 Hz
 *   EMG_ENV_LPF_CUTOFF_HZ   = 5 Hz
 *   EMG_ENVELOPE_DEADBAND_V = 0.020 V
 *   EMG_MAX_TORQUE_NM       = 2.5 Nm
 *   emg_assist_scale        = 0.5 (default maximum output = 1.25 Nm)
 *
 * The resulting assist torque increases with the measured EMG envelope and is
 * clamped to 0.0~(EMG_MAX_TORQUE_NM * emg_assist_scale).
 *
 * ============================================================================
 * 4. Required Operating Procedure
 * ============================================================================
 * 1) Connect the EMG sensors and switch the H10 device to ASSIST mode.
 * 2) Select torque_input_pair in STM32CubeIDE Live Expressions.
 * 3) Relax the target muscles, press BTN1 once, and remain still for 3 seconds.
 * 4) Contract the target muscles at a representative effort level, press BTN2
 *    once, and maintain the contraction for 3 seconds.
 * 5) Observe the raw, centered, envelope, and torque variables in Live
 *    Expressions. Confirm that relaxed EMG produces approximately zero torque.
 * 6) Validate the torque direction and amplitude on a bench setup.
 * 7) Set control_ON = 1 in Live Expressions only after the bench validation.
 *
 * BTN3 resets the calibration values to the defaults. Repeat BTN1 and BTN2
 * calibration after a reset, sensor repositioning, or electrode replacement.
 *
 * IMPORTANT CURRENT IMPLEMENTATION LIMITATION:
 * This version does not block BTN2 before BTN1 and does not require completed
 * calibration before control_ON enables torque. Always perform BTN1 and BTN2
 * calibration in order before setting control_ON = 1.
 *
 * ============================================================================
 * 5. Button Functions
 * ============================================================================
 *   BTN1 click: capture resting EMG bias for 3 seconds
 *   BTN2 click: capture representative active-effort envelope for 3 seconds
 *   BTN3 click: reset calibration values to defaults
 *
 * LED feedback:
 *   LED1 fast blink: resting calibration is running
 *   LED2 fast blink: active-effort calibration is running
 *   LED3 one-shot  : calibration values were reset
 *
 * ============================================================================
 * 6. Important Live Expressions Variables
 * ============================================================================
 * Write from debugger:
 *   control_ON        : 0 = torque disabled, 1 = proportional torque enabled
 *   torque_input_pair : 0 = PF3/PF4 pair, 1 = PF5/PF6 pair
 *   emg_assist_scale  : output scale, clamped to 0.0~1.0 (default = 0.5)
 *
 * Observe only:
 *   emg_pf3_raw_v .. emg_pf6_raw_v
 *       Raw sensor voltages before signal processing.
 *   emg_pf3_centered_v .. emg_pf6_centered_v
 *       Raw voltages after resting-bias removal.
 *   emg_pf3_envelope_v .. emg_pf6_envelope_v
 *       Smoothed muscle-activation envelopes after rectification.
 *   emg_pf3_torque_nm .. emg_pf6_torque_nm
 *       Per-channel proportional torque values before pair selection.
 *
 * ============================================================================
 * 7. USB CDC Custom Stream
 * ============================================================================
 * Module ID 0xF0 sends six float channels for PC-side live plotting or CSV
 * logging. The selected pair follows torque_input_pair.
 *
 * Default Module ID 0xF0 CSV channel order:
 *   1) Raw RH    : selected right-side raw EMG voltage
 *   2) Raw LH    : selected left-side raw EMG voltage
 *   3) Env RH    : selected right-side EMG envelope
 *   4) Env LH    : selected left-side EMG envelope
 *   5) Tau RH    : requested right-side assist torque
 *   6) Tau LH    : requested left-side assist torque
 *
 * ============================================================================
 * 8. Recommended Student Design Tasks
 * ============================================================================
 * Example A - Tune the envelope response:
 *   Adjust EMG_ENV_LPF_CUTOFF_HZ and compare the envelope signal during a
 *   repeated contraction task. A lower value is smoother but slower. A higher
 *   value reacts faster but may produce more torque fluctuation.
 *
 * Example B - Tune the resting-noise rejection:
 *   Adjust EMG_ENVELOPE_DEADBAND_V after measuring the relaxed envelope.
 *   Increase it if residual noise produces unintended torque. Do not increase
 *   it so far that intentional low-level contractions are ignored.
 *
 * Example C - Change the assist mapping:
 *   Modify _EnvelopeToTorque() to compare linear, threshold-based, squared, or
 *   piecewise torque mappings. Preserve the final torque clamp.
 *
 * Example D - Add a smooth torque profile:
 *   Add a torque ramp or slew-rate limiter after _SelectTorquePair() to avoid
 *   sudden torque changes when muscle activation rises quickly.
 *
 * Example E - Add a high-level assist condition:
 *   Combine EMG activation with an FSR contact state or gait-phase detector so
 *   that EMG-based torque is permitted only during the intended gait period.
 *
 * ============================================================================
 * 9. Areas Students Should Not Modify Without Instructor Review
 * ============================================================================
 * Do not remove the final torque saturation in _EnvelopeToTorque(), the zero-
 * torque commands during mode exit and calibration, or the low-level
 * XM_SetControlMode() and XM_SetAssistTorqueLH/RH() calls.
 *
 * Start with low torque, validate the output on a bench setup, and increase the
 * assist level gradually only after confirming the direction and signal
 * quality.
 *
 * ============================================================================
 * 10. Quick Code Map (Approximate Line Numbers)
 * ============================================================================
 * Line numbers may move slightly after edits. Search the function name first.
 *   approx. 260-302 : Live Expressions controls and EMG runtime variables
 *   approx. 519-533 : _SampleAdcChannels() raw ADC voltage sampling
 *   approx. 534-646 : _UpdateCalibration() BTN1/BTN2 calibration sequence
 *   approx. 656-685 : EMG filtering, rectification, and envelope extraction
 *   approx. 686-717 : _SelectTorquePair() selected right/left command routing
 *   approx. 764-787 : _EnvelopeToTorque() proportional mapping and clamp
 ******************************************************************************
 */

#include "xm_api.h"

#include <stdio.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define CONTROL_DT              0.001f
#define USB_DEBUG_PERIOD_MS     500U

#define ADC_CH_COUNT            4
#define EMG_DEFAULT_BIAS_V      1.65f
#define EMG_CAL_DURATION_MS     3000U

/* First low-pass after bias removal. Kept high enough to preserve EMG activity. */
#define EMG_RAW_LPF_CUTOFF_HZ   80.0f

/* Envelope low-pass after full-wave rectification. */
#define EMG_ENV_LPF_CUTOFF_HZ   5.0f

/* Ignore small residual noise around rest after envelope extraction. */
#define EMG_ENVELOPE_DEADBAND_V 0.020f

/* Default envelope voltage that maps to full torque after deadband. */
#define EMG_FULL_SCALE_V        1.000f
#define EMG_MIN_FULL_SCALE_V    0.050f

#define EMG_MAX_TORQUE_NM       2.5f
#define EMG_MIN_TORQUE_NM       0.0f

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef enum {
    CH_PF3 = 0,
    CH_PF4 = 1,
    CH_PF5 = 2,
    CH_PF6 = 3
} AdcChIdx_t;

typedef enum {
    CAL_IDLE,
    CAL_REST_RUNNING,
    CAL_EFFORT_RUNNING,
    CAL_DONE
} CalState_t;

typedef struct {
    float raw_rh_v;
    float raw_lh_v;
    float env_rh_v;
    float env_lh_v;
    float torque_rh_nm;
    float torque_lh_nm;
} EmgStreamData_t;

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

float emg_pf3_raw_v;
float emg_pf4_raw_v;
float emg_pf5_raw_v;
float emg_pf6_raw_v;

float emg_pf3_centered_v;
float emg_pf4_centered_v;
float emg_pf5_centered_v;
float emg_pf6_centered_v;

float emg_pf3_envelope_v;
float emg_pf4_envelope_v;
float emg_pf5_envelope_v;
float emg_pf6_envelope_v;

float emg_pf3_torque_nm;
float emg_pf4_torque_nm;
float emg_pf5_torque_nm;
float emg_pf6_torque_nm;

uint16_t control_ON;
uint16_t torque_input_pair;  /* 0: PF3/PF4 -> RH/LH, 1: PF5/PF6 -> RH/LH */
float emg_assist_scale = 0.5f;  /* 0.5: default maximum output is 1.25 Nm */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;

static const XmAdcPin_t s_adc_pins[ADC_CH_COUNT] = {
    XM_EXT_ADC_5,
    XM_EXT_ADC_6,
    XM_EXT_ADC_7,
    XM_EXT_ADC_8
};

static float s_raw_v[ADC_CH_COUNT];
static float s_centered_v[ADC_CH_COUNT];
static float s_pre_lpf_v[ADC_CH_COUNT];
static float s_rectified_v[ADC_CH_COUNT];
static float s_envelope_v[ADC_CH_COUNT];
static float s_torque_nm[ADC_CH_COUNT];
static float s_bias_v[ADC_CH_COUNT] = {
    EMG_DEFAULT_BIAS_V,
    EMG_DEFAULT_BIAS_V,
    EMG_DEFAULT_BIAS_V,
    EMG_DEFAULT_BIAS_V
};
static float s_full_scale_v[ADC_CH_COUNT] = {
    EMG_FULL_SCALE_V,
    EMG_FULL_SCALE_V,
    EMG_FULL_SCALE_V,
    EMG_FULL_SCALE_V
};

static CalState_t s_cal_state = CAL_IDLE;
static uint32_t s_cal_tick = 0U;
static uint32_t s_cal_count = 0U;
static double s_cal_sum[ADC_CH_COUNT] = {0.0};
static float s_cal_max_env[ADC_CH_COUNT] = {0.0f};

static uint32_t s_usb_debug_timer = 0U;
static EmgStreamData_t s_stream_data;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Off_Loop(void);
static void Standby_Loop(void);
static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

static float _ReadAdcVoltage(XmAdcPin_t pin);
static void _ResetEmgFilters(void);
static void _SampleAdcChannels(void);
static void _UpdateCalibration(void);
static void _StartRestCalibration(void);
static void _StartEffortCalibration(void);
static void _ResetCalibrationDefaults(void);
static void _ProcessEmgSignals(void);
static void _UpdatePublicSignals(void);
static void _SelectTorquePair(float *rh_torque, float *lh_torque);
static void _UpdateUsbDebug(float rh_torque, float lh_torque);
static void _UpdateStreamData(float rh_torque, float lh_torque);

static float _LowPassUpdate(float prev, float input, float cutoff_hz);
static float _AbsFloat(float x);
static float _ClampFloat(float x, float min_value, float max_value);
static float _EnvelopeToTorque(int ch, float envelope_v);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    XM_SetExtPowerVoltage(XM_EXT_PWR_5V);

    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = {
        .id = XM_STATE_OFF,
        .on_loop = Off_Loop
    };
    XM_TSM_AddState(s_tsm, &off_conf);

    XmStateConfig_t sb_conf = {
        .id = XM_STATE_STANDBY,
        .on_loop = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop = Active_Loop,
        .on_exit = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Raw RH\",\"unit\":\"V\"},"
        "{\"name\":\"Raw LH\",\"unit\":\"V\"},"
        "{\"name\":\"Env RH\",\"unit\":\"V\"},"
        "{\"name\":\"Env LH\",\"unit\":\"V\"},"
        "{\"name\":\"Tau RH\",\"unit\":\"Nm\"},"
        "{\"name\":\"Tau LH\",\"unit\":\"Nm\"}]");

    XM_SwitchDioToAdc(XM_EXT_DIO_1);
    XM_SwitchDioToAdc(XM_EXT_DIO_2);
    XM_SwitchDioToAdc(XM_EXT_DIO_3);
    XM_SwitchDioToAdc(XM_EXT_DIO_4);

    XM_SetControlMode(XM_CTRL_MONITOR);
}

void User_Loop(void)
{
    /* TSM 핸들 생성 실패 시 안전 정지 (NULL 역참조 HardFault 방지) */
    if (!s_tsm) {
        return;
    }

    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }

    XM_TSM_Run(s_tsm);

    /* [필수] 버튼 이벤트 디바운싱 + LED 효과 타이머 틱 (xm_api_led_btn.h).
     * 호출하지 않으면 XM_GetButtonEvent() 가 항상 NONE → BTN1/2/3 캘리브레이션 동작 불가. */
    XM_IO_Update();
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SendUsbDebugMessage("[EMG] CM connected -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[EMG] ASSIST mode detected -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);

    _ResetEmgFilters();
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SendUsbDebugMessage("[EMG] ACTIVE start, proportional torque enabled\r\n");
}

static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        XM_SendUsbDebugMessage("[EMG] ASSIST released -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _SampleAdcChannels();
    _UpdateCalibration();

    if (s_cal_state == CAL_REST_RUNNING) {
        _ResetEmgFilters();
        _UpdatePublicSignals();
        _UpdateStreamData(0.0f, 0.0f);
        _UpdateUsbDebug(0.0f, 0.0f);
        return;
    }

    _ProcessEmgSignals();

    if (s_cal_state == CAL_EFFORT_RUNNING) {
        for (int i = 0; i < ADC_CH_COUNT; i++) {
            if (s_envelope_v[i] > s_cal_max_env[i]) {
                s_cal_max_env[i] = s_envelope_v[i];
            }
        }
        _UpdatePublicSignals();
        _UpdateStreamData(0.0f, 0.0f);
        _UpdateUsbDebug(0.0f, 0.0f);
        return;
    }

    _UpdatePublicSignals();

    float rh_torque = 0.0f;
    float lh_torque = 0.0f;
    _SelectTorquePair(&rh_torque, &lh_torque);

    if (control_ON == 1U) {
        XM_SetAssistTorqueRH(rh_torque);
        XM_SetAssistTorqueLH(lh_torque);
    } else {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
    }

    _UpdateStreamData(rh_torque, lh_torque);
    _UpdateUsbDebug(rh_torque, lh_torque);
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);

    XM_SendUsbDebugMessage("[EMG] control stopped, torque cleared\r\n");
}

static float _ReadAdcVoltage(XmAdcPin_t pin)
{
    return (float)XM_AnalogReadMillivolts(pin) * 0.001f;
}

static void _ResetEmgFilters(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_raw_v[i] = _ReadAdcVoltage(s_adc_pins[i]);
        s_centered_v[i] = s_raw_v[i] - s_bias_v[i];
        s_pre_lpf_v[i] = 0.0f;
        s_rectified_v[i] = 0.0f;
        s_envelope_v[i] = 0.0f;
        s_torque_nm[i] = 0.0f;
    }
}

static void _SampleAdcChannels(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_raw_v[i] = _ReadAdcVoltage(s_adc_pins[i]);

        if (s_cal_state == CAL_REST_RUNNING) {
            s_cal_sum[i] += s_raw_v[i];
        }
    }

    if (s_cal_state == CAL_REST_RUNNING) {
        s_cal_count++;
    }
}

static void _UpdateCalibration(void)
{
    XmBtnEvent_t btn1 = XM_GetButtonEvent(XM_BTN_1);
    XmBtnEvent_t btn2 = XM_GetButtonEvent(XM_BTN_2);
    XmBtnEvent_t btn3 = XM_GetButtonEvent(XM_BTN_3);

    bool cal_busy = (s_cal_state == CAL_REST_RUNNING || s_cal_state == CAL_EFFORT_RUNNING);

    if (btn3 == XM_BTN_CLICK && !cal_busy) {
        _ResetCalibrationDefaults();
        return;
    }

    if (btn1 == XM_BTN_CLICK && !cal_busy) {
        _StartRestCalibration();
        return;
    }

    if (btn2 == XM_BTN_CLICK && !cal_busy) {
        _StartEffortCalibration();
        return;
    }

    if (s_cal_state == CAL_REST_RUNNING) {
        uint32_t elapsed = XM_GetTick() - s_cal_tick;
        if (elapsed >= EMG_CAL_DURATION_MS) {
            if (s_cal_count > 0U) {
                for (int i = 0; i < ADC_CH_COUNT; i++) {
                    s_bias_v[i] = (float)(s_cal_sum[i] / (double)s_cal_count);
                }
            }

            s_cal_state = CAL_DONE;
            _ResetEmgFilters();
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);

            char buf[128];
            snprintf(buf, sizeof(buf),
                     "[EMG CAL] rest done n=%lu | PF3:%.3f PF4:%.3f PF5:%.3f PF6:%.3f V\r\n",
                     (unsigned long)s_cal_count,
                     s_bias_v[CH_PF3], s_bias_v[CH_PF4],
                     s_bias_v[CH_PF5], s_bias_v[CH_PF6]);
            XM_SendUsbDebugMessage(buf);
        }
    } else if (s_cal_state == CAL_EFFORT_RUNNING) {
        uint32_t elapsed = XM_GetTick() - s_cal_tick;
        if (elapsed >= EMG_CAL_DURATION_MS) {
            for (int i = 0; i < ADC_CH_COUNT; i++) {
                float scale = s_cal_max_env[i] - EMG_ENVELOPE_DEADBAND_V;
                if (scale < EMG_MIN_FULL_SCALE_V) {
                    scale = EMG_MIN_FULL_SCALE_V;
                }
                s_full_scale_v[i] = scale + EMG_ENVELOPE_DEADBAND_V;
            }

            s_cal_state = CAL_DONE;
            _ResetEmgFilters();
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);

            char buf[160];
            snprintf(buf, sizeof(buf),
                     "[EMG CAL] effort done | FS PF3:%.3f PF4:%.3f PF5:%.3f PF6:%.3f V\r\n",
                     s_full_scale_v[CH_PF3], s_full_scale_v[CH_PF4],
                     s_full_scale_v[CH_PF5], s_full_scale_v[CH_PF6]);
            XM_SendUsbDebugMessage(buf);
        }
    }
}

static void _StartRestCalibration(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_cal_sum[i] = 0.0;
    }

    s_cal_count = 0U;
    s_cal_tick = XM_GetTick();
    s_cal_state = CAL_REST_RUNNING;

    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 50);
    XM_SendUsbDebugMessage("[EMG CAL] start, keep EMG relaxed for 3 seconds\r\n");
}

static void _StartEffortCalibration(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_cal_max_env[i] = 0.0f;
    }

    s_cal_tick = XM_GetTick();
    s_cal_state = CAL_EFFORT_RUNNING;

    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    _ResetEmgFilters();
    XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 50);
    XM_SendUsbDebugMessage("[EMG CAL] effort start, contract target muscles for 3 seconds\r\n");
}

static void _ResetCalibrationDefaults(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_bias_v[i] = EMG_DEFAULT_BIAS_V;
        s_full_scale_v[i] = EMG_FULL_SCALE_V;
    }

    s_cal_state = CAL_IDLE;
    _ResetEmgFilters();
    XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, 1000);
    XM_SendUsbDebugMessage("[EMG CAL] reset to defaults\r\n");
}

static void _ProcessEmgSignals(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_centered_v[i] = s_raw_v[i] - s_bias_v[i];
        s_pre_lpf_v[i] = _LowPassUpdate(s_pre_lpf_v[i],
                                        s_centered_v[i],
                                        EMG_RAW_LPF_CUTOFF_HZ);
        s_rectified_v[i] = _AbsFloat(s_pre_lpf_v[i]);
        s_envelope_v[i] = _LowPassUpdate(s_envelope_v[i],
                                         s_rectified_v[i],
                                         EMG_ENV_LPF_CUTOFF_HZ);
        s_torque_nm[i] = _EnvelopeToTorque(i, s_envelope_v[i]);
    }
}

static void _UpdatePublicSignals(void)
{
    emg_pf3_raw_v = s_raw_v[CH_PF3];
    emg_pf4_raw_v = s_raw_v[CH_PF4];
    emg_pf5_raw_v = s_raw_v[CH_PF5];
    emg_pf6_raw_v = s_raw_v[CH_PF6];

    emg_pf3_centered_v = s_centered_v[CH_PF3];
    emg_pf4_centered_v = s_centered_v[CH_PF4];
    emg_pf5_centered_v = s_centered_v[CH_PF5];
    emg_pf6_centered_v = s_centered_v[CH_PF6];

    emg_pf3_envelope_v = s_envelope_v[CH_PF3];
    emg_pf4_envelope_v = s_envelope_v[CH_PF4];
    emg_pf5_envelope_v = s_envelope_v[CH_PF5];
    emg_pf6_envelope_v = s_envelope_v[CH_PF6];

    emg_pf3_torque_nm = s_torque_nm[CH_PF3];
    emg_pf4_torque_nm = s_torque_nm[CH_PF4];
    emg_pf5_torque_nm = s_torque_nm[CH_PF5];
    emg_pf6_torque_nm = s_torque_nm[CH_PF6];
}

static void _SelectTorquePair(float *rh_torque, float *lh_torque)
{
    if (torque_input_pair == 1U) {
        *rh_torque = s_torque_nm[CH_PF5];
        *lh_torque = s_torque_nm[CH_PF6];
    } else {
        *rh_torque = s_torque_nm[CH_PF3];
        *lh_torque = s_torque_nm[CH_PF4];
    }
}

static void _UpdateUsbDebug(float rh_torque, float lh_torque)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer >= USB_DEBUG_PERIOD_MS) {
        s_usb_debug_timer = now;

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "EMG | pair:%u on:%u cal:%d env RH/LH:%.3f/%.3f V tau RH/LH:%.2f/%.2f Nm\r\n",
                 (unsigned int)torque_input_pair,
                 (unsigned int)control_ON,
                 (int)s_cal_state,
                 s_stream_data.env_rh_v,
                 s_stream_data.env_lh_v,
                 rh_torque,
                 lh_torque);
        XM_SendUsbDebugMessage(buf);
    }
}

static void _UpdateStreamData(float rh_torque, float lh_torque)
{
    if (torque_input_pair == 1U) {
        s_stream_data.raw_rh_v = s_raw_v[CH_PF5];
        s_stream_data.raw_lh_v = s_raw_v[CH_PF6];
        s_stream_data.env_rh_v = s_envelope_v[CH_PF5];
        s_stream_data.env_lh_v = s_envelope_v[CH_PF6];
    } else {
        s_stream_data.raw_rh_v = s_raw_v[CH_PF3];
        s_stream_data.raw_lh_v = s_raw_v[CH_PF4];
        s_stream_data.env_rh_v = s_envelope_v[CH_PF3];
        s_stream_data.env_lh_v = s_envelope_v[CH_PF4];
    }

    s_stream_data.torque_rh_nm = rh_torque;
    s_stream_data.torque_lh_nm = lh_torque;

    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

static float _LowPassUpdate(float prev, float input, float cutoff_hz)
{
    const float two_pi = 6.28318530718f;
    float rc = 1.0f / (two_pi * cutoff_hz);
    float alpha = CONTROL_DT / (rc + CONTROL_DT);

    return prev + alpha * (input - prev);
}

static float _AbsFloat(float x)
{
    return (x < 0.0f) ? -x : x;
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

static float _EnvelopeToTorque(int ch, float envelope_v)
{
    float active_v = envelope_v - EMG_ENVELOPE_DEADBAND_V;
    if (active_v <= 0.0f) {
        return 0.0f;
    }

    float span = s_full_scale_v[ch] - EMG_ENVELOPE_DEADBAND_V;
    if (span < EMG_MIN_FULL_SCALE_V) {
        span = EMG_MIN_FULL_SCALE_V;
    }

    float normalized = active_v / span;
    float assist_scale = _ClampFloat(emg_assist_scale, 0.0f, 1.0f);
    float scaled_max_torque = EMG_MAX_TORQUE_NM * assist_scale;
    float torque = normalized * scaled_max_torque;

    return _ClampFloat(torque, EMG_MIN_TORQUE_NM, scaled_max_torque);
}
