/**
 *-----------------------------------------------------------
 *              IOIF TIMER INTERFACE COMMON
 *-----------------------------------------------------------
 * @file ioif_timer_driver.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief IOIF Timer Common Implementation.
 *
 * This file implements the functions required for timer interactions at the IOIF level.
 * It provides an abstraction layer for using the hardware and BSP timer functions, making
 * it suitable for use at the task level.
 *
 * This driver is built upon the lower level timer drivers and is designed to simplify timer
 * interactions for higher level tasks and applications.
 *
 * @ref Relevant Datasheet or Reference (if available)
 */

#include "ioif_tim_common.h"

/** @defgroup TIM TIM
  * @brief TIM BSP module driver
  * @{
  */
#ifdef BSP_TIM_MODULE_ENABLED

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




/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */




/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- TIMER UTILITIES ------------------- */
void IOIF_msDelay(uint32_t msTime)
{
    BSP_Delay(msTime);
}

void IOIF_SetCCRCnt(IOIF_Tim_t timer, uint32_t CCRCnt)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return;
    }

    BSP_SetCCRCnt((BSP_Tim_t)timer, CCRCnt);
}

void IOIF_SetARRCnt(IOIF_Tim_t timer, uint32_t ARRCnt)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return;
    }

    BSP_SetARRCnt((BSP_Tim_t)timer, ARRCnt);
}

uint32_t IOIF_GetCCRCnt(IOIF_Tim_t timer)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    return BSP_GetCCRCnt((BSP_Tim_t)timer);
}

uint32_t IOIF_GetARRCnt(IOIF_Tim_t timer)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    return BSP_GetARRCnt((BSP_Tim_t)timer);
}

void IOIF_SetCounterMode(IOIF_Tim_t timer, uint32_t counterMode)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return;
    }

    BSP_SetCounterMode((BSP_Tim_t)timer, counterMode);
}

/* ------------------- TIMER REAL-TIME CHECK ------------------- */
/**
  * @brief  Retrieve the current tick count at the IOIF level.
  * @retval Current tick value.
  */
uint32_t IOIF_GetTick(void)
{
	return BSP_GetTick();
}

/* ------------------- TIMER INTERRUPT ------------------- */
/**
  * @brief  Starts the timer interrupt at the IOIF level.
  * @param  timer IOIF timer enumeration to specify which timer interrupt to start.
  * @retval IOIF_TimState_t status indicating success, error, or other states.
  */
IOIF_TimState_t IOIF_StartTimIT(IOIF_Tim_t timer)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunTimIT((BSP_Tim_t)timer, BSP_TIM_BASE_START_IT);

    return status;
}

/**
  * @brief  Stops the timer interrupt at the IOIF level.
  * @param  timer IOIF timer enumeration to specify which timer interrupt to stop.
  * @retval IOIF_TimState_t status indicating success, error, or other states.
  */
IOIF_TimState_t IOIF_StopTimIT(IOIF_Tim_t timer)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunTimIT((BSP_Tim_t)timer, BSP_TIM_BASE_STOP_IT);

    return status;
}

IOIF_TimState_t IOIF_StartTimOCIT(IOIF_Tim_t timer, uint32_t channel)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunTimOCIT((BSP_Tim_t)timer, channel, BSP_TIM_OC_START_IT);

    return status;
}

IOIF_TimState_t IOIF_StopTimOCIT(IOIF_Tim_t timer, uint32_t channel)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunTimOCIT((BSP_Tim_t)timer, channel, BSP_TIM_OC_STOP_IT);

    return status;
}


/* ------------------- TIMER PWM(Gate Driver) ------------------- */
IOIF_TimState_t IOIF_StartPWM(IOIF_Tim_t timer, uint32_t channel)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunPWM((BSP_Tim_t)timer, channel, BSP_TIM_PWM_START);

    return status;
}

IOIF_TimState_t IOIF_StopPWM(IOIF_Tim_t timer, uint32_t channel)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunPWM((BSP_Tim_t)timer, channel, BSP_TIM_PWM_STOP);

    return status;
}

IOIF_TimState_t IOIF_StartPWMN(IOIF_Tim_t timer, uint32_t channel)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunPWM((BSP_Tim_t)timer, channel, BSP_TIMEx_PWMN_START);

    return status;
}

IOIF_TimState_t IOIF_StopPWMN(IOIF_Tim_t timer, uint32_t channel)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_RunPWM((BSP_Tim_t)timer, channel, BSP_TIMEx_PWMN_STOP);

    return status;
}

IOIF_TimState_t IOIF_SetTimCompVal(IOIF_Tim_t timer, uint32_t channel, uint32_t compareValue)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    uint8_t status = BSP_SetTimCompVal(timer, channel, compareValue);

    return status;
}

/* ------------------- REGISTER TIMER CALLBACK ------------------- */
/**
  * @brief  Registers a callback for the specified timer event at the IOIF level.
  * @param  timer IOIF timer enumeration to specify which timer to set the callback for.
  * @param  callbackType Specifies the type of callback event.
  * @param  callback The function pointer to the callback function.
  * @param  params Pointer to parameters to be passed to the callback function.
  * @retval IOIF_TimState_t status indicating success, error, or other states.
  */
IOIF_TimState_t IOIF_SetTimCB(IOIF_Tim_t timer, IOIF_TimCBType_t callbackType, IOIF_TimCBPtr_t callback, void* params)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (timer < 0 || timer >= IOIF_TIM_COUNT) {
        return IOIF_TIM_ERROR;
    }

    if (callbackType < 0 || callbackType >= IOIF_TIM_CALLBACK_TYPE_COUNT) {
        return IOIF_TIM_ERROR;
    }

    if (!callback) {
        return IOIF_TIM_ERROR;
    }

    BSP_SetTimCB((BSP_Tim_t)timer, (BSP_TimCBType_t)callbackType, callback, params);

    return IOIF_TIM_OK;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* BSP_TIM_MODULE_ENABLED */
