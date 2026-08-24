/**
 ******************************************************************************
 * @file    ioif_agrb_i2c.c
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] I2C 하드웨어 추상화 계층 구현부
 * @version 4.0 (Production Quality - Deadlock-Free Redesign)
 * @date    Mar 22, 2026
 *
 * @details
 * - Handle-based API: ioif_i2c.mem_write(), ioif_i2c.mem_read() 등
 * - RTOS: DMA + Mutex(Priority Inheritance) + Completion Semaphore (TX DMA optional → Polling fallback)
 * - BareMetal: Polling 기반
 * - D-Cache: #if IOIF_HAS_DCACHE 가드
 *
 * @note Semaphore Symmetry: DEV_LOCK/DEV_UNLOCK은 항상 같은 함수 내에서 대칭.
 *       ISR에서는 SIGNAL_TX/RX_ISR만 호출, DEV_UNLOCK 금지.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_i2c.h"

#if defined(AGRB_IOIF_I2C_ENABLE)
#include <string.h>

#if defined(AGRB_IOIF_DMA_ENABLE)
#include "ioif_agrb_dma.h"
#endif

typedef struct {
    IOIF_I2C_Initialize_t init;

    #if defined(USE_FREERTOS)
    struct {
        SemaphoreHandle_t device;   // xSemaphoreCreateMutex() — Priority Inheritance
        SemaphoreHandle_t tx_done;  // Binary — TX DMA 완료 신호
        SemaphoreHandle_t rx_done;  // Binary — RX DMA 완료 신호
    } semaphores;

    struct {
        IOIF_DMAx_t* tx;
        IOIF_DMAx_t* rx;
    } dma;
    #endif

    volatile uint32_t error_count;
    volatile uint32_t last_hal_error;  // HAL_I2C_ERROR_xxx 비트마스크 (ErrorCallback에서 저장)

    IOIF_I2Cx_t id; //Instance ID, back reference

} IOIF_I2C_Instance_t;

/* ===================================================================
 * Semaphore 매크로 (FDCAN TX_LOCK 패턴)
 * - I2C_DEV_LOCK: bool 반환 (호출자가 에러 처리)
 * - I2C_DEV_UNLOCK: void (항상 성공)
 * - I2C_WAIT_TX/RX: bool 반환 (timeout 시 false)
 * - I2C_SIGNAL_TX/RX_ISR: ISR 전용
 * =================================================================== */
#if defined(USE_FREERTOS)

    #define I2C_DEV_LOCK(inst)   ((inst)->semaphores.device != NULL && \
                                  xSemaphoreTake((inst)->semaphores.device, \
                                  pdMS_TO_TICKS((inst)->init.timeout)) == pdTRUE)

    #define I2C_DEV_UNLOCK(inst) do { \
        if ((inst)->semaphores.device != NULL) { \
            xSemaphoreGive((inst)->semaphores.device); \
        } \
    } while(0)

    #define I2C_WAIT_TX(inst)    ((inst)->semaphores.tx_done != NULL && \
                                  xSemaphoreTake((inst)->semaphores.tx_done, \
                                  pdMS_TO_TICKS((inst)->init.timeout)) == pdTRUE)

    #define I2C_WAIT_RX(inst)    ((inst)->semaphores.rx_done != NULL && \
                                  xSemaphoreTake((inst)->semaphores.rx_done, \
                                  pdMS_TO_TICKS((inst)->init.timeout)) == pdTRUE)

    #define I2C_SIGNAL_TX_ISR(inst) do { \
        if ((inst)->semaphores.tx_done != NULL) { \
            BaseType_t xWoken = pdFALSE; \
            xSemaphoreGiveFromISR((inst)->semaphores.tx_done, &xWoken); \
            portYIELD_FROM_ISR(xWoken); \
        } \
    } while(0)

    #define I2C_SIGNAL_RX_ISR(inst) do { \
        if ((inst)->semaphores.rx_done != NULL) { \
            BaseType_t xWoken = pdFALSE; \
            xSemaphoreGiveFromISR((inst)->semaphores.rx_done, &xWoken); \
            portYIELD_FROM_ISR(xWoken); \
        } \
    } while(0)

#else
    /* BareMetal: 모든 매크로 no-op */
    #define I2C_DEV_LOCK(inst)       (true)
    #define I2C_DEV_UNLOCK(inst)     ((void)0)
    #define I2C_WAIT_TX(inst)        (true)
    #define I2C_WAIT_RX(inst)        (true)
    #define I2C_SIGNAL_TX_ISR(inst)  ((void)0)
    #define I2C_SIGNAL_RX_ISR(inst)  ((void)0)
#endif

/******************************************************************************************************************************/
static AGRBStatusDef ioif_i2c_assign_instance(IOIF_I2Cx_t* id, const IOIF_I2C_Initialize_t* init);
static AGRBStatusDef ioif_i2c_reset(IOIF_I2Cx_t id);
static AGRBStatusDef ioif_i2c_master_transmit(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size);
static AGRBStatusDef ioif_i2c_master_receive(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size);
static AGRBStatusDef ioif_i2c_mem_write(IOIF_I2Cx_t id, uint16_t dev_address, uint16_t mem_address, IOIF_I2C_MemAddrSize_e mem_address_size, void* data, size_t size);
static AGRBStatusDef ioif_i2c_mem_read(IOIF_I2Cx_t id, uint16_t dev_address, uint16_t mem_address, IOIF_I2C_MemAddrSize_e mem_address_size, void* data, size_t size);
static AGRBStatusDef ioif_i2c_raw_transmit(IOIF_I2Cx_t id, uint16_t dev_address, const void* data, size_t size);
static AGRBStatusDef ioif_i2c_raw_receive(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size);
static size_t ioif_i2c_get_tx_buffer_size(IOIF_I2Cx_t id);
static uint32_t ioif_i2c_get_last_error(IOIF_I2Cx_t id);
/******************************************************************************************************************************/

static IOIF_I2C_Instance_t* _ioif_agrb_i2c_get_instance(IOIF_I2Cx_t id);
static IOIF_I2C_Instance_t* _ioif_agrb_i2c_get_instance_by_hi2c(I2C_HandleTypeDef* hi2c);

static IOIF_I2C_Instance_t _i2c_instances[IOIF_I2C_MAX_INSTANCES] = {0, };
static uint32_t _i2c_instance_count = 0;

IOIF_I2C_Handle_t ioif_i2c = {
    .assign = ioif_i2c_assign_instance,
    .reset = ioif_i2c_reset,
    .master_transmit = ioif_i2c_master_transmit,
    .master_receive = ioif_i2c_master_receive,
    .mem_write = ioif_i2c_mem_write,
    .mem_read = ioif_i2c_mem_read,
    .raw_transmit = ioif_i2c_raw_transmit,
    .raw_receive = ioif_i2c_raw_receive,
    .get_tx_buffer_size = ioif_i2c_get_tx_buffer_size,
    .get_last_error = ioif_i2c_get_last_error
};

///////////////////////////////////////////////////////////////////////

