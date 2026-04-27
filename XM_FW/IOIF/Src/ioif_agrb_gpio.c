/**
 ******************************************************************************
 * @file    ioif_agrb_gpio.c
 * @author  HyundoKim
 * @brief   [IOIF Layer] GPIO 추상화 드라이버 구현부
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

#include "ioif_agrb_gpio.h"
#if defined(AGRB_IOIF_GPIO_ENABLE)

#include <string.h> // for memset

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// #define __USE_DEBRUIJN_SEQUENCE__ // (__builtin_ctz 사용 시 주석 처리)
/**
 * @brief EXTI 인터럽트가 16개 핀(0~15)만 지원함을 명시
 */
#define __IOIF_GPIO_EXTI_MAX_PIN_NUMBER__   (16)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief GPIO 인스턴스 내부 관리 구조체
 */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    IOIF_GPIO_Mode_e mode;

    IOIF_GPIO_Initialize_t config;
} IOIF_GPIO_Instance_t;

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
 * @brief EXTI 콜백 함수 포인터 배열 (Pin 0 ~ 15)
 * @details HAL_GPIO_EXTI_Callback에서 핀 인덱스로 이 배열을 조회합니다.
 */
static IOIF_GPIO_Callback_t _gpio_interrupt_callbacks[__IOIF_GPIO_EXTI_MAX_PIN_NUMBER__] = {0};

/**
 * @brief EXTI 콜백이 등록되었는지 확인하는 플래그 배열
 */
static bool _gpio_interrupt_initialized[__IOIF_GPIO_EXTI_MAX_PIN_NUMBER__] = {0};

/**
 * @brief GPIO 인스턴스 풀 (최대 32개)
 */
static IOIF_GPIO_Instance_t _gpio_instances[IOIF_GPIO_MAX_INSTANCES];

/**
 * @brief 현재 할당된 인스턴스 개수
 */
static uint32_t _gpio_instances_count = 0;

/**
 * @brief 디버깅용: HAL_GPIO_Init에 마지막으로 전달된 설정값
 */
GPIO_InitTypeDef _debug_gpio_reinit;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief GPIO_Pin (0x0001, 0x0002 ...) 비트 마스크를 인덱스 (0, 1...)로 변환
 * @param[in] input (예: GPIO_PIN_4 = 0x0010)
 * @return uint32_t (예: 4)
 */
static inline uint32_t _bit_to_index32(uint32_t input);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief [PUBLIC] GPIO 핀을 IOIF 드라이버에 등록하고 고유 ID를 할당받습니다.
 */
AGRBStatusDef ioif_gpio_assign_instance(IOIF_GPIOx_t* id, GPIO_TypeDef* port, uint16_t pin, IOIF_GPIO_Mode_e mode)
{
    if (id == NULL || port == NULL) return AGRBStatus_PARAM_ERROR;

    *id = IOIF_GPIO_ID_NOT_ALLOCATED;

    if (_gpio_instances_count >= IOIF_GPIO_MAX_INSTANCES) {
        return AGRBStatus_ERROR; // No available instance slots
    }

    // 이미 할당되었는지 검사
    for (IOIF_GPIOx_t i = 0; i < _gpio_instances_count; i++) {
        if (_gpio_instances[i].port == port && _gpio_instances[i].pin == pin) {
            *id = i; // 이미 할당된 ID 반환
            return AGRBStatus_BUSY; // Already assigned 
        }
    }

    // 새 인스턴스 할당
    IOIF_GPIO_Instance_t* instance = &_gpio_instances[_gpio_instances_count];
    memset(instance, 0, sizeof(IOIF_GPIO_Instance_t));
    instance->port = port;
    instance->pin = pin;
    instance->mode = mode;

    *id = _gpio_instances_count++;

    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] 등록된 GPIO 핀의 상세 구성을 런타임에 변경합니다.
 */
