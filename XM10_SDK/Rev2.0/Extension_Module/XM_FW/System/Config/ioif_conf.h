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

/* ===== FDCAN ISR-Direct Mode (V5.0) ===== */
/**
 * @brief FDCAN ISR에서 직접 FIFO drain + 콜백 호출 (RxTask 제거)
 * @details
 * - FDCAN NVIC = 4 (configMAX_SYSCALL(5) 위) → FreeRTOS critical section 마스킹 불가
 * - ISR에서 PDO 직접 처리 (Seqlock) + SDO는 Lock-Free Ring Buffer → SW IRQ → NonRealtimeTask
 * - f_sync/f_write 중에도 PDO 수신 보장 → Real Gap 해결
 * - 활성화 시 RxTask/Semaphore 생성 생략, Device Layer는 Mutex 대신 Seqlock 사용 필수
 */
#define IOIF_FDCAN_ISR_DIRECT_ENABLE

/* ===== IOIF Task Priority Override ===== */
/**
 * @brief UART RxTask 우선순위를 UserTask(54)보다 높게 설정
 * @details 데이터 도착 즉시 선점 처리하여 stale data(duplicate) 방지.
 *          RxTask 실행 시간 ~10-50µs/선점 → UserTask 지터 무시 가능.
 */
#define IOIF_UART_RX_TASK_PRIORITY      (osPriorityRealtime7)  /**< 기본 50 → 55 */

/* ===== Production Modules (현재 ��용 중) ===== */
#define AGRB_IOIF_FDCAN_ENABLE              /**< FDCAN (CAN FD) - DOP/PnP 통신 */
#define AGRB_IOIF_UART_ENABLE               /**< UART - 센서/디버그 통신 */
#define AGRB_IOIF_GPIO_ENABLE               /**< GPIO - LED, Button, Power Control */
#define AGRB_IOIF_TIM_ENABLE                /**< Timer - 시스템 타이머 */
#define AGRB_IOIF_DWT_ENABLE                /**< DWT - 고정밀 성능 측정 */
#define AGRB_IOIF_USB_ENABLE                /**< USB - CDC 디버그, MSC 데이터 로깅 */
#define AGRB_IOIF_ADC_ENABLE                /**< ADC - 아날로그 센서 입력 */
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
/* ===== Phase 2: PSRAM Cold Buffer MDMA 활성화 =====
 * [AS-IS] 256B — QSPI MDMA가 IOC에 미설정, DMA pool fallback 발생
 * [TO-BE] 5KB — IOC에서 QUADSPI MDMA 활성화 후, ioif_dma.allocate()가
 *               hqspi->hmdma MDMA Instance 감지 → MDMA pool로 라우팅.
 *               PSRAM 4KB 청크 + 정렬 여유 = 5KB.
 * [Impact] MDMA pool = .RAM_D3_data (D3 64KB). 256B → 5KB (+4.75KB). */
#define IOIF_MDMA_POOL_SIZE         (5 * 1024)

/* PSRAM Offload 청크 크기 오버라이드: 128B → 4KB (Offload 1회 전송량 최적화)
 * ioif_agrb_psram.c 기본값(128B)을 프로젝트별 오버라이드.
 * 6KB 전송 시: 47회(128B) → 2회(4KB). MDMA 전송 오버헤드 대폭 감소. */
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
