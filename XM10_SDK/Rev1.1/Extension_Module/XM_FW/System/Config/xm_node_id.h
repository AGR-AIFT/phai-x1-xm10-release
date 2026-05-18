/**
 ******************************************************************************
 * @file    xm_node_id.h
 * @author  HyundoKim
 * @brief   XM (Extension Module) CAN-FD Node ID Definitions — SUIT H10 System
 * @version 1.0
 * @date    Apr 16, 2026
 *
 * @details
 * XM이 속한 SUIT H10 시스템의 CAN-FD Node ID 할당.
 * CANopen CiA 301 호환 (7-bit, 0x01~0x7F).
 *
 * [역할] XM은 Master (CM 하위 / Sensor Hub 상위)
 * [구성] CM(0x01) + XM(0x02) + IMU/EMG/FES Hubs
 *
 * @note Node ID는 CAN-FD 전용이며 로봇 시스템별로 다르다.
 *       AGR_MW (middleware) 가 아닌 System Layer 에서 관리한다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd.
 ******************************************************************************
 */

#pragma once

#ifndef XM_NODE_ID_H
#define XM_NODE_ID_H

#include <stdint.h>

typedef uint8_t AGR_NodeID_t;

/* Broadcast / Special */
#define AGR_NODE_ID_BROADCAST        0x00

/* Core Modules */
#define AGR_NODE_ID_CM               0x01  /**< Control Module */
#define AGR_NODE_ID_XM               0x02  /**< Extension Module (자기 자신) */

/* Sensor Hub Modules */
#define AGR_NODE_ID_FES_HUB          0x0C  /**< FES Hub (2-ch Biphasic Stimulator) */
#define AGR_NODE_ID_IMU_HUB_A        0x0D  /**< IMU Hub — Group A (TPDO1, SDO/NMT) */
#define AGR_NODE_ID_IMU_HUB_B        0x0E  /**< IMU Hub — Group B (TPDO2) */
#define AGR_NODE_ID_IMU_HUB          AGR_NODE_ID_IMU_HUB_A  /**< SDO/NMT용 alias */
#define AGR_NODE_ID_EMG_HUB          0x0F  /**< EMG Hub */

/* Utility */
#define AGR_NODE_ID_IS_VALID(id)     ((id) >= 0x01 && (id) <= 0x7F)
#define AGR_NODE_ID_IS_BROADCAST(id) ((id) == AGR_NODE_ID_BROADCAST)

#endif /* XM_NODE_ID_H */
