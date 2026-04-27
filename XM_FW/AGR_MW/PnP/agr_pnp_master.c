/**
 ******************************************************************************
 * @file    agr_pnp_master.c
 * @author  HyundoKim
 * @brief   AGR PnP Master 구현 - Slave 추적 + Heartbeat 전송 + 재연결
 * @version 1.0.0
 * @date    2026-02-10
 *
 * @details
 * [구현 원칙 (.cursorrules 5.2)]
 * 1. Master Heartbeat 주기적 전송 (Slave 감지 유도)
 * 2. Boot-up 무조건 수신 → PRE_OP
 * 3. Slave Timeout → STOPPED → Boot-up 재대기
 * 4. 재연결 시도 무한
 *
 * [재연결 시나리오 100% 대응]
 * 1. Slave Reset → Boot-up 수신 → 자동 재연결
 * 2. Slave Breakpoint → Timeout → STOPPED → Boot-up 대기 → 재연결
 * 3. Master Reset → Slave가 Boot-up 재전송 → 수신 → 재연결
 * 4. 동시 부팅 → Heartbeat 우선 전송 → Slave 감지 → 재연결
 * 5. CAN 케이블 탈착 → 재장착 → 재연결
 * 6. 한쪽 전원 OFF/ON → 재연결
 * 7. 양쪽 Breakpoint → 재시작 → 재연결
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_pnp_master.h"
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief CANopen Heartbeat CAN ID = 0x700 + Node ID */
#define CANOPEN_HEARTBEAT_BASE_ID    0x700U

/** @brief CANopen Heartbeat Function Code Mask */
#define CANOPEN_HEARTBEAT_FUNC_MASK  0x780U

/** @brief CANopen NMT Command CAN ID */
#define CANOPEN_NMT_CMD_CAN_ID       0x000U

/** @brief CANopen Node ID Mask (하위 7비트) */
#define CANOPEN_NODE_ID_MASK          0x07FU

/** @brief Boot-up 식별자 (data[0] == 0x00) */
#define CANOPEN_BOOTUP_STATE          0x00U

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static AGR_PnP_SlaveInfo_t* _FindSlaveByNodeId(AGR_PnP_Master_t* master, uint8_t node_id);
static const AGR_PnP_SlaveInfo_t* _FindSlaveByNodeIdConst(const AGR_PnP_Master_t* master, uint8_t node_id);
static void _OnSlaveNmtTimeout(void* user_ctx);
static void _OnSlaveNmtStateChanged(AGR_NMT_State_t old_state, AGR_NMT_State_t new_state, void* user_ctx);
static int _SendHeartbeat(AGR_PnP_Master_t* master);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief Master PnP 초기화
 */
int AGR_PnP_Master_Init(AGR_PnP_Master_t* master,
                         uint8_t my_node_id,
                         AGR_PnP_TxFunc_t tx_func,
                         AGR_PnP_GetTickFunc_t get_tick)
{
    if (master == NULL || tx_func == NULL || get_tick == NULL) {
        return -1;
    }

    memset(master, 0, sizeof(AGR_PnP_Master_t));
    master->my_node_id = my_node_id;
    master->tx_func = tx_func;
    master->get_tick = get_tick;
    master->slave_count = 0;
    master->last_heartbeat_sent_ms = 0;
    master->is_initialized = true;

    return 0;
}

/**
 * @brief Slave 등록
 *
 * @details
 * [동작 원리]
 * 1. Slave 정보를 배열에 복사
 * 2. NMT 인스턴스 초기화 (BOOT_UP 상태, Timeout 설정)
 * 3. NMT 콜백 연결 (상태 변경 → PnP 콜백 라우팅)
 *
 * [NMT 콜백 라우팅]
 * AGR_NMT → _OnSlaveNmtStateChanged() → slave->callbacks.on_slave_state_changed()
 * AGR_NMT → _OnSlaveNmtTimeout() → slave->callbacks.on_slave_offline()
 */
