/**
 ******************************************************************************
 * @file    usb_memory_gait_log.c
 * @author  HyundoKim
 * @brief   [중급] H10 보행 데이터를 USB 메모리에 1 kHz 로 저장하기
 * @details
 * H10 이 보내는 보행 데이터를 1 ms 마다 구조체 하나(96 B)로 USB 메모리에 저장합니다.
 * 저장한 폴더를 PC 로 옮겨 XM10 Log Decoder(2.2.0 이상)로 풀면 CSV 가 나오고,
 * 빠진 표본이 있으면 그 위치와 길이를 알려 줍니다. 처음이라면 Ex.10 을 먼저 해 보세요.
 *
 * [저장하는 값]
 *   h10_loop_count    H10 이 세는 루프 카운터 (디코더가 시간축으로 씁니다)
 *   hip_angle         고관절 각도, 왼쪽 · 오른쪽 [deg]
 *   thigh_angle       허벅지 각도, 왼쪽 · 오른쪽 [deg]
 *   knee_angle        무릎 각도(추정), 왼쪽 · 오른쪽 [deg]
 *   pelvic_angle      골반 각도 [deg]
 *   forward_velocity  전방 보행 속도 [m/s]
 *   hip_torque        관절 토크 추정값, 왼쪽 · 오른쪽 [Nm]
 *   imu_acc / imu_gyr 좌우 고관절 IMU 의 가속도 [m/s^2] 와 각속도 [deg/s] (x, y, z)
 *   foot_contact      발 접지, 왼쪽 · 오른쪽 (0 = 공중, 1 = 접지)
 *   fsm_state         H10 의 FSM 상태
 *   h10_connected     H10 연결 여부
 *
 * [시간축]
 *   첫 필드 h10_loop_count 는 H10 의 1 kHz 루프 카운터입니다. 카운터가 올라가고 있으면 디코더가 이 값으로
 *   1 kHz 시간축을 만들고, 올라가지 않으면(H10 이 연결되지 않았을 때 등) 레코드마다 함께 저장되는 시각으로 만듭니다.
 *   그래서 H10 이 없어도 저장은 진행됩니다. 이때 H10 값은 연결된 적이 없으면 0 으로 남습니다.
 *
 * [H10 상태]
 *   XM10 이 H10 에 연결되면 H10 의 기존 보조 알고리즘은 기본으로 꺼집니다(XM_SetH10AssistExistingMode() 의 기본값).
 *   이 예제도 기본(USE_H10_ASSIST 0)은 그 상태 그대로 기록만 하므로 착용자는 H10 의 보조를 받지 않습니다.
 *   H10 이 보조(Assist) 상태가 아니면 h10_loop_count 가 올라가지 않을 수 있습니다(그러면 디코더는 레코드마다 함께 저장되는 시각으로 시간축을 만듭니다).
 *   보조를 받으며 걷는 데이터를 저장하려면 USE_H10_ASSIST 를 1 로 바꾸세요. Control_Setup() 에서 XM_SetH10AssistExistingMode(true) 를 부릅니다.
 *   그러면 착용자에게 H10 자체 알고리즘의 보조 토크가 걸리니, 착용자와 주변의 안전을 확인한 뒤에만 켜세요.
 *
 * [쓰는 순서]
 *   0. 빌드하기 전에 _SetupBodyData() 의 신체 정보(체중 · 키 · 다리 길이)를 착용자의 값으로 고칩니다.
 *   1. FAT32 로 포맷한(할당 단위 32 KB 권장) USB 메모리를 꽂습니다. LED 1 이 켜지면 저장할 수 있습니다.
 *      H10 을 연결하고 착용자가 입은 뒤에 시작하세요(H10 이 없으면 H10 값은 0 으로 저장됩니다).
 *   2. 버튼 1 을 짧게 누르면 저장을 시작합니다. LED 2 가 천천히 깜빡이는 동안 저장 중입니다.
 *   3. 버튼 2 · 3 으로 원하는 순간에 표시(마커)를 넣습니다.
 *   4. 버튼 1 을 다시 누르면 멈춥니다. LED 2 가 빠르게 깜빡이는 동안은 파일을 닫는 중이니 기다리세요.
 *      LED 2 가 켜진 채로 있으면 저장이 끝난 것입니다. 이제 USB 메모리를 뽑아도 됩니다.
 *      시작하자마자 바로 멈추면 기록이 하나도 없는 빈 폴더가 남을 수 있습니다.
 *   5. 메모리의 /LOGS/<날짜_시각>/ 폴더를 PC 로 옮겨 디코더로 풉니다.
 *
 * [버튼]
 *   버튼 1 : 저장 시작 / 정지
 *   버튼 2 : 마커 1 — 걷기 시작 (예: 출발하는 순간)
 *   버튼 3 : 마커 2 — 이벤트 (예: 표시하고 싶은 순간)
 *   마커는 디코더가 events.csv 로 따로 모아 줍니다.
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
 *   1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다. 빠진 위치와 길이는 디코더가 알려 줍니다.
 *   무릎 각도 · 허벅지 각도 · 발 접지는 H10 이 착용자의 신체 정보로 계산한 값입니다. 이 예제는 Control_Setup() 에서
 *   _SetupBodyData() 로 신체 정보를 보냅니다(Ex.22 · Ex.23 과 같은 방식). 들어 있는 값은 예시이니 착용자의 값으로 고치세요.
 *   신체 정보가 맞지 않으면 이 값들이 부정확하고, 보내지 않으면 발 접지가 항상 0 으로 남을 수 있습니다.
 *
 * @warning 기본(USE_H10_ASSIST 0)은 기록만 합니다. H10 에 보내는 것은 신체 정보뿐이고, XM_SetAssistTorque() · XM_SetControlMode() 같은
 *          출력 함수는 부르지 않으므로 토크나 제어 명령을 보내지 않습니다.
 * @warning USE_H10_ASSIST 를 1 로 바꾸면 H10 의 기존 보조 알고리즘이 켜져 착용자에게 보조 토크가 걸립니다.
 *          착용자와 주변의 안전을 확인한 뒤에만 켜세요.
 * @warning USB-C 포트는 하나입니다. USB 메모리를 꽂아 둔 동안에는 PhAI Studio · xm10 도구 · USB 로 하는 펌웨어 업로드를 쓸 수 없고,
 *          이 예제도 PC 로 아무것도 보내지 않습니다. 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요.
 * @warning 저장하는 동안(LED 2 가 깜빡이는 동안)에는 USB 메모리를 뽑지 마세요.
 * @warning Rev2.0 보드 전용입니다.
 *
 * @see     docs/api-reference/06-usb-data-logging.md
 * @see     docs/getting-started/05-usb-memory-log.md
 * @see     Extension_Module/Examples/10_USB_Memory_Log/usb_memory_log.c  (보드만으로 해 보는 기본 예제)
 * @version 1.0
 * @date    Oct 14, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/*
 * 저장 주기 [Hz] — 1000 의 약수 중에서 고릅니다. 1000 Hz 는 1 ms 마다 구조체 하나를 저장합니다.
 *
 * 1000 Hz 는 구조체가 134 B 이하일 때만 시작합니다 (타임스탬프를 켠 기본 설정 기준).
 * 이 구조체는 96 B 입니다. 필드를 더 담을 때도 작게 유지하세요. 구조체가 작을수록 내부 버퍼 여유가 늘어납니다.
 * 더 느린 주기가 필요하면 100 처럼 바꿉니다. 200 Hz 이하는 구조체가 512 B 까지 시작됩니다.
 */
