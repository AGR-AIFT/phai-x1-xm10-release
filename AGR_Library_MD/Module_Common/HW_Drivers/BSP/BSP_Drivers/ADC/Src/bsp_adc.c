/**
 * @file bsp_adc.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for ADC functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#include "bsp_adc.h"

/** @defgroup ADC ADC
  * @brief ADC HAL BSP module driver
  * @{
  */
#ifdef HAL_ADC_MODULE_ENABLED

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
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2, &hadc2},       // ADC 2
    {BSP_ADC3, &hadc3},       // ADC 3
};
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2, &hadc2},       // ADC 2
    {BSP_ADC3, &hadc3},       // ADC 3
};
#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED

#endif /* SUIT_MINICM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2, &hadc2},       // ADC 2
    {BSP_ADC3, &hadc3},       // ADC 3
};
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2, &hadc2},       // ADC 2
    {BSP_ADC3, &hadc3},       // ADC 3
};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2, &hadc2},       // ADC 2
    {BSP_ADC3, &hadc3},       // ADC 3
};
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MD_ENABLED
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2,   NULL},       // ADC 2
    {BSP_ADC3, &hadc3},       // ADC 3
};
#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED

#endif /* WIDM_ENABLED */

#ifdef KAIST_GRF_BOARD_ENABLED
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2,   NULL},       // ADC 2
    {BSP_ADC3, 	 NULL},       // ADC 3
};
#endif /* KAIST_GRF_BOARD_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
static BSP_ADCMap_t bsp_adcMap[BSP_ADC_COUNT] = {
    {BSP_ADC_COUNT, NULL},    // Dummy entry for index 0
    {BSP_ADC1, &hadc1},       // ADC 1
    {BSP_ADC2, &hadc2},       // ADC 2
    {BSP_ADC3, &hadc3},       // ADC 3
};
#endif /* EXPANSION_BOARD_ENABLED */

static BSP_ADCCB_t bsp_adcCB[BSP_ADC_COUNT];


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static ADC_HandleTypeDef* FindADCHandle(BSP_ADC_t adc);
static BSP_ADC_t FindADCEnum(ADC_HandleTypeDef* hadc);
static void ExecuteADCCB(ADC_HandleTypeDef* hadc, BSP_ADCCBType_t callbackType);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
/**
 * @brief Initialize the specified ADC peripheral.
 * @param adc Identifier for the ADC peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_InitADC(BSP_ADC_t adc) 
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    return (BSP_StatusTypeDef_t)HAL_ADC_Init(hadc);
}

/**
 * @brief Deinitialize the ADC peripheral.
 * @param adc Identifier for the ADC peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_DeInitADC(BSP_ADC_t adc) 
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    return (BSP_StatusTypeDef_t)HAL_ADC_DeInit(hadc);
}

/* ------------------- ADC DEVICE CHECK ------------------- */
/**
 * @brief Retrieve the current ADC state.
 * @param adc Identifier for the ADC peripheral.
 * @return HAL status.
 */
uint32_t BSP_GetStateADC(BSP_ADC_t adc)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    return (BSP_StatusTypeDef_t)HAL_ADC_GetState(hadc);
}

/**
 * @brief Retrieve the current ADC error state.
 * @param adc Identifier for the ADC peripheral.
 * @return HAL status.
 */
uint32_t BSP_GetErrorADC(BSP_ADC_t adc)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);
    
    return (BSP_StatusTypeDef_t)HAL_ADC_GetError(hadc);
}

