/**
 ******************************************************************************
 * @file canfd_rx_handler.c
 * @author HyundoKim
 * @brief FDCAN Rx Handler + Non-Realtime Processor (System Comm Layer)
 * @version 2.0 (SDO Processor 합병 + V1/V2 라우팅 완전 재설계)
 * @date 2026-02-10
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 * 
 * [변경 이유] .cursorrules "FDCAN Architecture" 섹션 기반 리팩토링
 * 
 * [AS-IS] V1.0 (sdo_processor.c 별도 파일)
 * - V1(CM) 메시지: 전부 CM_ProcessCANMessage()로 전달 (PDO 드롭 버그)
 * - V2(IMU) 메시지: GET_SRC_NODE_ID 비트 추출 오류 (bits[6:3] 사용)
 * - SDO Processor가 별도 파일로 분리되어 SDO_QueueItem_t 중복
 * 
 * [TO-BE] V2.0 (통합)
 * - V1/V2 CAN-ID 구조를 올바르게 구분:
 *   - V1: FuncCode[10:8] | SrcNode[7:4] | DstNode[3:0]
 *   - V2: FuncCode[10:7] | NodeID[6:0] (CANopen 표준)
 * - PDO/Heartbeat → 실시간 직접 라우팅
 * - SDO/NMT → Message Queue → Non-Realtime Task
 * - SDO Processor를 본 파일에 통합 (구조체 중복 제거)
 * 
 * [아키텍처 흐름 - V2.0]
 * 
 * 1. IOIF Layer (ISR Context)
 *    HAL_FDCAN_RxFifo0Callback() → xSemaphoreGiveFromISR(rx_semaphore)
 * 
 * 2. IOIF FDCAN RxTask (ioif_agrb_fdcan.c, Priority 52)
 *    Semaphore Take → HW FIFO 전체 Batch Read → 콜백 호출
 *    └─ _OnFdcanRxMessage() 콜백 (This File)
 * 
 * 3. _ClassifyAndRoute() (This File, IOIF RxTask 컨텍스트에서 실행)
 *    V1/V2 판별 → PDO/SDO 분기
 *    ├─ V1 PDO (0x312) → CM_Drv_ProcessPdo() [실시간]
 *    ├─ V1 SDO (0x212) → SDO Queue [비실시간]
 *    ├─ V2 TPDO (0x180/0x280) → ImuHub_Drv_ProcessCANMessage() [실시간]
 *    ├─ V2 Heartbeat (0x700) → AGR_PnP_Master_ProcessMessage() [실시간, V2.1]
 *    └─ V2 SDO/NMT → SDO Queue [비실시간]
 * 
 * 4. NonRealtime_Task (This File, Priority 51)
 *    Message Queue에서 SDO 꺼내 처리
 *    ├─ V1 CM SDO → CM_Drv_ProcessSdo()
 *    └─ V2 IMU Hub SDO/NMT → ImuHub_Drv_ProcessCANMessage()
 * 
 ******************************************************************************
 */

#include "canfd_rx_handler.h"

/* Standard Library */
#include <string.h>  /* memcpy */

/* IOIF Layer */
#include "ioif_agrb_fdcan.h"

/* Device Layer (DOP V1: CM/MD) */
#include "cm_drv.h"
// #include "md_drv.h"  // 향후 추가

/* Device Layer (DOP V2: Sensor Modules) */
#include "imu_hub_drv.h"
#include "emg_hub_drv.h"
#include "fes_hub_drv.h"

/* System Core */
#include "pnp_task.h"           /* PnP_Task_GetMaster(): 0x700 메시지 라우팅용 */
#include "system_startup.h"     /* System_GetFDCAN1_Id(): FDCAN1 핸들 조회 */

/* Services Layer */
#include "agr_dop_node_id.h"

/* FreeRTOS */
#include "cmsis_os2.h"
#include "semphr.h"
#include "queue.h"

/* System Config */
#include "module.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/**
 * @brief [V5.0] Lock-Free SPSC Ring Buffer 크기 (Power of 2)
 * @details
 * - ISR(NVIC 4)에서 enqueue → NonRealtimeTask에서 dequeue
 * - PnP 단계 최대 ~10 SDO + 안전 마진 = 32
 * - Power of 2: & mask로 빠른 modulo 연산
 */
#define NRT_RING_SIZE       32
#define NRT_RING_MASK       (NRT_RING_SIZE - 1)
#else
/**
 * @brief SDO Message Queue 크기
 * @details
 * - PnP 단계에서 최대 10개 SDO 전송 가능
 * - 안전 마진 2배 = 20개
 */
