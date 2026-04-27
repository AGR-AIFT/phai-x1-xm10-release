/**
 ******************************************************************************
 * @file    ioif_agrb_adc.c
 * @author  HyundoKim
 * @brief   [IOIF Layer] ADC 추상화 드라이버 구현부 (H7/G4 통합)
 * @version 0.2
 * @date    Jan 27, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_adc.h"
#if defined(AGRB_IOIF_ADC_ENABLE)

#include <string.h>
#include <stdbool.h>

/**
 * @brief DMA 내부 버퍼 (RTOS/BareMetal 공통)
 *
 * [STM32H7]
 * - MPU 설정으로 IOIF_DMA_SECTION이 non-cacheable
 * - 캐시 무효화 불필요
 *
 * [STM32G4]
 * - D-Cache 없음 → 캐시 문제 없음
 * - 일반 SRAM 영역 사용
 *
 * @note DMA는 RTOS 없이도 동작하므로 USE_FREERTOS 밖에서 선언
 */
#if defined(IOIF_MCU_SERIES_H7)
    __attribute__((section(IOIF_DMA_SECTION)))
    static uint16_t _adc_dma_normal_buffer[IOIF_ADC_MAX_INSTANCES][IOIF_ADC_MAX_CHANNEL];

    __attribute__((section(IOIF_DMA_SECTION)))
    static uint16_t _adc_dma_circular_buffer[IOIF_ADC_MAX_INSTANCES][IOIF_ADC_MAX_CHANNEL];
#else
    /* G4 또는 기타: D-Cache 없음 → 일반 버퍼 사용 */
    static uint16_t _adc_dma_normal_buffer[IOIF_ADC_MAX_INSTANCES][IOIF_ADC_MAX_CHANNEL];
    static uint16_t _adc_dma_circular_buffer[IOIF_ADC_MAX_INSTANCES][IOIF_ADC_MAX_CHANNEL];
#endif

#if defined(USE_FREERTOS)
#include "cmsis_os.h"
#endif

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#if defined(USE_FREERTOS)
/**
 * @brief ADC 인스턴스 Mutex 획득
 */
#define IOIF_ADC_ACQUIRE_DEV_SEMAPHORE(instance)  do {                  \
    if (instance == NULL) return AGRBStatus_ERROR;                      \
    SemaphoreHandle_t __semaphore = (instance)->handle;                 \
    if (__semaphore != NULL) {                                          \
        if (xSemaphoreTake(__semaphore,                                 \
            pdMS_TO_TICKS(IOIF_ADC_CONVERSION_TIMEOUT)) != pdTRUE) {    \
            return AGRBStatus_TIMEOUT;                                  \
        }                                                               \
    } else {                                                            \
        return AGRBStatus_ERROR;                                        \
    }                                                                   \
} while(0)

/**
 * @brief ADC 인스턴스 Mutex 해제
 */
#define IOIF_ADC_RELEASE_DEV_SEMAPHORE(instance)  do {                  \
    if (instance == NULL) return AGRBStatus_ERROR;                      \
    SemaphoreHandle_t __semaphore = (instance)->handle;                 \
    if (__semaphore != NULL) {                                          \
        xSemaphoreGive(__semaphore);                                    \
    }                                                                   \
} while(0)

/**
 * @brief ADC 인스턴스 Mutex 해제 (ISR)
 */
#define IOIF_ADC_RELEASE_DEV_SEMAPHORE_ISR(instance)  do {              \
    if (instance == NULL) return AGRBStatus_ERROR;                      \
    SemaphoreHandle_t __semaphore = (instance)->handle;                 \
    if (__semaphore != NULL) {                                          \
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;                  \
        xSemaphoreGiveFromISR(                                          \
            __semaphore,                                                \
            &xHigherPriorityTaskWoken);                                 \
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);                   \
    }                                                                   \
} while(0)

/**
 * @brief DMA 완료 시그널 대기
 */
#define IOIF_ADC_WAIT_DMA_COMPLETE(instance)  do {                      \
    if (instance == NULL) return AGRBStatus_ERROR;                      \
    SemaphoreHandle_t __semaphore = (instance)->dma;                    \
    if (__semaphore != NULL) {                                          \
        if (xSemaphoreTake(__semaphore,                                 \
            pdMS_TO_TICKS(IOIF_ADC_CONVERSION_TIMEOUT)) != pdTRUE) {    \
            HAL_ADC_Stop_DMA(instance->hadc);                           \
            return AGRBStatus_TIMEOUT;                                  \
        }                                                               \
    } else {                                                            \
        return AGRBStatus_ERROR;                                        \
    }                                                                   \
} while(0)

/**
 * @brief [ISR] DMA 완료 시그널 전송
 */
#define IOIF_ADC_SIGNAL_DMA_COMPLETE(instance)  do {                    \
    if (instance == NULL) return; /* 핸들러 없음 */                     \
    SemaphoreHandle_t __semaphore = (instance)->dma;                    \
    if (__semaphore != NULL) {                                          \
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;                  \
        xSemaphoreGiveFromISR(                                          \
            __semaphore,                                                \
            &xHigherPriorityTaskWoken);                                 \
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);                   \
    }                                                                   \
} while(0)

#else //No RTOS
/* ... (Bare-metal 매크로) ... */
#define IOIF_ADC_ACQUIRE_DEV_SEMAPHORE(instance)
#define IOIF_ADC_RELEASE_DEV_SEMAPHORE(instance)
#define IOIF_ADC_RELEASE_DEV_SEMAPHORE_ISR(instance)
#define IOIF_ADC_WAIT_DMA_COMPLETE(instance)
#define IOIF_ADC_SIGNAL_DMA_COMPLETE(instance)
#endif

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


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

/**
 * @brief ADC 인스턴스 풀 (최대 2개)
 */
static IOIF_ADC_Instance_t _instances[IOIF_ADC_MAX_INSTANCES];
static uint32_t _instance_count = 0;

/**
 * @brief User Callback 배열 (각 ADC 인스턴스별)
 * @note Index 매핑: ADC1=0, ADC2=1, ADC3=2, ADC4=3, ADC5=4
 */
#define IOIF_ADC_MAX_PERIPHERALS 5  // H7: ADC1,2,3 / G4: ADC1,2,3,4,5
static IOIF_ADC_UserCallback_t s_user_callbacks[IOIF_ADC_MAX_PERIPHERALS] = {NULL};

/**
 * ============================================================================
 * [신규] ADC Manual Init 관련 Private 변수
 * ============================================================================
 */

/**
 * @brief 수동 초기화된 ADC 채널 정보 추적 구조체
 */
typedef struct {
    uint32_t channel;         /**< ADC_CHANNEL_x */
    uint32_t rank;            /**< Rank (1부터 시작) */
    uint32_t sampling_time;   /**< ADC_SAMPLETIME_x */
    bool is_configured;       /**< 설정 완료 여부 */
} IOIF_ADC_ChannelConfig_t;

/**
 * @brief 수동 초기화된 ADC 인스턴스 확장 정보
 */
typedef struct {
    bool is_manual_init;                                    /**< 수동 초기화 여부 */
    ADC_HandleTypeDef hadc_manual;                          /**< 수동 초기화된 HAL 핸들 */
    DMA_HandleTypeDef hdma_manual;                          /**< 수동 초기화된 DMA 핸들 */
    IOIF_ADC_ChannelConfig_t channels[IOIF_ADC_MAX_CHANNEL]; /**< 채널 설정 배열 */
    uint8_t channel_count;                                  /**< 현재 설정된 채널 개수 */
} IOIF_ADC_ManualInitInfo_t;

