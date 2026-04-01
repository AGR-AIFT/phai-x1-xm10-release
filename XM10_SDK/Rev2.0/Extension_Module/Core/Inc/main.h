/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_USB_OTG_FS_HCD_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define L_GRF_PWR_EN_Pin GPIO_PIN_10
#define L_GRF_PWR_EN_GPIO_Port GPIOG
#define HMMG__FAULT_Pin GPIO_PIN_9
#define HMMG__FAULT_GPIO_Port GPIOG
#define EXT_UART_TX_Pin GPIO_PIN_5
#define EXT_UART_TX_GPIO_Port GPIOD
#define RTC_nINT_Pin GPIO_PIN_4
#define RTC_nINT_GPIO_Port GPIOD
#define PSRAM_QSPI_IO1A_Pin GPIO_PIN_10
#define PSRAM_QSPI_IO1A_GPIO_Port GPIOC
#define USB_DFP_UFP_Pin GPIO_PIN_15
#define USB_DFP_UFP_GPIO_Port GPIOA
#define RTC_SPI_SCK_Pin GPIO_PIN_1
#define RTC_SPI_SCK_GPIO_Port GPIOI
#define RTC_SPI_NSSA_Pin GPIO_PIN_0
#define RTC_SPI_NSSA_GPIO_Port GPIOI
#define R_GRF_UART_TX_Pin GPIO_PIN_1
#define R_GRF_UART_TX_GPIO_Port GPIOE
#define TP13_Pin GPIO_PIN_6
#define TP13_GPIO_Port GPIOB
#define L_GRF_UART_TX_Pin GPIO_PIN_4
#define L_GRF_UART_TX_GPIO_Port GPIOB
#define L_GRF__FAULT_Pin GPIO_PIN_11
#define L_GRF__FAULT_GPIO_Port GPIOG
#define EXT_UART_RX_Pin GPIO_PIN_6
#define EXT_UART_RX_GPIO_Port GPIOD
#define FUNC_BTN_1_Pin GPIO_PIN_11
#define FUNC_BTN_1_GPIO_Port GPIOC
#define DEBUG_SWCLK_Pin GPIO_PIN_14
#define DEBUG_SWCLK_GPIO_Port GPIOA
#define RTC_SPI_MISO_Pin GPIO_PIN_2
#define RTC_SPI_MISO_GPIO_Port GPIOI
#define FUNC_LED_3_Pin GPIO_PIN_15
#define FUNC_LED_3_GPIO_Port GPIOH
#define FUNC_LED_2_Pin GPIO_PIN_14
#define FUNC_LED_2_GPIO_Port GPIOH
#define PSRAM_QSPI_IO2A_Pin GPIO_PIN_2
#define PSRAM_QSPI_IO2A_GPIO_Port GPIOE
#define R_GRF_UART_RX_Pin GPIO_PIN_0
#define R_GRF_UART_RX_GPIO_Port GPIOE
#define TP14_Pin GPIO_PIN_7
#define TP14_GPIO_Port GPIOB
#define DEBUG_TRACE_SWO_Pin GPIO_PIN_3
#define DEBUG_TRACE_SWO_GPIO_Port GPIOB
#define R_GRF_PWR_EN_Pin GPIO_PIN_12
#define R_GRF_PWR_EN_GPIO_Port GPIOG
#define USB_PWR_ON_Pin GPIO_PIN_7
#define USB_PWR_ON_GPIO_Port GPIOD
#define FUNC_BTN_2_Pin GPIO_PIN_12
#define FUNC_BTN_2_GPIO_Port GPIOC
#define RTC_SPI_MOSI_Pin GPIO_PIN_3
#define RTC_SPI_MOSI_GPIO_Port GPIOI
#define DEBUG_SWDIO_Pin GPIO_PIN_13
#define DEBUG_SWDIO_GPIO_Port GPIOA
#define EXT_PWR_SEL_5V_Pin GPIO_PIN_3
#define EXT_PWR_SEL_5V_GPIO_Port GPIOE
#define R_GRF__FAULT_Pin GPIO_PIN_15
#define R_GRF__FAULT_GPIO_Port GPIOG
#define MCU_RMII_TXD1A_Pin GPIO_PIN_14
#define MCU_RMII_TXD1A_GPIO_Port GPIOG
#define MCU_RMII_TXD0A_Pin GPIO_PIN_13
#define MCU_RMII_TXD0A_GPIO_Port GPIOG
#define TP12_Pin GPIO_PIN_2
#define TP12_GPIO_Port GPIOD
#define FDCAN1_RX_Pin GPIO_PIN_0
#define FDCAN1_RX_GPIO_Port GPIOD
#define USB_OTG_FS_VBUS_Pin GPIO_PIN_9
#define USB_OTG_FS_VBUS_GPIO_Port GPIOA
#define FUNC_LED_1_Pin GPIO_PIN_13
#define FUNC_LED_1_GPIO_Port GPIOH
#define FUNC_BTN_3_Pin GPIO_PIN_13
#define FUNC_BTN_3_GPIO_Port GPIOC
#define FDCAN1_TX_Pin GPIO_PIN_1
#define FDCAN1_TX_GPIO_Port GPIOD
#define CM_LED_G_Pin GPIO_PIN_8
#define CM_LED_G_GPIO_Port GPIOC
#define CM_LED_B_Pin GPIO_PIN_9
#define CM_LED_B_GPIO_Port GPIOC
#define MCU_RESET_N_Pin GPIO_PIN_10
#define MCU_RESET_N_GPIO_Port GPIOI
#define CM_LED_R_Pin GPIO_PIN_7
#define CM_LED_R_GPIO_Port GPIOC
#define POWER_ON_LED_Pin GPIO_PIN_6
#define POWER_ON_LED_GPIO_Port GPIOC
#define HMMG_PWR_EN_Pin GPIO_PIN_8
#define HMMG_PWR_EN_GPIO_Port GPIOG
#define IMU__FAULT_Pin GPIO_PIN_7
#define IMU__FAULT_GPIO_Port GPIOG
#define FES__FAULT_Pin GPIO_PIN_5
#define FES__FAULT_GPIO_Port GPIOG
#define IMU_PWR_EN_Pin GPIO_PIN_6
#define IMU_PWR_EN_GPIO_Port GPIOG
#define EXT_GPIO_1_Pin GPIO_PIN_3
#define EXT_GPIO_1_GPIO_Port GPIOF
#define FES_PWR_EN_Pin GPIO_PIN_4
#define FES_PWR_EN_GPIO_Port GPIOG
#define EMG__FAULT_Pin GPIO_PIN_3
#define EMG__FAULT_GPIO_Port GPIOG
#define EMG_PWR_EN_Pin GPIO_PIN_2
#define EMG_PWR_EN_GPIO_Port GPIOG
#define EXT_GPIO_3_Pin GPIO_PIN_5
#define EXT_GPIO_3_GPIO_Port GPIOF
#define EXT_GPIO_2_Pin GPIO_PIN_4
#define EXT_GPIO_2_GPIO_Port GPIOF
#define LED_DRV_SPI_SCK_Pin GPIO_PIN_0
#define LED_DRV_SPI_SCK_GPIO_Port GPIOK
#define LED_DRV_SPI_NSSA_Pin GPIO_PIN_1
#define LED_DRV_SPI_NSSA_GPIO_Port GPIOK
#define EXT_GPIO_4_Pin GPIO_PIN_6
#define EXT_GPIO_4_GPIO_Port GPIOF
#define EXT_GPIO_5_Pin GPIO_PIN_7
#define EXT_GPIO_5_GPIO_Port GPIOF
#define EXT_GPIO_6_Pin GPIO_PIN_8
#define EXT_GPIO_6_GPIO_Port GPIOF
#define LED_DRV_SPI_MISO_Pin GPIO_PIN_11
#define LED_DRV_SPI_MISO_GPIO_Port GPIOJ
#define EXT_GPIO_8_Pin GPIO_PIN_10
#define EXT_GPIO_8_GPIO_Port GPIOF
#define EXT_GPIO_7_Pin GPIO_PIN_9
#define EXT_GPIO_7_GPIO_Port GPIOF
#define LED_DRV_SPI_MOSI_Pin GPIO_PIN_10
#define LED_DRV_SPI_MOSI_GPIO_Port GPIOJ
#define MCU_PHY_MDC_Pin GPIO_PIN_1
#define MCU_PHY_MDC_GPIO_Port GPIOC
#define MCU_PHY_RST_Pin GPIO_PIN_2
#define MCU_PHY_RST_GPIO_Port GPIOC
#define MCU_PHY_MDIO_Pin GPIO_PIN_2
#define MCU_PHY_MDIO_GPIO_Port GPIOA
#define USB_ADC_IN1_Pin GPIO_PIN_13
#define USB_ADC_IN1_GPIO_Port GPIOF
#define USB_ADC_IN2_Pin GPIO_PIN_14
#define USB_ADC_IN2_GPIO_Port GPIOF
#define PSRAM_QSPI_NCSA_Pin GPIO_PIN_10
#define PSRAM_QSPI_NCSA_GPIO_Port GPIOB
#define MCU_RMII_TX_EN_Pin GPIO_PIN_11
#define MCU_RMII_TX_EN_GPIO_Port GPIOB
#define TP9_Pin GPIO_PIN_6
#define TP9_GPIO_Port GPIOA
#define MCU_RMII_CRS_DV_Pin GPIO_PIN_7
#define MCU_RMII_CRS_DV_GPIO_Port GPIOA
#define PSRAM_QSPI_CLKA_Pin GPIO_PIN_2
#define PSRAM_QSPI_CLKA_GPIO_Port GPIOB
#define EXT_ADC_4_Pin GPIO_PIN_12
#define EXT_ADC_4_GPIO_Port GPIOF
#define PSRAM_QSPI_IO0A_Pin GPIO_PIN_11
#define PSRAM_QSPI_IO0A_GPIO_Port GPIOD
#define PSRAM_QSPI_IO3A_Pin GPIO_PIN_13
#define PSRAM_QSPI_IO3A_GPIO_Port GPIOD
#define MCU_RMII_REF_CLK_Pin GPIO_PIN_1
#define MCU_RMII_REF_CLK_GPIO_Port GPIOA
#define USB_OC__DET_Pin GPIO_PIN_5
#define USB_OC__DET_GPIO_Port GPIOA
#define MCU_RMII_RXD0_Pin GPIO_PIN_4
#define MCU_RMII_RXD0_GPIO_Port GPIOC
#define EXT_ADC_2_Pin GPIO_PIN_1
#define EXT_ADC_2_GPIO_Port GPIOB
#define EXT_ADC_3_Pin GPIO_PIN_11
#define EXT_ADC_3_GPIO_Port GPIOF
#define HWREV_2_Pin GPIO_PIN_0
#define HWREV_2_GPIO_Port GPIOG
#define FDCAN2_RX_Pin GPIO_PIN_12
#define FDCAN2_RX_GPIO_Port GPIOB
#define LED_DRV_nOE_Pin GPIO_PIN_9
#define LED_DRV_nOE_GPIO_Port GPIOD
#define TP8_Pin GPIO_PIN_4
#define TP8_GPIO_Port GPIOA
#define MCU_RMII_RXD1_Pin GPIO_PIN_5
#define MCU_RMII_RXD1_GPIO_Port GPIOC
#define EXT_ADC_1_Pin GPIO_PIN_0
#define EXT_ADC_1_GPIO_Port GPIOB
#define HWREV_1_Pin GPIO_PIN_1
#define HWREV_1_GPIO_Port GPIOG
#define HWREV_0_Pin GPIO_PIN_7
#define HWREV_0_GPIO_Port GPIOE
#define FDCAN2_TX_Pin GPIO_PIN_13
#define FDCAN2_TX_GPIO_Port GPIOB
#define LED_DRV_nRESET_Pin GPIO_PIN_8
#define LED_DRV_nRESET_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
