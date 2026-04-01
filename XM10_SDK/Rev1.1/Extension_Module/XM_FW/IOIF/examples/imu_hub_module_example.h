/**
 ******************************************************************************
 * @file    imu_hub_module_example.h
 * @brief   [예제] IMU Hub (BareMetal + STM32G4) 프로젝트에서 IOIF 모듈 활성화 예시
 * @details
 * 이 파일을 참고하여 각 Product Project의 module.h에서 
 * 필요한 IOIF 모듈을 활성화합니다.
 ******************************************************************************
 */

#ifndef IMU_HUB_MODULE_EXAMPLE_H_
#define IMU_HUB_MODULE_EXAMPLE_H_

/* ============================================================
 * IOIF Module Enable (IMU Hub - BareMetal + STM32G474)
 * ============================================================
 * 
 * [활성화 모듈]
 * - FDCAN: XM과 통신 (TPDO1/TPDO2)
 * - UART: XSENS/EBIMU 센서 통신 (최대 4채널)
 * - GPIO: LED, 센서 전원 제어
 * - Timer: Core_RunLoop 트리거 (1ms)
 * - DWT: ISR 성능 프로파일링
 */

#define AGRB_IOIF_FDCAN_ENABLE
#define AGRB_IOIF_UART_ENABLE
#define AGRB_IOIF_GPIO_ENABLE
#define AGRB_IOIF_TIM_ENABLE
#define AGRB_IOIF_DWT_ENABLE

/* [비활성 모듈]
 * - ADC: IMU Hub에서 미사용
 * - DMA Manager: BareMetal은 DMA Pool 불필요
 * - SPI/I2C: 직접 센서 연결 없음 (UART로 통신)
 * - USB/FS/SAI/PSRAM: G4에서 미사용
 */
/* #define AGRB_IOIF_ADC_ENABLE */
/* #define AGRB_IOIF_DMA_ENABLE */
/* #define AGRB_IOIF_SPI_ENABLE */
/* #define AGRB_IOIF_I2C_ENABLE */
/* #define AGRB_IOIF_USB_ENABLE */
/* #define AGRB_IOIF_FILESYSTEM_ENABLE */

/* ============================================================
 * G4: DMA Pool 설정 불필요 (BareMetal)
 * IOIF_DMA_SECTION은 자동으로 빈 문자열 (단일 SRAM)
 * ============================================================ */

#endif /* IMU_HUB_MODULE_EXAMPLE_H_ */
