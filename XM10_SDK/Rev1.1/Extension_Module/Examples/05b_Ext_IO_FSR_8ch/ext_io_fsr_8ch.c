/**
 ******************************************************************************
 * @file    ext_io_fsr_8ch.c
 * @author  HyundoKim
 * @brief   [중급] FSR 8채널 일괄 전환 및 Resolution 설정
 * @details
 * [학습 목표]
 *   - 8개 DIO를 한 번에 ADC로 전환하는 방법 (XM_SwitchAllDioToAdc)
 *   - 출력 Resolution을 변경하는 방법 (XM_SetAnalogReadResolution)
 *   - 현재 Resolution을 조회하는 방법 (XM_GetAnalogReadResolution)
 *   - 배열과 루프를 사용한 다채널 데이터 수집
 *
 * [하드웨어 연결]
 *   DIO 1~8번 핀(PF3~PF10)에 FSR 센서 8개 연결 (분압 회로)
 *   
 *   각 채널:  3.3V ─── FSR ──┬── DIO_x
 *                             └── 10kΩ ─── GND
 *
 * [Resolution 설정 가이드]
 *   ┌──────────┬───────────┬──────────────────────────────────┐
 *   │ 설정값   │ 범위      │ 용도                             │
 *   ├──────────┼───────────┼──────────────────────────────────┤
 *   │ 16-bit   │ 0~65535   │ 최대 정밀도 (기본값)             │
 *   │ 12-bit   │ 0~4095    │ FSR, 온도센서 등 일반 센서       │
 *   │ 10-bit   │ 0~1023    │ Arduino 호환, 저전력 애플리케이션│
 *   │  8-bit   │ 0~255     │ 간단한 ON/OFF 감지               │
 *   └──────────┴───────────┴──────────────────────────────────┘
 *
 *   FSR 센서는 일반적으로 12-bit(4096단계)면 충분합니다.
 *   16-bit로 읽어도 센서 자체의 노이즈가 하위 비트를 채우므로,
 *   12-bit로 설정하면 노이즈가 자연스럽게 제거되는 효과가 있습니다.
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

#define FSR_COUNT           8
#define FSR_RESOLUTION      12   /* 12-bit: 0~4095 */
#define FSR_PRESS_THRESHOLD 200  /* 12-bit 기준 약 0.16V */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;
static uint16_t s_fsr[FSR_COUNT];
static uint8_t  s_pressed_count;

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
     * [Step 1] 출력 Resolution 설정
     * - 이 설정은 XM_AnalogRead()의 반환 범위에만 영향
     * - 하드웨어 ADC(16-bit)는 변경되지 않음 (소프트웨어 정규화)
     * - FSR 센서는 12-bit면 충분 → 노이즈 감소 효과
     */
    XM_SetAnalogReadResolution(FSR_RESOLUTION);

    /*
     * [Step 2] DIO 8핀 일괄 ADC 전환
     * - XM_SwitchAllDioToAdc()는 내부적으로 DIO 1~8을 순서대로 전환
     * - 루프 작성 불필요, 단 한 줄로 완료
     */
    XM_SwitchAllDioToAdc();

    /* 확인: 현재 설정된 Resolution 조회 */
    uint8_t current_res = XM_GetAnalogReadResolution();
    (void)current_res;  /* 디버거에서 확인: 12 */

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
     * [Step 3] 8채널 배열 읽기
     * - XM_EXT_ADC_5 ~ XM_EXT_ADC_12가 DIO 1~8에 대응
     * - Resolution 12-bit 설정 → 모든 값이 0~4095 범위
     */
    s_pressed_count = 0;

    for (int i = 0; i < FSR_COUNT; i++) {
        s_fsr[i] = XM_AnalogRead(XM_EXT_ADC_5 + i);

        if (s_fsr[i] > FSR_PRESS_THRESHOLD) {
            s_pressed_count++;
        }
    }

    /* [Step 4] 눌린 센서 개수에 따라 LED 반응 */
    if (s_pressed_count == 0) {
        XM_SetLedState(XM_LED_1, XM_OFF);
        XM_SetLedState(XM_LED_2, XM_OFF);
    } else if (s_pressed_count <= 4) {
        XM_SetLedState(XM_LED_1, XM_ON);
        XM_SetLedState(XM_LED_2, XM_OFF);
    } else {
        XM_SetLedState(XM_LED_1, XM_ON);
        XM_SetLedState(XM_LED_2, XM_ON);
    }
}
