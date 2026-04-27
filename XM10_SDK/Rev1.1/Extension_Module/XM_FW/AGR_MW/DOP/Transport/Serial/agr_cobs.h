/**
 ******************************************************************************
 * @file    agr_cobs.h
 * @author  HyundoKim
 * @brief   COBS Framing Layer + CRC-16/CCITT
 * @version 1.0
 * @date    2026-03-25
 *
 * @details
 * Why:  SLIP(RFC 1055)은 가변 오버헤드(ESC 바이트)로 DMA 전송 시 길이 예측 불가.
 *       COBS는 고정 오버헤드(254바이트당 1바이트)로 DMA/BLE MTU 친화적.
 * What: 0x00 delimiter + COBS 바이트 스터핑으로 임의 바이너리 데이터를 프레이밍.
 *       CRC-16/CCITT(0xFFFF)로 전송 무결성 보장.
 *
 * [COBS Encoding — Cheshire & Baker 1999]
 *   Delimiter = 0x00
 *   인코딩된 데이터에 0x00이 절대 나타나지 않음 → 0x00이 프레임 경계
 *   최대 오버헤드: ceil(N/254) + 1 바이트
 *   프레임: [COBS-encoded(Data + CRC16-LE)] [0x00]
 *
 * [CRC-16/CCITT]
 *   Polynomial: 0x1021
 *   Init:       0xFFFF
 *   XorOut:     0x0000
 *   데이터 뒤에 CRC 2바이트(LE) 추가 후 COBS 인코딩.
 *
 * [SLIP 대비 장점]
 *   - 고정 오버헤드: 정확한 전송 길이 사전 계산 가능
 *   - DMA 친화: 전송 크기 확정 → scatter-gather 불필요
 *   - BLE MTU 호환: 고정 패킷 분할 용이
 *   - phai-studio와 동일 프레이밍 (COBS 통일)
 *
 * @note    XM Extension Module의 cobs_encode(phai_packet_builder.c) /
 *          _COBSDecode(boot_ftp_trigger.c) 로직과 byte-for-byte 동일.
 *          공통 모듈로 추출하여 단일 소스 유지.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_COBS_H
#define AGR_COBS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ===== Configuration ===== */

/** @brief 최대 디코딩 프레임 크기 (COBS 디코딩 후 원본 데이터) */
#ifndef AGR_COBS_MAX_FRAME_SIZE
#define AGR_COBS_MAX_FRAME_SIZE     128
#endif

/** @brief COBS 프레임 구분자 */
#define AGR_COBS_DELIMITER          ((uint8_t)0x00)

/* ===== COBS Raw Encode/Decode ===== */

/**
 * @brief   COBS 인코딩 (raw).
 *          0x00 바이트를 제거하여 프레이밍에 사용 가능하게 합니다.
 *
 * @param   src      원본 데이터
 * @param   src_len  원본 길이
 * @param   dst      인코딩 출력 버퍼
 * @param   dst_max  출력 버퍼 최대 크기
 * @return  인코딩된 바이트 수 (0x00 delimiter 미포함), 0이면 실패
 *
 * @note    XM phai_packet_builder.c cobs_encode()와 byte-for-byte 동일.
 *          최악 출력 크기: src_len + ceil(src_len/254) + 1
 */
uint32_t AGR_COBS_Encode(const uint8_t* src, uint32_t src_len,
                          uint8_t* dst, uint32_t dst_max);

/**
 * @brief   COBS 디코딩 (raw).
 *          인코딩된 데이터에서 원본 바이너리를 복원합니다.
 *
 * @param   encoded   COBS 인코딩된 데이터 (0x00 delimiter 미포함)
 * @param   enc_len   인코딩된 데이터 길이
 * @param   decoded   디코딩 출력 버퍼
 * @param   dec_max   출력 버퍼 최대 크기
 * @return  디코딩된 바이트 수, 0이면 실패 (잘못된 인코딩 또는 버퍼 부족)
 *
 * @note    XM boot_ftp_trigger.c _COBSDecode()와 byte-for-byte 동일.
 */
