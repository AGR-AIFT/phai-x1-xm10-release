/**
 ******************************************************************************
 * @file    memory_aware_design.c
 * @author  HyundoKim
 * @brief   [고급] 메모리 인식 설계 패턴
 * @details
 * MCU 환경(STM32H7, 1MB RAM)에서 동적 할당(malloc) 없이
 * 정적 메모리만 사용하는 설계 패턴을 다루는 교육용 예제입니다.
 *
 * [학습 목표]
 *   - 정적 링 버퍼(Ring Buffer)를 활용한 이동 평균 필터
 *   - 정적 풀 할당자(Pool Allocator)를 활용한 이벤트 로깅
 *   - sizeof()를 활용한 메모리 사용량 리포팅
 *   - malloc/free 없이 안전한 메모리 관리
 *
 * [MCU 메모리 구조 — STM32H743]
 *   - DTCM-RAM  (128KB): 가장 빠름, DMA 접근 불가, 스택/지역변수에 적합
 *   - AXI-SRAM  (512KB): 범용, DMA 가능, 대부분의 정적 변수
 *   - SRAM1~3   (288KB): DMA 버퍼, D-Cache 정렬 필요
 *   - SRAM4     (64KB) : D3 도메인, 저전력 모드에서도 유지
 *
 * [정적 vs 동적 할당 트레이드오프]
 *   정적 할당:
 *     + 결정론적 (메모리 파편화 없음)
 *     + 컴파일 타임에 메모리 사용량 확정
 *     + 실시간 시스템에 적합 (할당 시간 O(1))
 *     - 최대 사용량만큼 항상 점유
 *   동적 할당 (malloc/free):
 *     + 필요할 때만 메모리 사용
 *     - 파편화로 인한 할당 실패 위험
 *     - 할당 시간 비결정론적
 *     - 임베디드 양산 코드에서는 금지 (이 프로젝트 규칙)
 *
 * [D-Cache 정렬]
 *   STM32H7의 D-Cache 라인 크기는 32바이트입니다.
 *   DMA와 공유하는 버퍼는 32바이트 정렬이 필요하지만,
 *   이 예제의 데이터는 CPU 전용이므로 정렬 제약이 없습니다.
 *
 * @see     docs/api-reference/system.md
 * @version 1.0
 * @date    Mar 09, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <stdio.h>
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * 링 버퍼 크기 — 50ms 분량 @ 1ms 주기 = 50 샘플
 * 이동 평균 윈도우 크기와 동일합니다.
 */
#define RING_BUF_SIZE           (50U)

/**
 * 풀 할당자 슬롯 수 — 이벤트 저장용
 * 급격한 각도 변화 이벤트를 최대 10개 저장합니다.
 */
#define POOL_SIZE               (10U)

/** 이벤트 감지 임계값 (도) — 연속 루프 간 각도 변화량 */
#define EVENT_THRESHOLD_DEG     (5.0f)

/** 메모리 리포트 출력용 USB 버퍼 크기 */
#define USB_BUF_SIZE            (256U)

/** STM32H743 총 RAM 크기 (참조용 표시) */
#define TOTAL_RAM_BYTES         (1024U * 1024U)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * 정적 링 버퍼 — FIFO 방식의 고정 크기 순환 버퍼
 *
 * [스택/힙 분리 전략]
 * 이 구조체는 static 변수로 선언되어 .bss 섹션에 배치됩니다.
 * 스택(DTCM)을 소비하지 않으므로 재귀나 깊은 호출 스택에서도 안전합니다.
 */
typedef struct {
    float    data[RING_BUF_SIZE];   /**< 고정 크기 데이터 배열 */
    uint16_t head;                  /**< 쓰기 위치 (다음 Push 인덱스) */
    uint16_t tail;                  /**< 읽기 위치 (다음 Pop 인덱스) */
    uint16_t count;                 /**< 현재 저장된 요소 수 */
} RingBuffer_t;

/**
 * 풀 할당자 데이터 슬롯 — 이벤트 1건의 저장 단위
 *
 * 범용 데이터 블록으로 설계되어,
 * 타임스탬프 + 최대 4개의 float 값을 저장할 수 있습니다.
 */
typedef struct {
    uint32_t timestamp;             /**< 이벤트 발생 시각 (ms) */
    float    values[4];             /**< 범용 데이터 (각도, 토크 등) */
} DataSlot_t;

