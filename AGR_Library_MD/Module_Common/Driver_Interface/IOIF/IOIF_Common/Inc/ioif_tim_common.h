/**
 *-----------------------------------------------------------
 *          IOIF TIMER INTERFACE COMMON HEADER
 *-----------------------------------------------------------
 * @file ioif_timer_driver.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief IOIF Timer Common Header.
 *
 * This header file provides the declarations for the functions and data structures required
 * for timer interactions at the IOIF level.
 *
 * It offers an API that abstracts the hardware and BSP timer interactions, providing a simplified 
 * interface for task level applications to use timers.
 *
 * @ref Relevant Datasheet or Reference (if available)
 */

#ifndef IOIF_COMMON_INC_IOIF_TIM_COMMON_H_
#define IOIF_COMMON_INC_IOIF_TIM_COMMON_H_

#include "bsp_tim.h"
#include "module.h"

/** @defgroup TIM TIM
  * @brief TIM BSP module driver
  * @{
  */
#ifdef BSP_TIM_MODULE_ENABLED

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/** @defgroup TIM_Counter_Mode TIM Counter Mode
  * @{
  */
#define IOIF_TIM_COUNTERMODE_UP              BSP_TIM_COUNTERMODE_UP              /*!< Counter used as up-counter   */
#define IOIF_TIM_COUNTERMODE_DOWN            BSP_TIM_COUNTERMODE_DOWN            /*!< Counter used as down-counter */
#define IOIF_TIM_COUNTERMODE_CENTERALIGNED1  BSP_TIM_COUNTERMODE_CENTERALIGNED1  /*!< Center-aligned mode 1        */
#define IOIF_TIM_COUNTERMODE_CENTERALIGNED2  BSP_TIM_COUNTERMODE_CENTERALIGNED2  /*!< Center-aligned mode 2        */
#define IOIF_TIM_COUNTERMODE_CENTERALIGNED3  BSP_TIM_COUNTERMODE_CENTERALIGNED3  /*!< Center-aligned mode 3        */


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Function pointer type for timer callback functions.
 */
typedef void (*IOIF_TimCBPtr_t)(void* params);

/**
 * @enum IOIF_TimCBType_t
 * @brief Timer callback types for various timer events.
 */
typedef enum _IOIF_TimCBType_t {
    // Generic Callbacks
    IOIF_TIM_PERIOD_ELAPSED_CALLBACK,                  /**< Callback when a timer period has elapsed. */
    IOIF_TIM_PERIOD_ELAPSED_HALF_CPLT_CALLBACK,        /**< Callback when half of the timer period has elapsed. */
    IOIF_TIM_OC_DELAY_ELAPSED_CALLBACK,                /**< Callback for Output Compare delay elapsed event. */
    IOIF_TIM_IC_CAPTURE_CALLBACK,                      /**< Callback for Input Capture event. */
    IOIF_TIM_IC_CAPTURE_HALF_CPLT_CALLBACK,            /**< Callback for half completion of Input Capture event. */
    IOIF_TIM_PWM_PULSE_FINISHED_CALLBACK,              /**< Callback when a PWM pulse has finished. */
    IOIF_TIM_PWM_PULSE_FINISHED_HALF_CPLT_CALLBACK,    /**< Callback when half of a PWM pulse has finished. */
    IOIF_TIM_TRIGGER_CALLBACK,                         /**< Callback for timer trigger event. */
    IOIF_TIM_TRIGGER_HALF_CPLT_CALLBACK,               /**< Callback for half completion of timer trigger event. */
    IOIF_TIM_ERROR_CALLBACK,                           /**< Callback for timer error events. */
    IOIF_TIM_REGISTER_CALLBACK,                        /**< Callback for timer register events. */
    IOIF_TIM_UNREGISTER_CALLBACK,                      /**< Callback for timer unregister events. */

    // Extended Callbacks
    IOIF_TIM_COMMUTE_CALLBACK,                         /**< Extended callback for commute event. */
    IOIF_TIM_COMMUTE_HALF_CPLT_CALLBACK,               /**< Extended callback for half completion of commute event. */
    IOIF_TIM_BREAK_CALLBACK,                           /**< Extended callback for break event. */
    IOIF_TIM_BREAK2_CALLBACK,                          /**< Extended callback for secondary break event. */
    IOIF_TIM_CALLBACK_TYPE_COUNT,
} IOIF_TimCBType_t;

/**
  * @brief  IOIF Status structures definition
  */
typedef enum _IOIF_TimState_t {
    IOIF_TIM_OK       = 0x00,
    IOIF_TIM_ERROR    = 0x01,
    IOIF_TIM_BUSY     = 0x02,
    IOIF_TIM_TIMEOUT  = 0x03
} IOIF_TimState_t;

/**
 * @brief Enumeration for IOIF Timer identifiers.
 */
typedef enum _IOIF_Tim_t {
    IOIF_TIM1 = 1,  ///< Timer 1 Identifier
    IOIF_TIM2,      ///< Timer 2 Identifier
    IOIF_TIM3,      ///< Timer 3 Identifier
    IOIF_TIM4,      ///< Timer 4 Identifier
    IOIF_TIM5,      ///< Timer 5 Identifier
    IOIF_TIM6,      ///< Timer 6 Identifier
    IOIF_TIM7,      ///< Timer 7 Identifier
    IOIF_TIM8,      ///< Timer 8 Identifier
    IOIF_TIM9,      ///< Timer 9 Identifier
    IOIF_TIM10,     ///< Timer 10 Identifier
    IOIF_TIM11,     ///< Timer 11 Identifier
    IOIF_TIM12,     ///< Timer 12 Identifier
    IOIF_TIM13,     ///< Timer 13 Identifier
    IOIF_TIM14,     ///< Timer 14 Identifier
    IOIF_TIM15,     ///< Timer 15 Identifier
    IOIF_TIM16,     ///< Timer 16 Identifier
    IOIF_TIM17,     ///< Timer 17 Identifier
    IOIF_TIM_COUNT
} IOIF_Tim_t;

/**
 * @brief Enumeration for BSP Timer identifiers.
 */
typedef enum _IOIF_TimCh_t {
    IOIF_TIM_CHANNEL_1 = BSP_TIM_CHANNEL_1,  /**< Timer channel 1 */
    IOIF_TIM_CHANNEL_2 = BSP_TIM_CHANNEL_2,  /**< Timer channel 2 */
    IOIF_TIM_CHANNEL_3 = BSP_TIM_CHANNEL_3,  /**< Timer channel 3 */
    IOIF_TIM_CHANNEL_4 = BSP_TIM_CHANNEL_4,  /**< Timer channel 4 */
    IOIF_TIM_CHANNEL_5 = BSP_TIM_CHANNEL_5,  /**< Timer channel 5 */
    IOIF_TIM_CHANNEL_6 = BSP_TIM_CHANNEL_6,  /**< Timer channel 6 */
    IOIF_TIM_CHANNEL_ALL = BSP_TIM_CHANNEL_ALL /**< All timer channels */
} IOIF_TimCh_t;


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */




/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

/* ------------------- TIMER UTILITIES ------------------- */
void IOIF_msDelay(uint32_t msTime);
void IOIF_SetCCRCnt(IOIF_Tim_t timer, uint32_t CCRCnt);
void IOIF_SetARRCnt(IOIF_Tim_t timer, uint32_t ARRCnt);
uint32_t IOIF_GetCCRCnt(IOIF_Tim_t timer);
uint32_t IOIF_GetARRCnt(IOIF_Tim_t timer);
void IOIF_SetCounterMode(IOIF_Tim_t timer, uint32_t counterMode);

/* ------------------- TIMER REAL-TIME CHECK ------------------- */
uint32_t IOIF_GetTick(void);

/* ------------------- TIMER INTERRUPT ------------------- */
IOIF_TimState_t IOIF_StartTimIT(IOIF_Tim_t timer);
IOIF_TimState_t IOIF_StopTimIT(IOIF_Tim_t timer);

IOIF_TimState_t IOIF_StartTimOCIT(IOIF_Tim_t timer, uint32_t channel);
IOIF_TimState_t IOIF_StopTimOCIT(IOIF_Tim_t timer, uint32_t channel);

/* ------------------- TIMER PWM(Gate Driver) ------------------- */
IOIF_TimState_t IOIF_StartPWM(IOIF_Tim_t timer, uint32_t channel);
IOIF_TimState_t IOIF_StopPWM(IOIF_Tim_t timer, uint32_t channel);
IOIF_TimState_t IOIF_StartPWMN(IOIF_Tim_t timer, uint32_t channel);
IOIF_TimState_t IOIF_StopPWMN(IOIF_Tim_t timer, uint32_t channel);

IOIF_TimState_t IOIF_SetTimCompVal(IOIF_Tim_t timer, uint32_t channel, uint32_t compareValue);

/* ------------------- REGISTER TIMER CALLBACK ------------------- */
IOIF_TimState_t IOIF_SetTimCB(IOIF_Tim_t timer, IOIF_TimCBType_t callbackType, IOIF_TimCBPtr_t callback, void* params);


#endif /* BSP_TIM_MODULE_ENABLED */

#endif /* IOIF_COMMON_INC_IOIF_TIM_COMMON_H_ */
