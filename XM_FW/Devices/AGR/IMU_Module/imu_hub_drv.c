/**
 ******************************************************************************
 * @file    imu_hub_drv.c
 * @author  HyundoKim
 * @brief   [Device Layer] XM10 ↔ IMU Hub 통신 드라이버 (Master, CANopen 표준)
 * @version 4.0 (PnP V2 리팩토링 - 새 AGR_PnP_Master API)
 * @date    2026-02-10
 *
 * @details
 * [V4.0 변경사항 - PnP V2 리팩토링] (.cursorrules Phase 2)
 * - AS-IS: AGR_PnP_Inst_t (구 API), 0x700 자체 처리, _PnP_RunPreOp 콜백
 * - TO-BE: AGR_PnP_Master_t (신 API), 0x700 PnP Master 처리, RunPeriodic 직접 구동
 *
 * [콜백 1:1 대조 (기능 보존)]
 * | 기존 (V3.0)                    | 신규 (V4.0)                     | 동작      |
 * |--------------------------------|---------------------------------|-----------|
 * | _PnP_OnBootup(node_id)         | on_slave_bootup(node_id)        | Pre-Op 시작 |
 * | _PnP_OnNmtChange(node_id,..)   | on_slave_state_changed(node_id) | 로그      |
 * | _PnP_OnConnected(node_id)      | on_slave_online(node_id)        | Pre-Op 완료 |
 * | _PnP_OnDisconnected(node_id)   | on_slave_offline(node_id)       | Pre-Op 초기화 |
 * | _PnP_RunPreOp(node_id, inst)   | RunPeriodic()에서 직접 실행      | Pre-Op SM |
 * | 0x700 ProcessCANMessage         | PnP Master가 처리               | NMT 추적   |
 * 
 * [메시지 처리 흐름 (V4.0)]
 * canfd_rx_handler.c:
 *     ├─ 0x700 → AGR_PnP_Master_ProcessMessage() [PnP Master]
 *     ├─ 0x580 → ImuHub_Drv_ProcessCANMessage() [여기]
 *     └─ 0x180/0x280 → ImuHub_Drv_ProcessCANMessage() [여기]
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "imu_hub_drv.h"
#include "agr_pnp_master.h"  /* ✅ V4.0: 새 AGR_PnP_Master API */
#include "agr_dop_node_id.h"
#include "ioif_agrb_defs.h"  /* ✅ RTOS/BareMetal 자동 감지 */
#include "ioif_agrb_tim.h"   /* IOIF_TIM_GetTick() */
#include "ioif_conf.h"       /* IOIF_FDCAN_ISR_DIRECT_ENABLE */
#include <string.h>

/* V2 Mutex + Snapshot Pattern: CMSIS-OS2 (timeout=0, non-blocking) */
#include "cmsis_os2.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define IMUHUB_MAX_PDO_MAP_SIZE     64  /**< PDO Mapping 최대 크기 */

/**
 * @brief Object Dictionary Index (IMU Hub Slave와 동일)
 * @details XM10 (Master)이 IMU Hub (Slave)와 SDO 통신 시 사용하는 OD Index
 * 
 * [CANopen 표준 OD 영역 구분 (CiA 301)]
 * ┌────────────────────────────────────────────────────────┐
 * │ 0x0000 ~ 0x0FFF: Data Type Definitions (표준)          │
 * │ 0x1000 ~ 0x1FFF: Communication Profile (표준)          │
 * │ 0x2000 ~ 0x5FFF: Manufacturer Specific (제조사 정의) ✅ │
 * │ 0x6000 ~ 0x9FFF: Device Profile (CiA 401, 402 등)      │
 * │ 0xA000 ~ 0xFFFF: Reserved                              │
 * └────────────────────────────────────────────────────────┘
 * 
 * [중요] CANopen 메시지 타입별 처리
 * ┌─────────────────────────────────────────────────────────┐
 * │ NMT, Boot-up, Heartbeat → 별도 CAN ID (SDO 아님)        │
 * │ - CAN ID 0x000: NMT Command                             │
 * │ - CAN ID 0x700 + Node ID: Boot-up, Heartbeat           │
 * │ → AGR_NMT_ProcessMessage()가 처리 ✅                     │
 * │                                                         │
 * │ SDO, PDO → OD Index를 통해 처리                          │
 * │ - CAN ID 0x580 + Node ID: SDO Response                 │
 * │ - CAN ID 0x180/0x280 + Node ID: PDO                    │
 * │ → AGR_CANFD_ProcessRxMessage()가 처리                    │
 * └─────────────────────────────────────────────────────────┘
 */

/* ===== 0x2000 ~ 0x5FFF: Manufacturer Specific (Angel Robotics) ===== */

/* 센서 상태 (Read Only) */
#define IMUHUB_OD_IDX_IMU_CONN_MASK     0x2000  /**< IMU Connected Mask (RO, 1B) - bit0~5: IMU0~5 */

/* TPDO Mapping Parameter (CiA 301 표준: 0x1A00~0x1BFF) */
#define IMUHUB_OD_IDX_PDO_MAPPING_A     0x1A00  /**< TPDO1 Mapping (Group A: Metadata + IMU 0,1,2) */
#define IMUHUB_OD_IDX_PDO_MAPPING_B     0x1A01  /**< TPDO2 Mapping (Group B: Metadata + IMU 3,4,5) */

/* PDO Metadata */
#define IMUHUB_OD_IDX_METADATA          0x3000  /**< PDO Metadata (4B BLOB): Timestamp(3B) + Valid Mask(1B) */

/** @brief IMU Data OD Index (0x6000~0x6005) */
#define IMUHUB_OD_IDX_IMU_BASE          0x6000

/* SubIndex 정의 (IMU Hub xm_drv.h와 동일) */
#define IMUHUB_OD_SUBIDX_QUAT_W         0x00
#define IMUHUB_OD_SUBIDX_QUAT_X         0x01
#define IMUHUB_OD_SUBIDX_QUAT_Y         0x02
#define IMUHUB_OD_SUBIDX_QUAT_Z         0x03

#define IMUHUB_OD_SUBIDX_EULER_ROLL     0x10
#define IMUHUB_OD_SUBIDX_EULER_PITCH    0x11
#define IMUHUB_OD_SUBIDX_EULER_YAW      0x12

#define IMUHUB_OD_SUBIDX_ACC_X          0x20
#define IMUHUB_OD_SUBIDX_ACC_Y          0x21
#define IMUHUB_OD_SUBIDX_ACC_Z          0x22

#define IMUHUB_OD_SUBIDX_GYR_X          0x30
#define IMUHUB_OD_SUBIDX_GYR_Y          0x31
#define IMUHUB_OD_SUBIDX_GYR_Z          0x32

#define IMUHUB_OD_SUBIDX_MAG_X          0x40
#define IMUHUB_OD_SUBIDX_MAG_Y          0x41
#define IMUHUB_OD_SUBIDX_MAG_Z          0x42

#define IMUHUB_OD_SUBIDX_QUAT_ARRAY     0x50  // q[4] (8B)
#define IMUHUB_OD_SUBIDX_EULER_ARRAY    0x51  // rpy[3] (6B)
#define IMUHUB_OD_SUBIDX_ACC_ARRAY      0x52  // a[3] (6B)
#define IMUHUB_OD_SUBIDX_GYR_ARRAY      0x53  // g[3] (6B)
#define IMUHUB_OD_SUBIDX_MAG_ARRAY      0x54  // m[3] (6B)

#define IMUHUB_OD_SUBIDX_QAG            0x60  // {q,a,g} (20B)
#define IMUHUB_OD_SUBIDX_QAGM           0x61  // {q,a,g,m} (26B)
#define IMUHUB_OD_SUBIDX_FULL           0x62  // {q,rpy,a,g,m} (32B)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Pre-Operational 단계 (Device Layer 전용)
 * @details Master Pre-Op 시퀀스 상태 관리
 */
typedef enum {
    IMUHUB_PRE_OP_IDLE = 0,          /**< 대기 (Boot-up 전 또는 완료 후) */
    IMUHUB_PRE_OP_SEND_PDO_MAP_A,    /**< TPDO1 Mapping 전송 */
    IMUHUB_PRE_OP_WAIT_PDO_MAP_A,    /**< TPDO1 Mapping ACK 대기 */
    IMUHUB_PRE_OP_SEND_PDO_MAP_B,    /**< TPDO2 Mapping 전송 */
    IMUHUB_PRE_OP_WAIT_PDO_MAP_B,    /**< TPDO2 Mapping ACK 대기 */
    IMUHUB_PRE_OP_SEND_IMU_MASK_REQ, /**< IMU Connected Mask 조회 (Optional) */
    IMUHUB_PRE_OP_WAIT_IMU_MASK_RSP, /**< IMU Connected Mask 응답 대기 */
    IMUHUB_PRE_OP_SEND_NMT_START,    /**< NMT START 전송 */
    IMUHUB_PRE_OP_COMPLETE           /**< Pre-Op 완료 (OPERATIONAL 대기) */
} ImuHub_PreOpState_t;

/**
 * @brief Pre-Op Step Action 함수 타입 (Step Array 패턴)
 * @return 0: 성공 (다음 단계), -1: 실패 또는 대기
 * 
 * [V4.0 변경] AGR_PnP_Inst_t* 파라미터 제거
 * - 이유: 새 API에서는 AGR_PnP_Master_SendNmt()를 직접 호출
 * - SDO 전송은 AGR_DOP API 직접 사용
 */
typedef int (*ImuHub_PreOpAction_t)(void);

/**
 * @brief Pre-Op Step 정의 (Step Array 패턴)
 * @details 선언적 Pre-Op 시퀀스 정의 (향후 확장성)
 */
typedef struct {
    ImuHub_PreOpState_t send_state;     /**< 전송 상태 */
    ImuHub_PreOpState_t wait_state;     /**< 대기 상태 */
    ImuHub_PreOpAction_t action;        /**< 실행 함수 */
    uint32_t            timeout_ms;     /**< SDO Timeout (ms) */
    const char*         description;    /**< 단계 설명 (디버깅용) */
} ImuHub_PreOpStep_t;

/**
 * @brief IMU Hub 드라이버 인스턴스 (내부 관리)
 *
 * [V4.0 변경] AGR_PnP_Inst_t → AGR_PnP_Master_t, device_index → slave_index
 * - 이유: 새 PnP Master API 적용 (.cursorrules Phase 2)
 * - Master PnP는 pnp_task.c에서 보유, Device Driver는 참조만 유지
 */
typedef struct {
    /* CANopen 표준 프로토콜 */
    AGR_DOP_Ctx_t       dop_ctx;            /**< DOP V2 Context (SDO, PDO) */
    AGR_TxFunc_t        tx_func;            /**< FDCAN Tx 함수 */
    
    /* Master PnP (System Layer) - V4.0 */
    AGR_PnP_Master_t*   master_pnp;         /**< ✅ Master PnP 인스턴스 (pnp_task.c에서 제공) */
    int                 slave_index;        /**< ✅ AddSlave() 반환값 (Slave 접근용) */
    
    /* Pre-Op 상태 머신 (검증된 로직 유지) */
    ImuHub_PreOpState_t pre_op_state;       /**< Pre-Op 상태 */
    uint32_t            last_sdo_tx_time;   /**< 마지막 SDO 전송 시간 (Timeout 체크용) */
    uint8_t             sdo_retry_count;    /**< SDO 재시도 횟수 */
    uint8_t             imu_connected_mask; /**< IMU Connected Mask (SDO 조회 결과) */
    
    /* Rx Data (IMU Hub → XM10) */
    ImuHub_RxData_t     rx_data;            /**< 최종 처리된 데이터 (int16) */
    volatile bool       is_data_ready;      /**< TPDO 수신 플래그 */
    
} ImuHub_DrvInst_t;

/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static ImuHub_DrvInst_t s_imu_hub_inst;

/* ===== Forward Declarations (Step Array Actions) - V4.0 ===== */
static int _Step_SendPdoMapA(void);
static int _Step_SendPdoMapB(void);
static int _Step_SendImuMaskReq(void);
static int _Step_SendNmtStart(void);

/**
 * @brief Pre-Op Step Array (선언적 시퀀스 정의)
 * @details 확장성을 위한 Step Array 패턴
 */
static const ImuHub_PreOpStep_t s_pre_op_steps[] = {
    { IMUHUB_PRE_OP_SEND_PDO_MAP_A,    IMUHUB_PRE_OP_WAIT_PDO_MAP_A,    _Step_SendPdoMapA,     5000,    "TPDO1 Mapping" },
    { IMUHUB_PRE_OP_SEND_PDO_MAP_B,    IMUHUB_PRE_OP_WAIT_PDO_MAP_B,    _Step_SendPdoMapB,     5000,    "TPDO2 Mapping" },
    { IMUHUB_PRE_OP_SEND_IMU_MASK_REQ, IMUHUB_PRE_OP_WAIT_IMU_MASK_RSP, _Step_SendImuMaskReq,  5000,    "IMU Mask Read" },
    { IMUHUB_PRE_OP_SEND_NMT_START,    IMUHUB_PRE_OP_COMPLETE,          _Step_SendNmtStart,    5000,    "NMT START" },
};

#define PRE_OP_STEP_COUNT (sizeof(s_pre_op_steps) / sizeof(s_pre_op_steps[0]))

/** @brief PDO Mapping 데이터 (CANopen 표준 형식) */
/* 
 * TPDO1 (Group A): Metadata + IMU 0,1,2 (64B)
 * [CANopen 표준 PDO Mapping Format]
 * - Byte 0: Number of mapped objects
 * - Entry Format: Length(bits), SubIndex, Index(LE)
 */
static const uint8_t s_pdo_map_a[] = {
    0x04,  /* Number of mapped objects = 4 */
    /* Entry 1: 0x3000.0x00 - Metadata (4B = 32 bits) */
    0x20, 0x00, 0x00, 0x30,
    /* Entry 2: 0x6000.0x60 - IMU0 {q,a,g} (20B = 160 bits) */
    0xA0, 0x60, 0x00, 0x60,
    /* Entry 3: 0x6001.0x60 - IMU1 {q,a,g} (20B = 160 bits) */
    0xA0, 0x60, 0x01, 0x60,
    /* Entry 4: 0x6002.0x60 - IMU2 {q,a,g} (20B = 160 bits) */
    0xA0, 0x60, 0x02, 0x60,
};

/* TPDO2 (Group B): Metadata + IMU 3,4,5 (64B) */
static const uint8_t s_pdo_map_b[] = {
    0x04,  /* Number of mapped objects = 4 */
    /* Entry 1: 0x3000.0x00 - Metadata (4B = 32 bits) */
    0x20, 0x00, 0x00, 0x30,
    /* Entry 2: 0x6003.0x60 - IMU3 {q,a,g} (20B = 160 bits) */
    0xA0, 0x60, 0x03, 0x60,
    /* Entry 3: 0x6004.0x60 - IMU4 {q,a,g} (20B = 160 bits) */
    0xA0, 0x60, 0x04, 0x60,
    /* Entry 4: 0x6005.0x60 - IMU5 {q,a,g} (20B = 160 bits) */
    0xA0, 0x60, 0x05, 0x60,
};

/**
 * ✅ Single Buffer with Mutex + Snapshot (V2 Architecture, Rule 13)
 * - Producer (IOIF RxTask): Mutex로 보호된 버퍼에 PDO 디코딩 결과 업데이트
 * - Consumer (Main Task): Mutex + memcpy로 안전하게 복사 (Snapshot)
 * - timeout=0: Non-blocking, 경합 시 즉시 return → 이전 Snapshot 재사용
 * - 장점: 구현 단순, 모든 race condition 완벽 차단, RT jitter 없음
 */
