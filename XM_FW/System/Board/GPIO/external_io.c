/**
 ******************************************************************************
 * @file    external_io.c
 * @author  HyundoKim
 * @brief   [System Layer] 외부 확장 IO 핀 관리 구현부
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "external_io.h"
#include "ioif_agrb_tim.h"   // For TIM2 Stop/Start in _AddAdc3Channel
#include "ioif_agrb_uart.h"  // For UART Manual Init
#include "uart_rx_handler.h" // For Uart4Rx_XsensIMU_Init
#include <string.h>           // for memcpy, memset

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief ADC 샘플링 주파수 (Hz)
 */
#define EXTERNAL_IO_ADC_SAMPLING_FREQ_HZ    (10000U)  /* 10kHz */

/**
 * @brief ADC 기준 전압 (밀리볼트, STM32H743 VREF+ = 3.3V)
 */
#define EXTERNAL_IO_VREF_MV                 (3300U)

/**
 * @brief ADC1 CubeMX Rank 순서 (USB CC + External ADC)
 */
#define ADC1_RANK_USB_CC1       (0U)  /* PF11: CC1 */
#define ADC1_RANK_USB_CC2       (1U)  /* PF12: CC2 */
#define ADC1_RANK_EXT_ADC_PA0   (2U)  /* PA0: External ADC 1 */
#define ADC1_RANK_EXT_ADC_PA1   (3U)  /* PA1: External ADC 3 */

/**
 * @brief ADC2 CubeMX Rank 순서 (External ADC, Differential)
 */
#define ADC2_RANK_EXT_ADC_PA0C  (0U)  /* PA0_C: External ADC 2 */
#define ADC2_RANK_EXT_ADC_PA1C  (1U)  /* PA1_C: External ADC 4 */

/**
 * @brief ADC3 Channel Mapping (STM32H743XI Datasheet 기준)
 */
#define ADC3_CHANNEL_DIO1_PF3   ADC_CHANNEL_5   /* ADC3_INP5 */
#define ADC3_CHANNEL_DIO2_PF4   ADC_CHANNEL_9   /* ADC3_INP9 */
#define ADC3_CHANNEL_DIO3_PF5   ADC_CHANNEL_4   /* ADC3_INP4 */
#define ADC3_CHANNEL_DIO4_PF6   ADC_CHANNEL_8   /* ADC3_INP8 */
#define ADC3_CHANNEL_DIO5_PF7   ADC_CHANNEL_3   /* ADC3_INP3 */
#define ADC3_CHANNEL_DIO6_PF8   ADC_CHANNEL_7   /* ADC3_INP7 */
#define ADC3_CHANNEL_DIO7_PF9   ADC_CHANNEL_2   /* ADC3_INP2 */
#define ADC3_CHANNEL_DIO8_PF10  ADC_CHANNEL_6   /* ADC3_INP6 */

/**
 * @brief ADC3 DMA 버퍼 크기 (최대 8채널)
 */
#define ADC3_DMA_BUFFER_SIZE    8


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief DIO → ADC3 전환 상태 추적 구조체
 */
typedef struct {
    bool is_switched_to_adc3;   /**< 해당 DIO가 ADC3로 전환되었는지 여부 */
    uint32_t adc_channel;        /**< ADC3 Channel Number (ADC_CHANNEL_x) */
    uint8_t dma_buffer_index;    /**< DMA 버퍼 내 인덱스 (0~7) */
} DioToAdc3State_t;


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

// system_startup에서 주입받을 IOIF 핸들
static IOIF_GPIOx_t s_dio_ids[EXT_DIO_COUNT];
static IOIF_ADCx_t  s_adc1_id = IOIF_ADC_NOT_INITIALIZED;
static IOIF_ADCx_t  s_adc2_id = IOIF_ADC_NOT_INITIALIZED;

/**
 * ============================================================================
 * [신규] ADC1/ADC2/ADC3 DMA Circular 관련 변수 (Zero-copy 직접 읽기)
 * ============================================================================
 */

/* ✅ ADC1 DMA Circular 버퍼 포인터 (IOIF 소유, System은 읽기만) */
static const uint16_t* s_adc1_circular_buffer = NULL;

/* ✅ ADC2 DMA Circular 버퍼 포인터 (IOIF 소유, System은 읽기만) */
static const uint16_t* s_adc2_circular_buffer = NULL;

/* ✅ ADC3 IOIF 핸들 (수동 초기화) */
static IOIF_ADCx_t s_adc3_id = IOIF_ADC_NOT_INITIALIZED;

/* ✅ ADC3 DMA Circular 버퍼 포인터 (IOIF 소유, System은 읽기만) */
static const uint16_t* s_adc3_circular_buffer = NULL;

/* DIO → ADC3 전환 상태 추적 배열 */
static DioToAdc3State_t s_dio_adc3_states[EXT_DIO_COUNT] = {0};

/* ADC3 초기화 완료 플래그 */
static bool s_is_adc3_initialized = false;

/* ADC3 활성 채널 개수 (Rank 순서 추적) */
static uint8_t s_adc3_active_channel_count = 0;

/**
 * ============================================================================
 * ADC Resolution 정규화 (Arduino analogReadResolution 패턴)
 * ============================================================================
 */

/** @brief 출력 resolution (기본 16-bit, SetOutputResolution()으로 변경) */
static uint8_t s_output_resolution = 16;

/** @brief 핀별 네이티브 하드웨어 resolution (bit 수) */
static const uint8_t s_native_resolution[EXT_ADC_COUNT] = {
    [EXT_ADC_1]  = 12,  /* PA0:   ADC1, 12-bit (CubeMX) */
    [EXT_ADC_2]  = 16,  /* PA0_C: ADC2, 16-bit (CubeMX) */
    [EXT_ADC_3]  = 12,  /* PA1:   ADC1, 12-bit (CubeMX) */
    [EXT_ADC_4]  = 16,  /* PA1_C: ADC2, 16-bit (CubeMX) */
    [EXT_ADC_5]  = 16,  /* PF3:   ADC3, 16-bit (Manual Init) */
    [EXT_ADC_6]  = 16,  /* PF4:   ADC3, 16-bit (Manual Init) */
    [EXT_ADC_7]  = 16,  /* PF5:   ADC3, 16-bit (Manual Init) */
    [EXT_ADC_8]  = 16,  /* PF6:   ADC3, 16-bit (Manual Init) */
    [EXT_ADC_9]  = 16,  /* PF7:   ADC3, 16-bit (Manual Init) */
    [EXT_ADC_10] = 16,  /* PF8:   ADC3, 16-bit (Manual Init) */
    [EXT_ADC_11] = 16,  /* PF9:   ADC3, 16-bit (Manual Init) */
    [EXT_ADC_12] = 16,  /* PF10:  ADC3, 16-bit (Manual Init) */
};

static inline uint16_t _NormalizeAdcValue(uint16_t raw, uint8_t native_bits)
{
    if (native_bits == s_output_resolution) return raw;
    if (native_bits < s_output_resolution) {
        return raw << (s_output_resolution - native_bits);
    } else {
        return raw >> (native_bits - s_output_resolution);
    }
}

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ADC3 초기화 (첫 번째 DIO → ADC3 전환 시 자동 호출) */
static bool _InitAdc3(void);

/* ADC3에 새로운 채널 추가 (Rank 동적 설정) */
static bool _AddAdc3Channel(uint32_t adc_channel, uint8_t* out_buffer_index);

/* ADC DMA 버퍼에서 네이티브 raw 값 읽기 (정규화 없이) */
static uint16_t _ReadRawNative(ExternalAdcPin_t pin);


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void ExternalIO_Init(IOIF_GPIOx_t dio_ids[EXT_DIO_COUNT], IOIF_ADCx_t adc1_id, IOIF_ADCx_t adc2_id)
{
    if (dio_ids == NULL || 
        adc1_id == IOIF_ADC_NOT_INITIALIZED || 
        adc2_id == IOIF_ADC_NOT_INITIALIZED) 
    {
        return;
    }

    memcpy(s_dio_ids, dio_ids, sizeof(s_dio_ids));
    s_adc1_id = adc1_id;
    s_adc2_id = adc2_id;

    // 모든 DIO 핀을 기본 'Input Floating' 상태로 초기화
    IOIF_GPIO_Initialize_t default_config = {
        .mode = IOIF_GPIO_Mode_Input,
        .pull = IOIF_GPIO_Floating,
        .init_state = false
    };
    
    for (int i = 0; i < EXT_DIO_COUNT; i++) {
        IOIF_GPIO_REINITIALIZE(s_dio_ids[i], &default_config);
    }
    
    /* ========================================================================
     * [ADC1] USB CC + Ext ADC 통합 (DMA Circular, 10kHz, 12-bit from CubeMX)
     * ======================================================================== */
    /* 1. Calibration */
    IOIF_ADC_Calibrate(s_adc1_id);
    
    /* 2. DMA Circular 시작 */
    IOIF_ADC_START_CIRCULAR(s_adc1_id);

    /* 3. ✅ DMA Circular 버퍼 포인터 획득 (Zero-copy 읽기용) */
    s_adc1_circular_buffer = IOIF_ADC_GET_CIRCULAR_BUFFER(s_adc1_id);
    
    /* ========================================================================
     * [ADC2] Ext ADC 2,4 (DMA Circular, 10kHz, 16-bit from CubeMX)
     * ======================================================================== */
    /* 1. Calibration */
    IOIF_ADC_Calibrate(s_adc2_id);
    
    /* 2. DMA Circular 시작 (✅ IOIF 내부 버퍼 사용) */
    IOIF_ADC_START_CIRCULAR(s_adc2_id);
    
    /* 3. ✅ DMA Circular 버퍼 포인터 획득 (Zero-copy 읽기용) */
    s_adc2_circular_buffer = IOIF_ADC_GET_CIRCULAR_BUFFER(s_adc2_id);
}

