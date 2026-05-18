/**
 ******************************************************************************
 * @file    task_lifecycle.c
 * @brief   Ex.39 — OneShot Task Lifecycle (Create → IsComplete → Delete)
 *
 * 학습 포인트
 *   1. XM_Task_CreateOneShot() — 1회 실행하고 끝나는 보조 task
 *   2. XM_Task_IsComplete()    — 함수 본체 종료 후 외부에서 polling
 *   3. XM_Task_Delete()        — 명시 해제 (heap 누수 방지)
 *
 * Lifecycle 순환
 *   ┌── 버튼 1 PRESSED ──┐
 *   │                     ▼
 *   │             Create OneShot ─▶ task 함수 실행 ─▶ 자동 osThreadExit
 *   │                     │                                 │
 *   │                     ▼                                 ▼
 *   │             (LED 1 ON)                       s_heavy.done = true
 *   │                                                       │
 *   └───────────── IsComplete ◀───────────────────────────┘
 *                       │
 *                       ▼
 *                Delete + handle = NULL ─▶ (LED 1 OFF)
 *
 * @warning Delete 호출 후 handle 변수를 NULL 로 초기화하세요 — dangling pointer 방지.
 * @warning IsComplete 검사 없이 Delete 하면 task 함수 본체가 실행 중일 수 있음.
 *
 * 회로 / 보드
 *   - Button 1 (XM_BTN_1) 사용 — PRESSED 이벤트로 heavy task 트리거.
 *   - LED 1 (XM_LED_1) — heavy task 동작 중 ON.
 ******************************************************************************
 */

#include "xm_api.h"
#include "cmsis_os2.h"   /* osDelay — task 함수 내부에서 사용 가능 */

static XmTaskHandle_t s_heavy;

/* 보조 task 본체 — FFT / 학습 등 무거운 계산을 가정한 placeholder.
 * osDelay 로 200 ms 소요 시뮬레이션 (Control_Loop 200 cycle 동안 계속 실행). */
static void _HeavyCalc(void* arg)
{
    (void)arg;
    XM_SetLedState(XM_LED_1, XM_ON);

    /* 실제 사용자 코드 영역 — FFT / 학습 / 큰 행렬 곱 등 */
    osDelay(200U);   /* 200 ms heavy 계산 시뮬레이션 */

    XM_SetLedState(XM_LED_1, XM_OFF);
    /* return 시 OneShot wrapper 가 done=true 설정 + osThreadExit. */
}

void Control_Setup(void)
{
    /* 부팅 1회 — 별다른 초기화 없음 */
}

void Control_Loop(void)
{
    /* 1. 버튼 PRESSED 이벤트 → OneShot 생성 (이미 동작 중이면 skip) */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_PRESSED && s_heavy == NULL) {
        s_heavy = XM_Task_CreateOneShot("HeavyCalc",
                                        _HeavyCalc, NULL,
                                        XM_PRIO_BELOW_CONTROL);
        /* s_heavy == NULL 이면 가드 B (4개 한도) 가 거부한 것. */
    }

    /* 2. 완료 polling → Delete (heap 회수) + handle nil */
    if (s_heavy != NULL && XM_Task_IsComplete(s_heavy)) {
        XM_Task_Delete(s_heavy);
        s_heavy = NULL;                 /* dangling pointer 방지 */
    }
}
