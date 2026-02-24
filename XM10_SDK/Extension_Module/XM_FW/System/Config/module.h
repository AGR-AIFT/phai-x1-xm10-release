/**
 ******************************************************************************
 * @file    module.h
 * @author  HyundoKim
 * @brief   [System/Config] XM10 Platform System Layer Configuration
 * @version 3.0 (IMU Hub 스타일 리팩토링)
 * @date    Dec 5, 2025
 *
 * @details
 * System Layer 및 Application Layer에서 공유하는 설정값을 정의합니다.
 * 
 * [적용 범위]
 * - System/Core (core_process.c, system_startup.c)
 * - System/Links (cm_xm_link.c, imu_hub_xm_link.c, pnp_manager.c)
 * - System/Comm (uart_rx_handler.c, canfd_rx_handler.c)
 * - Application (main.c, XM_API)
 * 
 * [IOIF Layer는 사용 금지]
 * - IOIF Layer는 ioif_agrb_defs.h 사용
 * - IOIF는 System Layer 설정에 의존하지 않음 (독립성 유지)
 * 
 * [Device Layer는 사용 금지]
 * - Device Layer는 ioif_agrb_defs.h 사용
 * - Device는 제품 특화 설정에 의존하지 않음 (재사용성 확보)
 * 
 * [원칙]
 * - 실제로 여러 파일에서 공유되는 값만 정의
 * - 하드코딩된 매직 넘버를 여기로 이동
 * - 사용되지 않는 설정은 추가하지 않음
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_CONFIG_MODULE_H_
#define SYSTEM_CONFIG_MODULE_H_

/**
 *===========================================================================
 * PRODUCT IDENTIFICATION
 *===========================================================================
 */

#define XM10_FIRMWARE_VERSION        "3.0.0"
#define XM10_HARDWARE_REVISION       "Rev2.0"

/**
 *===========================================================================
 * EXECUTION ENVIRONMENT (CRITICAL)
 *===========================================================================
 */

/**
 * @brief XM10은 RTOS 전용 플랫폼입니다.
 * @note System Layer 전용 매크로입니다.
 *       Device Layer는 ioif_agrb_defs.h에서 자동 감지합니다.
 */
#define USE_FREERTOS

/**
 *===========================================================================
 * RTOS TASK CONFIGURATION (USE_FREERTOS defined)
 *===========================================================================
 */

#ifdef USE_FREERTOS

/**
 * ============================================================================
 * Task Priority 정의 (높은 순서대로 정리)
 * ============================================================================
 * 
 * [FreeRTOS Priority 범위]
 * - osPriorityRealtime7 (55): StartupTask (초기화)
 * - osPriorityRealtime6 (54): UserTask (Main Control Loop, main.c) ⭐ KING
 * - osPriorityRealtime4 (52): 실시간 데이터 처리
 * - osPriorityRealtime3 (51): 실시간 설정 처리
 * - osPriorityRealtime2 (50): UART 수신
 * - osPriorityNormal1   (25): 일반 서비스
 * - osPriorityNormal    (24): 일반 서비스
 * - osPriorityBelowNormal7 (17): 저우선순위 I/O
 * - osPriorityBelowNormal  (16): 저우선순위 I/O
 * - osPriorityLow       (8): DefaultTask
 * 
 * [제어 공학적 원칙] ⚡
 * 1. **Main Control Loop (King)**: 무조건 최우선
 *    - 500Hz 주기 보장 (±10µs Jitter 목표)
 *    - 다른 Task가 절대 방해 금지
 * 
 * 2. **Data Processing (Prime Minister)**: Main Task 다음
 *    - Main Task가 자는 시간(~800µs)에 데이터 준비
 *    - Main Task 깨어나면 즉시 양보 (Preemption)
 * 
 * 3. **Configuration (Admin)**: Main Task 완료 후
 *    - PnP/SDO는 비실시간 (수백 ms 지연 OK)
 */

