/**
 ******************************************************************************
 * @file    usb_memory_log.c
 * @author  HyundoKim
 * @brief   [중급] USB 메모리에 값 저장하기 — 버튼으로 시작 · 정지
 * @details
 * 구조체 하나를 USB 메모리에 100 Hz 로 저장합니다. 저장하는 동안 PC 는 필요 없고, H10 에서 값을 받지 않아도 됩니다.
 * 보드 전원은 KIT H10 본체에서 들어오므로 H10 본체의 전원은 켜 둡니다.
 * 저장한 폴더를 PC 로 옮겨 XM10 Log Decoder(2.2.0 이상)로 풀면 CSV 가 나오고,
 * 빠진 표본이 있으면 그 위치와 길이를 알려 줍니다.
 *
 * [저장하는 값]  루프 카운터 · 합성 사인파 · 합성 톱니파 · 버튼 상태 · H10 연결 여부 (16 B)
 *
 * [쓰는 순서]
 *   1. FAT32 로 포맷한(할당 단위 32 KB 권장) USB 메모리를 꽂습니다. LED 1 이 켜지면 저장할 수 있습니다.
 *   2. 버튼 1 을 짧게 누르면 저장을 시작합니다. LED 2 가 천천히 깜빡이는 동안 저장 중입니다.
 *   3. 버튼 1 을 다시 누르면 멈춥니다. LED 2 가 빠르게 깜빡이는 동안은 파일을 닫는 중이니 기다리세요.
 *      LED 2 가 켜진 채로 있으면 저장이 끝난 것입니다. 이제 USB 메모리를 뽑아도 됩니다.
 *      시작하자마자 바로 멈추면 기록이 하나도 없는 빈 폴더가 남을 수 있습니다.
 *   4. 메모리의 /LOGS/<날짜_시각>/ 폴더를 PC 로 옮겨 디코더로 풉니다.
 *
 * [버튼]
 *   버튼 1 : 저장 시작 / 정지
 *   버튼 2 : 저장 중에 표시(마커)를 넣습니다. 디코더가 표시를 events.csv 로 따로 모아 줍니다.
 *   버튼 3 : 마지막 저장의 통계를 PC 로 보냅니다. USB 메모리를 뽑고 PC 를 연결해 시리얼 터미널을 연 뒤에 누르세요.
 *
 * [LED]
 *   LED 1 켜짐          USB 메모리가 준비되었습니다.
 *   LED 2 느린 깜빡임   저장 중입니다.
 *   LED 2 빠른 깜빡임   파일을 열거나 닫는 중입니다. USB 메모리를 뽑지 마세요.
 *   LED 2 켜짐          저장이 끝났습니다. USB 메모리를 뽑아도 됩니다.
 *   LED 3 켜짐          오류입니다. 시작하지 못했거나 저장이 스스로 끝났습니다. 버튼 1 을 다시 누르면 꺼집니다.
 *   LED 3 짧게 한 번    표시(마커)를 넣지 못했습니다. 저장은 계속됩니다.
 *
 * [데이터 품질]
 *   100 Hz 는 10 ms 마다 구조체 하나를 저장합니다. 그 사이의 값은 저장하지 않아서 loop_count 는 보통 10 씩 늘어납니다.
 *   주기를 1000 Hz 로 올리면 표본이 일부 빠질 수 있습니다. 빠진 수와 가장 긴 공백은 버튼 3 통계와 디코더에 나옵니다.
 *
 * @warning USB-C 포트는 하나입니다. USB 메모리를 꽂아 둔 동안에는 PhAI Studio · xm10 도구 · USB 로 하는 펌웨어 업로드를 쓸 수 없고,
 *          이 예제도 PC 로 아무것도 보내지 않습니다. 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요.
 * @warning 이 예제는 TERMINAL 호스트 프로파일이라 Total Data(PhAI Studio 의 실시간 화면)가 전송되지 않습니다 — 실시간 그래프는 Ex.09 를 보세요.
 * @warning 저장하는 동안(LED 2 가 깜빡이는 동안)에는 USB 메모리를 뽑지 마세요.
 * @warning Rev2.0 보드 전용입니다.
 *
 * @see     docs/api-reference/06-usb-data-logging.md
 * @see     docs/getting-started/05-usb-memory-log.md
 * @see     Extension_Module/Examples/34_USB_Memory_Gait_Log/usb_memory_gait_log.c  (H10 보행 데이터를 1 kHz 로 저장)
 * @version 1.0
 * @date    Oct 14, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

#include <stdio.h>      /* snprintf — 통계를 문장으로 만들 때 */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/*
 * 저장 주기 [Hz] — 1000 의 약수 중에서 고릅니다. 100 Hz 는 10 ms 마다 구조체 하나를 저장합니다.
 *
 * 1000 으로 바꾸면 1 ms 마다 저장합니다. 다만 1000 Hz 는 구조체가 134 B 이하일 때만 시작합니다
 * (타임스탬프를 켠 기본 설정 기준). 구조체가 작을수록 내부 버퍼 여유가 늘어나니 꼭 필요한 값만 담으세요.
 * 1000 Hz 에서는 표본이 일부 빠질 수 있고, 빠진 위치와 길이는 디코더가 알려 줍니다.
 * 200 Hz 이하는 구조체가 512 B 까지 시작됩니다.
 */
