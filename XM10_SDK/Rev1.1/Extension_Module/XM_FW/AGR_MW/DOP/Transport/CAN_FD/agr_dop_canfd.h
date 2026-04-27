/**
 ******************************************************************************
 * @file    agr_dop_canfd.h
 * @author  HyundoKim
 * @brief   AGR DOP CAN-FD Transport - CAN-ID, Frame, Message Processing
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * CAN-FD Transport 계층: CAN-ID 계산, CAN 프레임 생성, RX 메시지 라우팅.
 * Core (OD, SDO, PDO) 모듈을 CAN-FD 프레임으로 감싸는 역할입니다.
 *
 * [의존성]
 * - Core: agr_od, agr_sdo_protocol, agr_pdo_engine (순수 C)
 * - AGR_TxFunc_t: CAN 전송 함수 (Dependency Injection)
 *
 * [Sensor Module 직접 사용 가능]
 * Facade(agr_dop.h) 없이 이 헤더를 직접 include하여 사용할 수 있습니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_CANFD_H
#define AGR_DOP_CANFD_H

#include "agr_dop_types.h"
#include "agr_dop_config.h"
#include "Core/agr_od.h"
#include "Core/agr_sdo_protocol.h"
#include "Core/agr_pdo_engine.h"

/**
 *-----------------------------------------------------------
 * CAN-ID UTILITY (CANopen CiA 301 Standard)
 *-----------------------------------------------------------
 */

/** @brief SDO Request CAN-ID (0x600 + node_id) */
static inline uint32_t AGR_CANFD_GetSDORequestID(uint8_t node_id) {
    return AGR_CAN_ID_SDO_RX + node_id;
}

/** @brief SDO Response CAN-ID (0x580 + node_id) */
static inline uint32_t AGR_CANFD_GetSDOResponseID(uint8_t node_id) {
    return AGR_CAN_ID_SDO_TX + node_id;
}

/** @brief TPDO CAN-ID (0x180/0x280/0x380/0x480 + node_id) */
static inline uint32_t AGR_CANFD_GetTPDOID(uint8_t node_id, uint8_t pdo_num) {
    const uint32_t base[] = {0, AGR_CAN_ID_TPDO1, AGR_CAN_ID_TPDO2,
                              AGR_CAN_ID_TPDO3, AGR_CAN_ID_TPDO4};
    if (pdo_num < 1 || pdo_num > 4) return 0;
    return base[pdo_num] + node_id;
}

/** @brief RPDO CAN-ID (0x200/0x300/0x400/0x500 + node_id) */
static inline uint32_t AGR_CANFD_GetRPDOID(uint8_t node_id, uint8_t pdo_num) {
    const uint32_t base[] = {0, AGR_CAN_ID_RPDO1, AGR_CAN_ID_RPDO2,
                              AGR_CAN_ID_RPDO3, AGR_CAN_ID_RPDO4};
    if (pdo_num < 1 || pdo_num > 4) return 0;
    return base[pdo_num] + node_id;
}

/** @brief Heartbeat CAN-ID (0x700 + node_id) */
static inline uint32_t AGR_CANFD_GetHeartbeatID(uint8_t node_id) {
    return AGR_CAN_ID_HEARTBEAT + node_id;
}

/**
 *-----------------------------------------------------------
 * INITIALIZATION
 *-----------------------------------------------------------
 */

/**
 * @brief CAN-FD Transport Context 초기화
 * @param ctx      DOP Context 포인터
 * @param od       Object Dictionary 테이블
 * @param node_id  자신의 Node ID (0x01~0x7F)
 * @param tx_func  CAN 전송 함수 (Dependency Injection)
 * @return 0=성공, <0=에러
 */
int AGR_CANFD_Init(AGR_DOP_Ctx_t* ctx,
                   const AGR_OD_Table_t* od,
                   uint8_t node_id,
                   AGR_TxFunc_t tx_func);

/**
 * @brief CAN-FD Transport Context 리셋 (PDO Mapping 초기화)
 * @param ctx DOP Context 포인터
 */
void AGR_CANFD_Reset(AGR_DOP_Ctx_t* ctx);

/**
 *-----------------------------------------------------------
 * MESSAGE PROCESSING (RX)
 *-----------------------------------------------------------
 */

/**
 * @brief CAN-FD 수신 메시지 처리 (Function Code 기반 라우팅)
 * @param ctx    DOP Context
 * @param can_id CAN-ID (11-bit)
 * @param data   수신 데이터
 * @param len    데이터 길이
 * @return 0=처리 완료, 1=해당 없음, <0=에러
 *
 * @details
 * CAN-ID를 Function Code(상위 4bit)와 Node ID(하위 7bit)로 분리하여
 * SDO/PDO/SYNC/EMCY 메시지를 자동 분류하고 Core 모듈에 위임합니다.
 */
int AGR_CANFD_ProcessRxMessage(AGR_DOP_Ctx_t* ctx,
                               uint32_t can_id,
                               const uint8_t* data,
                               uint8_t len);

/**
 *-----------------------------------------------------------
 * SDO TRANSPORT (TX)
 *-----------------------------------------------------------
 */

/**
 * @brief SDO 요청 처리 (on_sdo_request 콜백 우선 + Core 위임)
 * @param ctx     DOP Context
 * @param req     수신한 SDO 요청
 * @param out_rsp 응답 SDO 메시지 (출력)
 * @return 0=성공, <0=에러
 */
int AGR_CANFD_ProcessSDORequest(AGR_DOP_Ctx_t* ctx,
                                const AGR_SDO_Msg_t* req,
                                AGR_SDO_Msg_t* out_rsp);

/**
 * @brief SDO 메시지 전송 (CAN-ID 자동 결정)
 * @param ctx       DOP Context
 * @param target_id 대상 Node ID
 * @param msg       SDO 메시지
 * @return 0=성공, <0=에러
 */
int AGR_CANFD_SendSDO(AGR_DOP_Ctx_t* ctx,
                      uint8_t target_id,
                      const AGR_SDO_Msg_t* msg);

/**
 * @brief 단일 SDO Write 전송 (편의 함수)
 * @param ctx      DOP Context
 * @param index    OD Index
 * @param subindex OD Sub-Index
 * @param data     데이터 포인터
 * @param data_len 데이터 길이
 * @return 0=성공, <0=에러
 */
int AGR_CANFD_SendSDOWrite(AGR_DOP_Ctx_t* ctx,
                           uint16_t index,
                           uint8_t subindex,
                           const void* data,
                           uint8_t data_len);

/**
 * @brief 통신 대상 Node ID 설정
 * @param ctx            DOP Context
 * @param target_node_id 대상 Node ID
 */
void AGR_CANFD_SetTargetNodeId(AGR_DOP_Ctx_t* ctx, uint8_t target_node_id);

/**
 *-----------------------------------------------------------
 * PDO TRANSPORT (TX)
 *-----------------------------------------------------------
 */

/**
 * @brief TX PDO 전송 (Core Encode + CAN-ID + tx_func)
 * @param ctx     DOP Context
 * @param pdo_num PDO 번호 (1~4)
 * @return 0=성공, <0=에러
 */
int AGR_CANFD_SendTxPDO(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num);

/**
 *-----------------------------------------------------------
 * SYNC TRANSPORT
 *-----------------------------------------------------------
 */

/**
 * @brief SYNC 기능 활성화
 * @param ctx       DOP Context
 * @param period_us SYNC 주기 (μs, Master용)
 * @param on_sync   SYNC 수신 콜백 (ISR 내 호출)
 */
void AGR_CANFD_EnableSync(AGR_DOP_Ctx_t* ctx,
                          uint32_t period_us,
                          void (*on_sync)(void* ctx));

/**
 * @brief SYNC 기능 비활성화
 * @param ctx DOP Context
 */
void AGR_CANFD_DisableSync(AGR_DOP_Ctx_t* ctx);

/**
 * @brief SYNC 메시지 전송 (Master 모드, CAN-ID: 0x080)
 * @param ctx DOP Context
 * @return 0=성공, <0=에러
 */
int AGR_CANFD_SendSYNC(AGR_DOP_Ctx_t* ctx);

/**
 *-----------------------------------------------------------
 * EMERGENCY TRANSPORT
 *-----------------------------------------------------------
 */

/**
 * @brief Emergency 기능 활성화
 * @param ctx          DOP Context
 * @param on_emergency Emergency 수신 콜백 (CiA 301: error_code + error_register)
 */
void AGR_CANFD_EnableEmergency(AGR_DOP_Ctx_t* ctx,
                               void (*on_emergency)(uint16_t error_code,
                                                     uint8_t error_register,
                                                     void* ctx));

/**
 * @brief Emergency 메시지 전송 (CAN-ID: 0x080 + node_id)
 * @param ctx            DOP Context
 * @param error_code     Emergency Error Code (CiA 301)
 * @param error_register Error Register (OD 0x1001, CiA 301 byte 2)
 * @return 0=성공, <0=에러
 *
 * @details CiA 301 EMCY format (8 bytes):
 *   [ErrorCode_Lo][ErrorCode_Hi][ErrorRegister][MfgSpecific x5]
 */
int AGR_CANFD_SendEmergency(AGR_DOP_Ctx_t* ctx, uint16_t error_code,
                             uint8_t error_register);

#endif /* AGR_DOP_CANFD_H */
