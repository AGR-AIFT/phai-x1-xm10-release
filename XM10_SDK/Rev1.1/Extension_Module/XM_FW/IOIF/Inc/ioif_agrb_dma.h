/**
 ******************************************************************************
 * @file    ioif_agrb_dma.h
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF] DMA Pool Manager - Handle-based API (aeat_9955 origin)
 * @version 3.0 (Common Library - H7/G4 Dual Platform)
 * @date    Feb 12, 2026
 *
 * @details
 * - H7: DMA/BDMA/MDMA 3개 풀 관리, 링커 섹션별 배치
 * - G4: DMA 풀 1개만 관리, 일반 SRAM
 * - Handle-based API: ioif_dma.allocate() 패턴
 * - Pool 크기는 ioif_conf.h에서 #ifndef 오버라이드 가능
 *
 * @note aeat_9955 원본 기반, H7/G4 듀얼 플랫폼 + ENABLE 가드 적용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef _IOIF_AGRB_DMA_H_
#define _IOIF_AGRB_DMA_H_

#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_DMA_ENABLE)

#include <stdint.h>
#include <stddef.h>

/* HAL Includes (MCU-specific) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_dma.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx_hal.h"
    #include "stm32g4xx_hal_dma.h"
#else
    #error "Unsupported MCU series for IOIF DMA module"
#endif

#define MAX_DMA_CHANNELS            (16)

/**
 * Pool Sizes (overridable via ioif_conf.h)
 * - H7: 3개 풀 (DMA, BDMA, MDMA)
 * - G4: 1개 풀 (DMA only)
 */
#if defined(IOIF_MCU_SERIES_H7)
    #ifndef IOIF_DMA_POOL_SIZE
        #define IOIF_DMA_POOL_SIZE          (56 * 1024)
    #endif
    #ifndef IOIF_BDMA_POOL_SIZE
        #define IOIF_BDMA_POOL_SIZE         (1 * 1024)
    #endif
    #ifndef IOIF_MDMA_POOL_SIZE
        #define IOIF_MDMA_POOL_SIZE         (4 * 1024)
    #endif
#elif defined(IOIF_MCU_SERIES_G4)
    #ifndef IOIF_DMA_POOL_SIZE
        #define IOIF_DMA_POOL_SIZE          (4 * 1024)
    #endif
#endif

#define IOIF_DMA_NAME_SIZE    (24)

typedef enum {
    IOIF_DMA_Type_NONE,

    IOIF_DMA_Type_DMA,
#if defined(IOIF_MCU_SERIES_H7)
    IOIF_DMA_Type_BDMA,
    IOIF_DMA_Type_MDMA,
#endif

    IOIF_DMA_Type_INVALID,
} IOIF_DMA_Type_e;

typedef struct {
    IOIF_DMA_Type_e type;
    uint8_t* buffer;
    size_t size;
    uint8_t name[IOIF_DMA_NAME_SIZE];
} IOIF_DMAx_t;

typedef struct {
    uint32_t (*get_max_dma_channels)(void);
    uint32_t (*get_used_dma_channels)(IOIF_DMA_Type_e);
    size_t (*get_dma_remain)(IOIF_DMA_Type_e type);

    IOIF_DMAx_t* (*allocate)(DMA_HandleTypeDef* hdma, size_t size, const char* name); //각 DMA 핸들러에 맞게 할당
} IOIF_DMA_Handle_t;

extern IOIF_DMA_Handle_t ioif_dma;

#endif /* AGRB_IOIF_DMA_ENABLE */

#endif /* _IOIF_AGRB_DMA_H_ */
