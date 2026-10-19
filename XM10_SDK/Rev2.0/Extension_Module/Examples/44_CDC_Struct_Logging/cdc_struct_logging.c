/**
 ******************************************************************************
 * @file    cdc_struct_logging.c
 * @author  HyundoKim
 * @brief   [중급] 구조체를 이름 · 단위 · 타입과 함께 PC 로 보내기 (구조체 로깅)
 * @details
 * 직접 만든 구조체를 한 번 등록하면 필드 이름 · 단위 · 타입이 값과 함께 PC 로 갑니다.
 * float 만 보내던 Ex.09 와 달리 정수 · bool · 배열도 그대로 보낼 수 있고, PC 의 xm10 도구가
 * 구조체마다 탭을 열어 필드 이름으로 그래프와 CSV 를 만듭니다.
 *
 * [보내는 값] — 아래 USE_H10 으로 고릅니다
 *   USE_H10 = 0 (기본, 보드만)  SensorLog 100 Hz : tick · wave[2] · ramp · adc[4] · button[3]
 *   USE_H10 = 1 (H10)           SensorLog 1 kHz  : tick · hip_angle[2] · hip_gyro[2] · hip_torque[2] · h10_connected
 *   두 경우 모두                StatusLog 10 Hz  : uptime · clicks[3]
 *   H10 값의 배열 [0] 은 왼쪽, [1] 은 오른쪽입니다.
 *
 * [쓰는 순서]
 *   1. 보낼 구조체를 만듭니다. 크기는 4 의 배수(1,008 B 이하)이고, 남는 자리는 필드로 등록하지 않는 배열로 채웁니다.
 *   2. XM_LOG_DECLARE 로 구조체에 짧은 이름(11 자 이하)을 붙이고, XM_LOG_FIELDS 에 보낼 필드를 적습니다.
 *      필드 이름은 15 자, 단위는 7 자 이하의 ASCII 문자(영문 · 숫자 · 기호)입니다. 이 이름이 PC 의 탭 제목과 CSV 열 이름이 됩니다.
 *   3. Control_Setup() 에서 XM_LOG_REGISTER 로 모듈 ID(0xF0 ~ 0xFE)와 보낼 주기를 정합니다. 구조체는 4 개까지입니다.
 *   4. Control_Loop() 에서 XM_LOG_SEND 로 보냅니다. 매 루프 불러도 등록한 주기로만 나갑니다.
 *
 * [PC 에서 보기]
 *   SDK 에 함께 들어 있는 xm10 도구(pc-data-tool 폴더)를 보드의 COM 포트에 연결합니다.
 *   모듈 ID 마다 탭이 하나씩 열리고, 배열은 adc[0], adc[1] … 처럼 원소마다 열이 생깁니다.
 *   PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 xm10 도구로 보고 저장하세요.
 *
 * [버튼]
 *   버튼 1 ~ 3 : 누르고 있는 동안 button[] 이 1 입니다(USE_H10 = 0).
 *                짧게 누를 때마다 StatusLog 의 clicks[] 가 1 씩 늘어납니다.
 *
 * [LED]
 *   LED 1 켜짐          USB 가 준비되어 보낼 수 있습니다.
 *   LED 2 켜짐          H10 이 연결되었습니다(USE_H10 = 1).
 *   LED 3 빠른 깜빡임   구조체를 등록하지 못했습니다. 이름 · 단위 길이, 주기, 구조체 크기를 확인하세요.
 *
 * @warning PC 프로그램(PhAI Studio, xm10 도구, 시리얼 터미널 등)은 한 번에 하나만 연결하세요.
 * @warning SDK 에 함께 들어 있는 xm10 도구로 보세요. 예전 릴리스의 도구로는 일부 값이 보이지 않거나 틀리게 보일 수 있습니다.
 * @warning USE_H10 = 1 은 H10 의 값을 읽기만 합니다(토크를 내보내지 않습니다).
 *          H10 이 연결되기 전에는 값이 0 이고, 연결이 끊기면 마지막 값이 남습니다 — h10_connected 를 함께 보세요.
 * @warning Rev2.0 보드 전용입니다.
 *
 * @see     docs/api-reference/10-struct-logging.md
 * @see     docs/api-reference/05-usb-connectivity.md
 * @see     docs/getting-started/04-pc-data-tool.md
 * @see     Extension_Module/Examples/09_CDC_Stream/cdc_stream.c  (float 배열을 보내는 기본 스트리밍)
 * @see     Extension_Module/Examples/34_USB_Memory_Gait_Log/usb_memory_gait_log.c  (H10 보행 데이터를 USB 메모리에 저장)
 * @version 1.0
 * @date    Oct 15, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include "xm_api_logging_raw.h"   /* XM_LOG_* 매크로 지원 헤더 — 매크로를 쓰는 .c 파일은 꼭 include 하세요. 안의 함수는 직접 부르지 마세요(지원 대상 아님) */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/*
 * 보낼 데이터 고르기
 *   0 : 보드만으로 만든 값(기본) — H10 에서 값을 받지 않습니다. 보드 전원은 KIT H10 본체에서 들어오므로 본체 전원은 켜 둡니다.
 *   1 : H10 보행 데이터 — CAN-FD 로 연결된 H10 의 좌우 고관절 값을 1 kHz 로 보냅니다(읽기만 합니다).
 */
