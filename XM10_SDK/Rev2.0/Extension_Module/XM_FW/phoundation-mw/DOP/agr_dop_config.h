/**
 ******************************************************************************
 * @file    agr_dop_config.h
 * @author  HyundoKim
 * @brief   AGR-DOP V2 Configuration
 * @version 2.0
 * @date    Dec 2, 2025
 *
 * @details
 * AGR-DOP 설정 값들을 정의합니다.
 *
 * [프로젝트별 설정]
 * 각 프로젝트는 agr_mw_conf.h를 작성하여 Transport 활성화 및
 * 파라미터 크기 등을 #define합니다. (ioif_conf.h와 동일한 패턴)
 * Transport 매크로가 미정의 시 해당 Transport는 비활성(0)됩니다.
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_CONFIG_H
#define AGR_DOP_CONFIG_H

/* 프로젝트별 설정 Override (agr_mw_conf.h → #ifndef 가드보다 먼저 적용) */
#if __has_include("agr_mw_conf.h")
#include "agr_mw_conf.h"
#endif

/**
 *-----------------------------------------------------------
 * CANFD CONFIGURATION
 *-----------------------------------------------------------
 */

/** @brief CANFD 최대 페이로드 크기 (bytes) */
#ifndef AGR_CANFD_MAX_PAYLOAD
#define AGR_CANFD_MAX_PAYLOAD       64
#endif

/** @brief SDO 최대 데이터 크기 (CANFD 페이로드 - 헤더 4바이트) */
#ifndef AGR_SDO_MAX_DATA_SIZE
#define AGR_SDO_MAX_DATA_SIZE       60
#endif

/**
 *-----------------------------------------------------------
 * OBJECT DICTIONARY CONFIGURATION
 *-----------------------------------------------------------
 */

/** @brief OD Entry 개수 참고 상한 (사이징 가이드 — 강제 아님)
 *  @note  현재 코드 어디에서도 버퍼 크기로 사용하지 않는다. 실제 entry_count/sort_buf
 *         는 각 모듈이 sizeof 기반으로 정의 (실측: EMG ~34, IMU 134, SAM3x 424+).
 *         고정 버퍼를 이 값으로 잡는 코드를 추가할 경우 RAM 영향 재검토 필수. */
#ifndef AGR_OD_MAX_ENTRIES
#define AGR_OD_MAX_ENTRIES          512
#endif

/**
 *-----------------------------------------------------------
 * PDO CONFIGURATION
 *-----------------------------------------------------------
 */

/** @brief PDO Mapping Table 최대 Entry 개수 */
#ifndef AGR_PDO_MAP_MAX_ENTRIES
#define AGR_PDO_MAP_MAX_ENTRIES     32
#endif

/** @brief 최대 TX PDO 개수 */
#ifndef AGR_TX_PDO_MAX_COUNT
#define AGR_TX_PDO_MAX_COUNT        4
#endif

/** @brief 최대 RX PDO 개수 */
#ifndef AGR_RX_PDO_MAX_COUNT
#define AGR_RX_PDO_MAX_COUNT        4
#endif

/**
 *-----------------------------------------------------------
 * CAN-ID CONFIGURATION (CANopen 호환)
 *-----------------------------------------------------------
 * 
 * [Function Code 정의]
 * - 0x000: NMT
 * - 0x080: SYNC / EMCY
 * - 0x100: Command Vector Request (Master → Node, 결정적 제어입력)
 * - 0x180: TPDO1 (Node → Master)
 * - 0x200: RPDO1 (Master → Node)
 * - 0x280: TPDO2
 * - 0x300: RPDO2
 * - 0x380: TPDO3
 * - 0x400: RPDO3
 * - 0x480: TPDO4
 * - 0x500: RPDO4
 * - 0x580: SDO Response (Node → Master)
 * - 0x600: SDO Request (Master → Node)
 * - 0x680: Command Vector ACK/EVENT (Node → Master)
 * - 0x700: Heartbeat
 * 
 * CAN-ID = Function_Code + Node_ID
 */

/**
 * @brief CANopen Function Code (11-bit CAN-ID의 상위 4비트)
 * @details CAN-ID 구조: [Func Code 4-bit][Node ID 7-bit]
 *          추출 방법: func_code = (can_id & 0x780) >> 7
 */
typedef enum {
    AGR_CAN_FUNC_NMT         = 0x00,   /**< Network Management (0x000) */
    AGR_CAN_FUNC_SYNC_EMCY   = 0x01,   /**< SYNC (0x080) or EMCY (0x080+Node) */
    AGR_CAN_FUNC_VECTOR_REQ  = 0x02,   /**< Command Vector Request (0x100+Node) */
    AGR_CAN_FUNC_TPDO1       = 0x03,   /**< Transmit PDO 1 (0x180+Node) */
    AGR_CAN_FUNC_RPDO1       = 0x04,   /**< Receive PDO 1 (0x200+Node) */
    AGR_CAN_FUNC_TPDO2       = 0x05,   /**< Transmit PDO 2 (0x280+Node) */
    AGR_CAN_FUNC_RPDO2       = 0x06,   /**< Receive PDO 2 (0x300+Node) */
    AGR_CAN_FUNC_TPDO3       = 0x07,   /**< Transmit PDO 3 (0x380+Node) */
    AGR_CAN_FUNC_RPDO3       = 0x08,   /**< Receive PDO 3 (0x400+Node) */
    AGR_CAN_FUNC_TPDO4       = 0x09,   /**< Transmit PDO 4 (0x480+Node) */
    AGR_CAN_FUNC_RPDO4       = 0x0A,   /**< Receive PDO 4 (0x500+Node) */
    AGR_CAN_FUNC_SDO_TX      = 0x0B,   /**< SDO Transmit (0x580+Node) */
    AGR_CAN_FUNC_SDO_RX      = 0x0C,   /**< SDO Receive (0x600+Node) */
    AGR_CAN_FUNC_VECTOR_ACK  = 0x0D,   /**< Command Vector ACK/EVENT (0x680+Node) */
    AGR_CAN_FUNC_HEARTBEAT   = 0x0E,   /**< Heartbeat (0x700+Node) */
} AGR_CAN_FuncCode_t;

/** @brief NMT 기본 CAN-ID */
#define AGR_CAN_ID_NMT              0x000

/** @brief SYNC 기본 CAN-ID */
#define AGR_CAN_ID_SYNC             0x080

/** @brief Emergency 기본 CAN-ID */
#define AGR_CAN_ID_EMCY             0x080

/** @brief TPDO1 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_TPDO1            0x180

/** @brief RPDO1 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_RPDO1            0x200

/** @brief TPDO2 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_TPDO2            0x280

/** @brief RPDO2 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_RPDO2            0x300

/** @brief TPDO3 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_TPDO3            0x380

/** @brief RPDO3 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_RPDO3            0x400

/** @brief TPDO4 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_TPDO4            0x480

/** @brief RPDO4 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_RPDO4            0x500

/** @brief SDO Response 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_SDO_TX           0x580

/** @brief SDO Request 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_SDO_RX           0x600

/** @brief Command Vector Request 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_VECTOR_REQ       0x100

/** @brief Command Vector ACK/EVENT 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_VECTOR_ACK       0x680

/** @brief Heartbeat 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_HEARTBEAT        0x700

/**
 *-----------------------------------------------------------
 * COMMAND VECTOR CONFIGURATION
 *-----------------------------------------------------------
 * "제어 입력" 클래스 메시지 (P/F/I trajectory, ES vector 등) 전용 채널.
 * SDO(main-loop deferred)와 달리 단일 atomic 프레임을 소비 모듈이 정한
 * 제어 tick 에서 결정적으로 처리한다. 설계 SSOT:
 * SAM3x_FW `MD_FW/Devices/AGR/Control_Module/Doc/md_command_vector_design.md`.
 */

/** @brief Command Vector 채널 컴파일 스위치 (기본 OFF — 모터/제어 모듈만 opt-in).
 *  @details 0 = CAN-FD RX 핸들러(VECTOR_REQ/ACK) pass-through + TX 함수(SendVector*)
 *           미컴파일 + agr_vector.c inert → 미사용 모듈은 소스를 빌드에서 제거 가능.
 *           1 = 소비 모듈(SAM3x slave / CM master 등)이 agr_mw_conf.h 에서 override +
 *           agr_vector.c 를 빌드에 편입. **주의**: `AGR_Vector_Ctrl_t` 는 스위치와
 *           무관하게 AGR_DOP_Ctx_t 에 by-value 임베드 유지(모듈 간 ctx 레이아웃 동일).
 *           스위치가 없거나 0인데 소비 코드가 링크되면 agr_vector.c self-guard 로
 *           심볼 미정의 → 링크 에러(loud). 전 모듈 compat 검토 2026-07-02. */
#ifndef AGR_VECTOR_ENABLED
#define AGR_VECTOR_ENABLED          0
#endif

/** @brief Context 당 등록 가능한 vector type 수 (MD: P/I/F/F-zero = 4) */
#ifndef AGR_VECTOR_MAX_TYPES
#define AGR_VECTOR_MAX_TYPES        4
#endif

/** @brief Vector payload 최대 길이 (frame = [type][seq][len] 3B + payload) */
#define AGR_VECTOR_MAX_PAYLOAD      (AGR_CANFD_MAX_PAYLOAD - 3)

/**
 *-----------------------------------------------------------
 * SPECIAL OBJECT INDEX (CANopen 표준)
 *-----------------------------------------------------------
 */

/** @brief PDO Mapping Parameter: RPDO1 Mapping */
#define AGR_OD_IDX_RPDO1_MAPPING    0x1600

