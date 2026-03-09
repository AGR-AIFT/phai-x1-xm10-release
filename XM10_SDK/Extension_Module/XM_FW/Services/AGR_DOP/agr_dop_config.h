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
 * 프로젝트별로 이 파일을 수정하여 설정을 변경할 수 있습니다.
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_CONFIG_H
#define AGR_DOP_CONFIG_H

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

/** @brief OD Entry 최대 개수 (드라이버별, IMU Hub ~111개 수용) */
#ifndef AGR_OD_MAX_ENTRIES
#define AGR_OD_MAX_ENTRIES          128
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

/** @brief Heartbeat 기본 CAN-ID (+ Node ID) */
#define AGR_CAN_ID_HEARTBEAT        0x700

/**
 *-----------------------------------------------------------
 * SPECIAL OBJECT INDEX (CANopen 표준)
 *-----------------------------------------------------------
 */

/** @brief PDO Mapping Parameter: RPDO1 Mapping */
#define AGR_OD_IDX_RPDO1_MAPPING    0x1600

/** @brief PDO Mapping Parameter: RPDO2 Mapping */
#define AGR_OD_IDX_RPDO2_MAPPING    0x1601

/** @brief PDO Mapping Parameter: TPDO1 Mapping */
#define AGR_OD_IDX_TPDO1_MAPPING    0x1A00

/** @brief PDO Mapping Parameter: TPDO2 Mapping */
#define AGR_OD_IDX_TPDO2_MAPPING    0x1A01

/**
 *-----------------------------------------------------------
 * TRANSPORT SELECTION (프로젝트별 설정 — 유일한 제어점)
 *-----------------------------------------------------------
 * 각 모듈에서 사용할 Transport를 여기서만 설정합니다.
 * CMakeLists.txt는 모든 Transport 소스를 무조건 포함하되,
 * 비활성 Transport의 .c 파일은 빈 번역 단위로 컴파일됩니다.
 *
 * [모듈별 권장 설정]
 * XM (Extension Module):  CANFD=1, COE=0  (CAN-FD Master)
 * IMU Hub:                CANFD=1, COE=0  (CAN-FD Slave)
 * CM (Central Module):    CANFD=1, COE=1  (EtherCAT Slave + CAN-FD Bridge)
 * MD (Motor Driver):      CANFD=1, COE=1  (EtherCAT Slave + CAN-FD)
 * Jetson AM:              CANFD=0, COE=1  (EtherCAT Master)
 */

/** @brief CAN-FD Transport 활성화 (기본 ON) */
#ifndef AGR_DOP_TRANSPORT_CANFD
#define AGR_DOP_TRANSPORT_CANFD     1
#endif

/** @brief CoE (CAN over EtherCAT) Transport 활성화 (기본 OFF) */
#ifndef AGR_DOP_TRANSPORT_COE
#define AGR_DOP_TRANSPORT_COE       0
#endif

/**
 *-----------------------------------------------------------
 * CoE CONFIGURATION
 *-----------------------------------------------------------
 */
#if AGR_DOP_TRANSPORT_COE

/** @brief Process Image 최대 크기 (SM2/SM3 각각, bytes) */
#ifndef AGR_COE_MAX_PI_SIZE
#define AGR_COE_MAX_PI_SIZE         128
#endif

/** @brief CoE SDO 최대 데이터 크기 (bytes) */
#ifndef AGR_COE_SDO_MAX_DATA_SIZE
#define AGR_COE_SDO_MAX_DATA_SIZE   AGR_SDO_MAX_DATA_SIZE
#endif

#endif /* AGR_DOP_TRANSPORT_COE */

#endif /* AGR_DOP_CONFIG_H */
