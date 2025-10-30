/**
 *-----------------------------------------------------------
 *                 PCA9957HNMP LED DRIVER
 *-----------------------------------------------------------
 * @file ioif_pca9957hnmp.h
 * @date Created on: Sep 14, 2023
 * @author AngelRobotics HW Team
 * @brief Driver code for the PCA9957HNMP LED DRIVER.
 *
 * Todo: Add Annotation
 *
 * Refer to the PCA9957HNMP datasheet and related documents for more information.
 *
 * @ref PCA9957HNMP Datasheet
 */

#ifndef LED_24CH_DRIVER_PCA9957HNMP_INC_IOIF_PCA9957HNMP_H_
#define LED_24CH_DRIVER_PCA9957HNMP_INC_IOIF_PCA9957HNMP_H_

#include "module.h"
/** @defgroup SPI SPI
  * @brief SPI ICM20608G module driver
  * @{
  */
#ifdef IOIF_PCA9957HNMP_ENABLED

#include <string.h>

#include "ioif_spi_common.h"
#include "ioif_gpio_common.h"
#include "pca9957hnmp.h"

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

typedef enum _IOIF_led_module_t {
	DEFAULT = 0,
	POWER,
	BATTERY,
	BLUTOOTH,
	AUXILIARY
} IOIF_led_module_t;

typedef enum _IOIF_led_color_t {
	RED = 0,
	GREEN,
	BLUE,
	CYAN,
	YELLOW,
	WHITE
} IOIF_led_color_t;

typedef enum _IOIF_StatusTypeDef_t {
    IOIF_OK 	 = BSP_OK,
    IOIF_ERROR   = BSP_ERROR,
    IOIF_BUSY	 = BSP_BUSY,
	IOIF_TIMEOUT = BSP_TIMEOUT
} IOIF_StatusTypeDef_t;

typedef void (*pLEDControl)(IOIF_led_module_t, IOIF_led_color_t, uint8_t);


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

//Initialize Essentially Required
uint8_t IOIF_InitLEDDriver(IOIF_SPI_t spiChannel);
bool IOIF_LEDRead(uint8_t reg, uint8_t data, uint8_t* rxdata);
bool IOIF_LEDWrite(uint8_t reg, uint8_t data);

void Drive_LED_Power(uint8_t color, uint8_t brightness);
void Drive_LED_Battery(uint8_t color, uint8_t brightness);
void Drive_LED_Bluetooth(uint8_t color, uint8_t brightness);
void Blink_LED(uint16_t interval_time, FunctionPointer led_drive, uint8_t color, uint8_t brightness );
void Drive_LED_Auxiliary(uint8_t nth, uint8_t brightness);
void Blink_Auxiliary_LED(uint16_t interval_time, uint8_t nth, uint8_t brightness );

#endif /* IOIF_PCA9957HNMP_ENABLED */

#endif /* LED_24CH_DRIVER_PCA9957HNMP_INC_IOIF_PCA9957HNMP_H_ */
