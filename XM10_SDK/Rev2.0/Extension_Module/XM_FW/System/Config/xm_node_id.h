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
#define AGR_NODE_ID_IMU_HUB_A        0x0D  /**< IMU Hub (단일 노드 — TPDO1 0x18D + TPDO2 0x28D + SDO/NMT) */
#define AGR_NODE_ID_IMU_HUB          AGR_NODE_ID_IMU_HUB_A  /**< 기본 alias */
#define AGR_NODE_ID_EMG_HUB          0x0F  /**< EMG Hub */
#define AGR_NODE_ID_GRF_MODULE       0x11  /**< SM-GRF reserved node ID. XM UART path uses fixed-frame, port=identity. */
#define AGR_NODE_ID_RESERVED_SENSOR  0x10  /**< 예약 — 아직 쓰는 모듈 없음 */

/* Utility */
#define AGR_NODE_ID_IS_VALID(id)     ((id) >= 0x01 && (id) <= 0x7F)
#define AGR_NODE_ID_IS_BROADCAST(id) ((id) == AGR_NODE_ID_BROADCAST)

/* 센서 허브 번호 — 이 번호의 메시지는 XM10 의 허브 경로가 쓴다(FD 모드 자유 송수신에서 막는다) */
#define AGR_NODE_ID_IS_HUB(n)        ((n) == AGR_NODE_ID_FES_HUB || (n) == AGR_NODE_ID_IMU_HUB || (n) == AGR_NODE_ID_EMG_HUB)

/* 외부 장치에 줄 수 있는 번호 — 1~0x7F 중 위 시스템 · 허브 · 예약 번호를 뺀 나머지(번호는 사람이 코드에 정해 넣는다) */
#define AGR_NODE_ID_IS_DEVICE_OK(n)  (AGR_NODE_ID_IS_VALID(n) && (n) != AGR_NODE_ID_CM && (n) != AGR_NODE_ID_XM && \
                                      !AGR_NODE_ID_IS_HUB(n) && (n) != AGR_NODE_ID_RESERVED_SENSOR &&             \
                                      (n) != AGR_NODE_ID_GRF_MODULE)

#endif /* XM_NODE_ID_H */
