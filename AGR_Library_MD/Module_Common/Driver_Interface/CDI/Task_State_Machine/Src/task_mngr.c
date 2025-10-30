

#include "task_mngr.h"

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

// Device Interface
void InitTask(TaskObj_t* TaskObj)
{
	InitStateMachine(&TaskObj->stateMachine);
    InitRoutine(&TaskObj->routine);
    TaskObj->errCode = 0;
}

void RunTask(TaskObj_t* TaskObj)
{
    RunStateMachine(&TaskObj->stateMachine);
}

void SetTaskStateEntity(TaskObj_t* TaskObj, StateEnum_t state, StateEntityFuncPtr_t stateEntity, bool isDefault)
{
	TaskObj->stateMachine.entity[state] = stateEntity;

    if (isDefault) {
    	TaskObj->stateMachine.currState = state;
    	TaskObj->stateMachine.prevState = state;
    }
}

//TODO: id, order range/duplication check
void SetTaskRoutineEntity(TaskObj_t* TaskObj, int routineID, RoutineEntityFuncPtr_t routineEntity)
{
	TaskObj->routine.entities[routineID] = routineEntity;
}

#ifdef _USE_OS_RTOS
void TaskDelay(TaskObj_t* TaskObj)
{
    osDelay(TaskObj->period);
}
#endif /* _USE_OS_RTOS */

void SetTaskErrCode(TaskObj_t* TaskObj, uint32_t eCode)
{
    TaskObj->errCode = eCode;
}

uint32_t GetTaskErrCode(TaskObj_t* TaskObj)
{
    return TaskObj->errCode;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

