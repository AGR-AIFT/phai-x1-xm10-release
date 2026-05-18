/**
 ******************************************************************************
 * @file    xm_api_freertos.h
 * @author  HyundoKim
 * @brief   XM10 학생용 RTOS Task API (FreeRTOS 학생 친화 wrapper)
 * @details
 * Control_Loop(1kHz)를 방해하지 않고 추가 task 를 안전하게 만들 수 있는 API.
 *
 * 핵심 함수:
 *   - XM_Task_CreateOneShot()  : 한 번 실행 후 자동 종료 (예: NN 학습)
 *   - XM_Task_CreatePeriodic() : 주기적 실행 (예: 100Hz 로깅)
 *   - XM_Mutex_*               : 공유 변수 보호 (멀티 워드 구조체)
 *   - XM_RTOS_PrintTaskList()  : 현재 동작 중 task 목록 출력 (디버깅, Phase 2)
 *
 * 권장 사용 패턴:
 *   - 단일 워드 (int / float / bool) 공유 → volatile 으로 충분
 *   - 멀티 워드 구조체 공유 → XM_Mutex_* 필수
 *   - Control_Loop 안에서 mutex 는 항상 timeout=0 (절대 블로킹 금지)
 *
 * 시스템 task 우선순위 (참고):
 *   55  Realtime7  StartupTask / IOIF_UartRx
 *   54  Realtime6  Control_Loop                  ← 1kHz 제어 루프
 *   51  Realtime3  NRT_Proc (SDO)
 *   48  Realtime   ← XM_PRIO_NEAR_REALTIME (PnP 경합 주의)
 *   40  High       ← XM_PRIO_ABOVE_CONTROL
 *   32  AboveNormal← XM_PRIO_BELOW_CONTROL
 *   24  Normal     ← XM_PRIO_BACKGROUND (권장 기본값)
 *    8  Low        ← XM_PRIO_IDLE
 *
 * @version 2.0
 * @date    2026-05-13
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_FREERTOS_H_
#define XM_API_FREERTOS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *-----------------------------------------------------------
 * 시스템 가드 상수 (학생 변경 금지, 투명성 위해 헤더 노출)
 *-----------------------------------------------------------
 */

/** @brief 학생이 동시에 만들 수 있는 보조 task 한도 (heap 안전 분배) */
#define XM_TASK_MAX_INSTANCES           4U

/** @brief 학생 task 가 사용할 수 있는 heap 총량 (32 KB) */
#define XM_TASK_HEAP_BUDGET_BYTES       32768U

/** @brief 단일 task 최대 stack (words, 4 byte 단위) */
#define XM_TASK_STACK_MAX_WORDS         2048U

/** @brief 단일 task 최소 stack */
#define XM_TASK_STACK_MIN_WORDS         128U

/* prio hint 별 기본 stack (words) */
#define XM_TASK_STACK_IDLE_WORDS        256U   /* 1 KB */
#define XM_TASK_STACK_BACKGROUND_WORDS  2048U  /* 8 KB (Ex.36 NN_Train 실측치) */
#define XM_TASK_STACK_BELOW_WORDS       1024U  /* 4 KB */
#define XM_TASK_STACK_ABOVE_WORDS       512U   /* 2 KB */
#define XM_TASK_STACK_NRT_WORDS         512U   /* 2 KB */

/**
 *-----------------------------------------------------------
 * PUBLIC TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Task 우선순위 hint
 * @details CMSIS-OS2 숫자 값을 직접 노출하지 않고 의미 단위로 추상화.
 *          시스템 task 가 점유한 영역 (54/55) 은 enum 에서 제외.
 */
typedef enum {
    XM_PRIO_IDLE          = 8,   /**< osPriorityLow      — DefaultTask 와 동급 */
    XM_PRIO_BACKGROUND    = 24,  /**< osPriorityNormal   — 권장 기본값 */
    XM_PRIO_BELOW_CONTROL = 32,  /**< osPriorityAboveNormal — Control_Loop 보다 낮음 */
    XM_PRIO_ABOVE_CONTROL = 40,  /**< osPriorityHigh     — USBH 와 동급, Control_Loop 보다 낮음 */
    XM_PRIO_NEAR_REALTIME = 48,  /**< osPriorityRealtime — 주의: PnP 통신과 경합 가능 */
} XmTaskPrio_t;

/** @brief 통합 task 핸들 (NULL = 유효하지 않음) */
typedef void* XmTaskHandle_t;

/** @brief Mutex 핸들 */
typedef void* XmMutexHandle_t;

/** @brief OneShot task 함수 시그니처 */
typedef void (*XmTaskOneShotFunc_t)(void* arg);

/** @brief Periodic task 함수 시그니처 (인자 없음 — 공유 변수 패턴 유도) */
typedef void (*XmTaskPeriodicFunc_t)(void);

/**
 *-----------------------------------------------------------
 * OneShot Task API
 *-----------------------------------------------------------
 */

/**
 * @brief 한 번 실행 후 자동 종료되는 task 생성 (예: NN 학습, FFT 등 무거운 계산)
 * @param[in] name       Task 이름 (10자 이내 권장, NULL/빈 문자열/중복 거부)
 * @param[in] func       Task 함수 (return 시 자동 완료)
 * @param[in] arg        함수 인자 (static/heap 만 — stack 변수 포인터 금지)
 * @param[in] prio_hint  우선순위 hint (XM_PRIO_BACKGROUND 권장)
 * @return 유효한 핸들 / 거부 시 NULL + [XM-WARN] CDC 메시지
 * @note  완료 후 반드시 XM_Task_Delete 호출하여 heap 회수 (예제 39 참고).
 */
