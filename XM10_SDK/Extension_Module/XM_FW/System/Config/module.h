/**
 ******************************************************************************
 * @file    module.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Oct 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_CONFIG_MODULE_H_
#define SYSTEM_CONFIG_MODULE_H_

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// System Configuration
#define EXTENSION_MODULE_ENABLED

/* ====================================================================
 * 시스템 상세 구성 (System Configuration)
 * ==================================================================== */

/* --- 운영체제 및 시스템 설정 --- */
//  #define _USE_BAREMETAL
// #define _USE_OS_RTOS_BSP
// #define _USE_OS_RTOS
// #define _USE_CMSISV1
// #define _USE_CMSISV2
// #define _USE_SEMAPHORE
#define USE_FREERTOS_DMA  // RTOS 및 DMA 기반 비동기 API 활성화

#ifdef USE_FREERTOS_DMA
    // === RTOS Task Configuration ===
    // 1. Startup
    // #define TASK_PRIO_STARTUP       osPriorityRealtime7 main에 이미 정의되어 있음.
    // #define TASK_STACK_STARTUP      (128 * 4)

    // 2. Real-Time Data Path
    #define TASK_PRIO_FDCAN_RX          osPriorityRealtime6 // (가장 높음: 제어 데이터 수신)
    #define TASK_STACK_FDCAN_RX         (512) // 1024 * 2
    #define TASK_PRIO_UART_RX           osPriorityRealtime5 // (높음: UART 데이터 수신)
    #define TASK_STACK_UART_RX          (512) // 1024 * 2
    // #define TASK_PRIO_USER_MAIN     osPriorityRealtime4 main에 이미 정의되어 있음. (높음: 제어 설정)
    // #define TASK_STACK_USER_MAIN    (4096 * 4)
    #define TASK_PRIO_SDO_ROUTER        osPriorityRealtime3
    #define TASK_STACK_SDO_ROUTER       (512) // 1024 * 4

    // 3. Non-Real-Time Services
    #define TASK_PRIO_PNP_MANAGER       osPriorityNormal1   // (중간: 연결 관리)
    #define TASK_STACK_PNP_MANAGER      (512) // 1024 * 4
    #define TAST_PERIOD_MS_PNP_MANAGER  10                  // 10ms
    #define TASK_PRIO_USB_CONTROL       osPriorityNormal    // (중간: USB 모드 전환)
    #define TASK_STACK_USB_CONTROL      (1024) // 8192 * 4
    #define TAST_PERIOD_MS_USB_CONTROL  10                  // 10ms

    // 4. Low Priority I/O
    #define TASK_PRIO_BTN_CONTROL       osPriorityBelowNormal7 // (낮음: 버튼 입력 모니터링 및 출력)
    #define TASK_STACK_BTN_CONTROL      (512)
    #define TASK_PRIO_USB_SAVE          osPriorityBelowNormal // (낮음: USB 데이터 저장 100ms)
    #define TASK_STACK_USB_SAVE         (4096 * 4)
#endif

/* --- 하드웨어 종속적인 값 정의 --- */
// TODO: 추후 삭제 유무 결정
// #define BUS_VOLTAGE 24
// #define VBUS2DUTY_RATIO 200

// Device Configuration 
// TODO: Device 계층 리팩토링에 따른 추후 삭제 유무 결정
// #define IOIF_LTC2944_ENABLED
// #define IOIF_BM1422AGMV_ENABLED
// #define IOIF_ICM20608G_ENABLED
// #define IOIF_NZRLED_ENABLED
// #define IOIF_RMB30SC_ENABLED
// #define IOIF_THINPOT_LS_ENABLED
// #define IOIF_TB67H450FNGEL_ENABLED
// #define IOIF_BUZZER_ENABLED
// #define IOIF_BATTERYLED_ENABLED
// #define IOIF_GRFSENSOR_ENABLED

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


#endif /* SYSTEM_CONFIG_MODULE_H_ */
