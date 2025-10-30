/**
 *-----------------------------------------------------------
 *                   GPIO COMMON INTERFACE
 *-----------------------------------------------------------
 * @file ioif_gpio_common.h
 * @date Created on: Aug 24, 2023
 * @author AngelRobotics HW Team
 * @brief Header file for IO Interface functions for common GPIO operations.
 * 
 * This header file provides the declarations for functions, data structures,
 * and macros related to common GPIO operations in the AngelRobotics framework.
 * 
 * @ref Relevant_Datasheet_or_Document (If any)
 */

#ifndef IOIF_COMMON_INC_IOIF_GPIO_COMMON_H_
#define IOIF_COMMON_INC_IOIF_GPIO_COMMON_H_

#include "bsp_gpio.h"

/** @defgroup GPIO GPIO
  * @brief GPIO BSP module driver
  * @{
  */
#ifdef BSP_GPIO_MODULE_ENABLED

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
 * @brief Function pointer type for GPIO callback functions.
 */
typedef void (*IOIF_GPIOCBPtr_t)(uint16_t gpioPins);

typedef enum _IOIF_GPIOState_t {
    IOIF_GPIO_STATUS_OK = 0,
    IOIF_GPIO_STATUS_ERROR,
    IOIF_GPIO_STATUS_BUSY,
    IOIF_GPIO_STATUS_TIMEOUT,
} IOIF_GPIOState_t;

/**
  * @brief  GPIO Bit SET and Bit RESET enumeration
  */
typedef enum _IOIF_GPIOPinState_t {
    IOIF_GPIO_PIN_RESET = BSP_GPIO_PIN_RESET,
    IOIF_GPIO_PIN_SET = BSP_GPIO_PIN_SET,
} IOIF_GPIOPinState_t;

/**
 * @brief Enumeration for IOIF GPIO identifiers.
 * Starts from 1 to align with common STM32 naming (GPIOA, GPIOB, ...)
 */
typedef enum _IOIF_GPIOPort_t {
    IOIF_GPIO_PORT_A,      ///< GPIO A Identifier
    IOIF_GPIO_PORT_B,      ///< GPIO B Identifier
    IOIF_GPIO_PORT_C,      ///< GPIO C Identifier
    IOIF_GPIO_PORT_D,      ///< GPIO D Identifier
    IOIF_GPIO_PORT_E,      ///< GPIO E Identifier
    IOIF_GPIO_PORT_F,      ///< GPIO F Identifier
    IOIF_GPIO_PORT_G,      ///< GPIO G Identifier
    IOIF_GPIO_PORT_H,      ///< GPIO H Identifier
    IOIF_GPIO_PORT_I,      ///< GPIO I Identifier
    IOIF_GPIO_PORT_J,      ///< GPIO J Identifier
    IOIF_GPIO_PORT_K,      ///< GPIO K Identifier
    IOIF_GPIO_PORT_COUNT,
} IOIF_GPIOPort_t;

/**
 * @brief Enumeration for IOIF GPIO PiIN identifiers.
 * Starts from 1 to align with common STM32 naming (GPIO_PIN_0, GPIO_PIN_1, ...)
 */
typedef enum _IOIF_GPIOPin_t {
    IOIF_GPIO_PIN_0 = BSP_GPIO_PIN_0,
    IOIF_GPIO_PIN_1 = BSP_GPIO_PIN_1,
    IOIF_GPIO_PIN_2 = BSP_GPIO_PIN_2,
    IOIF_GPIO_PIN_3 = BSP_GPIO_PIN_3,
    IOIF_GPIO_PIN_4 = BSP_GPIO_PIN_4,
    IOIF_GPIO_PIN_5 = BSP_GPIO_PIN_5,
    IOIF_GPIO_PIN_6 = BSP_GPIO_PIN_6,
    IOIF_GPIO_PIN_7 = BSP_GPIO_PIN_7,
    IOIF_GPIO_PIN_8 = BSP_GPIO_PIN_8,
    IOIF_GPIO_PIN_9 = BSP_GPIO_PIN_9,
    IOIF_GPIO_PIN_10 = BSP_GPIO_PIN_10,
    IOIF_GPIO_PIN_11 = BSP_GPIO_PIN_11,
    IOIF_GPIO_PIN_12 = BSP_GPIO_PIN_12,
    IOIF_GPIO_PIN_13 = BSP_GPIO_PIN_13,
    IOIF_GPIO_PIN_14 = BSP_GPIO_PIN_14,
    IOIF_GPIO_PIN_15 = BSP_GPIO_PIN_15,
    IOIF_GPIO_PIN_COUNT,
    IOIF_GPIO_PIN_MASK = BSP_GPIO_PIN_MASK,
} IOIF_GPIOPin_t;

/**
 * @enum IOIF_GPIOCBType_t
 * @brief GPIO callback types for various gpio events.
 */
typedef enum _IOIF_GPIOCBType_t {
    IOIF_GPIO_EXTI_CALLBACK,
    IOIF_GPIO_CALLBACK_TYPE_COUNT,
} IOIF_GPIOCBType_t;


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

IOIF_GPIOPinState_t IOIF_ReadGPIOPin(IOIF_GPIOPort_t gpioPort, uint16_t gpioPin);
IOIF_GPIOState_t IOIF_WriteGPIOPin(IOIF_GPIOPort_t gpioPort, uint16_t gpioPin, IOIF_GPIOPinState_t pinState);
IOIF_GPIOState_t IOIF_ToggleGPIOPin(IOIF_GPIOPort_t gpioPort, uint16_t gpioPin);
IOIF_GPIOState_t IOIF_SetGPIOCB(uint16_t gpioPin, IOIF_GPIOCBType_t callbackType, IOIF_GPIOCBPtr_t callback);


#endif /* BSP_GPIO_MODULE_ENABLED */

#endif /* IOIF_COMMON_INC_IOIF_GPIO_COMMON_H_ */
