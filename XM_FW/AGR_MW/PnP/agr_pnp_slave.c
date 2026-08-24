/**
 ******************************************************************************
 * @file    agr_pnp_slave.c
 * @author  HyundoKim
 * @brief   AGR PnP Slave 구현 - Master 추적 + Boot-up/Heartbeat 전송 + 재연결
 * @version 1.0.0
 * @date    2026-02-10
 *
 * @details
 * [구현 원칙 (.cursorrules 5.3)]
 * 1. Boot-up 재전송 3가지 케이스
 *    - Master 못 봤음 → 주기적 재전송
 *    - Master 살아있음 → 연결 시도
 *    - Master 끊김 → Master 대기
 * 2. Master Timeout → BOOT_UP 리셋
 * 3. Master Heartbeat 감지 추적
 * 4. 재연결 시도 무한
 *
 * [재연결 시나리오 100% 대응]
 * 1. Master Reset → Timeout → BOOT_UP 리셋 → Boot-up 재전송 → 재연결
 * 2. Slave Reset → Boot-up 대기 → Master 감지 → Boot-up 전송 → 재연결
 * 3. Slave Breakpoint → 재시작 → Master 감지 → Boot-up 재전송 → 재연결
 * 4. 동시 부팅 → Boot-up 재전송 → Master 수신 → 재연결
 * 5. CAN 케이블 탈착 → 재장착 → 재연결
 * 6. 전원 OFF/ON → 재연결
 *
 * [BareMetal 호환]
 * - Mutex/Semaphore 미사용
 * - ISR Priority 차별화로 보호 (Timer ISR > FDCAN ISR)
 * - volatile 변수 사용 (ISR/Main 공유)
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_pnp_slave.h"
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

static int _SendBootup(AGR_PnP_Slave_t* slave);
static int _SendHeartbeat(AGR_PnP_Slave_t* slave);
static void _OnMasterNmtTimeout(void* user_ctx);
static void _OnMasterNmtStateChanged(AGR_NMT_State_t old_state,
                                      AGR_NMT_State_t new_state,
                                      void* user_ctx);
static void _OnMyNmtStateChanged(AGR_NMT_State_t old_state,
                                  AGR_NMT_State_t new_state,
                                  void* user_ctx);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief Slave PnP 초기화
 *
 * @details
 * [초기 상태]
 * - my_nmt = BOOT_UP (RunPeriodic에서 Boot-up 자동 전송)
 * - master.is_online = false
 * - master.ever_received_heartbeat = false
 *
 * [금지 사항 (.cursorrules)]
 * - ❌ Init에서 Boot-up 전송 안함
 * - ❌ Init에서 NMT 상태 임의 변경 안함
 */
int AGR_PnP_Slave_Init(AGR_PnP_Slave_t* slave,
                         uint8_t my_node_id,
                         uint8_t master_node_id,
                         uint32_t timeout_ms,
                         AGR_PnP_TxFunc_t tx_func,
                         AGR_PnP_GetTickFunc_t get_tick)
{
    if (slave == NULL || tx_func == NULL || get_tick == NULL) {
        return -1;
    }

    memset(slave, 0, sizeof(AGR_PnP_Slave_t));
    slave->my_node_id = my_node_id;
    slave->tx_func = tx_func;
    slave->get_tick = get_tick;

    /* ===== 나의 NMT 초기화 (BOOT_UP 시작) =====
     * [역할]
     * - 나의 NMT 상태 관리 (Master가 NMT 명령으로 제어)
     * - Timeout 없음 (자신의 NMT는 Timeout 불필요)
     */
    AGR_NMT_InitEx(&slave->my_nmt,
                    0,              /* Timeout 없음 (자기 자신은 Timeout 불필요) */
                    my_node_id,
                    _OnMyNmtStateChanged,
                    NULL,           /* Timeout 콜백 없음 */
                    slave);

    /* ===== Master NMT 초기화 (Master 추적용) =====
     * [역할]
     * - Master Heartbeat 수신 → Activity 갱신
     * - Master Timeout 감지 → STOPPED → BOOT_UP 리셋
     */
    slave->master.node_id = master_node_id;
    slave->master.heartbeat_timeout_ms = timeout_ms;
    slave->master.is_online = false;
    slave->master.ever_received_heartbeat = false;
    slave->master.last_heartbeat_rx_ms = 0;

    AGR_NMT_InitEx(&slave->master.nmt,
                    timeout_ms,
                    master_node_id,
                    _OnMasterNmtStateChanged,
                    _OnMasterNmtTimeout,
                    slave);

    /* 타이밍 초기화 */
    slave->last_bootup_sent_ms = 0;
    slave->last_heartbeat_sent_ms = 0;

    slave->is_initialized = true;

    /* ❌ Boot-up 전송 안함 (.cursorrules: Init에서 Boot-up 전송 금지)
     * → RunPeriodic에서 BOOT_UP 상태일 때 자동 전송 */

    return 0;
}

