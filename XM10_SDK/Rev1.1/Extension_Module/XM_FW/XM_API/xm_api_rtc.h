/**
 ******************************************************************************
 * @file    xm_api_rtc.h
 * @author  HyundoKim
 * @brief   XM10 RTC (Real-Time Clock) API
 * @details MCP79510 RTC를 통한 날짜/시간 관리 API.
 *          내부적으로 2-digit year(0~99)를 4-digit year(2000~2099)로 변환합니다.
 * @version 1.0.0
 * @date    2026-03-02
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_RTC_H_
#define XM_API_XM_API_RTC_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 날짜/시간 구조체 (4자리 연도)
 */
typedef struct {
    uint16_t year;      /**< 연도 (2000~2099) */
    uint8_t  month;     /**< 월 (1~12) */
    uint8_t  day;       /**< 일 (1~31) */
    uint8_t  weekday;   /**< 요일 (1=Mon ~ 7=Sun) */
    uint8_t  hour;      /**< 시 (0~23) */
    uint8_t  minute;    /**< 분 (0~59) */
    uint8_t  second;    /**< 초 (0~59) */
} XmDateTime_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief RTC에 날짜/시간을 설정합니다.
 * @param[in] dt 설정할 날짜/시간 (year: 2000~2099)
 * @return true: 성공, false: 실패 (범위 초과 또는 SPI 에러)
 *
 * @code
 * XmDateTime_t dt = {
 *     .year = 2026, .month = 3, .day = 2,
 *     .weekday = 1,
 *     .hour = 14, .minute = 30, .second = 0
 * };
 * XM_RTC_SetDateTime(&dt);
 * @endcode
 */
bool XM_RTC_SetDateTime(const XmDateTime_t* dt);

/**
 * @brief RTC에서 현재 날짜/시간을 읽습니다.
 * @param[out] dt 읽은 날짜/시간 (year: 2000~2099)
 * @return true: 성공, false: 실패 (SPI 에러)
 *
 * @code
 * XmDateTime_t now;
 * if (XM_RTC_GetDateTime(&now)) {
 *     printf("%04d-%02d-%02d %02d:%02d:%02d\n",
 *            now.year, now.month, now.day,
 *            now.hour, now.minute, now.second);
 * }
 * @endcode
 */
bool XM_RTC_GetDateTime(XmDateTime_t* dt);

/**
 * @brief RTC 오실레이터가 동작 중인지 확인합니다.
 * @return true: 동작 중, false: 정지 (배터리 방전 등)
 */
bool XM_RTC_IsRunning(void);

#endif /* XM_API_XM_API_RTC_H_ */
