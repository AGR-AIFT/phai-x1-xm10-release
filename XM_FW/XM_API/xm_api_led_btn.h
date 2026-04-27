/**
 ******************************************************************************
 * @file    xm_api_led_btn.h
 * @author  HyundoKim
 * @brief   XM10 LED 및 버튼 제어 통합 API
 * @details 사용자가 보드의 Function LED(1~3)와 버튼(1~3)을 쉽게 제어할 수 있도록 돕는 인터페이스입니다.
 * 단순한 On/Off뿐만 아니라 깜빡임(Blink), 롱 프레스(Long Press) 등의 고급 기능을 제공합니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_LED_BTN_H_
#define XM_API_XM_API_LED_BTN_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Function LED 식별자
 * @note  System Layer의 1-based index와 일치시킵니다.
 */
typedef enum {
    XM_LED_1 = 1, /**< 왼쪽 LED (ID: 1) */
    XM_LED_2 = 2, /**< 중간 LED (ID: 2) */
    XM_LED_3 = 3  /**< 오른쪽 LED (ID: 3) */
} XmLedId_t;

/**
 * @brief Function Button 식별자
 * @note  System Layer의 1-based index와 일치시킵니다.
 */
typedef enum {
    XM_BTN_1 = 1, /**< 왼쪽 버튼 (ID: 1) */
    XM_BTN_2 = 2, /**< 중간 버튼 (ID: 2) */
    XM_BTN_3 = 3  /**< 오른쪽 버튼 (ID: 3) */
} XmBtnId_t;

/**
 * @brief 논리적 ON/OFF 상태
 */
typedef enum {
    XM_OFF = 0, /**< 끄기 (Logic Low) */
    XM_ON  = 1  /**< 켜기 (Logic High) */
} XmState_t;

/**
 * @brief 버튼 물리적 상태 (Pressed/Released)
 * @note  IOIF 계층의 GPIO_PIN_SET(1)/RESET(0)과 매핑됩니다.
 * 회로(Active High/Low)에 상관없이 '눌렸는지'를 명확히 표현합니다.
 */
typedef enum {
    XM_RELEASED = 0, /**< 버튼이 떨어져 있음 */
    XM_PRESSED  = 1  /**< 버튼이 눌려 있음 */
} XmBtnState_t;

/* ========================== LED SECTION ========================== */
/**
 * @brief LED 동작 모드
 * @note  led_manager.h의 LedMode_t와 순서가 일치해야 합니다.
 */
typedef enum {
    XM_LED_OFF       = 0, /**< LED를 끕니다. */
    XM_LED_SOLID     = 1, /**< LED를 계속 켭니다. */
    XM_LED_BLINK     = 2, /**< 일정 주기로 깜빡입니다 (50% Duty). */
    XM_LED_HEARTBEAT = 3, /**< 심장 박동처럼 두 번 빠르게 깜빡입니다. (두근-두근) */
    XM_LED_ONESHOT   = 4  /**< 설정한 시간만큼 한 번 켜졌다가 자동으로 꺼집니다. (알림용) */
} XmLedMode_t;

/* ======================== BUTTON SECTION ======================== */
/**
 * @brief 버튼 이벤트 타입
 * @details 버튼의 상태 변화를 의미 있는 이벤트로 변환하여 제공합니다.
 * @note  button_manager.h의 ButtonEvent_t와 순서가 일치해야 합니다.
 */
typedef enum {
    XM_BTN_NONE       = 0, /**< 발생한 이벤트 없음 */
    XM_BTN_PRESSED    = 1, /**< 버튼을 막 누른 순간 (Rising Edge) */
    XM_BTN_RELEASED   = 2, /**< 버튼을 막 뗀 순간 (Falling Edge) */
    XM_BTN_CLICK      = 3, /**< 짧게 눌렀다 뗌 (클릭) */
    XM_BTN_LONG_PRESS = 4  /**< 1초 이상 길게 누름 (롱 프레스) */
} XmBtnEvent_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ========================== LED SECTION ========================== */
/**
 * @brief  LED를 켜거나 끕니다.
 * @param  led_id : XM_LED_1, XM_LED_2, XM_LED_3
 * @param  state  : XM_ON(켜기), XM_OFF(끄기)
 * @code
 * SetLedState(XM_LED_1, XM_ON); // "LED 1번을 켠다" -> 직관적!
 * @endcode
 */
