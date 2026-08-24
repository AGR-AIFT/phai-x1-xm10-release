/**
 ******************************************************************************
 * @file    ioif_agrb_dwt.c
 * @author  HyundoKim
 * @brief   [IOIF] DWT (Data Watchpoint and Trace) - 고정밀 시간 측정
 * @version 1.0
 * @date    Dec 3, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_dwt.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* DWT (Data Watchpoint and Trace) Registers */
#define DWT_CTRL        (*(volatile uint32_t*)0xE0001000)  /* DWT Control Register */
#define DWT_CYCCNT      (*(volatile uint32_t*)0xE0001004)  /* DWT Cycle Count Register */

#define DEM_CR          (*(volatile uint32_t*)0xE000EDFC)  /* CoreDebug->DEMCR */
#define DEM_CR_TRCENA   (1UL << 24)                         /* Trace Enable */

#define DWT_CTRL_CYCCNTENA  (1UL << 0)                      /* Cycle Counter Enable */

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

AGRBStatusDef IOIF_DWT_Init(void)
{
    /* 1. CoreDebug Trace 활성화 */
    DEM_CR |= DEM_CR_TRCENA;
    
    /* 2. DWT Cycle Counter 리셋 */
    DWT_CYCCNT = 0;
    
    /* 3. DWT Cycle Counter 활성화 */
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
    
    /* 4. 정상 동작 확인 */
    if (!(DWT_CTRL & DWT_CTRL_CYCCNTENA)) {
        return AGRBStatus_ERROR;  /* DWT 미지원 */
    }
    
    return AGRBStatus_OK;
}

