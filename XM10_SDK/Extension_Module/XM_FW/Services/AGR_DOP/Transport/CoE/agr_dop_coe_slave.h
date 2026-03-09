/**
 ******************************************************************************
 * @file    agr_dop_coe_slave.h
 * @author  HyundoKim
 * @brief   AGR DOP CoE Slave Transport - EtherCAT Slave (SOES) 연동
 * @version 1.0
 * @date    Feb 26, 2026
 *
 * @details
 * CoE (CAN over EtherCAT) Slave Transport 계층.
 * Core (OD, SDO, PDO)를 EtherCAT Process Image / Mailbox로 감싸는 역할입니다.
 *
 * [CAN-FD Transport와의 차이]
 * - CAN-FD: CAN 프레임 직접 전송 (tx_func 콜백)
 * - CoE:    Process Image (PDO) + Mailbox (SDO) 기반 (SOES가 전송 담당)
 *
 * [SDO 듀얼 경로]
 * - 경로 A (ProcessSDORequest): SOES가 raw SDO 바이트를 그대로 전달
 * - 경로 B (SDORead/SDOWrite): SOES가 index/subindex를 파싱해서 전달
 * SOES 버전/콜백 구조에 따라 적합한 경로를 선택합니다.
 *
 * [의존성]
 * - Core: agr_od, agr_sdo_protocol, agr_pdo_engine (순수 C)
 * - SOES: Device 레이어에서 초기화, 이 파일에서는 참조하지 않음
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_COE_SLAVE_H
#define AGR_DOP_COE_SLAVE_H

#include "agr_dop_types.h"
#include "agr_dop_config.h"

#if AGR_DOP_TRANSPORT_COE

#include "Core/agr_od.h"
#include "Core/agr_sdo_protocol.h"
#include "Core/agr_pdo_engine.h"

/**
 *-----------------------------------------------------------
 * INITIALIZATION
 *-----------------------------------------------------------
 */

/**
 * @brief CoE Slave Transport Context 초기화
 * @param ctx     DOP Context 포인터
 * @param od      Object Dictionary 테이블
 * @param node_id 자신의 Node ID (EtherCAT Slave Station Address와 별개)
 * @return 0=성공, <0=에러
 *
 * @details
 * CAN-FD와 달리 tx_func이 불필요합니다 (Process Image 기반).
 * SOES가 EtherCAT 프레임 전송을 직접 담당합니다.
 */
int AGR_COE_Init(AGR_DOP_Ctx_t* ctx,
                 const AGR_OD_Table_t* od,
                 uint8_t node_id);

/**
 * @brief CoE Slave Transport Context 리셋 (PDO Mapping 초기화)
 * @param ctx DOP Context 포인터
 */
void AGR_COE_Reset(AGR_DOP_Ctx_t* ctx);

/**
 *-----------------------------------------------------------
 * SDO TRANSPORT — 경로 A: Raw Bytes (SOES Mailbox 직접 전달)
 *-----------------------------------------------------------
 */

/**
 * @brief SDO 요청 처리 (raw bytes → Core 위임 → raw bytes 응답)
 * @param ctx      DOP Context
 * @param req_data SOES에서 전달받은 SDO 요청 바이트 (CS+Index+SubIdx+Data)
 * @param req_len  요청 데이터 길이
 * @param rsp_data SDO 응답 바이트 (출력, 호출자가 SOES Mailbox로 전송)
 * @param rsp_len  응답 데이터 길이 (출력)
 * @return 0=성공, <0=에러
 *
 * @details
 * SOES가 Mailbox SDO 바이트를 그대로 전달하는 경우 사용합니다.
 * 내부적으로 AGR_SDO_Decode → on_sdo_request 콜백 → AGR_SDO_ProcessRequest
 * → AGR_SDO_Encode 순서로 처리합니다.
 */
int AGR_COE_ProcessSDORequest(AGR_DOP_Ctx_t* ctx,
                              const uint8_t* req_data,
                              uint8_t req_len,
                              uint8_t* rsp_data,
                              uint8_t* rsp_len);

/**
 *-----------------------------------------------------------
 * SDO TRANSPORT — 경로 B: Parsed (SOES가 index/subindex 파싱)
 *-----------------------------------------------------------
 */

/**
 * @brief SDO Read (Upload) — SOES ESC_objecthandler에서 호출
 * @param ctx      DOP Context
 * @param index    Object Index (SOES가 파싱)
 * @param subindex Sub-Index (SOES가 파싱)
 * @param out_data 읽은 데이터 (출력)
 * @param out_len  데이터 길이 (출력)
 * @return 0=성공, <0=에러
 */
int AGR_COE_SDORead(AGR_DOP_Ctx_t* ctx,
                    uint16_t index,
                    uint8_t subindex,
                    void* out_data,
                    uint8_t* out_len);

/**
 * @brief SDO Write (Download) — SOES ESC_objecthandler에서 호출
 * @param ctx      DOP Context
 * @param index    Object Index (SOES가 파싱)
 * @param subindex Sub-Index (SOES가 파싱)
 * @param data     쓸 데이터
 * @param len      데이터 길이
 * @return 0=성공, <0=에러
 */
