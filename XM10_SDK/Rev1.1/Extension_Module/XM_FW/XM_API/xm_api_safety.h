/**
 ******************************************************************************
 * @file    xm_api_safety.h
 * @author  HyundoKim
 * @brief   XM10 사용자 알고리즘용 공통 안전 헬퍼 (header-only)
 * @details
 * 토크를 출력하는 사용자 알고리즘(Control_Loop)이 공통으로 필요로 하는
 * 안전 패턴을 표준화합니다:
 *
 *   1. 진입 소프트스타트 — 제어 시작 순간 토크가 계단(step)으로 인가되는 것을
 *      막는 0→1 게인 램프 (기본 500ms)
 *   2. Slew-rate 제한 — tick 당 토크 변화량 상한 (모델/파라미터 급변 방어)
 *   3. 진폭 클램프 — |토크| 상한 (FW 공통 레일 ±10Nm 보다 작게 설정할 것)
 *   4. 유한값 가드 — NaN/Inf 는 slew/램프를 우회해 그 tick 부터 즉시 0 출력
 *      (+ctx->nonfinite_count 증가). 파사드에도 동일 가드가 있으나 이 헬퍼가
 *      먼저 걸러내므로, 알고리즘 NaN 발생 관찰은 ctx->nonfinite_count 로.
 *
 * [사용 계약 — 반드시 지킬 것]
 *  - 컨텍스트(XmSafeTorque_t)는 관절(축)마다 1개씩 둡니다 (좌/우 독립).
 *  - Control_Setup() 또는 파일 초기화에서 XM_SafeTorque_Init() 1회.
 *  - 제어 (재)진입 시점(예: TSM Active_Entry)마다 XM_SafeTorque_Reset() 호출
 *    — 호출하지 않으면 이전 세션의 마지막 토크에서 이어져 소프트스타트가
 *    걸리지 않습니다.
 *  - XM_SafeTorque_Step() 은 Control_Loop(1kHz)에서 매 tick 정확히 1회 호출
 *    (내부 시간 진행이 호출 횟수 기반 — XM_SAFETY_TICK_MS).
 *
 * @code
 * // 예: 오른쪽 관절 — 최대 5Nm, slew 무제한, 진입 램프 500ms
 * static XmSafeTorque_t s_safe_rh;
 * void Control_Setup(void) {
 *     XM_SafeTorque_Init(&s_safe_rh, 5.0f, 0U, XM_SAFETY_DEFAULT_RAMP_MS);
 * }
 * static void Active_Entry(void) {
 *     XM_SafeTorque_Reset(&s_safe_rh);   // 재진입마다 램프 재시작
 * }
 * void Control_Loop(void) {
 *     float tau = ...;                   // 알고리즘 계산값
 *     XM_SetAssistTorqueRH(XM_SafeTorque_Step(&s_safe_rh, tau));
 * }
 * @endcode
 *
 * @note header-only(static inline) 이므로 라이브러리 재빌드 없이 예제/사용자
 *       코드에서 바로 사용됩니다. xm_api.h 를 include 하면 자동 포함됩니다.
 * @version 0.1
 * @date    Aug 19, 2026
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_SAFETY_H_
#define XM_API_XM_API_SAFETY_H_

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief Step() 1회 = 1ms (Control_Loop 1kHz 호출 규약) */
#define XM_SAFETY_TICK_MS          (1U)

/** @brief 진입 소프트스타트 기본 시간 (ms) */
#define XM_SAFETY_DEFAULT_RAMP_MS  (500U)

/** @brief H10 AssistLevel 상한 (0~10 — H10 규약) */
#define XM_SAFETY_ASSIST_LEVEL_MAX (10U)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 관절(축) 1개의 안전 토크 컨텍스트
 * @details 필드는 모두 내부 상태 — 직접 쓰지 말고 Init/Reset/Step 만 사용.
 */
typedef struct {
    /* --- 설정 (Init 에서 결정) --- */
    float    limit_nm;       /**< |토크| 클램프 상한 (Nm) */
    float    slew_nm_per_ms; /**< tick(1ms)당 최대 변화량 (0 = slew 제한 없음) */
    uint32_t ramp_ms;        /**< 진입 소프트스타트 시간 (0 = 램프 없음) */
    /* --- 내부 상태 (Reset/Step 이 관리) --- */
    float    last_nm;        /**< 직전 출력값 — slew 기준점 */
    uint32_t elapsed_ms;     /**< (재)진입 후 경과 시간 (Step 호출 횟수 기반) */
    /* --- 진단 (읽기 전용 관찰 — Live Expressions 등) --- */
    uint32_t nonfinite_count; /**< NaN/Inf 입력이 0 으로 치환된 누적 횟수 (기대값 0).
                               *   파사드 카운터(g_xm_api_nonfinite_count)는 이 헬퍼가
                               *   먼저 걸러내면 증가하지 않으므로, 알고리즘의 NaN 발생
                               *   여부는 이 필드로 관찰하세요. */
} XmSafeTorque_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS (static inline)
 *------------------------------------------------------------
 */

