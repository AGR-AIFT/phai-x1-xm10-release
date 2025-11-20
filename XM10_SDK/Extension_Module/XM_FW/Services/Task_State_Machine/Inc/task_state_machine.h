/**
 * @file task_state_machine.h
 * @brief Routine 의존성을 제거한 순수 FSM 엔진 헤더
 * @details Entry -> Loop -> Exit 의 명확한 생명주기를 관리합니다.
 */

#ifndef TASK_STATE_MACHINE_INC_TASK_STATE_MACHINE_H_
#define TASK_STATE_MACHINE_INC_TASK_STATE_MACHINE_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/* --- 1. 함수 포인터 타입 정의 (표준화된 네이밍) --- */
typedef void (*EntryFunc_t)(void); // 상태 진입 시 1회 실행
typedef void (*LoopFunc_t)(void);  // 상태 유지 중 반복 실행
typedef void (*ExitFunc_t)(void);  // 상태 탈출 시 1회 실행

/* --- 2. TSM 내부 라이프사이클 상태 --- */
typedef enum {
    TSM_LIFECYCLE_ENTRY, // 진입 단계
    TSM_LIFECYCLE_LOOP,  // 반복 단계
    TSM_LIFECYCLE_EXIT   // 종료 단계
} TsmLifecycle_e;

/* --- 3. 상태 노드 구조체 (Linked List Node) --- */
typedef struct TaskState {
    uint8_t id;            // 상태 고유 ID
    
    EntryFunc_t on_entry;   // [진입]
    LoopFunc_t  on_loop;    // [반복]
    ExitFunc_t  on_exit;    // [종료]
    
    struct TaskState* pNext; // 다음 노드 포인터
} TaskState_t;

/* --- 4. TSM 메인 객체 --- */
typedef struct {
    uint8_t curr_state_id; // 현재 상태 ID (디버깅용)
    
    // 실행 포인터 캐싱 (검색 속도 최적화)
    TaskState_t* pCurrNode; // 현재 실행 중인 상태 노드
    TaskState_t* pNextNode; // 전환 예정인 상태 노드 (Transition 시 사용)
    
    // 현재 실행 단계
    TsmLifecycle_e lifecycle;
    
    // 전체 상태 리스트 헤드
    TaskState_t* pStateHead;
} TaskStateMachine_t;

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

/**
 * @brief TSM 객체를 초기화합니다.
 * @param tsm TSM 객체 포인터
 * @param initial_state_id 초기 시작 상태 ID
 */
void TSM_Init(TaskStateMachine_t* tsm, uint8_t initial_state_id);

/**
 * @brief TSM에 새로운 상태(State)를 등록합니다.
 * @param entry/loop/exit 해당 시점에 실행할 함수 (NULL 가능)
 */
void TSM_AddState(TaskStateMachine_t* tsm, uint8_t state_id,
                  EntryFunc_t entry, LoopFunc_t loop, ExitFunc_t exit);

/**
 * @brief TSM을 실행합니다. (User Task의 루프 또는 Timer ISR에서 주기적 호출)
 */
void TSM_Run(TaskStateMachine_t* tsm);

/**
 * @brief 안전하게 상태 전환을 요청합니다.
 * @details 즉시 전환하지 않고, 현재 상태의 Exit을 수행하도록 예약합니다.
 */
void TSM_TransitionTo(TaskStateMachine_t* tsm, uint8_t next_state_id);

#endif /* TASK_STATE_MACHINE_INC_TASK_STATE_MACHINE_H_ */
