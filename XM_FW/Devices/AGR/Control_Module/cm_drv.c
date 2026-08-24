#include "cm_drv.h"
#include "data_object_interface.h" // DOP 서비스를 직접 사용
#include <string.h>
#include <math.h>

#include "cmsis_os2.h"
#include "ioif_conf.h"  /* IOIF_FDCAN_ISR_DIRECT_ENABLE */

/* ===== PnP 상태 머신 의존성 (cm_xm_link.c에서 이동) =====
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 */
#include "ioif_agrb_tim.h"
#include "ioif_agrb_dwt.h"      /* DWT: PDO 수신 주기 측정 (디버깅) */
#include "led_manager.h"

/**
 *-----------------------------------------------------------
 *               PRIVATE DEFINITIONS AND TYPES
 *-----------------------------------------------------------
 */

/* ===== PnP 상태 머신 (cm_xm_link.c에서 이동) =====
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: cm_xm_link.c의 LinkManagerInst_t
 * - TO-BE: cm_drv.c 내부 CM_PnP_State_t
 * 
 * [타임아웃 변경] (.cursorrules 표준 통일)
 * - OPERATIONAL Heartbeat 타임아웃: 1초 → 3초 (Breakpoint 대응)
 * - PRE_OP Heartbeat 타임아웃: 5초 (기존 유지)
 * - STOPPED → INIT 전환: 3초 (기존 유지)
 */

/** @brief PnP Heartbeat 타임아웃 (ms) - .cursorrules 표준 */
#define CM_PNP_HEARTBEAT_TIMEOUT_MS     3000
/** @brief PnP PRE_OP 타임아웃 (ms) */
#define CM_PNP_PREOP_TIMEOUT_MS         5000
/** @brief PnP Boot-up 전송 주기 (ms) */
#define CM_PNP_BOOTUP_INTERVAL_MS       1000
/** @brief PnP XM Heartbeat 전송 주기 (ms) */
#define CM_PNP_XM_HEARTBEAT_INTERVAL_MS 200
/** @brief PnP SDO 타임아웃 (ms) */
#define CM_PNP_SDO_TIMEOUT_MS           1000
/** @brief PnP SDO 최대 재시도 횟수 */
#define CM_PNP_SDO_MAX_RETRY            3
/** @brief PnP 동기화 타임아웃 (ms) */
#define CM_PNP_SYNC_TIMEOUT_MS          3000
/** @brief PnP STOPPED → INIT 전환 대기 (ms) */
#define CM_PNP_STOPPED_WAIT_MS          3000
/** @brief PnP STOPPED에서 Reset 명령 전송 시점 (ms) */
#define CM_PNP_STOPPED_RESET_CMD_MS     2000

// PRE_OPERATIONAL 하위 상태 정의
typedef enum {
    CM_PRE_OP_IDLE,
    CM_PRE_OP_WAIT_PDO_MAP_ACK,
    CM_PRE_OP_SEND_SYNC_STATES,
    CM_PRE_OP_WAIT_SYNC_STATES,
    CM_PRE_OP_WAIT_OPERATIONAL_HB
} CM_PreOpState_t;

// PnP 상태 머신 인스턴스
typedef struct {
    volatile CM_NmtState_t      nmtState;
    volatile CM_PreOpState_t    preOpState;
    volatile CM_NmtState_t      lastKnownCmState;
    volatile bool               isCmBooted;
    volatile bool               isCmConnected;
    volatile bool               isCmSynced;
    volatile uint32_t           lastCmHeartbeatMs;
    volatile uint32_t           lastSdoSentMs;
    volatile uint8_t            sdoRetryCount;
    volatile uint32_t           initEntryTime;
    volatile uint32_t           lastSyncStatesTime;
    volatile bool               isAllStatesSynced;
} CM_PnP_State_t;

/**
 * ✅ Single Buffer with Mutex + Snapshot
 * - FDCAN_Rx_Task: Mutex로 보호된 버퍼(s_cm_rx_buffer)에 업데이트
 * - Main Task: Mutex + memcpy로 안전하게 복사 (Snapshot)
 * - 장점: 구현 단순성, 메모리 절약, 완벽한 무결성(Tearing 없음)
 */
static CM_RxData_t s_cm_rx_buffer; /**< FDCAN_Rx_Task가 업데이트하는 원본 버퍼 */

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/**
 * @brief [V5.0] CM PDO Seqlock (ISR-Direct: FreeRTOS API 사용 불가)
 * @details
 * - Writer(FDCAN ISR, NVIC 4): seq 홀수 = writing, 짝수 = valid
 * - Reader(UserTask): seq 불일치 시 retry — 충돌 확률 < 0.5%
 * - Cortex-M NVIC가 동일/하위 우선순위 ISR 재진입 차단 → __disable_irq 불필요
 */
static volatile uint32_t s_cm_pdo_seq = 0;
#else
/**
 * @brief [V4.0] CM 데이터 보호용 Mutex (Device Layer 책임)
 */
static osMutexId_t s_cm_data_mutex = NULL;

#define CM_DATA_LOCK_READ()     (s_cm_data_mutex != NULL && osMutexAcquire(s_cm_data_mutex, 0) == osOK)
#define CM_DATA_LOCK_WRITE()    (s_cm_data_mutex != NULL && osMutexAcquire(s_cm_data_mutex, 1) == osOK)
#define CM_DATA_UNLOCK()        osMutexRelease(s_cm_data_mutex)
#endif

/* ===== [DEBUG] PDO 수신 진단 변수 (Live Expression 관찰용) =====
 * 확인 완료 후 이 블록 제거 또는 #if 0 처리
 */
static volatile uint32_t s_dbg_pdo_last_cycle    = 0;
static volatile uint32_t s_dbg_pdo_delta_us      = 0;      /**< 직전 수신 간격 (us), 기대값 ~1000 */
static volatile uint32_t s_dbg_pdo_min_us        = 0xFFFFFFFF; /**< 최소 간격 (us) */
static volatile uint32_t s_dbg_pdo_max_us        = 0;      /**< 최대 간격 (us) */
static volatile uint32_t s_dbg_pdo_count         = 0;      /**< 누적 수신 횟수 */
static volatile uint32_t s_dbg_pdo_last_loopcnt  = 0;
static volatile int32_t  s_dbg_pdo_loopcnt_delta = 0;      /**< 직전 LoopCnt 차이, 기대값 1 */
static volatile uint32_t s_dbg_pdo_miss_count    = 0;      /**< LoopCnt 유실 횟수, 기대값 0 */

#define CM_MAX_PDO		60
#define CM_MAX_SDO		60
#define CM_PDO_STAGE_MAX_SIZE 10

#define DATA_CONV_CONST_UINT16          65535       // uint16 conversion constant
#define DATA_CONV_CONST_INT16           32768       // int16 conversion constant
#define CURRENT_SCALING_FACTOR          60          // (A) -30 ~ +30
#define DEG_SCALING_FACTOR              720         // (deg) -360 ~ +360
#define VELDEG_SCALING_FACTOR           6000        // (deg/s) -3000 ~ +3000
#define FORWARD_VEL_X_SCALING_FACTOR    100         // (m/s) -50 ~ +50
#define GYR_SCALING_FACTOR              1000        // (deg/s) -500 ~ +500
#define ACC_SCALING_FACTOR              78.4532     // (m/s^2) 8 * g(9.80665) -39.24 ~ +39.24

/* ===== CM_EventCallbacks_t 필드 제거 =====
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: callbacks 필드로 외부 콜백 보관
 * - TO-BE: SDO 콜백에서 내부 PnP 상태 직접 업데이트
 */
typedef struct {
    CM_TxFunc_t txFunc;        // CAN 메시지 전송 함수 포인터
    uint16_t    sdoTxId;       // SDO 전송 ID
    uint16_t    pdoTxId;       // PDO 전송 ID
    
    // 송신 데이터 버퍼 (원시 값)
    struct {
        // --- 1. Tx PDO Payload (반드시 맨 위에 연속적으로 배치) ---
        // 이 구조체 자체가 CAN 페이로드로 전송됩니다.
        // PnP에서 약속된 고정 레이아웃이며, CAN 전송 시 이 메모리 블록이 직접 전송됨.
        #pragma pack(push, 1)
        CM_PdoTx_XmToCm_t pdo;
        #pragma pack(pop)

        // --- 2. SDO Data (순서 상관 없음) ---
        // SDO 전송 시 값이 참조되는 변수들
        uint8_t   pdoMappingBuffer[CM_MAX_PDO*2];
        // NMT
        uint8_t xmBootup; 	    // 확장팩의 Boot up
        uint8_t xmNmtState; 	// 확장팩의 NMT 상태 값
        uint8_t xmHeartbeat; 	// 확장팩의 Heartbeat 값
        uint8_t cmNmtStateCmd;  // CM의 NMT State 변경 명령
        // 신체 정보
        uint32_t userBodyData[8];	// uint32 4 Byte 데이터 8개
        uint32_t userWeight;
        uint32_t userHeight;
        uint32_t userRightThighLength;
        uint32_t userLeftThighLength;
        uint32_t userRightShinLength;
        uint32_t userLeftShinLength;
        uint32_t userRightAnkleLength;
        uint32_t userLeftAnkleLength;

        CM_PVector_t  pVectorRH;
        CM_PVector_t  pVectorLH;
        CM_IVector_t  iVectorRH;
        CM_IVector_t  iVectorLH;
        float iVectorKpMaxRH;
        float iVectorKdMaxRH;
        float iVectorKpMaxLH;
        float iVectorKdMaxLH;
        CM_FVector_t  fVectorRH;
        CM_FVector_t  fVectorLH;

        uint8_t setDegreeLimitRoutineRH;
        uint8_t clearDegreeLimitRoutineRH;
        uint8_t setDegreeLimitRoutineLH;
        uint8_t clearDegreeLimitRoutineLH;
        float     degreeLimitUpperRH;
        float     degreeLimitUpperLH;
        float     degreeLimitLowerRH;
        float     degreeLimitLowerLH;

        uint8_t setVelocityLimitRoutineRH;
        uint8_t clearVelocityLimitRoutineRH;
        uint8_t setVelocityLimitRoutineLH;
        uint8_t clearVelocityLimitRoutineLH;
        float     velocityLimitUpperRH;
        float     velocityLimitUpperLH;
        float     velocityLimitLowerRH;
        float     velocityLimitLowerLH;

        uint8_t setDOBRoutineRH;
        uint8_t clearDOBRoutineRH;
        uint8_t setDOBRoutineLH;
        uint8_t clearDOBRoutineLH;

        uint8_t normalCompGainRH;
        uint8_t normalCompGainLH;
        float resistiveCompGainRH;
        float resistiveCompGainLH;

        uint8_t H10ExistingAssistModeFlag;
    } txData;

    // 수신 데이터 버퍼 (원시 값)
    // 1. PDO 원시 수신 버퍼 (PDO 전용)
    // _IOIF_FDCAN_RxTask가 덮어쓰는 버퍼
    #pragma pack(push, 1)
    CM_PdoRx_CmToXm_t rawPdoData;
    #pragma pack(pop)

    // 2. 최종 데이터 캐시 (PDO + SDO)
    // s_cm_rx_buffer로 대체되었으므로 구조체 내에서 제거하거나 유지하되 사용하지 않음
    // 호환성을 위해 유지하되 로직은 s_cm_rx_buffer를 사용하도록 변경
    CM_RxData_t rxData_Unused; 
    bool        isDataReady; // rawPdoData에 새 PDO가 수신되었는지 알리는 플래그

    // --- DOP 관련 ---
    DOPI_DevObj_t cmDevObj;
    DOP_Dict_t    dods[CM_DICT_ID_COUNT];
} CM_Inst_t;


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 */
/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 */

 // 이 모듈의 모든 상태를 담는 정적(static) 인스턴스 -> 캡슐화
static CM_Inst_t s_cm;

// PnP 단계에서 CM으로부터 받을 "수신 맵"
// 이 맵은 이제 CM_SendSetPDOMapping에 의해 채워집니다.
static DOP_Header_t s_cm_pdoRxMap[CM_MAX_PDO];
static uint8_t      s_cm_currentPdoRxMapSize = 0;

/* ===== PnP 상태 머신 인스턴스 (cm_xm_link.c에서 이동) ===== */
static CM_PnP_State_t s_pnp = { .nmtState = CM_NMT_INITIALISING };

/* PnP RunPeriodic 내부에서 사용하는 static 타이머 변수들 */
static uint32_t s_pnp_last_bootup_sent_ms = 0;
static uint32_t s_pnp_last_xm_hb_time = 0;
static uint32_t s_pnp_stopped_entry_time = 0;
static bool     s_pnp_is_reset_cmd_sent = false;

/**
 *------------------------------------------------------------
 *                 PRIVATE FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */
// --- Object Dictionary Creation Functions ---
static void _CM_CreateObjectDictionary(void);
static void _CM_CreateSDO(CM_DictID_t dictId, uint8_t objId, uint8_t type, DOP_SDOCB_t callback);
static void _CM_CreatePDO(CM_DictID_t dictId, uint8_t objId, uint8_t type, uint8_t size, void* addr);
// --- Message Unpacking Functions (Rx Path) ---
static int _CM_UnpackSDO(const uint8_t* data, uint8_t len);
static int _CM_ReadSDO(const uint8_t* byte_arr);
static int _CM_CallSDO(DOP_SDO_t* sdo, DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);

static int _CM_UnpackPDO(const uint8_t* data, uint8_t len);
// static int _CM_ReadPDO(const uint8_t* byte_arr);

// --- Data Processing Functions (Final Rx Step) ---
static void _CM_DecodeRxPDOData(CM_RxPdoData_t* dst_buf);

// --- Message Packing Functions (Tx Path) ---
// static DOPI_SDOUnit_t _CM_CreateSDOUnit_payload(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID, DOP_SDOStatus_t t_SDOStatus, const void* payload, uint8_t payloadSize);
static int _CM_AppendSDO(DOPI_SDOUnit_t* sdo_unit, DOPI_SDOMsg_t* sdo_msg);
static uint8_t _CM_GetSDO_ElementSize(uint8_t dictId, uint8_t objId);
// static int _CM_AppendPDO(DOPI_PDOUnit_t* pdo_unit, DOPI_PDOMsg_t* pdo_msg);
// // --- PDO Staging Functions (Tx Helper) ---
// static void _CM_StagePDO(uint8_t dictId, uint8_t objId);
// static void _CM_ClearPdoStage(void);