/**
 * @brief 콜백 등록
 */
int AGR_PnP_Slave_SetCallbacks(AGR_PnP_Slave_t* slave,
                                const AGR_PnP_Slave_Callbacks_t* callbacks)
{
    if (slave == NULL || callbacks == NULL || !slave->is_initialized) {
        return -1;
    }

    slave->callbacks = *callbacks;
    return 0;
}

/**
 * @brief Slave 주기 실행 (100ms마다 호출)
 *
 * @details
 * [처리 순서]
 * 1. Master Heartbeat Timeout 체크
 *    - ever_received_heartbeat == false → 스킵 (아직 Master 미감지)
 *    - Timeout 발생 → 나의 NMT를 BOOT_UP으로 리셋
 * 2. 내 NMT 상태에 따라 메시지 전송
 *    - BOOT_UP → Boot-up 재전송 (3가지 Case)
 *    - PRE_OP/OPERATIONAL → Heartbeat 전송
 *
 * [Boot-up 재전송 전략 (.cursorrules 5.3)]
 * Case 1: Master 못 봤음 (ever_received_heartbeat == false)
 *   → 주기적 Boot-up 재전송 (Master 대기)
 * Case 2: Master 살아있음 (is_online == true) + 내가 BOOT_UP
 *   → Boot-up 재전송 (연결 시도)
 * Case 3: Master 끊김 (is_online == false, ever_received == true)
 *   → Boot-up 재전송 (Master 돌아올 때 대비)
 */
void AGR_PnP_Slave_RunPeriodic(AGR_PnP_Slave_t* slave)
{
    if (slave == NULL || !slave->is_initialized) {
        return;
    }

    uint32_t now = slave->get_tick();

    /* ===== 1. Master Timeout 체크 =====
     * [조건]
     * - Master Heartbeat를 한 번이라도 받은 적 있을 때만 체크
     * - 처음부터 Master 못 본 경우 → Timeout 불필요 (BOOT_UP 유지)
     */
    if (slave->master.ever_received_heartbeat) {
        bool was_master_online = slave->master.is_online;

        AGR_NMT_CheckTimeout(&slave->master.nmt, now);

        bool is_master_online_now = (slave->master.nmt.state != AGR_NMT_STOPPED &&
                                     slave->master.nmt.state != AGR_NMT_BOOT_UP);
        slave->master.is_online = is_master_online_now;

        /* ===== Master 끊김 감지 → 나의 NMT BOOT_UP 리셋 (.cursorrules 5.3) =====
         * [이유]
         * - Master Timeout → 재연결 프로세스 시작
         * - BOOT_UP 상태에서 Boot-up 재전송 → Master 돌아올 때 즉시 재연결
         */
        if (was_master_online && !is_master_online_now) {
            AGR_NMT_SetState(&slave->my_nmt, AGR_NMT_BOOT_UP);
        }
    }

    /* ===== 2. 내 NMT 상태에 따라 메시지 전송 ===== */
    AGR_NMT_State_t my_state = slave->my_nmt.state;

    if (my_state == AGR_NMT_BOOT_UP) {
        /* ===== Boot-up 재전송 (3가지 Case) ===== */
        bool should_send_bootup = false;

        if (now - slave->last_bootup_sent_ms >= AGR_PNP_SLAVE_BOOTUP_INTERVAL_MS) {
            /* Case 1: Master 못 봤음 → 주기적 재전송 (Master 대기) */
            if (!slave->master.ever_received_heartbeat) {
                should_send_bootup = true;
            }
            /* Case 2: Master 살아있음 → 연결 시도 */
            else if (slave->master.is_online) {
                should_send_bootup = true;
            }
            /* Case 3: Master 끊김 → Boot-up 재전송 (Master 돌아올 때 대비) */
            else {
                should_send_bootup = true;
            }
        }

        if (should_send_bootup) {
            int result = _SendBootup(slave);
            if (result >= 0) {
                slave->last_bootup_sent_ms = now;
            }
            /* ✅ 전송 실패 시 타임스탬프 갱신 안함 → 다음 주기에 재시도 */
        }
    }
    else if (my_state == AGR_NMT_PRE_OPERATIONAL || my_state == AGR_NMT_OPERATIONAL) {
        /* ===== Heartbeat 전송 (1초마다) ===== */
        if (now - slave->last_heartbeat_sent_ms >= AGR_PNP_SLAVE_HEARTBEAT_INTERVAL_MS) {
            int result = _SendHeartbeat(slave);
            if (result >= 0) {
                slave->last_heartbeat_sent_ms = now;
            }
            /* ✅ 전송 실패 시 타임스탬프 갱신 안함 → 다음 주기에 재시도 */
        }
    }
    /* STOPPED 상태 → 아무것도 안 함 (Master NMT 명령 대기) */
}

/**
 * @brief CAN 메시지 처리 (Master Heartbeat, NMT Command)
 *
 * @details
 * [처리하는 메시지]
 * 1. Master Heartbeat (0x700 + Master Node ID)
 *    - Activity 갱신 → Timeout 리셋
 *    - ever_received_heartbeat 플래그 설정
 * 2. NMT Command (0x000)
 *    - data = [cmd, target_node]
 *    - target이 나이거나 Broadcast(0)일 때 처리
 */
void AGR_PnP_Slave_ProcessMessage(AGR_PnP_Slave_t* slave,
                                    uint32_t can_id,
                                    const uint8_t* data,
                                    uint8_t len)
{
    if (slave == NULL || data == NULL || len == 0 || !slave->is_initialized) {
        return;
    }

    uint32_t now = slave->get_tick();

    /* ===== 1. NMT Command (CAN ID: 0x000) ===== */
    if (can_id == CANOPEN_NMT_CMD_CAN_ID && len >= 2) {
        uint8_t cmd_byte = data[0];
        uint8_t target_node = data[1];

        /* 브로드캐스트(0) 또는 나 자신 */
        if (target_node == 0 || target_node == slave->my_node_id) {
            AGR_NMT_Cmd_t cmd = (AGR_NMT_Cmd_t)cmd_byte;
            AGR_NMT_ProcessCommand(&slave->my_nmt, cmd);

            /* NMT 명령 콜백 (Device-specific 처리) */
            if (slave->callbacks.on_nmt_command != NULL) {
                slave->callbacks.on_nmt_command(cmd);
            }
        }
        return;
    }

    /* ===== 2. Master Heartbeat (0x700 + Master Node ID) ===== */
    uint32_t fnc_code = can_id & CANOPEN_HEARTBEAT_FUNC_MASK;
    if (fnc_code == CANOPEN_HEARTBEAT_BASE_ID && len >= 1) {
        uint8_t source_node_id = (uint8_t)(can_id & CANOPEN_NODE_ID_MASK);

        /* Master의 Heartbeat만 처리 */
        if (source_node_id == slave->master.node_id) {
            bool was_ever_received = slave->master.ever_received_heartbeat;

            /* ✅ Master 감지 이력 기록 (.cursorrules 5.3 재연결 로직) */
            slave->master.ever_received_heartbeat = true;
            slave->master.last_heartbeat_rx_ms = now;

            /* Master NMT 갱신: data[0]에서 NMT state 추출 후 반영 */
            AGR_NMT_State_t master_reported_state = (AGR_NMT_State_t)data[0];
            slave->master.nmt.state = master_reported_state;
            AGR_NMT_UpdateActivity(&slave->master.nmt, now);

            /* Master 온라인 상태 업데이트 */
            bool is_master_online_now = (slave->master.nmt.state != AGR_NMT_STOPPED &&
                                         slave->master.nmt.state != AGR_NMT_BOOT_UP);
            bool was_online = slave->master.is_online;
            slave->master.is_online = is_master_online_now;

            /* CANopen 표준: Master 감지 시 BOOT_UP → PRE_OPERATIONAL 자동 전환
             * Slave는 PRE_OP에 진입하여 SDO 설정(PDO Mapping 등)을 받을 준비 */
            if (slave->my_nmt.state == AGR_NMT_BOOT_UP && is_master_online_now) {
                AGR_NMT_SetState(&slave->my_nmt, AGR_NMT_PRE_OPERATIONAL);
            }

            /* 첫 Master 감지 시 콜백 호출 */
            if (!was_ever_received || (!was_online && is_master_online_now)) {
                if (slave->callbacks.on_master_online != NULL) {
                    slave->callbacks.on_master_online();
                }
            }
        }
    }
}

/**
 * @brief Master 연결 상태 확인
 */