AGRBStatusDef ioif_gpio_reinitialize(IOIF_GPIOx_t id, IOIF_GPIO_Initialize_t* config)
{
    if (id >= _gpio_instances_count) return AGRBStatus_ERROR; // Invalid instance ID
    if ( config == NULL ) return AGRBStatus_PARAM_ERROR;

    IOIF_GPIO_Instance_t* instance = &_gpio_instances[id];

    memcpy(&instance->config, config, sizeof(IOIF_GPIO_Initialize_t));

    if ( instance->port == NULL ) return AGRBStatus_ERROR; // Not initialized properly

    // 핀의 현재 설정을 초기화(DeInit)합니다.
    HAL_GPIO_DeInit(instance->port, instance->pin);
    
    // 만약 이전에 EXTI 모드로 등록되었다면, 콜백을 해제합니다.
    if ( instance->mode == IOIF_GPIO_Mode_EXTI ) {
        uint32_t pin_index = _bit_to_index32(instance->pin);
        if ( _gpio_interrupt_initialized[pin_index] ) {
            _gpio_interrupt_callbacks[pin_index] = NULL;
            _gpio_interrupt_initialized[pin_index] = false;
        }
    }

    GPIO_InitTypeDef _init = {0}; 
    
    // 1. Input 또는 Output 모드 설정
    if ( config->mode == IOIF_GPIO_Mode_Input || config->mode == IOIF_GPIO_Mode_Output ) {
        _init.Mode = (config->mode == IOIF_GPIO_Mode_Output) ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
        _init.Pull = (config->pull == IOIF_GPIO_PullUp) ? GPIO_PULLUP : ( (config->pull == IOIF_GPIO_PullDown) ? GPIO_PULLDOWN : GPIO_NOPULL );
        _init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        _init.Alternate = 0;
        _init.Pin = instance->pin;

        memcpy(&_debug_gpio_reinit, &_init, sizeof(GPIO_InitTypeDef));        
    } 
    // 2. EXTI (인터럽트) 모드 설정
    else if ( config->mode == IOIF_GPIO_Mode_EXTI )
    {
        uint32_t pin_index = _bit_to_index32(instance->pin);
        if ( pin_index >= __IOIF_GPIO_EXTI_MAX_PIN_NUMBER__ ) return AGRBStatus_ERROR; // EXTI pin number exceeded
        if ( config->interrupt.callback == NULL ) return AGRBStatus_PARAM_ERROR; // Callback function must be provided

        // 콜백 함수를 static 배열에 등록
        {
            if ( _gpio_interrupt_initialized[pin_index] ) return AGRBStatus_BUSY; // Already initialized
            _gpio_interrupt_callbacks[pin_index] = config->interrupt.callback;
            _gpio_interrupt_initialized[pin_index] = true;
        }

        // 인터럽트 엣지(Edge) 설정
        switch(config->interrupt.detection_mode)
        {
            case IOIF_GPIO_InterruptDetectionMode_Rising:
                _init.Mode = GPIO_MODE_IT_RISING; break;
            case IOIF_GPIO_InterruptDetectionMode_Falling:
                _init.Mode = GPIO_MODE_IT_FALLING; break;
            case IOIF_GPIO_InterruptDetectionMode_RisingFalling:
                _init.Mode = GPIO_MODE_IT_RISING_FALLING; break;
            default:
                return AGRBStatus_PARAM_ERROR; // Invalid EXTI mode
        }
        
        _init.Pull = (config->pull == IOIF_GPIO_PullUp) ? GPIO_PULLUP : ( (config->pull == IOIF_GPIO_PullDown) ? GPIO_PULLDOWN : GPIO_NOPULL );
        _init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        _init.Alternate = 0;
        _init.Pin = instance->pin; 
    }

    // 3. HAL 드라이버를 호출하여 핀 설정을 적용합니다.
    HAL_GPIO_Init(instance->port, &_init);

    // 4. Output 모드일 경우, 사용자가 요청한 초기 상태(High/Low)를 적용합니다.
    if (config->mode == IOIF_GPIO_Mode_Output) {
        if (config->init_state) {
            HAL_GPIO_WritePin(instance->port, instance->pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(instance->port, instance->pin, GPIO_PIN_RESET);
        }
    }

    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] 핀을 HIGH(Set) 상태로 설정합니다.
 */
AGRBStatusDef ioif_gpio_set(IOIF_GPIOx_t id)
{
    if (id >= _gpio_instances_count) return AGRBStatus_ERROR; // Invalid instance ID

    IOIF_GPIO_Instance_t* instance = &_gpio_instances[id];

    if (instance->mode != IOIF_GPIO_Mode_Output) {
        return AGRBStatus_NOT_ALLOWED; // Not an output pin
    }

    HAL_GPIO_WritePin(instance->port, instance->pin, GPIO_PIN_SET);

    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] 핀을 LOW(Reset) 상태로 설정합니다.
 */
AGRBStatusDef ioif_gpio_reset(IOIF_GPIOx_t id)
{
    if (id >= _gpio_instances_count) return AGRBStatus_ERROR; // Invalid instance ID

    IOIF_GPIO_Instance_t* instance = &_gpio_instances[id];

    if (instance->mode != IOIF_GPIO_Mode_Output) {
        return AGRBStatus_NOT_ALLOWED; // Not allowed to reset if not output mode
    }

    HAL_GPIO_WritePin(instance->port, instance->pin, GPIO_PIN_RESET);

    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] 핀의 출력을 반전(Toggle)시킵니다.
 */
AGRBStatusDef ioif_gpio_toggle(IOIF_GPIOx_t id)
{
    if (id >= _gpio_instances_count) return AGRBStatus_ERROR; // Invalid instance ID

    IOIF_GPIO_Instance_t* instance = &_gpio_instances[id];

    if (instance->mode != IOIF_GPIO_Mode_Output) {
        return AGRBStatus_NOT_ALLOWED; // Not allowed to toggle if not output mode
    }

    HAL_GPIO_TogglePin(instance->port, instance->pin);
    
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] 핀의 현재 입력 상태를 읽습니다.
 */
AGRBStatusDef ioif_gpio_get_state(IOIF_GPIOx_t id, bool* state)
{
    if (id >= _gpio_instances_count) return AGRBStatus_ERROR; // Invalid instance ID
    if ( state == NULL ) return AGRBStatus_PARAM_ERROR;

    IOIF_GPIO_Instance_t* instance = &_gpio_instances[id];

    GPIO_PinState pin_state = HAL_GPIO_ReadPin(instance->port, instance->pin);
    *state = (pin_state == GPIO_PIN_SET ? true : false);

    return AGRBStatus_OK;
}

/**
 * @brief 핀 상태 반환 함수 구현
 */
GPIO_PinState ioif_gpio_read_pin(IOIF_GPIOx_t id)
{
    // 1. ID 유효성 검사
    if (id >= _gpio_instances_count) {
        return GPIO_PIN_RESET; // 예외 처리 (기본 Low)
    }

    // 2. HAL 함수 호출하여 값 반환
    IOIF_GPIO_Instance_t* inst = &_gpio_instances[id];
    return HAL_GPIO_ReadPin(inst->port, inst->pin);
}

/**
 * @brief [범용] GPIO 핀을 임시로 Output LOW 모드로 변경
 */
AGRBStatusDef IOIF_GPIO_SetOutputLow(GPIO_TypeDef* port, uint16_t pin)
{
    if (port == NULL) return AGRBStatus_PARAM_ERROR;
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    
    return AGRBStatus_OK;
}

/**
 * @brief [범용] GPIO 핀을 Alternate Function 모드로 복구
 */
AGRBStatusDef IOIF_GPIO_RestoreAFMode(GPIO_TypeDef* port, 
                                       uint16_t pin, 
                                       uint32_t af_number,
                                       uint32_t pull)
{
    if (port == NULL) return AGRBStatus_PARAM_ERROR;
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = pull;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = af_number;
    
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    
    return AGRBStatus_OK;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief GPIO_Pin (0x0001, 0x0002 ...) 비트 마스크를 인덱스 (0, 1...)로 변환
 */
static inline uint32_t _bit_to_index32(uint32_t input)
{
#if defined(__USE_DEBRUIJN_SEQUENCE__)
    // (De Bruijn 시퀀스 로직) ...
    static const int debruijn32_table[32] = { ... };
    const uint32_t debruijn32 = 0x077CB531U;
    return debruijn32_table[((input & -input) * debruijn32) >> 27];
#else
    // GCC/ARM 컴파일러 내장 함수(Built-in function) 사용 (더 효율적)
    return __builtin_ctz(input);
#endif
}

/**
 *------------------------------------------------------------
 * CALLBACK FUNCTIONS (HAL)
 *------------------------------------------------------------
 */

/**
 * @brief [HAL Callback] STM32 HAL이 GPIO EXTI 인터럽트를 감지하면 이 함수를 호출합니다.
 * @details 이 함수는 "약한(__weak)" 정의를 덮어쓰는 "강한(strong)" 정의입니다.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // 0. __builtin_ctz(0) UB 방지
    if (GPIO_Pin == 0) return;

    // 1. 핀 비트(0x0010)를 핀 인덱스(4)로 변환합니다.
    uint32_t pin_index = _bit_to_index32(GPIO_Pin);
    if ( pin_index >= __IOIF_GPIO_EXTI_MAX_PIN_NUMBER__ ) return; // Invalid pin index

    // 2. [버그 수정] 해당 인덱스에 콜백이 등록되어 있으면 "호출"합니다.
    if ( _gpio_interrupt_initialized[pin_index] && _gpio_interrupt_callbacks[pin_index] != NULL )
    {
        _gpio_interrupt_callbacks[pin_index]();
    }
}

/**
 *------------------------------------------------------------
 * [신규] 범용 GPIO Analog Mode API (H7/G4 공용)
 *------------------------------------------------------------
 */

/**
 * @brief [PUBLIC] GPIO 핀을 Analog 모드로 설정합니다 (범용 API).
 * @details
 * - IOIF 인스턴스 풀과 독립적인 정적 유틸리티 함수입니다.
 * - STM32 HAL만 사용하므로 모든 H7/G4 프로젝트에서 재사용 가능합니다.
 * - external_io.c, battery_monitor.c 등 어디서든 호출 가능합니다.
 * 
 * @implementation
 * - HAL_GPIO_Init() 직접 호출 (IOIF 인스턴스 풀 불필요)
 * - GPIO Clock은 이미 활성화되어 있다고 가정 (CubeMX 또는 System Layer에서 처리)
 */
AGRBStatusDef IOIF_GPIO_SetAnalogMode(GPIO_TypeDef* port, uint16_t pin)
{
    if (port == NULL) return AGRBStatus_PARAM_ERROR;
    
    /* GPIO 초기화 구조체 설정 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;  /* ✅ Analog Input */
    GPIO_InitStruct.Pull = GPIO_NOPULL;       /* ADC는 Pull 저항 불필요 */
    
    /* HAL 호출 */
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] GPIO 핀 설정을 해제합니다 (범용 API).
 * @details
 * - HAL_GPIO_DeInit()을 래핑한 범용 API입니다.
 * - IOIF 인스턴스 풀과 독립적입니다.
 */
AGRBStatusDef IOIF_GPIO_DeInitPin(GPIO_TypeDef* port, uint16_t pin)
{
    if (port == NULL) return AGRBStatus_PARAM_ERROR;
    
    /* HAL 호출 */
    HAL_GPIO_DeInit(port, pin);
    
    return AGRBStatus_OK;
}

#endif /* AGRB_IOIF_GPIO_ENABLE */