// --- SDO Callback Functions (Event Handlers) ---
static void _CM_SdoCallback_OnCmBootup(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void _CM_SdoCallback_OnCmHeartbeat(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void _CM_SdoCallback_OnSuitSyncStates(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void _CM_SdoCallback_OnH10ModeChanged(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void _CM_SdoCallback_OnAssistLevelChanged(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void _CM_SdoCallback_OnPVectorCompletedRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void _CM_SdoCallback_OnPVectorCompletedLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void _CM_SdoCallback_OnNeutralPosSet(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);

// --- Low-Level Utility Functions ---
static DOP_SDO_t* _CM_FindSDO(uint8_t dictId, uint8_t objId);
static DOP_PDO_t* _CM_FindPDO(uint8_t dictId, uint8_t objId);
static float _CM_ScaleInt16ToFloat(int16_t value, float scaleFactor);
static void _CM_SendSdoMsg(DOPI_SDOMsg_t* sdoMsg);

// --- PnP 상태 머신 내부 함수 (cm_xm_link.c에서 이동) ---
static void _CM_PnP_GoToStopped(void);
static void _CM_PnP_OnCmBootup(void);
static void _CM_PnP_OnCmHeartbeat(uint8_t cmNmtState);
static void _CM_PnP_OnSdoResponse(uint8_t dictId, uint8_t sdoId, int8_t isSuccess);
static void _CM_PnP_OnSyncStatesReceived(void);

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

// --- 초기화 및 코어 처리 함수 (Initialization & Core Processing) ---
/**
 * @brief CM Device Driver 초기화 (DOP V1 + PnP 상태 머신 통합)
 * 
 * @details
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: CM_Init(txFunc, callbacks, xmNodeId, cmNodeId) + CM_XM_Link_Init()
 * - TO-BE: CM_Init(txFunc, xmNodeId, cmNodeId) 하나로 통합
 * - CM_EventCallbacks_t 제거 → SDO 콜백에서 내부 PnP 상태 직접 업데이트
 *
 * @param[in] txFunc      CAN 메시지를 전송할 함수 포인터 (IOIF 계층의 Transmit 함수).
 * @param[in] xmNodeId    이 모듈(XM)의 Node ID.
 * @param[in] cmNodeId    통신 대상인 CM의 Node ID.
 */
void CM_Init(CM_TxFunc_t txFunc, uint8_t xmNodeId, uint8_t cmNodeId)
{
    memset(&s_cm, 0, sizeof(CM_Inst_t));
    s_cm.txFunc = txFunc;
    s_cm.sdoTxId = SDO | (xmNodeId << 4) | cmNodeId;
    s_cm.pdoTxId = PDO | (xmNodeId << 4) | cmNodeId;

    /* ✅ Single Buffer 초기화 */
    memset(&s_cm_rx_buffer, 0, sizeof(CM_RxData_t));

#if !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V4.0] Mutex 생성 (Device Layer 책임) */
    if (s_cm_data_mutex == NULL) {
        const osMutexAttr_t cm_mutex_attr = {
            .name = "CM_DataMutex",
            .attr_bits = osMutexPrioInherit,
        };
        s_cm_data_mutex = osMutexNew(&cm_mutex_attr);
    }
#endif

    /* ===== PnP 상태 머신 초기화 (cm_xm_link.c에서 이동) =====
     * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
     */
    memset(&s_pnp, 0, sizeof(CM_PnP_State_t));
    s_pnp.nmtState = CM_NMT_INITIALISING;
    s_pnp_last_bootup_sent_ms = 0;
    s_pnp_last_xm_hb_time = 0;
    s_pnp_stopped_entry_time = 0;
    s_pnp_is_reset_cmd_sent = false;

    _CM_CreateObjectDictionary();
}

/**
 * @brief CM으로부터 수신된 SDO 메시지를 처리합니다.
 * @details SDO Processor Task (비실시간, Message Queue)에서 호출.
 * 
 * [변경 이유] .cursorrules "FDCAN Architecture" 섹션 기반 리팩토링
 * - AS-IS: CM_ProcessCANMessage() - SDO/PDO 분기 포함, PDO는 default 드롭
 * - TO-BE: CM_Drv_ProcessSdo() - SDO 전용, canfd_rx_handler에서 SDO만 라우팅
 * - PDO는 CM_Drv_ProcessPdo()에서 별도 처리 (실시간 경로)
 * 
 * @param[in] canId   수신된 메시지의 CAN ID (0x212).
 * @param[in] data    수신된 데이터 버퍼 포인터.
 * @param[in] len     수신된 데이터의 길이 (바이트).
 * 
 * @note Thread-Safe: 내부 Mutex 보호
 */
void CM_Drv_ProcessSdo(uint16_t canId, uint8_t* data, uint8_t len)
{
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] SDO는 NonRealtimeTask(Task context)에서 호출 → PDO ISR과 겹치지 않음 */
    _CM_UnpackSDO(data, len);
#else
    if (CM_DATA_LOCK_WRITE()) {
        _CM_UnpackSDO(data, len);
        CM_DATA_UNLOCK();
    }
#endif
}

/**
 * @brief CM으로부터 수신된 PDO 데이터를 처리합니다.
 * @details FDCAN_Rx_Task (실시간, 직접 라우팅)에서 호출.
 * 
 * [변경 이유] .cursorrules "FDCAN Architecture" 섹션 기반 리팩토링
 * - AS-IS: CM_UpdatePdoData() - 네이밍이 모호 (Update vs Process)
 * - TO-BE: CM_Drv_ProcessPdo() - PDO 전용 처리 함수로 명확화
 * 
 * @param[in] data    수신된 데이터 버퍼 포인터.
 * @param[in] len     수신된 데이터의 길이 (바이트).
 * 
 * @note Thread-Safe: 내부 Mutex 보호 (1ms Timeout)
 */
void CM_Drv_ProcessPdo(uint8_t* data, uint8_t len)
{
    /* [방법 1] DWT 수신 주기 측정 */
    uint32_t now_cycle = IOIF_DWT_GetCycles();
    if (s_dbg_pdo_last_cycle != 0) {
        uint32_t delta_cycles = now_cycle - s_dbg_pdo_last_cycle;
        s_dbg_pdo_delta_us = IOIF_DWT_CyclesToUs(delta_cycles);
        if (s_dbg_pdo_delta_us < s_dbg_pdo_min_us) s_dbg_pdo_min_us = s_dbg_pdo_delta_us;
        if (s_dbg_pdo_delta_us > s_dbg_pdo_max_us) s_dbg_pdo_max_us = s_dbg_pdo_delta_us;
    }
    s_dbg_pdo_last_cycle = now_cycle;
    s_dbg_pdo_count++;

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Seqlock Writer (ISR context, NVIC 4 — no FreeRTOS API) */
    s_cm_pdo_seq++;
    __DMB();
    if (_CM_UnpackPDO(data, len) > 0) {
        _CM_DecodeRxPDOData(&s_cm_rx_buffer.pdo);
        s_cm.isDataReady = true;

        uint32_t cur_loopcnt = s_cm_rx_buffer.pdo.h10AssistModeLoopCnt;
        if (s_dbg_pdo_last_loopcnt != 0 && cur_loopcnt != 0) {
            s_dbg_pdo_loopcnt_delta = (int32_t)(cur_loopcnt - s_dbg_pdo_last_loopcnt);
            if (s_dbg_pdo_loopcnt_delta != 1) {
                s_dbg_pdo_miss_count++;
            }
        }
        s_dbg_pdo_last_loopcnt = cur_loopcnt;
    }
    __DMB();
    s_cm_pdo_seq++;
#else
    /* [V4.0] Mutex Writer (RxTask context) */
    if (CM_DATA_LOCK_WRITE()) {
        if (_CM_UnpackPDO(data, len) > 0) {
            _CM_DecodeRxPDOData(&s_cm_rx_buffer.pdo);
            s_cm.isDataReady = true;

            uint32_t cur_loopcnt = s_cm_rx_buffer.pdo.h10AssistModeLoopCnt;
            if (s_dbg_pdo_last_loopcnt != 0 && cur_loopcnt != 0) {
                s_dbg_pdo_loopcnt_delta = (int32_t)(cur_loopcnt - s_dbg_pdo_last_loopcnt);
                if (s_dbg_pdo_loopcnt_delta != 1) {
                    s_dbg_pdo_miss_count++;
                }
            }
            s_dbg_pdo_last_loopcnt = cur_loopcnt;
        }
        CM_DATA_UNLOCK();
    }
#endif
}

// --- 데이터 수신 API (Data Getter API) ---
/**
 * @brief CM PDO 데이터 가져오기 (Mutex + Snapshot)
 * @details Main Task에서 호출, Mutex로 보호된 영역에서 안전하게 복사
 * @param[out] rxData 수신된 데이터를 저장할 CM_RxData_t 구조체 포인터.
 * @return            데이터 수신 성공 시 `true`, 아직 유효한 데이터가 없을 시 `false`.
 * 
 * @note Thread-Safe: 내부에서 Mutex 보호 (1ms Timeout)
 */
bool CM_GetRxData(CM_RxData_t* rxData)
{
    if (rxData == NULL || !s_cm.isDataReady) {
        return false;
    }
    
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Seqlock Reader (UserTask context) — ISR 충돌 시 자동 retry */
    uint32_t seq1, seq2;
    do {
        seq1 = s_cm_pdo_seq;
        __DMB();
        memcpy(rxData, &s_cm_rx_buffer, sizeof(CM_RxData_t));
        __DMB();
        seq2 = s_cm_pdo_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    return true;
#else
    /* [V4.0] Mutex + Snapshot */
    bool success = false;
    if (CM_DATA_LOCK_READ()) {
        memcpy(rxData, &s_cm_rx_buffer, sizeof(CM_RxData_t));
        success = true;
        CM_DATA_UNLOCK();
    }
    return success;
#endif
}

/**
 * @brief CM Raw PDO 데이터(int16)를 Mutex 보호 하에 복사합니다.
 * @details Total Data Packet에서 CM PDO 섹션을 raw memcpy로 채울 때 사용.
 *          CM_GetRxData()와 동일 Mutex 패턴, float 변환 없이 원본 제공.
 */
bool CM_GetRawPdoData(CM_PdoRx_CmToXm_t* raw_pdo)
{
    if (raw_pdo == NULL || !s_cm.isDataReady) {
        return false;
    }

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Seqlock Reader */
    uint32_t seq1, seq2;
    do {
        seq1 = s_cm_pdo_seq;
        __DMB();
        memcpy(raw_pdo, &s_cm.rawPdoData, sizeof(CM_PdoRx_CmToXm_t));
        __DMB();
        seq2 = s_cm_pdo_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    return true;
#else
    /* [V4.0] Mutex + Snapshot */
    bool success = false;
    if (CM_DATA_LOCK_READ()) {
        memcpy(raw_pdo, &s_cm.rawPdoData, sizeof(CM_PdoRx_CmToXm_t));
        success = true;
        CM_DATA_UNLOCK();
    }
    return success;
#endif
}

/**
 * @brief 유효한 PDO 데이터가 한 번 이상 수신되었는지 확인합니다.
 * @details s_cm.isDataReady는 PDO 언패킹 성공 시 true로 설정됩니다.
 */
bool CM_IsDataReady(void)
{
    return s_cm.isDataReady;
}

/**
 * @brief CM으로부터 수신된 모든 데이터 캐시를 0으로 초기화합니다.
 * @details 이 함수는 s_cm의 rawPdoData와 rxData 구조체를 모두 0으로 클리어하고,
 * isDataReady 플래그를 false로 설정하여 데이터 동기화 상태를 리셋합니다.
 * @note Thread-Safe: 내부에서 Mutex로 보호됨
 * 주로 cm_xm_link의 _Link_GoToStopped와 같이 연결이 끊어지는 시점에
 * 외부에서 호출하기 위해 설계되었습니다.
 */
void CM_ResetRxData(void)
{
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Task context — ISR과 동시성은 Seqlock이 보호 */
    memset(&s_cm.rawPdoData, 0, sizeof(s_cm.rawPdoData));
    memset(&s_cm_rx_buffer, 0, sizeof(CM_RxData_t));
    s_cm.isDataReady = false;
#else
    if (CM_DATA_LOCK_WRITE()) {
        memset(&s_cm.rawPdoData, 0, sizeof(s_cm.rawPdoData));
        memset(&s_cm_rx_buffer, 0, sizeof(CM_RxData_t));
        s_cm.isDataReady = false;
        CM_DATA_UNLOCK();
    }
#endif
}

/* ===================================================================
 *  PnP 상태 머신 Public API (cm_xm_link.c에서 이동)
 * ===================================================================
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: CM_XM_Link_RunPeriodic(), CM_XM_Link_IsConnected(), CM_XM_Link_GetXMNmtState()
 * - TO-BE: CM_Drv_RunPeriodic(), CM_Drv_IsConnected(), CM_Drv_GetNmtState()
 * 
 * [버그 수정]
 * 1. PRE_OP_SEND_SYNC_STATES → PRE_OP_WAIT_SYNC_STATES fall-through 수정 (break 추가)
 * 
 * [타임아웃 변경] (.cursorrules 표준 통일)
 * 2. OPERATIONAL Heartbeat 타임아웃: 1초 → 3초 (Breakpoint 디버깅 대응)
 */

/**
 * @brief CM PnP 상태 머신 주기적 실행
 * @details PnP_Task에서 100ms 주기로 호출
 */
void CM_Drv_RunPeriodic(void)
{
    switch (s_pnp.nmtState) {
        case CM_NMT_INITIALISING:
        {
            uint32_t initTime = s_pnp.initEntryTime;
            uint32_t initCurrentTime = IOIF_TIM_GetTick();

            if (initTime == 0) {
                s_pnp.initEntryTime = initCurrentTime;
                initTime = initCurrentTime;
            }

            /* 1초마다 주기적으로 Boot-up 메시지 전송 (메시지 유실 방지) */
            if (initCurrentTime - s_pnp_last_bootup_sent_ms > CM_PNP_BOOTUP_INTERVAL_MS) {
                CM_SendBootup();
                s_pnp_last_bootup_sent_ms = initCurrentTime;
            }

            /* 5초가 지나도 CM의 응답이 없으면 최종 실패 처리 */
            if (initCurrentTime - s_pnp.initEntryTime > CM_PNP_PREOP_TIMEOUT_MS) {
                _CM_PnP_GoToStopped();
                s_pnp.initEntryTime = 0;
            }
            break;
        }

        case CM_NMT_PRE_OPERATIONAL:
        {
            uint32_t preOpLastHeartbeatTime = s_pnp.lastCmHeartbeatMs;
            uint32_t preOplastSdoTime = s_pnp.lastSdoSentMs;
            uint32_t preOpCurrentTime = IOIF_TIM_GetTick();

            /* PRE_OP 중에도 CM 생존 감시 (5초 타임아웃) */
            if (preOpCurrentTime - preOpLastHeartbeatTime > CM_PNP_PREOP_TIMEOUT_MS) {
                _CM_PnP_GoToStopped();
                break;
            }

            /* PRE_OPERATIONAL 하위 상태 머신 */
            switch (s_pnp.preOpState) {
                case CM_PRE_OP_IDLE:
                    CM_SendSetPDOMapping();
                    s_pnp.lastSdoSentMs = preOpCurrentTime;
                    s_pnp.sdoRetryCount = 1;
                    s_pnp.preOpState = CM_PRE_OP_WAIT_PDO_MAP_ACK;
                    break;

                case CM_PRE_OP_WAIT_PDO_MAP_ACK:
                    if (preOpCurrentTime - preOplastSdoTime > CM_PNP_SDO_TIMEOUT_MS) {
                        if (s_pnp.sdoRetryCount < CM_PNP_SDO_MAX_RETRY) {
                            s_pnp.preOpState = CM_PRE_OP_IDLE; /* 재전송 */
                            s_pnp.sdoRetryCount++;
                        } else {
                            _CM_PnP_GoToStopped(); /* 3회 실패 시 정지 */
                        }
                    }
                    break;

                case CM_PRE_OP_SEND_SYNC_STATES:
                    CM_SendSyncStates();
                    s_pnp.lastSyncStatesTime = preOpCurrentTime;
                    s_pnp.preOpState = CM_PRE_OP_WAIT_SYNC_STATES;
                    break;  /* ✅ fall-through 버그 수정 (.cursorrules Phase 5) */

                case CM_PRE_OP_WAIT_SYNC_STATES:
                {
                    uint32_t preOpLastSyncStatesTime = s_pnp.lastSyncStatesTime;
                    bool preOpIsAllStatesSynced = s_pnp.isAllStatesSynced;

                    /* 3초 이내에 모든 동기화 SDO가 도착해야 함 */
                    if (preOpCurrentTime - preOpLastSyncStatesTime > CM_PNP_SYNC_TIMEOUT_MS) {
                        _CM_PnP_GoToStopped();
                        break;
                    }

                    if (preOpIsAllStatesSynced) {
                        /* 동기화 완료 → CM에게 OPERATIONAL 전환 명령 */
                        CM_SendSetNMTState(CM_NMT_OPERATIONAL);
                        s_pnp.lastSdoSentMs = preOpCurrentTime;
                        s_pnp.lastCmHeartbeatMs = preOpCurrentTime;
                        s_pnp.preOpState = CM_PRE_OP_WAIT_OPERATIONAL_HB;
                    }
                    break;
                }

                case CM_PRE_OP_WAIT_OPERATIONAL_HB:
                    if (preOpCurrentTime - preOpLastHeartbeatTime > CM_PNP_HEARTBEAT_TIMEOUT_MS) {
                        _CM_PnP_GoToStopped();
                    }
                    break;
            }
            break;
        }

        case CM_NMT_OPERATIONAL:
        {
            uint32_t opLastHeartbeatTime = s_pnp.lastCmHeartbeatMs;
            CM_NmtState_t opLastCmState = s_pnp.lastKnownCmState;
            uint32_t last_xm_hb_time_local = s_pnp_last_xm_hb_time;
            uint32_t opCurrentTime = IOIF_TIM_GetTick();

            /* ===== CM Heartbeat 타임아웃 검사 =====
             * [변경] 1초 → 3초 (.cursorrules 표준 통일)
             * [이유] Breakpoint 디버깅 시 잦은 재연결 방지
             */
            if ((opCurrentTime - opLastHeartbeatTime > CM_PNP_HEARTBEAT_TIMEOUT_MS) ||
                (opLastCmState != CM_NMT_OPERATIONAL)) {
                _CM_PnP_GoToStopped();
                break;
            }

            /* 주기적인 XM Heartbeat 전송 (200ms) */
            if (opCurrentTime - last_xm_hb_time_local > CM_PNP_XM_HEARTBEAT_INTERVAL_MS) {
                CM_SendHeartbeat(s_pnp.nmtState);
                s_pnp_last_xm_hb_time = opCurrentTime;
            }

            /* 데이터 동기화 검사 */
            if (CM_IsDataReady()) {
                s_pnp.isCmSynced = true;
            }
            break;
        }

        case CM_NMT_STOPPED:
        {
            uint32_t stopCurrentTime = IOIF_TIM_GetTick();

            if (s_pnp_stopped_entry_time == 0) {
                s_pnp_stopped_entry_time = stopCurrentTime;
                s_pnp_is_reset_cmd_sent = false;
            }

            /* 재연결 시도 1초 전, CM에게 리셋 명령 전송 */
            if (!s_pnp_is_reset_cmd_sent &&
                (stopCurrentTime - s_pnp_stopped_entry_time > CM_PNP_STOPPED_RESET_CMD_MS)) {
                CM_SendSetNMTState(CM_NMT_STOPPED);
                s_pnp_is_reset_cmd_sent = true;
            }

            /* 3초 후, INITIALISING으로 재진입 (재연결 시도) */
            if (stopCurrentTime - s_pnp_stopped_entry_time > CM_PNP_STOPPED_WAIT_MS) {
                s_pnp.nmtState = CM_NMT_INITIALISING;
                s_pnp_stopped_entry_time = 0;
            }
            break;
        }
    }

    /* LED 상태 업데이트 (10ms~100ms 주기)
     * CM_NmtState_t와 LinkState_t는 동일 순서 (0=INIT, 1=PRE_OP, 2=OP, 3=STOPPED) */
    LedManager_SetLinkState((LinkState_t)s_pnp.nmtState);
}

/**
 * @brief CM과의 PnP 연결 완료 여부 반환
 * @return true: OPERATIONAL + PDO 스트림 시작됨, false: 미연결
 */
bool CM_Drv_IsConnected(void)
{
    return s_pnp.isCmConnected && s_pnp.isCmSynced;
}

/**
 * @brief CM과의 PnP NMT 상태 반환
 * @return CM_NmtState_t 현재 XM→CM PnP NMT 상태
 */
CM_NmtState_t CM_Drv_GetNmtState(void)
{
    return s_pnp.nmtState;
}

// --- NMT 및 PnP 관련 API (SDO 전송 함수) ---
/**
 * @brief CM에게 전송을 요청할 PDO 목록을 SDO로 전송합니다.
 * @details PnP 과정 중 PRE-OPERATIONAL 상태에서 호출됩니다.
 */
void CM_SendSetPDOMapping(void)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    // XM이 CM으로부터 수신할 PDO 목록 (약속의 정의)
    static const uint8_t pdo_list[] = {
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_H10_ASSIST_LOOP_CNT,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_THIGH_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_THIGH_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_PELVIC_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_KNEE_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_KNEE_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_FOOT_CONTACT,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_FOOT_CONTACT,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_FORWARD_VELOCITY,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_TORQUE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_TORQUE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_MOTOR_ANGLE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_MOTOR_ANGLE,
        // CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_FRONTAL_ROLL,
        // CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_FORNTAL_ROLL,
        // CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_SAGITTAL_PITCH,
        // CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_SAGITTAL_PITCH,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_X,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_Y,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_Z,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_X,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_Y,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_Z,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_X,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_Y,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_Z,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_X,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_Y,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_Z,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_FSM_CURRENT_STATE,
        CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_POST_PROCESSING_CNT,
    };

    // XM 자신의 수신 맵(s_cm_pdoRxMap)을 이 리스트 기준으로 미리 채웁니다.
    s_cm_currentPdoRxMapSize = 0;
    for (int i = 0; i < sizeof(pdo_list); i += 2) {
        if (s_cm_currentPdoRxMapSize < CM_MAX_PDO) {
            s_cm_pdoRxMap[s_cm_currentPdoRxMapSize].dictID = pdo_list[i];
            s_cm_pdoRxMap[s_cm_currentPdoRxMapSize].objID  = pdo_list[i+1];
            s_cm_currentPdoRxMapSize++;
        }
    }

    memcpy(s_cm.txData.pdoMappingBuffer, pdo_list, sizeof(pdo_list));
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_PDO_MAPPING, SDO_REQU, sizeof(pdo_list));
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief CM의 NMT 상태 변경을 요청하는 SDO를 전송합니다.
 * @param[in] targetState 목표 NMT 상태 (예: NMT_STATE_OPERATIONAL).
 */
void CM_SendSetNMTState(uint8_t targetState)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    s_cm.txData.cmNmtStateCmd = targetState;
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CMD_SET_CM_NMT_STATE, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);
    
    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief XM(자신)의 부팅 완료를 알리는 Boot-up SDO 메시지를 보냅니다.
 * @details PnP(Plug and Play) 과정의 가장 첫 단계로, 전원이 켜지고 초기화가 완료되었음을
 * 네트워크의 다른 장치(CM)에게 알리기 위해 호출됩니다.
 * 주로 NMT 'INITIALISING' 상태에서 전송됩니다.
 */
void CM_SendBootup(void)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_NOTIFY_XM_BOOTUP, SDO_REQU, 0);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);
    
    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief XM(자신)의 생존 신호를 알리는 Heartbeat SDO 메시지를 보냅니다.
 * @details PnP 과정이 완료된 후, NMT 'OPERATIONAL' 상태에서 주기적으로 호출되어
 * 자신이 여전히 활성 상태이며 네트워크에 연결되어 있음을 알립니다.
 * 이를 통해 다른 장치는 이 노드의 통신 상태를 감시할 수 있습니다.
 */
void CM_SendHeartbeat(uint8_t xmNmtState)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    s_cm.txData.xmNmtState = xmNmtState;
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_NOTIFY_XM_HEARTBEAT, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);
    
    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief CM에게 현재 상태 변수(h10Mode 등) 동기화를 요청하는 SDO를 전송합니다.
 * @details PnP(Plug and Play) 과정 중 NMT 'PRE_OPERATIONAL' 상태에서
 * PDO 매핑이 성공한 직후 호출됩니다.
 * * 이 SDO(CM_SDO_ID_X2C_SYNC_STATES)를 수신한 CM은,
 * 자신의 SDO 콜백을 통해 현재 h10Mode, AssistLevel 등의
 * 핵심 상태 값들을 SDO로 XM에게 전송(응답)합니다.
 * * 이를 통해 XM은 재연결 시에도 CM의 최신 상태를 동기화할 수 있습니다.
 */
void CM_SendSyncStates(void)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    // CM_SDO_ID_X2C_SYNC_STATES SDO를 전송합니다. 데이터 페이로드는 없습니다.
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SYNC_STATES, SDO_REQU, 0);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);
    
    _CM_SendSdoMsg(&sdoMsg);
}

