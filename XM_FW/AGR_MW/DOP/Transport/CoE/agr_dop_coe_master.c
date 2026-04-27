/**
 ******************************************************************************
 * @file    agr_dop_coe_master.c
 * @author  HyundoKim
 * @brief   AGR DOP CoE Master Transport - Implementation
 * @version 1.0
 * @date    Feb 26, 2026
 *
 * @details
 * CoE Master Transport 구현: SOEM 함수 포인터 기반 SDO/PDO 처리.
 * Core 모듈(OD, SDO, PDO)을 SOEM IOmap / Mailbox로 연결합니다.
 *
 * [SOEM 함수 포인터 매핑 예시]
 * - sdo_read  → ec_SDOread(slave, index, subindex, FALSE, &size, data, EC_TIMEOUTRXM)
 * - sdo_write → ec_SDOwrite(slave, index, subindex, FALSE, size, data, EC_TIMEOUTRXM)
 * - pdo_exchange → ec_send_processdata() + ec_receive_processdata(timeout)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_dop_coe_master.h"

#if AGR_DOP_TRANSPORT_COE

#include <string.h>

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/*============================================================
 * INITIALIZATION
 *============================================================*/

int AGR_COE_Master_Init(AGR_COE_MasterCtx_t* mctx,
                        AGR_DOP_Ctx_t* ctx,
                        const AGR_OD_Table_t* od,
                        uint8_t node_id,
                        AGR_COE_MasterSDOReadFunc_t sdo_read,
                        AGR_COE_MasterSDOWriteFunc_t sdo_write)
{
    if (mctx == NULL || ctx == NULL || od == NULL) {
        return -1;
    }

    memset(mctx, 0, sizeof(AGR_COE_MasterCtx_t));

    mctx->dop_ctx = ctx;
    mctx->sdo_read = sdo_read;
    mctx->sdo_write = sdo_write;

    /* DOP Core Context 초기화 */
    memset(ctx, 0, sizeof(AGR_DOP_Ctx_t));
    ctx->od.entries = od->entries;
    ctx->od.entry_count = od->entry_count;
    ctx->node_id = node_id;

    for (uint8_t i = 0; i < 4; i++) {
        AGR_PDO_ClearMap(&ctx->tx_pdo_map[i]);
        AGR_PDO_ClearMap(&ctx->rx_pdo_map[i]);
    }

    return 0;
}

void AGR_COE_Master_Reset(AGR_COE_MasterCtx_t* mctx)
{
    if (mctx == NULL || mctx->dop_ctx == NULL) {
        return;
    }

    AGR_DOP_Ctx_t* ctx = mctx->dop_ctx;

    for (uint8_t i = 0; i < 4; i++) {
        AGR_PDO_ClearMap(&ctx->tx_pdo_map[i]);
        AGR_PDO_ClearMap(&ctx->rx_pdo_map[i]);
    }

    mctx->iomap = NULL;
    mctx->iomap_size = 0;
    mctx->pdo_exchange = NULL;
}

/*============================================================
 * IOmap CONNECTION
 *============================================================*/

void AGR_COE_Master_SetIOmap(AGR_COE_MasterCtx_t* mctx,
                             uint8_t* iomap,
                             uint32_t size)
{
    if (mctx == NULL) {
        return;
    }

    mctx->iomap = iomap;
    mctx->iomap_size = size;
}

void AGR_COE_Master_SetPDOExchange(AGR_COE_MasterCtx_t* mctx,
                                   AGR_COE_MasterPDOExchangeFunc_t pdo_exchange)
{
    if (mctx == NULL) {
        return;
    }

    mctx->pdo_exchange = pdo_exchange;
}

/*============================================================
 * SDO TRANSPORT (SOEM 래핑)
 *============================================================*/

int AGR_COE_Master_SDORead(AGR_COE_MasterCtx_t* mctx,
                           uint16_t slave,
                           uint16_t index,
                           uint8_t subindex,
                           void* data,
                           int* size)
{
    if (mctx == NULL || mctx->sdo_read == NULL || data == NULL || size == NULL) {
        return -1;
    }

    int wkc = mctx->sdo_read(slave, index, subindex, data, size);
    if (wkc <= 0) {
        return -2;  /* SOEM SDO read failed */
    }

    return 0;
}

int AGR_COE_Master_SDOWrite(AGR_COE_MasterCtx_t* mctx,
                            uint16_t slave,
                            uint16_t index,
                            uint8_t subindex,
                            const void* data,
                            int size)
{
    if (mctx == NULL || mctx->sdo_write == NULL || data == NULL) {
        return -1;
    }

    int wkc = mctx->sdo_write(slave, index, subindex, data, size);
    if (wkc <= 0) {
        return -2;  /* SOEM SDO write failed */
    }

    return 0;
}

/*============================================================
 * PDO TRANSPORT (IOmap 기반)
 *============================================================*/

int AGR_COE_Master_DecodeTxPDO(AGR_DOP_Ctx_t* ctx,
                               const uint8_t* iomap_region,
                               uint8_t size)
{
    if (ctx == NULL || iomap_region == NULL) {
        return -1;
    }

    /*
     * Slave TxPDO → Master가 읽기 (Input 방향)
     * tx_pdo_map: Slave의 TxPDO 매핑을 미러링
     * RPDO1~4 순서로 디코딩 (Master 관점: Slave TxPDO = Master RxPDO에 해당)
     */
    uint8_t offset = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (ctx->rx_pdo_map[i].count == 0) {
            continue;
        }

        uint8_t remaining = size - offset;
        if (remaining == 0) {
            break;
        }

        int decoded = AGR_PDO_Decode(&ctx->rx_pdo_map[i],
                                      &ctx->od,
                                      &iomap_region[offset],
                                      remaining);
        if (decoded > 0) {
            offset += (uint8_t)decoded;
        }
    }

    return 0;
}

int AGR_COE_Master_EncodeRxPDO(AGR_DOP_Ctx_t* ctx,
                               uint8_t* iomap_region,
                               uint8_t* size)
{
    if (ctx == NULL || iomap_region == NULL || size == NULL) {
        return -1;
    }

    /*
     * Master → Slave RxPDO (Output 방향)
     * tx_pdo_map: Master가 Slave에게 보낼 데이터 매핑
     */
    uint8_t offset = 0;

    for (uint8_t i = 0; i < 4; i++) {
        if (ctx->tx_pdo_map[i].count == 0) {
            continue;
        }

        uint8_t remaining = AGR_COE_MAX_PI_SIZE - offset;
        if (remaining == 0) {
            break;
        }

        int len = AGR_PDO_Encode(&ctx->tx_pdo_map[i],
                                  &ctx->od,
                                  &iomap_region[offset],
                                  remaining);
        if (len > 0) {
            offset += (uint8_t)len;
        }
    }

    *size = offset;
    return 0;
}

/*============================================================
 * PDO EXCHANGE (전체 Slave 일괄)
 *============================================================*/

int AGR_COE_Master_ExchangePDO(AGR_COE_MasterCtx_t* mctx,
                               int timeout_us)
{
    if (mctx == NULL || mctx->pdo_exchange == NULL) {
        return -1;
    }

    return mctx->pdo_exchange(timeout_us);
}

#endif /* AGR_DOP_TRANSPORT_COE */
