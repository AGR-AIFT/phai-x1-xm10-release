/**
 ******************************************************************************
 * @file    canopen_drive_pdo.c
 * @author  HyundoKim
 * @brief   [고급] CANopen 모터 드라이버를 PDO + SYNC 로 1 kHz 명령하기 — PC 시뮬레이터용 (클래식 CAN 1 Mbit/s)
 * @version 1.0
 * @date    Nov 6, 2026
 *
 * @details
 * Ex.46 은 SDO 로 100 Hz 를 보냅니다. 이 예제는 SDO 로 PDO 를 설정한 뒤 1 kHz 로 주고받습니다.
 *   RPDO1(0x200 + 노드)  제어 단어 2 B + 목표 속도 4 B
 *   TPDO1(0x180 + 노드)  상태 단어 2 B + 실제 속도 4 B
 *   SYNC(0x080)         XM10 이 매 주기 보내고, 드라이버는 SYNC 마다 TPDO 를 내고 받아 둔 RPDO 를 적용합니다
 *
 *  ⚠ 이 예제는 PC 시뮬레이터(드라이버 없이 PC 에서 CANopen 드라이버를 흉내 내는 프로그램)가 상대입니다.
 *    ② 에서 시뮬레이터 표시 객체(EX47_SIM_ID)를 확인하지 못하면 켜지 않고 멈춥니다.
 *    실제 드라이버 · 모터로 돌리려면 먼저 드라이버 설명서로 프로파일 · 객체 · 정지 방법 · 하트비트 소비 시간 ·
 *    통신 상실 반응을 정하고, 속도는 낮게, 버튼 정지를 확인한 뒤에 이 확인을 바꾸세요.
 *
 *  [순서]  버튼 1 = 시작 / 정지
 *   ① 시작 판정 — Ex.46 과 같습니다. 첫 하트비트로 드라이버 상태를 보고 PRE_OPERATIONAL 로 데려옵니다
 *   ② 설정 — SDO, 단계마다 응답을 확인합니다. 시뮬레이터 표시 → 하트비트 송신 주기 → (XM10 하트비트 시작) → 하트비트 소비 →
 *      동작 모드 → 통신 주기 → PDO 매핑. 매핑은 CiA 301 절차대로 바꿉니다:
 *      통신 파라미터로 PDO 끄기(COB-ID bit 31) → 개수 0 → 항목 → 개수 → 전송 형식 → 다시 켜기
 *      4 B 를 넘는 객체는 도움 함수(XM_CanOpenSdo*)로 쓸 수 없습니다(UNSUPPORTED — 직접 짜야 합니다).
 *      SDO 응답 ID 를 우편함에 등록하면 도움 함수가 거부합니다 — TPDO ID 만 등록합니다.
 *   ③ NMT START → 하트비트 0x05 → SYNC 시작 → TPDO 확인
 *   ④ 켜기 — RPDO 제어 단어로. 상태 단어를 보고 0x06 → 0x0F, 고장이면 0x00 → 0x80(한 번만)
 *   ⑤ 1 kHz — 새 TPDO 를 받은 주기에 RPDO 하나. SYNC · PDO 는 잠깐 쉴 수 있습니다(버스 정리 · 복구) — 1 kHz 를 가정하지 말고
 *      길이가 맞는 TPDO 가 EX47_TPDO_LOST_MS 끊기면 통신 이상으로 봅니다
 *   ⑥ 정지 — 버튼: RPDO 0x06 → 상태 단어로 확인 → SYNC 끔. 통신 이상: (a) RPDO 를 멈추고 (b) XM10 하트비트 · SYNC 를 끄고
 *      (c) 0x06 을 한 번 보내 봅니다(닿았다고 가정하지 않습니다) — 드라이버의 하트비트 소비 감시가 스스로 멈추게 하는 것이
 *      안전 정지의 핵심입니다
 *   ⑦ 통신이 돌아와도 다시 움직이지 않습니다 — 버튼 1 로 ① 부터 다시 설정하고 다시 켭니다
 *
 *  [확인 방법]
 *      LED1  느린 점멸 = 버튼 대기 · 빠른 점멸 = ①~④ 진행 · 켜짐 = ⑤ 동작 · 아주 빠른 점멸 = 정지하는 중
 *            · 꺼짐 = 버튼으로 정지 · 두근 = 멈춤(이유는 PC 문장)
 *      LED2  켜짐 = XM10 하트비트를 내는 중
 *      PC    xm10 도구 — 0xF0 채널에 단계 · 목표 속도 · 실제 속도 · 상태 단어, 단계가 바뀔 때마다 "[Ex47] ..." 문장
 *
 * @note 보내기 결과 XM_CAN_OK 는 '보내기 큐에 실림' 입니다. 전달 확인은 TPDO 로 봅니다.
 * @warning **본 예제는 XM10 Rev 2.0 전용입니다.** 클래식 모드에서는 이 포트에 CAN-FD 장치를 함께 연결하지 마세요.
 * @see     Ex.46 canopen_drive.c · xm_api_canopen.h · xm_api_canbus.h
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
#define EX47_NODE_ID                (1U)        /**< 드라이버 노드 ID(1~127) */
#define EX47_XM_NODE_ID             (0x7FU)     /**< XM10 하트비트 번호 — 드라이버 노드 ID 와 겹치지 않게 */
#define EX47_TARGET_VELOCITY        (100)       /**< 목표 속도(드라이버 단위) — 낮게 시작하세요 */
#define EX47_HB_PRODUCER_MS         (100U)      /**< 드라이버 하트비트 송신 주기(0x1017) — LOST 판정의 전제 */
#define EX47_WATCH_TIMEOUT_MS       (300U)      /**< 드라이버 하트비트가 이만큼 없으면 LOST */
#define EX47_XM_HB_PERIOD_MS        (100U)      /**< XM10 하트비트 주기 */
#define EX47_CONSUMER_TIME_MS       (300U)      /**< 드라이버가 XM10 하트비트를 기다리는 시간(0x1016:01) — 실제 드라이버 값은 직접 정하세요 */
#define EX47_SYNC_PERIOD_US         (1000UL)    /**< 통신 주기(0x1006) */
#define EX47_MODE                   (9U)        /**< 0x6060 동작 모드 — 9 = 주기 동기 속도(CSV) */

