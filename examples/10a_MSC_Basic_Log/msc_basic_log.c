/**
 ******************************************************************************
 * @file    msc_basic_log.c
 * @author  HyundoKim
 * @brief   [초급] USB 메모리에 데이터 저장하기 — 가장 간단한 예제
 * @details
 * [학습 목표]
 *   - USB 메모리가 준비되었는지 확인하는 방법
 *   - 저장할 데이터 구조체를 등록하는 방법
 *   - 버튼으로 로깅 시작/정지하는 방법
 *
 * [저장 파일 구조]
 *   /LOGS/BasicTest/
 *     ├── metadata.txt        ← 데이터 설명 + System 정보 (자동 생성)
 *     ├── summary.txt         ← 세션 통계 (Stop 시 자동 생성)
 *     └── data_000_part_000.bin  ← 바이너리 데이터
 *
 * [바이너리 레코드 포맷]
 *   auto_timestamp=ON (기본값) 이므로:
 *   [Header:4][tick_ms:4][leftHipAngle:4] = 12 bytes/record
 *
 * [디코딩]
 *   PythonDecoder/data_decoder_xm10.py를 사용하여 CSV 변환
 *
 * @version 1.0
 * @date    Feb 24, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef struct {
    float leftHipAngle;
} BasicLog_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static BasicLog_t s_log;
static bool s_is_logging = false;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    XM_SetUsbLogSource(&s_log, sizeof(BasicLog_t));
}

void User_Loop(void)
{
    /* BTN1: 로깅 시작 */
    if (!s_is_logging && XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogReady()) {
            s_is_logging = XM_StartUsbDataLog(
                "BasicTest",
                "leftHipAngle(float)"
            );
            if (s_is_logging) {
                XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
            }
        }
    }

    /* BTN2: 로깅 정지 */
    if (s_is_logging && XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_StopUsbDataLog();
        s_is_logging = false;
        XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0);
    }

    /* 데이터 갱신 (2ms마다 자동 저장됨) */
    if (s_is_logging) {
        s_log.leftHipAngle = XM.status.h10.leftHipAngle;
    }
}
