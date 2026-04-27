/**
 * @file task_state_machine.c
 * @brief Routine 의존성 제거 및 정적 메모리 할당 적용 구현부
 */

#include "task_state_machine.h"
#include <stddef.h> // for NULL

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */

/* --- Configuration: 최대 상태 노드 개수 (프로젝트 규모에 맞춰 조절) --- */
#define MAX_TOTAL_STATES    64

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

/* --- Object Pool --- */
static TaskState_t s_stateNodes[MAX_TOTAL_STATES];
static uint8_t s_nodeAllocIndex = 0;

/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static TaskState_t* _AllocStateNode(void);
static TaskState_t* _FindStateNode(TaskStateMachine_t* tsm, uint8_t id);

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

void TSM_ResetPool(void)
{
    s_nodeAllocIndex = 0;
}

void TSM_Init(TaskStateMachine_t* tsm, uint8_t initial_state_id)
{
    if (tsm == NULL) return;

    tsm->curr_state_id = initial_state_id;
    tsm->pStateHead = NULL;
    tsm->pCurrNode = NULL;
    tsm->pNextNode = NULL;
    
    // 초기 상태는 '진입(Entry)'부터 시작
    tsm->lifecycle = TSM_LIFECYCLE_ENTRY;
    
    // *참고: s_nodeAllocIndex는 전역 자원이므로 여기서 초기화하지 않음
}

void TSM_AddState(TaskStateMachine_t* tsm, uint8_t state_id,
                  EntryFunc_t entry, LoopFunc_t loop, ExitFunc_t exit)
{
    if (tsm == NULL) return;

    // 1. 중복 검사: 이미 등록된 state_id면 콜백만 갱신
    TaskState_t* existing = _FindStateNode(tsm, state_id);
    if (existing != NULL) {
        existing->on_entry = entry;
        existing->on_loop  = loop;
        existing->on_exit  = exit;
        return;
    }

    // 2. 메모리 풀에서 노드 할당
    TaskState_t* newState = _AllocStateNode();
    if (newState == NULL) return; // 할당 실패

    // 3. 노드 데이터 채우기
    newState->id = state_id;
    newState->on_entry = entry;
    newState->on_loop  = loop;
    newState->on_exit  = exit;

    // 4. 연결 리스트에 추가 (Head에 삽입 방식 - O(1))
    newState->pNext = tsm->pStateHead;
    tsm->pStateHead = newState;

    // 5. 만약 초기 설정된 ID라면 현재 노드로 등록
    if (tsm->curr_state_id == state_id) {
        tsm->pCurrNode = newState;
    }
}

void TSM_Run(TaskStateMachine_t* tsm)
{
    // 현재 상태 노드가 없으면 실행 불가
    if (tsm == NULL || tsm->pCurrNode == NULL) return;

    // 라이프사이클에 따른 실행 분기
    switch (tsm->lifecycle) {
        case TSM_LIFECYCLE_ENTRY:
            // [1. 진입]
            if (tsm->pCurrNode->on_entry != NULL) {
                tsm->pCurrNode->on_entry();
            }
            // Entry 완료 후 자동으로 Loop 상태로 전이
            tsm->lifecycle = TSM_LIFECYCLE_LOOP;
            break;

        case TSM_LIFECYCLE_LOOP:
            // [2. 반복]
            if (tsm->pCurrNode->on_loop != NULL) {
                tsm->pCurrNode->on_loop();
            }
            // 별도의 Transition 요청이 없으면 계속 LOOP 유지
            break;

        case TSM_LIFECYCLE_EXIT:
            // [3. 종료]
            if (tsm->pCurrNode->on_exit != NULL) {
                tsm->pCurrNode->on_exit();
            }

            // 종료 로직 수행 후, 예약된 다음 상태(NextNode)로 교체
            if (tsm->pNextNode != NULL) {
                tsm->pCurrNode = tsm->pNextNode;        // 노드 교체
                tsm->curr_state_id = tsm->pNextNode->id;// ID 업데이트
                tsm->pNextNode = NULL;                  // 예약 초기화
            }

            // 새로운 상태의 진입(Entry)을 위해 라이프사이클 리셋
            tsm->lifecycle = TSM_LIFECYCLE_ENTRY;
            break;

        default:
            // 메모리 손상 등으로 lifecycle 값이 유효하지 않은 경우 안전하게 복구
            tsm->lifecycle = TSM_LIFECYCLE_ENTRY;
            break;
    }
}

int TSM_TransitionTo(TaskStateMachine_t* tsm, uint8_t next_state_id)
{
    if (tsm == NULL) return -1;

    // 1. 이미 해당 상태이거나, 현재 전환 중(EXIT)이라면 중복 요청 무시
    if (tsm->curr_state_id == next_state_id) return 0;
    if (tsm->lifecycle == TSM_LIFECYCLE_EXIT) return 0;

    // 2. 이동할 타겟 노드 검색
    TaskState_t* targetNode = _FindStateNode(tsm, next_state_id);

    if (targetNode == NULL) {
        return -1; // 대상 상태를 찾을 수 없음
    }

    // 3. 다음 상태 예약
    tsm->pNextNode = targetNode;

    // 4. 현재 상태를 '종료(EXIT)' 단계로 변경
    // -> 다음 TSM_Run() 호출 시 on_exit()가 실행되고 상태가 바뀜
    tsm->lifecycle = TSM_LIFECYCLE_EXIT;
    return 0;
}

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief 정적 풀에서 비어있는 노드 하나를 할당받습니다.
 */
static TaskState_t* _AllocStateNode(void) 
{
    if (s_nodeAllocIndex >= MAX_TOTAL_STATES) {
        // [Error] 최대 상태 개수 초과. 
        // 필요 시 Error Handler 호출하거나 로그 출력
        return NULL; 
    }
    return &s_stateNodes[s_nodeAllocIndex++];
}

/**
 * @brief ID로 연결 리스트에서 상태 노드를 찾습니다.
 */
static TaskState_t* _FindStateNode(TaskStateMachine_t* tsm, uint8_t id)
{
    TaskState_t* pNode = tsm->pStateHead;
    while (pNode != NULL) {
        if (pNode->id == id) return pNode;
        pNode = pNode->pNext;
    }
    return NULL;
}
