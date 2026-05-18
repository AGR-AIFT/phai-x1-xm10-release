/**
 ******************************************************************************
 * @file    xm_task_manager.h
 * @author  HyundoKim
 * @brief   XM10 사용자 Task 매니저 (System 레이어 — XM_API facade 가 호출)
 * @details
 * 아키텍처 위치: System Layer (XM_FW/System/Task/)
 *   - XM_API/xm_api_freertos.c (facade) 가 본 API 호출
 *   - Slot 라이프사이클 + vPortFree 경로 + API contract 가드 (A/B/E/O/P/U/K)
 *
 * API contract 가드 (본 모듈 내장 — 단순 NULL/false 반환):
 *   - 가드 A: 컴파일 타임 _Static_assert (xm_task_manager.c)
 *   - 가드 B: instance / budget cap (4개 한도 / 32KB heap budget)
 *   - 가드 E: stack 파라미터 봉인 (prio_hint 기반 default stack)
 *   - 가드 O: Handle magic (Use-After-Delete)
 *   - 가드 P: Task 이름 검증 (NULL/빈)
 *   - 가드 U: Self-Delete 거부
 *   - 가드 K: Periodic period_ms 검증 (0/너무 짧음)
 *
 * Risk Management 영역 가드 (F/G/H/I/J/L/N/Q/R)는 본 모듈 범위 밖.
 * XM10 전체 차원에서 별도 계획 후 도입 예정 (Phase 2).
 *
 * Dependency 룰 준수 ([00-core-architecture.md:114]):
 *   - From: Application (XM_Apps), XM_API
 *   - To  : Core (cmsis_os2, FreeRTOS)
 *   - 금지: HAL 직접 접근
 *
 * @version 1.1
 * @date    2026-05-15
 ******************************************************************************
 */

#pragma once

#ifndef XM_TASK_MANAGER_H_
#define XM_TASK_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *-----------------------------------------------------------
 * 공개 상수 — XM_API 헤더와 일치 (Single Source of Truth)
 *-----------------------------------------------------------
 */

#define XM_TASK_MAX_INSTANCES           4U
#define XM_TASK_HEAP_BUDGET_BYTES       32768U
#define XM_TASK_STACK_MAX_WORDS         2048U
#define XM_TASK_STACK_MIN_WORDS         128U

#define XM_TASK_STACK_IDLE_WORDS        256U
#define XM_TASK_STACK_BACKGROUND_WORDS  2048U
#define XM_TASK_STACK_BELOW_WORDS       1024U
#define XM_TASK_STACK_ABOVE_WORDS       512U
#define XM_TASK_STACK_NRT_WORDS         512U

/**
 *-----------------------------------------------------------
 * 내부 타입 (XM_API 가 forward 시 같은 타입 재사용)
 *-----------------------------------------------------------
 */

typedef enum {
    XM_TASK_PRIO_IDLE          = 8,
    XM_TASK_PRIO_BACKGROUND    = 24,
    XM_TASK_PRIO_BELOW_CONTROL = 32,
    XM_TASK_PRIO_ABOVE_CONTROL = 40,
    XM_TASK_PRIO_NEAR_REALTIME = 48,
} XmTaskMgr_Prio_t;

typedef void* XmTaskMgr_Handle_t;
typedef void (*XmTaskMgr_OneShotFn_t)(void* arg);
typedef void (*XmTaskMgr_PeriodicFn_t)(void);

/**
 *-----------------------------------------------------------
 * OneShot / Periodic / Lifecycle
 *-----------------------------------------------------------
 */

XmTaskMgr_Handle_t XmTaskMgr_CreateOneShot(const char*           name,
                                            XmTaskMgr_OneShotFn_t func,
                                            void*                 arg,
                                            XmTaskMgr_Prio_t      prio_hint);

XmTaskMgr_Handle_t XmTaskMgr_CreatePeriodic(const char*            name,
                                             XmTaskMgr_PeriodicFn_t func,
                                             uint32_t               period_ms,
                                             XmTaskMgr_Prio_t       prio_hint);

bool XmTaskMgr_IsComplete(XmTaskMgr_Handle_t handle);
void XmTaskMgr_Suspend   (XmTaskMgr_Handle_t handle);
void XmTaskMgr_Resume    (XmTaskMgr_Handle_t handle);
void XmTaskMgr_Delete    (XmTaskMgr_Handle_t handle);

/**
 *-----------------------------------------------------------
 * Mutex
 *-----------------------------------------------------------
 */

typedef void* XmTaskMgr_MutexHandle_t;

XmTaskMgr_MutexHandle_t XmTaskMgr_MutexCreate (void);
bool                    XmTaskMgr_MutexLock   (XmTaskMgr_MutexHandle_t m, uint32_t timeout_ms);
void                    XmTaskMgr_MutexUnlock (XmTaskMgr_MutexHandle_t m);
void                    XmTaskMgr_MutexDelete (XmTaskMgr_MutexHandle_t m);

/**
 *-----------------------------------------------------------
 * Budget / 진단 API (단순 카운터 조회)
 *-----------------------------------------------------------
 */

uint32_t XmTaskMgr_GetHeapFreeBytes      (void);
uint32_t XmTaskMgr_GetHeapMinEverBytes   (void);
uint32_t XmTaskMgr_GetInstanceCount      (void);
uint32_t XmTaskMgr_GetBudgetRemainingBytes(void);

/**
 *-----------------------------------------------------------
 * Task topology 출력 (on-demand, 단순화 버전)
 *-----------------------------------------------------------
 * @note  Control_Loop (1 kHz) 안에서 호출 금지 — snprintf 누적 100us+ 가능.
 *        Control_Setup 또는 버튼 핸들러 등에서 호출.
 */
void XmTaskMgr_PrintTaskList(void);

#ifdef __cplusplus
}
#endif

#endif /* XM_TASK_MANAGER_H_ */