/**
 * @brief 수동 초기화 정보 배열 (인스턴스별)
 */
static IOIF_ADC_ManualInitInfo_t s_manual_init_info[IOIF_ADC_MAX_INSTANCES];

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static IOIF_ADC_Instance_t* _FindHandler(ADC_HandleTypeDef* hadc);
static inline uint32_t _ConvertResolution(uint32_t stm32_res_raw);
static inline IOIF_ADC_Method_t _ConvertMethod(uint32_t stm32_method_raw);
static AGRBStatusDef _GetValueDmaNormal(IOIF_ADC_Instance_t* instance, IOIF_ADC_ReportData_t* report);
static AGRBStatusDef _GetValuePolling(IOIF_ADC_Instance_t* instance, IOIF_ADC_ReportData_t* report);
static inline uint8_t _GetAdcIndex(ADC_TypeDef* adc_instance);
#if defined(IOIF_MCU_SERIES_G4)
static inline IOIF_ADC_Method_t _DetectMethodG4(ADC_HandleTypeDef* hadc);
#endif

/* [신규] Manual Init 관련 Private 함수 */
static void _EnableAdcClock(ADC_TypeDef* adc_instance);
static void _EnableDmaClock(void);
static void _EnableAdcNvic(ADC_TypeDef* adc_instance);
static IRQn_Type _GetDmaIrqFromInstance(void* dma_instance);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

AGRBStatusDef IOIF_ADC_AssignInstance(IOIF_ADCx_t* id, ADC_HandleTypeDef* hadc)
{
    if (_instance_count >= IOIF_ADC_MAX_INSTANCES) return AGRBStatus_NO_RESOURCE;
    if (id == NULL || hadc == NULL) return AGRBStatus_PARAM_ERROR;

    // 이미 할당되었는지 검사
    for (uint32_t i = 0; i < _instance_count; i++) {
        if (_instances[i].hadc == hadc) {
            *id = i; // 기존 ID 반환
            return AGRBStatus_BUSY; // Already assigned
        }
    }

    // 새 인스턴스 할당
    memset(&_instances[_instance_count], 0, sizeof(IOIF_ADC_Instance_t));
    IOIF_ADC_Instance_t* instance = &_instances[_instance_count];
    instance->hadc = hadc;
    instance->channel = hadc->Init.NbrOfConversion;
#if defined(IOIF_MCU_SERIES_H7)
    instance->method = _ConvertMethod(hadc->Init.ConversionDataManagement);
#elif defined(IOIF_MCU_SERIES_G4)
    instance->method = _DetectMethodG4(hadc);
#endif
    instance->resolution = _ConvertResolution(hadc->Init.Resolution);

    memset(&_adc_dma_normal_buffer[_instance_count], 0, sizeof(uint16_t) * IOIF_ADC_MAX_CHANNEL);

#if defined(USE_FREERTOS)
    instance->handle = xSemaphoreCreateMutex();
    instance->dma = xSemaphoreCreateBinary();

    if (instance->handle == NULL || instance->dma == NULL) {
        if (instance->handle != NULL) vSemaphoreDelete(instance->handle);
        if (instance->dma != NULL) vSemaphoreDelete(instance->dma);
        instance->handle = NULL;
        instance->dma = NULL;
        memset(instance, 0, sizeof(IOIF_ADC_Instance_t));
        return AGRBStatus_SEMAPHORE_ERROR;
    }

    /* Method vs CubeMX 설정 정합성 검증 */
    if (instance->method == IOIF_ADC_Method_DmaNormal) {
    #if defined(IOIF_MCU_SERIES_H7)
        if (hadc->Init.ConversionDataManagement == ADC_CONVERSIONDATA_DMA_CIRCULAR) {
            vSemaphoreDelete(instance->handle);
            vSemaphoreDelete(instance->dma);
            memset(instance, 0, sizeof(IOIF_ADC_Instance_t));
            return AGRBStatus_NOT_ALLOWED;
        }
    #elif defined(IOIF_MCU_SERIES_G4)
        if (hadc->Init.DMAContinuousRequests == ENABLE) {
            vSemaphoreDelete(instance->handle);
            vSemaphoreDelete(instance->dma);
            memset(instance, 0, sizeof(IOIF_ADC_Instance_t));
            return AGRBStatus_NOT_ALLOWED;
        }
    #endif
    }
#endif

    *id = _instance_count;
    _instance_count++;
    instance->assigned = true;

    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_ADC_ReleaseInstance(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    
    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return AGRBStatus_NOT_INITIALIZED;

#if defined(USE_FREERTOS)
    IOIF_ADC_ACQUIRE_DEV_SEMAPHORE(instance); // 진행중인 작업이 끝날 때까지 대기

    if (instance->handle != NULL) {
        vSemaphoreDelete(instance->handle);
        instance->handle = NULL;
    }
    if (instance->dma != NULL) {
        vSemaphoreDelete(instance->dma);
        instance->dma = NULL;
    }
#endif

    memset(instance, 0, sizeof(IOIF_ADC_Instance_t));
    instance->assigned = false;

    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] ADC를 시작합니다 (Polling/DMA Normal 모드용).
 * @details
 * - Circular 모드는 StartCircular()로 이미 시작되므로 이 함수 불필요
 * - Polling 모드: HAL_ADC_Start() 호출
 * - DMA Normal 모드: HAL_ADC_Start_DMA() 호출
 * 
 * @param[in] id ADC 인스턴스 ID
 * @return AGRBStatus_OK: 성공, 그 외: 실패
 */
AGRBStatusDef IOIF_ADC_Start(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    
    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return AGRBStatus_NOT_INITIALIZED;
    if (instance->hadc == NULL) return AGRBStatus_NOT_INITIALIZED;
    
    /* Circular 모드는 StartCircular()로 시작 */
    if (instance->method == IOIF_ADC_Method_DmaCircular) {
        return AGRBStatus_NOT_SUPPORTED;
    }
    
    HAL_StatusTypeDef hal_status = HAL_OK;
    
    if (instance->method == IOIF_ADC_Method_Polling) {
        /* Polling 모드 */
        hal_status = HAL_ADC_Start(instance->hadc);
    } else if (instance->method == IOIF_ADC_Method_DmaNormal) {
        /* DMA Normal 모드 */
        uint16_t* dma_buffer = _adc_dma_normal_buffer[id];
        hal_status = HAL_ADC_Start_DMA(instance->hadc, 
                                       (uint32_t*)dma_buffer, 
                                       instance->channel);
    }
    
    return (hal_status == HAL_OK) ? AGRBStatus_OK : AGRBStatus_ERROR;
}

/**
 * @brief [PUBLIC] ADC를 중지합니다 (Polling/DMA Normal 모드용).
 * @details
 * - Circular 모드는 StopCircular()로 중지
 * - Polling 모드: HAL_ADC_Stop() 호출
 * - DMA Normal 모드: HAL_ADC_Stop_DMA() 호출
 * 
 * @param[in] id ADC 인스턴스 ID
 * @return AGRBStatus_OK: 성공, 그 외: 실패
 */
AGRBStatusDef IOIF_ADC_Stop(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    
    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return AGRBStatus_NOT_INITIALIZED;
    if (instance->hadc == NULL) return AGRBStatus_NOT_INITIALIZED;
    
    /* Circular 모드는 StopCircular()로 중지 */
    if (instance->method == IOIF_ADC_Method_DmaCircular) {
        return AGRBStatus_NOT_SUPPORTED;
    }
    
    HAL_StatusTypeDef hal_status = HAL_OK;
    
    if (instance->method == IOIF_ADC_Method_Polling) {
        /* Polling 모드 */
        hal_status = HAL_ADC_Stop(instance->hadc);
    } else if (instance->method == IOIF_ADC_Method_DmaNormal) {
        /* DMA Normal 모드 */
        hal_status = HAL_ADC_Stop_DMA(instance->hadc);
    }
    
    return (hal_status == HAL_OK) ? AGRBStatus_OK : AGRBStatus_ERROR;
}

AGRBStatusDef IOIF_ADC_GetValue(IOIF_ADCx_t id, IOIF_ADC_ReportData_t* report)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    if (report == NULL) return AGRBStatus_PARAM_ERROR;

    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return AGRBStatus_NOT_INITIALIZED;
    if (instance->hadc == NULL) return AGRBStatus_NOT_INITIALIZED;
    if (instance->channel == 0) return AGRBStatus_PARAM_ERROR;
    if (instance->channel > IOIF_ADC_MAX_CHANNEL) return AGRBStatus_PARAM_ERROR;

    /* ✅ [신규] Circular 모드에서는 GetValue() 사용 불가 */
    if (instance->method == IOIF_ADC_Method_DmaCircular) {
        return AGRBStatus_NOT_SUPPORTED; // ReadCircular()를 사용하세요!
    }

    AGRBStatusDef result = AGRBStatus_OK; 
    
    // 인스턴스(ADC Peripheral) 보호 시작
    IOIF_ADC_ACQUIRE_DEV_SEMAPHORE(instance);
    
    switch(instance->method) 
    {
        case IOIF_ADC_Method_DmaNormal:
        {   
            result = _GetValueDmaNormal(instance, report);
        } break;
        case IOIF_ADC_Method_Polling:
        {
            result = _GetValuePolling(instance, report);
        } break;
        default:
        {
            result = AGRBStatus_NOT_SUPPORTED;
        } break;
    }
    
    // 인스턴스(ADC Peripheral) 보호 해제
    IOIF_ADC_RELEASE_DEV_SEMAPHORE(instance);

    if (result == AGRBStatus_OK) {
        report->channel_count = instance->channel;
        report->resolution = instance->resolution;
    }
    
    return result;
}

// DMA Circular 모드 API 구현
/**
 * @brief [PUBLIC] DMA Circular 모드 시작
 * @details IOIF 내부 버퍼를 사용하여 DMA Circular 모드를 시작합니다.
 */
AGRBStatusDef IOIF_ADC_StartCircular(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;

    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return AGRBStatus_NOT_INITIALIZED;
    if (instance->hadc == NULL) return AGRBStatus_NOT_INITIALIZED;
    if (instance->method != IOIF_ADC_Method_DmaCircular) return AGRBStatus_NOT_SUPPORTED;

    /* DMA Circular은 하드웨어 자율 동작 → RTOS 불필요 */
    uint16_t* circular_buffer = _adc_dma_circular_buffer[id];

    HAL_StatusTypeDef hal_status = HAL_ADC_Start_DMA(
        instance->hadc,
        (uint32_t*)circular_buffer,
        instance->channel
    );

    if (hal_status != HAL_OK) {
        return AGRBStatus_ERROR;
    }

    instance->is_circular_started = true;
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] DMA Circular 모드 중지
 */
AGRBStatusDef IOIF_ADC_StopCircular(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;

    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return AGRBStatus_NOT_INITIALIZED;
    if (!instance->is_circular_started) return AGRBStatus_OK;

    HAL_StatusTypeDef hal_status = HAL_ADC_Stop_DMA(instance->hadc);
    if (hal_status != HAL_OK) {
        return AGRBStatus_ERROR;
    }

    instance->is_circular_started = false;

    return AGRBStatus_OK;
}

/**
 * ============================================================================
 * [신규] DMA Circular 버퍼 직접 접근 API (Zero-copy)
 * ============================================================================
 */

/**
 * @brief [PUBLIC] DMA Circular 버퍼의 읽기 전용 포인터를 반환합니다 (Zero-copy).
 * @details 
 * - ✅ **아키텍처 개선**: IOIF가 버퍼 소유권을 가지되, System Layer는 직접 읽기 가능
 * - ✅ **Zero-copy**: memcpy 없이 직접 읽기 → 최고 성능
 * - ✅ **캡슐화**: System은 IOIF가 관리하는 버퍼의 포인터만 받음
 * - ✅ **Thread-safe**: DMA가 쓰는 동안 CPU가 읽어도 안전 (Non-cacheable RAM)
 * 
 * @param[in] id ADC 인스턴스 ID
 * @return const uint16_t* 버퍼 포인터 (실패 시 NULL)
 * 
 * @usage (System Layer)
 * @code
 * const uint16_t* p_adc2 = IOIF_ADC_GET_CIRCULAR_BUFFER(s_adc2_id);
 * if (p_adc2) {
 *     uint16_t ext_adc_2 = p_adc2[0];  // PA0_C (직접 읽기)
 *     uint16_t ext_adc_4 = p_adc2[1];  // PA1_C (직접 읽기)
 * }
 * @endcode
 * 
 * @note
 * - ⚠️ 반환된 포인터는 **읽기 전용**으로만 사용하세요 (쓰기 금지!)
 * - ⚠️ 버퍼는 DMA가 계속 업데이트합니다 (항상 최신 값)
 * - ⚠️ 10kHz 샘플링 시 값이 매우 빠르게 변하므로 원자적 읽기가 중요
 * 
 * @warning
 * - StartCircular() 호출 전에는 NULL 반환됩니다.
 * - 채널 개수는 `instance->channel`로 확인하세요.
 */
const uint16_t* IOIF_ADC_GetCircularBuffer(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return NULL;

    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return NULL;
    if (!instance->is_circular_started) return NULL;
    if (instance->method != IOIF_ADC_Method_DmaCircular) return NULL;

    /* ✅ 내부 버퍼 포인터 반환 (읽기 전용) */
    return (const uint16_t*)_adc_dma_circular_buffer[id];
}

/**
 *------------------------------------------------------------
 * [PUBLIC] IRQ Handler용 범용 Helper Functions (System Layer ISR 전용)
 *------------------------------------------------------------
 */

/**
 * @brief [범용] ADC HAL 핸들 포인터 반환 (IRQ Handler 전용)
 * @deprecated 이 함수는 곧 제거됩니다. IOIF_ADC_HandleAdcIsr() 사용을 권장합니다.
 */
ADC_HandleTypeDef* IOIF_ADC_GetHandleById(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return NULL;
    
    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned) return NULL;
    
    return instance->hadc;
}

/**
 * @brief [범용] ADC DMA 핸들 포인터 반환 (IRQ Handler 전용)
 * @deprecated 이 함수는 곧 제거됩니다. IOIF_ADC_HandleDmaIsr() 사용을 권장합니다.
 */
