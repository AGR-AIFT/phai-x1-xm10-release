/**
 ******************************************************************************
 * @file    agr_dop_serial.h
 * @author  HyundoKim
 * @brief   AGR DOP Serial Transport - DOP over COBS-framed Serial (UART/SPI/BLE)
 * @version 2.0
 * @date    2026-03-25
 *
 * @details
 * Why:  CAN-FD Transport는 CAN-ID로 메시지 타입/노드를 식별하지만,
 *       시리얼 인터페이스에는 CAN-ID가 없음.
 * What: COBS 프레이밍 + 메시지 헤더(Type+NodeID)로 동일한 DOP 기능 제공.
 *       Core(OD, SDO, PDO) 모듈을 그대로 재사용.
 *
 * [Serial Frame 구조]
 *   COBS Frame: [COBS-encoded(Header + DOP Payload + CRC16-LE)] [0x00]
 *
 *   Header (4 bytes):
 *     Byte 0:   Message Type (AGR_SERIAL_MSG_xxx)
 *     Byte 1:   Source Node ID (0x01~0x7F)
 *     Byte 2-3: Sequence ID (uint16 LE, per-context monotonic counter)
 *
 *   DOP Payload:
 *     SDO: CS(1B) + Index(2B) + SubIndex(1B) + Data(NB) — agr_sdo_protocol 인코딩
 *     PDO: OD 매핑 데이터(NB) — agr_pdo_engine 인코딩
 *
 * [CAN-FD Transport와의 대응]
 *   CAN Function Code ↔ AGR_SERIAL_MSG_xxx
 *   CAN-ID (Node ID)  ↔ Header Byte 1
 *   Sequence ID        ↔ (CAN-FD에는 없음, Serial Transport 전용)
 *   CAN Frame          ↔ COBS Frame
 *
 * [V2.0 변경: SLIP → COBS]
 *   - SLIP(RFC 1055): 가변 오버헤드 (ESC 이스케이프), DMA 길이 예측 불가
 *   - COBS: 고정 오버헤드 (254B당 1B), DMA/BLE MTU 친화, phai-studio 통일
 *
 * [사용 시나리오]
 *   SM ↔ Python GUI:   USB-CDC → DOP Serial (COBS) → 센서모듈 진단
 *   CM-WH ↔ ESP32 BLE: USART2 → DOP Serial (COBS) → ble_comm
 *   CM ↔ App:          BLE UART → DOP Serial (COBS) → 스마트폰 앱
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_SERIAL_H
#define AGR_DOP_SERIAL_H

#include "agr_dop_types.h"
#include "agr_dop_config.h"
#include "agr_cobs.h"
#include "Core/agr_od.h"
#include "Core/agr_sdo_protocol.h"
#include "Core/agr_pdo_engine.h"

/**
 *-----------------------------------------------------------
 * MESSAGE TYPE (CAN Function Code 대응)
 *-----------------------------------------------------------
 */
typedef enum {
    AGR_SERIAL_MSG_SDO_REQ      = 0x01,  /**< SDO Request  (Master → Slave, CAN 0x600) */
    AGR_SERIAL_MSG_SDO_RSP      = 0x02,  /**< SDO Response (Slave → Master, CAN 0x580) */
    AGR_SERIAL_MSG_TPDO1        = 0x03,  /**< TX PDO 1 (CAN 0x180) */
    AGR_SERIAL_MSG_TPDO2        = 0x04,  /**< TX PDO 2 (CAN 0x280) */
    AGR_SERIAL_MSG_TPDO3        = 0x05,  /**< TX PDO 3 (CAN 0x380) */
    AGR_SERIAL_MSG_TPDO4        = 0x06,  /**< TX PDO 4 (CAN 0x480) */
    AGR_SERIAL_MSG_RPDO1        = 0x07,  /**< RX PDO 1 (CAN 0x200) */
    AGR_SERIAL_MSG_RPDO2        = 0x08,  /**< RX PDO 2 (CAN 0x300) */
    AGR_SERIAL_MSG_RPDO3        = 0x09,  /**< RX PDO 3 (CAN 0x400) */
    AGR_SERIAL_MSG_RPDO4        = 0x0A,  /**< RX PDO 4 (CAN 0x500) */
    AGR_SERIAL_MSG_HEARTBEAT    = 0x0B,  /**< Heartbeat (CAN 0x700) */
    AGR_SERIAL_MSG_SYNC         = 0x0C,  /**< SYNC (CAN 0x080) */
    AGR_SERIAL_MSG_EMCY         = 0x0D,  /**< Emergency (CAN 0x080+Node) */
} AGR_Serial_MsgType_e;

/** @brief Serial 헤더 크기 (MsgType + NodeID + SeqID) */
#define AGR_SERIAL_HEADER_SIZE      4

/** @brief Serial 최대 페이로드 크기 (COBS 디코딩 후, 헤더 제외) */
#define AGR_SERIAL_MAX_PAYLOAD      (AGR_COBS_MAX_FRAME_SIZE - AGR_SERIAL_HEADER_SIZE)

/**
 *-----------------------------------------------------------
 * SERIAL TX FUNCTION TYPE
 *-----------------------------------------------------------
 * @brief   시리얼 바이트 전송 함수 (Dependency Injection).
 *          IOIF_UART_Write_DMA 또는 ESP32_Drv_BLE_Send를 래핑.
 *
 * @param   data    COBS 인코딩된 프레임 데이터
 * @param   len     데이터 길이
 * @return  0: 성공, <0: 에러
 */
