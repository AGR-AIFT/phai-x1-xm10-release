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
#include "ioif_agrb_tim.h"  // For g_tim2_id extern

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
 * @details ✅ ADC1/2/3 통합: DIO 핀도 ADC로 사용 가능
 */
typedef enum {
    /* ADC1/2 고정 핀 (항상 사용 가능) */
    EXT_ADC_1 = 0, // PA0 [Shared] ADC / UART4_TX (IMU 사용 시 GPIO 불가)
    EXT_ADC_2,     // PA0_C
    EXT_ADC_3,     // PA1 [Shared] ADC / UART4_RX (IMU 사용 시 GPIO 불가)
    EXT_ADC_4,     // PA1_C
    
    /* ADC3 동적 핀 (DIO → ADC 전환 필요) */
    EXT_ADC_5,     // PF3 (DIO 1 → ADC3, 전환 필요)
    EXT_ADC_6,     // PF4 (DIO 2 → ADC3, 전환 필요)
    EXT_ADC_7,     // PF5 (DIO 3 → ADC3, 전환 필요)
    EXT_ADC_8,     // PF6 (DIO 4 → ADC3, 전환 필요)
    EXT_ADC_9,     // PF7 (DIO 5 → ADC3, 전환 필요)
    EXT_ADC_10,    // PF8 (DIO 6 → ADC3, 전환 필요)
    EXT_ADC_11,    // PF9 (DIO 7 → ADC3, 전환 필요)
    EXT_ADC_12,    // PF10 (DIO 8 → ADC3, 전환 필요)
    
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
 * PUBLIC VARIABLES (extern)
 *------------------------------------------------------------
 */

/**
 * @brief [Global Export] TIM2 IOIF ID (system_startup에서 할당됨)
 * @details external_io.c에서 ADC3 채널 추가 시 TIM2 Stop/Start에 사용
 */
extern IOIF_TIMx_t g_tim2_id;

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
 * @brief [Facade용] 아날로그 핀의 값을 읽습니다 (실시간 안전, 정규화됨).
 * @details 
 * - ADC1 (USB CC + PA0, PA1): DMA Circular (12-bit native, 정규화 적용)
 * - ADC2 (PA0_C, PA1_C): DMA Circular (16-bit native, 정규화 적용)
 * - ADC3 (DIO 1~8): DMA Circular (16-bit native, 정규화 적용)
 * - 모든 핀의 반환값은 동일한 출력 resolution으로 정규화됩니다.
 * 
 * @param[in] pin 아날로그 핀 번호 (EXT_ADC_1 ~ EXT_ADC_12)
 * @return 정규화된 ADC 값 (기본 16-bit: 0~65535), 실패 시 0
 * 
 * @note 출력 resolution은 ExternalIO_SetOutputResolution()으로 변경 가능 (기본: 16-bit)
 */
uint16_t ExternalIO_ReadAdc(ExternalAdcPin_t pin);

/**
 * @brief [Facade용] ADC 출력 resolution을 설정합니다 (Arduino analogReadResolution 패턴).
 * @details 
 * - 설정된 resolution에 맞게 ExternalIO_ReadAdc()의 반환값이 정규화됩니다.
 * - 하드웨어 ADC resolution은 변경하지 않습니다 (소프트웨어 정규화만 수행).
 * - 네이티브보다 높은 resolution 요청 시 upscale (zero-padding, 좌측 shift)
 * - 네이티브보다 낮은 resolution 요청 시 downscale (우측 shift)
 * 
 * @param[in] bits 출력 resolution (8, 10, 12, 14, 16)
 * 
 * @note
 * - 기본값: 16 (최대 정밀도)
 * - FSR 등 12-bit 충분한 경우: ExternalIO_SetOutputResolution(12)
 */
void ExternalIO_SetOutputResolution(uint8_t bits);

/**
 * @brief [Facade용] 아날로그 핀의 네이티브 하드웨어 resolution을 반환합니다.
 * @param[in] pin 아날로그 핀 번호
 * @return 네이티브 resolution (bit 수, 예: 12, 16), 잘못된 핀이면 0
 */
uint8_t ExternalIO_GetNativeResolution(ExternalAdcPin_t pin);

/**
 * @brief [Facade용] 현재 설정된 출력 resolution을 반환합니다.
 * @return 현재 출력 resolution (bit 수, 기본값 16)
 */
uint8_t ExternalIO_GetOutputResolution(void);

/**
 * @brief [Facade용] DIO 핀이 ADC3로 전환되었는지 확인합니다.
 * @param[in] dio_pin 확인할 DIO 핀 (EXT_DIO_1 ~ EXT_DIO_8)
 * @return true: ADC3 모드, false: GPIO 모드 (또는 잘못된 핀)
 */
bool ExternalIO_IsDioSwitchedToAdc3(ExternalDioPin_t dio_pin);

/**
 * @brief [Facade용] 아날로그 핀의 전압을 밀리볼트(mV)로 반환합니다.
 * @details 
 * - 네이티브 raw 값에서 직접 변환하므로 resolution 설정에 영향받지 않습니다.
 * - VREF = 3.3V 기준: 0 ~ 3300 mV
 * 
 * @param[in] pin 아날로그 핀 번호 (EXT_ADC_1 ~ EXT_ADC_12)
 * @return 밀리볼트 단위 전압 (0 ~ 3300), 실패 시 0
 */
uint16_t ExternalIO_ReadAdcMillivolts(ExternalAdcPin_t pin);

/**
 * @brief [System] USB CC 핀의 ADC 값을 읽습니다 (스냅샷).
 * @details 
 * - ADC1 DMA Circular 버퍼에서 Decimation된(1kHz) 값을 읽어옵니다.
 * - USB CC는 2개의 채널(CC1, CC2)을 동시에 판단해야 하므로 스냅샷 방식으로 제공합니다.
 * 
 * @param[out] cc1 CC1 (PF11) 값 (출력, NULL 허용)
 * @param[out] cc2 CC2 (PF12) 값 (출력, NULL 허용)
 */
void ExternalIO_GetUsbCcValues(uint16_t* cc1, uint16_t* cc2);

/**
 * ============================================================================
 * [신규] DIO → ADC3 동적 전환 API (10kHz 고속 샘플링 지원)
 * ============================================================================
 */

/**
 * @brief [런타임] 외부 DIO 핀을 ADC3 입력으로 동적 전환합니다.
 * @details 
 * - 해당 핀의 GPIO 기능을 중지하고 ADC3 Analog 모드로 재설정합니다.
 * - ADC3는 최대 8채널(PF3~PF10) 동시 스캔 지원, DMA Circular 모드로 동작합니다.
 * - 10kHz 샘플링 레이트 최적화 설정 적용 (ADC Clock: /4, Sampling: 64.5 Cycles)
 * 
 * @param[in] dio_pin 전환할 DIO 핀 (EXT_DIO_1 ~ EXT_DIO_8)
 * @return true: 성공, false: 실패 (이미 ADC 모드이거나 하드웨어 오류)
 * 
 * @warning 
 * - 이 함수는 **비실시간** 함수입니다. 초기화 단계나 모드 전환 시에만 호출하세요.
 * - ADC3는 첫 번째 핀 전환 시 자동으로 초기화되며, 이후 추가 채널은 동적으로 추가됩니다.
 * - 전환 후 해당 핀은 GPIO로 복구할 수 없습니다 (재부팅 필요).
 * 
 * @note STM32H743XI ADC3 Channel Mapping:
 * - PF3 (DIO_1) → ADC3_INP5
 * - PF4 (DIO_2) → ADC3_INP9
 * - PF5 (DIO_3) → ADC3_INP4
 * - PF6 (DIO_4) → ADC3_INP8
 * - PF7 (DIO_5) → ADC3_INP3
 * - PF8 (DIO_6) → ADC3_INP7
 * - PF9 (DIO_7) → ADC3_INP2
 * - PF10 (DIO_8) → ADC3_INP6
 */
bool ExternalIO_SwitchDioToAdc3(ExternalDioPin_t dio_pin);

/**
 * @brief [런타임] ADC3로 전환된 DIO 핀의 값을 읽습니다 (실시간 안전, Zero-copy).
 * @details 
 * - IOIF 내부 DMA Circular 버퍼에서 직접 읽기 (< 1us, Zero-copy)
 * - 10kHz 샘플링 레이트로 자동 업데이트, 항상 최신 값 보장
 * 
 * @param[in] dio_pin ADC3로 전환된 DIO 핀
 * @return 16-bit ADC raw 값 (0~65535), 실패 시 0
 * 
 * @note 
 * - 정규화되지 않은 네이티브 raw 값을 반환합니다.
 * - 정규화된 값이 필요하면 ExternalIO_ReadAdc(EXT_ADC_5~12)를 사용하세요.
 * - ADC3로 전환되지 않은 핀을 읽으면 0을 반환합니다.
 */
uint16_t ExternalIO_ReadDioAsAdc3(ExternalDioPin_t dio_pin);

/**
 * ============================================================================
 * [신규] ISR Functions (stm32h7xx_it.c에서 호출, UART4 패턴과 동일)
 * ============================================================================
 */

/**
 * @brief ADC3 DMA 인터럽트 핸들러 (System Layer)
 * @details 
 * - stm32h7xx_it.c의 DMA2_Stream2_IRQHandler에서 호출됩니다.
 * - IOIF의 DMA 핸들을 조회하여 HAL_DMA_IRQHandler 호출.
 * 
 * @note UART4 패턴 참조: System_ISR_DMA_UART4_RX_Manual()
 */
void System_ISR_ADC3_DMA(void);

/**
 * @brief ADC3 인터럽트 핸들러 (System Layer)
 * @details 
 * - stm32h7xx_it.c의 ADC3_IRQHandler에서 호출됩니다.
 * - IOIF의 ADC 핸들을 조회하여 HAL_ADC_IRQHandler 호출.
 * 
 * @note UART4 패턴 참조: System_ISR_UART4_Manual()
 */
void System_ISR_ADC3(void);

/**
 * ============================================================================
 * [신규] UART4 ISR Functions (ADC3 패턴과 동일)
 * ============================================================================
 */

/**
 * @brief UART4 인터럽트 핸들러 (System Layer)
 * @details 
 * - ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 * - ✅ ADC3 패턴과 동일한 위임 구조
 * - stm32h7xx_it.c의 UART4_IRQHandler에서 호출됩니다.
 * 
 * @note 
 * - ExternalIO_SwitchToUartMode()로 UART4 초기화 시 자동으로 NVIC 설정됨
 * - RTOS/BareMetal 공통 사용 가능
 */
void System_ISR_UART4(void);

/**
 * @brief UART4 DMA RX 인터럽트 핸들러 (System Layer)
 * @details 
 * - ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 * - ✅ ADC3 패턴과 동일한 위임 구조
 * - stm32h7xx_it.c의 DMA2_Stream0_IRQHandler에서 호출됩니다.
 * 
 * @note 
 * - ExternalIO_SwitchToUartMode()로 UART4 초기화 시 자동으로 NVIC 설정됨
 * - RTOS/BareMetal 공통 사용 가능
 */
void System_ISR_UART4_RX_DMA(void);

/**
 * @brief UART4 DMA TX 인터럽트 핸들러 (System Layer)
 * @details 
 * - ✅ HAL 완전히 숨김 - IOIF에 완전 위임
 * - ✅ ADC3 패턴과 동일한 위임 구조
 * - stm32h7xx_it.c의 DMA2_Stream1_IRQHandler에서 호출됩니다.
 * 
 * @note 
 * - ExternalIO_SwitchToUartMode()로 UART4 초기화 시 자동으로 NVIC 설정됨
 * - RTOS/BareMetal 공통 사용 가능
 */
void System_ISR_UART4_TX_DMA(void);

/**
 * ============================================================================
 * [신규] 런타임 핀 변환 API (ADC ↔ UART 동적 전환)
 * ============================================================================
 */

/**
 * @brief [런타임] External IO의 ADC 핀을 UART4(IMU용)로 동적 전환합니다.
 * @details 
 * - PA0 (External ADC 1) → UART4_TX
 * - PA1 (External ADC 3) → UART4_RX
 * - ADC1/ADC2 중지 → GPIO DeInit → UART4 초기화 (921600 baud)
 * - DMA Circular RX 활성화 (Xsens IMU용)
 * 
 * @return true: 성공, false: 실패 (하드웨어 오류)
 * 
 * @warning 
 * - ⚠️ 이 함수는 **비실시간** 함수입니다. 초기화 단계에서만 호출하세요.
 * - ⚠️ 전환 후 External ADC 1/3은 사용 불가능합니다 (재부팅 필요).
 * - ⚠️ ADC1/ADC2를 사용 중인 경우 먼저 중지해야 합니다.
 * 
 * @usage (Facade Layer)
 * @code
 * // XM API에서 호출
 * if (XM_EnableExternalImu()) {
 *     // IMU 모드 활성화 성공
 * }
 * @endcode
 * 
 * @note 
 * - CubeMX ADC 설정을 런타임에 UART로 교체합니다.
 * - IOIF_UART_InitManual()로 GPIO+DMA+NVIC 자동 설정됩니다.
 * - System Layer는 HAL을 직접 호출하지 않습니다 (아키텍처 준수).
 */
bool ExternalIO_SwitchToUartMode(void);

/**
 * ============================================================================
 * [DEBUG] ADC3 디버그 API (문제 진단용)
 * ============================================================================
 */

/**
 * @brief [DEBUG] ADC3 상태를 확인합니다.
 * @return ADC3 초기화 여부
 */
bool ExternalIO_IsAdc3Initialized(void);

/**
 * @brief [DEBUG] ADC3 활성 채널 수를 확인합니다.
 * @return 현재 활성화된 ADC3 채널 수 (0~8)
 */
uint8_t ExternalIO_GetAdc3ChannelCount(void);

/**
 * @brief [DEBUG] ADC3 DMA 버퍼 포인터를 확인합니다.
 * @return DMA 버퍼 포인터 (NULL이면 비정상)
 */
const uint16_t* ExternalIO_GetAdc3Buffer(void);

#endif /* SYSTEM_BOARD_GPIO_EXTERNAL_IO_H_ */
