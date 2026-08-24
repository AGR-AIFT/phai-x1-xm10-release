/**
 ******************************************************************************
 * @file    ioif_agrb_psram.c
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] PSRAM (QSPI) 하드웨어 추상화 구현부 (aeat_9955 origin)
 * @version 3.0 (Common Library - H7 Only)
 * @date    Feb 12, 2026
 *
 * @details
 * - QSPI + Memory Mapped 모드 지원
 * - 8 MiB PSRAM, 8-바이트 정렬 할당
 * - DMA 기반 쓰기, Memory Mapped 읽기
 * - 벤치마크 기능 (IOIF_PSRAM_BENCHMARK_AVAILABLE)
 *
 * @note aeat_9955 원본 기반, ENABLE 가드 적용, D-Cache 조건부
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_psram.h"

#if defined(AGRB_IOIF_PSRAM_ENABLE) && defined(IOIF_MCU_SERIES_H7)

#include <string.h>

#if defined(USE_FREERTOS)
#include "cmsis_os2.h"
#endif

#if defined(AGRB_IOIF_DMA_ENABLE)
#include "ioif_agrb_dma.h"
#endif

#define IOIF_PSRAM_TOTAL_SIZE_MiB       (8U)
#define IOIF_PSRAM_TOTAL_SIZE_BYTES     (IOIF_PSRAM_TOTAL_SIZE_MiB * 1024U * 1024U)
#define IOIF_PSRAM_BASE_ADDRESS         (0x90000000U)
#define IOIF_PSRAM_END_ADDRESS          (IOIF_PSRAM_BASE_ADDRESS + IOIF_PSRAM_TOTAL_SIZE_BYTES)
#define IOIF_PSRAM_ALLOC_ALIGNMENT      (8U)
#define IOIF_PSRAM_DEFAULT_TIMEOUT_MS   (HAL_QPSI_TIMEOUT_DEFAULT_VALUE)

#define IOIF_PSRAM_ALLOCATE_NUM_MAX             (32)
#ifndef IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES
#define IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES    (4U * 32u)
#endif

#ifndef IOIF_PSRAM_BENCHMARK_BUFFER_SIZE_BYTES
#define IOIF_PSRAM_BENCHMARK_BUFFER_SIZE_BYTES   (256U * 1024U)
#endif

/***********************************************************/
/******************* Structure Definition *******************/
/***********************************************************/
typedef struct {
    uint32_t address;
    size_t size;
    uint32_t last_write_offset;

} IOIF_PSRAM_AllocationEntry_t;

typedef struct {
    bool initialized;

    IOIF_PSRAM_Initialize_t init;

    struct {
        osSemaphoreId_t mutex;
        osSemaphoreId_t tx;
    } semaphore;

    IOIF_DMAx_t* dma;

    IOIF_PSRAM_AllocationEntry_t allocations[IOIF_PSRAM_ALLOCATE_NUM_MAX];
    uint32_t allocation_count;

} IOIF_PSRAM_Instance_t;


/***********************************************************/
/******************* Variable Definition *******************/
/***********************************************************/
static bool _qspi_reset(QSPI_HandleTypeDef* hqspi);
static bool _qspi_mem_mapped(QSPI_HandleTypeDef* hqspi);
static void _qspi_acquire_semaphore(void);
static void _qspi_release_semaphore(void);
static void _qspi_acquire_tx_semaphore(void);
static void _qspi_release_tx_semaphore(void);
static size_t _get_allocatable_size(void);

/***********************************************************/
static AGRBStatusDef _initialize(IOIF_PSRAM_Initialize_t* config);
static IOIF_PSRAMx_t _alloc(size_t size);
static bool __alloc_more_big_memory(IOIF_PSRAMx_t id, size_t additional_size);
static bool _write(IOIF_PSRAMx_t id, void* data, size_t size);
static bool _write_offset(IOIF_PSRAMx_t id, uint32_t offset, void* data, size_t size);
static bool _write_raw(IOIF_PSRAMx_t id, uint32_t offset, void* data, size_t size);