/**
 * @brief  안전 토크 컨텍스트를 초기화합니다. (관절마다 1개, 1회 호출)
 * @param  ctx            대상 컨텍스트
 * @param  limit_nm       |토크| 클램프 상한 (Nm). FW 공통 레일(±10Nm,
 *                        CM_AUX_TORQUE_LIMIT_NM)보다 작거나 같게 설정하세요.
 * @param  full_scale_ms  0 → limit_nm 까지 도달하는 최소 시간(ms) — slew 제한.
 *                        0 이면 slew 제한 없음 (진입 램프만 사용하는 추종 제어용).
 * @param  ramp_ms        진입 소프트스타트 시간(ms). 0 이면 램프 없음.
 *                        보통 XM_SAFETY_DEFAULT_RAMP_MS(500ms) 권장.
 */
static inline void XM_SafeTorque_Init(XmSafeTorque_t *ctx, float limit_nm,
                                      uint32_t full_scale_ms, uint32_t ramp_ms)
{
    ctx->limit_nm        = (limit_nm > 0.0f) ? limit_nm : 0.0f;
    ctx->slew_nm_per_ms  = (full_scale_ms > 0U)
                           ? (ctx->limit_nm / (float)full_scale_ms) : 0.0f;
    ctx->ramp_ms         = ramp_ms;
    ctx->last_nm         = 0.0f;
    ctx->elapsed_ms      = 0U;
    ctx->nonfinite_count = 0U;
}

/**
 * @brief  제어 (재)진입 시 호출 — 소프트스타트 램프를 재시작하고 기준을 0 으로.
 * @note   TSM 을 쓰면 Active_Entry(), 자체 FSM 이면 제어 시작 분기에서 호출.
 */
static inline void XM_SafeTorque_Reset(XmSafeTorque_t *ctx)
{
    ctx->last_nm    = 0.0f;
    ctx->elapsed_ms = 0U;
}

/**
 * @brief  희망 토크를 안전 처리(유한값→클램프→소프트스타트→slew)해 반환합니다.
 * @param  ctx         대상 컨텍스트 (관절당 1개)
 * @param  desired_nm  알고리즘이 계산한 희망 토크 (Nm)
 * @return 안전 처리된 토크 (Nm) — 그대로 XM_SetAssistTorque*() 에 전달
 * @note   Control_Loop(1kHz)에서 매 tick 정확히 1회 호출해야 시간 진행이 맞습니다.
 */
static inline float XM_SafeTorque_Step(XmSafeTorque_t *ctx, float desired_nm)
{
    /* 1) 유한값 가드 — NaN/Inf 는 즉시 0 확정 (진단 카운터 증가).
     *    slew/램프를 우회해 이번 tick 부터 0 을 출력합니다 — slew 를 거치면
     *    이전 토크에서 서서히 감쇠(최대 full_scale_ms)하게 되어 비정상 입력에
     *    대한 즉시 차단이 되지 않기 때문입니다. */
    if (!isfinite(desired_nm)) {
        ctx->nonfinite_count++;
        ctx->last_nm = 0.0f;
        if (ctx->elapsed_ms < UINT32_MAX) {
            ctx->elapsed_ms += XM_SAFETY_TICK_MS;
        }
        return 0.0f;
    }
    float out = desired_nm;

    /* 2) 진폭 클램프 */
    if (out >  ctx->limit_nm) { out =  ctx->limit_nm; }
    if (out < -ctx->limit_nm) { out = -ctx->limit_nm; }

    /* 3) 진입 소프트스타트 (0→1 게인 램프) */
    if ((ctx->ramp_ms > 0U) && (ctx->elapsed_ms < ctx->ramp_ms)) {
        out *= (float)ctx->elapsed_ms / (float)ctx->ramp_ms;
    }

    /* 4) slew-rate 제한 (직전 출력 기준) */
    if (ctx->slew_nm_per_ms > 0.0f) {
        float delta = out - ctx->last_nm;
        if (delta >  ctx->slew_nm_per_ms) { delta =  ctx->slew_nm_per_ms; }
        if (delta < -ctx->slew_nm_per_ms) { delta = -ctx->slew_nm_per_ms; }
        out = ctx->last_nm + delta;
    }

    ctx->last_nm = out;
    if (ctx->elapsed_ms < UINT32_MAX) {
        ctx->elapsed_ms += XM_SAFETY_TICK_MS;
    }
    return out;
}

/**
 * @brief  H10 AssistLevel(0~10) 클램프 — 범위 밖 수신값을 상한으로 자릅니다.
 * @param  raw  XM.status.h10.h10AssistLevel 원시값
 * @return 0~10 로 클램프된 레벨
 */
static inline uint8_t XM_SafeAssistLevel(uint8_t raw)
{
    return (raw > XM_SAFETY_ASSIST_LEVEL_MAX) ? XM_SAFETY_ASSIST_LEVEL_MAX : raw;
}

/**
 * @brief  센서 freshness 판정 — 마지막 수신 후 timeout_ms 이내인지 확인합니다.
 * @param  last_update_tick  데이터 구조체의 lastUpdateTick (ms)
 * @param  now_tick          현재 tick (XM_GetTick(), ms)
 * @param  timeout_ms        허용 최대 나이 (ms)
 * @return true = 신선함(사용 가능), false = stale (해당 데이터 기반 제어 중단 권장)
 * @note   uint32 wrap-around 안전 (뺄셈 모듈로 연산).
 */
static inline bool XM_SafeIsFresh(uint32_t last_update_tick, uint32_t now_tick,
                                  uint32_t timeout_ms)
{
    return (uint32_t)(now_tick - last_update_tick) <= timeout_ms;
}

#endif /* XM_API_XM_API_SAFETY_H_ */
