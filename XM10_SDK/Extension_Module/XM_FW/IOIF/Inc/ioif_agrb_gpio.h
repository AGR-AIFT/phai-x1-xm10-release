/**
 ******************************************************************************
 * @file    ioif_agrb_gpio.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] GPIO 추상화 드라이버
 * @details HAL 드라이버를 래핑하여 GPIO의 입출력 및 EXTI를 관리합니다.
 * - STM32 HAL GPIO 드라이버를 래핑(wrapping)합니다.
 * - '인스턴스 풀' 기반으로 GPIO 핀에 고유 ID(IOIF_GPIOx_t)를 할당하여 관리합니다.
 * - IOIF_GPIO_INITIALIZE 매크로를 통해 핀을 쉽게 등록하고 사용할 수 있습니다.
 * - EXTI(외부 인터럽트) 콜백을 핀 인덱스 기반으로 관리합니다.
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_GPIO_ENABLE)

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_GPIO_H_
#define IOIF_INC_IOIF_AGRB_GPIO_H_

#include <stdint.h>
#include <stdbool.h>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_gpio.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief GPIO 핀에 할당되는 고유 ID(핸들) 타입입니다.
 * @details 실제로는 _gpio_instances 배열의 인덱스입니다.
 */
typedef uint32_t IOIF_GPIOx_t;

/**
 * @brief 관리할 수 있는 최대 GPIO 인스턴스 개수 (풀 크기)
 */
#define IOIF_GPIO_MAX_INSTANCES     (32)
/**
 * @brief 할당에 실패했거나 유효하지 않은 ID
 */
#define IOIF_GPIO_ID_NOT_ALLOCATED  (0xFFFFFFFF)
#define IOIF_GPIO_NOT_INITIALIZED   (0xFFFFFFFF)

/* ===================================================================
 * 편의 매크로 (Facade Macros)
 * System Layer가 이 매크로들을 사용하여 드라이버에 쉽게 접근할 수 있습니다.
 * =================================================================== */
/**
 * @brief [1] GPIO 포트와 핀을 드라이버에 할당(등록)하고 ID를 받습니다.
 * @param id (출력) 발급받은 IOIF_GPIOx_t 핸들 (ID)
 * @param port (입력) HAL 포트 (예: GPIOA, GPIOB...)
 * @param pin (입력) HAL 핀 (예: GPIO_PIN_0, GPIO_PIN_1...)
 * @param mode (입력) 이 핀의 주 용도 (Input/Output/EXTI)
 */
#define IOIF_GPIO_INITIALIZE(id, port, pin, mode) ioif_gpio_assign_instance(&(id), (port), (pin), (mode))

/**
 * @brief [2] 할당된 핀의 세부 모드(풀업/풀다운/인터럽트)를 재설정합니다.
 * @details HAL_GPIO_Init을 호출하여 핀의 구성을 런타임에 변경합니다.
 * @param id (입력) IOIF_GPIOx_t 핸들
 * @param config (입력) IOIF_GPIO_Initialize_t 구조체 포인터
 */
#define IOIF_GPIO_REINITIALIZE(id, config) ioif_gpio_reinitialize((id), (config))

/**
 * @brief [3] 핀을 HIGH (Set) 상태로 만듭니다.
 */
#define IOIF_GPIO_SET(id) ioif_gpio_set((id))

/**
 * @brief [4] 핀을 LOW (Reset) 상태로 만듭니다.
 */
#define IOIF_GPIO_RESET(id) ioif_gpio_reset((id))

/**
 * @brief [5] 핀의 상태를 반전(Toggle)시킵니다.
 */
#define IOIF_GPIO_TOGGLE(id) ioif_gpio_toggle((id))

/**
 * @brief [6] 핀의 현재 입력 상태를 읽어옵니다.
 * @param id (입력) IOIF_GPIOx_t 핸들
 * @param state (출력) 현재 상태를 저장할 bool 포인터 (true=HIGH, false=LOW)
 */
#define IOIF_GPIO_GET_STATE(id, state) ioif_gpio_get_state((id), (state))

/**
 * @brief [7] 핀의 현재 상태를 반환값으로 직접 읽습니다. (직관적 사용)
 * @details HAL_GPIO_ReadPin의 래퍼입니다. 조건문 등에서 바로 사용하기 좋습니다.
 * @param id (입력) IOIF_GPIOx_t 핸들
 * @return GPIO_PinState (GPIO_PIN_SET=1, GPIO_PIN_RESET=0)
 */
#define IOIF_GPIO_READ(id) ioif_gpio_read_pin((id))

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief EXTI 모드에서 사용할 인터럽트 감지 엣지(Edge) 타입
 */
typedef enum
{
    IOIF_GPIO_InterruptDetectionMode_Rising,       // 상승 엣지
    IOIF_GPIO_InterruptDetectionMode_Falling,      // 하강 엣지
    IOIF_GPIO_InterruptDetectionMode_RisingFalling // 양쪽 엣지
} IOIF_GPIO_InterruptDetectionMode_e;

/**
 * @brief 핀의 주 용도 (assign_instance 시 사용)
 */
typedef enum
{
    IOIF_GPIO_Mode_Input,   // 디지털 입력
    IOIF_GPIO_Mode_Output,  // 디지털 출력
    IOIF_GPIO_Mode_Alternate, // (미사용)
    IOIF_GPIO_Mode_EXTI,    // 외부 인터럽트 입력
} IOIF_GPIO_Mode_e;

/**
 * @brief HAL의 GPIO_Pull과 동일한 내부 풀업/풀다운 설정
 */
typedef enum
{
    IOIF_GPIO_Floating = 0x00U,   /*!< No Pull-up or Pull-down activation  */
    IOIF_GPIO_PullUp = 0x01U,     /*!< Pull-up activation                  */
    IOIF_GPIO_PullDown = 0x02U,   /*!< Pull-down activation                */
} IOIF_GPIO_Pull_e;

/**
 * @brief EXTI 인터럽트 콜백 함수의 원형(prototype)
 */
typedef void (*IOIF_GPIO_Callback_t)(void);

/**
 * @brief ioif_gpio_reinitialize 함수에 전달할 상세 설정 구조체
 */
typedef struct
{
    IOIF_GPIO_Mode_e mode;    // Input / Output / EXTI
    IOIF_GPIO_Pull_e pull;    // Floating / PullUp / PullDown
    bool init_state;          // (Output 모드일 때만) 초기 상태 (true=HIGH, false=LOW)

    // EXTI 모드일 때만 사용되는 설정
    struct {
        IOIF_GPIO_InterruptDetectionMode_e detection_mode;
        IOIF_GPIO_Callback_t callback; // 인터럽트 발생 시 호출될 함수
    } interrupt;

} IOIF_GPIO_Initialize_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief [PUBLIC] GPIO 핀을 IOIF 드라이버에 등록하고 고유 ID를 할당받습니다.
 * @param[out] id      할당받은 ID가 저장될 포인터
 * @param[in]  port    GPIO 포트 (예: GPIOA)
 * @param[in]  pin     GPIO 핀 (예: GPIO_PIN_0)
 * @param[in]  mode    핀의 주 용도
 * @return AGRBStatus_OK (성공), AGRBStatus_BUSY (이미 할당됨), AGRBStatus_ERROR (풀이 가득 참)
 */
AGRBStatusDef ioif_gpio_assign_instance(IOIF_GPIOx_t* id, GPIO_TypeDef* port, uint16_t pin, IOIF_GPIO_Mode_e mode);

/**
 * @brief [PUBLIC] 등록된 GPIO 핀의 상세 구성을 런타임에 변경합니다. (HAL_GPIO_Init 호출)
 * @param[in] id     ioif_gpio_assign_instance로 발급받은 ID
 * @param[in] config IOIF_GPIO_Initialize_t 설정 구조체
 * @return AGRBStatus_OK (성공)
 */
AGRBStatusDef ioif_gpio_reinitialize(IOIF_GPIOx_t id, IOIF_GPIO_Initialize_t* config);

/**
 * @brief [PUBLIC] 핀을 HIGH(Set) 상태로 설정합니다. (Output 모드 전용)
 * @param[in] id     IOIF_GPIOx_t 핸들
 * @return AGRBStatus_OK (성공), AGRBStatus_NOT_ALLOWED (Output 모드가 아님)
 */
AGRBStatusDef ioif_gpio_set(IOIF_GPIOx_t id);

/**
 * @brief [PUBLIC] 핀을 LOW(Reset) 상태로 설정합니다. (Output 모드 전용)
 * @param[in] id     IOIF_GPIOx_t 핸들
 * @return AGRBStatus_OK (성공), AGRBStatus_NOT_ALLOWED (Output 모드가 아님)
 */
AGRBStatusDef ioif_gpio_reset(IOIF_GPIOx_t id);

/**
 * @brief [PUBLIC] 핀의 출력을 반전(Toggle)시킵니다. (Output 모드 전용)
 * @param[in] id     IOIF_GPIOx_t 핸들
 * @return AGRBStatus_OK (성공), AGRBStatus_NOT_ALLOWED (Output 모드가 아님)
 */
AGRBStatusDef ioif_gpio_toggle(IOIF_GPIOx_t id);

/**
 * @brief [PUBLIC] 핀의 현재 입력 상태를 읽습니다. (Input/Output/EXTI 모든 모드에서 가능)
 * @param[in]  id    IOIF_GPIOx_t 핸들
 * @param[out] state 읽어온 상태를 저장할 포인터 (true=HIGH, false=LOW)
 * @return AGRBStatus_OK (성공)
 */
AGRBStatusDef ioif_gpio_get_state(IOIF_GPIOx_t id, bool* state);

/**
 * @brief [추가] 핀의 상태를 즉시 반환합니다 (값 반환형).
 * @param id IOIF 핸들 ID
 * @return 1 (GPIO_PIN_SET) 또는 0 (GPIO_PIN_RESET)
 */
GPIO_PinState ioif_gpio_read_pin(IOIF_GPIOx_t id);

#endif /* AGRB_IOIF_GPIO_ENABLE */

#endif /* IOIF_INC_IOIF_AGRB_GPIO_H_ */
