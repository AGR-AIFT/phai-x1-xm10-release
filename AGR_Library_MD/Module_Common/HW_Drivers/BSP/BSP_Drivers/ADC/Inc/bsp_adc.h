/**
 * @file bsp_adc.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for ADC functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef ADC_INC_BSP_ADC_H_
#define ADC_INC_BSP_ADC_H_

#include "main.h"
#include "module.h"

/** @defgroup ADC ADC
  * @brief ADC HAL BSP module driver
  * @
  */
#ifdef HAL_ADC_MODULE_ENABLED
#define BSP_ADC_MODULE_ENABLED

#include "adc.h"
#include "bsp_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define ADC_DMA_BUFF_SIZE 3

/**
 * @brief ADC Calibration Mode Definitions
 */
#define BSP_ADC_CALIB_OFFSET ADC_CALIB_OFFSET
#define BSP_ADC_CALIB_OFFSET_LINEARITY ADC_CALIB_OFFSET_LINEARITY

/**
 * @brief ADC Channel Ending Mode Definitions
 */
#define BSP_ADC_SINGLE_ENDED ADC_SINGLE_ENDED
#define BSP_ADC_DIFFERENTIAL_ENDED ADC_DIFFERENTIAL_ENDED

/**
 * @defgroup ADC event type Definitions
 * @{
 */
#define BSP_ADC_EOSMP_EVENT ADC_EOSMP_EVENT /**< ADC End of Sampling event */
#define BSP_ADC_AWD1_EVENT  ADC_AWD1_EVENT  /**< ADC Analog watchdog 1 event (main analog watchdog, present on all STM32 devices) */
#define BSP_ADC_AWD2_EVENT  ADC_AWD2_EVENT  /**< ADC Analog watchdog 2 event (additional analog watchdog, not present on all STM32 families) */
#define BSP_ADC_AWD3_EVENT  ADC_AWD3_EVENT  /**< ADC Analog watchdog 3 event (additional analog watchdog, not present on all STM32 families) */
#define BSP_ADC_OVR_EVENT   ADC_OVR_EVENT   /**< ADC Overrun event */
#define BSP_ADC_JQOVF_EVENT ADC_JQOVF_EVENT /**<  ADC Injected context queue overflow event */


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Function pointer type for ADC callback functions.
 */
typedef void (*BSP_ADCCBPtr_t)(void* params);

/**
 * @brief BSP_ADC_CH_X DEFINITIONS ADC Channel Definitions
 * @{
 */
typedef enum _BSP_ADCCh_t {
    BSP_ADC_CH_0 = ADC_CHANNEL_0,  /**< ADC channel 0 */
    BSP_ADC_CH_1 = ADC_CHANNEL_1,  /**< ADC channel 1 */
    BSP_ADC_CH_2 = ADC_CHANNEL_2,  /**< ADC channel 2 */
    BSP_ADC_CH_3 = ADC_CHANNEL_3,  /**< ADC channel 3 */
    BSP_ADC_CH_4 = ADC_CHANNEL_4,  /**< ADC channel 4 */
    BSP_ADC_CH_5 = ADC_CHANNEL_5,  /**< ADC channel 5 */
    BSP_ADC_CH_6 = ADC_CHANNEL_6,  /**< ADC channel 6 */
    BSP_ADC_CH_7 = ADC_CHANNEL_7,  /**< ADC channel 7 */
    BSP_ADC_CH_8 = ADC_CHANNEL_8,  /**< ADC channel 8 */
    BSP_ADC_CH_9 = ADC_CHANNEL_9,  /**< ADC channel 9 */
    BSP_ADC_CH_10 = ADC_CHANNEL_10,    /**< ADC channel 10 */
    BSP_ADC_CH_11 = ADC_CHANNEL_11,    /**< ADC channel 11 */
    BSP_ADC_CH_12 = ADC_CHANNEL_12,    /**< ADC channel 12 */
    BSP_ADC_CH_13 = ADC_CHANNEL_13,    /**< ADC channel 13 */
    BSP_ADC_CH_14 = ADC_CHANNEL_14,    /**< ADC channel 14 */
    BSP_ADC_CH_15 = ADC_CHANNEL_15,    /**< ADC channel 15 */
    BSP_ADC_CH_16 = ADC_CHANNEL_16,    /**< ADC channel 16 */
    BSP_ADC_CH_17 = ADC_CHANNEL_17,    /**< ADC channel 17 */
    BSP_ADC_CH_18 = ADC_CHANNEL_18,    /**< ADC channel 18 */
    BSP_ADC_CH_19 = ADC_CHANNEL_19,    /**< ADC channel 19 */
    BSP_ADC_CH_VREFINT = ADC_CHANNEL_VREFINT,           /**< ADC channel VREFINT */
    BSP_ADC_CH_TEMPSENSOR = ADC_CHANNEL_TEMPSENSOR,     /**< ADC channel TEMPSENSOR */
    BSP_ADC_CH_VBAT = ADC_CHANNEL_VBAT,                 /**< ADC channel VBAT */
//    BSP_ADC_CH_DAC1CH1_ADC2 = ADC_CHANNEL_DAC1CH1_ADC2,    /**< ADC channel DAC1CH1_ADC2 */
//    BSP_ADC_CH_DAC1CH2_ADC2 = ADC_CHANNEL_DAC1CH2_ADC2    /**< ADC channel DAC1CH2_ADC2 */
} BSP_ADCCh_t;

/**
 * @brief Enumeration for BSP ADC operations.
 * Represents the various adc-related operations available
 * within the BSP layer, abstracting HAL functions.
 */
typedef enum _BSP_ADCOP_t {
    // Generic Driver
    BSP_ADC_START,
    BSP_ADC_STOP,
    BSP_ADC_POLL_FOR_CONV,
    BSP_ADC_POLL_FOR_EVENT,
    BSP_ADC_START_IT,
    BSP_ADC_STOP_IT,
    BSP_ADC_START_DMA,
    BSP_ADC_STOP_DMA,
    BSP_ADC_GET_VALUE,

    // Extension Driver
    BSP_ADCEx_CALIBRATION_START,
    BSP_ADCEx_CALIBRATION_GETVALUE,
    BSP_ADCEx_LINEAR_CALIBRATION_GETVALUE,
    BSP_ADCEx_CALIBRATION_SETVALUE,
    BSP_ADCEx_LINEAR_CALIBRATION_SETVALUE,
    BSP_ADCEx_LINEAR_CALIBRATION_FACTORLOAD,
    BSP_ADCEx_INJECTED_START,
    BSP_ADCEx_INJECTED_STOP,
    BSP_ADCEx_INJECTED_POLL_FOR_CONV,
    BSP_ADCEx_INJECTED_START_IT,
    BSP_ADCEx_INJECTED_STOP_IT,
    BSP_ADCEx_MULTIMODE_START_DMA,
    BSP_ADCEx_MULTIMODE_STOP_DMA,
    BSP_ADCEx_MULTIMODE_GETVALUE,
    BSP_ADCEx_INJECTED_GETVALUE,
    BSP_ADCEx_REGULAR_STOP,
    BSP_ADCEx_REGULAR_STOP_IT,
    BSP_ADCEx_REGULAR_STOP_DMA,
    BSP_ADCEx_REGULAR_MULTIMODE_STOP_DMA,
    BSP_ADCEx_INJECTED_CONFIG_CHANNEL,
    BSP_ADCEx_MULTIMODE_CONFIG_CHANNEL,
    BSP_ADCEx_ENABLE_INJECTED_QUEUE,
    BSP_ADCEx_DISABLE_INJECTED_QUEUE,
    BSP_ADCEx_DISABLE_VOLT_REGULATOR,
    BSP_ADCEx_ENTER_ADC_DEEP_POWER_DOWN_MODE,

    BSP_ADC_OP_COUNT,
} BSP_ADCOP_t;

/**
 * @brief Enumeration for BSP ADC identifiers.
 * Starts from 1 to align with common STM32 naming (ADC1, ADC2, ...)
 */
typedef enum _BSP_ADC_t {
    BSP_ADC1 = 1,  ///< ADC 1 Identifier
    BSP_ADC2,      ///< ADC 2 Identifier
    BSP_ADC3,      ///< ADC 3 Identifier
    BSP_ADC_COUNT,
} BSP_ADC_t;

/**
 * @enum BSP_ADCCBType_t
 * @brief ADC callback types for various adc events.
 */
