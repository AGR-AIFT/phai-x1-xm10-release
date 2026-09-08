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

#include "module.h"     /* XM_EXTERNAL_UART_XSENS_ENABLE — External UART 소유권 게이트 */

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief DIO 핀 번호를 대응하는 ADC 핀 번호로 변환합니다.
 * @details DIO→ADC 전환 후 XM_AnalogRead()에 사용할 ADC 핀을 직관적으로 얻을 수 있습니다.
 * 
 * @param dio DIO 핀 번호 (XM_EXT_DIO_1 ~ XM_EXT_DIO_8)
 * @return 대응하는 ADC 핀 번호 (XM_EXT_ADC_5 ~ XM_EXT_ADC_12)
 * 
 * @code
 * XM_SwitchDioToAdc(XM_EXT_DIO_1);
 * uint16_t val = XM_AnalogRead(XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1));  // == XM_EXT_ADC_5
 * @endcode
 */
#define XM_DIO_TO_ADC_PIN(dio)  ((XmAdcPin_t)((dio) + XM_EXT_ADC_5))

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
 * @details ✅ ADC1/2/3 통합: DIO 핀도 ADC로 사용 가능
 */
typedef enum {
    /* ADC1 고정 핀 (Rev2.0: 전부 ADC1 16-bit, 항상 사용 가능) */
    XM_EXT_ADC_1 = 0, // PB0  (ADC1_INP9)
    XM_EXT_ADC_2,     // PB1  (ADC1_INP5)
    XM_EXT_ADC_3,     // PF11 (ADC1_INP2)
    XM_EXT_ADC_4,     // PF12 (ADC1_INP6)
    
    /* ADC3 동적 핀 (DIO → ADC 전환 필요) */
    XM_EXT_ADC_5,     // PF3 (DIO 1 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    XM_EXT_ADC_6,     // PF4 (DIO 2 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    XM_EXT_ADC_7,     // PF5 (DIO 3 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    XM_EXT_ADC_8,     // PF6 (DIO 4 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    XM_EXT_ADC_9,     // PF7 (DIO 5 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    XM_EXT_ADC_10,    // PF8 (DIO 6 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    XM_EXT_ADC_11,    // PF9 (DIO 7 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    XM_EXT_ADC_12,    // PF10 (DIO 8 → ADC3, XM_SwitchDioToAdc() 호출 필요)
    
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
 * @brief 확장 포트 전원 전압 선택 (EXT_PWR_SEL_5V, PE3)
 */
typedef enum {
    XM_EXT_PWR_3V3 = 0, /**< 3.3V 출력 (기본값, Low) */
    XM_EXT_PWR_5V  = 1  /**< 5V 출력 (High) */
} XmExtPwrVoltage_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */

/**
 * @brief [진단] ADC3 그룹 핀을 XM_SwitchDioToAdc() 전환 없이 읽은 이력 (핀별 bit).
 * @details
 *  bit0 = XM_EXT_ADC_5(DIO_1) … bit7 = XM_EXT_ADC_12(DIO_8). 0 이면 모든 ADC read 정상.
 *
 *  ADC3 그룹(XM_EXT_ADC_5~12)은 XM_SwitchDioToAdc() 전환 후에만 유효하다. 전환 없이
 *  XM_AnalogRead()/XM_AnalogReadMillivolts() 를 호출하면 하위 계층이 0(유효한 0V 와
 *  구분 불가)을 반환하는데, 그 상황을 채널 독립적으로 이 비트마스크에 기록한다.
 *  → USB 미사용·SWD-only·임의 GUI 어디서든 STM32CubeIDE Live Expressions 로 관찰하거나
 *    사용자 코드에서 읽어 분기할 수 있다 (특정 출력 채널에 의존하지 않음).
 *
 * @code
 * // ADC read 값이 계속 0 이라 원인을 확인하고 싶을 때:
 * if (g_xm_adc_read_before_switch != 0U) {
 *     // 전환 없이 읽은 ADC3 핀이 있음 → 해당 핀의 XM_SwitchDioToAdc() 호출 누락 확인
 *     // (bit0 = ADC_5 … bit7 = ADC_12)
 * }
 * @endcode
 */
extern volatile uint16_t g_xm_adc_read_before_switch;


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
 * @brief [통합 API] 아날로그 핀의 전압 값을 읽어옵니다 (정규화됨).
 * @details 
 * - ADC1/2/3 자동 판별 - 사용자는 핀 번호만 지정하면 됨
 * - 모든 핀의 반환값은 동일한 출력 resolution으로 정규화됨 (기본 16-bit)
 * - XM_EXT_ADC_1~4: ADC1/2 그룹 (항상 사용 가능)
 * - XM_EXT_ADC_5~12: ADC3 그룹 (XM_SwitchDioToAdc() 호출 후 사용 가능)
 * 
 * @param[in] pin 읽을 핀 (XM_EXT_ADC_1 ~ XM_EXT_ADC_12)
 * @return 정규화된 ADC 값 (기본: 0~65535), 에러 시 0
 * 
 * @note
 * - 출력 resolution은 XM_SetAnalogReadResolution()으로 변경 가능
 * - 12-bit 핀(ADC1)과 16-bit 핀(ADC2/3)이 동일한 범위로 반환됨
 * 
 * @code
 * void InitUserAlgorithm(void) {
 *     XM_SetAnalogReadResolution(12);  // 12-bit 모드 (0~4095)
 *     XM_SwitchDioToAdc(XM_EXT_DIO_1);
 *     XM_SwitchDioToAdc(XM_EXT_DIO_2);
 * }
 * 
 * void RunUserAlgorithm(void) {
 *     uint16_t fsr1 = XM_AnalogRead(XM_EXT_ADC_5);  // 0~4095
 *     uint16_t fsr2 = XM_AnalogRead(XM_EXT_ADC_6);  // 0~4095
 * }
 * @endcode
 */
uint16_t XM_AnalogRead(XmAdcPin_t pin);

/**
 * @brief [설정] ADC 출력 resolution을 설정합니다 (Arduino analogReadResolution 호환).
 * @details 
 * - XM_AnalogRead()의 반환값 범위를 변경합니다.
 * - 하드웨어 ADC resolution은 변경하지 않습니다 (소프트웨어 정규화).
 * - 네이티브보다 높은 resolution: upscale (좌측 shift)
 * - 네이티브보다 낮은 resolution: downscale (우측 shift)
 * 
 * @param[in] bits 출력 resolution (8, 10, 12, 14, 16)
 * 
 * @note 기본값: 16 (0~65535)
 * 
 * @code
 * XM_SetAnalogReadResolution(12);  // 모든 AnalogRead → 0~4095
 * XM_SetAnalogReadResolution(10);  // 모든 AnalogRead → 0~1023
 * XM_SetAnalogReadResolution(16);  // 모든 AnalogRead → 0~65535 (기본)
 * @endcode
 */
void XM_SetAnalogReadResolution(uint8_t bits);

/**
 * @brief [조회] 아날로그 핀의 네이티브 하드웨어 resolution을 반환합니다.
 * @details 실제 하드웨어 ADC가 몇 bit로 동작하는지 확인할 때 사용합니다.
 * 
 * @param[in] pin 아날로그 핀 번호 (XM_EXT_ADC_1 ~ XM_EXT_ADC_12)
 * @return 네이티브 resolution (bit 수, 예: 12, 16), 잘못된 핀이면 0
 * 
 * @code
 * uint8_t res = XM_GetAnalogResolution(XM_EXT_ADC_1);  // 12 (ADC1, 12-bit)
 * uint8_t res = XM_GetAnalogResolution(XM_EXT_ADC_2);  // 16 (ADC2, 16-bit)
 * @endcode
 */
uint8_t XM_GetAnalogResolution(XmAdcPin_t pin);

/**
 * @brief [조회] 현재 설정된 ADC 출력 resolution을 반환합니다.
 * @details XM_SetAnalogReadResolution()으로 설정한 현재 값을 확인합니다.
 * 
 * @return 현재 출력 resolution (bit 수, 기본값 16)
 * 
 * @code
 * XM_SetAnalogReadResolution(12);
 * uint8_t res = XM_GetAnalogReadResolution();  // 12
 * @endcode
 */
uint8_t XM_GetAnalogReadResolution(void);

/**
 * @brief [통합 API] 아날로그 핀의 전압을 밀리볼트(mV) 단위로 읽습니다.
 * @details 
 * - 네이티브 raw 값에서 직접 변환하므로 SetAnalogReadResolution 설정에 영향받지 않습니다.
 * - VREF = 3.3V 기준: 반환값 범위 0 ~ 3300 mV
 * - 수동 변환 `3.3f * raw / 65535.0f` 대신 이 API를 사용하면 직관적이고 오류가 줄어듭니다.
 * 
 * @param[in] pin 읽을 핀 (XM_EXT_ADC_1 ~ XM_EXT_ADC_12)
 * @return 밀리볼트 단위 전압 (0 ~ 3300), 에러 시 0
 * 
 * @code
 * uint16_t mv = XM_AnalogReadMillivolts(XM_EXT_ADC_1);
 * // mv == 1650이면 1.65V
 * float voltage = mv / 1000.0f;  // 1.65
 * @endcode
 */
uint16_t XM_AnalogReadMillivolts(XmAdcPin_t pin);

/**
 * @brief [조회] DIO 핀이 ADC 모드로 전환되었는지 확인합니다.
 * @param[in] pin DIO 핀 (XM_EXT_DIO_1 ~ XM_EXT_DIO_8)
 * @return true: ADC 모드, false: GPIO 모드 (또는 잘못된 핀)
 * 
 * @code
 * XM_SwitchDioToAdc(XM_EXT_DIO_1);
 * if (XM_IsDioSwitchedToAdc(XM_EXT_DIO_1)) {
 *     // ADC 모드 - XM_AnalogRead() 사용 가능
 * }
 * @endcode
 */
bool XM_IsDioSwitchedToAdc(XmDioPin_t pin);

/**
 * ============================================================================
 * [Rev2.0] 확장 포트 전원 전압 선택 API (3.3V / 5V)
 * ============================================================================
 */

/**
 * @brief [비실시간] 확장 포트의 공급 전압을 3.3V 또는 5V로 전환합니다.
 * @details
 * - PE3(EXT_PWR_SEL_5V) GPIO를 제어하여 전원 MUX를 전환합니다.
 * - 기본값은 3.3V (Low)이며, 5V 센서 사용 시 XM_EXT_PWR_5V로 전환하세요.
 *
 * @param[in] voltage 선택할 전압 (XM_EXT_PWR_3V3 또는 XM_EXT_PWR_5V)
 *
 * @warning 전환 시 확장 포트에 연결된 외부 디바이스의 전압 규격을 반드시 확인하세요.
 *
 * @code
 * void InitUserAlgorithm(void) {
 *     XM_SetExtPowerVoltage(XM_EXT_PWR_5V);   // 5V 센서 사용
 * }
 * @endcode
 */
void XM_SetExtPowerVoltage(XmExtPwrVoltage_t voltage);

/**
 * ============================================================================
 * [Rev2.0] External UART (USART2, PD5/PD6) API
 *
 * 이 포트는 921600 8N1, DMA + IDLE 라인 이벤트로 부팅 시 이미 열려 있습니다.
 * 주인은 `module.h` 의 `XM_EXTERNAL_UART_XSENS_ENABLE` 이 빌드 타임에 정합니다:
 *
 *   0 (기본) → 범용 Serial API. 아래 XM_AttachExternalUart() 등을 씁니다.
 *   1        → Xsens MTi-630 전용. 아래 XM_AttachXsensMTi630() 등을 씁니다.
 *
 * 두 그룹은 **동시에 선언되지 않습니다.** IOIF 가 포트당 RX 콜백을 1개만 갖고
 * 나중 등록이 앞의 것을 경고 없이 덮어쓰기 때문에, 빌드 타임에 하나만 고르게
 * 막아 둔 것입니다. 반대쪽 함수를 부르면 "선언되지 않은 함수" 컴파일 에러가
 * 납니다 — 그럴 때는 module.h 의 매크로 값을 확인하세요.
 * ============================================================================
 */

#if !XM_EXTERNAL_UART_XSENS_ENABLE

/**
 * @brief External UART 통신 속도.
 * @note 상대 장비와 **같은 값**이어야 통신됩니다. 8N1(데이터 8비트/패리티 없음/
 *       스톱 1비트)은 **고정이며 바꿀 수 없습니다** — 상대 장비를 8N1 로 맞추세요.
 */
typedef enum {
    XM_UART_BAUD_9600 = 0,
    XM_UART_BAUD_19200,
    XM_UART_BAUD_38400,
    XM_UART_BAUD_57600,
    XM_UART_BAUD_115200,
    XM_UART_BAUD_230400,
    XM_UART_BAUD_460800,
    XM_UART_BAUD_921600,    /**< 부팅 기본값 */
} XmUartBaudrate_t;

/**
 * @brief XM_SendExternalUartData() 1회 최대 바이트. 넘으면 나눠 보내세요.
 * @note 내부 송신 버퍼 크기와 같아야 하며, 어긋나면 xm_api_external_io.c 의
 *       컴파일 타임 검사(_Static_assert)에서 빌드가 멈춥니다.
 */
#define XM_EXT_UART_TX_MAX_BYTES   (128U)

/**
 * @brief External UART 수신 콜백 타입.
 * @param[in] data 수신된 바이트들
 * @param[in] len  이번 호출의 바이트 수
 *
 * @warning **여기서는 복사만 하고 즉시 리턴하세요.** 이 콜백은 XM10 내부의 공유
 *          수신 태스크에서 실행되고, 그 태스크는 발바닥 센서(GRF)의 1kHz 수신도
 *          함께 처리합니다. 여기서 오래 걸리면 발바닥 센서 데이터가 밀립니다.
 *          파싱·계산은 Control_Loop() 에서 하세요.
 * @warning `data` 포인터는 **콜백이 끝나면 무효**입니다. 포인터를 저장하지 말고
 *          내용을 자기 버퍼로 복사하세요.
 * @warning **콜백 1번 = 메시지 1개가 아닙니다.** 수신은 최대 128바이트 조각으로
 *          쪼개져 여러 번 올 수 있고, 한 번에 두 메시지가 붙어 올 수도 있습니다.
 *          메시지 경계(헤더/길이/체크섬)는 직접 찾아야 합니다.
 */
typedef void (*XmExternalUartRxFunc_t)(const uint8_t* data, uint32_t len);

/**
 * @brief External UART 수신을 시작하고 콜백을 등록합니다.
 * @param[in] rx_callback 수신 콜백. NULL 을 주면 수신을 해제합니다.
 * @return true=등록 성공 / false=실패(포트 미준비)
 * @note Control_Setup() 에서 1회 호출하세요.
 *
 * @code
 * static volatile uint8_t s_rx[256];
 * static volatile uint16_t s_rx_len;
 *
 * static void OnSerialRx(const uint8_t* data, uint32_t len) {
 *     for (uint32_t i = 0; i < len && s_rx_len < sizeof(s_rx); i++) {
 *         s_rx[s_rx_len++] = data[i];      // 복사만!
 *     }
 * }
 *
 * void Control_Setup(void) {
 *     XM_SetExternalUartBaudrate(XM_UART_BAUD_115200);
 *     XM_AttachExternalUart(OnSerialRx);
 * }
 * @endcode
 */
bool XM_AttachExternalUart(XmExternalUartRxFunc_t rx_callback);

/**
 * @brief 통신 속도를 바꿉니다 (상대 장비와 같은 값이어야 합니다).
 * @return true=성공 / false=실패(포트 미준비)
 * @note Control_Setup() 에서 XM_AttachExternalUart() 보다 먼저 호출하세요.
 */
bool XM_SetExternalUartBaudrate(XmUartBaudrate_t baud);

/**
 * @brief 데이터를 보냅니다 (논블로킹).
 * @param[in] data 보낼 데이터
 * @param[in] len  바이트 수 (1 ~ XM_EXT_UART_TX_MAX_BYTES)
 * @return true=송신 시작됨 / false=직전 송신이 아직 안 끝났거나 인자 오류
 *
 * @note **false 는 에러가 아니라 "지금은 바쁘다"입니다.** 1kHz Control_Loop() 에서
 *       매 틱 호출해도 안전하며, false 면 다음 틱에 다시 시도하면 됩니다.
 * @note 길이가 XM_EXT_UART_TX_MAX_BYTES 를 넘으면 보내지 않고 false 를 반환합니다.
 *       긴 데이터는 여러 번에 나눠 보내세요.
 */
bool XM_SendExternalUartData(const void* data, uint32_t len);

/**
 * @brief 데이터를 보냅니다 (송신 완료까지 대기).
 * @warning **Control_Setup() 전용입니다.** 최대 5초까지 멈출 수 있어서
 *          Control_Loop() 이나 수신 콜백 안에서 호출하면 제어 주기가 깨집니다.
 *          초기 설정 커맨드를 한 번 보낼 때만 쓰세요.
 */
bool XM_SendExternalUartDataBlocking(const void* data, uint32_t len);

/**
 * @brief 수신이 멈춰 있으면 되살립니다 (안전장치).
 * @details 케이블을 뽑았다 꽂거나 노이즈로 프레이밍 에러가 나면 하드웨어 수신이
 *          정지한 채로 남을 수 있습니다. 이 함수를 주기적으로 부르면 그런 경우에
 *          자동으로 복구됩니다. 정상일 때는 아무 일도 하지 않습니다.
 * @note 100ms 정도 주기로 호출하는 것을 권장합니다 (예제 43 참고).
 */
void XM_EnsureExternalUartRxArmed(void);

#else  /* XM_EXTERNAL_UART_XSENS_ENABLE == 1 */

/**
 * @brief Xsens MTi-630 IMU를 External UART(USART2)에 결합합니다.
 * @details
 *  - Control_Setup()에서 1회 호출. 호출 후 XM.status.ext_imu.* 로 데이터 접근.
 *  - 센서 미연결 상태에서도 호출 안전. 케이블 결합 시 자동 OPERATIONAL 전환.
 *  - 미호출 시 USART2 RX 콜백이 비어 있어 Xsens 데이터가 채워지지 않음.
 */
void XM_AttachXsensMTi630(void);

/**
 * @brief Xsens MTi-630 Output Configuration 1회 송신 (1kHz Quat+Acc+Gyro).
 * @details
 *  - 신품/공장 초기화 센서에만 필요. EEPROM에 설정 보존된 센서면 호출 불필요.
 *  - 블로킹 ~1초. Control_Setup() 또는 별도 명시 시점에 호출 (1kHz 루프 내 금지).
 *  - XM_AttachXsensMTi630() 선행 호출 필수.
 */
void XM_ConfigureXsensMTi630(void);

#endif /* XM_EXTERNAL_UART_XSENS_ENABLE */

/**
 * ============================================================================
 * [신규] DIO → ADC3 동적 전환 API (10kHz 고속 아날로그 센서 지원)
 * ============================================================================
 */

/**
 * @brief [초기화] 외부 DIO 핀을 고속 ADC3 입력으로 전환합니다.
 * @details 
 * - 기본적으로 DIO 핀(D1~D8)은 디지털 입출력용입니다.
 * - 이 함수를 호출하면 해당 핀이 16-bit ADC3 아날로그 입력으로 전환됩니다.
 * - ADC3는 최대 8채널 동시 스캔 지원, 10kHz 샘플링 레이트로 동작합니다.
 * - 전환된 핀은 `XM_AnalogRead(XM_EXT_ADC_5~12)`로 읽을 수 있습니다.
 * 
 * @param[in] pin 전환할 DIO 핀 (XM_EXT_DIO_1 ~ XM_EXT_DIO_8)
 * @return true: 성공, false: 실패
 * 
 * @warning 
 * - 이 함수는 **비실시간**입니다. 초기화 단계(InitUserAlgorithm)에서만 호출하세요.
 * - 전환 후 해당 핀은 디지털 GPIO로 복구할 수 없습니다 (재부팅 필요).
 * 
 * @code
 * // 예제: FSR 8채널 사용 (12-bit 모드)
 * void InitUserAlgorithm(void) {
 *     XM_SetAnalogReadResolution(12);   // 12-bit 출력 (0~4095)
 *     
 *     for (int i = 0; i < 8; i++) {
 *         XM_SwitchDioToAdc(XM_EXT_DIO_1 + i);  // DIO 1~8 → ADC3
 *     }
 * }
 * 
 * void RunUserAlgorithm(void) {
 *     uint16_t fsr[8];
 *     for (int i = 0; i < 8; i++) {
 *         fsr[i] = XM_AnalogRead(XM_EXT_ADC_5 + i);  // 모두 0~4095
 *     }
 * }
 * @endcode
 */
bool XM_SwitchDioToAdc(XmDioPin_t pin);

/**
 * @brief [초기화] 모든 DIO 핀(D1~D8)을 ADC3 입력으로 일괄 전환합니다.
 * @details 
 * - 8개 DIO 핀을 루프 없이 한 번에 전환하는 편의 API입니다.
 * - 전환된 핀은 XM_AnalogRead(XM_EXT_ADC_5~12)로 읽을 수 있습니다.
 * 
 * @return true: 전체 성공, false: 하나 이상 실패
 * 
 * @code
 * void InitUserAlgorithm(void) {
 *     XM_SetAnalogReadResolution(12);
 *     XM_SwitchAllDioToAdc();  // DIO 1~8 → ADC3 일괄 전환
 * }
 * @endcode
 */
bool XM_SwitchAllDioToAdc(void);

#endif /* XM_API_XM_API_EXTERNAL_IO_H_ */