/**
 *-----------------------------------------------------------
 * OD DISCOVERY CONFIGURATION (Serial Transport 전용)
 *-----------------------------------------------------------
 * PC(Python GUI / phai-studio)가 Connect 시 SM의 OD를 자동 스캔.
 * SDO Read 0x2F00:0 → entry_count, 0x2F00:1~N → entry descriptor.
 * CAN-FD Core SDO 무수정, Serial transport에서만 인터셉트.
 */

/** @brief OD Directory Index (Manufacturer Specific, 0x2F00) */
#define AGR_OD_IDX_DIRECTORY        0x2F00

/** @brief OD Discovery 기능 활성화 (1=ON, 0=OFF) */
#ifndef AGR_OD_DISCOVERY_ENABLED
#define AGR_OD_DISCOVERY_ENABLED    1
#endif

/** @brief OD Entry name 최대 길이 (ASCII, NULL 미포함) */
#define AGR_OD_NAME_MAX_LEN         32

/** @brief OD Entry unit 최대 길이 (ASCII, NULL 미포함) */
#define AGR_OD_UNIT_MAX_LEN         8

/** @brief PDO Mapping Parameter: RPDO2 Mapping */
#define AGR_OD_IDX_RPDO2_MAPPING    0x1601

/** @brief PDO Mapping Parameter: TPDO1 Mapping */
#define AGR_OD_IDX_TPDO1_MAPPING    0x1A00

/** @brief PDO Mapping Parameter: TPDO2 Mapping */
#define AGR_OD_IDX_TPDO2_MAPPING    0x1A01

/**
 *-----------------------------------------------------------
 * TRANSPORT SELECTION (프로젝트별 agr_mw_conf.h에서 설정)
 *-----------------------------------------------------------
 * 각 프로젝트는 agr_mw_conf.h에서 사용할 Transport를 정의해야 합니다.
 * 미정의 시 해당 Transport는 비활성(0)으로 처리됩니다.
 * CMakeLists.txt는 모든 Transport 소스를 무조건 포함하되,
 * 비활성 Transport의 .c 파일은 빈 번역 단위로 컴파일됩니다.
 *
 * [agr_mw_conf.h에 정의할 매크로]
 * AGR_DOP_TRANSPORT_CANFD   — CAN-FD (agr_dop_canfd.c)
 * AGR_DOP_TRANSPORT_UDP     — UDP/Ethernet (agr_dop_udp.c, Rev2.0 전용)
 * AGR_DOP_TRANSPORT_SERIAL  — Serial/COBS (agr_dop_serial.c, BLE UART 등)
 * AGR_DOP_TRANSPORT_COE     — [REMOVED 2026-07] DOP-over-EtherCAT 폐기.
 *     EtherCAT wire는 SOES(slave)/SOEM(master)가 전담하며 DOP는 사용되지 않음.
 *     agr_dop_coe_*.c 삭제됨. 근거: WalkON_H_CM_Temp/docs/ethercat-learning/08.
 *     모든 모듈 COE=0.
 *
 * [모듈별 권장 설정]  (COE는 전부 0 — EtherCAT은 SOES/SOEM 전담)
 * XM Rev1.1:              CANFD=1, UDP=0, COE=0, SERIAL=0  (CAN-FD Master)
 * XM Rev2.0:              CANFD=1, UDP=1, COE=0, SERIAL=0  (CAN-FD + Ethernet)
 * IMU Hub:                CANFD=1, UDP=0, COE=0, SERIAL=0  (CAN-FD Slave)
 * CM (Central Module):    CANFD=1, UDP=0, COE=0, SERIAL=0  (EtherCAT=SOES; CAN-FD Bridge)
 * CM-WH:                  CANFD=1, UDP=0, COE=0, SERIAL=1  (EtherCAT=SOES; CAN-FD + Serial)
 * MD (Motor Driver):      CANFD=1, UDP=0, COE=0, SERIAL=0  (EtherCAT=SOES; CAN-FD)
 * Jetson AM:              CANFD=0, UDP=0, COE=0, SERIAL=0  (EtherCAT Master=SOEM)
 */

/* [REMOVED 2026-07] CoE CONFIGURATION 블록 삭제 — DOP-over-EtherCAT 폐기,
 * agr_dop_coe_*.c 및 AGR_COE_* 상수 제거. EtherCAT은 SOES/SOEM 전담. */

/**
 *-----------------------------------------------------------
 * Serial CONFIGURATION
 *-----------------------------------------------------------
 */
#if AGR_DOP_TRANSPORT_SERIAL

/** @brief COBS 디코딩 최대 프레임 크기 (Header + Payload + CRC, bytes) */
#ifndef AGR_COBS_MAX_FRAME_SIZE
#define AGR_COBS_MAX_FRAME_SIZE     128
#endif

#endif /* AGR_DOP_TRANSPORT_SERIAL */

#endif /* AGR_DOP_CONFIG_H */
