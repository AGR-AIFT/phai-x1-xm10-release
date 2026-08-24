/**
 ******************************************************************************
 * @file    agr_sdo_protocol.h
 * @author  HyundoKim
 * @brief   AGR SDO Protocol - Core API (Transport-Agnostic)
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * CAN-ID / Transport에 의존하지 않는 순수 SDO 프로토콜 처리 API입니다.
 * SDO 메시지 생성, 인코딩/디코딩, OD 기반 요청 처리를 제공합니다.
 *
 * [CANopen SDO 프레임 구조 - CiA 301]
 * | Byte 0  | Byte 1-2 | Byte 3   | Byte 4~63 (CANFD) |
 * | CS      | Index    | SubIndex | Data               |
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_SDO_PROTOCOL_H
#define AGR_SDO_PROTOCOL_H

#include "agr_dop_types.h"
#include "agr_od.h"

/**
 *-----------------------------------------------------------
 * SDO MESSAGE CREATE API
 *-----------------------------------------------------------
 */

/**
 * @brief SDO Read 요청 메시지 생성 (Upload Initiate Request)
 * @param out_msg  출력 SDO 메시지
 * @param index    Object Index
 * @param subindex Sub-Index
 */
void AGR_SDO_CreateReadReq(AGR_SDO_Msg_t* out_msg,
                           uint16_t index,
                           uint8_t subindex);

/**
 * @brief SDO Write 요청 메시지 생성 (Download Initiate Request)
 * @param out_msg  출력 SDO 메시지
 * @param index    Object Index
 * @param subindex Sub-Index
 * @param data     쓸 데이터
 * @param data_len 데이터 길이
 */
void AGR_SDO_CreateWriteReq(AGR_SDO_Msg_t* out_msg,
                            uint16_t index,
                            uint8_t subindex,
                            const void* data,
                            uint8_t data_len);

/**
 * @brief SDO Abort 응답 생성 (CANopen 표준)
 * @param out_rsp    출력 SDO 메시지
 * @param index      Object Index
 * @param subindex   Sub-Index
 * @param abort_code Abort Code (CiA 301)
 */
void AGR_SDO_CreateAbortResponse(AGR_SDO_Msg_t* out_rsp,
                                 uint16_t index,
                                 uint8_t subindex,
                                 AGR_SDO_AbortCode_t abort_code);

/**
 *-----------------------------------------------------------
 * SDO PROCESS API (Slave-side)
 *-----------------------------------------------------------
 */

/**
 * @brief SDO 요청 처리 (OD 기반, transport-agnostic)
 * @param od      Object Dictionary 테이블
 * @param req     수신한 SDO 요청
 * @param out_rsp 응답 SDO 메시지 (출력)
 * @return 0=성공, <0=에러
 *
 * @details
 * CANopen Command Specifier에 따라 Upload(Read) / Download(Write)를 분류하고,
 * OD Entry를 조회하여 응답을 생성합니다.
 * DOP-level 콜백(on_sdo_request)은 상위 Facade에서 처리합니다.
 */
int AGR_SDO_ProcessRequest(const AGR_OD_Table_t* od,
                           const AGR_SDO_Msg_t* req,
                           AGR_SDO_Msg_t* out_rsp);

/**
 *-----------------------------------------------------------
 * SDO ENCODE / DECODE API
 *-----------------------------------------------------------
 */

/**
 * @brief SDO 메시지 인코딩 (바이트 버퍼로)
 * @param msg     SDO 메시지
 * @param out_buf 출력 버퍼 (최소 4 + data_len 바이트)
 * @return 인코딩된 바이트 수, <0=에러
 */
int AGR_SDO_Encode(const AGR_SDO_Msg_t* msg, uint8_t* out_buf);

/**
 * @brief SDO 메시지 디코딩 (바이트 버퍼에서)
 * @param in_buf  입력 버퍼
 * @param in_len  버퍼 길이
 * @param out_msg 출력 SDO 메시지
 * @return 0=성공, <0=에러
 */
int AGR_SDO_Decode(const uint8_t* in_buf,
                   uint8_t in_len,
                   AGR_SDO_Msg_t* out_msg);

#endif /* AGR_SDO_PROTOCOL_H */
