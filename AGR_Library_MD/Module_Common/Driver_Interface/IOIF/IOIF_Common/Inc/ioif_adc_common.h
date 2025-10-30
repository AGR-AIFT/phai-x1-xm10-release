

#ifndef IOIF_COMMON_INC_IOIF_ADC_COMMON_H_
#define IOIF_COMMON_INC_IOIF_ADC_COMMON_H_

#include "module.h"
#include "bsp_adc.h"




/** @defgroup ADC ADC
  * @brief ADC BSP module driver
  * @{
  */
#ifdef BSP_ADC_MODULE_ENABLED

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#ifdef WALKON5_CM_ENABLED
#define IOIF_ADC1_BUFFER_LENGTH     3
#define IOIF_ADC2_BUFFER_LENGTH     1
#define IOIF_ADC3_BUFFER_LENGTH     1
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
#define IOIF_ADC1_BUFFER_LENGTH     3
#define IOIF_ADC2_BUFFER_LENGTH     1
#define IOIF_ADC3_BUFFER_LENGTH     1
#endif /* L30_CM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
#define IOIF_ADC1_BUFFER_LENGTH     6
#define IOIF_ADC2_BUFFER_LENGTH     1
#define IOIF_ADC3_BUFFER_LENGTH     3
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
#define IOIF_ADC1_BUFFER_LENGTH     6
#define IOIF_ADC2_BUFFER_LENGTH     1
#define IOIF_ADC3_BUFFER_LENGTH     3
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
#define IOIF_ADC1_BUFFER_LENGTH     6
#define IOIF_ADC2_BUFFER_LENGTH     1
#define IOIF_ADC3_BUFFER_LENGTH     3
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MINICM_ENABLED

#endif /* SUIT_MINICM_ENABLED */

#ifdef SUIT_MD_ENABLED
#define IOIF_ADC1_BUFFER_LENGTH     6
#define IOIF_ADC2_BUFFER_LENGTH     1
#define IOIF_ADC3_BUFFER_LENGTH     3
#endif /* SUIT_MD_ENABLED */

#ifdef SUIT_WIDM_ENABLED

#endif /* SUIT_WIDM_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
#define IOIF_ADC1_BUFFER_LENGTH     4
#define IOIF_ADC2_BUFFER_LENGTH     2
#define IOIF_ADC3_BUFFER_LENGTH     10
#endif /* EXPANSION_BOARD_ENABLED */

#define IOIF_FSR_VREF          5
#define IOIF_FSR_RESOLUTION    4095
#define IOIF_EMG_VREF          5
#define IOIF_EMG_RESOLUTION    4095


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Function pointer type for ADC callback functions.
 */
typedef void (*IOIF_ADCCBPtr_t)(void* params);

typedef enum _IOIF_ADCState_t {
    IOIF_ADC_STATUS_OK = 0,
    IOIF_ADC_STATUS_ERROR,
} IOIF_ADCState_t;

/**
 * @brief IOIF_ADC_CH_X DEFINITIONS ADC Channel Definitions
 * @{
 */
typedef enum _IOIF_ADCCh_t {
    IOIF_ADC_CH_0 = BSP_ADC_CH_0,  /**< ADC channel 0 */
    IOIF_ADC_CH_1 = BSP_ADC_CH_1,  /**< ADC channel 1 */
    IOIF_ADC_CH_2 = BSP_ADC_CH_2,  /**< ADC channel 2 */
    IOIF_ADC_CH_3 = BSP_ADC_CH_3,  /**< ADC channel 3 */
    IOIF_ADC_CH_4 = BSP_ADC_CH_4,  /**< ADC channel 4 */
    IOIF_ADC_CH_5 = BSP_ADC_CH_5,  /**< ADC channel 5 */
    IOIF_ADC_CH_6 = BSP_ADC_CH_6,  /**< ADC channel 6 */
    IOIF_ADC_CH_7 = BSP_ADC_CH_7,  /**< ADC channel 7 */
    IOIF_ADC_CH_8 = BSP_ADC_CH_8,  /**< ADC channel 8 */
    IOIF_ADC_CH_9 = BSP_ADC_CH_9,  /**< ADC channel 9 */
    IOIF_ADC_CH_10 = BSP_ADC_CH_10,    /**< ADC channel 10 */
    IOIF_ADC_CH_11 = BSP_ADC_CH_11,    /**< ADC channel 11 */
    IOIF_ADC_CH_12 = BSP_ADC_CH_12,    /**< ADC channel 12 */
    IOIF_ADC_CH_13 = BSP_ADC_CH_13,    /**< ADC channel 13 */
    IOIF_ADC_CH_14 = BSP_ADC_CH_14,    /**< ADC channel 14 */
    IOIF_ADC_CH_15 = BSP_ADC_CH_15,    /**< ADC channel 15 */
    IOIF_ADC_CH_16 = BSP_ADC_CH_16,    /**< ADC channel 16 */
    IOIF_ADC_CH_17 = BSP_ADC_CH_17,    /**< ADC channel 17 */
    IOIF_ADC_CH_18 = BSP_ADC_CH_18,    /**< ADC channel 18 */
    IOIF_ADC_CH_19 = BSP_ADC_CH_19,    /**< ADC channel 19 */
    IOIF_ADC_CH_VREFINT = BSP_ADC_CH_VREFINT,           /**< ADC channel VREFINT */
    IOIF_ADC_CH_TEMPSENSOR = BSP_ADC_CH_TEMPSENSOR,     /**< ADC channel TEMPSENSOR */
    IOIF_ADC_CH_VBAT = BSP_ADC_CH_VBAT,                 /**< ADC channel VBAT */
//    IOIF_ADC_CH_DAC1CH1_ADC2 = BSP_ADC_CH_DAC1CH1_ADC2,    /**< ADC channel DAC1CH1_ADC2 */
//    IOIF_ADC_CH_DAC1CH2_ADC2 = BSP_ADC_CH_DAC1CH2_ADC2    /**< ADC channel DAC1CH2_ADC2 */
} IOIF_ADCCh_t;

/**
 * @brief Enumeration for IOIF ADC identifiers.
 * Starts from 1 to align with common STM32 naming (ADC1, ADC2, ...)
 */
typedef enum _IOIF_ADC_t {
    IOIF_ADC1 = 1,  ///< ADC 1 Identifier
    IOIF_ADC2,      ///< ADC 2 Identifier
    IOIF_ADC3,      ///< ADC 3 Identifier
    IOIF_ADC_COUNT,
} IOIF_ADC_t;

/**
 * @enum IOIF_ADCCBType_t
 * @brief ADC callback types for various adc events.
 */
typedef enum _IOIF_ADCCBType_t {
    // Generic Driver Callback types
    IOIF_ADC_CONV_CPLT_CALLBACK,
    IOIF_ADC_CONV_HALF_CPLT_CALLBACK,
    IOIF_ADC_LEVEL_OUT_OF_WINDOW_CALLBACK,
    IOIF_ADC_ERROR_CALLBACK,
    IOIF_ADC_INJECTED_CONV_CPLT_CALLBACK,
    IOIF_ADC_INJECTED_QUEUE_OVERFLOW_CALLBACK,
    IOIF_ADC_LEVEL_OUT_OF_WINDOW2_CALLBACK,
    IOIF_ADC_LEVEL_OUT_OF_WINDOW3_CALLBACK,
    IOIF_ADC_END_OF_SAMPLING_CALLBACK,
    IOIF_ADC_MSP_INIT_CALLBACK,
    IOIF_ADC_MSP_DEINIT_CALLBACK,

    // DMA Callback types
    IOIF_ADC_DMA_CONV_CPLT_CALLBACK,
    IOIF_ADC_DMA_HALG_CONV_CPLT_CALLBACK,
    IOIF_ADC_DMA_ERROR_CALLBACK,

    // Extension Driver Callback types
    IOIF_ADCEx_INJECTED_CONV_CPLT_CALLBACK,
    IOIF_ADCEx_INJECTED_QUEUE_OVERFLOW_CALLBACK,
    IOIF_ADCEx_LEVEL_OUT_OF_WINDOW2_CALLBACK,
    IOIF_ADCEx_LEVEL_OUT_OF_WINDOW3_CALLBACK,
    IOIF_ADCEx_END_OF_SAMPLING_CALLBACK,
    IOIF_ADC_CALLBACK_TYPE_COUNT,
} IOIF_ADCCBType_t;



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

IOIF_ADCState_t IOIF_StartADCBlock(IOIF_ADC_t adc);
IOIF_ADCState_t IOIF_StopADCBlock(IOIF_ADC_t adc);
IOIF_ADCState_t IOIF_RunADCConvBlock(IOIF_ADC_t adc, uint32_t timeout);
IOIF_ADCState_t IOIF_GetADCVal(IOIF_ADC_t adc, uint32_t* pData);
IOIF_ADCState_t IOIF_StartADCDMA(IOIF_ADC_t adc, uint16_t** ppData, uint32_t length);
IOIF_ADCState_t IOIF_StopADCDMA(IOIF_ADC_t adc);
IOIF_ADCState_t IOIF_SetADCCB(IOIF_ADC_t adc, IOIF_ADCCBType_t callbackType, IOIF_ADCCBPtr_t callback, void* params);


#endif /* BSP_ADC_MODULE_ENABLED */

#endif /* IOIF_COMMON_INC_IOIF_ADC_COMMON_H_ */
