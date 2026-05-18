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
 * Task Priority Map (높은 순서)
 * ============================================================================
 *
 *  Prio  Task                 설정 위치        주기     역할
 *  ────  ───────────────────  ──────────────  ──────  ──────────────────────
 *  55    StartupTask          main.c (IOC)    once    시스템 초기화 후 자기 삭제
 *  55    FDCAN RxTask         ioif_conf.h     event   PDO 수신, 즉시 선점 처리
 *  55    UART RxTask          ioif_conf.h     event   센서 패킷 파싱 (DMA→파서)
 *  54    UserTask             main.c (IOC)    1ms     IPO Control Loop
 *  51    SDO Processor        module.h        event   PnP/설정 (비실시간)
 *  25    PnP Manager          module.h        100ms   연결 관리
 *  24    USB Control          module.h        10ms    USB 모드 전환
 *  24    DataLoggerTask       module.h        100ms   USB MSC f_write
 *  17    Button Control       module.h        event   버튼 입력
 *   8    DefaultTask          main.c (IOC)    —       FreeRTOS idle (suspended)
 *
 *  [설계 원칙]
 *  RxTask(55) > UserTask(54): 데이터 도착 즉시 선점 → stale data 방지
 *  RxTask 실행 ~10-50us/선점 → UserTask 지터 무시 가능 (<100us/1ms)
 *  FDCAN/UART RxTask는 IOIF 내부 생성 → ioif_conf.h에서 오버라이드
 */

/* ----- FDCAN/UART RxTask (55) — ioif_conf.h 참조 ----- */
/* ----- UserTask (54) — main.c IOC 참조 ----- */

/* ----- SDO Processor (51) ----- */
#define TASK_PRIO_SDO_PROCESSOR     osPriorityRealtime3  /**< (51) SDO/PnP 설정 처리 */
#define TASK_STACK_SDO_PROCESSOR    (2048)

/* ----- Non-Real-Time Services ----- */
#define TASK_PRIO_PNP_MANAGER       osPriorityNormal1    /**< (25) PnP 연결 관리 */
/* [2026-05-12] 512 → 2048: vApplicationStackOverflowHook 으로 PnP_Task overflow 검출.
 *  CM/IMU/EMG/FES Hub PnP + Heartbeat + Boot-up + NMT timeout + XsensMTi_RunPeriodic
 *  까지 처리해서 512B 로는 부족. 작동 브랜치(5552725) 도 같은 값이라 같은 증상 보였음. */
#define TASK_STACK_PNP_MANAGER      (2048)
#define TASK_PERIOD_MS_PNP_MANAGER  100

#define TASK_PRIO_USB_CONTROL       osPriorityNormal     /**< (24) USB 모드 전환 */
#define TASK_STACK_USB_CONTROL      (1024)
#define TASK_PERIOD_MS_USB_CONTROL  10

/* ----- Low Priority I/O ----- */
#define TASK_PRIO_BTN_CONTROL       osPriorityBelowNormal7  /**< (17) 버튼 입력 */
#define TASK_STACK_BTN_CONTROL      (512)

#define TASK_PRIO_USB_SAVE          osPriorityNormal        /**< (24) USB 데이터 저장 — Priority inversion 완화 */
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

/**
 *===========================================================================
 * DIAGNOSTIC FEATURE FLAGS (Rev2.0 에서 Phase 1 계측 인프라 이식)
 *===========================================================================
 * - CMake 빌드: -DDIAG_PROFILE_ENABLED=ON / -DDIAG_KILL_DRAIN_WRITE=ON 로 지정 가능.
 *   CMake 가 먼저 #define 하면 여기 #ifndef 가 skip 되어 중복 없음.
 * - CubeIDE 빌드: CMake 없이 빌드 시 이 파일의 default 가 적용됨.
 *
 * 측정 워크플로우:
 *   K1  : DIAG_PROFILE_ENABLED 만 활성 (drain write 정상)
 *   K2a : 둘 다 활성 (drain write skip → tight spin 제거 측정)
 *   완료: 둘 다 주석 처리 (zero-cost 복귀)
 */

/* UserTask jitter histogram + ISR breakdown + f_write/pdo_gap histogram 수집 활성화.
 *  측정 종료 후 이 줄을 주석 처리하여 FW stub 상태로 복귀. */
#ifndef DIAG_PROFILE_ENABLED
#define DIAG_PROFILE_ENABLED
#endif

/* _WriteDataWithBlockCRC early-return 으로 drain write 전면 skip.
 *  tight spin 제거 측정 시 uncomment, 측정 완료 후 comment 복귀. */
/* #define DIAG_KILL_DRAIN_WRITE */

/* [HardFault dump 인프라 round-trip 검증용]
 *  boot 후 5초 지난 시점에 의도적 UsageFault (divide-by-zero) 발생 →
 *  NVIC_SystemReset → 재마운트 → /LOGS/hardfault_<uptime>.txt 생성 확인.
 *  검증 완료 후 반드시 comment 복귀 (production 안전). */
/* #define XM_TEST_FAULT_AT_BOOT */

#endif /* SYSTEM_CONFIG_MODULE_H_ */
