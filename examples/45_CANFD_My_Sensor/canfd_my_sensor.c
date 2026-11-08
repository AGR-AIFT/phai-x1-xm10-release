/**
 ******************************************************************************
 * @file    canfd_my_sensor.c
 * @author  HyundoKim
 * @brief   [중급] CAN-FD 로 연결한 내 장치(센서)와 주고받기 — 데이터 받기 · 명령 보내기 · 통계
 * @version 1.0
 * @date    Oct 29, 2026
 *
 * @details
 * 직접 만든 CAN-FD 장치(예: STM32G474 보드)를 XM10 의 CAN-FD #2 포트에 연결해
 *   ① 장치가 보내는 주기 데이터(카운터 + ADC 2채널)를 받아 PC 그래프로 보고
 *   ② 100 ms 마다 장치에 명령(장치 LED 켜기 · 끄기)을 보내고
 *   ③ 1 초마다 통계를 PC 로 보냅니다.
 *   ④ 장치 API 밖의 메시지도 ID 와 데이터만으로 보내고 받습니다(자유 송수신 — 아래 _SendFreeFrame · _ReadFreeFrames).
 *
 *  [장치가 지킬 것 — 장치 번호 n = MY_DEVICE_ID]
 *      버스        CAN-FD, 11비트 ID, 데이터 구간 고속 전송(BRS) · 1 Mbit/s / 5 Mbit/s · 샘플 포인트 75 %
 *      주기 데이터 ID 0x180 + n, 길이 16, 최대 1 kHz
 *                    바이트 0~3   카운터(uint32, 작은 쪽 바이트부터) — 보낼 때마다 +1
 *                    바이트 4~5   ADC 채널 0 (uint16)
 *                    바이트 6~7   ADC 채널 1 (uint16)
 *                    바이트 8~15  0
 *      살아있음    ID 0x700 + n, 1 바이트 0x05, 1 초마다 — 3 초 동안 없으면 XM10 은 '끊김' 으로 봅니다.
 *                  켜질 때 0x00 을 한 번 보내면(재시작 알림, 선택) XM10 은 그동안 받아 둔 값을 버립니다.
 *      명령(받기)  ID 0x200 + n, 2 바이트 [순번, LED(0 = 끔 · 1 = 켬)]
 *                  순번이 지난번보다 1~127 앞서면 새 명령, 아니면 버립니다(중복 · 순서 뒤바뀜 거르기 —
 *                  XM10 쪽에서 같은 판단을 하려면 XM_CanSeqIsNewer). 순번은 유효기간이 아닙니다.
 *
 *  [배선]  XM10 CAN-FD #2 포트의 CAN_H · CAN_L · GND 를 장치 트랜시버의 같은 이름에 연결합니다.
 *          버스 양 끝에는 종단 저항(120 Ω)이 있어야 합니다 — XM10 쪽은 하드웨어 문서를 확인하세요.
 *
 *  [확인 방법]
 *      LED1  켜짐 = 장치 연결 · 느린 점멸 = 끊김 · 빠른 점멸 = 장치 번호 등록 실패(MY_DEVICE_ID 확인)
 *      LED2  장치에 보내는 LED 명령(버튼 1 을 누를 때마다 켬 · 끔)
 *      PC    xm10 도구 — 0xF0 채널에 ADC0 · ADC1 · 건너뛴 값 수 · 연결(0/1) 그래프,
 *            1 초마다 "[CAN] ..." 통계 문장
 *
 * @note 받은 데이터는 XM10 이 장치별로 최신값 하나씩만 보관합니다. 1 kHz 로 오는 값을 매 주기 읽어도 가끔 읽기 전에
 *       새 값으로 바뀔 수 있습니다(통계 overwrite). 그래서 '건너뛴 값 수' 는 버스에서 잃은 수 + 읽기 전에 바뀐 수입니다.
 * @note 보내기 결과는 꼭 `== XM_CAN_OK` 로 확인하세요(XM_CAN_OK 가 0 입니다). XM_CAN_OK 는 '보내기 큐에 실림' 이지
 *       장치가 받았다는 뜻이 아닙니다.
 * @warning **본 예제는 XM10 Rev 2.0 전용입니다.** Rev 1.1 은 CAN-FD #2 포트로 내 장치와 주고받는 기능을 지원하지 않습니다.
 *          같은 포트에 센서 허브(IMU · EMG · FES)를 함께 연결해도 됩니다 — 장치 번호만 겹치지 않게 쓰세요(XM_CanIsDeviceIdAllowed).
 *
 * @see     xm_api_canbus.h (전체 규칙 · 반환값)
 * @see     Ex.41 imu_hub_dashboard.c (0xF0 사용자 채널 스트리밍)
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <stdio.h>   /* snprintf — 통계를 문장으로 만들 때 */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define MY_DEVICE_ID          (0x20U)   /**< 내 장치 번호 — 장치 펌웨어와 같게(쓸 수 있는 번호: XM_CanIsDeviceIdAllowed) */
#define MY_USE_SYNC           (false)   /**< true = 1 kHz 동기 신호(ID 0x080)를 내 장치에도 보냅니다 */

