

#ifndef TASK_STATE_MACHINE_INC_TASK_MNGR_H_
#define TASK_STATE_MACHINE_INC_TASK_MNGR_H_

#include "module.h"

#include <stdbool.h>

#include "routine_mngr.h"
#include "task_state_machine.h"

#ifdef _USE_OS_RTOS
#include "cmsis_os.h"
#endif /* _USE_OS_RTOS */

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define TASK_CREATE_STATE(taskObj, state, stateEntFncPtr, stateRunFncPtr, stateExtFncPtr, isDefault)  \
	SetTaskStateEntity((taskObj), (state), CreateTaskStateEntity((stateEntFncPtr), (stateRunFncPtr), (stateExtFncPtr)), isDefault)

#define TASK_CREATE_ROUTINE(taskObj, routineID, routineEntFncPtr, routineRunFncPtr, routineExtFncPtr) \
    SetTaskRoutineEntity((taskObj), (routineID), CreateRoutineEntity((routineEntFncPtr), (routineRunFncPtr), (routineExtFncPtr)))


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef struct _TaskObj_t {
    int id;
    int period;

    uint32_t errCode;

    StateMachineObj_t stateMachine;
    RoutineObj_t routine;
    char* name;
} TaskObj_t;


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

void InitTask(TaskObj_t* taskObj);
void RunTask(TaskObj_t* taskObj);

void SetTaskStateEntity(TaskObj_t* taskObj, StateEnum_t state, StateEntityFuncPtr_t stateEntity, bool isDefault);
void SetTaskRoutineEntity(TaskObj_t* taskObj, int routineID, RoutineEntityFuncPtr_t routineEntity);

#ifdef _USE_OS_RTOS
void TaskDelay(TaskObj_t* taskObj);
#endif /* _USE_OS_RTOS */
void SetTaskErrCode(TaskObj_t* taskObj, uint32_t eCode);
uint32_t GetTaskErrCode(TaskObj_t* taskObj);


#endif /* TASK_STATE_MACHINE_INC_TASK_MNGR_H_ */
