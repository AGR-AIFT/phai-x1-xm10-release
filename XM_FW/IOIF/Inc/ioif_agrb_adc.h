/**
 ******************************************************************************
 * @file    ioif_agrb_adc.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] ADC 추상화 드라이버 헤더 (H7/G4 통합)
 * @details
 * - STM32 HAL ADC 드라이버를 래핑(wrapping)합니다.
 * - '인스턴스 풀' 기반으로 ADC 핸들(&hadc)에 고유 ID(IOIF_ADCx_t)를 할당합니다.
 * - DMA One-Shot 모드와 Polling 모드를 모두 지원합니다.
 * - ADC 인스턴스는 '채널 그룹' 단위로 읽어옵니다.
 * - ✅ H7/G4 통합: DMA Stream/Channel, 레지스터 차이 흡수
 * @version 0.2
 * @date    Jan 27, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_ADC_ENABLE)

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_ADC_H_
#define IOIF_INC_IOIF_AGRB_ADC_H_

/* STM32 HAL Headers (MCU별 자동 선택) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h743xx.h"
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_adc.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx.h"
    #include "stm32g4xx_hal.h"
	#include "stm32g4xx_hal_adc.h"
#else
    #error "Unsupported MCU series for IOIF ADC"
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define IOIF_ADC_NOT_INITIALIZED    (0xFFFFFFFF)

#define IOIF_ADC_MAX_INSTANCES              (3) // 최대 ADC peripheral 개수 (ADC1, ADC2, ADC3)
#define IOIF_ADC_MAX_CHANNEL                (8) // 한 인스턴스(그룹)가 읽을 수 있는 최대 채널 수
#define IOIF_ADC_CONVERSION_TIMEOUT         (100U) // Polling/DMA 타임아웃 (ms)

/* ===================================================================
 * 편의 매크로 (Facade Macros)
 * =================================================================== */

/**
 * @brief [1] ADC HAL 핸들(&hadc)을 드라이버에 할당하고 ID를 받습니다.
 * @param id (출력) 발급받은 IOIF_ADCx_t 핸들
 * @param hadc (입력) HAL 핸들 (예: &hadc1)
 */
#define IOIF_ADC_INITIALIZE(id, hadc) IOIF_ADC_AssignInstance(&(id), (hadc))

/**
 * @brief [2] 할당된 ADC 인스턴스(채널 그룹)의 모든 값을 읽어옵니다 (Blocking).
 * @param id (입력) IOIF_ADCx_t 핸들
 * @param report (출력) IOIF_ADC_ReportData_t 구조체 포인터 (결과 저장)
 */
#define IOIF_ADC_GET_VALUE(id, report) IOIF_ADC_GetValue((id), (report))

/**
 * @brief [2-1] ADC를 시작합니다 (Polling/DMA Normal 모드용).
 * @param id (입력) IOIF_ADCx_t 핸들
 */
#define IOIF_ADC_START(id) IOIF_ADC_Start((id))

/**
 * @brief [2-2] ADC를 중지합니다 (Polling/DMA Normal 모드용).
 * @param id (입력) IOIF_ADCx_t 핸들
 */
#define IOIF_ADC_STOP(id) IOIF_ADC_Stop((id))

/**
 * @brief [3] 할당된 ADC 핸들을 해제합니다. (선택 사항)
 */
#define IOIF_ADC_RELEASE(id) IOIF_ADC_ReleaseInstance((id))

/**
 * @brief [4] DMA Circular 모드 시작
 * @note ✅ 아키텍처 개선: 내부 버퍼 사용 (외부 할당 불필요)
 */
#define IOIF_ADC_START_CIRCULAR(id) IOIF_ADC_StartCircular((id))

/**
 * @brief [5] DMA Circular 버퍼 포인터 획득 (Zero-copy 읽기용)
 */
#define IOIF_ADC_GET_CIRCULAR_BUFFER(id) IOIF_ADC_GetCircularBuffer((id))

/**
 * @brief [6] DMA Circular 모드 중지
 */
#define IOIF_ADC_STOP_CIRCULAR(id) IOIF_ADC_StopCircular((id))

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief ADC 핸들에 할당되는 고유 ID 타입
 */
typedef uint32_t IOIF_ADCx_t;

/**
 * @brief ADC 변환 방식 (CubeMX 설정에 따라 자동 감지)
 * 
 * - Polling: CPU가 ADC 완료를 폴링 (Blocking, 저속)
 * - DmaNormal: DMA One-Shot 모드 (Blocking, 고속)
 * - DmaCircular: DMA Circular 모드 (Non-blocking, Timer 트리거, 초고속 10kHz+)
 */
typedef enum {
    IOIF_ADC_Method_Polling = 0,    /**< CPU Polling (Blocking) */
    IOIF_ADC_Method_DmaNormal,      /**< DMA Normal/One-Shot (Blocking) */
    IOIF_ADC_Method_DmaCircular,    /**< DMA Circular (Non-blocking, Timer) */
} IOIF_ADC_Method_t;

/**
 * @brief IOIF ADC 드라이버가 관리하는 인스턴스 구조체
 */
typedef struct {
    bool assigned; // 할당 여부
    
    ADC_HandleTypeDef* hadc;    // 원본 HAL 핸들
    uint16_t channel;           // CubeMX에 설정된 총 변환 채널 수 (NbrOfConversion)
    IOIF_ADC_Method_t method;   // 변환 방식
    uint32_t resolution;        // 해상도 (비트 수, 예: 12, 16)

#if defined(USE_FREERTOS)
    SemaphoreHandle_t handle; // 인스턴스 접근 보호용 Mutex
    SemaphoreHandle_t dma;    // DMA 완료 신호용 Binary Semaphore
#endif

    /* DMA Circular 모드용 변수 */
    bool is_circular_started;   // DMA Circular 시작 여부
} IOIF_ADC_Instance_t;

/**
 * @brief ioif_adc_get_value()가 반환하는 결과 구조체
 */
typedef struct {
    uint32_t resolution;    // 해상도 (비트 수)
    uint32_t channel_count; // 읽어온 채널 개수
    uint32_t value[IOIF_ADC_MAX_CHANNEL]; // 채널 순서대로의 ADC Raw 값

} IOIF_ADC_ReportData_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief [PUBLIC] ADC HAL 핸들을 IOIF 드라이버에 등록하고 고유 ID를 할당받습니다.
 */
AGRBStatusDef IOIF_ADC_AssignInstance(IOIF_ADCx_t* id, ADC_HandleTypeDef* hadc);

/**
 * @brief [PUBLIC] 할당된 ADC 인스턴스를 해제합니다.
 */
AGRBStatusDef IOIF_ADC_ReleaseInstance(IOIF_ADCx_t id);

/**
 * @brief [PUBLIC] ADC 인스턴스(채널 그룹)의 모든 값을 읽어옵니다.
 * @details Polling 또는 DMA Normal 방식으로 동작하며, 완료될 때까지 Blocking됩니다.
 * @note DMA Circular 모드에서는 사용 불가 (GetCircularBuffer로 직접 읽기)
 */
AGRBStatusDef IOIF_ADC_GetValue(IOIF_ADCx_t id, IOIF_ADC_ReportData_t* report);

/**
 * @brief [PUBLIC] ADC를 시작합니다 (Polling/DMA Normal 모드용).
 * @details
 * - Polling 모드: HAL_ADC_Start() 호출
 * - DMA Normal 모드: HAL_ADC_Start_DMA() 호출
 * - Circular 모드는 StartCircular()로 시작 (이 함수 사용 불가)
 * 
 * @param[in] id ADC 인스턴스 ID
 * @return AGRBStatus_OK: 성공, 그 외: 실패
 */
AGRBStatusDef IOIF_ADC_Start(IOIF_ADCx_t id);

