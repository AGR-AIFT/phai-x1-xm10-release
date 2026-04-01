/**
 ******************************************************************************
 * @file    xm_api.h
 * @author  HyundoKim
 * @brief   XM10 통합 API 헤더 (Entry Point)
 * @details 
 * 이 파일은 XM10 펌웨어의 모든 사용자 API를 포함합니다.
 * 사용자는 이 파일 하나만 include하면 로봇 제어, 센서 수신, I/O 제어, 로깅 기능을 모두 사용할 수 있습니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once // 현대 컴파일러를 위한 최적화

#ifndef XM_API_INC_XM_API_H_
#define XM_API_INC_XM_API_H_

/* --- Standard Libraries --- */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* --- XM10 Module APIs --- */
#include "xm_api_data.h"        // 로봇 데이터 및 제어 명령
#include "xm_api_tsm.h"         // 태스크 상태 머신 (FSM)
#include "xm_api_led_btn.h"     // 내장 LED 및 버튼 제어
#include "xm_api_external_io.h" // 외부 확장 IO (GPIO/ADC)
#include "xm_api_usb.h"         // USB 로깅 및 디버그

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
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * =================================================================================================
 * 타이머 유틸리티 API (Timer Utility API)
 * =================================================================================================
 */

/**
 * @brief 시스템 부팅 후 경과된 시간(밀리초)을 가져옵니다.
 * @details 이 값은 32비트 변수이므로 약 49.7일마다 0으로 되돌아갑니다(rollover).
 * 시간 차이를 계산할 때는 (현재시간 - 과거시간) 형태로 안전하게 사용해야 합니다.
 * @return uint32_t 경과된 시간 (ms).
 */
uint32_t XM_GetTick(void);

#endif /* XM_API_INC_XM_API_H_ */