static AGRBStatusDef ioif_i2c_assign_instance(IOIF_I2Cx_t* id, const IOIF_I2C_Initialize_t* init)
{
    if (id == NULL || init == NULL) return AGRBStatus_PARAM_ERROR;
    if ( init->hi2c == NULL ) return AGRBStatus_PARAM_ERROR;

    IOIF_I2Cx_t index = _i2c_instance_count;

    if ( _ioif_agrb_i2c_get_instance_by_hi2c(init->hi2c) != NULL ) return AGRBStatus_ALREADY_INITIALIZED;

    if (index >= IOIF_I2C_MAX_INSTANCES) return AGRBStatus_INSTANCE_FULL;

    IOIF_I2C_Instance_t* instance = &_i2c_instances[index];

    memset(instance, 0, sizeof(IOIF_I2C_Instance_t));

    instance->init.timeout = IOIF_I2C_DEFAULT_TIMEOUT;
    if (init != NULL) memcpy(&(instance->init), init, sizeof(IOIF_I2C_Initialize_t));

    #if defined(USE_FREERTOS)

    char i2c_tx_name[IOIF_DMA_NAME_SIZE] = "i2c_tx_";
    char i2c_rx_name[IOIF_DMA_NAME_SIZE] = "i2c_rx_";

    /* TX DMA: optional — hdmatx == NULL이면 Polling fallback (DMA Stream 절약) */
    instance->dma.tx = NULL;
    if (instance->init.hi2c->hdmatx != NULL && instance->init.dma.tx_size > 0) {
        i2c_tx_name[7] = '0' + (index % 10);
        i2c_tx_name[8] = '\0';
        instance->dma.tx = ioif_dma.allocate( instance->init.hi2c->hdmatx, instance->init.dma.tx_size, i2c_tx_name );
        if (instance->dma.tx == NULL) {
            memset(instance, 0, sizeof(IOIF_I2C_Instance_t));
            return AGRBStatus_DMA_ALLOCATION_ERROR;
        }
    }

    /* RX DMA: 항상 필수 */
    i2c_rx_name[7] = '0' + (index % 10);
    i2c_rx_name[8] = '\0';
    instance->dma.rx = ioif_dma.allocate( instance->init.hi2c->hdmarx, instance->init.dma.rx_size, i2c_rx_name );
    if (instance->dma.rx == NULL) {
        memset(instance, 0, sizeof(IOIF_I2C_Instance_t));
        return AGRBStatus_DMA_ALLOCATION_ERROR;
    }

    /* Mutex (Priority Inheritance) for device access */
    instance->semaphores.device = xSemaphoreCreateMutex();
    /* Binary semaphores for DMA completion signals */
    instance->semaphores.tx_done = xSemaphoreCreateBinary();
    instance->semaphores.rx_done = xSemaphoreCreateBinary();

    if (instance->semaphores.device == NULL ||
        instance->semaphores.tx_done == NULL ||
        instance->semaphores.rx_done == NULL) {
        /* Rollback: 이미 생성된 semaphore 정리 */
        if (instance->semaphores.device != NULL) vSemaphoreDelete(instance->semaphores.device);
        if (instance->semaphores.tx_done != NULL) vSemaphoreDelete(instance->semaphores.tx_done);
        if (instance->semaphores.rx_done != NULL) vSemaphoreDelete(instance->semaphores.rx_done);
        memset(instance, 0, sizeof(IOIF_I2C_Instance_t));
        return AGRBStatus_SEMAPHORE_ERROR;
    }

    /* Mutex는 생성 시 이미 available (xSemaphoreGive 불필요) */
    /* Binary semaphore는 생성 시 empty 상태 (ISR이 Give하기를 대기) */

    #endif

    instance->id = index;

    *id = index;

    _i2c_instance_count++;

    return AGRBStatus_OK;
}

/**
 * @brief I2C 버스 리셋 (GPIO bit-bang 버스 복구 포함)
 *
 * [복구 시퀀스] (pin_map이 설정된 경우)
 *   1. HAL_I2C_DeInit — 페리페럴 비활성화
 *   2. SCL/SDA를 GPIO Open-Drain으로 전환
 *   3. SDA Stuck Low 감지 → SCL 최대 18회 토글 (슬레이브 릴리즈)
 *   4. STOP 조건 생성 (SDA Low→High while SCL High)
 *   5. GPIO를 I2C Alternate Function으로 복원
 *   6. HAL_I2C_Init — 페리페럴 재초기화
 *
 * [Fallback] pin_map.sda.port == NULL → HAL DeInit/Init만 수행 (기존 동작)
 */
