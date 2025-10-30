/**
 * @file bsp_tim.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW
 * @brief Board Support Package for Timer functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#include "bsp_tim.h"

/** @defgroup TIM TIM
  * @brief TIM HAL BSP module driver
  * @{
  */
#ifdef HAL_TIM_MODULE_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */




/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */



/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */

#ifdef WALKON5_CM_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, NULL},         // Timer not defined
    {BSP_TIM2, NULL},       // Timer 2
    {BSP_TIM3, &htim3},       // Timer 3
    {BSP_TIM4, NULL},         // Timer not defined
    {BSP_TIM5, NULL},         // Timer not defined
    {BSP_TIM6, NULL},       // Timer 6
    {BSP_TIM7, NULL},       // Timer 7
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, &htim13},     // Timer 13
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, NULL},     // Timer 15
    {BSP_TIM16, &htim16},     // Timer 16
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, NULL},         // Timer not defined
    {BSP_TIM2, &htim2},       // Timer 2
    {BSP_TIM3, &htim3},       // Timer 3
    {BSP_TIM4, NULL},         // Timer not defined
    {BSP_TIM5, NULL},         // Timer not defined
    {BSP_TIM6, &htim6},       // Timer 6
    {BSP_TIM7, &htim7},       // Timer 7
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, &htim13},     // Timer 13
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, &htim15},     // Timer 15
    {BSP_TIM16, &htim16},     // Timer 16
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, NULL},         // Timer not defined
	{BSP_TIM2, NULL},         // Timer 2
    {BSP_TIM3, NULL},         // Timer not defined
    {BSP_TIM4, NULL},         // Timer not defined
    {BSP_TIM5, NULL},         // Timer not defined
    {BSP_TIM6, &htim6},       // Timer 6
    {BSP_TIM7, &htim7},       // Timer 7
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, &htim12},     // Timer 12
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, &htim15},     // Timer 15
    {BSP_TIM16, &htim16},     // Timer 16
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* SUIT_MINICM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, &htim1},       // Timer 1
    {BSP_TIM2, &htim2},       // Timer 2
    {BSP_TIM3, NULL},         // Timer not defined
    {BSP_TIM4, &htim4},       // Timer 4
    {BSP_TIM5, &htim5},       // Timer 5
    {BSP_TIM6, &htim6},       // Timer 6
    {BSP_TIM7, &htim7},       // Timer 7
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, &htim15},     // Timer 15
    {BSP_TIM16, &htim16},     // Timer 16
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, &htim1},       // Timer 1
    {BSP_TIM2, &htim2},       // Timer 2
    {BSP_TIM3, NULL},         // Timer not defined
    {BSP_TIM4, &htim4},       // Timer 4
    {BSP_TIM5, &htim5},       // Timer 5
    {BSP_TIM6, &htim6},       // Timer 6
    {BSP_TIM7, &htim7},       // Timer 7
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, &htim15},     // Timer 15
    {BSP_TIM16, &htim16},     // Timer 16
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, &htim1},       // Timer 1
    {BSP_TIM2, &htim2},       // Timer 2
    {BSP_TIM3, NULL},         // Timer not defined
    {BSP_TIM4, &htim4},       // Timer 4
    {BSP_TIM5, &htim5},       // Timer 5
    {BSP_TIM6, &htim6},       // Timer 6
    {BSP_TIM7, &htim7},       // Timer 7
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, &htim15},     // Timer 15
    {BSP_TIM16, &htim16},     // Timer 16
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MD_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, &htim1},       // Timer 1
    {BSP_TIM2, &htim2},       // Timer 2
    {BSP_TIM3, NULL},         // Timer not defined
    {BSP_TIM4, NULL},        // Timer 4
    {BSP_TIM5, &htim5},       // Timer 5
    {BSP_TIM6, &htim6},       // Timer 6
    {BSP_TIM7, &htim7},       // Timer 7
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, &htim15},     // Timer 15
    {BSP_TIM16, &htim16},     // Timer 16
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, NULL},         // Timer not defined
    {BSP_TIM2, &htim2},       // Timer 2
    {BSP_TIM3, NULL},         // Timer not defined
    {BSP_TIM4, NULL},         // Timer not defined
    {BSP_TIM5, NULL},         // Timer not defined
    {BSP_TIM6, &htim6},       // Timer 6
    {BSP_TIM7, NULL},         // Timer not defined
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, NULL},        // Timer not defined
    {BSP_TIM16, NULL},        // Timer not defined
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* WIDM_ENABLED */

