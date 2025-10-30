/*
 * ioif_ra30p.c
 *
 *  Created on: Jan 2, 2025
 *      Author: INVINCIBLE_1NE
 */


#include "ioif_ra30p.h"

/** @defgroup ADC ADC
  * @brief ADC FSR RA30P module driver
  * @{
  */
#ifdef IOIF_RA30P_ENABLED

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

IOIF_FSRState_t IOIF_CalFSRVolt(IOIF_FSR_Obj_t* fsr, uint16_t adcBuff)
{
    if (!fsr) {
        return IOIF_FSR_NULL_POINTER;
    }

    if (adcBuff > IOIF_FSR_RESOLUTION) {
        return IOIF_FSR_ADC_OUT_OF_RANGE;
    }

    fsr->fsrVolt = (float)adcBuff * IOIF_FSR_VREF / IOIF_FSR_RESOLUTION;

    return IOIF_FSR_STATUS_OK;  // 0 indicates success
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* IOIF_FSR_RA30P_ENABLED */
