/**
 ******************************************************************************
 * @file    led_manager.c
 * @author  HyundoKim
 * @brief   [System Layer] 온보드 RGB LED 관리 구현부
 * @details CM과 XM10의 연결 상태에 대한 LED indicator를 관리함
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "led_manager.h"
#include "ioif_agrb_gpio.h"
#include <stdbool.h>

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

// Status LED (RGB는 논리적으로 하나의 상태를 가짐)
static struct {
    IOIF_GPIOx_t r_id;
    IOIF_GPIOx_t g_id;
    IOIF_GPIOx_t b_id;
    LinkState_t current_state; // 현재 NMT 상태 기억
    LedMode_t mode;
    uint32_t period;
    uint32_t last_toggle;
    bool blink_state;       // Blink 시 On/Off 상태
    uint8_t color_mask;     // R(1)|G(2)|B(4) 비트 마스크
} s_link_led;

// For Function LED
typedef struct {
    IOIF_GPIOx_t id;
    LedMode_t mode;
    uint32_t period;        // Blink 주기
    uint32_t last_toggle;   // 마지막 토글 시간
    bool is_on;             // 현재 물리적 상태
    uint32_t start_time;    // One-shot 등을 위한 시작 시간
} LedContext_t;

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

// User LEDs (3개)
static LedContext_t s_user_leds[3];

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _ApplyRgbColor(uint8_t mask);
static void _UpdateSingleLed(LedContext_t* led, uint32_t now);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/* --------- Status LED for CM-XM Link ---------*/
void LedManager_InitLinkStatusLeds(IOIF_GPIOx_t r_id, IOIF_GPIOx_t g_id, IOIF_GPIOx_t b_id)
{
    s_link_led.r_id = r_id;
    s_link_led.g_id = g_id;
    s_link_led.b_id = b_id;

    // 초기 상태를 '알 수 없음'이나 다른 값으로 설정하여
    // 첫 호출 시 반드시 업데이트가 일어나도록 함 (또는 명시적 호출)
    s_link_led.current_state = (LinkState_t)-1;
    
    // 초기 상태: Booting (White)
    LedManager_SetLinkState(LINK_STATE_INITIALISING);
}

void LedManager_SetLinkState(LinkState_t state)
{
    // 상태가 동일하면 아무것도 하지 않고 리턴 (Redundancy Check)
    if (s_link_led.current_state == state) {
        return;
    }

    // 상태 업데이트
    s_link_led.current_state = state;

    // 상태에 따른 색상 및 패턴 매핑
    switch (state) {
        case LINK_STATE_INITIALISING:
            s_link_led.mode = LED_MODE_SOLID;
            s_link_led.color_mask = 0x07; // White (R+G+B)
            break;
        case LINK_STATE_PRE_OPERATIONAL:
            s_link_led.mode = LED_MODE_BLINK;
            s_link_led.period = 500;      // 500ms 주기 깜빡임
            s_link_led.color_mask = 0x04; // Blue
            break;
        case LINK_STATE_OPERATIONAL:
            s_link_led.mode = LED_MODE_SOLID;
            s_link_led.color_mask = 0x04; // Blue
            break;
        // case LINK_STATE_ERROR:
        //     s_link_led.mode = LED_MODE_BLINK;
        //     s_link_led.period = 200;      // 200ms 빠른 깜빡임
        //     s_link_led.color_mask = 0x01; // Red
        //     break;
        case LINK_STATE_STOPPED:
            s_link_led.mode = LED_MODE_SOLID;
            s_link_led.color_mask = 0x01; // Red
            break;
    }
    
    // 모드 변경 시 즉각적인 반응을 위해 초기화
    s_link_led.last_toggle = 0; // 타이머 리셋
    s_link_led.blink_state = true; // 켜진 상태로 시작

    // Solid 모드면 즉시 LED 적용, Blink면 Update()에서 처리됨
    if (s_link_led.mode == LED_MODE_SOLID) {
        _ApplyRgbColor(s_link_led.color_mask);
    } else {
        _ApplyRgbColor(s_link_led.color_mask); // Blink 시작 시 일단 켬
    }
}

