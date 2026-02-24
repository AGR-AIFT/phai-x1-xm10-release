/**
 ******************************************************************************
 * @file    ioif_agrb_sai.h
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] SAI Audio 하드웨어 추상화 헤더 (aeat_9955 origin)
 * @version 3.0 (Common Library - H7 Only)
 * @date    Feb 12, 2026
 *
 * @details
 * - Handle-based API: ioif_sai.play(), ioif_sai.stop() 등
 * - DMA Circular 버퍼 기반 오디오 전송
 * - FreeRTOS 전용 (BareMetal 미지원)
 * - H7 전용 하드웨어 (SAI 페리페럴)
 *
 * @note aeat_9955 원본 기반, ENABLE 가드 적용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef __IOIF_AGRB_SAI_H__
#define __IOIF_AGRB_SAI_H__

#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_SAI_ENABLE) && defined(IOIF_MCU_SERIES_H7)

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_sai.h"

#define IOIF_SAI_SAMPLE_RATE_16kHz

typedef struct __attribute__((__packed__)) {
    uint16_t left;
    uint16_t right;
} IOIF_SAI_AudioSample_t;

//전송 단위 (바이트) 여기서 확실히 정의하기
#if defined(IOIF_SAI_SAMPLE_RATE_16kHz)

#define IOIF_SAI_AUDIO_BLOCK_SAMPLE_LENGTH  (1024 * 8)

#else
#error "No IOIF SAI Sample Rate defined"
#endif

typedef void (*IOIF_SAI_EventCallback)(void); //TX HALF 또는 TX COMPLETE 시 호출되는 콜백 함수

typedef struct {
    SAI_HandleTypeDef* hsai;
    size_t capacity; //단일 버퍼 용량 (바이트)
    uint32_t timeout; //세마포어 대기 시간 (ms)
    IOIF_SAI_EventCallback callback;

} IOIF_SAI_Initialize_t;

typedef struct {
    AGRBStatusDef   (*assign)(IOIF_SAI_Initialize_t* init);
    AGRBStatusDef   (*play)(void* buffer, size_t length);
    AGRBStatusDef   (*stop)(void);
    AGRBStatusDef   (*reset)(void);

    size_t          (*capacity)(void);
} IOIF_SAI_Handle_t;

extern IOIF_SAI_Handle_t ioif_sai;

#endif /* AGRB_IOIF_SAI_ENABLE && IOIF_MCU_SERIES_H7 */

#endif /* __IOIF_AGRB_SAI_H__ */
