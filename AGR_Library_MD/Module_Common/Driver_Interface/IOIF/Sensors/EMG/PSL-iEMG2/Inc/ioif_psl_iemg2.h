/*
 * ioif_psl_iemg2.h
 *
 *  Created on: Jan 2, 2025
 *      Author: INVINCIBLE_1NE
 */

#ifndef PSL_IEMG2_INC_IOIF_PSL_IEMG2_H_
#define PSL_IEMG2_INC_IOIF_PSL_IEMG2_H_

#include "module.h"

/** @defgroup ADC ADC
  * @brief ADC EMG PSL_iEMG2 module driver
  * @{
  */
#ifdef IOIF_PSL_IEMG2_ENABLED

#include "ioif_adc_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */




/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef enum _IOIF_EMGState_t {
    IOIF_EMG_STATUS_OK = 0,
	IOIF_EMG_STATUS_ERROR,
    IOIF_EMG_NULL_POINTER,
    IOIF_EMG_ADC_OUT_OF_RANGE,
} IOIF_EMGState_t;

typedef struct _IOIF_EMG_Obj_t {
    float emgVolt;
} IOIF_EMG_Obj_t;


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

IOIF_EMGState_t IOIF_CalEMGVolt(IOIF_EMG_Obj_t* emg, uint16_t adcBuff);

#endif /* IOIF_PSL_IEMG2_ENABLED */

#endif /* PSL_IEMG2_INC_IOIF_PSL_IEMG2_H_ */
