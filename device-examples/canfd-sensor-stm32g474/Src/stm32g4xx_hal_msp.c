/**
 * @file    stm32g4xx_hal_msp.c
 * @brief   주변장치의 클럭 · 핀 — FDCAN1(PA11 · PA12) · ADC1(PA0 · PA1)
 */

#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef* hfdcan)
{
    if (hfdcan->Instance != FDCAN1) {
        return;
    }
    /* FDCAN 클럭 = PLLQ 80 MHz — XM10 과 같은 클럭이라 비트 타이밍 값을 그대로 씁니다 */
    RCC_PeriphCLKInitTypeDef clk = {0};
    clk.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    clk.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
        Error_Handler();
    }
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA11 = FDCAN1_RX, PA12 = FDCAN1_TX. PB8 · PB9 로도 쓸 수 있지만 PB8 은 부팅 핀(BOOT0)을 겸해서 피했습니다 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = CAN_RX_Pin | CAN_TX_Pin;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(CAN_GPIO_Port, &gpio);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance != ADC1) {
        return;
    }
    RCC_PeriphCLKInitTypeDef clk = {0};
    clk.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
    clk.Adc12ClockSelection  = RCC_ADC12CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
        Error_Handler();
    }
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = ADC0_Pin | ADC1_Pin;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ADC_GPIO_Port, &gpio);
}
