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
 * [PDO Mapping Format - CANopen 표준 4B]
 * data[0]   = Number of mapped objects (1B)
 * data[1]   = BitLength  (object 1)
 * data[2]   = SubIndex   (object 1)
 * data[3-4] = Index      (object 1, Little Endian)
 * data[5]   = BitLength  (object 2)
 * ...
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
 * OD Entry를 검색하여 존재 여부를 확인하고, bit_length를 자동으로 채웁니다.
 * 동일한 (index, subindex)가 이미 있으면 중복 무시하고 0을 반환합니다.
 */
int AGR_PDO_AddMap(AGR_PDO_MapTable_t* map,
                   const AGR_OD_Table_t* od,
                   uint16_t index,
                   uint8_t subindex);

/**
 * @brief SDO 데이터에서 PDO Mapping 적용 (4B Format)
 * @param map      Mapping Table 포인터 (기존 매핑 클리어 후 적용)
 * @param od       Object Dictionary 테이블
 * @param data     SDO 데이터 (4B CANopen PDO Mapping Format)
 * @param data_len 데이터 길이
 * @return 추가된 Entry 개수, <0=에러
 *
 * @details
 * [4B PDO Mapping Format]
 * data[0]: Number of mapped objects
 * data[1+n*4]: BitLength (bits)
 * data[2+n*4]: SubIndex
 * data[3+n*4 .. 4+n*4]: Index (Little Endian)
 *
 * 기존 매핑을 클리어한 뒤 SDO 데이터로 새로 구성합니다.
 */
int AGR_PDO_ApplyMapFromSDO(AGR_PDO_MapTable_t* map,
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
int AGR_PDO_Encode(const AGR_PDO_MapTable_t* map,
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
int AGR_PDO_Decode(const AGR_PDO_MapTable_t* map,
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
