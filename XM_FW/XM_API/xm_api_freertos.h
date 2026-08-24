/**
 ******************************************************************************
 * @file    xm_api_freertos.h
 * @author  HyundoKim
 * @brief   XM10 백그라운드 태스크 API (FreeRTOS 래퍼)
 * @details
 * User_Loop(1kHz)를 방해하지 않고 무거운 연산을 별도 태스크에서 실행합니다.
 * 우선순위는 UserTask(54)보다 낮아 제어 루프에 영향 없음.
 * 백그라운드 태스크에서 XM_SetAssistTorque 등 제어 API 호출 금지.
 * 데이터 공유는 volatile 변수로.
 *
 * @version 1.0
 * @date    2026-04-03
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_FREERTOS_H_
#define XM_API_FREERTOS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 백그라운드 태스크 함수 포인터 (return 시 태스크 자동 완료) */
typedef void (*XmBgTaskFunc_t)(void* arg);

/** @brief 백그라운드 태스크 핸들 (NULL = 유효하지 않음) */
typedef void* XmBgTaskHandle_t;

/**
 * @brief 백그라운드 태스크를 생성하고 즉시 실행합니다.
 * @param[in] name        태스크 이름 (디버거용, NULL 가능)
 * @param[in] func        태스크 함수 (return 시 자동 완료)
 * @param[in] arg         함수에 전달할 인자 (NULL 가능)
 * @param[in] stack_words 스택 크기 (words). 0이면 기본값 1024 사용
 * @return 유효한 핸들, 실패 시 NULL
 */
XmBgTaskHandle_t XM_BgTask_Create(const char* name, XmBgTaskFunc_t func,
                                   void* arg, uint32_t stack_words);

/**
 * @brief 백그라운드 태스크가 완료되었는지 확인합니다.
 * @param handle  태스크 핸들
 * @return true: 함수가 return하여 완료됨, false: 실행 중 또는 잘못된 핸들
 */
bool XM_BgTask_IsDone(XmBgTaskHandle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* XM_API_FREERTOS_H_ */
