/**
 ******************************************************************************
 * @file    xm_api_external_io.c
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api_external_io.h"
#include "external_io.h"
#include "system_startup.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * ============================================================================
 * [신규] 외부 확장 IO API (Arduino 스타일)
 * ============================================================================
 */

void XM_SetPinMode(XmDioPin_t pin, XmPinMode_t mode)
{
    // Facade: System Layer(external_io)로 변환 및 호출
    ExternalIO_SetPinMode(pin, (mode));
}

void XM_DigitalWrite(XmDioPin_t pin, XmLogicLevel_t level)
{
    ExternalIO_WritePin((ExternalDioPin_t)pin, (bool)level);
}

XmLogicLevel_t XM_DigitalRead(XmDioPin_t pin)
{
    return (XmLogicLevel_t)ExternalIO_ReadPin((ExternalDioPin_t)pin);
}

/**
 * ============================================================================
 * [신규] 아날로그 입력 API (통합 + Resolution 정규화)
 * ============================================================================
 */

uint16_t XM_AnalogRead(XmAdcPin_t pin)
{
    return ExternalIO_ReadAdc((ExternalAdcPin_t)pin);
}

void XM_SetAnalogReadResolution(uint8_t bits)
{
    ExternalIO_SetOutputResolution(bits);
}

uint8_t XM_GetAnalogResolution(XmAdcPin_t pin)
{
    return ExternalIO_GetNativeResolution((ExternalAdcPin_t)pin);
}

uint8_t XM_GetAnalogReadResolution(void)
{
    return ExternalIO_GetOutputResolution();
}

uint16_t XM_AnalogReadMillivolts(XmAdcPin_t pin)
{
    return ExternalIO_ReadAdcMillivolts((ExternalAdcPin_t)pin);
}

bool XM_IsDioSwitchedToAdc(XmDioPin_t pin)
{
    return ExternalIO_IsDioSwitchedToAdc3((ExternalDioPin_t)pin);
}

void XM_SetExtPowerVoltage(XmExtPwrVoltage_t voltage)
{
    IOIF_GPIOx_t id = System_GetExtPwrEnGpioId();

    if (voltage == XM_EXT_PWR_5V) {
        IOIF_GPIO_SET(id);
    } else {
        IOIF_GPIO_RESET(id);
    }
}

/**
 * ============================================================================
 * External GPIO 중 ADC Pin(PA0, PA1) -> UART4로 동적 전환 (XM10에서 XSENS IMU를 사용하기 위함)
 * ============================================================================
 */

bool XM_EnableExternalImu(void)
{
    // ✅ Facade: System Layer의 External IO로 완전히 위임
    return ExternalIO_SwitchToUartMode();
}

/**
 * ============================================================================
 * [신규] DIO → ADC 동적 전환 API
 * ============================================================================
 */

/**
 * @brief [런타임] DIO 핀을 ADC3 모드로 전환합니다.
 * @details ✅ DIO 5~8(PF7~PF10)을 아날로그 입력으로 사용할 수 있게 함
 * - 전환 후 XM_AnalogRead(XM_ADC_5~12)로 읽기 가능
 * - 16-bit 고해상도, 10kHz 샘플링
 * 
 * @param[in] pin DIO 핀 번호 (XM_DIO_1 ~ XM_DIO_8)
 * @return true: 성공, false: 실패
 * 
 * @note
 * - ⚠️ 전환 후 해당 DIO 핀은 디지털 IO로 사용 불가
 * - ✅ 여러 핀 동시 전환 가능 (최대 8개)
 */
bool XM_SwitchDioToAdc(XmDioPin_t pin)
{
    return ExternalIO_SwitchDioToAdc3((ExternalDioPin_t)pin);
}

bool XM_SwitchAllDioToAdc(void)
{
    bool all_ok = true;
    for (int i = 0; i < XM_EXT_DIO_COUNT; i++) {
        if (!ExternalIO_SwitchDioToAdc3((ExternalDioPin_t)i)) {
            all_ok = false;
        }
    }
    return all_ok;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */
