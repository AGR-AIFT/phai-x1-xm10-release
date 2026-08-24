/**
 ******************************************************************************
 * @file    xm_api_freertos.c
 * @author  HyundoKim
 * @brief   XM10 백그라운드 태스크 API 구현
 * @version 1.0
 * @date    2026-04-03
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api_freertos.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "portable.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS
 *-----------------------------------------------------------
 */

#define BG_DEFAULT_STACK    1024
#define BG_PRIORITY         osPriorityNormal  /* 24 — UserTask(54)보다 훨씬 낮음 */

/**
 *-----------------------------------------------------------
 * PRIVATE TYPES
 *-----------------------------------------------------------
 */

typedef struct {
    XmBgTaskFunc_t  func;
    void*           arg;
    volatile bool   done;
    osThreadId_t    thread_id;
} BgTaskSlot_t;

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES
 *-----------------------------------------------------------
 */

/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void _BgTaskWrapper(void* argument);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

XmBgTaskHandle_t XM_BgTask_Create(const char* name, XmBgTaskFunc_t func,
                                   void* arg, uint32_t stack_words)
{
    if (func == NULL) return NULL;

    /* 슬롯 동적 할당 (태스크 완료 후에도 done 플래그 접근 필요) */
    BgTaskSlot_t* slot = (BgTaskSlot_t*)pvPortMalloc(sizeof(BgTaskSlot_t));
    if (slot == NULL) return NULL;

    slot->func = func;
    slot->arg  = arg;
    slot->done = false;
    slot->thread_id = NULL;

    uint32_t stack = (stack_words > 0) ? stack_words : BG_DEFAULT_STACK;

    osThreadAttr_t attr = {
        .name       = (name != NULL) ? name : "BgTask",
        .stack_size = stack * sizeof(uint32_t),  /* CMSIS-OS2는 bytes 단위 */
        .priority   = (osPriority_t)BG_PRIORITY,
    };

    slot->thread_id = osThreadNew(_BgTaskWrapper, slot, &attr);
    if (slot->thread_id == NULL) {
        vPortFree(slot);
        return NULL;
    }

    return (XmBgTaskHandle_t)slot;
}

bool XM_BgTask_IsDone(XmBgTaskHandle_t handle)
{
    if (handle == NULL) return false;
    BgTaskSlot_t* slot = (BgTaskSlot_t*)handle;
    return slot->done;
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS
 *-----------------------------------------------------------
 */

/** @brief RTOS 태스크 엔트리 — 사용자 함수 실행 후 done 플래그 세팅 + 태스크 종료 */
static void _BgTaskWrapper(void* argument)
{
    BgTaskSlot_t* slot = (BgTaskSlot_t*)argument;

    /* 사용자 함수 실행 (시간 제한 없음) */
    slot->func(slot->arg);

    /* 완료 플래그 세팅 */
    slot->done = true;

    /* RTOS 태스크 종료 (리소스는 idle task가 회수) */
    osThreadExit();
}