XmTaskHandle_t XM_Task_CreateOneShot(const char*         name,
                                      XmTaskOneShotFunc_t func,
                                      void*               arg,
                                      XmTaskPrio_t        prio_hint);

/**
 * @brief OneShot task 가 완료되었는지 확인
 * @return true: 함수가 return 하여 완료됨, false: 실행 중 또는 잘못된 핸들
 */
bool XM_Task_IsComplete(XmTaskHandle_t handle);

/**
 *-----------------------------------------------------------
 * Periodic Task API
 *-----------------------------------------------------------
 */

/**
 * @brief 주기적으로 실행되는 task 생성 (예: 100Hz 로깅, 10Hz UI 업데이트)
 * @param[in] name       Task 이름
 * @param[in] func       매 주기 호출될 함수 (인자 없음 — 공유 변수 사용)
 * @param[in] period_ms  주기 (ms 단위, 최소 2ms 권장 — 1ms 는 Control_Loop 전용)
 * @param[in] prio_hint  우선순위 hint
 * @return 유효한 핸들 / 거부 시 NULL
 */
XmTaskHandle_t XM_Task_CreatePeriodic(const char*          name,
                                       XmTaskPeriodicFunc_t func,
                                       uint32_t             period_ms,
                                       XmTaskPrio_t         prio_hint);

/**
 *-----------------------------------------------------------
 * Task Lifecycle API
 *-----------------------------------------------------------
 */

/** @brief Task 일시 중단 (다음 Resume 호출 시까지 실행 안 됨) */
void XM_Task_Suspend(XmTaskHandle_t handle);

/** @brief Suspend 된 task 재개 */
void XM_Task_Resume(XmTaskHandle_t handle);

/**
 * @brief Task 종료 + 메모리 해제 (heap 누수 방지)
 * @details OneShot: IsComplete=true 확인 후 호출 권장.
 *          Periodic: 언제든 호출 가능 (강제 종료).
 *          자기 자신 (현재 실행 컨텍스트) Delete 시도는 거부됨.
 */
void XM_Task_Delete(XmTaskHandle_t handle);

/**
 *-----------------------------------------------------------
 * Mutex API (학생 친화 wrapper)
 *-----------------------------------------------------------
 */

/** @brief Mutex 생성. 실패 시 NULL */
XmMutexHandle_t XM_Mutex_Create(void);

/**
 * @brief Mutex 잠금 시도
 * @param[in] mutex      Mutex 핸들
 * @param[in] timeout_ms 대기 시간 (Control_Loop 안에서는 반드시 0 — 무블로킹)
 * @return true: 잠금 성공, false: timeout 또는 잘못된 핸들
 */
bool XM_Mutex_Lock(XmMutexHandle_t mutex, uint32_t timeout_ms);

/** @brief Mutex 잠금 해제 (lock 호출한 task 만 unlock 가능) */
void XM_Mutex_Unlock(XmMutexHandle_t mutex);

/** @brief Mutex 삭제 + 메모리 해제 */
void XM_Mutex_Delete(XmMutexHandle_t mutex);

/**
 *-----------------------------------------------------------
 * HW 제약 진단 API
 *-----------------------------------------------------------
 */

/** @brief 현재 시스템 heap 잔량 (bytes) */
uint32_t XM_Task_GetHeapFreeBytes(void);

/** @brief 시스템 heap 의 역대 최저 잔량 (누적 누수 감지) */
uint32_t XM_Task_GetHeapMinEverBytes(void);

/** @brief 현재 학생이 만든 task 개수 */
uint32_t XM_Task_GetInstanceCount(void);

/** @brief 학생 budget 잔량 (bytes) */
uint32_t XM_Task_GetBudgetRemainingBytes(void);

/**
 *-----------------------------------------------------------
 * 진단 API
 *-----------------------------------------------------------
 */

/**
 * @brief 현재 동작 중 task 목록 출력 (on-demand 진단)
 * @note  Phase 1: 진단 출력 채널 미연결 — no-op stub.
 *        Phase 2 (Risk Management 통합) 에서 PhAI Studio 진단 채널 연동 예정.
 */
void XM_RTOS_PrintTaskList(void);

/**
 *-----------------------------------------------------------
 * Deprecated API (v1.0 호환 — 다음 major 릴리즈에서 제거 예정)
 *-----------------------------------------------------------
 */

typedef void (*XmBgTaskFunc_t)(void* arg);
typedef void* XmBgTaskHandle_t;

/**
 * @brief [Deprecated] XM_Task_CreateOneShot 으로 마이그레이션 권장
 */
XmBgTaskHandle_t XM_BgTask_Create(const char* name, XmBgTaskFunc_t func,
                                   void* arg, uint32_t stack_words)
    __attribute__((deprecated("XM_Task_CreateOneShot(name, func, arg, XM_PRIO_BACKGROUND) 을 사용하세요. 완료 후 XM_Task_Delete 호출 필수.")));

/**
 * @brief [Deprecated] XM_Task_IsComplete 으로 마이그레이션 권장
 */
bool XM_BgTask_IsDone(XmBgTaskHandle_t handle)
    __attribute__((deprecated("XM_Task_IsComplete 를 사용하세요.")));

#ifdef __cplusplus
}
#endif

#endif /* XM_API_FREERTOS_H_ */