/* --------- Function LED 1,2,3 ---------*/
void LedManager_InitUserLeds(IOIF_GPIOx_t led1_id, IOIF_GPIOx_t led2_id, IOIF_GPIOx_t led3_id)
{
    s_user_leds[0].id = led1_id;
    s_user_leds[1].id = led2_id;
    s_user_leds[2].id = led3_id;

    for(int i=0; i<3; i++) {
        s_user_leds[i].mode = LED_MODE_OFF;
        IOIF_GPIO_RESET(s_user_leds[i].id); // 초기 Off
    }
}

void LedManager_SetUserLedMode(uint8_t led_index, LedMode_t mode, uint32_t period_ms)
{
    if (led_index < 1 || led_index > 3) return;
    int idx = led_index - 1;

    s_user_leds[idx].mode = mode;
    s_user_leds[idx].period = period_ms;
    s_user_leds[idx].start_time = 0; // Reset start time (will be set in update)
    
    // Solid/Off는 즉시 반영
    if (mode == LED_MODE_SOLID) {
        IOIF_GPIO_SET(s_user_leds[idx].id);
        s_user_leds[idx].is_on = true;
    } else if (mode == LED_MODE_OFF) {
        IOIF_GPIO_RESET(s_user_leds[idx].id);
        s_user_leds[idx].is_on = false;
    }
}

/* --------- Tick Based LED Update Logic ---------*/
void LedManager_Update(uint32_t timestamp_ms)
{
    // 1. Status LED Update
    if (s_link_led.mode == LED_MODE_BLINK) {
        if (timestamp_ms - s_link_led.last_toggle >= s_link_led.period) {
            s_link_led.last_toggle = timestamp_ms;
            s_link_led.blink_state = !s_link_led.blink_state;
            
            if (s_link_led.blink_state) {
                _ApplyRgbColor(s_link_led.color_mask);
            } else {
                _ApplyRgbColor(0); // Off
            }
        }
    }

    // 2. User LEDs Update
    for (int i = 0; i < 3; i++) {
        _UpdateSingleLed(&s_user_leds[i], timestamp_ms);
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief RGB Status LED 색상 설정 (R, G, B)
 * @details Common Cathode(LOW=ON)가 아닌 Common Anode(HIGH=ON)으로 가정.
 * (만약 반대라면 SET/RESET을 교차하면 됩니다.)
 */
static void _ApplyRgbColor(uint8_t mask)
{
    // R(bit0), G(bit1), B(bit2)
    (mask & 0x01) ? IOIF_GPIO_SET(s_link_led.r_id) : IOIF_GPIO_RESET(s_link_led.r_id);
    (mask & 0x02) ? IOIF_GPIO_SET(s_link_led.g_id) : IOIF_GPIO_RESET(s_link_led.g_id);
    (mask & 0x04) ? IOIF_GPIO_SET(s_link_led.b_id) : IOIF_GPIO_RESET(s_link_led.b_id);
}

static void _UpdateSingleLed(LedContext_t* led, uint32_t now)
{
    // 초기화되지 않은 start_time 설정 (One-shot 등을 위해)
    if (led->start_time == 0) led->start_time = now;

    switch (led->mode) {
        case LED_MODE_BLINK:
            if (now - led->last_toggle >= led->period) {
                led->last_toggle = now;
                led->is_on = !led->is_on;
                (led->is_on) ? IOIF_GPIO_SET(led->id) : IOIF_GPIO_RESET(led->id);
            }
            break;

        case LED_MODE_HEARTBEAT:
            // 패턴: 톡-톡----톡-톡---- (비트 박자 구현)
            // 구현 간소화: 주기 내의 특정 시점에만 ON
            {
                uint32_t phase = (now % 1000); // 1초 주기 고정
                // 0~100ms: ON, 200~300ms: ON, 나머지: OFF
                bool should_be_on = (phase < 100) || (phase > 200 && phase < 300);
                if (led->is_on != should_be_on) {
                    led->is_on = should_be_on;
                    (should_be_on) ? IOIF_GPIO_SET(led->id) : IOIF_GPIO_RESET(led->id);
                }
            }
            break;

        case LED_MODE_ONE_SHOT:
            // 설정된 period만큼 켜지고 자동으로 꺼짐
            if (now - led->start_time < led->period) {
                if (!led->is_on) {
                    IOIF_GPIO_SET(led->id);
                    led->is_on = true;
                }
            } else {
                if (led->is_on) {
                    IOIF_GPIO_RESET(led->id);
                    led->is_on = false;
                    led->mode = LED_MODE_OFF; // 동작 완료 후 모드 변경
                }
            }
            break;

        default:
            break;
    }
}