void XM_SetLedState(XmLedId_t led_idx, XmState_t state);

/**
 * @brief  LED에 특수 효과(깜빡임 등)를 설정합니다.
 * @note   이 함수는 설정만 변경하며, 실제 동작은 XM_IO_Update()에 의해 처리됩니다.
 * @param  led_idx   : 제어할 LED 번호 (XM_LED_1 ~ 3)
 * @param  mode      : XM_LED_BLINK 등 동작 모드 (XmLedMode_t 참조)
 * @param  period_ms : 효과의 주기 (밀리초 단위).
 * - BLINK 모드: 깜빡임 주기 (예: 500 = 0.5초)
 * - ONESHOT 모드: 켜져 있는 시간
 * - SOLID/OFF 모드: 무시됨 (0 입력 권장)
 * @code
 * // 2번 LED를 0.5초 간격으로 깜빡이게 설정
 * SetLedEffect(2, XM_LED_BLINK, 500);
 * * // 3번 LED를 심장 박동 모드로 설정 (1초 주기)
 * SetLedEffect(3, XM_LED_HEARTBEAT, 1000);
 * @endcode
 */
void XM_SetLedEffect(XmLedId_t led_idx, XmLedMode_t mode, uint32_t period_ms);

/* ======================== BUTTON SECTION ======================== */
/**
 * @brief  버튼의 현재 물리적 상태를 확인합니다. (폴링용)
 * @param  btn_idx : 확인할 버튼 번호 (XM_BTN_1 ~ 3)
 * @return XM_PRESSED(눌림), XM_RELEASED(안 눌림)
 */
XmBtnState_t XM_GetButtonState(XmBtnId_t btn_idx);

/**
 * @brief  버튼에서 발생한 최신 이벤트를 가져옵니다. (이벤트 기반)
 * @note   이벤트를 한 번 읽으면 내부 큐에서 사라집니다(Read-Clear 방식).
 * 따라서 if/else 문보다는 switch 문이나 변수에 저장해서 사용하는 것이 좋습니다.
 * @param  btn_idx : 확인할 버튼 번호 (XM_BTN_1 ~ 3)
 * @return 감지된 이벤트 (XmBtnEvent_t)
 * @code
 * XmBtnEvent_t evt = GetButtonEvent(1);
 * if (evt == XM_BTN_CLICK) {
 * // 버튼 1이 클릭되었을 때 실행할 코드
 * } else if (evt == XM_BTN_LONG_PRESS) {
 * // 버튼 1이 길게 눌렸을 때 실행할 코드
 * }
 * @endcode
 */
XmBtnEvent_t XM_GetButtonEvent(XmBtnId_t btn_idx);

/* ======================== SYSTEM UPDATE SECTION ======================== */
/**
 * @brief  [필수] I/O 상태 업데이트 함수
 * @details LED의 깜빡임 타이밍 계산과 버튼의 디바운싱/롱프레스 처리를 수행합니다.
 * @warning User Task의 메인 루프 안에서 반드시 주기적으로 호출해야 합니다.
 * 호출하지 않으면 Blink나 Button Event가 동작하지 않습니다.
 * @code
 * RunUserTask() {
 * // 사용자 알고리즘 ...
 * * XM_IO_Update(); // 루프의 마지막에 호출 권장
 * }
 * @endcode
 */
void XM_IO_Update(void);

#endif /* XM_API_XM_API_LED_BTN_H_ */
