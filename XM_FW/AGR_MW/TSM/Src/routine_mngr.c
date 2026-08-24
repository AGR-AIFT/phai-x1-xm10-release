
#include "routine_mngr.h"
#include <stddef.h> // for NULL
#include <string.h> // for memset

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

void RoutineMngr_Init(RoutineMngr_t* mngr)
{
    if (mngr == NULL) return;
    // 전체 0으로 초기화 (count = 0)
    memset(mngr, 0, sizeof(RoutineMngr_t));
}

bool RoutineMngr_Add(RoutineMngr_t* mngr, uint32_t id, 
                     RoutineFunc_t init, RoutineFunc_t run, RoutineFunc_t exit)
{
    if (mngr == NULL) return false;
    if (mngr->count >= MAX_ROUTINES_PER_MNGR) return false; // Full

    Routine_t* r = &mngr->routines[mngr->count];
    r->id = id;
    r->is_enabled = false; // 기본은 꺼둠
    r->on_init = init;
    r->on_run  = run;
    r->on_exit = exit;

    mngr->count++;
    return true;
}

void RoutineMngr_SetEnable(RoutineMngr_t* mngr, uint32_t id, bool enable)
{
    if (mngr == NULL) return;

    // ID로 루틴 찾기
    for (uint32_t i = 0; i < mngr->count; i++) {
        Routine_t* r = &mngr->routines[i];
        
        if (r->id == id) {
            // 상태 변화가 없다면 리턴
            if (r->is_enabled == enable) return;

            if (enable) {
                // [OFF -> ON] 켜는 순간 Init 호출
                if (r->on_init != NULL) r->on_init();
                r->is_enabled = true;
            } else {
                // [ON -> OFF] 끄는 순간 Exit 호출 (매우 중요: 안전장치)
                if (r->on_exit != NULL) r->on_exit();
                r->is_enabled = false;
            }
            return; // 찾았으니 종료
        }
    }
}

void RoutineMngr_RunAll(RoutineMngr_t* mngr)
{
    if (mngr == NULL) return;

    for (uint32_t i = 0; i < mngr->count; i++) {
        Routine_t* r = &mngr->routines[i];
        
        // 켜져 있는 기능만 실행
        if (r->is_enabled && r->on_run != NULL) {
            r->on_run();
        }
    }
}

// // Routine Entity
// RoutineEntityFuncPtr_t CreateRoutineEntity(RoutineFncPtr ent, RoutineFncPtr run, RoutineFncPtr ext)
// {
//     RoutineEntityFuncPtr_t tEntityFuncPtr;
//     tEntityFuncPtr.onEnter = ent;
//     tEntityFuncPtr.onRun = run;
//     tEntityFuncPtr.onExit = ext;
//     return tEntityFuncPtr;
// }


// // DriveRoutine Interface
// void InitRoutine(RoutineObj_t* routine)
// {
//     for (int i = 0; i < ROUTINE_MAX_ENTITIES; i++) {
//     	routine->id[i] = 0;
//     	routine->entities[i].onEnter = NULL;
//     	routine->entities[i].onRun = NULL;
//     	routine->entities[i].onExit = NULL;
//     }
//     routine->numOfRoutineID = 0;
// }

// int EntRoutines(RoutineObj_t* routine)
// {
// 	int tRes = 0;
// 	int tempID = 0;

//     for (int i = 0; i < routine->numOfRoutineID; i++){
//         //TODO: routine func exception handling

//     	tempID = routine->id[i];
// 		if (routine->entities[tempID].onEnter) {
// 			tRes = routine->entities[tempID].onEnter();
// 		}
//         if (tRes < 0) {
//             return tRes;
//         }
//     }
//     return 0;
// }

// int RunRoutines(RoutineObj_t* routine)
// {
// 	int tRes = 0;
// 	int tempID = 0;

//     for (int i = 0; i < routine->numOfRoutineID; i++) {
//         //TODO: routine func exception handling

//     	tempID = routine->id[i];
// 		if (routine->entities[tempID].onRun) {
// 			tRes = routine->entities[tempID].onRun();
// 		}
//         if (tRes < 0) {
//             return tRes;
//         }
//     }
//     return 0;
// }

// int ExtRoutines(RoutineObj_t* routine)
// {
// 	int tRes = 0;
// 	int tempID = 0;

//     for (int i = 0; i < routine->numOfRoutineID; i++) {
//         //TODO: routine func exception handling

//     	tempID = routine->id[i];
// 		if (routine->entities[tempID].onExit) {
// 			tRes = routine->entities[tempID].onExit();
// 		}
//         if (tRes < 0) {
//             return tRes;
//         }
//     }
//     return 0;
// }

// void ClearRoutines(RoutineObj_t* routine)
// {
//     for (int i = 0; i < ROUTINE_MAX_ENTITIES; i++) {
//     	routine->id[i] = ROUTINE_DEFAULT_ID;
//     }
//     routine->numOfRoutineID = 0;
// }

// int PushRoutine(RoutineObj_t* routine, uint8_t routineID)
// {
//     if (routine->numOfRoutineID >= ROUTINE_MAX_ENTITIES) {
//         return -1;
//     }

//     for (int i = 0; i < routine->numOfRoutineID; i++) {
//     	if(routine->id[i] == routineID){return 0;}
//     }
    
//     routine->id[routine->numOfRoutineID++] = routineID;
//     return 0;
// }


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */



