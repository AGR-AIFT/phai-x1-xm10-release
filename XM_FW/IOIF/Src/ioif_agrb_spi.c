/**
 ******************************************************************************
 * @file    ioif_agrb_spi.c
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] SPI 하드웨어 추상화 계층 구현부
 * @version 4.0 (Production Quality - Deadlock-Free Redesign)
 * @date    Mar 22, 2026
 *
 * @details
 * - Handle-based API: ioif_spi.write(), ioif_spi.read() 등
 * - Resource Pool + Instance 2계층 아키텍처 (같은 SPI 버스의 디바이스가 Mutex 공유)
 * - H7/G4 통합 (D-Cache 조건부, HAL 조건부)
 * - DMA Pool Manager 연동 (AGRB_IOIF_DMA_ENABLE)
 *
 * @note Semaphore Symmetry: DEV_LOCK/DEV_UNLOCK은 항상 같은 함수 내에서 대칭.
 *       ISR에서는 SIGNAL_TX/RX_ISR만 호출, DEV_UNLOCK 금지.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_spi.h"

#if defined(AGRB_IOIF_SPI_ENABLE)

#include <string.h>

#if defined(USE_FREERTOS)
#include "cmsis_os2.h"
#endif

#if defined(AGRB_IOIF_DMA_ENABLE)
#include "ioif_agrb_dma.h"
#endif

/** @defgroup SPI
  * @brief SPI HAL I/O module driver
  * @{
  */

#define STATIC_ISR_MESSAGE_QUEUE_LENGTH    (32)
#define STATIC_ISR_MESSAGE_QUEUE_MASK      (STATIC_ISR_MESSAGE_QUEUE_LENGTH - 1)

typedef struct {
    uint8_t data[IOIF_SPI_ISR_MQ_UNIT_SIZE];
} SPI_ISR_Message_t;

typedef struct {
    IOIF_SPI_Initialize_t init;

    volatile uint32_t error_count;

    uint32_t mutex_index; //Index for semaphore arrays
    struct {
        bool        is_enabled;
        bool        enable_overwrite;

        #if defined(USE_FREERTOS)
        osMessageQueueId_t isr_rx; //ISR 기반 수신용 메시지 큐
        #else
        struct{
            uint32_t head;
            uint32_t tail;
            SPI_ISR_Message_t messages[STATIC_ISR_MESSAGE_QUEUE_LENGTH];
        } message;
        #endif
        struct {
            IOIF_DMAx_t* tx;
            IOIF_DMAx_t* rx;
        } dma;
    } isr_mode;

} IOIF_SPI_Instance_t;


typedef struct {
#if defined(USE_FREERTOS)
    IOIF_SPIx_t current; //현재 점유중인 사용자 ID

    SPI_HandleTypeDef* hspi;
    SemaphoreHandle_t device;   // xSemaphoreCreateMutex() — Priority Inheritance
    SemaphoreHandle_t tx_done;  // Binary — TX DMA 완료 신호
    SemaphoreHandle_t rx_done;  // Binary — RX DMA 완료 신호

    struct {
        IOIF_DMAx_t* tx;
        IOIF_DMAx_t* rx;
    } dma;

    struct {
        bool        is_enabled;
        bool        enable_overwrite;
    } isr_mode;
#endif
} IOIF_SPI_Resource_t;

/*********************************************************************************/
/* ===== Semaphore 매크로 (FDCAN TX_LOCK 패턴) =====
 * - SPI_DEV_LOCK: bool 반환 (호출자가 에러 처리)
 * - SPI_DEV_UNLOCK: void (항상 성공)
 * - SPI_WAIT_TX/RX: bool 반환 (timeout 시 false)
 * - SPI_SIGNAL_TX/RX_ISR: ISR 전용 (device mutex 터치 안 함)
 */

#if defined(USE_FREERTOS)

    #define SPI_DEV_LOCK(idx)   (_spi_resource_pool[idx].device != NULL && \
                                 xSemaphoreTake(_spi_resource_pool[idx].device, \
                                 pdMS_TO_TICKS(IOIF_SPI_DEFAULT_TIMEOUT)) == pdTRUE)

    #define SPI_DEV_UNLOCK(idx) do { \
        if (_spi_resource_pool[idx].device != NULL) { \
            xSemaphoreGive(_spi_resource_pool[idx].device); \
        } \
    } while(0)

    #define SPI_WAIT_TX(idx)    (_spi_resource_pool[idx].tx_done != NULL && \
                                 xSemaphoreTake(_spi_resource_pool[idx].tx_done, \
                                 pdMS_TO_TICKS(IOIF_SPI_DEFAULT_TIMEOUT)) == pdTRUE)

    #define SPI_WAIT_RX(idx)    (_spi_resource_pool[idx].rx_done != NULL && \
                                 xSemaphoreTake(_spi_resource_pool[idx].rx_done, \
                                 pdMS_TO_TICKS(IOIF_SPI_DEFAULT_TIMEOUT)) == pdTRUE)

    #define SPI_SIGNAL_TX_ISR(idx) do { \
        if (_spi_resource_pool[idx].tx_done != NULL) { \
            BaseType_t xWoken = pdFALSE; \
            xSemaphoreGiveFromISR(_spi_resource_pool[idx].tx_done, &xWoken); \
            portYIELD_FROM_ISR(xWoken); \
        } \
    } while(0)

    #define SPI_SIGNAL_RX_ISR(idx) do { \
        if (_spi_resource_pool[idx].rx_done != NULL) { \
            BaseType_t xWoken = pdFALSE; \
            xSemaphoreGiveFromISR(_spi_resource_pool[idx].rx_done, &xWoken); \
            portYIELD_FROM_ISR(xWoken); \
        } \
    } while(0)

#else
    /* BareMetal: 모든 매크로 no-op */
    #define SPI_DEV_LOCK(idx)       (true)
    #define SPI_DEV_UNLOCK(idx)     ((void)0)
    #define SPI_WAIT_TX(idx)        (true)
    #define SPI_WAIT_RX(idx)        (true)
    #define SPI_SIGNAL_TX_ISR(idx)  ((void)0)
    #define SPI_SIGNAL_RX_ISR(idx)  ((void)0)
#endif

/*********************************************************************************/
static AGRBStatusDef ioif_spi_assign_instance(IOIF_SPIx_t* id, IOIF_SPI_Initialize_t* init);
static AGRBStatusDef ioif_spi_write(IOIF_SPIx_t id, void* tx_buffer, uint16_t size);
static AGRBStatusDef ioif_spi_read(IOIF_SPIx_t id, void* tx_buffer, uint16_t tx_size, void* rx_buffer, uint16_t rx_size);
static AGRBStatusDef ioif_spi_duplex(IOIF_SPIx_t id, void* tx_buffer, void* rx_buffer, uint16_t size);
static AGRBStatusDef ioif_spi_reset(IOIF_SPIx_t id);

static AGRBStatusDef ioif_spi_start_isr_mode(IOIF_SPIx_t id, bool enable_overwrite);
static AGRBStatusDef ioif_spi_stop_isr_mode(IOIF_SPIx_t id);
static AGRBStatusDef ioif_spi_change_isr_queue_capacity(IOIF_SPIx_t id, size_t size);
static AGRBStatusDef ioif_spi_write_isr(IOIF_SPIx_t id, void* tx_buffer, uint16_t total_size);
static AGRBStatusDef ioif_spi_read_isr(IOIF_SPIx_t id, void* rx_buffer, uint16_t size);

static AGRBStatusDef ioif_spi_set_ss_pin(IOIF_SPIx_t id, IOIF_GPIOx_t ss_pin);
static IOIF_GPIOx_t ioif_spi_get_ss_pin(IOIF_SPIx_t id);
static AGRBStatusDef ioif_spi_mode_update(IOIF_SPIx_t id, bool cpol, bool cpha);
/*********************************************************************************/
IOIF_SPI_Handle_t ioif_spi = {
    .assign     = ioif_spi_assign_instance,
    .write      = ioif_spi_write,
    .read       = ioif_spi_read,
    .duplex     = ioif_spi_duplex,
    .reset      = ioif_spi_reset,
    .start_isr_mode = ioif_spi_start_isr_mode,
    .stop_isr_mode  = ioif_spi_stop_isr_mode,
    .change_isr_queue_capacity = ioif_spi_change_isr_queue_capacity,
    .write_isr      = ioif_spi_write_isr,
    .read_isr       = ioif_spi_read_isr,
    .get_ss_pin    = ioif_spi_get_ss_pin,
    .set_ss_pin    = ioif_spi_set_ss_pin,
    .mode_update   = ioif_spi_mode_update,
};
/*********************************************************************************/

static IOIF_SPI_Resource_t _spi_resource_pool[IOIF_SPI_MAX_HANDLERS];
static IOIF_SPIx_t _spi_resource_count = 0;

static IOIF_SPI_Instance_t _spi_instances[IOIF_SPI_MAX_INSTANCES];
static IOIF_SPIx_t _spi_instances_count = 0;

static void _cs_low(IOIF_SPI_Instance_t* instance);
static void _cs_high(IOIF_SPI_Instance_t* instance);
static IOIF_SPI_Instance_t* _get_instance(SPI_HandleTypeDef* hspi);
#if defined(USE_FREERTOS)
static uint32_t _get_resource_index_by_hspi(SPI_HandleTypeDef* hspi);
#endif

/*********************************************************************************/

static AGRBStatusDef ioif_spi_assign_instance(IOIF_SPIx_t* id, IOIF_SPI_Initialize_t* init)
{
    if (id == NULL || init == NULL) return AGRBStatus_ERROR;

    *id = IOIF_SPI_ID_NOT_ALLOCATED;

    if (_spi_instances_count >= IOIF_SPI_MAX_INSTANCES) {
        return AGRBStatus_INITIAL_FAILED;
    }

    for (IOIF_SPIx_t i = 0; i < _spi_instances_count; i++) {
        if (_spi_instances[i].init.hspi == init->hspi
            && _spi_instances[i].init.ss == init->ss ) {
            return AGRBStatus_BUSY;
        }
    }

    IOIF_SPI_Instance_t* instance = &_spi_instances[_spi_instances_count];

    memset(instance, 0, sizeof(IOIF_SPI_Instance_t));
    memcpy(&instance->init, init, sizeof(IOIF_SPI_Initialize_t));

    if (instance->init.timeout == 0) {
        instance->init.timeout = IOIF_SPI_DEFAULT_TIMEOUT;
    }

    #if defined(USE_FREERTOS)

    if ( init->dma.tx_size == 0 || init->dma.rx_size == 0 ) {
        return AGRBStatus_ERROR;
    }

    bool found_existing = false;
    for ( uint32_t i = 0; i < _spi_resource_count; i++ ) {
        if ( _spi_resource_pool[i].hspi == init->hspi ) {
            found_existing = true;
            instance->mutex_index = i;
            break;
        }
    }

    if ( !found_existing ) {
        if ( _spi_resource_count >= IOIF_SPI_MAX_HANDLERS ) return AGRBStatus_ERROR;

        uint32_t new_index = _spi_resource_count;

        IOIF_SPI_Resource_t* spi_resource = &_spi_resource_pool[new_index];

        memset(spi_resource, 0, sizeof(IOIF_SPI_Resource_t));

        spi_resource->current  = IOIF_SPI_ID_NOT_ALLOCATED;
        spi_resource->hspi     = init->hspi;

        /* Mutex (Priority Inheritance) for device bus access */
        spi_resource->device   = xSemaphoreCreateMutex();
        /* Binary semaphores for DMA completion signals */
        spi_resource->tx_done  = xSemaphoreCreateBinary();
        spi_resource->rx_done  = xSemaphoreCreateBinary();

        if (spi_resource->device == NULL ||
            spi_resource->tx_done == NULL ||
            spi_resource->rx_done == NULL) {
            /* Rollback: 이미 생성된 semaphore 정리 */
            if (spi_resource->device != NULL) vSemaphoreDelete(spi_resource->device);
            if (spi_resource->tx_done != NULL) vSemaphoreDelete(spi_resource->tx_done);
            if (spi_resource->rx_done != NULL) vSemaphoreDelete(spi_resource->rx_done);
            memset(spi_resource, 0, sizeof(IOIF_SPI_Resource_t));
            return AGRBStatus_ERROR;
        }

        /* Mutex는 생성 시 이미 available (Give 불필요) */
        /* Binary semaphore는 생성 시 empty (ISR이 Give하기를 대기) */

        char spi_tx_name[IOIF_DMA_NAME_SIZE] = "SPI_TX_";
        char spi_rx_name[IOIF_DMA_NAME_SIZE] = "SPI_RX_";
        spi_tx_name[8] = '0' + (new_index % 10);
        spi_tx_name[9] = '\0';
        spi_rx_name[8] = '0' + (new_index % 10);
        spi_rx_name[9] = '\0';

        spi_resource->dma.tx = ioif_dma.allocate( instance->init.hspi->hdmatx, instance->init.dma.tx_size , spi_tx_name );
        spi_resource->dma.rx = ioif_dma.allocate( instance->init.hspi->hdmarx, instance->init.dma.rx_size , spi_rx_name );

        if ( spi_resource->dma.tx == NULL || spi_resource->dma.rx == NULL ) {
            vSemaphoreDelete(spi_resource->device);
            vSemaphoreDelete(spi_resource->tx_done);
            vSemaphoreDelete(spi_resource->rx_done);
            memset(spi_resource, 0, sizeof(IOIF_SPI_Resource_t));
            return AGRBStatus_DMA_ALLOCATION_ERROR;
        }

        instance->mutex_index = new_index;

        _spi_resource_count++;
    }

    /* ISR MQ 생성은 resource pool 생성과 독립 (기존 리소스에도 ISR device 추가 가능) */
    if ( instance->init.options.isr_mode.enable )
    {
        uint32_t resource_index = instance->mutex_index;
        IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];

        if ( res->isr_mode.is_enabled ) return AGRBStatus_BUSY;

        if ( instance->init.dma.tx_size < IOIF_SPI_ISR_MQ_UNIT_SIZE ||
             instance->init.dma.rx_size < IOIF_SPI_ISR_MQ_UNIT_SIZE ) {
            return AGRBStatus_ERROR;
        }

        if ( instance->isr_mode.isr_rx != NULL ) return AGRBStatus_BUSY;
        instance->isr_mode.isr_rx = osMessageQueueNew(
            instance->init.options.isr_mode.queue_length,
            sizeof(SPI_ISR_Message_t),
            NULL );
        if ( instance->isr_mode.isr_rx == NULL ) return AGRBStatus_ERROR;
    }
    #else
    {
        instance->isr_mode.dma.tx = ioif_dma.allocate( instance->init.hspi->hdmatx, IOIF_SPI_ISR_MQ_UNIT_SIZE , "ISR_TX" );
        instance->isr_mode.dma.rx = ioif_dma.allocate( instance->init.hspi->hdmarx, IOIF_SPI_ISR_MQ_UNIT_SIZE , "ISR_RX" );
    }
    #endif

    *id = _spi_instances_count++;

    return AGRBStatus_OK;
}

