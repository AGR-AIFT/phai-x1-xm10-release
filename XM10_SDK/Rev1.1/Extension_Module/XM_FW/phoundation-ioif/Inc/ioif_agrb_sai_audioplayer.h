/**
 ******************************************************************************
 * @file    ioif_agrb_sai_audioplayer.h
 * @author  HyundoKim
 * @brief   [IOIF] SAI Audio Player — Ring Queue 기반 글리치-프리 오디오 재생
 * @version 1.0
 * @date    2026-04-13
 *
 * @details
 * SD카드 오디오 파일을 SAI DMA로 스트리밍 재생하는 IOIF 서브모듈.
 *
 * [모듈 분리 근거]
 * ioif_agrb_sai.c = SAI DMA 하드웨어 추상화 (인스턴스, DMA Circular, D-Cache)
 *                   → 모든 제품 공통, FATFS 의존 없음
 * ioif_agrb_sai_audioplayer.c = 응용 기능 (Ring Queue, Reader Task, FATFS)
 *                   → FATFS 있는 제품(CM, XM)만 사용
 *                   → SM(BareMetal, FATFS 없음) 등에서는 빌드 제외 가능
 *
 * [지원 포맷]
 * - WAV (PCM 16-bit, Mono/Stereo) — 현재 유일 포맷
 * - 향후 포맷 추가 시: 포맷 파서만 분리 (ioif_agrb_audio_xxx.c)
 *   Ring Queue + Reader Task + SAI 콜백은 포맷 무관 범용 구조
 *
 * [아키텍처]
 * - Audio Reader Task: SD → 포맷 파싱 → mono→stereo → Ring Queue 채움
 * - SAI Callback: Ring Queue에서 꺼내서 DMA 버퍼 갱신 (비블로킹)
 * - Underrun 시 silence 전송 (글리치 대신 무음)
 *
 * [사용법]
 * 1. IOIF_SAI_AudioPlayer_Init(&config)  — system_startup 또는 Device Init에서 1회
 * 2. IOIF_SAI_AudioPlayer_Play("0:/audio.wav")  — 재생 시작
 * 3. IOIF_SAI_AudioPlayer_Stop()  — 수동 정지 (EOF 시 자동 정지)
 *
 * [계층 역할]
 * - IOIF AudioPlayer: Ring Queue + Reader Task + DMA 콜백 + 포맷 파싱
 * - IOIF SAI: DMA Circular + D-Cache + 인스턴스 관리
 * - Device Driver (TAS5825M 등): I2C 앰프 제어 + Beep 합성
 * - Application: Play/Stop 호출만
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef __IOIF_AGRB_SAI_AUDIOPLAYER_H__
#define __IOIF_AGRB_SAI_AUDIOPLAYER_H__

#include "ioif_agrb_defs.h"
#include "ioif_agrb_sai.h"

#if defined(AGRB_IOIF_SAI_ENABLE) && defined(AGRB_IOIF_FILESYSTEM_ENABLE)

/**
 *-----------------------------------------------------------
 * CONFIGURATION (ioif_conf.h에서 override 가능)
 *-----------------------------------------------------------
 */

/** @brief Ring Queue 블록 크기 (SAI half-buffer와 동일해야 함) */
#ifndef IOIF_SAI_AUDIOPLAYER_BLOCK_SAMPLES
#define IOIF_SAI_AUDIOPLAYER_BLOCK_SAMPLES    (2048)
#endif

/** @brief Ring Queue 깊이 (블록 수, 1개는 sentinel → 유효 = DEPTH-1) */
#ifndef IOIF_SAI_AUDIOPLAYER_QUEUE_DEPTH
#define IOIF_SAI_AUDIOPLAYER_QUEUE_DEPTH      (4)
#endif

/** @brief Audio Reader Task 스택 크기 (words) */
#ifndef IOIF_SAI_AUDIOPLAYER_TASK_STACK
#define IOIF_SAI_AUDIOPLAYER_TASK_STACK       (512)
#endif

/** @brief Audio Reader Task 우선순위 */
#ifndef IOIF_SAI_AUDIOPLAYER_TASK_PRIORITY
#define IOIF_SAI_AUDIOPLAYER_TASK_PRIORITY    (osPriorityAboveNormal)
#endif

/**
 *-----------------------------------------------------------
 * TYPES
 *-----------------------------------------------------------
 */

/** @brief 재생 완료/정지 콜백 (Reader Task 컨텍스트에서 호출) */
typedef void (*IOIF_SAI_AudioPlayer_DoneCallback_t)(void* ctx);

/** @brief 초기화 설정 */
typedef struct {
    IOIF_SAIx_t  sai_id;    /**< 사용할 SAI 인스턴스 ID */
} IOIF_SAI_AudioPlayer_Config_t;

/**
 *-----------------------------------------------------------
 * PUBLIC API
 *-----------------------------------------------------------
 */

/**
 * @brief Audio Player 초기화 (Reader Task + Ring Queue 생성)
 * @param config  SAI 인스턴스 ID
 * @return AGRBStatus_OK on success
 */
AGRBStatusDef IOIF_SAI_AudioPlayer_Init(const IOIF_SAI_AudioPlayer_Config_t* config);

/**
 * @brief 재생 완료 콜백 등록
 * @param cb   콜백 함수 (NULL=해제)
 * @param ctx  사용자 컨텍스트
 * @note Device Driver에서 앰프 Hi-Z 전환용으로 등록
 */
AGRBStatusDef IOIF_SAI_AudioPlayer_RegisterDoneCallback(
    IOIF_SAI_AudioPlayer_DoneCallback_t cb, void* ctx);

/**
 * @brief 오디오 파일 재생 시작
 * @param filepath  FATFS 경로 (예: "0:/AudioFiles/016_audio_preview.wav")
 * @return AGRBStatus_OK on success
 * @note 현재 WAV(PCM 16-bit, Mono/Stereo) 지원.
 *       재생 중 호출 시 기존 재생 정지 후 시작.
 *       호출 전 앰프 PLAY 상태 필수.
 */
AGRBStatusDef IOIF_SAI_AudioPlayer_Play(const char* filepath);

/**
 * @brief 재생 정지
 * @return AGRBStatus_OK on success
 * @note Done 콜백은 호출되지 않음 (수동 정지이므로).
 */
AGRBStatusDef IOIF_SAI_AudioPlayer_Stop(void);

/**
 * @brief 재생 중 여부
 */
bool IOIF_SAI_AudioPlayer_IsPlaying(void);

#endif /* AGRB_IOIF_SAI_ENABLE && AGRB_IOIF_FILESYSTEM_ENABLE */

#endif /* __IOIF_AGRB_SAI_AUDIOPLAYER_H__ */