#define MY_STREAM             (1U)      /**< 주기 데이터 1(ID 0x180 + n) */
#define MY_DATA_MIN_LEN       (8U)      /**< 카운터 4 + ADC 2 × 2 — 이보다 짧으면 버립니다 */
#define MY_CMD_LEN            (2U)      /**< 명령 [순번, LED] */

#define STREAM_PERIOD_MS      (10U)     /**< PC 그래프 100 Hz — USB 대역 절약 */
#define CMD_PERIOD_MS         (100U)    /**< 명령 10 Hz */
#define STATS_PERIOD_MS       (1000U)   /**< 통계 · 자유 송신 1 Hz */

#define FREE_TX_ID            (0x321U)  /**< 자유 송신 ID — 보낼 수 있는 ID 인지는 XM_CanIsIdAllowed 로 확인합니다 */

#define USB_MODULE_ID         (0xF0U)
#define USB_CH_COUNT          (4U)      /**< ADC0 · ADC1 · 건너뛴 값 수 · 연결 */
#define STATS_MSG_SIZE        (400U)

#define LED_BLINK_SLOW_MS     (1000U)
#define LED_BLINK_FAST_MS     (100U)

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/* 장치 API */
static bool     s_attached;          /**< 장치 번호 등록 성공 */
static bool     s_have_seq;          /**< 최신값을 한 번이라도 읽었나 */
static uint32_t s_last_seq;          /**< 지난번에 읽은 최신값의 seq — 같으면 새 메시지가 아닙니다 */
static bool     s_have_counter;
static uint32_t s_last_counter;
static uint32_t s_skipped;           /**< 건너뛴 값 수(카운터의 빈 번호) */
static uint32_t s_short_count;       /**< 너무 짧아 버린 메시지 수 */
static uint16_t s_adc[2];

static uint8_t  s_cmd_seq;           /**< 명령 순번 — 수락될 때만 +1 */
static uint8_t  s_led_cmd;           /**< 장치에 보낼 LED 값(0/1) */

/* 자유 송수신 */
static uint32_t s_free_tx_count;
static uint32_t s_free_rx_count;
static uint32_t s_free_gap;          /**< 큐 seq 의 빈 번호 합 = 큐가 가득해 버린 수 */
static bool     s_free_have_seq;
static uint32_t s_free_last_seq;
static uint16_t s_free_last_id;

/* 주기 · 표시 */
static uint32_t s_stream_tick;
static uint32_t s_cmd_tick;
static uint32_t s_stats_tick;
static int8_t   s_led1_shown = -1;   /**< LED1 에 마지막으로 그린 상태(-1 = 아직 없음) — 바뀔 때만 다시 그립니다 */
static int8_t   s_led2_shown = -1;

static float    s_usb[USB_CH_COUNT];

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void     _ReadMySensor(void);
static void     _ReadFreeFrames(void);
static void     _SendCommand(void);
static void     _SendFreeFrame(void);
static void     _StreamToPc(void);
static void     _SendStatsToPc(void);
static void     _UpdateLeds(void);
static bool     _Every(uint32_t* last, uint32_t now, uint32_t period_ms);
static uint32_t _GetU32(const uint8_t* p);
static uint16_t _GetU16(const uint8_t* p);
static void     _PutU32(uint8_t* p, uint32_t v);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void Control_Setup(void)
{
    /* 1) 장치 번호 등록 — Control_Setup() 안에서만 됩니다. 실패면 LED1 빠른 점멸 */
    s_attached = XM_CanAttachDevice(MY_DEVICE_ID);

    /* 2) (선택) 1 kHz 동기 신호. 장치는 샘플링 정렬에만 쓰고, 없으면 자기 주기로 계속 보내게 하세요 */
    if (MY_USE_SYNC) {
        XM_CanEnableSync(true);
    }

    /* 3) xm10 도구용 그래프 이름 — 보내는 float 개수(USB_CH_COUNT)와 같게 */
    XM_SetUsbCustomMeta(USB_MODULE_ID,
        "[{\"name\":\"ADC0\"},{\"name\":\"ADC1\"},{\"name\":\"Skipped\"},{\"name\":\"Connected\"}]");

    const uint32_t now = XM_GetTick();
    s_stream_tick = now;
    s_cmd_tick    = now;
    s_stats_tick  = now;
}