/* ===== PC 시뮬레이터 전용 객체(제조사 영역) ===== */
#define EX47_SIM_ID                 (0x2F00U)        /**< :00 이 EX47_SIM_ID_VALUE 면 PC 시뮬레이터 */
#define EX47_SIM_ID_VALUE           (0x4D495358UL)   /**< 바이트로 "XSIM" */
#define EX47_SIM_HB_CONSUMER_STATE  (0x2F01U)        /**< :00 = 0 미설정 · 1 첫 하트비트 전 · 2 감시 중 */
#define EX47_SIM_CONSUMER_ACTIVE    (2U)

/* ===== 시간 ===== */
#define EX47_FIRST_HB_WAIT_MS       (3000U)     /**< ① 첫 하트비트 */
#define EX47_BOOTUP_WAIT_MS         (2000U)     /**< ① 통신 재설정 뒤 부팅 알림 */
#define EX47_NMT_STATE_WAIT_MS      (2000U)     /**< ① · ③ NMT 상태 확인 */
#define EX47_CONSUMER_WAIT_MS       (1000U)     /**< ② 소비 감시 확인 */
#define EX47_TPDO_FIRST_WAIT_MS     (1000U)     /**< ③ SYNC 뒤 첫 TPDO */
#define EX47_ENABLE_WAIT_MS         (1000U)     /**< ④ 켜기 전체 */
#define EX47_STOP_WAIT_MS           (500U)      /**< ⑥ 정지 확인 */
#define EX47_TPDO_LOST_MS           (50U)       /**< ⑤ TPDO 가 이만큼 끊기면 통신 이상 — 정리 창 · 복구로 잠깐 쉬는 것은 넘깁니다 */
#define EX47_SEND_LIMIT_MS          (3000U)     /**< SDO · NMT 보내기가 BUSY 로 이만큼 막히면 실패 */
#define EX47_POLL_MS                (20U)
#define EX47_STREAM_PERIOD_MS       (20U)

/* ===== CiA 301 · 402 객체 ===== */
#define OD_COMM_CYCLE               (0x1006U)
#define OD_HB_CONSUMER              (0x1016U)   /**< :01 = (하트비트를 보내는 노드 ID << 16) | 시간 ms */
#define OD_HB_PRODUCER              (0x1017U)
#define OD_RPDO1_COMM               (0x1400U)   /**< :01 COB-ID · :02 전송 형식 */
#define OD_RPDO1_MAP                (0x1600U)   /**< :00 개수 · :01~ 항목 */
#define OD_TPDO1_COMM               (0x1800U)
#define OD_TPDO1_MAP                (0x1A00U)
#define OD_CONTROLWORD              (0x6040U)
#define OD_MODES_OF_OPERATION       (0x6060U)

#define MAP_ENTRY(index, sub, bits) (((uint32_t)(index) << 16) | ((uint32_t)(sub) << 8) | (uint32_t)(bits))
#define MAP_CONTROLWORD             MAP_ENTRY(0x6040U, 0U, 16U)
#define MAP_TARGET_VELOCITY         MAP_ENTRY(0x60FFU, 0U, 32U)
#define MAP_STATUSWORD              MAP_ENTRY(0x6041U, 0U, 16U)
#define MAP_VELOCITY_ACTUAL         MAP_ENTRY(0x606CU, 0U, 32U)
#define MAP_COUNT                   (2U)
#define COB_INVALID                 (0x80000000UL)   /**< COB-ID bit 31 — PDO 끔 */
#define PDO_TYPE_EVERY_SYNC         (1U)

#define ID_RPDO1                    (0x200U + EX47_NODE_ID)
#define ID_TPDO1                    (0x180U + EX47_NODE_ID)
#define PDO_LEN                     (6U)
#define CW_DISABLE_VOLTAGE          (0x0000U)
#define CW_SHUTDOWN                 (0x0006U)
#define CW_ENABLE_OPERATION         (0x000FU)
#define CW_FAULT_RESET              (0x0080U)
#define SW_SWITCH_ON_DISABLED       (0x0040U)   /**< 상태 단어 bit 6 */
#define SW_FAULT_MASK               (0x004FU)
#define SW_FAULT                    (0x0008U)
#define SW_FAULT_REACTION           (0x000FU)
#define SW_STATE_MASK               (0x006FU)
#define SW_OPERATION_ENABLED        (0x0027U)
#define MAX_FAULT_RESETS            (1U)

#define USB_MODULE_ID               (0xF0U)
#define USB_CH_COUNT                (4U)
#define REPORT_MSG_SIZE             (200U)
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
    EX_START_NODE,      /**< ③ NMT START → SYNC → TPDO */
    EX_ENABLE,          /**< ④ 켜기 */
    EX_RUN,             /**< ⑤ 1 kHz */
    EX_STOPPING,        /**< ⑥ 정지 확인 */
    EX_HALTED,          /**< 멈춤 — 버튼 1 로 ① 부터 */
} ExState_t;

typedef enum {
    R_NONE = 0,
    R_BUTTON,           /**< 버튼으로 정지 */
    R_NO_RESPONSE,      /**< 응답 없음 */
    R_NO_DEVICE,        /**< 장치 없음 */
    R_NOT_SIM,          /**< PC 시뮬레이터가 아님 — 켜지 않음 */
    R_NO_TPDO,          /**< SYNC 뒤에 TPDO 가 오지 않음 */
    R_FAULT,            /**< 고장 */
    R_NOT_ENABLED,      /**< 켜지지 않음 */
    R_NOT_STOPPED,      /**< 정지를 확인하지 못함 */
    R_COMM,             /**< 통신 이상 */
    R_PORT,             /**< 포트 · 등록 실패 */
} ExReason_t;

typedef enum { EX_SDO_WAIT = 0, EX_SDO_OK, EX_SDO_FAIL } ExSdoStep_t;

/** @brief SDO 쓰기 한 줄 */
typedef struct {
    uint16_t index;
    uint8_t  sub;
    uint8_t  size;
    uint32_t value;
} CfgStep_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static const char* const k_state_name[] = {
    "버튼 대기", "① 첫 하트비트", "① 통신 재설정", "① PRE_OPERATIONAL 로", "① 정지 쓰기", "② 설정", "② 소비 감시 확인",
    "③ NMT START", "④ 켜기", "⑤ 동작", "⑥ 정지 확인", "멈춤",
};
static const char* const k_reason_name[] = {
    "", "버튼", "응답 없음", "장치 없음", "PC 시뮬레이터가 아님 — 켜지 않음", "TPDO 없음", "고장", "켜지지 않음",
    "정지를 확인하지 못함", "통신 이상", "포트 설정",
};

/** @brief ② 설정 표 — 위에서 아래로, 단계마다 응답을 확인합니다. 하트비트 소비 줄 앞에서 XM10 하트비트를 시작합니다 */
static const CfgStep_t k_cfg[] = {
    { OD_HB_PRODUCER,        0U, 2U, EX47_HB_PRODUCER_MS },
    { OD_HB_CONSUMER,        1U, 4U, ((uint32_t)EX47_XM_NODE_ID << 16) | EX47_CONSUMER_TIME_MS },
    { OD_MODES_OF_OPERATION, 0U, 1U, EX47_MODE },
    { OD_COMM_CYCLE,         0U, 4U, EX47_SYNC_PERIOD_US },
    /* RPDO1 — 끄기 → 개수 0 → 항목 → 개수 → 전송 형식 → 켜기 */
    { OD_RPDO1_COMM,         1U, 4U, COB_INVALID | ID_RPDO1 },
    { OD_RPDO1_MAP,          0U, 1U, 0U },
    { OD_RPDO1_MAP,          1U, 4U, MAP_CONTROLWORD },
    { OD_RPDO1_MAP,          2U, 4U, MAP_TARGET_VELOCITY },
    { OD_RPDO1_MAP,          0U, 1U, MAP_COUNT },
    { OD_RPDO1_COMM,         2U, 1U, PDO_TYPE_EVERY_SYNC },
    { OD_RPDO1_COMM,         1U, 4U, ID_RPDO1 },
    /* TPDO1 */
    { OD_TPDO1_COMM,         1U, 4U, COB_INVALID | ID_TPDO1 },
    { OD_TPDO1_MAP,          0U, 1U, 0U },
    { OD_TPDO1_MAP,          1U, 4U, MAP_STATUSWORD },
    { OD_TPDO1_MAP,          2U, 4U, MAP_VELOCITY_ACTUAL },
    { OD_TPDO1_MAP,          0U, 1U, MAP_COUNT },
    { OD_TPDO1_COMM,         2U, 1U, PDO_TYPE_EVERY_SYNC },
    { OD_TPDO1_COMM,         1U, 4U, ID_TPDO1 },
};
#define CFG_STEP_COUNT  (sizeof(k_cfg) / sizeof(k_cfg[0]))
#define CFG_STEP_CONSUMER  (1U)   /**< 이 줄 앞에서 XM10 하트비트를 시작합니다 — 소비 감시는 설정 뒤 첫 하트비트를 받아야 시작합니다 */

static ExState_t  s_state = EX_IDLE;
static ExReason_t s_reason = R_NONE;
static uint8_t    s_sub;
static uint32_t   s_t0;
static uint32_t   s_t1;
static uint32_t   s_poll_last;
static uint32_t   s_bootup_base;
static uint32_t   s_bootup_gen;            /**< ② 에 들어올 때의 부팅 알림 수 — 이 값과 다르면 드라이버가 다시 켜진 것 */
static uint8_t    s_fault_resets;
static bool       s_fault_low_sent;        /**< 고장 해제 앞에 bit 7 을 내렸나 */

/* SDO — 한 번에 하나(Ex.46 과 같음) */
static bool       s_sdo_busy;
static bool       s_sdo_write;
static uint16_t   s_sdo_index;
static uint8_t    s_sdo_sub;
static uint32_t   s_sdo_value;
static uint32_t   s_sdo_since;
static XmCanOpenSdoResult_t s_sdo_res;
static XmCanOpenSdoStatus_t s_sdo_last;
static XmCanResult_t s_send_last;
static bool       s_sdo_drained;

/* PDO */
static uint32_t   s_tpdo_seq;              /**< 마지막으로 쓴 TPDO — SYNC 시작 뒤에 받은 것만 셉니다 */
static uint32_t   s_tpdo_tick;             /**< 길이가 맞는 TPDO 를 마지막으로 받은 시각 */
static uint16_t   s_statusword;
static int32_t    s_actual;
static bool       s_hb_on;
static bool       s_sync_on;

/* 보여 주기 */
static uint32_t   s_stream_last;
static float      s_usb[USB_CH_COUNT];
static int8_t     s_led1_shown = -1;
static int8_t     s_led2_shown = -1;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void        _OnButton(uint32_t now);
static void        _Step(uint32_t now);
static void        _StepFirstHeartbeat(uint32_t now);
static void        _StepResetWait(uint32_t now);
static void        _StepToPreOperational(uint32_t now);
static void        _StepOperStop(uint32_t now);
static void        _StepConfig(uint32_t now);
static void        _StepConsumer(uint32_t now);
static void        _StepStartNode(uint32_t now);
static void        _StepEnable(uint32_t now);
static void        _StepRun(uint32_t now);
static void        _StepStopping(uint32_t now);
static bool        _CommCheck(bool need_operational, uint32_t now);
static bool        _Rebooted(void);
static bool        _ReadTpdo(void);
static bool        _TpdoLost(void);
static bool        _IsFault(void);
static XmCanResult_t _SendRpdo(uint16_t controlword, int32_t target);
static ExSdoStep_t _Sdo(bool write, uint16_t index, uint8_t sub, uint32_t value, uint8_t size, uint32_t now);
static bool        _Nmt(uint8_t command);
static void        _Enter(ExState_t st, uint32_t now);
static void        _Halt(ExReason_t why, uint32_t now);
static void        _CommLost(ExReason_t why, uint32_t now);
static void        _SetHeartbeat(bool on);
static void        _SetSync(bool on);
static void        _Report(void);
static void        _Stream(uint32_t now);
static void        _UpdateLeds(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/** @brief 포트를 클래식 CAN 으로 — 전원을 켤 때 한 번 불립니다. 값만 돌려주세요(다른 XM 함수는 아직 준비 전) */
XmCanBusMode_t XM_CanBusMode(void)
{
    return XM_CAN_BUS_CLASSIC;
}

void Control_Setup(void)
{
    XM_SetUsbCustomMeta(USB_MODULE_ID,
        "[{\"name\":\"Step\"},{\"name\":\"Target\"},{\"name\":\"Actual\"},{\"name\":\"Statusword\"}]");

    /* TPDO 는 1 kHz 라 최신값 우편함으로 받습니다. 하트비트 감시 · 우편함 등록은 Control_Setup() 안에서만 됩니다 */
    if (XM_CanGetBusMode() != XM_CAN_BUS_CLASSIC ||
        !XM_CanOpenWatchNode(EX47_NODE_ID, EX47_WATCH_TIMEOUT_MS) || !XM_CanSubscribe(ID_TPDO1)) {
        s_reason = R_PORT;
        s_state = EX_HALTED;
    }
}

void Control_Loop(void)
{
    const uint32_t now = XM_GetTick();

    _OnButton(now);
    _Step(now);
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
        if (s_reason != R_PORT) {
            s_fault_resets = 0U;
            s_fault_low_sent = false;
            _Enter(EX_FIRST_HB, now);
        }
        break;
    case EX_FIRST_HB:
    case EX_RESET_WAIT:
    case EX_TO_PREOP:
        _Halt(R_BUTTON, now);                /* 아직 아무것도 켜지 않았습니다 */
        break;
    case EX_ENABLE:
    case EX_RUN:
        s_reason = R_BUTTON;
        _Enter(EX_STOPPING, now);
        break;
    case EX_OPER_STOP:
    case EX_STOPPING:
        break;                               /* 이미 멈추는 중 */
    default:
        _SetSync(false);
        _Halt(R_BUTTON, now);                /* ② · ③ — 켜기 전입니다 */
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
    default:                                         break;   /* 버튼을 기다림 — 스스로 다시 시작하지 않습니다 */
    }
}

