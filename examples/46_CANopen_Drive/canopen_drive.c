/**
 ******************************************************************************
 * @file    canopen_drive.c
 * @author  HyundoKim
 * @brief   [고급] CANopen 모터 드라이버를 SDO 로 설정하고 명령하기 — PC 시뮬레이터용 (클래식 CAN 1 Mbit/s)
 * @version 1.0
 * @date    Oct 29, 2026
 *
 * @details
 * CAN-FD #2 포트를 클래식 CAN 1 Mbit/s 로 정하고(XM_CanBusMode), CiA 402 속도 모드 드라이버 하나를 SDO 로
 * 설정 · 켜기 · 100 Hz 속도 명령 · 정지합니다. 1 kHz 제어가 필요하면 PDO 를 쓰세요(XM_CanSendFrame ·
 * XM_CanSubscribe — 이 예제는 다루지 않습니다).
 *
 *  ⚠ 이 예제는 PC 시뮬레이터(드라이버 없이 PC 에서 CANopen 드라이버를 흉내 내는 프로그램)가 상대입니다.
 *    ② 에서 시뮬레이터 표시 객체(EX46_SIM_ID)를 확인하지 못하면 켜지 않고 멈춥니다.
 *    실제 드라이버 · 모터로 돌리려면 먼저 드라이버 설명서로 정지 방법 · 하트비트 소비 시간 · 통신 상실 반응을
 *    정하고, 속도는 낮게, 버튼 정지를 확인한 뒤에 이 확인을 바꾸세요.
 *
 *  [순서]  버튼 1 = 시작 / 정지
 *   ① 시작 판정 — 첫 하트비트로 드라이버 상태를 보고 나눕니다. 부팅 알림(0x00)은 드라이버가 켜질 때 한 번뿐이라
 *      XM10 만 다시 켜졌으면 오지 않습니다.
 *        0x00 · 0x7F         → ②
 *        0x04 (STOPPED)      → NMT PRE_OPERATIONAL → 2 초 안에 0x7F 확인 → ②   (STOPPED 는 SDO 에 답하지 않습니다)
 *        0x05 (OPERATIONAL)  → 정지 쓰기(0x6040 = 0x06) → NMT PRE_OPERATIONAL → 0x7F 확인 → ②
 *        3 초 동안 없음       → 그 노드에 NMT 통신 재설정 한 번 → 부팅 알림 수가 2 초 안에 늘면 ②, 아니면 '장치 없음'
 *                              (통신 재설정은 통신 설정을 기본값으로 돌립니다 — 그래서 ② 가 통신 설정을 다시 씁니다)
 *   ② 설정 — SDO: 시뮬레이터 표시 확인 · 0x1000 읽기(장치 종류) · 0x1017 하트비트 송신 주기 100 ms · (XM10 하트비트 시작)
 *      · 0x1016:01 하트비트 소비(XM10 번호 · 시간) · 0x6060 = 3(프로파일 속도). 드라이버의 소비 감시가 켜진 것을
 *      확인한 뒤에만 ③ 으로 갑니다 — 소비 감시는 설정 뒤 첫 하트비트를 받아야 시작합니다. 이 예제는 시뮬레이터 객체
 *      EX46_SIM_HB_CONSUMER_STATE 가 2 가 될 때까지(1 초 안) 읽습니다. 실제 드라이버에서 확인하는 방법은
 *      드라이버마다 다릅니다(설명서). ② 에 들어온 뒤 드라이버가 다시 켜지면(부팅 알림 수가 늘면) 쓴 설정과 소비 감시가
 *      사라지므로, 확인 결과를 받았더라도 통신 이상으로 멈춥니다.
 *   ③ 켜기 — NMT START → 하트비트 0x05 확인 → 0x6041 이 고장이면 0x6040 = 0x00 → 0x80(고장 해제는 bit 7 이 0 에서 1 로
 *      바뀌는 순간입니다 — 원인 0x603F 는 보여 주기만) → 0x6040 = 0x06 → 0x0F, 각각 0x6041 로 확인
 *   ④ 100 Hz — 0x60FF 목표 속도 쓰기 · 0x606C 실제 속도 읽기(SDO 는 한 번에 하나)
 *   ⑤ 정지 — 버튼: 0x6040 = 0x06. 통신 이상(② 부터: 드라이버가 다시 켜짐, ③ 부터: 하트비트 LOST · OPERATIONAL 이 아님,
 *      언제나: SDO 시간 초과 · 중단 · 버스 복구 중 — XM10 하트비트를 보내다 알게 된 것도)이면
 *      (a) 명령을 멈추고 (b) 정지 쓰기를 한 번 시도해 결과만 보여 주고(닿았다고 가정하지 않습니다)
 *      (c) XM10 하트비트를 멈춥니다 — 드라이버의 하트비트 소비 감시가 XM10 을 잃은 것을 보고 스스로 멈추게 하는 것이
 *      안전 정지의 핵심입니다(소비 시간 · 상실 반응은 드라이버에서 정합니다).
 *   ⑥ 통신이 돌아와도 다시 움직이지 않습니다 — 버튼 1 로 ① 부터.
 *
 *  [확인 방법]
 *      LED1  느린 점멸 = 버튼 대기 · 빠른 점멸 = ①~③ 진행 · 켜짐 = ④ 동작 · 아주 빠른 점멸 = 정지하는 중
 *            · 꺼짐 = 버튼으로 정지 · 두근 = 멈춤(이유는 PC 문장)
 *      LED2  켜짐 = XM10 하트비트를 내는 중
 *      PC    xm10 도구 — 0xF0 채널에 단계 · 목표 속도 · 실제 속도 · 0x6041, 단계가 바뀔 때마다 "[Ex46] ..." 문장
 *
 * @warning **본 예제는 XM10 Rev 2.0 전용입니다.** 클래식 모드에서는 이 포트에 CAN-FD 장치를 함께 연결하지 마세요.
 * @see     xm_api_canopen.h · xm_api_canbus.h
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <stdio.h>   /* snprintf — 단계 문장 */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* ===== 설정 — 드라이버(또는 PC 시뮬레이터)와 맞추세요 ===== */
#define EX46_NODE_ID                (1U)        /**< 드라이버 노드 ID(1~127) */
#define EX46_XM_NODE_ID             (0x7FU)     /**< XM10 하트비트 번호 — 드라이버 노드 ID 와 겹치지 않게 */
#define EX46_TARGET_VELOCITY        (100)       /**< 목표 속도(드라이버 단위) — 낮게 시작하세요 */
#define EX46_HB_PRODUCER_MS         (100U)      /**< 드라이버 하트비트 송신 주기(0x1017) — LOST 판정의 전제 */
#define EX46_WATCH_TIMEOUT_MS       (300U)      /**< 드라이버 하트비트가 이만큼 없으면 LOST */
#define EX46_XM_HB_PERIOD_MS        (100U)      /**< XM10 하트비트 주기 */
#define EX46_CONSUMER_TIME_MS       (300U)      /**< 드라이버가 XM10 하트비트를 기다리는 시간(0x1016:01) — 실제 드라이버 값은 직접 정하세요 */

/* ===== PC 시뮬레이터 전용 객체(제조사 영역) ===== */
#define EX46_SIM_ID                 (0x2F00U)        /**< :00 이 EX46_SIM_ID_VALUE 면 PC 시뮬레이터 */
#define EX46_SIM_ID_VALUE           (0x4D495358UL)   /**< 바이트로 "XSIM" */
#define EX46_SIM_HB_CONSUMER_STATE  (0x2F01U)        /**< :00 = 0 미설정 · 1 첫 하트비트 전 · 2 감시 중 */
#define EX46_SIM_CONSUMER_ACTIVE    (2U)

/* ===== 시간 ===== */
#define EX46_FIRST_HB_WAIT_MS       (3000U)     /**< ① 첫 하트비트 */
#define EX46_BOOTUP_WAIT_MS         (2000U)     /**< ① 통신 재설정 뒤 부팅 알림 */
#define EX46_NMT_STATE_WAIT_MS      (2000U)     /**< ① · ③ NMT 상태 확인 */
#define EX46_CONSUMER_WAIT_MS       (1000U)     /**< ② 소비 감시 확인 */
#define EX46_DRIVE_STATE_WAIT_MS    (500U)      /**< ③ 0x6041 확인 */
#define EX46_SEND_LIMIT_MS          (3000U)     /**< 보내기가 BUSY 로 이만큼 막히면 실패로 봅니다(시간 초과 뒤 쉬는 시간보다 김) */
#define EX46_POLL_MS                (20U)       /**< 상태를 다시 읽는 간격 */
#define EX46_CMD_PERIOD_MS          (10U)       /**< ④ 100 Hz */
#define EX46_STREAM_PERIOD_MS       (20U)       /**< PC 그래프 50 Hz */

/* ===== CiA 301 · 402 객체 ===== */
#define OD_DEVICE_TYPE              (0x1000U)
#define OD_HB_CONSUMER              (0x1016U)   /**< :01 = (하트비트를 보내는 노드 ID << 16) | 시간 ms */
#define OD_HB_PRODUCER              (0x1017U)
#define OD_ERROR_CODE               (0x603FU)
#define OD_CONTROLWORD              (0x6040U)
#define OD_STATUSWORD               (0x6041U)
#define OD_MODES_OF_OPERATION       (0x6060U)
#define OD_VELOCITY_ACTUAL          (0x606CU)
#define OD_TARGET_VELOCITY          (0x60FFU)

#define MODE_PROFILE_VELOCITY       (3U)
#define CW_DISABLE_VOLTAGE          (0x0000U)   /**< 고장 해제 앞에 bit 7 을 내립니다 */
#define CW_SHUTDOWN                 (0x0006U)
#define CW_ENABLE_OPERATION         (0x000FU)
#define CW_FAULT_RESET              (0x0080U)
#define SW_FAULT_MASK               (0x004FU)
#define SW_FAULT                    (0x0008U)
#define SW_FAULT_REACTION           (0x000FU)
#define SW_STATE_MASK               (0x006FU)
#define SW_READY_TO_SWITCH_ON       (0x0021U)
#define SW_OPERATION_ENABLED        (0x0027U)

#define HB_ID_BASE                  (0x700U)
#define HB_STATE_OPERATIONAL        (0x05U)
#define USB_MODULE_ID               (0xF0U)
#define USB_CH_COUNT                (4U)
#define REPORT_MSG_SIZE             (256U)
#define MAX_FAULT_RESETS            (1U)

#define LED_BLINK_IDLE_MS           (1000U)
#define LED_BLINK_BUSY_MS           (200U)
#define LED_BLINK_STOP_MS           (100U)
#define LED_HEARTBEAT_MS            (1000U)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef enum {
    EX_IDLE = 0,        /**< 버튼 1 을 기다림 */
    EX_FIRST_HB,        /**< ① 첫 하트비트 */
    EX_RESET_WAIT,      /**< ① 통신 재설정 뒤 부팅 알림 */
    EX_TO_PREOP,        /**< ① PRE_OPERATIONAL 로 */
    EX_OPER_STOP,       /**< ① OPERATIONAL 이던 드라이버에 정지 쓰기 */
    EX_CONFIG,          /**< ② 설정 */
    EX_CONSUMER,        /**< ② 소비 감시 확인 */
    EX_START_NODE,      /**< ③ NMT START */
    EX_ENABLE,          /**< ③ 켜기 */
    EX_RUN,             /**< ④ 100 Hz 명령 */
    EX_STOPPING,        /**< ⑤ 정지 쓰기(버튼 · 켜기 실패) */
    EX_COMM_LOST,       /**< ⑤ 통신 이상 — 정지 쓰기 한 번 */
    EX_HALTED,          /**< 멈춤 — 버튼 1 로 ① 부터 */
} ExState_t;

