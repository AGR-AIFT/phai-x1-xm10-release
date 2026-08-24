/**
 ******************************************************************************
 * @file    cdc_handler.c
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 13, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "cdc_handler.h"
#include "usbd_cdc_if.h" // CDC_Transmit_FS, CDC_Register_Callbacks

#include "FreeRTOS.h"
#include "stream_buffer.h"

#include <string.h>
#include <stdatomic.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief TX Drop 기반 호스트 disconnect 감지 임계값
 * @details 연속 N 사이클(100ms 주기) 동안 drop 증가 시 streaming 자동 해제.
 *          32KB 링버퍼, 300B/ms 전송률 → 호스트 미수신 시 ~100ms 후 drop 시작.
 *          5 사이클(500ms)이면 오탐 없이 충분히 빠른 감지.
 */
#define CDC_DROP_DISCONNECT_CYCLES  5


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

// --- Non-Cacheable 영역에 Tx 링버퍼 할당 (DMA Zero-Copy를 위해 필수) ---
__attribute__((section(IOIF_USB_CDC_SECTION))) 
static uint8_t s_cdcTxRingBuffer[CDC_TX_RING_BUFFER_SIZE];

/*
 * Tx Control — Lock-Free SPSC (Single Producer Single Consumer)
 *
 * Producer: UserTask (1ms, Prio53) — CdcStream_Send()에서 head 갱신
 * Consumer: USB TxCplt ISR — _TryTransmit()에서 tail 갱신
 *
 * SPSC 제약: Producer가 1개 Task로 한정됨.
 * 여러 Task에서 CDC 전송이 필요해지면 Queue+TxTask 패턴으로 전환 필요:
 *   - 별도 TxTask 생성 (UserTask보다 낮은 Priority)
 *   - FreeRTOS Queue로 패킷 전달 (MPSC 지원)
 *   - TxTask에서 Queue Receive → CDC_Transmit_FS → Sem_Take(TxCplt)
 */
static volatile atomic_uint s_txHead = 0;
static volatile uint32_t s_txTail = 0;
static volatile atomic_bool s_isTxBusy = false;

// --- Rx Control — FreeRTOS StreamBuffer (UART Rx와 동일 패턴) ---
static StreamBufferHandle_t s_rxStreamBuffer = NULL;

static volatile atomic_bool s_isStreamingActive = false;

// Tx 실패 통계 (링버퍼 풀로 인한 드롭 횟수)
static volatile atomic_uint s_txDropCount = 0;

// Auto-Stream 모드 (true: USB 연결 시 자동 스트리밍, false: 명령 대기)
static volatile atomic_bool s_isAutoStreamEnabled = true;

// ISR에서 설정, Task에서 처리하는 지연 리셋 플래그
static volatile atomic_bool s_rxResetPending = false;

/* TX Drop 기반 호스트 disconnect 감지 */
static uint32_t s_prev_drop_snapshot = 0;   /**< 이전 체크 시점의 drop count */
static uint8_t  s_drop_streak = 0;          /**< 연속 drop 증가 사이클 수 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _TryTransmit(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void CdcStream_Init(void)
{
    atomic_store(&s_txHead, 0);
    s_txTail = 0;
    atomic_store(&s_isTxBusy, false);
    atomic_store(&s_isStreamingActive, false);
    /* Auto-Stream 활성화: DTR=1 수신 시 자동 스트리밍 시작.
     * FTP QUERY_INFO 수신 시 _Boot_FTP_CdcTxWrapper가 자동으로 streaming 중단.
     * Dashboard: DTR=1 → streaming, FW Upload: QUERY_INFO → streaming 중단 → FTP. */
    atomic_store(&s_isAutoStreamEnabled, true);

    if (s_rxStreamBuffer == NULL) {
        s_rxStreamBuffer = xStreamBufferCreate(CDC_RX_RING_BUFFER_SIZE, 1);
    } else {
        xStreamBufferReset(s_rxStreamBuffer);
    }
}

