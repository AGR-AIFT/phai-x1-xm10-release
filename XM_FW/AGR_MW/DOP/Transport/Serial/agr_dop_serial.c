/**
 ******************************************************************************
 * @file    agr_dop_serial.c
 * @author  HyundoKim
 * @brief   AGR DOP Serial Transport - Implementation
 * @version 2.0
 * @date    2026-03-25
 *
 * @details
 * DOP Serial Transport 구현: CAN-FD Transport와 동일한 DOP Core 모듈을
 * COBS-framed 시리얼 프레임으로 감쌉니다.
 *
 * [TX Flow]
 * 1. DOP Payload 생성 (SDO Encode / PDO Encode)
 * 2. Serial Header 추가 (MsgType + NodeID + SeqID)
 * 3. COBS FrameEncode (Header + Payload + CRC-16)
 * 4. tx_func() 호출 (UART DMA / USB CDC)
 *
 * [RX Flow]
 * 1. AGR_Serial_ProcessRxData() ← UART/CDC RX 콜백
 * 2. COBS Decode → CRC 검증 → _OnFrameReceived()
 * 3. Header 파싱 → MsgType 라우팅
 * 4. SDO: Core ProcessRequest → Response 자동 전송
 * 5. PDO: Core Decode → on_pdo_received 콜백
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_dop_serial.h"

#if AGR_DOP_TRANSPORT_SERIAL

#include <string.h>

/* ===== TX 버퍼 (COBS 인코딩 후 최대 크기) ===== */

/** @brief COBS 프레임 최대 크기: COBS overhead + data + CRC + delimiter */
#define SERIAL_TX_BUF_SIZE      ((AGR_COBS_MAX_FRAME_SIZE + 2) + ((AGR_COBS_MAX_FRAME_SIZE + 2) / 254) + 2 + 1)

/* ===== Private Function Prototypes ===== */

static void _OnFrameReceived(const uint8_t* data, uint32_t len, void* ctx);
static int32_t _SendFrame(AGR_Serial_Ctx_t* sctx,
                          AGR_Serial_MsgType_e msg_type,
                          const uint8_t* payload, uint32_t payload_len);

#if AGR_OD_DISCOVERY_ENABLED
static void _SendODDirectoryResponse(AGR_Serial_Ctx_t* sctx,
                                     const AGR_SDO_Msg_t* req);
#endif

/* ===== Initialization ===== */

int32_t AGR_Serial_Init(AGR_Serial_Ctx_t* sctx,
                        AGR_DOP_Ctx_t* dop_ctx,
                        AGR_Serial_TxFunc_t tx_func)
{
    if (sctx == NULL || dop_ctx == NULL || tx_func == NULL) {
        return -1;
    }

    sctx->dop_ctx = dop_ctx;
    sctx->tx_func = tx_func;
    sctx->tx_seq = 0;
    sctx->initialized = true;

    AGR_COBS_DecoderInit(&sctx->cobs_dec);

    return 0;
}

void AGR_Serial_Reset(AGR_Serial_Ctx_t* sctx)
{
    if (sctx == NULL) return;

    AGR_COBS_DecoderReset(&sctx->cobs_dec);

    if (sctx->dop_ctx != NULL) {
        for (uint8_t i = 0; i < 4; i++) {
            AGR_PDO_ClearMap(&sctx->dop_ctx->tx_pdo_map[i]);
            AGR_PDO_ClearMap(&sctx->dop_ctx->rx_pdo_map[i]);
        }
    }
}

/* ===== RX Processing ===== */

void AGR_Serial_ProcessRxData(AGR_Serial_Ctx_t* sctx,
                              const uint8_t* data, uint32_t len)
{
    if (sctx == NULL || !sctx->initialized || data == NULL || len == 0) {
        return;
    }

    AGR_COBS_DecoderFeed(&sctx->cobs_dec, data, len,
                         _OnFrameReceived, sctx);
}

/* ===== SDO Transport (TX) ===== */

