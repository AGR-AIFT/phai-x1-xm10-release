/**
 ******************************************************************************
 * @file canfd_rx_handler.h
 * @author HyundoKim
 * @brief FDCAN Rx Handler + Non-Realtime Processor (System Comm Layer)
 * @version 2.0 (SDO Processor 합병 + V1/V2 라우팅 완전 재설계)
 * @date 2026-02-10
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 * 
 * [역할 - V2.0]
 * - IOIF FDCAN RxTask에 콜백 등록 → 수신 메시지 라우팅
 * - V1/V2 CAN-ID 구조 기반 정확한 라우팅:
 *   - V1 (CM): PDO → 직접 라우팅, SDO → Message Queue
 *   - V2 (IMU Hub): TPDO/Heartbeat → 직접 라우팅, SDO/NMT → Message Queue
 * - Non-Realtime Task: Message Queue에서 SDO/NMT 비실시간 처리
 * 
 * [설계 원칙 - V2.0]
 * 1. 세마포어 Give/Take 동일 소스 원칙: RxTask는 IOIF 내부
 * 2. System Layer는 콜백 등록만 (IOIF_FDCAN_RegisterRxCallback)
 * 3. V1/V2 CAN-ID 구조 올바른 구분 (bits[7:4] vs bits[6:0])
 * 4. PDO/Heartbeat = 실시간 (직접), SDO/NMT = 비실시간 (Queue)
 * 5. Lock-Free: Device Layer API는 내부에서 Mutex 보호
 * 
 * [Task Priority]
 * - IOIF FDCAN RxTask: osPriorityRealtime4 (52) - IOIF 내부
 * - Main Control Task: osPriorityRealtime6 (54) - KING
 * - NonRealtime_Task: osPriorityRealtime3 (51)
 * 
 ******************************************************************************
 */

#pragma once

#ifndef CANFD_RX_HANDLER_H_
#define CANFD_RX_HANDLER_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* [V2.0] FDCAN_RX_BATCH_LIMIT 제거됨
 * - IOIF 내부 RxTask가 HW FIFO를 빌 때까지 모두 읽음
 * - System Layer에서 Batch 제한 불필요
 */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief FDCAN Rx Handler 초기화 결과
 */
typedef enum {
    FDCAN_RX_HANDLER_OK = 0,
    FDCAN_RX_HANDLER_ERROR_TASK,     /**< Non-Realtime Task 생성 실패 */
    FDCAN_RX_HANDLER_ERROR_QUEUE,    /**< Non-Realtime Queue 생성 실패 */
} FDCANRxHandler_Status_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief FDCAN Rx Handler 초기화 (V2.0 - 콜백 등록 패턴)
 * @details 
 * - [V2.0] IOIF 내부 RxTask에 _ClassifyAndRoute 콜백 등록
 * - Non-Realtime Queue + Task 생성 (SDO/NMT 처리)
 * - 세마포어 획득 + FDCANRxHandler_Task 생성 제거됨 (IOIF가 관리)
 * 
 * @return FDCANRxHandler_Status_t 초기화 결과
 * @note System Layer (system_startup.c)에서 호출
 */
FDCANRxHandler_Status_t FDCANRxHandler_Init(void);

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/**
 * @brief [V5.0] TIM7 SW IRQ에서 호출 — NonRealtimeTask 깨우기
 * @details
 * FDCAN ISR(NVIC 4)에서 FreeRTOS API 호출 불가하므로,
 * NVIC_SetPendingIRQ(TIM7_IRQn)으로 NVIC 6 SW IRQ를 트리거한다.
 * TIM7_IRQHandler에서 이 함수를 호출하여 xSemaphoreGiveFromISR 수행.
 *
 * @note stm32h7xx_it.c의 TIM7_IRQHandler(USER CODE)에서 호출
 */
void FDCANRxHandler_SwIrqNotify(void);
#endif

#endif /* CANFD_RX_HANDLER_H_ */
