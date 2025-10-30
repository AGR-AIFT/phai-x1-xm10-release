

#ifndef BUZZER_INC_IOIF_BUZZER_H_
#define BUZZER_INC_IOIF_BUZZER_H_

#include "module.h"

/** @defgroup TIM TIM
  * @brief TIM Buzzer module driver
  * @{
  */
#ifdef IOIF_BUZZER_ENABLED

#include <string.h>

#include "ioif_tim_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define BUZZER_TOGGLE_ARR_SIZE 		10
#define BUZZER_DEFAULT_ON_DUTY 		50
#define BUZZER_DEFAULT_OFF_DUTY 	0


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef enum _BuzzerState_t {
	BUZZER_OFF = 0,
	BUZZER_ON,
} BuzzerState_t;

typedef struct _IOIF_BuzzerObj_t {
	uint16_t parent_loop_time; //ms
	uint8_t duty;

	BuzzerState_t onoff_state;

	uint16_t episode_time;
	uint8_t tick_count;

	uint16_t* toggle_points;		// ms, The element must be lower than episode_count)
	uint8_t toggle_point_num;
	uint8_t toggle_index;
} IOIF_BuzzerObj_t;


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

void IOIF_InitBuzzer(IOIF_Tim_t timer, IOIF_TimCh_t timCh, IOIF_BuzzerObj_t* buzzerObj, uint32_t parentLoopTime);
void IOIF_DeInitBuzzer(IOIF_Tim_t timer, IOIF_TimCh_t timCh, IOIF_BuzzerObj_t* buzzerObj);
void IOIF_ConfigBuzzer(IOIF_BuzzerObj_t* buzzerObj, uint16_t epiTime, uint16_t* togPoints, uint8_t togNum);
void IOIF_ExecuteBuzzer(IOIF_Tim_t timer, IOIF_BuzzerObj_t* buzzerObj);


#endif /* IOIF_BUZZER_ENABLED */

#endif /* BUZZER_INC_IOIF_BUZZER_H_ */
