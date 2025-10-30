

#ifndef TASK_STATE_MACHINE_INC_TASK_STATE_MACHINE_H_
#define TASK_STATE_MACHINE_INC_TASK_STATE_MACHINE_H_

#include <unistd.h>

#include "module.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

// State & Transition Map
#define STATE_MACHINE_N_STATES 4


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

// State Functions
typedef void (*StateFuncPtr) (void);

typedef enum _StateEnum_t {
    TASK_STATE_OFF       = 0,
	TASK_STATE_STANDBY   = 1,
	TASK_STATE_ENABLE    = 2,
	TASK_STATE_ERROR     = 3,
} StateEnum_t;

typedef struct _StateEntityFuncPtr_t {
    StateFuncPtr onEnter;
    StateFuncPtr onRun;
    StateFuncPtr onExit;
} StateEntityFuncPtr_t;

// State Machine
typedef enum _StateEntityLifeCycle_t {
    TASK_STATE_ENTITY_ENT,
	TASK_STATE_ENTITY_RUN,
	TASK_STATE_ENTITY_EXT,
} StateEntityLifeCycle_t;

typedef struct _StateMachineObj_t {
    StateEnum_t currState;
    StateEnum_t prevState;
    StateEntityLifeCycle_t entityLifeCycle;

    StateEntityFuncPtr_t entity[STATE_MACHINE_N_STATES];
} StateMachineObj_t;

StateEntityFuncPtr_t CreateTaskStateEntity(StateFuncPtr ent, StateFuncPtr run, StateFuncPtr ext);


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

// State Machine Interfaces
void InitStateMachine(StateMachineObj_t* StateMachine);
void RunStateMachine(StateMachineObj_t* StateMachine);

void StateTransition(StateMachineObj_t* StateMachine, StateEnum_t CmdState);


#endif /* TASK_STATE_MACHINE_INC_TASK_STATE_MACHINE_H_ */