/**
 * @brief [PUBLIC] ADC를 중지합니다 (Polling/DMA Normal 모드용).
 * @details
 * - Polling 모드: HAL_ADC_Stop() 호출
 * - DMA Normal 모드: HAL_ADC_Stop_DMA() 호출
 * - Circular 모드는 StopCircular()로 중지 (이 함수 사용 불가)
 * 
 * @param[in] id ADC 인스턴스 ID
 * @return AGRBStatus_OK: 성공, 그 외: 실패
 */
AGRBStatusDef IOIF_ADC_Stop(IOIF_ADCx_t id);

/**
 * ============================================================================
 * DMA Circular 모드 API (10kHz 고속 샘플링 지원)
 * ============================================================================
 */

/**
 * @brief [PUBLIC] DMA Circular 모드를 시작합니다.
 * @details 
 * - ✅ 아키텍처 개선: IOIF 내부 버퍼 사용 (캡슐화 향상)
 * - TIM 트리거 기반 ADC DMA Circular 모드를 시작합니다.
 * - 외부에서 타이머를 시작해야 실제 샘플링이 진행됩니다.
 * - 한 번 시작하면 명시적으로 중지할 때까지 무한 반복합니다.
 * 
 * @param[in] id ADC 인스턴스 ID
 * @return AGRBStatusDef 상태 코드
 * 
 * @note 
 * - 이 함수는 **초기화 단계**에서만 호출하세요.
 * - ✅ 버퍼는 IOIF 내부에서 자동 할당됩니다 (Non-cacheable RAM).
 * - ✅ GetCircularBuffer()로 포인터를 받아 직접 읽으세요 (Zero-copy).
 * - ADC는 CubeMX에서 **DMA Circular + TIM Trigger** 모드로 설정되어야 합니다.
 * 
 * @code
 * // 예제: ADC2 DMA Circular 시작
 * void System_Init(void) {
 *     IOIF_ADC_INITIALIZE(s_adc2_id, &hadc2);
 *     IOIF_ADC_START_CIRCULAR(s_adc2_id);  // ✅ 외부 버퍼 불필요!
 *     HAL_TIM_Base_Start(&htim1);  // 타이머 시작 → 10kHz 샘플링 시작!
 * 
 *     // 포인터 획득
 *     const uint16_t* p_adc2 = IOIF_ADC_GET_CIRCULAR_BUFFER(s_adc2_id);
 *     
 *     // 직접 읽기 (Zero-copy)
 *     uint16_t val = p_adc2[0];  // PA0
 * }
 * @endcode
 */
AGRBStatusDef IOIF_ADC_StartCircular(IOIF_ADCx_t id);

/**
 * @brief [PUBLIC] DMA Circular 모드를 중지합니다.
 * @param[in] id ADC 인스턴스 ID
 * @return AGRBStatusDef 상태 코드
 */
AGRBStatusDef IOIF_ADC_StopCircular(IOIF_ADCx_t id);

/**
 * ============================================================================
 * DMA Circular 버퍼 직접 접근 API (Zero-copy, 아키텍처 개선)
 * ============================================================================
 */

