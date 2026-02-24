/**
 ******************************************************************************
 * @file    agr_dop.h
 * @author  HyundoKim
 * @brief   AGR-DOP V2 Main Header - Data Object Protocol API
 * @version 2.0
 * @date    Dec 2, 2025
 *
 * @details
 * AGR-DOP (Angel Robotics Data Object Protocol)은 CANopen을 참고하여 설계된
 * 경량 통신 프로토콜입니다. CANFD/EtherCAT에 최적화되어 있습니다.
 * 
 * [주요 기능]
 * - Object Dictionary (OD) 관리
 * - SDO (Service Data Object) 처리: 파라미터 읽기/쓰기
 * - PDO (Process Data Object) 처리: 실시간 데이터 송수신
 * - 동적 PDO Mapping: Master가 SDO로 PDO 구성 설정
 * 
 * [사용 순서]
 * 1. 드라이버에서 OD Entry 정의 (static const 배열)
 * 2. AGR_DOP_Init()으로 Context 초기화
 * 3. CAN 메시지 수신 시 AGR_DOP_ProcessRxMessage() 호출
 * 4. 주기적으로 AGR_DOP_SendPDO() 호출하여 PDO 전송
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_H
#define AGR_DOP_H

#include "agr_dop_config.h"
#include "agr_dop_types.h"
#include <string.h>

/**
 *-----------------------------------------------------------
 * INITIALIZATION API
 *-----------------------------------------------------------
 */

/**
 * @brief DOP Context 초기화
 * @param ctx      Context 포인터
 * @param od       Object Dictionary 테이블 (드라이버에서 정의)
 * @param node_id  자신의 Node ID
 * @param tx_func  CAN 전송 함수 (Dependency Injection)
 * @return 0=성공, <0=에러
 */
int AGR_DOP_Init(AGR_DOP_Ctx_t* ctx,
                 const AGR_OD_Table_t* od,
                 uint8_t node_id,
                 AGR_TxFunc_t tx_func);

/**
 * @brief DOP Context 리셋 (PDO Mapping 포함)
 * @param ctx Context 포인터
 */
void AGR_DOP_Reset(AGR_DOP_Ctx_t* ctx);

/**
 *-----------------------------------------------------------
 * OBJECT DICTIONARY API
 *-----------------------------------------------------------
 */

/**
 * @brief OD Entry 검색 (Index로)
 * @param ctx   Context 포인터
 * @param index Object Index
 * @return OD Entry 포인터, 없으면 NULL
 */
const AGR_OD_Entry_t* AGR_DOP_FindODEntry(const AGR_DOP_Ctx_t* ctx, uint16_t index);

/**
 * @brief OD Entry 검색 (Index + SubIndex로)
 * @param ctx      Context 포인터
 * @param index    Object Index
 * @param subindex Sub-Index
 * @return OD Entry 포인터, 없으면 NULL
 */
const AGR_OD_Entry_t* AGR_DOP_FindODEntryEx(const AGR_DOP_Ctx_t* ctx, 
                                             uint16_t index, 
                                             uint8_t subindex);

/**
 * @brief OD Entry 값 읽기
 * @param entry   OD Entry 포인터
 * @param out_buf 출력 버퍼
 * @param buf_len 버퍼 크기
 * @return 읽은 바이트 수, <0=에러
 */
int AGR_DOP_ReadODValue(const AGR_OD_Entry_t* entry, void* out_buf, uint8_t buf_len);

/**
 * @brief OD Entry 값 쓰기
 * @param entry   OD Entry 포인터
 * @param in_buf  입력 데이터
 * @param in_len  데이터 길이
 * @return 0=성공, <0=에러
 */
int AGR_DOP_WriteODValue(const AGR_OD_Entry_t* entry, const void* in_buf, uint8_t in_len);

/**
 *-----------------------------------------------------------
 * SDO API
 *-----------------------------------------------------------
 */

/**
 * @brief SDO 요청 메시지 생성 (Read)
 * @param out_msg 출력 SDO 메시지
 * @param index   Object Index
 * @param subindex Sub-Index
 */
void AGR_DOP_CreateSDOReadReq(AGR_SDO_Msg_t* out_msg, uint16_t index, uint8_t subindex);

/**
 * @brief SDO 요청 메시지 생성 (Write)
 * @param out_msg  출력 SDO 메시지
 * @param index    Object Index
 * @param subindex Sub-Index
 * @param data     쓸 데이터
 * @param data_len 데이터 길이
 */
