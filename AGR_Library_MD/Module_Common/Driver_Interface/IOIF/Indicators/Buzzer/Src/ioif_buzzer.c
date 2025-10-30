

#include "ioif_buzzer.h"

/** @defgroup TIM TIM
  * @brief TIM Buzzer module driver
  * @{
  */
#ifdef IOIF_BUZZER_ENABLED

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

static void SetDuty(IOIF_Tim_t timer, IOIF_BuzzerObj_t* buzzerObj, uint8_t buzzerDuty);
static void CountTick(IOIF_BuzzerObj_t* buzzerObj);
static void ResetTick(IOIF_BuzzerObj_t* buzzerObj);
static void ToggleBuzzer(IOIF_Tim_t timer, IOIF_BuzzerObj_t* buzzerObj);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

void IOIF_InitBuzzer(IOIF_Tim_t timer, IOIF_TimCh_t timCh, IOIF_BuzzerObj_t* buzzerObj, uint32_t parentLoopTime)
{
	memset(buzzerObj, 0, sizeof(IOIF_BuzzerObj_t));
	buzzerObj->parent_loop_time = parentLoopTime;
	SetDuty(timer, buzzerObj, BUZZER_DEFAULT_OFF_DUTY);

	IOIF_StartPWM(timer, timCh);
}

void IOIF_DeInitBuzzer(IOIF_Tim_t timer, IOIF_TimCh_t timCh, IOIF_BuzzerObj_t* buzzerObj)
{	
	IOIF_StopPWM(timer, timCh);
}

void IOIF_ConfigBuzzer(IOIF_BuzzerObj_t* buzzerObj, uint16_t epiTime, uint16_t* togPoints, uint8_t togNum)
{
	ResetTick(buzzerObj);

	buzzerObj->episode_time = epiTime;
	buzzerObj->toggle_points = togPoints;
	buzzerObj->toggle_point_num = togNum;
	buzzerObj->toggle_index = 0;
	buzzerObj->onoff_state = BUZZER_OFF;
}

void IOIF_ExecuteBuzzer(IOIF_Tim_t timer, IOIF_BuzzerObj_t* buzzerObj)
{
	uint32_t tCurrentTime = 0;  // ms

	tCurrentTime = (uint32_t)buzzerObj->tick_count * (uint32_t)buzzerObj->parent_loop_time;

	/* End of Episode*/
	if(tCurrentTime >= (uint32_t)buzzerObj->episode_time) {
		SetDuty(timer, buzzerObj, BUZZER_DEFAULT_OFF_DUTY);
		return;
	}

	/* End of Toggle Points*/
	if(buzzerObj->toggle_index >= buzzerObj->toggle_point_num){
		SetDuty(timer, buzzerObj, BUZZER_DEFAULT_OFF_DUTY);
		return;
	}

	/* Toggle Buzzer */
	if(tCurrentTime >= buzzerObj->toggle_points[buzzerObj->toggle_index]){
		ToggleBuzzer(timer, buzzerObj);
		buzzerObj->toggle_index++;
	}

	CountTick(buzzerObj);
}

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

static void SetDuty(IOIF_Tim_t timer, IOIF_BuzzerObj_t* buzzerObj, uint8_t buzzerDuty)
{
	uint32_t tCnt = 0;

	buzzerObj->duty = buzzerDuty;

	if (buzzerDuty == 0) {
		IOIF_SetCCRCnt(timer, 0);
		return;
	}

	tCnt = (IOIF_GetARRCnt(timer) * buzzerDuty / 100) - 1;
	IOIF_SetCCRCnt(timer, tCnt);
}

static void CountTick(IOIF_BuzzerObj_t* buzzerObj)
{	
    buzzerObj->tick_count++;
}

static void ResetTick(IOIF_BuzzerObj_t* buzzerObj)
{	
    buzzerObj->tick_count = 0;
}

static void ToggleBuzzer(IOIF_Tim_t timer, IOIF_BuzzerObj_t* buzzerObj)
{
	if (buzzerObj->onoff_state == BUZZER_OFF) {
		SetDuty(timer, buzzerObj, BUZZER_DEFAULT_ON_DUTY);
		buzzerObj->onoff_state = BUZZER_ON;
	} else if (buzzerObj->onoff_state == BUZZER_ON) {
		SetDuty(timer, buzzerObj, BUZZER_DEFAULT_OFF_DUTY);
		buzzerObj->onoff_state = BUZZER_OFF;
	}
}


#endif /* IOIF_BUZZER_ENABLED */ 
