

#include "task_state_machine.h"

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

static StateEnum_t TransitionMap(StateEnum_t curr, StateEnum_t cmd);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

// State Functions
StateEntityFuncPtr_t CreateTaskStateEntity(StateFuncPtr ent, StateFuncPtr run, StateFuncPtr ext)
{
    StateEntityFuncPtr_t tStateEntityFuncPtr;
    tStateEntityFuncPtr.onEnter = ent;
    tStateEntityFuncPtr.onRun =   run;
    tStateEntityFuncPtr.onExit =  ext;
    return tStateEntityFuncPtr;
}


// State Machine Interfaces
void InitStateMachine(StateMachineObj_t* StateMachine)
{
    for (int i = 0; i < STATE_MACHINE_N_STATES; i++) {
    	StateMachine->entity[i].onEnter = NULL;
    	StateMachine->entity[i].onRun   = NULL;
    	StateMachine->entity[i].onExit  = NULL;
    }
    StateMachine->entityLifeCycle = TASK_STATE_ENTITY_ENT;
}

void RunStateMachine(StateMachineObj_t* StateMachine)
{
    switch (StateMachine->entityLifeCycle) {
        case TASK_STATE_ENTITY_ENT:
            if (StateMachine->entity[StateMachine->currState].onEnter) {
                StateMachine->entity[StateMachine->currState].onEnter();
            }
            if (StateMachine->entityLifeCycle == TASK_STATE_ENTITY_ENT){
                StateMachine->entityLifeCycle = TASK_STATE_ENTITY_RUN;
            }
            break;

        case TASK_STATE_ENTITY_RUN:
            if (StateMachine->entity[StateMachine->currState].onRun) {
                StateMachine->entity[StateMachine->currState].onRun();
            }
            break;
        
        case TASK_STATE_ENTITY_EXT:
            if (StateMachine->entity[StateMachine->prevState].onExit) {
                StateMachine->entity[StateMachine->prevState].onExit();
            }
            StateMachine->entityLifeCycle = TASK_STATE_ENTITY_ENT;
            break;

        default: // Invalid Lifecycle
            break;
    }
}

void StateTransition(StateMachineObj_t* StateMachine, StateEnum_t CmdState)
{
    StateEnum_t newState = TransitionMap(StateMachine->currState, CmdState);
    if (StateMachine->currState != newState) {
    	StateMachine->prevState = StateMachine->currState;
    	StateMachine->currState = newState;
    	StateMachine->entityLifeCycle = TASK_STATE_ENTITY_EXT;
    }
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

// State & Transition Map
static StateEnum_t TransitionMap(StateEnum_t curr, StateEnum_t cmd)
{
    if (curr == cmd) {
        return curr;
    }

    switch (curr) {
        /*  |--- From State ---|  |------------------------------------------------ To State ---------------------------------------------------| |- If valid -| |else| */
        case TASK_STATE_OFF:     if (0                      || cmd==TASK_STATE_STANDBY      || 0      	                || 0                    )   {return cmd;} break;
        case TASK_STATE_STANDBY: if (cmd==TASK_STATE_OFF    || 0                            || cmd==TASK_STATE_ENABLE   || 0                    )   {return cmd;} break;
        case TASK_STATE_ENABLE:  if (cmd==TASK_STATE_OFF    || cmd==TASK_STATE_STANDBY      || 0         	            || cmd==TASK_STATE_ERROR)   {return cmd;} break;
        case TASK_STATE_ERROR:   if (cmd==TASK_STATE_OFF    || cmd==TASK_STATE_STANDBY      || 0             	        || 0                    )   {return cmd;} break;
        default: break;
    }

    return curr;
}