#ifndef USE_H10
#define USE_H10                 (0)
#endif
_Static_assert((USE_H10 == 0) || (USE_H10 == 1), "USE_H10 은 0 또는 1 입니다");

/*
 * 모듈 ID — 구조체마다 하나씩, 0xF0 ~ 0xFE 중에서 고릅니다.
 * 등록한 ID 는 그 구조체만 씁니다. xm10 도구는 모듈 ID 마다 탭을 하나씩 엽니다.
 */
#define SENSOR_LOG_MODULE_ID    (0xF0U)
#define STATUS_LOG_MODULE_ID    (0xF1U)

/*
 * 보내는 주기 [Hz] — 1 ~ 1000 Hz 이고, 1 ms 단위로 맞춰 보냅니다.
 * 주기를 올리거나 구조체를 키우면 PC 로 보내는 양이 늘어 등록이 거절될 수 있습니다(LED 3).
 */
#if (USE_H10 == 1)
#define SENSOR_LOG_RATE_HZ      (1000U)     /* H10 값은 1 ms 마다 새로 들어오므로 1 kHz 로 보냅니다 */
#else
#define SENSOR_LOG_RATE_HZ      (100U)      /* 10 ms 마다 하나 — 합성 신호와 버튼은 이 정도면 충분합니다 */
#endif
#define STATUS_LOG_RATE_HZ      (10U)       /* 0.1 초마다 하나 */

/* 배율(scale) — PC 에 보이는 값 = 보낸 값 × 배율 */
#define SCALE_NONE              (1.0f)      /* 보낸 값 그대로 */
#define RAMP_SCALE              (0.1f)      /* 0.1 단위 정수 → PC 에서는 소수점 한 자리 값 */

/* 배열 크기와 칸 번호 */
#define BUTTON_COUNT            (3U)        /* 버튼 1 ~ 3 */
#define EXT_ADC_COUNT           (4U)        /* 확장 ADC 1 ~ 4 — 따로 설정하지 않아도 읽을 수 있습니다 */
#define SIDE_COUNT              (2U)        /* H10 좌우 */
#define SIDE_LEFT               (0U)
#define SIDE_RIGHT              (1U)
#define WAVE_COUNT              (2U)
#define WAVE_SIN                (0U)
#define WAVE_COS                (1U)

/* 구조체 크기를 4 의 배수로 맞추는 빈 자리 [B] — 필드로 등록하지 않습니다 */
#define SENSOR_PAD_BYTES        (3U)        /* SensorLog: 보드만 4 + 8 + 2 + 8 + 3 = 25 → 28 B, H10 4 + 24 + 1 = 29 → 32 B */
#define STATUS_PAD_BYTES        (2U)        /* StatusLog: 4 + 6 = 10 → 12 B */

/* 합성 신호 — 보드 시간 [ms] 으로 만듭니다 */
#define WAVE_PERIOD_MS          (2000U)     /* 사인 · 코사인 한 바퀴 2 초 */
#define RAMP_PERIOD_MS          (1000U)     /* 톱니파가 0 → 100 % 를 1 초마다 반복 */
#define RAMP_FULL_X10           (1000U)     /* 100.0 % 를 0.1 단위 정수로 */
#define TWO_PI_F                (6.2831853f)

#define MS_PER_S                (1000U)

