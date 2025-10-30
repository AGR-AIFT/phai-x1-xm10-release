/**
 *-----------------------------------------------------------
 *                   NZR LED DRIVER HEADER
 *-----------------------------------------------------------
 * @file ioif_nzrled.h
 * @date Created on: Sep 2, 2023
 * @author AngelRobotics HW Team
 * @brief Header file for the NZR LED driver.
 *
 * This header file provides the interface definitions and macros
 * for controlling the NZR LED.
 * 
 * This driver leverages Pulse Width Modulation (PWM) to control the LEDs 
 * and assumes the presence of a hardware timer for accurate PWM signal generation.
 * 
 * @ref Documentation or Datasheet reference if any
 */

#ifndef NZR_LED_INC_IOIF_NZR_LED_H_
#define NZR_LED_INC_IOIF_NZR_LED_H_

#include "module.h"

/** @defgroup TIM TIM
  * @brief TIM NZR LED module driver
  * @{
  */
#ifdef IOIF_NZRLED_ENABLED

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "ioif_tim_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define NZR_DUTY_HIGH	    	144		//
#define NZR_DUTY_LOW	    	72		//72
#define NZR_DUTY_REST	    	0

#ifdef WALKON5_CM_ENABLED
#define LED_NUM_PER_CH			2
#define CH_NUM					2
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
#define LED_NUM_PER_CH			2
#define CH_NUM					2
#endif /* L30_CM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
#define LED_NUM_PER_CH			4
#define CH_NUM					1
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
#define LED_NUM_PER_CH			4
#define CH_NUM					1
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
#define LED_NUM_PER_CH			4
#define CH_NUM					1
#endif /* L30_MD_REV08_ENABLED */

#define COLOR_NUM				4
#define BYTE_PER_COLOR			8
#define COLOR_TOTAL_BITS 	 	COLOR_NUM * BYTE_PER_COLOR
#define REST_SIGNAL_BYTE_NUM	64
#define TOTAL_BYTE_NUM			(LED_NUM_PER_CH * BYTE_PER_COLOR * COLOR_NUM) + REST_SIGNAL_BYTE_NUM

// Color definitions for GRBW LEDs
#define IOIF_COLOR_NONE     0x00000000
#define IOIF_COLOR_GREEN    0xFF000000
#define IOIF_COLOR_RED      0x00FF0000
#define IOIF_COLOR_BLUE     0x0000FF00
#define IOIF_COLOR_WHITE    0x000000FF

#define IOIF_NZR_LED1	0
#define IOIF_NZR_LED2	1
#define IOIF_NZR_LED3	2
#define IOIF_NZR_LED4	3


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Enumeration to describe the state of the NZR LED.
 */
typedef enum _IOIF_NZRLEDState_t {
    IOIF_NZRLED_STATUS_OK = 0,
    IOIF_NZRLED_STATUS_ERROR,
    IOIF_NZRLED_STATUS_BUSY,
    IOIF_NZRLED_STATUS_TIMEOUT,
} IOIF_NZRLEDState_t;

/**
 * @brief Structure that holds parameters for each NZR LED.
 */
typedef struct _IOIF_NzrParams_t {
    float stepSize[COLOR_NUM];       // GRBW step sizes
    uint32_t stepNum;

    uint32_t colorCode;              // GRBW color code
    uint32_t activeColor;            // Active color for the LED

    uint32_t blinkTime;              // Blink time for the LED in ms
    uint32_t tickCount;

    uint8_t nzrId;                   // NZR LED ID
    uint8_t rCode;                   // Red code
    uint8_t gCode;                   // Green code
    uint8_t bCode;                   // Blue code
    uint8_t wCode;                   // White code
} IOIF_NzrParams_t;

/**
 * @brief Structure that holds the entire NZR LED object.
 */
typedef struct _IOIF_NzrLedObj_t {
    IOIF_NzrParams_t nzrParams[LED_NUM_PER_CH]; // NZR LED parameters
    uint32_t timCh;             // Timer channel
    uint32_t* pNzrDutyBuff;     // Pointer to the NZR duty buffer
    uint16_t parentLoopTime;    // Parent loop time in ms
} IOIF_NzrLedObj_t;


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

IOIF_NZRLEDState_t IOIF_InitNZRLED(IOIF_Tim_t timer, IOIF_TimCh_t timCh, IOIF_NzrLedObj_t* nzrObj, uint16_t loopTime);
IOIF_NZRLEDState_t IOIF_SetNZRLED(IOIF_NzrLedObj_t* nzrObj, uint8_t ledIndex, uint32_t color, uint32_t blinkTime);
IOIF_NZRLEDState_t IOIF_RunNZRLED(IOIF_TimCh_t timCh, IOIF_NzrLedObj_t* nzrObj);
IOIF_NZRLEDState_t IOIF_ClearNZRLED(IOIF_TimCh_t timCh, IOIF_NzrLedObj_t* nzrObj);


#endif /* IOIF_NZRLED_ENABLED */

#endif /* NZR_LED_INC_IOIF_NZR_LED_H_ */
