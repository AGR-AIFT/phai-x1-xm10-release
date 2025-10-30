/**
 * @file bsp_tim.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW
 * @brief Board Support Package for Timer functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef TIMER_INC_BSP_TIM_H_
#define TIMER_INC_BSP_TIM_H_

#include "main.h"
#include "module.h"

/** @defgroup TIM TIM
  * @brief TIM HAL BSP module driver
  * @
  */
#ifdef HAL_TIM_MODULE_ENABLED
#define BSP_TIM_MODULE_ENABLED

#include "tim.h"
#include "bsp_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/** @defgroup TIM_Counter_Mode TIM Counter Mode
  * @{
  */
#define BSP_TIM_COUNTERMODE_UP              TIM_COUNTERMODE_UP              /*!< Counter used as up-counter   */
#define BSP_TIM_COUNTERMODE_DOWN            TIM_COUNTERMODE_DOWN            /*!< Counter used as down-counter */
#define BSP_TIM_COUNTERMODE_CENTERALIGNED1  TIM_COUNTERMODE_CENTERALIGNED1  /*!< Center-aligned mode 1        */
#define BSP_TIM_COUNTERMODE_CENTERALIGNED2  TIM_COUNTERMODE_CENTERALIGNED2  /*!< Center-aligned mode 2        */
#define BSP_TIM_COUNTERMODE_CENTERALIGNED3  TIM_COUNTERMODE_CENTERALIGNED3  /*!< Center-aligned mode 3        */


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */


/**
 * @brief Function pointer type for timer callback functions.
 */
typedef void (*BSP_TimCBPtr_t)(void* params);

/**
 * @brief Enumeration for BSP Timer operations.
 * Represents the various timer-related operations available within the BSP layer.
 */
typedef enum _BSP_TimOP_t {
    // Timer Base Functions
    BSP_TIM_BASE_INIT,
    BSP_TIM_BASE_DEINIT,
    BSP_TIM_BASE_MSP_INIT,
    BSP_TIM_BASE_MSP_DEINIT,
    BSP_TIM_BASE_START,
    BSP_TIM_BASE_STOP,
    BSP_TIM_BASE_START_IT,      /**< Start timer with interrupt */
    BSP_TIM_BASE_STOP_IT,       /**< Stop timer with interrupt */
    BSP_TIM_BASE_START_DMA,
    BSP_TIM_BASE_STOP_DMA,

    // Timer Output Compare Functions
    BSP_TIM_OC_INIT,
    BSP_TIM_OC_DEINIT,
    BSP_TIM_OC_MSP_INIT,
    BSP_TIM_OC_MSP_DEINIT,
    BSP_TIM_OC_START,
    BSP_TIM_OC_STOP,
    BSP_TIM_OC_START_IT,
    BSP_TIM_OC_STOP_IT,
    BSP_TIM_OC_START_DMA,
    BSP_TIM_OC_STOP_DMA,

    // Timer PWM Functions
    BSP_TIM_PWM_INIT,
    BSP_TIM_PWM_DEINIT,
    BSP_TIM_PWM_MSP_INIT,
    BSP_TIM_PWM_MSP_DEINIT,
    BSP_TIM_PWM_START,          /**< Start PWM without DMA */
    BSP_TIM_PWM_STOP,           /**< Stop PWM without DMA */
    BSP_TIM_PWM_START_IT,
    BSP_TIM_PWM_STOP_IT,
    BSP_TIM_PWM_START_DMA,      /**< Start PWM with DMA */
    BSP_TIM_PWM_STOP_DMA,       /**< Stop PWM with DMA */

    // Timer Input Capture Functions
    BSP_TIM_IC_INIT,
    BSP_TIM_IC_DEINIT,
    BSP_TIM_IC_MSP_INIT,
    BSP_TIM_IC_MSP_DEINIT,
    BSP_TIM_IC_START,
    BSP_TIM_IC_STOP,
    BSP_TIM_IC_STRAT_IT,
    BSP_TIM_IC_STOP_IT,
    BSP_TIM_IC_START_DMA,
    BSP_TIM_IC_STOP_DMA,

    // Timer One Pulse Functions
    BSP_TIM_ONEPULSE_INIT,
    BSP_TIM_ONEPULSE_DEINIT,
    BSP_TIM_ONEPULSE_MSP_INIT,
    BSP_TIM_ONEPULSE_MSP_DEINIT,
    BSP_TIM_ONEPULSE_START,
    BSP_TIM_ONEPULSE_STOP,
    BSP_TIM_ONEPULSE_START_IT,
    BSP_TIM_ONEPULSE_STOP_IT,

    // Timer Encoder Functions
    BSP_TIM_ENCODER_INIT,
    BSP_TIM_ENCODER_DEINIT,
    BSP_TIM_ENCODER_MSP_INIT,
    BSP_TIM_ENCODER_MSP_DEINIT,
    BSP_TIM_ENCODER_START,
    BSP_TIM_ENCODER_STOP,
    BSP_TIM_ENCODER_START_IT,
    BSP_TIM_ENCODER_STOP_IT,
    BSP_TIM_ENCODER_START_DMA,
    BSP_TIM_ENCODER_STOP_DMA,

    // Timer Hall Sensor Functions
    BSP_TIMEx_HALLSENSOR_INIT,
    BSP_TIMEx_HALLSENSOR_DEINIT,
    BSP_TIMEx_HALLSENSOR_MSP_INIT,
    BSP_TIMEx_HALLSENSOR_MSP_DEINIT,
    BSP_TIMEx_HALLSENSOR_START,
    BSP_TIMEx_HALLSENSOR_STOP,
    BSP_TIMEx_HALLSENSOR_START_IT,
    BSP_TIMEx_HALLSENSOR_STOP_IT,
    BSP_TIMEx_HALLSENSOR_START_DMA,
    BSP_TIMEx_HALLSENSOR_STOP_DMA,

    // Timer Complementary Output Compare Functions
    BSP_TIMEx_OCN_START,
    BSP_TIMEx_OCN_STOP,
    BSP_TIMEx_OCN_START_IT,
    BSP_TIMEx_OCN_STOP_IT,
    BSP_TIMEx_OCN_START_DMA,
    BSP_TIMEx_OCN_STOP_DMA,

    // Timer Complementary PWM Functions
    BSP_TIMEx_PWMN_START,       /**< Start extended PWM without DMA */
    BSP_TIMEx_PWMN_STOP,        /**< Stop extended PWM without DMA */
    BSP_TIMEx_PWMN_START_IT,
    BSP_TIMEx_PWMN_STOP_IT,
    BSP_TIMEx_PWMN_START_DMA,   /**< Start extended PWM with DMA */
    BSP_TIMEx_PWMN_STOP_DMA,    /**< Stop extended PWM with DMA */

    // Timer Complementary One Pulse Functions
    BSP_TIMEx_ONEPULSEN_START,
    BSP_TIMEx_ONEPULSEN_STOP,
    BSP_TIMEx_ONEPULSEN_START_IT,
    BSP_TIMEx_ONEPULSEN_STOP_IT,

    // Timer Peripheral Control Functions
    BSP_TIMEx_CONFIGCOMMUTEVENT,
    BSP_TIMEx_CONFIGCOMMUTEVENT_IT,
    BSP_TIMEx_CONFIGCOMMUTEVENT_DMA,
    BSP_TIMEx_MASTERCONFIGSYNCHRONIZATION,
    BSP_TIMEx_CONFIGBREAKDEADTIME,
    BSP_TIMEx_CONFIGBREAKINPUT,
    BSP_TIMEx_REMAPCONFIG,
    BSP_TIMEx_TISELECTION,
    BSP_TIMEx_GROUPCHANNEL5,

    // Timer Extended Periphrals State Functions
    BSP_TIMEx_HALLSENSOR_GETSTATE,
    BSP_TIMEx_GETCHANNELNSTATE,

    // Timer Capture Compare Register Value witout Config Channel Function
    BSP_TIM_SET_COMPARE,        /**< Set timer compare value */
    BSP_TIM_OPERATION_COUNT,
} BSP_TimOP_t;

/**
 * @brief Enumeration for BSP Timer identifiers.
 */
typedef enum _BSP_Tim_t {
    BSP_TIM1 = 1,  ///< Timer 1 Identifier
    BSP_TIM2,      ///< Timer 2 Identifier
    BSP_TIM3,      ///< Timer 3 Identifier
    BSP_TIM4,      ///< Timer 4 Identifier
    BSP_TIM5,      ///< Timer 5 Identifier
    BSP_TIM6,      ///< Timer 6 Identifier
    BSP_TIM7,      ///< Timer 7 Identifier
    BSP_TIM8,      ///< Timer 8 Identifier
    BSP_TIM9,      ///< Timer 9 Identifier
    BSP_TIM10,     ///< Timer 10 Identifier
    BSP_TIM11,     ///< Timer 11 Identifier
    BSP_TIM12,     ///< Timer 12 Identifier
    BSP_TIM13,     ///< Timer 13 Identifier
    BSP_TIM14,     ///< Timer 14 Identifier
    BSP_TIM15,     ///< Timer 15 Identifier
    BSP_TIM16,     ///< Timer 16 Identifier
    BSP_TIM17,     ///< Timer 17 Identifier
    BSP_TIM_COUNT
} BSP_Tim_t;

/**
 * @brief Enumeration for BSP Timer identifiers.
 */
typedef enum _BSP_TimCh_t {
    BSP_TIM_CHANNEL_1 = TIM_CHANNEL_1,  /**< Timer channel 1 */
    BSP_TIM_CHANNEL_2 = TIM_CHANNEL_2,  /**< Timer channel 2 */
    BSP_TIM_CHANNEL_3 = TIM_CHANNEL_3,  /**< Timer channel 3 */
    BSP_TIM_CHANNEL_4 = TIM_CHANNEL_4,  /**< Timer channel 4 */
    BSP_TIM_CHANNEL_5 = TIM_CHANNEL_5,  /**< Timer channel 5 */
    BSP_TIM_CHANNEL_6 = TIM_CHANNEL_6,  /**< Timer channel 6 */
    BSP_TIM_CHANNEL_ALL = TIM_CHANNEL_ALL /**< All timer channels */
} BSP_TimCh_t;

/**
 * @enum BSP_TimCBType_t
 * @brief Timer callback types for various timer events.
 */
typedef enum _BSP_TimCBType_t {
    // Generic Callbacks
    BSP_TIM_PERIOD_ELAPSED_CALLBACK,                  /**< Callback when a timer period has elapsed. */
    BSP_TIM_PERIOD_ELAPSED_HALF_CPLT_CALLBACK,        /**< Callback when half of the timer period has elapsed. */
    BSP_TIM_OC_DELAY_ELAPSED_CALLBACK,                /**< Callback for Output Compare delay elapsed event. */
    BSP_TIM_IC_CAPTURE_CALLBACK,                      /**< Callback for Input Capture event. */
    BSP_TIM_IC_CAPTURE_HALF_CPLT_CALLBACK,            /**< Callback for half completion of Input Capture event. */
    BSP_TIM_PWM_PULSE_FINISHED_CALLBACK,              /**< Callback when a PWM pulse has finished. */
    BSP_TIM_PWM_PULSE_FINISHED_HALF_CPLT_CALLBACK,    /**< Callback when half of a PWM pulse has finished. */
    BSP_TIM_TRIGGER_CALLBACK,                         /**< Callback for timer trigger event. */
    BSP_TIM_TRIGGER_HALF_CPLT_CALLBACK,               /**< Callback for half completion of timer trigger event. */
    BSP_TIM_ERROR_CALLBACK,                           /**< Callback for timer error events. */
    BSP_TIM_REGISTER_CALLBACK,                        /**< Callback for timer register events. */
    BSP_TIM_UNREGISTER_CALLBACK,                      /**< Callback for timer unregister events. */

    // Extended Callbacks
    BSP_TIM_COMMUTE_CALLBACK,                         /**< Extended callback for commute event. */
    BSP_TIM_COMMUTE_HALF_CPLT_CALLBACK,               /**< Extended callback for half completion of commute event. */
    BSP_TIM_BREAK_CALLBACK,                           /**< Extended callback for break event. */
    BSP_TIM_BREAK2_CALLBACK,                          /**< Extended callback for secondary break event. */
    BSP_TIM_CALLBACK_TYPE_COUNT,
} BSP_TimCBType_t;

/**
 * @struct BSP_TimMapp_t
 * @brief Maps BSP Timer enumerations to their corresponding HAL Timer handles.
 */
typedef struct _BSP_TimMap_t {
    BSP_Tim_t timer;                     /**< Enumeration of the timer. */
    TIM_HandleTypeDef* handle;                /**< Pointer to the HAL Timer handle. */
} BSP_TimMap_t;

/**
 * @struct BSP_TimCB_t
 * @brief Manager BSP Timer Custom Callbacks and Parameters.
 */
typedef struct _BSP_TimCB_t {
    BSP_TimCBPtr_t callbacks[BSP_TIM_CALLBACK_TYPE_COUNT];
    void* params[BSP_TIM_CALLBACK_TYPE_COUNT];
} BSP_TimCB_t;

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

/* ------------------- TIMER INTERRUPT ------------------- */
BSP_StatusTypeDef_t BSP_RunTimIT(BSP_Tim_t timer, BSP_TimOP_t operation);
BSP_StatusTypeDef_t BSP_RunTimOCIT(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation);

/* ------------------- TIMER PWM ------------------- */
BSP_StatusTypeDef_t BSP_RunPWM(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation);
BSP_StatusTypeDef_t BSP_StartPWMDMA(BSP_Tim_t timer, uint32_t channel, const uint32_t* pData, uint16_t length, BSP_TimOP_t operation);
BSP_StatusTypeDef_t BSP_StopPWMDMA(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation);
BSP_StatusTypeDef_t BSP_SetTimCompVal(BSP_Tim_t timer, uint32_t channel, uint32_t compareValue);

/* ------------------- TIMER UTILITIES ------------------- */
void BSP_Delay(uint32_t msDelay);
uint32_t BSP_GetTick(void);
int32_t BSP_GetCnt(BSP_Tim_t timer);
void BSP_SetCCRCnt(BSP_Tim_t timer, uint32_t CCRCnt);
void BSP_SetARRCnt(BSP_Tim_t timer, uint32_t ARRCnt);
uint32_t BSP_GetCCRCnt(BSP_Tim_t timer);
uint32_t BSP_GetARRCnt(BSP_Tim_t timer);
void BSP_SetCounterMode(BSP_Tim_t timer, uint32_t counterMode);
BSP_StatusTypeDef_t BSP_RunTimEnc(BSP_Tim_t timer, uint32_t channel, BSP_TimOP_t operation);

/* ------------------- TIMER CALLBACKS ------------------- */
void BSP_SetTimCB(BSP_Tim_t timer, BSP_TimCBType_t callbackType, BSP_TimCBPtr_t callback, void* params);

#endif /* HAL_TIM_MODULE_ENABLED */

#endif /* TIMER_INC_BSP_TIM_H_ */
