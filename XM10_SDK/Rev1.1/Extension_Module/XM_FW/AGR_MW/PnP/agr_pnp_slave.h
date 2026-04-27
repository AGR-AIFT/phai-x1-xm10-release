/**
 ******************************************************************************
 * @file    agr_pnp_slave.h
 * @author  HyundoKim
 * @brief   AGR PnP Slave API - "나는 Slave, Master를 추적한다"
 * @version 1.0.0
 * @date    2026-02-10
 *
 * @details
 * CANopen 표준 기반 PnP Slave 추상화 레이어입니다.
 * 하나의 Slave 인스턴스가 자신의 Master를 추적합니다.
 *
 * [핵심 역할]
 * - Boot-up 메시지 자동 재전송 (BOOT_UP 상태, 1초마다)
 * - Heartbeat 전송 (PRE_OP/OPERATIONAL 상태, 1초마다)
 * - Master Heartbeat Timeout 감지 → BOOT_UP 리셋
 * - NMT 명령 수신 처리 (START, STOP, PRE_OP)
 * - 100% 재연결 보장 (.cursorrules 5.3 준수)
 *
 * [사용 위치]
 * - IMU Hub (BareMetal): System Layer에서 인스턴스 생성, xm_drv.c에서 콜백 등록
 * - 기타 Sensor Module: 동일 패턴
 *
 * [Boot-up 재전송 3가지 Case (.cursorrules 5.3)]
 * - Case 1: Master 못 봤음 → 주기적 Boot-up 재전송 (1초마다)
 * - Case 2: Master 살아있음 + 내가 BOOT_UP → Boot-up 재전송
 * - Case 3: Master 끊김 → Boot-up 재전송 (Master 돌아올 때 대비)
 *
 * [사용 예시]
 * ```c
 * // system_startup.c (IMU Hub System Layer)
 * static AGR_PnP_Slave_t s_slave_pnp;
 *
 * void System_Init(void) {
 *     AGR_PnP_Slave_Init(&s_slave_pnp, AGR_NODE_ID_IMU_HUB,
 *                          AGR_NODE_ID_XM, 3000,
 *                          App_Fdcan_Transmit, IOIF_TIM_GetTick);
 *     XM_Drv_Init(tx_func, &s_slave_pnp);
 * }
 *
 * // Timer ISR (100ms마다)
 * if (cnt % 100 == 0) {
 *     AGR_PnP_Slave_RunPeriodic(&s_slave_pnp);
 *     XM_Drv_RunPeriodic();
 * }
 * ```
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_PNP_SLAVE_H
#define AGR_PNP_SLAVE_H

#include <stdint.h>
#include <stdbool.h>
#include "agr_nmt.h"
#include "agr_pnp_types.h"

/**
 *-----------------------------------------------------------
 * CONFIGURATION
 *-----------------------------------------------------------
 */

#define AGR_PNP_SLAVE_HEARTBEAT_INTERVAL_MS     1000  /**< Heartbeat 전송 주기 (ms) */
#define AGR_PNP_SLAVE_BOOTUP_INTERVAL_MS        1000  /**< Boot-up 재전송 주기 (ms) */

/**
 * @brief Slave 이벤트 콜백 (Device Driver에서 구현)
 *
 * @details
 * Device Driver가 PnP 이벤트를 받아 상태를 업데이트합니다.
 * - BareMetal: Timer ISR 컨텍스트에서 호출
 * - RTOS: PnP Task 컨텍스트에서 호출
 */
typedef struct {
    /** @brief Master 연결됨 (Master Heartbeat 첫 수신)
     *  @note  Device Driver에서 "Master 연결" 상태 업데이트 */
    void (*on_master_online)(void);

    /** @brief Master 연결 끊김 (Heartbeat Timeout)
     *  @note  Device Driver에서 PDO 전송 중단 등 처리 */
    void (*on_master_offline)(void);

    /** @brief 나의 NMT 상태 변경됨
     *  @param old_state  이전 NMT 상태
     *  @param new_state  새 NMT 상태
     *  @note  PRE_OP → OPERATIONAL 전환 시 TPDO 전송 시작 등 */
    void (*on_my_state_changed)(AGR_NMT_State_t old_state,
                                AGR_NMT_State_t new_state);

    /** @brief NMT 명령 수신 (Master → 나)
     *  @param cmd  NMT 명령 (AGR_NMT_CMD_START, STOP, PRE_OP 등)
     *  @note  Device-specific 처리가 필요할 때 사용 */
    void (*on_nmt_command)(AGR_NMT_Cmd_t cmd);
} AGR_PnP_Slave_Callbacks_t;

/**
 * @brief Slave PnP 인스턴스
 *
 * @details
 * System Layer에서 하나만 생성합니다 (Node당 1개).
 * Boot-up/Heartbeat 전송, Master 추적을 자동으로 수행합니다.
 *
 * [BareMetal 호환]
 * - Mutex/Semaphore 미사용 (ISR Priority 차별화로 보호)
 * - volatile 사용 (ISR/Main에서 공유)
 *
 * [재연결 전략 (.cursorrules 5.3)]
 * - Master Timeout → 자신의 NMT를 BOOT_UP으로 리셋
 * - BOOT_UP 상태 → Boot-up 메시지 주기적 재전송
 * - Master Heartbeat 재수신 → 즉시 Boot-up 재전송
 */
typedef struct {
    /* 설정 */
    uint8_t                  my_node_id;           /**< 나의 CANopen Node ID */
    AGR_PnP_TxFunc_t         tx_func;              /**< CAN 전송 함수 */
    AGR_PnP_GetTickFunc_t    get_tick;             /**< Tick 함수 */
    AGR_PnP_Slave_Callbacks_t callbacks;           /**< 이벤트 콜백 */

    /* 나의 NMT 상태 */
    AGR_NMT_Inst_t           my_nmt;               /**< 나의 NMT (BOOT_UP → PRE_OP → OPERATIONAL) */

    /* Master 추적 */
    struct {
        uint8_t              node_id;              /**< Master Node ID (e.g., AGR_NODE_ID_XM) */
        AGR_NMT_Inst_t       nmt;                  /**< Master NMT (Heartbeat 기반 추적) */
        uint32_t             heartbeat_timeout_ms; /**< Master Heartbeat Timeout (ms) */
        uint32_t             last_heartbeat_rx_ms; /**< 마지막 Master Heartbeat 수신 시간 */
        volatile bool        is_online;            /**< Master 온라인 여부 */
        bool                 ever_received_heartbeat; /**< Master Heartbeat 수신 이력 (.cursorrules) */
    } master;

    /* 타이밍 */
    uint32_t                 last_bootup_sent_ms;  /**< 마지막 Boot-up 전송 시간 */
    uint32_t                 last_heartbeat_sent_ms; /**< 마지막 Heartbeat 전송 시간 */

    /* 상태 */
    bool                     is_initialized;       /**< 초기화 완료 여부 */
} AGR_PnP_Slave_t;

/**
 *-----------------------------------------------------------
 * PUBLIC API
 *-----------------------------------------------------------
 */

/**
 * @brief Slave PnP 초기화
 *
 * @param slave          Slave 인스턴스 (System Layer에서 정적 할당)
 * @param my_node_id     나의 CANopen Node ID (e.g., AGR_NODE_ID_IMU_HUB)
 * @param master_node_id Master의 Node ID (e.g., AGR_NODE_ID_XM)
 * @param timeout_ms     Master Heartbeat Timeout (ms), 권장: 3000
 * @param tx_func        CAN 전송 함수 (Dependency Injection)
 * @param get_tick       Tick 함수
 * @return 0: 성공, <0: 에러
 *
 * @note
 * - System Layer(system_startup.c)에서 1회 호출합니다.
 * - 초기 상태: my_nmt = BOOT_UP (Boot-up 전송은 RunPeriodic에서 자동)
 * - ❌ Init에서 Boot-up 전송 안함 (.cursorrules 금지사항)
 */
int AGR_PnP_Slave_Init(AGR_PnP_Slave_t* slave,
                         uint8_t my_node_id,
                         uint8_t master_node_id,
                         uint32_t timeout_ms,
                         AGR_PnP_TxFunc_t tx_func,
                         AGR_PnP_GetTickFunc_t get_tick);

/**
 * @brief 콜백 등록 (Device Driver에서 호출)
 *
 * @param slave      Slave 인스턴스
 * @param callbacks  이벤트 콜백 구조체
 * @return 0: 성공, <0: 에러
 *
 * @note Device Driver의 Init()에서 호출합니다.
 *
 * @example
 * ```c
 * void XM_Drv_Init(AGR_TxFunc_t tx, AGR_PnP_Slave_t* slave_pnp) {
 *     AGR_PnP_Slave_SetCallbacks(slave_pnp, &(AGR_PnP_Slave_Callbacks_t){
 *         .on_master_online = _OnMasterOnline,
 *         .on_master_offline = _OnMasterOffline,
 *         .on_my_state_changed = _OnMyStateChanged,
 *         .on_nmt_command = _OnNmtCommand,
 *     });
 * }
 * ```
 */
int AGR_PnP_Slave_SetCallbacks(AGR_PnP_Slave_t* slave,
                                const AGR_PnP_Slave_Callbacks_t* callbacks);

/**
 * @brief 주기 실행 (100ms마다 호출)
 *
 * @param slave  Slave 인스턴스
 *
 * @details
 * [처리 항목]
 * 1. Master Heartbeat Timeout 체크
 *    - Timeout 발생 → 나의 NMT를 BOOT_UP으로 리셋
 * 2. 내 상태에 따라 메시지 전송:
 *    - BOOT_UP → Boot-up 재전송 (1초마다, 3가지 Case)
 *    - PRE_OP/OPERATIONAL → Heartbeat 전송 (1초마다)
 *
 * [Boot-up 재전송 3가지 Case (.cursorrules 5.3)]
 * - Case 1: Master 못 봤음 → 주기적 재전송 (Master 대기)
 * - Case 2: Master 살아있음 → Boot-up 재전송 (연결 시도)
 * - Case 3: Master 끊김 → Boot-up 재전송 (Master 돌아올 때 대비)
 *
 * @note
 * - RTOS: PnP Task에서 호출
 * - BareMetal: Timer ISR 100ms 카운터에서 호출
 */
void AGR_PnP_Slave_RunPeriodic(AGR_PnP_Slave_t* slave);

/**
 * @brief CAN 메시지 처리 (FDCAN ISR 또는 Rx Handler에서 호출)
 *
 * @param slave   Slave 인스턴스
 * @param can_id  CAN ID
 * @param data    수신 데이터
 * @param len     데이터 길이
 *
 * @details
 * [처리하는 메시지]
 * - Master Heartbeat (0x700 + Master Node ID)
 * - NMT Command (0x000, data = [cmd, target_node])
 *
 * [처리하지 않는 메시지 (Device Driver가 처리)]
 * - SDO Request (0x600 + Node ID) → AGR_DOP
 * - PDO (0x200/0x300 + Node ID) → AGR_DOP
 *
 * @note
 * - RTOS: canfd_rx_handler의 _NonRealtimeTask에서 호출
 * - BareMetal: FDCAN ISR에서 직접 호출
 */
void AGR_PnP_Slave_ProcessMessage(AGR_PnP_Slave_t* slave,
                                    uint32_t can_id,
                                    const uint8_t* data,
                                    uint8_t len);

/**
 * @brief Master 연결 상태 확인
 *
 * @param slave  Slave 인스턴스
 * @return true: Master 온라인, false: Master 오프라인
 */
bool AGR_PnP_Slave_IsMyMasterOnline(const AGR_PnP_Slave_t* slave);

/**
 * @brief 나의 NMT 상태 조회
 *
 * @param slave  Slave 인스턴스
 * @return 나의 현재 NMT 상태
 */
AGR_NMT_State_t AGR_PnP_Slave_GetMyState(const AGR_PnP_Slave_t* slave);

/**
 * @brief 나의 NMT 인스턴스 접근 (Device Driver 전용)
 *
 * @param slave  Slave 인스턴스
 * @return 나의 NMT 인스턴스 포인터
 *
 * @note Device Driver가 DOP 처리 시 NMT 인스턴스가 필요할 때 사용합니다.
 */
AGR_NMT_Inst_t* AGR_PnP_Slave_GetMyNmt(AGR_PnP_Slave_t* slave);

/**
 * @brief Boot-up 메시지 즉시 전송 (테스트/디버깅용)
 *
 * @param slave  Slave 인스턴스
 * @return 0: 성공, <0: 에러
 *
 * @warning 일반적으로 RunPeriodic()이 자동 전송합니다. 수동 호출 불필요.
 */
int AGR_PnP_Slave_SendBootupNow(AGR_PnP_Slave_t* slave);

/**
 * @brief Heartbeat 메시지 즉시 전송 (테스트/디버깅용)
 *
 * @param slave  Slave 인스턴스
 * @return 0: 성공, <0: 에러
 *
 * @warning 일반적으로 RunPeriodic()이 자동 전송합니다. 수동 호출 불필요.
 */
int AGR_PnP_Slave_SendHeartbeatNow(AGR_PnP_Slave_t* slave);

#endif /* AGR_PNP_SLAVE_H */
