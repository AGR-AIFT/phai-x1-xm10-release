/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* Phase 2: FreeRTOS Heap — DTCMRAM (Zero-Wait-State, DMA 접근 불가 → 커널 객체 보호) */
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".dtcm_data"), aligned(8)));
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
/* [DIAG 2026-05-12 cross-port from 0428] Stack overflow 검출 진단.
 *  - 이전에는 빈 hook → silent corruption → 이후 freeze/hardfault 의 root cause.
 *  - 활성화 후: stack overflow 발생 즉시 infinite loop 에 진입.
 *    디버거 Suspend 하면 g_overflow_task_name 변수로 어느 task 인지 확인 가능.
 *  - 다음 단계: 식별된 task 의 stack size 를 늘리거나 stack 사용 줄이기. */
/* .noinit 배치 — POR 외 reset (NVIC_SystemReset, lockup auto-reset) 에서 보존.
 * 다음 boot 시 main() 가 g_overflow_detected 검사로 이전 boot 의 stack overflow
 * 흔적 확인 가능. POR 직후엔 random 값일 수 있으므로 detected==1 일 때만 신뢰. */
__attribute__((section(".noinit"), used))
volatile char     g_overflow_task_name[16];
__attribute__((section(".noinit"), used))
volatile uint32_t g_overflow_detected;

/* 직전 boot 시점의 RCC->RSR snapshot — main() 진입 직후 저장 + RMVF clear.
 * STM32H7 RSR 은 software 가 clear 안 하면 모든 boot 의 flag 누적. snapshot
 * + clear 로 정확히 "직전 reset 원인" 만 표시. POR 후엔 random, 이후엔 0xXX
 * 형식의 RSR 값. CubeProgrammer 로 read 가능 (주소 map 에서 확인). */
__attribute__((section(".noinit"), used))
volatile uint32_t g_last_rcc_rsr;
/* [2026-07-08 이더넷 무응답 진단] malloc 실패 카운터 + 원인 task 이름.
 * regular .bss 배치(zero-init) — .noinit 는 POR 직후 garbage 라 "실패 0회"를
 * 표현 못 한다(디버거서 g_malloc_failed_count=2.1G garbage 관측). malloc-fail 훅은
 * 실패 task 를 park 만 하고 reset 하지 않으므로 reset-survival 불필요 → .bss 가 정확.
 * count 는 OD(0x7E80)로 GUI 가 디버거 없이 판독(0=malloc 정상, ≥1=힙 고갈).
 * 095e730(힙 100→60KB 축소 + malloc-fail 훅 park) 회귀 진단 인프라. */
__attribute__((used))
volatile uint32_t g_malloc_failed_count;
__attribute__((used))
volatile char     g_malloc_failed_task_name[16];

void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
{
    (void)xTask;

    if (pcTaskName != NULL) {
        for (int i = 0; i < 15 && pcTaskName[i] != '\0'; i++) {
            g_overflow_task_name[i] = pcTaskName[i];
        }
        g_overflow_task_name[15] = '\0';
    } else {
        g_overflow_task_name[0] = '?';
        g_overflow_task_name[1] = '\0';
    }
    g_overflow_detected = 1;

    /* Infinite loop — 디버거 Suspend 시 여기서 잡힘. PC + g_overflow_task_name 확인.
     * [Note] 전역 인터럽트는 차단하지 않는다 (hook 정책 + 다른 ISR가 디버거 attach
     *  안정성을 깨뜨릴 만큼 corrupted 영역을 건드릴 가능성 낮음).
     * [Phase 2 RM] LED/CDC 친화 메시지는 XM10 Risk Management 통합 단계에서 도입. */
    for (;;) {
        __NOP();
    }
}

/* [malloc-hook] 진단 구현은 CubeMX 가 생성하는 hook(USER CODE BEGIN 5) 안으로 이관.
 * USE_MALLOC_FAILED_HOOK 를 .ioc 로 켜면 CubeMX 가 vApplicationMallocFailedHook 를
 * 자체 생성하므로, 여기(USER CODE 4)에 수기 정의를 두면 재정의(redefinition) 충돌.
 * → 정의는 USER CODE 5 로 단일화, config 는 .ioc 가 관리(regen 생존). */

/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
    /* configUSE_MALLOC_FAILED_HOOK=1 시 pvPortMalloc() 실패마다 호출.
     * 어느 task 에서 힙 할당이 실패했는지 캡처(스택오버플로 훅과 동일 진단 패턴).
     * scheduler 는 잠기지 않으므로(heap_4 가 hook 전에 xTaskResumeAll) 다른 task
     * (SDO/OD 응답)는 계속 동작 → g_malloc_failed_count 를 OD(0x7E80)로 읽어
     * 디버거 없이 원인 확인 가능. pcTaskGetName 은 TCB 만 읽어 재할당 없음(훅 내 안전). */
    g_malloc_failed_count++;

    const char *name = pcTaskGetName(NULL);
    if (name != NULL) {
        int i = 0;
        for (; i < 15 && name[i] != '\0'; i++) {
            g_malloc_failed_task_name[i] = name[i];
        }
        g_malloc_failed_task_name[i] = '\0';
    } else {
        g_malloc_failed_task_name[0] = '?';
        g_malloc_failed_task_name[1] = '\0';
    }

    for (;;) {
        __NOP();
    }
}
/* USER CODE END 5 */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

