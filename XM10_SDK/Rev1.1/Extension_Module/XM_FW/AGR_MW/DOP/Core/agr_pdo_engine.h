/**
 ******************************************************************************
 * @file    agr_pdo_engine.h
 * @author  HyundoKim
 * @brief   AGR PDO Engine - Core API (Transport-Agnostic)
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * DOP Context에 의존하지 않는 순수 PDO 매핑/인코딩/디코딩/Inhibit API입니다.
 * AGR_PDO_MapTable_t, AGR_OD_Table_t, AGR_PDO_Inhibit_t를 직접 받아 처리합니다.
 *
 * [사용 주체]
 * - Slave 모듈 (SM-IMU, SM-EMG, SM-FES): TPDO Encode + RPDO Decode에 사용
 *   → AGR_CANFD_ProcessRxMessage() 내부에서 AGR_PDO_Decode() 호출
 * - Master 모듈 (XM, CM-WH): TPDO 수신에는 사용하지 않음
 *   → Master는 각 Device Driver에서 raw CAN 데이터를 직접 디코딩
 *   → Master의 DOP Context는 SDO 전송(Pre-Op 설정)에만 사용
 *   → 이유: Master는 여러 Slave의 서로 다른 TPDO 레이아웃을 수신하며,
 *     본 엔진은 단일 노드의 단일 OD 기준 설계
 *
 * [PDO Mapping Wire Format]
 *
 * data[0] = Header byte
 *   bit 7     = Extended flag (0 = Legacy 4B, 1 = Extended 8B)
 *   bit 0..6  = Number of mapped objects (N, max 127)
 *
 * Legacy 4B entry (CiA 301, bit_length ≤ 255 bits = 31.875 B):
 *   byte 0   = BitLength (bits)
 *   byte 1   = SubIndex
 *   byte 2-3 = Index (Little Endian)
 *
 * Extended 8B entry (AGR, length_bytes up to 4 GB):
 *   byte 0-1 = Index (Little Endian)
 *   byte 2   = SubIndex
 *   byte 3   = Flags (bit 0 = signed, bit 1..7 = reserved)
 *   byte 4-7 = length_bytes (Little Endian uint32)
 *
 * @note AGR-DOP 는 Transport-Agnostic (CAN-FD/CoE/UDP/Serial) 이며,
 *       Extended 포맷이 기본. Legacy 포맷은 CiA 301 호환 목적의
 *       하위 호환 지원 (deprecation 예정, CLAUDE.md 정책 참조).
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_PDO_ENGINE_H
#define AGR_PDO_ENGINE_H

#include "agr_dop_types.h"
#include "agr_od.h"

/**
 *-----------------------------------------------------------
 * PDO MAPPING API
 *-----------------------------------------------------------
 */

/**
 * @brief PDO Mapping Table 초기화 (Clear)
 * @param map Mapping Table 포인터
 */
void AGR_PDO_ClearMap(AGR_PDO_MapTable_t* map);

/**
 * @brief PDO Mapping Entry 추가
 * @param map      Mapping Table 포인터
 * @param od       Object Dictionary 테이블 (OD 검증용)
 * @param index    OD Entry Index
 * @param subindex OD Entry Sub-Index
 * @return 0=성공, -1=테이블 가득 참 또는 파라미터 에러, -2=OD Entry 없음
 *
 * @details
 * OD Entry를 검색하여 존재 여부를 확인하고, length_bytes를 entry->size로 채웁니다.
 * 동일한 (index, subindex)가 이미 있으면 중복 무시하고 0을 반환합니다.
 */
int32_t AGR_PDO_AddMap(AGR_PDO_MapTable_t* map,
                   const AGR_OD_Table_t* od,
                   uint16_t index,
                   uint8_t subindex);

/**
 * @brief PDO Mapping Entry 추가 (length_bytes 명시)
 * @param map          Mapping Table 포인터
 * @param od           Object Dictionary 테이블 (OD 검증용)
 * @param index        OD Entry Index
 * @param subindex     OD Entry Sub-Index
 * @param length_bytes 매핑 크기 (bytes). 0 이면 entry->size 를 사용.
 * @param flags        Mapping flags (bit 0: signed, bit 1..7: reserved)
 * @return 0=성공, -1=파라미터 에러/가득 참, -2=OD Entry 없음
 *
 * @details
 * BLOB 타입 OD Entry (예: 37B 구조체 TPDO payload) 를 단일 Mapping
 * Entry 로 선언하기 위한 Extended API. AGR_PDO_AddMap() 은 내부에서
 * 본 함수를 length_bytes=0 으로 호출한 것과 동일.
 */