uint32_t AGR_COBS_Decode(const uint8_t* encoded, uint32_t enc_len,
                          uint8_t* decoded, uint32_t dec_max);

/* ===== CRC-16/CCITT ===== */

/**
 * @brief   CRC-16/CCITT 계산.
 * @param   data    입력 데이터
 * @param   len     데이터 길이
 * @return  CRC-16 값
 *
 * @note    XM phai_packet_builder.c PhAI_CRC16()과 동일한 알고리즘/LUT.
 */
uint16_t AGR_COBS_CRC16(const uint8_t* data, uint32_t len);

/* ===== Frame-Level Encode (CRC + COBS + Delimiter) ===== */

/**
 * @brief   데이터를 COBS 프레임으로 인코딩.
 *          [COBS-encoded(data + CRC16-LE)] [0x00]
 *
 * @param   data        원본 데이터 (헤더 + 페이로드)
 * @param   data_len    원본 데이터 길이
 * @param   out_buf     COBS 프레임 출력 버퍼
 * @param   out_buf_size 출력 버퍼 크기
 * @return  인코딩된 바이트 수 (0x00 delimiter 포함), <0: 에러
 *          (-1: NULL, -2: 버퍼 부족)
 *
 * @note    최악 출력 크기: (data_len+2) + ceil((data_len+2)/254) + 1 + 1(delimiter)
 */
int32_t AGR_COBS_FrameEncode(const uint8_t* data, uint32_t data_len,
                              uint8_t* out_buf, uint32_t out_buf_size);

/* ===== COBS Streaming Decoder ===== */

/**
 * @brief   COBS 디코더 컨텍스트 (상태 유지형).
 *          바이트 스트림에서 0x00 구분자로 프레임을 분리한 뒤 COBS 디코딩.
 */
typedef struct {
    uint8_t           buf[AGR_COBS_MAX_FRAME_SIZE * 2];  /**< COBS 인코딩 수신 버퍼 */
    uint32_t          pos;                                /**< 현재 버퍼 위치 */
    uint32_t          frame_count;                        /**< 완료된 프레임 카운트 (디버그) */
    uint32_t          error_count;                        /**< 오류 카운트 (디코딩 실패, CRC 등) */
} AGR_COBS_Decoder_t;

/**
 * @brief   디코더 초기화.
 * @param   dec     디코더 컨텍스트
 */
void AGR_COBS_DecoderInit(AGR_COBS_Decoder_t* dec);

/**
 * @brief   디코더 리셋 (현재 프레임 폐기).
 * @param   dec     디코더 컨텍스트
 */
void AGR_COBS_DecoderReset(AGR_COBS_Decoder_t* dec);

/**
 * @brief   수신 데이터 청크를 디코더에 투입.
 *          0x00 구분자로 프레임 분리 → COBS 디코딩 → CRC 검증 → 콜백 호출.
 *
 * @param   dec         디코더 컨텍스트
 * @param   chunk       수신 데이터 (DMA bounce buffer)
 * @param   chunk_len   수신 데이터 길이
 * @param   on_frame    프레임 완료 콜백 (CRC 검증 완료된 데이터)
 * @param   user_ctx    콜백에 전달할 사용자 컨텍스트
 *
 * @note    on_frame 콜백 파라미터:
 *          - data: CRC 제거된 원본 데이터 (스택 버퍼, 즉시 복사 필요)
 *          - len:  원본 데이터 길이 (CRC 2바이트 제외)
 *          - ctx:  user_ctx 전달
 *
 * @note    ISR/콜백 컨텍스트에서 호출 가능 (힙 미사용, 블로킹 없음).
 */
void AGR_COBS_DecoderFeed(AGR_COBS_Decoder_t* dec,
                           const uint8_t* chunk, uint32_t chunk_len,
                           void (*on_frame)(const uint8_t* data, uint32_t len, void* ctx),
                           void* user_ctx);

#endif /* AGR_COBS_H */
