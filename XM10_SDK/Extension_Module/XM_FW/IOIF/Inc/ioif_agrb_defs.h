/**
 ******************************************************************************
 * @file    ioif_agrb_defs.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Oct 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef IOIF_IOIF_COMMON_IOIF_AGRB_DEFS_H_
#define IOIF_IOIF_COMMON_IOIF_AGRB_DEFS_H_

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define USE_FREERTOS_DMA

#ifdef USE_FREERTOS_DMA

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// D2 Domain (A*SRAM 2, 288KB): FatFs, USB MSC DataLogger 링버퍼용
#define IOIF_FS_SECTION ".RAM_D2_data"

// D3 Domain (A*SRAM 3, 64KB): ADC, SPI, UART 등 범용 DMA 버퍼용
#define IOIF_DMA_SECTION ".RAM_D3_data"
#define IOIF_USB_CDC_SECTION ".RAM_D3_data"

#endif

#define AGRB_IOIF_FDCAN_ENABLE
// #define AGRB_IOIF_SPI_ENABLE
// #define AGRB_IOIF_I2C_ENABLE
#define AGRB_IOIF_UART_ENABLE
#define AGRB_IOIF_GPIO_ENABLE
#define AGRB_IOIF_USB_ENABLE
#define AGRB_IOIF_ADC_ENABLE
#define AGRB_IOIF_FILESYSTEM_ENABLE

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef enum {
    AGRBStatus_OK,
    AGRBStatus_ERROR,               // 일반적인 오류

    AGRBStatus_NO_RESOURCE,         // 사용 가능한 리소스 없음
    AGRBStatus_INITIAL_FAILED,
    AGRBStatus_BUSY,                // 장치가 사용 중
    AGRBStatus_TIMEOUT,             // 작업 시간 초과
    AGRBStatus_NOT_INITIALIZED,     // 초기화되지 않음

    AGRBStatus_PARAM_ERROR,         // 잘못된 매개변수
    
    AGRBStatus_INTEGRITY_ERROR,
    AGRBStatus_SECURITY_ERROR,

    AGRBStatus_NOT_ALLOWED,
    AGRBStatus_BUFFER_OVERFLOW,

    AGRBStatus_NOT_FOUND,
    AGRBStatus_NOT_SUPPORTED,       // 지원되지 않는 기능
    AGRBStatus_SEMAPHORE_ERROR,
    
    AGRBStatus_FAILED,

} AGRBStatusDef;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


#endif /* IOIF_IOIF_COMMON_IOIF_AGRB_DEFS_H_ */