void AGR_DOP_CreateSDOWriteReq(AGR_SDO_Msg_t* out_msg, 
                                uint16_t index, 
                                uint8_t subindex,
                                const void* data, 
                                uint8_t data_len);

/**
 * @brief SDO 요청 처리 (Slave로서 수신한 SDO 처리)
 * @param ctx     Context 포인터
 * @param req     수신한 SDO 요청
 * @param out_rsp 응답 SDO 메시지 (출력)
 * @return 0=성공, <0=에러
 */
int AGR_DOP_ProcessSDORequest(AGR_DOP_Ctx_t* ctx,
                               const AGR_SDO_Msg_t* req,
                               AGR_SDO_Msg_t* out_rsp);

/**
 * @brief SDO 메시지 인코딩 (CAN 프레임으로)
 * @param msg     SDO 메시지
 * @param out_buf 출력 버퍼 (최소 4 + data_len 바이트)
 * @return 인코딩된 바이트 수
 */
int AGR_DOP_EncodeSDO(const AGR_SDO_Msg_t* msg, uint8_t* out_buf);

/**
 * @brief SDO 메시지 디코딩 (CAN 프레임에서)
 * @param in_buf  입력 버퍼
 * @param in_len  버퍼 길이
 * @param out_msg 출력 SDO 메시지
 * @return 0=성공, <0=에러
 */
int AGR_DOP_DecodeSDO(const uint8_t* in_buf, uint8_t in_len, AGR_SDO_Msg_t* out_msg);

/**
 * @brief SDO 전송 (CAN으로)
 * @param ctx       Context 포인터
 * @param target_id 대상 Node ID
 * @param msg       SDO 메시지
 * @return 0=성공, <0=에러
 */
int AGR_DOP_SendSDO(AGR_DOP_Ctx_t* ctx, uint8_t target_id, const AGR_SDO_Msg_t* msg);

/**
 *-----------------------------------------------------------
 * PDO MAPPING API
 *-----------------------------------------------------------
 */

/**
 * @brief TX PDO Mapping Table 초기화 (Clear) - 모든 TPDO
 * @param ctx Context 포인터
 * @deprecated 멀티 TPDO 지원을 위해 AGR_DOP_ClearTxPDOMapN() 사용 권장
 */
void AGR_DOP_ClearTxPDOMap(AGR_DOP_Ctx_t* ctx);

/**
 * @brief TX PDO Mapping Table 초기화 (Clear) - 특정 TPDO
 * @param ctx Context 포인터
 * @param pdo_num PDO 번호 (1~4)
 */
void AGR_DOP_ClearTxPDOMapN(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num);

/**
 * @brief TX PDO Mapping Entry 추가 (TPDO1 전용)
 * @param ctx      Context 포인터
 * @param index    OD Entry Index
 * @param subindex OD Entry Sub-Index
 * @return 0=성공, -1=테이블 가득 참, -2=OD Entry 없음
 * @deprecated 멀티 TPDO 지원을 위해 AGR_DOP_AddTxPDOMapN() 사용 권장
 */
int AGR_DOP_AddTxPDOMap(AGR_DOP_Ctx_t* ctx, uint16_t index, uint8_t subindex);

/**
 * @brief TX PDO Mapping Entry 추가 - 특정 TPDO
 * @param ctx      Context 포인터
 * @param pdo_num  PDO 번호 (1~4)
 * @param index    OD Entry Index
 * @param subindex OD Entry Sub-Index
 * @return 0=성공, -1=테이블 가득 참, -2=OD Entry 없음
 */
int AGR_DOP_AddTxPDOMapN(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num, uint16_t index, uint8_t subindex);

/**
 * @brief SDO로 받은 PDO Mapping 데이터 적용
 * @param ctx      Context 포인터
 * @param data     SDO 데이터 (Index[2B] + SubIndex[1B] 쌍의 배열)
 * @param data_len 데이터 길이
 * @return 추가된 Entry 개수, <0=에러
 * 
 * @example
 * ```c
 * // SDO 콜백에서 호출
 * void OnSetPDOMapping(const AGR_SDO_Msg_t* req) {
 *     AGR_DOP_ApplyTxPDOMapFromSDO(&s_ctx, req->data, req->data_len);
 * }
 * ```
 */
int AGR_DOP_ApplyTxPDOMapFromSDO(AGR_DOP_Ctx_t* ctx, 
                                  const uint8_t* data, 
                                  uint8_t data_len);