typedef enum _BSP_ADCCBType_t {
    // Generic Driver Callback types
    BSP_ADC_CONV_CPLT_CALLBACK,
    BSP_ADC_CONV_HALF_CPLT_CALLBACK,
    BSP_ADC_LEVEL_OUT_OF_WINDOW_CALLBACK,
    BSP_ADC_ERROR_CALLBACK,
    BSP_ADC_INJECTED_CONV_CPLT_CALLBACK,
    BSP_ADC_INJECTED_QUEUE_OVERFLOW_CALLBACK,
    BSP_ADC_LEVEL_OUT_OF_WINDOW2_CALLBACK,
    BSP_ADC_LEVEL_OUT_OF_WINDOW3_CALLBACK,
    BSP_ADC_END_OF_SAMPLING_CALLBACK,
    BSP_ADC_MSP_INIT_CALLBACK,
    BSP_ADC_MSP_DEINIT_CALLBACK,

    // DMA Callback types
    BSP_ADC_DMA_CONV_CPLT_CALLBACK,
    BSP_ADC_DMA_HALG_CONV_CPLT_CALLBACK,
    BSP_ADC_DMA_ERROR_CALLBACK,

    // Extension Driver Callback types
    BSP_ADCEx_INJECTED_CONV_CPLT_CALLBACK,
    BSP_ADCEx_INJECTED_QUEUE_OVERFLOW_CALLBACK,
    BSP_ADCEx_LEVEL_OUT_OF_WINDOW2_CALLBACK,
    BSP_ADCEx_LEVEL_OUT_OF_WINDOW3_CALLBACK,
    BSP_ADCEx_END_OF_SAMPLING_CALLBACK,
    BSP_ADC_CALLBACK_TYPE_COUNT,
} BSP_ADCCBType_t;

/**
 * @struct BSP_ADCMap_t
 * @brief Maps BSP ADC enumerations to their corresponding HAL ADC handles.
 */
typedef struct _BSP_ADCMap_t {
    BSP_ADC_t adc;       ///< Enumeration of the adcer
    ADC_HandleTypeDef *handle;  ///< Pointer to the HAL ADC handle
} BSP_ADCMap_t;

/**
 * @struct BSP_ADCCB_t
 * @brief Manager BSP ADC Custom Callbacks and Parameters.
 */
typedef struct _BSP_ADCCB_t {
    BSP_ADCCBPtr_t callbacks[BSP_ADC_CALLBACK_TYPE_COUNT];
    void* params[BSP_ADC_CALLBACK_TYPE_COUNT];
} BSP_ADCCB_t;


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

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
BSP_StatusTypeDef_t BSP_InitADC(BSP_ADC_t adc);
BSP_StatusTypeDef_t BSP_DeInitADC(BSP_ADC_t adc);

/* ------------------- ADC DEVICE CHECK ------------------- */
uint32_t BSP_GetStateADC(BSP_ADC_t adc);
uint32_t BSP_GetErrorADC(BSP_ADC_t adc);

/* ------------------- ADC CALIBRATION ------------------- */
BSP_StatusTypeDef_t BSP_StartADCExCalib(BSP_ADC_t adc, uint32_t calibMode, uint32_t singleDiff);

/* ------------------- POLLING MODE ------------------- */
BSP_StatusTypeDef_t BSP_RunADCBlock(BSP_ADC_t adc, BSP_ADCOP_t operation);
BSP_StatusTypeDef_t BSP_RunADCConvBlock(BSP_ADC_t adc, uint32_t timeout);
BSP_StatusTypeDef_t BSP_RunADCEventBlock(BSP_ADC_t adc, uint32_t eventType, uint32_t timeout);

/* ------------------- NON-BLOCKING MODE WITH IT ------------------- */
BSP_StatusTypeDef_t BSP_RunADCIT(BSP_ADC_t adc, BSP_ADCOP_t operation);

/* ------------------- NON-BLOCKING MODE WITH DMA ------------------- */
BSP_StatusTypeDef_t BSP_RunADCDMA(BSP_ADC_t adc, uint32_t** ppData, uint32_t length, BSP_ADCOP_t operation);

/* ------------------- GET VALUE ------------------- */
uint32_t BSP_GetADCValue(BSP_ADC_t adc);

/* ------------------- ADC CALLBACKS ------------------- */
void BSP_SetADCCB(BSP_ADC_t adc, BSP_ADCCBType_t callbackType, BSP_ADCCBPtr_t callback, void* params);


#endif /* HAL_ADC_MODULE_ENABLED */

#endif /* ADC_INC_BSP_ADC_H_ */
