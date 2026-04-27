/**
 ******************************************************************************
 * @file    ioif_agrb_psram.h
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] PSRAM (QSPI) 하드웨어 추상화 헤더 (aeat_9955 origin)
 * @version 3.0 (Common Library - H7 Only)
 * @date    Feb 12, 2026
 *
 * @details
 * - Handle-based API: ioif_psram.alloc(), ioif_psram.write() 등
 * - QSPI + Memory Mapped 모드 지원
 * - FreeRTOS DMA 기반 전송
 * - H7 전용 하드웨어 (QSPI + PSRAM)
 *
 * @note aeat_9955 원본 기반, ENABLE 가드 적용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef _IOIF_AGRB_PSRAM_H_
#define _IOIF_AGRB_PSRAM_H_

#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_PSRAM_ENABLE) && defined(IOIF_MCU_SERIES_H7)

#if !defined(IOIF_PSRAM_BENCHMARK_DISABLE)
#define IOIF_PSRAM_BENCHMARK_AVAILABLE
#endif

#include <stdint.h>
#include <stddef.h>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_qspi.h"

typedef struct {
    QSPI_HandleTypeDef* hqspi;

} IOIF_PSRAM_Initialize_t;

typedef uint32_t IOIF_PSRAMx_t;

typedef struct {
    struct {
        size_t target_size_bytes;
        size_t chunk_size_bytes;
    } memory;

    struct {
        float embed;
        float psram;
        float ratio;
    } sequential_write;

    struct {
        float embed;
        float psram;
        float ratio;
    } sequential_read;

    struct {
        float embed;
        float psram;
        float ratio;
    } random_write;

    struct {
        float embed;
        float psram;
        float ratio;
    } random_read;

} IOIF_PSRAM_Benchmark_t;

typedef struct {
    AGRBStatusDef (*initialize)(IOIF_PSRAM_Initialize_t* config);
    IOIF_PSRAMx_t (*alloc)(size_t size);

    bool (*write)(IOIF_PSRAMx_t id, void* data, size_t size);
    bool (*write_offset)(IOIF_PSRAMx_t id, uint32_t offset, void* data, size_t size);
    const void* (*get_mapped_address)(IOIF_PSRAMx_t id);
    void (*start_memory_mapped)(void);
    void (*stop_memory_mapped)(void);
    void (*free)(IOIF_PSRAMx_t id);
    void (*flush)(void);
    size_t (*get_total_size)(void);

    void (*benchmark)(IOIF_PSRAM_Benchmark_t* report);

} IOIF_PSRAM_Handle_t;

extern IOIF_PSRAM_Handle_t ioif_psram;

#endif /* AGRB_IOIF_PSRAM_ENABLE && IOIF_MCU_SERIES_H7 */

#endif /* _IOIF_AGRB_PSRAM_H_ */