int32_t AGR_Serial_SendSDO(AGR_Serial_Ctx_t* sctx,
                           const AGR_SDO_Msg_t* msg,
                           bool is_response)
{
    if (sctx == NULL || !sctx->initialized || msg == NULL) {
        return -1;
    }

    uint8_t payload[AGR_SERIAL_MAX_PAYLOAD];
    int len = AGR_SDO_Encode(msg, payload);
    if (len <= 0) {
        return -2;
    }

    AGR_Serial_MsgType_e type = is_response
        ? AGR_SERIAL_MSG_SDO_RSP
        : AGR_SERIAL_MSG_SDO_REQ;

    return _SendFrame(sctx, type, payload, (uint32_t)len);
}

int32_t AGR_Serial_SendSDOWrite(AGR_Serial_Ctx_t* sctx,
                                uint16_t index,
                                uint8_t subindex,
                                const void* data,
                                uint8_t data_len)
{
    if (sctx == NULL || !sctx->initialized || data == NULL) {
        return -1;
    }

    if (data_len > AGR_SDO_MAX_DATA_SIZE) {
        return -2;
    }

    AGR_SDO_Msg_t sdo_req;
    AGR_SDO_CreateWriteReq(&sdo_req, index, subindex, data, data_len);

    return AGR_Serial_SendSDO(sctx, &sdo_req, false);
}

/* ===== PDO Transport (TX) ===== */

int32_t AGR_Serial_SendTxPDO(AGR_Serial_Ctx_t* sctx, uint8_t pdo_num)
{
    if (sctx == NULL || !sctx->initialized || sctx->dop_ctx == NULL) {
        return -1;
    }

    if (pdo_num < 1 || pdo_num > 4) {
        return -2;
    }

    uint8_t payload[AGR_SERIAL_MAX_PAYLOAD];
    int len = AGR_PDO_Encode(&sctx->dop_ctx->tx_pdo_map[pdo_num - 1],
                             &sctx->dop_ctx->od,
                             payload, sizeof(payload));
    if (len <= 0) {
        return -3;
    }

    /* TPDO1 → AGR_SERIAL_MSG_TPDO1(0x03), TPDO2 → 0x04, ... */
    AGR_Serial_MsgType_e type = (AGR_Serial_MsgType_e)(AGR_SERIAL_MSG_TPDO1 + (pdo_num - 1));

    return _SendFrame(sctx, type, payload, (uint32_t)len);
}

/* ===== Heartbeat / Emergency (TX) ===== */

int32_t AGR_Serial_SendHeartbeat(AGR_Serial_Ctx_t* sctx, uint8_t state)
{
    if (sctx == NULL || !sctx->initialized) {
        return -1;
    }

    uint8_t payload[1] = { state };
    return _SendFrame(sctx, AGR_SERIAL_MSG_HEARTBEAT, payload, 1);
}

int32_t AGR_Serial_SendEmergency(AGR_Serial_Ctx_t* sctx, uint16_t error_code,
                                  uint8_t error_register)
{
    if (sctx == NULL || !sctx->initialized) {
        return -1;
    }

    /* CiA 301 EMCY format: [ErrCode Lo][ErrCode Hi][ErrReg][MfgSpec x5] */
    uint8_t payload[8] = {
        (uint8_t)(error_code & 0xFF),
        (uint8_t)(error_code >> 8),
        error_register,
        0x00, 0x00, 0x00, 0x00, 0x00,
    };

    return _SendFrame(sctx, AGR_SERIAL_MSG_EMCY, payload, 8);
}

/* ===== Private Functions ===== */

/**
 * @brief   Serial 프레임 전송.
 *          Header(MsgType+NodeID+SeqID) + Payload → COBS FrameEncode → tx_func.
 */
