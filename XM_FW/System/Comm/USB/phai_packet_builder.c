/**
 ******************************************************************************
 * @file    phai_packet_builder.c
 * @author  HyundoKim
 * @brief   PhAI V2.2 프로토콜 패킷 빌더 구현부
 *          CRC16-CCITT + COBS framing
 * @version 2.2
 * @date    Feb 24, 2026
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "phai_packet_builder.h"
#include "cdc_handler.h"

#include <string.h>
#include <stdatomic.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static volatile atomic_uint s_seqId = 0;
static uint32_t s_lastReportedDrops = 0;

/*
 * CRC16-CCITT Lookup Table — polynomial 0x1021, init 0xFFFF
 * PhAI Studio frontend/src/lib/serial/types.ts CRC16_TABLE과 동일
 */
static const uint16_t s_crc16Table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
};

/* Raw 패킷 조립용 버퍼 (1ms 루프 전용, 재진입 안 됨) */
static uint8_t s_pktBuf[PHAI_MAX_PACKET_SIZE];

/* COBS 인코딩 + 0x00 delimiter 출력 버퍼 */
static uint8_t s_cobsBuf[PHAI_MAX_COBS_SIZE];

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static uint32_t cobs_encode(const uint8_t *src, uint32_t src_len,
                            uint8_t *dst, uint32_t dst_max);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void PhAI_PacketBuilder_Init(void)
{
    atomic_store(&s_seqId, 0);
    s_lastReportedDrops = 0;
    CdcStream_ResetTxDropCount();
}

uint16_t PhAI_CRC16(const uint8_t* data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc = (uint16_t)((crc << 8) ^ s_crc16Table[((crc >> 8) ^ data[i]) & 0xFF]);
    }
    return crc;
}

bool PhAI_PacketBuild(const void* payload, uint32_t payload_len, uint8_t module_id)
{
    if (payload == NULL || payload_len == 0) return false;

    /* LEN = 4-byte 단위 개수 (올림) */
    uint32_t len_units = (payload_len + 3) / 4;
    if (len_units > PHAI_MAX_PAYLOAD_UNITS) return false;

    uint32_t padded_payload_len = len_units * 4;
    uint32_t raw_size = PHAI_HEADER_SIZE + padded_payload_len + PHAI_CRC_SIZE;

    /* 헤더 조립 */
    uint16_t seq = (uint16_t)atomic_fetch_add(&s_seqId, 1);

    s_pktBuf[0] = PHAI_SOF_BYTE;
    s_pktBuf[1] = (uint8_t)len_units;
    s_pktBuf[2] = (uint8_t)(seq & 0xFF);
    s_pktBuf[3] = (uint8_t)((seq >> 8) & 0xFF);
    s_pktBuf[4] = module_id;

    /* STATUS 바이트: bits 0-6 = Tx 드롭 delta (0~127 포화), bit 7 = reserved */
    uint32_t cur_drops = CdcStream_GetTxDropCount();
    uint32_t delta = cur_drops - s_lastReportedDrops;
    uint8_t status = (delta > 127) ? 127 : (uint8_t)delta;
    s_pktBuf[5] = status;
    if (delta > 0) {
        s_lastReportedDrops = cur_drops;
    }

    /* Payload 복사 */
    memcpy(&s_pktBuf[PHAI_HEADER_SIZE], payload, payload_len);

    /* 패딩 영역 0으로 클리어 (payload_len이 4의 배수가 아닌 경우) */
    if (padded_payload_len > payload_len) {
        memset(&s_pktBuf[PHAI_HEADER_SIZE + payload_len], 0x00,
               padded_payload_len - payload_len);
    }

    /* CRC16-CCITT: bytes[0] ~ bytes[raw_size - 3] (SOF 포함, CRC 자체 제외) */
    uint16_t crc = PhAI_CRC16(s_pktBuf, raw_size - PHAI_CRC_SIZE);
    s_pktBuf[raw_size - 2] = (uint8_t)(crc & 0xFF);
    s_pktBuf[raw_size - 1] = (uint8_t)((crc >> 8) & 0xFF);

    /* COBS 인코딩 */
    uint32_t cobs_len = cobs_encode(s_pktBuf, raw_size, s_cobsBuf, sizeof(s_cobsBuf) - 1);
    if (cobs_len == 0) return false;

    /* 0x00 delimiter 추가 */
    s_cobsBuf[cobs_len] = 0x00;
    cobs_len += 1;

    /* CDC Tx 링버퍼에 적재 */
    return CdcStream_Send(s_cobsBuf, cobs_len);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief COBS 인코딩. 0x00 바이트를 제거하여 프레이밍에 사용 가능하게 합니다.
 * @param[in]  src      원본 데이터
 * @param[in]  src_len  원본 길이
 * @param[out] dst      인코딩 출력 버퍼
 * @param[in]  dst_max  출력 버퍼 최대 크기
 * @return 인코딩된 바이트 수 (0x00 delimiter 미포함), 0이면 실패
 */
static uint32_t cobs_encode(const uint8_t *src, uint32_t src_len,
                            uint8_t *dst, uint32_t dst_max)
{
    uint32_t out = 0;
    uint32_t code_idx;
    uint8_t code = 1;

    if (out >= dst_max) return 0;
    code_idx = out++;   /* 첫 code byte 자리 예약 */

    for (uint32_t i = 0; i < src_len; i++) {
        if (src[i] == 0x00) {
            if (code_idx >= dst_max) return 0;
            dst[code_idx] = code;
            code = 1;
            if (out >= dst_max) return 0;
            code_idx = out++;
        } else {
            if (out >= dst_max) return 0;
            dst[out++] = src[i];
            code++;
            if (code == 0xFF) {
                dst[code_idx] = code;
                code = 1;
                if (out >= dst_max) return 0;
                code_idx = out++;
            }
        }
    }

    if (code_idx >= dst_max) return 0;
    dst[code_idx] = code;
    return out;
}