/**
 * 정적 풀 할당자 — malloc 없이 고정 슬롯을 관리
 *
 * [설계 원리]
 * 미리 할당된 배열에서 슬롯을 "빌려주고" 반환받는 패턴입니다.
 * 할당 시간 O(N)이지만, POOL_SIZE가 작으므로 문제 없습니다.
 * 대규모 풀이 필요하면 비트맵 기반 할당을 고려해야 합니다.
 */
typedef struct {
    DataSlot_t pool[POOL_SIZE];     /**< 데이터 슬롯 배열 */
    bool       is_used[POOL_SIZE];  /**< 슬롯 사용 여부 플래그 */
} PoolAlloc_t;

/**
 * USB 스트리밍 데이터 — PhAI Studio로 실시간 전송
 */
typedef struct {
    float raw_angle;                /**< 원본 각도 (필터 전) */
    float filtered_angle;           /**< 필터링된 각도 (이동 평균) */
    float pool_used_count;          /**< 풀 사용 슬롯 수 (float로 전송) */
    float ring_count;               /**< 링 버퍼 저장 요소 수 */
} StreamData_t;

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

static XmTsmHandle_t s_tsm;

/* 패턴 1: 정적 링 버퍼 (이동 평균 필터용) */
static RingBuffer_t s_ring_buf;

/* 패턴 2: 정적 풀 할당자 (이벤트 로깅용) */
static PoolAlloc_t s_pool;

/* USB 스트리밍 데이터 */
static StreamData_t s_stream_data;

/* 이전 각도 값 (이벤트 감지용) */
static float s_prev_angle = 0.0f;

/* USB 출력 버퍼 */
static char s_usb_buf[USB_BUF_SIZE];

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* TSM 콜백 */
static void Run_Loop(void);

/* 링 버퍼 API */
static void  _RingBuf_Init(RingBuffer_t *p_buf);
static bool  _RingBuf_Push(RingBuffer_t *p_buf, float value);
static bool  _RingBuf_Pop(RingBuffer_t *p_buf, float *p_value);
static bool  _RingBuf_IsFull(const RingBuffer_t *p_buf);
static bool  _RingBuf_IsEmpty(const RingBuffer_t *p_buf);
static uint16_t _RingBuf_GetCount(const RingBuffer_t *p_buf);

/* 풀 할당자 API */
static void       _Pool_Init(PoolAlloc_t *p_pool);
static DataSlot_t *_Pool_Alloc(PoolAlloc_t *p_pool);
static void       _Pool_Free(PoolAlloc_t *p_pool, DataSlot_t *p_slot);
static uint16_t   _Pool_GetUsedCount(const PoolAlloc_t *p_pool);

/* 이동 평균 계산 */
static float _CalcMovingAverage(const RingBuffer_t *p_buf);

/* 이벤트 감지 및 기록 */
static void _DetectAndLogEvent(float current_angle, uint32_t now);

/* 버튼 처리 */
static void _HandleButtons(void);

/* 메모리 리포트 */
static void _PrintMemoryReport(void);

/* 이벤트 덤프 */
static void _DumpAndFreeEvents(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    /* 단일 상태 TSM — 항상 동작 */
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
    XM_TSM_AddState(s_tsm, &conf);

    /* 데이터 구조 초기화 */
    _RingBuf_Init(&s_ring_buf);
    _Pool_Init(&s_pool);

    /* USB 스트리밍 등록 (4 × float = 16 bytes) */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Raw Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Filtered Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Pool Used\",\"unit\":\"cnt\"},"
        "{\"name\":\"Ring Count\",\"unit\":\"cnt\"}]");

    XM_SendUsbDebugMessage("[MEM] Memory-Aware Design example started.\r\n");
    XM_SendUsbDebugMessage("[MEM] BTN1=MemReport  BTN2=EventDump  BTN3=Reserved\r\n");
}