DMA_HandleTypeDef* IOIF_ADC_GetDmaHandleById(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return NULL;
    
    /* Manual Init된 인스턴스만 DMA 핸들 반환 */
    IOIF_ADC_ManualInitInfo_t* manual_info = &s_manual_init_info[id];
    if (!manual_info->is_manual_init) return NULL;
    
    return &manual_info->hdma_manual;
}

/**
 * ============================================================================
 * [신규] ISR 완전 캡슐화 API (HAL 타입 완전히 숨김)
 * ============================================================================
 */

/**
 * @brief [범용] ADC DMA 인터럽트 처리 (System Layer ISR에서 호출)
 * @details 
 * - ✅ System Layer는 HAL을 전혀 알 필요 없음
 * - ✅ IOIF가 내부에서 HAL_DMA_IRQHandler() 호출
 */
void IOIF_ADC_HandleDmaIsr(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return;
    
    /* Manual Init된 인스턴스만 처리 */
    IOIF_ADC_ManualInitInfo_t* manual_info = &s_manual_init_info[id];
    if (!manual_info->is_manual_init) return;
    
    /* ✅ IOIF에서만 HAL 호출 (System Layer는 HAL 타입조차 모름) */
    HAL_DMA_IRQHandler(&manual_info->hdma_manual);
}

/**
 * @brief [범용] ADC 인터럽트 처리 (System Layer ISR에서 호출)
 * @details 
 * - ✅ System Layer는 HAL을 전혀 알 필요 없음
 * - ✅ IOIF가 내부에서 HAL_ADC_IRQHandler() 호출
 */