static ImuHub_RxData_t s_pdo_shared = {0};

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/** @brief [V5.0] IMU Hub PDO Seqlock (ISR-Direct: no FreeRTOS API in writer) */
static volatile uint32_t s_imu_pdo_seq = 0;
#else
/** @brief [V4.0] PDO 보호용 Mutex */
static osMutexId_t s_pdo_mutex = NULL;
#endif

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * ===== [V4.0] AGR_PnP_Master 콜백 (Device Driver에서 구현) =====
 * 
 * [콜백 호출 흐름] (V4.0)
 * Boot-up 수신 → AGR_PnP_Master_ProcessMessage() (pnp_task.c)
 *   → on_slave_bootup 콜백 → _PnP_OnBootup() (여기) ✅
 * Timeout 발생 → AGR_PnP_Master_RunPeriodic() (pnp_task.c)
 *   → _OnSlaveNmtTimeout → on_slave_offline → _PnP_OnOffline() ✅
 *
 * [콜백 1:1 대조] (.cursorrules "리팩토링 시 기능 보존 최우선")
 * V3.0 _PnP_OnBootup       → V4.0 _PnP_OnBootup       (동일 로직)
 * V3.0 _PnP_OnNmtChange    → V4.0 _PnP_OnStateChanged  (동일 로직)
 * V3.0 _PnP_OnConnected    → V4.0 _PnP_OnOnline        (동일 로직)
 * V3.0 _PnP_OnDisconnected → V4.0 _PnP_OnOffline       (동일 로직)
 * V3.0 _PnP_RunPreOp       → V4.0 RunPeriodic()에서 직접 실행 ⚠️
 */

/* AGR_PnP_Master_SlaveCallbacks_t 매핑 */
static void _PnP_OnBootup(uint8_t slave_node_id);
static void _PnP_OnStateChanged(uint8_t slave_node_id, AGR_NMT_State_t old_state, AGR_NMT_State_t new_state);
static void _PnP_OnOnline(uint8_t slave_node_id);
static void _PnP_OnOffline(uint8_t slave_node_id);

/* SDO Response 처리 (Pre-Op 상태 전이) */
static void _OnSdoResponse(const AGR_SDO_Msg_t* response);

/* Pre-Op State Machine 실행 (V4.0: RunPeriodic에서 직접 호출) */
static void _RunPreOpStateMachine(void);

/* ===== PDO 디코딩 (Mutex 보호 영역 내에서 호출) ===== */
static void _ImuHub_DecodeTpdo1(const uint8_t* data, uint8_t len);
static void _ImuHub_DecodeTpdo2(const uint8_t* data, uint8_t len);

/* Helper 함수 제거 (AGR_PnP API 사용) */

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief IMU Hub 드라이버 초기화 (V4.0: 새 AGR_PnP_Master API)
 *
 * [V4.0 변경] (.cursorrules Phase 2)
 * - AS-IS: AGR_PnP_RegisterDevice() (구 API, AGR_PnP_Device_t)
 * - TO-BE: AGR_PnP_Master_AddSlave() (신 API, AGR_PnP_SlaveConfig_t)
 *
 * [콜백 매핑]
 * - on_slave_bootup: Pre-Op SM 시작 (V3.0 _PnP_OnBootup과 동일)
 * - on_slave_state_changed: NMT 변경 로그 (V3.0 _PnP_OnNmtChange과 동일)
 * - on_slave_online: Pre-Op 완료 처리 (V3.0 _PnP_OnConnected과 동일)
 * - on_slave_offline: Pre-Op 초기화 (V3.0 _PnP_OnDisconnected과 동일)
 *
 * [제거된 콜백]
 * - on_run_pre_op: RunPeriodic()에서 직접 실행 (PnP Master가 호출하지 않음)
 * - on_error: RunPeriodic()의 SDO Timeout/Retry로 대체
 */
int ImuHub_Drv_Init(AGR_TxFunc_t tx_func, AGR_PnP_Master_t* master_pnp)
{
    if (tx_func == NULL || master_pnp == NULL) {
        return -1;
    }
    
    memset(&s_imu_hub_inst, 0, sizeof(ImuHub_DrvInst_t));
    
    /* 1. Tx 함수 저장 */
    s_imu_hub_inst.tx_func = tx_func;
    
    /* 2. Master PnP 인스턴스 저장 */
    s_imu_hub_inst.master_pnp = master_pnp;
    s_imu_hub_inst.slave_index = -1;
    
    memset(&s_pdo_shared, 0, sizeof(ImuHub_RxData_t));
#if !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    if (s_pdo_mutex == NULL) {
        const osMutexAttr_t mutex_attr = {
            .name = "ImuPdoMtx",
            .attr_bits = osMutexPrioInherit,
        };
        s_pdo_mutex = osMutexNew(&mutex_attr);
    }
