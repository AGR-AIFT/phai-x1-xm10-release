/**
 ******************************************************************************
 * @file    agr_dop_node_id.h
 * @author  HyundoKim
 * @brief   AGR-DOP Node ID Definitions
 * @version 2.0
 * @date    Dec 2, 2025
 *
 * @details
 * 모든 AGR 모듈의 CAN Node ID를 정의합니다.
 * 
 * [Node ID 범위]
 * - Standard ID (11-bit CAN ID): Node ID 1~127 사용 가능
 * - Extended ID (29-bit CAN ID): Node ID 1~127 이상 확장 가능 (필요 시)
 * 
 * [Node ID 할당 정책]
 * - 0x00      : Broadcast (모든 노드)
 * - 0x01~0x0F : Core Modules (CM, XM 등)
 * - 0x10~0x1F : Extension Modules (IMU Hub 등)
 * - 0x20~0x3F : Actuator Modules (Motor Drivers)
 * - 0x40~0x5F : Sensor Modules
 * - 0x60~0x7F : Reserved
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_NODE_ID_H
#define AGR_DOP_NODE_ID_H

#include <stdint.h>

/**
 *-----------------------------------------------------------
 * NODE ID TYPE
 *-----------------------------------------------------------
 */

typedef uint8_t AGR_NodeID_t;

/**
 *-----------------------------------------------------------
 * BROADCAST / SPECIAL NODE IDs
 *-----------------------------------------------------------
 */

/** @brief Broadcast to all nodes */
#define AGR_NODE_ID_BROADCAST       0x00

/**
 *-----------------------------------------------------------
 * CORE MODULES (0x01 ~ 0x0F)
 *-----------------------------------------------------------
 */

/** @brief Control Module (Main Controller) */
#define AGR_NODE_ID_CM              0x01

/** @brief Extension Module (XM10) */
#define AGR_NODE_ID_XM              0x02

/**
 *-----------------------------------------------------------
 * ACTUATOR MODULES (0x06 ~ 0x0F) - Legacy Compatible
 *-----------------------------------------------------------
 * @note 기존 시스템과 호환을 위해 0x06~0x07 유지
 */

/** @brief Right Hip Motor Driver */
#define AGR_NODE_ID_MD_RH           0x06

/** @brief Left Hip Motor Driver */
#define AGR_NODE_ID_MD_LH           0x07

/**
 *-----------------------------------------------------------
 * EXTENSION SENSOR MODULES (0x08 ~ 0x1F)
 *-----------------------------------------------------------
 */

/** @brief IMU Hub Module - Group A (PDO 1) */
#define AGR_NODE_ID_IMU_HUB_A       0x08

/** @brief IMU Hub Module - Group B (PDO 2) */
#define AGR_NODE_ID_IMU_HUB_B       0x09

/** @brief IMU Hub Module - SDO/NMT용 (설정/상태 관리) */
#define AGR_NODE_ID_IMU_HUB         0x08  /* Group A와 동일, SDO는 단일 ID 사용 */

/**
 *-----------------------------------------------------------
 * UTILITY MACROS
 *-----------------------------------------------------------
 */

/** @brief Node ID가 유효한지 확인 (Standard ID 범위) */
#define AGR_NODE_ID_IS_VALID(id)    ((id) >= 0x01 && (id) <= 0x7F)

/** @brief Node ID가 Broadcast인지 확인 */
#define AGR_NODE_ID_IS_BROADCAST(id) ((id) == AGR_NODE_ID_BROADCAST)

#endif /* AGR_DOP_NODE_ID_H */

