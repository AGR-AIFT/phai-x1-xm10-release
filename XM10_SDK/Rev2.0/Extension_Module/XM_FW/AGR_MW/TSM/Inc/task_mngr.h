/**
 * @file  task_mngr.h
 * @brief AGR_MW TSM public API — task/FSM lifecycle manager
 * @details
 *   This is the only public entry point for TSM. The internal FSM engine
 *   (task_state_machine.h) is private and lives under TSM/Inc/internal/.
 */

#ifndef AGR_MW_TSM_INC_TASK_MNGR_H_
#define AGR_MW_TSM_INC_TASK_MNGR_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 *                     TYPE DECLARATIONS
 *-----------------------------------------------------------
 */

/* State callbacks. */
typedef void (*EntryFunc_t)(void);
typedef void (*LoopFunc_t)(void);
typedef void (*ExitFunc_t)(void);

/* State lifecycle phase. */
typedef enum {
    TSM_LIFECYCLE_ENTRY,
    TSM_LIFECYCLE_LOOP,
    TSM_LIFECYCLE_EXIT
} TsmLifecycle_e;

/* Opaque task handle. Consumers receive a pointer from TaskMngr_Create()
 * and may only pass it back to TaskMngr_* APIs. Direct field access is
 * unavailable by design — use the getters below. */
typedef struct TsmObject_s TsmObject_t;

/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/** @brief Allocate a task from the static pool. Returns NULL if pool full. */
TsmObject_t* TaskMngr_Create(uint8_t initial_state_id);

/** @brief Register a state with Entry/Loop/Exit callbacks. */
void TaskMngr_AddState(TsmObject_t* obj, uint8_t state_id,
                       EntryFunc_t entry, LoopFunc_t loop, ExitFunc_t exit);

/** @brief Advance the task one tick. Call from a periodic context. */
void TaskMngr_Run(TsmObject_t* obj);

/** @brief Request a safe transition (exit current → enter next).
 *  @return 0 on success, -1 if state id not registered. */
int32_t TaskMngr_Transition(TsmObject_t* obj, uint8_t next_state_id);

/** @brief Reset the static pool (invalidates all outstanding handles). */
void TaskMngr_ResetPool(void);

/**
 *------------------------------------------------------------
 *                     STATE ACCESSORS
 *------------------------------------------------------------
 */

/** @brief Current state id, or 0xFF if obj is NULL. */
uint8_t TaskMngr_GetStateId(const TsmObject_t* obj);

/** @brief Current lifecycle phase, or TSM_LIFECYCLE_ENTRY if obj is NULL. */
TsmLifecycle_e TaskMngr_GetLifecycle(const TsmObject_t* obj);

/** @brief Previous state id, or 0xFF if obj is NULL. */
uint8_t TaskMngr_GetPrevStateId(const TsmObject_t* obj);

/** @brief Previous lifecycle phase, or TSM_LIFECYCLE_ENTRY if obj is NULL. */
TsmLifecycle_e TaskMngr_GetPrevLifecycle(const TsmObject_t* obj);

#endif /* AGR_MW_TSM_INC_TASK_MNGR_H_ */