/* ------------------- ADC CALIBRATION ------------------- */
/**
 * @brief Perform ADC calibration in single-ended or differential mode.
 * @param adc Identifier for the ADC peripheral.
 * @param calibMode Calibration mode (e.g., ADC_CALIB_OFFSET or ADC_CALIB_GAIN).
 * @param singleDiff Single-ended or differential mode (e.g., ADC_SINGLE_ENDED or ADC_DIFFERENTIAL).
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_StartADCExCalib(BSP_ADC_t adc, uint32_t calibMode, uint32_t singleDiff)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    return (BSP_StatusTypeDef_t)HAL_ADCEx_Calibration_Start(hadc, calibMode, singleDiff);
}

/* ------------------- POLLING MODE ------------------- */
/**
 * @brief Perform a blocking ADC operation in polling mode.
 * @param adc ADC device identifier.
 * @param eventType ADC event type (e.g., ADC_EOSMP_EVENT, ADC_AWD_EVENT, etc.).
 * @param timeout timeout duration in millisecond.
 * @param operation ADC operation type (e.g., ADC_POLL_FOR_CONV or ADC_POLL_FOR_EVENT).
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunADCBlock(BSP_ADC_t adc, BSP_ADCOP_t operation)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_ADC_START:
            status = (BSP_StatusTypeDef_t)HAL_ADC_Start(hadc);
            break;
        case BSP_ADC_STOP:
            status = (BSP_StatusTypeDef_t)HAL_ADC_Stop(hadc);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief Perform a blocking ADC operation in polling mode.
 * @param adc ADC device identifier.
 * @param eventType ADC event type (e.g., ADC_EOSMP_EVENT, ADC_AWD_EVENT, etc.).
 * @param timeout timeout duration in millisecond.
 * @param operation ADC operation type (e.g., ADC_POLL_FOR_CONV or ADC_POLL_FOR_EVENT).
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunADCConvBlock(BSP_ADC_t adc, uint32_t timeout)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    return (BSP_StatusTypeDef_t)HAL_ADC_PollForConversion(hadc, timeout);
}

/**
 * @brief Perform a blocking ADC operation in polling mode.
 * @param adc ADC device identifier.
 * @param eventType ADC event type (e.g., ADC_EOSMP_EVENT, ADC_AWD_EVENT, etc.).
 * @param timeout timeout duration in millisecond.
 * @param operation ADC operation type (e.g., ADC_POLL_FOR_CONV or ADC_POLL_FOR_EVENT).
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunADCEventBlock(BSP_ADC_t adc, uint32_t eventType, uint32_t timeout)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    return (BSP_StatusTypeDef_t)HAL_ADC_PollForEvent(hadc, eventType, timeout);
}

/* ------------------- NON-BLOCKING MODE WITH IT ------------------- */
/**
 * @brief Perform a non-blocking ADC operation in interrupt mode.
 * @param adc ADC device identifier.
 * @param operation ADC operation type.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunADCIT(BSP_ADC_t adc, BSP_ADCOP_t operation)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_ADC_START_IT:
            status = (BSP_StatusTypeDef_t)HAL_ADC_Start_IT(hadc);
            break;
        case BSP_ADC_STOP_IT:
            status = (BSP_StatusTypeDef_t)HAL_ADC_Stop_IT(hadc);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- NON-BLOCKING MODE WITH DMA ------------------- */
/**
 * @brief Perform a non-blocking ADC operation with DMA.
 * @param adc ADC device identifier.
 * @param pData Pointer to data buffer.
 * @param length Amount of data to be processed.
 * @param operation ADC operation type.
 * @return HAL status.
 * ! TODO: ADC Start DMA Circular mode
 */
BSP_StatusTypeDef_t BSP_RunADCDMA(BSP_ADC_t adc, uint32_t** ppData, uint32_t length, BSP_ADCOP_t operation)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_ADC_START_DMA:
            status = (BSP_StatusTypeDef_t)HAL_ADC_Start_DMA(hadc, *ppData, length);
            break;
        case BSP_ADC_STOP_DMA:
            status = (BSP_StatusTypeDef_t)HAL_ADC_Stop_DMA(hadc);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- GET VALUE ------------------- */
/**
 * @brief Retrieve the latest ADC conversion result.
 * @param adc ADC device identifier.
 * @return The latest ADC conversion result.
 */
