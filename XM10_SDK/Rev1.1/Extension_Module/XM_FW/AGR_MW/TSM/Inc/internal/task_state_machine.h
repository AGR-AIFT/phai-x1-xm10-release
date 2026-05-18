/**
 * @file  internal/task_state_machine.h
 * @brief TSM internal FSM engine (private — not part of AGR_MW public API)
 * @note  Consumers must use task_mngr.h. Direct include of this file from
 *        outside the TSM module is a build error by convention.
 */

#ifndef TSM_INTERNAL_TASK_STATE_MACHINE_H_
#define TSM_INTERNAL_TASK_STATE_MACHINE_H_

#include <stdint.h>
#include <stdbool.h>

#include "task_mngr.h"  /* public types: TsmLifecycle_e, EntryFunc_t, LoopFunc_t, ExitFunc_t */

/* Engine state node (linked list). */
typedef struct TaskState {
    uint8_t id;

    EntryFunc_t on_entry;
    LoopFunc_t  on_loop;
    ExitFunc_t  on_exit;

    struct TaskState* pNext;
} TaskState_t;

/* Engine object. */
typedef struct {
    uint8_t curr_state_id;

    TaskState_t* pCurrNode;
    TaskState_t* pNextNode;

    TsmLifecycle_e lifecycle;

    TaskState_t* pStateHead;
} TaskStateMachine_t;

void TSM_ResetPool(void);
void TSM_Init(TaskStateMachine_t* tsm, uint8_t initial_state_id);
void TSM_AddState(TaskStateMachine_t* tsm, uint8_t state_id,
                  EntryFunc_t entry, LoopFunc_t loop, ExitFunc_t exit);
void TSM_Run(TaskStateMachine_t* tsm);
int32_t TSM_TransitionTo(TaskStateMachine_t* tsm, uint8_t next_state_id);

#endif /* TSM_INTERNAL_TASK_STATE_MACHINE_H_ */