#endif

    /* ===================================================================
     * [Layer 2] AGR_CANFD 초기화 (CANopen SDO/PDO 프로토콜)
     * ===================================================================
     * [역할] SDO Request/Response, PDO 송수신
     * [Master] OD 불필요 (Slave의 OD만 접근)
     */
    AGR_CANFD_Init(&s_imu_hub_inst.dop_ctx,
                   NULL,              /* OD Table: Master는 불필요 (Slave OD만 접근) */
                   AGR_NODE_ID_XM,    /* XM10 Node ID (0x02) */
                   tx_func);
    AGR_CANFD_SetTargetNodeId(&s_imu_hub_inst.dop_ctx, AGR_NODE_ID_IMU_HUB);
    
    /* ===================================================================
     * [Layer 3] AGR_PnP_Master에 Slave 등록 (V4.0: 신 API)
     * ===================================================================
     * [역할] Slave(IMU Hub) 연결 상태 관리 (PnP Master)
     * [콜백] Device Driver에서 구현한 PnP 이벤트 콜백 등록
     * 
     * [Master PnP는 pnp_task.c에서 하나만 관리]
     * - 각 Device Driver는 AddSlave()로 등록만 수행
     * - Heartbeat는 PnP Task에서 1회만 전송 (중복 없음)
     */
    AGR_PnP_SlaveConfig_t slave_config = {
        .name = "IMU Hub",
        .node_id = AGR_NODE_ID_IMU_HUB,
        .heartbeat_timeout_ms = 3000,    /* .cursorrules: Breakpoint 대응 3초 */
        .callbacks = {
            .on_slave_bootup        = _PnP_OnBootup,
            .on_slave_state_changed = _PnP_OnStateChanged,
            .on_slave_online        = _PnP_OnOnline,
            .on_slave_offline       = _PnP_OnOffline,
        }
    };
    
    int idx = AGR_PnP_Master_AddSlave(master_pnp, &slave_config);
    if (idx < 0) {
        return -1;  /* 등록 실패 */
    }
    s_imu_hub_inst.slave_index = idx;
    
    return 0;
}

/**
 * @brief CAN 메시지 처리 (TPDO/SDO만, Heartbeat 제외)
 *
 * [V4.0 변경] (.cursorrules Phase 2)
 * - 제거: 0x700 (Boot-up/Heartbeat) 처리
 *   → canfd_rx_handler.c에서 AGR_PnP_Master_ProcessMessage()로 라우팅
 * - 유지: 0x580 (SDO Response), 0x180/0x280 (TPDO)
 */
void ImuHub_Drv_ProcessCANMessage(uint16_t can_id, uint8_t* data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return;
    }
    
    /* ===== CAN ID 기반 분기 (CANopen 표준) ===== */
    uint16_t fnc_code = can_id & 0x780;  /* Function Code 추출 */
    
    /* [V4.0 제거] 0x700 (Boot-up/Heartbeat) 처리
     * → canfd_rx_handler.c → AGR_PnP_Master_ProcessMessage() 에서 처리
     * → PnP Master가 on_slave_bootup 콜백으로 Pre-Op SM 시작 */
    
    /* 1. SDO Response (CAN ID 0x580 + Node ID) */
    if (fnc_code == 0x580) {
        /* ✅ AGR_DOP가 SDO 디코딩 (CANopen 표준 프로토콜 처리) */
        AGR_SDO_Msg_t sdo_msg;
        if (AGR_SDO_Decode(data, len, &sdo_msg) == 0) {
            /* Device Driver가 Pre-Op 상태 전환 (응용 로직) */
            _OnSdoResponse(&sdo_msg);
        }
        return;
    }
    
    /* 3. PDO (CAN ID 0x180/0x280 + Node ID) — Mutex + Snapshot 패턴 */
    if (fnc_code == 0x180) {  /* TPDO1 (Group A: IMU 0,1,2) */
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
        /* [V5.0] Seqlock Writer (ISR context) */
        s_imu_pdo_seq++;
        __DMB();
        _ImuHub_DecodeTpdo1(data, len);
        __DMB();
        s_imu_pdo_seq++;
        s_imu_hub_inst.is_data_ready = true;
#else
        if (osMutexAcquire(s_pdo_mutex, 1) == osOK) {
            _ImuHub_DecodeTpdo1(data, len);
            osMutexRelease(s_pdo_mutex);
            s_imu_hub_inst.is_data_ready = true;
        }
#endif
        return;
    }

    if (fnc_code == 0x280) {  /* TPDO2 (Group B: IMU 3,4,5) */
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
        s_imu_pdo_seq++;
        __DMB();
        _ImuHub_DecodeTpdo2(data, len);
        __DMB();
        s_imu_pdo_seq++;
        s_imu_hub_inst.is_data_ready = true;
#else
        if (osMutexAcquire(s_pdo_mutex, 1) == osOK) {
            _ImuHub_DecodeTpdo2(data, len);
            osMutexRelease(s_pdo_mutex);
            s_imu_hub_inst.is_data_ready = true;
        }
#endif
        return;
    }
}

/**
 * @brief IMU Hub 연결 상태 확인 (V4.0: 새 PnP Master API)
 * @return true: 연결됨 (OPERATIONAL + Heartbeat OK)
 *
 * [V4.0 변경] AGR_NMT_IsConnected() → AGR_PnP_Master_IsSlaveOnline()
 */
bool ImuHub_Drv_IsConnected(void)
{
    return AGR_PnP_Master_IsSlaveOnline(s_imu_hub_inst.master_pnp,
                                         AGR_NODE_ID_IMU_HUB);
}

/**
 * @brief IMU Hub NMT 상태 조회 (V4.0: 새 PnP Master API)
 * @return NMT 상태
 *
 * [V4.0 변경] AGR_NMT_GetState() → AGR_PnP_Master_GetSlaveState()
 */
AGR_NMT_State_t ImuHub_Drv_GetNmtState(void)
{
    return AGR_PnP_Master_GetSlaveState(s_imu_hub_inst.master_pnp,
                                         AGR_NODE_ID_IMU_HUB);
}

