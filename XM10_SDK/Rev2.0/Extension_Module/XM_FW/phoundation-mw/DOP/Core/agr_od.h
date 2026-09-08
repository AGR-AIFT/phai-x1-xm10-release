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
 * @brief OD 정렬 인덱스 빌드 — FindEntryEx를 이진탐색(O(log n))으로 전환
 * @details
 * 드라이버 Init에서 1회 호출 (런타임 중 호출 금지 — lookup과 비동기 갱신 불가).
 * key = (index << 8) | subindex 오름차순으로 entry 위치 인덱스를 정렬해
 * index_buf에 채우고 od->sorted_idx에 연결한다. 삽입정렬 — boot 1회 비용만.
 * 중복 key 검출 시 연결하지 않고 -3 반환 (fail loud — 중복 entry는 선형
 * 탐색에서 앞쪽이 뒤쪽을 가리는 잠재 결함이므로 부팅 시점에 드러낸다).
 *
 * > DETERMINISTIC > OBVIOUS — lookup 상한 O(log n) 보장, 정렬은 Init 1회.
 * > 테이블 소스는 섹션 단위 가독성 순서를 유지한다 (정렬 강제는 인덱스가 흡수).
 *
 * @param od           OD 테이블 (sorted_idx 필드가 채워짐)
 * @param index_buf    호출자 제공 정적 버퍼 (수명 = od 수명, stack 금지)
 * @param buf_capacity index_buf 원소 수 (>= od->entry_count)
 * @return 0=성공, -1=NULL 인자, -2=capacity 부족, -3=중복 (index,subindex)
 */
int32_t AGR_OD_BuildSortedIndex(AGR_OD_Table_t* od,
                                uint16_t* index_buf,
                                uint16_t buf_capacity);

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
int32_t AGR_OD_ReadValue(const AGR_OD_Entry_t* entry,
                     void* out_buf,
                     uint16_t buf_len);

/**
 * @brief OD Entry 값 쓰기
 * @param entry  OD Entry 포인터
 * @param in_buf 입력 데이터
 * @param in_len 데이터 길이
 * @return 0=성공, <0=에러 (-1=NULL, -2=no data, -3=read-only, -4=size overflow)
 */
int32_t AGR_OD_WriteValue(const AGR_OD_Entry_t* entry,
                      const void* in_buf,
                      uint16_t in_len);

/**
 *-----------------------------------------------------------
 * OD 무결성 (중복 index + type↔size 정합) — auto-sort 미채택 모듈까지 커버
 *-----------------------------------------------------------
 */

/**
 * @brief OD 테이블 debug 정합성 검사 (release=NDEBUG 는 no-op).
 * @details transport Init 이 자동 호출. 두 가지를 검사한다:
 *          1) 중복 (index,subindex) → AGR_OD_OnDuplicateIndex() weak hook.
 *          2) 스칼라 type↔size 정합 → AGR_OD_OnSizeMismatch() weak hook.
 *             size 는 스칼라 원소 크기의 0 아닌 배수여야 함 — 배수 허용은
 *             "원소 타입 + 총 바이트"로 배열을 노출하는 관례(예: FLOAT32/12
 *             = float[3], SAM3x 생성 OD) 지원. FLOAT32/2 같은 절단 오기는 검출.
 *             BLOB 은 가변이라 검사 제외.
 *          sort_buf 미제공(선형) slave 도 커버 → "정렬 인덱스 배선을 잊어도
 *          테이블 오류는 debug 서 드러난다"를 보장.
 * @note release 정책(부팅 정지/에러 LED)은 별도 결정 — 본 함수는 debug 검출 전용.
 */
void AGR_OD_DebugCheckUnique(const AGR_OD_Table_t* od);

/**
 * @brief 중복 index 발견 시 호출되는 weak hook (debug 전용, override 가능).
 * @details 기본 = 무한 루프(부팅서 halt → 디버거 정지). 소비 모듈이 강한 심볼로
 *          override 하여 LED/로그 처리 가능.
 */
void AGR_OD_OnDuplicateIndex(uint16_t index, uint8_t subindex);

/**
 * @brief 스칼라 type↔size 부정합 발견 시 호출되는 weak hook (debug 전용, override 가능).
 * @details 기본 = 무한 루프(부팅서 halt → 디버거 정지). 소비 모듈이 강한 심볼로
 *          override 하여 LED/로그 처리 가능.
 */
void AGR_OD_OnSizeMismatch(uint16_t index, uint8_t subindex);

/**
 * @brief 정렬 인덱스 버퍼를 품은 OD 테이블 초기화자 — transport Init 이 자동으로
 *        AGR_OD_BuildSortedIndex 실행(lookup O(log n) + 중복 index 검출).
 * @param entries_arr  static const AGR_OD_Entry_t[] 배열
 * @param sortbuf      static uint16_t[AGR_OD_ENTRY_COUNT(entries_arr)] (수명=od)
 * @code
 *   static uint16_t s_od_sortbuf[AGR_OD_ENTRY_COUNT(s_od_entries)];
 *   static const AGR_OD_Table_t s_od_table =
 *       AGR_OD_TABLE_WITH_SORT(s_od_entries, s_od_sortbuf);
 * @endcode
 */
#define AGR_OD_TABLE_WITH_SORT(entries_arr, sortbuf)    \
    {                                                   \
        .entries     = (entries_arr),                   \
        .entry_count = AGR_OD_ENTRY_COUNT(entries_arr), \
        .sorted_idx  = NULL,                            \
        .sort_buf    = (sortbuf),                       \
    }

#endif /* AGR_OD_H */
