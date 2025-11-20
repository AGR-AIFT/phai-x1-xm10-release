/**
 ******************************************************************************
 * @file    ioif_agrb_adc.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] ADC 추상화 드라이버 헤더
 * @details
 * - STM32 HAL ADC 드라이버를 래핑(wrapping)합니다.
 * - '인스턴스 풀' 기반으로 ADC 핸들(&hadc)에 고유 ID(IOIF_ADCx_t)를 할당합니다.
 * - DMA One-Shot 모드와 Polling 모드를 모두 지원합니다.
 * - ADC 인스턴스는 '채널 그룹' 단위로 읽어옵니다.
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_ADC_ENABLE)

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_ADC_H_
#define IOIF_INC_IOIF_AGRB_ADC_H_

#include "stm32h743xx.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_adc.h"

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define IOIF_ADC_NOT_INITIALIZED    (0xFFFFFFFF)

#define IOIF_ADC_MAX_INSTANCES              (2) // 최대 ADC peripheral 개수 (예: ADC1, ADC2)
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
#define IOIF_ADC_INITIALIZE(id, hadc) ioif_adc_assign_instance(&(id), (hadc))

/**
 * @brief [2] 할당된 ADC 인스턴스(채널 그룹)의 모든 값을 읽어옵니다.
 * @param id (입력) IOIF_ADCx_t 핸들
 * @param report (출력) IOIF_ADC_ReportData_t 구조체 포인터 (결과 저장)
 */
#define IOIF_ADC_GET_VALUE(id, report) ioif_adc_get_value((id), (report))

/**
 * @brief [3] 할당된 ADC 핸들을 해제합니다. (선택 사항)
 */
#define IOIF_ADC_RELEASE(id) ioif_adc_release_instance((id))

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
 */
typedef enum {
    IOIF_ADC_Method_POLLING = 0,    // HAL_ADC_PollForConversion
    IOIF_ADC_Method_DMAOneShot,   // HAL_ADC_Start_DMA
    //IOIF_ADC_Method_DMALoop, (미지원)
} ioif_agrb_adc_method_e;

typedef ioif_agrb_adc_method_e IOIF_ADC_Method_e;

/**
 * @brief IOIF ADC 드라이버가 관리하는 인스턴스 구조체
 */
typedef struct {
    bool assigned; // 할당 여부
    
    ADC_HandleTypeDef* hadc;    // 원본 HAL 핸들
    uint16_t channel;           // CubeMX에 설정된 총 변환 채널 수 (NbrOfConversion)
    ioif_agrb_adc_method_e method; // 변환 방식
    uint32_t resolution;        // 해상도 (비트 수, 예: 12, 16)

#if defined(USE_FREERTOS_DMA)
    SemaphoreHandle_t handle; // 인스턴스 접근 보호용 Mutex
    SemaphoreHandle_t dma;    // DMA 완료 신호용 Binary Semaphore
#endif
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
AGRBStatusDef ioif_adc_assign_instance(IOIF_ADCx_t* id, ADC_HandleTypeDef* hadc);

/**
 * @brief [PUBLIC] 할당된 ADC 인스턴스를 해제합니다.
 */
AGRBStatusDef ioif_adc_release_instance(IOIF_ADCx_t id);

/**
 * @brief [PUBLIC] ADC 인스턴스(채널 그룹)의 모든 값을 읽어옵니다.
 * @details Polling 또는 DMA One-Shot 방식으로 동작하며, 완료될 때까지 Blocking됩니다.
 */
AGRBStatusDef ioif_adc_get_value(IOIF_ADCx_t id, IOIF_ADC_ReportData_t* report);

#endif /* AGRB_IOIF_ADC_ENABLE */

#endif /* IOIF_INC_IOIF_AGRB_ADC_H_ */