/* ===== 외부 업데이트 함수 Deprecated =====
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: cm_xm_link.c 콜백에서 이 함수들을 호출하여 데이터 업데이트
 * - TO-BE: SDO 콜백에서 s_cm_rx_buffer를 직접 업데이트 (내부화 완료)
 * - 이 함수들은 하위 호환성을 위해 유지하되, 내부적으로만 사용됩니다.
 *   향후 외부 호출부가 없어지면 삭제 가능합니다.
 */
void CM_UpdateH10Mode(CM_H10Mode_t newMode)
{
    s_cm_rx_buffer.sdo.h10Mode = newMode;
}

void CM_UpdateAssistLevel(uint8_t newLevel)
{
    s_cm_rx_buffer.sdo.h10AssistLevel = newLevel;
}

void CM_UpdatePVectorCompletedRH(uint8_t isCompleted)
{
    s_cm_rx_buffer.sdo.isPVectorRHDone = isCompleted;
}

void CM_UpdatePVectorCompletedLH(uint8_t isCompleted)
{
    s_cm_rx_buffer.sdo.isPVectorLHDone = isCompleted;
}

void CM_UpdateNeutralPosSet(void)
{
    s_cm_rx_buffer.sdo.h10NeutralPosSet = true;
}

// --- 주 기능 API - SDO (주로 Apps 계층에서 사용) ---
/**
 * @brief 사용자 신체 정보 SDO를 전송합니다.
 * @param[in] bodyData 8개의 uint32_t 값을 담은 배열.
 */
void CM_SendUserBodyData(const uint32_t bodyData[8])
{
    if (bodyData == NULL) return;

    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    memcpy(s_cm.txData.userBodyData, bodyData, sizeof(uint32_t) * 8);
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_USER_BODY_DATA, SDO_REQU, 8);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)에 P-Vector 궤적 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (SYS_NODE_ID_RH 또는 SYS_NODE_ID_LH).
 * @param[in] pVector   전송할 P-Vector 데이터를 담은 구조체 포인터.
 */
void CM_SendPVector(SystemNodeID_t nodeId, const CM_PVector_t* pVector)
{
    if (pVector == NULL) return;

    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    switch(nodeId) {
        case SYS_NODE_ID_RH:
            memcpy(&s_cm.txData.pVectorRH, pVector, sizeof(CM_PVector_t));
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_YD_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_L_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_S0_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_SD_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        case SYS_NODE_ID_LH:
            memcpy(&s_cm.txData.pVectorLH, pVector, sizeof(CM_PVector_t));
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_YD_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_L_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_S0_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_SD_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        default:
            return; // 지원하지 않는 관절
    }
    
    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)에 P-Vector Reset SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (SYS_NODE_ID_RH 또는 SYS_NODE_ID_LH).
 */
void CM_SendPVectorReset(SystemNodeID_t nodeId)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    switch(nodeId) {
        case SYS_NODE_ID_RH:
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_RESET_RH, SDO_REQU, 0);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        case SYS_NODE_ID_LH:
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_RESET_LH, SDO_REQU, 0);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        default:
            return; // 지원하지 않는 관절
    }
    
    _CM_SendSdoMsg(&sdoMsg);
}
// [중요] CM_UpdateH10Mode, CM_UpdateAssistLevel, CM_UpdatePVectorCompletedRH/LH 함수들은
// CM_Drv_ProcessSdo -> _CM_UnpackSDO -> _CM_SdoCallback... 을 통해 호출됩니다.
// 이미 CM_Drv_ProcessSdo에서 Mutex를 잡았으므로, 이 함수들 내부에는 Mutex를 추가하면 안 됩니다. (Deadlock 발생)
// 현재 코드는 이 구조를 잘 따르고 있습니다.
/**
 * @brief P-Vector 완료 상태 플래그를 false로 리셋합니다.
 * @note Thread-Safe: 내부에서 Mutex로 보호됨
 */
void CM_ClearPVectorCompletedFlag(SystemNodeID_t nodeId)
{
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Task context — SDO 필드, ISR(PDO)와 겹치지 않음 */
    if (nodeId == SYS_NODE_ID_RH) {
        s_cm_rx_buffer.sdo.isPVectorRHDone = false;
    } else if (nodeId == SYS_NODE_ID_LH) {
        s_cm_rx_buffer.sdo.isPVectorLHDone = false;
    }
#else
    if (CM_DATA_LOCK_WRITE()) {
        if (nodeId == SYS_NODE_ID_RH) {
            s_cm_rx_buffer.sdo.isPVectorRHDone = false;
        } else if (nodeId == SYS_NODE_ID_LH) {
            s_cm_rx_buffer.sdo.isPVectorLHDone = false;
        }
        CM_DATA_UNLOCK();
    }
#endif
}

/**
 * @brief 지정된 관절(Node)에 I-Vector Setting을 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (예: SYS_NODE_ID_RH).
 * @param[in] iVector   전송할 I-Vector 데이터를 담은 구조체 포인터.
 */
void CM_SendIVector(SystemNodeID_t nodeId, const CM_IVector_t* iVector)
{
    if (iVector == NULL) return;

    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            memcpy(&s_cm.txData.iVectorRH, iVector, sizeof(CM_IVector_t));
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        case SYS_NODE_ID_LH:
            memcpy(&s_cm.txData.iVectorLH, iVector, sizeof(CM_IVector_t));
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        default:
            return; // 지원하지 않는 관절
    }

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)에 I-Vector Setting 중 Kp와 Kd max값 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (예: SYS_NODE_ID_RH).
 * @param[in] iVector   전송할 I-Vector 데이터를 담은 구조체 포인터.
 */
void CM_SendIVectorKpKdmax(SystemNodeID_t nodeId, const float kpMax, const float kdMax)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            s_cm.txData.iVectorKpMaxRH = kpMax;
            s_cm.txData.iVectorKdMaxRH = kdMax;
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        case SYS_NODE_ID_LH:
            s_cm.txData.iVectorKpMaxLH = kpMax;
            s_cm.txData.iVectorKdMaxLH = kdMax;
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        default:
            return; // 지원하지 않는 관절
    }

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)에 F-Vector 궤적 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] fVector   전송할 F-Vector 데이터를 담은 구조체 포인터.
 */
void CM_SendFVector(SystemNodeID_t nodeId, const CM_FVector_t* fVector)
{
    if (fVector == NULL) return;

    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            memcpy(&s_cm.txData.fVectorRH, fVector, sizeof(CM_FVector_t));
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_RH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        case SYS_NODE_ID_LH:
            memcpy(&s_cm.txData.fVectorLH, fVector, sizeof(CM_FVector_t));
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_LH, SDO_REQU, 1);
            _CM_AppendSDO(&sdoUnit, &sdoMsg);
            break;
        default:
            return; // 지원하지 않는 관절
    }

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)의 각도 제한 루틴을 SDO로 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void CM_SendSetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    CM_SdoToCmID_t sdoId;

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            sdoId = isSet ? CM_SDO_ID_X2C_SET_DEGREE_LIMIT_ROUTINE_RH : CM_SDO_ID_X2C_CLEAR_DEGREE_LIMIT_ROUTINE_RH;
            break;
        case SYS_NODE_ID_LH:
            sdoId = isSet ? CM_SDO_ID_X2C_SET_DEGREE_LIMIT_ROUTINE_LH : CM_SDO_ID_X2C_CLEAR_DEGREE_LIMIT_ROUTINE_LH;
            break;
        default:
            return;
    }
    
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoId, SDO_REQU, 0);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)의 각도 제한 루틴을 SDO로 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void CM_SendSetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    CM_SdoToCmID_t sdoId;

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            sdoId = isSet ? CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_ROUTINE_RH : CM_SDO_ID_X2C_CLEAR_VELOCITY_LIMIT_ROUTINE_RH;
            break;
        case SYS_NODE_ID_LH:
            sdoId = isSet ? CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_ROUTINE_LH : CM_SDO_ID_X2C_CLEAR_VELOCITY_LIMIT_ROUTINE_LH;
            break;
        default:
            return;
    }
    
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoId, SDO_REQU, 0);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)의 외란 관측기(DOB) 루틴을 SDO로 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void CM_SendSetDOBRoutine(SystemNodeID_t nodeId, bool isSet)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    CM_SdoToCmID_t sdoId;

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            sdoId = isSet ? CM_SDO_ID_X2C_SET_DOB_ROUTINE_RH : CM_SDO_ID_X2C_CLEAR_DOB_ROUTINE_RH;
            break;
        case SYS_NODE_ID_LH:
            sdoId = isSet ? CM_SDO_ID_X2C_SET_DOB_ROUTINE_LH : CM_SDO_ID_X2C_CLEAR_DOB_ROUTINE_LH;
            break;
        default:
            return;
    }
    
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoId, SDO_REQU, 0);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)의 가동범위(ROM) 상/하한을 SDO로 설정합니다.
 * @param[in] nodeId      명령을 전달할 관절의 Node ID.
 * @param[in] upperLimit  설정할 가동범위 상한값 (단위: degree).
 * @param[in] lowerLimit  설정할 가동범위 하한값 (단위: degree).
 */
void CM_SendDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    CM_SdoToCmID_t sdoIdUpper, sdoIdLower;

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            s_cm.txData.degreeLimitUpperRH = upperLimit;
            s_cm.txData.degreeLimitLowerRH = lowerLimit;
            sdoIdUpper = CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_RH;
            sdoIdLower = CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_RH;
            break;
        case SYS_NODE_ID_LH:
            s_cm.txData.degreeLimitUpperLH = upperLimit;
            s_cm.txData.degreeLimitLowerLH = lowerLimit;
            sdoIdUpper = CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_LH;
            sdoIdLower = CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_LH;
            break;
        default:
            return;
    }

    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoIdUpper, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoIdLower, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)의 가동속도범위 상/하한을 설정합니다.
 * @param[in] nodeId      명령을 전달할 관절의 Node ID.
 * @param[in] upperLimit  설정할 가동속도범위 상한값 (단위: deg/s).
 * @param[in] lowerLimit  설정할 가동속도범위 하한값 (단위: deg/s).
 */
void CM_SendVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    CM_SdoToCmID_t sdoIdUpper, sdoIdLower;

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            s_cm.txData.velocityLimitUpperRH = upperLimit;
            s_cm.txData.velocityLimitLowerRH = lowerLimit;
            sdoIdUpper = CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_RH;
            sdoIdLower = CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_RH;
            break;
        case SYS_NODE_ID_LH:
            s_cm.txData.velocityLimitUpperLH = upperLimit;
            s_cm.txData.velocityLimitLowerLH = lowerLimit;
            sdoIdUpper = CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_LH;
            sdoIdLower = CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_LH;
            break;
        default:
            return;
    }

    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoIdUpper, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoIdLower, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)의 Normal Compensation Gain을 SDO로 설정합니다.
 * @details 주로 중력 보상과 같은 일반적인 보상 로직의 강도를 조절하는 데 사용됩니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] gain      설정할 게인 값.
 */
void CM_SendNormalCompGain(SystemNodeID_t nodeId, uint8_t gain)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    CM_SdoToCmID_t sdoId;
    
    switch(nodeId) {
        case SYS_NODE_ID_RH:
            s_cm.txData.normalCompGainRH = gain;
            sdoId = CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_RH;
            break;
        case SYS_NODE_ID_LH:
            s_cm.txData.normalCompGainLH = gain;
            sdoId = CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_LH;
            break;
        default:
            return;
    }

    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoId, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief 지정된 관절(Node)의 Resistive Compensation Gain을 SDO로 설정합니다.
 * @details 저항 모드(Resistive Mode)에서 사용자가 느끼는 저항의 강도를 조절합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] gain      설정할 저항 게인 값.
 */
void CM_SendResistiveCompGain(SystemNodeID_t nodeId, float gain)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);
    
    CM_SdoToCmID_t sdoId;

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            s_cm.txData.resistiveCompGainRH = gain;
            sdoId = CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_RH;
            break;
        case SYS_NODE_ID_LH:
            s_cm.txData.resistiveCompGainLH = gain;
            sdoId = CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_LH;
            break;
        default:
            return;
    }
    
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, sdoId, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);

    _CM_SendSdoMsg(&sdoMsg);
}

/**
 * @brief H10의 기존 보조 모드를 설정하는 SDO를 전송합니다.
 * @param[in] H10AssistModeEnable H10 보조 모드 활성화 (0 비활성화 또는 1 활성화).
 */
void CM_SendSetH10AssistExistingMode(bool H10AssistModeEnable)
{
    DOPI_SDOMsg_t sdoMsg;
    DOPI_SDOUnit_t sdoUnit;
    DOPI_ClearSDO(&sdoMsg);

    s_cm.txData.H10ExistingAssistModeFlag = (uint8_t)H10AssistModeEnable;
    sdoUnit = DOPI_CreateSDOUnit(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_H10_ORIGINAL_ASSIST_MODE, SDO_REQU, 1);
    _CM_AppendSDO(&sdoUnit, &sdoMsg);
    
    _CM_SendSdoMsg(&sdoMsg);
}

// --- 주 기능 API - PDO (주로 Apps 계층, 실시간 제어 루프에서 사용) ---
/**
 * @brief 지정된 관절(Node)의 보조 토크를 PDO 전송 대기열에 추가합니다.
 * @details 이 함수는 즉시 전송하지 않고, 전송할 데이터를 '스테이징'만 합니다.
 * 실제 전송은 CM_FlushControlPDOs() 호출 시 이루어집니다.
 * @param[in] nodeId    토크를 인가할 관절의 Node ID.
 * @param[in] torque    보조 토크 값 (단위: Nm).
 */
void CM_StageAuxTorque(SystemNodeID_t nodeId, float torque)
{
    float limited_torque = fminf(10.0f, fmaxf(-10.0f, torque));
    int16_t scaled_torque = (int16_t)(limited_torque * 100.0f);

    switch(nodeId) {
        case SYS_NODE_ID_RH:
            s_cm.txData.pdo.auxTorqueInputRH = scaled_torque;
            break;
        case SYS_NODE_ID_LH:
            s_cm.txData.pdo.auxTorqueInputLH = scaled_torque;
            break;
        default:
            break;
    }
}

/**
 * @brief 대기열에 있는 모든 제어 관련 PDO를 하나의 CAN 메시지로 묶어 보냅니다.
 * @details 제어 루프의 마지막에 주기적으로 호출되어야 합니다.
 */
void  CM_FlushControlPDOs(void)
{
    // 1. 전송할 CAN 페이로드의 크기는 PnP에서 약속된 pdo 블록의 크기입니다.
    const uint32_t payloadSize = sizeof(s_cm.txData.pdo);

    // 2. txFunc를 통해 txData.pdo 블록을 memcpy 없이 통째로 전송합니다.
    if (s_cm.txFunc) {
        s_cm.txFunc(s_cm.pdoTxId,
                    (uint8_t*)&s_cm.txData.pdo, // pdo 블록의 시작 주소
                    payloadSize);
    }
}

/**
 *------------------------------------------------------------
 *                      PRIVATE FUNCTIONS
 *------------------------------------------------------------
 */

// --- Object Dictionary Creation Functions ---
/**
 * @brief CM-XM 통신 전용 Object Dictionary를 생성하고 내부 변수에 연결합니다.
 */
