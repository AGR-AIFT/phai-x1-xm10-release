

#include "routine_mngr.h"

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

// Routine Entity
RoutineEntityFuncPtr_t CreateRoutineEntity(RoutineFncPtr ent, RoutineFncPtr run, RoutineFncPtr ext)
{
    RoutineEntityFuncPtr_t tEntityFuncPtr;
    tEntityFuncPtr.onEnter = ent;
    tEntityFuncPtr.onRun = run;
    tEntityFuncPtr.onExit = ext;
    return tEntityFuncPtr;
}


// DriveRoutine Interface
void InitRoutine(RoutineObj_t* routine)
{
    for (int i = 0; i < ROUTINE_MAX_ENTITIES; i++) {
    	routine->id[i] = 0;
    	routine->entities[i].onEnter = NULL;
    	routine->entities[i].onRun = NULL;
    	routine->entities[i].onExit = NULL;
    }
    routine->numOfRoutineID = 0;
}

int EntRoutines(RoutineObj_t* routine)
{
	int tRes = 0;
	int tempID = 0;

    for (int i = 0; i < routine->numOfRoutineID; i++){
        //TODO: routine func exception handling

    	tempID = routine->id[i];
		if (routine->entities[tempID].onEnter) {
			tRes = routine->entities[tempID].onEnter();
		}
        if (tRes < 0) {
            return tRes;
        }
    }
    return 0;
}

int RunRoutines(RoutineObj_t* routine)
{
	int tRes = 0;
	int tempID = 0;

    for (int i = 0; i < routine->numOfRoutineID; i++) {
        //TODO: routine func exception handling

    	tempID = routine->id[i];
		if (routine->entities[tempID].onRun) {
			tRes = routine->entities[tempID].onRun();
		}
        if (tRes < 0) {
            return tRes;
        }
    }
    return 0;
}

int ExtRoutines(RoutineObj_t* routine)
{
	int tRes = 0;
	int tempID = 0;

    for (int i = 0; i < routine->numOfRoutineID; i++) {
        //TODO: routine func exception handling

    	tempID = routine->id[i];
		if (routine->entities[tempID].onExit) {
			tRes = routine->entities[tempID].onExit();
		}
        if (tRes < 0) {
            return tRes;
        }
    }
    return 0;
}

void ClearRoutines(RoutineObj_t* routine)
{
    for (int i = 0; i < ROUTINE_MAX_ENTITIES; i++) {
    	routine->id[i] = ROUTINE_DEFAULT_ID;
    }
    routine->numOfRoutineID = 0;
}

int PushRoutine(RoutineObj_t* routine, uint8_t routineID)
{
    if (routine->numOfRoutineID >= ROUTINE_MAX_ENTITIES) {
        return -1;
    }

    for (int i = 0; i < routine->numOfRoutineID; i++) {
    	if(routine->id[i] == routineID){return 0;}
    }
    
    routine->id[routine->numOfRoutineID++] = routineID;
    return 0;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */



