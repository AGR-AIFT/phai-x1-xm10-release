/**
 ******************************************************************************
 * @file    agr_dop_coe_slave.c
 * @author  HyundoKim
 * @brief   AGR DOP CoE Slave Transport - Implementation
 * @version 1.0
 * @date    Feb 26, 2026
 *
 * @details
 * CoE Slave Transport 구현: Process Image PDO, Mailbox SDO, Emergency.
 * Core 모듈(OD, SDO, PDO)을 EtherCAT Process Image / Mailbox로 감쌉니다.
 *
 * [CAN-FD agr_dop_canfd.c와의 대응 관계]
 * - AGR_CANFD_Init          → AGR_COE_Init (tx_func 불필요)
 * - AGR_CANFD_SendTxPDO     → AGR_COE_EncodeTxPDO (Process Image에 패킹)
 * - AGR_CANFD_ProcessRxMsg  → AGR_COE_DecodeRxPDO (Process Image에서 언패킹)
 * - AGR_CANFD_ProcessSDOReq → AGR_COE_ProcessSDORequest / SDORead / SDOWrite
 * - AGR_CANFD_SendEmergency → AGR_COE_PrepareEmergency (데이터 준비만)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_dop_coe_slave.h"

#if AGR_DOP_TRANSPORT_COE

#include <string.h>

/**
 *------------------------------------------------------------
 * PRIVATE HELPER: PDO Mapping 크기 계산
 *------------------------------------------------------------
 */

/**
 * @brief 단일 PDO Mapping의 총 바이트 크기 계산
 * @param map Mapping Table
 * @param od  Object Dictionary
 * @return 총 바이트 수
 */
static uint8_t _CalcPDOSize(const AGR_PDO_MapTable_t* map,
                            const AGR_OD_Table_t* od)
{
    uint8_t total = 0;

    for (uint8_t i = 0; i < map->count; i++) {
        const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(
            od, map->items[i].od_index, map->items[i].od_subindex);
        if (entry != NULL) {
            total += entry->size;
        }
    }

    return total;
}

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/*============================================================
 * INITIALIZATION
 *============================================================*/

int AGR_COE_Init(AGR_DOP_Ctx_t* ctx,
                 const AGR_OD_Table_t* od,
                 uint8_t node_id)
{
    if (ctx == NULL || od == NULL) {
        return -1;
    }

    memset(ctx, 0, sizeof(AGR_DOP_Ctx_t));

    ctx->od.entries = od->entries;
    ctx->od.entry_count = od->entry_count;
    ctx->node_id = node_id;
    /* tx_func은 NULL — CoE는 Process Image 기반이므로 불필요 */

    for (uint8_t i = 0; i < 4; i++) {
        AGR_PDO_ClearMap(&ctx->tx_pdo_map[i]);
        AGR_PDO_ClearMap(&ctx->rx_pdo_map[i]);
    }

    return 0;
}

void AGR_COE_Reset(AGR_DOP_Ctx_t* ctx)
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
 * SDO TRANSPORT — 경로 A: Raw Bytes
 *============================================================*/

int AGR_COE_ProcessSDORequest(AGR_DOP_Ctx_t* ctx,
                              const uint8_t* req_data,
                              uint8_t req_len,
                              uint8_t* rsp_data,
                              uint8_t* rsp_len)
{
    if (ctx == NULL || req_data == NULL || rsp_data == NULL || rsp_len == NULL) {
        return -1;
    }

    /* SDO 바이트 → 구조체 디코딩 */
    AGR_SDO_Msg_t req;
    if (AGR_SDO_Decode(req_data, req_len, &req) != 0) {
        return -2;
    }

    AGR_SDO_Msg_t rsp;

    /* on_sdo_request 콜백 우선 처리 (CAN-FD와 동일 패턴) */
    if (ctx->on_sdo_request != NULL) {
        ctx->on_sdo_request(&req, &rsp);
        if (rsp.cs != 0) {
            int len = AGR_SDO_Encode(&rsp, rsp_data);
            if (len > 0) {
                *rsp_len = (uint8_t)len;
                return 0;
            }
            return -3;
        }
    }

    /* Core OD 기반 처리 */
    if (AGR_SDO_ProcessRequest(&ctx->od, &req, &rsp) != 0) {
        return -4;
    }

    int len = AGR_SDO_Encode(&rsp, rsp_data);
    if (len <= 0) {
        return -5;
    }

    *rsp_len = (uint8_t)len;
    return 0;
}

/*============================================================
 * SDO TRANSPORT — 경로 B: Parsed (index/subindex)
 *============================================================*/

int AGR_COE_SDORead(AGR_DOP_Ctx_t* ctx,
                    uint16_t index,
                    uint8_t subindex,
                    void* out_data,
                    uint8_t* out_len)
{
    if (ctx == NULL || out_data == NULL || out_len == NULL) {
        return -1;
    }

    const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(&ctx->od, index, subindex);
    if (entry == NULL) {
        return -2;  /* Object does not exist */
    }

    int read_len = AGR_OD_ReadValue(entry, out_data, entry->size);
    if (read_len < 0) {
        return read_len;  /* Pass through OD error code */
    }

    *out_len = (uint8_t)read_len;
    return 0;
}

