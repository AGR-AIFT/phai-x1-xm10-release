/**
 * @file task_mngr.h
 * @brief TSM 엔진을 감싸는 Wrapper (용어 통일 및 정적 할당 반영)
 */

#ifndef TASK_STATE_MACHINE_INC_TASK_MNGR_H_
#define TASK_STATE_MACHINE_INC_TASK_MNGR_H_

#include "task_state_machine.h"

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

/**
 * @brief TSM 공개 객체 (핸들 겸 모니터링 데이터)
 * @details 이 모듈을 사용하는 상위 계층(API)은 이 구조체를 핸들로 사용합니다.
 */
typedef struct {
    /* Public Monitoring Area */
    uint8_t current_state_id;
    TsmLifecycle_e current_step;

    uint8_t prev_state_id;
    TsmLifecycle_e prev_step;
    
    /* Private Hidden Area (상위 계층은 접근 금지) */
    // C언어의 상속 흉내내기: 내부 구현은 .c 파일의 '확장 구조체'에 숨김
} TsmObject_t;

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
 * @brief 정적 풀에서 태스크 핸들을 할당받아 초기화합니다.
 */
TsmObject_t* TaskMngr_Create(uint8_t initial_state_id);

/**
 * @brief 태스크에 상태를 추가합니다. (용어 변경: Entry/Loop/Exit)
 */
void TaskMngr_AddState(TsmObject_t* obj, uint8_t state_id,
                  EntryFunc_t entry, LoopFunc_t loop, ExitFunc_t exit);

/**
 * @brief 태스크 실행 (Loop에서 호출)
 */
void TaskMngr_Run(TsmObject_t* obj);

/**
 * @brief 상태 전환 요청
 */
void TaskMngr_Transition(TsmObject_t* obj, uint8_t next_state_id);


#endif /* TASK_STATE_MACHINE_INC_TASK_MNGR_H_ */