#define LOG_SAMPLE_RATE_HZ      (100U)

/*
 * 세션 이름 — 저장 폴더 이름입니다 (/LOGS/<이름>/).
 * "" 이면 시계(RTC)로 YYMMDD_HHMMSS 를 만듭니다. 시계는 Rev2.0 보드에 있고 XM_RTC_SetDateTime() 으로 맞춥니다.
 * 시계를 읽지 못하면 시작하지 못하니(XM_USB_LOG_START_BAD_NAME) 그때는 "walk01" 처럼 이름을 직접 정하세요.
 * 같은 이름의 폴더가 있으면 거기에 이어서 저장합니다.
 */
#define LOG_SESSION_NAME        ""

/* 마커 종류 (1 ~ 254, 뜻은 사용자가 정합니다). 여기서는 1 = 버튼 2 로 넣은 표시 */
#define MARKER_TYPE_BUTTON      (1U)

/* LED 효과 시간 [ms] */
#define BLINK_SLOW_MS           (1000U)     /* 저장 중 */
#define BLINK_FAST_MS           (200U)      /* 파일을 열거나 닫는 중 */
#define ALERT_ON_MS             (300U)      /* 마커를 넣지 못했을 때 LED 3 알림 */

/* 합성 신호 — Control_Loop() 를 부른 횟수(1 회 = 1 ms)로 만듭니다 */
#define SINE_PERIOD_TICKS       (2000U)     /* 사인파 주기 2 초 */
#define RAMP_PERIOD_TICKS       (1000U)     /* 톱니파 주기 1 초 */
#define TWO_PI_F                (6.2831853f)

/* PC 로 보내는 통계 문장의 버퍼 크기 [B] */
#define STATS_MSG_SIZE          (256U)

/* 상태 머신의 세 번째 상태 — 대기(STANDBY) · 저장(ACTIVE) 는 표준 ID, 이것은 사용자 ID 입니다 */
#define STATE_CLOSING           (XM_STATE_USER_START)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB 메모리에 저장할 구조체
 * @details
 * - __attribute__((packed)) 는 필드 사이에 빈 바이트가 끼지 않게 합니다. 디코더는 이 순서대로 바이트를 읽습니다.
 * - 바로 아래 메타 줄(LOG_METADATA)과 필드의 순서 · 타입이 같아야 합니다.
 *   필드를 고치면 메타 줄도 같이 고치세요. 크기가 맞지 않으면 저장을 시작하지 않습니다(METADATA_MISMATCH).
 */