/* LED */
#define LED_USB                 (XM_LED_1)  /* USB 준비 */
#define LED_H10                 (XM_LED_2)  /* H10 연결 (USE_H10 = 1) */
#define LED_ERROR               (XM_LED_3)  /* 등록 실패 */
#define LED_ERROR_BLINK_MS      (200U)      /* 등록 실패 — 빠른 깜빡임 주기 */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief PC 로 보낼 구조체 1 — 센서 값 (SensorLog)
 * @details 멤버 타입이 그대로 PC 로 갑니다. 쓸 수 있는 타입은 float · double · bool · 정수(int8_t ~ uint64_t)와
 *          그 배열(원소 255 개까지)입니다. char · 포인터 · 구조체 멤버는 컴파일 오류가 납니다.
 */
typedef struct {
    uint32_t tick_ms;                       /**< 보드 시간 [ms] — XM_GetTick() */
#if (USE_H10 == 1)
    float    hip_angle[SIDE_COUNT];         /**< 고관절 각도 [deg] */
    float    hip_gyro[SIDE_COUNT];          /**< 고관절에 달린 IMU 의 Y 축 각속도 [deg/s] */
    float    hip_torque[SIDE_COUNT];        /**< 고관절 토크 추정값 [Nm] — 모터 전류로 계산한 값입니다 */
    bool     h10_connected;                 /**< H10 이 연결되어 있는가 */
#else
    float    wave[WAVE_COUNT];              /**< 합성 신호 — [0] 사인 · [1] 코사인 (진폭 1) */
    int16_t  ramp_x10;                      /**< 톱니파 0 ~ 100.0 % 를 0.1 단위 정수로 — 배율 0.1 로 등록합니다 */
    uint16_t adc_mv[EXT_ADC_COUNT];         /**< 확장 ADC 1 ~ 4 전압 [mV] — 아무것도 연결하지 않으면 잡음 값입니다 */
    bool     button[BUTTON_COUNT];          /**< 버튼 1 ~ 3 이 눌려 있는가 */
#endif
    uint8_t  pad[SENSOR_PAD_BYTES];         /**< 크기를 4 의 배수로 맞추는 빈 자리 (등록하지 않음) */
} SensorLog_t;

/**
 * @brief PC 로 보낼 구조체 2 — 느린 상태 (StatusLog)
 * @details 구조체마다 주기와 모듈 ID 를 따로 정할 수 있습니다. 자주 바뀌지 않는 값은 낮은 주기로 따로 보냅니다.
 */
typedef struct {
    uint32_t uptime_s;                      /**< 보드가 켜진 뒤 지난 시간 [s] */
    uint16_t clicks[BUTTON_COUNT];          /**< 버튼 1 ~ 3 을 짧게 누른 횟수 (켜진 뒤부터) */
    uint8_t  pad[STATUS_PAD_BYTES];         /**< 크기를 4 의 배수로 맞추는 빈 자리 (등록하지 않음) */
} StatusLog_t;

/*
 * 구조체에 짧은 이름을 붙입니다. 이 이름이 PC 의 탭 제목이 됩니다(11 자 이하, 영문 · 숫자 · _).
 * 크기가 4 의 배수가 아니거나 1,008 B 를 넘으면 이 줄에서 컴파일 오류가 납니다.
 */
XM_LOG_DECLARE(SensorLog, SensorLog_t);
XM_LOG_DECLARE(StatusLog, StatusLog_t);

/*
 * 보낼 필드 — XM_FIELD(이름, 멤버, "필드 이름", "단위", 배율), 배열은 XM_FIELD_ARRAY.
 * 타입 · 위치 · 배열 길이는 멤버에서 자동으로 정해지므로 적지 않습니다. 빈 자리(pad)는 적지 않습니다.
 */
XM_LOG_FIELDS(SensorLog) = {
    XM_FIELD(SensorLog, tick_ms, "tick", "ms", SCALE_NONE),
#if (USE_H10 == 1)
    XM_FIELD_ARRAY(SensorLog, hip_angle,  "hip_angle",  "deg",   SCALE_NONE),
    XM_FIELD_ARRAY(SensorLog, hip_gyro,   "hip_gyro",   "deg/s", SCALE_NONE),
    XM_FIELD_ARRAY(SensorLog, hip_torque, "hip_torque", "Nm",    SCALE_NONE),
    XM_FIELD(SensorLog, h10_connected, "h10_connected", "", SCALE_NONE),
#else
    XM_FIELD_ARRAY(SensorLog, wave,   "wave",   "",   SCALE_NONE),
    XM_FIELD(SensorLog, ramp_x10,     "ramp",   "%",  RAMP_SCALE),
    XM_FIELD_ARRAY(SensorLog, adc_mv, "adc",    "mV", SCALE_NONE),
    XM_FIELD_ARRAY(SensorLog, button, "button", "",   SCALE_NONE),
#endif
};