static int32_t _SendFrame(AGR_Serial_Ctx_t* sctx,
                          AGR_Serial_MsgType_e msg_type,
                          const uint8_t* payload, uint32_t payload_len)
{
    /* Header + Payload 조립 */
    uint8_t raw[AGR_COBS_MAX_FRAME_SIZE];
    uint32_t raw_len = AGR_SERIAL_HEADER_SIZE + payload_len;

    if (raw_len > AGR_COBS_MAX_FRAME_SIZE) {
        return -3;
    }

    uint16_t seq = sctx->tx_seq++;  /* monotonic, wraps at 0xFFFF→0 */

    raw[0] = (uint8_t)msg_type;
    raw[1] = sctx->dop_ctx->node_id;
    raw[2] = (uint8_t)(seq & 0xFF);        /* SeqID Lo */
    raw[3] = (uint8_t)((seq >> 8) & 0xFF); /* SeqID Hi */
    memcpy(&raw[AGR_SERIAL_HEADER_SIZE], payload, payload_len);

    /* COBS FrameEncode (CRC 자동 포함 + 0x00 delimiter) */
    uint8_t cobs_buf[SERIAL_TX_BUF_SIZE];
    int32_t cobs_len = AGR_COBS_FrameEncode(raw, raw_len, cobs_buf, sizeof(cobs_buf));
    if (cobs_len <= 0) {
        return -4;
    }

    return sctx->tx_func(cobs_buf, (uint32_t)cobs_len);
}

/**
 * @brief   COBS 디코더 프레임 완료 콜백.
 *          CRC 검증 완료된 프레임을 메시지 타입별로 라우팅.
 *
 * @param   data    디코딩된 데이터 (Header + DOP Payload, CRC 제거됨)
 * @param   len     데이터 길이
 * @param   ctx     AGR_Serial_Ctx_t*
 */
