/**
 ******************************************************************************
 * @file    external_io.h
 * @author  HyundoKim
 * @brief   [System Layer] 외부 확장 IO 핀 관리
 * @details system_startup으로부터 IOIF 핸들을 주입받아 저장하고,
 * Facade Layer(xm_api)에 Arduino 스타일의 API를 제공합니다.
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_BOARD_GPIO_EXTERNAL_IO_H_
#define SYSTEM_BOARD_GPIO_EXTERNAL_IO_H_

#include "ioif_agrb_gpio.h"
#include "ioif_agrb_adc.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief xm_api.h와 공유하는 디지털 핀 ID
 */
typedef enum {
    EXT_DIO_1 = 0, // PF3
    EXT_DIO_2,     // PF4
    EXT_DIO_3,     // PF5
    EXT_DIO_4,     // PF6
    EXT_DIO_5,     // PF7
    EXT_DIO_6,     // PF8
    EXT_DIO_7,     // PF9
    EXT_DIO_8,     // PF10
    EXT_DIO_COUNT
} ExternalDioPin_t;

/**
 * @brief xm_api.h와 공유하는 아날로그 핀 ID
 */
typedef enum {
    EXT_ADC_1 = 0, // PA0 [Shared] ADC / UART4_TX (IMU 사용 시 GPIO 불가)
    EXT_ADC_2,     // PA0_C
    EXT_ADC_3,     // PA1 [Shared] ADC / UART4_RX (IMU 사용 시 GPIO 불가)
    EXT_ADC_4,     // PA1_C
    EXT_ADC_COUNT
} ExternalAdcPin_t;

/**
 * @brief xm_api.h와 공유하는 핀 모드
 */
typedef enum {
    EXT_IO_MODE_INPUT,
    EXT_IO_MODE_INPUT_PULLUP,
    EXT_IO_MODE_INPUT_PULLDOWN,
    EXT_IO_MODE_OUTPUT
} ExternalPinMode_t;

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief 외부 IO 모듈을 초기화합니다.
 * @details system_startup에서 GPIO/ADC의 IOIF ID를 주입받습니다.
 * @param[in] dio_ids 8개 DIO 핀의 IOIF_GPIOx_t ID 배열
 * @param[in] adc1_id ADC1(CC + A0/A1) 그룹의 IOIF_ADCx_t ID
 * @param[in] adc2_id ADC2(A2/A3) 그룹의 IOIF_ADCx_t ID
 */
void ExternalIO_Init(IOIF_GPIOx_t dio_ids[EXT_DIO_COUNT], IOIF_ADCx_t adc1_id, IOIF_ADCx_t adc2_id);

/**
 * @brief [Facade용] 디지털 핀의 모드를 설정합니다.
 */
void ExternalIO_SetPinMode(ExternalDioPin_t pin, ExternalPinMode_t mode);

/**
 * @brief [Facade용] 디지털 핀의 상태를 씁니다.
 */
void ExternalIO_WritePin(ExternalDioPin_t pin, bool state);

/**
 * @brief [Facade용] 디지털 핀의 상태를 읽습니다.
 */
bool ExternalIO_ReadPin(ExternalDioPin_t pin);

/**
 * @brief [Facade용] 아날로그 핀의 값을 읽습니다.
 * @details ADC 그룹 전체를 읽은 후, 요청한 핀의 값을 반환합니다.
 */
uint16_t ExternalIO_ReadAdc(ExternalAdcPin_t pin);

#endif /* SYSTEM_BOARD_GPIO_EXTERNAL_IO_H_ */
