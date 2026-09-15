/**
 ******************************************************************************
 * @file    ioif_agrb_crc.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] CRC 하드웨어 유닛 추상화 (HAL 캡슐화)
 * @details
 *  CubeMX 생성 HAL CRC 핸들을 주입받아(IOIF DMA/핸들 주입 패턴) HAL_CRC_*
 *  직접 호출을 캡슐화한다. System/Device 레이어는 본 API 만 사용한다.
 *
 *  [사용 순서]
 *    1. IOIF_CRC_Init(&hcrc)        — Init 단계 1회 (HAL 핸들 주입)
 *    2. IOIF_CRC_Calculate(p, n)    — CRC 유닛 리셋 후 새로 누적
 *    3. IOIF_CRC_Accumulate(p, n)   — 이어서 누적
 *    4. IOIF_CRC_GetValue()         — 현재 누적값(DR) read
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef IOIF_AGRB_CRC_H
#define IOIF_AGRB_CRC_H

#include "ioif_agrb_defs.h"
#include <stdint.h>

#if defined(AGRB_IOIF_CRC_ENABLE)

/* MCU별 HAL CRC 핸들 타입 (CRC_HandleTypeDef) — IOIF 모듈 헤더 표준 패턴 */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_crc.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx_hal.h"
    #include "stm32g4xx_hal_crc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HAL CRC 핸들 주입 (Init 단계 1회).
 * @param hcrc CubeMX 생성 CRC_HandleTypeDef* (예: &hcrc). MX_CRC_Init() 이후 호출.
 * @return AGRBStatus_OK / AGRBStatus_PARAM_ERROR(hcrc==NULL)
 */
AGRBStatusDef IOIF_CRC_Init(CRC_HandleTypeDef* hcrc);

/**
 * @brief CRC 유닛을 리셋하고 입력 버퍼로 새로 누적 시작. HAL_CRC_Calculate 래퍼.
 * @param data 입력 버퍼 (InputDataFormat 에 따라 byte/word 해석)
 * @param len  버퍼 길이 (CubeMX InputDataFormat 단위)
 * @return 누적 CRC 값 (핸들 미주입/NULL 시 0)
 */
uint32_t IOIF_CRC_Calculate(uint32_t* data, uint32_t len);

/**
 * @brief 직전 상태에 이어서 CRC 누적. HAL_CRC_Accumulate 래퍼.
 * @param data 입력 버퍼
 * @param len  버퍼 길이
 * @return 누적 CRC 값 (핸들 미주입/NULL 시 0)
 */
uint32_t IOIF_CRC_Accumulate(uint32_t* data, uint32_t len);

/**
 * @brief 현재 누적된 CRC 값(DR 레지스터)을 직접 read.
 * @return CRC DR 값 (핸들 미주입 시 0)
 */
uint32_t IOIF_CRC_GetValue(void);

#ifdef __cplusplus
}
#endif

#endif /* AGRB_IOIF_CRC_ENABLE */

#endif /* IOIF_AGRB_CRC_H */
