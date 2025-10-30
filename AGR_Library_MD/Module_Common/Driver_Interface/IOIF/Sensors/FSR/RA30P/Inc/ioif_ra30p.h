/*
 * ioif_ra30p.h
 *
 *  Created on: Jan 2, 2025
 *      Author: INVINCIBLE_1NE
 */

#ifndef RA30P_INC_IOIF_RA30P_H_
#define RA30P_INC_IOIF_RA30P_H_



#include "module.h"

/** @defgroup ADC ADC
  * @brief ADC FSR RA30P module driver
  * @{
  */
#ifdef IOIF_RA30P_ENABLED

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

typedef enum _IOIF_FSRState_t {
    IOIF_FSR_STATUS_OK = 0,
	IOIF_FSR_STATUS_ERROR,
    IOIF_FSR_NULL_POINTER,
    IOIF_FSR_ADC_OUT_OF_RANGE,
} IOIF_FSRState_t;

typedef struct _IOIF_FSR_Obj_t {
    float fsrVolt;
} IOIF_FSR_Obj_t;


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

IOIF_FSRState_t IOIF_CalFSRVolt(IOIF_FSR_Obj_t* fsr, uint16_t adcBuff);

#endif /* IOIF_RA30P_ENABLED */

#endif /* RA30P_INC_IOIF_RA30P_H_ */
