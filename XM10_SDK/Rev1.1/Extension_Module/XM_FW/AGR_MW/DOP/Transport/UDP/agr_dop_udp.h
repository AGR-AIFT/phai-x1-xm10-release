/**
 ******************************************************************************
 * @file    agr_dop_udp.h
 * @author  HyundoKim
 * @brief   AGR DOP UDP Transport - Packet Header, Message Processing
 * @version 1.0
 * @date    Mar 2, 2026
 *
 * @details
 * UDP Transport 계층: UDP 패킷 헤더 파싱/생성, RX 메시지 라우팅.
 * Core (OD, SDO, PDO) 모듈을 UDP 패킷으로 감싸는 역할입니다.
 *
 * [의존성]
 * - Core: agr_od, agr_sdo_protocol, agr_pdo_engine (순수 C)
 * - eth_udp_socket: LwIP UDP 소켓 인프라 (3채널)
 *
 * [UDP 패킷 구조]
 *   [0] version=0x01  [1] type  [2] src_node_id  [3] reserved=0x00
 *   [4+] payload (SDO/PDO/EMCY 데이터)
 *
 * [Type 정의 — 채널별]
 *   PDO (Port 5001): TPDO1~4=0x01~0x04, RPDO1~4=0x11~0x14, SYNC=0x80
 *   SDO (Port 5002): REQ=0x01, RSP=0x02
 *   EMCY(Port 5003): EMCY=0x01
 *
 * [설계 원칙]
 * - mutex/snapshot 없음 — Device Layer(am_drv)가 담당
 * - 콜백 기반 RX: decode 후 Device Layer 콜백 호출
 * - CAN-FD Transport(agr_dop_canfd)와 대칭 구조
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_UDP_H
#define AGR_DOP_UDP_H

#include "agr_dop_types.h"
#include "agr_dop_config.h"

#if AGR_DOP_TRANSPORT_UDP

#include "Core/agr_od.h"
#include "Core/agr_sdo_protocol.h"
#include "Core/agr_pdo_engine.h"

/**
 *-----------------------------------------------------------
 * UDP PACKET HEADER
 *-----------------------------------------------------------
 */

#define AGR_UDP_HEADER_SIZE     4U      /**< UDP 패킷 헤더 크기 */
#define AGR_UDP_VERSION         0x01    /**< 프로토콜 버전 */
#define AGR_UDP_MAX_PAYLOAD     1468U   /**< MTU 1500 - IP(20) - UDP(8) - Header(4) */

/** @brief PDO Type (Port 5001) */
#define AGR_UDP_TYPE_TPDO1      0x01
#define AGR_UDP_TYPE_TPDO2      0x02
#define AGR_UDP_TYPE_TPDO3      0x03
#define AGR_UDP_TYPE_TPDO4      0x04
#define AGR_UDP_TYPE_RPDO1      0x11
#define AGR_UDP_TYPE_RPDO2      0x12
#define AGR_UDP_TYPE_RPDO3      0x13
#define AGR_UDP_TYPE_RPDO4      0x14
#define AGR_UDP_TYPE_SYNC       0x80

/** @brief SDO Type (Port 5002) */
#define AGR_UDP_TYPE_SDO_REQ    0x01
#define AGR_UDP_TYPE_SDO_RSP    0x02

/** @brief EMCY Type (Port 5003) */
#define AGR_UDP_TYPE_EMCY       0x01

/**
 *-----------------------------------------------------------
 * CALLBACK TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief PDO 수신 콜백 (am_drv가 등록)
 * @param pdo_type PDO 타입 (TPDO1~4, RPDO1~4 등)
 * @param src_id   송신자 Node ID
 * @param data     PDO payload 포인터
 * @param len      payload 길이
 */
typedef void (*AGR_UDP_PdoCallback_t)(uint8_t pdo_type, uint8_t src_id,
                                       const uint8_t* data, uint8_t len);

/**
 * @brief SDO 수신 콜백 (am_drv가 등록)
 * @param msg_type SDO 타입 (REQ or RSP)
 * @param src_id   송신자 Node ID
 * @param data     SDO payload 포인터
 * @param len      payload 길이
 */
typedef void (*AGR_UDP_SdoCallback_t)(uint8_t msg_type, uint8_t src_id,
                                       const uint8_t* data, uint8_t len);

/**
 * @brief EMCY 수신 콜백 (am_drv가 등록)
 * @param src_id         송신자 Node ID
 * @param error_code     Emergency Error Code
 * @param error_register Error Register (OD 0x1001, CiA 301 byte 2)
 */
typedef void (*AGR_UDP_EmcyCallback_t)(uint8_t src_id, uint16_t error_code,
                                        uint8_t error_register);

/**
 *-----------------------------------------------------------
 * INITIALIZATION
 *-----------------------------------------------------------
 */

/**
 * @brief UDP Transport 초기화
 * @param ctx      DOP Context 포인터
 * @param od       Object Dictionary 테이블 (NULL 허용 — OD 미사용 시)
 * @param node_id  자신의 Node ID (0x01~0x7F)
 * @return 0=성공, <0=에러
 */
int AGR_UDP_Init(AGR_DOP_Ctx_t* ctx, const AGR_OD_Table_t* od, uint8_t node_id);

/**
 * @brief UDP Transport 종료
 */
void AGR_UDP_DeInit(void);

/**
 *-----------------------------------------------------------
 * TX FUNCTIONS (Device Layer가 호출)
 *-----------------------------------------------------------
 */

/**
 * @brief TX PDO 전송 (Core Encode + UDP 패킷)
 * @param ctx     DOP Context
 * @param pdo_num PDO 번호 (1~4)
 * @return 0=성공, <0=에러
 */
int AGR_UDP_SendTxPDO(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num);

/**
 * @brief SDO 메시지 전송
 * @param ctx       DOP Context
 * @param target_id 대상 Node ID (현재 미사용, 기본 AM IP로 전송)
 * @param msg       SDO 메시지
 * @return 0=성공, <0=에러
 */
int AGR_UDP_SendSDO(AGR_DOP_Ctx_t* ctx, uint8_t target_id,
                     const AGR_SDO_Msg_t* msg);

/**
 * @brief SYNC 메시지 전송
 * @param ctx DOP Context
 * @return 0=성공, <0=에러
 */
int AGR_UDP_SendSYNC(AGR_DOP_Ctx_t* ctx);

/**
 * @brief Emergency 메시지 전송
 * @param ctx            DOP Context
 * @param error_code     Emergency Error Code
 * @param error_register Error Register (OD 0x1001, CiA 301 byte 2)
 * @return 0=성공, <0=에러
 */
int AGR_UDP_SendEmergency(AGR_DOP_Ctx_t* ctx, uint16_t error_code,
                           uint8_t error_register);

/**
 * @brief Raw PDO 데이터 전송 (OD/Mapping 없이 직접 전송)
 * @param pdo_type PDO 타입 (TPDO1~4)
 * @param data     payload 포인터
 * @param len      payload 길이
 * @return 0=성공, <0=에러
 */
int AGR_UDP_SendRawPDO(uint8_t pdo_type, const uint8_t* data, uint8_t len);

/**
 *-----------------------------------------------------------
 * RX CALLBACK REGISTRATION (am_drv가 등록)
 *-----------------------------------------------------------
 */

void AGR_UDP_RegisterPdoCallback(AGR_UDP_PdoCallback_t cb);
void AGR_UDP_RegisterSdoCallback(AGR_UDP_SdoCallback_t cb);
void AGR_UDP_RegisterEmcyCallback(AGR_UDP_EmcyCallback_t cb);

#else /* !AGR_DOP_TRANSPORT_UDP */

/* Transport 비활성 시 빈 인라인 함수 */
static inline int AGR_UDP_Init(void* ctx, const void* od, uint8_t node_id)
    { (void)ctx; (void)od; (void)node_id; return -1; }
static inline void AGR_UDP_DeInit(void) {}

#endif /* AGR_DOP_TRANSPORT_UDP */

#endif /* AGR_DOP_UDP_H */