/**
 * @brief [PUBLIC] DMA Circular 버퍼의 읽기 전용 포인터를 반환합니다 (Zero-copy).
 * @details 
 * - ✅ **아키텍처 개선**: IOIF가 버퍼 소유권을 가지되, System Layer는 직접 읽기 가능
 * - ✅ **Zero-copy**: memcpy 없이 직접 읽기 → 최고 성능 (< 1us)
 * - ✅ **캡슐화**: System은 IOIF가 관리하는 버퍼의 포인터만 받음
 * - ✅ **Thread-safe**: DMA가 쓰는 동안 CPU가 읽어도 안전 (Non-cacheable RAM)
 * - ✅ **실시간 안전**: Blocking 없음, 결정론적
 * 
 * @param[in] id ADC 인스턴스 ID
 * @return const uint16_t* 버퍼 포인터 (실패 시 NULL)
 * 
 * @usage (System Layer)
 * @code
 * // ADC2 DMA Circular 시작
 * IOIF_ADC_START_CIRCULAR(s_adc2_id);
 * 
 * // 포인터 획득 (한 번만)
 * const uint16_t* p_adc2 = IOIF_ADC_GET_CIRCULAR_BUFFER(s_adc2_id);
 * 
 * // 실시간 루프 (2ms)
 * void RunUserAlgorithm(void) {
 *     if (p_adc2) {
 *         uint16_t ext_adc_2 = p_adc2[0];  // PA0_C (직접 읽기, Zero-copy)
 *         uint16_t ext_adc_4 = p_adc2[1];  // PA1_C (직접 읽기, Zero-copy)
 *         // 제어 로직
 *     }
 * }
 * @endcode
 * 
 * @note
 * - ⚠️ 반환된 포인터는 **읽기 전용**으로만 사용하세요 (쓰기 금지!)
 * - ⚠️ 버퍼는 DMA가 계속 업데이트합니다 (항상 최신 값)
 * - ⚠️ 10kHz 샘플링 시 값이 매우 빠르게 변하므로 원자적 읽기가 중요
 * - H7: Non-cacheable RAM (MPU) → Cache Invalidate 불필요
 * - G4: D-Cache 없음 → Cache Invalidate 불필요
 * 
 * @warning
 * - StartCircular() 호출 전에는 NULL 반환됩니다.
 * - 다중 채널인 경우 배열 인덱스는 CubeMX Rank 순서를 따릅니다.
 */
const uint16_t* IOIF_ADC_GetCircularBuffer(IOIF_ADCx_t id);

/**
 * ============================================================================
 * User Callback 등록 (System Layer → IOIF 콜백 체인)
 * ============================================================================
 */

/**
 * @brief ADC 변환 완료 User Callback 함수 타입
 * @details HAL_ADC_ConvCpltCallback에서 IOIF 내부 처리 후 호출됩니다.
 * 
 * @param[in] hadc ADC 핸들
 */
typedef void (*IOIF_ADC_UserCallback_t)(ADC_HandleTypeDef* hadc);

/**
 * @brief [PUBLIC] ADC 변환 완료 User Callback을 등록합니다.
 * @details 
 * - System Layer에서 ADC 특정 인스턴스에 대한 추가 처리가 필요할 때 사용합니다.
 * - HAL_ADC_ConvCpltCallback은 IOIF Layer가 소유하며, User Callback을 체인으로 호출합니다.
 * - 예: ADC3 Lock-free Double Buffering (external_io.c)
 * 
 * @param[in] hadc ADC 핸들 (예: &hadc3)
 * @param[in] callback User Callback 함수 포인터
 * @return AGRBStatusDef 상태 코드
 * 
 * @warning
 * - Callback은 ISR 컨텍스트에서 실행됩니다 (빠르게 처리 필수).
 * - Blocking 함수 호출 금지 (osDelay, printf 등).
 * 
 * @code
 * // 예제: ADC3 Double Buffering Callback 등록
 * static void _Adc3ConvCpltCallback(ADC_HandleTypeDef* hadc)
 * {
 *     memcpy(s_read_buffer, s_dma_buffer, 16);
 *     s_data_ready = true;
 * }
 * 
 * void ExternalIO_Init(void)
 * {
 *     IOIF_ADC_InitManual(ADC3, &adc_config, &adc3_id);
 *     IOIF_ADC_RegisterCallback(adc3_id, _Adc3ConvCpltCallback);  // ✅ ID만 사용!
 * }
 * @endcode
 */
AGRBStatusDef IOIF_ADC_RegisterCallback(IOIF_ADCx_t id, IOIF_ADC_UserCallback_t callback);

/**
 * ============================================================================
 * [신규] 범용 ADC Manual Init API (CubeMX 없이 런타임 초기화, H7/G4 공용)
 * ============================================================================
 * 
 * @brief H7/G4 통합 지원
 * @details
 * - ✅ DMA Controller 차이 흡수: H7 DMA Stream vs. G4 DMA Channel + DMAMUX
 * - ✅ ADC 레지스터 차이 흡수: H7 ADC3 PCSEL vs. G4 표준 HAL
 * - ✅ Clock 설정 차이 흡수: H7 ADC12/ADC3 vs. G4 ADC345
 * - ✅ Calibration API 차이 흡수: H7 ADC_CALIB_OFFSET vs. G4 단순 호출
 * - ✅ Cache 차이 흡수: H7 Non-cacheable RAM vs. G4 일반 SRAM
 * 
 * @note
 * - conversion_mode 필드는 IOIF_ADC_CONVMODE_* 포터블 매크로를 사용합니다:
 *   - IOIF_ADC_CONVMODE_POLLING / _DMA_NORMAL / _DMA_CIRCULAR
 *   - H7: 내부적으로 ConversionDataManagement 필드에 직접 설정 (값 동일)
 *   - G4: 내부적으로 DMAContinuousRequests 필드로 변환
 */

/**
 * @brief ADC Conversion Mode 포터블 매크로 (H7/G4 공용)
 * @details
 * - H7: ADC_CONVERSIONDATA_* HAL 매크로와 동일 값 (하위 호환)
 * - G4: HAL에 해당 매크로가 없으므로 IOIF 자체 정의
 * - ManualConfig.conversion_mode 필드에 사용
 */
#if defined(IOIF_MCU_SERIES_H7)
    #define IOIF_ADC_CONVMODE_POLLING       ADC_CONVERSIONDATA_DR
    #define IOIF_ADC_CONVMODE_DMA_NORMAL    ADC_CONVERSIONDATA_DMA_ONESHOT
    #define IOIF_ADC_CONVMODE_DMA_CIRCULAR  ADC_CONVERSIONDATA_DMA_CIRCULAR
#elif defined(IOIF_MCU_SERIES_G4)
    #define IOIF_ADC_CONVMODE_POLLING       (0x00000000UL)
    #define IOIF_ADC_CONVMODE_DMA_NORMAL    (0x00000001UL)
    #define IOIF_ADC_CONVMODE_DMA_CIRCULAR  (0x00000002UL)
#endif

/**
 * @brief ADC 수동 초기화 설정 구조체
 * @details
 * - 모든 필드는 HAL 매크로 사용 (예: ADC_RESOLUTION_12B)
 * - 범용 API이므로 특정 사용 사례에 종속되지 않음
 * - ✅ H7/G4 공용: conversion_mode 필드는 H7/G4 모두 지원
 * - ✅ DMA 정보는 System Layer에서 주입 (IOIF는 고정 맵핑 안함!)
 */
typedef struct {
    uint32_t resolution;           /**< ADC_RESOLUTION_12B, [H7] _14B/_16B, [G4] _10B/_8B/_6B */
    uint32_t clock_prescaler;      /**< ADC_CLOCK_ASYNC_DIV2, _DIV4, _DIV8, ... */
    bool scan_mode;                /**< Multi-channel scan 활성화 여부 */
    bool continuous_mode;          /**< Continuous Conversion 활성화 여부 */
    uint32_t external_trigger;     /**< ADC_EXTERNALTRIG_T1_TRGO, T2_TRGO, T6_TRGO, ... */
    uint32_t trigger_edge;         /**< ADC_EXTERNALTRIGCONVEDGE_RISING, FALLING, BOTH */
    uint32_t conversion_mode;      /**< IOIF_ADC_CONVMODE_POLLING / _DMA_NORMAL / _DMA_CIRCULAR */
    bool oversampling_enable;      /**< Oversampling 활성화 여부 */
    
    /* ===== DMA 설정 (System Layer에서 주입) ===== */
    bool enable_dma;               /**< DMA 사용 여부 */
#if defined(IOIF_MCU_SERIES_H7)
    void* dma_stream;              /**< [H7] DMA_Stream_TypeDef* (예: DMA1_Stream0) */
#elif defined(IOIF_MCU_SERIES_G4)
    void* dma_channel;             /**< [G4] DMA_Channel_TypeDef* (예: DMA1_Channel1) */
#endif
    uint32_t dma_request;          /**< DMA Request (예: DMA_REQUEST_ADC1) */
    uint8_t dma_irq_priority;      /**< DMA 인터럽트 우선순위 (0~15) */
} IOIF_ADC_ManualConfig_t;

/**
 * @brief [범용] ADC를 수동으로 초기화합니다 (CubeMX 없이).
 * @details 
 * - 런타임에 ADC/DMA를 설정합니다.
 * - 모든 STM32H7/G4 프로젝트에서 재사용 가능합니다.
 * - IOIF 인스턴스 풀에 자동으로 등록됩니다.
 * 
 * @usage
 * - Battery Voltage 모니터링 (ADC1, 1kHz, 12-bit)
 * - 고속 센서 데이터 수집 (ADC3, 10kHz, 16-bit)
 * - Multi-channel 동시 샘플링
 * 
 * @note
 * - ⚠️ 이 함수는 external_io.c 전용이 아닙니다! 범용 API입니다.
 * - ⚠️ 특정 사용 사례(DIO → ADC3)에 종속되지 않습니다.
 * - DMA는 자동으로 설정됩니다 (DMA1_Stream4, Circular/OneShot).
 * - Calibration은 별도로 호출해야 합니다 (IOIF_ADC_Calibrate).
 * 
 * @param adc_instance ADC 인스턴스 (예: ADC1, ADC2, ADC3)
 * @param config 설정 구조체 포인터
 * @param out_id (출력) 할당된 IOIF_ADCx_t 핸들 ID
 * @return AGRBStatusDef
 *         - AGRBStatus_OK: 성공
 *         - AGRBStatus_PARAM_ERROR: NULL 포인터 또는 잘못된 설정
 *         - AGRBStatus_ERROR: HAL 초기화 실패
 * 
 * @example
 * @code
 * // ADC3를 16-bit, DMA Circular, TIM2 트리거로 초기화
 * IOIF_ADC_ManualConfig_t config = {
 *     .resolution = ADC_RESOLUTION_16B,
 *     .clock_prescaler = ADC_CLOCK_ASYNC_DIV4,
 *     .scan_mode = true,
 *     .continuous_mode = false,
 *     .external_trigger = ADC_EXTERNALTRIG_T2_TRGO,
 *     .trigger_edge = ADC_EXTERNALTRIGCONVEDGE_RISING,
 *     .conversion_mode = IOIF_ADC_CONVMODE_DMA_CIRCULAR,
 *     .oversampling_enable = false,
 * };
 * 
 * IOIF_ADCx_t adc3_id;
 * IOIF_ADC_InitManual(ADC3, &config, &adc3_id);
 * @endcode
 */
AGRBStatusDef IOIF_ADC_InitManual(
    ADC_TypeDef* adc_instance,
    const IOIF_ADC_ManualConfig_t* config,
    IOIF_ADCx_t* out_id
);

/**
 * @brief [범용] ADC에 채널을 동적으로 추가합니다.
 * @details 
 * - 런타임에 ADC 채널을 추가합니다.
 * - Rank는 자동으로 증가합니다 (내부 카운터 관리).
 * - 모든 STM32H7/G4 프로젝트에서 재사용 가능합니다.
 * 
 * @usage
 * - DIO → ADC 동적 전환
 * - Multi-channel 순차 추가
 * 
 * @note
 * - ⚠️ 채널 추가 후 IOIF_ADC_ReconfigureChannels()를 호출해야 적용됩니다.
 * - ⚠️ ADC/DMA가 이미 시작된 상태에서는 호출하지 마세요 (재설정 필요).
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * @param channel ADC_CHANNEL_0 ~ ADC_CHANNEL_19 (HAL 매크로)
 * @param sampling_time ADC_SAMPLETIME_2CYCLES_5, _8CYCLES_5, _64CYCLES_5, ... (HAL 매크로)
 * @return AGRBStatusDef
 * 
 * @example
 * @code
 * // ADC3에 PF3 (ADC3_INP5) 추가
 * IOIF_ADC_AddChannel(adc3_id, ADC_CHANNEL_5, ADC_SAMPLETIME_64CYCLES_5);
 * @endcode
 */
