/**
 ******************************************************************************
 * @file    xm_api_user_custom.h
 * @author  HyundoKim
 * @brief   In-packet User Custom data slot API (lives inside Total Data 0x20)
 * @details
 * 사용자가 알고리즘 출력(필터링된 EMG, 디버그 변수, 상태 플래그 등)을
 * 365B Total Data Packet(Module ID 0x20)의 User_Custom 영역(28B)에 직접 써
 * 1kHz로 PhAI Studio 에 스트리밍 + OPFS 녹화에 자동 포함되게 한다.
 *
 * Layout (28B at offset 337..364):
 *   - user_f[4]     float32 — 가공/필터링 신호
 *   - user_i16[4]   int16   — raw 카운터/ADC
 *   - user_flags    uint16  — 16개 비트 플래그
 *   - user_u8[2]    uint8   — 작은 enum/state
 *
 * 사용 예:
 * @code
 * void Control_Setup(void) {
 *     // (선택) PhAI Studio 측 라벨 — 미호출 시 기본 라벨 사용
 *     XM_SetUsbCustomMeta(0xE0,
 *         "[{\"slot\":\"f0\",\"name\":\"EMG_envelope\",\"unit\":\"uV\"},"
 *          " {\"slot\":\"i16_0\",\"name\":\"PF3_raw\",\"unit\":\"LSB\"}]");
 * }
 *
 * void Control_Loop(void) {
 *     XM_UserCustom_SetFloat(0, envelope_uV);
 *     XM_UserCustom_SetI16  (0, pf3_raw);
 *     XM_UserCustom_SetFlag (0, is_fsm_active);
 * }
 * @endcode
 *
 * @note   Setter 는 Non-blocking 이며 Control_Loop() 외 어디서든 호출 가능하다.
 *         out-of-range 인덱스는 silent ignore (fault 없음).
 *
 * @version 1.0
 * @date    May 13, 2026
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_USER_CUSTOM_H_
#define XM_API_XM_API_USER_CUSTOM_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define XM_USER_CUSTOM_FLOAT_COUNT  4U  /**< user_f[] 슬롯 수 */
#define XM_USER_CUSTOM_I16_COUNT    4U  /**< user_i16[] 슬롯 수 */
#define XM_USER_CUSTOM_U8_COUNT     2U  /**< user_u8[] 슬롯 수 */
#define XM_USER_CUSTOM_FLAG_COUNT   16U /**< user_flags 비트 수 */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief User_Custom 28B 블록의 메모리 레이아웃 거울본
 * @note  Total Data Packet 의 user_* 필드 4개와 1:1 대응되며,
 *        snapshot 시 XM_UserCustom_GetBlock() 으로 한번에 읽어가 사용된다.
 */
typedef struct {
    float    f[XM_USER_CUSTOM_FLOAT_COUNT];   /**< 16B */
    int16_t  i16[XM_USER_CUSTOM_I16_COUNT];   /**<  8B */
    uint16_t flags;                           /**<  2B */
    uint8_t  u8[XM_USER_CUSTOM_U8_COUNT];     /**<  2B */
} XM_UserCustomBlock_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief Float 슬롯에 값을 기록한다 (필터 출력, envelope 등).
 * @param idx   슬롯 인덱스 (0 ~ XM_USER_CUSTOM_FLOAT_COUNT-1)
 * @param value 기록할 값
 * @note  idx 가 범위를 벗어나면 silent ignore.
 */
void XM_UserCustom_SetFloat(uint8_t idx, float value);

/**
 * @brief Int16 슬롯에 값을 기록한다 (raw ADC, 카운터 등).
 * @param idx   슬롯 인덱스 (0 ~ XM_USER_CUSTOM_I16_COUNT-1)
 * @param value 기록할 값
 * @note  idx 가 범위를 벗어나면 silent ignore.
 */
void XM_UserCustom_SetI16(uint8_t idx, int16_t value);

/**
 * @brief uint8 슬롯에 값을 기록한다 (FSM state 등).
 * @param idx   슬롯 인덱스 (0 ~ XM_USER_CUSTOM_U8_COUNT-1)
 * @param value 기록할 값
 * @note  idx 가 범위를 벗어나면 silent ignore.
 */
void XM_UserCustom_SetU8(uint8_t idx, uint8_t value);

/**
 * @brief 16-bit flags 워드를 한 번에 설정한다.
 * @param flags 새 16-bit 워드 (모든 비트 덮어씀)
 */
void XM_UserCustom_SetFlags(uint16_t flags);

/**
 * @brief 16-bit flags 중 한 비트만 설정/클리어한다.
 * @param bit   비트 인덱스 (0 ~ XM_USER_CUSTOM_FLAG_COUNT-1)
 * @param value true=1, false=0
 * @note  bit 가 범위를 벗어나면 silent ignore.
 */
void XM_UserCustom_SetFlag(uint8_t bit, bool value);

/**
 * @brief 모든 User_Custom 슬롯을 0으로 초기화한다.
 * @note  부팅 시 자동 호출되므로 일반 사용자는 호출 불필요.
 *        값을 리셋하고 싶을 때만 명시적으로 호출한다.
 */
void XM_UserCustom_Reset(void);

/**
 * @brief 내부 User_Custom 블록의 현재 값을 스냅샷한다.
 * @param[out] out 28B 분량의 출력 버퍼 (NULL 금지)
 * @note  System Layer (XM_TotalData_Snapshot) 가 매 tick 호출한다.
 *        일반 사용자가 직접 호출할 일은 없다.
 */
void XM_UserCustom_GetBlock(XM_UserCustomBlock_t* out);

#endif /* XM_API_XM_API_USER_CUSTOM_H_ */