/**
 * @brief SPI Write (blocking, TransmitReceive DMA — RX FIFO 오염 방지)
 *
 * RTOS: DEV_LOCK → CS low → TransmitReceive_DMA → WAIT_RX → CS high → DEV_UNLOCK
 * RX 데이터는 무시 (dummy). H7 Full-Duplex에서 TX-only Transmit_DMA 사용 시
 * RX FIFO에 dummy 데이터가 쌓여 다음 Receive에서 garbage 발생하는 문제 해결.
 */
static AGRBStatusDef ioif_spi_write(IOIF_SPIx_t id, void* tx_buffer, uint16_t size)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;
    if (tx_buffer == NULL || size == 0) return AGRBStatus_ERROR;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];

    #if defined(USE_FREERTOS)
    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];

    if ( res->isr_mode.is_enabled ) return AGRBStatus_BUSY;
    if ( !SPI_DEV_LOCK(resource_index) ) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;
    HAL_StatusTypeDef hal_status = HAL_OK;

    do {
        if (size > res->dma.tx->size || size > res->dma.rx->size) {
            result = AGRBStatus_BUFFER_OVERFLOW;
            break;
        }

        res->current = id;

        uint8_t* tx_dma_buffer = res->dma.tx->buffer;
        uint8_t* rx_dma_buffer = res->dma.rx->buffer;
        memcpy(tx_dma_buffer, (uint8_t*)tx_buffer, size );
        memset(rx_dma_buffer, 0, size);

        _cs_low(instance);

        #if IOIF_HAS_DCACHE
        SCB_CleanDCache_by_Addr((uint32_t*)tx_dma_buffer, size);
        #endif
        hal_status = HAL_SPI_TransmitReceive_DMA(res->hspi, tx_dma_buffer, rx_dma_buffer, size);

        if (hal_status != HAL_OK) {
            _cs_high(instance);
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
            break;
        }

        if ( !SPI_WAIT_RX(resource_index) ) {
            _cs_high(instance);
            result = AGRBStatus_TIMEOUT;
            break;
        }

        _cs_high(instance);
        /* RX dummy 데이터 무시 — memcpy 없음 */

        res->current = IOIF_SPI_ID_NOT_ALLOCATED;
    } while(0);

    SPI_DEV_UNLOCK(resource_index);
    return result;
    #else
    {
        if (size > IOIF_SPI_DEFAULT_DUMMY_SIZE) return AGRBStatus_BUFFER_OVERFLOW;

        uint8_t rx_dummy[IOIF_SPI_DEFAULT_DUMMY_SIZE];

        _cs_low(instance);

        HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive(
            instance->init.hspi, (uint8_t*)tx_buffer, rx_dummy, size, IOIF_SPI_DEFAULT_TIMEOUT);

        _cs_high(instance);

        return IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
    }
    #endif
}

/**
 * @brief SPI Read (단일 TransmitReceive — TX prefix + RX 통합)
 *
 * RTOS: DEV_LOCK → CS low → TransmitReceive_DMA(total) → WAIT_RX → CS high → DEV_UNLOCK
 * TX 버퍼: [reg_addr(tx_size) | dummy(rx_size)], RX에서 tx_size offset 이후 유효 데이터 복사.
 * 2단계 분리(Transmit→Receive) 대비 RX FIFO 오염 방지 + 1회 DMA로 효율 향상.
 */
static AGRBStatusDef ioif_spi_read(IOIF_SPIx_t id, void* tx_buffer, uint16_t tx_size, void* rx_buffer, uint16_t rx_size)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;
    if (tx_buffer != NULL && tx_size == 0) return AGRBStatus_ERROR;
    if (tx_buffer == NULL && tx_size != 0) return AGRBStatus_ERROR;
    if (rx_buffer == NULL || rx_size == 0) return AGRBStatus_ERROR;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];

    #if defined(USE_FREERTOS)
    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];

    if ( res->isr_mode.is_enabled ) return AGRBStatus_BUSY;
    if ( !SPI_DEV_LOCK(resource_index) ) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;
    HAL_StatusTypeDef hal_status = HAL_OK;

    do {
        uint16_t total_size = (tx_buffer != NULL ? tx_size : 0) + rx_size;

        if (total_size > res->dma.tx->size || total_size > res->dma.rx->size) {
            result = AGRBStatus_BUFFER_OVERFLOW;
            break;
        }

        uint8_t* dma_tx_buffer = res->dma.tx->buffer;
        uint8_t* dma_rx_buffer = res->dma.rx->buffer;

        /* TX 버퍼 구성: [reg_addr(tx_size) | dummy(rx_size)] */
        memset(dma_tx_buffer, 0x00, total_size);
        if (tx_buffer != NULL && tx_size > 0) {
            memcpy(dma_tx_buffer, (uint8_t*)tx_buffer, tx_size);
        }
        memset(dma_rx_buffer, 0, total_size);

        res->current = id;
        _cs_low(instance);

        #if IOIF_HAS_DCACHE
        SCB_CleanDCache_by_Addr((uint32_t*)dma_tx_buffer, total_size);
        #endif
        hal_status = HAL_SPI_TransmitReceive_DMA(res->hspi, dma_tx_buffer, dma_rx_buffer, total_size);

        if (hal_status != HAL_OK) {
            _cs_high(instance);
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
            break;
        }

        if ( !SPI_WAIT_RX(resource_index) ) {
            _cs_high(instance);
            result = AGRBStatus_TIMEOUT;
            break;
        }

        #if IOIF_HAS_DCACHE
        SCB_InvalidateDCache_by_Addr((uint32_t*)dma_rx_buffer, total_size);
        #endif

        /* RX 데이터에서 TX 구간 skip, 유효 데이터만 복사 */
        uint16_t rx_offset = (tx_buffer != NULL) ? tx_size : 0;
        memcpy((uint8_t*)rx_buffer, dma_rx_buffer + rx_offset, rx_size);

        _cs_high(instance);

        res->current = IOIF_SPI_ID_NOT_ALLOCATED;
    } while(0);
    SPI_DEV_UNLOCK(resource_index);
    return result;
    #else
    {
        uint16_t total_size = (tx_buffer != NULL ? tx_size : 0) + rx_size;
        uint8_t tx_combined[IOIF_SPI_DEFAULT_DUMMY_SIZE];
        uint8_t rx_combined[IOIF_SPI_DEFAULT_DUMMY_SIZE];

        if (total_size > IOIF_SPI_DEFAULT_DUMMY_SIZE) return AGRBStatus_BUFFER_OVERFLOW;

        /* TX 버퍼 구성: [reg_addr | dummy] */
        memset(tx_combined, 0x00, total_size);
        if (tx_buffer != NULL && tx_size > 0) {
            memcpy(tx_combined, (uint8_t*)tx_buffer, tx_size);
        }

        _cs_low(instance);

        HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive(
            instance->init.hspi, tx_combined, rx_combined, total_size, IOIF_SPI_DEFAULT_TIMEOUT);

        _cs_high(instance);

        if (hal_status != HAL_OK) {
            return IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
        }

        /* RX에서 TX 구간 skip, 유효 데이터만 복사 */
        uint16_t rx_offset = (tx_buffer != NULL) ? tx_size : 0;
        memcpy((uint8_t*)rx_buffer, rx_combined + rx_offset, rx_size);

        return AGRBStatus_OK;
    }
    #endif
}

/**
 * @brief SPI Full-Duplex (blocking, DMA+completion wait)
 *
 * RTOS: DEV_LOCK → CS low → TransmitReceive_DMA → WAIT_RX → CS high → DEV_UNLOCK
 * NOTE: TxRxCplt ISR signals RX completion only (duplex completes when RX done)
 */
static AGRBStatusDef ioif_spi_duplex(IOIF_SPIx_t id, void* tx_buffer, void* rx_buffer, uint16_t size)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;
    if (tx_buffer == NULL || rx_buffer == NULL || size == 0) return AGRBStatus_ERROR;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];

    #if defined(USE_FREERTOS)
    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];

    if ( res->isr_mode.is_enabled ) return AGRBStatus_BUSY;
    if ( !SPI_DEV_LOCK(resource_index) ) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;
    HAL_StatusTypeDef hal_status = HAL_OK;

    do {
        res->current = id;

        if ( size > res->dma.tx->size || size > res->dma.rx->size ) {
            result = AGRBStatus_BUFFER_OVERFLOW;
            break;
        }

        uint8_t* dma_tx_buffer = res->dma.tx->buffer;
        uint8_t* dma_rx_buffer = res->dma.rx->buffer;

        memcpy( dma_tx_buffer, (uint8_t*)tx_buffer, size );
        memset( dma_rx_buffer, 0, size );

        _cs_low(instance);

        #if IOIF_HAS_DCACHE
        SCB_CleanDCache_by_Addr( (uint32_t*)dma_tx_buffer, size );
        #endif
        hal_status = HAL_SPI_TransmitReceive_DMA(
            res->hspi, dma_tx_buffer, dma_rx_buffer, size );
        if (hal_status != HAL_OK) {
            _cs_high(instance);
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
            break;
        }

        if ( !SPI_WAIT_RX(resource_index) ) {
            _cs_high(instance);
            result = AGRBStatus_TIMEOUT;
            break;
        }

        #if IOIF_HAS_DCACHE
        SCB_InvalidateDCache_by_Addr( (uint32_t*)dma_rx_buffer, size );
        #endif
        memcpy( (uint8_t*)rx_buffer, dma_rx_buffer, size );

        _cs_high(instance);

        res->current = IOIF_SPI_ID_NOT_ALLOCATED;
    } while(0);
    SPI_DEV_UNLOCK(resource_index);
    return result;
    #else
    {
        _cs_low(instance);

        HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive(instance->init.hspi, (uint8_t*)tx_buffer, (uint8_t*)rx_buffer, size, IOIF_SPI_DEFAULT_TIMEOUT);

        _cs_high(instance);

        return IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
    }
    #endif
}

/**
 * @brief SPI Reset — DeInit + Init
 *
 * DEV_LOCK → DeInit → Init → DEV_UNLOCK (에러 시에도 반드시 unlock)
 */
static AGRBStatusDef ioif_spi_reset(IOIF_SPIx_t id)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];

    uint32_t resource_index = instance->mutex_index;
    if ( !SPI_DEV_LOCK(resource_index) ) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;
    do {
        if (HAL_SPI_DeInit(instance->init.hspi) != HAL_OK) {
            result = AGRBStatus_ERROR;
            break;
        }
        if (HAL_SPI_Init(instance->init.hspi) != HAL_OK) {
            result = AGRBStatus_ERROR;
            break;
        }
        instance->error_count = 0;
    } while(0);

    SPI_DEV_UNLOCK(resource_index);
    return result;
}

/**
 * @brief ISR Mode 시작
 *
 * ISR 모드: write_isr() → DMA TransmitReceive → ISR enqueue → read_isr() dequeue
 * 주의: ISR 모드 중에는 write/read/duplex 사용 불가 (AGRBStatus_BUSY 반환)
 *
 * Device mutex를 영구 보유하지 않음 — is_enabled 플래그로 보호
 */
static AGRBStatusDef ioif_spi_start_isr_mode(IOIF_SPIx_t id, bool overwrite)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;
    IOIF_SPI_Instance_t* instance = &_spi_instances[id];
#if defined(USE_FREERTOS)
    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];

    if ( res->isr_mode.is_enabled ) return AGRBStatus_BUSY;
    if ( instance->isr_mode.isr_rx == NULL ) return AGRBStatus_ERROR;
    if ( res->hspi == NULL ) return AGRBStatus_ERROR;

    if ( !SPI_DEV_LOCK(resource_index) ) return AGRBStatus_TIMEOUT;

    res->current = id;
    res->isr_mode.is_enabled = true;
    res->isr_mode.enable_overwrite = overwrite;
    osMessageQueueReset( instance->isr_mode.isr_rx );

    SPI_DEV_UNLOCK(resource_index);

    return AGRBStatus_OK;
#else
    instance->isr_mode.is_enabled = true;
    instance->isr_mode.enable_overwrite = overwrite;
    instance->isr_mode.message.head = 0;
    instance->isr_mode.message.tail = 0;
    memset( instance->isr_mode.message.messages, 0, sizeof(instance->isr_mode.message.messages) );

    return AGRBStatus_OK;
#endif
}

/**
 * @brief ISR Mode 정지
 *
 * Semaphore release 불필요 (start에서 영구 보유하지 않으므로)
 */
static AGRBStatusDef ioif_spi_stop_isr_mode(IOIF_SPIx_t id)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;
    IOIF_SPI_Instance_t* instance = &_spi_instances[id];
#if defined(USE_FREERTOS)
    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];
    if ( !res->isr_mode.is_enabled ) return AGRBStatus_INVALID_STATE;

    if ( !SPI_DEV_LOCK(resource_index) ) return AGRBStatus_TIMEOUT;

    res->current = IOIF_SPI_ID_NOT_ALLOCATED;
    res->isr_mode.is_enabled = false;

    SPI_DEV_UNLOCK(resource_index);

    return AGRBStatus_OK;
#else
    instance->isr_mode.is_enabled = false;

    return AGRBStatus_OK;
#endif
}


static AGRBStatusDef ioif_spi_change_isr_queue_capacity(IOIF_SPIx_t id, size_t capacity)
{
#if defined(USE_FREERTOS)
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;
    IOIF_SPI_Instance_t* instance = &_spi_instances[id];

    if ( capacity == 0 || capacity > IOIF_SPI_ISR_MQ_MAX_CAPACITY ) return AGRBStatus_PARAM_ERROR;

    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];
    if ( res->isr_mode.is_enabled ) return AGRBStatus_INVALID_STATE;

    if ( instance->isr_mode.isr_rx != NULL ) {
        uint32_t current_capacity = osMessageQueueGetCapacity( instance->isr_mode.isr_rx );
        if ( current_capacity == capacity ) return AGRBStatus_OK;

        osMessageQueueDelete( instance->isr_mode.isr_rx );
    }

    instance->isr_mode.isr_rx = osMessageQueueNew(
        capacity,
        sizeof(SPI_ISR_Message_t),
        NULL );

    if ( instance->isr_mode.isr_rx == NULL ) return AGRBStatus_ERROR;

    return AGRBStatus_OK;
#else
    (void)id;
    (void)capacity;
    return AGRBStatus_NOT_SUPPORTED;
#endif
}


static inline AGRBStatusDef ioif_spi_write_isr(IOIF_SPIx_t id, void* tx_buffer, uint16_t total_size)
{
    if (id >= _spi_instances_count) return AGRBStatus_NOT_FOUND;
    if (tx_buffer == NULL || total_size == 0) return AGRBStatus_PARAM_ERROR;
    if ( total_size > IOIF_SPI_ISR_MQ_UNIT_SIZE ) return AGRBStatus_PARAM_ERROR;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];

    #if defined(USE_FREERTOS)
    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];
    if ( !res->isr_mode.is_enabled ) return AGRBStatus_INVALID_STATE;

    uint8_t* tx_dma = res->dma.tx->buffer;
    uint8_t* rx_dma = res->dma.rx->buffer;
    #else
    uint8_t* tx_dma = instance->isr_mode.dma.tx->buffer;
    uint8_t* rx_dma = instance->isr_mode.dma.rx->buffer;
    #endif

    memset( tx_dma, 0xFF, sizeof(SPI_ISR_Message_t) );
    memcpy( tx_dma, (uint8_t*)tx_buffer, total_size );
    memset( rx_dma, 0, sizeof(SPI_ISR_Message_t) );

    _cs_low(instance);

    #if IOIF_HAS_DCACHE
    SCB_CleanDCache_by_Addr( (uint32_t*)tx_dma, total_size );
    #endif
    HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive_DMA(
        instance->init.hspi,
        tx_dma,
        rx_dma,
        total_size );

    return IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
}

