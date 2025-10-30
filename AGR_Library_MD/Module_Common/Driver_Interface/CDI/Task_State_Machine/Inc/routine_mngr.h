

#ifndef TASK_STATE_MACHINE_INC_ROUTINE_MNGR_H_
#define TASK_STATE_MACHINE_INC_ROUTINE_MNGR_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define ROUTINE_MAX_ENTITIES    32
#define ROUTINE_DEFAULT_ID      -1


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

// Routine Entity
typedef int (*RoutineFncPtr) (void);

typedef enum _RoutineEntityEnum_t {
    TASK_ROUTINE_ENTITY_ENT,
	TASK_ROUTINE_ENTITY_RUN,
	TASK_ROUTINE_ENTITY_EXT,
} RoutineEntityEnum_t;

typedef struct _RoutineEntityFuncPtr_t {
    RoutineFncPtr onEnter;
    RoutineFncPtr onRun;
    RoutineFncPtr onExit;
} RoutineEntityFuncPtr_t;

typedef struct _RoutineObj_t {
    int id[ROUTINE_MAX_ENTITIES];
    size_t numOfRoutineID;
    RoutineEntityFuncPtr_t entities[ROUTINE_MAX_ENTITIES];
} RoutineObj_t;

RoutineEntityFuncPtr_t CreateRoutineEntity(RoutineFncPtr ent, RoutineFncPtr run, RoutineFncPtr ext);


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

// DriveRoutine Interface
void InitRoutine(RoutineObj_t* routine);

int EntRoutines(RoutineObj_t* routine);
int RunRoutines(RoutineObj_t* routine);
int ExtRoutines(RoutineObj_t* routine);

void ClearRoutines(RoutineObj_t* routine);

int PushRoutine(RoutineObj_t* routine, uint8_t routineID);


#endif /* TASK_STATE_MACHINE_INC_ROUTINE_MNGR_H_ */
