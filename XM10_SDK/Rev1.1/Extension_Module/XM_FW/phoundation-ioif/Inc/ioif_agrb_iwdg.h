/**
 ******************************************************************************
 * @file    ioif_agrb_iwdg.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] IWDG (Independent Watchdog) 추상화 — 직접 레지스터 구현
 * @details
 * HAL IWDG 드라이버(stm32h7xx_hal_iwdg.c)를 벤더링하지 않고 CMSIS 레지스터를
 * 직접 사용합니다 (CubeMX .ioc / HAL conf surface 비의존 — 재생성 무영향).
 * IWDG 는 LSI 독립 클럭 다운카운터로, 한번 시작하면 리셋 전까지 정지 불가.
 *
 * [정책 — XM10 D2 (2026-08-19)]
 * - 시스템 init 완료 후 늦게 시작 (init 실패 halt 는 IWDG 이전 = 진정한 halt)
 * - 런타임 fault 로 refresh 가 끊기면 타임아웃 후 IWDG 리셋 (RSR 로 원인 분류)
 * - Init 시 DBGMCU freeze 설정 — 디버거 halt 중 카운터 정지 (개발 오탐 방지)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef IOIF_AGRB_IWDG_H_
#define IOIF_AGRB_IWDG_H_

#include "ioif_agrb_defs.h"

#include <stdint.h>
#include <stdbool.h>

#if defined(AGRB_IOIF_IWDG_ENABLE)

#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"     /* IWDG1 / DBGMCU / FLASH CMSIS 정의 */
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx_hal.h"     /* IWDG (G4 단일 인스턴스) */
#endif

/**
 * @brief IWDG 시작 (직접 레지스터 — HAL_IWDG 미사용)
 * @param[in] timeout_ms 타임아웃 (ms). LSI 32kHz 공칭 기준 최대 약 32760ms.
 * @return AGRBStatus_OK / AGRBStatus_PARAM_ERROR(범위 초과) /
 *         AGRBStatus_TIMEOUT(SR PVU/RVU busy-wait 초과)
 * @warning 한번 시작하면 정지 불가 — 이후 IOIF_IWDG_Refresh() 를 타임아웃보다
 *          짧은 주기로 호출하지 않으면 시스템 리셋됩니다. 시스템 init 이
 *          모두 끝난 지점에서 1회만 호출하세요.
 * @note    LSI 는 uncalibrated RC(공칭 32kHz, 편차 수십%) — 타임아웃은 근사값.
 *          내부에서 DBGMCU 디버그 freeze 를 함께 설정합니다.
 */
AGRBStatusDef IOIF_IWDG_Init(uint32_t timeout_ms);

/**
 * @brief 카운터 리로드 (KR=0xAAAA). 단일 레지스터 write — 어느 컨텍스트든 안전.
 */
void IOIF_IWDG_Refresh(void);

#endif /* AGRB_IOIF_IWDG_ENABLE */

#endif /* IOIF_AGRB_IWDG_H_ */