int32_t AGR_PDO_AddMapExt(AGR_PDO_MapTable_t* map,
                      const AGR_OD_Table_t* od,
                      uint16_t index,
                      uint8_t subindex,
                      uint32_t length_bytes,
                      uint8_t flags);

/**
 * @brief SDO 데이터에서 PDO Mapping 적용 (Legacy 4B / Extended 8B 자동 판별)
 * @param map      Mapping Table 포인터 (기존 매핑 클리어 후 적용)
 * @param od       Object Dictionary 테이블
 * @param data     SDO 데이터 (헤더 1B + Entry N 개)
 * @param data_len 데이터 길이
 * @return 추가된 Entry 개수, <0=에러
 *
 * @details
 * data[0] 의 bit 7 (Extended flag) 로 포맷 판별:
 *   - 0: Legacy 4B entry (CiA 301 호환)
 *   - 1: Extended 8B entry (AGR 확장)
 *
 * 기존 매핑을 클리어한 뒤 SDO 데이터로 새로 구성합니다.
 */
int32_t AGR_PDO_ApplyMapFromSDO(AGR_PDO_MapTable_t* map,
                            const AGR_OD_Table_t* od,
                            const uint8_t* data,
                            uint8_t data_len);

/**
 *-----------------------------------------------------------
 * PDO ENCODE / DECODE API
 *-----------------------------------------------------------
 */

/**
 * @brief PDO 페이로드 인코딩 (Mapping Table → 바이트 버퍼)
 * @param map      Mapping Table 포인터
 * @param od       Object Dictionary 테이블
 * @param out_buf  출력 버퍼
 * @param buf_size 버퍼 크기
 * @return 생성된 페이로드 바이트 수, <0=에러
 *
 * @details
 * Mapping Table에 등록된 OD Entry들의 값을 순서대로 패킹합니다.
 */
int32_t AGR_PDO_Encode(const AGR_PDO_MapTable_t* map,
                   const AGR_OD_Table_t* od,
                   uint8_t* out_buf,
                   uint8_t buf_size);

/**
 * @brief PDO 페이로드 디코딩 (바이트 버퍼 → OD Entry)
 * @param map    Mapping Table 포인터
 * @param od     Object Dictionary 테이블
 * @param in_buf 입력 버퍼
 * @param in_len 버퍼 길이
 * @return 디코딩된 바이트 수, <0=에러
 *
 * @details
 * Mapping Table에 등록된 OD Entry들에 순서대로 값을 언패킹합니다.
 * 읽기 전용(RO) Entry는 건너뛰고, 쓰기 완료 콜백(on_write)을 호출합니다.
 */
int32_t AGR_PDO_Decode(const AGR_PDO_MapTable_t* map,
                   const AGR_OD_Table_t* od,
                   const uint8_t* in_buf,
                   uint8_t in_len);

/**
 *-----------------------------------------------------------
 * PDO INHIBIT TIME API
 *-----------------------------------------------------------
 */

/**
 * @brief PDO Inhibit Time 설정
 * @param inhibit Inhibit 제어 구조체
 * @param inhibit_time_us 최소 전송 간격 (μs)
 */
void AGR_PDO_SetInhibitTime(AGR_PDO_Inhibit_t* inhibit,
                            uint32_t inhibit_time_us);

/**
 * @brief PDO Inhibit Time 비활성화
 * @param inhibit Inhibit 제어 구조체
 */
void AGR_PDO_DisableInhibit(AGR_PDO_Inhibit_t* inhibit);

/**
 * @brief PDO 전송 가능 여부 확인
 * @param inhibit Inhibit 제어 구조체
 * @param current_time_us 현재 시각 (μs)
 * @return true=전송 가능, false=Inhibit Time 미경과
 */
bool AGR_PDO_CanSend(const AGR_PDO_Inhibit_t* inhibit,
                     uint32_t current_time_us);

/**
 * @brief PDO 전송 완료 기록 (Inhibit Time 타이머 갱신)
 * @param inhibit Inhibit 제어 구조체
 * @param current_time_us 현재 시각 (μs)
 */
void AGR_PDO_MarkSent(AGR_PDO_Inhibit_t* inhibit,
                      uint32_t current_time_us);

#endif /* AGR_PDO_ENGINE_H */