AGRBStatusDef IOIF_ADC_AddChannel(
    IOIF_ADCx_t id,
    uint32_t channel,
    uint32_t sampling_time
);

/**
 * @brief [범용] ADC에서 특정 채널을 제거합니다 (범용 API).
 * @details 
 * - 이미 추가된 채널을 동적으로 제거합니다.
 * - 모든 STM32H7/G4 프로젝트에서 재사용 가능합니다.
 * - ⚠️ 제거 후 반드시 IOIF_ADC_ReconfigureChannels()를 호출해야 합니다.
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * @param channel ADC 채널 번호 (예: ADC_CHANNEL_0, ADC_CHANNEL_5)
 * @return AGRBStatusDef
 *         - AGRBStatus_OK: 성공
 *         - AGRBStatus_PARAM_ERROR: 잘못된 ID 또는 채널 없음
 *         - AGRBStatus_NOT_SUPPORTED: CubeMX 초기화된 인스턴스 (수동 초기화만 지원)
 * 
 * @example
 * @code
 * // ADC1에서 PA0, PA1 채널 제거 (UART4 전환 시)
 * IOIF_ADC_RemoveChannel(adc1_id, ADC_CHANNEL_0);  // PA0
 * IOIF_ADC_RemoveChannel(adc1_id, ADC_CHANNEL_1);  // PA1
 * IOIF_ADC_ReconfigureChannels(adc1_id);  // 적용!
 * @endcode
 */
AGRBStatusDef IOIF_ADC_RemoveChannel(IOIF_ADCx_t id, uint32_t channel);

/**
 * @brief [범용] ADC 채널 설정을 완료하고 DMA를 재시작합니다.
 * @details 
 * - AddChannel()로 추가한 채널들을 실제로 ADC에 적용합니다.
 * - RemoveChannel()로 제거된 채널을 반영합니다.
 * - 내부적으로 HAL_ADC_Stop_DMA() → Init() → HAL_ADC_Start_DMA() 호출합니다.
 * - 모든 STM32H7/G4 프로젝트에서 재사용 가능합니다.
 * 
 * @usage
 * - 채널 추가/제거 완료 후 호출
 * - DMA Circular 버퍼 재시작
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * @return AGRBStatusDef
 * 
 * @example
 * @code
 * IOIF_ADC_AddChannel(adc3_id, ADC_CHANNEL_5, ADC_SAMPLETIME_64CYCLES_5);
 * IOIF_ADC_AddChannel(adc3_id, ADC_CHANNEL_9, ADC_SAMPLETIME_64CYCLES_5);
 * IOIF_ADC_ReconfigureChannels(adc3_id);  // 적용!
 * @endcode
 */
AGRBStatusDef IOIF_ADC_ReconfigureChannels(IOIF_ADCx_t id);

/**
 * @brief [범용] ADC Calibration을 실행합니다.
 * @details 
 * - ADC 초기화 후 첫 변환 전에 호출해야 합니다.
 * - 모든 STM32H7/G4 프로젝트에서 재사용 가능합니다.
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * @return AGRBStatusDef
 * 
 * @example
 * @code
 * IOIF_ADC_InitManual(ADC3, &config, &adc3_id);
 * IOIF_ADC_Calibrate(adc3_id);  // Calibration
 * IOIF_ADC_AddChannel(adc3_id, ADC_CHANNEL_5, ADC_SAMPLETIME_64CYCLES_5);
 * @endcode
 */
AGRBStatusDef IOIF_ADC_Calibrate(IOIF_ADCx_t id);

/**
 * ============================================================================
 * [신규] 인터럽트 핸들러용 범용 헬퍼 함수 (System Layer ISR 전용)
 * ============================================================================
 */

