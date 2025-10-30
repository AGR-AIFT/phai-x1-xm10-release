/*
 * ioif_sm1205c.h
 *
 *  Created on: May 28, 2025
 *      Author: INVINCIBLE
 */

#ifndef SM1205C_INC_IOIF_SM1205C_H_
#define SM1205C_INC_IOIF_SM1205C_H_

#include "module.h"

/** @defgroup GPIO
  * @brief GPIO BUZZER module driver
  * @{
  */
#ifdef IOIF_SM1205C_ENABLED

#include "gpio.h"


/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define systickMHz_Buzzer						480
/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */
typedef enum _IOIF_BUZZER_State_t {
    IOIF_BUZZER_STATUS_ON = 0,
	IOIF_BUZZER_STATUS_OFF,
} IOIF_BUZZER_State_t;



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
void IOIF_BUZZER_Activation(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, uint8_t num, uint32_t duration);
void IOIF_BUZZER_us_Delay(uint32_t us_delay);



#endif /* IOIF_SM1205C_ENABLED */

#endif /* SM1205C_INC_IOIF_SM1205C_H_ */