static void _CM_CreateObjectDictionary(void)
{
    s_cm.cmDevObj.numOfTask = CM_DICT_ID_COUNT;

    s_cm.dods[CM_DICT_ID_CM_TO_XM].dictID = CM_DICT_ID_CM_TO_XM;
    s_cm.dods[CM_DICT_ID_XM_TO_CM].dictID = CM_DICT_ID_XM_TO_CM;

    //================================================================================
    // Dictionary 0: CM -> XM (수신용 객체)
    //================================================================================
    // --- SDOs (CM -> XM): SDO 수신 시 호출될 콜백을 등록합니다. ---
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_NOTIFY_CM_BOOTUP, DOP_UINT8, _CM_SdoCallback_OnCmBootup);
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_NOTIFY_CM_HEARTBEAT, DOP_UINT8, _CM_SdoCallback_OnCmHeartbeat);
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_GET_H10_MODE, DOP_UINT8, _CM_SdoCallback_OnH10ModeChanged);
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_SYNC_STATES, DOP_UINT8, _CM_SdoCallback_OnSuitSyncStates);
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_GET_H10_ASSIST_LEVEL, DOP_UINT8, _CM_SdoCallback_OnAssistLevelChanged);
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_NOTIFY_PVECTOR_DONE_RH, DOP_UINT8, _CM_SdoCallback_OnPVectorCompletedRH);
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_NOTIFY_PVECTOR_DONE_LH, DOP_UINT8, _CM_SdoCallback_OnPVectorCompletedLH);
    _CM_CreateSDO(CM_DICT_ID_CM_TO_XM, CM_SDO_ID_C2X_GET_H10_NEUTRAL_POS_SETFLAG, DOP_UINT8, _CM_SdoCallback_OnNeutralPosSet);

    // --- PDOs (CM -> XM): 수신된 PDO 데이터가 저장될 내부 변수 주소를 연결합니다. ---
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_H10_ASSIST_LOOP_CNT, DOP_UINT32, 1, &s_cm.rawPdoData.h10AssistModeLoopCnt);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.leftHipAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.rightHipAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_THIGH_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.leftThighAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_THIGH_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.rightThighAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_PELVIC_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.pelvicAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_PELVIC_VEL_Y, DOP_INT16, 1, &s_cm.rawPdoData.pelvicVelY);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_KNEE_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.leftKneeAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_KNEE_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.rightKneeAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_FOOT_CONTACT, DOP_UINT8, 1, &s_cm.rawPdoData.isLeftFootContact);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_FOOT_CONTACT, DOP_UINT8, 1, &s_cm.rawPdoData.isRightFootContact);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_GAIT_STATE, DOP_UINT8, 1, &s_cm.rawPdoData.gaitState);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_GAIT_CYCLE, DOP_UINT8, 1, &s_cm.rawPdoData.gaitCycle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_FORWARD_VELOCITY, DOP_INT16, 1, &s_cm.rawPdoData.forwardVelocity);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_TORQUE, DOP_INT16, 1, &s_cm.rawPdoData.leftHipTorque);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_TORQUE, DOP_INT16, 1, &s_cm.rawPdoData.rightHipTorque);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_MOTOR_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.leftHipMotorAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_MOTOR_ANGLE, DOP_INT16, 1, &s_cm.rawPdoData.rightHipMotorAngle);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_FRONTAL_ROLL, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuFrontalRoll);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_SAGITTAL_PITCH, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuSagittalPitch);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_FORNTAL_ROLL, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuFrontalRoll);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_SAGITTAL_PITCH, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuSagittalPitch);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_X, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuGlobalAccX);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_Y, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuGlobalAccY);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_Z, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuGlobalAccZ);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_X, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuGlobalGyrX);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_Y, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuGlobalGyrY);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_Z, DOP_INT16, 1, &s_cm.rawPdoData.leftHipImuGlobalGyrZ);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_X, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuGlobalAccX);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_Y, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuGlobalAccY);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_Z, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuGlobalAccZ);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_X, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuGlobalGyrX);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_Y, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuGlobalGyrY);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_Z, DOP_INT16, 1, &s_cm.rawPdoData.rightHipImuGlobalGyrZ);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_FSM_CURRENT_STATE, DOP_UINT8, 1, &s_cm.rawPdoData.h10FSMcurrentState);
    _CM_CreatePDO(CM_DICT_ID_CM_TO_XM, CM_PDO_ID_C2X_GET_POST_PROCESSING_CNT, DOP_UINT32, 1, &s_cm.rawPdoData.postProcessingCnt);

    //================================================================================
    // Dictionary 1: XM -> CM (송신용 객체)
    //================================================================================
    
    // --- SDOs (XM -> CM): 송신 데이터가 담길 내부 변수 주소를 연결 (콜백은 NULL) ---
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_PDO_MAPPING, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_PDO_MAPPING, s_cm.txData.pdoMappingBuffer);
    
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_NOTIFY_XM_BOOTUP, DOP_UINT8, NULL); // bootup은 데이터가 없으므로 주소 연결 불필요
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CMD_SET_CM_NMT_STATE, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CMD_SET_CM_NMT_STATE, &s_cm.txData.cmNmtStateCmd);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_NOTIFY_XM_HEARTBEAT, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_NOTIFY_XM_HEARTBEAT, &s_cm.txData.xmNmtState);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SYNC_STATES, DOP_UINT8, NULL); // sync는 데이터가 없으므로 주소 연결 불필요
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_USER_BODY_DATA, DOP_UINT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_USER_BODY_DATA, s_cm.txData.userBodyData);

    // P-Vector (Right Hip)
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_YD_RH, DOP_INT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_YD_RH, &s_cm.txData.pVectorRH.yd);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_L_RH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_L_RH, &s_cm.txData.pVectorRH.L);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_S0_RH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_S0_RH, &s_cm.txData.pVectorRH.s0);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_SD_RH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_SD_RH, &s_cm.txData.pVectorRH.sd);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_RESET_RH, DOP_UINT8, NULL); // pVector Reset은 데이터가 없으므로 주소 연결 불필요

    // P-Vector (Left Hip)
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_YD_LH, DOP_INT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_YD_LH, &s_cm.txData.pVectorLH.yd);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_L_LH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_L_LH, &s_cm.txData.pVectorLH.L);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_S0_LH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_S0_LH, &s_cm.txData.pVectorLH.s0);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_SD_LH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_SD_LH, &s_cm.txData.pVectorLH.sd);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_P_VECTOR_RESET_LH, DOP_UINT8, NULL); // pVector Reset은 데이터가 없으므로 주소 연결 불필요

    // I-Vector (Right Hip)
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_RH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_RH, &s_cm.txData.iVectorRH.epsilon);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_RH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_RH, &s_cm.txData.iVectorRH.kp);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_RH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_RH, &s_cm.txData.iVectorRH.kd);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_RH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_RH, &s_cm.txData.iVectorRH.lambda);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_RH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_RH, &s_cm.txData.iVectorRH.duration);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_RH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_RH, &s_cm.txData.iVectorKpMaxRH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_RH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_RH, &s_cm.txData.iVectorKdMaxRH);

    // I-Vector (Left Hip)
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_LH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_LH, &s_cm.txData.iVectorLH.epsilon);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_LH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_LH, &s_cm.txData.iVectorLH.kp);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_LH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_LH, &s_cm.txData.iVectorLH.kd);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_LH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_LH, &s_cm.txData.iVectorLH.lambda);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_LH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_LH, &s_cm.txData.iVectorLH.duration);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_LH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_LH, &s_cm.txData.iVectorKpMaxLH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_LH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_LH, &s_cm.txData.iVectorKdMaxLH);

    // F-Vector (Right Hip)
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_RH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_RH, &s_cm.txData.fVectorRH.modeIdx);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_RH, DOP_INT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_RH, &s_cm.txData.fVectorRH.tauMax);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_RH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_RH, &s_cm.txData.fVectorRH.delay);

    // F-Vector (Left Hip)
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_LH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_LH, &s_cm.txData.fVectorLH.modeIdx);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_LH, DOP_INT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_LH, &s_cm.txData.fVectorLH.tauMax);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_LH, DOP_UINT16, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_LH, &s_cm.txData.fVectorLH.delay);

    // Degree Limit ROM & Routines
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_ROUTINE_RH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CLEAR_DEGREE_LIMIT_ROUTINE_RH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_ROUTINE_LH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CLEAR_DEGREE_LIMIT_ROUTINE_LH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_RH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_RH, &s_cm.txData.degreeLimitUpperRH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_RH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_RH, &s_cm.txData.degreeLimitLowerRH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_LH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_LH, &s_cm.txData.degreeLimitUpperLH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_LH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_LH, &s_cm.txData.degreeLimitLowerLH);
    
    // Velocity Limit ROM & Routines
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_ROUTINE_RH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CLEAR_VELOCITY_LIMIT_ROUTINE_RH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_ROUTINE_LH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CLEAR_VELOCITY_LIMIT_ROUTINE_LH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_RH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_RH, &s_cm.txData.velocityLimitUpperRH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_RH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_RH, &s_cm.txData.velocityLimitLowerRH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_LH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_LH, &s_cm.txData.velocityLimitUpperLH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_LH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_LH, &s_cm.txData.velocityLimitLowerLH);

    // DOB Routines
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DOB_ROUTINE_RH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CLEAR_DOB_ROUTINE_RH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_DOB_ROUTINE_LH, DOP_UINT8, NULL);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_CLEAR_DOB_ROUTINE_LH, DOP_UINT8, NULL);

    // Gravity & Velocity Compensation Gain
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_RH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_RH, &s_cm.txData.normalCompGainRH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_LH, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_LH, &s_cm.txData.normalCompGainLH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_RH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_RH, &s_cm.txData.resistiveCompGainRH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_LH, DOP_FLOAT32, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_LH, &s_cm.txData.resistiveCompGainLH);
    _CM_CreateSDO(CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_H10_ORIGINAL_ASSIST_MODE, DOP_UINT8, NULL);
    DOPI_SetSDOAddr(&s_cm.cmDevObj, CM_DICT_ID_XM_TO_CM, CM_SDO_ID_X2C_SET_H10_ORIGINAL_ASSIST_MODE, &s_cm.txData.H10ExistingAssistModeFlag);
    
    
    // --- PDOs (XM -> CM): 송신 데이터가 담길 내부 변수 주소를 연결합니다. ---
    _CM_CreatePDO(CM_DICT_ID_XM_TO_CM, CM_PDO_ID_X2C_SET_AUX_TORQUE_RH, DOP_INT16, 1, &s_cm.txData.pdo.auxTorqueInputRH);
    _CM_CreatePDO(CM_DICT_ID_XM_TO_CM, CM_PDO_ID_X2C_SET_AUX_TORQUE_LH, DOP_INT16, 1, &s_cm.txData.pdo.auxTorqueInputLH);
}

/**
 * @brief 전용 SDO 테이블에 SDO 객체를 생성하고 콜백을 등록합니다.
 * (기존 _CmLink_CreateSDO 로직)
 */
static void _CM_CreateSDO(CM_DictID_t dictId, uint8_t objId, uint8_t type, DOP_SDOCB_t callback)
{
    DOP_SDO_t* sdo = &s_cm.dods[dictId].SDOs[objId];
    sdo->objID            = objId;
    sdo->dataType         = type;
    sdo->callback         = callback;
    sdo->args.status      = DOP_SDO_IDLE;
    sdo->args.dataSize    = 0;
    sdo->args.data        = NULL;
    sdo->args.typeSize    = DOP_GetDataTypeInfo(type).typeSize;
}

/**
 * @brief 전용 PDO 테이블에 PDO 객체를 생성하고 변수 주소를 연결합니다.
 * (기존 _CmLink_CreatePDO 로직)
 */
static void _CM_CreatePDO(CM_DictID_t dictId, uint8_t objId, uint8_t type, uint8_t size, void* addr)
{
    DOP_PDO_t* pdo = &s_cm.dods[dictId].PDOs[objId];
    pdo->objID     = objId;
    pdo->dataType  = type;
    pdo->dataSize  = size;
    pdo->addr      = addr;
    pdo->objSize   = DOP_GetDataTypeInfo(type).typeSize * size;
    pdo->lastPub   = malloc(pdo->objSize);
    if (pdo->lastPub != NULL) {
        memset(pdo->lastPub, 0xFF, pdo->objSize);
    }
}

// --- Message Unpacking Functions (Rx Path) ---
/**
 * @brief 수신된 SDO 메시지 묶음을 언패킹합니다.
 */
static int _CM_UnpackSDO(const uint8_t* data, uint8_t len)
{
    int cursor = 0;
    uint8_t num_of_sdo = 0;
    memcpy(&num_of_sdo, &data[cursor++], 1);

    for (int i = 0; i < num_of_sdo; ++i) {
        int bytes_read = _CM_ReadSDO(&data[cursor]);
        if (bytes_read > 0) {
            cursor += bytes_read;
        } else {
            return -1; // SDO 처리 실패
        }
    }
    return 0; // 성공
}

/**
 * @brief SDO 메시지 하나를 읽고 처리합니다.
 */
static int _CM_ReadSDO(const uint8_t* byte_arr)
{
    int bytes_read = 0;
    DOP_Header_t header;
    memcpy(&header, byte_arr, sizeof(DOP_Header_t));
    bytes_read += sizeof(DOP_Header_t);

    DOP_SDO_t* sdo = _CM_FindSDO(header.dictID, header.objID);
    if (sdo == NULL) return -1;

    uint16_t req_bytes = 0;
    DOP_SDOArgs_t req_args = Bytes2SDOreq((uint8_t*)byte_arr + bytes_read, &req_bytes);
    req_args.typeSize = sdo->args.typeSize;
    bytes_read += req_bytes;

    if (req_args.status == DOP_SDO_REQU) {
        DOP_SDOArgs_t res_args = {0};
        int data_bytes = _CM_CallSDO(sdo, &req_args, &res_args);
        if (data_bytes >= 0) {
            bytes_read += data_bytes;
        }
    } 
    else if (req_args.status == DOP_SDO_SUCC || req_args.status == DOP_SDO_FAIL) {
        /* ===== SDO Response 내부 처리 (cm_xm_link.c _Link_OnSdoResponse 통합) =====
         * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
         * - AS-IS: s_cm.callbacks.onSdoResponse(dictId, objId, isSuccess)
         * - TO-BE: _CM_PnP_OnSdoResponse() 직접 호출
         */
        _CM_PnP_OnSdoResponse(header.dictID, header.objID, (int8_t)req_args.status);
    }
    return bytes_read;
}

