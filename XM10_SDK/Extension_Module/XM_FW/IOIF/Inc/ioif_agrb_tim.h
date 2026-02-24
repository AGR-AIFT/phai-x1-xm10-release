/**
 ******************************************************************************
 * @file    ioif_agrb_tim.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Oct 30, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_TIM_H_
#define IOIF_INC_IOIF_AGRB_TIM_H_

#include "ioif_agrb_defs.h"

/* STM32 HAL Headers (MCU별 자동 선택) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_tim.h"
    #include "stm32h7xx_hal_tim_ex.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx_hal.h"
	#include "stm32g4xx_hal_tim.h"
	#include "stm32g4xx_hal_tim_ex.h"
#else
    #error "Unsupported MCU series for IOIF TIM"
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define IOIF_TIM_MAX_INSTANCES      (2) // 1ms Main Timer + 여유분
#define IOIF_TIM_NOT_INITIALIZED    (0xFFFFFFFF)
#define IOIF_TIM_INVALID_ID         IOIF_TIM_NOT_INITIALIZED  /* Alias for compatibility */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef uint32_t IOIF_TIMx_t;

/**
 * @brief 타이머 인터럽트 발생 시 호출될 콜백 함수 타입
 */
typedef void (*IOIF_TIM_PeriodElapsedCallback_t)(void);

typedef struct {
    uint32_t                        period_ms;   // (참고용) 설정된 주기
    IOIF_TIM_PeriodElapsedCallback_t callback;   // 주기마다 호출될 함수
} IOIF_TIM_Config_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief  타이머 인스턴스를 할당하고 초기화합니다.
 * @param[out] id      할당된 IOIF 핸들 ID
 * @param[in]  htim    STM32 HAL TIM 핸들
 * @param[in]  config  초기화 설정 (콜백 포함)
 * @return AGRBStatus_OK 성공 시
 */
AGRBStatusDef IOIF_TIM_Assign_Instance(IOIF_TIMx_t* id, TIM_HandleTypeDef* htim, IOIF_TIM_Config_t* config);

/**
 * @brief  타이머 인터럽트(Base_IT)를 시작합니다.
 */
AGRBStatusDef IOIF_TIM_Start_IT(IOIF_TIMx_t id);

/**
 * @brief  타이머 인터럽트를 정지합니다.
 */
AGRBStatusDef IOIF_TIM_Stop_IT(IOIF_TIMx_t id);

/**
 * @brief  시스템 틱(Tick)을 반환합니다. (HAL_GetTick 래퍼)
 */
uint32_t IOIF_TIM_GetTick(void);

/**
 * @brief  Delay 함수 (BareMetal/RTOS 분기 처리됨)
 */
void IOIF_TIM_Delay(uint32_t ms);

/**
 * ============================================================================
 * [신규] 범용 Timer PWM Trigger API (ADC/DAC 트리거용, H7/G4 공용)
 * ============================================================================
 */

/**
 * @brief [범용] CubeMX 생성 Timer를 IOIF에 할당합니다.
 * @details 
 * - CubeMX에서 생성된 Timer HAL 핸들을 IOIF 관리로 전환합니다.
 * - System Layer는 ID만 사용하며 HAL 핸들을 직접 다루지 않습니다.
 * - 모든 STM32H7/G4 프로젝트에서 재사용 가능합니다.
 * 
 * @usage
 * - ADC External Trigger (1kHz ~ 100kHz)
 * - DAC 파형 생성
 * - Multi-channel 동기화 타이밍
 * 
 * @note
 * - ⚠️ 이 함수는 external_io.c 전용이 아닙니다! 범용 API입니다.
 * - ⚠️ 특정 사용 사례(ADC3 트리거)에 종속되지 않습니다.
 * - CubeMX에서 TIM 설정 (TRGO, Frequency)이 완료되어야 합니다.
 * 
 * @param tim_id (출력) 할당된 IOIF TIM ID
 * @param htim CubeMX에서 생성된 TIM_HandleTypeDef 포인터
 * @return AGRBStatusDef
 *         - AGRBStatus_OK: 성공
 *         - AGRBStatus_PARAM_ERROR: htim이 NULL
 *         - AGRBStatus_ERROR: 풀이 가득 참
 * 
 * @example
 * ```c
 * // ADC3를 10kHz로 트리거 (TIM2 사용)
 * @example
 * ```c
 * // system_startup.c (System Layer)
 * extern TIM_HandleTypeDef htim2;  // CubeMX 생성
 * static IOIF_TIMx_t s_tim2_id;
 * 
 * IOIF_TIM_AssignInstance(&s_tim2_id, &htim2);
 * IOIF_TIM_StartBase(s_tim2_id);  // ID로 시작
 * 
 * // external_io.c (System Layer)
 * extern IOIF_TIMx_t g_tim2_id;  // ID만 참조
 * IOIF_TIM_StopBase(g_tim2_id);  // HAL 핸들 몰라도 됨!
 * ```
 */
AGRBStatusDef IOIF_TIM_AssignInstance(IOIF_TIMx_t* tim_id, TIM_HandleTypeDef* htim);

/**
 * @brief [범용] IOIF 관리 Timer를 Base 모드로 시작합니다.
 * @param tim_id IOIF TIM ID
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_StartBase(IOIF_TIMx_t tim_id);

/**
 * @brief [범용] IOIF 관리 Timer를 Base 모드로 정지합니다.
 * @param tim_id IOIF TIM ID
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_StopBase(IOIF_TIMx_t tim_id);

#endif /* IOIF_INC_IOIF_AGRB_TIM_H_ */
