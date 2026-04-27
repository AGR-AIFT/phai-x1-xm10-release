/**
 ******************************************************************************
 * @file    ioif_agrb_tim.c
 * @author  HyundoKim
 * @brief   [IOIF Layer] TIMER 하드웨어 추상화 계층 구현
 * @details HAL TIM Base를 감싸서 인터럽트 기반의 주기적 콜백을 지원합니다.
 * @version 0.1
 * @date    Oct 30, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_tim.h"
#include <string.h>

#if defined(USE_FREERTOS)
#include "cmsis_os2.h" // RTOS
#endif

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

typedef struct {
    bool                is_assigned;
    TIM_HandleTypeDef*  htim;
    IOIF_TIM_Config_t   config;
} IOIF_TIM_Instance_t;

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

static IOIF_TIM_Instance_t s_tim_instances[IOIF_TIM_MAX_INSTANCES];
static uint32_t s_tim_instance_count = 0;

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
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* No private functions (모든 로직이 HAL_TIM_PeriodElapsedCallback 내부에 인라인) */

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

AGRBStatusDef IOIF_TIM_Assign_Instance(IOIF_TIMx_t* id, TIM_HandleTypeDef* htim, IOIF_TIM_Config_t* config)
{
    if (id == NULL || htim == NULL || config == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }

    if (s_tim_instance_count >= IOIF_TIM_MAX_INSTANCES) {
        return AGRBStatus_NO_RESOURCE;
    }

    uint32_t new_id = s_tim_instance_count;
    IOIF_TIM_Instance_t* instance = &s_tim_instances[new_id];

    instance->is_assigned = true;
    instance->htim = htim;
    memcpy(&instance->config, config, sizeof(IOIF_TIM_Config_t));

    *id = new_id;
    s_tim_instance_count++;

    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_Start_IT(IOIF_TIMx_t id)
{
    if (id >= s_tim_instance_count || !s_tim_instances[id].is_assigned) {
        return AGRBStatus_PARAM_ERROR;
    }

    if (HAL_TIM_Base_Start_IT(s_tim_instances[id].htim) != HAL_OK) {
        return AGRBStatus_ERROR;
    }

    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_Stop_IT(IOIF_TIMx_t id)
{
    if (id >= s_tim_instance_count || !s_tim_instances[id].is_assigned) {
        return AGRBStatus_PARAM_ERROR;
    }

    if (HAL_TIM_Base_Stop_IT(s_tim_instances[id].htim) != HAL_OK) {
        return AGRBStatus_ERROR;
    }

    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_SetCallback(IOIF_TIMx_t id, IOIF_TIM_PeriodElapsedCallback_t callback)
{
    if (id >= s_tim_instance_count || !s_tim_instances[id].is_assigned) {
        return AGRBStatus_PARAM_ERROR;
    }

    s_tim_instances[id].config.callback = callback;
    return AGRBStatus_OK;
}

uint32_t IOIF_TIM_GetTick(void)
{
    return HAL_GetTick();
}

void IOIF_TIM_Delay(uint32_t ms)
{
#if defined(USE_FREERTOS)
    osDelay(ms);
#else
    HAL_Delay(ms);
#endif
}

/**
 *------------------------------------------------------------
 * HAL CALLBACK FUNCTIONS
 *------------------------------------------------------------
 */
#if defined(USE_BAREMETAL)
/**
 * @brief  [HAL Callback] 타이머 인터럽트 발생 시 호출됨 (ISR Context)
 * @details
 * 1. 인터럽트를 발생시킨 타이머가 IOIF에 등록된 녀석인지 확인합니다.
 * 2. 등록된 타이머라면 -> 사용자가 등록한 함수(Core_RunLoop 등)를 실행합니다.
 * 3. 등록되지 않은 타이머라면 -> HAL의 Timebase(SysTick 대용)로 간주하고 Tick을 증가시킵니다.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    bool is_registered_instance = false;

    // 1. 등록된 인스턴스인지 검색
    for (uint32_t i = 0; i < s_tim_instance_count; i++) {
        if (s_tim_instances[i].is_assigned && s_tim_instances[i].htim == htim) {
            
            // 등록된 콜백 함수 실행 (여기가 Core_RunLoop가 실행될 위치입니다!)
            if (s_tim_instances[i].config.callback) {
                s_tim_instances[i].config.callback();
            }
            
            is_registered_instance = true;
            break; // 처리 완료
        }
    }

    // 2. IOIF에 등록되지 않은 타이머라면 HAL Timebase로 처리
    // (CubeMX에서 Timebase Source로 설정한 타이머가 여기 해당됩니다)
    if (is_registered_instance == false) {
        HAL_IncTick();
    }
}
#endif

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* 
 * _get_instance_by_handle 함수는 제거됨
 * 
 * 이유:
 * 1. 사용하는 곳이 HAL_TIM_PeriodElapsedCallback 한 곳뿐
 * 2. ISR에서 함수 호출 오버헤드 제거 (성능 최적화)
 * 3. 직접 for 루프가 더 효율적
 */

/**
 *------------------------------------------------------------
 * [신규] 범용 Timer PWM Trigger API (ADC/DAC 트리거용, H7/G4 공용)
 *------------------------------------------------------------
 */

/**
 * @brief [PRIVATE] Timer Clock 주파수를 계산합니다 (APB1/APB2 자동 감지).
 * @details
 * - APB Prescaler가 1이 아니면 Timer Clock = APB Clock × 2
 * - H7: TIM2,3,4,5,6,7,12,13,14 → APB1, TIM1,8,15,16,17 → APB2
 * - G4: TIM2,3,4,5,6,7 → APB1, TIM1,8,15,16,17,20 → APB2
 * 
 * @param tim_instance TIM1, TIM2, ...
 * @return Timer Clock 주파수 (Hz), 실패 시 0
 */
static uint32_t _GetTimerClock(TIM_TypeDef* tim_instance)
{
    if (tim_instance == NULL) return 0;
    
    RCC_ClkInitTypeDef clk_config;
    uint32_t flash_latency;
    HAL_RCC_GetClockConfig(&clk_config, &flash_latency);
    
    uint32_t apb_clock = 0;
    uint32_t apb_prescaler = 0;
    
    /* [1] APB1 Timer 그룹 */
#if defined(TIM2)
    if (tim_instance == TIM2) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM3)
    else if (tim_instance == TIM3) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM4)
    else if (tim_instance == TIM4) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM5)
    else if (tim_instance == TIM5) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM6)
    else if (tim_instance == TIM6) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM7)
    else if (tim_instance == TIM7) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM12)
    else if (tim_instance == TIM12) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM13)
    else if (tim_instance == TIM13) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
#if defined(TIM14)
    else if (tim_instance == TIM14) { apb_clock = HAL_RCC_GetPCLK1Freq(); apb_prescaler = clk_config.APB1CLKDivider; }
#endif
    
    /* [2] APB2 Timer 그룹 */
#if defined(TIM1)
    else if (tim_instance == TIM1) { apb_clock = HAL_RCC_GetPCLK2Freq(); apb_prescaler = clk_config.APB2CLKDivider; }
#endif
#if defined(TIM8)
    else if (tim_instance == TIM8) { apb_clock = HAL_RCC_GetPCLK2Freq(); apb_prescaler = clk_config.APB2CLKDivider; }
#endif
#if defined(TIM15)
    else if (tim_instance == TIM15) { apb_clock = HAL_RCC_GetPCLK2Freq(); apb_prescaler = clk_config.APB2CLKDivider; }
#endif
#if defined(TIM16)
    else if (tim_instance == TIM16) { apb_clock = HAL_RCC_GetPCLK2Freq(); apb_prescaler = clk_config.APB2CLKDivider; }
#endif
#if defined(TIM17)
    else if (tim_instance == TIM17) { apb_clock = HAL_RCC_GetPCLK2Freq(); apb_prescaler = clk_config.APB2CLKDivider; }
#endif
#if defined(TIM20)
    else if (tim_instance == TIM20) { apb_clock = HAL_RCC_GetPCLK2Freq(); apb_prescaler = clk_config.APB2CLKDivider; }
