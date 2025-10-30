

#include "ioif_adc_common.h"

/** @defgroup ADC ADC
  * @brief ADC BSP module driver
  * @{
  */
#ifdef BSP_ADC_MODULE_ENABLED

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
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // 0 => CM Board Total Current Sensor, 1 => Hip Left Depth ADC, 2 => Hip Right Length ADC
static uint16_t adc2DmaBuff[IOIF_ADC2_BUFFER_LENGTH] __attribute__((section(".adc2DmaBuff"))) = {0}; // 0 => Hip Right Depth ADC
static uint16_t adc3DmaBuff[IOIF_ADC3_BUFFER_LENGTH] __attribute__((section(".adc3DmaBuff"))) = {0}; // 0 => Hip Left Length ADC
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // 0 => CM Board Total Current Sensor, 1 => Hip Left Depth ADC, 2 => Hip Right Length ADC
static uint16_t adc2DmaBuff[IOIF_ADC2_BUFFER_LENGTH] __attribute__((section(".adc2DmaBuff"))) = {0}; // 0 => Hip Right Depth ADC
static uint16_t adc3DmaBuff[IOIF_ADC3_BUFFER_LENGTH] __attribute__((section(".adc3DmaBuff"))) = {0}; // 0 => Hip Left Length ADC
#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED
// TODO : Please Update!
#endif /* SUIT_MINICM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // 0,1,2 => UVW Phase Current / 3,4,5 => Back EMF Voltage
static uint16_t adc3DmaBuff[IOIF_ADC3_BUFFER_LENGTH] __attribute__((section(".adc3DmaBuff"))) = {0}; // 0 => NTC, 1 => FSR, 2 => Linear Potentiometer(10Kohm)
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // 0,1,2 => UVW Phase Current / 3,4,5 => Back EMF Voltage
static uint16_t adc3DmaBuff[IOIF_ADC3_BUFFER_LENGTH] __attribute__((section(".adc3DmaBuff"))) = {0}; // 0 => NTC, 1 => FSR, 2 => Linear Potentiometer(10Kohm)
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // 0,1,2 => UVW Phase Current / 3,4,5 => Back EMF Voltage
static uint16_t adc3DmaBuff[IOIF_ADC3_BUFFER_LENGTH] __attribute__((section(".adc3DmaBuff"))) = {0}; // 0 => NTC, 1 => FSR, 2 => Linear Potentiometer(10Kohm)
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MD_ENABLED
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // 0,1,2 => UVW Phase Current / 3,4,5 => Back EMF Voltage
static uint16_t adc3DmaBuff[IOIF_ADC3_BUFFER_LENGTH] __attribute__((section(".adc3DmaBuff"))) = {0}; // 0 => NTC, 1 => GRF Sensor 1, 2 => GRF Sensor 2
#endif /* SUIT_MD_ENABLED */

#ifdef SUIT_WIDM_ENABLED
// TODO : Please Update!
#endif /* SUIT_WIDM_ENABLED */

#ifdef KAIST_GRF_BOARD_ENABLED
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // 0,1,2 => S1, 3,4,5 => S2, 6,7,9 => S3
#endif /* KAIST_GRF_BOARD_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
static uint16_t adc1DmaBuff[IOIF_ADC1_BUFFER_LENGTH] __attribute__((section(".adc1DmaBuff"))) = {0}; // (R_EMG_1, R_EMG_2, L_FSR_1, L_FSR_2) -> TOTAL: 4
static uint16_t adc2DmaBuff[IOIF_ADC2_BUFFER_LENGTH] __attribute__((section(".adc2DmaBuff"))) = {0}; // (R_EMG_3, R_EMG_4) -> TOTAL: 2
static uint16_t adc3DmaBuff[IOIF_ADC3_BUFFER_LENGTH] __attribute__((section(".adc3DmaBuff"))) = {0}; // (L_FSR_3, L_FSR_4, L_EMG_3, L_EMG_1, R_FSR_3, R_FSR_1, L_EMG_4, L_EMG_2, R_FSR_4, R_FSR_2) -> TOTAL: 10
#endif /* EXPANSION_BOARD_ENABLED */

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

/**
 * @brief Start the specified ADC in block mode.
 * 
 * @param adc The ADC to start.
 * 
 * @return IOIF_ADC_STATUS_ERROR if the ADC is invalid.
 *         Otherwise, it returns the status from BSP_RunADCBlock.
 */
IOIF_ADCState_t IOIF_StartADCBlock(IOIF_ADC_t adc)
{
    if (adc >= IOIF_ADC_COUNT) {
        return IOIF_ADC_STATUS_ERROR; // Invalid adc
    }

    return BSP_RunADCBlock(adc, BSP_ADC_START);
}

/**
 * @brief Stop the specified ADC in block mode.
 * 
 * @param adc The ADC to stop.
 * 
 * @return IOIF_ADC_STATUS_ERROR if the ADC is invalid.
 *         Otherwise, it returns the status from BSP_RunADCBlock.
 */
IOIF_ADCState_t IOIF_StopADCBlock(IOIF_ADC_t adc)
{
    if (adc >= IOIF_ADC_COUNT) {
        return IOIF_ADC_STATUS_ERROR; // Invalid adc
    }

    return BSP_RunADCBlock(adc, BSP_ADC_STOP);
}

