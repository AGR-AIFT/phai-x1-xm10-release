/**
 ******************************************************************************
 * @file    uart_rx_handler.h
 * @author  HyundoKim
 * @brief   [System/Comm] UART 센서 RX 파이프라인 바인딩 (FSR + Xsens IMU)
 * @details
 *  - IOIF Idle Event 콜백 등록과 Device 파서 인스턴스 초기화를 담당.
 *  - 매핑(Rev1.1): UART7=GRF L, UART8=GRF R, UART4(PA0/PA1 동적 전환)=Xsens MTi-630.
 *  - Rev1.1 Xsens는 ExternalIO_SwitchToUartMode()를 통해 UART4를 런타임에 초기화한 후
 *    XsensMTi630_AttachUart(uart4_id)를 호출하는 구조 (system_startup이 직접 잡지 못함).
 * @version 0.2
 * @date    Apr 27, 2026
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
 * @brief FSR(GRF) UART RX 파이프라인 바인딩 (UART7 = Left, UART8 = Right)
 * @details
 *  - MarvelDex MDAF-25-6850 드라이버 init + IOIF Idle 콜백 등록.
 *  - 센서 미장착 상태에서도 호출 안전.
 * @param[in] grf_left_id  왼발 FSR UART 인스턴스 ID
 * @param[in] grf_right_id 오른발 FSR UART 인스턴스 ID
 */
void UartRxHandler_Init(IOIF_UARTx_t grf_left_id, IOIF_UARTx_t grf_right_id);

/**
 * @brief Xsens MTi-630 IMU를 지정 UART에 결합 (Rev1.1: UART4, PA0/PA1 동적 전환 후 호출)
 * @details
 *  - 파서 인스턴스 초기화 + IOIF Idle Event 콜백 등록만 수행.
 *  - Rev1.1: ExternalIO_SwitchToUartMode() 내부에서 UART4 InitManual 후 자동 호출됨.
 *    사용자는 직접 호출하지 않고 XM_AttachXsensMTi630() Facade를 사용.
 *  - 센서 미연결 상태에서도 안전. 케이블 연결 시 자동 OPERATIONAL 전환.
 *  - Output Configuration은 EEPROM 보존이 일반적이므로 여기서 호출하지 않음.
 *    신품/공장 초기화 센서는 XsensMTi630_ConfigureOutput()을 별도 호출.
 *  - 함수명에 모델(MTi-630)을 명시한 이유: 미래 다른 디바이스 결합 API와 충돌 회피.
 * @param[in] imu_id  IOIF UART 인스턴스 ID
 */
void XsensMTi630_AttachUart(IOIF_UARTx_t imu_id);

/**
 * @brief Xsens MTi-630 Output Configuration 1회 송신 (1kHz Quat+Acc+Gyro)
 * @details
 *  - 신품/공장 초기화 센서에만 필요. EEPROM에 이미 저장된 센서면 호출 불필요.
 *  - 블로킹 ~1초 (GoToConfig + SetOutputCfg + GoToMeasurement, 각 200ms 대기 포함).
 *  - XsensMTi630_AttachUart() 선행 호출 필수. 미호출 시 무시.
 *  - Control_Setup() 또는 별도 명시 시점에 호출 권장 (1kHz 제어 루프 안에서는 호출 금지).
 */
void XsensMTi630_ConfigureOutput(void);

/**
 * @brief UART Rx 진단 정보를 반환합니다. (Live Expression 모니터링용)
 * @param[in] type 0=FSR, 1=IMU
 * @param[out] out_diag 진단 구조체 포인터
 */
void UartRxHandler_GetDiag(uint8_t type, UartRxDiag_t* out_diag);

#endif /* SYSTEM_COMM_UART_UART_RX_HANDLER_H_ */
