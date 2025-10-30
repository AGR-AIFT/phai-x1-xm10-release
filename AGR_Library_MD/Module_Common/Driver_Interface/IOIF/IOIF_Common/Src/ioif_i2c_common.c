

#include "ioif_i2c_common.h"

/** @defgroup I2C I2C
  * @brief I2C BSP module driver
  * @{
  */
#ifdef BSP_I2C_MODULE_ENABLED

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
 * @brief Function prototypes for this module.
 */




/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

uint8_t IOIF_SetI2CCB(IOIF_I2C_t i2c, IOIF_I2CCBType_t callbackType, IOIF_I2CCBPtr_t callback, void* params)
{
    // Check the validity of the i2c
    if (i2c >= IOIF_I2C_COUNT) {
        return IOIF_I2C_STATUS_ERROR; // Invalid i2c
    }

    // Check the validity of the callbackType
    if (callbackType >= IOIF_I2C_CALLBACK_TYPE_COUNT) {
        return IOIF_I2C_STATUS_ERROR; // Invalid callbackType
    }

    // Check if the callback pointer is valid
    if (!callback) {
        return IOIF_I2C_STATUS_ERROR; // Invalid callback pointer
    }

    // Map IOIF enums, parameters, callback to BSP equivalents if necessary
    BSP_SetI2CCB((BSP_I2C_t)(i2c), (BSP_I2CCBType_t)(callbackType), (BSP_I2CCBPtr_t)(callback), params);

    return IOIF_I2C_STATUS_OK;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* BSP_I2C_MODULE_ENABLED */