void User_Loop(void)
{
    XM_TSM_Run(s_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 메인 루프 — 이동 평균 + 이벤트 감지 + 스트리밍
 */
static void Run_Loop(void)
{
    uint32_t now = XM_GetTick();

    /* 패턴 4: 센서 데이터를 링 버퍼에 Push → 이동 평균 계산 */
    float raw_angle = XM.status.h10.rightHipAngle;
    _RingBuf_Push(&s_ring_buf, raw_angle);

    float filtered_angle = _CalcMovingAverage(&s_ring_buf);

    /* 패턴 5: 급격한 각도 변화 감지 → 풀에 이벤트 기록 */
    _DetectAndLogEvent(raw_angle, now);

    /* USB 스트리밍 데이터 갱신 */
    s_stream_data.raw_angle        = raw_angle;
    s_stream_data.filtered_angle   = filtered_angle;
    s_stream_data.pool_used_count  = (float)_Pool_GetUsedCount(&s_pool);
    s_stream_data.ring_count       = (float)_RingBuf_GetCount(&s_ring_buf);
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);

    /* 버튼 입력 처리 */
    _HandleButtons();
}

/* ============================================================
 * 링 버퍼 구현 (패턴 1)
 *
 * [자료구조 설명]
 * head: 다음 쓰기 위치 (Push 시 head 이동)
 * tail: 다음 읽기 위치 (Pop 시 tail 이동)
 * count: 현재 저장된 요소 수 (Full/Empty 판별)
 *
 * 버퍼가 가득 차면 가장 오래된 데이터를 덮어씁니다 (overwrite).
 * 이동 평균 필터에서는 항상 최신 N개 데이터만 필요하므로
 * 이 정책이 적합합니다.
 * ============================================================ */

/**
 * @brief 링 버퍼 초기화
 * @param p_buf 링 버퍼 포인터
 */
static void _RingBuf_Init(RingBuffer_t *p_buf)
{
    memset(p_buf->data, 0, sizeof(p_buf->data));
    p_buf->head  = 0;
    p_buf->tail  = 0;
    p_buf->count = 0;
}

/**
 * @brief 링 버퍼에 데이터 추가
 * @param p_buf 링 버퍼 포인터
 * @param value 추가할 값
 * @return true: 성공, false: 오버라이트 발생 (가장 오래된 값 덮어씀)
 *
 * 버퍼가 가득 찬 경우 tail을 전진시켜 가장 오래된 데이터를 버립니다.
 * 이동 평균에서는 이 동작이 의도된 것입니다.
 */
static bool _RingBuf_Push(RingBuffer_t *p_buf, float value)
{
    bool is_overwrite = false;

    if (_RingBuf_IsFull(p_buf)) {
        /* 가득 참 → tail 전진 (가장 오래된 데이터 폐기) */
        p_buf->tail = (p_buf->tail + 1U) % RING_BUF_SIZE;
        p_buf->count--;
        is_overwrite = true;
    }

    p_buf->data[p_buf->head] = value;
    p_buf->head = (p_buf->head + 1U) % RING_BUF_SIZE;
    p_buf->count++;

    return !is_overwrite;
}

/**
 * @brief 링 버퍼에서 데이터 추출 (FIFO)
 * @param p_buf   링 버퍼 포인터
 * @param p_value 추출된 값 저장 포인터
 * @return true: 성공, false: 버퍼 비어있음
 */
static bool _RingBuf_Pop(RingBuffer_t *p_buf, float *p_value)
{
    if (_RingBuf_IsEmpty(p_buf)) {
        return false;
    }

    *p_value = p_buf->data[p_buf->tail];
    p_buf->tail = (p_buf->tail + 1U) % RING_BUF_SIZE;
    p_buf->count--;

    return true;
}

/**
 * @brief 링 버퍼 Full 여부 확인
 * @param p_buf 링 버퍼 포인터 (읽기 전용)
 * @return true: 가득 참
 */
static bool _RingBuf_IsFull(const RingBuffer_t *p_buf)
{
    return (p_buf->count >= RING_BUF_SIZE);
}

/**
 * @brief 링 버퍼 Empty 여부 확인
 * @param p_buf 링 버퍼 포인터 (읽기 전용)
 * @return true: 비어있음
 */
static bool _RingBuf_IsEmpty(const RingBuffer_t *p_buf)
{
    return (p_buf->count == 0U);
}

/**
 * @brief 링 버퍼에 저장된 요소 수 반환
 * @param p_buf 링 버퍼 포인터 (읽기 전용)
 * @return 현재 저장된 요소 수
 */
static uint16_t _RingBuf_GetCount(const RingBuffer_t *p_buf)
{
    return p_buf->count;
}

/* ============================================================
 * 풀 할당자 구현 (패턴 2)
 *
 * [malloc 대체 전략]
 * 고정 크기 슬롯을 미리 배열로 선언하고,
 * is_used[] 플래그로 할당 상태를 관리합니다.
 *
 * 장점:
 *   - 메모리 파편화 없음 (고정 크기 슬롯)
 *   - 할당/해제 시간 예측 가능 (O(N), N은 POOL_SIZE)
 *   - 컴파일 타임에 최대 메모리 사용량 확정
 *
 * 제약:
 *   - 슬롯 크기가 고정 (가변 크기 데이터 불가)
 *   - 최대 슬롯 수가 고정 (런타임 확장 불가)
 * ============================================================ */

/**
 * @brief 풀 할당자 초기화
 * @param p_pool 풀 할당자 포인터
 */
static void _Pool_Init(PoolAlloc_t *p_pool)
{
    memset(p_pool->pool, 0, sizeof(p_pool->pool));
    memset(p_pool->is_used, 0, sizeof(p_pool->is_used));
}

/**
 * @brief 풀에서 빈 슬롯 할당
 * @param p_pool 풀 할당자 포인터
 * @return 할당된 슬롯 포인터, NULL: 풀 가득 참
 *
 * 선형 탐색으로 빈 슬롯을 찾습니다.
 * POOL_SIZE가 작으므로 성능 문제 없습니다.
 */
static DataSlot_t *_Pool_Alloc(PoolAlloc_t *p_pool)
{
    for (uint16_t i = 0; i < POOL_SIZE; i++) {
        if (!p_pool->is_used[i]) {
            p_pool->is_used[i] = true;
            memset(&p_pool->pool[i], 0, sizeof(DataSlot_t));
            return &p_pool->pool[i];
        }
    }
    return NULL;  /* 풀 가득 참 */
}

/**
 * @brief 풀에 슬롯 반환
 * @param p_pool 풀 할당자 포인터
 * @param p_slot 반환할 슬롯 포인터
 *
 * [방어적 프로그래밍]
 * 포인터가 풀 범위 내인지 확인하여 잘못된 반환을 방지합니다.
 */
static void _Pool_Free(PoolAlloc_t *p_pool, DataSlot_t *p_slot)
{
    /* NULL 포인터 방어 */
    if (p_slot == NULL) {
        return;
    }

    /* 풀 범위 검사 — 포인터가 풀 배열 내부를 가리키는지 확인 */
    if ((p_slot >= &p_pool->pool[0]) && (p_slot <= &p_pool->pool[POOL_SIZE - 1U])) {
        uint16_t index = (uint16_t)(p_slot - &p_pool->pool[0]);
        p_pool->is_used[index] = false;
    }
}

/**
 * @brief 풀에서 사용 중인 슬롯 수 반환
 * @param p_pool 풀 할당자 포인터 (읽기 전용)
 * @return 사용 중인 슬롯 수
 */
static uint16_t _Pool_GetUsedCount(const PoolAlloc_t *p_pool)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < POOL_SIZE; i++) {
        if (p_pool->is_used[i]) {
            count++;
        }
    }
    return count;
}