#define LOG_SAMPLE_RATE_HZ      (1000U)

/*
 * 세션 이름 — 저장 폴더 이름입니다 (/LOGS/<이름>/).
 * "" 이면 시계(RTC)로 YYMMDD_HHMMSS 를 만듭니다. 시계는 Rev2.0 보드에 있고 XM_RTC_SetDateTime() 으로 맞춥니다.
 * 시계를 읽지 못하면 시작하지 못하니(XM_USB_LOG_START_BAD_NAME) 그때는 "walk01" 처럼 이름을 직접 정하세요.
 * 같은 이름의 폴더가 있으면 거기에 이어서 저장합니다.
 */
#define LOG_SESSION_NAME        ""

/*
 * H10 의 기존 보조 알고리즘 — 0: 끔(기본, 기록만) · 1: 켬
 * 1 이면 Control_Setup() 에서 XM_SetH10AssistExistingMode(true) 를 불러 H10 자체 알고리즘의 보조를 켭니다.
 * 착용자에게 보조 토크가 걸립니다. 착용자와 주변의 안전을 확인한 뒤에만 1 로 바꾸세요.
 */
#define USE_H10_ASSIST          (0)

/* 마커 종류 (1 ~ 254, 뜻은 사용자가 정합니다) */
#define MARKER_WALK_START       (1U)        /* 버튼 2 — 걷기 시작 */
#define MARKER_EVENT            (2U)        /* 버튼 3 — 이벤트 */

