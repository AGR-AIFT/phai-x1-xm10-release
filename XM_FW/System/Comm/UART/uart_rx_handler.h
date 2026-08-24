/**
 ******************************************************************************
 * @file    uart_rx_handler.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 5, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_UART_UART_RX_HANDLER_H_
#define SYSTEM_COMM_UART_UART_RX_HANDLER_H_

#include "ioif_agrb_uart.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief UART Rx 진단 구조체 (Live Expression 모니터링용)
 * @details cursorrule 13-comm-core-patterns 참조
 *
 * 모니터링 기준:
 *   - queue_full_count > 0   : Task 우선순위 낮거나 시스템 과부하
 *   - max_batch_seen > 1     : 정상적 지연 (1~2는 OK, 3+ 주의)
 *   - batch_overflow_count > 0 : 치명적 타이밍 문제
 */
typedef struct {
    volatile uint32_t queue_full_count;      /**< ISR: Queue Full 횟수 (0이어야 정상) */
    volatile uint32_t max_batch_seen;        /**< Task: while 루프 최대 batch 크기 */
    volatile uint32_t batch_overflow_count;  /**< Task: batch_max 초과 횟수 (0이어야 정상) */
    volatile uint32_t total_packets;         /**< ISR: 총 수신 패킷 수 */
} UartRxDiag_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief UART 디바이스 드라이버를 초기화하고,
 * 데이터 분배 태스크(StartUartRxTask)를 생성합니다.
 * @param[in] grf_left_id  왼발 FSR (MDAF-25-6850) UART ID
 * @param[in] grf_right_id 오른발 FSR (MDAF-25-6850) UART ID
 */
void UartRxHandler_Init(IOIF_UARTx_t grf_left_id, IOIF_UARTx_t grf_right_id);

/**
 * @brief XSENS IMU 사용을 위한 UART4 디바이스 드라이버를 초기화
 * @param[in] imu_id       IMU (Xsens MTi-630) UART ID
 */
void Uart4Rx_XsensIMU_Init(IOIF_UARTx_t imu_id);

/**
 * @brief UART Rx 진단 정보를 반환합니다. (Live Expression 모니터링용)
 * @param[in] type 0=FSR, 1=IMU
 * @param[out] out_diag 진단 구조체 포인터
 */
void UartRxHandler_GetDiag(uint8_t type, UartRxDiag_t* out_diag);

#endif /* SYSTEM_COMM_UART_UART_RX_HANDLER_H_ */
