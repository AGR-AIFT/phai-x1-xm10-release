/**
 ******************************************************************************
 * @file    agr_pnp_types.h
 * @author  HyundoKim
 * @brief   AGR PnP 공통 타입 정의 (Master/Slave 공유)
 * @version 1.0.0
 * @date    2026-02-10
 *
 * @details
 * agr_pnp_master.h와 agr_pnp_slave.h가 공유하는 타입을 정의합니다.
 * 중복 typedef 방지를 위해 별도 헤더로 분리합니다.
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_PNP_TYPES_H
#define AGR_PNP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief CAN 전송 함수 타입 (Dependency Injection)
 * @param can_id CAN ID
 * @param data   데이터 포인터
 * @param len    데이터 길이
 * @return 0: 성공, <0: 실패
 */
typedef int (*AGR_PnP_TxFunc_t)(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief Tick 함수 타입 (시간 소스)
 * @return 현재 시간 (ms)
 * @note HAL_GetTick() 또는 IOIF_TIM_GetTick() 사용
 */
typedef uint32_t (*AGR_PnP_GetTickFunc_t)(void);

#endif /* AGR_PNP_TYPES_H */
