/**
 ******************************************************************************
 * @file    ioif_agrb_spi.h
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] SPI 하드웨어 추상화 계층 헤더 (aeat_9955 origin)
 * @version 3.0 (Common Library - H7/G4 Dual Platform)
 * @date    Feb 12, 2026
 *
 * @details
 * - Handle-based API: ioif_spi.write(), ioif_spi.read() 등
 * - Resource Pool + Instance 2계층 아키텍처
 * - ISR 기반 SPI 모드 지원
 *
 * @note aeat_9955 원본 기반, H7/G4 듀얼 플랫폼 + ENABLE 가드 적용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef _IOIF_AGRB_SPI_H_
#define _IOIF_AGRB_SPI_H_

#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_SPI_ENABLE)

#include <stdint.h>
#include <stdbool.h>

/* HAL Includes (MCU-specific) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_spi.h"
    #include "stm32h7xx_hal_gpio.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx_hal.h"
    #include "stm32g4xx_hal_spi.h"
    #include "stm32g4xx_hal_gpio.h"
#else
    #error "Unsupported MCU series for IOIF SPI module"
#endif

#include "ioif_agrb_gpio.h"

#define IOIF_SPI_ID_NOT_ALLOCATED  (0xFFFFFFFF)

#define IOIF_SPI_MAX_INSTANCES              (16) //Max 16 devices can be handled
#define IOIF_SPI_MAX_HANDLERS               (4) //Max 4 SPI instances (SPI1, SPI2, SPI3, SPI4, ...)
#define IOIF_SPI_DEFAULT_TIMEOUT            (1000U)
#define IOIF_SPI_DEFAULT_DMA_TX_SIZE        (128)
#define IOIF_SPI_DEFAULT_DMA_RX_SIZE        (128)
#define IOIF_SPI_DEFAULT_DUMMY_SIZE         (256U) //BareMetal TransmitReceive용 스택 더미 RX 버퍼 크기
#define IOIF_SPI_ISR_MQ_MAX_CAPACITY        (256U) //ISR에서 관리하는 메시지 큐의 최대 크기 (메시지 수)

#define IOIF_SPI_ISR_MQ_UNIT_SIZE           (16U) //ISR에서 관리하는 메시지 큐의 단위 크기 (바이트)

#if defined(AGRB_IOIF_SPI_POLL_ENABLE)
/** @brief set_poll_deadline 상한(µs) — 폴링은 짧은 전송 전용이라 API 가 길이를 강제한다 (PLAN-20260917 D5) */
#define IOIF_SPI_POLL_DEADLINE_MAX_US       (1000U)
#endif

typedef uint32_t IOIF_SPIx_t;
#define IOIF_SPI_NOT_ALLOCATED_ID           ((IOIF_SPIx_t)(-1))

typedef struct {
    SPI_HandleTypeDef* hspi;
    IOIF_GPIOx_t ss; //SS/CS pin as GPIO

    #if defined(USE_FREERTOS)
    struct {
        size_t tx_size;
        size_t rx_size;
    } dma;
    #endif

    struct {
        struct {
            bool enable; //ISR 기반 모드 활성화 여부
            size_t queue_length; //ISR 기반 모드에서 사용하는 메시지 큐 길이 (단위: 메시지 수)
        } isr_mode;
    } options;

    uint32_t timeout; //in ms
} IOIF_SPI_Initialize_t;

/**
 *-----------------------------------------------------------
 *            TIMING PROBE (W0) — IOIF_TIMING_PROBE_SPI=1 에서만 (기본 = IOIF_TIMING_PROBE)
 *-----------------------------------------------------------
 */
#if IOIF_TIMING_PROBE_SPI

/**
 * @brief SPI 트랜잭션 1건의 DWT 스탬프 (task mailbox + ISR mailbox 를 txn_id 로 JOIN)
 * @details 값은 **DWT raw cycle** 이다 — 호출자가 자기 쪽 스탬프(①호출 진입 / ⑦복귀)와
 *          같은 시간축에서 직접 차분할 수 있어야 하므로 IOIF 는 µs 로 접지 않는다.
 *          변환은 `IOIF_DWT_CyclesToUs()`.
 *
 * | 스탬프 | 의미 | writer |
 * |---|---|---|
 * | `t_lock`      | ② device mutex 획득 직후        | 호출 task |
 * | `t_cs_low`    | ③ CS low 직후                   | 호출 task |
 * | `t_dma_start` | ④ HAL DMA start 수락 직후       | 호출 task |
 * | `t_isr_entry` | ⑤ 완료 ISR 진입                 | 완료 ISR |
 * | `t_cs_high`   | ⑥ CS high 직후                  | 완료 ISR |
 *
 * @note `complete=false` = task 측과 ISR 측 txn_id 가 다르다 = 아직 완료 전이거나
 *       ISR 이 인스턴스를 찾지 못한 경우. 이때 ⑤⑥ 값을 쓰면 안 된다.
 * @note BareMetal 빌드의 non-ISR(DMA blocking) 경로는 완료 콜백이 인스턴스를 해석하지
 *       못해 ⑤⑥ 가 채워지지 않는다(= 항상 `complete=false`). RTOS 경로 전용 계측이다.
 */