static void _OnFrameReceived(const uint8_t* data, uint32_t len, void* ctx)
{
    AGR_Serial_Ctx_t* sctx = (AGR_Serial_Ctx_t*)ctx;
    if (sctx == NULL || sctx->dop_ctx == NULL) return;

    /* Header 파싱 (4B: MsgType + NodeID + SeqID_LE16) */
    if (len < AGR_SERIAL_HEADER_SIZE) return;

    AGR_Serial_MsgType_e msg_type = (AGR_Serial_MsgType_e)data[0];
    /* uint8_t source_node = data[1]; */  /* 필요시 사용 */
    /* uint16_t rx_seq = (uint16_t)data[2] | ((uint16_t)data[3] << 8); */  /* 필요시 사용 */

    const uint8_t* payload = &data[AGR_SERIAL_HEADER_SIZE];
    uint32_t payload_len = len - AGR_SERIAL_HEADER_SIZE;

    AGR_DOP_Ctx_t* dop = sctx->dop_ctx;

    switch (msg_type) {
        case AGR_SERIAL_MSG_SDO_REQ: {
            /* SDO Request 수신 → Core 처리 → Response 자동 전송 */
            AGR_SDO_Msg_t req, rsp;
            if (AGR_SDO_Decode(payload, (uint8_t)payload_len, &req) != 0) {
                break;
            }

#if AGR_OD_DISCOVERY_ENABLED
            /* OD Directory (0x2F00) → Serial 전용 핸들러 (Core SDO 무수정) */
            if (req.index == AGR_OD_IDX_DIRECTORY
                && (req.cs & 0xE0) == 0x40)     /* Upload (Read) Request */
            {
                _SendODDirectoryResponse(sctx, &req);
                break;
            }
#endif

            /* on_sdo_request 콜백 우선 */
            memset(&rsp, 0, sizeof(rsp));
            if (dop->on_sdo_request != NULL) {
                dop->on_sdo_request(&req, &rsp);
                if (rsp.cs != 0) {
                    AGR_Serial_SendSDO(sctx, &rsp, true);
                    break;
                }
            }

            /* Core OD 처리 */
            if (AGR_SDO_ProcessRequest(&dop->od, &req, &rsp) == 0) {
                AGR_Serial_SendSDO(sctx, &rsp, true);
            }
            break;
        }

        case AGR_SERIAL_MSG_SDO_RSP:
            /* SDO Response 수신 — 상위 계층 콜백으로 전달 (TODO: async SDO) */
            break;

        case AGR_SERIAL_MSG_TPDO1:
        case AGR_SERIAL_MSG_TPDO2:
        case AGR_SERIAL_MSG_TPDO3:
        case AGR_SERIAL_MSG_TPDO4: {
            /* 수신 TPDO → Master가 수신하는 경우 (RX PDO로 디코딩) */
            uint8_t pdo_idx = (uint8_t)(msg_type - AGR_SERIAL_MSG_TPDO1);
            if (pdo_idx < 4) {
                AGR_PDO_Decode(&dop->rx_pdo_map[pdo_idx],
                               &dop->od, payload, (uint8_t)payload_len);

                if (dop->on_pdo_received != NULL) {
                    /* CAN-ID 대응값 생성 (호환성) */
                    uint32_t cob_id = (uint32_t)(0x180 + (pdo_idx * 0x100));
                    dop->on_pdo_received(cob_id, payload, (uint8_t)payload_len);
                }
            }
            break;
        }

        case AGR_SERIAL_MSG_RPDO1:
        case AGR_SERIAL_MSG_RPDO2:
        case AGR_SERIAL_MSG_RPDO3:
        case AGR_SERIAL_MSG_RPDO4: {
            /* 수신 RPDO → Slave가 수신하는 경우 (RX PDO로 디코딩) */
            uint8_t pdo_idx = (uint8_t)(msg_type - AGR_SERIAL_MSG_RPDO1);
            if (pdo_idx < 4) {
                AGR_PDO_Decode(&dop->rx_pdo_map[pdo_idx],
                               &dop->od, payload, (uint8_t)payload_len);

                if (dop->on_pdo_received != NULL) {
                    uint32_t cob_id = (uint32_t)(0x200 + (pdo_idx * 0x100));
                    dop->on_pdo_received(cob_id, payload, (uint8_t)payload_len);
                }
            }
            break;
        }

        case AGR_SERIAL_MSG_HEARTBEAT:
            /* Heartbeat 수신 — 상위 계층 처리 */
            break;

        case AGR_SERIAL_MSG_SYNC:
            /* SYNC 수신 */
            if (dop->sync.sync_enabled && dop->sync.on_sync != NULL) {
                dop->sync.on_sync(dop->user_ctx);
            }
            break;

        case AGR_SERIAL_MSG_EMCY:
            /* Emergency 수신 (CiA 301: error_code + error_register) */
            if (dop->emcy.emergency_enabled && payload_len >= 3) {
                uint16_t error_code = (uint16_t)payload[0]
                                    | ((uint16_t)payload[1] << 8);
                uint8_t error_register = payload[2];
                dop->emcy.last_error_code = error_code;
                dop->emcy.last_error_register = error_register;
                if (dop->emcy.on_emergency != NULL) {
                    dop->emcy.on_emergency(error_code, error_register, dop->user_ctx);
                }
            }
            break;

        default:
            break;
    }
}

/* ===== OD Discovery (Serial Transport 전용) ===== */

#if AGR_OD_DISCOVERY_ENABLED

/**
 * @brief   OD Directory SDO Read 응답 (0x2F00).
 *          Sub 0: entry_count (UINT16 LE).
 *          Sub 1~N: entry[sub-1] descriptor BLOB.
 *
 * @details Core SDO Encode(8B 고정)를 우회하여 가변 길이 SDO 응답을
 *          Serial 프레임으로 직접 전송합니다.
 *
 * [Descriptor Wire Format]
 *   SDO Header: [CS:1B][Index:2B LE][SubIdx:1B]
 *   Payload:    [od_index:2B LE][od_subindex:1B][type:1B][size:1B][access:1B]
 *               [name_len:1B][name:NB ASCII][unit_len:1B][unit:NB ASCII]
 */
