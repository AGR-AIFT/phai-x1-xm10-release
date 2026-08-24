/**
 ******************************************************************************
 * @file    xm_module_example.h
 * @brief   [예제] XM (RTOS + STM32H7) 프로젝트에서 IOIF 모듈 활성화 예시
 * @details
 * 이 파일을 참고하여 각 Product Project의 module.h에서 
 * 필요한 IOIF 모듈을 활성화합니다.
 ******************************************************************************
 */

#ifndef XM_MODULE_EXAMPLE_H_
#define XM_MODULE_EXAMPLE_H_

/* ============================================================
 * IOIF Module Enable (XM10 - RTOS + STM32H743)
 * ============================================================
 * 
 * [활성화 모듈]
 * - FDCAN: Control Module, IMU Hub 통신
 * - UART: XSENS, GRF 센서 통신
 * - GPIO: LED, RS485 방향 제어
 * - Timer: Core_RunLoop 트리거
 * - ADC: 배터리 전압, External IO
 * - DWT: ISR/Task 성능 프로파일링
 * - DMA: SPI/I2C DMA 버퍼 풀 (H7 메모리 맵 관리)
 * - USB: CDC (디버그 콘솔), MSC (로그 저장)
 * - FileSystem: USB MSC 파일 시스템
 */

#define AGRB_IOIF_FDCAN_ENABLE
#define AGRB_IOIF_UART_ENABLE
#define AGRB_IOIF_GPIO_ENABLE
#define AGRB_IOIF_TIM_ENABLE
#define AGRB_IOIF_ADC_ENABLE
#define AGRB_IOIF_DWT_ENABLE
#define AGRB_IOIF_DMA_ENABLE
#define AGRB_IOIF_USB_ENABLE
#define AGRB_IOIF_FILESYSTEM_ENABLE

/* [비활성 모듈]
 * - SPI: XM10에서 사용 안 함
 * - I2C: XM10에서 사용 안 함
 * - SAI: 오디오 미사용
 * - PSRAM: 외부 SRAM 미사용
 */
/* #define AGRB_IOIF_SPI_ENABLE */
/* #define AGRB_IOIF_I2C_ENABLE */
/* #define AGRB_IOIF_SAI_ENABLE */
/* #define AGRB_IOIF_PSRAM_ENABLE */

/* ============================================================
 * DMA Pool Configuration (H7: 큰 풀, 다중 DMA 타입)
 * ============================================================ */
#define IOIF_DMA_POOL_SIZE      (56 * 1024)  /**< DMA Pool: 56KB (AXI SRAM) */
#define IOIF_BDMA_POOL_SIZE     (1 * 1024)   /**< BDMA Pool: 1KB (SRAM4) */
#define IOIF_MDMA_POOL_SIZE     (4 * 1024)   /**< MDMA Pool: 4KB */

/* ============================================================
 * Memory Section Override (H7 링커 스크립트에 맞춤)
 * ============================================================ */
#define IOIF_DMA_SECTION        ".RAM_D3_data"
#define IOIF_BDMA_SECTION       ".RAM_D3_data"
#define IOIF_USB_CDC_SECTION    ".RAM_D3_data"

#endif /* XM_MODULE_EXAMPLE_H_ */