static AGRBStatusDef ioif_i2c_reset(IOIF_I2Cx_t id)
{
    IOIF_I2C_Instance_t* instance = _ioif_agrb_i2c_get_instance(id);
    if (instance == NULL) return AGRBStatus_PARAM_ERROR;

    if (!I2C_DEV_LOCK(instance)) return AGRBStatus_TIMEOUT;

    /* Step 1: 페리페럴 비활성화 */
    HAL_I2C_DeInit(instance->init.hi2c);

    /* Step 2: GPIO bit-bang 버스 복구 (pin_map이 설정된 경우에만) */
    const IOIF_I2C_PinMap_t* pm = &instance->init.pin_map;
    if (pm->sda.port != NULL && pm->scl.port != NULL) {

        /* 2a. SCL/SDA를 GPIO Open-Drain 출력으로 전환 */
        GPIO_InitTypeDef gpio_cfg = {0};
        gpio_cfg.Mode  = GPIO_MODE_OUTPUT_OD;
        gpio_cfg.Pull  = GPIO_NOPULL;
        gpio_cfg.Speed = GPIO_SPEED_FREQ_LOW;

        gpio_cfg.Pin = pm->sda.pin;
        HAL_GPIO_Init(pm->sda.port, &gpio_cfg);

        gpio_cfg.Pin = pm->scl.pin;
        HAL_GPIO_Init(pm->scl.port, &gpio_cfg);

        /* 2b. SCL/SDA 초기 상태 High */
        HAL_GPIO_WritePin(pm->scl.port, pm->scl.pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(pm->sda.port, pm->sda.pin, GPIO_PIN_SET);

        /* 2c. SDA Stuck Low → SCL 토글로 슬레이브 릴리즈 */
        for (uint32_t i = 0; i < IOIF_I2C_BUS_RECOVERY_SCL_TOGGLES; i++) {
            /* SDA가 High로 복구되었으면 조기 종료 */
            if (HAL_GPIO_ReadPin(pm->sda.port, pm->sda.pin) == GPIO_PIN_SET) {
                break;
            }
            /* SCL 토글: High→Low→High */
            HAL_GPIO_WritePin(pm->scl.port, pm->scl.pin, GPIO_PIN_RESET);
            for (volatile uint32_t d = 0; d < IOIF_I2C_BUS_RECOVERY_DELAY_LOOPS; d++) {}
            HAL_GPIO_WritePin(pm->scl.port, pm->scl.pin, GPIO_PIN_SET);
            for (volatile uint32_t d = 0; d < IOIF_I2C_BUS_RECOVERY_DELAY_LOOPS; d++) {}
        }

        /* 2d. STOP 조건 생성: SDA Low → SCL High → SDA High */
        HAL_GPIO_WritePin(pm->sda.port, pm->sda.pin, GPIO_PIN_RESET);
        for (volatile uint32_t d = 0; d < IOIF_I2C_BUS_RECOVERY_DELAY_LOOPS; d++) {}
        HAL_GPIO_WritePin(pm->scl.port, pm->scl.pin, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < IOIF_I2C_BUS_RECOVERY_DELAY_LOOPS; d++) {}
        HAL_GPIO_WritePin(pm->sda.port, pm->sda.pin, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < IOIF_I2C_BUS_RECOVERY_DELAY_LOOPS; d++) {}

        /* 2e. GPIO를 I2C Alternate Function으로 복원 */
        gpio_cfg.Mode      = GPIO_MODE_AF_OD;
        gpio_cfg.Pull      = GPIO_PULLUP;
        gpio_cfg.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio_cfg.Alternate = pm->alternate_function;

        gpio_cfg.Pin = pm->sda.pin;
        HAL_GPIO_Init(pm->sda.port, &gpio_cfg);

        gpio_cfg.Pin = pm->scl.pin;
        HAL_GPIO_Init(pm->scl.port, &gpio_cfg);
    }

    /* Step 3: 페리페럴 재초기화 */
    HAL_I2C_Init(instance->init.hi2c);

    instance->error_count = 0;
    instance->last_hal_error = 0;

    I2C_DEV_UNLOCK(instance);

    return AGRBStatus_OK;
}

/**
 * @brief I2C Master Transmit (blocking, DMA+completion wait)
 *
 * RTOS: DEV_LOCK → DMA start → WAIT_TX → DEV_UNLOCK
 * BareMetal: Polling HAL_I2C_Master_Transmit
 */
static AGRBStatusDef ioif_i2c_master_transmit(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size)
{
    if (id >= _i2c_instance_count) return AGRBStatus_PARAM_ERROR;
    if (data == NULL || size == 0) return AGRBStatus_PARAM_ERROR;
    if (dev_address > 0x77) return AGRBStatus_PARAM_ERROR;  /* 7-bit address only */

    IOIF_I2C_Instance_t* instance = &_i2c_instances[id];
    uint16_t dev_addr = dev_address << 1;

    if (!I2C_DEV_LOCK(instance)) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;

    #if defined(USE_FREERTOS)
    do {
        if (instance->dma.tx != NULL) {
            /* ── DMA 경로 ── */
            if ( instance->dma.tx->size < size ) {
                result = AGRBStatus_BUFFER_OVERFLOW;
                break;
            }
            uint8_t* data_buffer = instance->dma.tx->buffer;
            memcpy( data_buffer, data, size );
            #if IOIF_HAS_DCACHE
            SCB_CleanDCache_by_Addr( (uint32_t*)data_buffer, size );
            #endif

            HAL_StatusTypeDef hal_res = HAL_I2C_Master_Transmit_DMA(
                instance->init.hi2c, dev_addr, data_buffer, size );

            if ( hal_res != HAL_OK ) {
                result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
                break;
            }

            if ( !I2C_WAIT_TX(instance) ) {
                HAL_I2C_Master_Abort_IT(instance->init.hi2c, dev_addr);
                result = AGRBStatus_TIMEOUT;
                break;
            }
        } else {
            /* ── Polling fallback (hdmatx == NULL) ── */
            HAL_StatusTypeDef hal_res = HAL_I2C_Master_Transmit( instance->init.hi2c,
                dev_addr, (uint8_t*)data, size, instance->init.timeout );
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
        }
    } while(0);
    #else
    do {
        HAL_StatusTypeDef hal_res = HAL_I2C_Master_Transmit( instance->init.hi2c,
            dev_addr, (uint8_t*)data, size, instance->init.timeout );
        result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
    } while(0);
    #endif

    I2C_DEV_UNLOCK(instance);
    return result;
}

/**
 * @brief I2C Master Receive (blocking, DMA+completion wait)
 *
 * RTOS: DEV_LOCK → DMA start → WAIT_RX → copy → DEV_UNLOCK
 * BareMetal: Polling HAL_I2C_Master_Receive
 */
static AGRBStatusDef ioif_i2c_master_receive(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size)
{
    if (id >= _i2c_instance_count) return AGRBStatus_PARAM_ERROR;
    if (data == NULL || size == 0) return AGRBStatus_PARAM_ERROR;
    if (dev_address > 0x77) return AGRBStatus_PARAM_ERROR;  /* 7-bit address only */

    IOIF_I2C_Instance_t* instance = &_i2c_instances[id];
    uint16_t dev_addr = dev_address << 1;

    if (!I2C_DEV_LOCK(instance)) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;

    #if defined(USE_FREERTOS)
    do {
        if ( instance->dma.rx->size < size ) {
            result = AGRBStatus_BUFFER_OVERFLOW;
            break;
        }
        uint8_t* rx_buffer = instance->dma.rx->buffer;
        memset( rx_buffer, 0, instance->dma.rx->size );

        HAL_StatusTypeDef hal_res = HAL_I2C_Master_Receive_DMA(
            instance->init.hi2c, dev_addr, rx_buffer, size );

        if ( hal_res != HAL_OK ) {
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
            break;
        }

        if ( !I2C_WAIT_RX(instance) ) {
            HAL_I2C_Master_Abort_IT(instance->init.hi2c, dev_addr);
            result = AGRBStatus_TIMEOUT;
            break;
        }

        #if IOIF_HAS_DCACHE
        SCB_InvalidateDCache_by_Addr( (uint32_t*)rx_buffer, size );
        #endif
        memcpy( data, rx_buffer, size );
        result = AGRBStatus_OK;
    } while(0);
    #else
    do {
        HAL_StatusTypeDef hal_res = HAL_I2C_Master_Receive( instance->init.hi2c,
            dev_addr, (uint8_t*)data, size, instance->init.timeout );
        result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
    } while(0);
    #endif

    I2C_DEV_UNLOCK(instance);
    return result;
}

/**
 * @brief I2C Memory Write (blocking, DMA+completion wait)
 *
 * RTOS: DEV_LOCK → DMA start → WAIT_TX → DEV_UNLOCK
 * BareMetal: Polling HAL_I2C_Mem_Write
 */
static AGRBStatusDef ioif_i2c_mem_write(IOIF_I2Cx_t id, uint16_t dev_address, uint16_t mem_address, IOIF_I2C_MemAddrSize_e mem_address_size, void* data, size_t size)
{
    if (id >= _i2c_instance_count) return AGRBStatus_PARAM_ERROR;
    if (data == NULL || size == 0) return AGRBStatus_PARAM_ERROR;
    if (mem_address_size != IOIF_I2C_MemAddrSize_8BIT && mem_address_size != IOIF_I2C_MemAddrSize_16BIT) return AGRBStatus_PARAM_ERROR;
    if (dev_address > 0x77) return AGRBStatus_PARAM_ERROR;  /* 7-bit address only */

    IOIF_I2C_Instance_t* instance = &_i2c_instances[id];
    uint16_t dev_addr = dev_address << 1;

    if (!I2C_DEV_LOCK(instance)) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;

    #if defined(USE_FREERTOS)
    do {
        if (instance->dma.tx != NULL) {
            /* ── DMA 경로 ── */
            if ( instance->dma.tx->size < size ) {
                result = AGRBStatus_BUFFER_OVERFLOW;
                break;
            }
            uint8_t* tx_buffer = instance->dma.tx->buffer;
            memcpy( tx_buffer, data, size );
            #if IOIF_HAS_DCACHE
            SCB_CleanDCache_by_Addr( (uint32_t*)tx_buffer, size );
            #endif

            HAL_StatusTypeDef hal_res = HAL_I2C_Mem_Write_DMA( instance->init.hi2c,
                dev_addr, mem_address, mem_address_size, tx_buffer, size );

            if ( hal_res != HAL_OK ) {
                result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
                break;
            }

            if ( !I2C_WAIT_TX(instance) ) {
                HAL_I2C_Master_Abort_IT(instance->init.hi2c, dev_addr);
                result = AGRBStatus_TIMEOUT;
                break;
            }
        } else {
            /* ── Polling fallback (hdmatx == NULL) ── */
            HAL_StatusTypeDef hal_res = HAL_I2C_Mem_Write( instance->init.hi2c,
                dev_addr, mem_address, mem_address_size,
                (uint8_t*)data, size, instance->init.timeout );
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
        }
    } while(0);
    #else
    do {
        HAL_StatusTypeDef hal_res = HAL_I2C_Mem_Write( instance->init.hi2c,
            dev_addr, mem_address, mem_address_size,
            (uint8_t*)data, size, instance->init.timeout );
        result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
    } while(0);
    #endif

    I2C_DEV_UNLOCK(instance);
    return result;
}

/**
 * @brief I2C Memory Read (blocking, DMA+completion wait)
 *
 * RTOS: DEV_LOCK → DMA start → WAIT_RX → copy → DEV_UNLOCK
 * BareMetal: Polling HAL_I2C_Mem_Read
 */
static AGRBStatusDef ioif_i2c_mem_read(IOIF_I2Cx_t id, uint16_t dev_address, uint16_t mem_address, IOIF_I2C_MemAddrSize_e mem_address_size, void* data, size_t size)
{
    if (data == NULL || size == 0) return AGRBStatus_PARAM_ERROR;
    if (mem_address_size != IOIF_I2C_MemAddrSize_8BIT && mem_address_size != IOIF_I2C_MemAddrSize_16BIT) return AGRBStatus_PARAM_ERROR;
    if (id >= _i2c_instance_count) return AGRBStatus_PARAM_ERROR;
    if (dev_address > 0x77) return AGRBStatus_PARAM_ERROR;  /* 7-bit address only */

    IOIF_I2C_Instance_t* instance = &_i2c_instances[id];
    uint16_t dev_addr = dev_address << 1;

    if (!I2C_DEV_LOCK(instance)) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;

    #if defined(USE_FREERTOS)
    do {
        if ( instance->dma.rx->size < size ) {
            result = AGRBStatus_BUFFER_OVERFLOW;
            break;
        }

        uint8_t* rx_buffer = instance->dma.rx->buffer;

        HAL_StatusTypeDef hal_res = HAL_I2C_Mem_Read_DMA( instance->init.hi2c,
            dev_addr, mem_address, mem_address_size, rx_buffer, size );

        if ( hal_res != HAL_OK ) {
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
            break;
        }

        if ( !I2C_WAIT_RX(instance) ) {
            HAL_I2C_Master_Abort_IT(instance->init.hi2c, dev_addr);
            result = AGRBStatus_TIMEOUT;
            break;
        }

        #if IOIF_HAS_DCACHE
        SCB_InvalidateDCache_by_Addr( (uint32_t*)rx_buffer, size );
        #endif
        memcpy( data, rx_buffer, size );
        result = AGRBStatus_OK;
    } while(0);
    #else
    do {
        HAL_StatusTypeDef hal_res = HAL_I2C_Mem_Read( instance->init.hi2c,
            dev_addr, mem_address, mem_address_size,
            (uint8_t*)data, size, instance->init.timeout );
        result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
    } while(0);
    #endif

    I2C_DEV_UNLOCK(instance);
    return result;
}


static size_t ioif_i2c_get_tx_buffer_size(IOIF_I2Cx_t id)
{
    if (id >= _i2c_instance_count) return 0;
    IOIF_I2C_Instance_t* instance = &_i2c_instances[id];
    #if defined(USE_FREERTOS)
    if (instance->dma.tx != NULL) {
        return instance->dma.tx->size;
    }
    return 0;  /* Polling 모드 — DMA 버퍼 없음 */
    #else
    (void)instance;
    return 0;
    #endif
}

/**
 * @brief I2C Raw Transmit — DMA 버퍼를 거치지 않고 원본 데이터 직접 전송
 *
 * RTOS: DEV_LOCK → DMA start → WAIT_TX → DEV_UNLOCK
 * BareMetal: Polling
 *
 * @warning data 포인터가 DMA-accessible 메모리에 있어야 함 (D1/D2 SRAM, non-cacheable)
 */
static AGRBStatusDef ioif_i2c_raw_transmit(IOIF_I2Cx_t id, uint16_t dev_address, const void* data, size_t size)
{
    if (id >= _i2c_instance_count) return AGRBStatus_PARAM_ERROR;
    if (data == NULL || size == 0) return AGRBStatus_PARAM_ERROR;
    if (dev_address > 0x77) return AGRBStatus_PARAM_ERROR;  /* 7-bit address only */

    IOIF_I2C_Instance_t* instance = &_i2c_instances[id];
    uint16_t dev_addr = dev_address << 1;

    if (!I2C_DEV_LOCK(instance)) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;

    #if defined(USE_FREERTOS)
    do {
        if (instance->init.hi2c->hdmatx != NULL) {
            /* ── DMA 직접 전송 (사용자 버퍼 → DMA) ── */
            HAL_StatusTypeDef hal_res = HAL_I2C_Master_Transmit_DMA(
                instance->init.hi2c, dev_addr, (uint8_t*)data, size );

            if ( hal_res != HAL_OK ) {
                result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
                break;
            }

            if ( !I2C_WAIT_TX(instance) ) {
                HAL_I2C_Master_Abort_IT(instance->init.hi2c, dev_addr);
                result = AGRBStatus_TIMEOUT;
                break;
            }
        } else {
            /* ── Polling fallback (hdmatx == NULL) ── */
            HAL_StatusTypeDef hal_res = HAL_I2C_Master_Transmit( instance->init.hi2c,
                dev_addr, (uint8_t*)data, size, instance->init.timeout );
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
        }
    } while(0);
    #else
    do {
        HAL_StatusTypeDef hal_res = HAL_I2C_Master_Transmit( instance->init.hi2c,
            dev_addr, (uint8_t*)data, size, instance->init.timeout );
        result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
    } while(0);
    #endif

    I2C_DEV_UNLOCK(instance);
    return result;
}

/**
 * @brief I2C Raw Receive — DMA 버퍼를 거치지 않고 원본 데이터 직접 수신
 *
 * RTOS: DEV_LOCK → DMA start → WAIT_RX → DEV_UNLOCK
 * BareMetal: Polling
 *
 * @warning data 포인터가 DMA-accessible 메모리에 있어야 함
 */
static AGRBStatusDef ioif_i2c_raw_receive(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size)
{
    if (id >= _i2c_instance_count) return AGRBStatus_PARAM_ERROR;
    if (data == NULL || size == 0) return AGRBStatus_PARAM_ERROR;
    if (dev_address > 0x77) return AGRBStatus_PARAM_ERROR;  /* 7-bit address only */

    IOIF_I2C_Instance_t* instance = &_i2c_instances[id];
    uint16_t dev_addr = dev_address << 1;

    if (!I2C_DEV_LOCK(instance)) return AGRBStatus_TIMEOUT;

    AGRBStatusDef result = AGRBStatus_OK;

    #if defined(USE_FREERTOS)
    do {
        HAL_StatusTypeDef hal_res = HAL_I2C_Master_Receive_DMA(
            instance->init.hi2c, dev_addr, (uint8_t*)data, size );

        if ( hal_res != HAL_OK ) {
            result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
            break;
        }

        if ( !I2C_WAIT_RX(instance) ) {
            HAL_I2C_Master_Abort_IT(instance->init.hi2c, dev_addr);
            result = AGRBStatus_TIMEOUT;
            break;
        }

        result = AGRBStatus_OK;
    } while(0);
    #else
    do {
        HAL_StatusTypeDef hal_res = HAL_I2C_Master_Receive( instance->init.hi2c,
            dev_addr, (uint8_t*)data, size, instance->init.timeout );
        result = IOIF_HAL_ERROR_TO_AGRB_STATUSDEF( hal_res );
    } while(0);
    #endif

    I2C_DEV_UNLOCK(instance);
    return result;
}



/**
 * @brief 마지막 HAL I2C 에러 코드 조회
 * @details ErrorCallback에서 저장한 hi2c->ErrorCode를 반환합니다.
 *          호출자가 NACK(AF) / Bus Error(BERR) / Timeout 등을 구분할 수 있습니다.
 * @return HAL_I2C_ERROR_xxx 비트마스크 (0 = 에러 없음)
 */
static uint32_t ioif_i2c_get_last_error(IOIF_I2Cx_t id)
{
    if (id >= _i2c_instance_count) return 0;
    return _i2c_instances[id].last_hal_error;
}


////////////////////////////////////////////////
///  Helper Functions  /////////////////////////
////////////////////////////////////////////////

static inline IOIF_I2C_Instance_t* _ioif_agrb_i2c_get_instance(IOIF_I2Cx_t id)
{
    if (id >= _i2c_instance_count) return NULL;
    return &_i2c_instances[id];
}

static inline IOIF_I2C_Instance_t* _ioif_agrb_i2c_get_instance_by_hi2c(I2C_HandleTypeDef* hi2c)
{
    for (IOIF_I2Cx_t i = 0; i < _i2c_instance_count; i++) {
        if ( _i2c_instances[i].init.hi2c == hi2c ) {
            return &_i2c_instances[i];
        }
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////
/* ISR Callbacks
 *
 * 핵심 원칙:
 * - ISR에서 device mutex 터치 금지 (Mutex 소유권은 Task에서만 관리)
 * - TX 완료 → SIGNAL_TX_ISR (대기 중인 Task가 Wake up)
 * - RX 완료 → SIGNAL_RX_ISR
 * - Error → error_count++ + SIGNAL_TX + SIGNAL_RX (대기 중인 Task가 깨어나서 에러 확인)
 */

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    IOIF_I2C_Instance_t* instance = _ioif_agrb_i2c_get_instance_by_hi2c(hi2c);
    if (instance == NULL) return;

    I2C_SIGNAL_TX_ISR(instance);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    IOIF_I2C_Instance_t* instance = _ioif_agrb_i2c_get_instance_by_hi2c(hi2c);
    if (instance == NULL) return;

    I2C_SIGNAL_RX_ISR(instance);
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    IOIF_I2C_Instance_t* instance = _ioif_agrb_i2c_get_instance_by_hi2c(hi2c);
    if (instance == NULL) return;

    I2C_SIGNAL_TX_ISR(instance);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    IOIF_I2C_Instance_t* instance = _ioif_agrb_i2c_get_instance_by_hi2c(hi2c);
    if (instance == NULL) return;

    I2C_SIGNAL_RX_ISR(instance);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    IOIF_I2C_Instance_t* instance = _ioif_agrb_i2c_get_instance_by_hi2c(hi2c);
    if (instance == NULL) return;

    instance->error_count++;
    instance->last_hal_error = hi2c->ErrorCode;  /* HAL_I2C_ERROR_AF, _BERR, _TIMEOUT 등 */

    /* 대기 중인 Task를 깨워서 에러 경로로 진입시킴 (device mutex는 터치 안 함) */
    I2C_SIGNAL_TX_ISR(instance);
    I2C_SIGNAL_RX_ISR(instance);
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c)
{
    IOIF_I2C_Instance_t* instance = _ioif_agrb_i2c_get_instance_by_hi2c(hi2c);
    if (instance == NULL) return;

    I2C_SIGNAL_TX_ISR(instance);
    I2C_SIGNAL_RX_ISR(instance);
}

#endif /* AGRB_IOIF_I2C_ENABLE */
