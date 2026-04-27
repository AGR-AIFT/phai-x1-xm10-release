/**
 ******************************************************************************
 * @file    agr_dop_canfd.c
 * @author  HyundoKim
 * @brief   AGR DOP CAN-FD Transport - Implementation
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * CAN-FD Transport 계층 구현: CAN-ID 계산, RX 메시지 라우팅, TX 프레임 생성.
 * Core 모듈(OD, SDO, PDO)을 CAN-FD 프레임으로 감쌉니다.
 *
 * 이전 agr_dop.c에서 CAN-FD 의존 코드를 분리한 결과물입니다 (Session 4).
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_dop_canfd.h"

#if AGR_DOP_TRANSPORT_CANFD

#include <string.h>

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _ProcessSyncMessage(AGR_DOP_Ctx_t* ctx, uint32_t current_ms);

static void _ProcessEmergencyMessage(AGR_DOP_Ctx_t* ctx,
                                      const uint8_t* data,
                                      uint8_t len);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/*============================================================
 * INITIALIZATION
 *============================================================*/

int AGR_CANFD_Init(AGR_DOP_Ctx_t* ctx,
                   const AGR_OD_Table_t* od,
                   uint8_t node_id,
                   AGR_TxFunc_t tx_func)
{
    if (ctx == NULL || tx_func == NULL) {
        return -1;
    }

    memset(ctx, 0, sizeof(AGR_DOP_Ctx_t));

    /* OD는 Slave 역할에서만 필요 (Master는 NULL 허용) */
    if (od != NULL) {
        ctx->od.entries = od->entries;
        ctx->od.entry_count = od->entry_count;
    }
    ctx->node_id = node_id;
    ctx->tx_func = tx_func;

    for (uint8_t i = 0; i < 4; i++) {
        AGR_PDO_ClearMap(&ctx->tx_pdo_map[i]);
        AGR_PDO_ClearMap(&ctx->rx_pdo_map[i]);
    }

    return 0;
}

void AGR_CANFD_Reset(AGR_DOP_Ctx_t* ctx)
{
    if (ctx == NULL) {
        return;
    }

    for (uint8_t i = 0; i < 4; i++) {
        AGR_PDO_ClearMap(&ctx->tx_pdo_map[i]);
        AGR_PDO_ClearMap(&ctx->rx_pdo_map[i]);
    }
}

/*============================================================
 * SDO TRANSPORT
 *============================================================*/

int AGR_CANFD_ProcessSDORequest(AGR_DOP_Ctx_t* ctx,
                                const AGR_SDO_Msg_t* req,
                                AGR_SDO_Msg_t* out_rsp)
{
    if (ctx == NULL || req == NULL || out_rsp == NULL) {
        return -1;
    }

    if (ctx->on_sdo_request != NULL) {
        ctx->on_sdo_request(req, out_rsp);
        if (out_rsp->cs != 0) {
            return 0;
        }
    }

    return AGR_SDO_ProcessRequest(&ctx->od, req, out_rsp);
}

int AGR_CANFD_SendSDO(AGR_DOP_Ctx_t* ctx, uint8_t target_id, const AGR_SDO_Msg_t* msg)
{
    if (ctx == NULL || ctx->tx_func == NULL || msg == NULL) {
        return -1;
    }

    uint8_t buf[AGR_CANFD_MAX_PAYLOAD];
    int len = AGR_SDO_Encode(msg, buf);

    if (len < 0) {
        return len;
    }

    uint32_t can_id;
    if (msg->cs == AGR_SDO_CS_UPLOAD_INIT_REQ || msg->cs == AGR_SDO_CS_DOWNLOAD_INIT_REQ) {
        can_id = AGR_CANFD_GetSDORequestID(target_id);
    } else {
        can_id = AGR_CANFD_GetSDOResponseID(ctx->node_id);
    }

    return ctx->tx_func(can_id, buf, (uint8_t)len);
}

int AGR_CANFD_SendSDOWrite(AGR_DOP_Ctx_t* ctx,
                           uint16_t index,
                           uint8_t subindex,
                           const void* data,
                           uint8_t data_len)
{
    if (ctx == NULL || ctx->tx_func == NULL || data == NULL) {
        return -1;
    }

    if (data_len > AGR_SDO_MAX_DATA_SIZE) {
        return -2;
    }

    AGR_SDO_Msg_t sdo_req;
    AGR_SDO_CreateWriteReq(&sdo_req, index, subindex, data, data_len);

    uint8_t buf[AGR_CANFD_MAX_PAYLOAD];
    int len = AGR_SDO_Encode(&sdo_req, buf);
    if (len <= 0) {
        return -3;
    }

    uint32_t can_id = AGR_CANFD_GetSDORequestID(ctx->target_node_id);
    return ctx->tx_func(can_id, buf, (uint8_t)len);
}

