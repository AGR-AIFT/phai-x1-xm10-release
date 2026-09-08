/**
 ******************************************************************************
 * @file    external_uart_pingpong.c
 * @author  HyundoKim
 * @brief   [중급] XM10 두 대를 External UART 로 직결해 서로 데이터 주고받기
 * @version 1.0
 * @date    Sep 08, 2026
 *
 * @details
 * 같은 펌웨어를 XM10 두 대에 굽고, 두 보드의 External UART(PD5/PD6)를 서로
 * 교차 결선하면 1초에 10번씩 서로에게 메시지를 보내고 받습니다.
 *
 *  [하드웨어 설정 — .ioc 고정값. 상대 장비를 여기 맞춘다]
 *      USART2 / TX=PD5(EXT_UART_TX) / RX=PD6(EXT_UART_RX) / 3.3V 로직
 *      921600 8N1, 흐름제어 없음 (속도만 XM_SetExternalUartBaudrate 로 변경 가능)
 *      수신 = DMA + IDLE 라인 검출, RX 버퍼 256B / TX 버퍼 128B(1회 송신 상한)
 *      GPIO 풀업/풀다운 없음 — 상대가 꺼지면 RX 플로팅으로 잡음이 데이터로 보일 수 있다
 *
 *  [배선]  보드 A                      보드 B
 *          PD5 (TX)  ──────────────→  PD6 (RX)
 *          PD6 (RX)  ←──────────────  PD5 (TX)
 *          GND       ──────────────   GND      ← 공통 GND 필수
 *
 *          ⚠ 3.3V 로직입니다. 5V 장비를 직결하지 마세요(레벨 시프터 필요).
 *          ⚠ TX↔RX 를 교차해야 합니다. TX-TX 로 연결하면 아무것도 안 옵니다.
 *          ⚠ Rev1.1 보드에서는 이 배선 금지 — External UART 가 없고 PD6 이
 *            USB_PWR_ON(USB 전원 제어 출력)이라 출력끼리 맞부딪친다.
 *
 *  [프로토콜] 이 예제가 임의로 정한 5바이트 고정 프레임입니다.
 *             XM10 이 강제하는 형식이 아니라, "프레임 경계를 스스로 찾는 법"의
 *             본보기입니다. 여러분의 장비 형식에 맞게 바꾸세요.
 *
 *      +------+------+---------+---------+----------+
 *      | 0xA5 | 0x5A | seq(hi) | seq(lo) | checksum |  = 5 bytes
 *      +------+------+---------+---------+----------+
 *        헤더 2바이트        16비트 카운터      XOR
 *
 *  [확인 방법] 디버거 Live Expressions 에 아래를 등록하세요.
 *      g_tx_count      계속 증가          → 내가 보내고 있다
 *      g_rx_frame_ok   계속 증가          → 상대 것이 도착하고 파싱된다
 *      g_rx_bad_crc    0 이어야 정상      → 0 이 아니면 배선/속도 의심
 *      g_peer_seq      상대 보드의 카운터  → 실제로 값이 건너온다
 *
 * @note 흐름: Control_Setup() 등록 → 콜백은 **복사만** → Control_Loop() 에서 파싱.
 *       콜백이 도는 곳은 XM10 내부 공유 수신 태스크이고, 그 태스크는 발바닥
 *       센서(GRF) 1kHz 수신도 함께 처리합니다. 콜백에서 파싱까지 하면 GRF 가
 *       밀립니다 — 그래서 링버퍼로 넘기고 제어 루프에서 처리합니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define FRAME_SYNC0         (0xA5U)
#define FRAME_SYNC1         (0x5AU)
#define FRAME_LEN           (5U)        /**< sync0 sync1 seq_hi seq_lo checksum */

#define RX_RING_SIZE        (256U)      /**< 2의 거듭제곱 — 마스크 연산용 */
#define RX_RING_MASK        (RX_RING_SIZE - 1U)

#define SEND_PERIOD_MS      (100U)      /**< 10Hz 송신 */
#define REARM_PERIOD_MS     (100U)      /**< 수신 재무장 점검 주기 */

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES  — Live Expressions 로 관찰
 *-----------------------------------------------------------
 */

volatile uint32_t g_tx_count;        /**< 송신 성공 횟수 */
volatile uint32_t g_tx_busy;         /**< 송신 스킵(직전 전송 미완) 횟수 — 소수면 정상 */
volatile uint32_t g_rx_bytes;        /**< 콜백이 받은 총 바이트 */
volatile uint32_t g_rx_frame_ok;     /**< 체크섬까지 통과한 프레임 수 */
volatile uint32_t g_rx_bad_crc;      /**< 체크섬 불일치 — 0 이어야 정상 */
volatile uint32_t g_rx_ring_drop;    /**< 링버퍼 가득참 — 0 이어야 정상 */
volatile uint16_t g_peer_seq;        /**< 마지막으로 받은 상대 카운터 */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/* 수신 링버퍼.
 * writer = 수신 콜백(공유 RxTask) / reader = Control_Loop(UserTask) 단 하나씩이라
 * 락 없는 SPSC 링으로 충분하다. head/tail 은 각자 한쪽만 쓴다. */
static volatile uint8_t  s_rx_ring[RX_RING_SIZE];
static volatile uint16_t s_rx_head;   /**< writer(콜백)만 증가 */
static volatile uint16_t s_rx_tail;   /**< reader(루프)만 증가 */