typedef struct __attribute__((packed)) {
    uint32_t loop_count;     /**< Control_Loop() 를 부른 횟수 (1 ms 마다 +1) */
    float    sine;           /**< 합성 사인파 (주기 2 초, 진폭 1) */
    float    ramp;           /**< 합성 톱니파 (0 → 1, 1 초마다 반복) */
    uint8_t  btn1;           /**< 버튼 1 (0 = 뗌, 1 = 누름) */
    uint8_t  btn2;           /**< 버튼 2 */
    uint8_t  btn3;           /**< 버튼 3 */
    uint8_t  h10_connected;  /**< H10 연결 여부 (0 / 1) — XM.status 의 값을 담는 예입니다. H10 이 없으면 0 */
} LogRecord_t;

/* 구조체 크기가 바뀌면 컴파일러가 알려 줍니다. 그때 메타 줄도 같이 고치세요. (4 + 4 + 4 + 1 + 1 + 1 + 1) */
_Static_assert(sizeof(LogRecord_t) == 16U, "LogRecord_t 크기가 바뀌었습니다 - 메타 줄(LOG_METADATA)도 같이 고치세요");

/* 메타 줄 — 구조체의 필드를 선언 순서대로 "이름(타입)" 으로 적고 쉼표로 잇습니다 */
#define LOG_METADATA \
    "loop_count(uint32_t), sine(float), ramp(float), " \
    "btn1(uint8_t), btn2(uint8_t), btn3(uint8_t), h10_connected(uint8_t)"

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES  — 디버거 Live Expressions 로 관찰
 *-----------------------------------------------------------
 */

/* 마지막 시작의 결과 (0 = 시작함, 그 밖에는 시작하지 못한 이유 — XmUsbLogStartError_e). 버튼 3 통계에도 나옵니다 */
volatile XmUsbLogStartError_e g_last_start_error = XM_USB_LOG_START_OK;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;

/* 저장할 구조체 — 전역 또는 static 이어야 합니다 (프로그램이 도는 동안 계속 유효해야 합니다) */
static LogRecord_t   s_log;

static bool          s_error;            /* LED 3 켜짐 — 시작하지 못했거나 저장이 스스로 끝났다 */
static bool          s_saved;            /* LED 2 켜짐 — 저장이 끝나 파일이 모두 닫혔다 */
static bool          s_recording_seen;   /* 이번 시작에서 파일이 열린 적이 있는가 */
static bool          s_ended_by_itself;  /* 정지를 요청하지 않았는데 저장이 끝났다(USB 메모리가 빠짐 · 버퍼 넘침 등) — 파일이 닫혀도 오류로 보여 준다 */
static bool          s_stop_sent;        /* 정지 요청을 받아들였는가 */
static uint16_t      s_marker_count;     /* 이번 저장에서 넣은 마커 수 */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Standby_Entry(void);
static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);

static void Closing_Entry(void);
static void Closing_Loop(void);

static void    _UpdateLogData(void);
static uint8_t _IsPressed(XmBtnId_t btn);
static void    _ShowStickLed(void);
static void    _TryStart(void);
static void    _ShowStartError(XmUsbLogStartError_e reason);
static void    _SendStatsToPc(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 부팅 후 한 번 호출됩니다. 상태 머신을 만들고 저장할 구조체와 주기를 등록합니다.
 */
void Control_Setup(void)
{
    /*
     * 버튼 3 으로 PC 에 텍스트를 보냅니다. 일반 시리얼 터미널에 깨끗하게 보이도록 호스트 프로파일을 TERMINAL 로 지정합니다 (Ex.07 과 같음).
     * 이때는 1 kHz 실시간 데이터(Total Data) 자동 전송이 꺼지므로 PhAI Studio 의 실시간 화면은 나오지 않습니다.
     */
    XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL);

    /* 1. 상태 머신: 대기(STANDBY) → 저장(ACTIVE) → 닫는 중(CLOSING) → 대기 */
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);

    XmStateConfig_t sb_conf = {
        .id       = XM_STATE_STANDBY,
        .on_entry = Standby_Entry,
        .on_loop  = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    XmStateConfig_t cl_conf = {
        .id       = STATE_CLOSING,
        .on_entry = Closing_Entry,
        .on_loop  = Closing_Loop
    };
    XM_TSM_AddState(s_tsm, &cl_conf);

    /*
     * 2. 저장할 구조체와 주기를 등록합니다. 등록은 Control_Setup() 에서 한 번만 합니다.
     *    등록한 뒤에는 기록 함수를 따로 부르지 않습니다. Control_Loop() 에서 s_log 의 값만 채우면
     *    저장하는 동안 정한 주기마다 시스템이 s_log 를 USB 메모리에 기록합니다.
     *    타임스탬프는 기본으로 켜져 있어서 레코드마다 시각이 함께 저장됩니다.
     *    [선택] 파일을 나누는 크기(기본 10 MB)는 XM_SetUsbLogRollingSize() 로 바꿉니다.
     */
    bool ok = XM_SetUsbLogSource(&s_log, sizeof(s_log));
    ok = XM_SetUsbLogSampleRateHz(LOG_SAMPLE_RATE_HZ) && ok;
    if (!ok) {
        s_error = true;         /* 등록하지 못했습니다. LED 3 이 켜집니다 */
    }
}

/**
 * @brief 1 ms 마다 호출됩니다.
 */
void Control_Loop(void)
{
    /* 1. 저장할 값을 채웁니다. 저장은 Control_Loop() 가 끝난 뒤 시스템이 합니다. */
    _UpdateLogData();

    /* 2. 버튼 · LED · 시작 · 정지 */
    XM_TSM_Run(s_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* ====================================================
 * State: STANDBY (대기)
 * - USB 메모리가 준비되기를 기다리고, 버튼 1 로 저장을 시작합니다.
 * ==================================================== */
static void Standby_Entry(void)
{
    /* 직전 저장의 결과를 LED 에 이어서 보여 줍니다 */
    XM_SetLedState(XM_LED_2, s_saved ? XM_ON : XM_OFF);     /* 켜짐 = 저장이 끝났다. 뽑아도 된다 */
    XM_SetLedState(XM_LED_3, s_error ? XM_ON : XM_OFF);     /* 켜짐 = 오류 */
}

static void Standby_Loop(void)
{
    _ShowStickLed();

    /* USB 메모리를 뽑으면 '저장이 끝났다' 표시도 지웁니다 */
    if (s_saved && !XM_IsUsbLogReady()) {
        s_saved = false;
        XM_SetLedState(XM_LED_2, XM_OFF);
    }

    /* 저장 중이 아닐 때 누른 버튼 2 는 버립니다 (쌓여 있다가 시작하자마자 마커가 들어가지 않도록) */
    (void)XM_GetButtonEvent(XM_BTN_2);

    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        _SendStatsToPc();
    }

    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        _TryStart();
    }
}

/* ====================================================
 * State: ACTIVE (저장)
 * - 시작을 요청한 뒤부터 저장이 끝날 때까지입니다.
 *   파일이 열리면 XM_IsUsbLogRecording() 이 true 가 됩니다.
 * ==================================================== */
static void Active_Entry(void)
{
    /* 시작은 기다리지 않고 바로 돌아옵니다. 파일을 여는 동안은 빠르게 깜빡입니다 */
    XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, BLINK_FAST_MS);
}

