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

#ifndef IOIF_TIM_MAX_INSTANCES
#define IOIF_TIM_MAX_INSTANCES      (4) /**< module.h에서 override 가능 */
#endif
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
 * @brief  할당된 타이머의 콜백 함수를 변경합니다.
 * @param  id       IOIF TIM 핸들 ID
 * @param  callback 새로운 콜백 함수 (NULL 허용 — 콜백 비활성화)
 * @return AGRBStatus_OK 성공 시
 * @note   Assign 후, Start 전/후 언제든 호출 가능.
 *         Composition Root에서 System Init과 App 콜백을 분리할 때 유용.
 */
AGRBStatusDef IOIF_TIM_SetCallback(IOIF_TIMx_t id, IOIF_TIM_PeriodElapsedCallback_t callback);

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

/**
 * ============================================================================
 * [신규] PWM Output API (H7/G4 공용)
 * ============================================================================
 * @details
 * CubeMX에서 설정된 Timer의 PWM 출력을 제어합니다.
 * Device Layer에서 HAL 직접 접근 없이 PWM을 사용할 수 있도록 합니다.
 *
 * [사용 예시 — FES Hub H-Bridge 제어]
 * ```c
 * // system_startup.c (System Layer)
 * IOIF_TIM_AssignInstance(&s_tim1_id, &htim1);
 *
 * // fes_stim_drv.c (Device Layer)
 * IOIF_TIM_PWM_Start(tim1_id, TIM_CHANNEL_1);
 * IOIF_TIM_PWMN_Start(tim1_id, TIM_CHANNEL_1);  // Complementary
 * IOIF_TIM_PWM_SetCompare(tim1_id, TIM_CHANNEL_1, 800);
 * IOIF_TIM_PWM_Stop(tim1_id, TIM_CHANNEL_1);
 * ```
 *
 * [H7/G4 호환성]
 * - HAL_TIM_PWM_Start/Stop: H7, G4 동일 API
 * - HAL_TIMEx_PWMN_Start: H7, G4 동일 API (Complementary output)
 * - __HAL_TIM_SET_COMPARE: H7, G4 동일 매크로
 * - Dead-time: CubeMX 설정 유지 (IOIF에서 변경하지 않음)
 */

/**
 * @brief PWM 출력 시작 (CHx)
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널 (TIM_CHANNEL_1, TIM_CHANNEL_2, ...)
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_PWM_Start(IOIF_TIMx_t tim_id, uint32_t channel);

/**
 * @brief Complementary PWM 출력 시작 (CHxN)
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널 (TIM_CHANNEL_1, TIM_CHANNEL_2, ...)
 * @return AGRBStatusDef
 * @note TIM1, TIM8, TIM15, TIM16, TIM17, TIM20에서만 사용 가능
 */
AGRBStatusDef IOIF_TIM_PWMN_Start(IOIF_TIMx_t tim_id, uint32_t channel);

/**
 * @brief PWM Compare(Duty) 값 설정
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널
 * @param compare CCR 값 (0 ~ ARR)
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_PWM_SetCompare(IOIF_TIMx_t tim_id, uint32_t channel, uint32_t compare);

/**
 * @brief PWM 출력 정지 (CHx)
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_PWM_Stop(IOIF_TIMx_t tim_id, uint32_t channel);

/**
 * @brief Complementary PWM 출력 정지 (CHxN)
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_PWMN_Stop(IOIF_TIMx_t tim_id, uint32_t channel);

/**
 * ============================================================================
 * [신규] PWM/OC Interrupt API + Timer Register Access API
 * ============================================================================
 * @details
 * CC 인터럽트가 필요한 PWM/OC 제어 및 레지스터 접근 API.
 * FES Hub H-Bridge biphasic 제어 등에서 CC 인터럽트 콜백 + 런타임
 * ARR/CCR/OC모드 변경이 필요합니다.
 *
 * [HAL Callback 연동]
 * - PWM_Start_IT → HAL_TIM_PWM_PulseFinishedCallback 활성화
 * - OC_Start_IT  → HAL_TIM_OC_DelayElapsedCallback 활성화
 * - SetOCMode    → 런타임 중 OC 모드 전환 (Forced Inactive ↔ PWM Mode)
 * - GenerateUpdate → UG 이벤트로 shadow register 즉시 적용
 */

/**
 * @brief PWM 출력 시작 + CC 인터럽트 활성화 (CHx)
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널 (TIM_CHANNEL_1, TIM_CHANNEL_2, ...)
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_PWM_Start_IT(IOIF_TIMx_t tim_id, uint32_t channel);

/**
 * @brief Complementary PWM 출력 시작 + CC 인터럽트 활성화 (CHxN)
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_PWMN_Start_IT(IOIF_TIMx_t tim_id, uint32_t channel);

/**
 * @brief Output Compare 시작 + CC 인터럽트 활성화
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_OC_Start_IT(IOIF_TIMx_t tim_id, uint32_t channel);

/**
 * @brief Auto-Reload Register(ARR) 값 읽기
 * @param tim_id IOIF TIM ID
 * @param[out] arr ARR 값 반환
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_GetAutoReload(IOIF_TIMx_t tim_id, uint32_t *arr);

/**
 * @brief Auto-Reload Register(ARR) 값 설정
 * @param tim_id IOIF TIM ID
 * @param arr 새 ARR 값
 * @return AGRBStatusDef
 */
AGRBStatusDef IOIF_TIM_SetAutoReload(IOIF_TIMx_t tim_id, uint32_t arr);

/**
 * @brief Output Compare 모드 변경 (런타임)
 * @param tim_id IOIF TIM ID
 * @param channel HAL 채널 (TIM_CHANNEL_1 ~ TIM_CHANNEL_4)
 * @param mode OC 모드 값 (TIM_OCMODE_PWM1, TIM_OCMODE_FORCED_INACTIVE, ...)
 * @return AGRBStatusDef
 * @note Biphasic 제어 시 Forced Inactive ↔ PWM Mode 전환에 사용
 */
AGRBStatusDef IOIF_TIM_SetOCMode(IOIF_TIMx_t tim_id, uint32_t channel, uint32_t mode);

/**
 * @brief Update Generation (UG) 이벤트 발생
 * @param tim_id IOIF TIM ID
 * @return AGRBStatusDef
 * @note Shadow register(ARR, CCRx)를 즉시 활성 레지스터로 전송
 */
AGRBStatusDef IOIF_TIM_GenerateUpdate(IOIF_TIMx_t tim_id);

/**
 * @brief HAL TIM 핸들로부터 IOIF TIM ID를 역변환
 * @param htim HAL TIM 핸들 포인터 (ISR 콜백에서 받는 값)
 * @return IOIF TIM ID, 못 찾으면 IOIF_TIM_NOT_INITIALIZED
 * @note ISR 콜백(HAL_TIM_OC_DelayElapsedCallback 등)에서 IOIF API 호출 시 사용
 */
IOIF_TIMx_t IOIF_TIM_FindByHandle(const TIM_HandleTypeDef *htim);

#endif /* IOIF_INC_IOIF_AGRB_TIM_H_ */
