/**
 ******************************************************************************
 * @file    button_manager.c
 * @author  HyundoKim
 * @brief   [System Layer] 온보드 기능 버튼 관리 구현부
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "button_manager.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define DEBOUNCE_DELAY_MS    50   // 디바운싱 시간
#define LONG_PRESS_TIME_MS   1000 // 롱프레스 기준 시간 (1초)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef enum {
    BTN_STATE_IDLE,
    BTN_STATE_DEBOUNCE,
    BTN_STATE_PRESSED,
    BTN_STATE_LONG_PRESS_HOLD
} BtnInternalState_t;

typedef struct {
    IOIF_GPIOx_t id;
    BtnInternalState_t state;
    uint32_t start_time;      // 상태 변경 시작 시간
    ButtonEvent_t last_event; // 사용자에게 전달할 이벤트 버퍼
} ButtonContext_t;

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

static ButtonContext_t s_buttons[3]; // Button 1, 2, 3

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _UpdateButton(ButtonContext_t* btn, uint32_t now);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void ButtonManager_Init(IOIF_GPIOx_t btn1_id, IOIF_GPIOx_t btn2_id, IOIF_GPIOx_t btn3_id)
{
    s_buttons[0].id = btn1_id;
    s_buttons[1].id = btn2_id;
    s_buttons[2].id = btn3_id;

    for(int i=0; i<3; i++) {
        s_buttons[i].state = BTN_STATE_IDLE;
        s_buttons[i].last_event = BTN_EVENT_NONE;
    }
}

void ButtonManager_Update(uint32_t timestamp_ms)
{
    for(int i=0; i<3; i++) {
        _UpdateButton(&s_buttons[i], timestamp_ms);
    }
}

uint8_t ButtonManager_GetState(uint8_t btn_index)
{
    if (btn_index < 1 || btn_index > 3) return 0;
    // IOIF 계층을 통해 현재 물리적 핀 상태 읽기 (Active High 가정)
    // (GPIO_PIN_SET은 1로 정의되어 있으므로 == 1 비교도 가능하지만, 명확하게 매크로 비교 권장)
    // return값이 GPIO_PIN_SET인 경우 하드웨어적으로 풀업이 되어 있으므로 기본적인 상태가 SET임.
    // 즉, 버튼을 눌렀을 때, LOW 버튼을 누르지 않은 상태가 HIGH가 되어야 함.
    // 이는 모듈의 버튼 풀업/풀다운 로직에 따라 변경되어야 함
    return (IOIF_GPIO_READ(s_buttons[btn_index - 1].id) == GPIO_PIN_RESET) ? 1 : 0;
}

ButtonEvent_t ButtonManager_PopEvent(uint8_t btn_index)
{
    if (btn_index < 1 || btn_index > 3) return BTN_EVENT_NONE;
    
    // 이벤트를 읽어가면 즉시 초기화 (Consume)
    ButtonEvent_t evt = s_buttons[btn_index - 1].last_event;
    s_buttons[btn_index - 1].last_event = BTN_EVENT_NONE;
    return evt;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

static void _UpdateButton(ButtonContext_t* btn, uint32_t now)
{
    // Active High (누르면 0)
    bool is_physically_pressed = (IOIF_GPIO_READ(btn->id) == GPIO_PIN_RESET);

    switch (btn->state) {
        case BTN_STATE_IDLE:
            if (is_physically_pressed) {
                btn->start_time = now;
                btn->state = BTN_STATE_DEBOUNCE;
            }
            break;

        case BTN_STATE_DEBOUNCE:
            if (is_physically_pressed) {
                if (now - btn->start_time >= DEBOUNCE_DELAY_MS) {
                    // 디바운싱 통과 -> 눌림 확정
                    btn->state = BTN_STATE_PRESSED;
                    btn->last_event = BTN_EVENT_PRESSED; // Event 발생
                    btn->start_time = now; // Long Press 타이머 시작
                }
            } else {
                // 노이즈였음 -> 복귀
                btn->state = BTN_STATE_IDLE;
            }
            break;

        case BTN_STATE_PRESSED:
            if (!is_physically_pressed) {
                // 손을 뗐음 -> 짧게 누른 것 (Click)
                btn->state = BTN_STATE_IDLE;
                btn->last_event = BTN_EVENT_CLICK; // Click Event
            } else {
                // 계속 누르고 있음 -> 시간 체크
                if (now - btn->start_time >= LONG_PRESS_TIME_MS) {
                    btn->state = BTN_STATE_LONG_PRESS_HOLD;
                    btn->last_event = BTN_EVENT_LONG_PRESS; // Long Press Event
                }
            }
            break;

        case BTN_STATE_LONG_PRESS_HOLD:
            if (!is_physically_pressed) {
                // 롱프레스 후 손을 뗐음
                btn->state = BTN_STATE_IDLE;
                btn->last_event = BTN_EVENT_RELEASED;
            }
            break;
    }
}