static const void* _get_mapped_address(IOIF_PSRAMx_t id);
static void _start_memory_mapped(void);
static void _stop_memory_mapped(void);
static void _free(IOIF_PSRAMx_t id);
static void _flush(void);
static size_t _get_total_size(void);
static uint32_t xorshift32(uint32_t xs);
static void _benchmark(IOIF_PSRAM_Benchmark_t* report);
/***********************************************************/
/******************* Handle Definition *********************/
/***********************************************************/
IOIF_PSRAM_Instance_t _psram_instance = {0};

IOIF_PSRAM_Handle_t ioif_psram = {
    .initialize = _initialize,
    .alloc = _alloc,
    .write = _write,
    .write_offset = _write_offset,
    .get_mapped_address = _get_mapped_address,
    .start_memory_mapped = _start_memory_mapped,
    .stop_memory_mapped = _stop_memory_mapped,
    .free = _free,
    .flush = _flush,
    .get_total_size = _get_total_size,
    .benchmark = _benchmark,
};

/***********************************************************/
/************* Handle Function Implements ******************/
/***********************************************************/

static AGRBStatusDef _initialize(IOIF_PSRAM_Initialize_t* init)
{
    if ( init == NULL || init->hqspi == NULL ) return AGRBStatus_PARAM_ERROR;

    memset( &_psram_instance, 0, sizeof(IOIF_PSRAM_Instance_t) );
    memcpy( &_psram_instance.init, init, sizeof(IOIF_PSRAM_Initialize_t) );

    _qspi_reset( _psram_instance.init.hqspi );

    _psram_instance.semaphore.mutex = osSemaphoreNew(1, 1, NULL);
    if ( _psram_instance.semaphore.mutex == NULL ) return AGRBStatus_SEMAPHORE_ERROR;
    _psram_instance.semaphore.tx = osSemaphoreNew(1, 1, NULL);
    if ( _psram_instance.semaphore.tx == NULL ) return AGRBStatus_SEMAPHORE_ERROR;

    _psram_instance.dma = ioif_dma.allocate( (DMA_HandleTypeDef*)_psram_instance.init.hqspi->hmdma, IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES, (const char*)"PSRAM_DMA_Buffer" );
    if ( _psram_instance.dma == NULL ) return AGRBStatus_SEMAPHORE_ERROR;

    _psram_instance.initialized = true;

    return AGRBStatus_OK;
}

static IOIF_PSRAMx_t _alloc(size_t size)
{
    if ( size == 0 ) return (IOIF_PSRAMx_t)-1;
    if ( _psram_instance.allocation_count >= IOIF_PSRAM_ALLOCATE_NUM_MAX ) return (IOIF_PSRAMx_t)-1;

    size_t aligned_size = (size + (IOIF_PSRAM_ALLOC_ALIGNMENT - 1)) & ~(IOIF_PSRAM_ALLOC_ALIGNMENT - 1);

    if ( aligned_size > _get_allocatable_size() ) return (IOIF_PSRAMx_t)-1;

    uint32_t last_available_position = (0);

    _qspi_acquire_semaphore();
    IOIF_PSRAMx_t new_id = _psram_instance.allocation_count;
    do {
        if ( new_id > 0 ) {
            IOIF_PSRAM_AllocationEntry_t* last_entry = &_psram_instance.allocations[new_id - 1];
            last_available_position = last_entry->address + (uint32_t)(last_entry->size);
        }

        _psram_instance.allocations[new_id].address = last_available_position;
        _psram_instance.allocations[new_id].size = aligned_size;

        if ( (_psram_instance.allocations[new_id].address + (uint32_t)_psram_instance.allocations[new_id].size) > IOIF_PSRAM_END_ADDRESS ) {
            _psram_instance.allocations[new_id].address = 0;
            _psram_instance.allocations[new_id].size = 0;
            new_id = (IOIF_PSRAMx_t)-1;
            break;
        }

        size_t remain = aligned_size;
        while ( remain > 0 ) {
            size_t chunk_size = ( remain > IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES ) ? IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES : remain;

            if ( !_write_raw( new_id, (uint32_t)(aligned_size - remain), NULL, chunk_size ) ) {
                _psram_instance.allocations[new_id].address = 0;
                _psram_instance.allocations[new_id].size = 0;
                new_id = (IOIF_PSRAMx_t)-1;

                break;
            }

            remain -= chunk_size;
        }

    } while (0);
    _qspi_release_semaphore();

    if ( new_id == (IOIF_PSRAMx_t)-1 ) return (IOIF_PSRAMx_t)-1;
    return _psram_instance.allocation_count++;
}

static bool __alloc_more_big_memory(IOIF_PSRAMx_t id, size_t additional_size)
{
    bool result = false;
    _qspi_acquire_semaphore();
    do {
        if ( id != (IOIF_PSRAMx_t)_psram_instance.allocation_count - 1 ) break;

        size_t aligned_additional_size = (additional_size + (IOIF_PSRAM_ALLOC_ALIGNMENT - 1)) & ~(IOIF_PSRAM_ALLOC_ALIGNMENT - 1);
        aligned_additional_size = aligned_additional_size << 3;

        if ( aligned_additional_size > _get_allocatable_size() ) break;

        _psram_instance.allocations[id].size += aligned_additional_size;

        result = true;
    } while(0);
    _qspi_release_semaphore();

    return result;
}

static bool _write(IOIF_PSRAMx_t id, void* data, size_t size)
{
    return _write_offset( id, _psram_instance.allocations[id].last_write_offset, data, size );
}

static bool _write_offset(IOIF_PSRAMx_t id, uint32_t offset, void* data, size_t size)
{
    if ( id >= (IOIF_PSRAMx_t)_psram_instance.allocation_count ) return false;
    if ( data == NULL || size == 0 ) return false;
    if ( (offset % IOIF_PSRAM_ALLOC_ALIGNMENT) != 0 ) return false;
    if ( (size % IOIF_PSRAM_ALLOC_ALIGNMENT) != 0 ) return false;
    if ( (offset + size) > _psram_instance.allocations[id].size )
    {
        if ( !__alloc_more_big_memory(id, size) ) return false;
        if ( (offset + size) > _psram_instance.allocations[id].size ) return false;
    }
    if ( size > IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES ) return false;

    bool result;

    _qspi_acquire_semaphore();
    {
        result = _write_raw( id, offset, data, size );
        if ( result ) _psram_instance.allocations[id].last_write_offset = offset + (uint32_t)size;
    }
    _qspi_release_semaphore();

    return result;
}

static bool _write_raw(IOIF_PSRAMx_t id, uint32_t offset, void* data, size_t size)
{
    /* [Fix] APS6404L Quad Write (0x38) 커맨드 설정
     *
     * [AS-IS] AddressSize 미설정 (= 0 = QSPI_ADDRESS_8_BITS) + DummyCycles = 6
     *   → 8-bit 주소만 전송 → 256바이트 이후 주소 전부 오류
     *   → 6 dummy cycles → PSRAM이 dummy를 데이터로 해석 → 3바이트 shift
     *   → Write 주소 ≠ MM Read 주소 → 전체 데이터 손상
     *
     * [TO-BE] AddressSize = 24_BITS (MM Read와 동일) + DummyCycles = 0
     *   → APS6404L Quad Write: Instruction → Address(24-bit) → Data (no dummy)
     *   → APS6404L Fast Read Quad(0xEB): Instruction → Address → 6 dummy → Data
     *
     * [Impact] _alloc zero-fill + write_offset 모두 정상 주소에 기록.
     *          MM Read가 동일 주소에서 정확한 데이터 반환. */
    static QSPI_CommandTypeDef _cmd = {
        .Instruction = 0x38,
        .InstructionMode = QSPI_INSTRUCTION_4_LINES,
        .Address = 0,
        .AddressSize = QSPI_ADDRESS_24_BITS,
        .AddressMode = QSPI_ADDRESS_4_LINES,
        .DataMode = QSPI_DATA_4_LINES,
        .DummyCycles = 0,
        .DdrMode = QSPI_DDR_MODE_DISABLE,
        .DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY,
        .SIOOMode = QSPI_SIOO_INST_EVERY_CMD,
    };

    QSPI_HandleTypeDef* hqspi = _psram_instance.init.hqspi;
    IOIF_PSRAM_AllocationEntry_t* entry = &_psram_instance.allocations[id];
    uint8_t* tx_buffer = _psram_instance.dma->buffer;

    if ( hqspi->State == HAL_QSPI_STATE_BUSY_MEM_MAPPED )  return false;

    _qspi_acquire_tx_semaphore();
    do {
        _cmd.Address = (entry->address + offset);
        _cmd.NbData = size;

        if ( (HAL_QSPI_Command_IT( hqspi, &_cmd )) != HAL_OK )
        {
            _qspi_release_tx_semaphore();
            return false;
        }

        if ( data != NULL ) memcpy( tx_buffer, data, size );
        else memset( tx_buffer, 0, size );

        #if IOIF_HAS_DCACHE
        SCB_CleanDCache_by_Addr( (uint32_t*)tx_buffer, size );
        #endif
        if ( (HAL_QSPI_Transmit_DMA( hqspi, tx_buffer )) != HAL_OK )
        {
            _qspi_release_tx_semaphore();
            return false;
        }
    } while(0);

    return true;
}

static const void* _get_mapped_address(IOIF_PSRAMx_t id)
{
    return (const void*)(uintptr_t)(_psram_instance.allocations[id].address + IOIF_PSRAM_BASE_ADDRESS);
}

static void _start_memory_mapped(void)
{
    _qspi_acquire_semaphore();
    _qspi_acquire_tx_semaphore();
    _qspi_mem_mapped( _psram_instance.init.hqspi );
}

static void _stop_memory_mapped(void)
{
    HAL_QSPI_Abort( _psram_instance.init.hqspi );
    _qspi_release_tx_semaphore();
    _qspi_release_semaphore();
}

static void _free(IOIF_PSRAMx_t id)
{
    if ( _psram_instance.allocation_count == 0 ) return;
    if ( id >= (IOIF_PSRAMx_t)_psram_instance.allocation_count ) return;

    _qspi_acquire_semaphore();
    _qspi_acquire_tx_semaphore();
    {
        if ( id == (IOIF_PSRAMx_t)(_psram_instance.allocation_count - 1) ) {
            _psram_instance.allocations[id].address = 0;
            _psram_instance.allocations[id].size = 0;
            _psram_instance.allocation_count--;
        }
    }
    _qspi_release_tx_semaphore();
    _qspi_release_semaphore();
}

static void _flush(void)
{
    _qspi_acquire_semaphore();
    _qspi_acquire_tx_semaphore();
    {
        _psram_instance.allocation_count = 0;
        memset( _psram_instance.allocations, 0, sizeof(_psram_instance.allocations) );
    }
    _qspi_release_tx_semaphore();
    _qspi_release_semaphore();
}

static size_t _get_total_size(void)
{
    return IOIF_PSRAM_TOTAL_SIZE_BYTES;
}


static inline uint32_t xorshift32(uint32_t xs) {
#if defined(IOIF_PSRAM_BENCHMARK_AVAILABLE)
    static uint32_t count = 0;
    uint32_t x = xs + (++count);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
#else
    (void)xs;
    return 0;
#endif
}

static void _benchmark(IOIF_PSRAM_Benchmark_t* report)
{
#if defined(IOIF_PSRAM_BENCHMARK_AVAILABLE)

    static uint8_t _dirty_buffer[IOIF_PSRAM_BENCHMARK_BUFFER_SIZE_BYTES];
    static uint8_t test_chunk_buffer[ IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES ];
    volatile uint32_t __bench_dummy = 0;

    uint32_t rnd_seed = 0x12345678;

    #define WORK_LOOP_COUNT    (64U)
    #define RANDOM_ACCESS_COUNT   (2048U) * ( 32 )

    if ( report == NULL ) return;

    memset( report, 0, sizeof(IOIF_PSRAM_Benchmark_t) );

    report->memory.target_size_bytes = IOIF_PSRAM_BENCHMARK_BUFFER_SIZE_BYTES;
    report->memory.chunk_size_bytes = IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES;

    uint32_t start_tick, end_tick, elapsed_ms;

    for ( size_t i = 0; i < sizeof(_dirty_buffer); i++ ) {
        _dirty_buffer[i] = (uint8_t)(xorshift32(rnd_seed) & 0xFF);
    }

    uint32_t total_size = IOIF_PSRAM_BENCHMARK_BUFFER_SIZE_BYTES;

    /* Sequential Read - Embedded SRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < WORK_LOOP_COUNT; i++ ) {
        for ( uint32_t addr = 0; addr < total_size; addr += IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES ) {
            memcpy( test_chunk_buffer, &_dirty_buffer[addr], IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
            __bench_dummy += test_chunk_buffer[0];
        }
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->sequential_read.embed = ( (float)(total_size * WORK_LOOP_COUNT) ) / (float)(elapsed_ms);

    /* Random Read - Embedded SRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < RANDOM_ACCESS_COUNT; i++ ) {
        uint32_t random_offset = xorshift32(rnd_seed) % (total_size);
        random_offset &= ~(IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES - 1);
        if ( (random_offset + IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) > total_size ) {
            random_offset = total_size - IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES;
        }
        memcpy( test_chunk_buffer, &_dirty_buffer[random_offset], IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
        __bench_dummy += test_chunk_buffer[0];
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->random_read.embed = ( (float)(RANDOM_ACCESS_COUNT * IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) ) / (float)(elapsed_ms);

    for ( size_t i = 0; i < IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES; i++ ) {
        test_chunk_buffer[i] = (uint8_t)(xorshift32(rnd_seed) & 0xFF);
    }

    /* Sequential Write - Embedded SRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < WORK_LOOP_COUNT; i++ ) {
        for ( uint32_t addr = 0; addr < total_size; addr += IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES ) {
            memcpy( &_dirty_buffer[addr], test_chunk_buffer, IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
            __bench_dummy += _dirty_buffer[addr];
        }
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->sequential_write.embed = ( (float)(total_size * WORK_LOOP_COUNT) ) / (float)(elapsed_ms);

    /* Random Write - Embedded SRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < RANDOM_ACCESS_COUNT; i++ ) {
        uint32_t random_offset = xorshift32(rnd_seed) % (total_size);
        random_offset &= ~(IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES - 1);
        if ( (random_offset + IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) > total_size ) {
            random_offset = total_size - IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES;
        }
        memcpy( &_dirty_buffer[random_offset], test_chunk_buffer, IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
        __bench_dummy += _dirty_buffer[random_offset];
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->random_write.embed = ( (float)(RANDOM_ACCESS_COUNT * IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) ) / (float)(elapsed_ms);

    /* PSRAM Tests */
    IOIF_PSRAMx_t benchmark_id = ioif_psram.alloc( IOIF_PSRAM_BENCHMARK_BUFFER_SIZE_BYTES );

    /* Sequential Write - PSRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < WORK_LOOP_COUNT; i++ ) {
        for ( uint32_t addr = 0; addr < total_size; addr += IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES ) {
            ioif_psram.write_offset( benchmark_id, addr, test_chunk_buffer, IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
        }
        __bench_dummy += test_chunk_buffer[0];
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->sequential_write.psram = ( (float)(total_size * WORK_LOOP_COUNT) ) / (float)(elapsed_ms);

    /* Random Write - PSRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < RANDOM_ACCESS_COUNT; i++ ) {
        uint32_t random_offset = xorshift32(rnd_seed) % (total_size);
        random_offset &= ~(IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES - 1);
        if ( (random_offset + IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) > total_size ) {
            random_offset = total_size - IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES;
        }
        ioif_psram.write_offset( benchmark_id, random_offset, test_chunk_buffer, IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
        __bench_dummy += test_chunk_buffer[0];
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->random_write.psram = ( (float)(RANDOM_ACCESS_COUNT * IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) ) / (float)(elapsed_ms);

    /* Read Start */
    _start_memory_mapped();

    uint8_t* psram_mapped_address = (uint8_t*)ioif_psram.get_mapped_address( benchmark_id );

    /* Sequential Read - PSRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < WORK_LOOP_COUNT; i++ ) {
        for ( uint32_t addr = 0; addr < total_size; addr += IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES ) {
            memcpy( test_chunk_buffer, (uint8_t*)( psram_mapped_address + addr ), IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
            __bench_dummy += test_chunk_buffer[0];
        }
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->sequential_read.psram = ( (float)(total_size * WORK_LOOP_COUNT) ) / (float)(elapsed_ms);

    /* Random Read - PSRAM */
    start_tick = osKernelGetTickCount();
    for ( uint32_t i = 0; i < RANDOM_ACCESS_COUNT; i++ ) {
        uint32_t random_offset = xorshift32(rnd_seed) % (total_size);
        random_offset &= ~(IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES - 1);
        if ( (random_offset + IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) > total_size ) {
            random_offset = total_size - IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES;
        }
        memcpy( test_chunk_buffer, (uint8_t*)( (uint8_t*)psram_mapped_address + random_offset ), IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES );
        __bench_dummy += test_chunk_buffer[0];
    }
    end_tick = osKernelGetTickCount();
    elapsed_ms = end_tick - start_tick;
    report->random_read.psram = ( (float)(RANDOM_ACCESS_COUNT * IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES) ) / (float)(elapsed_ms);

    _stop_memory_mapped();

    ioif_psram.free( benchmark_id );

    {
        report->sequential_read.ratio = report->sequential_read.psram / report->sequential_read.embed;
        report->sequential_write.ratio = report->sequential_write.psram / report->sequential_write.embed;
        report->random_read.ratio = report->random_read.psram / report->random_read.embed;
        report->random_write.ratio = report->random_write.psram / report->random_write.embed;
    }
#else
    (void)report;
#endif
}

