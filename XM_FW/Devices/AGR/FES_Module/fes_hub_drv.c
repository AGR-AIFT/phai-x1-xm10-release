/**
 ******************************************************************************
 * @file    fes_hub_drv.c
 * @author  HyundoKim
 * @brief   [Device Layer] XM10 ↔ FES Hub 통신 드라이버 (Master, CANopen 표준)
 * @version 1.0
 * @date    2026-03-21
 *
 * @details
 * IMU Hub 드라이버(imu_hub_drv.c V4.0) 패턴을 기반으로 FES Hub에 맞게 구현.
 *
 * [IMU Hub 대비 차이점]
 * - TPDO1만 사용 (IMU Hub: TPDO1 + TPDO2 alternating)
 * - RPDO1 전송 기능 추가 (Master → Slave 명령, IMU Hub에는 없음)
 * - 16B 고정 페이로드 (IMU Hub: 64B PDO Mapping)
 * - Pre-Op: TPDO1 Mapping + RPDO1 Mapping + NMT START (IMU Hub: TPDO1+TPDO2+IMU Mask+NMT)
 *
 * [데이터 흐름]
 * - TPDO Rx: ISR → Double Buffer → Main Loop GetRxData()
 * - RPDO Tx: Application → SendRPDO1() → FDCAN Tx
 * - SDO: Pre-Op PDO Mapping 설정용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "fes_hub_drv.h"
#include "agr_pnp_master.h"
#include "agr_dop_node_id.h"
#include "ioif_agrb_defs.h"
#include "ioif_agrb_tim.h"
#include <string.h>

#include "cmsis_os2.h"
#include "ioif_conf.h"  /* IOIF_FDCAN_ISR_DIRECT_ENABLE */

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief OD Index (FES Hub Slave와 동일, SDO 통신용)
 *
 * [FES Hub OD 구조]
 * - 0x1A00: TPDO1 Mapping
 * - 0x1600: RPDO1 Mapping
 * - 0x7010: FES Command (RPDO 데이터)
 * - 0x7018: FES Feedback (TPDO 데이터)
 */
#define FESHUB_OD_IDX_TPDO1_MAPPING     0x1A00
#define FESHUB_OD_IDX_RPDO1_MAPPING     0x1600

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Pre-Op 상태 (Step Array 패턴)
 */
typedef enum {
    FESHUB_PRE_OP_IDLE = 0,
    FESHUB_PRE_OP_SEND_TPDO_MAP,
    FESHUB_PRE_OP_WAIT_TPDO_MAP,
    FESHUB_PRE_OP_SEND_RPDO_MAP,
    FESHUB_PRE_OP_WAIT_RPDO_MAP,
    FESHUB_PRE_OP_SEND_NMT_START,
    FESHUB_PRE_OP_COMPLETE,
} FesHub_PreOpState_t;

typedef int (*FesHub_PreOpAction_t)(void);

typedef struct {
    FesHub_PreOpState_t send_state;
    FesHub_PreOpState_t wait_state;
    FesHub_PreOpAction_t action;
    uint32_t            timeout_ms;
    const char*         description;
} FesHub_PreOpStep_t;

/**
 * @brief FES Hub 드라이버 인스턴스
 */
typedef struct {
    AGR_DOP_Ctx_t       dop_ctx;
    AGR_TxFunc_t        tx_func;

    /* PnP Master (System Layer) */
    AGR_PnP_Master_t*   master_pnp;
    int                 slave_index;

    /* Pre-Op State Machine */
    FesHub_PreOpState_t pre_op_state;
    uint32_t            last_sdo_tx_time;
    uint8_t             sdo_retry_count;

    /* Rx Data (TPDO1: FES Hub → XM) */
    volatile bool       is_data_ready;
} FesHub_DrvInst_t;

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES
 *-----------------------------------------------------------
 */

static FesHub_DrvInst_t s_fes_hub_inst;

/* Forward declarations */
static int _Step_SendTpdoMap(void);
static int _Step_SendRpdoMap(void);
static int _Step_SendNmtStart(void);

/**
 * @brief Pre-Op Step Array
 */
static const FesHub_PreOpStep_t s_pre_op_steps[] = {
    { FESHUB_PRE_OP_SEND_TPDO_MAP,  FESHUB_PRE_OP_WAIT_TPDO_MAP,  _Step_SendTpdoMap,   5000, "TPDO1 Mapping" },
    { FESHUB_PRE_OP_SEND_RPDO_MAP,  FESHUB_PRE_OP_WAIT_RPDO_MAP,  _Step_SendRpdoMap,   5000, "RPDO1 Mapping" },
    { FESHUB_PRE_OP_SEND_NMT_START, FESHUB_PRE_OP_COMPLETE,        _Step_SendNmtStart,  5000, "NMT START" },
};

