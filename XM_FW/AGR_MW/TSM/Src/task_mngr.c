

#include "task_mngr.h"
#include <stddef.h> // NULL

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */

/* --- Configuration --- */
#define MAX_SYSTEM_TASKS    10  // 시스템 전체에서 사용할 최대 태스크 수

/* [내부 확장 구조체] */
typedef struct {
    // [1] 공개 영역 (반드시 맨 앞!)
    TsmObject_t public; 
    
    // [2] 비공개 엔진
    TaskStateMachine_t engine;
} InternalTask_t;

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

static InternalTask_t s_taskPool[MAX_SYSTEM_TASKS];
static uint8_t s_taskAllocIdx = 0;

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

TsmObject_t* TaskMngr_Create(uint8_t initial_state_id)
{
    if (s_taskAllocIdx >= MAX_SYSTEM_TASKS) {
        return NULL; // 할당 초과
    }

    // 정적 풀에서 하나 꺼내옴
    InternalTask_t* newTask = &s_taskPool[s_taskAllocIdx++];
    
    // 내부 TSM 초기화
    TSM_Init(&newTask->engine, initial_state_id);

    // 공개 데이터 초기화
    newTask->public.current_state_id = initial_state_id;
    newTask->public.prev_state_id    = initial_state_id;
    newTask->public.current_step     = TSM_LIFECYCLE_ENTRY;
    newTask->public.prev_step        = TSM_LIFECYCLE_ENTRY;
    
    return (TsmObject_t*)newTask;
}

void TaskMngr_AddState(TsmObject_t* obj, uint8_t state_id,
                  EntryFunc_t entry, LoopFunc_t loop, ExitFunc_t exit)
{
    if (obj == NULL) return;
    InternalTask_t* task = (InternalTask_t*)obj; // Downcasting

    // TSM API 호출 (변경된 파라미터 이름 반영)
    TSM_AddState(&task->engine, state_id, entry, loop, exit);
}

void TaskMngr_Run(TsmObject_t* obj)
{
    if (obj == NULL) return;
    InternalTask_t* task = (InternalTask_t*)obj;

    // 1. 실행
    TSM_Run(&task->engine);

    // 2. 모니터링 데이터 동기화 (Sync)
    // State ID 변경 감지
    if (task->public.current_state_id != task->engine.curr_state_id) {
        task->public.prev_state_id = task->public.current_state_id;
        task->public.current_state_id = task->engine.curr_state_id;
    }
    
    // Lifecycle 변경 감지
    if (task->public.current_step != (TsmLifecycle_e)task->engine.lifecycle) {
        task->public.prev_step = task->public.current_step;
        task->public.current_step = (TsmLifecycle_e)task->engine.lifecycle;
    }
}

int TaskMngr_Transition(TsmObject_t* obj, uint8_t next_state_id)
{
    if (obj == NULL) return -1;
    InternalTask_t* task = (InternalTask_t*)obj;
    return TSM_TransitionTo(&task->engine, next_state_id);
}

void TaskMngr_ResetPool(void)
{
    s_taskAllocIdx = 0;
    TSM_ResetPool();
}

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