static void Active_Loop(void)
{
    _ShowStickLed();

    if (XM_IsUsbLogRecording()) {
        /* 파일이 열렸습니다. 천천히 깜빡입니다 */
        if (!s_recording_seen) {
            s_recording_seen = true;
            XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, BLINK_SLOW_MS);
        }
    } else if (XM_IsUsbLogSessionIdle()) {
        /*
         * 저장 중이 아닌데 저장 세션도 없습니다. 정지를 요청하지 않았으니 저장이 스스로 끝난 것입니다.
         *  - 파일이 열리기 전이면: 시작하지 못했습니다. 이유는 XM_GetUsbLogLastStartError() 에 남아 있습니다.
         *  - 파일이 열린 뒤면: USB 메모리가 빠졌거나 내부 버퍼가 넘친 경우 등입니다.
         */
        if (!s_recording_seen) {
            g_last_start_error = XM_GetUsbLogLastStartError();
        }
        s_error = true;
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    } else if (s_recording_seen) {
        /*
         * 저장하던 중이었는데 지금은 저장 중도 아니고 세션도 남아 있습니다. 정지를 요청하지 않았으니 저장이 스스로 끝났고 파일을 닫는 중입니다.
         * 기억해 두었다가 파일이 닫히면 오류로 보여 줍니다 — 그 사이 버튼 1 을 눌러 '닫는 중' 으로 넘어가도 '저장이 끝났다' 로 보이지 않게.
         */
        s_ended_by_itself = true;
    }
    /* 그 밖에는 파일을 여는 중입니다 */

    /* 버튼 2: 저장 중에만 마커를 넣습니다. value 에는 몇 번째 마커인지 담았습니다 */
    if ((XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) && XM_IsUsbLogRecording()) {
        if (XM_InsertUsbLogMarker(MARKER_TYPE_BUTTON, (uint16_t)(s_marker_count + 1U))) {
            s_marker_count++;
        } else {
            XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, ALERT_ON_MS);     /* 마커를 넣지 못했습니다 */
        }
    }

    /* 버튼 1: 정지 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, STATE_CLOSING);
    }
}

/* ====================================================
 * State: CLOSING (닫는 중)
 * - 정지를 요청하고, 파일이 모두 닫히기를 기다립니다. 그동안 USB 메모리를 뽑지 마세요.
 * ==================================================== */
static void Closing_Entry(void)
{
    s_stop_sent = false;
    XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, BLINK_FAST_MS);
}

