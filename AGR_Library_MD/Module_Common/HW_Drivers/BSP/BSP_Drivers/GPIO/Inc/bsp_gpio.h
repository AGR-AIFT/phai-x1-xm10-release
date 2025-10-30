/**
 * @file bsp_gpio.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for GPIO functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef GPIO_INC_BSP_GPIO_H_
#define GPIO_INC_BSP_GPIO_H_

#include "main.h"
#include "module.h"

/** @defgroup GPIO GPIO
  * @brief GPIO HAL BSP module driver
  * @
  */
#ifdef HAL_GPIO_MODULE_ENABLED
#define BSP_GPIO_MODULE_ENABLED

#include <math.h>
#include <stdbool.h>

#include "gpio.h"
#include "bsp_common.h"

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
typedef void (*BSP_GPIOCBPtr_t)(uint16_t gpioPins);

typedef enum{
	LOW = 0,
	HIGH = 1
} GPIO_PIN_STATE;


/**
  * @brief  GPIO Bit SET and Bit RESET enumeration
  */
typedef enum _BSP_GPIOPinState_t {
    BSP_GPIO_PIN_RESET = GPIO_PIN_RESET,
    BSP_GPIO_PIN_SET = GPIO_PIN_SET
} BSP_GPIOPinState_t;

/**
 * @brief Enumeration for BSP GPIO identifiers.
 * Starts from 1 to align with common STM32 naming (GPIOA, GPIOB, ...)
 */
typedef enum _BSP_GPIOPort_t {
    BSP_GPIO_PORT_A,      ///< GPIO A Identifier
    BSP_GPIO_PORT_B,      ///< GPIO B Identifier
    BSP_GPIO_PORT_C,      ///< GPIO C Identifier
    BSP_GPIO_PORT_D,      ///< GPIO D Identifier
    BSP_GPIO_PORT_E,      ///< GPIO E Identifier
    BSP_GPIO_PORT_F,      ///< GPIO F Identifier
    BSP_GPIO_PORT_G,      ///< GPIO G Identifier
    BSP_GPIO_PORT_H,      ///< GPIO H Identifier
    BSP_GPIO_PORT_I,      ///< GPIO I Identifier
    BSP_GPIO_PORT_J,      ///< GPIO J Identifier
    BSP_GPIO_PORT_K,      ///< GPIO K Identifier
    BSP_GPIO_PORT_COUNT,
} BSP_GPIOPort_t;

/**
 * @brief Enumeration for BSP GPIO PiIN identifiers.
 * Starts from 1 to align with common STM32 naming (GPIO_PIN_0, GPIO_PIN_1, ...)
 */
typedef enum _BSP_GPIOPin_t {
    BSP_GPIO_PIN_0 = GPIO_PIN_0,
    BSP_GPIO_PIN_1 = GPIO_PIN_1,
    BSP_GPIO_PIN_2 = GPIO_PIN_2,
    BSP_GPIO_PIN_3 = GPIO_PIN_3,
    BSP_GPIO_PIN_4 = GPIO_PIN_4,
    BSP_GPIO_PIN_5 = GPIO_PIN_5,
    BSP_GPIO_PIN_6 = GPIO_PIN_6,
    BSP_GPIO_PIN_7 = GPIO_PIN_7,
    BSP_GPIO_PIN_8 = GPIO_PIN_8,
    BSP_GPIO_PIN_9 = GPIO_PIN_9,
    BSP_GPIO_PIN_10 = GPIO_PIN_10,
    BSP_GPIO_PIN_11 = GPIO_PIN_11,
    BSP_GPIO_PIN_12 = GPIO_PIN_12,
    BSP_GPIO_PIN_13 = GPIO_PIN_13,
    BSP_GPIO_PIN_14 = GPIO_PIN_14,
    BSP_GPIO_PIN_15 = GPIO_PIN_15,
    BSP_GPIO_PIN_COUNT = 16,

    BSP_GPIO_PIN_MASK = GPIO_PIN_MASK,
} BSP_GPIOPin_t;

/**
 * @enum BSP_GPIOCBType_t
 * @brief GPIO callback types for various gpio events.
 */
typedef enum _BSP_GPIOCBType_t {
    BSP_GPIO_EXTI_CALLBACK,
    BSP_GPIO_CALLBACK_TYPE_COUNT,
} BSP_GPIOCBType_t;

/**
 * @struct GPIOMapping_t
 * @brief Maps BSP GPIO enumerations to their corresponding HAL GPIO handles.
 */
typedef struct _BSP_GPIOPortMap_t {
    BSP_GPIOPort_t gpioPort;    ///< Enumeration of the gpioer
    GPIO_TypeDef* bsp_port;     ///< Pointer to the HAL GPIO handle
} BSP_GPIOPortMap_t;

/**
 * @struct BSP_GPIOCB_t
 * @brief Manager BSP GPIO Custom Callbacks and Parameters.
 */
typedef struct _BSP_GPIOCB_t {
    BSP_GPIOCBPtr_t callbacks[BSP_GPIO_CALLBACK_TYPE_COUNT];
    uint16_t gpioPins[BSP_GPIO_CALLBACK_TYPE_COUNT];
} BSP_GPIOCB_t;


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

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
void BSP_InitGPIO(BSP_GPIOPort_t gpio, GPIO_InitTypeDef* GPIO_Init);
void BSP_DeInitGPIO(BSP_GPIOPort_t gpio, uint16_t gpioPin);

/* ------------------- GPIO OPERATION ------------------- */
BSP_GPIOPinState_t BSP_ReadGPIOPin(BSP_GPIOPort_t gpio, uint16_t gpioPin);
void BSP_WriteGPIOPin(BSP_GPIOPort_t gpio, uint16_t gpioPin, BSP_GPIOPinState_t pinState);
void BSP_ToggleGPIOPin(BSP_GPIOPort_t gpio, uint16_t gpioPin);
BSP_StatusTypeDef_t BSP_LockGPIOPin(BSP_GPIOPort_t gpio, uint16_t gpioPin);

/* ------------------- GPIO CALLBACKS ------------------- */
void BSP_SetGPIOCB(uint16_t gpioPin, BSP_GPIOCBType_t callbackType, BSP_GPIOCBPtr_t callback);
bool Is_BT_connected(void);

#endif /* HAL_GPIO_MODULE_ENABLED */

#endif /* GPIO_INC_BSP_GPIO_H_ */