/***********************************************************/
static bool _qspi_reset(QSPI_HandleTypeDef* hqspi)
{
    if ( hqspi == NULL ) return false;

    HAL_QSPI_Abort( hqspi );

    osDelay(1);

    /* [Fix] IOC 설정과 일치시킴 (CubeMX QUADSPI Configuration)
     *
     * [AS-IS] ClockPrescaler=2, FifoThreshold=1, SampleShifting=NONE, CSHighTime=3
     *   → IOC(main.c)와 불일치 → MDMA BufferTransferLength=4와 FifoThreshold 미스매치
     *   → Read SampleShifting=NONE으로 타이밍 마진 부족
     *
     * [TO-BE] IOC 설정 그대로 사용
     *   → FifoThreshold=4: MDMA BTL=4와 정확히 매칭
     *   → SampleShifting=HALFCYCLE: MM Read 타이밍 마진 확보
     *   → ClockPrescaler=3: APS6404L 스펙 내 안정적 동작 */
    hqspi->Instance = QUADSPI;
    hqspi->Init.ClockPrescaler = 3;
    hqspi->Init.FifoThreshold = 4;
    hqspi->Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi->Init.FlashSize = 22;
    hqspi->Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
    hqspi->Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi->Init.FlashID = QSPI_FLASH_ID_1;
    hqspi->Init.DualFlash = QSPI_DUALFLASH_DISABLE;

    if ( HAL_QSPI_Init( hqspi ) != HAL_OK ) return false;

    QSPI_CommandTypeDef cmd;

    {
        memset( &cmd, 0, sizeof(QSPI_CommandTypeDef) );
        cmd.Instruction = 0x66; //Reset Enable
        cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
        if ( HAL_QSPI_Command( hqspi, &cmd, IOIF_PSRAM_DEFAULT_TIMEOUT_MS ) != HAL_OK ) return false;
    }

    osDelay(1);

    {
        memset( &cmd, 0, sizeof(QSPI_CommandTypeDef) );
        cmd.Instruction = 0x99; //Reset Memory
        cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
        if ( HAL_QSPI_Command( hqspi, &cmd, IOIF_PSRAM_DEFAULT_TIMEOUT_MS ) != HAL_OK ) return false;
    }

    osDelay(1);

    {
        memset( &cmd, 0, sizeof(QSPI_CommandTypeDef) );
        cmd.Instruction = 0x35; //Enter Quad Mode
        cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
        if ( HAL_QSPI_Command( hqspi, &cmd, IOIF_PSRAM_DEFAULT_TIMEOUT_MS ) != HAL_OK ) return false;
    }

    osDelay(1);

    return true;
}

static bool _qspi_mem_mapped(QSPI_HandleTypeDef* hqspi)
{
    if ( (hqspi->State == HAL_QSPI_STATE_BUSY_MEM_MAPPED) ) return true;

    QSPI_CommandTypeDef         cmd;
    QSPI_MemoryMappedTypeDef    mem_mapped_cfg;

    memset( &cmd, 0, sizeof(QSPI_CommandTypeDef) );
    memset( &mem_mapped_cfg, 0, sizeof(QSPI_MemoryMappedTypeDef) );

    {
        cmd.Instruction = 0xEB;
        cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
        cmd.AddressMode = QSPI_ADDRESS_4_LINES;
        cmd.DataMode = QSPI_DATA_4_LINES;
        cmd.DummyCycles = 6;
        cmd.AddressSize = QSPI_ADDRESS_24_BITS;

        mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
        mem_mapped_cfg.TimeOutPeriod = 0;

        HAL_QSPI_MemoryMapped( hqspi, &cmd, &mem_mapped_cfg );
    }

    return true;
}

