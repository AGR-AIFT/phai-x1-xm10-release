/**
 ******************************************************************************
 * @file    diag_perf.h
 * @author  HyundoKim
 * @brief   Step 0 진단 계측 — UserTask jitter histogram + p99
 *
 * @details
 * Plan v1.5 REV2_USB_TightSpin_InvestigationPlan 의 Step 0 구현.
 *
 * - Histogram 21 bin:
 *   [0..4]  : 0-200us, 200-400us, 400-600us, 600-800us, 800-1000us
 *   [5..19] : 1-2ms, 2-3ms, ..., 15-16ms (1ms 간격 15개)
 *   [20]    : 16ms+ overflow
 *
 * - 측정 윈도우: 세션 시작 500 샘플 이후부터 기록
 *   (metadata.txt write 의 tight spin 오염 회피)
 *
 * - DTCM 배치 — FreeRTOS API 의존 없음, DWT only
 * - `DIAG_PROFILE_ENABLED` 매크로 OFF 시 zero-cost (inline no-op)
 *
 * 사용 순서:
 *   1. DiagPerf_Reset()            — 세션 시작 시
 *   2. DiagPerf_RecordJitter(jit)  — UserTask 매 cycle
 *   3. DiagPerf_Finalize()         — _WriteSummary_WithStatus 직전
 *   4. DiagPerf_FormatReport(buf)  — diag_profile.txt 작성
 ******************************************************************************
 */

#ifndef DIAG_PERF_H_
#define DIAG_PERF_H_

#include <stdint.h>
#include "module.h"  /* DIAG_PROFILE_ENABLED / DIAG_KILL_DRAIN_WRITE 기본값 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DIAG_PROFILE_ENABLED)
  #define DIAG_PERF_ACTIVE 1
#else
  #define DIAG_PERF_ACTIVE 0
#endif

/** 측정 윈도우 시작 오프셋 — 첫 500 샘플 (500ms) 는 histogram 제외 */
#define DIAG_PERF_WINDOW_EXCLUDE_START   500U

/** Histogram bin 수 */
#define DIAG_PERF_N_BINS                 21U

/** ISR nesting 최대 깊이 (Cortex-M NVIC PriorityGroup=4 현실 상한) */
#define DIAG_PERF_ISR_NEST_MAX           4U

/** 단일 ISR exclusive 상한 — 초과 시 outlier bucket 으로 분리.
 *  5ms × 480MHz = 2.4M cycles. ISR 이 5ms 넘으면 측정 오류 or 비정상 blocking
 *  으로 간주 (정상 ISR 은 수십~수백 µs). */
#define DIAG_PERF_ISR_CLAMP_CYCLES       (5UL * 480000UL)

/** Phase 2A: f_write 크기 분포 bin 수.
 *  [0] ≤4B (CRC tail only), [1] ≤64B, [2] ≤256B, [3] ≤1KB,
 *  [4] ≤4KB (CRC data block), [5] ≤16KB, [6] ≤32KB (MAX_WRITE_CHUNK),
 *  [7] >32KB (overflow) */
#define DIAG_PERF_WRITE_N_BINS           8U

/** [2026-04-18] OffloadTask stall 진단용 bin 수.
 *  wake_gap (iter 간 간격, 정상 20ms) / iter_dur (iter 실행 시간) 공용.
 *  [0] ≤25ms (정상), [1] ≤50ms (경미), [2] ≤100ms (중간),
 *  [3] ≤500ms (심각), [4] >500ms (치명 — HOT_OVERFLOW 임박) */
#define DIAG_PERF_OFFLOAD_N_BINS         5U

/**
 * @brief Step A-1: ISR 분류 및 ID.
 * @details category 는 "USB 기여 vs FDCAN 기여 vs 기타" 분리용 (plan §9.1).
 *          ID 는 per-ISR 원시 카운터용. 컴파일타임 테이블로 category 조회.
 */
typedef enum {
    DIAG_ISR_CAT_USB = 0,
    DIAG_ISR_CAT_FDCAN,
    DIAG_ISR_CAT_DMA,
    DIAG_ISR_CAT_OTHER,
    DIAG_ISR_CAT_COUNT
} DiagIsrCat_t;

typedef enum {
    DIAG_ISR_OTG_FS = 0,
    DIAG_ISR_FDCAN1_IT0,
    DIAG_ISR_FDCAN2_IT0,
    DIAG_ISR_TIM7,
    DIAG_ISR_DMA2_S0,
    DIAG_ISR_DMA2_S1,
    DIAG_ISR_DMA2_S2,
    DIAG_ISR_DMA2_S3,
    DIAG_ISR_DMA2_S4,
    DIAG_ISR_QUADSPI,
    DIAG_ISR_ID_COUNT
} DiagIsrId_t;

#if DIAG_PERF_ACTIVE

/**
 * @brief 세션 시작 시 호출 — 모든 카운터 초기화.
 * @note  msc_gait_analysis_log.c _Logging_Entry 에서 호출.
 */
void DiagPerf_Reset(void);

/**
 * @brief UserTask 매 cycle 에서 호출. 첫 500 샘플은 무시 (측정 윈도우 밖).
 * @param jitter_us |delta_us - 1000| 형태의 절대 jitter (uint16 truncated 기준)
 */
void DiagPerf_RecordJitter(uint32_t jitter_us);

/**
 * @brief 기록 중단 — 이 이후 RecordJitter 호출은 무시.
 * @note  _WriteSummary_WithStatus 직전에 호출하여 summary write 자체의
 *        tight spin 이 histogram 에 포함되지 않도록 함.
 */
void DiagPerf_Finalize(void);

/**
 * @brief 현재까지 기록된 histogram/stats 를 텍스트로 포맷.
 * @param buf      출력 버퍼
 * @param buf_size 버퍼 크기
 * @return 실제 기록된 바이트 수 (snprintf 기준)
 */
int  DiagPerf_FormatReport(char* buf, int buf_size);

/**
 * @brief Step A-1: ISR 진입/종료 훅. nesting-aware exclusive cycle 누적.
 * @note  ISR context 전용. DWT->CYCCNT 기반. 내부 stack 깊이 초과 시 샘플 드롭.
 */
void DiagPerf_EnterISR(DiagIsrId_t id);
void DiagPerf_ExitISR(void);

/**
 * @brief Phase 2A: f_write 크기 기록. ioif_filesystem_write 에서 1회/call.
 * @param size bytes requested in f_write (pre-FATFS, pre-USBH).
 * @note  Mutex 보유 컨텍스트 가정. ISR context 에서는 호출 금지.
 */
void DiagPerf_RecordWrite(uint32_t size);

/**
 * @brief [2026-04-18] OffloadTask 1회 iteration 기록 — stall 진단.
 * @param wake_gap_us  직전 iteration 시작 ~ 이번 iteration 시작 간격 (정상 20ms).
 *                    크면 UserTask 에 preempt 되거나 osDelay 에서 깨어나지 못함.
 * @param iter_dur_us  이번 iteration 실행 시간 (_TransferHotToCold while loop 포함).
 *                    크면 QSPI mutex block 이나 PSRAM write 지연.
 * @note Task 컨텍스트. 첫 iteration 은 wake_gap=0 으로 전달 (skip 처리됨).
 */
void DiagPerf_RecordOffloadIter(uint32_t wake_gap_us, uint32_t iter_dur_us);

#define DIAG_ISR_ENTER(id)  DiagPerf_EnterISR(id)
#define DIAG_ISR_EXIT()     DiagPerf_ExitISR()

#else  /* !DIAG_PERF_ACTIVE — zero-cost stubs */

static inline void DiagPerf_Reset(void) {}
static inline void DiagPerf_RecordJitter(uint32_t j) { (void)j; }
static inline void DiagPerf_Finalize(void) {}
static inline int  DiagPerf_FormatReport(char* b, int n) { (void)b; (void)n; return 0; }
static inline void DiagPerf_EnterISR(DiagIsrId_t id) { (void)id; }
static inline void DiagPerf_ExitISR(void) {}
static inline void DiagPerf_RecordWrite(uint32_t size) { (void)size; }
static inline void DiagPerf_RecordOffloadIter(uint32_t g, uint32_t d) { (void)g; (void)d; }

#define DIAG_ISR_ENTER(id)  ((void)(id))
#define DIAG_ISR_EXIT()     ((void)0)

#endif /* DIAG_PERF_ACTIVE */

#ifdef __cplusplus
}
#endif

#endif /* DIAG_PERF_H_ */
