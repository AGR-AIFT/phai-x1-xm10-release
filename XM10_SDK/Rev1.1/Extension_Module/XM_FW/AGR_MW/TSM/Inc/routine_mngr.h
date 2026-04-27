/**
 * @file routine_mngr.h
 * @brief 동적 기능(Routine) 관리자
 * @details TSM 상태(Loop) 내에서 조건부로 실행될 기능들을 관리합니다.
 */

#ifndef TASK_STATE_MACHINE_INC_ROUTINE_MNGR_H_
#define TASK_STATE_MACHINE_INC_ROUTINE_MNGR_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/**
 * @brief 루틴 관리자 객체
 */
#define MAX_ROUTINES_PER_MNGR   10 // 한 관리자가 가질 수 있는 최대 기능 수

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

// 기능별 함수 포인터 정의
typedef void (*RoutineFunc_t)(void);

/**
 * @brief 개별 기능(Routine) 정의 구조체
 */
typedef struct {
    uint32_t id;             // 기능 ID (예: ROUTINE_ID_POS_CTRL)
    bool is_enabled;         // 활성화 여부 Flag
    
    RoutineFunc_t on_init;   // 켜질 때 1회 실행 (자원 할당, 변수 초기화)
    RoutineFunc_t on_run;    // 켜져 있는 동안 반복 실행
    RoutineFunc_t on_exit;   // 꺼질 때 1회 실행 (안전 종료, 출력 0 등)
} Routine_t;

typedef struct {
    Routine_t routines[MAX_ROUTINES_PER_MNGR];
    uint32_t count;
} RoutineMngr_t;

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
 * @brief 루틴 관리자를 초기화합니다.
 */
void RoutineMngr_Init(RoutineMngr_t* mngr);

/**
 * @brief 관리자에 새로운 기능(Routine)을 등록합니다.
 * @note 초기 상태는 무조건 Disable입니다.
 */
bool RoutineMngr_Add(RoutineMngr_t* mngr, uint32_t id, 
                     RoutineFunc_t init, RoutineFunc_t run, RoutineFunc_t exit);

/**
 * @brief 특정 기능의 활성화/비활성화를 설정합니다. (Master 명령 수신 시 호출)
 * @details Disable -> Enable 전환 시 on_init() 호출
 * Enable -> Disable 전환 시 on_exit() 호출
 */
void RoutineMngr_SetEnable(RoutineMngr_t* mngr, uint32_t id, bool enable);

/**
 * @brief 활성화된 모든 기능의 Run 함수를 실행합니다.
 * @note TSM의 Loop 함수 내에서 호출됩니다.
 */
void RoutineMngr_RunAll(RoutineMngr_t* mngr);


#endif /* TASK_STATE_MACHINE_INC_ROUTINE_MNGR_H_ */