static inline void _qspi_acquire_semaphore(void)
{
    if ( _psram_instance.semaphore.mutex == NULL ) while (1);
    osSemaphoreAcquire( _psram_instance.semaphore.mutex, osWaitForever );
}
static inline void _qspi_release_semaphore(void)
{
    if ( _psram_instance.semaphore.mutex == NULL ) while (1);
    osSemaphoreRelease( _psram_instance.semaphore.mutex );
}
static inline void _qspi_acquire_tx_semaphore(void)
{
    if ( _psram_instance.semaphore.tx == NULL ) while (1);
    osSemaphoreAcquire( _psram_instance.semaphore.tx, osWaitForever );
}
static inline void _qspi_release_tx_semaphore(void)
{
    if ( _psram_instance.semaphore.tx == NULL ) while (1);
    osSemaphoreRelease( _psram_instance.semaphore.tx );
}

static size_t _get_allocatable_size(void)
{
    if( _psram_instance.allocation_count == 0 ) return IOIF_PSRAM_TOTAL_SIZE_BYTES;
    if ( _psram_instance.allocation_count >= IOIF_PSRAM_ALLOCATE_NUM_MAX ) return 0;

    size_t remain_size;
    _qspi_acquire_semaphore();
    {
        IOIF_PSRAM_AllocationEntry_t* last_entry = &_psram_instance.allocations[ _psram_instance.allocation_count - 1 ];
        remain_size = (IOIF_PSRAM_TOTAL_SIZE_BYTES) - (last_entry->address + last_entry->size);
    }
    _qspi_release_semaphore();

    return (remain_size/8)*8;
}


/***********************************************************************/
void HAL_QSPI_ErrorCallback(QSPI_HandleTypeDef *hqspi)
{
    if ( hqspi != _psram_instance.init.hqspi ) return;

    osSemaphoreRelease( _psram_instance.semaphore.mutex );
    osSemaphoreRelease( _psram_instance.semaphore.tx );
}

void HAL_QSPI_AbortCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    (void)hqspi;
    osSemaphoreRelease( _psram_instance.semaphore.tx );
}

void HAL_QSPI_CmdCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    if ( hqspi != _psram_instance.init.hqspi ) return;

    /* [Fix] CmdCplt = command phase 완료 (instruction + address + dummy).
     * 전체 write 동작(command + data DMA)은 TxCpltCallback에서 완료.
     *
     * [AS-IS] 여기서 tx_sem release → start_memory_mapped()가 DMA 완료 전에
     *   tx_sem 획득 성공 → HAL_QSPI_MemoryMapped() 호출 시 QSPI 상태 BUSY_INDIRECT_TX
     *   → HAL_BUSY 반환 → MM 모드 진입 실패 → PSRAM 읽기 garbage.
     *
     * [TO-BE] CmdCplt에서 tx_sem 미해제 → TxCplt(DMA 완료)에서만 release
     *   → start_memory_mapped()가 tx_sem 대기 → DMA 완료 후 MM 모드 진입 → 정상 읽기.
     *
     * [Impact] _alloc 루프도 정상: 다음 _write_raw의 tx_sem acquire가
     *   이전 DMA의 TxCplt까지 자연스럽게 대기 → 순차 정합성 보장.
     */
    /* osSemaphoreRelease( _psram_instance.semaphore.tx ); — removed */
}

void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    (void)hqspi;
    /* TODO? */
}

void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    if ( hqspi != _psram_instance.init.hqspi ) return;

    osSemaphoreRelease( _psram_instance.semaphore.tx );
}

void HAL_QSPI_TimeOutCallback(QSPI_HandleTypeDef *hqspi)
{
    (void)hqspi;
    /* TODO ? */
}
/***********************************************************************/

#endif /* AGRB_IOIF_PSRAM_ENABLE && IOIF_MCU_SERIES_H7 */