/**
 * @brief SDO로 받은 PDO Mapping 데이터 적용 - 특정 TPDO
 * @param ctx      Context 포인터
 * @param pdo_num  PDO 번호 (1~4)
 * @param data     SDO 데이터 (Index[2B] + SubIndex[1B] 쌍의 배열)
 * @param data_len 데이터 길이
 * @return 추가된 Entry 개수, <0=에러
 * 
 * @example
 * ```c
 * // TPDO1: IMU 0,1 전체 (0x6000.0x62, 0x6001.0x62)
 * uint8_t map1[] = { 0x00, 0x60, 0x62, 0x01, 0x60, 0x62 };
 * AGR_DOP_ApplyTxPDOMapFromSDON(&ctx, 1, map1, sizeof(map1));
 * 
 * // TPDO2: IMU 2,3 전체 (0x6002.0x62, 0x6003.0x62)
 * uint8_t map2[] = { 0x02, 0x60, 0x62, 0x03, 0x60, 0x62 };
 * AGR_DOP_ApplyTxPDOMapFromSDON(&ctx, 2, map2, sizeof(map2));
 * ```
 */
int AGR_DOP_ApplyTxPDOMapFromSDON(AGR_DOP_Ctx_t* ctx, 
                                   uint8_t pdo_num,
                                   const uint8_t* data, 
                                   uint8_t data_len);

/**
 * @brief RX PDO Mapping Table 초기화 (Clear)
 * @param ctx Context 포인터
 */
void AGR_DOP_ClearRxPDOMap(AGR_DOP_Ctx_t* ctx);

/**
 * @brief RX PDO Mapping Entry 추가
 * @param ctx      Context 포인터
 * @param index    OD Entry Index
 * @param subindex OD Entry Sub-Index
 * @return 0=성공, -1=테이블 가득 참, -2=OD Entry 없음
 */
int AGR_DOP_AddRxPDOMap(AGR_DOP_Ctx_t* ctx, uint16_t index, uint8_t subindex);

/**
 *-----------------------------------------------------------
 * PDO ENCODE/DECODE API
 *-----------------------------------------------------------
 */

/**
 * @brief TX PDO 페이로드 생성 (Mapping Table 기준) - TPDO1 전용
 * @param ctx      Context 포인터
 * @param out_buf  출력 버퍼
 * @param buf_size 버퍼 크기
 * @return 생성된 페이로드 바이트 수, <0=에러
 * @deprecated 멀티 TPDO 지원을 위해 AGR_DOP_EncodeTxPDON() 사용 권장
 * 
 * @details
 * tx_pdo_map[0]에 등록된 OD Entry들의 값을 순서대로 패킹합니다.
 */
int AGR_DOP_EncodeTxPDO(const AGR_DOP_Ctx_t* ctx, uint8_t* out_buf, uint8_t buf_size);

/**
 * @brief TX PDO 페이로드 생성 - 특정 TPDO
 * @param ctx      Context 포인터
 * @param pdo_num  PDO 번호 (1~4)
 * @param out_buf  출력 버퍼
 * @param buf_size 버퍼 크기
 * @return 생성된 페이로드 바이트 수, <0=에러
 * 
 * @details
 * tx_pdo_map[pdo_num-1]에 등록된 OD Entry들의 값을 순서대로 패킹합니다.
 * 
 * @example
 * ```c
 * // TPDO1 전송
 * uint8_t pdo_buf[64];
 * int len = AGR_DOP_EncodeTxPDON(&s_ctx, 1, pdo_buf, sizeof(pdo_buf));
 * if (len > 0) {
 *     uint32_t can_id = AGR_DOP_GetTPDOCANID(node_id, 1);
 *     CAN_Transmit(can_id, pdo_buf, len);
 * }
 * ```
 */
int AGR_DOP_EncodeTxPDON(const AGR_DOP_Ctx_t* ctx, uint8_t pdo_num, uint8_t* out_buf, uint8_t buf_size);

/**
 * @brief RX PDO 페이로드 디코딩 (Mapping Table 기준)
 * @param ctx    Context 포인터
 * @param in_buf 입력 버퍼
 * @param in_len 버퍼 길이
 * @return 디코딩된 바이트 수, <0=에러
 * 
 * @details
 * rx_pdo_map에 등록된 OD Entry들에 순서대로 값을 언패킹합니다.
 */
int AGR_DOP_DecodeRxPDO(AGR_DOP_Ctx_t* ctx, const uint8_t* in_buf, uint8_t in_len);

/**
 *-----------------------------------------------------------
 * MESSAGE PROCESSING API
 *-----------------------------------------------------------
 */

/**
 * @brief CAN 메시지 처리 (수신 시 호출)
 * @param ctx    Context 포인터
 * @param can_id 수신된 CAN-ID
 * @param data   데이터 버퍼
 * @param len    데이터 길이
 * @return 0=처리됨, 1=해당 없음, <0=에러
 * 
 * @details
 * CAN-ID를 분석하여 SDO/PDO/NMT 메시지를 자동 분류하고 처리합니다.
 */
int AGR_DOP_ProcessRxMessage(AGR_DOP_Ctx_t* ctx, 
                              uint32_t can_id, 
                              const uint8_t* data, 
                              uint8_t len);

/**
 * @brief TX PDO 전송
 * @param ctx    Context 포인터
 * @param pdo_num PDO 번호 (1~4)
 * @return 0=성공, <0=에러
 */
int AGR_DOP_SendTxPDO(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num);

/**
 *-----------------------------------------------------------
 * CAN-ID UTILITY API
 *-----------------------------------------------------------
 */

/**
 * @brief SDO Request CAN-ID 계산
 * @param node_id 대상 Node ID
 * @return CAN-ID (0x600 + node_id)
 */
static inline uint32_t AGR_DOP_GetSDORequestCANID(uint8_t node_id) {
    return AGR_CAN_ID_SDO_RX + node_id;
}

/**
 * @brief SDO Response CAN-ID 계산
 * @param node_id 자신의 Node ID
 * @return CAN-ID (0x580 + node_id)
 */
static inline uint32_t AGR_DOP_GetSDOResponseCANID(uint8_t node_id) {
    return AGR_CAN_ID_SDO_TX + node_id;
}

/**
 * @brief TPDO CAN-ID 계산
 * @param node_id 자신의 Node ID
 * @param pdo_num PDO 번호 (1~4)
 * @return CAN-ID
 */
static inline uint32_t AGR_DOP_GetTPDOCANID(uint8_t node_id, uint8_t pdo_num) {
    const uint32_t base[] = {0, AGR_CAN_ID_TPDO1, AGR_CAN_ID_TPDO2, 
                              AGR_CAN_ID_TPDO3, AGR_CAN_ID_TPDO4};
    if (pdo_num < 1 || pdo_num > 4) return 0;
    return base[pdo_num] + node_id;
}

/**
 * @brief RPDO CAN-ID 계산
 * @param node_id 자신의 Node ID
 * @param pdo_num PDO 번호 (1~4)
 * @return CAN-ID
 */
static inline uint32_t AGR_DOP_GetRPDOCANID(uint8_t node_id, uint8_t pdo_num) {
    const uint32_t base[] = {0, AGR_CAN_ID_RPDO1, AGR_CAN_ID_RPDO2, 
                              AGR_CAN_ID_RPDO3, AGR_CAN_ID_RPDO4};
    if (pdo_num < 1 || pdo_num > 4) return 0;
    return base[pdo_num] + node_id;
}

/**
 * @brief Heartbeat CAN-ID 계산
 * @param node_id 자신의 Node ID
 * @return CAN-ID (0x700 + node_id)
 */
static inline uint32_t AGR_DOP_GetHeartbeatCANID(uint8_t node_id) {
    return AGR_CAN_ID_HEARTBEAT + node_id;
}

/**
 *-----------------------------------------------------------
 * SYNC API (CANopen 동기화 객체)
 *-----------------------------------------------------------
 */

/**
 * @brief SYNC 기능 활성화
 * @param ctx DOP Context
 * @param period_us SYNC 주기 (μs, Master용)
 * @param on_sync SYNC 수신 시 콜백 (ISR에서 호출됨)
 * @details
 * Slave 모드: SYNC 수신 시 on_sync 콜백 호출 → 즉시 PDO 전송
 * Master 모드: period_us마다 SYNC 전송 (AGR_DOP_SendSYNC 호출 필요)
 */
void AGR_DOP_EnableSync(AGR_DOP_Ctx_t* ctx, 
                        uint32_t period_us,
                        void (*on_sync)(void* ctx));

/**
 * @brief SYNC 기능 비활성화
 * @param ctx DOP Context
 */
void AGR_DOP_DisableSync(AGR_DOP_Ctx_t* ctx);

/**
 * @brief SYNC 메시지 전송 (Master 모드)
 * @param ctx DOP Context
 * @return 0=성공, <0=에러
 * @details
 * CAN-ID: 0x080 (고정)
 * Data: [sync_counter] (1 byte, Optional)
 */
