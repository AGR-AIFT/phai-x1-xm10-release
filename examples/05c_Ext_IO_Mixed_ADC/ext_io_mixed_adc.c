/**
 ******************************************************************************
 * @file    ext_io_mixed_adc.c
 * @author  HyundoKim
 * @brief   [중급] 고정 ADC 4핀 + DIO→ADC 8핀 혼합 사용 (최대 12채널)
 * @details
 * [학습 목표]
 *   - XM의 전체 ADC 핀맵 이해 (고정 4핀 + 동적 8핀 = 최대 12핀)
 *   - 네이티브 Resolution 차이 이해 (12-bit vs 16-bit)
 *   - 정규화(Normalization)가 일관된 값을 제공하는 것 확인
 *   - 밀리볼트 API가 Resolution 설정에 무관하게 정확한 것 확인
 *
 * [XM ADC 핀 맵]
 *   ┌────────────┬───────┬──────────┬──────────┬────────────────────────┐
 *   │ Facade     │ 핀    │ ADC      │ 네이티브 │ 비고                   │
 *   ├────────────┼───────┼──────────┼──────────┼────────────────────────┤
 *   │ XM_ADC_1   │ PA0   │ ADC1     │ 12-bit   │ [Shared] UART4_TX      │
 *   │ XM_ADC_2   │ PA0_C │ ADC2     │ 16-bit   │ 항상 사용 가능         │
 *   │ XM_ADC_3   │ PA1   │ ADC1     │ 12-bit   │ [Shared] UART4_RX      │
 *   │ XM_ADC_4   │ PA1_C │ ADC2     │ 16-bit   │ 항상 사용 가능         │
 *   ├────────────┼───────┼──────────┼──────────┼────────────────────────┤
 *   │ XM_ADC_5   │ PF3   │ ADC3     │ 16-bit   │ DIO 1 → ADC 전환 필요  │
 *   │ XM_ADC_6   │ PF4   │ ADC3     │ 16-bit   │ DIO 2 → ADC 전환 필요  │
 *   │ ...        │ ...   │ ADC3     │ 16-bit   │ ...                    │
 *   │ XM_ADC_12  │ PF10  │ ADC3     │ 16-bit   │ DIO 8 → ADC 전환 필요  │
 *   └────────────┴───────┴──────────┴──────────┴────────────────────────┘
 *
 *   ⚠️ 핵심: ADC_1/3은 네이티브 12-bit, ADC_2/4/5~12는 16-bit
 *   → XM_SetAnalogReadResolution()으로 정규화하면 모든 핀이 동일 범위 출력
 *   → XM_AnalogReadMillivolts()는 네이티브에서 직접 변환하므로 항상 정확
 *
 * [하드웨어 연결]
 *   - ADC_2 (PA0_C): 조이스틱 X축 (항상 사용 가능)
 *   - ADC_4 (PA1_C): 조이스틱 Y축 (항상 사용 가능)
 *   - DIO 1~4 (PF3~PF6): FSR 센서 4개 (ADC로 전환)
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

#define FSR_COUNT   4

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef struct {
    uint16_t joystick_x_mv;
    uint16_t joystick_y_mv;
    uint16_t fsr_mv[FSR_COUNT];
} SensorData_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;
static SensorData_t  s_data;

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
     * [Step 1] 네이티브 Resolution 확인
     * 고정 ADC 핀은 하드웨어마다 다른 해상도를 가짐
     */
    uint8_t res_adc1 = XM_GetAnalogResolution(XM_EXT_ADC_1);   /* 12 */
    uint8_t res_adc2 = XM_GetAnalogResolution(XM_EXT_ADC_2);   /* 16 */
    (void)res_adc1;
    (void)res_adc2;

    /*
     * [Step 2] DIO 1~4만 ADC로 전환 (DIO 5~8은 GPIO로 유지)
     * - 필요한 채널만 선택적으로 전환 가능
     */
    for (int i = 0; i < FSR_COUNT; i++) {
        XM_SwitchDioToAdc(XM_EXT_DIO_1 + i);
    }

    /*
     * [Step 3] Resolution은 기본값(16-bit) 유지
     * - 이 예제에서는 XM_AnalogReadMillivolts()를 주로 사용하므로
     *   Resolution 설정에 영향받지 않음
     */

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
     * [Step 4] 고정 ADC 핀 읽기 (조이스틱)
     * - ADC_2(PA0_C)는 16-bit 네이티브 → 밀리볼트 변환 정확
     * - ADC_4(PA1_C)도 16-bit 네이티브
     * - XM_AnalogReadMillivolts()는 Resolution 설정과 무관하게 정확한 mV 반환
     */
    s_data.joystick_x_mv = XM_AnalogReadMillivolts(XM_EXT_ADC_2);
    s_data.joystick_y_mv = XM_AnalogReadMillivolts(XM_EXT_ADC_4);

    /*
     * [Step 5] DIO→ADC 전환 핀 읽기 (FSR)
     * - XM_DIO_TO_ADC_PIN() 매크로로 DIO 번호에서 ADC 번호를 바로 얻음
     * - ADC3는 16-bit 네이티브
     */
    for (int i = 0; i < FSR_COUNT; i++) {
        XmAdcPin_t adc_pin = XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1 + i);
        s_data.fsr_mv[i] = XM_AnalogReadMillivolts(adc_pin);
    }

    /*
     * [핵심 포인트]
     * 고정 ADC 핀(12-bit/16-bit 혼재)과 DIO→ADC 핀(16-bit)을 함께 사용해도
     * XM_AnalogReadMillivolts()는 네이티브 raw에서 직접 변환하므로
     * 모든 핀에서 일관되게 정확한 밀리볼트 값을 반환합니다.
     *
     * 반면 XM_AnalogRead()는 설정된 출력 Resolution으로 정규화된 값을 반환하므로,
     * 네이티브 12-bit 핀과 16-bit 핀 모두 동일한 범위(예: 0~4095)로 출력됩니다.
     */

    /* 조이스틱 중립 감지 (약 1.65V = 절반 전압) */
    bool joystick_centered =
        (s_data.joystick_x_mv > 1500 && s_data.joystick_x_mv < 1800) &&
        (s_data.joystick_y_mv > 1500 && s_data.joystick_y_mv < 1800);

    XM_SetLedState(XM_LED_1, joystick_centered ? XM_ON : XM_OFF);
}
