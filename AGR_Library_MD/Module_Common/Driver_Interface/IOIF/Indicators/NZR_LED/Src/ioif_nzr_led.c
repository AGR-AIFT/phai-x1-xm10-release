/**
 *-----------------------------------------------------------
 *                      NZR LED DRIVER
 *-----------------------------------------------------------
 * @file ioif_nzrled.c
 * @date Created on: Sep 2, 2023
 * @author AngelRobotics HW Team
 * @brief Driver code for the NZR LED control.
 *
 * This source file provides functionality to interface
 * with the NZR LED, including initialization, setting color values, 
 * and running or clearing the LED states.
 * 
 * This driver leverages Pulse Width Modulation (PWM) to control the LEDs 
 * and assumes the presence of a hardware timer for accurate PWM signal generation.
 * 
 * @ref ld140.pdf
 */

#include "ioif_nzr_led.h"

/** @defgroup TIM TIM
  * @brief TIM NZR LED module driver
  * @{
  */
#ifdef IOIF_NZRLED_ENABLED

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
static uint32_t nzrDutyTxBuff1[(BYTE_PER_COLOR * COLOR_NUM * LED_NUM_PER_CH) + REST_SIGNAL_BYTE_NUM] __attribute__((section(".nzrLedTxBuff1"))) = {0};
static uint32_t nzrDutyTxBuff2[(BYTE_PER_COLOR * COLOR_NUM * LED_NUM_PER_CH) + REST_SIGNAL_BYTE_NUM] __attribute__((section(".nzrLedTxBuff2"))) = {0};
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
static uint32_t nzrDutyTxBuff1[(BYTE_PER_COLOR * COLOR_NUM * LED_NUM_PER_CH) + REST_SIGNAL_BYTE_NUM +1] __attribute__((section(".nzrLedTxBuff1"))) = {0};
static uint32_t nzrDutyTxBuff2[(BYTE_PER_COLOR * COLOR_NUM * LED_NUM_PER_CH) + REST_SIGNAL_BYTE_NUM +1] __attribute__((section(".nzrLedTxBuff2"))) = {0};
#endif /* L30_CM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static uint32_t nzrDutyTxBuff1[(BYTE_PER_COLOR * COLOR_NUM * LED_NUM_PER_CH) + REST_SIGNAL_BYTE_NUM] __attribute__((section(".nzrLedTxBuff1"))) = {0};
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static uint32_t nzrDutyTxBuff1[(BYTE_PER_COLOR * COLOR_NUM * LED_NUM_PER_CH) + REST_SIGNAL_BYTE_NUM] __attribute__((section(".nzrLedTxBuff1"))) = {0};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static uint32_t nzrDutyTxBuff1[(BYTE_PER_COLOR * COLOR_NUM * LED_NUM_PER_CH) + REST_SIGNAL_BYTE_NUM] __attribute__((section(".nzrLedTxBuff1"))) = {0};
#endif /* L30_MD_REV08_ENABLED */

static IOIF_Tim_t timHandle;


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static void CountTick(IOIF_NzrParams_t* nzrParams);
static void ResetTick(IOIF_NzrParams_t* nzrParams);
static IOIF_NZRLEDState_t SetLedDuty(IOIF_NzrLedObj_t* nzrObj);
static IOIF_NZRLEDState_t GrbwToCode(uint32_t* colorCode, uint8_t gCode, uint8_t rCode, uint8_t bCode, uint8_t wCode);
static IOIF_NZRLEDState_t CodeToGrbw(IOIF_NzrParams_t* nzrParams, uint32_t colorCode);
static void StopNZRDMA(void* params);
static bool IsValColor(uint32_t color);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/**
 * @brief Initialization function for the NZR LED.
 * @param nzrObj The NZR LED object to be initialized.
 */
