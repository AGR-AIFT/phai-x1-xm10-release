

#include "data_object_common.h"

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

#ifdef CM_MODULE
TaskObj_t* pTaskObjs[TASK_NUMS];

#endif

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

#ifdef CM_MODULE
void DOPC_AssignTaskID(TaskObj_t* p_taskObj, int t_IDX)
{
	pTaskObjs[t_IDX] = p_taskObj;
}

uint8_t DOPC_GetTaskState(int t_IDX) 
{
    return pTaskObjs[t_IDX]->stateMachine.currState;
}

void DOPC_SetTaskState(int t_IDX, uint8_t t_state) 
{
	StateTransition(&pTaskObjs[t_IDX]->stateMachine, t_state);
}

#endif

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */


