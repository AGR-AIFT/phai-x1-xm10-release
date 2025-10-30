/*
 * ioif_psl_iemg2.c
 *
 *  Created on: Jan 2, 2025
 *      Author: INVINCIBLE_1NE
 */


#include "ioif_psl_iemg2.h"

/** @defgroup ADC ADC
  * @brief ADC EMG PSL_iEMG2 module driver
  * @{
  */
#ifdef IOIF_PSL_IEMG2_ENABLED

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

IOIF_EMGState_t IOIF_CalEMGVolt(IOIF_EMG_Obj_t* emg, uint16_t adcBuff)
{
    if (!emg) {
        return IOIF_EMG_NULL_POINTER;
    }

    if (adcBuff > IOIF_EMG_RESOLUTION) {
        return IOIF_EMG_ADC_OUT_OF_RANGE;
    }

    emg->emgVolt = (float)adcBuff * IOIF_EMG_VREF / IOIF_EMG_RESOLUTION;

    return IOIF_EMG_STATUS_OK;  // 0 indicates success
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */


#endif /* IOIF_EMG_PSL_IEMG2_ENABLED */
