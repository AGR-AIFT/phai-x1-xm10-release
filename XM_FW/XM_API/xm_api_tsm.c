/**
 ******************************************************************************
 * @file    xm_api_tsm.c
 * @author  HyundoKim
 * @brief   Task State Machine API Wrapper
 * @details 
 * 사용자가 정의한 XmStateConfig_t 구조체를 내부의 Task Manager 엔진에 전달합니다.
 * 복잡한 내부 포인터 연산을 숨기고, 직관적인 설정 방식을 제공하는 어댑터입니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api_tsm.h"
#include "task_mngr.h"
#include <stddef.h> // NULL

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

XmTsmHandle_t XM_TSM_Create(uint8_t initial_state_id)
{
    // 내부 Task Manager를 통해 태스크 객체 생성 (정적 할당됨)
    return (XmTsmHandle_t)TaskMngr_Create(initial_state_id);
}

void XM_TSM_AddState(XmTsmHandle_t handle, const XmStateConfig_t* config)
{
    if (handle == NULL || config == NULL) {
        return;
    }

    // 구조체의 내용을 풀어서 내부 함수로 전달
    // 사용자가 NULL로 남겨둔 필드는 자동으로 NULL 처리됨
    TaskMngr_AddState(
        (TsmObject_t*)handle,
        (uint8_t)config->id,
        config->on_entry, // Active -> Entry
        config->on_loop,  // Run -> Loop
        config->on_exit   // Inactive -> Exit
    );
}

void XM_TSM_Run(XmTsmHandle_t handle)
{
    if (handle == NULL) return;
    
    // 내부 Task Manager 실행
    TaskMngr_Run((TsmObject_t*)handle);
}

void XM_TSM_TransitionTo(XmTsmHandle_t handle, uint8_t next_state_id)
{
    if (handle == NULL) return;

    // 내부 Task Manager에 전환 요청
    TaskMngr_Transition((TsmObject_t*)handle, next_state_id);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */
