/**
 ******************************************************************************
 * @file    xm_api_logging_raw.h
 * @author  HyundoKim
 * @brief   UNSAFE — no type checking. 지원 대상 아님.
 * @details
 * XM_LOG_REGISTER · XM_LOG_SEND 매크로(xm_api_logging.h)가 안에서 부르는 함수의 원형입니다.
 * 매크로를 쓰는 .c 파일은 이 헤더를 include 해야 컴파일됩니다.
 *
 * 아래 함수를 직접 부르지 마세요. 타입 · 크기 검사를 거치지 않아, 다른 구조체를 보내도 컴파일러가 막지 못합니다.
 * 이 헤더는 XM_API 폴더에 있지만 아래 함수의 직접 호출은 지원 대상이 아닙니다 — 항상 XM_LOG_* 매크로를 쓰세요.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_LOGGING_RAW_H_
#define XM_API_XM_API_LOGGING_RAW_H_

#include <stdint.h>
#include <stdbool.h>
#include "xm_api_logging.h"

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES — 직접 호출은 지원 대상 아님 (UNSAFE)
 *------------------------------------------------------------
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief XM_LOG_REGISTER 가 부릅니다.
 * @param slot_io         토큰의 슬롯 변수 — XM_LOG_SLOT_NONE 일 때만 등록하고, 성공하면 슬롯 번호를 씁니다
 * @param module_id       모듈 ID (0xF0 ~ 0xFE)
 * @param struct_name     구조체 이름(토큰)
 * @param struct_size     구조체 크기(바이트)
 * @param nominal_rate_hz 보낼 주기(Hz)
 * @param fields          필드 설명 배열
 * @param field_count     필드 수
 * @return true: 등록됨. false: 이미 등록된 슬롯이거나 등록 실패 — *slot_io 는 바뀌지 않습니다
 */
bool XM_Log_RegisterSchemaRaw(uint8_t* slot_io, uint8_t module_id, const char* struct_name, uint16_t struct_size,
                              uint16_t nominal_rate_hz, const XM_FieldDesc_t* fields, uint8_t field_count);

/**
 * @brief XM_LOG_SEND 가 부릅니다.
 * @param slot 등록된 슬롯 번호
 * @param data 보낼 구조체
 * @param size 구조체 크기(바이트)
 * @return true: 보냄. false: 등록 안 된 슬롯, size 가 그 슬롯의 구조체 크기와 다름, data 가 NULL, 등록 주기보다 일찍 부름,
 *         USB 장치가 준비되지 않음, 송신 버퍼가 가득 참, Control_Setup() · Control_Loop() 밖(다른 태스크 · 인터럽트)에서 부름
 * @note 송신 버퍼가 가득 차 false 가 나와도 다음 송신은 등록한 주기가 지난 뒤에 나갑니다
 */
bool XM_Log_SendRaw(uint8_t slot, const void* data, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* XM_API_XM_API_LOGGING_RAW_H_ */
