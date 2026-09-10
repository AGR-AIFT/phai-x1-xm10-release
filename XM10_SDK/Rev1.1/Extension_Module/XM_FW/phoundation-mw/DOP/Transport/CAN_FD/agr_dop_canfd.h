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
#include "Core/agr_sdo_master.h"
#include "Core/agr_pdo_engine.h"
#if AGR_VECTOR_ENABLED
#include "Core/agr_vector.h"   /* Command Vector — opt-in (agr_dop_config.h AGR_VECTOR_ENABLED) */
#endif

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

/** @brief Command Vector Request CAN-ID (0x100 + node_id) */
static inline uint32_t AGR_CANFD_GetVectorRequestID(uint8_t node_id) {
    return AGR_CAN_ID_VECTOR_REQ + node_id;
}

/** @brief Command Vector ACK/EVENT CAN-ID (0x680 + node_id) */
static inline uint32_t AGR_CANFD_GetVectorAckID(uint8_t node_id) {
    return AGR_CAN_ID_VECTOR_ACK + node_id;
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
int32_t AGR_CANFD_Init(AGR_DOP_Ctx_t* ctx,
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
int32_t AGR_CANFD_ProcessRxMessage(AGR_DOP_Ctx_t* ctx,
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
int32_t AGR_CANFD_ProcessSDORequest(AGR_DOP_Ctx_t* ctx,
                                const AGR_SDO_Msg_t* req,
                                AGR_SDO_Msg_t* out_rsp);

/**
 * @brief SDO 메시지 전송 (CAN-ID 자동 결정)
 * @param ctx       DOP Context
 * @param target_id 대상 Node ID
 * @param msg       SDO 메시지
 * @return 0=성공, <0=에러
 */
int32_t AGR_CANFD_SendSDO(AGR_DOP_Ctx_t* ctx,
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
int32_t AGR_CANFD_SendSDOWrite(AGR_DOP_Ctx_t* ctx,
                           uint16_t index,
                           uint8_t subindex,
                           const void* data,
                           uint16_t data_len);

/**
 * @brief 통신 대상 Node ID 설정
 * @param ctx            DOP Context
 * @param target_node_id 대상 Node ID
 */
void AGR_CANFD_SetTargetNodeId(AGR_DOP_Ctx_t* ctx, uint8_t target_node_id);

/**
 * @brief SDO Master request — 응답/abort/timeout 을 callback 으로 통지 (C6).
 * @details fire-and-forget(AGR_CANFD_SendSDO/Write) 와 달리 transaction 을 풀에
 *          등록하고, slave 응답(0x580+slave)을 AGR_CANFD_ProcessRxMessage 의
 *          SDO_TX case 가 AGR_SDO_Master_ProcessResponse 로 매칭 → on_done 호출.
 *          Wire format 은 fire-and-forget 과 동일(CiA 301). Timeout 은 호출자가
 *          AGR_SDO_Master_Tick(&ctx->sdo_master, now) 을 주기 호출해야 동작.
 *          사전 조건: AGR_SDO_Master_Init(&ctx->sdo_master, get_tick_ms).
 * @param ctx        Master DOP context (sdo_master 초기화 필요).
 * @param slave_id   대상 slave node id.
 * @param index      OD index.
 * @param subindex   OD sub-index.
 * @param is_write   true=Download(Write), false=Upload(Read).
 * @param data       write 데이터 (read 시 NULL).
 * @param data_len   write 데이터 길이 (read 시 0). ≤ AGR_SDO_MAX_DATA_SIZE.
 * @param timeout_ms 응답 대기 한도 (Tick 기준).
 * @param on_done    완료 콜백 (필수, NULL 금지).
 * @param user_ctx   콜백 전달 context.
 * @return 0=송신 성공(대기 시작), -1=인자, -2=oversize, -3=busy/풀가득,
 *         -4=encode 실패, -5=TX 실패.
 */
int32_t AGR_CANFD_Master_Request(AGR_DOP_Ctx_t*         ctx,
                                 uint8_t                slave_id,
                                 uint16_t               index,
                                 uint8_t                subindex,
                                 bool                   is_write,
                                 const uint8_t*         data,
                                 uint8_t                data_len,
                                 uint32_t               timeout_ms,
                                 AGR_SDO_CompletionCb_t on_done,
                                 void*                  user_ctx);

/**
 *-----------------------------------------------------------
 * COMMAND VECTOR TRANSPORT (TX) — opt-in (AGR_VECTOR_ENABLED)
 *-----------------------------------------------------------
 */
#if AGR_VECTOR_ENABLED

/**
 * @brief Command Vector Request 전송 (Master → Slave, 0x100+target)
 * @details 단일 atomic 프레임 = vector 하나. seq 는 per-node 단조 증가를
 *          호출자(master)가 관리 — 동일 seq 재전송 = 멱등 재시도.
 *          ACK([echo_seq][status])는 AGR_Vector_SetMasterCallbacks 로 소비.
 * @param ctx         DOP Context (master)
 * @param target_node 대상 slave node id
 * @param type        vector_type 코드 (예: 0x10 P / 0x11 I / 0x12 F)
 * @param seq         시퀀스 번호
 * @param payload     payload (len==0 이면 NULL 허용)
 * @param len         payload 길이 (≤ AGR_VECTOR_MAX_PAYLOAD)
 * @return 0=성공, -1=인자, -2=인코드 실패, 그 외=tx_func 반환값
 */
int32_t AGR_CANFD_SendVectorRequest(AGR_DOP_Ctx_t* ctx,
                                    uint8_t target_node,
                                    uint8_t type,
                                    uint8_t seq,
                                    const void* payload,
                                    uint8_t len);

/**
 * @brief Command Vector EVENT 전송 (Slave → Master, 0x680+own, 1B 정확 DLC)
 * @details ACK 채널의 1B frame = EVENT (2B=ACK 와 길이로 구분). 예: MD 의
 *          P-Done = 0x80|0x10 = 0x90 (P decoder 의 duration_completed 시점).
 * @param ctx        DOP Context (slave)
 * @param event_code 이벤트 코드 — bit7 필수 (0x80|type 규약)
 * @return 0=성공, -1=인자, -2=bit7 없는 코드, 그 외=tx_func 반환값
 */
int32_t AGR_CANFD_SendVectorEvent(AGR_DOP_Ctx_t* ctx, uint8_t event_code);

#endif /* AGR_VECTOR_ENABLED */

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
int32_t AGR_CANFD_SendTxPDO(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num);

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
int32_t AGR_CANFD_SendSYNC(AGR_DOP_Ctx_t* ctx);

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
int32_t AGR_CANFD_SendEmergency(AGR_DOP_Ctx_t* ctx, uint16_t error_code,
                             uint8_t error_register);

#endif /* AGR_DOP_CANFD_H */