XM_LOG_FIELDS(StatusLog) = {
    XM_FIELD(StatusLog, uptime_s, "uptime", "s", SCALE_NONE),
    XM_FIELD_ARRAY(StatusLog, clicks, "clicks", "", SCALE_NONE),
};

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES  — 디버거 Live Expressions 로 관찰
 *-----------------------------------------------------------
 */

/* 등록 결과 — false 인 쪽이 등록하지 못한 구조체입니다(LED 3) */
volatile bool g_sensor_log_ok = false;
volatile bool g_status_log_ok = false;

/*
 * 구조체 로깅 카운터(xm_api_logging.h 의 XM_LogStats_t) — 보내지 못할 때 이유를 찾는 데 씁니다.
 * rate_gate_suppressed_calls 는 등록한 주기보다 일찍 부른 횟수라, 이 예제처럼 매 루프 부르면 계속 늘어나는 것이 정상입니다.
 */
XM_LogStats_t g_log_stats;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static const XmBtnId_t s_buttons[BUTTON_COUNT] = { XM_BTN_1, XM_BTN_2, XM_BTN_3 };

#if (USE_H10 == 0)
static const XmAdcPin_t s_adc_pins[EXT_ADC_COUNT] = { XM_EXT_ADC_1, XM_EXT_ADC_2, XM_EXT_ADC_3, XM_EXT_ADC_4 };
#endif

static uint16_t s_clicks[BUTTON_COUNT];     /* 버튼별 짧게 누른 횟수 */
static bool     s_usb_led_on;               /* LED 1 의 마지막 상태 — 바뀔 때만 다시 씁니다 */
#if (USE_H10 == 1)
static bool     s_h10_led_on;               /* LED 2 의 마지막 상태 */
#endif

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _UpdateLeds(void);
static void _CountClicks(void);
static void _FillSensorLog(SensorLog_t* rec);
static void _FillStatusLog(StatusLog_t* rec);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 부팅 후 한 번 호출됩니다. 보낼 구조체를 등록합니다.
 */
void Control_Setup(void)
{
    /*
     * [선택] PC 프로그램 종류(XM_USB_SetHostProfile)나 목표 시간(XM_Log_SetMinStallMs)을 바꾸려면
     * 첫 XM_LOG_REGISTER 보다 먼저 부르세요. 구조체를 등록한 뒤에는 바뀌지 않습니다.
     * xm10 도구는 기본값 그대로 쓰므로 이 예제는 부르지 않습니다.
     */

    /*
     * 1. 보낼 구조체를 등록합니다. 등록은 Control_Setup() 안에서만 됩니다.
     *    (구조체 이름, 모듈 ID, 보내는 주기 [Hz])
     */
    g_sensor_log_ok = XM_LOG_REGISTER(SensorLog, SENSOR_LOG_MODULE_ID, SENSOR_LOG_RATE_HZ);
    g_status_log_ok = XM_LOG_REGISTER(StatusLog, STATUS_LOG_MODULE_ID, STATUS_LOG_RATE_HZ);

    /*
     * 2. 등록하지 못했으면 LED 3 을 빠르게 깜빡입니다.
     *    Control_Setup() 에서는 USB 가 아직 준비되지 않아 PC 로 보낸 메시지가 보통 나가지 않습니다.
     *    거절되는 경우는 xm_api_logging.h 의 XM_LOG_REGISTER 설명에 있습니다.
     */
    if (!g_sensor_log_ok || !g_status_log_ok) {
        XM_SetLedEffect(LED_ERROR, XM_LED_BLINK, LED_ERROR_BLINK_MS);
    }
}

/**
 * @brief 1 ms 마다 호출됩니다.
 */
