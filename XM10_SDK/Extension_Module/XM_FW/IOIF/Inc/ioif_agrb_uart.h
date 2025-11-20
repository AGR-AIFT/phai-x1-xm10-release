/**
 ******************************************************************************
 * @file    ioif_agrb_uart.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] UART 하드웨어 추상화 계층 헤더
 * @details STM32 HAL UART를 감싸서 DMA 기반의 비동기 수신 및 동기 송신을 지원합니다.
 * @version 0.1
 * @date    Nov 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_UART_ENABLE)

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_UART_H_
#define IOIF_INC_IOIF_AGRB_UART_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define IOIF_UART_MAX_INSTANCES      (4) // [수정] FSR(2) + IMU(1) + 여유(1)
#define IOIF_UART_ID_NOT_ALLOCATED   (0xFFFFFFFF)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef uint32_t IOIF_UARTx_t;

typedef enum {
    IOIF_UART_Baudrate_9600,
    IOIF_UART_Baudrate_19200,
    IOIF_UART_Baudrate_38400,
    IOIF_UART_Baudrate_57600,
    IOIF_UART_Baudrate_115200,
    IOIF_UART_Baudrate_230400,
    IOIF_UART_Baudrate_460800,
    IOIF_UART_Baudrate_921600,
    // 필요시 추가
} IOIF_UART_Baudrate_e;

/**
 * @brief [신규] UART 수신 방식을 선택하는 열거형
 */
typedef enum {
    IOIF_UART_MODE_POLLING_TASK, // 1ms 태스크가 DMA 링버퍼를 폴링 (FSR용)
    IOIF_UART_MODE_IDLE_EVENT,   // IDLE 라인 인터럽트 기반 (Xsens IMU용)
} IOIF_UART_RxMode_e;

// [RX 콜백] DMA 수신 데이터가 처리된 후 System Layer로 전달되는 콜백
typedef void (*IOIF_UART_RxEventCallback_t)(uint8_t* rx_buf, uint32_t size, uint32_t id);

typedef struct {
    IOIF_UART_Baudrate_e baudrate;
    IOIF_UART_RxMode_e rxMode; // 수신 모드 선택
    uint32_t bounce_buffer_size; // (POLLING_TASK 모드 전용)
    IOIF_UART_RxEventCallback_t rx_event_callback;
} IOIF_UART_Config_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief UART 인스턴스를 할당하고 초기화합니다.
 * @param[out] id       할당된 IOIF 핸들 ID
 * @param[in]  huart    STM32 HAL UART 핸들
 * @param[in]  config   초기화 설정 구조체
 * @return AGRBStatusDef
 */
AGRBStatusDef ioif_uart_assign_instance(IOIF_UARTx_t* id, UART_HandleTypeDef* huart, IOIF_UART_Config_t* config);

/**
 * @brief UART 수신을 시작합니다. (DMA Circular Mode)
 */
AGRBStatusDef ioif_uart_start(IOIF_UARTx_t id);

/**
 * @brief UART 수신 콜백을 갱신합니다. (런타임 변경용)
 */
AGRBStatusDef ioif_uart_update_rx_callback(IOIF_UARTx_t id, IOIF_UART_RxEventCallback_t callback);

/**
 * @brief 데이터를 송신합니다. (Blocking w/ DMA)
 * @details 전송이 완료될 때까지 세마포어를 대기합니다.
 */
AGRBStatusDef ioif_uart_write(IOIF_UARTx_t id, uint8_t* tx_buf, uint32_t size);

/**
 * @brief UART 버퍼를 비웁니다.
 */
AGRBStatusDef ioif_uart_flush(IOIF_UARTx_t id);

#endif // AGRB_IOIF_UART_ENABLE
#endif /* IOIF_INC_IOIF_AGRB_UART_H_ */