static AGRBStatusDef ioif_spi_read_isr(IOIF_SPIx_t id, void* rx_buffer, uint16_t size)
{
    if (id >= _spi_instances_count) return AGRBStatus_NOT_FOUND;
    if (rx_buffer == NULL || size == 0) return AGRBStatus_PARAM_ERROR;
    if ( size > IOIF_SPI_ISR_MQ_UNIT_SIZE ) return AGRBStatus_PARAM_ERROR;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];
    memset( rx_buffer, 0, size );

#if defined(USE_FREERTOS)
    SPI_ISR_Message_t msg;
    osStatus_t os_status = osMessageQueueGet(
        instance->isr_mode.isr_rx,
        &msg,
        NULL,
        0 );
    if ( os_status != osOK ) return AGRBStatus_FAILED;

    memcpy( (uint8_t*)rx_buffer, msg.data, size );
    return AGRBStatus_OK;
#else
    if ( !instance->isr_mode.is_enabled ) return AGRBStatus_INVALID_STATE;
    if ( instance->isr_mode.message.head == instance->isr_mode.message.tail )  return AGRBStatus_EMPTY;
    SPI_ISR_Message_t* msg = &instance->isr_mode.message.messages[instance->isr_mode.message.tail];
    memcpy( (uint8_t*)rx_buffer, msg->data, size );
    instance->isr_mode.message.tail = (instance->isr_mode.message.tail + 1) & STATIC_ISR_MESSAGE_QUEUE_MASK;

    return AGRBStatus_OK;
#endif
}

static AGRBStatusDef ioif_spi_set_ss_pin(IOIF_SPIx_t id, IOIF_GPIOx_t ss_pin)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];
    instance->init.ss = ss_pin;

    return AGRBStatus_OK;
}
static IOIF_GPIOx_t ioif_spi_get_ss_pin(IOIF_SPIx_t id)
{
    if (id >= _spi_instances_count) return IOIF_GPIO_ID_NOT_ALLOCATED;

    IOIF_SPI_Instance_t* instance = &_spi_instances[id];
    return instance->init.ss;
}

/**
 * @brief SPI Mode Update (CPOL/CPHA 변경)
 *
 * DEV_LOCK → Init → DEV_UNLOCK (에러 시에도 unlock)
 */
static AGRBStatusDef ioif_spi_mode_update(IOIF_SPIx_t id, bool cpol, bool cpha)
{
    if (id >= _spi_instances_count) return AGRBStatus_ERROR;
    IOIF_SPI_Instance_t* instance = &_spi_instances[id];

    if (instance->init.hspi == NULL) return AGRBStatus_ERROR;

    uint32_t resource_index = instance->mutex_index;
    if ( !SPI_DEV_LOCK(resource_index) ) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;
    do {
        instance->init.hspi->Init.CLKPolarity  = (cpol) ? SPI_POLARITY_HIGH : SPI_POLARITY_LOW;
        instance->init.hspi->Init.CLKPhase     = (cpha) ? SPI_PHASE_2EDGE : SPI_PHASE_1EDGE;

        if (HAL_SPI_Init(instance->init.hspi) != HAL_OK) {
            result = AGRBStatus_ERROR;
            break;
        }
    } while(0);

    SPI_DEV_UNLOCK(resource_index);
    return result;
}
/************** inline functions ************/

static inline void _cs_low(IOIF_SPI_Instance_t* instance)
{
    ioif_gpio_reset(instance->init.ss);
}

static inline void _cs_high(IOIF_SPI_Instance_t* instance)
{
    ioif_gpio_set(instance->init.ss);
}

static inline IOIF_SPI_Instance_t* _get_instance(SPI_HandleTypeDef* hspi)
{
    if ( hspi == NULL ) return NULL;

#if defined(USE_FREERTOS)
    for (uint32_t i = 0; i < _spi_resource_count; i++) {
        if (_spi_resource_pool[i].hspi != hspi) continue;

        IOIF_SPIx_t current_id = _spi_resource_pool[i].current;
        if ( current_id != IOIF_SPI_ID_NOT_ALLOCATED &&
                current_id < _spi_instances_count ) {
            return &_spi_instances[current_id];
        }
    }
#else
    for (IOIF_SPIx_t i = 0; i < _spi_instances_count; i++) {
        if (_spi_instances[i].init.hspi == hspi) {
            if (_spi_instances[i].isr_mode.is_enabled) {
                return &_spi_instances[i];
            }
        }
    }
#endif

    return NULL;
}