/* ============================================================
 * 이동 평균 필터 (패턴 4)
 *
 * [알고리즘]
 * 링 버퍼에 저장된 모든 값의 산술 평균을 계산합니다.
 * 매 루프마다 전체 합산을 수행하므로 O(N)이지만,
 * N=50이므로 1ms 루프에서 충분히 빠릅니다.
 *
 * [최적화 가능성]
 * 증분 합산(incremental sum)을 사용하면 O(1)로 개선 가능하나,
 * 부동소수점 누적 오차 문제가 발생합니다.
 * 교육 목적으로 단순한 전체 합산 방식을 사용합니다.
 * ============================================================ */

/**
 * @brief 링 버퍼 데이터의 이동 평균 계산
 * @param p_buf 링 버퍼 포인터 (읽기 전용)
 * @return 이동 평균 값 (버퍼 비어있으면 0.0f)
 */
static float _CalcMovingAverage(const RingBuffer_t *p_buf)
{
    if (p_buf->count == 0U) {
        return 0.0f;
    }

    float sum = 0.0f;
    uint16_t idx = p_buf->tail;

    for (uint16_t i = 0; i < p_buf->count; i++) {
        sum += p_buf->data[idx];
        idx = (idx + 1U) % RING_BUF_SIZE;
    }

    return sum / (float)p_buf->count;
}

/* ============================================================
 * 이벤트 감지 및 풀 기록 (패턴 5)
 * ============================================================ */

/**
 * @brief 급격한 각도 변화 감지 및 이벤트 기록
 * @param current_angle 현재 각도 (도)
 * @param now           현재 시각 (ms)
 *
 * |delta| > 5.0도일 때 이벤트로 판정하여 풀에 기록합니다.
 * 풀이 가득 차면 LED3 Blink 경고를 표시합니다.
 */
static void _DetectAndLogEvent(float current_angle, uint32_t now)
{
    float delta = current_angle - s_prev_angle;

    /* 절대값 계산 (math.h 의존 제거) */
    if (delta < 0.0f) {
        delta = -delta;
    }

    if (delta > EVENT_THRESHOLD_DEG) {
        DataSlot_t *p_slot = _Pool_Alloc(&s_pool);

        if (p_slot != NULL) {
            /* 이벤트 데이터 기록 */
            p_slot->timestamp = now;
            p_slot->values[0] = s_prev_angle;       /* 이전 각도 */
            p_slot->values[1] = current_angle;       /* 현재 각도 */
            p_slot->values[2] = current_angle - s_prev_angle;  /* 변화량 (부호 포함) */
            p_slot->values[3] = XM.status.h10.rightHipTorque;  /* 해당 시점 토크 */

            snprintf(s_usb_buf, USB_BUF_SIZE,
                     "[EVENT] Angle jump: %.2f -> %.2f (delta=%.2f) at %lums [%u/%u slots]\r\n",
                     s_prev_angle, current_angle,
                     current_angle - s_prev_angle,
                     (unsigned long)now,
                     _Pool_GetUsedCount(&s_pool), (unsigned)POOL_SIZE);
            XM_SendUsbDebugMessage(s_usb_buf);
        } else {
            /* 풀 가득 참 → LED 경고 */
            XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);
        }
    }

    s_prev_angle = current_angle;
}

/* ============================================================
 * 버튼 처리 및 리포트 출력
 * ============================================================ */

/**
 * @brief 버튼 입력 처리
 *
 * [버튼 매핑]
 * - BTN1 클릭: 메모리 사용량 리포트 출력
 * - BTN2 클릭: 저장된 이벤트 덤프 후 풀 해제
 * - BTN3 클릭: 링 버퍼 초기화 (이동 평균 리셋)
 */
static void _HandleButtons(void)
{
    /* BTN1: 메모리 사용량 리포트 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        _PrintMemoryReport();
    }

    /* BTN2: 이벤트 덤프 및 풀 해제 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        _DumpAndFreeEvents();
    }

    /* BTN3: 링 버퍼 초기화 — _RingBuf_Pop()으로 하나씩 비우기 (API 활용 시연) */
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        uint16_t popped = 0;
        float dummy;
        while (_RingBuf_Pop(&s_ring_buf, &dummy)) {
            popped++;
        }
        snprintf(s_usb_buf, USB_BUF_SIZE,
                 "[RING] Buffer cleared: %u samples popped. Moving avg reset.\r\n",
                 popped);
        XM_SendUsbDebugMessage(s_usb_buf);
    }
}

/**
 * @brief 메모리 사용량 리포트 — USB CDC 출력 (패턴 3)
 *
 * 각 구조체의 sizeof()와 전체 static 변수 합계를 출력합니다.
 * 컴파일러가 패딩을 추가할 수 있으므로,
 * sizeof()가 필드 합산보다 클 수 있습니다.
 */