IOIF_NZRLEDState_t IOIF_InitNZRLED(IOIF_Tim_t timer, IOIF_TimCh_t timCh, IOIF_NzrLedObj_t* nzrObj, uint16_t loopTime)
{
	nzrObj->timCh = timCh;

		// Assign LED IDs
    for (int i = 0; i < LED_NUM_PER_CH; i++) {
        nzrObj->nzrParams[i].nzrId =i;// (uint8_t)i;
    }

    timHandle = timer;
    nzrObj->parentLoopTime = loopTime;

    // Assign the duty buffer
    switch (timCh) {
        case IOIF_TIM_CHANNEL_1:
            nzrObj->pNzrDutyBuff = nzrDutyTxBuff1;
            break;
#if defined(WALKON5_CM_ENABLED) || defined (L30_CM_ENABLED)
        case IOIF_TIM_CHANNEL_2:
            nzrObj->pNzrDutyBuff = nzrDutyTxBuff2;
            break;
#endif
        default:
            // This should be unreachable due to the validation above, but added for safety.
            return IOIF_NZRLED_STATUS_ERROR;
    }
    uint8_t status;
    // Set the PWM callback for stopping the DMA
    BSP_SetTimCB((BSP_Tim_t)timer, BSP_TIM_PWM_PULSE_FINISHED_CALLBACK, StopNZRDMA, NULL);

    // Clear the LED to its initial state
#ifdef L30_CM_ENABLED
    switch (timCh) {
    case IOIF_TIM_CHANNEL_1:
    //status =
    	BSP_StopPWMDMA((BSP_Tim_t)timHandle, BSP_TIM_CHANNEL_1, BSP_TIM_PWM_STOP_DMA);
    /*if (status != BSP_OK) {
		return status;
	}*/
    break;

    case IOIF_TIM_CHANNEL_2:
    //status =
    		BSP_StopPWMDMA((BSP_Tim_t)timHandle, BSP_TIM_CHANNEL_2, BSP_TIM_PWM_STOP_DMA);
  /*  if (status != BSP_OK) {
		return status;
	}*/
    break;
    }
#endif /* L30_CM_ENABLED */

#if defined(L30_MD_REV06_ENABLED) || defined (L30_MD_REV07_ENABLED) || defined (L30_MD_REV08_ENABLED)
	status = BSP_StopPWMDMA((BSP_Tim_t)timHandle, BSP_TIM_CHANNEL_1, BSP_TIM_PWM_STOP_DMA);
    if (status != BSP_OK) {
		return status;
	}
#endif

    return status;
//    uint8_t status = IOIF_ClearNZRLED(timCh, nzrObj);
    	//	if (status != IOIF_NZRLED_STATUS_OK) {
    		//		return status;
//	}
//
//	return status;
}

/**
 * @brief Function to set a specific NZR LED's color.
 * @param nzrObj The NZR LED object.
 * @param ledIndex Index of the LED to be set.
 * @param color The color to be set for the LED.
 */
IOIF_NZRLEDState_t IOIF_SetNZRLED(IOIF_NzrLedObj_t* nzrObj, uint8_t ledIndex, uint32_t color, uint32_t blinkTime)
{
    // Check if the nzrObj and its buffer pointer are valid
  /*  if (!nzrObj) {
        return IOIF_NZRLED_STATUS_ERROR;
    }

    // Check if the ledIndex is valid
    if (ledIndex >= LED_NUM_PER_CH) {
        return IOIF_NZRLED_STATUS_ERROR;
    }

    // Check if the color code is valid
    if (!IsValColor(color)) {
        return IOIF_NZRLED_STATUS_ERROR;
    }
*/
  //  IOIF_ClearNZRLED(nzrObj->timCh, nzrObj);

    // Convert the provided color code to individual GRBW components
    CodeToGrbw(&nzrObj->nzrParams[ledIndex], color);

    nzrObj->nzrParams[ledIndex].colorCode = color;
    nzrObj->nzrParams[ledIndex].blinkTime = blinkTime;

/*    if((2*nzrObj->parentLoopTime) != 0){
    	nzrObj->nzrParams[ledIndex].stepNum = blinkTime/(2 * nzrObj->parentLoopTime);
    }
    else{
    	nzrObj->nzrParams[ledIndex].stepNum = 0;
    }
*/
    if (nzrObj->nzrParams[ledIndex].stepNum == 0) { // Turn On
        nzrObj->nzrParams[ledIndex].stepNum = 1;
		nzrObj->nzrParams[ledIndex].stepSize[0] = 1;
		nzrObj->nzrParams[ledIndex].stepSize[1] = 1;
		nzrObj->nzrParams[ledIndex].stepSize[2] = 1;
		nzrObj->nzrParams[ledIndex].stepSize[3] = 1;
    } else {    // Blink
		nzrObj->nzrParams[ledIndex].stepSize[0] = nzrObj->nzrParams[ledIndex].gCode / (float)(nzrObj->nzrParams[ledIndex].stepNum);
		nzrObj->nzrParams[ledIndex].stepSize[1] = nzrObj->nzrParams[ledIndex].rCode / (float)(nzrObj->nzrParams[ledIndex].stepNum);
		nzrObj->nzrParams[ledIndex].stepSize[2] = nzrObj->nzrParams[ledIndex].bCode / (float)(nzrObj->nzrParams[ledIndex].stepNum);
		nzrObj->nzrParams[ledIndex].stepSize[3] = nzrObj->nzrParams[ledIndex].wCode / (float)(nzrObj->nzrParams[ledIndex].stepNum);
    }

	ResetTick(&nzrObj->nzrParams[ledIndex]);

	return IOIF_NZRLED_STATUS_OK;
}

/**
 * @brief Function to run the NZR LED with the set configurations.
 * @param nzrObj The NZR LED object.
 */
IOIF_NZRLEDState_t IOIF_RunNZRLED(IOIF_TimCh_t timCh, IOIF_NzrLedObj_t* nzrObj)
{
	// Check if the nzrObj and its buffer pointer are valid
/*    if (!nzrObj || !nzrObj->pNzrDutyBuff) {
        return IOIF_NZRLED_STATUS_ERROR;
    }
*/
	uint8_t tRCode = 0, tGCode = 0, tBCode = 0, tWCode = 0;
    uint32_t tCurrentStep;
	uint8_t status = IOIF_NZRLED_STATUS_OK;

    // Iterate over all LEDs and set the active color based on the individual GRBW components
    for (int i = 0; i < LED_NUM_PER_CH; i++) {
        /* TURN ON or BLINK*/
		if(nzrObj->nzrParams[i].stepNum != 1){
			if ((nzrObj->nzrParams[i].tickCount / nzrObj->nzrParams[i].stepNum) % 2 == 0) { // Uphill
				tCurrentStep = (nzrObj->nzrParams[i].tickCount % nzrObj->nzrParams[i].stepNum);
			} else { // Downhill
				tCurrentStep = nzrObj->nzrParams[i].stepNum - (nzrObj->nzrParams[i].tickCount % nzrObj->nzrParams[i].stepNum);
			}

            tGCode = (uint8_t)(tCurrentStep * nzrObj->nzrParams[i].stepSize[0]);
            tRCode = (uint8_t)(tCurrentStep * nzrObj->nzrParams[i].stepSize[1]);
            tBCode = (uint8_t)(tCurrentStep * nzrObj->nzrParams[i].stepSize[2]);
            tWCode = (uint8_t)(tCurrentStep * nzrObj->nzrParams[i].stepSize[3]);
            
           // status =
            GrbwToCode(&nzrObj->nzrParams[i].activeColor, tGCode, tRCode, tBCode, tWCode);
           /* if (status != IOIF_NZRLED_STATUS_OK) {
                return status;
            }*/
        } else {
            tGCode = nzrObj->nzrParams[i].gCode;
            tRCode = nzrObj->nzrParams[i].rCode;
            tBCode = nzrObj->nzrParams[i].bCode;
            tWCode = nzrObj->nzrParams[i].wCode;
            
            //status =
            GrbwToCode(&nzrObj->nzrParams[i].activeColor, tGCode, tRCode, tBCode, tWCode);
            /*if (status != IOIF_NZRLED_STATUS_OK) {
                return status;
            }*/
        }
        CountTick(&nzrObj->nzrParams[i]);
    }

    // Set the PWM duty cycle values for the LEDs
    //status =
    SetLedDuty(nzrObj);
	/*if (status != IOIF_NZRLED_STATUS_OK) {
		return status;
	}*/

    // Start the PWM using DMA
    //status =
    		BSP_StartPWMDMA((BSP_Tim_t)timHandle, timCh, (uint32_t*)nzrObj->pNzrDutyBuff, TOTAL_BYTE_NUM, BSP_TIM_PWM_START_DMA);
/*	if (status != BSP_OK) {
		return status;
	}
*/
	return 0;//status;
}