/**
 * @brief 주기 실행 (V4.0: Pre-Op SM Timeout + Step Array 실행)
 *
 * @details
 * pnp_task.c의 PnP Task에서 100ms 주기로 호출합니다.
 *
 * [V4.0 변경] (.cursorrules Phase 2)
 * - AS-IS: Timeout만 체크, Pre-Op SM은 _PnP_RunPreOp 콜백에서 실행
 * - TO-BE: Timeout + Pre-Op SM을 직접 실행 (콜백 의존 제거)
 *
 * [처리 항목]
 * 1. Pre-Op SDO Timeout 체크 (5초) + Retry (최대 3회)
 * 2. Pre-Op Step Array 기반 SDO 전송 (SEND 상태 처리)
 *
 * @note Heartbeat/NMT Timeout은 PnP Master(pnp_task.c)가 처리합니다.
 */
void ImuHub_Drv_RunPeriodic(void)
{
    uint32_t current_ms = IOIF_TIM_GetTick();
    
    /* ===== 1. Pre-Op Timeout 체크 (SDO ACK 대기 상태) ===== */
    if (s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_WAIT_PDO_MAP_A ||
        s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_WAIT_PDO_MAP_B ||
        s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_WAIT_IMU_MASK_RSP) {
        
        if (current_ms - s_imu_hub_inst.last_sdo_tx_time > 5000) {
            if (s_imu_hub_inst.sdo_retry_count < 3) {
                /* ✅ 재시도: WAIT → SEND (같은 단계 재시도) */
                s_imu_hub_inst.sdo_retry_count++;
                if (s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_WAIT_PDO_MAP_A) {
                    s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_PDO_MAP_A;
                } else if (s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_WAIT_PDO_MAP_B) {
                    s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_PDO_MAP_B;
                } else if (s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_WAIT_IMU_MASK_RSP) {
                    /* IMU Mask는 Optional → Skip하고 다음 단계 */
                    s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_NMT_START;
                    s_imu_hub_inst.sdo_retry_count = 0;
                }
            } else {
                /* 최대 재시도 초과 → Error → IDLE */
                s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_IDLE;
                s_imu_hub_inst.sdo_retry_count = 0;
            }
        }
    }
    
    /* ===== 2. Pre-Op State Machine 구동 (SEND 상태 처리) =====
     * [V4.0] V3.0의 _PnP_RunPreOp 콜백 로직을 여기로 통합
     * - IDLE + Slave PRE_OP → Pre-Op 시작 (Boot-up 놓친 경우 대응)
     * - SEND 상태 → Step Array Action 실행
     */
    _RunPreOpStateMachine();
}

/**
 * @brief IMU Hub PDO 데이터 가져오기 (Mutex + Snapshot, V2 Architecture)
 * @details
 * [V2 Mutex + Snapshot Pattern]
 * - osMutexAcquire(timeout=0): Non-blocking
 * - 성공 시: memcpy로 Snapshot 생성 → Mutex 즉시 해제
 * - 실패 시: 이전 Snapshot 재사용 (Graceful Degradation)
 * - Snapshot 이후: Lock 없이 안전하게 사용 (no jitter)
 */