static void _PrintMemoryReport(void)
{
    XM_SendUsbDebugMessage("========== MEMORY REPORT ==========\r\n");

    /* 링 버퍼 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[MEMORY] Ring Buffer: %u bytes (%u x float + %u overhead)\r\n",
             (unsigned)sizeof(RingBuffer_t),
             (unsigned)RING_BUF_SIZE,
             (unsigned)(sizeof(RingBuffer_t) - sizeof(float) * RING_BUF_SIZE));
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 풀 할당자 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[MEMORY] Pool Allocator: %u bytes (%u x %u-byte slot + %u x bool + padding)\r\n",
             (unsigned)sizeof(PoolAlloc_t),
             (unsigned)POOL_SIZE,
             (unsigned)sizeof(DataSlot_t),
             (unsigned)POOL_SIZE);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 스트리밍 데이터 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[MEMORY] Stream Data: %u bytes (%u x float)\r\n",
             (unsigned)sizeof(StreamData_t),
             (unsigned)(sizeof(StreamData_t) / sizeof(float)));
    XM_SendUsbDebugMessage(s_usb_buf);

    /* USB 버퍼 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[MEMORY] USB Buffer: %u bytes\r\n",
             (unsigned)USB_BUF_SIZE);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 전체 합계 (이 파일의 static 변수만 집계)
     * 주의: s_tsm은 XmTsmHandle_t (포인터) — 실제 TSM 구조체는
     * XM_TSM_Create() 내부에서 할당되므로 여기서 sizeof 불가.
     * 포인터 크기(4B)만 계산에 포함합니다. */
    uint32_t total = sizeof(RingBuffer_t)        /* s_ring_buf */
                   + sizeof(PoolAlloc_t)         /* s_pool */
                   + sizeof(StreamData_t)        /* s_stream_data */
                   + sizeof(s_usb_buf)           /* USB 버퍼 */
                   + sizeof(s_prev_angle)        /* 이전 각도 */
                   + sizeof(s_tsm)               /* TSM 핸들 (포인터, 4B) */
                   + sizeof(s_stream_data);      /* 스트리밍 데이터 */

    /*
     * 전체 RAM 대비 비율 계산
     * STM32H743 총 RAM: ~1MB (DTCM 128K + AXI 512K + SRAM 288K + SRAM4 64K)
     */
    float usage_percent = ((float)total / (float)TOTAL_RAM_BYTES) * 100.0f;
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[MEMORY] Total Static: %lu bytes (%.2f%% of 1MB RAM)\r\n",
             (unsigned long)total, usage_percent);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 풀 상태 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[MEMORY] Pool: %u/%u slots used | Ring: %u/%u samples\r\n",
             _Pool_GetUsedCount(&s_pool), (unsigned)POOL_SIZE,
             _RingBuf_GetCount(&s_ring_buf), (unsigned)RING_BUF_SIZE);
    XM_SendUsbDebugMessage(s_usb_buf);

    XM_SendUsbDebugMessage("========== END REPORT =============\r\n");
}

/**
 * @brief 저장된 이벤트 덤프 후 전체 풀 해제
 *
 * 풀에 기록된 모든 이벤트를 USB CDC로 출력한 뒤,
 * 모든 슬롯을 해제합니다.
 */
static void _DumpAndFreeEvents(void)
{
    uint16_t used = _Pool_GetUsedCount(&s_pool);

    if (used == 0U) {
        XM_SendUsbDebugMessage("[EVENT] No events recorded.\r\n");
        return;
    }

    snprintf(s_usb_buf, USB_BUF_SIZE,
             "========== EVENT DUMP (%u events) ==========\r\n",
             used);
    XM_SendUsbDebugMessage(s_usb_buf);

    uint16_t event_num = 0;
    for (uint16_t i = 0; i < POOL_SIZE; i++) {
        if (s_pool.is_used[i]) {
            event_num++;
            const DataSlot_t *p_slot = &s_pool.pool[i];
            snprintf(s_usb_buf, USB_BUF_SIZE,
                     "[%2u] t=%lums | Angle: %.2f -> %.2f (delta=%.2f) | Torque: %.3f Nm\r\n",
                     event_num,
                     (unsigned long)p_slot->timestamp,
                     p_slot->values[0],  /* 이전 각도 */
                     p_slot->values[1],  /* 현재 각도 */
                     p_slot->values[2],  /* 변화량 */
                     p_slot->values[3]); /* 토크 */
            XM_SendUsbDebugMessage(s_usb_buf);
        }
    }

    /* 전체 풀 해제 — _Pool_Free() API를 사용하여 개별 슬롯 반환 */
    for (uint16_t i = 0; i < POOL_SIZE; i++) {
        if (s_pool.is_used[i]) {
            _Pool_Free(&s_pool, &s_pool.pool[i]);
        }
    }

    /* LED 경고 해제 */
    XM_SetLedState(XM_LED_3, XM_OFF);

    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[EVENT] %u events dumped. Pool cleared.\r\n", used);
    XM_SendUsbDebugMessage(s_usb_buf);
}
