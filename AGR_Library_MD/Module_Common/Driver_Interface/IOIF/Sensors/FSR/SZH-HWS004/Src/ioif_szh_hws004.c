/**
 *-----------------------------------------------------------
 *             SZH-HWS004 FSR SENSOR IMPLEMENTATION
 *-----------------------------------------------------------
 * @file ioif_szh_hws004.c
 * @date Created on: Aug 24, 2023
 * @author AngelRobotics HW Team
 * @brief IO Interface implementation for the SZH-HWS004 Force-Sensitive Resistor (FSR) sensor.
 * 
 * This source file provides the functionality to interface 
 * with the SZH-HWS004 FSR sensor. The code includes methods for 
 * initializing the sensor, retrieving data, and converting ADC values 
 * to corresponding voltage levels for pressure detection.
 * 
 * @ref SZH-HWS004 Datasheet
 */

#include "ioif_szh_hws004.h"

/** @defgroup ADC ADC
  * @brief ADC FSR SZH HWS004 module driver
  * @{
  */
#ifdef IOIF_FSR_SZH_HWS004_ENABLED

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

IOIF_FSRState_t IOIF_CalFSRVolt(IOIF_FSR_t* fsr, uint16_t adcBuff)
{
    if (!fsr) {
        return IOIF_FSR_NULL_POINTER;
    }
    
    if (adcBuff >= IOIF_ADC3_RESOLUTION) {
        return IOIF_FSR_ADC_OUT_OF_RANGE;
    }

    fsr->fsrVolt = (float)adcBuff * IOIF_ADC3_VREF / IOIF_ADC3_RESOLUTION;

    return IOIF_FSR_STATUS_OK;  // 0 indicates success
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* IOIF_FSR_SZH_HWS004_ENABLED */