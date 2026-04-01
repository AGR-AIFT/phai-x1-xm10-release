/**
 ******************************************************************************
 * @file    phai_packet_builder.h
 * @author  HyundoKim
 * @brief   PhAI 프로토콜 패킷 빌더 — User payload를 PhAI 바이너리 패킷으로 래핑
 * @details
 * PhAI Studio(Web) 및 Python PoC 도구와 호환되는 바이너리 패킷을 생성합니다.
 * User는 payload 구조체만 정의하면, SOF / LEN / SEQ_ID / MODULE_ID / CRC16은
 * 이 모듈이 투명하게 처리합니다.
 *
 * 패킷 구조 (Little-Endian) — V2.2:
 *   내부 패킷:
 *   [SOF:0xAA] [LEN:1] [SEQ_ID:2 LE] [MODULE_ID:1] [STATUS:1] [PAYLOAD: LEN×4] [CRC16:2 LE]
 *
 *   와이어 포맷:
 *   [COBS_ENCODE(내부 패킷)] [0x00 delimiter]
 *
 * STATUS 바이트:
 *   - Bits 0-6 (0x7F): Tx 드롭 카운트 (직전 성공 이후 드롭 수, 0~127 포화)
 *   - Bit  7   (0x80): Reserved (향후 확장용)
 *
 * CRC16-CCITT:
 *   - Polynomial: 0x1021, Initial: 0xFFFF
 *   - 범위: SOF 포함, CRC 자체 제외 (bytes[0] ~ bytes[total-3])
 *
 * @version 2.2
 * @date    Feb 24, 2026
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_USB_PHAI_PACKET_BUILDER_H_
#define SYSTEM_COMM_USB_PHAI_PACKET_BUILDER_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define PHAI_SOF_BYTE           0xAA

#define PHAI_HEADER_SIZE        6       /* SOF(1) + LEN(1) + SEQ_ID(2) + MODULE_ID(1) + STATUS(1) */
#define PHAI_CRC_SIZE           2       /* CRC16-CCITT (2 bytes LE) */
#define PHAI_MAX_PAYLOAD_UNITS  255     /* LEN field is uint8 */
#define PHAI_MAX_PAYLOAD_BYTES  (PHAI_MAX_PAYLOAD_UNITS * 4)   /* 1020 bytes */
#define PHAI_MAX_PACKET_SIZE    (PHAI_HEADER_SIZE + PHAI_MAX_PAYLOAD_BYTES + PHAI_CRC_SIZE) /* 1028 */

/* COBS worst-case: data + ceil(data/254) + 1 (code byte) + 1 (0x00 delimiter) */
#define PHAI_MAX_COBS_SIZE      (PHAI_MAX_PACKET_SIZE + (PHAI_MAX_PACKET_SIZE / 254) + 2)

/* Module ID 정의 (PhAI Studio types.ts MODULE_IDS와 일치) */
#define PHAI_MODULE_IMU_ACCEL   0x01
#define PHAI_MODULE_IMU_GYRO    0x02
#define PHAI_MODULE_IMU_QUAT    0x03
#define PHAI_MODULE_GRF_LEFT    0x04
#define PHAI_MODULE_GRF_RIGHT   0x05
#define PHAI_MODULE_MOTOR_LEFT  0x06
#define PHAI_MODULE_MOTOR_RIGHT 0x07
#define PHAI_MODULE_COMBINED    0x10
#define PHAI_MODULE_TOTAL_DATA  0x20    /* System-managed Total Data (자동 전송) */
#define PHAI_MODULE_USER_META   0xEF    /* User Custom Metadata (JSON, 연결 시 1회) */
#define PHAI_MODULE_USER_BASE   0xF0    /* 0xF0~0xFE: User Custom */
#define PHAI_MODULE_USER_CUSTOM_1  (PHAI_MODULE_USER_BASE + 1)  /* 0xF1 */
#define PHAI_MODULE_USER_CUSTOM_2  (PHAI_MODULE_USER_BASE + 2)  /* 0xF2 */
#define PHAI_MODULE_USER_CUSTOM_3  (PHAI_MODULE_USER_BASE + 3)  /* 0xF3 */
#define PHAI_MODULE_DEBUG       0xFF

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief 패킷 빌더를 초기화합니다. (SEQ_ID 카운터 리셋)
 * @note  CdcStream_Init() 이후에 호출해야 합니다.
 */
void PhAI_PacketBuilder_Init(void);

/**
 * @brief User payload를 PhAI V2.2 프로토콜 패킷으로 래핑하여 CDC Tx 링버퍼에 전송합니다.
 *
 * 내부 동작:
 *   1. 헤더 + payload + CRC16 조립 (내부 raw 패킷)
 *   2. COBS 인코딩
 *   3. 0x00 delimiter 추가
 *   4. CDC Tx 링버퍼에 적재
 *
 * @param[in] payload      User 정의 데이터 (4-byte 정렬 권장)
 * @param[in] payload_len  payload 바이트 수 (4의 배수 권장, 아니면 내부에서 패딩)
 * @param[in] module_id    Module ID (PHAI_MODULE_COMBINED 등)
 * @return true: Tx 링버퍼 적재 성공, false: 버퍼 풀 또는 payload 초과
 */
bool PhAI_PacketBuild(const void* payload, uint32_t payload_len, uint8_t module_id);

/**
 * @brief CRC16-CCITT를 계산합니다. (polynomial 0x1021, init 0xFFFF)
 * @param[in] data  계산 대상 데이터
 * @param[in] len   데이터 길이
 * @return CRC16 값
 */
uint16_t PhAI_CRC16(const uint8_t* data, uint32_t len);

#endif /* SYSTEM_COMM_USB_PHAI_PACKET_BUILDER_H_ */
