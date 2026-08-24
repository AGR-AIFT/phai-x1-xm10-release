/**
 ******************************************************************************
 * @file    agr_pnp_master.h
 * @author  HyundoKim
 * @brief   AGR PnP Master API - "나는 Master, Slave들을 추적한다"
 * @version 1.0.0
 * @date    2026-02-10
 *
 * @details
 * CANopen 표준 기반 PnP Master 추상화 레이어입니다.
 * 하나의 Master 인스턴스가 여러 Slave를 관리합니다.
 *
 * [핵심 역할]
 * - Master Heartbeat 전송 (1초마다, Node 단위 1회)
 * - Slave Boot-up 수신 → PRE_OPERATIONAL 전환
 * - Slave Heartbeat Timeout 감지 → STOPPED 전환
 * - Slave NMT 명령 전송 (START, STOP, PRE_OP)
 * - 100% 재연결 보장 (.cursorrules 5.2 준수)
 *
 * [사용 위치]
 * - XM10 (RTOS): System Layer에서 인스턴스 생성, Device Layer에서 Slave 등록
 * - CM (RTOS): 동일 패턴 (MD를 Slave로 관리)
 *
 * [사용 예시]
 * ```c
 * // system_startup.c (System Layer)
 * static AGR_PnP_Master_t s_master_pnp;
 *
 * void System_Init(void) {
 *     AGR_PnP_Master_Init(&s_master_pnp, AGR_NODE_ID_XM,
 *                          System_Fdcan1_Transmit, HAL_GetTick);
 *     ImuHub_Drv_Init(tx_func, &s_master_pnp);
 * }
 *
 * // imu_hub_drv.c (Device Layer)
 * void ImuHub_Drv_Init(AGR_TxFunc_t tx, AGR_PnP_Master_t* master) {
 *     AGR_PnP_Master_AddSlave(master, &(AGR_PnP_SlaveConfig_t){
 *         .name = "IMU Hub",
 *         .node_id = AGR_NODE_ID_IMU_HUB,
 *         .heartbeat_timeout_ms = 3000,
 *         .on_bootup = _OnBootup,
 *     });
 * }
 * ```
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_PNP_MASTER_H
#define AGR_PNP_MASTER_H

#include <stdint.h>
#include <stdbool.h>
#include "agr_nmt.h"
#include "agr_pnp_types.h"

/**
 *-----------------------------------------------------------
 * CONFIGURATION
 *-----------------------------------------------------------
 */

#ifndef AGR_PNP_MASTER_MAX_SLAVES
#define AGR_PNP_MASTER_MAX_SLAVES       8   /**< 최대 Slave 수 */
#endif

#define AGR_PNP_MASTER_HEARTBEAT_INTERVAL_MS    1000  /**< Master Heartbeat 전송 주기 (ms) */

/**
 * @brief Slave 이벤트 콜백 (Device Layer에서 구현)
 *
 * @details
 * Device Driver가 PnP 이벤트를 받아 Pre-Op State Machine 등을 구동합니다.
 * 모든 콜백은 PnP Task 컨텍스트에서 호출됩니다 (ISR 아님).
 */
typedef struct {
    /** @brief Slave Boot-up 수신 (PRE_OPERATIONAL 진입)
     *  @param slave_node_id  Boot-up을 보낸 Slave의 Node ID
     *  @note  Device Driver는 여기서 Pre-Op State Machine을 시작합니다 */
    void (*on_slave_bootup)(uint8_t slave_node_id);

    /** @brief Slave NMT 상태 변경
     *  @param slave_node_id  Slave Node ID
     *  @param old_state      이전 NMT 상태
     *  @param new_state      새 NMT 상태 */
    void (*on_slave_state_changed)(uint8_t slave_node_id,
                                   AGR_NMT_State_t old_state,
                                   AGR_NMT_State_t new_state);

    /** @brief Slave 연결됨 (OPERATIONAL 진입 후 첫 Heartbeat 확인)
     *  @param slave_node_id  Slave Node ID */
    void (*on_slave_online)(uint8_t slave_node_id);

    /** @brief Slave 연결 끊김 (Heartbeat Timeout → STOPPED)
     *  @param slave_node_id  Slave Node ID */
    void (*on_slave_offline)(uint8_t slave_node_id);
} AGR_PnP_Master_SlaveCallbacks_t;

/**
 * @brief Slave 등록 설정 (AddSlave 시 전달)
 */
typedef struct {
    const char* name;                        /**< Slave 이름 (디버깅용, e.g., "IMU Hub") */
    uint8_t     node_id;                     /**< Slave의 CANopen Node ID (1~127) */
    uint32_t    heartbeat_timeout_ms;        /**< Heartbeat Timeout (ms), 권장: 3000 */
    AGR_PnP_Master_SlaveCallbacks_t callbacks; /**< 이벤트 콜백 */
} AGR_PnP_SlaveConfig_t;

/**
 * @brief Slave 내부 관리 정보 (Master가 추적)
 * @note  사용자가 직접 접근하지 마세요. API를 통해 접근하세요.
 */
typedef struct {
    /* 설정 (등록 시 복사) */
    const char* name;
    uint8_t     node_id;
    uint32_t    heartbeat_timeout_ms;
    AGR_PnP_Master_SlaveCallbacks_t callbacks;

    /* NMT 상태 추적 */
    AGR_NMT_Inst_t nmt;                 /**< Slave NMT 인스턴스 */
    bool           is_registered;       /**< 등록 완료 여부 */
    bool           ever_received_heartbeat; /**< Slave 감지 이력 (.cursorrules 재연결) */
} AGR_PnP_SlaveInfo_t;

/**
 * @brief Master PnP 인스턴스
 *
 * @details
 * System Layer에서 하나만 생성하고, 각 Device Driver가 AddSlave()로 Slave를 등록합니다.
 * Heartbeat는 RunPeriodic()에서 1초마다 1회 전송됩니다 (중복 없음).
 *
 * [메모리 레이아웃]
 * - 정적 할당 (malloc 없음)
 * - MAX_SLAVES 개까지 등록 가능
 */
typedef struct {
    /* 설정 */
    uint8_t              my_node_id;     /**< Master 자신의 Node ID (e.g., AGR_NODE_ID_XM) */
    AGR_PnP_TxFunc_t     tx_func;        /**< CAN 전송 함수 */
    AGR_PnP_GetTickFunc_t get_tick;      /**< Tick 함수 */

    /* Slave 관리 */
    AGR_PnP_SlaveInfo_t  slaves[AGR_PNP_MASTER_MAX_SLAVES]; /**< 등록된 Slave 배열 */
    uint8_t              slave_count;    /**< 등록된 Slave 수 */

    /* Master Heartbeat */
    uint32_t             last_heartbeat_sent_ms; /**< 마지막 Heartbeat 전송 시간 */

    /* 상태 */
    bool                 is_initialized; /**< 초기화 완료 여부 */
} AGR_PnP_Master_t;

/**
 *-----------------------------------------------------------
 * PUBLIC API
 *-----------------------------------------------------------
 */

/**
 * @brief Master PnP 초기화
 *
 * @param master     Master 인스턴스 (System Layer에서 정적 할당)
 * @param my_node_id 자신의 CANopen Node ID (e.g., AGR_NODE_ID_XM)
 * @param tx_func    CAN 전송 함수 (Dependency Injection)
 * @param get_tick   Tick 함수 (HAL_GetTick 또는 IOIF_TIM_GetTick)
 * @return 0: 성공, <0: 에러
 *
 * @note System Layer(system_startup.c)에서 1회 호출합니다.
 */
int AGR_PnP_Master_Init(AGR_PnP_Master_t* master,
                         uint8_t my_node_id,
                         AGR_PnP_TxFunc_t tx_func,
                         AGR_PnP_GetTickFunc_t get_tick);

/**
 * @brief Slave 등록 (Device Driver에서 호출)
 *
 * @param master  Master 인스턴스
 * @param config  Slave 설정 (이름, Node ID, Timeout, 콜백)
 * @return >=0: Slave Index (성공), <0: 에러
 *
 * @note Device Driver의 Init()에서 호출합니다.
 *
 * @example
 * ```c
 * int idx = AGR_PnP_Master_AddSlave(master, &(AGR_PnP_SlaveConfig_t){
 *     .name = "IMU Hub",
 *     .node_id = AGR_NODE_ID_IMU_HUB,
 *     .heartbeat_timeout_ms = 3000,
 *     .callbacks = {
 *         .on_slave_bootup = _OnBootup,
 *         .on_slave_state_changed = _OnNmtChange,
 *         .on_slave_online = _OnConnected,
 *         .on_slave_offline = _OnDisconnected,
 *     }
 * });
 * ```
 */