static void Closing_Loop(void)
{
    _ShowStickLed();

    /* 닫는 동안 누른 버튼 1 · 2 는 버립니다 (닫히자마자 새로 시작되지 않도록) */
    (void)XM_GetButtonEvent(XM_BTN_1);
    (void)XM_GetButtonEvent(XM_BTN_2);

    /* 정지는 한 번만 요청합니다. false(잠깐 바쁨)이면 다음 루프에 다시 부릅니다 */
    if (!s_stop_sent) {
        s_stop_sent = XM_StopUsbDataLog();
    }

    /* 파일이 모두 닫히면 저장 세션이 없습니다. 이제 USB 메모리를 뽑아도 됩니다 */
    if (s_stop_sent && XM_IsUsbLogSessionIdle()) {
        /* 저장이 스스로 끝났거나 닫는 동안 USB 메모리가 빠졌다면 '저장이 끝났다' 가 아니라 오류입니다 */
        if (s_ended_by_itself || !XM_IsUsbLogReady()) {
            s_error = true;
        } else {
            s_saved = true;
        }
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/* ====================================================
 * 보조 함수
 * ==================================================== */

/* 저장할 값을 채웁니다 */
static void _UpdateLogData(void)
{
    s_log.loop_count++;

    /* 합성 신호 — 사인파는 2 초에 한 바퀴, 톱니파는 1 초에 0 → 1 */
    const float sine_phase = (float)(s_log.loop_count % SINE_PERIOD_TICKS) / (float)SINE_PERIOD_TICKS;
    s_log.sine = sinf(TWO_PI_F * sine_phase);
    s_log.ramp = (float)(s_log.loop_count % RAMP_PERIOD_TICKS) / (float)RAMP_PERIOD_TICKS;

    /* 버튼 상태 — 지금 눌려 있는가. 이벤트(XM_GetButtonEvent)와 따로 읽어도 됩니다 */
    s_log.btn1 = _IsPressed(XM_BTN_1);
    s_log.btn2 = _IsPressed(XM_BTN_2);
    s_log.btn3 = _IsPressed(XM_BTN_3);

    /* XM.status 의 값도 담을 수 있습니다. 이 값은 H10 이 없어도 읽을 수 있습니다 (H10 값은 Ex.34 참고) */
    s_log.h10_connected = XM.status.h10.is_connected ? 1U : 0U;
}

static uint8_t _IsPressed(XmBtnId_t btn)
{
    return (XM_GetButtonState(btn) == XM_PRESSED) ? 1U : 0U;
}

/* LED 1: USB 메모리가 준비되었는가 */
static void _ShowStickLed(void)
{
    XM_SetLedState(XM_LED_1, XM_IsUsbLogReady() ? XM_ON : XM_OFF);
}

/*
 * 저장을 시작합니다.
 * 시작은 USB 메모리가 준비되었고(Ready) 저장 세션이 없을 때(SessionIdle)만 부릅니다.
 * TRY_AGAIN 처럼 잠깐 바빠서 거부된 경우는 버튼 1 을 다시 누르면 됩니다.
 */
static void _TryStart(void)
{
    s_error = false;
    XM_SetLedState(XM_LED_3, XM_OFF);

    if (!XM_IsUsbLogReady()) {
        _ShowStartError(XM_USB_LOG_START_NOT_READY);
        return;
    }
    if (!XM_IsUsbLogSessionIdle()) {
        _ShowStartError(XM_USB_LOG_START_BUSY);
        return;
    }

    if (XM_StartUsbDataLog(LOG_SESSION_NAME, LOG_METADATA)) {
        /* 시작 요청을 받아들였습니다. 파일이 열리는 것은 Active_Loop() 가 지켜봅니다 */
        g_last_start_error = XM_USB_LOG_START_OK;
        s_saved            = false;
        s_recording_seen   = false;
        s_ended_by_itself  = false;
        s_marker_count     = 0U;
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    } else {
        /* 시작하지 않았습니다. 이유를 읽어 둡니다 */
        _ShowStartError(XM_GetUsbLogLastStartError());
    }
}

static void _ShowStartError(XmUsbLogStartError_e reason)
{
    g_last_start_error = reason;
    s_error            = true;
    XM_SetLedState(XM_LED_3, XM_ON);
}

/*
 * 마지막 저장의 통계를 PC 로 보냅니다.
 * USB 메모리를 꽂은 동안에는 PC 가 연결되지 않으므로, PC 가 연결된 때만 보냅니다.
 * 한 줄 안의 이름은 XmUsbLogStats_t 의 멤버 이름과 같습니다 (start_error 만 이 예제의 g_last_start_error 입니다).
 *   empty_slots       빠진 표본 수 (0 이면 빠진 것이 없습니다)
 *   longest_gap_ms    가장 긴 공백 [ms]
 *   overflow_records  내부 버퍼가 넘쳐 저장하지 못한 레코드 수 (1 이상이면 그 저장은 거기서 멈춥니다)
 *   start_error       마지막 시작의 결과 (XmUsbLogStartError_e 의 값, 0 = 시작함)
 */
static void _SendStatsToPc(void)
{
    if (!XM_IsUsbStreamConnected()) {
        return;
    }

    XmUsbLogStats_t st;
    if (!XM_GetUsbLogStats(&st)) {
        return;
    }

    char msg[STATS_MSG_SIZE];
    snprintf(msg, sizeof(msg),
             "[USB Log] records=%lu bytes=%lu duration_ms=%lu empty_slots=%lu longest_gap_ms=%lu "
             "overflow_records=%lu write_errors=%lu buffer_peak_percent=%u start_error=%d\r\n",
             (unsigned long)st.records,
             (unsigned long)st.bytes,
             (unsigned long)st.duration_ms,
             (unsigned long)st.empty_slots,
             (unsigned long)st.longest_gap_ms,
             (unsigned long)st.overflow_records,
             (unsigned long)st.write_errors,
             (unsigned)st.buffer_peak_percent,
             (int)g_last_start_error);
    XM_SendUsbDebugMessage(msg);
}
