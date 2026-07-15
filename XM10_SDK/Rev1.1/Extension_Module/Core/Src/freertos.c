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

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName);

/* USER CODE BEGIN 4 */

/* [DIAG 2026-05-12] Stack overflow 검출 진단.
 *  - 이전에는 빈 hook → silent corruption → 이후 freeze/hardfault 의 root cause.
 *  - 활성화 후: stack overflow 발생 즉시 infinite loop 에 진입.
 *    디버거 Suspend 하면 g_overflow_task_name 변수로 어느 task 인지 확인 가능.
 *  - 다음 단계: 식별된 task 의 stack size 를 늘리거나 stack 사용 줄이기. */
volatile char     g_overflow_task_name[16] = {0};
volatile uint32_t g_overflow_detected      = 0;

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
     *  안정성을 깨뜨릴 만큼 corrupted 영역을 건드릴 가능성 낮음). */
    for (;;) {
        __NOP();
    }
}

/* USER CODE END 4 */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