uint32_t BSP_GetADCValue (BSP_ADC_t adc)
{
    ADC_HandleTypeDef* hadc = FindADCHandle(adc);

    return HAL_ADC_GetValue(hadc);
}

/* ------------------- ADC CALLBACKS ------------------- */
/**
 * @brief Registers a custom callback for a specified ADC port and callback type.
 *
 * @param adc Enum value representing the ADC port.
 * @param callbackType Enum value representing the type of callback (e.g., transmit complete, receive complete).
 * @param callback Function pointer to the custom callback function.
 * @param params Pointer to any parameters that the custom callback function may need.
 */
void BSP_SetADCCB(BSP_ADC_t adc, BSP_ADCCBType_t callbackType, BSP_ADCCBPtr_t callback, void* params)
{
    if (adc < BSP_ADC_COUNT && callbackType < BSP_ADC_CALLBACK_TYPE_COUNT && callback) {
        bsp_adcCB[adc].callbacks[callbackType] = callback;
        bsp_adcCB[adc].params[callbackType] = params;
    }
}

/**
 * @brief Callback executed when the ADC conversion is completed.
 * @param hadc HAL ADC handle.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    UNUSED(hadc);

   	ExecuteADCCB(hadc, BSP_ADC_CONV_CPLT_CALLBACK);
}

/**
 * @brief Callback executed when half of the ADC conversion is completed.
 * @param hadc HAL ADC handle.
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    UNUSED(hadc);

   	ExecuteADCCB(hadc, BSP_ADC_CONV_HALF_CPLT_CALLBACK);
}

/**
 * @brief Callback executed when ADC conversion level goes out of the set window.
 * @param hadc HAL ADC handle.
 */
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc)
{
    UNUSED(hadc);

   	ExecuteADCCB(hadc, BSP_ADC_LEVEL_OUT_OF_WINDOW_CALLBACK);
}

/**
 * @brief Callback executed when an ADC error occurs.
 * @param hadc HAL ADC handle.
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc)
{
    UNUSED(hadc);

   	ExecuteADCCB(hadc, BSP_ADC_ERROR_CALLBACK);
}

/**
 * @brief Callback executed at the end of ADC sampling.
 * @param hadc HAL ADC handle.
 */
void HAL_ADCEx_EndOfSamplingCallback(ADC_HandleTypeDef* hadc)
{
    UNUSED(hadc);

   	ExecuteADCCB(hadc, BSP_ADCEx_END_OF_SAMPLING_CALLBACK);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Execute an ADC callback
 * @param hadc HAL adc handle.
 * @param callbackType Callback type
 */
static void ExecuteADCCB(ADC_HandleTypeDef* hadc, BSP_ADCCBType_t callbackType) 
{
    BSP_ADC_t adc = FindADCEnum(hadc);
    if (adc < BSP_ADC_COUNT) {
        BSP_ADCCBPtr_t callback = bsp_adcCB[adc].callbacks[callbackType];
        void* params = bsp_adcCB[adc].params[callbackType];
        if (callback) {
            callback(params); // Executes the custom callback with the specified parameters
        }
    }
}

/**
 * @brief Find the ADC handle corresponding to the specified ADC enumeration
 * @param adc ADC enumeration
 * @return Pointer to ADC handle or NULL if not found
 */
static ADC_HandleTypeDef* FindADCHandle(BSP_ADC_t adc) 
{
    for (int i = 0; i < ARRAY_SIZE(bsp_adcMap); i++) {
        if (bsp_adcMap[i].adc == adc) {
            return bsp_adcMap[i].handle;
        }
    }
    return NULL;
}

static BSP_ADC_t FindADCEnum(ADC_HandleTypeDef* hadc)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_adcMap); i++) {
        if (bsp_adcMap[i].handle == hadc) {
            return bsp_adcMap[i].adc;
        }
    }
    return BSP_ERROR;
}


#endif /* HAL_ADC_MODULE_ENABLED */
