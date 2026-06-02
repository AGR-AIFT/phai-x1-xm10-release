/**
 ******************************************************************************
 * @file    pd_realtime_control_emg.c
 * @brief   EMG proportional assist torque control using external ADC inputs.
 * @details
 * This user algorithm follows the same runtime structure as
 * pd_realtime_control.c, but converts biased analog EMG sensor signals to
 * proportional hip assist torque commands.
 *
 * Signal chain per channel:
 *   raw ADC voltage -> bias removal -> pre-rectification LPF ->
 *   full-wave rectification -> envelope LPF -> deadband/gain/saturation.
 *
 * Default EMG bias is 1.65 V for 0.0~3.3 V biased sensors. Press BTN_1 in
 * ACTIVE state to measure a 3 second resting bias for all four channels.
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

/* Envelope voltage that maps to full torque after deadband. Tune per sensor/user. */
#define EMG_FULL_SCALE_V        1.000f

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
    CAL_RUNNING,
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

static CalState_t s_cal_state = CAL_IDLE;
static uint32_t s_cal_tick = 0U;
static uint32_t s_cal_count = 0U;
static double s_cal_sum[ADC_CH_COUNT] = {0.0};

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
static void _StartCalibration(void);
static void _ProcessEmgSignals(void);
static void _UpdatePublicSignals(void);
static void _SelectTorquePair(float *rh_torque, float *lh_torque);
static void _UpdateUsbDebug(float rh_torque, float lh_torque);
static void _UpdateStreamData(float rh_torque, float lh_torque);

static float _LowPassUpdate(float prev, float input, float cutoff_hz);
static float _AbsFloat(float x);
static float _ClampFloat(float x, float min_value, float max_value);
static float _EnvelopeToTorque(float envelope_v);

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
     * 호출하지 않으면 XM_GetButtonEvent() 가 항상 NONE → 캘리브레이션 동작 불가. */
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

    if (s_cal_state == CAL_RUNNING) {
        _ResetEmgFilters();
        _UpdatePublicSignals();
        _UpdateStreamData(0.0f, 0.0f);
        _UpdateUsbDebug(0.0f, 0.0f);
        return;
    }

    _ProcessEmgSignals();
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

        if (s_cal_state == CAL_RUNNING) {
            s_cal_sum[i] += s_raw_v[i];
        }
    }

    if (s_cal_state == CAL_RUNNING) {
        s_cal_count++;
    }
}

static void _UpdateCalibration(void)
{
    XmBtnEvent_t evt = XM_GetButtonEvent(XM_BTN_1);
    if (evt == XM_BTN_CLICK && s_cal_state != CAL_RUNNING) {
        _StartCalibration();
        return;
    }

    if (s_cal_state == CAL_RUNNING) {
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
                     "[EMG CAL] done n=%lu | PF3:%.3f PF4:%.3f PF5:%.3f PF6:%.3f V\r\n",
                     (unsigned long)s_cal_count,
                     s_bias_v[CH_PF3], s_bias_v[CH_PF4],
                     s_bias_v[CH_PF5], s_bias_v[CH_PF6]);
            XM_SendUsbDebugMessage(buf);
        }
    }
}

static void _StartCalibration(void)
{
    for (int i = 0; i < ADC_CH_COUNT; i++) {
        s_cal_sum[i] = 0.0;
    }

    s_cal_count = 0U;
    s_cal_tick = XM_GetTick();
    s_cal_state = CAL_RUNNING;

    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 50);
    XM_SendUsbDebugMessage("[EMG CAL] start, keep EMG relaxed for 3 seconds\r\n");
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
        s_torque_nm[i] = _EnvelopeToTorque(s_envelope_v[i]);
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
                 "EMG | pair:%u on:%u env RH/LH:%.3f/%.3f V tau RH/LH:%.2f/%.2f Nm\r\n",
                 (unsigned int)torque_input_pair,
                 (unsigned int)control_ON,
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

static float _EnvelopeToTorque(float envelope_v)
{
    float active_v = envelope_v - EMG_ENVELOPE_DEADBAND_V;
    if (active_v <= 0.0f) {
        return 0.0f;
    }

    float normalized = active_v / (EMG_FULL_SCALE_V - EMG_ENVELOPE_DEADBAND_V);
    float torque = normalized * EMG_MAX_TORQUE_NM;

    return _ClampFloat(torque, EMG_MIN_TORQUE_NM, EMG_MAX_TORQUE_NM);
}