int AGR_PnP_Master_AddSlave(AGR_PnP_Master_t* master,
                              const AGR_PnP_SlaveConfig_t* config)
{
    if (master == NULL || config == NULL || !master->is_initialized) {
        return -1;
    }

    if (master->slave_count >= AGR_PNP_MASTER_MAX_SLAVES) {
        return -2;  /* 슬롯 부족 */
    }

    /* 중복 등록 방지 */
    if (_FindSlaveByNodeId(master, config->node_id) != NULL) {
        return -3;  /* 이미 등록됨 */
    }

    /* Slave 정보 복사 */
    AGR_PnP_SlaveInfo_t* slave = &master->slaves[master->slave_count];
    slave->name = config->name;
    slave->node_id = config->node_id;
    slave->heartbeat_timeout_ms = config->heartbeat_timeout_ms;
    slave->callbacks = config->callbacks;
    slave->is_registered = true;
    slave->ever_received_heartbeat = false;

    /* ===== NMT 인스턴스 초기화 (Slave 추적용) =====
     * [변경 이유]
     * - NMT가 Slave의 Heartbeat Timeout을 자동으로 감지
     * - on_state_changed 콜백으로 PnP 이벤트 라우팅
     * - on_timeout 콜백으로 Slave Offline 감지
     */
    AGR_NMT_InitEx(&slave->nmt,
                    config->heartbeat_timeout_ms,
                    config->node_id,
                    _OnSlaveNmtStateChanged,
                    _OnSlaveNmtTimeout,
                    slave);  /* user_ctx = SlaveInfo 자체 */

    master->slave_count++;

    return (int)(master->slave_count - 1);  /* Slave Index 반환 */
}

/**
 * @brief Master 주기 실행 (100ms마다 호출)
 *
 * @details
 * [처리 순서]
 * 1. 모든 Slave의 Heartbeat Timeout 체크
 *    - ever_received_heartbeat == false: Timeout 체크 스킵 (아직 미감지)
 *    - Timeout 발생 시: NMT가 STOPPED 전환 → 콜백 호출
 * 2. Master Heartbeat 전송 (1초마다, 1회)
 *    - Slave가 Master를 감지하도록 유도
 */
void AGR_PnP_Master_RunPeriodic(AGR_PnP_Master_t* master)
{
    if (master == NULL || !master->is_initialized) {
        return;
    }

    uint32_t now = master->get_tick();

    /* ===== 1. Slave Timeout 체크 ===== */
    for (uint8_t i = 0; i < master->slave_count; i++) {
        AGR_PnP_SlaveInfo_t* slave = &master->slaves[i];
        if (!slave->is_registered) {
            continue;
        }

        /* ===== 미감지 Slave는 Timeout 체크 스킵 (.cursorrules 5.2) =====
         * [이유]
         * - 아직 Boot-up/Heartbeat를 한 번도 받지 못한 Slave
         * - Timeout을 체크하면 NMT가 STOPPED로 전환됨
         * - BOOT_UP 상태를 유지해야 Boot-up 수신 시 정상 전이 가능
         */
        if (!slave->ever_received_heartbeat) {
            continue;
        }

        AGR_NMT_CheckTimeout(&slave->nmt, now);
    }

    /* ===== 2. Master Heartbeat 전송 (1초마다) =====
     * [역할]
     * - Slave가 Master를 감지하도록 유도
     * - 동시 부팅 시 Slave가 Boot-up 재전송 트리거
     * - CAN 케이블 재장착 시 Slave가 즉시 감지
     */
    if (now - master->last_heartbeat_sent_ms >= AGR_PNP_MASTER_HEARTBEAT_INTERVAL_MS) {
        int result = _SendHeartbeat(master);
        if (result >= 0) {
            master->last_heartbeat_sent_ms = now;
        }
        /* ✅ 전송 실패 시 타임스탬프 갱신 안함 → 다음 주기에 재시도 */
    }
}

/**
 * @brief CAN 메시지 처리 (Boot-up, Heartbeat)
 *
 * @details
 * [처리 흐름]
 * 1. CAN ID에서 Function Code와 Node ID 추출 (CANopen 표준)
 * 2. 0x700 Range → Boot-up 또는 Heartbeat
 * 3. Node ID로 등록된 Slave 검색
 * 4. NMT 처리 위임 (상태 전이 + 콜백)
 * 5. ever_received_heartbeat 플래그 설정 (.cursorrules 재연결)
 * 6. Boot-up 시 on_slave_bootup 콜백 호출
 */