#define SDO_QUEUE_SIZE      20
#endif

/* ===== DOP V1 CAN-ID Structure =====
 * [10:8] Function Code | [7:4] Source Node | [3:0] Dest Node
 * 
 * 예시 (XM이 수신하는 CM 메시지):
 * - 0x212 = SDO(0x2) | CM(0x1) | XM(0x2) → CM이 XM에게 보낸 SDO
 * - 0x312 = PDO(0x3) | CM(0x1) | XM(0x2) → CM이 XM에게 보낸 PDO
 */
#define DOP_V1_FUNC_CODE_SDO    0x200   /**< SDO Function Code (bits [10:8] = 0x2) */
#define DOP_V1_FUNC_CODE_PDO    0x300   /**< PDO Function Code (bits [10:8] = 0x3) */

#define DOP_V1_GET_FUNC_CODE(can_id)    ((can_id) & 0x700)          /**< bits [10:8] */
#define DOP_V1_GET_SRC_NODE(can_id)     (((can_id) >> 4) & 0x0F)   /**< bits [7:4] */

/* ===== DOP V2 CAN-ID Structure (CANopen Standard) =====
 * [10:7] Function Code | [6:0] Node-ID
 * 
 * 예시 (XM이 수신하는 IMU Hub 메시지):
 * - 0x188 = TPDO1(0x3) | IMU_HUB_A(0x08)
 * - 0x288 = TPDO2(0x5) | IMU_HUB_A(0x08)
 * - 0x588 = SDO_RESP(0xB) | IMU_HUB_A(0x08)
 * - 0x708 = HEARTBEAT(0xE) | IMU_HUB_A(0x08)
 */
#define CANOPEN_FC_NMT          0x0     /**< NMT Command (0x000) */
#define CANOPEN_FC_SYNC_EMCY    0x1     /**< SYNC (0x080) / EMCY (0x08X~0x0FX) */
#define CANOPEN_FC_TPDO1        0x3     /**< TPDO1 (0x180~0x1FF) */
#define CANOPEN_FC_TPDO2        0x5     /**< TPDO2 (0x280~0x2FF) */
#define CANOPEN_FC_TPDO3        0x7     /**< TPDO3 (0x380~0x3FF) */
#define CANOPEN_FC_TPDO4        0x9     /**< TPDO4 (0x480~0x4FF) */
#define CANOPEN_FC_SDO_RESP     0xB     /**< SDO Response (0x580~0x5FF) */
#define CANOPEN_FC_SDO_REQ      0xC     /**< SDO Request (0x600~0x6FF) */
#define CANOPEN_FC_HEARTBEAT    0xE     /**< Heartbeat/Boot-up (0x700~0x7FF) */

#define DOP_V2_GET_FUNC_CODE(can_id)    (((can_id) >> 7) & 0x0F)   /**< bits [10:7] */
#define DOP_V2_GET_NODE_ID(can_id)      ((can_id) & 0x7F)          /**< bits [6:0] */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Non-Realtime Message Queue Item
 * @details 
 * - FDCAN_Rx_Task → NonRealtime_Task로 전달
 * - SDO, NMT 등 비실시간 메시지 포함
 */
typedef struct {
    uint8_t  src_node_id;   /**< Source Node ID (DOP V1: bits[7:4], V2: bits[6:0]) */
    uint16_t can_id;        /**< CAN-ID (원본, 메시지 식별용) */
    uint8_t  data[64];      /**< Payload (최대 64 bytes, CAN-FD) */
    uint8_t  len;           /**< Payload Length */
    bool     is_v1;         /**< true: DOP V1 (CM/MD), false: DOP V2 (CANopen) */
} NonRealtimeQueueItem_t;

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *-----------------------------------------------------------
 */

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/**
 * @brief [V5.0] Lock-Free SPSC Ring Buffer (ISR → NonRealtimeTask)
 * @details
 * - Producer: FDCAN ISR (NVIC 4) — head 증가만
 * - Consumer: NonRealtimeTask — tail 증가만
 * - 단일 producer/consumer → Lock-Free 안전 (Cortex-M 단일 코어)
 */
static struct {
    volatile uint32_t head;     /**< ISR(Producer)만 쓰기 */
    volatile uint32_t tail;     /**< Task(Consumer)만 쓰기 */
    NonRealtimeQueueItem_t items[NRT_RING_SIZE];
} s_nrt_ring = {0};

