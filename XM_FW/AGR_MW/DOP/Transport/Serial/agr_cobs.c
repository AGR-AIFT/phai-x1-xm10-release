/**
 ******************************************************************************
 * @file    agr_cobs.c
 * @author  HyundoKim
 * @brief   COBS Framing Layer + CRC-16/CCITT Implementation
 * @version 1.0
 * @date    2026-03-25
 *
 * @details
 * COBS 인코더/디코더 + CRC-16/CCITT 구현.
 * 0x00 delimiter 기반 프레이밍으로 UART/SPI/BLE 시리얼 인터페이스에서
 * 프레임 경계 감지 + 무결성 검증을 제공합니다.
 *
 * [성능 고려]
 * - CRC LUT (256B) 사용으로 O(1) per byte
 * - 디코더: 힙 미사용, ISR 내 호출 안전
 * - 인코더: 스택 버퍼만 사용, 재진입 안전
 *
 * [출처]
 * - AGR_COBS_Encode(): XM phai_packet_builder.c cobs_encode() 동일 추출
 * - AGR_COBS_Decode(): XM boot_ftp_trigger.c _COBSDecode() 동일 추출
 * - CRC-16/CCITT LUT: XM phai_packet_builder.c PhAI_CRC16() 동일
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_cobs.h"

/* ===== CRC-16/CCITT Lookup Table ===== */

/**
 * @brief   CRC-16/CCITT(0x1021) LUT.
 *          Polynomial: x^16 + x^12 + x^5 + 1
 */
static const uint16_t s_crc16_lut[256] = {
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

/* ===== CRC-16/CCITT ===== */

uint16_t AGR_COBS_CRC16(const uint8_t* data, uint32_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint32_t i = 0; i < len; i++) {
        uint8_t idx = (uint8_t)((crc >> 8) ^ data[i]);
        crc = (crc << 8) ^ s_crc16_lut[idx];
    }

    return crc;
}

/* ===== COBS Raw Encode ===== */

uint32_t AGR_COBS_Encode(const uint8_t* src, uint32_t src_len,
                          uint8_t* dst, uint32_t dst_max)
{
    if (src == NULL || dst == NULL || dst_max == 0) {
        return 0;
    }

    uint32_t out = 0;
    uint32_t code_idx;
    uint8_t code = 1;

    /* 첫 code byte 자리 예약 */
    if (out >= dst_max) return 0;
    code_idx = out++;

    for (uint32_t i = 0; i < src_len; i++) {
        if (src[i] == 0x00) {
            /* 0x00 발견 → 현재 code 기록, 새 code byte 자리 예약 */
            if (code_idx >= dst_max) return 0;
            dst[code_idx] = code;
            code = 1;
            if (out >= dst_max) return 0;
            code_idx = out++;
        } else {
            /* 일반 바이트 복사 */
            if (out >= dst_max) return 0;
            dst[out++] = src[i];
            code++;
            if (code == 0xFF) {
                /* 254바이트 연속 non-zero → code 기록, 새 블록 시작 */
                dst[code_idx] = code;
                code = 1;
                if (out >= dst_max) return 0;
                code_idx = out++;
            }
        }
    }

    /* 마지막 code 기록 */
    if (code_idx >= dst_max) return 0;
    dst[code_idx] = code;

    return out;
}

/* ===== COBS Raw Decode ===== */

uint32_t AGR_COBS_Decode(const uint8_t* encoded, uint32_t enc_len,
                          uint8_t* decoded, uint32_t dec_max)
{
    if (encoded == NULL || decoded == NULL || enc_len == 0) {
        return 0;
    }

    uint32_t dec_idx = 0;
    uint32_t enc_idx = 0;

    while (enc_idx < enc_len) {
        uint8_t code = encoded[enc_idx++];
        if (code == 0U) {
            /* 0x00은 COBS 인코딩 데이터에 나타나지 않음 → 에러 */
            return 0;
        }

        /* code - 1 개의 데이터 바이트 복사 */
        for (uint8_t i = 1; i < code; i++) {
            if (enc_idx >= enc_len || dec_idx >= dec_max) {
                return 0;
            }
            decoded[dec_idx++] = encoded[enc_idx++];
        }

        /* code < 0xFF이고 아직 데이터가 남아있으면 → 원본에 0x00이 있었음 */
        if (code < 0xFFU && enc_idx < enc_len) {
            if (dec_idx >= dec_max) {
                return 0;
            }
            decoded[dec_idx++] = 0x00U;
        }
    }

    return dec_idx;
}