/** @brief ① 첫 하트비트의 상태로 나눕니다. 3 초 동안 없으면 NMT 통신 재설정을 한 번 보냅니다 */
static void _StepFirstHeartbeat(uint32_t now)
{
    switch (XM_CanOpenGetNodeState(EX47_NODE_ID)) {
    case XM_CANOPEN_NODE_BOOTUP:
    case XM_CANOPEN_NODE_PRE_OPERATIONAL:
        _Enter(EX_CONFIG, now);
        return;
    case XM_CANOPEN_NODE_STOPPED:
        _Enter(EX_TO_PREOP, now);
        return;
    case XM_CANOPEN_NODE_OPERATIONAL:
        _Enter(EX_OPER_STOP, now);           /* XM10 만 다시 켜졌을 때 · 지난 실행 뒤 — 드라이버가 아직 돌고 있을 수 있습니다 */
        return;
    default:
        break;
    }
    if ((now - s_t0) < EX47_FIRST_HB_WAIT_MS) {
        return;
    }
    const uint32_t base = XM_CanOpenGetBootupCount(EX47_NODE_ID);   /* 보내기 전에 — 부팅 알림이 바로 와도 놓치지 않게 */
    if (_Nmt(XM_CANOPEN_NMT_RESET_COMM)) {
        s_bootup_base = base;
        _Enter(EX_RESET_WAIT, now);
    } else if ((now - s_t0) >= (EX47_FIRST_HB_WAIT_MS + EX47_SEND_LIMIT_MS)) {
        _Halt(R_NO_RESPONSE, now);
    }
}

static void _StepResetWait(uint32_t now)
{
    if (XM_CanOpenGetBootupCount(EX47_NODE_ID) != s_bootup_base) {
        _Enter(EX_CONFIG, now);
    } else if ((now - s_t0) >= EX47_BOOTUP_WAIT_MS) {
        _Halt(R_NO_DEVICE, now);
    }
}

static void _StepToPreOperational(uint32_t now)
{
    if (s_sub == 0U) {
        if (_Nmt(XM_CANOPEN_NMT_PRE_OPERATIONAL)) {
            s_sub = 1U;
            s_t1 = now;
        } else if ((now - s_t0) >= EX47_SEND_LIMIT_MS) {
            _Halt(R_NO_RESPONSE, now);
        }
        return;
    }
    if (XM_CanOpenGetNodeState(EX47_NODE_ID) == XM_CANOPEN_NODE_PRE_OPERATIONAL) {
        _Enter(EX_CONFIG, now);
    } else if ((now - s_t1) >= EX47_NMT_STATE_WAIT_MS) {
        _Halt(R_NO_RESPONSE, now);
    }
}

/** @brief ① OPERATIONAL 이던 드라이버를 먼저 멈춥니다 */
static void _StepOperStop(uint32_t now)
{
    const ExSdoStep_t r = _Sdo(true, OD_CONTROLWORD, 0U, CW_SHUTDOWN, 2U, now);
    if (r == EX_SDO_OK) {
        _Enter(EX_TO_PREOP, now);
    } else if (r == EX_SDO_FAIL) {
        _CommLost(R_NO_RESPONSE, now);
    }
}

/** @brief ② 시뮬레이터 표시 → 설정 표. s_sub = 0 표시 확인, 1~ = 표의 (s_sub - 1) 번째 줄 */
static void _StepConfig(uint32_t now)
{
    ExSdoStep_t r;

    if (_Rebooted()) {
        _CommLost(R_COMM, now);              /* 설정 도중 다시 켜짐 — 앞에서 쓴 설정이 사라졌습니다 */
        return;
    }
    if (s_sub == 0U) {
        r = _Sdo(false, EX47_SIM_ID, 0U, 0U, 0U, now);
        if (r == EX_SDO_OK && s_sdo_res.value != EX47_SIM_ID_VALUE) {
            _Halt(R_NOT_SIM, now);
            return;
        }
        if (r == EX_SDO_FAIL && s_sdo_last == XM_CANOPEN_SDO_ABORTED) {
            _Halt(R_NOT_SIM, now);           /* 그 객체가 없습니다 */
            return;
        }
    } else {
        const uint8_t row = (uint8_t)(s_sub - 1U);
        const CfgStep_t* c = &k_cfg[row];
        if (row == CFG_STEP_CONSUMER) {
            _SetHeartbeat(true);
        }
        r = _Sdo(true, c->index, c->sub, c->value, c->size, now);
        if (r == EX_SDO_OK && (row + 1U) == CFG_STEP_COUNT) {
            _Enter(EX_CONSUMER, now);
            return;
        }
    }
    if (r == EX_SDO_OK) {
        s_sub++;
    } else if (r == EX_SDO_FAIL) {
        _CommLost(R_COMM, now);
    } else {
        /* 응답을 기다립니다 */
    }
}

