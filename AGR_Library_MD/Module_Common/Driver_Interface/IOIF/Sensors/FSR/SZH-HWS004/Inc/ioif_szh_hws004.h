/**
 *-----------------------------------------------------------
 *               SZH-HWS004 FSR SENSOR INTERFACE
 *-----------------------------------------------------------
 * @file ioif_szh_hws004.h
 * @date Created on: Aug 24, 2023
 * @author AngelRobotics HW Team
 * @brief IO Interface header for the SZH-HWS004 Force-Sensitive Resistor (FSR) sensor.
 * 
 * This header file provides the interface definitions for the 
 * SZH-HWS004 FSR sensor. The interface includes macros, 
 * type declarations, and function prototypes essential for detecting 
 * and processing pressure changes based on the user's weight on the L30 shoe sole.
 * 
 * @ref SZH-HWS004 Datasheet
 */

#ifndef SZH_HWS004_INC_IOIF_SZH_HWS004_H_
#define SZH_HWS004_INC_IOIF_SZH_HWS004_H_

#include "module.h"

/** @defgroup ADC ADC
  * @brief ADC FSR SZH HWS004 module driver
  * @{
  */
#ifdef IOIF_FSR_SZH_HWS004_ENABLED

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

typedef struct _IOIF_FSR_t { 
    float fsrVolt;        
} IOIF_FSR_t;


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

IOIF_FSRState_t IOIF_CalFSRVolt(IOIF_FSR_t* fsr, uint16_t adcBuff);


#endif /* BSP_ADC_MODULE_ENABLED */

#endif /* SZH_HWS004_INC_IOIF_SZH_HWS004_H_ */
