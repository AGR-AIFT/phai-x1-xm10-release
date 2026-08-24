/**
 ******************************************************************************
 * @file    emg_hub_drv.c
 * @author  HyundoKim
 * @brief   [Device Layer] XM10 ↔ EMG Hub 통신 드라이버 (Master, CANopen 표준)
 * @version 1.1
 * @date    2026-03-20
 *
 * @details
 * [V2 Architecture Compliance]
 * - PDO: Mutex + Snapshot 패턴 (timeout=0, non-blocking)
 * - SDO: Pre-Op SM에서 직접 처리 (Device Layer 범위)
 * - ISR: IOIF RxTask 콜백 체인 (Semaphore Give는 IOIF가 처리)
 *
 * [동기화 패턴] (Rule 13: comm-core-patterns)
 * ┌─────────────────────────────────────────────────────────┐
 * │ IOIF RxTask (RT4, callback)                             │
 * │   → EmgHub_Drv_ProcessCANMessage()                      │
 * │   → osMutexAcquire(s_pdo_mutex, 0) + memcpy            │
 * │   → osMutexRelease()                                    │
 * │                                                         │
 * │ User Task / core_process (RT6, IPO Input)               │
 * │   → EmgHub_Drv_GetRxData()                              │
 * │   → osMutexAcquire(s_pdo_mutex, 0) + memcpy (Snapshot) │
 * │   → osMutexRelease()                                    │
 * │   → Use snapshot without lock (no jitter)               │
 * └─────────────────────────────────────────────────────────┘
 *
 * [메시지 처리 흐름]
 * canfd_rx_handler.c:
 *     ├─ 0x70A → AGR_PnP_Master_ProcessMessage() [PnP Master]
 *     ├─ 0x58A → EmgHub_Drv_ProcessCANMessage() [여기]
 *     └─ 0x18A → EmgHub_Drv_ProcessCANMessage() [여기]
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "emg_hub_drv.h"
#include "agr_pnp_master.h"
#include "agr_dop_node_id.h"
#include "ioif_conf.h"       /* IOIF_FDCAN_ISR_DIRECT_ENABLE */
#include "Transport/CAN_FD/agr_dop_canfd.h"
#include "ioif_agrb_defs.h"
#include "ioif_agrb_tim.h"
#include <string.h>

/* FreeRTOS (Mutex) */
#include "cmsis_os2.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define EMGHUB_MAX_PDO_MAP_SIZE     64

/**
 * @brief Object Dictionary Index (EMG Hub Slave와 동일)
 *
 * [OD 구조]
 * 0x2100: ADC Status (RO, 1B)
 * 0x2110: TPDO1 Mapping (WO, BLOB)
 * 0x3000: PDO Metadata (4B)
 * 0x6010: EMG Sensor Data (SubIndex 0x60 = 10B processed set)
 */
#define EMGHUB_OD_IDX_PDO_MAPPING       0x2110
#define EMGHUB_OD_IDX_METADATA          0x3000
#define EMGHUB_OD_IDX_EMG_DATA          0x6010

/* EMG Data SubIndex */
#define EMGHUB_OD_SUBIDX_PROCESSED      0x60  /**< {raw,voltage,rms,envelope,mvc,active} 10B */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Pre-Operational 단계 (IMU Hub보다 간소화)
 * @details EMG Hub는 단일 TPDO만 사용하므로 PDO Map B 단계 없음
 */
typedef enum {
    EMGHUB_PRE_OP_IDLE = 0,
    EMGHUB_PRE_OP_SEND_PDO_MAP,
    EMGHUB_PRE_OP_WAIT_PDO_MAP,
    EMGHUB_PRE_OP_SEND_NMT_START,
    EMGHUB_PRE_OP_COMPLETE
} EmgHub_PreOpState_t;

typedef int (*EmgHub_PreOpAction_t)(void);

typedef struct {
    EmgHub_PreOpState_t send_state;
    EmgHub_PreOpState_t wait_state;
    EmgHub_PreOpAction_t action;
    uint32_t            timeout_ms;
    const char*         description;
} EmgHub_PreOpStep_t;