void AGR_PnP_Master_ProcessMessage(AGR_PnP_Master_t* master,
                                    uint32_t can_id,
                                    const uint8_t* data,
                                    uint8_t len)
{
    if (master == NULL || data == NULL || len == 0 || !master->is_initialized) {
        return;
    }

    /* ===== Heartbeat / Boot-up (CAN ID: 0x700 + Node ID) ===== */
    uint32_t fnc_code = can_id & CANOPEN_HEARTBEAT_FUNC_MASK;
    if (fnc_code == CANOPEN_HEARTBEAT_BASE_ID && len >= 1) {
        uint8_t source_node_id = (uint8_t)(can_id & CANOPEN_NODE_ID_MASK);

        /* Master 자신의 메시지 무시 */
        if (source_node_id == master->my_node_id) {
            return;
        }

        /* 등록된 Slave 검색 */
        AGR_PnP_SlaveInfo_t* slave = _FindSlaveByNodeId(master, source_node_id);
        if (slave == NULL) {
            return;  /* 미등록 Slave → 무시 */
        }

        uint32_t now = master->get_tick();
        bool is_bootup = (data[0] == CANOPEN_BOOTUP_STATE);
        bool was_ever_received = slave->ever_received_heartbeat;

        /* ✅ Slave 감지 이력 기록 (.cursorrules 5.2 재연결 로직) */
        slave->ever_received_heartbeat = true;

        if (is_bootup) {
            /* ===== Boot-up 수신: 무조건 PRE_OP 전환 (.cursorrules 5.2) =====
             * [동작]
             * 1. NMT를 BOOT_UP으로 리셋
             * 2. AGR_NMT_UpdateActivity → BOOT_UP → PRE_OPERATIONAL 자동 전이
             * 3. on_slave_bootup 콜백 호출 → Device의 Pre-Op State Machine 시작
             */
            slave->nmt.state = AGR_NMT_BOOT_UP;
            AGR_NMT_UpdateActivity(&slave->nmt, now);

            /* Boot-up 콜백 (Device Driver의 Pre-Op 시작 트리거) */
            if (slave->callbacks.on_slave_bootup != NULL) {
                slave->callbacks.on_slave_bootup(source_node_id);
            }

            /* 첫 감지 시 Online 콜백도 호출 */
            if (!was_ever_received && slave->callbacks.on_slave_online != NULL) {
                slave->callbacks.on_slave_online(source_node_id);
            }
        } else {
            /* ===== Heartbeat 수신: Slave NMT State 반영 + Activity 갱신 ===== */
            AGR_NMT_State_t old_state = slave->nmt.state;
            AGR_NMT_State_t reported_state = (AGR_NMT_State_t)data[0];

            /* Heartbeat data[0] = Slave의 현재 NMT 상태 (CANopen 표준) */
            slave->nmt.state = reported_state;
            AGR_NMT_UpdateActivity(&slave->nmt, now);

            /* OPERATIONAL 전환 감지 → Online 콜백 */
            if (old_state != AGR_NMT_OPERATIONAL &&
                reported_state == AGR_NMT_OPERATIONAL) {
                if (slave->callbacks.on_slave_online != NULL) {
                    slave->callbacks.on_slave_online(source_node_id);
                }
            }

            /* 첫 감지 시 Online 콜백 */
            if (!was_ever_received && slave->callbacks.on_slave_online != NULL) {
                slave->callbacks.on_slave_online(source_node_id);
            }
        }
    }
}

/**
 * @brief NMT 명령 전송 (Master → Slave)
 *
 * @details CANopen 표준: CAN ID = 0x000, Data = [cmd, target_node_id]
 */
int AGR_PnP_Master_SendNmt(AGR_PnP_Master_t* master,
                            uint8_t slave_node_id,
                            AGR_NMT_Cmd_t cmd)
{
    if (master == NULL || !master->is_initialized || master->tx_func == NULL) {
        return -1;
    }

    uint8_t nmt_data[2] = {
        (uint8_t)cmd,
        slave_node_id   /* 0 = Broadcast */
    };

    return master->tx_func(CANOPEN_NMT_CMD_CAN_ID, nmt_data, 2);
}

/**
 * @brief Slave 연결 상태 확인
 */
bool AGR_PnP_Master_IsSlaveOnline(const AGR_PnP_Master_t* master,
                                   uint8_t slave_node_id)
{
    if (master == NULL || !master->is_initialized) {
        return false;
    }

    const AGR_PnP_SlaveInfo_t* slave = _FindSlaveByNodeIdConst(master, slave_node_id);
    if (slave == NULL) {
        return false;
    }

    /* Online = Heartbeat 수신 이력 있음 + STOPPED가 아님 */
    return slave->ever_received_heartbeat &&
           (slave->nmt.state != AGR_NMT_STOPPED) &&
           (slave->nmt.state != AGR_NMT_BOOT_UP);
}

/**
 * @brief Slave NMT 상태 조회
 */
AGR_NMT_State_t AGR_PnP_Master_GetSlaveState(const AGR_PnP_Master_t* master,
                                               uint8_t slave_node_id)
{
    if (master == NULL || !master->is_initialized) {
        return AGR_NMT_STOPPED;
    }

    const AGR_PnP_SlaveInfo_t* slave = _FindSlaveByNodeIdConst(master, slave_node_id);
    if (slave == NULL) {
        return AGR_NMT_STOPPED;
    }

    return slave->nmt.state;
}

/**
 * @brief Slave 정보 접근
 */
AGR_PnP_SlaveInfo_t* AGR_PnP_Master_GetSlaveInfo(AGR_PnP_Master_t* master,
                                                    uint8_t slave_node_id)
{
    if (master == NULL || !master->is_initialized) {
        return NULL;
    }

    return _FindSlaveByNodeId(master, slave_node_id);
}

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief Node ID로 Slave 검색
 */
static AGR_PnP_SlaveInfo_t* _FindSlaveByNodeId(AGR_PnP_Master_t* master, uint8_t node_id)
{
    for (uint8_t i = 0; i < master->slave_count; i++) {
        if (master->slaves[i].is_registered && master->slaves[i].node_id == node_id) {
            return &master->slaves[i];
        }
    }
    return NULL;
}

/**
 * @brief Node ID로 Slave 검색 (const 버전)
 */
static const AGR_PnP_SlaveInfo_t* _FindSlaveByNodeIdConst(const AGR_PnP_Master_t* master, uint8_t node_id)
{
    for (uint8_t i = 0; i < master->slave_count; i++) {
        if (master->slaves[i].is_registered && master->slaves[i].node_id == node_id) {
            return &master->slaves[i];
        }
    }
    return NULL;
}

/**
 * @brief Slave NMT Timeout 콜백 (NMT → PnP 라우팅)
 *
 * @details
 * AGR_NMT_CheckTimeout()이 Timeout 감지 시 호출합니다.
 * Slave가 STOPPED로 전환되었음을 Device Driver에 알립니다.
 */
static void _OnSlaveNmtTimeout(void* user_ctx)
{
    AGR_PnP_SlaveInfo_t* slave = (AGR_PnP_SlaveInfo_t*)user_ctx;
    if (slave == NULL) {
        return;
    }

    /* Slave Offline 콜백 호출 */
    if (slave->callbacks.on_slave_offline != NULL) {
        slave->callbacks.on_slave_offline(slave->node_id);
    }
}

/**
 * @brief Slave NMT 상태 변경 콜백 (NMT → PnP 라우팅)
 *
 * @details
 * AGR_NMT가 상태 전이 시 호출합니다.
 * Device Driver의 on_slave_state_changed 콜백으로 전달합니다.
 */
static void _OnSlaveNmtStateChanged(AGR_NMT_State_t old_state,
                                     AGR_NMT_State_t new_state,
                                     void* user_ctx)
{
    AGR_PnP_SlaveInfo_t* slave = (AGR_PnP_SlaveInfo_t*)user_ctx;
    if (slave == NULL) {
        return;
    }

    /* Device Driver 콜백 호출 */
    if (slave->callbacks.on_slave_state_changed != NULL) {
        slave->callbacks.on_slave_state_changed(slave->node_id, old_state, new_state);
    }
}

/**
 * @brief Master Heartbeat 전송
 *
 * @details
 * CAN ID: 0x700 + my_node_id
 * Data: [0x05] (OPERATIONAL 상태)
 *
 * [목적]
 * - Slave가 Master를 감지하도록 유도
 * - 동시 부팅 시 Slave가 Boot-up 재전송 트리거
 */
static int _SendHeartbeat(AGR_PnP_Master_t* master)
{
    uint8_t hb_data[1] = { (uint8_t)AGR_NMT_OPERATIONAL };
    uint32_t can_id = CANOPEN_HEARTBEAT_BASE_ID + master->my_node_id;

    return master->tx_func(can_id, hb_data, 1);
}