/** @brief ② 드라이버의 소비 감시가 켜졌는지 확인합니다 — 안 되면 켜지 않습니다. 실제 드라이버의 확인 방법은 설명서를 보세요 */
static void _StepConsumer(uint32_t now)
{
    if (_Rebooted()) {
        _CommLost(R_COMM, now);
        return;
    }
    if (s_sdo_busy || (now - s_poll_last) >= EX47_POLL_MS) {
        const ExSdoStep_t r = _Sdo(false, EX47_SIM_HB_CONSUMER_STATE, 0U, 0U, 0U, now);
        if (r == EX_SDO_OK) {
            if (s_sdo_res.value == EX47_SIM_CONSUMER_ACTIVE) {
                _Enter(EX_START_NODE, now);
                return;
            }
            s_poll_last = now;
        } else if (r == EX_SDO_FAIL) {
            _CommLost(R_COMM, now);
            return;
        } else {
            /* 응답을 기다립니다 */
        }
    }
    if ((now - s_t0) >= EX47_CONSUMER_WAIT_MS) {
        _CommLost(R_NO_RESPONSE, now);
    }
}

/** @brief ③ NMT START → 하트비트 0x05 → SYNC 시작 → SYNC 뒤에 받은 TPDO 로 확인 */
static void _StepStartNode(uint32_t now)
{
    if (_CommCheck(false, now)) {
        return;
    }
    switch (s_sub) {
    case 0U:
        if (_Nmt(XM_CANOPEN_NMT_START)) {
            s_sub = 1U;
            s_t1 = now;
        } else if ((now - s_t0) >= EX47_SEND_LIMIT_MS) {
            _CommLost(R_COMM, now);
        } else {
            /* 다음 주기에 다시 */
        }
        break;
    case 1U:
        if (XM_CanOpenGetNodeState(EX47_NODE_ID) == XM_CANOPEN_NODE_OPERATIONAL) {
            XmCanFrame_t f;
            s_tpdo_seq = XM_CanReadLatestById(ID_TPDO1, &f) ? f.seq : 0U;   /* 시작 전에 받은 것은 세지 않습니다 */
            _SetSync(true);
            s_sub = 2U;
            s_t1 = now;
        } else if ((now - s_t1) >= EX47_NMT_STATE_WAIT_MS) {
            _CommLost(R_NO_RESPONSE, now);
        } else {
            /* 기다립니다 */
        }
        break;
    default:
        if (_ReadTpdo()) {
            _Enter(EX_ENABLE, now);
        } else if ((now - s_t1) >= EX47_TPDO_FIRST_WAIT_MS) {
            _CommLost(R_NO_TPDO, now);
        } else {
            /* 기다립니다 */
        }
        break;
    }
}

/** @brief ④ 새 TPDO 마다 상태 단어를 보고 다음 제어 단어를 RPDO 로 보냅니다. 목표는 켜진 뒤에만 */
static void _StepEnable(uint32_t now)
{
    if (_CommCheck(true, now)) {
        return;
    }
    const bool fresh = _ReadTpdo();
    if (_TpdoLost()) {
        _CommLost(R_COMM, now);
        return;
    }
    if (!fresh) {
        return;
    }
    if ((s_statusword & SW_STATE_MASK) == SW_OPERATION_ENABLED) {
        _Enter(EX_RUN, now);
        return;
    }
    uint16_t cw;
    if (_IsFault()) {
        if (s_fault_resets >= MAX_FAULT_RESETS) {
            s_reason = R_FAULT;
            _Enter(EX_STOPPING, now);
            return;
        }
        if (!s_fault_low_sent) {
            cw = CW_DISABLE_VOLTAGE;         /* 고장 해제는 bit 7 이 0 에서 1 로 바뀌는 순간입니다 */
        } else {
            cw = CW_FAULT_RESET;
        }
    } else {
        s_fault_low_sent = false;
        /* 스위치 온 금지 상태(bit 6)면 0x06, 그 밖이면 0x0F */
        cw = ((s_statusword & SW_SWITCH_ON_DISABLED) != 0U) ? CW_SHUTDOWN : CW_ENABLE_OPERATION;
    }
    const XmCanResult_t sr = _SendRpdo(cw, 0);
    if (sr == XM_CAN_OK) {
        if (cw == CW_DISABLE_VOLTAGE) {
            s_fault_low_sent = true;
        } else if (cw == CW_FAULT_RESET) {
            s_fault_resets++;
            s_fault_low_sent = false;
        } else {
            /* 다음 TPDO 로 확인합니다 */
        }
    } else if (sr != XM_CAN_BUSY) {
        _CommLost(R_COMM, now);
        return;
    } else {
        /* 잠깐 막힘 — 다음 TPDO 에 다시 */
    }
    if ((now - s_t0) >= EX47_ENABLE_WAIT_MS) {
        s_reason = R_NOT_ENABLED;
        _Enter(EX_STOPPING, now);
    }
}

/** @brief ⑤ 새 TPDO 를 받은 주기에 RPDO 하나 — 새 TPDO 가 없으면 보내지 않습니다 */
static void _StepRun(uint32_t now)
{
    if (_CommCheck(true, now)) {
        return;
    }
    const bool fresh = _ReadTpdo();
    if (_TpdoLost()) {
        _CommLost(R_COMM, now);
        return;
    }
    if (!fresh) {
        return;
    }
    if ((s_statusword & SW_STATE_MASK) != SW_OPERATION_ENABLED) {
        s_reason = _IsFault() ? R_FAULT : R_NOT_ENABLED;
        _Enter(EX_STOPPING, now);
        return;
    }
    const XmCanResult_t sr = _SendRpdo(CW_ENABLE_OPERATION, EX47_TARGET_VELOCITY);
    if (sr != XM_CAN_OK && sr != XM_CAN_BUSY) {
        _CommLost(R_COMM, now);              /* 버스 복구 중 · 거부 — BUSY 는 다음 TPDO 에 다시 */
    }
}