/* LED 효과 시간 [ms] */
#define BLINK_SLOW_MS           (1000U)     /* 저장 중 */
#define BLINK_FAST_MS           (200U)      /* 파일을 열거나 닫는 중 */
#define ALERT_ON_MS             (300U)      /* 마커를 넣지 못했을 때 LED 3 알림 */

/* 상태 머신의 세 번째 상태 — 대기(STANDBY) · 저장(ACTIVE) 는 표준 ID, 이것은 사용자 ID 입니다 */
#define STATE_CLOSING           (XM_STATE_USER_START)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB 메모리에 저장할 구조체 (96 B)
 * @details
 * - __attribute__((packed)) 는 필드 사이에 빈 바이트가 끼지 않게 합니다. 디코더는 이 순서대로 바이트를 읽습니다.
 * - 바로 아래 메타 줄(LOG_METADATA)과 필드의 순서 · 타입이 같아야 합니다.
 *   필드를 고치면 메타 줄도 같이 고치세요. 크기가 맞지 않으면 저장을 시작하지 않습니다(METADATA_MISMATCH).
 * - 첫 필드 h10_loop_count 의 이름을 바꾸지 마세요. 디코더가 이 이름의 열을 시간축으로 씁니다.
 */
typedef struct __attribute__((packed)) {
    uint32_t h10_loop_count;    /**< H10 루프 카운터 (XM.status.h10.h10AssistModeLoopCnt) */

    /* --- 각도 [deg] --- */
    float    hip_angle_L;       /**< 왼쪽 고관절 */
    float    hip_angle_R;       /**< 오른쪽 고관절 */
    float    thigh_angle_L;     /**< 왼쪽 허벅지 */
    float    thigh_angle_R;     /**< 오른쪽 허벅지 */
    float    knee_angle_L;      /**< 왼쪽 무릎 (추정) */
    float    knee_angle_R;      /**< 오른쪽 무릎 (추정) */
    float    pelvic_angle;      /**< 골반 */

    /* --- 보행 --- */
    float    forward_velocity;  /**< 전방 보행 속도 [m/s] */
    float    hip_torque_L;      /**< 왼쪽 관절 토크 추정값 [Nm] */
    float    hip_torque_R;      /**< 오른쪽 관절 토크 추정값 [Nm] */

    /* --- 좌우 고관절 IMU --- */
    float    imu_acc_L_x;       /**< 왼쪽 가속도 [m/s^2] */
    float    imu_acc_L_y;
    float    imu_acc_L_z;
    float    imu_acc_R_x;       /**< 오른쪽 가속도 [m/s^2] */
    float    imu_acc_R_y;
    float    imu_acc_R_z;
    float    imu_gyr_L_x;       /**< 왼쪽 각속도 [deg/s] */
    float    imu_gyr_L_y;
    float    imu_gyr_L_z;
    float    imu_gyr_R_x;       /**< 오른쪽 각속도 [deg/s] */
    float    imu_gyr_R_y;
    float    imu_gyr_R_z;

    /* --- 상태 --- */
    uint8_t  foot_contact_L;    /**< 왼발 접지 (0 = 공중, 1 = 접지) */
    uint8_t  foot_contact_R;    /**< 오른발 접지 */
    uint8_t  fsm_state;         /**< H10 의 FSM 상태 */
    uint8_t  h10_connected;     /**< H10 연결 여부 (0 이면 H10 값을 믿지 마세요. 중간에 끊기면 마지막 값이 남아 있을 수 있습니다) */
} GaitLogRecord_t;

/* 구조체 크기가 바뀌면 컴파일러가 알려 줍니다. 그때 메타 줄도 같이 고치세요. (4 + 4 × 22 + 4) */
_Static_assert(sizeof(GaitLogRecord_t) == 96U, "GaitLogRecord_t 크기가 바뀌었습니다 - 메타 줄(LOG_METADATA)도 같이 고치세요");