static void _SendODDirectoryResponse(AGR_Serial_Ctx_t* sctx,
                                     const AGR_SDO_Msg_t* req)
{
    const AGR_OD_Table_t* od = &sctx->dop_ctx->od;

    /* SDO 응답 payload 직접 조립 (Core AGR_SDO_Encode 우회) */
    uint8_t sdo_buf[AGR_SERIAL_MAX_PAYLOAD];
    uint32_t sdo_len = 0;

    if (req->subindex == 0) {
        /* Sub 0: OD entry count (UINT16 LE) — Expedited (8B) */
        uint8_t n = 4 - 2;  /* data_len=2, unused=2 */
        sdo_buf[0] = (uint8_t)(0x43 | (n << 2));  /* Upload Expedited, size indicated */
        sdo_buf[1] = (uint8_t)(req->index & 0xFF);
        sdo_buf[2] = (uint8_t)(req->index >> 8);
        sdo_buf[3] = req->subindex;
        sdo_buf[4] = (uint8_t)(od->entry_count & 0xFF);
        sdo_buf[5] = (uint8_t)(od->entry_count >> 8);
        sdo_buf[6] = 0;
        sdo_buf[7] = 0;
        sdo_len = 8;
    } else {
        /* Sub 1~N: entry descriptor — Non-Expedited (가변 길이) */
        uint16_t entry_idx = (uint16_t)(req->subindex - 1);
        if (entry_idx >= od->entry_count) {
            /* Abort: sub-index does not exist */
            sdo_buf[0] = 0x80;  /* Abort */
            sdo_buf[1] = (uint8_t)(req->index & 0xFF);
            sdo_buf[2] = (uint8_t)(req->index >> 8);
            sdo_buf[3] = req->subindex;
            /* Abort code: 0x06090011 — Sub-index does not exist */
            sdo_buf[4] = 0x11;
            sdo_buf[5] = 0x00;
            sdo_buf[6] = 0x09;
            sdo_buf[7] = 0x06;
            sdo_len = 8;
        } else {
            const AGR_OD_Entry_t* e = &od->entries[entry_idx];
            const char* name = (e->name != NULL) ? e->name : "";
            const char* unit = (e->unit != NULL) ? e->unit : "";
            uint8_t name_len = (uint8_t)strlen(name);
            uint8_t unit_len = (uint8_t)strlen(unit);

            if (name_len > AGR_OD_NAME_MAX_LEN) name_len = AGR_OD_NAME_MAX_LEN;
            if (unit_len > AGR_OD_UNIT_MAX_LEN) unit_len = AGR_OD_UNIT_MAX_LEN;

            /* Buffer overflow guard: 4(hdr) + 6(meta) + 1 + name + 1 + unit */
            uint32_t total = 4 + 6 + 1 + name_len + 1 + unit_len;
            if (total > AGR_SERIAL_MAX_PAYLOAD) {
                name_len = 0;  /* Truncate names to fit */
                unit_len = 0;
            }

            /* SDO Header (4B) */
            sdo_buf[0] = 0x41;  /* Non-Expedited Upload Response, size indicated */
            sdo_buf[1] = (uint8_t)(req->index & 0xFF);
            sdo_buf[2] = (uint8_t)(req->index >> 8);
            sdo_buf[3] = req->subindex;

            /* Descriptor payload */
            uint32_t off = 4;
            sdo_buf[off++] = (uint8_t)(e->index & 0xFF);
            sdo_buf[off++] = (uint8_t)(e->index >> 8);
            sdo_buf[off++] = e->subindex;
            sdo_buf[off++] = (uint8_t)e->type;
            sdo_buf[off++] = e->size;
            sdo_buf[off++] = (uint8_t)e->access;
            sdo_buf[off++] = name_len;
            memcpy(&sdo_buf[off], name, name_len);
            off += name_len;
            sdo_buf[off++] = unit_len;
            memcpy(&sdo_buf[off], unit, unit_len);
            off += unit_len;

            sdo_len = off;
        }
    }

    /* Serial 프레임으로 직접 전송 (_SendFrame 재사용) */
    _SendFrame(sctx, AGR_SERIAL_MSG_SDO_RSP, sdo_buf, sdo_len);
}

#endif /* AGR_OD_DISCOVERY_ENABLED */

#endif /* AGR_DOP_TRANSPORT_SERIAL */