void IOIF_ADC_HandleAdcIsr(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return;
    
    IOIF_ADC_Instance_t* instance = &_instances[id];
    if (!instance->assigned || instance->hadc == NULL) return;
    
    /* ✅ IOIF에서만 HAL 호출 (System Layer는 HAL 타입조차 모름) */
    HAL_ADC_IRQHandler(instance->hadc);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief DMA Normal 방식으로 ADC 그룹 읽기 (내부 함수)
 * @note Mutex는 상위 함수(IOIF_ADC_GetValue)에서 처리
 */
static AGRBStatusDef _GetValueDmaNormal(IOIF_ADC_Instance_t* instance, IOIF_ADC_ReportData_t* report)
{
    uint16_t* dma_buffer = _adc_dma_normal_buffer[instance - _instances];
    memset(dma_buffer, 0, sizeof(uint16_t) * IOIF_ADC_MAX_CHANNEL);

#if defined(USE_FREERTOS)
    /* === RTOS: Semaphore 기반 DMA 완료 대기 === */
    HAL_StatusTypeDef hal_status;

    /* 이전 변환이 멈췄는지 확인 */
    while (HAL_ADC_GetState(instance->hadc) & HAL_ADC_STATE_REG_BUSY) {
        osDelay(1);
    }

    hal_status = HAL_ADC_Start_DMA(
        instance->hadc,
        (uint32_t*)dma_buffer,
        instance->channel
    );
    if (hal_status != HAL_OK) {
        return AGRBStatus_ERROR;
    }

    /* DMA 완료 시그널 대기 (Binary Semaphore) */
    IOIF_ADC_WAIT_DMA_COMPLETE(instance);

    hal_status = HAL_ADC_Stop(instance->hadc);
    if (hal_status != HAL_OK) {
        return AGRBStatus_ERROR;
    }
#else
    /* === BareMetal: Blocking Polling 기반 DMA 완료 대기 === */
    if (HAL_ADC_Start_DMA(instance->hadc, (uint32_t*)dma_buffer, instance->channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }

    uint32_t tick_start = HAL_GetTick();
    while (HAL_ADC_GetState(instance->hadc) & HAL_ADC_STATE_REG_BUSY) {
        if ((HAL_GetTick() - tick_start) > IOIF_ADC_CONVERSION_TIMEOUT) {
            HAL_ADC_Stop_DMA(instance->hadc);
            return AGRBStatus_TIMEOUT;
        }
    }

    HAL_ADC_Stop(instance->hadc);
#endif

    /* ✅ Cache Invalidate 불필요 (H7: Non-cacheable RAM, G4: D-Cache 없음) */
    for (uint32_t i = 0; i < instance->channel; i++) {
        report->value[i] = (uint32_t)dma_buffer[i];
    }

    return AGRBStatus_OK;
}

/**
 * @brief Polling 방식으로 ADC 그룹 읽기 (내부 함수)
 * @note Mutex는 상위 함수(IOIF_ADC_GetValue)에서 처리
 */
static AGRBStatusDef _GetValuePolling(IOIF_ADC_Instance_t* instance, IOIF_ADC_ReportData_t* report)
{
    if (HAL_ADC_Start(instance->hadc) != HAL_OK) {
        HAL_ADC_Stop(instance->hadc);
        return AGRBStatus_ERROR;
    }

    for (uint32_t i = 0; i < instance->channel; i++) {
        if (HAL_ADC_PollForConversion(instance->hadc, IOIF_ADC_CONVERSION_TIMEOUT) != HAL_OK) {
            HAL_ADC_Stop(instance->hadc);
            return AGRBStatus_TIMEOUT;
        }

        report->value[i] = HAL_ADC_GetValue(instance->hadc);
    }

    if (HAL_ADC_Stop(instance->hadc) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    
    return AGRBStatus_OK;
}

/**
 * @brief HAL의 ADC 해상도 정의를 12, 16 같은 정수로 변환
 */
static inline uint32_t _ConvertResolution(uint32_t stm32_res_raw)
{
    switch (stm32_res_raw) {
        case ADC_RESOLUTION_12B: return 12;
#if defined(IOIF_MCU_SERIES_H7)
        case ADC_RESOLUTION_14B: return 14;
        case ADC_RESOLUTION_16B: return 16;
    #if defined(ADC_VER_V5_X)
        case ADC_RESOLUTION_14B_OPT: return 14;
        case ADC_RESOLUTION_12B_OPT: return 12;
    #endif
#elif defined(IOIF_MCU_SERIES_G4)
        case ADC_RESOLUTION_10B: return 10;
        case ADC_RESOLUTION_8B:  return 8;
        case ADC_RESOLUTION_6B:  return 6;
#endif
        default: return 12;
    }
}

/**
 * @brief Conversion Mode 값을 IOIF enum으로 변환 (H7/G4 공용)
 * @details IOIF_ADC_CONVMODE_* 포터블 매크로 사용
 */
static inline IOIF_ADC_Method_t _ConvertMethod(uint32_t conversion_mode)
{
    if (conversion_mode == IOIF_ADC_CONVMODE_DMA_NORMAL)   return IOIF_ADC_Method_DmaNormal;
    if (conversion_mode == IOIF_ADC_CONVMODE_DMA_CIRCULAR) return IOIF_ADC_Method_DmaCircular;
    return IOIF_ADC_Method_Polling;
}

#if defined(IOIF_MCU_SERIES_G4)
/**
 * @brief [G4] CubeMX 설정에서 ADC 변환 방식 감지
 * @details G4는 ConversionDataManagement가 없으므로
 *          DMA_Handle 연결 여부 + DMAContinuousRequests로 판별
 * @param[in] hadc CubeMX로 초기화된 ADC HAL 핸들
 * @return IOIF_ADC_Method_t 감지된 변환 방식
 */
static inline IOIF_ADC_Method_t _DetectMethodG4(ADC_HandleTypeDef* hadc)
{
    if (hadc->DMA_Handle == NULL) {
        return IOIF_ADC_Method_Polling;
    }
    if (hadc->Init.DMAContinuousRequests == ENABLE) {
        return IOIF_ADC_Method_DmaCircular;
    }
    return IOIF_ADC_Method_DmaNormal;
}
#endif

/**
 * @brief HAL 핸들 포인터로 IOIF 인스턴스를 찾음 (콜백용)
 */
static inline IOIF_ADC_Instance_t* _FindHandler(ADC_HandleTypeDef* hadc)
{
    if (hadc == NULL) return NULL;

    for (uint32_t i = 0; i < _instance_count; i++) {
        if (_instances[i].assigned && _instances[i].hadc == hadc) {
            return &_instances[i];
        }
    }
    return NULL;
}

/**
 * @brief ADC Instance를 배열 인덱스로 변환 (User Callback 매핑용)
 * @return 0~4: ADC1~5, 0xFF: Unknown
 */
static inline uint8_t _GetAdcIndex(ADC_TypeDef* adc_instance)
{
    if (adc_instance == ADC1) return 0;
    if (adc_instance == ADC2) return 1;
#if defined(ADC3)
    if (adc_instance == ADC3) return 2;
#endif
#if defined(ADC4)
    if (adc_instance == ADC4) return 3;
#endif
#if defined(ADC5)
    if (adc_instance == ADC5) return 4;
#endif
    return 0xFF;  // Unknown
}

/**
 *------------------------------------------------------------
 * USER CALLBACK API
 *------------------------------------------------------------
 */

/**
 * @brief [PUBLIC] ADC 변환 완료 User Callback 등록
 */
AGRBStatusDef IOIF_ADC_RegisterCallback(IOIF_ADCx_t id, IOIF_ADC_UserCallback_t callback)
{
    if (id >= IOIF_ADC_MAX_INSTANCES) return AGRBStatus_PARAM_ERROR;
    if (!_instances[id].assigned) return AGRBStatus_PARAM_ERROR;
    
    ADC_HandleTypeDef* hadc = _instances[id].hadc;
    if (hadc == NULL) return AGRBStatus_PARAM_ERROR;
    
    uint8_t adc_index = _GetAdcIndex(hadc->Instance);
    if (adc_index >= IOIF_ADC_MAX_PERIPHERALS) return AGRBStatus_PARAM_ERROR;
    
    s_user_callbacks[adc_index] = callback;
    return AGRBStatus_OK;
}

/**
 *------------------------------------------------------------
 * CALLBACK FUNCTIONS (HAL)
 *------------------------------------------------------------
 */

/**
 * @brief [HAL Callback] DMA 전송이 완료되면 호출됨
 * @details
 * - IOIF 내부 처리 (DMA Normal 모드) 수행
 * - User Callback 체인 호출 (System Layer)
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    /* ✅ [1] IOIF 내부 처리 (DMA Normal 모드) */
    IOIF_ADC_Instance_t* instance = _FindHandler(hadc);
    if (instance != NULL && instance->method == IOIF_ADC_Method_DmaNormal) {
        IOIF_ADC_SIGNAL_DMA_COMPLETE(instance);
    }
    
    /* ✅ [2] User Callback 호출 (System Layer, 예: ADC3 Double Buffering) */
    uint8_t adc_index = _GetAdcIndex(hadc->Instance);
    if (adc_index < IOIF_ADC_MAX_PERIPHERALS && s_user_callbacks[adc_index] != NULL) {
        s_user_callbacks[adc_index](hadc);
    }
}

/**
 * @brief [HAL Callback] (미사용)
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    UNUSED(hadc);
}

/**
 * @brief [HAL Callback] (미사용)
 */
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
    UNUSED(hadc);
}

/**
 * @brief [HAL Callback] ADC 에러 발생 시 호출됨
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    IOIF_ADC_Instance_t* instance = _FindHandler(hadc);
    IOIF_ADC_SIGNAL_DMA_COMPLETE(instance);
}

/**
 *------------------------------------------------------------
 * [신규] ADC Manual Init API 구현
 *------------------------------------------------------------
 */

/**
 * @brief [PRIVATE] ADC Clock 활성화
 */
static void _EnableAdcClock(ADC_TypeDef* adc_instance)
{
    if (adc_instance == NULL) return;
    
#if defined(ADC1)
    if (adc_instance == ADC1) __HAL_RCC_ADC12_CLK_ENABLE();
#endif
#if defined(ADC2)
    else if (adc_instance == ADC2) __HAL_RCC_ADC12_CLK_ENABLE();
#endif
#if defined(ADC3)
    else if (adc_instance == ADC3) {
        #if defined(IOIF_MCU_SERIES_H7)
        __HAL_RCC_ADC3_CLK_ENABLE();
        #elif defined(IOIF_MCU_SERIES_G4)
        __HAL_RCC_ADC345_CLK_ENABLE();
        #endif
    }
#endif
#if defined(ADC4)
    else if (adc_instance == ADC4) __HAL_RCC_ADC345_CLK_ENABLE();
#endif
#if defined(ADC5)
    else if (adc_instance == ADC5) __HAL_RCC_ADC345_CLK_ENABLE();
#endif
}

/**
 * @brief [PRIVATE] DMA Clock 활성화
 */
static void _EnableDmaClock(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
#if defined(DMA2)
    __HAL_RCC_DMA2_CLK_ENABLE();
#endif
}

/**
 * @brief [PRIVATE] DMA Instance로부터 IRQ 번호 반환 (범용)
 * @note System Layer가 지정한 DMA Stream/Channel에서 IRQ 추출
 */
static IRQn_Type _GetDmaIrqFromInstance(void* dma_instance)
{
    if (dma_instance == NULL) return 0;
    
#if defined(IOIF_MCU_SERIES_H7)
    /* H7: DMA Stream → IRQ 매핑 */
    DMA_Stream_TypeDef* stream = (DMA_Stream_TypeDef*)dma_instance;
    
    if (stream == DMA1_Stream0) return DMA1_Stream0_IRQn;
    else if (stream == DMA1_Stream1) return DMA1_Stream1_IRQn;
    else if (stream == DMA1_Stream2) return DMA1_Stream2_IRQn;
    else if (stream == DMA1_Stream3) return DMA1_Stream3_IRQn;
    else if (stream == DMA1_Stream4) return DMA1_Stream4_IRQn;
    else if (stream == DMA1_Stream5) return DMA1_Stream5_IRQn;
    else if (stream == DMA1_Stream6) return DMA1_Stream6_IRQn;
    else if (stream == DMA1_Stream7) return DMA1_Stream7_IRQn;
    else if (stream == DMA2_Stream0) return DMA2_Stream0_IRQn;
    else if (stream == DMA2_Stream1) return DMA2_Stream1_IRQn;
    else if (stream == DMA2_Stream2) return DMA2_Stream2_IRQn;
    else if (stream == DMA2_Stream3) return DMA2_Stream3_IRQn;
    else if (stream == DMA2_Stream4) return DMA2_Stream4_IRQn;
    else if (stream == DMA2_Stream5) return DMA2_Stream5_IRQn;
    else if (stream == DMA2_Stream6) return DMA2_Stream6_IRQn;
    else if (stream == DMA2_Stream7) return DMA2_Stream7_IRQn;
    
#elif defined(IOIF_MCU_SERIES_G4)
    /* G4: DMA Channel → IRQ 매핑 */
    DMA_Channel_TypeDef* channel = (DMA_Channel_TypeDef*)dma_instance;
    
    if (channel == DMA1_Channel1) return DMA1_Channel1_IRQn;
    else if (channel == DMA1_Channel2) return DMA1_Channel2_IRQn;
    else if (channel == DMA1_Channel3) return DMA1_Channel3_IRQn;
    else if (channel == DMA1_Channel4) return DMA1_Channel4_IRQn;
    else if (channel == DMA1_Channel5) return DMA1_Channel5_IRQn;
    else if (channel == DMA1_Channel6) return DMA1_Channel6_IRQn;
#if defined(DMA1_Channel7)
    else if (channel == DMA1_Channel7) return DMA1_Channel7_IRQn;
#endif
#if defined(DMA1_Channel8)
    else if (channel == DMA1_Channel8) return DMA1_Channel8_IRQn;
#endif
#if defined(DMA2)
    else if (channel == DMA2_Channel1) return DMA2_Channel1_IRQn;
    else if (channel == DMA2_Channel2) return DMA2_Channel2_IRQn;
    else if (channel == DMA2_Channel3) return DMA2_Channel3_IRQn;
    else if (channel == DMA2_Channel4) return DMA2_Channel4_IRQn;
    else if (channel == DMA2_Channel5) return DMA2_Channel5_IRQn;
#if defined(DMA2_Channel6)
    else if (channel == DMA2_Channel6) return DMA2_Channel6_IRQn;
#endif
#if defined(DMA2_Channel7)
    else if (channel == DMA2_Channel7) return DMA2_Channel7_IRQn;
#endif
#if defined(DMA2_Channel8)
    else if (channel == DMA2_Channel8) return DMA2_Channel8_IRQn;
#endif
#endif /* DMA2 */
#endif
    
    return 0;  /* Unknown */
}

/**
 * @brief [PRIVATE] ADC NVIC 활성화
 * @note H7/G4 통합: ADC IRQ 번호 차이 흡수
 */
static void _EnableAdcNvic(ADC_TypeDef* adc_instance)
{
    IRQn_Type adc_irq = 0;
    
#if defined(IOIF_MCU_SERIES_H7)
    /* H7: ADC1/2 공통, ADC3 별도 */
    if (adc_instance == ADC1 || adc_instance == ADC2) {
        adc_irq = ADC_IRQn;
    }
#if defined(ADC3)
    else if (adc_instance == ADC3) {
        adc_irq = ADC3_IRQn;
    }
#endif
#elif defined(IOIF_MCU_SERIES_G4)
    /* G4: ADC1/2 공통, ADC3/4/5 별도 */
    if (adc_instance == ADC1 || adc_instance == ADC2) {
        adc_irq = ADC1_2_IRQn;
    }
#if defined(ADC3) && defined(ADC3_IRQn)
    else if (adc_instance == ADC3) {
        adc_irq = ADC3_IRQn;
    }
#endif
#if defined(ADC4) && defined(ADC4_IRQn)
    else if (adc_instance == ADC4) {
        adc_irq = ADC4_IRQn;
    }
#endif
#if defined(ADC5) && defined(ADC5_IRQn)
    else if (adc_instance == ADC5) {
        adc_irq = ADC5_IRQn;
    }
#endif
#endif
    
    if (adc_irq != 0) {
        HAL_NVIC_SetPriority(adc_irq, 5, 0);
        HAL_NVIC_EnableIRQ(adc_irq);
    }
}

/**
 * @brief [PUBLIC] ADC를 수동으로 초기화합니다 (범용 API).
 */
AGRBStatusDef IOIF_ADC_InitManual(
    ADC_TypeDef* adc_instance,
    const IOIF_ADC_ManualConfig_t* config,
    IOIF_ADCx_t* out_id
)
{
    if (adc_instance == NULL || config == NULL || out_id == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }
    if (_instance_count >= IOIF_ADC_MAX_INSTANCES) {
        return AGRBStatus_NO_RESOURCE;
    }
    
    /* [1] 인스턴스 확장 정보 초기화 */
    uint32_t new_id = _instance_count;
    IOIF_ADC_ManualInitInfo_t* manual_info = &s_manual_init_info[new_id];
    memset(manual_info, 0, sizeof(IOIF_ADC_ManualInitInfo_t));
    manual_info->is_manual_init = true;
    
    /* [2] ADC Clock 활성화 */
    _EnableAdcClock(adc_instance);
    
    /* [3] DMA 초기화 */
    bool is_circular = (config->conversion_mode == IOIF_ADC_CONVMODE_DMA_CIRCULAR);
    
    /* ✅ DMA 활성화 여부 확인 */
    if (!config->enable_dma) {
        /* DMA 없이 초기화 (Polling 모드) */
        /* ADC HAL 핸들 초기화로 스킵 */
    } else {
        /* ✅ System Layer가 지정한 DMA 정보 사용 (고정 맵핑 제거!) */
#if defined(IOIF_MCU_SERIES_H7)
        if (config->dma_stream == NULL) {
            return AGRBStatus_PARAM_ERROR;  /* DMA Stream 필수 */
        }
        manual_info->hdma_manual.Instance = (DMA_Stream_TypeDef*)config->dma_stream;
#elif defined(IOIF_MCU_SERIES_G4)
        if (config->dma_channel == NULL) {
            return AGRBStatus_PARAM_ERROR;  /* DMA Channel 필수 */
        }
        manual_info->hdma_manual.Instance = (DMA_Channel_TypeDef*)config->dma_channel;
#endif
        
        /* DMA Clock 활성화 */
        _EnableDmaClock();
        
        /* DMA 설정 */
        manual_info->hdma_manual.Init.Request = config->dma_request;
        manual_info->hdma_manual.Init.Direction = DMA_PERIPH_TO_MEMORY;
        manual_info->hdma_manual.Init.PeriphInc = DMA_PINC_DISABLE;
        manual_info->hdma_manual.Init.MemInc = DMA_MINC_ENABLE;
        manual_info->hdma_manual.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        manual_info->hdma_manual.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        manual_info->hdma_manual.Init.Mode = is_circular ? DMA_CIRCULAR : DMA_NORMAL;
        manual_info->hdma_manual.Init.Priority = DMA_PRIORITY_HIGH;
        
#if defined(IOIF_MCU_SERIES_H7)
        /* H7 전용: FIFO 설정 (Stream Mode) */
        manual_info->hdma_manual.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
        manual_info->hdma_manual.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
        manual_info->hdma_manual.Init.MemBurst = DMA_MBURST_SINGLE;
        manual_info->hdma_manual.Init.PeriphBurst = DMA_PBURST_SINGLE;
#endif
        
        if (HAL_DMA_Init(&manual_info->hdma_manual) != HAL_OK) {
            return AGRBStatus_ERROR;
        }
        
        /* DMA NVIC 활성화 */
        IRQn_Type dma_irq = _GetDmaIrqFromInstance(manual_info->hdma_manual.Instance);
        if (dma_irq != 0) {
            HAL_NVIC_SetPriority(dma_irq, config->dma_irq_priority, 0);
            HAL_NVIC_EnableIRQ(dma_irq);
        }
    }
    
    /* [4] ADC HAL 핸들 초기화 */
    ADC_HandleTypeDef* hadc = &manual_info->hadc_manual;
    hadc->Instance = adc_instance;
    hadc->Init.ClockPrescaler = config->clock_prescaler;
    hadc->Init.Resolution = config->resolution;
    hadc->Init.ScanConvMode = config->scan_mode ? ADC_SCAN_ENABLE : ADC_SCAN_DISABLE;
    hadc->Init.EOCSelection = ADC_EOC_SEQ_CONV;
    hadc->Init.LowPowerAutoWait = DISABLE;
    hadc->Init.ContinuousConvMode = config->continuous_mode ? ENABLE : DISABLE;
    hadc->Init.NbrOfConversion = 0;  /* 초기값 0, AddChannel()로 증가 */
    hadc->Init.DiscontinuousConvMode = DISABLE;
    hadc->Init.ExternalTrigConv = config->external_trigger;
    hadc->Init.ExternalTrigConvEdge = config->trigger_edge;
    hadc->Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;  /* ✅ ADC1/2와 동일하게 설정 */
    
#if defined(IOIF_MCU_SERIES_H7)
    /* ===== H7 전용 설정 ===== */
    /* IOIF_ADC_CONVMODE_* 매크로는 H7에서 ADC_CONVERSIONDATA_* 와 동일 값 */
    hadc->Init.ConversionDataManagement = config->conversion_mode;
    hadc->Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    hadc->Init.OversamplingMode = config->oversampling_enable ? ENABLE : DISABLE;
    
    #ifdef ADC_OVERSAMPLING_RATIO_16  /* Oversampling 매크로가 정의된 경우만 사용 */
    if (config->oversampling_enable) {
        /* ADC3는 다른 oversampling 매크로 사용 (STM32H7) */
        #ifdef ADC3_OVERSAMPLING_RATIO_16
        if (adc_instance == ADC3) {
            hadc->Init.Oversampling.Ratio = ADC3_OVERSAMPLING_RATIO_16;
        } else
        #endif
        {
            hadc->Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_16;
        }
        hadc->Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
        hadc->Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
        hadc->Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
    }
    #else
    /* Oversampling 매크로가 정의되지 않은 HAL 버전 - Disable 강제 */
    hadc->Init.OversamplingMode = DISABLE;
    (void)config->oversampling_enable;  /* 경고 제거 */
    #endif
    
#elif defined(IOIF_MCU_SERIES_G4)
    /* ===== G4 전용 설정 ===== */
    /* G4는 DataAlign, DMAContinuousRequests 필드 사용 */
    hadc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc->Init.GainCompensation = 0;
    
    /* DMA 설정 */
    if (config->conversion_mode == IOIF_ADC_CONVMODE_DMA_CIRCULAR) {
        hadc->Init.DMAContinuousRequests = ENABLE;
    } else if (config->conversion_mode == IOIF_ADC_CONVMODE_DMA_NORMAL) {
        hadc->Init.DMAContinuousRequests = DISABLE;
    } else {
        /* Polling 모드 */
        hadc->Init.DMAContinuousRequests = DISABLE;
    }
    
    /* Oversampling (G4도 지원) */
    hadc->Init.OversamplingMode = config->oversampling_enable ? ENABLE : DISABLE;
    if (config->oversampling_enable) {
        hadc->Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_16;
        hadc->Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
        hadc->Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
        hadc->Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
    }
#endif
    
    /* DMA 연결 */
    __HAL_LINKDMA(hadc, DMA_Handle, manual_info->hdma_manual);
    
    if (HAL_ADC_Init(hadc) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    
    /* [5] NVIC 활성화 (ADC + DMA) */
    _EnableAdcNvic(adc_instance);
    
    /* [6] IOIF 인스턴스 풀에 등록 */
    IOIF_ADC_Instance_t* instance = &_instances[new_id];
    memset(instance, 0, sizeof(IOIF_ADC_Instance_t));
    instance->hadc = hadc;
    instance->channel = 0;  /* AddChannel()로 설정 */
    instance->method = _ConvertMethod(config->conversion_mode);
    instance->resolution = _ConvertResolution(config->resolution);
    instance->assigned = true;
    
#if defined(USE_FREERTOS)
    instance->handle = xSemaphoreCreateMutex();
    instance->dma = xSemaphoreCreateBinary();
    if (instance->handle == NULL || instance->dma == NULL) {
        return AGRBStatus_SEMAPHORE_ERROR;
    }
#endif
    
    *out_id = new_id;
    _instance_count++;
    
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] ADC에 채널을 동적으로 추가합니다 (범용 API).
 */
AGRBStatusDef IOIF_ADC_AddChannel(
    IOIF_ADCx_t id,
    uint32_t channel,
    uint32_t sampling_time
)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    
    IOIF_ADC_ManualInitInfo_t* manual_info = &s_manual_init_info[id];
    if (!manual_info->is_manual_init) {
        return AGRBStatus_NOT_SUPPORTED;  /* CubeMX 초기화된 인스턴스 */
    }
    if (manual_info->channel_count >= IOIF_ADC_MAX_CHANNEL) {
        return AGRBStatus_NO_RESOURCE;
    }
    
    /* 채널 설정 저장 (실제 적용은 ReconfigureChannels()에서) */
    uint8_t idx = manual_info->channel_count;
    manual_info->channels[idx].channel = channel;
    manual_info->channels[idx].rank = idx + 1;
    manual_info->channels[idx].sampling_time = sampling_time;
    manual_info->channels[idx].is_configured = true;
    manual_info->channel_count++;
    
    /* IOIF 인스턴스 채널 카운트 업데이트 */
    _instances[id].channel = manual_info->channel_count;
    
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] ADC에서 특정 채널을 제거합니다 (범용 API).
 * @details
 * - 이미 추가된 채널을 동적으로 제거합니다.
 * - 제거 후 반드시 IOIF_ADC_ReconfigureChannels()를 호출해야 합니다.
 */
AGRBStatusDef IOIF_ADC_RemoveChannel(IOIF_ADCx_t id, uint32_t channel)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    
    IOIF_ADC_ManualInitInfo_t* manual_info = &s_manual_init_info[id];
    if (!manual_info->is_manual_init) {
        return AGRBStatus_NOT_SUPPORTED;  /* CubeMX 초기화된 인스턴스 */
    }
    
    /* [1] 제거할 채널 찾기 */
    int8_t remove_index = -1;
    for (uint8_t i = 0; i < manual_info->channel_count; i++) {
        if (manual_info->channels[i].is_configured && 
            manual_info->channels[i].channel == channel) {
            remove_index = i;
            break;
        }
    }
    
    if (remove_index == -1) {
        return AGRBStatus_PARAM_ERROR;  /* 채널이 존재하지 않음 */
    }
    
    /* [2] 채널 배열에서 제거 (뒤쪽 채널들을 앞으로 당김) */
    for (uint8_t i = remove_index; i < manual_info->channel_count - 1; i++) {
        manual_info->channels[i] = manual_info->channels[i + 1];
        /* Rank 재조정 (1-based index) */
        if (manual_info->channels[i].is_configured) {
            manual_info->channels[i].rank = i + 1;
        }
    }
    
    /* [3] 마지막 채널 정보 초기화 */
    memset(&manual_info->channels[manual_info->channel_count - 1], 0, 
           sizeof(IOIF_ADC_ChannelConfig_t));
    
    /* [4] 채널 카운트 감소 */
    manual_info->channel_count--;
    
    /* IOIF 인스턴스 채널 카운트 업데이트 */
    _instances[id].channel = manual_info->channel_count;
    
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] ADC 채널 설정을 완료하고 DMA를 재시작합니다 (범용 API).
 */