#if defined(USE_FREERTOS)
static uint32_t _get_resource_index_by_hspi(SPI_HandleTypeDef* hspi)
{
    for (uint32_t i = 0; i < _spi_resource_count; i++) {
        if (_spi_resource_pool[i].hspi == hspi) return i;
    }
    return IOIF_SPI_ID_NOT_ALLOCATED;
}
#endif

/***************************
 * ISR Callbacks
 *
 * 핵심 원칙:
 * - ISR에서 device mutex 터치 금지
 * - TX 완료 → SIGNAL_TX_ISR
 * - RX 완료 → SIGNAL_RX_ISR
 * - TxRx 완료 → ISR 모드 ? enqueue : SIGNAL_RX_ISR
 * - Error → error_count++ + SIGNAL_TX + SIGNAL_RX
 ****************************/

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
#if defined(USE_FREERTOS)
    uint32_t resource_index = _get_resource_index_by_hspi(hspi);
    if (resource_index == IOIF_SPI_ID_NOT_ALLOCATED) return;

    SPI_SIGNAL_TX_ISR(resource_index);
#else
    UNUSED(hspi);
#endif
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi)
{
    IOIF_SPI_Instance_t* instance = _get_instance(hspi);
    if (instance == NULL) {
        /* Non-ISR mode: instance not tracked → try signaling RX directly */
        #if defined(USE_FREERTOS)
        uint32_t resource_index = _get_resource_index_by_hspi(hspi);
        if (resource_index != IOIF_SPI_ID_NOT_ALLOCATED) {
            SPI_SIGNAL_RX_ISR(resource_index);
        }
        #endif
        return;
    }

#if defined(USE_FREERTOS)
    _cs_high(instance);
    uint32_t resource_index = instance->mutex_index;
    IOIF_SPI_Resource_t* res = &_spi_resource_pool[resource_index];

    if ( !res->isr_mode.is_enabled ) {
        SPI_SIGNAL_RX_ISR(resource_index);
        return;
    }

    SPI_ISR_Message_t msg;
    uint8_t* rx_dma_buffer = res->dma.rx->buffer;
    if ( res->isr_mode.enable_overwrite ) {
        if ( osMessageQueueGetCount( instance->isr_mode.isr_rx ) >= osMessageQueueGetCapacity( instance->isr_mode.isr_rx ) ) {
            osMessageQueueGet( instance->isr_mode.isr_rx, &msg,  NULL, 0 );
        }
    }
    #if IOIF_HAS_DCACHE
    SCB_InvalidateDCache_by_Addr( (uint32_t*)rx_dma_buffer, IOIF_SPI_ISR_MQ_UNIT_SIZE );
    #endif
    memcpy( msg.data, rx_dma_buffer, IOIF_SPI_ISR_MQ_UNIT_SIZE );

    osMessageQueuePut( instance->isr_mode.isr_rx, &msg, 0, 0 );
#else
    if ( !instance->isr_mode.is_enabled ) return;

    _cs_high(instance);
    SPI_ISR_Message_t* msg = &instance->isr_mode.message.messages[instance->isr_mode.message.head];
    uint8_t* rx_dma_buffer = instance->isr_mode.dma.rx->buffer;
    #if IOIF_HAS_DCACHE
    SCB_InvalidateDCache_by_Addr( (uint32_t*)rx_dma_buffer, IOIF_SPI_ISR_MQ_UNIT_SIZE );
    #endif
    memcpy( msg->data, rx_dma_buffer, IOIF_SPI_ISR_MQ_UNIT_SIZE );

    instance->isr_mode.message.head = (instance->isr_mode.message.head + 1) & STATIC_ISR_MESSAGE_QUEUE_MASK;
    /* BareMetal: enable_overwrite 존중 */
    if ( instance->isr_mode.message.head == instance->isr_mode.message.tail ) {
        if ( instance->isr_mode.enable_overwrite ) {
            instance->isr_mode.message.tail = (instance->isr_mode.message.tail + 1) & STATIC_ISR_MESSAGE_QUEUE_MASK;
        } else {
            /* Overwrite 비활성 시 head 롤백 (데이터 드롭) */
            instance->isr_mode.message.head = (instance->isr_mode.message.head - 1) & STATIC_ISR_MESSAGE_QUEUE_MASK;
        }
    }

#endif
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi)
{
#if defined(USE_FREERTOS)
    uint32_t resource_index = _get_resource_index_by_hspi(hspi);
    if (resource_index == IOIF_SPI_ID_NOT_ALLOCATED) return;

    SPI_SIGNAL_RX_ISR(resource_index);
#else
    UNUSED(hspi);
#endif
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
#if defined(USE_FREERTOS)
    IOIF_SPI_Instance_t* instance = _get_instance(hspi);
    if (instance != NULL) {
        instance->error_count++;
        _cs_high(instance);
    }

    uint32_t resource_index = _get_resource_index_by_hspi(hspi);
    if (resource_index == IOIF_SPI_ID_NOT_ALLOCATED) return;

    /* Device mutex는 터치 안 함 — 대기 중인 Task를 깨워서 에러 경로로 진입 */
    SPI_SIGNAL_TX_ISR(resource_index);
    SPI_SIGNAL_RX_ISR(resource_index);
#else
    UNUSED(hspi);
#endif
}


#endif /* AGRB_IOIF_SPI_ENABLE */