int AGR_COE_SDOWrite(AGR_DOP_Ctx_t* ctx,
                     uint16_t index,
                     uint8_t subindex,
                     const void* data,
                     uint8_t len)
{
    if (ctx == NULL || data == NULL) {
        return -1;
    }

    const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(&ctx->od, index, subindex);
    if (entry == NULL) {
        return -2;  /* Object does not exist */
    }

    return AGR_OD_WriteValue(entry, data, len);
}

/*============================================================
 * PDO TRANSPORT — Process Image 기반 (개별)
 *============================================================*/

int AGR_COE_EncodeTxPDO(AGR_DOP_Ctx_t* ctx,
                        uint8_t pdo_num,
                        uint8_t* process_image,
                        uint8_t* size)
{
    if (ctx == NULL || process_image == NULL || size == NULL) {
        return -1;
    }

    if (pdo_num < 1 || pdo_num > 4) {
        return -2;
    }

    int len = AGR_PDO_Encode(&ctx->tx_pdo_map[pdo_num - 1],
                             &ctx->od, process_image, AGR_COE_MAX_PI_SIZE);
    if (len < 0) {
        return -3;
    }

    *size = (uint8_t)len;
    return 0;
}

int AGR_COE_DecodeRxPDO(AGR_DOP_Ctx_t* ctx,
                        uint8_t pdo_num,
                        const uint8_t* process_image,
                        uint8_t size)
{
    if (ctx == NULL || process_image == NULL) {
        return -1;
    }

    if (pdo_num < 1 || pdo_num > 4) {
        return -2;
    }

    int decoded = AGR_PDO_Decode(&ctx->rx_pdo_map[pdo_num - 1],
                                 &ctx->od, process_image, size);
    if (decoded < 0) {
        return -3;
    }

    return 0;
}

/*============================================================
 * PDO TRANSPORT — Process Image 기반 (일괄)
 *============================================================*/

int AGR_COE_EncodeAllTxPDO(AGR_DOP_Ctx_t* ctx,
                           uint8_t* process_image,
                           uint8_t buf_size,
                           uint8_t* out_size)
{
    if (ctx == NULL || process_image == NULL || out_size == NULL) {
        return -1;
    }

    uint8_t offset = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (ctx->tx_pdo_map[i].count == 0) {
            continue;
        }

        uint8_t remaining = buf_size - offset;
        if (remaining == 0) {
            break;
        }

        int len = AGR_PDO_Encode(&ctx->tx_pdo_map[i],
                                 &ctx->od,
                                 &process_image[offset],
                                 remaining);
        if (len > 0) {
            offset += (uint8_t)len;
        }
    }

    *out_size = offset;
    return 0;
}

int AGR_COE_DecodeAllRxPDO(AGR_DOP_Ctx_t* ctx,
                           const uint8_t* process_image,
                           uint8_t size)
{
    if (ctx == NULL || process_image == NULL) {
        return -1;
    }

    uint8_t offset = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (ctx->rx_pdo_map[i].count == 0) {
            continue;
        }

        uint8_t pdo_size = _CalcPDOSize(&ctx->rx_pdo_map[i], &ctx->od);
        if (pdo_size == 0 || (offset + pdo_size) > size) {
            break;
        }

        AGR_PDO_Decode(&ctx->rx_pdo_map[i],
                        &ctx->od,
                        &process_image[offset],
                        pdo_size);

        offset += pdo_size;
    }

    return 0;
}

/*============================================================
 * PROCESS IMAGE SIZE QUERY
 *============================================================*/

uint8_t AGR_COE_GetTxProcessImageSize(const AGR_DOP_Ctx_t* ctx)
{
    if (ctx == NULL) {
        return 0;
    }

    uint8_t total = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (ctx->tx_pdo_map[i].count == 0) {
            continue;
        }
        total += _CalcPDOSize(&ctx->tx_pdo_map[i], &ctx->od);
    }

    return total;
}

uint8_t AGR_COE_GetRxProcessImageSize(const AGR_DOP_Ctx_t* ctx)
{
    if (ctx == NULL) {
        return 0;
    }

    uint8_t total = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (ctx->rx_pdo_map[i].count == 0) {
            continue;
        }
        total += _CalcPDOSize(&ctx->rx_pdo_map[i], &ctx->od);
    }

    return total;
}

/*============================================================
 * EMERGENCY over CoE (Mailbox 경유)
 *============================================================*/

int AGR_COE_PrepareEmergency(AGR_DOP_Ctx_t* ctx,
                             uint16_t error_code,
                             uint8_t error_register,
                             uint8_t* out_data,
                             uint8_t* out_len)
{
    if (ctx == NULL || out_data == NULL || out_len == NULL) {
        return -1;
    }

    /* CiA 301 Emergency 데이터 포맷 (8 bytes):
     * [0-1] Error Code (Little Endian)
     * [2]   Error Register (OD 0x1001)
     * [3-7] Manufacturer Specific (0으로 초기화) */
    out_data[0] = (uint8_t)(error_code & 0xFF);
    out_data[1] = (uint8_t)(error_code >> 8);
    out_data[2] = error_register;
    out_data[3] = 0x00;
    out_data[4] = 0x00;
    out_data[5] = 0x00;
    out_data[6] = 0x00;
    out_data[7] = 0x00;

    *out_len = 8;

    /* Context에 마지막 에러 코드 기록 */
    ctx->emcy.last_error_code = error_code;

    return 0;
}

#endif /* AGR_DOP_TRANSPORT_COE */