/**
 * @brief [범용] ADC HAL 핸들 포인터 반환 (IRQ Handler 전용)
 * @details 
 * - System Layer의 ISR에서만 사용합니다 (예: System_ISR_ADC3).
 * - ⚠️ 특정 ADC에 종속되지 않은 범용 API입니다.
 * - ⚠️ Application Layer에서 직접 호출 금지!
 * 
 * @deprecated 이 함수는 곧 제거됩니다. IOIF_ADC_HandleAdcIsr() 사용을 권장합니다.
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * @return ADC_HandleTypeDef* (NULL if invalid ID)
 * 
 * @usage (System Layer)
 * @code
 * void System_ISR_ADC3(void)
 * {
 *     ADC_HandleTypeDef* hadc = IOIF_ADC_GetHandleById(s_adc3_id);
 *     if (hadc != NULL) HAL_ADC_IRQHandler(hadc);
 * }
 * @endcode
 */
ADC_HandleTypeDef* IOIF_ADC_GetHandleById(IOIF_ADCx_t id);

/**
 * @brief [범용] ADC DMA 핸들 포인터 반환 (IRQ Handler 전용)
 * @details 
 * - System Layer의 DMA ISR에서만 사용합니다 (예: System_ISR_ADC3_DMA).
 * - ⚠️ 특정 ADC에 종속되지 않은 범용 API입니다.
 * - ⚠️ Application Layer에서 직접 호출 금지!
 * 
 * @deprecated 이 함수는 곧 제거됩니다. IOIF_ADC_HandleDmaIsr() 사용을 권장합니다.
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * @return DMA_HandleTypeDef* (NULL if invalid ID or not manual init)
 * 
 * @usage (System Layer)
 * @code
 * void System_ISR_ADC3_DMA(void)
 * {
 *     DMA_HandleTypeDef* hdma = IOIF_ADC_GetDmaHandleById(s_adc3_id);
 *     if (hdma != NULL) HAL_DMA_IRQHandler(hdma);
 * }
 * @endcode
 */
DMA_HandleTypeDef* IOIF_ADC_GetDmaHandleById(IOIF_ADCx_t id);

/**
 * ============================================================================
 * [신규] ISR 완전 캡슐화 API (HAL 타입 완전히 숨김, 권장)
 * ============================================================================
 */

/**
 * @brief [범용] ADC DMA 인터럽트 처리 (System Layer ISR에서 호출)
 * @details 
 * - ✅ System Layer는 HAL을 전혀 알 필요 없음
 * - ✅ IOIF가 내부에서 HAL_DMA_IRQHandler() 호출
 * - ✅ 완전한 캡슐화: HAL 타입 노출 안함
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * 
 * @usage (System Layer)
 * @code
 * void System_ISR_ADC3_DMA(void)
 * {
 *     IOIF_ADC_HandleDmaIsr(s_adc3_id);  // ✅ HAL 완전히 숨김
 * }
 * @endcode
 * 
 * @note UART4 패턴과 동일: System_ISR_DMA_UART4_RX_Manual() 참조
 */
void IOIF_ADC_HandleDmaIsr(IOIF_ADCx_t id);

/**
 * @brief [범용] ADC 인터럽트 처리 (System Layer ISR에서 호출)
 * @details 
 * - ✅ System Layer는 HAL을 전혀 알 필요 없음
 * - ✅ IOIF가 내부에서 HAL_ADC_IRQHandler() 호출
 * - ✅ 완전한 캡슐화: HAL 타입 노출 안함
 * 
 * @param id IOIF_ADCx_t 핸들 ID
 * 
 * @usage (System Layer)
 * @code
 * void System_ISR_ADC3(void)
 * {
 *     IOIF_ADC_HandleAdcIsr(s_adc3_id);  // ✅ HAL 완전히 숨김
 * }
 * @endcode
 * 
 * @note UART4 패턴과 동일: System_ISR_UART4_Manual() 참조
 */
void IOIF_ADC_HandleAdcIsr(IOIF_ADCx_t id);

#endif /* AGRB_IOIF_ADC_ENABLE */

#endif /* IOIF_INC_IOIF_AGRB_ADC_H_ */