/* ===== Frame-Level Encode ===== */

int32_t AGR_COBS_FrameEncode(const uint8_t* data, uint32_t data_len,
                              uint8_t* out_buf, uint32_t out_buf_size)
{
    if (data == NULL || out_buf == NULL) {
        return -1;
    }

    /* 1. 원본 데이터 + CRC-16(LE)을 임시 버퍼에 조립 */
    uint8_t raw[AGR_COBS_MAX_FRAME_SIZE + 2];
    uint32_t raw_len = data_len + 2;

    if (raw_len > sizeof(raw)) {
        return -2;
    }

    for (uint32_t i = 0; i < data_len; i++) {
        raw[i] = data[i];
    }

    uint16_t crc = AGR_COBS_CRC16(data, data_len);
    raw[data_len]     = (uint8_t)(crc & 0xFF);       /* CRC Lo */
    raw[data_len + 1] = (uint8_t)(crc >> 8);          /* CRC Hi */

    /* 2. COBS 인코딩 (delimiter 공간 확보) */
    if (out_buf_size < 2) {
        return -2;
    }

    uint32_t cobs_len = AGR_COBS_Encode(raw, raw_len, out_buf, out_buf_size - 1);
    if (cobs_len == 0) {
        return -2;
    }

    /* 3. 0x00 delimiter 추가 */
    out_buf[cobs_len] = AGR_COBS_DELIMITER;

    return (int32_t)(cobs_len + 1);
}

/* ===== COBS Streaming Decoder ===== */

void AGR_COBS_DecoderInit(AGR_COBS_Decoder_t* dec)
{
    if (dec == NULL) return;

    dec->pos = 0;
    dec->frame_count = 0;
    dec->error_count = 0;
}

void AGR_COBS_DecoderReset(AGR_COBS_Decoder_t* dec)
{
    if (dec == NULL) return;

    dec->pos = 0;
}

void AGR_COBS_DecoderFeed(AGR_COBS_Decoder_t* dec,
                           const uint8_t* chunk, uint32_t chunk_len,
                           void (*on_frame)(const uint8_t* data, uint32_t len, void* ctx),
                           void* user_ctx)
{
    if (dec == NULL || chunk == NULL) return;

    for (uint32_t i = 0; i < chunk_len; i++) {
        uint8_t byte = chunk[i];

        if (byte == AGR_COBS_DELIMITER) {
            /* 0x00 수신 → 프레임 경계 */
            if (dec->pos > 0) {
                /* COBS 디코딩 */
                uint8_t decoded[AGR_COBS_MAX_FRAME_SIZE + 2];
                uint32_t dec_len = AGR_COBS_Decode(dec->buf, dec->pos,
                                                    decoded, sizeof(decoded));

                if (dec_len >= 3) {
                    /* 최소 1B 데이터 + 2B CRC */
                    uint32_t payload_len = dec_len - 2;
                    uint16_t recv_crc = (uint16_t)decoded[dec_len - 2]
                                      | ((uint16_t)decoded[dec_len - 1] << 8);
                    uint16_t calc_crc = AGR_COBS_CRC16(decoded, payload_len);

                    if (recv_crc == calc_crc) {
                        dec->frame_count++;
                        if (on_frame != NULL) {
                            on_frame(decoded, payload_len, user_ctx);
                        }
                    } else {
                        dec->error_count++;
                    }
                } else if (dec_len > 0) {
                    /* 너무 짧은 프레임 */
                    dec->error_count++;
                }
                /* dec_len == 0: 디코딩 실패 (잘못된 COBS), 이미 에러 */
            }
            /* pos == 0: 연속 delimiter 무시 (inter-frame) */
            dec->pos = 0;
        } else {
            /* COBS 인코딩된 바이트 누적 */
            if (dec->pos < sizeof(dec->buf)) {
                dec->buf[dec->pos++] = byte;
            } else {
                /* 오버플로우 — 프레임 폐기, 다음 delimiter 대기 */
                dec->error_count++;
                dec->pos = 0;
            }
        }
    }
}