/**
 * @brief ⑥ RPDO 0x06 → 동작 상태를 벗어난 것을 상태 단어로 확인 → SYNC 끔. XM10 하트비트는 그대로(통신은 정상)
 * @note  s_reason 은 들어올 때 정해져 있습니다. 확인하지 못하면 통신 이상으로 끝냅니다
 */
static void _StepStopping(uint32_t now)
{
    if (_CommCheck(true, now)) {
        return;
    }
    const bool fresh = _ReadTpdo();
    if (_TpdoLost()) {
        _CommLost(R_COMM, now);
        return;
    }
    if (fresh && (s_statusword & SW_STATE_MASK) != SW_OPERATION_ENABLED) {
        const ExReason_t why = s_reason;
        _SetSync(false);
        _Halt(why, now);
        return;
    }
    if ((now - s_t0) >= EX47_STOP_WAIT_MS) {
        _CommLost(R_NOT_STOPPED, now);       /* 마지막 0x06 은 여기서 한 번만 보냅니다 */
        return;
    }
    if (fresh) {
        const XmCanResult_t sr = _SendRpdo(CW_SHUTDOWN, 0);
        if (sr != XM_CAN_OK && sr != XM_CAN_BUSY) {
            _CommLost(R_COMM, now);
        }
    }
    /* 다음 TPDO 로 확인합니다 */
}

/** @brief 하트비트 LOST · 드라이버가 다시 켜짐 · (need_operational 이면) OPERATIONAL 이 아님이면 통신 이상. 멈췄으면 true */
static bool _CommCheck(bool need_operational, uint32_t now)
{
    const XmCanOpenNodeState_t ns = XM_CanOpenGetNodeState(EX47_NODE_ID);
    if (ns == XM_CANOPEN_NODE_LOST || _Rebooted() || (need_operational && ns != XM_CANOPEN_NODE_OPERATIONAL)) {
        _CommLost(R_COMM, now);
        return true;
    }
    return false;
}

static bool _Rebooted(void)
{
    return XM_CanOpenGetBootupCount(EX47_NODE_ID) != s_bootup_gen;
}

/** @brief TPDO 가 끊겼는가 — 읽은 때가 아니라 받은 때로 잽니다. 늦게 읽은 옛 값으로 RPDO 를 보내거나 시간을 되돌리지 않습니다 */
static bool _TpdoLost(void)
{
    return (uint32_t)(XM_GetTick() - s_tpdo_tick) >= EX47_TPDO_LOST_MS;
}

/** @brief 지난번 뒤로 새로 받은 TPDO 가 있으면 풀어 두고 true */
static bool _ReadTpdo(void)
{
    XmCanFrame_t f;

    if (!XM_CanReadLatestById(ID_TPDO1, &f) || f.seq == s_tpdo_seq || f.len < PDO_LEN) {
        return false;
    }
    s_tpdo_seq = f.seq;
    s_tpdo_tick = f.tick_ms;
    s_statusword = (uint16_t)f.data[0] | ((uint16_t)f.data[1] << 8);
    s_actual = (int32_t)((uint32_t)f.data[2] | ((uint32_t)f.data[3] << 8) | ((uint32_t)f.data[4] << 16) | ((uint32_t)f.data[5] << 24));
    return true;
}

static bool _IsFault(void)
{
    const uint16_t f = (uint16_t)(s_statusword & SW_FAULT_MASK);
    return f == SW_FAULT || f == SW_FAULT_REACTION;
}

static XmCanResult_t _SendRpdo(uint16_t controlword, int32_t target)
{
    const uint32_t t = (uint32_t)target;
    const uint8_t d[PDO_LEN] = {
        (uint8_t)controlword, (uint8_t)(controlword >> 8),
        (uint8_t)t, (uint8_t)(t >> 8), (uint8_t)(t >> 16), (uint8_t)(t >> 24),
    };
    const XmCanResult_t r = XM_CanSendFrame(ID_RPDO1, d, PDO_LEN);
    if (r != XM_CAN_OK) {
        s_send_last = r;
    }
    return r;
}