AGRBStatusDef IOIF_ADC_ReconfigureChannels(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    
    IOIF_ADC_ManualInitInfo_t* manual_info = &s_manual_init_info[id];
    if (!manual_info->is_manual_init) {
        return AGRBStatus_NOT_SUPPORTED;
    }
    if (manual_info->channel_count == 0) {
        return AGRBStatus_PARAM_ERROR;
    }
    
    ADC_HandleTypeDef* hadc = &manual_info->hadc_manual;
    
    /* [1] ADC/DMA 중지 (이미 시작된 경우) */
    HAL_ADC_Stop_DMA(hadc);
    
    /* ✅ CRITICAL FIX: ADC를 완전히 Disable 후 Deinit
     * - STM32H7에서 ADC가 활성화된 상태에서는 채널 재설정 불가
     * - ADC를 완전히 끄고 다시 초기화해야 함
     * 
     * - STM32G4도 동일한 제약 존재 (HAL 권장 사항)
     */
    /* ADC Disable & Deinit */
    if (HAL_ADC_DeInit(hadc) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    
    /* NbrOfConversion 업데이트 */
    hadc->Init.NbrOfConversion = manual_info->channel_count;
    
    /* ADC 재초기화 (채널 설정 전에 호출!) */
    if (HAL_ADC_Init(hadc) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    
    /* [2] 모든 채널 설정 적용 */
    /* ✅ CRITICAL FIX: HAL 버그 우회 - 직접 레지스터 설정 (STM32H7 ADC3 전용)
     * 
     * 문제:
     * - STM32H7 ADC3에서 HAL_ADC_ConfigChannel()을 순차 호출 시 SQR 설정 오류
     * - DeInit 후에도 PCSEL/SQR이 초기화되지 않는 경우 있음
     * 
     * 해결책:
     * - STM32H7 ADC3만 직접 레지스터 제어
     * - 나머지 (H7 ADC1/2, G4 모든 ADC)는 HAL 사용
     */
#if defined(IOIF_MCU_SERIES_H7) && defined(ADC3)
    /* ✅ STM32H7 ADC3 전용: 직접 레지스터 제어 (PCSEL 필요) */
    if (hadc->Instance == ADC3) {
        /* Step 2-1: PCSEL 초기화 (Pre-Channel Selection for ADC3 only) */
        hadc->Instance->PCSEL = 0;  // 모든 채널 비활성화
        
        /* Step 2-2: SQR 레지스터 초기화 */
        hadc->Instance->SQR1 = 0;
        hadc->Instance->SQR2 = 0;
        hadc->Instance->SQR3 = 0;
        hadc->Instance->SQR4 = 0;
        
        /* Step 2-3: SQR1의 L 필드 설정 (conversion count) */
        hadc->Instance->SQR1 = ((uint32_t)(manual_info->channel_count - 1) << ADC_SQR1_L_Pos);
        
        /* Step 2-4: 각 채널별로 PCSEL + SQR 설정 */
        for (uint8_t i = 0; i < manual_info->channel_count; i++) {
            if (!manual_info->channels[i].is_configured) continue;
            
            uint32_t ll_channel = manual_info->channels[i].channel;
            uint32_t rank = manual_info->channels[i].rank;
            uint32_t sampling_time = manual_info->channels[i].sampling_time;
            
            /* ① PCSEL 설정: 채널 번호 추출 */
            uint32_t channel_num = __LL_ADC_CHANNEL_TO_DECIMAL_NB(ll_channel);
            hadc->Instance->PCSEL |= (1UL << channel_num);
            
            /* ② SQR 설정: Rank에 채널 번호 할당 */
            uint32_t channel_bits = __LL_ADC_CHANNEL_TO_DECIMAL_NB(ll_channel);
            
            if (rank <= 4) {
                hadc->Instance->SQR1 |= (channel_bits << (ADC_SQR1_SQ1_Pos + (rank - 1) * 6));
            } else if (rank <= 9) {
                hadc->Instance->SQR2 |= (channel_bits << (ADC_SQR2_SQ5_Pos + (rank - 5) * 6));
            } else if (rank <= 14) {
                hadc->Instance->SQR3 |= (channel_bits << (ADC_SQR3_SQ10_Pos + (rank - 10) * 6));
            } else {
                hadc->Instance->SQR4 |= (channel_bits << (ADC_SQR4_SQ15_Pos + (rank - 15) * 6));
            }
            
            /* ③ SMPRx 설정: Sampling Time */
            if (channel_bits <= 9) {
                uint32_t smpr_pos = channel_bits * 3;
                hadc->Instance->SMPR1 &= ~(0x7UL << smpr_pos);
                hadc->Instance->SMPR1 |= (sampling_time << smpr_pos);
            } else {
                uint32_t smpr_pos = (channel_bits - 10) * 3;
                hadc->Instance->SMPR2 &= ~(0x7UL << smpr_pos);
                hadc->Instance->SMPR2 |= (sampling_time << smpr_pos);
            }
        }
    } else
#endif
    /* ✅ 나머지 모든 경우: 표준 HAL 사용 (H7 ADC1/2, G4 모든 ADC) */
    {
        for (uint8_t i = 0; i < manual_info->channel_count; i++) {
            if (!manual_info->channels[i].is_configured) continue;
            
            ADC_ChannelConfTypeDef sConfig = {0};
            sConfig.Channel = manual_info->channels[i].channel;
            sConfig.Rank = manual_info->channels[i].rank;
            sConfig.SamplingTime = manual_info->channels[i].sampling_time;
            sConfig.SingleDiff = ADC_SINGLE_ENDED;
            sConfig.OffsetNumber = ADC_OFFSET_NONE;
            sConfig.Offset = 0;
            
            /* H7 Rev.V 전용 필드 (G4에는 존재하지 않음) */
#if defined(ADC_VER_V5_X)
            sConfig.OffsetRightShift = DISABLE;
            sConfig.OffsetSignedSaturation = DISABLE;
#endif
            
            if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK) {
                return AGRBStatus_ERROR;
            }
        }
    }
    
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] ADC Calibration을 실행합니다 (범용 API).
 * @note H7/G4 통합: Calibration API 차이 흡수
 */
AGRBStatusDef IOIF_ADC_Calibrate(IOIF_ADCx_t id)
{
    if (id >= _instance_count) return AGRBStatus_PARAM_ERROR;
    
    IOIF_ADC_ManualInitInfo_t* manual_info = &s_manual_init_info[id];
    if (!manual_info->is_manual_init) {
        return AGRBStatus_NOT_SUPPORTED;
    }
    
    ADC_HandleTypeDef* hadc = &manual_info->hadc_manual;
    
#if defined(IOIF_MCU_SERIES_H7)
    /* H7: ADC_CALIB_OFFSET 매개변수 필요 */
    if (HAL_ADCEx_Calibration_Start(hadc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
#elif defined(IOIF_MCU_SERIES_G4)
    /* G4: ADC_CALIB_OFFSET 매개변수 없음 */
    if (HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
#endif
    
    return AGRBStatus_OK;
}

#endif /* AGRB_IOIF_ADC_ENABLE */