void ExternalIO_SetPinMode(ExternalDioPin_t pin, ExternalPinMode_t mode)
{
    if (pin >= EXT_DIO_COUNT) return;
    if (s_dio_adc3_states[pin].is_switched_to_adc3) return;

    IOIF_GPIO_Initialize_t config;
    
    switch(mode)
    {
        case EXT_IO_MODE_INPUT:
            config.mode = IOIF_GPIO_Mode_Input;
            config.pull = IOIF_GPIO_Floating;
            break;
        case EXT_IO_MODE_INPUT_PULLUP:
            config.mode = IOIF_GPIO_Mode_Input;
            config.pull = IOIF_GPIO_PullUp;
            break;
        case EXT_IO_MODE_INPUT_PULLDOWN:
            config.mode = IOIF_GPIO_Mode_Input;
            config.pull = IOIF_GPIO_PullDown;
            break;
        case EXT_IO_MODE_OUTPUT:
            config.mode = IOIF_GPIO_Mode_Output;
            config.pull = IOIF_GPIO_Floating;
            config.init_state = false; // 기본 LOW 출력
            break;
        default:
            return;
    }

    IOIF_GPIO_REINITIALIZE(s_dio_ids[pin], &config);
}

void ExternalIO_WritePin(ExternalDioPin_t pin, bool state)
{
    if (pin >= EXT_DIO_COUNT) return;
    if (s_dio_adc3_states[pin].is_switched_to_adc3) return;
    
    if (state) {
        IOIF_GPIO_SET(s_dio_ids[pin]);
    } else {
        IOIF_GPIO_RESET(s_dio_ids[pin]);
    }
}

bool ExternalIO_ReadPin(ExternalDioPin_t pin)
{
    if (pin >= EXT_DIO_COUNT) return false;
    if (s_dio_adc3_states[pin].is_switched_to_adc3) return false;
    
    bool state = false;
    IOIF_GPIO_GET_STATE(s_dio_ids[pin], &state);
    return state;
}

/**
 * @brief [System] USB CC 핀의 ADC 값을 읽습니다 (10kHz → 1kHz decimation).
 * @details ✅ 아키텍처 개선: ADC1 DMA Circular로 직접 읽기 (Zero-copy)
 * @note USB CC는 10kHz 샘플링이지만, 여기서는 최신값만 반환 (Task가 1kHz면 자동 decimation)
 */
void ExternalIO_GetUsbCcValues(uint16_t* cc1, uint16_t* cc2)
{
    /* ✅ ADC1 DMA Circular 버퍼에서 직접 읽기 (Zero-copy) */
    if (s_adc1_circular_buffer == NULL) {
        if (cc1) *cc1 = 0;
        if (cc2) *cc2 = 0;
        return;
    }
    
    /* CubeMX Rank 순서: [0]=PF11(CC1), [1]=PF12(CC2) */
    if (cc1) *cc1 = s_adc1_circular_buffer[ADC1_RANK_USB_CC1];  /* PF11 */
    if (cc2) *cc2 = s_adc1_circular_buffer[ADC1_RANK_USB_CC2];  /* PF12 */
}

/**
 * @brief [내부] DMA Circular 버퍼에서 네이티브 raw 값을 읽습니다 (정규화 없음).
 * @details ExternalIO_ReadAdc()와 ExternalIO_ReadAdcMillivolts()에서 공통 사용
 */
static uint16_t _ReadRawNative(ExternalAdcPin_t pin)
{
    /* ========== ADC1 그룹 (PA0, PA1) ========== */
    if (pin == EXT_ADC_1 || pin == EXT_ADC_3)
    {
        if (s_adc1_circular_buffer == NULL) return 0;
        return (pin == EXT_ADC_1) ? s_adc1_circular_buffer[ADC1_RANK_EXT_ADC_PA0]
                                  : s_adc1_circular_buffer[ADC1_RANK_EXT_ADC_PA1];
    }
    /* ========== ADC2 그룹 (PA0_C, PA1_C) ========== */
    if (pin == EXT_ADC_2 || pin == EXT_ADC_4)
    {
        if (s_adc2_circular_buffer == NULL) return 0;
        return (pin == EXT_ADC_2) ? s_adc2_circular_buffer[ADC2_RANK_EXT_ADC_PA0C]
                                  : s_adc2_circular_buffer[ADC2_RANK_EXT_ADC_PA1C];
    }
    /* ========== ADC3 그룹 (DIO 1~8 → ADC3) ========== */
    if (pin >= EXT_ADC_5 && pin <= EXT_ADC_12)
    {
        ExternalDioPin_t dio_pin = (ExternalDioPin_t)(pin - EXT_ADC_5);
        if (!s_dio_adc3_states[dio_pin].is_switched_to_adc3) return 0;
        if (s_adc3_circular_buffer == NULL) return 0;
        return s_adc3_circular_buffer[s_dio_adc3_states[dio_pin].dma_buffer_index];
    }
    return 0;
}

uint16_t ExternalIO_ReadAdc(ExternalAdcPin_t pin)
{
    if (pin >= EXT_ADC_COUNT) return 0;
    uint16_t raw = _ReadRawNative(pin);
    return _NormalizeAdcValue(raw, s_native_resolution[pin]);
}

/**
 * ============================================================================
 * DIO → ADC3 동적 전환 API 구현
 * ============================================================================
 */

/**
 * @brief [PUBLIC] DIO 핀을 ADC3 입력으로 전환합니다.
 */
bool ExternalIO_SwitchDioToAdc3(ExternalDioPin_t dio_pin)
{
    if (dio_pin >= EXT_DIO_COUNT) return false;
    
    /* 1. 이미 ADC3로 전환되었는지 확인 */
    if (s_dio_adc3_states[dio_pin].is_switched_to_adc3) {
        return true;  /* 이미 전환됨 */
    }
    
    /* 2. DIO → ADC3 Channel Mapping */
    uint32_t adc_channel;
    switch (dio_pin) {
        case EXT_DIO_1: adc_channel = ADC3_CHANNEL_DIO1_PF3; break;
        case EXT_DIO_2: adc_channel = ADC3_CHANNEL_DIO2_PF4; break;
        case EXT_DIO_3: adc_channel = ADC3_CHANNEL_DIO3_PF5; break;
        case EXT_DIO_4: adc_channel = ADC3_CHANNEL_DIO4_PF6; break;
        case EXT_DIO_5: adc_channel = ADC3_CHANNEL_DIO5_PF7; break;
        case EXT_DIO_6: adc_channel = ADC3_CHANNEL_DIO6_PF8; break;
        case EXT_DIO_7: adc_channel = ADC3_CHANNEL_DIO7_PF9; break;
        case EXT_DIO_8: adc_channel = ADC3_CHANNEL_DIO8_PF10; break;
        default: return false;
    }
    
    /* 3. 첫 번째 전환 시 ADC3 초기화 */
    if (!s_is_adc3_initialized) {
        if (!_InitAdc3()) {
            return false;  /* ADC3 초기화 실패 */
        }
    }
    
    /* 4. GPIO를 Analog 모드로 전환 (✅ IOIF API 사용) */
    GPIO_TypeDef* port = GPIOF;
    uint16_t pin;
    
    switch (dio_pin) {
        case EXT_DIO_1: pin = GPIO_PIN_3; break;
        case EXT_DIO_2: pin = GPIO_PIN_4; break;
        case EXT_DIO_3: pin = GPIO_PIN_5; break;
        case EXT_DIO_4: pin = GPIO_PIN_6; break;
        case EXT_DIO_5: pin = GPIO_PIN_7; break;
        case EXT_DIO_6: pin = GPIO_PIN_8; break;
        case EXT_DIO_7: pin = GPIO_PIN_9; break;
        case EXT_DIO_8: pin = GPIO_PIN_10; break;
        default: return false;
    }
    
    /* ✅ IOIF API로 GPIO Analog 모드 설정 */
    IOIF_GPIO_DeInitPin(port, pin);
    IOIF_GPIO_SetAnalogMode(port, pin);
    
    /* 5. ADC3에 채널 추가 (Rank 동적 설정) */
    uint8_t buffer_index;
    if (!_AddAdc3Channel(adc_channel, &buffer_index)) {
        return false;
    }
    
    /* 6. 상태 추적 업데이트 */
    s_dio_adc3_states[dio_pin].is_switched_to_adc3 = true;
    s_dio_adc3_states[dio_pin].adc_channel = adc_channel;
    s_dio_adc3_states[dio_pin].dma_buffer_index = buffer_index;
    
    return true;
}