/* 메타 줄 — 구조체의 필드를 선언 순서대로 "이름(타입)" 으로 적고 쉼표로 잇습니다 */
#define LOG_METADATA \
    "h10_loop_count(uint32_t), " \
    "hip_angle_L(float), hip_angle_R(float), " \
    "thigh_angle_L(float), thigh_angle_R(float), " \
    "knee_angle_L(float), knee_angle_R(float), " \
    "pelvic_angle(float), " \
    "forward_velocity(float), hip_torque_L(float), hip_torque_R(float), " \
    "imu_acc_L_x(float), imu_acc_L_y(float), imu_acc_L_z(float), " \
    "imu_acc_R_x(float), imu_acc_R_y(float), imu_acc_R_z(float), " \
    "imu_gyr_L_x(float), imu_gyr_L_y(float), imu_gyr_L_z(float), " \
    "imu_gyr_R_x(float), imu_gyr_R_y(float), imu_gyr_R_z(float), " \
    "foot_contact_L(uint8_t), foot_contact_R(uint8_t), " \
    "fsm_state(uint8_t), h10_connected(uint8_t)"

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES  — 디버거 Live Expressions 로 관찰
 *-----------------------------------------------------------
 */

/* 마지막 시작의 결과 (0 = 시작함, 그 밖에는 시작하지 못한 이유 — XmUsbLogStartError_e). LED 3 이 켜지면 이 값을 보세요 */
volatile XmUsbLogStartError_e g_last_start_error = XM_USB_LOG_START_OK;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;

/* 저장할 구조체 — 전역 또는 static 이어야 합니다 (프로그램이 도는 동안 계속 유효해야 합니다) */
static GaitLogRecord_t s_log;

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

static void _SetupBodyData(void);
static void _UpdateLogData(void);
static void _ShowStickLed(void);
static void _TryStart(void);
static void _ShowStartError(XmUsbLogStartError_e reason);
static void _HandleMarkerButton(XmBtnId_t btn, uint8_t type);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 부팅 후 한 번 호출됩니다. 상태 머신을 만들고 저장할 구조체와 주기를 등록한 뒤 신체 정보를 H10 에 보냅니다.
 * @details 기본은 기록 전용입니다. 토크 · 제어 모드 · 벡터 명령은 부르지 않습니다.
 *          USE_H10_ASSIST 가 1 이면 H10 의 기존 보조 알고리즘을 켭니다(착용자에게 보조 토크가 걸립니다).
 */
void Control_Setup(void)
{
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
     *    주기를 정하지 않으면 1000 Hz 입니다. 여기서는 값이 보이도록 적어 두었습니다.
     *    타임스탬프는 기본으로 켜져 있어서 레코드마다 시각이 함께 저장됩니다.
     *    [선택] 파일을 나누는 크기(기본 10 MB)는 XM_SetUsbLogRollingSize() 로 바꿉니다.
     */
    bool ok = XM_SetUsbLogSource(&s_log, sizeof(s_log));
    ok = XM_SetUsbLogSampleRateHz(LOG_SAMPLE_RATE_HZ) && ok;
    if (!ok) {
        s_error = true;         /* 등록하지 못했습니다. LED 3 이 켜집니다 */
    }

    /*
     * 3. 착용자의 신체 정보를 H10 에 보냅니다. H10 이 무릎 · 허벅지 각도와 발 접지를 추정할 때 씁니다.
     *    토크를 내보내는 명령이 아닙니다. 값은 _SetupBodyData() 에서 착용자의 값으로 고치세요.
     */
    _SetupBodyData();

#if (USE_H10_ASSIST == 1)
    /* 4. [선택] H10 의 기존 보조 알고리즘을 켭니다. 착용자에게 보조 토크가 걸립니다(USE_H10_ASSIST 설명 참고) */
    XM_SetH10AssistExistingMode(true);
#endif
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

    /* 저장 중이 아닐 때 누른 버튼 2 · 3 은 버립니다 (쌓여 있다가 시작하자마자 마커가 들어가지 않도록) */
    (void)XM_GetButtonEvent(XM_BTN_2);
    (void)XM_GetButtonEvent(XM_BTN_3);

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

    /* 버튼 2 · 3: 저장 중에만 마커를 넣습니다 */
    _HandleMarkerButton(XM_BTN_2, MARKER_WALK_START);
    _HandleMarkerButton(XM_BTN_3, MARKER_EVENT);

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

    /* 닫는 동안 누른 버튼은 버립니다 (닫히자마자 새로 시작되지 않도록) */
    (void)XM_GetButtonEvent(XM_BTN_1);
    (void)XM_GetButtonEvent(XM_BTN_2);
    (void)XM_GetButtonEvent(XM_BTN_3);

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

/* 착용자의 신체 정보 — 아래 값은 예시입니다. 착용자를 직접 잰 값으로 고치세요(Ex.22 · Ex.23 과 같은 순서 · 단위) */
static void _SetupBodyData(void)
{
    uint32_t body_data[8] = {
        70000U,  /* 체중 (g): 70 kg */
        1750U,   /* 신장 (mm): 175 cm */
        450U,    /* 오른쪽 대퇴 길이 (mm) */
        450U,    /* 왼쪽 대퇴 길이 (mm) */
        420U,    /* 오른쪽 하퇴 길이 (mm) */
        420U,    /* 왼쪽 하퇴 길이 (mm) */
        60U,     /* 오른쪽 발목 높이 (mm) */
        60U,     /* 왼쪽 발목 높이 (mm) */
    };
    XM_SendUserBodyData(body_data);
}

/* 저장할 값을 채웁니다. XM.status.h10 은 루프가 시작될 때 H10 에서 받은 가장 최근 값입니다(같은 값이 두 번 보이거나 한 번 건너뛸 수 있습니다) */
static void _UpdateLogData(void)
{
    const XmH10Data_t* h10 = &XM.status.h10;

    /* 시간축 — H10 의 루프 카운터. 디코더는 카운터가 올라가는 동안 이 값으로 1 kHz 시간축을 만듭니다 */
    s_log.h10_loop_count = h10->h10AssistModeLoopCnt;

    /* 각도 */
    s_log.hip_angle_L    = h10->leftHipAngle;
    s_log.hip_angle_R    = h10->rightHipAngle;
    s_log.thigh_angle_L  = h10->leftThighAngle;
    s_log.thigh_angle_R  = h10->rightThighAngle;
    s_log.knee_angle_L   = h10->leftKneeAngle;
    s_log.knee_angle_R   = h10->rightKneeAngle;
    s_log.pelvic_angle   = h10->pelvicAngle;

    /* 보행 속도 · 관절 토크 추정값 (읽기만 합니다. 토크를 내보내지 않습니다) */
    s_log.forward_velocity = h10->forwardVelocity;
    s_log.hip_torque_L     = h10->leftHipTorque;
    s_log.hip_torque_R     = h10->rightHipTorque;

    /* 좌우 고관절 IMU */
    s_log.imu_acc_L_x = h10->leftHipImuGlobalAccX;
    s_log.imu_acc_L_y = h10->leftHipImuGlobalAccY;
    s_log.imu_acc_L_z = h10->leftHipImuGlobalAccZ;
    s_log.imu_acc_R_x = h10->rightHipImuGlobalAccX;
    s_log.imu_acc_R_y = h10->rightHipImuGlobalAccY;
    s_log.imu_acc_R_z = h10->rightHipImuGlobalAccZ;
    s_log.imu_gyr_L_x = h10->leftHipImuGlobalGyrX;
    s_log.imu_gyr_L_y = h10->leftHipImuGlobalGyrY;
    s_log.imu_gyr_L_z = h10->leftHipImuGlobalGyrZ;
    s_log.imu_gyr_R_x = h10->rightHipImuGlobalGyrX;
    s_log.imu_gyr_R_y = h10->rightHipImuGlobalGyrY;
    s_log.imu_gyr_R_z = h10->rightHipImuGlobalGyrZ;

    /* 상태 */
    s_log.foot_contact_L = h10->isLeftFootContact  ? 1U : 0U;
    s_log.foot_contact_R = h10->isRightFootContact ? 1U : 0U;
    s_log.fsm_state      = h10->h10FSMcurrentState;
    s_log.h10_connected  = h10->is_connected ? 1U : 0U;
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

/* 버튼을 짧게 누르면 저장 중에만 마커를 넣습니다. value 에는 몇 번째 마커인지 담았습니다 */
static void _HandleMarkerButton(XmBtnId_t btn, uint8_t type)
{
    if ((XM_GetButtonEvent(btn) == XM_BTN_CLICK) && XM_IsUsbLogRecording()) {
        if (XM_InsertUsbLogMarker(type, (uint16_t)(s_marker_count + 1U))) {
            s_marker_count++;
        } else {
            XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, ALERT_ON_MS);     /* 마커를 넣지 못했습니다 */
        }
    }
}
