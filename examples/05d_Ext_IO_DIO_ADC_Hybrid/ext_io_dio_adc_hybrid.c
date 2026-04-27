/**
 ******************************************************************************
 * @file    ext_io_dio_adc_hybrid.c
 * @author  HyundoKim
 * @brief   [응용] DIO/ADC 혼합 모드 — 디지털 + 아날로그 동시 활용
 * @details
 * [학습 목표]
 *   - 동일한 DIO 포트에서 일부는 GPIO, 일부는 ADC로 사용하는 방법
 *   - ADC 전환된 핀에 GPIO 접근 시 보호 장치가 작동하는 것 확인
 *   - 실전 시나리오: 버튼으로 FSR 측정 모드를 전환하는 장치 구성
 *
 * [하드웨어 연결]
 *   ┌─────────────────────────────────────────────────────┐
 *   │ DIO 1~4 (PF3~PF6)  → FSR 센서 4개 (ADC로 전환)     │
 *   │ DIO 5   (PF7)      → 외부 푸시 버튼 (Input Pullup) │
 *   │ DIO 6   (PF8)      → 상태 표시 LED (Output)        │
 *   │ DIO 7~8 (PF9~PF10) → 미사용 (기본 Input Floating)  │
 *   └─────────────────────────────────────────────────────┘
 *
 * [동작 설명]
 *   1. 초기 상태: LED OFF, 대기 모드
 *   2. 버튼 누름: 측정 모드 토글
 *      - 측정 모드 ON  → LED ON, FSR 데이터 수집 시작
 *      - 측정 모드 OFF → LED OFF, 수집 중지
 *   3. 측정 모드에서 FSR 4개의 압력 합계를 계산
 *
 * [보호 장치 동작]
 *   DIO 1~4는 ADC로 전환 후 GPIO 함수를 호출해도 안전하게 무시됩니다.
 *   예: XM_DigitalRead(XM_EXT_DIO_1) → false 반환 (충돌 없음)
 *       XM_SetPinMode(XM_EXT_DIO_1, ...) → 무시 (ADC 설정 유지)
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

#define FSR_ADC_COUNT     4
#define BUTTON_PIN        XM_EXT_DIO_5
#define LED_PIN           XM_EXT_DIO_6

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;
static bool     s_measuring = false;
static bool     s_btn_prev  = false;
static uint16_t s_fsr_mv[FSR_ADC_COUNT];
static uint32_t s_total_pressure_mv;

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
     * [Step 1] DIO 1~4를 ADC로 전환 (FSR 센서용)
     */
    XM_SetAnalogReadResolution(12);

    for (int i = 0; i < FSR_ADC_COUNT; i++) {
        XM_SwitchDioToAdc(XM_EXT_DIO_1 + i);
    }

    /*
     * [Step 2] DIO 5~6은 GPIO로 유지 (버튼, LED)
     * - ADC 전환과 무관하게 별도로 설정 가능
     */
    XM_SetPinMode(BUTTON_PIN, XM_EXT_DIO_MODE_INPUT_PULLUP);
    XM_SetPinMode(LED_PIN, XM_EXT_DIO_MODE_OUTPUT);
    XM_DigitalWrite(LED_PIN, XM_LOW);

    /*
     * [확인] 핀 상태 점검
     * - DIO 1~4: ADC 모드 (true)
     * - DIO 5~6: GPIO 모드 (false)
     */
    bool dio1_is_adc = XM_IsDioSwitchedToAdc(XM_EXT_DIO_1);  /* true  */
    bool dio5_is_adc = XM_IsDioSwitchedToAdc(BUTTON_PIN);     /* false */
    (void)dio1_is_adc;
    (void)dio5_is_adc;

    /*
     * [보호 장치 테스트]
     * ADC 전환된 DIO 1번에 GPIO 함수를 호출해도 안전하게 무시됨
     */
    XM_SetPinMode(XM_EXT_DIO_1, XM_EXT_DIO_MODE_OUTPUT);  /* 무시됨 */
    XM_DigitalWrite(XM_EXT_DIO_1, XM_HIGH);                /* 무시됨 */
    XmLogicLevel_t dummy = XM_DigitalRead(XM_EXT_DIO_1);   /* XM_LOW 반환 */
    (void)dummy;

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
     * [Step 3] 외부 버튼으로 측정 모드 토글 (Edge Detection)
     * - DIO 5번은 GPIO 모드이므로 DigitalRead 정상 동작
     */
    bool btn_now = (XM_DigitalRead(BUTTON_PIN) == XM_LOW);
    bool btn_pressed = (btn_now && !s_btn_prev);
    s_btn_prev = btn_now;

    if (btn_pressed) {
        s_measuring = !s_measuring;
        XM_DigitalWrite(LED_PIN, s_measuring ? XM_HIGH : XM_LOW);
    }

    /*
     * [Step 4] 측정 모드일 때만 FSR 데이터 수집
     * - DIO 1~4는 ADC 모드이므로 AnalogReadMillivolts 사용
     */
    if (s_measuring) {
        s_total_pressure_mv = 0;

        for (int i = 0; i < FSR_ADC_COUNT; i++) {
            XmAdcPin_t adc_pin = XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1 + i);
            s_fsr_mv[i] = XM_AnalogReadMillivolts(adc_pin);
            s_total_pressure_mv += s_fsr_mv[i];
        }

        /* 총 압력이 높으면 내부 LED로 알림 */
        if (s_total_pressure_mv > 6000) {
            XM_SetLedState(XM_LED_1, XM_ON);
        } else {
            XM_SetLedState(XM_LED_1, XM_OFF);
        }
    } else {
        XM_SetLedState(XM_LED_1, XM_OFF);
    }
}
