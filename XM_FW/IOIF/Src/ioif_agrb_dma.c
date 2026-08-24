/**
 ******************************************************************************
 * @file    ioif_agrb_dma.c
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF] DMA Pool Manager 구현부 (aeat_9955 origin)
 * @version 3.0 (Common Library - H7/G4 Dual Platform)
 * @date    Feb 12, 2026
 *
 * @details
 * - H7: DMA/BDMA/MDMA 3개 풀 관리, 링커 섹션별 배치
 * - G4: DMA 풀 1개만 관리, 일반 SRAM
 *
 * @note aeat_9955 원본 기반, H7/G4 듀얼 플랫폼 + ENABLE 가드 적용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_dma.h"

#if defined(AGRB_IOIF_DMA_ENABLE)

#include <stdbool.h>
#include <string.h>

#define IOIF_DMA_DEFAULT_ALLOC_SIZE    (128) //bytes

/**
 *-----------------------------------------------------------
 * INTERNAL DMA BUFFER POOLS
 *-----------------------------------------------------------
 */
#if defined(IOIF_MCU_SERIES_H7)
    /* H7: 3개 풀, 링커 섹션별 배치 (MPU Non-cacheable 영역)
     * aligned(32): D-Cache 라인 경계 정렬 — 인접 변수 cache line 오염 방지 */
    static uint8_t  _dma_buffer_pool[IOIF_DMA_POOL_SIZE]      __attribute__((section(IOIF_DMA_SECTION),  aligned(32))) = {0};
    static uint8_t  _bdma_buffer_pool[IOIF_BDMA_POOL_SIZE]    __attribute__((section(IOIF_BDMA_SECTION), aligned(32))) = {0};
    static uint8_t  _mdma_buffer_pool[IOIF_MDMA_POOL_SIZE]    __attribute__((section(IOIF_MDMA_SECTION), aligned(32))) = {0};
#elif defined(IOIF_MCU_SERIES_G4)
    /* G4: 1개 풀, 일반 SRAM (D-Cache 없음) */
    static uint8_t  _dma_buffer_pool[IOIF_DMA_POOL_SIZE] __attribute__((aligned(4))) = {0};
#endif

/**
 *-----------------------------------------------------------
 * INTERNAL DMA POOL USAGE TRACKERS
 *-----------------------------------------------------------
 */
typedef struct {
    struct {
        size_t used;
        size_t total;
        uint16_t use_count;
    } dma;
#if defined(IOIF_MCU_SERIES_H7)
    struct {
        size_t used;
        size_t total;
        uint16_t use_count;
    } bdma;
    struct {
        size_t used;
        size_t total;
        uint16_t use_count;
    } mdma;
#endif

    uint16_t total_count;

} IOIF_DMA_Manager_t;

/** Internal DMA channel trackers **/
static IOIF_DMAx_t _dma_channels[MAX_DMA_CHANNELS];
static IOIF_DMA_Manager_t _dma_manager = {
    .dma = {
        .used = 0,
        .total = IOIF_DMA_POOL_SIZE,
        .use_count = 0,
    },
#if defined(IOIF_MCU_SERIES_H7)
    .bdma = {
        .used = 0,
        .total = IOIF_BDMA_POOL_SIZE,
        .use_count = 0,
    },
    .mdma = {
        .used = 0,
        .total = IOIF_MDMA_POOL_SIZE,
        .use_count = 0,
    },
#endif

    .total_count = 0,
};

/**********************************************/
/** Helper Functions***************************/
/**********************************************/
static IOIF_DMA_Type_e _ioif_dma_get_type_from_hdma(DMA_HandleTypeDef* hdma);
/**********************************************/
/** DMA Handle Functions **********************/
/**********************************************/
static uint32_t _ioif_dma_get_max_channels(void);
static uint32_t _ioif_dma_get_used_channels(IOIF_DMA_Type_e type);
static size_t _ioif_dma_get_remain(IOIF_DMA_Type_e type);
static IOIF_DMAx_t* _ioif_dma_allocate(DMA_HandleTypeDef* hdma, size_t size, const char* name);
/*********************************************************************/
IOIF_DMA_Handle_t ioif_dma = {
    .get_max_dma_channels = _ioif_dma_get_max_channels,
    .get_used_dma_channels = _ioif_dma_get_used_channels,
    .get_dma_remain = _ioif_dma_get_remain,
    .allocate = _ioif_dma_allocate,
};
/**********************************************/
static uint32_t _ioif_dma_get_max_channels(void)
{
    return MAX_DMA_CHANNELS;
}
/**********************************************/
static uint32_t _ioif_dma_get_used_channels(IOIF_DMA_Type_e type)
{
    switch (type) {
        case IOIF_DMA_Type_DMA:
            return _dma_manager.dma.use_count;
#if defined(IOIF_MCU_SERIES_H7)
        case IOIF_DMA_Type_BDMA:
            return _dma_manager.bdma.use_count;
        case IOIF_DMA_Type_MDMA:
            return _dma_manager.mdma.use_count;
#endif
        default:
            return 0;
    }
}
/**********************************************/
static size_t _ioif_dma_get_remain(IOIF_DMA_Type_e type)
{
    switch (type) {
        case IOIF_DMA_Type_DMA:
            return _dma_manager.dma.total - _dma_manager.dma.used;
#if defined(IOIF_MCU_SERIES_H7)
        case IOIF_DMA_Type_BDMA:
            return _dma_manager.bdma.total - _dma_manager.bdma.used;
        case IOIF_DMA_Type_MDMA:
            return _dma_manager.mdma.total - _dma_manager.mdma.used;
#endif
        default:
            return 0;
    }
}
/**********************************************/
static IOIF_DMAx_t* _ioif_dma_allocate(DMA_HandleTypeDef* hdma, size_t size, const char* name)
{
    if ( _dma_manager.total_count >= MAX_DMA_CHANNELS) {
        return NULL; //No more DMA channel available
    }

    //if ( size == 0 ) size = IOIF_DMA_DEFAULT_ALLOC_SIZE;
    if ( size == 0 ) return NULL;

    /* H7: 32-byte 정렬 (D-Cache 라인 크기), G4: 4-byte 정렬 (D-Cache 없음) */
#if defined(IOIF_MCU_SERIES_H7)
    size = (size + 31) & ~(size_t)0x1F;
#else
    size = (size + 3) & ~(size_t)0x03;
#endif

    IOIF_DMAx_t* dma_channel = &_dma_channels[_dma_manager.total_count];

    IOIF_DMA_Type_e type = _ioif_dma_get_type_from_hdma(hdma);

    switch (type) {
        case IOIF_DMA_Type_DMA:
            if ( ( _dma_manager.dma.used + size ) > IOIF_DMA_POOL_SIZE ) return NULL; //Not enough DMA pool
            dma_channel->type = IOIF_DMA_Type_DMA;
            dma_channel->buffer = &_dma_buffer_pool[_dma_manager.dma.used];
            dma_channel->size = size;
            _dma_manager.dma.used += size;
            _dma_manager.dma.use_count++;
            break;
#if defined(IOIF_MCU_SERIES_H7)
        case IOIF_DMA_Type_BDMA:
            if ( (_dma_manager.bdma.used + size) > IOIF_BDMA_POOL_SIZE ) return NULL; //Not enough BDMA pool
            dma_channel->type = IOIF_DMA_Type_BDMA;
            dma_channel->buffer = &_bdma_buffer_pool[_dma_manager.bdma.used];
            dma_channel->size = size;
            _dma_manager.bdma.used += size;
            _dma_manager.bdma.use_count++;
            break;
        case IOIF_DMA_Type_MDMA:
            if ( (_dma_manager.mdma.used + size) > IOIF_MDMA_POOL_SIZE ) return NULL; //Not enough MDMA pool
            dma_channel->type = IOIF_DMA_Type_MDMA;
            dma_channel->buffer = &_mdma_buffer_pool[_dma_manager.mdma.used];
            dma_channel->size = size;
            _dma_manager.mdma.used += size;
            _dma_manager.mdma.use_count++;
            break;
#endif
        case IOIF_DMA_Type_NONE:
        case IOIF_DMA_Type_INVALID:
        default:
            return NULL; //Invalid type
    }

    //Clean buffer
    memset( dma_channel->buffer, 0, size );

    if ( name != NULL ) {
        strncpy( (char*)dma_channel->name, name, IOIF_DMA_NAME_SIZE - 1 );
        dma_channel->name[IOIF_DMA_NAME_SIZE - 1] = '\0'; //Ensure null termination
    } else {
        strncpy( (char*)dma_channel->name, "unnamed", IOIF_DMA_NAME_SIZE - 1 );
        dma_channel->name[IOIF_DMA_NAME_SIZE - 1] = '\0';
    }

    //Update manager
    _dma_manager.total_count++;

    return dma_channel;
}

static IOIF_DMA_Type_e _ioif_dma_get_type_from_hdma(DMA_HandleTypeDef* hdma)
{
    if ( hdma == NULL ) return IOIF_DMA_Type_NONE; //hdma NULL → 할당 거부 (NONE → switch default → return NULL)

#if defined(IOIF_MCU_SERIES_H7)
    void* instance = hdma->Instance; //MDMA, BDMA, DMA 모두 공통 필드

    if ( IS_DMA_STREAM_INSTANCE( instance ) ) return IOIF_DMA_Type_DMA;
    if ( IS_BDMA_CHANNEL_INSTANCE( instance ) ) return IOIF_DMA_Type_BDMA;
    if ( IS_MDMA_STREAM_ALL_INSTANCE( instance ) ) return IOIF_DMA_Type_MDMA;

    return IOIF_DMA_Type_INVALID; //Unknown type
#elif defined(IOIF_MCU_SERIES_G4)
    /* G4: 모든 DMA 채널은 일반 DMA */
    (void)hdma;
    return IOIF_DMA_Type_DMA;
#else
    (void)hdma;
    return IOIF_DMA_Type_DMA;
#endif
}

#endif /* AGRB_IOIF_DMA_ENABLE */
