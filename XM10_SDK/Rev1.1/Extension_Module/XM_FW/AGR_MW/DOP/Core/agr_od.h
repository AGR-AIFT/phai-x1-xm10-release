/**
 ******************************************************************************
 * @file    agr_od.h
 * @author  HyundoKim
 * @brief   AGR Object Dictionary - Core API
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * DOP Context에 의존하지 않는 순수 OD 조작 API입니다.
 * AGR_OD_Table_t를 직접 받아 OD Entry를 검색/읽기/쓰기합니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_OD_H
#define AGR_OD_H

#include "agr_dop_types.h"
#include <string.h>

/**
 *-----------------------------------------------------------
 * OD LOOKUP API
 *-----------------------------------------------------------
 */

/**
 * @brief OD Entry 검색 (Index)
 * @param od    Object Dictionary 테이블
 * @param index Object Index
 * @return OD Entry 포인터, 없으면 NULL
 */
const AGR_OD_Entry_t* AGR_OD_FindEntry(const AGR_OD_Table_t* od,
                                       uint16_t index);

/**
 * @brief OD Entry 검색 (Index + SubIndex)
 * @param od       Object Dictionary 테이블
 * @param index    Object Index
 * @param subindex Sub-Index
 * @return OD Entry 포인터, 없으면 NULL
 */
const AGR_OD_Entry_t* AGR_OD_FindEntryEx(const AGR_OD_Table_t* od,
                                         uint16_t index,
                                         uint8_t subindex);

/**
 *-----------------------------------------------------------
 * OD VALUE ACCESS API
 *-----------------------------------------------------------
 */

/**
 * @brief OD Entry 값 읽기
 * @param entry   OD Entry 포인터
 * @param out_buf 출력 버퍼
 * @param buf_len 버퍼 크기
 * @return 읽은 바이트 수, <0=에러 (-1=NULL, -2=no data, -3=write-only)
 */
int AGR_OD_ReadValue(const AGR_OD_Entry_t* entry,
                     void* out_buf,
                     uint8_t buf_len);

/**
 * @brief OD Entry 값 쓰기
 * @param entry  OD Entry 포인터
 * @param in_buf 입력 데이터
 * @param in_len 데이터 길이
 * @return 0=성공, <0=에러 (-1=NULL, -2=no data, -3=read-only, -4=size overflow)
 */
int AGR_OD_WriteValue(const AGR_OD_Entry_t* entry,
                      const void* in_buf,
                      uint8_t in_len);

#endif /* AGR_OD_H */