/**
 * @brief EMG Hub 드라이버 인스턴스
 */
typedef struct {
    AGR_DOP_Ctx_t       dop_ctx;
    AGR_TxFunc_t        tx_func;

    AGR_PnP_Master_t*   master_pnp;
    int                 slave_index;

    EmgHub_PreOpState_t pre_op_state;
    uint32_t            last_sdo_tx_time;
    uint8_t             sdo_retry_count;

    volatile bool       is_data_ready;
} EmgHub_DrvInst_t;

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES
 *-----------------------------------------------------------
 */

static EmgHub_DrvInst_t s_emg_hub_inst;

/**
 * @brief PDO 공유 데이터 + Mutex (V2 Architecture: Mutex + Snapshot)
 * @details
 * - Producer: IOIF RxTask → EmgHub_Drv_ProcessCANMessage() (osMutexAcquire, timeout=0)
 * - Consumer: core_process → EmgHub_Drv_GetRxData() (osMutexAcquire, timeout=0 → Snapshot)
 * - Critical Section: memcpy만 (≤ 1µs for 16B)
 */
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
static volatile uint32_t s_emg_pdo_seq = 0;
#else
static osMutexId_t      s_pdo_mutex = NULL;
#endif
static EmgHub_RxData_t  s_pdo_shared = {0};

/* Forward Declarations (Step Array Actions) */
static int _Step_SendPdoMap(void);
static int _Step_SendNmtStart(void);

static const EmgHub_PreOpStep_t s_pre_op_steps[] = {
    { EMGHUB_PRE_OP_SEND_PDO_MAP,   EMGHUB_PRE_OP_WAIT_PDO_MAP,   _Step_SendPdoMap,    5000, "TPDO1 Mapping" },
    { EMGHUB_PRE_OP_SEND_NMT_START, EMGHUB_PRE_OP_COMPLETE,        _Step_SendNmtStart,  5000, "NMT START" },
};
#define PRE_OP_STEP_COUNT (sizeof(s_pre_op_steps) / sizeof(s_pre_op_steps[0]))

/**
 * @brief PDO Mapping 데이터 (CANopen 표준 형식)
 * @details TPDO1: Metadata(4B) + EMG 0x6010.0x60 (10B) = 14B
 */
static const uint8_t s_pdo_map[] = {
    0x02,  /* Number of mapped objects = 2 */
    /* Entry 1: 0x3000.0x00 - Metadata (4B = 32 bits) */
    0x20, 0x00, 0x00, 0x30,
    /* Entry 2: 0x6010.0x60 - EMG Processed Set (10B = 80 bits) */
    0x50, 0x60, 0x10, 0x60,
};

/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/* PnP 콜백 */
static void _PnP_OnBootup(uint8_t slave_node_id);
static void _PnP_OnStateChanged(uint8_t slave_node_id, AGR_NMT_State_t old_state, AGR_NMT_State_t new_state);
static void _PnP_OnOnline(uint8_t slave_node_id);
static void _PnP_OnOffline(uint8_t slave_node_id);

/* SDO Response 처리 */
static void _OnSdoResponse(const AGR_SDO_Msg_t* response);

/* Pre-Op SM */
static void _RunPreOpStateMachine(void);

/* PDO 디코딩 */
static void _DecodeTpdo1(EmgHub_RxData_t* buf, const uint8_t* data, uint8_t len);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

int EmgHub_Drv_Init(AGR_TxFunc_t tx_func, AGR_PnP_Master_t* master_pnp)
{
    if (tx_func == NULL || master_pnp == NULL) {
        return -1;
    }

    memset(&s_emg_hub_inst, 0, sizeof(EmgHub_DrvInst_t));
    memset(&s_pdo_shared, 0, sizeof(EmgHub_RxData_t));

    s_emg_hub_inst.tx_func = tx_func;
    s_emg_hub_inst.master_pnp = master_pnp;
    s_emg_hub_inst.slave_index = -1;

#if !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V4.0] PDO Mutex 생성 */
    if (s_pdo_mutex == NULL) {
        const osMutexAttr_t mutex_attr = {
            .name = "EmgPdoMtx",
            .attr_bits = osMutexPrioInherit,
        };
        s_pdo_mutex = osMutexNew(&mutex_attr);
    }
#endif

    /* AGR_CANFD 초기화 (Master: OD 불필요) */
    AGR_CANFD_Init(&s_emg_hub_inst.dop_ctx,
                   NULL,
                   AGR_NODE_ID_XM,
                   tx_func);
    AGR_CANFD_SetTargetNodeId(&s_emg_hub_inst.dop_ctx, AGR_NODE_ID_EMG_HUB);

    /* PnP Master에 Slave 등록 */
    AGR_PnP_SlaveConfig_t slave_config = {
        .name = "EMG Hub",
        .node_id = AGR_NODE_ID_EMG_HUB,
        .heartbeat_timeout_ms = 3000,
        .callbacks = {
            .on_slave_bootup        = _PnP_OnBootup,
            .on_slave_state_changed = _PnP_OnStateChanged,
            .on_slave_online        = _PnP_OnOnline,
            .on_slave_offline       = _PnP_OnOffline,
        }
    };

    int idx = AGR_PnP_Master_AddSlave(master_pnp, &slave_config);
    if (idx < 0) {
        return -1;
    }
    s_emg_hub_inst.slave_index = idx;

    return 0;
}

/**
 * @brief CAN 메시지 처리 (TPDO/SDO만, Heartbeat 제외)
 * @details
 * [V2 Architecture] IOIF RxTask(RT4) 콜백 컨텍스트에서 실행.
 * PDO 수신 시 Mutex + memcpy로 공유 데이터 갱신 (timeout=0).
 * Mutex 획득 실패 시 이번 PDO는 Drop (다음 사이클에서 갱신).
 */
void EmgHub_Drv_ProcessCANMessage(uint16_t can_id, uint8_t* data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    uint16_t fnc_code = can_id & 0x780;

    /* SDO Response (0x580 + Node ID) */
    if (fnc_code == 0x580) {
        AGR_SDO_Msg_t sdo_msg;
        if (AGR_SDO_Decode(data, len, &sdo_msg) == 0) {
            _OnSdoResponse(&sdo_msg);
        }
        return;
    }

    /* TPDO1 (0x180 + Node ID = 0x18A) — Mutex + Snapshot 패턴 */
    if (fnc_code == 0x180) {
        EmgHub_RxData_t decoded;
        _DecodeTpdo1(&decoded, data, len);

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
        /* [V5.0] Seqlock Writer (ISR context) */
        s_emg_pdo_seq++;
        __DMB();
        memcpy(&s_pdo_shared, &decoded, sizeof(EmgHub_RxData_t));
        __DMB();
        s_emg_pdo_seq++;
        s_emg_hub_inst.is_data_ready = true;
#else
        if (osMutexAcquire(s_pdo_mutex, 1) == osOK) {
            memcpy(&s_pdo_shared, &decoded, sizeof(EmgHub_RxData_t));
            osMutexRelease(s_pdo_mutex);
            s_emg_hub_inst.is_data_ready = true;
        }
#endif
        return;
    }
}

bool EmgHub_Drv_IsConnected(void)
{
    return AGR_PnP_Master_IsSlaveOnline(s_emg_hub_inst.master_pnp,
                                         AGR_NODE_ID_EMG_HUB);
}

AGR_NMT_State_t EmgHub_Drv_GetNmtState(void)
{
    return AGR_PnP_Master_GetSlaveState(s_emg_hub_inst.master_pnp,
                                         AGR_NODE_ID_EMG_HUB);
}

void EmgHub_Drv_RunPeriodic(void)
{
    uint32_t current_ms = IOIF_TIM_GetTick();

    /* 1. Pre-Op Timeout 체크 (SDO ACK 대기 상태) */
    if (s_emg_hub_inst.pre_op_state == EMGHUB_PRE_OP_WAIT_PDO_MAP) {
        if (current_ms - s_emg_hub_inst.last_sdo_tx_time > 5000) {
            if (s_emg_hub_inst.sdo_retry_count < 3) {
                s_emg_hub_inst.sdo_retry_count++;
                s_emg_hub_inst.pre_op_state = EMGHUB_PRE_OP_SEND_PDO_MAP;
            } else {
                s_emg_hub_inst.pre_op_state = EMGHUB_PRE_OP_IDLE;
                s_emg_hub_inst.sdo_retry_count = 0;
            }
        }
    }

    /* 2. Pre-Op State Machine 구동 */
    _RunPreOpStateMachine();
}

/**
 * @brief 최신 EMG 데이터 읽기 (Mutex + Snapshot, V2 Architecture)
 * @details
 * [V2 Mutex + Snapshot Pattern]
 * - osMutexAcquire(timeout=0): Non-blocking
 * - 성공 시: memcpy로 Snapshot 생성 → Mutex 즉시 해제
 * - 실패 시: 이전 Snapshot 재사용 (Graceful Degradation)
 * - Snapshot 이후: Lock 없이 안전하게 사용 (no jitter)
 */