int AGR_DOP_SendSYNC(AGR_DOP_Ctx_t* ctx);

/**
 *-----------------------------------------------------------
 * EMERGENCY API (긴급 메시지)
 *-----------------------------------------------------------
 */

/**
 * @brief Emergency 기능 활성화
 * @param ctx DOP Context
 * @param on_emergency Emergency 수신 시 콜백
 */
void AGR_DOP_EnableEmergency(AGR_DOP_Ctx_t* ctx,
                              void (*on_emergency)(uint16_t error_code, void* ctx));

/**
 * @brief Emergency 메시지 전송
 * @param ctx DOP Context
 * @param error_code Emergency Error Code (CiA 301 표준)
 *   - 0x0000: Error Reset
 *   - 0x1000: Generic Error
 *   - 0x81xx~0x8Fxx: Device Specific (Angel Robotics)
 * @return 0=성공, <0=에러
 * @details
 * CAN-ID: 0x080 + node_id
 * Data: [Error Code Low, Error Code High, Error Register, Manufacturer Data...]
 */
int AGR_DOP_SendEmergency(AGR_DOP_Ctx_t* ctx, uint16_t error_code);

/**
 *-----------------------------------------------------------
 * PDO INHIBIT TIME API (전송 간격 제한)
 *-----------------------------------------------------------
 */

/**
 * @brief PDO Inhibit Time 설정
 * @param ctx DOP Context
 * @param pdo_num PDO 번호 (1~4)
 * @param inhibit_time_us 최소 전송 간격 (μs)
 * @details
 * PDO 전송 시 최소 간격을 강제합니다.
 * 예: inhibit_time_us = 1000 → 최소 1ms 간격으로만 전송
 * 
 * @example
 * ```c
 * // TPDO1을 최소 1ms 간격으로만 전송
 * AGR_DOP_SetPDOInhibitTime(&ctx, 1, 1000);
 * ```
 */
void AGR_DOP_SetPDOInhibitTime(AGR_DOP_Ctx_t* ctx, 
                                uint8_t pdo_num, 
                                uint32_t inhibit_time_us);

/**
 * @brief PDO Inhibit Time 비활성화
 * @param ctx DOP Context
 * @param pdo_num PDO 번호 (1~4)
 */
void AGR_DOP_DisablePDOInhibit(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num);

/**
 * @brief PDO 전송 가능 여부 확인 (Inhibit Time 체크)
 * @param ctx DOP Context
 * @param pdo_num PDO 번호 (1~4)
 * @param current_time_us 현재 시각 (μs)
 * @return true=전송 가능, false=Inhibit Time 때문에 전송 불가
 * @details
 * PDO 전송 전에 호출하여 전송 가능 여부를 확인합니다.
 * Inhibit Time이 경과하지 않았으면 false를 반환합니다.
 */
bool AGR_DOP_CanSendPDO(AGR_DOP_Ctx_t* ctx, 
                         uint8_t pdo_num, 
                         uint32_t current_time_us);

/**
 * @brief PDO 전송 완료 기록 (Inhibit Time 업데이트)
 * @param ctx DOP Context
 * @param pdo_num PDO 번호 (1~4)
 * @param current_time_us 현재 시각 (μs)
 * @details
 * PDO 전송 후 호출하여 마지막 전송 시각을 기록합니다.
 */
void AGR_DOP_MarkPDOSent(AGR_DOP_Ctx_t* ctx, 
                          uint8_t pdo_num, 
                          uint32_t current_time_us);

/**
 *-----------------------------------------------------------
 * SDO TRANSMISSION API
 *-----------------------------------------------------------
 */

/**
 * @brief 통신 대상 Node ID 설정
 * @param ctx DOP Context
 * @param target_node_id 통신 대상 Node ID
 * @details SDO 전송 시 사용할 상대방 Node ID를 설정합니다.
 */
void AGR_DOP_SetTargetNodeId(AGR_DOP_Ctx_t* ctx, uint8_t target_node_id);

/**
 * @brief 단일 SDO Write Request 전송
 * @param ctx DOP Context
 * @param index OD Index
 * @param subindex OD Subindex
 * @param data 데이터 포인터
 * @param data_len 데이터 길이
 * @return 0=성공, <0=실패
 */
int AGR_DOP_SendSDOWrite(AGR_DOP_Ctx_t* ctx, 
                         uint16_t index, 
                         uint8_t subindex,
                         const void* data, 
                         uint8_t data_len);

#endif /* AGR_DOP_H */