/* ===== 1. Real-Time Control (Highest) ===== */
/**
 * @note UserTask (osPriorityRealtime6, 54) ⭐ KING
 * - main.c에서 정의 (IOC 자동 생성)
 * - Main Control Loop (1kHz, 1ms 주기)
 * - 모든 제어 알고리즘 실행
 * - **절대 Preemption 당하면 안됨**
 */

/* ===== 2. Real-Time Data Processing (Below King) ===== */
/**
 * @brief FDCAN Rx Task Priority (IOIF → System Comm)
 * @details 
 * - Main Task(54)보다 낮음: 제어 주기 보장 우선
 * - Slack Time(~800µs)에 데이터 준비
 * - Main Task 깨어나면 즉시 양보
 */
#define TASK_PRIO_FDCAN_RX          osPriorityRealtime4  /**< FDCAN 수신 */
#define TASK_STACK_FDCAN_RX         (2048)

/* ===== 3. Real-Time Configuration (Below Data) ===== */
/**
 * @brief SDO Processor Task Priority
 * @details 
 * - Main Task(54)보다 낮음: 제어 방해 금지
 * - PnP/설정은 비실시간 (수백 ms 지연 OK)
 * - Message Queue FIFO로 순서 보장
 */
#define TASK_PRIO_SDO_PROCESSOR     osPriorityRealtime3  /**< SDO 처리 */
#define TASK_STACK_SDO_PROCESSOR    (2048)

/**
 * @brief UART Rx Task Priority
 * @details 
 * - 디버깅/로깅 용도
 * - Main Loop보다 낮은 우선순위
 */
#define TASK_PRIO_UART_RX           osPriorityRealtime2  /**< UART 수신 */
#define TASK_STACK_UART_RX          (512)

/* ===== 4. Non-Real-Time Services (Normal Priority) ===== */
#define TASK_PRIO_PNP_MANAGER       osPriorityNormal1    /**< PnP 연결 관리 */
#define TASK_STACK_PNP_MANAGER      (512)
#define TASK_PERIOD_MS_PNP_MANAGER  100

#define TASK_PRIO_USB_CONTROL       osPriorityNormal     /**< USB 모드 전환 */
#define TASK_STACK_USB_CONTROL      (1024)
#define TASK_PERIOD_MS_USB_CONTROL  10

/* ===== 5. Low Priority I/O ===== */
#define TASK_PRIO_BTN_CONTROL       osPriorityBelowNormal7  /**< 버튼 입력 */
#define TASK_STACK_BTN_CONTROL      (512)

#define TASK_PRIO_USB_SAVE          osPriorityBelowNormal   /**< USB 데이터 저장 */
#define TASK_STACK_USB_SAVE         (4096 * 4)

#endif  /* USE_FREERTOS */

/**
 *===========================================================================
 * HARDWARE CONFIGURATION (실제 사용)
 *===========================================================================
 */

/* --- Connected Devices --- */
#define XM_MAX_SENSOR_MODULES       7           /**< 최대 센서 모듈 개수 (IMU Hub, EMG Hub, FES Hub, GRF Hub) */

/* --- Sensor Device Instances (Device Layer Multi-Instance 설정) ---
 * module.h에서 정의하면 Device 헤더의 #ifndef 기본값을 오버라이드합니다.
 * XM: XSENS 1개, MarvelDex GRF 2개(L/R)
 */
#define XSENS_MAX_INSTANCES         1           /**< XM은 XSENS IMU 1개 사용 */
#define MARVELDEX_MAX_INSTANCES     2           /**< XM은 GRF 2개 사용 (L/R) */

/**
 *===========================================================================
 * TIMING CONFIGURATION (실제 사용)
 *===========================================================================
 */

/* --- Main Loop --- */
#define XM_MAIN_LOOP_FREQ_HZ        1000        /**< Main Control Loop 주파수 (1kHz = 1ms) */

/* --- Communication Timeout --- */
#define XM_FDCAN_RX_TIMEOUT_MS      100         /**< FDCAN 수신 타임아웃 */
#define XM_HEARTBEAT_INTERVAL_MS    1000        /**< Heartbeat 전송 주기 */

#endif /* SYSTEM_CONFIG_MODULE_H_ */
