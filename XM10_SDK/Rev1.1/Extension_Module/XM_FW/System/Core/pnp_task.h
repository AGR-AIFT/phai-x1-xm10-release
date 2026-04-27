/**
 ******************************************************************************
 * @file    pnp_task.h
 * @author  HyundoKim
 * @brief   PnP Task - DOP V2 Master PnP + Device 통합 관리
 * @version 2.0.0
 * @date    2026-02-11
 *
 * @details
 * [역할]
 * 1. Master PnP 인스턴스 보유 (XM10 노드 단위, Heartbeat 1회 전송)
 * 2. PnP Task 운영 (100ms 주기)
 *    - AGR_PnP_Master_RunPeriodic(): Heartbeat 전송 + Slave Timeout 체크
 *    - Protocol Device RunPeriodic(): CM (DOP V1), ImuHub (DOP V2)
 *    - Sensor Device RunPeriodic(): GRF, XSENS (Auto-Sense 타임아웃)
 *
 * [V2.0 변경사항 - Link Layer 완전 제거]
 * - AS-IS: V1 Legacy Links (LinkModule_t 인터페이스, GRF/XSENS Link 등록)
 * - TO-BE: 모든 Device를 직접 호출 (CM/ImuHub/GRF/XSENS 동일 패턴)
 * - LinkModule_t, link_interface.h 완전 폐기
 * - System/Links 폴더 삭제
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_CORE_PNP_TASK_H_
#define SYSTEM_CORE_PNP_TASK_H_

#include "agr_pnp_master.h"
#include "agr_pnp_types.h"

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief PnP Task 초기화 (System Startup에서 1회 호출)
 *
 * @details
 * [처리 항목]
 * 1. AGR_PnP_Master_Init() - Master PnP 인스턴스 초기화
 * 2. Protocol Device Init (CM)
 * 3. Sensor Device StateInit (GRF, XSENS Auto-Sense 초기화)
 * 4. PnP Task 생성 (100ms 주기)
 *
 * @param pnp_tx_func  PnP Master Heartbeat 전송 함수 (Ch2, System_Fdcan2_Transmit)
 * @param cm_tx_func   CM(DOP V1) 전송 함수 (Ch1, System_Fdcan1_Transmit)
 * @param get_tick     Tick 함수 (HAL_GetTick 또는 IOIF_TIM_GetTick)
 *
 * @note V2 Device Driver Init()보다 먼저 호출해야 합니다.
 */
void PnP_Task_Init(AGR_PnP_TxFunc_t pnp_tx_func,
                    AGR_PnP_TxFunc_t cm_tx_func,
                    AGR_PnP_GetTickFunc_t get_tick);

/**
 * @brief Master PnP 인스턴스 반환 (Device Driver 등록용)
 *
 * @return AGR_PnP_Master_t* Master PnP 포인터 (초기화 전이면 NULL)
 */
AGR_PnP_Master_t* PnP_Task_GetMaster(void);

#endif /* SYSTEM_CORE_PNP_TASK_H_ */