typedef enum {
    R_NONE = 0,
    R_BUTTON,           /**< 버튼으로 정지 */
    R_NO_RESPONSE,      /**< 응답 없음 */
    R_NO_DEVICE,        /**< 장치 없음 */
    R_NOT_SIM,          /**< PC 시뮬레이터가 아님 — 켜지 않음 */
    R_FAULT,            /**< 고장이 풀리지 않음 */
    R_NOT_ENABLED,      /**< 켜지지 않음 */
    R_COMM,             /**< 통신 이상 */
    R_PORT,             /**< 포트를 클래식 모드로 정하지 못함 */
    R_WATCH,            /**< 하트비트 감시 등록 실패 */
} ExReason_t;

typedef enum { EX_SDO_WAIT = 0, EX_SDO_OK, EX_SDO_FAIL } ExSdoStep_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static const char* const k_state_name[] = {
    "버튼 대기", "① 첫 하트비트", "① 통신 재설정", "① PRE_OPERATIONAL 로", "① 정지 쓰기", "② 설정", "② 소비 감시 확인",
    "③ NMT START", "③ 켜기", "④ 동작", "⑤ 정지하는 중", "⑤ 통신 이상 — 정지 쓰기", "멈춤",
};
static const char* const k_reason_name[] = {
    "", "버튼", "응답 없음", "장치 없음", "PC 시뮬레이터가 아님 — 켜지 않음", "고장이 풀리지 않음", "켜지지 않음",
    "통신 이상", "포트를 클래식 모드로 정하지 못함", "하트비트 감시 등록 실패",
};

static ExState_t  s_state = EX_IDLE;
static ExReason_t s_reason = R_NONE;       /**< 멈춤 이유 */
static ExReason_t s_after_stop = R_NONE;   /**< EX_STOPPING 이 끝나면 쓸 이유 */
static uint8_t    s_sub;                   /**< 단계 안의 작은 단계 */
static uint32_t   s_t0;                    /**< 단계에 들어온 시각 */
static uint32_t   s_t1;                    /**< 작은 단계의 시각(시간 제한 기준) */
static uint32_t   s_poll_last;             /**< 상태를 마지막으로 읽은 시각 */
static uint32_t   s_bootup_base;           /**< 통신 재설정 전의 부팅 알림 수 */
static uint32_t   s_bootup_gen;            /**< ② 에 들어올 때의 부팅 알림 수 — ② 부터 이 값과 다르면 드라이버가 다시 켜진 것 */
static uint8_t    s_fault_resets;
static bool       s_stop_confirmed;        /**< 통신 이상 뒤 정지 쓰기가 DONE 이었나 */

/* SDO — 한 번에 하나. 지금 낸 요청과 같은 요청인지 보고, 다르면 앞 결과를 받아 버린 뒤 새로 냅니다 */
static bool       s_sdo_busy;
static bool       s_sdo_write;
static uint16_t   s_sdo_index;
static uint8_t    s_sdo_sub;
static uint32_t   s_sdo_value;
static uint32_t   s_sdo_since;             /**< 이 요청을 내려고 기다리기 시작한 시각 */
static XmCanOpenSdoResult_t s_sdo_res;
static XmCanOpenSdoStatus_t s_sdo_last;    /**< 마지막으로 끝난 요청의 상태 */
static XmCanResult_t s_send_last;          /**< 마지막으로 거부된 보내기 결과 */
static bool       s_sdo_drained;           /**< 마지막 EX_SDO_FAIL 이 앞 단계 요청의 실패였나 */

/* XM10 하트비트 */
static bool       s_xm_hb_on;
static uint32_t   s_xm_hb_last;