void AGR_CANFD_SetTargetNodeId(AGR_DOP_Ctx_t* ctx, uint8_t target_node_id)
{
    if (ctx != NULL) {
        ctx->target_node_id = target_node_id;
    }
}

/*============================================================
 * PDO TRANSPORT
 *============================================================*/

int AGR_CANFD_SendTxPDO(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num)
{
    if (ctx == NULL || ctx->tx_func == NULL) {
        return -1;
    }

    if (pdo_num < 1 || pdo_num > 4) {
        return -2;
    }

    uint8_t buf[AGR_CANFD_MAX_PAYLOAD];
    int len = AGR_PDO_Encode(&ctx->tx_pdo_map[pdo_num - 1],
                             &ctx->od, buf, sizeof(buf));

    if (len <= 0) {
        return -3;
    }

    uint32_t can_id = AGR_CANFD_GetTPDOID(ctx->node_id, pdo_num);

    return ctx->tx_func(can_id, buf, (uint8_t)len);
}

/*============================================================
 * MESSAGE PROCESSING (RX)
 *============================================================*/

int AGR_CANFD_ProcessRxMessage(AGR_DOP_Ctx_t* ctx,
                               uint32_t can_id,
                               const uint8_t* data,
                               uint8_t len)
{
    if (ctx == NULL || data == NULL) {
        return -1;
    }

    uint8_t func_code = (can_id & 0x780) >> 7;
    uint8_t source_node = can_id & 0x7F;

    switch (func_code) {
        case AGR_CAN_FUNC_NMT:
            return 1;

        case AGR_CAN_FUNC_SYNC_EMCY:
            if (can_id == AGR_CAN_ID_SYNC) {
                uint32_t current_ms = 0;
                _ProcessSyncMessage(ctx, current_ms);
                return 0;
            }
            if (source_node != 0) {
                _ProcessEmergencyMessage(ctx, data, len);
                return 0;
            }
            return 1;

        case AGR_CAN_FUNC_TPDO1:
        case AGR_CAN_FUNC_TPDO2:
        case AGR_CAN_FUNC_TPDO3:
        case AGR_CAN_FUNC_TPDO4:
            if (source_node == ctx->target_node_id) {
                return 0;
            }
            return 1;

        case AGR_CAN_FUNC_RPDO1:
        case AGR_CAN_FUNC_RPDO2:
        case AGR_CAN_FUNC_RPDO3:
        case AGR_CAN_FUNC_RPDO4:
            {
                uint8_t pdo_num = 0;
                uint32_t expected_can_id = 0;

                if (func_code == AGR_CAN_FUNC_RPDO1) {
                    pdo_num = 1;
                    expected_can_id = AGR_CANFD_GetRPDOID(ctx->node_id, pdo_num);
                } else if (func_code == AGR_CAN_FUNC_RPDO2) {
                    pdo_num = 2;
                    expected_can_id = AGR_CANFD_GetRPDOID(ctx->node_id, pdo_num);
                } else if (func_code == AGR_CAN_FUNC_RPDO3) {
                    pdo_num = 3;
                    expected_can_id = AGR_CANFD_GetRPDOID(ctx->node_id, pdo_num);
                } else if (func_code == AGR_CAN_FUNC_RPDO4) {
                    pdo_num = 4;
                    expected_can_id = AGR_CANFD_GetRPDOID(ctx->node_id, pdo_num);
                }

                if (can_id == expected_can_id) {
                    AGR_PDO_Decode(&ctx->rx_pdo_map[pdo_num - 1],
                                   &ctx->od, data, len);

                    if (ctx->on_pdo_received != NULL) {
                        ctx->on_pdo_received(can_id, data, len);
                    }

                    return 0;
                }
            }
            return 1;

        case AGR_CAN_FUNC_SDO_TX:
            return 1;

        case AGR_CAN_FUNC_SDO_RX:
            if (can_id == AGR_CANFD_GetSDORequestID(ctx->node_id)) {
                AGR_SDO_Msg_t req, rsp;

                if (AGR_SDO_Decode(data, len, &req) != 0) {
                    return -2;
                }

                if (AGR_CANFD_ProcessSDORequest(ctx, &req, &rsp) != 0) {
                    return -3;
                }

                uint8_t rsp_buf[AGR_CANFD_MAX_PAYLOAD];
                int rsp_len = AGR_SDO_Encode(&rsp, rsp_buf);
                if (rsp_len > 0) {
                    ctx->tx_func(AGR_CANFD_GetSDOResponseID(ctx->node_id),
                                rsp_buf, (uint8_t)rsp_len);
                }

                return 0;
            }
            return 1;

        case AGR_CAN_FUNC_HEARTBEAT:
            return 1;

        default:
            return 1;
    }
}

/*============================================================
 * SYNC TRANSPORT
 *============================================================*/

void AGR_CANFD_EnableSync(AGR_DOP_Ctx_t* ctx,
                          uint32_t period_us,
                          void (*on_sync)(void* ctx))
{
    if (ctx == NULL) {
        return;
    }

    ctx->sync.sync_enabled = true;
    ctx->sync.sync_period_us = period_us;
    ctx->sync.sync_counter = 0;
    ctx->sync.last_sync_tick = 0;
    ctx->sync.on_sync = on_sync;
}

void AGR_CANFD_DisableSync(AGR_DOP_Ctx_t* ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->sync.sync_enabled = false;
    ctx->sync.on_sync = NULL;
}

int AGR_CANFD_SendSYNC(AGR_DOP_Ctx_t* ctx)
{
    if (ctx == NULL || ctx->tx_func == NULL) {
        return -1;
    }

    if (!ctx->sync.sync_enabled) {
        return -2;
    }

    ctx->sync.sync_counter++;

    uint8_t sync_data[1] = { ctx->sync.sync_counter };
    uint32_t can_id = AGR_CAN_ID_SYNC;

    return ctx->tx_func(can_id, sync_data, 1);
}

static void _ProcessSyncMessage(AGR_DOP_Ctx_t* ctx, uint32_t current_ms)
{
    if (!ctx->sync.sync_enabled) {
        return;
    }

    ctx->sync.last_sync_tick = current_ms;

    if (ctx->sync.on_sync != NULL) {
        ctx->sync.on_sync(ctx->user_ctx);
    }
}

/*============================================================
 * EMERGENCY TRANSPORT
 *============================================================*/

void AGR_CANFD_EnableEmergency(AGR_DOP_Ctx_t* ctx,
                               void (*on_emergency)(uint16_t error_code,
                                                     uint8_t error_register,
                                                     void* ctx))
{
    if (ctx == NULL) {
        return;
    }

    ctx->emcy.emergency_enabled = true;
    ctx->emcy.last_error_code = 0;
    ctx->emcy.last_emcy_sent_ms = 0;
    ctx->emcy.on_emergency = on_emergency;
}

int AGR_CANFD_SendEmergency(AGR_DOP_Ctx_t* ctx, uint16_t error_code,
                             uint8_t error_register)
{
    if (ctx == NULL || ctx->tx_func == NULL) {
        return -1;
    }

    if (!ctx->emcy.emergency_enabled) {
        return -2;
    }

    /* CiA 301 EMCY format: [ErrCode Lo][ErrCode Hi][ErrReg][MfgSpec x5] */
    uint8_t emcy_data[8] = {
        (uint8_t)(error_code & 0xFF),
        (uint8_t)(error_code >> 8),
        error_register,
        0x00, 0x00, 0x00, 0x00, 0x00
    };

    uint32_t can_id = AGR_CAN_ID_EMCY + ctx->node_id;

    ctx->emcy.last_error_code = error_code;

    return ctx->tx_func(can_id, emcy_data, 8);
}

static void _ProcessEmergencyMessage(AGR_DOP_Ctx_t* ctx,
                                      const uint8_t* data,
                                      uint8_t len)
{
    if (!ctx->emcy.emergency_enabled || len < 3) {
        return;
    }

    uint16_t error_code = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    uint8_t error_register = data[2];

    ctx->emcy.last_error_code = error_code;
    ctx->emcy.last_error_register = error_register;

    if (ctx->emcy.on_emergency != NULL) {
        ctx->emcy.on_emergency(error_code, error_register, ctx->user_ctx);
    }
}

#endif /* AGR_DOP_TRANSPORT_CANFD */