void Control_Loop(void)
{
    /* 1. LED 와 버튼 클릭 수 */
    _UpdateLeds();
    _CountClicks();

    /*
     * 2. 구조체를 채워 보냅니다. 매 루프 불러도 됩니다 — 등록한 주기가 되기 전에는 보내지 않고 false 입니다.
     *    USB 가 준비되지 않았거나 보낼 자리가 잠시 없을 때도 false 이니, 다시 보내려 하지 말고
     *    다음 루프에서 새 값을 보내세요. 구조체는 이 호출 안에서 복사되므로 지역 변수여도 됩니다.
     */
    SensorLog_t sensor;
    _FillSensorLog(&sensor);
    (void)XM_LOG_SEND(SensorLog, &sensor);

    StatusLog_t status;
    _FillStatusLog(&status);
    (void)XM_LOG_SEND(StatusLog, &status);

    /* 3. [관찰용] 카운터를 디버거로 볼 수 있게 복사합니다 */
    XM_Log_GetStats(&g_log_stats);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* LED 1: USB 준비 · LED 2: H10 연결 — 바뀔 때만 다시 씁니다 */
static void _UpdateLeds(void)
{
    const bool usb_ready = XM_IsUsbStreamConnected();
    if (usb_ready != s_usb_led_on) {
        s_usb_led_on = usb_ready;
        XM_SetLedState(LED_USB, usb_ready ? XM_ON : XM_OFF);
    }

#if (USE_H10 == 1)
    const bool h10_ready = XM.status.h10.is_connected;
    if (h10_ready != s_h10_led_on) {
        s_h10_led_on = h10_ready;
        XM_SetLedState(LED_H10, h10_ready ? XM_ON : XM_OFF);
    }
#endif
}

/* 버튼을 짧게 누를 때마다 센다 — 이벤트는 한 번 읽으면 지워집니다 */
static void _CountClicks(void)
{
    for (uint32_t i = 0U; i < BUTTON_COUNT; i++) {
        if (XM_GetButtonEvent(s_buttons[i]) == XM_BTN_CLICK) {
            s_clicks[i]++;
        }
    }
}

/* 구조체 1 — 센서 값을 채웁니다 */
static void _FillSensorLog(SensorLog_t* rec)
{
    const SensorLog_t empty = { 0 };
    const uint32_t now_ms = XM_GetTick();

    *rec = empty;                           /* 빈 자리(pad)까지 0 으로 */
    rec->tick_ms = now_ms;

#if (USE_H10 == 1)
    /* XM.status.h10 은 H10 에서 받은 가장 최근 값입니다. 읽기만 합니다(토크를 내보내지 않습니다) */
    const XmH10Data_t* h10 = &XM.status.h10;

    rec->hip_angle[SIDE_LEFT]   = h10->leftHipAngle;
    rec->hip_angle[SIDE_RIGHT]  = h10->rightHipAngle;
    rec->hip_gyro[SIDE_LEFT]    = h10->leftHipImuGlobalGyrY;
    rec->hip_gyro[SIDE_RIGHT]   = h10->rightHipImuGlobalGyrY;
    rec->hip_torque[SIDE_LEFT]  = h10->leftHipTorque;
    rec->hip_torque[SIDE_RIGHT] = h10->rightHipTorque;
    rec->h10_connected          = h10->is_connected;
#else
    /* 합성 신호 — 사인 · 코사인은 2 초에 한 바퀴, 톱니파는 1 초마다 0 → 100 % */
    const float phase = (float)(now_ms % WAVE_PERIOD_MS) / (float)WAVE_PERIOD_MS;
    rec->wave[WAVE_SIN] = sinf(TWO_PI_F * phase);
    rec->wave[WAVE_COS] = cosf(TWO_PI_F * phase);
    rec->ramp_x10 = (int16_t)(((now_ms % RAMP_PERIOD_MS) * RAMP_FULL_X10) / RAMP_PERIOD_MS);

    /* 확장 ADC 1 ~ 4 */
    for (uint32_t i = 0U; i < EXT_ADC_COUNT; i++) {
        rec->adc_mv[i] = XM_AnalogReadMillivolts(s_adc_pins[i]);
    }

    /* 버튼 — 지금 눌려 있는가. 클릭 이벤트(_CountClicks)와 따로 읽어도 됩니다 */
    for (uint32_t i = 0U; i < BUTTON_COUNT; i++) {
        rec->button[i] = (XM_GetButtonState(s_buttons[i]) == XM_PRESSED);
    }
#endif
}

/* 구조체 2 — 느린 상태를 채웁니다 */
static void _FillStatusLog(StatusLog_t* rec)
{
    const StatusLog_t empty = { 0 };

    *rec = empty;                           /* 빈 자리(pad)까지 0 으로 */
    rec->uptime_s = XM_GetTick() / MS_PER_S;
    for (uint32_t i = 0U; i < BUTTON_COUNT; i++) {
        rec->clicks[i] = s_clicks[i];
    }
}
