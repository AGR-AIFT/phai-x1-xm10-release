/**
 ******************************************************************************
 * @file    agr_dop_udp.c
 * @author  HyundoKim
 * @brief   AGR DOP UDP Transport - Implementation
 * @version 1.0
 * @date    Mar 2, 2026
 *
 * @details
 * UDP Transport 구현: 패킷 헤더 파싱/생성, eth_udp_socket 콜백 연동.
 * agr_dop_canfd.c와 대칭 구조.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_dop_config.h"

#if AGR_DOP_TRANSPORT_UDP

#include "agr_dop_udp.h"
#include "eth_udp_socket.h"
#include <string.h>

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES (Singleton)
 *-----------------------------------------------------------
 */

static AGR_DOP_Ctx_t*       s_udp_ctx    = NULL;
static uint8_t              s_node_id    = 0;

static AGR_UDP_PdoCallback_t  s_pdo_cb   = NULL;
static AGR_UDP_SdoCallback_t  s_sdo_cb   = NULL;
static AGR_UDP_EmcyCallback_t s_emcy_cb  = NULL;

/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void _OnPdoReceived(EthUdpChannel_t ch, const uint8_t* data,
                            uint16_t len, uint32_t src_ip, uint16_t src_port);
static void _OnSdoReceived(EthUdpChannel_t ch, const uint8_t* data,
                            uint16_t len, uint32_t src_ip, uint16_t src_port);
static void _OnEmcyReceived(EthUdpChannel_t ch, const uint8_t* data,
                             uint16_t len, uint32_t src_ip, uint16_t src_port);

static int  _BuildHeader(uint8_t* buf, uint8_t type);
static int  _SendUdpPacket(EthUdpChannel_t ch, uint8_t type,
                            const uint8_t* payload, uint16_t payload_len);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

int AGR_UDP_Init(AGR_DOP_Ctx_t* ctx, const AGR_OD_Table_t* od, uint8_t node_id)
{
    if (ctx == NULL) return -1;
    if (!EthUdpSocket_IsReady()) return -2;

    s_udp_ctx = ctx;
    s_node_id = node_id;

    /* Context 기본 설정 */
    memset(ctx, 0, sizeof(AGR_DOP_Ctx_t));
    ctx->node_id = node_id;
    if (od != NULL) {
        ctx->od = *od;
    }

    /* eth_udp_socket RX 콜백 등록 (채널별) */
    EthUdpSocket_RegisterRxCallback(ETH_UDP_CH_PDO,  _OnPdoReceived);
    EthUdpSocket_RegisterRxCallback(ETH_UDP_CH_SDO,  _OnSdoReceived);
    EthUdpSocket_RegisterRxCallback(ETH_UDP_CH_EMCY, _OnEmcyReceived);

    return 0;
}

void AGR_UDP_DeInit(void)
{
    EthUdpSocket_RegisterRxCallback(ETH_UDP_CH_PDO,  NULL);
    EthUdpSocket_RegisterRxCallback(ETH_UDP_CH_SDO,  NULL);
    EthUdpSocket_RegisterRxCallback(ETH_UDP_CH_EMCY, NULL);

    s_udp_ctx = NULL;
    s_pdo_cb  = NULL;
    s_sdo_cb  = NULL;
    s_emcy_cb = NULL;
}

/* --- TX Functions --- */

int AGR_UDP_SendTxPDO(AGR_DOP_Ctx_t* ctx, uint8_t pdo_num)
{
    if (ctx == NULL || pdo_num < 1 || pdo_num > 4) return -1;

    /* Core PDO Encode → payload */
    uint8_t payload[AGR_CANFD_MAX_PAYLOAD];
    int enc_len = AGR_PDO_Encode(&ctx->tx_pdo_map[pdo_num - 1], &ctx->od,
                                  payload, sizeof(payload));
    if (enc_len <= 0) return -2;

    /* TPDO type = 0x01~0x04 */
    uint8_t type = AGR_UDP_TYPE_TPDO1 + (pdo_num - 1);
    return _SendUdpPacket(ETH_UDP_CH_PDO, type, payload, (uint16_t)enc_len);
}

int AGR_UDP_SendSDO(AGR_DOP_Ctx_t* ctx, uint8_t target_id,
                     const AGR_SDO_Msg_t* msg)
{
    (void)target_id; /* UDP는 기본 AM IP로 전송 */
    if (ctx == NULL || msg == NULL) return -1;

    uint8_t payload[AGR_CANFD_MAX_PAYLOAD];
    int enc_len = AGR_SDO_Encode(msg, payload);
    if (enc_len <= 0) return -2;

    /* SDO Response */
    return _SendUdpPacket(ETH_UDP_CH_SDO, AGR_UDP_TYPE_SDO_RSP,
                           payload, (uint16_t)enc_len);
}

int AGR_UDP_SendSYNC(AGR_DOP_Ctx_t* ctx)
{
    if (ctx == NULL) return -1;
    /* SYNC는 payload 없이 헤더만 전송 */
    return _SendUdpPacket(ETH_UDP_CH_PDO, AGR_UDP_TYPE_SYNC, NULL, 0);
}

