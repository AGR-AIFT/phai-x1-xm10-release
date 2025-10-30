/*
 * ioif_sm1205c.c
 *
 *  Created on: May 28, 2025
 *      Author: INVINCIBLE
 */


#include "ioif_sm1205c.h"

/** @defgroup GPIO
  * @brief GPIO BUZZER module driver
  * @{
  */
#ifdef IOIF_SM1205C_ENABLED


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

void IOIF_BUZZER_Activation(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, uint8_t num, uint32_t duration)
{
	for (uint8_t i = 0; i < num; i++) {
		HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET);
		IOIF_BUZZER_us_Delay(duration * 1000);							// duration[ms]
		HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
		IOIF_BUZZER_us_Delay(100000);
	}
}


void IOIF_BUZZER_us_Delay(uint32_t us_delay)
{
	uint32_t tickStart = DWT->CYCCNT;
	uint32_t tickDelay = us_delay * systickMHz_Buzzer;

	if (tickStart > 4294967295 - (us_delay * systickMHz_Buzzer)) {
		uint32_t elapsed = 4294967295 - tickStart;
		uint32_t remainder = tickDelay - elapsed;
		while ( DWT->CYCCNT >= tickStart && DWT->CYCCNT <= 4294967295 )
		{
		}
		while ( DWT->CYCCNT <= remainder)
		{
		}
	}
	else {
	    while ( DWT->CYCCNT - tickStart <= tickDelay )
	    {
	    }
	}
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */


#endif /* IOIF_SM1205C_ENABLED */



