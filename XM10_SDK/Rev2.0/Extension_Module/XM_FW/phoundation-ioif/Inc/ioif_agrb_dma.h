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

/*-------------------------------------------------------------------------
 * DMA Pool Manager 와 무관한 공용 조회 유틸 - AGRB_IOIF_DMA_ENABLE 가드 밖이다.
 *
 * [주의] 이 선언/정의를 아래 ENABLE 가드 안으로 옮기지 말 것. UART/ADC 의
 * Manual-Init 가 자기 ENABLE 플래그만으로 DMA NVIC 를 설정해야 하는데,
 * DMA_ENABLE 을 켜지 않는 소비 모듈이 실제로 있다 - pheel-imu 는 UART_ENABLE 만,
 * pheel-emg 는 ADC_ENABLE 만 켠다(각 모듈 ioif_conf.h 실측, 2026-09-10).
 * 가드 안으로 넣으면 그 두 모듈에서 undefined reference 가 난다.
 *
 * MCU die 헤더만 include 한다(HAL 전체 아님). ioif_agrb_defs.h 는 CMSIS/HAL
 * 타입을 주지 않으므로 IRQn_Type / DMA_Stream_TypeDef / DMA_Channel_TypeDef 를
 * 여기서 확보한다.
 *-----------------------------------------------------------------------*/
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h743xx.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx.h"
#endif

/**
 * @brief 주입된 DMA stream(H7) / channel(G4) 포인터에서 NVIC IRQ 번호를 조회한다.
 * @param dma_instance HAL DMA 핸들의 Instance 필드 (예: hdma.Instance)
 * @return 매핑된 IRQn. 모르는 stream/channel 이면 (IRQn_Type)0 -
 *         호출자는 0 이면 NVIC 를 건드리지 않아야 한다(fail-safe).
 * @note ioif_agrb_uart.c / ioif_agrb_adc.c 의 Manual-Init NVIC 설정 공용 진입점.
 *       예전에는 두 파일이 같은 표를 각자 갖고 있었다(PLAN-20260907 E4-c 통합).
 */
IRQn_Type IOIF_DMA_GetIrqFromInstance(void* dma_instance);

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