int AGR_UDP_SendEmergency(AGR_DOP_Ctx_t* ctx, uint16_t error_code,
                           uint8_t error_register)
{
    if (ctx == NULL) return -1;

    /* CiA 301 EMCY format: [ErrCode Lo][ErrCode Hi][ErrReg] */
    uint8_t payload[3];
    payload[0] = (uint8_t)(error_code & 0xFF);
    payload[1] = (uint8_t)(error_code >> 8);
    payload[2] = error_register;

    return _SendUdpPacket(ETH_UDP_CH_EMCY, AGR_UDP_TYPE_EMCY,
                           payload, 3);
}

int AGR_UDP_SendRawPDO(uint8_t pdo_type, const uint8_t* data, uint8_t len)
{
    return _SendUdpPacket(ETH_UDP_CH_PDO, pdo_type, data, len);
}

/* --- Callback Registration --- */

void AGR_UDP_RegisterPdoCallback(AGR_UDP_PdoCallback_t cb)  { s_pdo_cb  = cb; }
void AGR_UDP_RegisterSdoCallback(AGR_UDP_SdoCallback_t cb)  { s_sdo_cb  = cb; }
void AGR_UDP_RegisterEmcyCallback(AGR_UDP_EmcyCallback_t cb){ s_emcy_cb = cb; }

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief UDP 패킷 헤더 생성
 * @param buf  최소 AGR_UDP_HEADER_SIZE 바이트 버퍼
 * @param type 메시지 타입
 * @return AGR_UDP_HEADER_SIZE (항상 4)
 */
static int _BuildHeader(uint8_t* buf, uint8_t type)
{
    buf[0] = AGR_UDP_VERSION;
    buf[1] = type;
    buf[2] = s_node_id;
    buf[3] = 0x00; /* reserved */
    return AGR_UDP_HEADER_SIZE;
}

/**
 * @brief 헤더 + payload를 조합하여 UDP 전송
 */
static int _SendUdpPacket(EthUdpChannel_t ch, uint8_t type,
                           const uint8_t* payload, uint16_t payload_len)
{
    uint8_t buf[AGR_UDP_HEADER_SIZE + AGR_CANFD_MAX_PAYLOAD];
    uint16_t total_len = AGR_UDP_HEADER_SIZE + payload_len;

    if (total_len > sizeof(buf)) return -1;

    _BuildHeader(buf, type);
    if (payload != NULL && payload_len > 0) {
        memcpy(&buf[AGR_UDP_HEADER_SIZE], payload, payload_len);
    }

    return EthUdpSocket_Send(ch, buf, total_len);
}

/* --- RX Callbacks (eth_udp_socket → AGR_DOP → am_drv) --- */

/**
 * @brief PDO 수신 핸들러 (tcpip_thread context)
 */
static void _OnPdoReceived(EthUdpChannel_t ch, const uint8_t* data,
                            uint16_t len, uint32_t src_ip, uint16_t src_port)
{
    (void)ch; (void)src_ip; (void)src_port;

    if (len < AGR_UDP_HEADER_SIZE) return;
    if (data[0] != AGR_UDP_VERSION) return;

    uint8_t type   = data[1];
    uint8_t src_id = data[2];
    const uint8_t* payload = &data[AGR_UDP_HEADER_SIZE];
    uint16_t payload_len = len - AGR_UDP_HEADER_SIZE;

    /* SYNC 처리 */
    if (type == AGR_UDP_TYPE_SYNC) {
        if (s_udp_ctx != NULL && s_udp_ctx->sync.sync_enabled &&
            s_udp_ctx->sync.on_sync != NULL) {
            s_udp_ctx->sync.on_sync(s_udp_ctx->user_ctx);
        }
        return;
    }

    /* PDO → Device Layer 콜백 */
    if (s_pdo_cb != NULL && payload_len > 0) {
        s_pdo_cb(type, src_id, payload, (uint8_t)payload_len);
    }
}

/**
 * @brief SDO 수신 핸들러 (tcpip_thread context)
 */
static void _OnSdoReceived(EthUdpChannel_t ch, const uint8_t* data,
                            uint16_t len, uint32_t src_ip, uint16_t src_port)
{
    (void)ch; (void)src_ip; (void)src_port;

    if (len < AGR_UDP_HEADER_SIZE) return;
    if (data[0] != AGR_UDP_VERSION) return;

    uint8_t msg_type = data[1];
    uint8_t src_id   = data[2];
    const uint8_t* payload = &data[AGR_UDP_HEADER_SIZE];
    uint16_t payload_len = len - AGR_UDP_HEADER_SIZE;

    /* SDO → Device Layer 콜백 */
    if (s_sdo_cb != NULL && payload_len > 0) {
        s_sdo_cb(msg_type, src_id, payload, (uint8_t)payload_len);
    }
}

/**
 * @brief EMCY 수신 핸들러 (tcpip_thread context)
 */
static void _OnEmcyReceived(EthUdpChannel_t ch, const uint8_t* data,
                             uint16_t len, uint32_t src_ip, uint16_t src_port)
{
    (void)ch; (void)src_ip; (void)src_port;

    if (len < AGR_UDP_HEADER_SIZE + 3) return;
    if (data[0] != AGR_UDP_VERSION) return;

    uint8_t src_id = data[2];
    uint16_t error_code = (uint16_t)data[AGR_UDP_HEADER_SIZE] |
                          ((uint16_t)data[AGR_UDP_HEADER_SIZE + 1] << 8);
    uint8_t error_register = data[AGR_UDP_HEADER_SIZE + 2];

    if (s_emcy_cb != NULL) {
        s_emcy_cb(src_id, error_code, error_register);
    }
}

#endif /* AGR_DOP_TRANSPORT_UDP */