#endif
    else {
        return 0;  /* Unsupported Timer */
    }
    
    /* [3] APB Prescaler != 1이면 Timer Clock = APB Clock × 2 */
    uint32_t timer_clock = (apb_prescaler == RCC_HCLK_DIV1) ? apb_clock : (apb_clock * 2);
    
    return timer_clock;
}

/**
 * @brief [PRIVATE] Timer Clock을 활성화합니다.
 */
static void _EnableTimerClock(TIM_TypeDef* tim_instance)
{
    if (tim_instance == NULL) return;
    
#if defined(TIM1)
    if (tim_instance == TIM1) __HAL_RCC_TIM1_CLK_ENABLE();
#endif
#if defined(TIM2)
    else if (tim_instance == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
#endif
#if defined(TIM3)
    else if (tim_instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
#endif
#if defined(TIM4)
    else if (tim_instance == TIM4) __HAL_RCC_TIM4_CLK_ENABLE();
#endif
#if defined(TIM5)
    else if (tim_instance == TIM5) __HAL_RCC_TIM5_CLK_ENABLE();
#endif
#if defined(TIM6)
    else if (tim_instance == TIM6) __HAL_RCC_TIM6_CLK_ENABLE();
#endif
#if defined(TIM7)
    else if (tim_instance == TIM7) __HAL_RCC_TIM7_CLK_ENABLE();
#endif
#if defined(TIM8)
    else if (tim_instance == TIM8) __HAL_RCC_TIM8_CLK_ENABLE();
#endif
#if defined(TIM12)
    else if (tim_instance == TIM12) __HAL_RCC_TIM12_CLK_ENABLE();
#endif
#if defined(TIM13)
    else if (tim_instance == TIM13) __HAL_RCC_TIM13_CLK_ENABLE();
#endif
#if defined(TIM14)
    else if (tim_instance == TIM14) __HAL_RCC_TIM14_CLK_ENABLE();
#endif
#if defined(TIM15)
    else if (tim_instance == TIM15) __HAL_RCC_TIM15_CLK_ENABLE();
#endif
#if defined(TIM16)
    else if (tim_instance == TIM16) __HAL_RCC_TIM16_CLK_ENABLE();
#endif
#if defined(TIM17)
    else if (tim_instance == TIM17) __HAL_RCC_TIM17_CLK_ENABLE();
#endif
#if defined(TIM20)
    else if (tim_instance == TIM20) __HAL_RCC_TIM20_CLK_ENABLE();
#endif
}

/**
 * @brief [PUBLIC] CubeMX 생성 Timer를 IOIF에 할당합니다 (범용 API).
 * @implementation
 * - Timer 인스턴스 풀에 등록하고 ID 발급
 * - System Layer는 HAL 핸들을 몰라도 됨
 */
AGRBStatusDef IOIF_TIM_AssignInstance(IOIF_TIMx_t* tim_id, TIM_HandleTypeDef* htim)
{
    if (tim_id == NULL || htim == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }
    
    if (s_tim_instance_count >= IOIF_TIM_MAX_INSTANCES) {
        return AGRBStatus_ERROR;  /* Pool full */
    }
    
    /* ✅ Instance Pool에 등록 */
    uint32_t id = s_tim_instance_count++;
    s_tim_instances[id].is_assigned = true;
    s_tim_instances[id].htim = htim;
    s_tim_instances[id].config.callback = NULL;  /* Callback은 선택적 */
    
    *tim_id = id;
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] IOIF 관리 Timer를 Base 모드로 시작합니다 (범용 API).
 */
AGRBStatusDef IOIF_TIM_StartBase(IOIF_TIMx_t tim_id)
{
    if (tim_id >= s_tim_instance_count) return AGRBStatus_PARAM_ERROR;
    if (!s_tim_instances[tim_id].is_assigned) return AGRBStatus_PARAM_ERROR;
    
    TIM_HandleTypeDef* htim = s_tim_instances[tim_id].htim;
    if (HAL_TIM_Base_Start(htim) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    
    return AGRBStatus_OK;
}

/**
 * @brief [PUBLIC] IOIF 관리 Timer를 Base 모드로 중지합니다 (범용 API).
 */
AGRBStatusDef IOIF_TIM_StopBase(IOIF_TIMx_t tim_id)
{
    if (tim_id >= s_tim_instance_count) return AGRBStatus_PARAM_ERROR;
    if (!s_tim_instances[tim_id].is_assigned) return AGRBStatus_PARAM_ERROR;
    
    TIM_HandleTypeDef* htim = s_tim_instances[tim_id].htim;
    if (HAL_TIM_Base_Stop(htim) != HAL_OK) {
        return AGRBStatus_ERROR;
    }

    return AGRBStatus_OK;
}

/**
 *------------------------------------------------------------
 * [신규] PWM Output API (H7/G4 공용)
 *------------------------------------------------------------
 * @details
 * HAL TIM PWM API를 IOIF ID 기반으로 래핑합니다.
 * Device Layer에서 HAL 직접 접근 없이 PWM 제어가 가능합니다.
 *
 * [H7/G4 호환성 검증]
 * - HAL_TIM_PWM_Start/Stop: stm32h7xx_hal_tim.h / stm32g4xx_hal_tim.h 동일 시그니처
 * - HAL_TIMEx_PWMN_Start/Stop: stm32h7xx_hal_tim_ex.h / stm32g4xx_hal_tim_ex.h 동일
 * - __HAL_TIM_SET_COMPARE: 동일 매크로 (htim->Instance->CCRx = value)
 * - Dead-time: CubeMX에서 BDTRInitTypeDef로 설정, IOIF에서 변경하지 않음
 */

/** @brief [PRIVATE] ID 유효성 검사 + htim 포인터 획득 헬퍼 */
static TIM_HandleTypeDef* _GetTimHandle(IOIF_TIMx_t tim_id)
{
    if (tim_id >= s_tim_instance_count) return NULL;
    if (!s_tim_instances[tim_id].is_assigned) return NULL;
    return s_tim_instances[tim_id].htim;
}

AGRBStatusDef IOIF_TIM_PWM_Start(IOIF_TIMx_t tim_id, uint32_t channel)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    if (HAL_TIM_PWM_Start(htim, channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_PWMN_Start(IOIF_TIMx_t tim_id, uint32_t channel)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    if (HAL_TIMEx_PWMN_Start(htim, channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_PWM_SetCompare(IOIF_TIMx_t tim_id, uint32_t channel, uint32_t compare)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    __HAL_TIM_SET_COMPARE(htim, channel, compare);
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_PWM_Stop(IOIF_TIMx_t tim_id, uint32_t channel)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    if (HAL_TIM_PWM_Stop(htim, channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_PWMN_Stop(IOIF_TIMx_t tim_id, uint32_t channel)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    if (HAL_TIMEx_PWMN_Stop(htim, channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    return AGRBStatus_OK;
}

/* ============================================================================
 * PWM/OC Interrupt API + Timer Register Access API
 * ============================================================================ */

AGRBStatusDef IOIF_TIM_PWM_Start_IT(IOIF_TIMx_t tim_id, uint32_t channel)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    if (HAL_TIM_PWM_Start_IT(htim, channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_PWMN_Start_IT(IOIF_TIMx_t tim_id, uint32_t channel)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    if (HAL_TIMEx_PWMN_Start_IT(htim, channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_OC_Start_IT(IOIF_TIMx_t tim_id, uint32_t channel)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    if (HAL_TIM_OC_Start_IT(htim, channel) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_GetAutoReload(IOIF_TIMx_t tim_id, uint32_t *arr)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL || arr == NULL) return AGRBStatus_PARAM_ERROR;

    *arr = __HAL_TIM_GET_AUTORELOAD(htim);
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_SetAutoReload(IOIF_TIMx_t tim_id, uint32_t arr)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    __HAL_TIM_SET_AUTORELOAD(htim, arr);
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_SetOCMode(IOIF_TIMx_t tim_id, uint32_t channel, uint32_t mode)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    /*
     * ISR-safe: CCMR 레지스터의 OCxM 비트만 직접 변경.
     * HAL_TIM_OC_ConfigChannel()은 __HAL_LOCK 사용 + 전체 OC 재설정이라
     * CC ISR 콜백 안에서 호출하기 부적절.
     *
     * OCxM 비트 위치: CH1=CCMR1[6:4], CH2=CCMR1[14:12],
     *                 CH3=CCMR2[6:4], CH4=CCMR2[14:12]
     * mode 값: TIM_OCMODE_FORCED_INACTIVE(0x0040), TIM_OCMODE_PWM2(0x0070) 등
     *          HAL 매크로 하위 8비트가 CCMR OCxM[2:0], 비트16이 OCxM[3] (G4 전용)
     */
    TIM_TypeDef* TIMx = htim->Instance;
    uint32_t mode_bits = (mode & 0x00000070U);       /* OCxM[2:0] */
    uint32_t mode_bit3 = (mode & 0x00010000U) >> 16; /* OCxM[3] (G4/H7) */

    switch (channel) {
        case TIM_CHANNEL_1:
            MODIFY_REG(TIMx->CCMR1, TIM_CCMR1_OC1M, mode_bits);
            MODIFY_REG(TIMx->CCMR1, TIM_CCMR1_OC1M_3,
                       mode_bit3 ? TIM_CCMR1_OC1M_3 : 0U);
            break;
        case TIM_CHANNEL_2:
            MODIFY_REG(TIMx->CCMR1, TIM_CCMR1_OC2M, mode_bits << 8);
            MODIFY_REG(TIMx->CCMR1, TIM_CCMR1_OC2M_3,
                       mode_bit3 ? TIM_CCMR1_OC2M_3 : 0U);
            break;
        case TIM_CHANNEL_3:
            MODIFY_REG(TIMx->CCMR2, TIM_CCMR2_OC3M, mode_bits);
            MODIFY_REG(TIMx->CCMR2, TIM_CCMR2_OC3M_3,
                       mode_bit3 ? TIM_CCMR2_OC3M_3 : 0U);
            break;
        case TIM_CHANNEL_4:
            MODIFY_REG(TIMx->CCMR2, TIM_CCMR2_OC4M, mode_bits << 8);
            MODIFY_REG(TIMx->CCMR2, TIM_CCMR2_OC4M_3,
                       mode_bit3 ? TIM_CCMR2_OC4M_3 : 0U);
            break;
        default:
            return AGRBStatus_PARAM_ERROR;
    }
    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_TIM_GenerateUpdate(IOIF_TIMx_t tim_id)
{
    TIM_HandleTypeDef* htim = _GetTimHandle(tim_id);
    if (htim == NULL) return AGRBStatus_PARAM_ERROR;

    /* ISR-safe: EGR 레지스터 직접 설정 (HAL_TIM_GenerateEvent는 __HAL_LOCK 사용) */
    htim->Instance->EGR |= TIM_EGR_UG;
    return AGRBStatus_OK;
}

IOIF_TIMx_t IOIF_TIM_FindByHandle(const TIM_HandleTypeDef *htim)
{
    if (htim == NULL) return IOIF_TIM_NOT_INITIALIZED;

    for (uint8_t i = 0; i < s_tim_instance_count; i++) {
        if (s_tim_instances[i].is_assigned && s_tim_instances[i].htim == htim) {
            return (IOIF_TIMx_t)i;
        }
    }
    return IOIF_TIM_NOT_INITIALIZED;
}
