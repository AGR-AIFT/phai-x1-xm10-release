/**
 ******************************************************************************
 * @file    ioif_agrb_sai.h
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] SAI Audio 하드웨어 추상화 헤더
 * @version 4.0 (Production Quality - Multi-Instance Redesign)
 * @date    Mar 22, 2026
 *
 * @details
 * - Instance-based API: IOIF_SAI_Play(id, buffer, len)
 * - DMA Circular 버퍼 기반 오디오 전송
 * - ISR → TaskNotify → EventTask → 사용자 콜백 (ISR 직접 콜백 제거)
 * - H7 + RTOS 전용 (나머지 플랫폼: NOT_SUPPORTED stub)
 *
 * @note Breaking change from v3.0: ioif_sai 전역 핸들 삭제, 함수형 API로 전환
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef __IOIF_AGRB_SAI_H__
#define __IOIF_AGRB_SAI_H__

#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_SAI_ENABLE)

#include <stdint.h>
#include <stddef.h>

#if defined(IOIF_MCU_SERIES_H7)
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_sai.h"
#endif

#define IOIF_SAI_MAX_INSTANCES    (2)

/* Sample Rate — ioif_conf.h에서 override 가능 (예: #define IOIF_SAI_SAMPLE_RATE_44kHz) */
#if !defined(IOIF_SAI_SAMPLE_RATE_16kHz) && !defined(IOIF_SAI_SAMPLE_RATE_44kHz) && !defined(IOIF_SAI_SAMPLE_RATE_48kHz)
#define IOIF_SAI_SAMPLE_RATE_16kHz          /**< 기본값: 16kHz */
#endif

typedef struct __attribute__((__packed__)) {
    uint16_t left;
    uint16_t right;
} IOIF_SAI_AudioSample_t;

#if defined(IOIF_SAI_SAMPLE_RATE_44kHz) || defined(IOIF_SAI_SAMPLE_RATE_48kHz)
#define IOIF_SAI_AUDIO_BLOCK_SAMPLE_LENGTH  (1024 * 2)    /**< ~23ms @44.1kHz, ~21ms @48kHz */
#elif defined(IOIF_SAI_SAMPLE_RATE_16kHz)
#define IOIF_SAI_AUDIO_BLOCK_SAMPLE_LENGTH  (1024 * 8)    /**< ~512ms @16kHz */
#else
#error "No IOIF SAI Sample Rate defined"
#endif

typedef uint32_t IOIF_SAIx_t;

typedef enum {
    IOIF_SAI_EVENT_HALF_COMPLETE,   // 첫 번째 반 버퍼 소비 완료
    IOIF_SAI_EVENT_COMPLETE,        // 두 번째 반 버퍼 소비 완료
    IOIF_SAI_EVENT_ERROR,           // DMA/SAI 에러
} IOIF_SAI_Event_e;

typedef void (*IOIF_SAI_EventCallback_t)(IOIF_SAIx_t id, IOIF_SAI_Event_e event, void* ctx);

#if defined(IOIF_MCU_SERIES_H7)
typedef struct {
    SAI_HandleTypeDef* hsai;
    size_t capacity;        // 단일 반 버퍼 용량 (바이트)
    uint32_t timeout_ms;    // 0 = osWaitForever
} IOIF_SAI_Config_t;
#endif

/* ===== Public API ===== */

AGRBStatusDef IOIF_SAI_AssignInstance(IOIF_SAIx_t* id, IOIF_SAI_Config_t* config);
AGRBStatusDef IOIF_SAI_RegisterCallback(IOIF_SAIx_t id, IOIF_SAI_EventCallback_t cb, void* ctx);
AGRBStatusDef IOIF_SAI_Play(IOIF_SAIx_t id, const void* buffer, size_t length);
AGRBStatusDef IOIF_SAI_Stop(IOIF_SAIx_t id);
AGRBStatusDef IOIF_SAI_Reset(IOIF_SAIx_t id);
size_t        IOIF_SAI_GetBufferCapacity(IOIF_SAIx_t id);

#endif /* AGRB_IOIF_SAI_ENABLE */

#endif /* __IOIF_AGRB_SAI_H__ */