/* 프레임 파서 상태 (Control_Loop 컨텍스트 전용) */
static uint8_t  s_parse_buf[FRAME_LEN];
static uint8_t  s_parse_len;

static uint16_t s_my_seq;
static uint32_t s_tick_ms;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void OnSerialRx(const uint8_t* data, uint32_t len);
static void SendPingFrame(void);
static void DrainAndParse(void);
static void HandleFrame(const uint8_t* f);
static uint8_t Checksum(const uint8_t* f);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void Control_Setup(void)
{
    /* 1) 속도를 먼저 맞춘다. 두 보드가 같은 값이어야 한다.
     *    (부팅 기본값도 921600 이라 이 줄이 없어도 동작하지만, 상대 장비에
     *     맞춰 바꾸는 자리를 보여주기 위해 명시한다.) */
    XM_SetExternalUartBaudrate(XM_UART_BAUD_921600);

    /* 2) 수신 콜백 등록. 이 시점부터 상대 데이터가 들어오기 시작한다. */
    XM_AttachExternalUart(OnSerialRx);
}

void Control_Loop(void)
{
    s_tick_ms++;   /* Control_Loop 는 1ms 주기다 */

    /* (1) 도착한 바이트를 꺼내 프레임으로 조립 — 매 틱 수행 */
    DrainAndParse();

    /* (2) 100ms 마다 한 번 보낸다 */
    if ((s_tick_ms % SEND_PERIOD_MS) == 0U) {
        SendPingFrame();
    }

    /* (3) 100ms 마다 수신이 죽지 않았는지 점검 (케이블 탈착·노이즈 대비).
     *     정상일 때는 아무 일도 하지 않는 값싼 호출이다. */
    if ((s_tick_ms % REARM_PERIOD_MS) == 0U) {
        XM_EnsureExternalUartRxArmed();
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief [공유 RxTask 컨텍스트] 수신 콜백 — 복사만 하고 즉시 리턴한다.
 * @warning 여기에 파싱·printf·대기 를 넣지 말 것. GRF 1kHz 수신이 밀린다.
 */
static void OnSerialRx(const uint8_t* data, uint32_t len)
{
    g_rx_bytes += len;

    uint16_t head = s_rx_head;
    for (uint32_t i = 0U; i < len; i++) {
        uint16_t next = (uint16_t)((head + 1U) & RX_RING_MASK);
        if (next == s_rx_tail) {        /* 가득 참 — 루프가 못 따라오고 있다 */
            g_rx_ring_drop++;
            break;
        }
        s_rx_ring[head] = data[i];
        head = next;
    }
    s_rx_head = head;                   /* 데이터를 다 쓴 뒤 마지막에 공개 */
}

/**
 * @brief [Control_Loop 컨텍스트] 링버퍼를 비우며 프레임 경계를 찾는다.
 * @details 한 번의 콜백이 프레임 하나가 아니므로(쪼개져 오거나 붙어 온다),
 *          바이트 상태기계로 헤더 2바이트를 찾아 동기를 잡는다.
 */
static void DrainAndParse(void)
{
    while (s_rx_tail != s_rx_head) {
        uint8_t b = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) & RX_RING_MASK);

        /* 헤더 동기 맞추기 */
        if (s_parse_len == 0U) {
            if (b != FRAME_SYNC0) continue;             /* 첫 바이트가 아니면 버린다 */
        } else if (s_parse_len == 1U) {
            if (b != FRAME_SYNC1) {
                /* 두 번째가 틀렸다 — 이 바이트가 새 프레임의 시작일 수 있다 */
                s_parse_len = (b == FRAME_SYNC0) ? 1U : 0U;
                if (s_parse_len == 1U) s_parse_buf[0] = b;
                continue;
            }
        }

        s_parse_buf[s_parse_len++] = b;

        if (s_parse_len >= FRAME_LEN) {
            HandleFrame(s_parse_buf);
            s_parse_len = 0U;
        }
    }
}

/**
 * @brief 완성된 프레임 1개 처리 (체크섬 확인 후 값 반영).
 */
static void HandleFrame(const uint8_t* f)
{
    if (f[FRAME_LEN - 1U] != Checksum(f)) {
        g_rx_bad_crc++;
        return;
    }
    g_peer_seq = (uint16_t)(((uint16_t)f[2] << 8) | f[3]);
    g_rx_frame_ok++;
}

/**
 * @brief 프레임 1개 송신 (논블로킹).
 */
static void SendPingFrame(void)
{
    uint8_t frame[FRAME_LEN];

    frame[0] = FRAME_SYNC0;
    frame[1] = FRAME_SYNC1;
    frame[2] = (uint8_t)(s_my_seq >> 8);
    frame[3] = (uint8_t)(s_my_seq & 0xFFU);
    frame[4] = Checksum(frame);

    if (XM_SendExternalUartData(frame, FRAME_LEN)) {
        s_my_seq++;
        g_tx_count++;
    } else {
        /* 직전 DMA 송신이 아직 안 끝났다 — 에러가 아니다. 다음 주기에 다시 보낸다. */
        g_tx_busy++;
    }
}

/**
 * @brief 앞 4바이트의 XOR (체크섬 자리는 제외).
 */
static uint8_t Checksum(const uint8_t* f)
{
    uint8_t c = 0U;
    for (uint8_t i = 0U; i < (FRAME_LEN - 1U); i++) {
        c = (uint8_t)(c ^ f[i]);
    }
    return c;
}
