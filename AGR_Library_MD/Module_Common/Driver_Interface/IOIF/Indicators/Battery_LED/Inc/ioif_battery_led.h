

#ifndef BATTERY_LED_INC_IOIF_BATTERY_LED_H_
#define BATTERY_LED_INC_IOIF_BATTERY_LED_H_

#include "module.h"

/** @defgroup TIM TIM
  * @brief TIM Buzzer module driver
  * @{
  */
#ifdef IOIF_BATTERYLED_ENABLED

#include <math.h>

#include "ioif_i2c_common.h"
#include "ioif_gpio_common.h"
#include "pca9531.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define LED1_ON		1
#define LED2_ON		1
#define LED3_ON		1
#define LED4_ON		1

#define LED1_OFF		0
#define LED2_OFF		0
#define LED3_OFF		0
#define LED4_OFF		0

#define LED_TOTAL 4

#define LED_BATTERY			-1

#define LED_BATTERY_ON 		1
#define LED_BATTERY_OFF 	0

#define BATTERY_LED_PWM0	180
#define BATTERY_LED_PWM1	90
#define BATTERY_LED_OFF		0x00
#define BATTERY_LED_ADDRESS	0b11000000

#define IOIF_BATLED_BUFF_SIZE        32

#define IOIF_BATLED_TRIALS           10
#define IOIF_BATLED_STRAT_UP_DELAY   10
#define IOIF_BATLED_TIMEOUT          1


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef enum _IOIF_BATLEDState_t {
    IOIF_BATLED_STATUS_OK = 0,
    IOIF_BATLED_STATUS_ERROR,
    IOIF_BATLED_STATUS_BUSY,
    IOIF_BATLED_STATUS_TIMEOUT,
} IOIF_BATLEDState_t;


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

IOIF_BATLEDState_t IOIF_InitBatteryLED(IOIF_I2C_t i2c);
IOIF_BATLEDState_t IOIF_RunBatteryLED(uint8_t ledOnBits);


#endif /* IOIF_BATTERYLED_ENABLED */

#endif /* BATTERY_LED_INC_IOIF_BATTERY_LED_H_ */