/** @brief NonRealtimeTask 깨우기 세마포어 (TIM7 SW IRQ에서 Give) */
static SemaphoreHandle_t s_nrt_sem = NULL;

#else
/** @brief Non-Realtime Message Queue Handle (V4.0) */
static osMessageQueueId_t s_nrt_queue = NULL;
#endif

/** @brief Non-Realtime Processor Task Handle */
static osThreadId_t s_nrt_task_handle = NULL;

/** @brief FDCAN Rx Task Statistics (디버깅용) */
static struct {
    uint32_t total_messages_processed;  /**< 총 처리된 메시지 수 */
    uint32_t batch_count;               /**< Batch 실행 횟수 */
    uint32_t max_batch_size;            /**< 최대 Batch 크기 */
    uint32_t nrt_queue_full_count;      /**< Non-Realtime Queue Full 횟수 */
    uint32_t v1_pdo_count;              /**< V1 PDO 처리 수 */
    uint32_t v1_sdo_count;              /**< V1 SDO 처리 수 */
    uint32_t v2_realtime_count;         /**< V2 실시간 처리 수 (TPDO, Heartbeat) */
    uint32_t v2_nonrealtime_count;      /**< V2 비실시간 처리 수 (SDO, NMT) */
    uint32_t nmt_cmd_count;             /**< NMT Broadcast 수 */
    uint32_t unknown_drop_count;        /**< Unknown 메시지 드롭 수 */
} s_stats = {0};

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/** @brief [V2.0] IOIF RxTask 콜백 래퍼 (IOIF_FDCAN_Msg_t → _ClassifyAndRoute) */
static void _OnFdcanRxMessage(IOIF_FDCAN_Msg_t* msg);

/** @brief V1/V2 판별 → 실시간/비실시간 분기 라우팅 */
static void _ClassifyAndRoute(uint16_t can_id, uint8_t* data, uint8_t len);

/** @brief V2 실시간 메시지를 Node-ID 기반으로 Device Layer에 전달 */
static void _RouteRealtimeV2(uint8_t node_id, uint16_t can_id, uint8_t* data, uint8_t len);

/** @brief 비실시간 메시지를 Queue에 추가 */
static bool _EnqueueNonRealtime(uint8_t src_node_id, uint16_t can_id, uint8_t* data, uint8_t len, bool is_v1);

/** @brief Queue에서 꺼낸 비실시간 메시지를 Device Layer로 전달 */
static void _ProcessNonRealtimeMessage(const NonRealtimeQueueItem_t* item);

/** @brief Non-Realtime Processor Task Entry Point */
static void _NonRealtimeTask(void* argument);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief FDCAN Rx Handler 초기화 (V2.0 - 콜백 등록 패턴)
 * @details 
 * - [V2.0] IOIF 내부 RxTask에 _OnFdcanRxMessage 콜백 등록
 * - Non-Realtime Queue + Task 생성 (SDO/NMT 처리)
 * - 세마포어 획득 + FDCANRxHandler_Task 생성 제거됨 (IOIF가 관리)
 * 
 * @return FDCANRxHandler_Status_t 초기화 결과
 * @note System Layer (system_startup.c)에서 호출
 */
FDCANRxHandler_Status_t FDCANRxHandler_Init(void)
{
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] SPSC Ring Buffer는 static zero-init → 별도 초기화 불필요
     * NonRealtimeTask 깨우기용 Binary Semaphore 생성 */
    s_nrt_sem = xSemaphoreCreateBinary();
    if (s_nrt_sem == NULL) {
        return FDCAN_RX_HANDLER_ERROR_QUEUE;
    }
#else
    /* [V4.0] Non-Realtime Message Queue 생성 */
    s_nrt_queue = osMessageQueueNew(SDO_QUEUE_SIZE, sizeof(NonRealtimeQueueItem_t), NULL);
    if (s_nrt_queue == NULL) {
        return FDCAN_RX_HANDLER_ERROR_QUEUE;
    }
#endif

    /* Step 2: [V5.0] IOIF FDCAN RxTask에 콜백 등록 (듀얼 채널)
     * - FDCAN1 (CM Bus): V1 메시지 수신 → _ClassifyAndRoute 에서 V1 경로
     * - FDCAN2 (Sensor Hub Bus): V2 메시지 수신 → _ClassifyAndRoute 에서 V2 경로
     * - 동일한 _OnFdcanRxMessage 콜백으로 V1/V2 자동 판별
     */
    IOIF_FDCAN_RegisterRxCallback(System_GetFDCAN1_Id(), _OnFdcanRxMessage);