typedef struct {
    uint32_t txn_id;        /**< JOIN 키 — 호출자가 probe_set_txn_id 로 발급한 번호 */
    uint32_t t_lock;        /**< ② lock 획득 (DWT cycles) */
    uint32_t t_cs_low;      /**< ③ CS low */
    uint32_t t_dma_start;   /**< ④ DMA start 수락 */
    uint32_t t_isr_entry;   /**< ⑤ 완료 ISR 진입 */
    uint32_t t_cs_high;     /**< ⑥ CS high */
    bool     complete;      /**< task/ISR txn_id 일치 여부 (false = ⑤⑥ 무효) */
#if defined(AGRB_IOIF_SPI_POLL_ENABLE)
    /** 폴링 경로 트랜잭션인가. true 면 ⑤=④(ISR 없음), ⑥=task 가 올린 CS high,
     *  `complete` = 폴링 정상 완료. `complete` 뒤 패딩 자리(offset 25)라 기존 필드
     *  오프셋·sizeof(28) 불변 (PLAN-20260917 D6). */
    bool     polled;
#endif
} IOIF_SPI_ProbeTxn_t;

#endif /* IOIF_TIMING_PROBE_SPI */

typedef struct {
    AGRBStatusDef (*assign)(IOIF_SPIx_t* id, IOIF_SPI_Initialize_t* init);
    AGRBStatusDef (*write)(IOIF_SPIx_t id, void* tx_buffer, uint16_t size);
    AGRBStatusDef (*read)(IOIF_SPIx_t id, void* tx_buffer, uint16_t tx_size, void* rx_buffer, uint16_t rx_size);
    AGRBStatusDef (*duplex)(IOIF_SPIx_t id, void* tx_buffer, void* rx_buffer, uint16_t size);

    /// @brief ISR 기반 모드 시작 함수 포인터.
    AGRBStatusDef (*start_isr_mode)(IOIF_SPIx_t id, bool enable_overwrite);
    AGRBStatusDef (*stop_isr_mode)(IOIF_SPIx_t id);
    AGRBStatusDef (*change_isr_queue_capacity)(IOIF_SPIx_t id, size_t size);

    /// @brief ISR 기반 송신 처리 함수 포인터.
    AGRBStatusDef (*write_isr)(IOIF_SPIx_t id, void* tx_buffer, uint16_t total_size);
    /// @brief ISR 기반 수신 처리 함수 포인터.
    AGRBStatusDef (*read_isr)(IOIF_SPIx_t id, void* rx_buffer, uint16_t size);

    IOIF_GPIOx_t (*get_ss_pin)(IOIF_SPIx_t id);
    AGRBStatusDef (*set_ss_pin)(IOIF_SPIx_t id, IOIF_GPIOx_t ss_pin);

    AGRBStatusDef (*mode_update)(IOIF_SPIx_t id, bool cpol, bool cpha);

    AGRBStatusDef (*reset)(IOIF_SPIx_t id);

#if IOIF_TIMING_PROBE_SPI
    /**
     * @brief 다음 트랜잭션에 붙일 txn_id 발급 (W0 프로브)
     * @details 호출자가 write/read/duplex **직전에** 호출한다. IOIF 는 이 번호를
     *          task mailbox 와 ISR mailbox 양쪽에 그대로 실어 두 mailbox 를 묶는다.
     *          번호 체계(단조 증가·wrap 판정)는 호출자 몫이다.
     * @note 0 은 "아직 기록 없음" 예약값 — 발급은 1 부터 시작한다.
     * @note 유효한 사용 순서는 "발급 → 트랜잭션 호출 → 반환 후 probe_last_txn" 이다.
     *       write/read/duplex 는 완료 ISR 이 신호를 준 뒤에 반환하므로(ISR 이 스탬프를
     *       먼저 쓰고 신호한다) 이 순서에서는 번호가 어긋나지 않는다. 반면 비동기
     *       경로(write_isr/read_isr)는 발급과 이전 트랜잭션의 완료가 겹칠 수 있어
     *       ISR 이 새 번호를 스탬프할 수 있다 — 그 경로에서는 계측을 신뢰하지 말 것.
     * @return AGRBStatus_OK / AGRBStatus_ERROR (invalid id)
     */
    AGRBStatusDef (*probe_set_txn_id)(IOIF_SPIx_t id, uint32_t txn_id);

    /**
     * @brief 마지막 트랜잭션의 DWT 스탬프 조회 (W0 프로브)
     * @details txn_id 를 seq 로 쓰는 재읽기 검증 — 읽는 도중 새 트랜잭션이 끼어들면
     *          최대 3회 재시도한다.
     * @return AGRBStatus_OK / AGRBStatus_EMPTY (기록 없음) / AGRBStatus_BUSY (3회 모두
     *         경합 — 찢어진 값을 반환하느니 실패로 알린다) / AGRBStatus_ERROR (인자 오류)
     * @note `IOIF_DWT_Init()` 선행 필수 — 미호출 시 모든 스탬프가 0.
     */
    AGRBStatusDef (*probe_last_txn)(IOIF_SPIx_t id, IOIF_SPI_ProbeTxn_t* out);
#endif /* IOIF_TIMING_PROBE_SPI */

#if defined(AGRB_IOIF_SPI_POLL_ENABLE)
    /* ===== 소형 전송 폴링 경로 (F′, PLAN-20260917) — 게이트 AGRB_IOIF_SPI_POLL_ENABLE =====
     * 기존 write/read 는 그대로 두고 **별도 진입점**으로 둔다. 시그니처가 write/read 와 같아
     * 호출자는 wire 길이로 함수 포인터만 고르면 된다. 경로 선택(DMA vs 폴링)은 호출자 몫. */

    /**
     * @brief 폴링 deadline 설정 (인스턴스별 1회)
     * @param deadline_us 1 ~ IOIF_SPI_POLL_DEADLINE_MAX_US. 전송 1건의 DWT 기준 상한
     * @return AGRBStatus_OK / AGRBStatus_PARAM_ERROR (0 또는 상한 초과) / AGRBStatus_ERROR (invalid id)
     * @note 미설정이면 read_polled/write_polled 가 AGRBStatus_NOT_INITIALIZED (fail-closed).
     * @note deadline 은 **전송**만 묶는다. 실패 복구(HAL_SPI_Abort)는 HW 가 SUSP 를 풀지 않으면
     *       HAL 내부 반복 카운트만큼 더 걸릴 수 있다(기존 DMA 경로 복구와 같은 성질).
     */
    AGRBStatusDef (*set_poll_deadline)(IOIF_SPIx_t id, uint32_t deadline_us);

    /**
     * @brief read 의 폴링판 — DMA·인터럽트·세마포어 없이 FIFO 를 직접 폴링
     * @details wire = tx_size + rx_size. [tx_buffer | 0x00 × rx_size] 를 클럭하고, 앞 tx_size
     *          바이트를 버린 rx_size 바이트를 rx_buffer 에 게시한다 (read 와 같은 의미).
     * @return AGRBStatus_OK
     *         / AGRBStatus_BUFFER_OVERFLOW  wire > 인스턴스 FIFO (SPI1~3 16 B, SPI4~6 8 B) — DMA 로 우회하지 않는다
     *         / AGRBStatus_NOT_INITIALIZED  deadline 미설정 또는 DWT 미가동
     *         / AGRBStatus_NOT_SUPPORTED    master·2-lines·8-bit·FifoThreshold 01DATA 가 아님, 또는 H7+RTOS 외 빌드
     *         / AGRBStatus_BUSY             isr_mode 활성 또는 SPI 가 유휴 상태가 아님
     *         / AGRBStatus_TIMEOUT          device mutex 획득 실패 또는 전송 deadline 초과
     *         / AGRBStatus_ERROR            인자 오류, 전송 중 SPE/HAL State 가 바뀜, OVR/UDR/MODF
     * @note 실패 시 rx_buffer 는 **건드리지 않는다**(부분 수신 미게시).
     * @note Task 컨텍스트 전용. device mutex 를 쥔 채 양보 없이 폴링한다 — 보유 시간 ≤ deadline(+실패 복구).
     * @note D-Cache 조작 없음: TX/RX 모두 CPU 가 TXDR/RXDR 을 직접 다룬다.
     */
    AGRBStatusDef (*read_polled)(IOIF_SPIx_t id, void* tx_buffer, uint16_t tx_size, void* rx_buffer, uint16_t rx_size);

    /**
     * @brief write 의 폴링판 — 반환값·제약은 read_polled 와 같다 (wire = size, RX 는 버린다)
     */
    AGRBStatusDef (*write_polled)(IOIF_SPIx_t id, void* tx_buffer, uint16_t size);
#endif /* AGRB_IOIF_SPI_POLL_ENABLE */

} IOIF_SPI_Handle_t;

extern IOIF_SPI_Handle_t ioif_spi;

#endif /* AGRB_IOIF_SPI_ENABLE */

#endif /* _IOIF_AGRB_SPI_H_ */