void Control_Loop(void)
{
    const uint32_t now = XM_GetTick();

    /* 버튼 1 을 누를 때마다 장치 LED 명령을 바꿉니다 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_led_cmd = (uint8_t)(s_led_cmd ^ 1U);
    }

    _ReadMySensor();     /* 새 값이 왔을 때만 파싱 */
    _ReadFreeFrames();   /* 자유 수신 큐는 매 주기 비웁니다 */
    _UpdateLeds();

    if (_Every(&s_stream_tick, now, STREAM_PERIOD_MS)) {
        _StreamToPc();
    }
    if (_Every(&s_cmd_tick, now, CMD_PERIOD_MS)) {
        _SendCommand();
    }
    if (_Every(&s_stats_tick, now, STATS_PERIOD_MS)) {
        _SendFreeFrame();
        _SendStatsToPc();
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 장치의 주기 데이터 최신값을 읽어, seq 가 바뀌었을 때만 파싱합니다.
 * @details XM_CanReadLatest 는 같은 값을 다시 읽어도 true 입니다 — 새 값인지는 seq 로 가립니다.
 *          끊긴 뒤에도 옛 값이 true 로 읽히므로 연결 여부는 XM_CanIsConnected 로 따로 봅니다.
 */
static void _ReadMySensor(void)
{
    XmCanFrame_t f;
    if (!XM_CanReadLatest(MY_DEVICE_ID, MY_STREAM, &f)) {
        return;                              /* 아직 못 받음 · 재시작 알림 직후 · 이번엔 못 읽음 */
    }
    if (s_have_seq && (f.seq == s_last_seq)) {
        return;                              /* 지난번과 같은 메시지 */
    }
    s_have_seq = true;
    s_last_seq = f.seq;

    if (f.len < MY_DATA_MIN_LEN) {
        s_short_count++;
        return;
    }

    const uint32_t counter = _GetU32(&f.data[0]);
    /* 카운터가 2 이상 뛰면 그 사이 값을 건너뛴 것. 작아지면 장치가 다시 켜진 것이라 세지 않습니다 */
    if (s_have_counter && (counter > s_last_counter)) {
        s_skipped += counter - s_last_counter - 1U;
    }
    s_have_counter = true;
    s_last_counter = counter;

    s_adc[0] = _GetU16(&f.data[4]);
    s_adc[1] = _GetU16(&f.data[6]);
}

/**
 * @brief 자유 송수신 — 장치 API 가 가져가지 않은 메시지를 받은 순서대로 꺼냅니다.
 * @details 큐 깊이는 64 입니다. 가득 차면 새 메시지를 버리고, 버린 것도 seq 번호를 씁니다 → 빈 번호 = 버린 수.
 */
static void _ReadFreeFrames(void)
{
    XmCanFrame_t f;
    while (XM_CanReadFrame(&f)) {
        if (s_free_have_seq && ((f.seq - s_free_last_seq) > 1U)) {
            s_free_gap += f.seq - s_free_last_seq - 1U;
        }
        s_free_have_seq = true;
        s_free_last_seq = f.seq;
        s_free_rx_count++;
        s_free_last_id = f.id;
    }
}

/**
 * @brief 장치에 명령 [순번, LED] 를 보냅니다(100 ms 마다).
 * @details 끊긴 동안에는 XM_CAN_NOT_CONNECTED 로 거부됩니다. 순번은 수락(XM_CAN_OK)됐을 때만 올립니다 —
 *          거부된 명령은 버스에 나가지 않았으므로 다음에 같은 순번으로 다시 보내면 됩니다.
 */
static void _SendCommand(void)
{
    if (!s_attached) {
        return;
    }
    const uint8_t cmd[MY_CMD_LEN] = { s_cmd_seq, s_led_cmd };
    if (XM_CanSend(MY_DEVICE_ID, cmd, MY_CMD_LEN) == XM_CAN_OK) {
        s_cmd_seq++;
    }
    /* 실패 이유(BUSY · RECOVERY · NOT_CONNECTED …)는 통계의 last_send_fail 로 봅니다 */
}

/**
 * @brief 자유 송신 — 프로토콜 없이 ID 와 데이터만으로 하나 보냅니다(1 초마다, 4 바이트 카운터).
 */
static void _SendFreeFrame(void)
{
    if (!XM_CanIsIdAllowed(FREE_TX_ID)) {
        return;                              /* 이 모드에서 보낼 수 없는 ID */
    }
    uint8_t data[4];
    _PutU32(data, s_free_tx_count);
    if (XM_CanSendFrame(FREE_TX_ID, data, sizeof(data)) == XM_CAN_OK) {
        s_free_tx_count++;
    }
}

/** @brief 0xF0 사용자 채널로 [ADC0, ADC1, 건너뛴 값 수, 연결] 을 보냅니다. */
static void _StreamToPc(void)
{
    s_usb[0] = (float)s_adc[0];
    s_usb[1] = (float)s_adc[1];
    s_usb[2] = (float)s_skipped;
    s_usb[3] = XM_CanIsConnected(MY_DEVICE_ID) ? 1.0f : 0.0f;
    XM_SendUsbDataWithId(s_usb, sizeof(s_usb), USB_MODULE_ID);
}

/**
 * @brief 1 초마다 장치별 통계와 포트 공통 통계를 한 줄씩 PC 로 보냅니다.
 * @details 이름은 XmCanDeviceStats_t · XmCanBusStats_t 의 멤버 이름과 같습니다.
 *   overwrite   읽기 전에 새 값으로 바뀐 수(unread_overwrite_count)
 *   last_fail   마지막 XM_CanSend 실패 결과(XmCanResult_t, 0 = 아직 없음)
 *   pending     지금 하드웨어에 남은 송신 수 — 계속 크면 받아 주는 장치가 없거나 버스가 바쁩니다
 *   status      bit0 bus-off · bit1 오류 경고 · bit2 오류 수동
 */
static void _SendStatsToPc(void)
{
    if (!XM_IsUsbStreamConnected()) {
        return;
    }
    XmCanDeviceStats_t ds;
    XmCanBusStats_t    bs;
    if (!XM_CanGetBusStats(&bs)) {
        return;
    }
    const bool have_dev = s_attached && XM_CanGetStats(MY_DEVICE_ID, &ds);   /* 등록 안 한 번호는 bad_call 로 셉니다 */

    char msg[STATS_MSG_SIZE];
    if (have_dev) {
        snprintf(msg, sizeof(msg),
                 "[CAN] dev=0x%02X connected=%d rx=%lu alive=%lu reboot=%lu overwrite=%lu skipped=%lu "
                 "send_ok=%lu send_fail=%lu last_fail=%u\r\n",
                 (unsigned)MY_DEVICE_ID, XM_CanIsConnected(MY_DEVICE_ID) ? 1 : 0,
                 (unsigned long)ds.rx_count[0], (unsigned long)ds.alive_count,
                 (unsigned long)ds.reboot_count, (unsigned long)ds.unread_overwrite_count,
                 (unsigned long)s_skipped, (unsigned long)ds.send_accept_count,
                 (unsigned long)ds.send_fail_count, (unsigned)ds.last_send_fail);
        XM_SendUsbDebugMessage(msg);
    }
    snprintf(msg, sizeof(msg),
             "[CAN] mode=%u rx=%lu free_rx=%lu free_tx=%lu queue_drop=%lu gap=%lu last_id=0x%03X "
             "pending=%lu tec=%u rec=%u status=0x%02X bad_call=%lu\r\n",
             (unsigned)bs.bus_mode, (unsigned long)bs.rx_frame_count, (unsigned long)s_free_rx_count,
             (unsigned long)s_free_tx_count, (unsigned long)bs.rx_queue_drop_count, (unsigned long)s_free_gap,
             (unsigned)s_free_last_id, (unsigned long)bs.tx_pending_count, (unsigned)bs.tec, (unsigned)bs.rec,
             (unsigned)bs.bus_status, (unsigned long)bs.bad_call_count);
    XM_SendUsbDebugMessage(msg);
}

/** @brief LED1 = 연결 상태, LED2 = 장치에 보내는 LED 명령 — 바뀔 때만 다시 그립니다. */
static void _UpdateLeds(void)
{
    const int8_t led1 = !s_attached ? 2 : (XM_CanIsConnected(MY_DEVICE_ID) ? 1 : 0);
    if (led1 != s_led1_shown) {
        s_led1_shown = led1;
        if (led1 == 1) {
            XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0U);
        } else if (led1 == 0) {
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, LED_BLINK_SLOW_MS);
        } else {
            XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, LED_BLINK_FAST_MS);
        }
    }

    const int8_t led2 = (int8_t)s_led_cmd;
    if (led2 != s_led2_shown) {
        s_led2_shown = led2;
        XM_SetLedState(XM_LED_2, (led2 != 0) ? XM_ON : XM_OFF);
    }
}

/** @brief period_ms 가 지났으면 true 를 돌려주고 기준 시각을 옮깁니다(XM_GetTick 이 한 바퀴 돌아도 맞습니다). */
static bool _Every(uint32_t* last, uint32_t now, uint32_t period_ms)
{
    if ((now - *last) < period_ms) {
        return false;
    }
    *last = now;
    return true;
}

static uint32_t _GetU32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t _GetU16(const uint8_t* p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void _PutU32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}