#define PRE_OP_STEP_COUNT (sizeof(s_pre_op_steps) / sizeof(s_pre_op_steps[0]))

/**
 * @brief TPDO1 Mapping (FES Feedback → XM)
 *
 * TPDO1 (0x18A, 16B):
 * - 0x7018.0x00: FES Feedback BLOB (16B = 128 bits)
 */
static const uint8_t s_tpdo_map[] = {
    0x01,  /* Number of mapped objects = 1 */
    /* Entry 1: 0x7018.0x00 - FES Feedback (16B = 128 bits) */
    0x80, 0x00, 0x18, 0x70,
};

/**
 * @brief RPDO1 Mapping (XM → FES Command)
 *
 * RPDO1 (0x20A, 14B):
 * - 0x7010.0x00: FES Command BLOB (14B = 112 bits)
 */
static const uint8_t s_rpdo_map[] = {
    0x01,  /* Number of mapped objects = 1 */
    /* Entry 1: 0x7010.0x00 - FES Command (14B = 112 bits) */
    0x70, 0x00, 0x10, 0x70,
};

/**
 * @brief TPDO1 Raw 페이로드 (디코딩 중간 구조)
 */
typedef struct __attribute__((packed)) {
    uint8_t  ch_state[FESHUB_CH_COUNT];
    int16_t  ch_current_x10[FESHUB_CH_COUNT];
    uint8_t  ch_fault_code[FESHUB_CH_COUNT];
    uint16_t hv_voltage_x100;
    uint8_t  digipot_pos;
    uint8_t  reserved;
    uint8_t  timestamp_low;
    uint8_t  timestamp_mid;
    uint8_t  timestamp_high;
    uint8_t  error_register;
} FesHub_TpdoRaw_t;  /* 16 bytes */

/**
 * @brief PDO 공유 데이터 + Mutex (V2 Architecture: Mutex + Snapshot)
 * @details
 * - Producer: IOIF RxTask → FesHub_Drv_ProcessCANMessage() (osMutexAcquire, timeout=0)
 * - Consumer: core_process → FesHub_Drv_GetRxData() (osMutexAcquire, timeout=0 → Snapshot)
 * - Critical Section: memcpy만 (≤ 1µs for 16B)
 */
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
static volatile uint32_t s_fes_pdo_seq = 0;
#else
static osMutexId_t      s_pdo_mutex = NULL;
#endif
static FesHub_RxData_t  s_pdo_shared = {0};

/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/* PnP Callbacks */
static void _PnP_OnBootup(uint8_t slave_node_id);
static void _PnP_OnStateChanged(uint8_t slave_node_id, AGR_NMT_State_t old_state, AGR_NMT_State_t new_state);
static void _PnP_OnOnline(uint8_t slave_node_id);
static void _PnP_OnOffline(uint8_t slave_node_id);

/* SDO Response */
static void _OnSdoResponse(const AGR_SDO_Msg_t* response);

/* Pre-Op State Machine */
static void _RunPreOpStateMachine(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

int FesHub_Drv_Init(AGR_TxFunc_t tx_func, AGR_PnP_Master_t* master_pnp)
{
    if (tx_func == NULL || master_pnp == NULL) {
        return -1;
    }

    memset(&s_fes_hub_inst, 0, sizeof(FesHub_DrvInst_t));
    memset(&s_pdo_shared, 0, sizeof(FesHub_RxData_t));

#if !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    if (s_pdo_mutex == NULL) {
        const osMutexAttr_t mutex_attr = {
            .name = "FesPdoMtx",
            .attr_bits = osMutexPrioInherit,
        };
        s_pdo_mutex = osMutexNew(&mutex_attr);
    }
#endif

    s_fes_hub_inst.tx_func = tx_func;
    s_fes_hub_inst.master_pnp = master_pnp;
    s_fes_hub_inst.slave_index = -1;

    /* DOP 초기화 (Master: OD 불필요) */
    AGR_CANFD_Init(&s_fes_hub_inst.dop_ctx,
                   NULL,
                   AGR_NODE_ID_XM,
                   tx_func);
    AGR_CANFD_SetTargetNodeId(&s_fes_hub_inst.dop_ctx, AGR_NODE_ID_FES_HUB);

    /* PnP Master에 Slave 등록 */
    AGR_PnP_SlaveConfig_t slave_config = {
        .name = "FES Hub",
        .node_id = AGR_NODE_ID_FES_HUB,
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
    s_fes_hub_inst.slave_index = idx;

    return 0;
}

void FesHub_Drv_ProcessCANMessage(uint16_t can_id, uint8_t* data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return;
    }

    uint16_t fnc_code = can_id & 0x780;

    /* 1. SDO Response (0x580 + Node ID) */
    if (fnc_code == 0x580) {
        AGR_SDO_Msg_t sdo_msg;
        if (AGR_SDO_Decode(data, len, &sdo_msg) == 0) {
            _OnSdoResponse(&sdo_msg);
        }
        return;
    }

    /* 2. TPDO1 (0x180 + Node ID) — FES Feedback, 16B — Mutex + Snapshot 패턴 */
    if (fnc_code == 0x180) {
        if (len < sizeof(FesHub_TpdoRaw_t)) {
            return;
        }

        {
            const FesHub_TpdoRaw_t* raw = (const FesHub_TpdoRaw_t*)data;
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
            /* [V5.0] Seqlock Writer (ISR context) */
            s_fes_pdo_seq++;
            __DMB();
#else
        if (osMutexAcquire(s_pdo_mutex, 1) == osOK) {
#endif
            for (int i = 0; i < FESHUB_CH_COUNT; i++) {
                s_pdo_shared.ch_state[i] = (FesHub_ChState_t)raw->ch_state[i];
                s_pdo_shared.ch_current_mA[i] = (float)raw->ch_current_x10[i] / FESHUB_SCALE_AMPLITUDE;
                s_pdo_shared.ch_fault_code[i] = raw->ch_fault_code[i];
            }
            s_pdo_shared.hv_voltage_V = (float)raw->hv_voltage_x100 / FESHUB_SCALE_VOLTAGE;
            s_pdo_shared.digipot_pos = raw->digipot_pos;
            s_pdo_shared.timestamp = (uint32_t)raw->timestamp_low |
                                     ((uint32_t)raw->timestamp_mid << 8) |
                                     ((uint32_t)raw->timestamp_high << 16);
            s_pdo_shared.error_register = raw->error_register;

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
            __DMB();
            s_fes_pdo_seq++;
            s_fes_hub_inst.is_data_ready = true;
#else
            osMutexRelease(s_pdo_mutex);
            s_fes_hub_inst.is_data_ready = true;
        }
#endif
        }
        return;
    }
}

/**
 * @brief 최신 FES 데이터 읽기 (Mutex + Snapshot, V2 Architecture)
 * @details
 * - osMutexAcquire(timeout=0): Non-blocking
 * - 성공 시: memcpy로 Snapshot 생성 → Mutex 즉시 해제
 * - 실패 시: 이전 Snapshot 재사용 (Graceful Degradation)
 */
bool FesHub_Drv_GetRxData(FesHub_RxData_t* rx_data)
{
    if (rx_data == NULL) {
        return false;
    }

    if (!s_fes_hub_inst.is_data_ready) {
        return false;
    }

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Seqlock Reader */
    uint32_t seq1, seq2;
    do {
        seq1 = s_fes_pdo_seq;
        __DMB();
        memcpy(rx_data, &s_pdo_shared, sizeof(FesHub_RxData_t));
        __DMB();
        seq2 = s_fes_pdo_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    return true;
#else
    if (osMutexAcquire(s_pdo_mutex, 0) == osOK) {
        memcpy(rx_data, &s_pdo_shared, sizeof(FesHub_RxData_t));
        osMutexRelease(s_pdo_mutex);
        return true;
    }
    return false;
#endif
}

bool FesHub_Drv_IsDataReady(void)
{
    return s_fes_hub_inst.is_data_ready;
}

bool FesHub_Drv_IsConnected(void)
{
    return AGR_PnP_Master_IsSlaveOnline(s_fes_hub_inst.master_pnp,
                                         AGR_NODE_ID_FES_HUB);
}

AGR_NMT_State_t FesHub_Drv_GetNmtState(void)
{
    return AGR_PnP_Master_GetSlaveState(s_fes_hub_inst.master_pnp,
                                         AGR_NODE_ID_FES_HUB);
}

int FesHub_Drv_SendRPDO1(const FesHub_RpdoPayload_t* payload)
{
    if (payload == NULL || s_fes_hub_inst.tx_func == NULL) {
        return -1;
    }

    /* RPDO1 CAN ID = 0x200 + FES Hub Node ID */
    uint32_t can_id = 0x200 + AGR_NODE_ID_FES_HUB;
    return s_fes_hub_inst.tx_func(can_id, (const uint8_t*)payload,
                                   sizeof(FesHub_RpdoPayload_t));
}

void FesHub_Drv_RunPeriodic(void)
{
    uint32_t current_ms = IOIF_TIM_GetTick();

    /* 1. Pre-Op Timeout 체크 (SDO ACK 대기) */
    if (s_fes_hub_inst.pre_op_state == FESHUB_PRE_OP_WAIT_TPDO_MAP ||
        s_fes_hub_inst.pre_op_state == FESHUB_PRE_OP_WAIT_RPDO_MAP) {

        if (current_ms - s_fes_hub_inst.last_sdo_tx_time > 5000) {
            if (s_fes_hub_inst.sdo_retry_count < 3) {
                s_fes_hub_inst.sdo_retry_count++;
                /* WAIT → SEND (재시도) */
                if (s_fes_hub_inst.pre_op_state == FESHUB_PRE_OP_WAIT_TPDO_MAP) {
                    s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_SEND_TPDO_MAP;
                } else {
                    s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_SEND_RPDO_MAP;
                }
            } else {
                /* 최대 재시도 → IDLE */
                s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_IDLE;
                s_fes_hub_inst.sdo_retry_count = 0;
            }
        }
    }

    /* 2. Pre-Op State Machine */
    _RunPreOpStateMachine();
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — PnP Callbacks
 *-----------------------------------------------------------
 */

static void _PnP_OnBootup(uint8_t slave_node_id)
{
    (void)slave_node_id;

    /* IDLE 상태에서만 Pre-Op 시작 허용 (COMPLETE에서 재진입 방지) */
    if (s_fes_hub_inst.pre_op_state == FESHUB_PRE_OP_IDLE) {
        s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_SEND_TPDO_MAP;
        s_fes_hub_inst.sdo_retry_count = 0;
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
    s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_IDLE;
    s_fes_hub_inst.sdo_retry_count = 0;
}

static void _PnP_OnOffline(uint8_t slave_node_id)
{
    (void)slave_node_id;
    s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_IDLE;
    s_fes_hub_inst.sdo_retry_count = 0;
    s_fes_hub_inst.is_data_ready = false;
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — Pre-Op State Machine
 *-----------------------------------------------------------
 */

static void _RunPreOpStateMachine(void)
{
    /* IDLE 상태 + Slave PRE_OP → Pre-Op 시작 */
    if (s_fes_hub_inst.pre_op_state == FESHUB_PRE_OP_IDLE) {
        AGR_NMT_State_t slave_state = AGR_PnP_Master_GetSlaveState(
            s_fes_hub_inst.master_pnp, AGR_NODE_ID_FES_HUB);
        if (slave_state == AGR_NMT_PRE_OPERATIONAL) {
            s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_SEND_TPDO_MAP;
            s_fes_hub_inst.sdo_retry_count = 0;
        }
        return;
    }

    if (s_fes_hub_inst.pre_op_state == FESHUB_PRE_OP_COMPLETE) {
        return;
    }

    /* Step Array 기반 실행 */
    for (uint8_t i = 0; i < PRE_OP_STEP_COUNT; i++) {
        const FesHub_PreOpStep_t* step = &s_pre_op_steps[i];

        if (s_fes_hub_inst.pre_op_state == step->send_state) {
            int result = step->action();
            if (result >= 0) {
                s_fes_hub_inst.pre_op_state = step->wait_state;
                s_fes_hub_inst.last_sdo_tx_time = IOIF_TIM_GetTick();
                s_fes_hub_inst.sdo_retry_count = 0;
            }
            return;
        }

        if (s_fes_hub_inst.pre_op_state == step->wait_state) {
            return;
        }
    }
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — Step Array Actions
 *-----------------------------------------------------------
 */

static int _Step_SendTpdoMap(void)
{
    return AGR_CANFD_SendSDOWrite(&s_fes_hub_inst.dop_ctx,
                                FESHUB_OD_IDX_TPDO1_MAPPING, 0,
                                s_tpdo_map, sizeof(s_tpdo_map));
}

static int _Step_SendRpdoMap(void)
{
    return AGR_CANFD_SendSDOWrite(&s_fes_hub_inst.dop_ctx,
                                FESHUB_OD_IDX_RPDO1_MAPPING, 0,
                                s_rpdo_map, sizeof(s_rpdo_map));
}

static int _Step_SendNmtStart(void)
{
    return AGR_PnP_Master_SendNmt(s_fes_hub_inst.master_pnp,
                                   AGR_NODE_ID_FES_HUB,
                                   AGR_NMT_CMD_START);
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — SDO Response
 *-----------------------------------------------------------
 */

static void _OnSdoResponse(const AGR_SDO_Msg_t* response)
{
    uint8_t cs = response->cs & 0xE0;

    /* SDO Download Response (ACK) */
    if (cs == AGR_SDO_CS_DOWNLOAD_INIT_RSP) {
        if (response->index == FESHUB_OD_IDX_TPDO1_MAPPING) {
            /* TPDO Mapping 완료 → RPDO Mapping */
            s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_SEND_RPDO_MAP;
            s_fes_hub_inst.sdo_retry_count = 0;
        }
        else if (response->index == FESHUB_OD_IDX_RPDO1_MAPPING) {
            /* RPDO Mapping 완료 → NMT START */
            s_fes_hub_inst.pre_op_state = FESHUB_PRE_OP_SEND_NMT_START;
            s_fes_hub_inst.sdo_retry_count = 0;
        }
    }
}