int AGR_PnP_Master_AddSlave(AGR_PnP_Master_t* master,
                              const AGR_PnP_SlaveConfig_t* config);

/**
 * @brief 주기 실행 (PnP Task에서 100ms마다 호출)
 *
 * @param master  Master 인스턴스
 *
 * @details
 * [처리 항목]
 * 1. Master Heartbeat 전송 (1초마다, 1회)
 * 2. 모든 Slave의 Heartbeat Timeout 체크
 * 3. 연결 상태 변화 콜백 호출
 *
 * @note System PnP Task에서 호출합니다. Device의 RunPeriodic()과 별도입니다.
 *
 * [재연결 시나리오 100% 대응 (.cursorrules 5.2)]
 * - Slave Reset → Boot-up 수신 → 자동 재연결
 * - Slave Breakpoint → Timeout → STOPPED → Boot-up 대기 → 재연결
 * - Master Reset → Heartbeat 재전송 → Slave 감지 → 재연결
 */
void AGR_PnP_Master_RunPeriodic(AGR_PnP_Master_t* master);

/**
 * @brief CAN 메시지 처리 (FDCAN Rx Handler에서 호출)
 *
 * @param master  Master 인스턴스
 * @param can_id  CAN ID
 * @param data    수신 데이터
 * @param len     데이터 길이
 *
 * @details
 * [처리하는 메시지]
 * - Boot-up (0x700 + Node ID, data[0] == 0x00)
 * - Heartbeat (0x700 + Node ID, data[0] != 0x00)
 *
 * [처리하지 않는 메시지 (Device Driver가 처리)]
 * - SDO Response (0x580 + Node ID) → AGR_DOP
 * - PDO (0x180/0x280 + Node ID) → AGR_DOP
 *
 * @note Boot-up 수신 시 on_slave_bootup 콜백을 호출합니다.
 */
void AGR_PnP_Master_ProcessMessage(AGR_PnP_Master_t* master,
                                    uint32_t can_id,
                                    const uint8_t* data,
                                    uint8_t len);

/**
 * @brief NMT 명령 전송 (Master → Slave)
 *
 * @param master        Master 인스턴스
 * @param slave_node_id 대상 Slave Node ID (0 = Broadcast)
 * @param cmd           NMT 명령 (AGR_NMT_CMD_START 등)
 * @return 0: 성공, <0: 에러
 *
 * @details CANopen 표준: CAN ID = 0x000, Data = [cmd, node_id]
 */
int AGR_PnP_Master_SendNmt(AGR_PnP_Master_t* master,
                            uint8_t slave_node_id,
                            AGR_NMT_Cmd_t cmd);

/**
 * @brief Slave 연결 상태 확인
 *
 * @param master        Master 인스턴스
 * @param slave_node_id Slave Node ID
 * @return true: 온라인 (Heartbeat Timeout 이내), false: 오프라인
 */
bool AGR_PnP_Master_IsSlaveOnline(const AGR_PnP_Master_t* master,
                                   uint8_t slave_node_id);

/**
 * @brief Slave NMT 상태 조회
 *
 * @param master        Master 인스턴스
 * @param slave_node_id Slave Node ID
 * @return Slave의 현재 NMT 상태
 */
AGR_NMT_State_t AGR_PnP_Master_GetSlaveState(const AGR_PnP_Master_t* master,
                                               uint8_t slave_node_id);

/**
 * @brief Slave 정보 접근 (NMT 인스턴스 등)
 *
 * @param master        Master 인스턴스
 * @param slave_node_id Slave Node ID
 * @return Slave 정보 포인터 (없으면 NULL)
 *
 * @note Device Driver가 내부 NMT에 접근해야 할 때 사용합니다.
 */
AGR_PnP_SlaveInfo_t* AGR_PnP_Master_GetSlaveInfo(AGR_PnP_Master_t* master,
                                                    uint8_t slave_node_id);

#endif /* AGR_PNP_MASTER_H */