/**
 * @brief SDO 콜백을 실행하고 처리된 데이터 길이를 반환합니다.
 */
static int _CM_CallSDO(DOP_SDO_t* sdo, DOP_SDOArgs_t* req, DOP_SDOArgs_t* res) 
{
    if (sdo == NULL || sdo->callback == NULL) return -1;
    sdo->callback(req, res);
    uint8_t type_size = DOP_GetDataTypeInfo(sdo->dataType).typeSize;
    return req->dataSize * type_size;
}

/**
 * @brief 수신된 PDO 메시지 묶음을 언패킹합니다.
 */
static int _CM_UnpackPDO(const uint8_t* data, uint8_t len)
{
    // int cursor = 0;
    // uint8_t num_of_pdo = 0;
    // memcpy(&num_of_pdo, &data[cursor++], 1);

    // for (int i = 0; i < num_of_pdo; ++i) {
    //     int bytes_read = _CM_ReadPDO(&data[cursor]);
    //     if (bytes_read > 0) {
    //         cursor += bytes_read;
    //     } else {
    //         return -1; // PDO 처리 실패
    //     }
    // }
    // return 0; // 
    
    int cursor = 0;
    for (int i = 0; i < s_cm_currentPdoRxMapSize; i++) {
        DOP_Header_t header = s_cm_pdoRxMap[i];
        DOP_PDO_t* pdo = _CM_FindPDO(header.dictID, header.objID);

        if (pdo && pdo->addr && (cursor + pdo->objSize <= len)) {
            // 약속된 순서대로 s_cm.rawPdoData에 복사
            memcpy(pdo->addr, &data[cursor], pdo->objSize);
            cursor += pdo->objSize;
        } else {
            // PnP 약속과 실제 데이터 크기가 맞지 않음
            break;
        }
    }
    return cursor;
}

// /**
//  * @brief PDO 메시지 하나를 읽고 내부 변수에 저장합니다.
//  */
// static int _CM_ReadPDO(const uint8_t* byte_arr)
// {
//     int cursor = 0;
//     DOP_Header_t header;
//     memcpy(&header, &byte_arr[cursor], sizeof(DOP_Header_t));
//     cursor += sizeof(DOP_Header_t);

//     DOP_PDO_t* pdo = _CM_FindPDO(header.dictID, header.objID);
//     if (pdo == NULL || pdo->addr == NULL) return -1;

//     memcpy(pdo->addr, &byte_arr[cursor], pdo->objSize);
//     cursor += pdo->objSize;

//     return cursor;
// }

// --- Data Processing Functions (Final Rx Step) ---
/**
 * @brief 수신된 원시 PDO 데이터를 실제 물리 단위(float, bool 등)로 변환하여
 * 최종 rxData 구조체에 저장합니다.
 */
static void _CM_DecodeRxPDOData(CM_RxPdoData_t* dst_buf)
{
    // --- Loop & State Data ---
    dst_buf->h10AssistModeLoopCnt = s_cm.rawPdoData.h10AssistModeLoopCnt;

    // --- Kinematics Data (Scaling Applied) ---
    dst_buf->leftHipAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipAngle, DEG_SCALING_FACTOR);
    dst_buf->rightHipAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipAngle, DEG_SCALING_FACTOR);
    dst_buf->leftThighAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftThighAngle, DEG_SCALING_FACTOR);
    dst_buf->rightThighAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightThighAngle, DEG_SCALING_FACTOR);
    dst_buf->pelvicAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.pelvicAngle, DEG_SCALING_FACTOR);
    dst_buf->pelvicVelY = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.pelvicVelY, VELDEG_SCALING_FACTOR);
    dst_buf->leftKneeAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftKneeAngle, DEG_SCALING_FACTOR);
    dst_buf->rightKneeAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightKneeAngle, DEG_SCALING_FACTOR);
    dst_buf->forwardVelocity = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.forwardVelocity, VELDEG_SCALING_FACTOR);
    dst_buf->leftHipMotorAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipMotorAngle, DEG_SCALING_FACTOR);
    dst_buf->rightHipMotorAngle = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipMotorAngle, DEG_SCALING_FACTOR);

    // --- Gait Data ---
    dst_buf->isLeftFootContact = (bool)s_cm.rawPdoData.isLeftFootContact;
    dst_buf->isRightFootContact = (bool)s_cm.rawPdoData.isRightFootContact;
    dst_buf->gaitState = s_cm.rawPdoData.gaitState;
    dst_buf->gaitCycle = s_cm.rawPdoData.gaitCycle;
    
    // --- Torque Data (Scaling Applied) ---
    // TODO : Current -> Torque (T = kt*i)
    dst_buf->leftHipTorque = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipTorque, CURRENT_SCALING_FACTOR);
    dst_buf->rightHipTorque = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipTorque, CURRENT_SCALING_FACTOR);

    // --- IMU Data ---
    dst_buf->leftHipImuFrontalRoll = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuFrontalRoll, DEG_SCALING_FACTOR);
    dst_buf->leftHipImuSagittalPitch = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuSagittalPitch, DEG_SCALING_FACTOR);
    dst_buf->rightHipImuFrontalRoll = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuFrontalRoll, DEG_SCALING_FACTOR);
    dst_buf->rightHipImuSagittalPitch = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuSagittalPitch, DEG_SCALING_FACTOR);
    dst_buf->leftHipImuGlobalAccX = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuGlobalAccX, ACC_SCALING_FACTOR);
    dst_buf->leftHipImuGlobalAccY = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuGlobalAccY, ACC_SCALING_FACTOR);
    dst_buf->leftHipImuGlobalAccZ = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuGlobalAccZ, ACC_SCALING_FACTOR);
    dst_buf->leftHipImuGlobalGyrX = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuGlobalGyrX, GYR_SCALING_FACTOR);
    dst_buf->leftHipImuGlobalGyrY = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuGlobalGyrY, GYR_SCALING_FACTOR);
    dst_buf->leftHipImuGlobalGyrZ = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.leftHipImuGlobalGyrZ, GYR_SCALING_FACTOR);
    dst_buf->rightHipImuGlobalAccX = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuGlobalAccX, ACC_SCALING_FACTOR);
    dst_buf->rightHipImuGlobalAccY = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuGlobalAccY, ACC_SCALING_FACTOR);
    dst_buf->rightHipImuGlobalAccZ = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuGlobalAccZ, ACC_SCALING_FACTOR);
    dst_buf->rightHipImuGlobalGyrX = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuGlobalGyrX, GYR_SCALING_FACTOR);
    dst_buf->rightHipImuGlobalGyrY = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuGlobalGyrY, GYR_SCALING_FACTOR);
    dst_buf->rightHipImuGlobalGyrZ = _CM_ScaleInt16ToFloat(s_cm.rawPdoData.rightHipImuGlobalGyrZ, GYR_SCALING_FACTOR);

    dst_buf->h10FSMcurrentState = s_cm.rawPdoData.h10FSMcurrentState;

    // --- Post-Processing Counter (no scale — raw uint32) ---
    dst_buf->postProcessingCnt = s_cm.rawPdoData.postProcessingCnt;
}

// --- Message Packing Functions (Tx Path) ---
// /**
//  * @brief  데이터 주소를 직접 지정하여 SDO 유닛을 생성합니다. (직접 페이로드 전송용)
//  * @param  t_obj       DOP 장치 객체
//  * @param  t_taskID    Dict ID
//  * @param  t_SDOID     SDO ID
//  * @param  t_SDOStatus SDO 상태 (REQU 등)
//  * @param  payload     전송할 데이터 페이로드의 포인터
//  * @param  payloadSize 전송할 데이터의 전체 바이트 크기
//  * @return 생성된 SDO 유닛
//  */
// static DOPI_SDOUnit_t _CM_CreateSDOUnit_payload(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID, DOP_SDOStatus_t t_SDOStatus, const void* payload, uint8_t payloadSize)
// {
//     DOPI_SDOUnit_t t_unit = {0};

//     t_unit.taskID = t_taskID;
//     t_unit.SDOID = t_SDOID;
//     t_unit.param.SDOStatus = t_SDOStatus;
//     t_unit.param.data = (void*)payload; // 데이터 포인터를 직접 설정

//     // 핵심: 바이트 배열을 보내는 것이므로, 데이터 개수 = 바이트 크기, 타입 크기 = 1로 간주
//     t_unit.param.numOfData = payloadSize; 
    
//     // _CM_AppendSDO 함수에서 타입 크기를 참조하므로,
//     // 이 SDO 객체의 타입 사이즈를 임시로 1로 설정해주는 작업이 필요합니다.
//     DOP_SDO_t* sdo = _CM_FindSDO(t_taskID, t_SDOID); // 또는 DOP_FindSDO
//     if (sdo) {
//         sdo->args.typeSize = sizeof(uint8_t); // 타입 크기를 1로 설정
//     }

//     return t_unit;
// }

/**
 * @brief SDO 유닛을 전송 메시지 버퍼에 패킹합니다.
 */
static int _CM_AppendSDO(DOPI_SDOUnit_t* sdo_unit, DOPI_SDOMsg_t* sdo_msg) 
{
    uint8_t cursor = (sdo_msg->msgLength == 0) ? 1 : sdo_msg->msgLength;

    sdo_msg->txBuf[cursor++] = sdo_unit->taskID;
    sdo_msg->txBuf[cursor++] = sdo_unit->SDOID;
    sdo_msg->txBuf[cursor++] = sdo_unit->param.SDOStatus;
    sdo_msg->txBuf[cursor++] = sdo_unit->param.numOfData;
    
    // 1. SDO의 단일 데이터 요소 크기를 가져옵니다.
    uint8_t element_size = _CM_GetSDO_ElementSize(sdo_unit->taskID, sdo_unit->SDOID);
    if (element_size == 0) {
        // 존재하지 않거나 타입이 정의되지 않은 SDO. 패킹을 중단합니다.
        return -1;
    }
    
    // 2. 총 데이터 크기를 계산합니다.
    uint8_t total_size = sdo_unit->param.numOfData * element_size;
    
    // 3. 데이터 복사 (memcpy)
    memcpy(&sdo_msg->txBuf[cursor], sdo_unit->param.data, total_size);
    cursor += total_size;

    sdo_msg->numOfSDO++;
    sdo_msg->txBuf[0] = sdo_msg->numOfSDO;
    sdo_msg->msgLength = cursor;

    return 0;
}

/**
 * @brief (신규) XM의 SDO 테이블에서 데이터 타입 크기를 가져옵니다.
 * @note  CM의 _CmLink_GetSDOInfo, DOP_ConvertDataSize 와 동일한 로직을 수행합니다.
 */
static uint8_t _CM_GetSDO_ElementSize(uint8_t dictId, uint8_t objId)
{
    // 1. SDO 객체를 찾습니다.
    DOP_SDO_t* sdo = _CM_FindSDO(dictId, objId);
    if (sdo == NULL) {
        // 존재하지 않는 SDO는 크기가 0입니다.
        return 0;
    }

    // 2. SDO 객체 내의 pre-calculated 된 typeSize 대신,
    //    dataType enum을 사용하여 크기를 변환합니다. (CM과 동일 방식)
    //    (DOP_SDO_t 구조체에 dataType 멤버가 있다고 가정합니다.)
    return DOP_ConvertDataSize(sdo->dataType);
}