// [Tx] UserTask가 호출 (구조체 전송)
bool CdcStream_Send(const void* data, uint32_t len)
{
    if (len == 0 || len > CDC_TX_RING_BUFFER_SIZE) return false;

    uint32_t head = atomic_load(&s_txHead);
    uint32_t tail = s_txTail; // ISR 변수지만 volatile 읽기 안전

    // 공간 계산
    uint32_t free = (head >= tail) ? (CDC_TX_RING_BUFFER_SIZE - (head - tail) - 1) : ((tail - head) - 1);
    if (len > free) {
        atomic_fetch_add(&s_txDropCount, 1);
        return false;
    }

    // 데이터 복사 (Wrap-around)
    const uint8_t* pData = (const uint8_t*)data;
    uint32_t chunk1 = (head + len > CDC_TX_RING_BUFFER_SIZE) ? (CDC_TX_RING_BUFFER_SIZE - head) : len;
    memcpy(&s_cdcTxRingBuffer[head], pData, chunk1);
    if (len > chunk1) memcpy(&s_cdcTxRingBuffer[0], pData + chunk1, len - chunk1);

    // Head 갱신 (Memory Barrier 역할 포함)
    atomic_store(&s_txHead, (head + len) % CDC_TX_RING_BUFFER_SIZE);

    // 전송 트리거 (Lock-Free)
    // 이미 전송 중이면 ISR이 끝나고 이어서 보낼 것이므로 무시
    if (!atomic_load(&s_isTxBusy)) {
        _TryTransmit();
    }
    return true;
}

// [Rx] UserTask가 호출 — StreamBuffer에서 읽기 + 레거시 명령 처리
uint32_t CdcStream_Read(void* buf, uint32_t max_len)
{
    if (s_rxStreamBuffer == NULL) return 0;

    if (atomic_exchange(&s_rxResetPending, false)) {
        xStreamBufferReset(s_rxStreamBuffer);
    }

    size_t n = xStreamBufferReceive(s_rxStreamBuffer, buf, max_len, 0);
    if (n == 0) return 0;

    // Legacy command processing (deprecated, Auto-Stream이 기본)
    if (n >= strlen(CDC_CMD_STREAMING_START) &&
        strncmp((char*)buf, CDC_CMD_STREAMING_START, strlen(CDC_CMD_STREAMING_START)) == 0) {
        atomic_store(&s_isStreamingActive, true);
        return 0;
    }
    if (n >= strlen(CDC_CMD_STREAMING_STOP) &&
        strncmp((char*)buf, CDC_CMD_STREAMING_STOP, strlen(CDC_CMD_STREAMING_STOP)) == 0) {
        atomic_store(&s_isStreamingActive, false);
        return 0;
    }

    return (uint32_t)n;
}

bool CdcStream_IsStreamingActive(void)
{
    return atomic_load(&s_isStreamingActive);
}

void CdcStream_SetAutoStreamEnabled(bool enabled)
{
    atomic_store(&s_isAutoStreamEnabled, enabled);
}

/**
 * @brief USB CDC 연결 상태 변경 시 호출
 * @details [변경] 연결 시 자동 스트리밍 시작을 제거하고, DTR=1 수신 시에만 시작.
 *          [근거] WebSerial 등 DTR=0을 보내지 않는 호스트 환경에서,
 *          USB 연결 즉시 streaming=true가 되면 drop 기반 감지까지
 *          ~500ms간 불필요한 LED Flickering이 발생함.
 *          [AS-IS] connected=true → streaming=true (auto-stream)
 *          [TO-BE] connected=true → 대기 (DTR=1이 오면 그때 시작)
 *          [영향] pyserial: 기본 DTR=True이므로 동작 동일.
 *                 Legacy "AGRB MON START": OnConnectionChanged 무관, 동작 동일.
 */
void CdcStream_OnConnectionChanged(bool connected)
{
    if (!connected) {
        atomic_store(&s_isStreamingActive, false);
        s_drop_streak = 0;
    }
    /* connected=true: DTR=1 수신 시 OnHostDtrChanged에서 streaming 시작 */
}

uint32_t CdcStream_GetTxDropCount(void)
{
    return atomic_load(&s_txDropCount);
}

void CdcStream_ResetTxDropCount(void)
{
    atomic_store(&s_txDropCount, 0);
}

/**
 * @brief ISR 컨텍스트에서 호출됨 — FreeRTOS Task-only API 사용 금지
 * @note xStreamBufferReset()은 내부에서 taskENTER_CRITICAL()을 호출하므로
 *       ISR에서 직접 호출하면 데드락 발생. 플래그로 지연 처리.
 */
void CdcStream_OnHostDtrChanged(uint8_t dtr)
{
    atomic_store(&s_isTxBusy, false);
    atomic_store(&s_txHead, 0);
    s_txTail = 0;
    atomic_store(&s_rxResetPending, true);

    /* Drop tracking 리셋 (DTR 이벤트가 정상 경로이므로) */
    s_drop_streak = 0;
    s_prev_drop_snapshot = atomic_load(&s_txDropCount);

    if (dtr) {
        if (atomic_load(&s_isAutoStreamEnabled)) {
            atomic_store(&s_isStreamingActive, true);
        }
    } else {
        atomic_store(&s_isStreamingActive, false);
    }
}

void CdcStream_CheckForDisconnect(void)
{
    if (!atomic_load(&s_isStreamingActive)) {
        s_drop_streak = 0;
        return;
    }

    uint32_t cur_drops = atomic_load(&s_txDropCount);

    if (cur_drops > s_prev_drop_snapshot) {
        s_drop_streak++;
    } else {
        s_drop_streak = 0;
    }
    s_prev_drop_snapshot = cur_drops;

    if (s_drop_streak >= CDC_DROP_DISCONNECT_CYCLES) {
        /* 연속 drop → 호스트가 데이터를 소비하지 않음 → streaming 자동 해제 */
        atomic_store(&s_isStreamingActive, false);
        s_drop_streak = 0;
    }
}

// ISR: 전송 완료 콜백
void CdcStream_OnTxComplete(void)
{
    atomic_store(&s_isTxBusy, false); // HW 바쁨 해제
    _TryTransmit(); // 남은 데이터 연쇄 전송
}

/**
 * @brief ISR: Rx 수신 콜백 — StreamBuffer에 raw bytes 적재만 수행
 * @note ISR 최소화 원칙 (Rule 13): 파싱 없이 데이터만 버퍼링.
 *       명령 파싱은 UserTask의 CdcStream_Read()에서 처리됨.
 */
void CdcStream_OnRxReceived(uint8_t* data, uint32_t len)
{
    if (s_rxStreamBuffer == NULL || len == 0) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xStreamBufferSendFromISR(s_rxStreamBuffer, data, len, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/*
 * 실제 전송 시도 — UserTask 또는 ISR 양쪽에서 호출됨 (Lock-Free 핵심)
 *
 * 호출 경로 1: UserTask → CdcStream_Send() → _TryTransmit() (최초 전송 트리거)
 * 호출 경로 2: ISR → CdcStream_OnTxComplete() → _TryTransmit() (연쇄 전송)
 *
 * atomic_compare_exchange로 동시 진입을 방지하므로 Lock 없이 안전.
 * Ring Buffer 포인터를 CDC_Transmit_FS에 직접 전달 (Zero-Copy DMA).
 */
static void _TryTransmit(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_isTxBusy, &expected, true)) {
        return;
    }

    uint32_t head = atomic_load(&s_txHead);
    uint32_t tail = s_txTail;

    if (head == tail) {
        atomic_store(&s_isTxBusy, false);
        return;
    }

    uint32_t len = (head > tail) ? (head - tail) : (CDC_TX_RING_BUFFER_SIZE - tail);
    if (len > APP_TX_DATA_SIZE) len = APP_TX_DATA_SIZE;

    // Zero-Copy: D3 Non-Cacheable Ring Buffer를 DMA에 직접 전달
    if (CDC_Transmit_FS(&s_cdcTxRingBuffer[tail], (uint16_t)len) == USBD_OK) {
        s_txTail = (tail + len) % CDC_TX_RING_BUFFER_SIZE;
    } else {
        atomic_store(&s_isTxBusy, false);
    }
}