bool ImuHub_Drv_GetRxData(ImuHub_RxData_t* rx_data)
{
    if (rx_data == NULL) {
        return false;
    }

    if (!s_imu_hub_inst.is_data_ready) {
        return false;
    }

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Seqlock Reader — ISR 충돌 시 자동 retry */
    uint32_t seq1, seq2;
    do {
        seq1 = s_imu_pdo_seq;
        __DMB();
        memcpy(rx_data, &s_pdo_shared, sizeof(ImuHub_RxData_t));
        __DMB();
        seq2 = s_imu_pdo_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    return true;
#else
    if (osMutexAcquire(s_pdo_mutex, 0) == osOK) {
        memcpy(rx_data, &s_pdo_shared, sizeof(ImuHub_RxData_t));
        osMutexRelease(s_pdo_mutex);
        return true;
    }
    return false;
#endif
}

bool ImuHub_Drv_IsDataReady(void)
{
    return s_imu_hub_inst.is_data_ready;
}

/* Legacy 함수들 제거 (AGR_PnP와 AGR_DOP가 처리) */

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS - AGR_PnP Callbacks (검증된 Pre-Op 로직)
 *------------------------------------------------------------
 */

/**
 * @brief IMU Hub 연결 완료 콜백
 */
/**
 * ===== [V4.0] AGR_PnP_Master 콜백 구현 =====
 *
 * [콜백 1:1 대조] (.cursorrules "리팩토링 시 기능 보존 최우선")
 * V3.0 _PnP_OnBootup       → V4.0 _PnP_OnBootup       동일 로직 ✅
 * V3.0 _PnP_OnNmtChange    → V4.0 _PnP_OnStateChanged  동일 로직 ✅
 * V3.0 _PnP_OnConnected    → V4.0 _PnP_OnOnline        동일 로직 ✅
 * V3.0 _PnP_OnDisconnected → V4.0 _PnP_OnOffline       동일 로직 ✅
 * V3.0 _PnP_RunPreOp       → V4.0 _RunPreOpStateMachine 통합 ✅
 * V3.0 _PnP_OnError        → V4.0 RunPeriodic Timeout/Retry로 대체 ✅
 */

/**
 * @brief Slave Boot-up 수신 콜백
 * @details PnP Master가 Boot-up(0x700, data[0]==0x00) 수신 시 호출
 *          Pre-Op 상태 머신을 시작합니다.
 */
static void _PnP_OnBootup(uint8_t slave_node_id)
{
    (void)slave_node_id;
    
    /* Boot-up 수신 → Pre-Op SM 시작 (IDLE 상태에서만) */
    /* COMPLETE: NMT START 전송 직후 — OPERATIONAL 전환 대기 중이므로 무시 */
    /* 진행 중: 중복 Boot-up 방지 */
    if (s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_IDLE) {
        s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_PDO_MAP_A;
        s_imu_hub_inst.sdo_retry_count = 0;
    }
}

/**
 * @brief Slave NMT 상태 변경 콜백
 * @details PnP Master가 Slave의 NMT 상태 변경을 감지했을 때 호출
 */
static void _PnP_OnStateChanged(uint8_t slave_node_id, AGR_NMT_State_t old_state, AGR_NMT_State_t new_state)
{
    (void)slave_node_id;
    (void)old_state;
    (void)new_state;
    
    /* TODO: 상태 변경 로그 */
}

/**
 * @brief Slave Online 콜백 (OPERATIONAL 도달)
 * @details PnP Master가 Slave OPERATIONAL 전환 감지 시 호출
 */
static void _PnP_OnOnline(uint8_t slave_node_id)
{
    (void)slave_node_id;
    
    /* Pre-Op 완료 → IDLE */
    s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_IDLE;
    s_imu_hub_inst.sdo_retry_count = 0;
    
    /* TODO: User 콜백 호출 */
}

/**
 * @brief Slave Offline 콜백 (Timeout → STOPPED)
 * @details PnP Master가 Slave Timeout 감지 시 호출
 */
static void _PnP_OnOffline(uint8_t slave_node_id)
{
    (void)slave_node_id;
    
    /* Pre-Op 상태 초기화 (재연결 시 처음부터) */
    s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_IDLE;
    s_imu_hub_inst.sdo_retry_count = 0;
    s_imu_hub_inst.imu_connected_mask = 0x00;
    
    /* TODO: User 콜백 호출 */
}

/**
 * @brief Pre-Op State Machine 실행 (V4.0: RunPeriodic에서 직접 호출)
 *
 * [V4.0 변경]
 * - AS-IS: _PnP_RunPreOp 콜백으로 PnP에서 호출
 * - TO-BE: RunPeriodic()에서 직접 호출
 *
 * [통합된 로직]
 * 1. IDLE + Slave PRE_OP → Pre-Op 시작 (Boot-up 놓친 경우 대응)
 * 2. SEND 상태 → Step Array Action 실행
 * 3. WAIT 상태 → SDO Response 대기 (대기 중)
 * 4. COMPLETE 상태 → 아무것도 안함
 */
static void _RunPreOpStateMachine(void)
{
    /* IDLE 상태이고 Slave가 PRE_OP이면 Pre-Op 시작 (Boot-up 놓친 경우) */
    if (s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_IDLE) {
        AGR_NMT_State_t slave_state = AGR_PnP_Master_GetSlaveState(
            s_imu_hub_inst.master_pnp, AGR_NODE_ID_IMU_HUB);
        if (slave_state == AGR_NMT_PRE_OPERATIONAL) {
            /* XM 리셋 시 Slave가 이미 PRE_OP 상태라면 능동적으로 Pre-Op 시작 */
            s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_PDO_MAP_A;
            s_imu_hub_inst.sdo_retry_count = 0;
        }
        return;
    }
    
    /* COMPLETE 상태는 아무것도 안함 */
    if (s_imu_hub_inst.pre_op_state == IMUHUB_PRE_OP_COMPLETE) {
        return;
    }
    
    /* Step Array 기반 실행 (확장성) */
    for (uint8_t i = 0; i < PRE_OP_STEP_COUNT; i++) {
        const ImuHub_PreOpStep_t* step = &s_pre_op_steps[i];
        
        /* 현재 상태가 SEND 상태인 경우 → Action 실행 */
        if (s_imu_hub_inst.pre_op_state == step->send_state) {
            int result = step->action();
            if (result >= 0) {
                /* 성공 → WAIT 상태로 전환 */
                s_imu_hub_inst.pre_op_state = step->wait_state;
                s_imu_hub_inst.last_sdo_tx_time = IOIF_TIM_GetTick();
                s_imu_hub_inst.sdo_retry_count = 0;
            }
            return;
        }
        
        /* 현재 상태가 WAIT 상태인 경우 → SDO Response 대기 */
        if (s_imu_hub_inst.pre_op_state == step->wait_state) {
            return;
        }
    }
}

/**
 *-----------------------------------------------------------
 * STEP ARRAY - Action 함수 구현 (V4.0: AGR_DOP 직접 사용)
 *-----------------------------------------------------------
 * [V4.0 변경]
 * - AS-IS: AGR_PnP_SendSDOWrite(inst, node_id, ...) (구 PnP API)
 * - TO-BE: AGR_CANFD_SendSDOWrite(dop_ctx, ...) (Transport 직접 사용)
 * - NMT: AGR_PnP_Master_SendNmt(master, ...) (신 PnP API)
 */

/**
 * @brief Step 1: TPDO1 Mapping 전송
 */
static int _Step_SendPdoMapA(void)
{
    return AGR_CANFD_SendSDOWrite(&s_imu_hub_inst.dop_ctx,
                                  IMUHUB_OD_IDX_PDO_MAPPING_A, 0,
                                  s_pdo_map_a, sizeof(s_pdo_map_a));
}

/**
 * @brief Step 2: TPDO2 Mapping 전송
 */
static int _Step_SendPdoMapB(void)
{
    return AGR_CANFD_SendSDOWrite(&s_imu_hub_inst.dop_ctx,
                                  IMUHUB_OD_IDX_PDO_MAPPING_B, 0,
                                  s_pdo_map_b, sizeof(s_pdo_map_b));
}

/**
 * @brief Step 3: IMU Connected Mask 조회 (Optional)
 * @details AGR_CANFD_SendSDO를 직접 사용하여 SDO Upload Request 전송
 */
static int _Step_SendImuMaskReq(void)
{
    /* SDO Upload Initiate Request (Read) 구성 */
    AGR_SDO_Msg_t sdo_req = {
        .cs = AGR_SDO_CS_UPLOAD_INIT_REQ,
        .index = IMUHUB_OD_IDX_IMU_CONN_MASK,
        .subindex = 0,
        .data_len = 0
    };
    return AGR_CANFD_SendSDO(&s_imu_hub_inst.dop_ctx,
                             AGR_NODE_ID_IMU_HUB, &sdo_req);
}

/**
 * @brief Step 4: NMT START 전송
 * @details AGR_PnP_Master_SendNmt()으로 NMT Start 명령 전송
 */
static int _Step_SendNmtStart(void)
{
    return AGR_PnP_Master_SendNmt(s_imu_hub_inst.master_pnp,
                                   AGR_NODE_ID_IMU_HUB,
                                   AGR_NMT_CMD_START);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS - AGR_DOP Callbacks
 *------------------------------------------------------------
 */

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS - Device Driver 콜백 (응용 로직)
 *------------------------------------------------------------
 */

/**
 * @brief SDO Response 처리 (Pre-Op 상태 전환)
 * @details 
 * ✅ 레이어 분리:
 * - AGR_DOP: CANopen 표준 프로토콜 처리 (SDO 디코딩)
 * - Device Driver: 응용 로직 (Pre-Op 상태 머신)
 */
static void _OnSdoResponse(const AGR_SDO_Msg_t* response)
{
    uint8_t cs = response->cs & 0xE0;
    
    /* SDO Download Response (ACK) */
    if (cs == AGR_SDO_CS_DOWNLOAD_INIT_RSP) {
        if (response->index == IMUHUB_OD_IDX_PDO_MAPPING_A) {
            /* TPDO1 Mapping 완료 → TPDO2로 */
            s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_PDO_MAP_B;
            s_imu_hub_inst.sdo_retry_count = 0;
        }
        else if (response->index == IMUHUB_OD_IDX_PDO_MAPPING_B) {
            /* TPDO2 Mapping 완료 → IMU Mask 조회 */
            s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_IMU_MASK_REQ;
            s_imu_hub_inst.sdo_retry_count = 0;
        }
    }
    /* SDO Upload Response (Data) */
    else if ((response->cs & 0xE0) == 0x40) {
        if (response->index == IMUHUB_OD_IDX_IMU_CONN_MASK) {
            /* IMU Connected Mask 수신 */
            s_imu_hub_inst.imu_connected_mask = response->data[0];
            s_imu_hub_inst.pre_op_state = IMUHUB_PRE_OP_SEND_NMT_START;
            s_imu_hub_inst.sdo_retry_count = 0;
        }
    }
}

/* ===== PDO 디코딩 (Mutex 보호 영역 내에서 호출, s_pdo_shared에 직접 쓰기) ===== */

/**
 * @brief TPDO1 디코딩 (Group A: IMU 0,1,2) → s_pdo_shared에 직접 쓰기
 * @note 반드시 Mutex 획득 후 호출할 것
 *
 * Payload (64B): Metadata(4B) + IMU0(20B) + IMU1(20B) + IMU2(20B)
 * Metadata: Timestamp(24bit) + Valid Mask(8bit)
 */
static void _ImuHub_DecodeTpdo1(const uint8_t* data, uint8_t len)
{
    (void)len;

    /* Metadata (4B) */
    s_pdo_shared.timestamp = (data[0]) | (data[1] << 8) | (data[2] << 16);
    /* connected_mask: TPDO1의 mask만 업데이트 (TPDO2 비트 보존) */
    uint8_t mask = data[3];
    s_pdo_shared.connected_mask = (s_pdo_shared.connected_mask & 0x38) | (mask & 0x07);

    /* IMU Data (20B × 3) - Group A: IMU 0,1,2 */
    int offset = 4;
    for (int i = 0; i < IMUHUB_GROUP_A_COUNT; i++) {
        memcpy(&s_pdo_shared.imu[i], data + offset, 20);
        offset += 20;
    }
}

/**
 * @brief TPDO2 디코딩 (Group B: IMU 3,4,5) → s_pdo_shared에 직접 쓰기
 * @note 반드시 Mutex 획득 후 호출할 것
 *
 * Payload (64B): Metadata(4B) + IMU3(20B) + IMU4(20B) + IMU5(20B)
 * Metadata: Timestamp(24bit) + Valid Mask(8bit)
 */
static void _ImuHub_DecodeTpdo2(const uint8_t* data, uint8_t len)
{
    (void)len;

    /* Metadata (4B) — timestamp는 TPDO1이 primary */
    /* connected_mask: TPDO2의 mask만 업데이트 (TPDO1 비트 보존) */
    uint8_t mask = data[3];
    s_pdo_shared.connected_mask = (s_pdo_shared.connected_mask & 0x07) | (mask & 0x38);

    /* IMU Data (20B × 3) - Group B: IMU 3,4,5 → imu[3],imu[4],imu[5] */
    int offset = 4;
    for (int i = 0; i < IMUHUB_GROUP_B_COUNT; i++) {
        memcpy(&s_pdo_shared.imu[3 + i], data + offset, 20);
        offset += 20;
    }
}