#ifdef KAIST_GRF_BOARD_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, NULL},       // Timer 1
    {BSP_TIM2, &htim2},       // Timer 2
    {BSP_TIM3, NULL},       // Timer 3
    {BSP_TIM4, NULL},         // Timer not defined
    {BSP_TIM5, NULL},         // Timer not defined
    {BSP_TIM6, &htim6},         // Timer not defined
    {BSP_TIM7, NULL},         // Timer not defined
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, NULL},        // Timer not defined
    {BSP_TIM16, NULL},        // Timer not defined
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* KAIST_GRF_BOARD_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
static BSP_TimMap_t bsp_timMap[BSP_TIM_COUNT] = {
    {BSP_TIM_COUNT, NULL},    // Dummy entry for index 0
    {BSP_TIM1, NULL},         // Timer not defined
    {BSP_TIM2, &htim2},       // Timer 2				// exppack_ctrl
    {BSP_TIM3, &htim3},       // Timer 3				// algorithm_ctrl
    {BSP_TIM4, NULL},         // Timer not defined
    {BSP_TIM5, NULL},         // Timer not defined
    {BSP_TIM6, &htim6},       // Timer 6				// msg_hdlr
    {BSP_TIM7, NULL},         // Timer not defined
    {BSP_TIM8, NULL},         // Timer not defined
    {BSP_TIM9, NULL},         // Timer not defined
    {BSP_TIM10, NULL},        // Timer not defined
    {BSP_TIM11, NULL},        // Timer not defined
    {BSP_TIM12, NULL},        // Timer not defined
    {BSP_TIM13, NULL},        // Timer not defined
    {BSP_TIM14, NULL},        // Timer not defined
    {BSP_TIM15, NULL},        // Timer not defined
    {BSP_TIM16, NULL},        // Timer not defined
    {BSP_TIM17, NULL},        // Timer not defined
};
#endif /* EXPANSION_BOARD_ENABLED */

static BSP_TimCB_t bsp_timCB[BSP_TIM_COUNT];


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static TIM_HandleTypeDef* FindTimHandle(BSP_Tim_t timer);
static BSP_Tim_t FindTimEnum(TIM_HandleTypeDef* htim);
static void ExecuteTimCB(TIM_HandleTypeDef* htim, BSP_TimCBType_t callbackType);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- TIMER INTERRUPT ------------------- */
/**
 * @brief   Process timer interrupt operations based on specified timer and operation.
 * @param   timer       The timer enumeration specifying which timer to operate on.
 * @param   operation   The specific operation to be executed on the timer.
 * @return  BSP_StatusTypeDef_t - Status of the operation (e.g., BSP_OK, BSP_ERROR).
 * @details This function checks the validity of the provided timer handle and then
 *          executes the specified timer interrupt operation. If the handle is invalid
 *          or an unsupported operation is requested, it returns BSP_ERROR.
 */
BSP_StatusTypeDef_t BSP_RunTimIT(BSP_Tim_t timer, BSP_TimOP_t operation)
{
	TIM_HandleTypeDef* htim = FindTimHandle(timer);

    BSP_StatusTypeDef_t status = BSP_OK;
    // Process the operation based on the provided BSP_TimOP_t value
    switch (operation) {
        case BSP_TIM_BASE_START_IT:
            status = (BSP_StatusTypeDef_t)HAL_TIM_Base_Start_IT(htim);
            break;
        case BSP_TIM_BASE_STOP_IT:
            status = (BSP_StatusTypeDef_t)HAL_TIM_Base_Stop_IT(htim);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

BSP_StatusTypeDef_t BSP_RunTimOCIT(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation)
{
	TIM_HandleTypeDef* htim = FindTimHandle(timer);

    BSP_StatusTypeDef_t status = BSP_OK;
    // Process the operation based on the provided BSP_TimOP_t value
    switch (operation) {
        case BSP_TIM_OC_START_IT:
            status = (BSP_StatusTypeDef_t)HAL_TIM_OC_Start_IT(htim, channel);
            break;
        case BSP_TIM_OC_STOP_IT:
            status = (BSP_StatusTypeDef_t)HAL_TIM_OC_Stop_IT(htim, channel);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- TIMER PWM ------------------- */
/**
 * @brief Execute PWM operations based on the specified BSP Timer, channel, and operation.
 * @param timer The BSP Timer enumeration to specify which timer to operate upon.
 * @param channel The channel number to specify which channel of the timer to utilize.
 * @param operation The desired PWM operation to execute (e.g., start, stop).
 * @return BSP_StatusTypeDef_t Indicates the success or failure of the PWM operation.
 */
BSP_StatusTypeDef_t BSP_RunPWM(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation)
{
	TIM_HandleTypeDef* htim = FindTimHandle(timer);

    BSP_StatusTypeDef_t status = BSP_OK;
    // Process the operation based on the provided BSP_TimOP_t value
    switch (operation) {
        case BSP_TIM_PWM_START:
            status = (BSP_StatusTypeDef_t)HAL_TIM_PWM_Start(htim, channel);
            break;
        case BSP_TIM_PWM_STOP:
            status = (BSP_StatusTypeDef_t)HAL_TIM_PWM_Stop(htim, channel);
            break;
        case BSP_TIMEx_PWMN_START:
            status = (BSP_StatusTypeDef_t)HAL_TIMEx_PWMN_Start(htim, channel);
            break;
        case BSP_TIMEx_PWMN_STOP:
            status = (BSP_StatusTypeDef_t)HAL_TIMEx_PWMN_Stop(htim, channel);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief Stop the PWM operation using DMA.
 * @param timer BSP timer enumeration.
 * @param channel Channel number.
 * @param operation PWM operation type.
 * @return HAL status indicating success or failure.
 */
BSP_StatusTypeDef_t BSP_StartPWMDMA(BSP_Tim_t timer, uint32_t channel, const uint32_t* pData, uint16_t length, BSP_TimOP_t operation)
{
	TIM_HandleTypeDef* htim = FindTimHandle(timer);

    BSP_StatusTypeDef_t status = BSP_OK;
    // Process the operation based on the provided BSP_TimOP_t value
    switch (operation) {
        case BSP_TIM_PWM_START_DMA:
            status = (BSP_StatusTypeDef_t)HAL_TIM_PWM_Start_DMA(htim, channel, pData, length);
            break;
        case BSP_TIMEx_PWMN_START_DMA:
            status = (BSP_StatusTypeDef_t)HAL_TIMEx_PWMN_Start_DMA(htim, channel, pData, length);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief Set the compare value for a given timer and channel.
 * @param timer BSP timer enumeration.
 * @param channel Channel number.
 * @param operation Operation type (set compare).
 * @param compareValue Value to set for comparison.
 * @return HAL status indicating success or failure.
 */
BSP_StatusTypeDef_t BSP_StopPWMDMA(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation)
{
	TIM_HandleTypeDef* htim = FindTimHandle(timer);

    BSP_StatusTypeDef_t status = BSP_OK;
    // Process the operation based on the provided BSP_TimOP_t value
    switch (operation) {
        case BSP_TIM_PWM_STOP_DMA:
            status = (BSP_StatusTypeDef_t)HAL_TIM_PWM_Stop_DMA(htim, channel);
            break;
        case BSP_TIMEx_PWMN_STOP_DMA:
            status = (BSP_StatusTypeDef_t)HAL_TIMEx_PWMN_Stop_DMA(htim, channel);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
    
    return status;
}

/**
 * @brief Set the compare value for a given timer and channel.
 * @param timer BSP timer enumeration.
 * @param channel Channel number.
 * @param operation Operation type (set compare).
 * @param compareValue Value to set for comparison.
 * @return HAL status indicating success or failure.
 */
BSP_StatusTypeDef_t BSP_SetTimCompVal(BSP_Tim_t timer, uint32_t channel, uint32_t compareValue)
{
	TIM_HandleTypeDef* htim = FindTimHandle(timer);
    __HAL_TIM_SET_COMPARE(htim, channel, compareValue);

    return BSP_OK;
}

/* ------------------- TIMER UTILITIES ------------------- */
void BSP_Delay(uint32_t msDelay)
{
    HAL_Delay(msDelay);
}

/**
 * @brief Retrieve the current tick count.
 * @return Current tick value.
 */
uint32_t BSP_GetTick(void)
{
    return HAL_GetTick();
}

/**
 * @brief Retrieve the current counter value of a specified timer.
 * @param timer BSP timer enumeration to specify which timer's counter value to retrieve.
 * @return Current counter value of the specified timer or BSP_ERROR if the timer handle is not valid.
 */
int32_t BSP_GetCnt(BSP_Tim_t timer)
{
    TIM_HandleTypeDef* htim = FindTimHandle(timer);
    return __HAL_TIM_GET_COUNTER(htim);
}

void BSP_SetCCRCnt(BSP_Tim_t timer, uint32_t CCRCnt)
{
    TIM_HandleTypeDef* htim = FindTimHandle(timer);
    htim->Instance->CCR1 = CCRCnt;
}

void BSP_SetARRCnt(BSP_Tim_t timer, uint32_t ARRCnt)
{
    TIM_HandleTypeDef* htim = FindTimHandle(timer);
    htim->Instance->ARR = ARRCnt;
}

uint32_t BSP_GetCCRCnt(BSP_Tim_t timer)
{
    TIM_HandleTypeDef* htim = FindTimHandle(timer);
    return htim->Instance->CCR1;
}

uint32_t BSP_GetARRCnt(BSP_Tim_t timer)
{
    TIM_HandleTypeDef* htim = FindTimHandle(timer);
    return htim->Instance->ARR;
}

void BSP_SetCounterMode(BSP_Tim_t timer, uint32_t counterMode)
{
    TIM_HandleTypeDef* htim = FindTimHandle(timer);
    htim->Init.CounterMode = counterMode;
}

/**
  * @brief  Controls the BSP Timer Encoder Interface based on the specified operation.
  * @param  timer BSP timer enumeration specifying which timer's Encoder Interface to control.
  * @param  channel Channel number to be enabled or disabled. Ensure that the channel number is within a valid range for the timer.
  *          This parameter can be one of the valid channel numbers for the timer (based on BSP_TIM_CHANNEL_COUNT).
  * @param  operation Specifies the operation to perform on the timer's Encoder Interface.
  *          This parameter can be one of the following values:
  *            @arg BSP_TIM_ENCODER_START: Start the encoder interface for the specified channel.
  *            @arg BSP_TIM_ENCODER_STOP: Stop the encoder interface for the specified channel.
  * @retval BSP_StatusTypeDef_t status indicating success or error.
  */
BSP_StatusTypeDef_t BSP_RunTimEnc(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation)
{
    TIM_HandleTypeDef* htim = FindTimHandle(timer);

    BSP_StatusTypeDef_t status = BSP_OK;
    // Process the operation based on the provided BSP_TimOP_t value
    switch (operation) {
        case BSP_TIM_ENCODER_START:
            status = (BSP_StatusTypeDef_t)HAL_TIM_Encoder_Start(htim, channel);
            break;
        case BSP_TIM_ENCODER_STOP:
            status = (BSP_StatusTypeDef_t)HAL_TIM_Encoder_Stop(htim, channel);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- TIMER CALLBACKS ------------------- */
/**
 * @brief Registers a custom callback for a specified Timer port and callback type.
 *
 * @param timer Enum value representing the Timer port.
 * @param callbackType Enum value representing the type of callback (e.g., transmit complete, receive complete).
 * @param callback Function pointer to the custom callback function.
 * @param params Pointer to any parameters that the custom callback function may need.
 */
void BSP_SetTimCB(BSP_Tim_t timer, BSP_TimCBType_t callbackType, BSP_TimCBPtr_t callback, void* params)
{
    if (timer < BSP_TIM_COUNT && callbackType < BSP_TIM_CALLBACK_TYPE_COUNT && callback) {
        bsp_timCB[timer].callbacks[callbackType] = callback;
        bsp_timCB[timer].params[callbackType] = params;
    }
}
#ifdef _USE_OS_RTOS_BSP

#else  // For BareMetal
/**
 * @brief Callback executed when the timer period elapses.
 * @param htim HAL timer handle.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    UNUSED(htim);


    // TODO: Nothing!!

   	ExecuteTimCB(htim, BSP_TIM_PERIOD_ELAPSED_CALLBACK);
}
#endif /* _USE_OS_RTOS_BSP */

/**
 * @brief Callback executed when the output compare delay elapses.
 * @param htim HAL timer handle.
 */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim)
{
    UNUSED(htim);

	ExecuteTimCB(htim, BSP_TIM_OC_DELAY_ELAPSED_CALLBACK);
}

/**
 * @brief Callback executed when an input capture event occurs.
 * @param htim HAL timer handle.
 */
void HAL_TIM_IC_CaptureCallbac(TIM_HandleTypeDef* htim)
{
    UNUSED(htim);

	ExecuteTimCB(htim, BSP_TIM_IC_CAPTURE_CALLBACK);
}

/**
 * @brief Callback executed when a PWM pulse operation finishes.
 * @param htim HAL timer handle.
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim)
{
    UNUSED(htim);

    ExecuteTimCB(htim, BSP_TIM_PWM_PULSE_FINISHED_CALLBACK);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Executes a custom callback for a specified Timer port and callback type.
 *
 * @param htim HAL Timer handle, used to find the corresponding Timer port enum value.
 * @param callbackType Enum value representing the type of callback to execute.
 */
static void ExecuteTimCB(TIM_HandleTypeDef* htim, BSP_TimCBType_t callbackType)
{
    BSP_Tim_t timer = FindTimEnum(htim);
    if (timer < BSP_TIM_COUNT) {
    	BSP_TimCBPtr_t callback = bsp_timCB[timer].callbacks[callbackType];
        void* params = bsp_timCB[timer].params[callbackType];
        if (callback) {
            callback(params); // Executes the custom callback with the specified parameters
        }
    }
}

/**
 * @brief Retrieve the HAL Timer handle associated with the given BSP Timer enumeration.
 * @param timer The BSP Timer enumeration for which the HAL Timer handle is sought.
 * @return TIM_HandleTypeDef* Pointer to the HAL Timer handle, or NULL if not found.
 */
static TIM_HandleTypeDef* FindTimHandle(BSP_Tim_t timer)
{
	for (int i = 0; i < ARRAY_SIZE(bsp_timMap); i++) {
        if (bsp_timMap[i].timer == timer) {
            return bsp_timMap[i].handle;
        }
    }
    return NULL;
}

static BSP_Tim_t FindTimEnum(TIM_HandleTypeDef* htim)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_timMap); i++) {
        if (bsp_timMap[i].handle == htim) {
            return bsp_timMap[i].timer;
        }
    }

    return BSP_ERROR;
}


#endif /* HAL_TIM_MODULE_ENABLED */
