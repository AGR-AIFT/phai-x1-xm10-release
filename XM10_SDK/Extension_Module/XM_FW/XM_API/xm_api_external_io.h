/**
 ******************************************************************************
 * @file    xm_api_external_io.h
 * @author  HyundoKim
 * @brief   외부 확장 포트(Extension Port) GPIO 및 ADC 제어 API
 * @details 
 * 보드 측면의 확장 핀을 통해 디지털 입출력 및 아날로그 센서 값을 읽을 수 있습니다.
 * * @warning [자원 충돌 주의]
 * - IMU 모듈을 활성화한 경우, 특정 핀(예: XM_PIN_1, XM_PIN_2)은 UART 통신용으로 
 * 자동 점유되므로 GPIO/ADC로 사용할 수 없습니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_EXTERNAL_IO_H_
#define XM_API_XM_API_EXTERNAL_IO_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 확장 포트의 디지털 핀 ID (DIO)
 */
typedef enum {
    XM_EXT_DIO_1 = 0, // PF3
    XM_EXT_DIO_2,     // PF4
    XM_EXT_DIO_3,     // PF5
    XM_EXT_DIO_4,     // PF6
    XM_EXT_DIO_5,     // PF7
    XM_EXT_DIO_6,     // PF8
    XM_EXT_DIO_7,     // PF9
    XM_EXT_DIO_8,     // PF10
    XM_EXT_DIO_COUNT
} XmDioPin_t;

/**
 * @brief 확장 포트의 아날로그 핀 ID (ADC)
 */
typedef enum {
    XM_EXT_ADC_1 = 0, // PA0 [Shared] ADC / UART4_TX (IMU 사용 시 GPIO 불가)
    XM_EXT_ADC_2,     // PA0_C
    XM_EXT_ADC_3,     // PA1 [Shared] ADC / UART4_RX (IMU 사용 시 GPIO 불가)
    XM_EXT_ADC_4,     // PA1_C
    XM_EXT_ADC_COUNT
} XmAdcPin_t;

/**
 * @brief 디지털 핀의 모드
 */
typedef enum {
    XM_EXT_DIO_MODE_INPUT,           /**< 디지털 입력 (Floating) */
    XM_EXT_DIO_MODE_INPUT_PULLUP,    /**< 디지털 입력 (내부 Pull-up 저항) */
    XM_EXT_DIO_MODE_INPUT_PULLDOWN,  /**< 디지털 입력 (내부 Pull-down 저항) */
    XM_EXT_DIO_MODE_OUTPUT           /**< 디지털 출력 */
} XmPinMode_t;

/**
 * @brief 디지털 논리 레벨 (Digital Logic Level)
 * @note  0/1 대신 이 상수를 사용하여 가독성을 높이세요.
 */
typedef enum {
    XM_LOW  = 0, /**< 0V (GND) */
    XM_HIGH = 1  /**< 3.3V (VCC) */
} XmLogicLevel_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * ============================================================================
 * 외부 확장 IO API 
 * ============================================================================
 */

/**
 * @brief [비실시간] 디지털 핀의 모드(입력/출력/풀업/풀다운)를 설정합니다.
 * @warning 2ms 실시간 루프 안에서 호출하지 마십시오. (HAL_GPIO_Init 호출로 인한 지연)
 * @param[in] pin   설정할 핀 (D0 ~ D7)
 * @param[in] mode  설정할 모드 (XM_INPUT, XM_OUTPUT 등)
 */
void XM_SetPinMode(XmDioPin_t pin, XmPinMode_t mode);

/**
 * @brief  디지털 핀에 전압을 출력합니다.
 * @param  pin 핀 번호 (XM_PIN_x)
 * @param  level 출력 레벨 (XM_HIGH / XM_LOW)
 * @code
 * DigitalWrite(XM_PIN_1, XM_HIGH); // "핀 1번을 High로 만들어라"
 * @endcode
 */
void XM_DigitalWrite(XmDioPin_t pin, XmLogicLevel_t level);

/**
 * @brief  디지털 핀의 전압 상태를 읽습니다.
 * @param  pin 핀 번호
 * @return XM_HIGH 또는 XM_LOW
 * @code
 * if (DigitalRead(XM_PIN_2) == XM_HIGH) { ... }
 * @endcode
 */
XmLogicLevel_t XM_DigitalRead(XmDioPin_t pin);

/**
 * @brief 아날로그 핀의 전압 값을 읽어옵니다.
 * @param[in] pin 읽을 핀 (A0 ~ A3)
 * @return 12비트 ADC 값 (0 ~ 4095) (16비트일 경우 0 ~ 65535)
 */
uint16_t XM_AnalogRead(XmAdcPin_t pin);

/**
 * ============================================================================
 * External GPIO 중 ADC Pin(PA0, PA1) -> UART4로 동적 전환 (XM10에서 XSENS IMU를 사용하기 위함)
 * ============================================================================
 */

/**
 * @brief [초기화] 확장 포트(PA0, PA1)를 외부 IMU(Xsens) 통신용으로 설정합니다.
 * @details 기본적으로 이 포트는 ADC로 설정되어 있습니다. 
 * 이 함수를 호출하면 ADC 기능이 중지되고, UART(921600bps)로 재설정되어 IMU와 통신을 시작합니다.
 * * @warning 이 함수는 시스템 부팅 후 초기화 단계(InitUserAlgorithm)에서 한 번만 호출하는 것을 권장합니다.
 * @return 설정 변경 및 IMU 연결 성공 시 true.
 */
bool XM_EnableExternalImu(void);

#endif /* XM_API_XM_API_EXTERNAL_IO_H_ */
