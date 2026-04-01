/**
 ******************************************************************************
 * @file    xm_api_tsm.h
 * @author  HyundoKim
 * @brief   XM10 Task State Machine API
 * @details 사용자가 복잡한 로직을 상태 기반(State-Based)으로 쉽게 구현할 수 있도록 돕는 프레임워크입니다.
 * 내부적으로 Entry -> Loop -> Exit 의 생명주기를 자동으로 관리합니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_TSM_H_
#define XM_API_XM_API_TSM_H_

#include "task_mngr.h"

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief [표준] XM10 권장 상태 ID
 * @note  이 ID를 사용해도 되고, 10번(XM_STATE_USER_START)부터 자유롭게 정의해서 사용해도 됩니다.
 */
typedef enum {
    XM_STATE_OFF      = 0,   // 초기 상태, 동작 정지
    XM_STATE_STANDBY  = 1,   // 대기 상태 (센서 On, 출력 Off)
    XM_STATE_ACTIVE   = 2,   // 동작 중 (제어 알고리즘 수행)
    XM_STATE_ERROR    = 3,   // 에러 발생 (안전 모드)
    
    XM_STATE_USER_START = 10 // 사용자 정의 상태 시작 번호
} XmStateId_e;

/**
 * @brief TSM 생명주기 단계 (Lifecycle)
 */
typedef enum {
    XM_LIFECYCLE_ENTRY = 0, /**< 진입 (Initialization) 1회 실행 */
    XM_LIFECYCLE_LOOP  = 1, /**< 반복 (Execution) 실행 */
    XM_LIFECYCLE_EXIT  = 2  /**< 종료 (Clean-up) 1회 실행 */
} XmLifecycle_t;

/**
 * @brief TSM 태스크 객체
 * @details 이 구조체는 핸들 역할도 하면서, 동시에 모니터링 데이터를 담고 있습니다.
 * User는 이 구조체의 멤버를 읽어서(Read-Only) 현재 상태를 확인할 수 있습니다.
 */
typedef struct {
    /* --- Monitoring Area (Live Expression에서 보임) --- */
    XmStateId_e currentStateId;   /**< 현재 상태 ID */
    XmLifecycle_t currentStep;      /**< 현재 실행 단계 */

    XmStateId_e prevStateId;      /**< 이전 상태 ID */
    XmLifecycle_t prevStep;         /**< 이전 실행 단계 */
} XmTask_t;

/**
 * @brief TSM 핸들 재정의
 */
typedef XmTask_t* XmTsmHandle_t;

/**
 * @brief 상태별 동작을 정의하는 함수 포인터
 */
typedef void (*XmStateFunc_t)(void);

/**
 * @brief 상태(State) 설정 구조체
 * @note  구조체 지정 초기화(.field = value)를 사용하면 편리합니다.
 */
typedef struct {
    XmStateId_e id;         // 상태 ID (XmStateId_e)
    XmStateFunc_t on_entry; // [진입] 상태 진입 시 1회 실행 (초기화)
    XmStateFunc_t on_loop;  // [반복] 상태 유지 중 매 주기마다 실행 (제어 로직)
    XmStateFunc_t on_exit;  // [종료] 상태 탈출 시 1회 실행 (정리)
} XmStateConfig_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief  새로운 Task State Machine을 생성합니다.
 * @param  initial_state_id : 초기 시작 상태의 ID
 * @return 생성된 TSM 핸들 (실패 시 NULL)
 */
XmTsmHandle_t XM_TSM_Create(uint8_t initial_state_id);

/**
 * @brief  TSM에 새로운 상태를 등록합니다.
 * @param  handle : TSM 핸들
 * @param  config : 상태 설정 정보 구조체 포인터
 * * @code
 * // 예시: STANDBY 상태 등록
 * XmStateConfig_t sb_conf = {
 * .id = XM_STATE_STANDBY,
 * .on_entry = Standby_Entry, // 진입 함수
 * .on_loop  = Standby_Loop,  // 반복 함수
 * // .on_exit는 필요 없으면 생략 (자동 NULL)
 * };
 * XM_TSM_AddState(my_handle, &sb_conf);
 * @endcode
 */
void XM_TSM_AddState(XmTsmHandle_t handle, const XmStateConfig_t* config);

/**
 * @brief  TSM을 실행합니다.
 * @note   User Task의 메인 루프(while문) 안에서 주기적으로 호출해야 합니다.
 * @param  handle : TSM 핸들
 */
void XM_TSM_Run(XmTsmHandle_t handle);

/**
 * @brief  상태 전환을 요청합니다.
 * @note   즉시 전환되지 않고, 현재 상태의 Exit 함수가 실행된 후 다음 주기에 전환됩니다.
 * @param  handle : TSM 핸들
 * @param  next_state_id : 이동할 다음 상태 ID
 */
void XM_TSM_TransitionTo(XmTsmHandle_t handle, uint8_t next_state_id);

#endif /* XM_API_XM_API_TSM_H_ */