bool EmgHub_Drv_GetRxData(EmgHub_RxData_t* rx_data)
{
    if (rx_data == NULL) {
        return false;
    }

    if (!s_emg_hub_inst.is_data_ready) {
        return false;
    }

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Seqlock Reader */
    uint32_t seq1, seq2;
    do {
        seq1 = s_emg_pdo_seq;
        __DMB();
        memcpy(rx_data, &s_pdo_shared, sizeof(EmgHub_RxData_t));
        __DMB();
        seq2 = s_emg_pdo_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    return true;
#else
    if (osMutexAcquire(s_pdo_mutex, 0) == osOK) {
        memcpy(rx_data, &s_pdo_shared, sizeof(EmgHub_RxData_t));
        osMutexRelease(s_pdo_mutex);
        return true;
    }
    return false;
#endif
}

bool EmgHub_Drv_IsDataReady(void)
{
    return s_emg_hub_inst.is_data_ready;
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — PnP Callbacks
 *-----------------------------------------------------------
 */

static void _PnP_OnBootup(uint8_t slave_node_id)
{
    (void)slave_node_id;

    /* Boot-up 수신 → Pre-Op SM 시작 (IDLE 상태에서만)
     * COMPLETE: NMT START 전송 직후 — OPERATIONAL 전환 대기 중이므로 무시
     * 진행 중: 중복 Boot-up 방지 (FIX-6B) */
    if (s_emg_hub_inst.pre_op_state == EMGHUB_PRE_OP_IDLE) {
        s_emg_hub_inst.pre_op_state = EMGHUB_PRE_OP_SEND_PDO_MAP;
        s_emg_hub_inst.sdo_retry_count = 0;
    }
}

static void _PnP_OnStateChanged(uint8_t slave_node_id, AGR_NMT_State_t old_state, AGR_NMT_State_t new_state)
{
    (void)slave_node_id;
    (void)old_state;
    (void)new_state;
}

static void _PnP_OnOnline(uint8_t slave_node_id)
{
    (void)slave_node_id;
}

static void _PnP_OnOffline(uint8_t slave_node_id)
{
    (void)slave_node_id;

    s_emg_hub_inst.pre_op_state = EMGHUB_PRE_OP_IDLE;
    s_emg_hub_inst.sdo_retry_count = 0;
    s_emg_hub_inst.is_data_ready = false;
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — SDO Response
 *-----------------------------------------------------------
 */

static void _OnSdoResponse(const AGR_SDO_Msg_t* response)
{
    if (response == NULL) return;

    if (response->index == EMGHUB_OD_IDX_PDO_MAPPING) {
        if (s_emg_hub_inst.pre_op_state == EMGHUB_PRE_OP_WAIT_PDO_MAP) {
            s_emg_hub_inst.pre_op_state = EMGHUB_PRE_OP_SEND_NMT_START;
            s_emg_hub_inst.sdo_retry_count = 0;
        }
    }
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — Pre-Op State Machine
 *-----------------------------------------------------------
 */

static void _RunPreOpStateMachine(void)
{
    /* Boot-up Recovery: IDLE 상태에서 Slave가 PRE_OP이면 SM 시작
     * → Boot-up 콜백을 놓쳤을 때 복구 (IMU Hub 동일 패턴) */
    if (s_emg_hub_inst.pre_op_state == EMGHUB_PRE_OP_IDLE) {
        AGR_NMT_State_t slave_state = AGR_PnP_Master_GetSlaveState(
            s_emg_hub_inst.master_pnp, AGR_NODE_ID_EMG_HUB);
        if (slave_state == AGR_NMT_PRE_OPERATIONAL) {
            s_emg_hub_inst.pre_op_state = EMGHUB_PRE_OP_SEND_PDO_MAP;
            s_emg_hub_inst.sdo_retry_count = 0;
        }
        return;
    }

    if (s_emg_hub_inst.pre_op_state == EMGHUB_PRE_OP_COMPLETE) {
        return;
    }

    for (uint8_t i = 0; i < PRE_OP_STEP_COUNT; i++) {
        if (s_emg_hub_inst.pre_op_state == s_pre_op_steps[i].send_state) {
            if (s_pre_op_steps[i].action() == 0) {
                s_emg_hub_inst.last_sdo_tx_time = IOIF_TIM_GetTick();
                s_emg_hub_inst.pre_op_state = s_pre_op_steps[i].wait_state;
            }
            return;
        }
    }
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — Pre-Op Step Actions
 *-----------------------------------------------------------
 */

static int _Step_SendPdoMap(void)
{
    return AGR_CANFD_SendSDOWrite(&s_emg_hub_inst.dop_ctx,
                                  EMGHUB_OD_IDX_PDO_MAPPING, 0x00,
                                  s_pdo_map, sizeof(s_pdo_map));
}

static int _Step_SendNmtStart(void)
{
    int ret = AGR_PnP_Master_SendNmt(s_emg_hub_inst.master_pnp,
                                      AGR_NODE_ID_EMG_HUB,
                                      AGR_NMT_CMD_START);
    if (ret == 0) {
        s_emg_hub_inst.pre_op_state = EMGHUB_PRE_OP_COMPLETE;
    }
    return ret;
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — PDO Decoding
 *-----------------------------------------------------------
 */

/**
 * @brief TPDO1 데이터 디코딩 (Metadata + EMG Processed Set)
 * @details
 * Byte Layout (14B total):
 *   [0]   timestamp_low
 *   [1]   timestamp_mid
 *   [2]   timestamp_high
 *   [3]   status_flags
 *   [4-5] raw_adc (uint16, LE)
 *   [6-7] voltage_uv_x10 (int16, LE)
 *   [8-9] rms_uv_x10 (int16, LE)
 *   [10-11] envelope_uv_x10 (int16, LE)
 *   [12]  mvc_percent (uint8)
 *   [13]  is_active (uint8)
 */
static void _DecodeTpdo1(EmgHub_RxData_t* buf, const uint8_t* data, uint8_t len)
{
    if (len < 14) return;

    /* Metadata (4B) */
    buf->timestamp = (uint32_t)data[0]
                   | ((uint32_t)data[1] << 8)
                   | ((uint32_t)data[2] << 16);
    buf->status_flags = data[3];

    /* EMG Data (10B, Little-Endian) */
    buf->raw_adc         = (uint16_t)data[4]  | ((uint16_t)data[5] << 8);
    buf->voltage_uv_x10  = (int16_t)((uint16_t)data[6]  | ((uint16_t)data[7] << 8));
    buf->rms_uv_x10      = (int16_t)((uint16_t)data[8]  | ((uint16_t)data[9] << 8));
    buf->envelope_uv_x10 = (int16_t)((uint16_t)data[10] | ((uint16_t)data[11] << 8));
    buf->mvc_percent      = data[12];
    buf->is_active        = data[13];
}
