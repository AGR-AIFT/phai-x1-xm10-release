/**
 ******************************************************************************
 * @file    ioif_conf.h
 * @author  HyundoKim
 * @brief   [System/Config] XM10 IOIF Module Configuration
 * @version 1.0
 * @date    Feb 11, 2026
 *
 * @details
 * IOIF Submodule (AGR-EXO/IOIF)에서 사용할 모듈을 활성화합니다.
 * STM32 HAL의 stm32xx_hal_conf.h 패턴과 동일한 방식입니다.
 * 
 * ioif_agrb_defs.h에서 이 파일을 #include 합니다.
 * 프로젝트 include 경로(System/Config/)에 위치해야 합니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef IOIF_CONF_H_
#define IOIF_CONF_H_

/**
 *===========================================================================
 * IOIF Module Enable/Disable (XM10 Platform)
 *===========================================================================
 * 사용할 모듈만 #define으로 활성화합니다.
 * 비활성 모듈은 빈 번역 단위로 컴파일됩니다 (오버헤드 없음).
 */

/* ===== FDCAN Rx 처리 모드 (RxTask — CM-WH 공통 RTOS Best Practice) ===== */
/**
 * @brief FDCAN 수신 = ISR(sem give) → RxTask → NonRealtimeTask 3계층 RTOS 패턴.
 * @details
 * - FDCAN NVIC = 5 (configMAX_SYSCALL 경계) → ISR에서 xSemaphoreGiveFromISR 합법
 *   (전제: .ioc NVIC.FDCAN1/2_IT0/1 preemption = 5. 4이면 FromISR @ NVIC4 = configASSERT HardFault)
 * - ISR(sem give만) → RxTask(55, > UserTask 54): 빠른 PDO를 Mutex+Snapshot으로 DataLake 기록
 *                   → NonRealtimeTask(51, < UserTask 54): 느린 SDO/NMT 처리
 * - Device Layer = Mutex + Snapshot (Reader timeout=0 / Writer timeout=1)
 */

/* ===== IOIF Task Priority Override ===== */
/**
 * @brief UART/FDCAN RxTask 우선순위를 UserTask(54)보다 높게 설정
 * @details 데이터 도착 즉시 선점 처리하여 stale data(duplicate) 방지 (B001 실증).
 *          RxTask 실행 시간 ~10-50µs/선점 → UserTask 지터 무시 가능.
 *          [2026-07-14] 우선순위 재배치 55(FDCAN) > 54(UART) > 53(UserTask):
 *          동급(55=55) 라운드로빈 비결정성을 없애 FDCAN(로봇/IMU/EMG 1kHz)이 UART
 *          (GRF 1kHz)를 결정론적으로 앞서게 한다. 두 RxTask 모두 '짧은' 태스크(큐잉/
 *          스냅샷, 무거운 SDO/NMT 는 NonRealtimeTask 51 로 위임)라 FDCAN 선점 시간은
 *          유계(수~수십µs) → 정상동작에서 UART GRF 1ms 데드라인 안전. 두 RxTask 모두
 *          UserTask(53) 위라 stale-data 방지 불변식 유지.
 */
#define IOIF_UART_RX_TASK_PRIORITY      (osPriorityRealtime6)  /**< 54: FDCAN(55) > UART(54) > UserTask(53) */
#define IOIF_FDCAN_RX_TASK_PRIORITY     (osPriorityRealtime7)  /**< 55: 로봇/IMU/EMG PDO, UART(54)보다 우선 */

/* [2026-07-14] 공유 UART RxTask drain-cap backoff 무력화 (XM 전용).
 * IOIF 기본 BACKOFF_TICKS=1 은 한 인스턴스 백로그(>4×128=512B) 시 vTaskDelay(1ms) 로
 * 공유 RxTask 전체를 블로킹 → UART7·UART8(좌우 GRF, 둘 다 활성 1kHz RX) 상호 지연.
 * 0 = taskYIELD (고정 1ms 블록 제거). XM 은 다중 활성 RX 인스턴스라 유일하게 필요 —
 * IMU(BareMetal, 경로 배제)·GRF(RX discard-only, 단일 인스턴스)는 불필요. */
#define IOIF_UART_RX_TASK_BACKOFF_TICKS (0U)

/* ===== IOIF Task Stack Size Override =====
 * [2026-05-12 cross-port from 0428] vApplicationStackOverflowHook 으로
 * "IOIF_UartRx" overflow 검출. 기본 512B 는 Xsens packet 파싱 + DataLake 갱신 +
 * StreamBuffer 처리에 부족. 2048B 로 상향 (다른 시스템 task 와 동일 수준). */
#define IOIF_UART_RX_TASK_STACK_SIZE    (2048U)

/* ===== Production Modules (현재 ��용 중) ===== */
#define AGRB_IOIF_FDCAN_ENABLE              /**< FDCAN (CAN FD) - DOP/PnP 통신 */
#define AGRB_IOIF_UART_ENABLE               /**< UART - 센서/디버그 통신 */
#define AGRB_IOIF_GPIO_ENABLE               /**< GPIO - LED, Button, Power Control */
#define AGRB_IOIF_TIM_ENABLE                /**< Timer - 시스템 타이머 */
#define AGRB_IOIF_DWT_ENABLE                /**< DWT - 고정밀 성능 측정 */
#define AGRB_IOIF_USB_ENABLE                /**< USB - CDC 디버그, MSC 데이터 로깅 */
#define AGRB_IOIF_USB_MODE_DRP              /**< Host MSC (USB 스틱) ↔ Device CDC 런타임 스위칭 */
#define AGRB_IOIF_ADC_ENABLE                /**< ADC - 아날로그 센서 입력 */
#define AGRB_IOIF_CRC_ENABLE                /**< CRC - HW CRC32 (data_logger 블록 무결성) */
#define AGRB_IOIF_FILESYSTEM_ENABLE         /**< FatFs - USB MSC 파일 시스템 */

/* ===== Rev2.0 Modules (SPI 디바이스 + PSRAM + ETH 추가) ===== */
#define AGRB_IOIF_SPI_ENABLE                /**< SPI - RTC(MCP79510), LED Driver(PCA9957) */
#define AGRB_IOIF_DMA_ENABLE                /**< DMA Pool Manager - SPI DMA 버퍼 할당 */
#define AGRB_IOIF_PSRAM_ENABLE              /**< PSRAM - APS6404L 8MB QSPI Memory-Mapped */
#define AGRB_IOIF_ETH_MDIO_ENABLE           /**< ETH MDIO - RTL8201F PHY 레지스터 R/W */
#define IOIF_PSRAM_BENCHMARK_DISABLE        /**< Benchmark 비활성화 — 256KB .bss 절약 (개발 시 주석처리하여 활성화) */

/**
 * DMA Pool Size Override (XM10 전용)
 * - 기본값: DMA=56KB, BDMA=1KB, MDMA=4KB = 총 61KB → RAM_D3(64KB) 초과
 * - XM10 SPI DMA 실사용: RTC(16B TX+RX) + LED(32B TX+RX) ≈ 96B
 * - 여유 포함 1KB면 충분. BDMA/MDMA는 최소값 유지.
 */
#define IOIF_DMA_POOL_SIZE          (1 * 1024)  /**< 1KB (기본 56KB → XM10 SPI용 축소) */
#define IOIF_BDMA_POOL_SIZE         (256)       /**< 256B (기본 1KB → 최소) */
/* ===== [DEPRECATED 2026-04-18 C안] Phase 2: PSRAM Cold Buffer MDMA =====
 * [현재] PSRAM Cold Buffer 폐기. data_logger.c Hot-only 회귀. data_logger.c:28 참조.
 * [참고] 아래 매크로 값은 잠정 유지 — MDMA 인프라 자체는 IOC 활성 (stm32h7xx_hal_msp.c:627~).
 *        Cold Buffer 외 다른 용도 (예: PSRAM 진단 자극) 에서 MDMA pool/청크 사용 가능.
 *
 * [이전 설계 — 참고]
 *   [AS-IS] 256B — QSPI MDMA가 IOC에 미설정, DMA pool fallback 발생
 *   [TO-BE] 5KB — IOC에서 QUADSPI MDMA 활성화 후, ioif_dma.allocate()가
 *                 hqspi->hmdma MDMA Instance 감지 → MDMA pool로 라우팅.
 *                 PSRAM 4KB 청크 + 정렬 여유 = 5KB.
 *   [Impact] MDMA pool = .RAM_D3_data (D3 64KB). 256B → 5KB (+4.75KB).
 */
#define IOIF_MDMA_POOL_SIZE         (5 * 1024)

/* [DEPRECATED 2026-04-18 C안 — Cold Buffer 폐기] PSRAM Offload 청크 크기.
 * 매크로 자체는 PSRAM 진단/사용자 영역 다른 호출에서 잔존 사용 가능 (잠정 유지).
 * 이전 설계: 128B → 4KB (Offload 1회 전송량 최적화). 6KB 전송 시 47회 → 2회. */
#define IOIF_PSRAM_TRANSFER_CHUNK_SIZE_BYTES    (4U * 1024U)

/**
 * GPIO Pool Size Override (XM10 전용)
 * - 기본값: 32개 — Rev1.x까지 충분
 * - XM10 Rev2.0: 34개 사용 (SPI CS×2, PHY_RST, EXT_PWR_SEL, RTC_nINT 추가)
 * - 32 초과 시 s_gpio_spi5_cs_id(PCA9957 CS) 할당 실패 → SPI5 통신 불가!
 */
#define IOIF_GPIO_MAX_INSTANCES     (48)

/* ===== Optional Modules (필요시 활성화) ===== */
// #define AGRB_IOIF_I2C_ENABLE             /**< I2C - 외부 센서 (미사용) */
// #define AGRB_IOIF_SAI_ENABLE             /**< SAI - Audio (H10 CM 전용, 미사용) */

#endif /* IOIF_CONF_H_ */