typedef int32_t (*AGR_Serial_TxFunc_t)(const uint8_t* data, uint32_t len);

/**
 *-----------------------------------------------------------
 * SERIAL TRANSPORT CONTEXT
 *-----------------------------------------------------------
 * @brief   Serial Transport 인스턴스.
 *          DOP Context + COBS 디코더 + TX 함수를 묶음.
 */
typedef struct {
    AGR_DOP_Ctx_t*          dop_ctx;        /**< DOP Context (Core 모듈 공유) */
    AGR_COBS_Decoder_t      cobs_dec;       /**< COBS 디코더 (RX) */
    AGR_Serial_TxFunc_t     tx_func;        /**< 바이트 전송 함수 (DI) */
    uint16_t                tx_seq;         /**< TX 시퀀스 카운터 (monotonic, 0xFFFF→0 wrap) */
    bool                    initialized;    /**< 초기화 완료 여부 */
} AGR_Serial_Ctx_t;

/**
 *-----------------------------------------------------------
 * INITIALIZATION
 *-----------------------------------------------------------
 */

/**
 * @brief   Serial Transport 초기화.
 * @param   sctx    Serial Transport Context
 * @param   dop_ctx DOP Context (OD, Node ID 등 설정 완료 상태)
 * @param   tx_func 바이트 전송 함수 (e.g., ESP32_Drv_BLE_Send)
 * @return  0: 성공, <0: 에러
 */
int32_t AGR_Serial_Init(AGR_Serial_Ctx_t* sctx,
                        AGR_DOP_Ctx_t* dop_ctx,
                        AGR_Serial_TxFunc_t tx_func);

/**
 * @brief   Serial Transport 리셋 (COBS 디코더 + PDO Mapping 초기화).
 * @param   sctx    Serial Transport Context
 */
void AGR_Serial_Reset(AGR_Serial_Ctx_t* sctx);

/**
 *-----------------------------------------------------------
 * RX PROCESSING
 *-----------------------------------------------------------
 */

/**
 * @brief   수신 데이터 처리.
 *          ESP32 UART RX 콜백에서 호출.
 *          COBS 디코딩 → CRC 검증 → 메시지 라우팅 (SDO/PDO).
 *
 * @param   sctx    Serial Transport Context
 * @param   data    수신 데이터 (DMA bounce buffer)
 * @param   len     수신 데이터 길이
 *
 * @note    ISR 컨텍스트에서 호출 가능 (COBS 디코더가 ISR-safe).
 *          완료된 프레임은 내부적으로 자동 라우팅됨.
 */
void AGR_Serial_ProcessRxData(AGR_Serial_Ctx_t* sctx,
                              const uint8_t* data, uint32_t len);

/**
 *-----------------------------------------------------------
 * SDO TRANSPORT (TX)
 *-----------------------------------------------------------
 */

/**
 * @brief   SDO 메시지 전송 (COBS 프레이밍).
 * @param   sctx    Serial Transport Context
 * @param   msg     SDO 메시지
 * @param   is_response true: SDO Response, false: SDO Request
 * @return  0: 성공, <0: 에러
 */
int32_t AGR_Serial_SendSDO(AGR_Serial_Ctx_t* sctx,
                           const AGR_SDO_Msg_t* msg,
                           bool is_response);

/**
 * @brief   단일 SDO Write 전송 (편의 함수).
 * @param   sctx    Serial Transport Context
 * @param   index   OD Index
 * @param   subindex OD Sub-Index
 * @param   data    데이터 포인터
 * @param   data_len 데이터 길이
 * @return  0: 성공, <0: 에러
 */
int32_t AGR_Serial_SendSDOWrite(AGR_Serial_Ctx_t* sctx,
                                uint16_t index,
                                uint8_t subindex,
                                const void* data,
                                uint8_t data_len);

/**
 *-----------------------------------------------------------
 * PDO TRANSPORT (TX)
 *-----------------------------------------------------------
 */

/**
 * @brief   TX PDO 전송 (Core Encode + COBS 프레이밍).
 * @param   sctx    Serial Transport Context
 * @param   pdo_num PDO 번호 (1~4)
 * @return  0: 성공, <0: 에러
 */
int32_t AGR_Serial_SendTxPDO(AGR_Serial_Ctx_t* sctx, uint8_t pdo_num);

/**
 *-----------------------------------------------------------
 * HEARTBEAT / SYNC / EMERGENCY (TX)
 *-----------------------------------------------------------
 */

/**
 * @brief   Heartbeat 전송.
 * @param   sctx    Serial Transport Context
 * @param   state   NMT State (0x00=Boot, 0x04=Stopped, 0x05=Operational, 0x7F=Pre-Op)
 * @return  0: 성공, <0: 에러
 */
int32_t AGR_Serial_SendHeartbeat(AGR_Serial_Ctx_t* sctx, uint8_t state);

/**
 * @brief   Emergency 전송.
 * @param   sctx           Serial Transport Context
 * @param   error_code     Emergency Error Code
 * @param   error_register Error Register (OD 0x1001, CiA 301 byte 2)
 * @return  0: 성공, <0: 에러
 */
int32_t AGR_Serial_SendEmergency(AGR_Serial_Ctx_t* sctx, uint16_t error_code,
                                  uint8_t error_register);

#endif /* AGR_DOP_SERIAL_H */