/**
 * @brief [PUBLIC] ADC3로 전환된 DIO 핀의 값을 읽습니다 (실시간 안전, Zero-copy).
 * @details ✅ 아키텍처 개선: IOIF 내부 버퍼를 직접 읽기 (ISR/Seqlock 불필요)
 * - ✅ **Zero-copy**: IOIF 내부 버퍼 포인터로 직접 읽기 (< 1us)
 * - ✅ **실시간 안전**: Blocking 없음, 결정론적
 * - ✅ **완벽한 캡슐화**: IOIF가 버퍼 소유, System은 읽기만
 * 
 * @return ADC 값 (0~65535), 에러 시 0
 *         - 전환 안됨: 0
 *         - 첫 변환 전: 0
 *         - 정상: ADC 값
 */
uint16_t ExternalIO_ReadDioAsAdc3(ExternalDioPin_t dio_pin)
{
    if (dio_pin >= EXT_DIO_COUNT) return 0;
    
    /* 1. ADC3로 전환되지 않았으면 0 반환 */
    if (!s_dio_adc3_states[dio_pin].is_switched_to_adc3) {
        return 0;  // ⚠️ Debug: 전환 안됨!
    }
    
    /* 2. 버퍼 포인터가 아직 없으면 0 반환 */
    if (s_adc3_circular_buffer == NULL) {
        return 0;  // ⚠️ Debug: 버퍼 미준비!
    }
    
    /* 3. ✅ IOIF 내부 버퍼에서 직접 읽기 (Zero-copy, 실시간 안전!) */
    /* 아키텍처 개선: IOIF가 버퍼 소유, System은 읽기만 → 완벽한 캡슐화 */
    uint8_t buffer_idx = s_dio_adc3_states[dio_pin].dma_buffer_index;
    return s_adc3_circular_buffer[buffer_idx];
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief [신규] ADC3 초기화 (10kHz 샘플링 최적화)
 * @details ✅ 모든 HAL 호출을 IOIF API로 대체 (아키텍처 준수)
 * @note TIM2는 system_startup.c에서 CubeMX가 생성하고 HAL_TIM_Base_Start()로 시작됨
 * 
 * ✅ System Layer 책임: DMA Channel 맵핑
 * - XM에서 사용 중인 DMA 현황:
 *   - DMA1_Stream0: ADC1 (USB CC)
 *   - DMA1_Stream1: ADC2 (External ADC)
 *   - DMA2_Stream0/1: UART4 RX/TX
 *   - **DMA2_Stream2: ADC3 (DIO → ADC) ← 여기!**
 */
static bool _InitAdc3(void)
{
    if (s_is_adc3_initialized) return true;
    
    /* ✅ ADC3 수동 초기화 (IOIF API) */
    IOIF_ADC_ManualConfig_t adc_config = {
        .resolution = ADC_RESOLUTION_16B,
        .clock_prescaler = ADC_CLOCK_ASYNC_DIV4,  // 20MHz ADC Clock
        .scan_mode = true,
        .continuous_mode = false,  // Trigger 사용 (폭주 방지)
        .external_trigger = ADC_EXTERNALTRIG_T2_TRGO,  // TIM2 연결 (CubeMX에서 10kHz로 설정됨)
        .trigger_edge = ADC_EXTERNALTRIGCONVEDGE_RISING,
        .conversion_mode = ADC_CONVERSIONDATA_DMA_CIRCULAR,
        .oversampling_enable = false,  // 16-bit면 충분
        
        /* ===== ✅ System Layer가 DMA 정보 지정 ===== */
        .enable_dma = true,
        .dma_stream = DMA2_Stream2,  /* XM 전용: ADC3 → DMA2_Stream2 */
        .dma_request = DMA_REQUEST_ADC3,
        .dma_irq_priority = 5,  /* DMA 인터럽트 우선순위 */
    };
    
    if (IOIF_ADC_InitManual(ADC3, &adc_config, &s_adc3_id) != AGRBStatus_OK) {
        return false;
    }
    
    /* ✅ Calibration (IOIF API) */
    IOIF_ADC_Calibrate(s_adc3_id);
    
    s_is_adc3_initialized = true;
    return true;
}

/**
 * @brief ADC3에 새로운 채널 추가 (동적 Rank 설정)
 * @details ✅ 모든 HAL 호출을 IOIF API로 대체 (아키텍처 준수)
 * @note TIM2는 ADC2/ADC3 공용이므로 Stop/Start 시 주의 필요
 */
static bool _AddAdc3Channel(uint32_t adc_channel, uint8_t* out_buffer_index)
{
    if (s_adc3_active_channel_count >= ADC3_DMA_BUFFER_SIZE) return false;
    
    /* ✅ [1] 타이머 중지 (IOIF API, ADC2/ADC3 공용) */
    IOIF_TIM_StopBase(g_tim2_id);  // ✅ ID만 사용!
    
    /* ✅ [2] ADC 채널 추가 (IOIF API) */
    /* ✅ FSR 센서 실측: ADC2 (387.5 Cycles)로 정상 동작 확인
     * → 실제 FSR 임피던스 < 10kΩ (저임피던스)
     * → 모든 ADC 채널에 387.5 Cycles 적용 가능
     * 
     * 8채널 사용 시 총 시간:
     * (387.5 + 16.5) × 8 = 3232 cycles = 64.64 us < 100us ✅
     */
    if (IOIF_ADC_AddChannel(s_adc3_id, adc_channel, ADC_SAMPLETIME_387CYCLES_5) != AGRBStatus_OK) {
        IOIF_TIM_StartBase(g_tim2_id);  // 실패 시 타이머 재시작
        return false;
    }
    
    /* ✅ [3] ADC 재설정 (IOIF API) */
    if (IOIF_ADC_ReconfigureChannels(s_adc3_id) != AGRBStatus_OK) {
        IOIF_TIM_StartBase(g_tim2_id);  // 실패 시 타이머 재시작
        return false;
    }
    
    /* ✅ [4] DMA Circular 시작 (IOIF API, 내부 버퍼 사용) */
    if (IOIF_ADC_START_CIRCULAR(s_adc3_id) != AGRBStatus_OK) {
        IOIF_TIM_StartBase(g_tim2_id);  // 실패 시 타이머 재시작
        return false;
    }
    
    /* ✅ [5] DMA Circular 버퍼 포인터 획득 (Zero-copy 읽기용) */
    s_adc3_circular_buffer = IOIF_ADC_GET_CIRCULAR_BUFFER(s_adc3_id);
    if (s_adc3_circular_buffer == NULL) {
        IOIF_TIM_StartBase(g_tim2_id);  // 실패 시 타이머 재시작
        return false;
    }
    
    /* ✅ [6] 타이머 재시작 (IOIF API, ADC2/ADC3 공용) */
    if (IOIF_TIM_StartBase(g_tim2_id) != AGRBStatus_OK) {
        return false;
    }
    
    /* [7] 버퍼 인덱스 할당 */
    *out_buffer_index = s_adc3_active_channel_count;
    s_adc3_active_channel_count++;
    
    return true;
}

/**
 *------------------------------------------------------------
 * [PUBLIC] ISR Functions (stm32h7xx_it.c에서 호출, UART4 패턴과 동일)
 *------------------------------------------------------------
 */

/**
 * @brief ADC3 DMA 인터럽트 핸들러 (System Layer)
 * @details ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 */
void System_ISR_ADC3_DMA(void)
{
    /* ✅ 아키텍처 준수: System → IOIF (HAL 타입조차 모름) */
    IOIF_ADC_HandleDmaIsr(s_adc3_id);
}

/**
 * @brief ADC3 인터럽트 핸들러 (System Layer)
 * @details ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 */
void System_ISR_ADC3(void)
{
    /* ✅ 아키텍처 준수: System → IOIF (HAL 타입조차 모름) */
    IOIF_ADC_HandleAdcIsr(s_adc3_id);
}

/**
 *------------------------------------------------------------
 * [PUBLIC] ADC Resolution API
 *------------------------------------------------------------
 */

void ExternalIO_SetOutputResolution(uint8_t bits)
{
    if (bits >= 8 && bits <= 16) {
        s_output_resolution = bits;
    }
}

uint8_t ExternalIO_GetNativeResolution(ExternalAdcPin_t pin)
{
    if (pin >= EXT_ADC_COUNT) return 0;
    return s_native_resolution[pin];
}

uint8_t ExternalIO_GetOutputResolution(void)
{
    return s_output_resolution;
}

bool ExternalIO_IsDioSwitchedToAdc3(ExternalDioPin_t dio_pin)
{
    if (dio_pin >= EXT_DIO_COUNT) return false;
    return s_dio_adc3_states[dio_pin].is_switched_to_adc3;
}

uint16_t ExternalIO_ReadAdcMillivolts(ExternalAdcPin_t pin)
{
    if (pin >= EXT_ADC_COUNT) return 0;
    
    uint16_t raw = _ReadRawNative(pin);
    uint32_t max_val = (1U << s_native_resolution[pin]) - 1U;
    if (max_val == 0) return 0;
    
    return (uint16_t)((uint32_t)raw * EXTERNAL_IO_VREF_MV / max_val);
}

/**
 *------------------------------------------------------------
 * [DEBUG] ADC3 진단 함수 (디버깅용)
 *------------------------------------------------------------
 */

/**
 * @brief [DEBUG] ADC3 상태를 확인합니다.
 */
bool ExternalIO_IsAdc3Initialized(void)
{
    return s_is_adc3_initialized;
}

/**
 * @brief [DEBUG] ADC3 활성 채널 수를 확인합니다.
 */
uint8_t ExternalIO_GetAdc3ChannelCount(void)
{
    return s_adc3_active_channel_count;
}

/**
 * @brief [DEBUG] ADC3 DMA 버퍼 포인터를 확인합니다.
 */
const uint16_t* ExternalIO_GetAdc3Buffer(void)
{
    return s_adc3_circular_buffer;
}

/**
 * ============================================================================
 * [신규] UART4 ISR Functions (ADC3 패턴과 동일)
 * ============================================================================
 */

/* UART4 IOIF ID (ExternalIO_SwitchToUartMode()에서 초기화) */
static IOIF_UARTx_t s_uart4_id = IOIF_UART_ID_NOT_ALLOCATED;

/**
 * @brief UART4 인터럽트 핸들러 (System Layer)
 * @details ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 */
void System_ISR_UART4(void)
{
    /* ✅ 아키텍처 준수: System → IOIF (HAL 타입조차 모름) */
    IOIF_UART_HandleIsr(s_uart4_id);
}

/**
 * @brief UART4 DMA RX 인터럽트 핸들러 (System Layer)
 * @details ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 */
void System_ISR_UART4_RX_DMA(void)
{
    /* ✅ 아키텍처 준수: System → IOIF (HAL 타입조차 모름) */
    IOIF_UART_HandleDmaRxIsr(s_uart4_id);
}

/**
 * @brief UART4 DMA TX 인터럽트 핸들러 (System Layer)
 * @details ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 */
void System_ISR_UART4_TX_DMA(void)
{
    /* ✅ 아키텍처 준수: System → IOIF (HAL 타입조차 모름) */
    IOIF_UART_HandleDmaTxIsr(s_uart4_id);
}

/**
 * ============================================================================
 * [신규] 런타임 핀 변환 API (ADC → UART 동적 전환)
 * ============================================================================
 */

/**
 * @brief [런타임] External IO의 ADC 핀을 UART4(IMU용)로 동적 전환
 * @details 
 * - PA0 (External ADC 1) → UART4_TX
 * - PA1 (External ADC 3) → UART4_RX
 * - ✅ System Layer는 HAL을 전혀 호출하지 않음 (아키텍처 준수)
 * - ✅ IOIF_UART_InitManual()로 GPIO+DMA+NVIC 자동 설정
 * - ✅ ADC1 안전하게 재구성 (USB CC 핀 유지)
 * 
 * @note ADC1 구성 변경
 * - Before: PF11(CC1), PF12(CC2), PA0, PA1 (4채널)
 * - After:  PF11(CC1), PF12(CC2) (2채널, USB CC 전용)
 * 
 * @warning 
 * - 이 함수 호출 후 PA0, PA1은 ADC로 읽을 수 없음
 * - External ADC 1/3 대신 External ADC 2/4(PA0_C, PA1_C)를 사용하세요
 */
bool ExternalIO_SwitchToUartMode(void)
{
    /* ========== [1] ADC1 중지 (안전한 채널 재구성을 위해) ========== */
    /* ADC1은 USB CC 핀(PF11, PF12)과 External ADC 핀(PA0, PA1)을 모두 사용 중
     * PA0, PA1을 UART로 전환하기 전에 ADC1을 먼저 중지해야 안전함
     */
    if (s_adc1_id != IOIF_ADC_NOT_INITIALIZED) {
        IOIF_ADC_STOP(s_adc1_id);
    }
    
    /* ========== [2] ADC1에서 PA0, PA1 채널 제거 ========== */
    /* ⚠️ 문제: ADC1은 CubeMX에서 4채널(CC1, CC2, PA0, PA1)로 초기화되었음
     * ✅ 해결: IOIF_ADC_RemoveChannel() API로 PA0, PA1 채널을 제거
     * 
     * [STM32H743XI ADC1 Channel Mapping]
     * - PA0: ADC1_INP16 (ADC_CHANNEL_16)
     * - PA1: ADC1_INP17 (ADC_CHANNEL_17)
     */
    if (IOIF_ADC_RemoveChannel(s_adc1_id, ADC_CHANNEL_16) != AGRBStatus_OK) {
        /* PA0 채널 제거 실패 시 ADC1 재시작 (복구) */
        if (s_adc1_id != IOIF_ADC_NOT_INITIALIZED) {
            IOIF_ADC_START(s_adc1_id);
        }
        return false;
    }
    
    if (IOIF_ADC_RemoveChannel(s_adc1_id, ADC_CHANNEL_17) != AGRBStatus_OK) {
        /* PA1 채널 제거 실패 시 ADC1 재시작 (복구) */
        if (s_adc1_id != IOIF_ADC_NOT_INITIALIZED) {
            IOIF_ADC_START(s_adc1_id);
        }
        return false;
    }
    
    /* [3] ADC1 채널 재구성 적용 (USB CC 2채널만 유지) */
    if (IOIF_ADC_ReconfigureChannels(s_adc1_id) != AGRBStatus_OK) {
        /* 재구성 실패 시 ADC1 재시작 (원래 상태 복구 시도) */
        if (s_adc1_id != IOIF_ADC_NOT_INITIALIZED) {
            IOIF_ADC_START(s_adc1_id);
        }
        return false;
    }
    
    /* ========== [4] ADC2는 영향 없음 (계속 동작) ========== */
    /* ADC2는 PA0_C, PA1_C (Differential 핀)를 사용하므로 PA0, PA1과 무관 */
    
    /* ========== [5] UART4 초기화 (PA0, PA1 → UART) ========== */
    IOIF_UART_ManualInitConfig_t uart4_config = {
        /* GPIO 설정 */
        .gpio_port = GPIOA,
        .tx_pin = GPIO_PIN_0,  /* PA0: ADC1에서 제거됨 */
        .rx_pin = GPIO_PIN_1,  /* PA1: ADC1에서 제거됨 */
        .alternate_function = GPIO_AF8_UART4,
        
        /* UART 기본 설정 */
        .baudrate = 921600,
        .word_length = UART_WORDLENGTH_8B,
        .stop_bits = UART_STOPBITS_1,
        .parity = UART_PARITY_NONE,
        .enable_fifo = true,
        
        /* DMA RX 설정 */
        .enable_dma_rx = true,
        .dma_rx_stream = DMA2_Stream0,
        .dma_rx_request = DMA_REQUEST_UART4_RX,
        .dma_rx_circular = true,
        
        /* DMA TX 설정 (비활성화) */
        .enable_dma_tx = false,
        .dma_tx_stream = NULL,
        .dma_tx_request = 0,
        
        /* NVIC 우선순위 */
        .uart_irq_priority = 5,
        .dma_rx_irq_priority = 5,
        .dma_tx_irq_priority = 5,
        
        /* Advanced Features */
        .overrun_disable = false,  /* Overrun 감지 활성화 */
        .dma_on_rx_error = true,   /* RX Error 시 DMA 계속 동작 */
    };
    
    if (IOIF_UART_InitManual(UART4, &uart4_config, &s_uart4_id) != AGRBStatus_OK) {
        /* UART4 초기화 실패 시 ADC1 복구 시도 */
        if (s_adc1_id != IOIF_ADC_NOT_INITIALIZED) {
            /* TODO: PA0, PA1 채널 재추가 로직 구현 필요 (롤백) */
            IOIF_ADC_START(s_adc1_id);  /* USB CC 2채널로 재시작 */
        }
        return false;
    }
    
    /* ========== [6] XSENS IMU 수신 핸들러 초기화 ========== */
    /* ✅ uart_rx_handler.c에 이미 구현되어 있음 */
    Uart4Rx_XsensIMU_Init(s_uart4_id);
    
    /* ========== [7] ADC1 재시작 (USB CC 2채널만) ========== */
    /* ✅ PA0, PA1 채널이 제거되었으므로 USB CC 핀만 샘플링됨
     * - s_adc1_report.value[0]: PF11 (USB CC1) ✅ 정상
     * - s_adc1_report.value[1]: PF12 (USB CC2) ✅ 정상
     * - s_adc1_report.value[2/3]: 사용 안함 (채널 제거됨)
     */
    if (s_adc1_id != IOIF_ADC_NOT_INITIALIZED) {
        IOIF_ADC_START(s_adc1_id);
    }
    
    return true;
}
