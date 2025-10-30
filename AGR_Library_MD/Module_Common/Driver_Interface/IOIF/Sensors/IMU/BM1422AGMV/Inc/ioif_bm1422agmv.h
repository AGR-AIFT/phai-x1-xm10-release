/*
 * ioif_bm1422agmv.h
 *
 *  Created on: Jul 28, 2023
 *      Author: INVINCIBLE_1NE
 */


#ifndef BM1422AGMV_INC_IOIF_BM1422AGMV_H_
#define BM1422AGMV_INC_IOIF_BM1422AGMV_H_

#include "module.h"

/** @defgroup I2C I2C
  * @brief I2C BM1422AGMV module driver
  * @{
  */
#ifdef IOIF_BM1422AGMV_ENABLED

#include <string.h>

#include "ioif_i2c_common.h"
#include "bm1422agmv.h"

/**
*-----------------------------------------------------------
*              MACROS AND PREPROCESSOR DIRECTIVES
*-----------------------------------------------------------
* @brief Directives and macros for readability and efficiency.
*/

#define IOIF_BM1422AGMV_BUFF_SIZE        32

#define IOIF_BM1422AGMV_TRIALS           10
#define IOIF_BM1422AGMV_STRAT_UP_DELAY   10
#define IOIF_BM1422AGMV_TIMEOUT          1


/**
*------------------------------------------------------------
*                     TYPE DECLARATIONS
*------------------------------------------------------------
* @brief Custom data types and structures for the module.
*/

/**
* @brief Enumeration to describe the state of the 6-axis IMU.
*/
typedef enum _IOIF_MagState_t {
   IOIF_IMU3AXIS_STATUS_OK = 0,
   IOIF_IMU3AXIS_STATUS_ERROR,
   IOIF_IMU3AXIS_STATUS_BUSY,
   IOIF_IMU3AXIS_STATUS_TIMEOUT,
} IOIF_MagState_t;

/**
* @brief Structure to hold the data from the 6-axis IMU.
*/
typedef struct _IOIF_MagData_t {
   float magX;
   float magY;
   float magZ;
} IOIF_MagData_t;


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

uint8_t IOIF_InitMag(IOIF_I2C_t i2c);
uint8_t IOIF_GetMagValue(IOIF_MagData_t* magData);


#endif /* IOIF_BM1422AGMV_ENABLED */

#endif /* BM1422AGMV_INC_IOIF_BM1422AGMV_H_ */