bool AGR_PnP_Slave_IsMyMasterOnline(const AGR_PnP_Slave_t* slave)
{
    if (slave == NULL || !slave->is_initialized) {
        return false;
    }

    return slave->master.is_online;
}

/**
 * @brief 나의 NMT 상태 조회
 */
AGR_NMT_State_t AGR_PnP_Slave_GetMyState(const AGR_PnP_Slave_t* slave)
{
    if (slave == NULL || !slave->is_initialized) {
        return AGR_NMT_STOPPED;
    }

    return slave->my_nmt.state;
}

/**
 * @brief 나의 NMT 인스턴스 접근
 */
AGR_NMT_Inst_t* AGR_PnP_Slave_GetMyNmt(AGR_PnP_Slave_t* slave)
{
    if (slave == NULL || !slave->is_initialized) {
        return NULL;
    }

    return &slave->my_nmt;
}

/**
 * @brief Boot-up 즉시 전송
 */
int AGR_PnP_Slave_SendBootupNow(AGR_PnP_Slave_t* slave)
{
    if (slave == NULL || !slave->is_initialized) {
        return -1;
    }

    return _SendBootup(slave);
}

/**
 * @brief Heartbeat 즉시 전송
 */
int AGR_PnP_Slave_SendHeartbeatNow(AGR_PnP_Slave_t* slave)
{
    if (slave == NULL || !slave->is_initialized) {
        return -1;
    }

    return _SendHeartbeat(slave);
}

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief Boot-up 메시지 전송
 *
 * @details CANopen 표준: CAN ID = 0x700 + my_node_id, Data = [0x00]
 */
static int _SendBootup(AGR_PnP_Slave_t* slave)
{
    uint8_t bootup_data[1] = { CANOPEN_BOOTUP_STATE };
    uint32_t can_id = CANOPEN_HEARTBEAT_BASE_ID + slave->my_node_id;

    return slave->tx_func(can_id, bootup_data, 1);
}

/**
 * @brief Heartbeat 메시지 전송
 *
 * @details CANopen 표준: CAN ID = 0x700 + my_node_id, Data = [NMT State]
 */
static int _SendHeartbeat(AGR_PnP_Slave_t* slave)
{
    uint8_t hb_data[1] = { (uint8_t)slave->my_nmt.state };
    uint32_t can_id = CANOPEN_HEARTBEAT_BASE_ID + slave->my_node_id;

    return slave->tx_func(can_id, hb_data, 1);
}

/**
 * @brief Master NMT Timeout 콜백
 *
 * @details
 * Master Heartbeat Timeout 발생 시 호출됩니다.
 * Slave는 Master Offline 콜백을 호출합니다.
 *
 * [주의]
 * - 나의 NMT BOOT_UP 리셋은 RunPeriodic에서 처리
 * - 여기서는 콜백만 호출
 */
static void _OnMasterNmtTimeout(void* user_ctx)
{
    AGR_PnP_Slave_t* slave = (AGR_PnP_Slave_t*)user_ctx;
    if (slave == NULL) {
        return;
    }

    slave->master.is_online = false;

    /* Master Offline 콜백 */
    if (slave->callbacks.on_master_offline != NULL) {
        slave->callbacks.on_master_offline();
    }
}

/**
 * @brief Master NMT 상태 변경 콜백
 *
 * @details
 * Master NMT가 상태 전이할 때 호출됩니다.
 * (내부 사용 - 현재 특별한 처리 없음)
 */
static void _OnMasterNmtStateChanged(AGR_NMT_State_t old_state,
                                      AGR_NMT_State_t new_state,
                                      void* user_ctx)
{
    /* 현재 특별한 처리 불필요 (Master 상태 전이는 NMT가 자동 관리) */
    (void)old_state;
    (void)new_state;
    (void)user_ctx;
}

/**
 * @brief 나의 NMT 상태 변경 콜백
 *
 * @details
 * 나의 NMT가 상태 전이할 때 호출됩니다 (Master NMT 명령에 의해).
 * Device Driver의 on_my_state_changed 콜백으로 전달합니다.
 */
static void _OnMyNmtStateChanged(AGR_NMT_State_t old_state,
                                  AGR_NMT_State_t new_state,
                                  void* user_ctx)
{
    AGR_PnP_Slave_t* slave = (AGR_PnP_Slave_t*)user_ctx;
    if (slave == NULL) {
        return;
    }

    /* Device Driver 콜백 호출 */
    if (slave->callbacks.on_my_state_changed != NULL) {
        slave->callbacks.on_my_state_changed(old_state, new_state);
    }
}
