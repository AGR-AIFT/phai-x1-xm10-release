/**
 * @file    main.h
 * @brief   NUCLEO-G474RE — XM10 Ex.45 의 상대 장치(참조 구현)
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx_hal.h"

/* 보드 핀 — NUCLEO-G474RE */
#define LD2_Pin          GPIO_PIN_5      /**< 초록 LED LD2 — XM10 이 보낸 LED 명령 */
#define LD2_GPIO_Port    GPIOA
#define CAN_RX_Pin       GPIO_PIN_11     /**< FDCAN1_RX → 트랜시버 RXD */
#define CAN_TX_Pin       GPIO_PIN_12     /**< FDCAN1_TX → 트랜시버 TXD */
#define CAN_GPIO_Port    GPIOA
#define ADC0_Pin         GPIO_PIN_0      /**< ADC1 채널 1 → 주기 데이터의 ADC 0 */
#define ADC1_Pin         GPIO_PIN_1      /**< ADC1 채널 2 → 주기 데이터의 ADC 1 */
#define ADC_GPIO_Port    GPIOA

void Error_Handler(void);

#endif /* MAIN_H_ */