#if 0  /* Rev2.0: FDCAN2 Sensor Hub Bus 별도 수신 */
    IOIF_FDCAN_RegisterRxCallback(System_GetFDCAN2_Id(), _OnFdcanRxMessage);
#endif

    /* Step 3: Non-Realtime Processor Task 생성 (SDO/NMT 처리) */
    const osThreadAttr_t nrt_task_attr = {
        .name = "NRT_Proc",
        .stack_size = TASK_STACK_SDO_PROCESSOR,
        .priority = TASK_PRIO_SDO_PROCESSOR,  /* osPriorityRealtime3 (51) */
    };

    s_nrt_task_handle = osThreadNew(_NonRealtimeTask, NULL, &nrt_task_attr);
    if (s_nrt_task_handle == NULL) {
        return FDCAN_RX_HANDLER_ERROR_TASK;
    }

    return FDCAN_RX_HANDLER_OK;
}

/* [V2.0] FDCANRxHandler_Task 제거됨
 * - IOIF 내부 _IOIF_FDCAN_RxTask가 대체 (세마포어 동일 소스 원칙)
 * - V5.0에서는 ISR에서 직접 콜백 호출 (RxTask도 제거)
 */

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/**
 * @brief [V5.0] TIM7 SW IRQ에서 호출 — NonRealtimeTask 깨우기
 * @note stm32h7xx_it.c TIM7_IRQHandler(USER CODE)에서 호출
 */
void FDCANRxHandler_SwIrqNotify(void)
{
    BaseType_t xWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_nrt_sem, &xWoken);
    portYIELD_FROM_ISR(xWoken);
}
#endif

/**
 * @brief [V2.0] IOIF FDCAN RxTask 콜백 래퍼
 * @details
 * IOIF RxTask에서 HW FIFO 메시지 1개당 1회 호출됨.
 * IOIF_FDCAN_Msg_t 구조체를 기존 _ClassifyAndRoute 인터페이스로 변환.
 * 통계 업데이트 포함.
 */