int AGR_COE_SDOWrite(AGR_DOP_Ctx_t* ctx,
                     uint16_t index,
                     uint8_t subindex,
                     const void* data,
                     uint8_t len);

/**
 *-----------------------------------------------------------
 * PDO TRANSPORT — Process Image 기반
 *-----------------------------------------------------------
 * CAN-FD: PDO → CAN 프레임 → tx_func 전송
 * CoE:    PDO → Process Image 버퍼 → SOES가 EtherCAT SM으로 전송
 */

/**
 * @brief 개별 TX PDO 인코딩 (OD → Process Image)
 * @param ctx            DOP Context
 * @param pdo_num        PDO 번호 (1~4)
 * @param process_image  Process Image 출력 버퍼 (SM3 Input)
 * @param size           인코딩된 바이트 수 (출력)
 * @return 0=성공, <0=에러
 */
int AGR_COE_EncodeTxPDO(AGR_DOP_Ctx_t* ctx,
                        uint8_t pdo_num,
                        uint8_t* process_image,
                        uint8_t* size);

/**
 * @brief 개별 RX PDO 디코딩 (Process Image → OD)
 * @param ctx            DOP Context
 * @param pdo_num        PDO 번호 (1~4)
 * @param process_image  Process Image 입력 버퍼 (SM2 Output)
 * @param size           데이터 크기
 * @return 0=성공, <0=에러
 */
int AGR_COE_DecodeRxPDO(AGR_DOP_Ctx_t* ctx,
                        uint8_t pdo_num,
                        const uint8_t* process_image,
                        uint8_t size);

/**
 * @brief 전체 TX PDO 일괄 인코딩 (SM3 Input Process Image)
 * @param ctx            DOP Context
 * @param process_image  Process Image 출력 버퍼
 * @param buf_size       버퍼 크기
 * @param out_size       인코딩된 총 바이트 수 (출력)
 * @return 0=성공, <0=에러
 *
 * @details
 * TPDO1~4를 순서대로 Process Image에 연속 패킹합니다.
 * SYNC0 IRQ 핸들러에서 호출하여 SM3 Input 버퍼를 갱신합니다.
 */
int AGR_COE_EncodeAllTxPDO(AGR_DOP_Ctx_t* ctx,
                           uint8_t* process_image,
                           uint8_t buf_size,
                           uint8_t* out_size);

/**
 * @brief 전체 RX PDO 일괄 디코딩 (SM2 Output Process Image)
 * @param ctx            DOP Context
 * @param process_image  Process Image 입력 버퍼
 * @param size           데이터 크기
 * @return 0=성공, <0=에러
 *
 * @details
 * RPDO1~4를 순서대로 Process Image에서 연속 언패킹합니다.
 * SYNC0 IRQ 핸들러에서 호출하여 SM2 Output 버퍼를 읽습니다.
 */
int AGR_COE_DecodeAllRxPDO(AGR_DOP_Ctx_t* ctx,
                           const uint8_t* process_image,
                           uint8_t size);

/**
 *-----------------------------------------------------------
 * PROCESS IMAGE SIZE QUERY
 *-----------------------------------------------------------
 */

/**
 * @brief TX Process Image 총 크기 조회 (SM3 Input)
 * @param ctx DOP Context
 * @return 총 바이트 수 (TPDO1~4 합산)
 *
 * @details
 * SOES 초기화 시 SM3 크기 설정에 사용합니다.
 * ESI XML의 TxPDO Size와 일치해야 합니다.
 */
uint8_t AGR_COE_GetTxProcessImageSize(const AGR_DOP_Ctx_t* ctx);

/**
 * @brief RX Process Image 총 크기 조회 (SM2 Output)
 * @param ctx DOP Context
 * @return 총 바이트 수 (RPDO1~4 합산)
 *
 * @details
 * SOES 초기화 시 SM2 크기 설정에 사용합니다.
 * ESI XML의 RxPDO Size와 일치해야 합니다.
 */
uint8_t AGR_COE_GetRxProcessImageSize(const AGR_DOP_Ctx_t* ctx);

/**
 *-----------------------------------------------------------
 * EMERGENCY over CoE (Mailbox 경유)
 *-----------------------------------------------------------
 */

/**
 * @brief Emergency 데이터 준비 (CiA 301 포맷, 8바이트)
 * @param ctx            DOP Context
 * @param error_code     Emergency Error Code (CiA 301)
 * @param error_register Error Register (OD 0x1001)
 * @param out_data       Emergency 데이터 (출력, 8바이트)
 * @param out_len        데이터 길이 (출력, 항상 8)
 * @return 0=성공, <0=에러
 *
 * @details
 * CAN-FD: SendEmergency() → tx_func으로 직접 전송
 * CoE:    PrepareEmergency() → 데이터 준비만, SOES Mailbox API로 전송
 *
 * Emergency 데이터 포맷 (CiA 301, 8바이트):
 * [Error Code Lo][Error Code Hi][Error Register][Manufacturer Specific x 5]
 */
int AGR_COE_PrepareEmergency(AGR_DOP_Ctx_t* ctx,
                             uint16_t error_code,
                             uint8_t error_register,
                             uint8_t* out_data,
                             uint8_t* out_len);

#endif /* AGR_DOP_TRANSPORT_COE */

#endif /* AGR_DOP_COE_SLAVE_H */