/**
 * @brief Run ADC conversion in block mode with a timeout.
 * 
 * @param adc The ADC for the conversion.
 * @param timeout The timeout value for the conversion.
 * 
 * @return IOIF_ADC_STATUS_ERROR if the ADC is invalid.
 *         Otherwise, it returns the status from BSP_RunADCConvBlock.
 */
IOIF_ADCState_t IOIF_RunADCConvBlock(IOIF_ADC_t adc, uint32_t timeout)
{
    if (adc >= IOIF_ADC_COUNT) {
        return IOIF_ADC_STATUS_ERROR; // Invalid adc
    }

    return BSP_RunADCConvBlock(adc, timeout);
}

/**
 * @brief Get the ADC value.
 * 
 * @param adc The ADC to get the value from.
 * @param pData Pointer to store the ADC value.
 * 
 * @return IOIF_ADC_STATUS_ERROR if the ADC is invalid or if pData is NULL.
 *         IOIF_ADC_STATUS_OK otherwise.
 */
IOIF_ADCState_t IOIF_GetADCVal(IOIF_ADC_t adc, uint32_t* pData)
{
    if (adc >= IOIF_ADC_COUNT || pData == NULL) {
        return IOIF_ADC_STATUS_ERROR; // Invalid adc or NULL pointer
    }

    *pData = BSP_GetADCValue(adc);
    return IOIF_ADC_STATUS_OK;
}

/**
 * @brief Start the ADC with DMA using the given parameters.
 * 
 * This function configures the ADC with the specified settings and starts it
 * using DMA. Before starting, it checks for the validity of the ADC and the pointer.
 *
 * @param adc The ADC to start. Its value should be less than IOIF_ADC_COUNT.
 * @param pData Pointer to the buffer where the ADC data will be stored.
 * @param length The number of data to be read from the ADC.
 * 
 * @return IOIF_ADC_STATUS_ERROR if there's an error (invalid ADC or NULL pointer).
 *         Otherwise, it returns the status from BSP_RunADCDMA.
 */
IOIF_ADCState_t IOIF_StartADCDMA(IOIF_ADC_t adc, uint16_t** ppData, uint32_t length)
{
    // Check the validity
    if (adc >= IOIF_ADC_COUNT || ppData == NULL || length == 0) {
        return IOIF_ADC_STATUS_ERROR; // Invalid adc or NULL pointer
    }

    switch(adc) {
        case IOIF_ADC1:
        	*ppData = adc1DmaBuff;
            break;
        case IOIF_ADC2:
        	*ppData = adc2DmaBuff;
        	break;
        case IOIF_ADC3:
        	*ppData = adc3DmaBuff;
            break;
        default:
        	return IOIF_ADC_STATUS_ERROR;
    }

    uint8_t status = BSP_StartADCExCalib(adc, BSP_ADC_CALIB_OFFSET, BSP_ADC_SINGLE_ENDED);
    
    if (status != IOIF_ADC_STATUS_OK) {
        return status; // Return the error status from BSP_StartADCExCalib
    }

    return BSP_RunADCDMA(adc, (uint32_t**)ppData, length, BSP_ADC_START_DMA);
}

/**
 * @brief Stop the ADC using DMA for the specified ADC.
 * 
 * @param adc The ADC to stop.
 * 
 * @return IOIF_ADC_STATUS_ERROR if the ADC is invalid.
 *         Otherwise, it returns the status from BSP_RunADCDMA.
 */
IOIF_ADCState_t IOIF_StopADCDMA(IOIF_ADC_t adc)
{
    // Check the validity of the adc
    if (adc >= IOIF_ADC_COUNT) {
        return IOIF_ADC_STATUS_ERROR; // Invalid adc
    }

    return BSP_RunADCDMA(adc, NULL, 0, BSP_ADC_STOP_DMA);
}

/**
 * @brief Set a callback for the specified ADC event.
 * 
 * This function sets a callback for a specific ADC event. The callback can be used
 * for various purposes such as processing ADC data or handling ADC errors.
 *
 * @param adc The ADC for which the callback is set.
 * @param callbackType The type of ADC event for which the callback is set.
 * @param callback The callback function to set.
 * @param params Additional parameters for the callback.
 * 
 * @return IOIF_ADC_STATUS_ERROR if the ADC, callbackType, or callback is invalid.
 *         IOIF_ADC_STATUS_OK otherwise.
 */
IOIF_ADCState_t IOIF_SetADCCB(IOIF_ADC_t adc, IOIF_ADCCBType_t callbackType, IOIF_ADCCBPtr_t callback, void* params)
{
    // Check the validity of the adc
    if (adc >= IOIF_ADC_COUNT) {
        return IOIF_ADC_STATUS_ERROR; // Invalid adc
    }

    // Check the validity of the callbackType
    if (callbackType >= IOIF_ADC_CALLBACK_TYPE_COUNT) {
        return IOIF_ADC_STATUS_ERROR; // Invalid callbackType
    }

    // Check if the callback pointer is valid
    if (!callback) {
        return IOIF_ADC_STATUS_ERROR; // Invalid callback pointer
    }

    // Map IOIF enums, parameters, callback to BSP equivalents if necessary
    BSP_SetADCCB((BSP_ADC_t)(adc), (BSP_ADCCBType_t)(callbackType), (BSP_ADCCBPtr_t)(callback), params);

    return IOIF_ADC_STATUS_OK;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* BSP_ADC_MODULE_ENABLED */