/* 보여 주기 */
static uint32_t   s_device_type;
static uint32_t   s_error_code;
static uint16_t   s_statusword;
static int32_t    s_actual;
static uint32_t   s_stream_last;
static float      s_usb[USB_CH_COUNT];
static int8_t     s_led1_shown = -1;
static int8_t     s_led2_shown = -1;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void      _OnButton(uint32_t now);
static void      _Step(uint32_t now);
static void      _StepFirstHeartbeat(uint32_t now);
static void      _StepResetWait(uint32_t now);
static void      _StepToPreOperational(uint32_t now);
static void      _StepOperStop(uint32_t now);
static void      _StepConfig(uint32_t now);
static void      _StepConsumer(uint32_t now);
static void      _StepStartNode(uint32_t now);
static void      _StepEnable(uint32_t now);
static void      _StepRun(uint32_t now);
static void      _StepStopping(uint32_t now);
static void      _StepCommLost(uint32_t now);
static bool      _CommCheck(bool need_operational, uint32_t now);
static bool      _Rebooted(void);
static bool      _ReadStatusword(uint32_t now, bool* failed);
static ExSdoStep_t _Sdo(bool write, uint16_t index, uint8_t sub, uint32_t value, uint8_t size, uint32_t now);
static bool      _Nmt(uint8_t command);
static void      _Enter(ExState_t st, uint32_t now);
static void      _Halt(ExReason_t why, bool stop_heartbeat, uint32_t now);
static void      _StopThenHalt(ExReason_t why, uint32_t now);
static void      _CommLost(ExReason_t why, bool try_stop_write, uint32_t now);
static void      _XmHeartbeatStart(uint32_t now);
static void      _XmHeartbeatTick(uint32_t now);
static void      _Report(void);
static void      _Stream(uint32_t now);
static void      _UpdateLeds(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/** @brief 포트를 클래식 CAN 1 Mbit/s 로 — 전원을 켤 때 한 번 불립니다. 값만 돌려주세요(다른 XM 함수는 아직 준비 전) */
XmCanBusMode_t XM_CanBusMode(void)
{
    return XM_CAN_BUS_CLASSIC;
}

void Control_Setup(void)
{
    XM_SetUsbCustomMeta(USB_MODULE_ID,
        "[{\"name\":\"Step\"},{\"name\":\"Target\"},{\"name\":\"Actual\"},{\"name\":\"Statusword\"}]");

    if (XM_CanGetBusMode() != XM_CAN_BUS_CLASSIC) {
        s_reason = R_PORT;
        s_state = EX_HALTED;
        return;
    }
    /* 하트비트 감시는 Control_Setup() 안에서만 등록됩니다 */
    if (!XM_CanOpenWatchNode(EX46_NODE_ID, EX46_WATCH_TIMEOUT_MS)) {
        s_reason = R_WATCH;
        s_state = EX_HALTED;
        return;
    }
    s_state = EX_IDLE;
}

void Control_Loop(void)
{
    const uint32_t now = XM_GetTick();

    _OnButton(now);
    _Step(now);
    _XmHeartbeatTick(now);   /* 같은 주기에 통신 이상으로 멈췄으면 보내지 않습니다 */
    _Stream(now);
    _UpdateLeds();
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/** @brief 버튼 1 — 멈춰 있으면 시작, 움직이는 중이면 정지 */
static void _OnButton(uint32_t now)
{
    if (XM_GetButtonEvent(XM_BTN_1) != XM_BTN_CLICK) {
        return;
    }
    switch (s_state) {
    case EX_IDLE:
    case EX_HALTED:
        if (s_reason != R_PORT && s_reason != R_WATCH) {
            s_fault_resets = 0U;
            _Enter(EX_FIRST_HB, now);
        }
        break;
    case EX_FIRST_HB:
    case EX_RESET_WAIT:
    case EX_TO_PREOP:
        _Halt(R_BUTTON, false, now);         /* 아직 아무것도 켜지 않았습니다 */
        break;
    case EX_OPER_STOP:
    case EX_STOPPING:
    case EX_COMM_LOST:
        break;                               /* 이미 정지 쓰기 중 */
    default:
        _StopThenHalt(R_BUTTON, now);        /* ② ~ ④ */
        break;
    }
}

static void _Step(uint32_t now)
{
    switch (s_state) {
    case EX_FIRST_HB:   _StepFirstHeartbeat(now);    break;
    case EX_RESET_WAIT: _StepResetWait(now);         break;
    case EX_TO_PREOP:   _StepToPreOperational(now);  break;
    case EX_OPER_STOP:  _StepOperStop(now);          break;
    case EX_CONFIG:     _StepConfig(now);            break;
    case EX_CONSUMER:   _StepConsumer(now);          break;
    case EX_START_NODE: _StepStartNode(now);         break;
    case EX_ENABLE:     _StepEnable(now);            break;
    case EX_RUN:        _StepRun(now);               break;
    case EX_STOPPING:   _StepStopping(now);          break;
    case EX_COMM_LOST:  _StepCommLost(now);          break;
    default:                                         break;   /* EX_IDLE · EX_HALTED — 버튼을 기다림(자동으로 다시 시작하지 않음) */
    }
}

/** @brief ① 첫 하트비트의 상태로 나눕니다. 3 초 동안 없으면 NMT 통신 재설정을 한 번 보냅니다 */
static void _StepFirstHeartbeat(uint32_t now)
{
    switch (XM_CanOpenGetNodeState(EX46_NODE_ID)) {
    case XM_CANOPEN_NODE_BOOTUP:
    case XM_CANOPEN_NODE_PRE_OPERATIONAL:
        _Enter(EX_CONFIG, now);
        return;
    case XM_CANOPEN_NODE_STOPPED:
        _Enter(EX_TO_PREOP, now);
        return;
    case XM_CANOPEN_NODE_OPERATIONAL:
        _Enter(EX_OPER_STOP, now);           /* XM10 만 다시 켜졌을 때 — 드라이버가 아직 돌고 있을 수 있습니다 */
        return;
    default:
        break;                               /* 아직 못 받음 · LOST */
    }
    if ((now - s_t0) < EX46_FIRST_HB_WAIT_MS) {
        return;
    }
    const uint32_t base = XM_CanOpenGetBootupCount(EX46_NODE_ID);   /* 보내기 전에 — 부팅 알림이 바로 와도 놓치지 않게 */
    if (_Nmt(XM_CANOPEN_NMT_RESET_COMM)) {
        s_bootup_base = base;
        _Enter(EX_RESET_WAIT, now);
    } else if ((now - s_t0) >= (EX46_FIRST_HB_WAIT_MS + EX46_SEND_LIMIT_MS)) {
        _Halt(R_NO_RESPONSE, true, now);
    }
}

/** @brief ① 통신 재설정 뒤 부팅 알림 수가 늘면 ② — 받은 알림의 수일 뿐 재설정의 응답은 아닙니다 */
static void _StepResetWait(uint32_t now)
{
    if (XM_CanOpenGetBootupCount(EX46_NODE_ID) != s_bootup_base) {
        _Enter(EX_CONFIG, now);
    } else if ((now - s_t0) >= EX46_BOOTUP_WAIT_MS) {
        _Halt(R_NO_DEVICE, true, now);
    }
}

/** @brief ① NMT PRE_OPERATIONAL 을 보내고 하트비트 0x7F 를 확인합니다 */
static void _StepToPreOperational(uint32_t now)
{
    if (s_sub == 0U) {
        if (_Nmt(XM_CANOPEN_NMT_PRE_OPERATIONAL)) {
            s_sub = 1U;
            s_t1 = now;
        } else if ((now - s_t0) >= EX46_SEND_LIMIT_MS) {
            _Halt(R_NO_RESPONSE, true, now);
        }
        return;
    }
    if (XM_CanOpenGetNodeState(EX46_NODE_ID) == XM_CANOPEN_NODE_PRE_OPERATIONAL) {
        _Enter(EX_CONFIG, now);
    } else if ((now - s_t1) >= EX46_NMT_STATE_WAIT_MS) {
        _Halt(R_NO_RESPONSE, true, now);
    }
}

/** @brief ① OPERATIONAL 이던 드라이버를 먼저 멈춥니다(0x6040 = 0x06) */
static void _StepOperStop(uint32_t now)
{
    const ExSdoStep_t r = _Sdo(true, OD_CONTROLWORD, 0U, CW_SHUTDOWN, 2U, now);
    if (r == EX_SDO_OK) {
        _Enter(EX_TO_PREOP, now);
    } else if (r == EX_SDO_FAIL) {
        _CommLost(R_NO_RESPONSE, false, now);   /* 정지 쓰기가 방금 실패했습니다 — 다시 시도하지 않습니다 */
    }
}

/** @brief ② 시뮬레이터 표시 → 장치 종류 → 하트비트 송신 주기 → (XM10 하트비트) 하트비트 소비 → 속도 모드 */
static void _StepConfig(uint32_t now)
{
    ExSdoStep_t r = EX_SDO_WAIT;

    if (_Rebooted()) {
        _CommLost(R_COMM, true, now);        /* 설정 도중 다시 켜짐 — 앞에서 쓴 설정이 사라졌습니다 */
        return;
    }
    switch (s_sub) {
    case 0U:
        r = _Sdo(false, EX46_SIM_ID, 0U, 0U, 0U, now);
        if (r == EX_SDO_OK && s_sdo_res.value != EX46_SIM_ID_VALUE) {
            _Halt(R_NOT_SIM, true, now);
            return;
        }
        if (r == EX_SDO_FAIL && s_sdo_last == XM_CANOPEN_SDO_ABORTED) {
            _Halt(R_NOT_SIM, true, now);     /* 그 객체가 없습니다 */
            return;
        }
        break;
    case 1U:
        r = _Sdo(false, OD_DEVICE_TYPE, 0U, 0U, 0U, now);
        if (r == EX_SDO_OK) {
            s_device_type = s_sdo_res.value;
        }
        break;
    case 2U:
        r = _Sdo(true, OD_HB_PRODUCER, 0U, EX46_HB_PRODUCER_MS, 2U, now);
        break;
    case 3U:
        _XmHeartbeatStart(now);              /* 소비 감시는 설정 뒤 첫 하트비트를 받아야 시작합니다 */
        r = _Sdo(true, OD_HB_CONSUMER, 1U, ((uint32_t)EX46_XM_NODE_ID << 16) | EX46_CONSUMER_TIME_MS, 4U, now);
        break;
    default:
        r = _Sdo(true, OD_MODES_OF_OPERATION, 0U, MODE_PROFILE_VELOCITY, 1U, now);
        if (r == EX_SDO_OK) {
            _Enter(EX_CONSUMER, now);
            return;
        }
        break;
    }
    if (r == EX_SDO_OK) {
        s_sub++;
    } else if (r == EX_SDO_FAIL) {
        _CommLost(R_COMM, true, now);
    }
}

/**
 * @brief ② 드라이버의 소비 감시가 켜졌는지 확인합니다(시뮬레이터 객체가 2) — 안 되면 켜지 않습니다.
 * @details 다시 켜졌는지는 ② 에 들어올 때 정한 부팅 알림 수로 봅니다. 확인 응답을 받은 뒤 결과를 읽기 전에 다시 켜졌을 수
 *          있어서, 읽은 때의 수로 기준을 다시 잡지 않습니다(그 결과는 이전 부팅의 것일 수 있습니다 — ③ 이 다시 봅니다).
 */
static void _StepConsumer(uint32_t now)
{
    if (_Rebooted()) {
        _CommLost(R_COMM, true, now);        /* 소비 확인 도중 다시 켜짐 — 소비 감시 설정이 사라졌습니다 */
        return;
    }
    if (s_sdo_busy || (now - s_poll_last) >= EX46_POLL_MS) {
        const ExSdoStep_t r = _Sdo(false, EX46_SIM_HB_CONSUMER_STATE, 0U, 0U, 0U, now);
        if (r == EX_SDO_OK) {
            if (s_sdo_res.value == EX46_SIM_CONSUMER_ACTIVE) {
                _Enter(EX_START_NODE, now);
                return;
            }
            s_poll_last = now;
        } else if (r == EX_SDO_FAIL) {
            _CommLost(R_COMM, true, now);
            return;
        }
    }
    if ((now - s_t0) >= EX46_CONSUMER_WAIT_MS) {
        _CommLost(R_NO_RESPONSE, true, now);
    }
}

/** @brief ③ NMT START → 하트비트 0x05 */
static void _StepStartNode(uint32_t now)
{
    if (_CommCheck(false, now)) {
        return;                              /* 아직 PRE_OPERATIONAL 이라 LOST · 다시 켜짐만 봅니다 */
    }
    if (s_sub == 0U) {
        if (_Nmt(XM_CANOPEN_NMT_START)) {
            s_sub = 1U;
            s_t1 = now;
        } else if ((now - s_t0) >= EX46_SEND_LIMIT_MS) {
            _CommLost(R_COMM, true, now);
        }
        return;
    }
    if (XM_CanOpenGetNodeState(EX46_NODE_ID) == XM_CANOPEN_NODE_OPERATIONAL) {
        _Enter(EX_ENABLE, now);
    } else if ((now - s_t1) >= EX46_NMT_STATE_WAIT_MS) {
        _CommLost(R_NO_RESPONSE, true, now);
    }
}

/**
 * @brief ③ 0x6041 을 읽어 고장이면 해제 → 0x06 → 0x0F, 각각 0x6041 로 확인합니다.
 * @details 작은 단계: 0 고장 확인 · 1 0x06 · 2 준비 확인 · 3 0x0F · 4 동작 확인 · 10 원인 읽기 · 11 bit 7 내리기(0x00) ·
 *          12 고장 해제(0x80 — 앞 값이 이미 0x80 이어도 0 에서 1 로 바뀌게)
 */
static void _StepEnable(uint32_t now)
{
    bool failed = false;
    ExSdoStep_t r = EX_SDO_WAIT;

    if (_CommCheck(true, now)) {
        return;                              /* 켜기 동안에도 — 하트비트 LOST · 다시 켜짐 · OPERATIONAL 이 아님 */
    }
    switch (s_sub) {
    case 0U:
        if (_ReadStatusword(now, &failed)) {
            const uint16_t f = (uint16_t)(s_statusword & SW_FAULT_MASK);
            if (f == SW_FAULT || f == SW_FAULT_REACTION) {
                if (s_fault_resets >= MAX_FAULT_RESETS) {
                    _StopThenHalt(R_FAULT, now);
                    return;
                }
                s_sub = 10U;
            } else {
                s_sub = 1U;
            }
        }
        break;
    case 10U:
        r = _Sdo(false, OD_ERROR_CODE, 0U, 0U, 0U, now);
        if (r == EX_SDO_OK) {
            s_error_code = s_sdo_res.value;  /* 원인은 보여 주기만 합니다 */
            _Report();
            s_sub = 11U;
        }
        break;
    case 11U:
        r = _Sdo(true, OD_CONTROLWORD, 0U, CW_DISABLE_VOLTAGE, 2U, now);
        if (r == EX_SDO_OK) {
            s_sub = 12U;
        }
        break;
    case 12U:
        r = _Sdo(true, OD_CONTROLWORD, 0U, CW_FAULT_RESET, 2U, now);
        if (r == EX_SDO_OK) {
            s_fault_resets++;
            s_sub = 0U;
            s_poll_last = now;               /* 다시 읽기는 EX46_POLL_MS 뒤 */
        }
        break;
    case 1U:
    case 3U:
        r = _Sdo(true, OD_CONTROLWORD, 0U, (s_sub == 1U) ? CW_SHUTDOWN : CW_ENABLE_OPERATION, 2U, now);
        if (r == EX_SDO_OK) {
            s_sub++;
            s_t1 = now;
            s_poll_last = now - EX46_POLL_MS;    /* 바로 읽기 */
        }
        break;
    default: {                               /* 2 · 4 — 기대한 상태가 될 때까지 다시 읽습니다 */
        const uint16_t want = (s_sub == 2U) ? SW_READY_TO_SWITCH_ON : SW_OPERATION_ENABLED;
        if (_ReadStatusword(now, &failed)) {
            const uint16_t f = (uint16_t)(s_statusword & SW_FAULT_MASK);
            if ((s_statusword & SW_STATE_MASK) == want) {
                if (s_sub == 2U) {
                    s_sub = 3U;
                } else {
                    _Enter(EX_RUN, now);
                }
                return;
            }
            if (f == SW_FAULT || f == SW_FAULT_REACTION) {
                _StopThenHalt(R_FAULT, now);
                return;
            }
        }
        if (!failed && (now - s_t1) >= EX46_DRIVE_STATE_WAIT_MS) {
            _StopThenHalt(R_NOT_ENABLED, now);   /* 늦게 켜질 수도 있으니 정지 쓰기를 먼저 */
            return;
        }
        break;
    }
    }
    if (failed || r == EX_SDO_FAIL) {
        _CommLost(R_COMM, true, now);
    }
}

/** @brief ④ 10 ms 마다 목표 속도 쓰기 → 실제 속도 읽기. 드라이버가 OPERATIONAL 이 아니면 통신 이상 */
static void _StepRun(uint32_t now)
{
    if (_CommCheck(true, now)) {
        return;                              /* LOST · 다시 켜짐 · 다른 상태 */
    }
    ExSdoStep_t r = EX_SDO_WAIT;
    if (s_sub == 0U) {
        if ((now - s_t1) < EX46_CMD_PERIOD_MS) {
            return;
        }
        s_t1 = now;
        s_sub = 1U;
    }
    if (s_sub == 1U) {
        r = _Sdo(true, OD_TARGET_VELOCITY, 0U, (uint32_t)(int32_t)EX46_TARGET_VELOCITY, 4U, now);
        if (r == EX_SDO_OK) {
            s_sub = 2U;
        }
    } else {
        r = _Sdo(false, OD_VELOCITY_ACTUAL, 0U, 0U, 0U, now);
        if (r == EX_SDO_OK) {
            s_actual = (int32_t)s_sdo_res.value;
            s_sub = 0U;
        }
    }
    if (r == EX_SDO_FAIL) {
        _CommLost(R_COMM, true, now);
    }
}

/**
 * @brief ⑤ 정지 쓰기(0x6040 = 0x06) — DONE 이면 멈춤, 실패면 통신 이상.
 * @details 정지 쓰기 자체가 실패했으면 다시 시도하지 않습니다. 정지 쓰기를 내기 전에 앞 요청이 실패로 끝났으면
 *          (통신 이상의 증거) XM10 하트비트를 바로 멈추고 정지 쓰기를 한 번 시도합니다.
 */
static void _StepStopping(uint32_t now)
{
    const ExSdoStep_t r = _Sdo(true, OD_CONTROLWORD, 0U, CW_SHUTDOWN, 2U, now);
    if (r == EX_SDO_OK) {
        _Halt(s_after_stop, false, now);
    } else if (r == EX_SDO_FAIL) {
        _CommLost(R_COMM, s_sdo_drained, now);
    }
}

/** @brief ⑤ 통신 이상 — XM10 하트비트는 이미 멈췄습니다. 정지 쓰기를 한 번 내고 결과만 기록합니다 */
static void _StepCommLost(uint32_t now)
{
    const ExSdoStep_t r = _Sdo(true, OD_CONTROLWORD, 0U, CW_SHUTDOWN, 2U, now);
    if (r == EX_SDO_FAIL && s_sdo_drained) {
        return;                              /* 앞 요청의 실패 — 정지 쓰기는 다음 주기에 냅니다 */
    }
    if (r != EX_SDO_WAIT) {
        s_stop_confirmed = (r == EX_SDO_OK);
        _Halt(s_reason, true, now);
    }
}

/**
 * @brief ③ ~ ④ 통신 확인 — 하트비트 LOST · 드라이버가 다시 켜짐(부팅 알림 수가 ② 에 들어올 때와 다름 — 하트비트 소비 설정도
 *        기본값으로 돌아갔습니다) · (need_operational 이면) OPERATIONAL 이 아님이면 통신 이상으로 멈춥니다. 멈췄으면 true.
 */
static bool _CommCheck(bool need_operational, uint32_t now)
{
    const XmCanOpenNodeState_t ns = XM_CanOpenGetNodeState(EX46_NODE_ID);
    const bool rebooted = _Rebooted();
    if (ns == XM_CANOPEN_NODE_LOST || rebooted || (need_operational && ns != XM_CANOPEN_NODE_OPERATIONAL)) {
        _CommLost(R_COMM, true, now);
        return true;
    }
    return false;
}

/** @brief ② 에 들어올 때의 부팅 알림 수와 다르면 드라이버가 다시 켜진 것입니다 — 그사이 쓴 설정 · 소비 감시가 사라졌습니다 */
static bool _Rebooted(void)
{
    return XM_CanOpenGetBootupCount(EX46_NODE_ID) != s_bootup_gen;
}

/** @brief 0x6041 을 EX46_POLL_MS 간격으로 읽습니다 — 새 값을 받으면 true, 실패면 *failed = true */
static bool _ReadStatusword(uint32_t now, bool* failed)
{
    if (!s_sdo_busy && (now - s_poll_last) < EX46_POLL_MS) {
        return false;
    }
    const ExSdoStep_t r = _Sdo(false, OD_STATUSWORD, 0U, 0U, 0U, now);
    if (r == EX_SDO_FAIL) {
        *failed = true;
        return false;
    }
    if (r != EX_SDO_OK) {
        return false;
    }
    s_statusword = (uint16_t)s_sdo_res.value;
    s_poll_last = now;
    return true;
}

/**
 * @brief SDO 한 걸음 — 낼 수 있으면 내고, 결과가 오면 EX_SDO_OK / EX_SDO_FAIL 을 돌려줍니다.
 * @details 같은 요청을 매 주기 불러도 됩니다. 앞 단계의 요청이 아직 끝나지 않았으면 그 결과를 받아 버린 뒤 냅니다
 *          (요청 칸이 하나라서). BUSY(쉬는 시간 · 송신 대기 비우기)는 EX46_SEND_LIMIT_MS 까지 다시 시도합니다.
 */
static ExSdoStep_t _Sdo(bool write, uint16_t index, uint8_t sub, uint32_t value, uint8_t size, uint32_t now)
{
    const bool same = (s_sdo_write == write) && (s_sdo_index == index) && (s_sdo_sub == sub) &&
                      (!write || s_sdo_value == value);
    XmCanOpenSdoResult_t res;

    if (s_sdo_busy && !same) {
        const XmCanOpenSdoStatus_t prev = XM_CanOpenSdoGetResult(&res);
        if (prev == XM_CANOPEN_SDO_BUSY) {
            return EX_SDO_WAIT;
        }
        s_sdo_busy = false;                  /* 앞 요청은 끝났습니다 — 이 요청은 다음 주기에 냅니다 */
        s_sdo_since = now;
        if (prev == XM_CANOPEN_SDO_DONE || prev == XM_CANOPEN_SDO_IDLE) {
            return EX_SDO_WAIT;
        }
        s_sdo_last = prev;                   /* 앞 요청이 실패로 끝났습니다 — 통신 이상의 증거 */
        s_sdo_res = res;
        s_sdo_drained = true;
        return EX_SDO_FAIL;
    }
    s_sdo_drained = false;
    if (!s_sdo_busy) {
        const XmCanResult_t sr = write ? XM_CanOpenSdoWrite(EX46_NODE_ID, index, sub, value, size)
                                       : XM_CanOpenSdoRead(EX46_NODE_ID, index, sub);
        if (sr == XM_CAN_OK) {
            s_sdo_busy = true;
            s_sdo_write = write;
            s_sdo_index = index;
            s_sdo_sub = sub;
            s_sdo_value = value;
            return EX_SDO_WAIT;
        }
        s_send_last = sr;
        if (sr == XM_CAN_BUSY && (now - s_sdo_since) < EX46_SEND_LIMIT_MS) {
            return EX_SDO_WAIT;              /* 다음 주기에 다시 */
        }
        s_sdo_since = now;
        return EX_SDO_FAIL;                  /* 버스 복구 중 · 거부 · 너무 오래 BUSY */
    }
    const XmCanOpenSdoStatus_t st = XM_CanOpenSdoGetResult(&res);
    if (st == XM_CANOPEN_SDO_BUSY) {
        return EX_SDO_WAIT;
    }
    s_sdo_busy = false;
    s_sdo_since = now;
    s_sdo_last = st;
    if (st != XM_CANOPEN_SDO_IDLE) {
        s_sdo_res = res;                     /* 읽은 값 · 중단 코드 */
    }
    return (st == XM_CANOPEN_SDO_DONE) ? EX_SDO_OK : EX_SDO_FAIL;   /* ABORTED · TIMEOUT · UNSUPPORTED (IDLE = 결과를 잃음) */
}

/** @brief NMT 한 번 — 수락(XM_CAN_OK)이면 true. BUSY 등은 다음 주기에 다시(단계 시간 제한이 끝을 정합니다) */
static bool _Nmt(uint8_t command)
{
    const XmCanResult_t r = XM_CanOpenSendNmt(command, EX46_NODE_ID);
    if (r != XM_CAN_OK) {
        s_send_last = r;
    }
    return r == XM_CAN_OK;
}

static void _Enter(ExState_t st, uint32_t now)
{
    if (st == EX_CONFIG) {
        s_bootup_gen = XM_CanOpenGetBootupCount(EX46_NODE_ID);   /* ② 부터 이 부팅에 쓰고 확인합니다 */
    }
    s_state = st;
    s_sub = 0U;
    s_t0 = now;
    s_t1 = now;
    s_poll_last = now - EX46_POLL_MS;        /* 첫 읽기는 바로 */
    s_sdo_since = now;
    _Report();
}

/** @brief 멈춤 — stop_heartbeat 면 XM10 하트비트도 멈춥니다(통신 이상 · 응답 없음) */
static void _Halt(ExReason_t why, bool stop_heartbeat, uint32_t now)
{
    if (stop_heartbeat) {
        s_xm_hb_on = false;
    }
    s_reason = why;
    _Enter(EX_HALTED, now);
}

/** @brief 정지 쓰기를 한 뒤 멈춥니다(버튼 · 켜기 실패 — 통신은 정상이라 XM10 하트비트는 그대로) */
static void _StopThenHalt(ExReason_t why, uint32_t now)
{
    s_after_stop = why;
    _Enter(EX_STOPPING, now);
}

/** @brief 통신 이상 — (a) 명령을 멈추고 (c) XM10 하트비트를 바로 멈춘 뒤 (b) 정지 쓰기를 한 번 시도합니다 */
static void _CommLost(ExReason_t why, bool try_stop_write, uint32_t now)
{
    s_xm_hb_on = false;
    s_reason = why;
    s_stop_confirmed = false;
    if (try_stop_write) {
        _Enter(EX_COMM_LOST, now);
    } else {
        _Halt(why, true, now);
    }
}

static void _XmHeartbeatStart(uint32_t now)
{
    if (!s_xm_hb_on) {
        s_xm_hb_on = true;
        s_xm_hb_last = now - EX46_XM_HB_PERIOD_MS;   /* 바로 하나 보냅니다 */
    }
}

/** @brief XM10 하트비트 [0x05] — 100 ms 마다. BUSY(잠깐 막힘)면 다음 주기(1 ms)에 다시, 버스 복구 중 · 거부면 통신 이상 */
static void _XmHeartbeatTick(uint32_t now)
{
    if (!s_xm_hb_on || (now - s_xm_hb_last) < EX46_XM_HB_PERIOD_MS) {
        return;
    }
    const uint8_t state = HB_STATE_OPERATIONAL;
    const XmCanResult_t r = XM_CanSendFrame((uint16_t)(HB_ID_BASE + EX46_XM_NODE_ID), &state, 1U);
    if (r == XM_CAN_OK) {
        s_xm_hb_last = now;
    } else if (r != XM_CAN_BUSY) {
        s_send_last = r;
        _CommLost(R_COMM, true, now);        /* SDO 가 진행 중이어도 — 하트비트를 멈추고 정지 쓰기를 한 번 */
    }
}

/** @brief 단계가 바뀔 때마다 한 줄 — 멈춤이면 이유와 마지막 결과도 */
static void _Report(void)
{
    if (!XM_IsUsbStreamConnected()) {
        return;
    }
    char msg[REPORT_MSG_SIZE];
    if (s_state == EX_HALTED) {
        snprintf(msg, sizeof(msg),
                 "[Ex46] %s — %s (정지 쓰기 %s · SDO %u · abort 0x%08lX · 보내기 %u · 장치 종류 0x%08lX)\r\n",
                 k_state_name[s_state], k_reason_name[s_reason], s_stop_confirmed ? "확인" : "확인 못 함",
                 (unsigned)s_sdo_last, (unsigned long)s_sdo_res.abort_code, (unsigned)s_send_last,
                 (unsigned long)s_device_type);
    } else if (s_state == EX_ENABLE && s_sub == 10U) {
        snprintf(msg, sizeof(msg), "[Ex46] 드라이버 고장 — 원인 0x603F = 0x%04lX, 고장 해제를 시도합니다\r\n",
                 (unsigned long)s_error_code);
    } else {
        snprintf(msg, sizeof(msg), "[Ex46] %s\r\n", k_state_name[s_state]);
    }
    XM_SendUsbDebugMessage(msg);
}

/** @brief 0xF0 사용자 채널 — [단계, 목표 속도, 실제 속도, 0x6041] */
static void _Stream(uint32_t now)
{
    if ((now - s_stream_last) < EX46_STREAM_PERIOD_MS) {
        return;
    }
    s_stream_last = now;
    s_usb[0] = (float)s_state;
    s_usb[1] = (s_state == EX_RUN) ? (float)EX46_TARGET_VELOCITY : 0.0f;
    s_usb[2] = (float)s_actual;
    s_usb[3] = (float)s_statusword;
    XM_SendUsbDataWithId(s_usb, sizeof(s_usb), USB_MODULE_ID);
}

/** @brief LED1 = 단계, LED2 = XM10 하트비트 — 바뀔 때만 다시 그립니다 */
static void _UpdateLeds(void)
{
    int8_t led1;
    if (s_state == EX_IDLE) {
        led1 = 0;
    } else if (s_state == EX_RUN) {
        led1 = 2;
    } else if (s_state == EX_STOPPING || s_state == EX_COMM_LOST) {
        led1 = 3;
    } else if (s_state == EX_HALTED) {
        led1 = (s_reason == R_BUTTON) ? 4 : 5;
    } else {
        led1 = 1;                            /* ① ~ ③ */
    }
    if (led1 != s_led1_shown) {
        s_led1_shown = led1;
        switch (led1) {
        case 0:  XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, LED_BLINK_IDLE_MS);    break;
        case 1:  XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, LED_BLINK_BUSY_MS);    break;
        case 2:  XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0U);                   break;
        case 3:  XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, LED_BLINK_STOP_MS);    break;
        case 4:  XM_SetLedEffect(XM_LED_1, XM_LED_OFF, 0U);                     break;
        default: XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, LED_HEARTBEAT_MS); break;
        }
    }
    const int8_t led2 = s_xm_hb_on ? 1 : 0;
    if (led2 != s_led2_shown) {
        s_led2_shown = led2;
        XM_SetLedState(XM_LED_2, s_xm_hb_on ? XM_ON : XM_OFF);
    }
}
