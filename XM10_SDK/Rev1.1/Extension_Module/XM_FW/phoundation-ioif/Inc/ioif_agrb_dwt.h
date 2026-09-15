/**
 ******************************************************************************
 * @file    ioif_agrb_dwt.h
 * @author  HyundoKim
 * @brief   [IOIF] DWT (Data Watchpoint and Trace) - 고정밀 시간 측정
 * @version 1.0
 * @date    Dec 3, 2025
 *
 * @details
 * ARM Cortex-M DWT를 사용한 CPU 사이클 단위 시간 측정
 * 
 * [특징]
 * - 나노초 단위 정확도 (STM32G474 @170MHz: 1 cycle = 5.88ns)
 * - 오버헤드 최소화 (측정 자체가 3~5 cycle 소요)
 * - ISR 측정에 최적
 * 
 * [사용 예시]
 * @code
 * IOIF_DWT_Init();  // 초기화 (1회)
 * 
 * uint32_t start = IOIF_DWT_GetCycles();
 * // ... 측정할 코드 ...
 * uint32_t cycles = IOIF_DWT_GetCycles() - start;
 * uint32_t time_ns = IOIF_DWT_CyclesToNs(cycles);
 * @endcode
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef IOIF_AGRB_DWT_H_
#define IOIF_AGRB_DWT_H_

#include <stdint.h>
#include <stdbool.h>
#include "ioif_agrb_defs.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief MCU CPU 최대 클럭 (Hz) */
// ioif_agrb_defh.h에 IOIF_CPU_FREQ_HZ 정의 되어 있음

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief DWT 초기화
 * @return AGRBStatus_OK: 성공, AGRBStatus_ERROR: 실패 (DWT 미지원)
 * @details 
 * - DWT CYCCNT 카운터 활성화
 * - 1회만 호출하면 됩니다 (system_startup에서 호출)
 */
AGRBStatusDef IOIF_DWT_Init(void);

/**
 * @brief 현재 CPU 사이클 수 읽기
 * @return 현재 사이클 카운터 (32-bit, 약 25초마다 오버플로)
 * @note 오버플로 처리는 호출자가 해야 합니다 (차분 계산 시 자동 처리됨)
 */
static inline uint32_t IOIF_DWT_GetCycles(void)
{
    return *((volatile uint32_t*)0xE0001004);  /* DWT->CYCCNT */
}

/**
 * @brief CPU 사이클을 마이크로초로 변환
 * @param cycles CPU 사이클 수
 * @return 마이크로초 (μs)
 */
static inline uint32_t IOIF_DWT_CyclesToUs(uint32_t cycles)
{
    return (cycles / (IOIF_CPU_FREQ_HZ / 1000000UL));
}

/**
 * @brief CPU 사이클을 나노초로 변환
 * @param cycles CPU 사이클 수
 * @return 나노초 (ns)
 */
static inline uint32_t IOIF_DWT_CyclesToNs(uint32_t cycles)
{
    return (cycles * 1000UL) / (IOIF_CPU_FREQ_HZ / 1000000UL);
}

/**
 * @brief 마이크로초를 CPU 사이클로 변환
 * @param us 마이크로초
 * @return CPU 사이클 수
 */
static inline uint32_t IOIF_DWT_UsToCycles(uint32_t us)
{
    return (us * (IOIF_CPU_FREQ_HZ / 1000000UL));
}

/**
 * @brief 현재 CPU 사용률 측정 (간단 버전)
 * @param active_cycles 활동 사이클 (ISR 또는 Task 실행 시간)
 * @param total_cycles 전체 사이클 (측정 구간)
 * @return CPU 사용률 (0~100%)
 */
static inline uint8_t IOIF_DWT_CalcCpuUsage(uint32_t active_cycles, uint32_t total_cycles)
{
    if (total_cycles == 0) {
        return 0;
    }
    return (uint8_t)((active_cycles * 100UL) / total_cycles);
}

#endif /* IOIF_AGRB_DWT_H_ */