/**
 * @brief SDO 한 걸음(Ex.46 과 같음) — 낼 수 있으면 내고, 결과가 오면 EX_SDO_OK / EX_SDO_FAIL 을 돌려줍니다.
 * @details 같은 요청을 매 주기 불러도 됩니다. 앞 단계의 요청이 아직 끝나지 않았으면 그 결과를 받아 버린 뒤 냅니다.
 *          BUSY(쉬는 시간 · 송신 대기 비우기)는 EX47_SEND_LIMIT_MS 까지 다시 시도합니다.
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
        const XmCanResult_t sr = write ? XM_CanOpenSdoWrite(EX47_NODE_ID, index, sub, value, size)
                                       : XM_CanOpenSdoRead(EX47_NODE_ID, index, sub);
        if (sr == XM_CAN_OK) {
            s_sdo_busy = true;
            s_sdo_write = write;
            s_sdo_index = index;
            s_sdo_sub = sub;
            s_sdo_value = value;
            return EX_SDO_WAIT;
        }
        s_send_last = sr;
        if (sr == XM_CAN_BUSY && (now - s_sdo_since) < EX47_SEND_LIMIT_MS) {
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
    return (st == XM_CANOPEN_SDO_DONE) ? EX_SDO_OK : EX_SDO_FAIL;
}

static bool _Nmt(uint8_t command)
{
    const XmCanResult_t r = XM_CanOpenSendNmt(command, EX47_NODE_ID);
    if (r != XM_CAN_OK) {
        s_send_last = r;
    }
    return r == XM_CAN_OK;
}

static void _Enter(ExState_t st, uint32_t now)
{
    if (st == EX_CONFIG) {
        s_bootup_gen = XM_CanOpenGetBootupCount(EX47_NODE_ID);   /* ② 부터 이 부팅에 쓰고 확인합니다 */
    }
    s_state = st;
    s_sub = 0U;
    s_t0 = now;
    s_t1 = now;
    s_poll_last = now - EX47_POLL_MS;        /* 첫 읽기는 바로 */
    s_sdo_since = now;
    _Report();
}

/** @brief 멈춤 — 하트비트는 _CommLost 만 끕니다(드라이버가 지금 켜져 있지 않거나, 버튼 정지를 확인했습니다) */
static void _Halt(ExReason_t why, uint32_t now)
{
    s_reason = why;
    _Enter(EX_HALTED, now);
}

/**
 * @brief 통신 이상 — (a) RPDO 를 멈추고 (b) XM10 하트비트 · SYNC 를 끄고 (c) 0x06 을 한 번 보내 봅니다(결과는 보지 않습니다).
 *        드라이버의 하트비트 소비 감시가 XM10 을 잃은 것을 보고 스스로 멈춥니다(소비 시간 · 상실 반응은 드라이버에서 정합니다)
 */
static void _CommLost(ExReason_t why, uint32_t now)
{
    const bool pdo_live = s_sync_on && !_Rebooted();   /* SYNC 전 · 다시 켜졌으면 PDO 가 켜져 있지 않습니다 */
    _SetHeartbeat(false);
    _SetSync(false);
    if (pdo_live) {
        (void)_SendRpdo(CW_SHUTDOWN, 0);
    }
    _Halt(why, now);
}

static void _SetHeartbeat(bool on)
{
    if (on != s_hb_on) {
        const XmCanResult_t r = XM_CanEnableHeartbeat(on ? EX47_XM_NODE_ID : 0U, on ? EX47_XM_HB_PERIOD_MS : 0U);
        if (r == XM_CAN_OK) {
            s_hb_on = on;
        } else {
            s_send_last = r;
        }
    }
}

static void _SetSync(bool on)
{
    if (on != s_sync_on) {
        XM_CanEnableSync(on);
        s_sync_on = on;
    }
}

static void _Report(void)
{
    if (!XM_IsUsbStreamConnected()) {
        return;
    }
    char msg[REPORT_MSG_SIZE];
    if (s_state == EX_HALTED) {
        snprintf(msg, sizeof(msg), "[Ex47] %s — %s (SDO %u · abort 0x%08lX · 보내기 %u · 상태 단어 0x%04X)\r\n",
                 k_state_name[s_state], k_reason_name[s_reason], (unsigned)s_sdo_last,
                 (unsigned long)s_sdo_res.abort_code, (unsigned)s_send_last, (unsigned)s_statusword);
    } else {
        snprintf(msg, sizeof(msg), "[Ex47] %s\r\n", k_state_name[s_state]);
    }
    XM_SendUsbDebugMessage(msg);
}

static void _Stream(uint32_t now)
{
    if ((now - s_stream_last) < EX47_STREAM_PERIOD_MS) {
        return;
    }
    s_stream_last = now;
    s_usb[0] = (float)s_state;
    s_usb[1] = (s_state == EX_RUN) ? (float)EX47_TARGET_VELOCITY : 0.0f;
    s_usb[2] = (float)s_actual;
    s_usb[3] = (float)s_statusword;
    XM_SendUsbDataWithId(s_usb, sizeof(s_usb), USB_MODULE_ID);
}

static void _UpdateLeds(void)
{
    int8_t led1;
    if (s_state == EX_IDLE) {
        led1 = 0;
    } else if (s_state == EX_RUN) {
        led1 = 2;
    } else if (s_state == EX_STOPPING) {
        led1 = 3;
    } else if (s_state == EX_HALTED) {
        led1 = (s_reason == R_BUTTON) ? 4 : 5;
    } else {
        led1 = 1;
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
    const int8_t led2 = s_hb_on ? 1 : 0;
    if (led2 != s_led2_shown) {
        s_led2_shown = led2;
        XM_SetLedState(XM_LED_2, s_hb_on ? XM_ON : XM_OFF);
    }
}
