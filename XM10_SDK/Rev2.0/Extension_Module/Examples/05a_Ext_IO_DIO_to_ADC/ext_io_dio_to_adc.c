/**
 ******************************************************************************
 * @file    ext_io_dio_to_adc.c
 * @author  HyundoKim
 * @brief   [초급] DIO 핀을 ADC로 전환하여 FSR 센서 읽기
 * @details
 * [학습 목표]
 *   - DIO 핀을 ADC 모드로 전환하는 방법 (XM_SwitchDioToAdc)
 *   - DIO→ADC 핀 번호 매핑 매크로 사용법 (XM_DIO_TO_ADC_PIN)
 *   - 전환 상태 확인 방법 (XM_IsDioSwitchedToAdc)
 *   - 밀리볼트 변환 API 사용법 (XM_AnalogReadMillivolts)
 *
 * [하드웨어 연결]
 *   - DIO 1번 핀(PF3)에 FSR 센서 연결
 *     FSR 한쪽 → 3.3V, 다른 쪽 → DIO_1 + 10kΩ → GND (분압 회로)
 *
 * [핀 전환 원리]
 *   XM 보드의 DIO 1~8번 핀(PF3~PF10)은 기본적으로 디지털 GPIO입니다.
 *   XM_SwitchDioToAdc()를 호출하면 해당 핀이 16-bit ADC3 아날로그 입력으로
 *   전환되며, 10kHz 샘플링 레이트로 자동 업데이트됩니다.
 *
 *   전환 후에는 XM_AnalogRead() 또는 XM_AnalogReadMillivolts()로 값을 읽습니다.
 *   이때 DIO 핀 번호(XM_EXT_DIO_x)와 ADC 핀 번호(XM_EXT_ADC_x)가 다르므로,
 *   XM_DIO_TO_ADC_PIN() 매크로를 사용하면 직관적으로 변환할 수 있습니다.
 *
 * [매핑 표]
 *   XM_EXT_DIO_1 (PF3)  → XM_EXT_ADC_5
 *   XM_EXT_DIO_2 (PF4)  → XM_EXT_ADC_6
 *   ...
 *   XM_EXT_DIO_8 (PF10) → XM_EXT_ADC_12
 *
 * @version 1.1
 * @date    Mar 09, 2026
 * @see     docs/api-reference/04-external-io.md
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define FSR_PIN         XM_EXT_DIO_1
#define FSR_THRESHOLD_MV  500   /* 0.5V 이상이면 "눌림" 판단 */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Run_Loop(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    /*
     * [Step 1] DIO 핀을 ADC로 전환
     * - 이 함수는 비실시간이므로 반드시 초기화 단계에서 호출
     * - 전환 후 해당 핀은 GPIO로 복구 불가 (재부팅 필요)
     */
    XM_SwitchDioToAdc(FSR_PIN);

    /*
     * [Step 2] 전환 상태 확인 (디버깅용)
     * - true이면 ADC 모드, XM_AnalogRead() 사용 가능
     */
    if (XM_IsDioSwitchedToAdc(FSR_PIN)) {
        XM_SetLedState(XM_LED_1, XM_ON);   /* 전환 성공 표시 */
    }

    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = {
        .id = XM_STATE_USER_START,
        .on_loop = Run_Loop
    };
    XM_TSM_AddState(s_tsm, &conf);
}

void User_Loop(void)
{
    XM_TSM_Run(s_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

static void Run_Loop(void)
{
    /*
     * [Step 3] ADC 값 읽기 — 두 가지 방법
     *
     * 방법 A: 밀리볼트로 읽기 (직관적)
     *   XM_DIO_TO_ADC_PIN() 매크로로 DIO→ADC 핀 번호 변환
     */
    XmAdcPin_t adc_pin = XM_DIO_TO_ADC_PIN(FSR_PIN);  /* == XM_EXT_ADC_5 */
    uint16_t mv = XM_AnalogReadMillivolts(adc_pin);

    /*
     * 방법 B: 정규화된 raw 값으로 읽기 (정밀 제어)
     *   기본 16-bit: 0~65535
     */
    uint16_t raw = XM_AnalogRead(adc_pin);
    (void)raw;  /* 이 예제에서는 밀리볼트 사용 */

    /* [Step 4] 임계치 판단 */
    if (mv > FSR_THRESHOLD_MV) {
        XM_SetLedState(XM_LED_2, XM_ON);
    } else {
        XM_SetLedState(XM_LED_2, XM_OFF);
    }
}