static void _OnFdcanRxMessage(IOIF_FDCAN_Msg_t* msg)
{
    if (msg == NULL) return;

    /* V1/V2 판별 → 실시간/비실시간 분기 (기존 로직 그대로) */
    _ClassifyAndRoute(msg->id, msg->data, msg->len);

    /* Statistics 업데이트 */
    s_stats.total_messages_processed++;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief V1/V2 판별 → 실시간/비실시간 분기 라우팅
 * 
 * @details
 * [V1/V2 판별 전략]
 * - DOP V1 (CM/MD): bits[7:4]에 Source Node ID 존재
 *   - AGR_NODE_ID_CM(0x01)이면 V1 확정
 *   - V2 Node들은 0x08+ 이므로 bits[7:4]가 0x8+, 충돌 없음
 * 
 * - DOP V2 (CANopen): 나머지 모든 메시지
 *   - Function Code: bits[10:7]
 *   - Node-ID: bits[6:0]
 * 
 * [라우팅 규칙] (.cursorrules 기반)
 * 
 * | DOP | 실시간 (직접 라우팅)          | 비실시간 (Message Queue)  |
 * |-----|-------------------------------|-------------------------|
 * | V1  | PDO (0x300) → CM_Drv         | SDO (0x200)             |
 * | V2  | TPDO (0x180,0x280) → Device  | SDO (0x580,0x600)       |
 * |     | Heartbeat (0x700) → PnP Mst  | NMT (0x000)             |
 */
static void _ClassifyAndRoute(uint16_t can_id, uint8_t* data, uint8_t len)
{
    /* ===== 특수 처리: NMT Broadcast (0x000) ===== */
    if (can_id == 0x000) {
        _EnqueueNonRealtime(AGR_NODE_ID_BROADCAST, can_id, data, len, false);
        s_stats.nmt_cmd_count++;
        return;
    }

    /* ===== 특수 처리: SYNC (0x080) ===== */
    if (can_id == 0x080) {
        /* TODO: SYNC Handler 호출 (제어 루프 타이밍 동기화) */
        return;
    }

    /* ===== DOP V1 판별: bits[7:4] == AGR_NODE_ID_CM (0x01) ===== */
    uint8_t v1_src_node = DOP_V1_GET_SRC_NODE(can_id);
    
    if (v1_src_node == AGR_NODE_ID_CM) {
        /* ===== DOP V1: CM 메시지 ===== */
        uint16_t v1_func = DOP_V1_GET_FUNC_CODE(can_id);
        
        if (v1_func == DOP_V1_FUNC_CODE_PDO) {
            /* V1 PDO → 실시간 직접 라우팅 */
            CM_Drv_ProcessPdo(data, len);
            s_stats.v1_pdo_count++;
        } 
        else if (v1_func == DOP_V1_FUNC_CODE_SDO) {
            /* V1 SDO → 비실시간 Message Queue */
            _EnqueueNonRealtime(AGR_NODE_ID_CM, can_id, data, len, true);
            s_stats.v1_sdo_count++;
        }
        /* 기타 V1 Function Code (Heartbeat 등): 향후 확장 */
        return;
    }

    /* ===== DOP V2: CANopen 표준 메시지 ===== */
    uint8_t v2_func = DOP_V2_GET_FUNC_CODE(can_id);
    uint8_t v2_node = DOP_V2_GET_NODE_ID(can_id);
    
    switch (v2_func) {
        /* ===== TPDO → 실시간 직접 라우팅 (센서 데이터) ===== */
        case CANOPEN_FC_TPDO1:      /* 0x180+ */
        case CANOPEN_FC_TPDO2:      /* 0x280+ */
            _RouteRealtimeV2(v2_node, can_id, data, len);
            s_stats.v2_realtime_count++;
            break;
        
        /* ===== Heartbeat/Boot-up → PnP Master로 라우팅 =====
         * [V2.1 변경] (.cursorrules Phase 2)
         * - AS-IS: ImuHub_Drv_ProcessCANMessage()에서 0x700 직접 처리
         * - TO-BE: AGR_PnP_Master_ProcessMessage()로 라우팅
         *   → PnP Master가 NMT 상태 관리 + on_slave_bootup 콜백 호출
         *   → Device Driver의 Pre-Op SM이 콜백으로 트리거됨
         */
        case CANOPEN_FC_HEARTBEAT:  /* 0x700+ (Boot-up/Heartbeat) */
        {
            AGR_PnP_Master_t* master = PnP_Task_GetMaster();
            if (master != NULL) {
                AGR_PnP_Master_ProcessMessage(master, can_id, data, len);
            }
            s_stats.v2_realtime_count++;
            break;
        }
        
        /* ===== 비실시간 메시지 → Message Queue ===== */
        case CANOPEN_FC_SDO_RESP:   /* 0x580+ (SDO 응답) */
        case CANOPEN_FC_SDO_REQ:    /* 0x600+ (SDO 요청) */
        case CANOPEN_FC_NMT:        /* NMT to specific node */
            _EnqueueNonRealtime(v2_node, can_id, data, len, false);
            s_stats.v2_nonrealtime_count++;
            break;
        
        /* ===== EMCY (0x080+) → 향후 Hybrid 처리 ===== */
        case CANOPEN_FC_SYNC_EMCY:
            /* TODO: EMCY → Flag 즉시 설정 + Queue (Hybrid 패턴) */
            break;
        
        /* ===== Unknown → Drop ===== */
        default:
            s_stats.unknown_drop_count++;
            break;
    }
}

/**
 * @brief V2 실시간 메시지를 Node-ID 기반으로 Device Layer에 전달
 */
static void _RouteRealtimeV2(uint8_t node_id, uint16_t can_id, uint8_t* data, uint8_t len)
{
    /* ===== IMU Hub (Group A: 0x08, Group B: 0x09) ===== */
    if (node_id == AGR_NODE_ID_IMU_HUB_A || 
        node_id == AGR_NODE_ID_IMU_HUB_B) {
        ImuHub_Drv_ProcessCANMessage(can_id, data, len);
        return;
    }
    /* ===== EMG Hub (Node ID: 0x0A) ===== */
    if (node_id == AGR_NODE_ID_EMG_HUB) {
        EmgHub_Drv_ProcessCANMessage(can_id, data, len);
        return;
    }
    /* ===== FES Hub (Node ID: 0x0B) ===== */
    if (node_id == AGR_NODE_ID_FES_HUB) {
        FesHub_Drv_ProcessCANMessage(can_id, data, len);
        return;
    }

    /* Unknown V2 Node → Drop */
    s_stats.unknown_drop_count++;
}

/**
 * @brief 비실시간 메시지를 Queue에 추가
 * @return true: Queue 전달 성공, false: Queue Full
 */
static bool _EnqueueNonRealtime(uint8_t src_node_id, uint16_t can_id, uint8_t* data, uint8_t len, bool is_v1)
{
    NonRealtimeQueueItem_t item = {
        .src_node_id = src_node_id,
        .can_id = can_id,
        .len = len,
        .is_v1 = is_v1,
    };
    memcpy(item.data, data, len);

#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* [V5.0] Lock-Free SPSC Ring Buffer enqueue (ISR context, no FreeRTOS API) */
    uint32_t next = (s_nrt_ring.head + 1) & NRT_RING_MASK;
    if (next == s_nrt_ring.tail) {
        s_stats.nrt_queue_full_count++;
        return false;  /* Ring Full → drop */
    }
    s_nrt_ring.items[s_nrt_ring.head] = item;
    __DMB();
    s_nrt_ring.head = next;

    /* SW IRQ 트리거 → TIM7_IRQHandler(NVIC 6) → xSemaphoreGiveFromISR → NonRealtimeTask */
    NVIC_SetPendingIRQ(TIM7_IRQn);
#else
    /* [V4.0] osMessageQueue (Task context) */
    osStatus_t status = osMessageQueuePut(s_nrt_queue, &item, 0, 0);
    if (status != osOK) {
        s_stats.nrt_queue_full_count++;
        return false;
    }
#endif

    return true;
}

/**
 * @brief Non-Realtime Processor Task (SDO/NMT 처리)
 * @details 
 * - Message Queue에서 비실시간 메시지 꺼내기 (Blocking Wait)
 * - Node-ID별 Device Layer Callback 호출
 * - Main Task(54)보다 낮은 Priority(51)로 제어 루프 방해 없음
 */
static void _NonRealtimeTask(void* argument)
{
    (void)argument;

    NonRealtimeQueueItem_t item;

    while (1)
    {
#if defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
        /* [V5.0] Semaphore Wait (TIM7 SW IRQ에서 Give) → Ring Buffer Batch Drain */
        if (xSemaphoreTake(s_nrt_sem, portMAX_DELAY) == pdTRUE) {
            while (s_nrt_ring.tail != s_nrt_ring.head) {
                item = s_nrt_ring.items[s_nrt_ring.tail];
                __DMB();
                s_nrt_ring.tail = (s_nrt_ring.tail + 1) & NRT_RING_MASK;

                _ProcessNonRealtimeMessage(&item);
            }
        }
#else
        /* [V4.0] Message Queue Wait (Blocking) */
        osStatus_t status = osMessageQueueGet(s_nrt_queue, &item, NULL, osWaitForever);
        if (status != osOK) {
            continue;
        }

        _ProcessNonRealtimeMessage(&item);
        osThreadYield();
#endif
    }
}

/**
 * @brief Queue에서 꺼낸 비실시간 메시지를 Device Layer로 전달
 */
static void _ProcessNonRealtimeMessage(const NonRealtimeQueueItem_t* item)
{
    /* ===== 특수 처리: NMT Broadcast (0x000) ===== */
    if (item->can_id == 0x000) {
        s_stats.nmt_cmd_count++;
        /* TODO: NMT Broadcast → 모든 Device에 전달 (PnP 리팩토링 시 구현) */
        return;
    }

    /* ===== DOP V1: CM SDO ===== */
    if (item->is_v1 && item->src_node_id == AGR_NODE_ID_CM) {
        CM_Drv_ProcessSdo(item->can_id, (uint8_t*)item->data, item->len);
        return;
    }
    // else if (item->is_v1 && item->src_node_id == AGR_NODE_ID_MD_RH) {
    //     MD_Drv_ProcessSdo(item->can_id, (uint8_t*)item->data, item->len);
    //     return;
    // }

    /* ===== DOP V2: Sensor Modules SDO/NMT ===== */
    if (!item->is_v1) {
        if (item->src_node_id == AGR_NODE_ID_IMU_HUB_A || 
            item->src_node_id == AGR_NODE_ID_IMU_HUB_B) {
            ImuHub_Drv_ProcessCANMessage(item->can_id, (uint8_t*)item->data, item->len);
            return;
        }
        if (item->src_node_id == AGR_NODE_ID_EMG_HUB) {
            EmgHub_Drv_ProcessCANMessage(item->can_id, (uint8_t*)item->data, item->len);
            return;
        }
        if (item->src_node_id == AGR_NODE_ID_FES_HUB) {
            FesHub_Drv_ProcessCANMessage(item->can_id, (uint8_t*)item->data, item->len);
            return;
        }
    }

    /* Unknown → Drop */
    s_stats.unknown_drop_count++;
}
