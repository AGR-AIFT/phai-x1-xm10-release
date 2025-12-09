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

uint16_t XM_AnalogRead(XmAdcPin_t pin)
{
    return ExternalIO_ReadAdc(pin);
}

/**
 * ============================================================================
 * External GPIO 중 ADC Pin(PA0, PA1) -> UART4로 동적 전환 (XM10에서 XSENS IMU를 사용하기 위함)
 * ============================================================================
 */

bool XM_EnableExternalImu(void)
{
    // Facade: 복잡한 하드웨어 재설정 로직은 System Layer에 위임하고, 결과만 전달
    return System_Switch_To_IMU_Mode();
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */
