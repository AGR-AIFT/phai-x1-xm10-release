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

#endif /* SYSTEM_COMM_UART_UART_RX_HANDLER_H_ */
