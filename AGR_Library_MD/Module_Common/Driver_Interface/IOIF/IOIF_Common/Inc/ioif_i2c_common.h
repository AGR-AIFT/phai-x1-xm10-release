

#ifndef IOIF_COMMON_INC_IOIF_I2C_COMMON_H_
#define IOIF_COMMON_INC_IOIF_I2C_COMMON_H_

#include "module.h"
#include "bsp_i2c.h"



/** @defgroup I2C I2C
  * @brief I2C BSP module driver
  * @{
  */
#ifdef BSP_I2C_MODULE_ENABLED

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

/**
 * @brief Function pointer type for I2C callback functions.
 */
typedef void (*IOIF_I2CCBPtr_t)(void* params);

typedef enum _IOIF_I2CState_t {
    IOIF_I2C_STATUS_OK = 0,
    IOIF_I2C_STATUS_ERROR,
    IOIF_I2C_STATUS_BUSY,
    IOIF_I2C_STATUS_TIMEOUT,
} IOIF_I2CState_t;

/**
 * @brief Enumeration for IOIF I2C identifiers.
 * Starts from 1 to align with common STM32 naming (I2C1, I2C2, ...)
 */
typedef enum _IOIF_I2C_t {
    IOIF_I2C1 = 1,  ///< I2C 1 Identifier
    IOIF_I2C2,      ///< I2C 2 Identifier
    IOIF_I2C3,      ///< I2C 3 Identifier
    IOIF_I2C4,      ///< I2C 4 Identifier
    IOIF_I2C_COUNT
} IOIF_I2C_t;

/**
 * @enum IOIF_I2CCBType_t
 * @brief I2C callback types for various i2c events.
 */
typedef enum _IOIF_I2CCBType_t {
    IOIF_I2C_MASTER_TX_CPLT_CALLBACK,
    IOIF_I2C_MASTER_RX_CPLT_CALLBACK,
    IOIF_I2C_SLAVE_TX_CPLT_CALLBACK,
    IOIF_I2C_SLAVE_RX_CPLT_CALLBACK,
    IOIF_I2C_MEM_TX_CPLT_CALLBACK,
    IOIF_I2C_MEM_RX_CPLT_CALLBACK,
    IOIF_I2C_LISTEN_CPLT_CALLBACK,
    IOIF_I2C_ERROR_CALLBACK,
    IOIF_I2C_ABORT_CPLT_CALLBACK,
    IOIF_I2C_CALLBACK_TYPE_COUNT,
} IOIF_I2CCBType_t;


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

uint8_t IOIF_SetI2CCB(IOIF_I2C_t i2cEnum, IOIF_I2CCBType_t callbackType, IOIF_I2CCBPtr_t callback, void* params);


#endif /* BSP_I2C_MODULE_ENABLED */

#endif /* IOIF_COMMON_INC_IOIF_I2C_COMMON_H_ */
