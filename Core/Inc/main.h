/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define R_PMMG4_SPI_CS_A_Pin GPIO_PIN_4
#define R_PMMG4_SPI_CS_A_GPIO_Port GPIOE
#define L_PMMG1_SPI_CS_A_Pin GPIO_PIN_10
#define L_PMMG1_SPI_CS_A_GPIO_Port GPIOG
#define R_PMMG3_SPI_CS_A_Pin GPIO_PIN_15
#define R_PMMG3_SPI_CS_A_GPIO_Port GPIOA
#define R_FSR_1_ADC_Pin GPIO_PIN_3
#define R_FSR_1_ADC_GPIO_Port GPIOF
#define R_FSR_3_ADC_Pin GPIO_PIN_5
#define R_FSR_3_ADC_GPIO_Port GPIOF
#define L_EMG_1_ADC_Pin GPIO_PIN_7
#define L_EMG_1_ADC_GPIO_Port GPIOF
#define R_FSR_2_ADC_Pin GPIO_PIN_4
#define R_FSR_2_ADC_GPIO_Port GPIOF
#define R_FSR_4_ADC_Pin GPIO_PIN_6
#define R_FSR_4_ADC_GPIO_Port GPIOF
#define L_EMG_3_ADC_Pin GPIO_PIN_9
#define L_EMG_3_ADC_GPIO_Port GPIOF
#define R_EMG_3_ADC_Pin GPIO_PIN_13
#define R_EMG_3_ADC_GPIO_Port GPIOF
#define L_PMMG1_SPI_CS_B_Pin GPIO_PIN_8
#define L_PMMG1_SPI_CS_B_GPIO_Port GPIOG
#define L_EMG_4_ADC_Pin GPIO_PIN_10
#define L_EMG_4_ADC_GPIO_Port GPIOF
#define L_EMG_2_ADC_Pin GPIO_PIN_8
#define L_EMG_2_ADC_GPIO_Port GPIOF
#define R_EMG_4_ADC_Pin GPIO_PIN_14
#define R_EMG_4_ADC_GPIO_Port GPIOF
#define EXTI_BUTTON_Pin GPIO_PIN_8
#define EXTI_BUTTON_GPIO_Port GPIOE
#define EXTI_BUTTON_EXTI_IRQn EXTI9_5_IRQn
#define L_FSR_1_ADC_Pin GPIO_PIN_0
#define L_FSR_1_ADC_GPIO_Port GPIOC
#define L_FSR_2_ADC_Pin GPIO_PIN_1
#define L_FSR_2_ADC_GPIO_Port GPIOC
#define MUX_A_Pin GPIO_PIN_9
#define MUX_A_GPIO_Port GPIOE
#define BUZZER_Pin GPIO_PIN_14
#define BUZZER_GPIO_Port GPIOE
#define LED_BLUE_Pin GPIO_PIN_13
#define LED_BLUE_GPIO_Port GPIOD
#define R_PMMG4_SPI_CS_B_Pin GPIO_PIN_15
#define R_PMMG4_SPI_CS_B_GPIO_Port GPIOD
#define R_PMMG3_SPI_CS_B_Pin GPIO_PIN_14
#define R_PMMG3_SPI_CS_B_GPIO_Port GPIOD
#define L_FSR_4_ADC_Pin GPIO_PIN_3
#define L_FSR_4_ADC_GPIO_Port GPIOC
#define L_FSR_3_ADC_Pin GPIO_PIN_2
#define L_FSR_3_ADC_GPIO_Port GPIOC
#define MUX_S3_Pin GPIO_PIN_13
#define MUX_S3_GPIO_Port GPIOE
#define L_PMMG2_SPI_CS_B_Pin GPIO_PIN_5
#define L_PMMG2_SPI_CS_B_GPIO_Port GPIOH
#define MUX_S2_Pin GPIO_PIN_12
#define MUX_S2_GPIO_Port GPIOE
#define R_EMG_1_ADC_Pin GPIO_PIN_11
#define R_EMG_1_ADC_GPIO_Port GPIOF
#define MUX_B_Pin GPIO_PIN_10
#define MUX_B_GPIO_Port GPIOE
#define R_EMG_2_ADC_Pin GPIO_PIN_12
#define R_EMG_2_ADC_GPIO_Port GPIOF
#define MUX_S1_Pin GPIO_PIN_11
#define MUX_S1_GPIO_Port GPIOE
#define FP_SYNC_Pin GPIO_PIN_15
#define FP_SYNC_GPIO_Port GPIOE
#define L_PMMG2_SPI_CS_A_Pin GPIO_PIN_12
#define L_PMMG2_SPI_CS_A_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
