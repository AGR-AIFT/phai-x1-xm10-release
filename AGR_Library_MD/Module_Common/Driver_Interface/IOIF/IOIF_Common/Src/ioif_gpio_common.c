/**
 *-----------------------------------------------------------
 *                  GPIO COMMON IMPLEMENTATION
 *-----------------------------------------------------------
 * @file ioif_gpio_common.c
 * @date Created on: Aug 24, 2023
 * @author AngelRobotics HW Team
 * @brief Implementation of IO Interface functions for common GPIO operations.
 * 
 * This source file provides implementations for various common GPIO operations.
 * It contains methods for reading, writing, toggling, and setting callbacks 
 * for GPIO pins. It abstracts the underlying hardware operations, providing 
 * a higher-level interface for GPIO control and management.
 * 
 * @ref Relevant_Datasheet_or_Document (If any)
 */

#include "ioif_gpio_common.h"

/** @defgroup GPIO GPIO
  * @brief GPIO BSP module driver
  * @{
  */
#ifdef BSP_GPIO_MODULE_ENABLED

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

IOIF_GPIOPinState_t IOIF_ReadGPIOPin(IOIF_GPIOPort_t gpioPort, uint16_t gpioPin)
{
    if (gpioPort >= IOIF_GPIO_PORT_COUNT) {
        return IOIF_GPIO_STATUS_ERROR; // The specified GPIO port is not valid
    }

    // Check if the specified GPIO pin is valid
    if (gpioPin >= IOIF_GPIO_PIN_COUNT) {
        return IOIF_GPIO_STATUS_ERROR; // The specified GPIO pin is not valid
    }

    return (IOIF_GPIOPinState_t)BSP_ReadGPIOPin((BSP_GPIOPort_t)gpioPort, gpioPin);
}

IOIF_GPIOState_t IOIF_WriteGPIOPin(IOIF_GPIOPort_t gpioPort, uint16_t gpioPin, IOIF_GPIOPinState_t pinState)
{
    if (gpioPort >= IOIF_GPIO_PORT_COUNT) {
        return IOIF_GPIO_STATUS_ERROR; // The specified GPIO port is not valid
    }

    // Check if the specified GPIO pin is valid
    if (gpioPin >= IOIF_GPIO_PIN_COUNT) {
        return IOIF_GPIO_STATUS_ERROR; // The specified GPIO pin is not valid
    }

    BSP_WriteGPIOPin((BSP_GPIOPort_t)gpioPort, gpioPin, (BSP_GPIOPinState_t)pinState);

    return IOIF_GPIO_STATUS_OK;
}

IOIF_GPIOState_t IOIF_ToggleGPIOPin(IOIF_GPIOPort_t gpioPort, uint16_t gpioPin)
{
    if (gpioPort >= IOIF_GPIO_PORT_COUNT) {
        return IOIF_GPIO_STATUS_ERROR; // The specified GPIO port is not valid
    }

    // Check if the specified GPIO pin is valid
    if (gpioPin >= IOIF_GPIO_PIN_COUNT) {
        return IOIF_GPIO_STATUS_ERROR; // The specified GPIO pin is not valid
    }
    
    BSP_ToggleGPIOPin((BSP_GPIOPort_t)gpioPort, gpioPin);

    return IOIF_GPIO_STATUS_OK;
}

IOIF_GPIOState_t IOIF_SetGPIOCB(uint16_t gpioPin, IOIF_GPIOCBType_t callbackType, IOIF_GPIOCBPtr_t callback)
{
    // TODO: Additional Logic Requirement: Perform validation or logic at the IOIF level only if distinct from what's handled at the IOIF level.
    if (gpioPin >= IOIF_GPIO_PIN_COUNT) {
        return IOIF_GPIO_STATUS_ERROR; // The specified GPIO port is not valid
    }

    if (callbackType < 0 || callbackType >= IOIF_GPIO_CALLBACK_TYPE_COUNT) {
        return IOIF_GPIO_STATUS_ERROR;
    }

    if (!callback) {
        return IOIF_GPIO_STATUS_ERROR;
    }

    BSP_SetGPIOCB(gpioPin, (BSP_GPIOCBType_t)callbackType, (BSP_GPIOCBPtr_t)callback);

    return IOIF_GPIO_STATUS_OK;
}

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* BSP_GPIO_MODULE_ENABLED */