// /**
//  * @brief PDO 유닛을 전송 메시지 버퍼에 패킹합니다.
//  */
// static int _CM_AppendPDO(DOPI_PDOUnit_t* pdo_unit, DOPI_PDOMsg_t* pdo_msg)
// {
//     uint8_t cursor = (pdo_msg->msgLength == 0) ? 1 : pdo_msg->msgLength;

//     pdo_msg->txBuf[cursor++] = pdo_unit->taskID;
//     pdo_msg->txBuf[cursor++] = pdo_unit->PDOID;

//     DOP_PDO_t* pdo = _CM_FindPDO(pdo_unit->taskID, pdo_unit->PDOID);
//     if (pdo == NULL) return -1;
    
//     memcpy(&pdo_msg->txBuf[cursor], pdo_unit->addr, pdo->objSize);
//     cursor += pdo->objSize;

//     pdo_msg->numOfPDO++;
//     pdo_msg->txBuf[0] = pdo_msg->numOfPDO;
//     pdo_msg->msgLength = cursor;

//     return 0;
// }

// --- PDO Staging Functions (Tx Helper) ---
// /**
//  * @brief PDO를 전송 대기열(Staging List)에 추가합니다.
//  */
// static void _CM_StagePDO(uint8_t dictId, uint8_t objId)
// {
//     if (s_cm.pdoStagingListSize >= CM_PDO_STAGE_MAX_SIZE) return;
    
//     DOP_Header_t header = {dictId, objId};
//     for (int i = 0; i < s_cm.pdoStagingListSize; i++) {
//         if (s_cm.pdoStagingList[i].dictID == dictId && s_cm.pdoStagingList[i].objID == objId) {
//             return;
//         }
//     }
//     s_cm.pdoStagingList[s_cm.pdoStagingListSize++] = header;
// }

// /**
//  * @brief PDO 전송 대기열을 비웁니다.
//  */
// static void _CM_ClearPdoStage(void)
// {
//     s_cm.pdoStagingListSize = 0;
// }

/* ===================================================================
 *  SDO Callback Functions (내부화)
 * ===================================================================
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: s_cm.callbacks.onXXX() 외부 콜백 호출
 * - TO-BE: 내부 PnP 상태 머신 + CM_UpdateXXX() 직접 호출
 * - 이 콜백들은 CM_Drv_ProcessSdo → _CM_UnpackSDO → _CM_CallSDO 경로에서 호출됨
 * - 호출 시점에 이미 Mutex가 획득된 상태이므로, 내부 함수에서 Mutex 재획득 금지
 */

/**
 * @brief CM Boot-up SDO 수신 → 내부 PnP 상태 업데이트
 */
static void _CM_SdoCallback_OnCmBootup(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    _CM_PnP_OnCmBootup();
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

/**
 * @brief CM Heartbeat SDO 수신 → 내부 PnP 상태 업데이트
 */
static void _CM_SdoCallback_OnCmHeartbeat(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    uint8_t newCmNmtState = 0;
    if (req->dataSize > 0 && req->data != NULL) {
        memcpy(&newCmNmtState, req->data, sizeof(newCmNmtState));
    }
    _CM_PnP_OnCmHeartbeat(newCmNmtState);
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

/**
 * @brief 상태 동기화 SDO 수신 → PnP isAllStatesSynced 플래그 설정
 */
static void _CM_SdoCallback_OnSuitSyncStates(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    _CM_PnP_OnSyncStatesReceived();
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

/**
 * @brief H10 Mode 변경 SDO → s_cm_rx_buffer 직접 업데이트
 * @note Mutex 이미 획득 상태 (CM_Drv_ProcessSdo에서)
 */
static void _CM_SdoCallback_OnH10ModeChanged(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    if (req->dataSize > 0 && req->data != NULL) {
        uint8_t newMode;
        memcpy(&newMode, req->data, sizeof(newMode));
        /* 직접 업데이트 (외부 콜백 제거) */
        s_cm_rx_buffer.sdo.h10Mode = (CM_H10Mode_t)newMode;
    }
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

/**
 * @brief Assist Level 변경 SDO → s_cm_rx_buffer 직접 업데이트
 * @note Mutex 이미 획득 상태
 */
static void _CM_SdoCallback_OnAssistLevelChanged(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    if (req->dataSize > 0 && req->data != NULL) {
        uint8_t newLevel;
        memcpy(&newLevel, req->data, sizeof(newLevel));
        s_cm_rx_buffer.sdo.h10AssistLevel = newLevel;
    }
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

/**
 * @brief P-Vector 완료(RH) SDO → s_cm_rx_buffer 직접 업데이트
 * @note Mutex 이미 획득 상태
 */
static void _CM_SdoCallback_OnPVectorCompletedRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    if (req->dataSize > 0 && req->data != NULL) {
        uint8_t isCompleted;
        memcpy(&isCompleted, req->data, sizeof(isCompleted));
        s_cm_rx_buffer.sdo.isPVectorRHDone = isCompleted;
    }
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

/**
 * @brief P-Vector 완료(LH) SDO → s_cm_rx_buffer 직접 업데이트
 * @note Mutex 이미 획득 상태
 */
static void _CM_SdoCallback_OnPVectorCompletedLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    if (req->dataSize > 0 && req->data != NULL) {
        uint8_t isCompleted;
        memcpy(&isCompleted, req->data, sizeof(isCompleted));
        s_cm_rx_buffer.sdo.isPVectorLHDone = isCompleted;
    }
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

/**
 * @brief 중립각도 설정 완료 SDO → s_cm_rx_buffer 직접 업데이트
 * @note Mutex 이미 획득 상태
 */
static void _CM_SdoCallback_OnNeutralPosSet(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    s_cm_rx_buffer.sdo.h10NeutralPosSet = true;
    res->status = DOP_SDO_SUCC;
    res->dataSize = 0;
}

// --- Low-Level Utility Functions ---
/**
 * @brief 전용 SDO 테이블에서 SDO 객체를 찾습니다.
 */
static DOP_SDO_t* _CM_FindSDO(uint8_t dictId, uint8_t objId) 
{
    if (dictId >= CM_DICT_ID_COUNT || objId >= DOP_SDO_MAX_NUM) return NULL;
    return &s_cm.dods[dictId].SDOs[objId];
}

/**
 * @brief 전용 PDO 테이블에서 PDO 객체를 찾습니다.
 */
static DOP_PDO_t* _CM_FindPDO(uint8_t dictId, uint8_t objId) 
{
    if (dictId >= CM_DICT_ID_COUNT || objId >= DOP_PDO_MAX_NUM) return NULL;
    return &s_cm.dods[dictId].PDOs[objId];
}

static float _CM_ScaleInt16ToFloat(int16_t value, float scaleFactor)
{
    // Scale the float value
    float scaledValue = (float)(value * scaleFactor / DATA_CONV_CONST_INT16);
    return scaledValue;
}

// Private 헬퍼 함수 추가
static void _CM_SendSdoMsg(DOPI_SDOMsg_t* sdoMsg)
{
    if (sdoMsg->msgLength > 0 && s_cm.txFunc) {
        s_cm.txFunc(s_cm.sdoTxId, sdoMsg->txBuf, sdoMsg->msgLength);
    }
}

/* ===================================================================
 *  PnP 상태 머신 내부 함수 (cm_xm_link.c에서 이동)
 * ===================================================================
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - 이 함수들은 SDO 콜백에서 호출되어 PnP 상태를 직접 업데이트
 */

/**
 * @brief PnP STOPPED 상태로 전환 + 데이터 리셋
 * @details cm_xm_link.c의 _Link_GoToStopped() 이동
 */
static void _CM_PnP_GoToStopped(void)
{
    s_pnp.isCmBooted = false;
    s_pnp.isCmConnected = false;
    s_pnp.isCmSynced = false;
    s_pnp.isAllStatesSynced = false;
    s_pnp.nmtState = CM_NMT_STOPPED;

    /* 데이터 캐시 리셋 (Stale 데이터 방지) */
    CM_ResetRxData();
}

/**
 * @brief CM Boot-up SDO 수신 시 PnP 상태 전환
 * @details cm_xm_link.c의 _Link_OnCmBootup() 이동
 */
static void _CM_PnP_OnCmBootup(void)
{
    if (s_pnp.nmtState == CM_NMT_INITIALISING) {
        s_pnp.isCmBooted = true;
        s_pnp.lastCmHeartbeatMs = IOIF_TIM_GetTick();
        s_pnp.nmtState = CM_NMT_PRE_OPERATIONAL;
        s_pnp.preOpState = CM_PRE_OP_IDLE;
        s_pnp.initEntryTime = 0;
    }
}

/**
 * @brief CM Heartbeat SDO 수신 시 PnP 상태 업데이트
 * @details cm_xm_link.c의 _Link_OnCmHeartbeat() 이동
 * @param[in] cmNmtState CM이 보고한 자신의 NMT 상태
 */
static void _CM_PnP_OnCmHeartbeat(uint8_t cmNmtState)
{
    s_pnp.lastCmHeartbeatMs = IOIF_TIM_GetTick();
    s_pnp.lastKnownCmState = (CM_NmtState_t)cmNmtState;

    if (s_pnp.nmtState == CM_NMT_PRE_OPERATIONAL &&
        s_pnp.preOpState == CM_PRE_OP_WAIT_OPERATIONAL_HB) {
        /* NMT 명령 후 첫 Heartbeat → OPERATIONAL 전환 */
        s_pnp.isCmConnected = true;
        s_pnp.nmtState = CM_NMT_OPERATIONAL;
    } else if (s_pnp.nmtState == CM_NMT_INITIALISING) {
        /* 초기화 중인데 CM이 Heartbeat → CM이 과거 상태에 갇힘 → STOPPED */
        _CM_PnP_GoToStopped();
    }
}

/**
 * @brief XM이 보낸 SDO 요청에 대한 CM의 응답 처리
 * @details cm_xm_link.c의 _Link_OnSdoResponse() 이동
 * @param[in] dictId    Dictionary ID
 * @param[in] sdoId     SDO Object ID
 * @param[in] isSuccess SDO 응답 상태 (DOP_SDO_SUCC / DOP_SDO_FAIL)
 */
static void _CM_PnP_OnSdoResponse(uint8_t dictId, uint8_t sdoId, int8_t isSuccess)
{
    /* CM→XM 비동기 요청 (필요시 작성) */
    if (dictId == CM_DICT_ID_CM_TO_XM) {
        /* Reserved */
    }
    /* XM→CM 요청에 대한 동기적 응답 처리 */
    else if (dictId == CM_DICT_ID_XM_TO_CM) {
        if (s_pnp.nmtState == CM_NMT_PRE_OPERATIONAL) {
            if (s_pnp.preOpState == CM_PRE_OP_WAIT_PDO_MAP_ACK &&
                sdoId == CM_SDO_ID_X2C_SET_PDO_MAPPING) {
                if (isSuccess == DOP_SDO_SUCC || isSuccess == DOP_SDO_REQU) {
                    /* PDO 매핑 응답 성공 → 상태 동기화 단계로 전환 */
                    s_pnp.lastSdoSentMs = IOIF_TIM_GetTick();
                    s_pnp.lastCmHeartbeatMs = IOIF_TIM_GetTick();
                    s_pnp.preOpState = CM_PRE_OP_SEND_SYNC_STATES;
                    s_pnp.isAllStatesSynced = false;
                } else {
                    _CM_PnP_GoToStopped();
                }
            }
        }
    }
}

/**
 * @brief 상태 동기화 완료 알림 수신
 * @details cm_xm_link.c의 _Link_OnCmSyncStates() 이동
 */
static void _CM_PnP_OnSyncStatesReceived(void)
{
    s_pnp.isAllStatesSynced = true;
}