/**
 * @brief Function to clear the NZR LED (typically turns off the LEDs).
 * @param nzrObj The NZR LED object.
 */
IOIF_NZRLEDState_t IOIF_ClearNZRLED(IOIF_TimCh_t timCh, IOIF_NzrLedObj_t* nzrObj)
{
    // Check if the nzrObj and its buffer pointer are valid
  /*  if (!nzrObj || !nzrObj->pNzrDutyBuff) {
        return IOIF_NZRLED_STATUS_ERROR;
    }
*/
	for (int i = 0; i < TOTAL_BYTE_NUM; i++) {
		nzrObj->pNzrDutyBuff[i] = (i < TOTAL_BYTE_NUM) ? NZR_DUTY_REST : 0;
	}

	//uint8_t status = IOIF_NZRLED_STATUS_OK;

    for (int i = 0; i < LED_NUM_PER_CH; i++) {
        //status =
    	IOIF_SetNZRLED(nzrObj, i, IOIF_COLOR_NONE, 0);
        /*if (status != IOIF_NZRLED_STATUS_OK) {
            return status;
        }*/
    }

   	// Start the PWM using DMA with the cleared values
    //status =
    		BSP_StartPWMDMA((BSP_Tim_t)timHandle, timCh, (uint32_t*)nzrObj->pNzrDutyBuff, TOTAL_BYTE_NUM, BSP_TIM_PWM_START_DMA);
    //status = IOIF_RunNZRLED(timCh, nzrObj);
	/*if (status != IOIF_NZRLED_STATUS_OK) {
		return status;
	}
*/
	return 0;//status;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

static void CountTick(IOIF_NzrParams_t* nzrParams)
{	
    nzrParams->tickCount++;	
}

static void ResetTick(IOIF_NzrParams_t* nzrParams)
{	
    nzrParams->tickCount = 0;	
}

/** 
 * @brief Set LED duty based on active color.
 * @param nzrObj Pointer to the NZR LED object.
 */
static IOIF_NZRLEDState_t SetLedDuty(IOIF_NzrLedObj_t* nzrObj)
{
	// Check if the nzrObj and its buffer pointer are valid
/*    if (!nzrObj || !nzrObj->pNzrDutyBuff) {
        return IOIF_NZRLED_STATUS_ERROR;
    }
*/
    uint32_t tCode = 0;

    for (int i = 0; i < LED_NUM_PER_CH; i++) {
        tCode = nzrObj->nzrParams[i].activeColor;

        for (int j = 0; j < COLOR_TOTAL_BITS; j++) {
        	//if((((i+1) * BYTE_PER_COLOR * COLOR_NUM) - (j+1))!=0){
        		if ((tCode & (1 << j)) == 0) {
        			nzrObj->pNzrDutyBuff[((i+1) * BYTE_PER_COLOR * COLOR_NUM) - (j+1)] = NZR_DUTY_LOW;
        		} else {
        			nzrObj->pNzrDutyBuff[((i+1) * BYTE_PER_COLOR * COLOR_NUM) - (j+1)] = NZR_DUTY_HIGH;
        		}
        	//}
        	//else{
        		//break;
        	//}
        }
    }

    for (int k = 0; k < REST_SIGNAL_BYTE_NUM; k++) {
        nzrObj->pNzrDutyBuff[(LED_NUM_PER_CH * BYTE_PER_COLOR * COLOR_NUM) + k] = NZR_DUTY_REST;
    }

    return IOIF_NZRLED_STATUS_OK;
}

/** 
 * @brief Convert GRBW values to a single color code.
 * @param colorCode Pointer to store the generated color code.
 * @param gCode Green value.
 * @param rCode Red value.
 * @param bCode Blue value.
 * @param wCode White value.
 */
static IOIF_NZRLEDState_t GrbwToCode(uint32_t* colorCode, uint8_t gCode, uint8_t rCode, uint8_t bCode, uint8_t wCode)
{
 /*   if (!colorCode || gCode > 0xFF || rCode > 0xFF || bCode > 0xFF || wCode > 0xFF) {
        return IOIF_NZRLED_STATUS_ERROR;
    }
*/
    *colorCode = (((uint32_t)(gCode)) << 24) | \
                 (((uint32_t)(rCode)) << 16) | \
                 (((uint32_t)(bCode)) << 8)  | \
                 (((uint32_t)(wCode)));

	return IOIF_NZRLED_STATUS_OK;
}

/** 
 * @brief Convert a color code to individual GRBW values.
 * @param nzrObj Pointer to the NZR LED object to store the extracted GRBW values.
 * @param colorCode Color code to be converted.
 */
static IOIF_NZRLEDState_t CodeToGrbw(IOIF_NzrParams_t* nzrParams, uint32_t colorCode)
{
/*    // Check if the nzrObj is valid
    if (!nzrParams) {
        return IOIF_NZRLED_STATUS_ERROR;
    }

    // Check if the color code is valid
    if (!IsValColor(colorCode)) {
        return IOIF_NZRLED_STATUS_ERROR;
    }
*/
    nzrParams->gCode = (uint8_t)((colorCode & 0xff000000) >> 24);
    nzrParams->rCode = (uint8_t)((colorCode & 0x00ff0000) >> 16);
    nzrParams->bCode = (uint8_t)((colorCode & 0x0000ff00) >> 8);
    nzrParams->wCode = (uint8_t)(colorCode & 0x000000ff);

	return IOIF_NZRLED_STATUS_OK;
}

/** 
 * @brief Stop NZR DMA upon completion for Timer PWM Finished Callback.
 * @param params Optional parameters (not used in current implementation).
 */
static void StopNZRDMA(void* params)
{
    #ifdef L30_CM_ENABLED
	BSP_StopPWMDMA((BSP_Tim_t)timHandle, BSP_TIM_CHANNEL_1, BSP_TIM_PWM_STOP_DMA);
    BSP_StopPWMDMA((BSP_Tim_t)timHandle, BSP_TIM_CHANNEL_2, BSP_TIM_PWM_STOP_DMA);
    #endif /* L30_CM_ENABLED */
    
    #if defined(L30_MD_REV06_ENABLED) || defined (L30_MD_REV07_ENABLED) || defined (L30_MD_REV08_ENABLED)
	BSP_StopPWMDMA((BSP_Tim_t)timHandle, BSP_TIM_CHANNEL_1, BSP_TIM_PWM_STOP_DMA);
    #endif
}

/**
 * @brief Check if the provided color code is one of the predefined valid color codes.
 * @param colorCode Color code to be verified.
 * @return true if the color code is valid, false otherwise.
 */
static bool IsValColor(uint32_t color)
{
    return (color == IOIF_COLOR_WHITE || color == IOIF_COLOR_RED || 
            color == IOIF_COLOR_GREEN || color == IOIF_COLOR_BLUE ||
			color == IOIF_COLOR_NONE);
}


#endif /* IOIF_NZRLED_ENABLED */
