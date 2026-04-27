/**
 *-----------------------------------------------------------
 *                 FDCAN Communication Driver
 *-----------------------------------------------------------
 * @file ioif_agrb_fdcan.c
 * @version Common Library
 * @date Created on: Aug 23, 2023
 * @author AngelRobotics HW Team
 * @brief Driver code for the FDCAN communication.
 *
 * This source file provides functionality to interface
 * with FDCAN hardware. Common Library version - shared
 * across XM, IMU Hub, and other Angel Robotics projects.
 *
 * @ref FDCAN reference
 */

#include "ioif_agrb_fdcan.h"
#if defined(AGRB_IOIF_FDCAN_ENABLE)

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(USE_FREERTOS)
#include "cmsis_os2.h" // For xSemaphore...
#endif

/**
 *-----------------------------------------------------------
 *              PRIVATE DEFINITIONS AND TYPES
 *-----------------------------------------------------------
 */

/* Software Tx Queue 크기 (HW FIFO 3개 보완) */
#define SW_TX_QUEUE_SIZE  16  /**< 16 slots (3개 HW FIFO 보완) */

#if defined(USE_FREERTOS) && !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/** @brief FDCAN RxTask 설정 (세마포어 Give/Take 동일 소스 원칙)
 *  @note ioif_conf.h에서 #ifndef 오버라이드 가능
 *  @note FDCAN Rx > SDO Processor(osPriorityRealtime3): FDCAN이 SDO에 데이터를 공급
 *  @note V5.0 ISR-Direct 모드에서는 RxTask 불필요 (ISR에서 직접 콜백)
 */
#ifndef IOIF_FDCAN_RX_TASK_STACK_SIZE
    #define IOIF_FDCAN_RX_TASK_STACK_SIZE    (2048U)              /**< RxTask 스택 크기 (bytes) */
#endif
#ifndef IOIF_FDCAN_RX_TASK_PRIORITY
    #define IOIF_FDCAN_RX_TASK_PRIORITY      (osPriorityRealtime4) /**< RxTask 우선순위 */
#endif
#define FDCAN_RX_TASK_STACK_SIZE    IOIF_FDCAN_RX_TASK_STACK_SIZE
#define FDCAN_RX_TASK_PRIORITY      IOIF_FDCAN_RX_TASK_PRIORITY
#endif

/**
 * @brief Tx Mutex Timeout (ms)
 * @details 
 * HAL_FDCAN_AddMessageToTxFifoQ 소요 시간: ~2µs
 * SW Queue 조작 소요 시간: ~5µs
 * 5ms Timeout은 매우 보수적 (실제 Hold Time 대비 1000배 마진)
 */
#define TX_MUTEX_TIMEOUT_MS     5

/* ===== Tx Mutex 매크로 (.cursorrules: RTOS/BareMetal 양립) ===== */
#if defined(USE_FREERTOS)
    #define TX_LOCK(inst)   ((inst)->tx_mutex != NULL && \
                             xSemaphoreTake((inst)->tx_mutex, pdMS_TO_TICKS(TX_MUTEX_TIMEOUT_MS)) == pdTRUE)
    #define TX_UNLOCK(inst) xSemaphoreGive((inst)->tx_mutex)
#else
    /* BareMetal: ISR 우선순위 차별화로 보호 (Critical Section 금지) */
    #define TX_LOCK(inst)   (true)
    #define TX_UNLOCK(inst) ((void)0)
#endif

/**
 * @brief Software Tx Queue 아이템
 */
typedef struct {
    uint32_t can_id;
    uint8_t  data[64];
    uint8_t  len;
    uint8_t  priority;  /**< 0=최고, 255=최저 */
    bool     valid;     /**< true: 유효한 메시지 */
} CanTxQueueItem_t;

// FDCAN 인스턴스의 내부 상태를 관리하는 구조체 (Private)
typedef struct {
    bool                    is_assigned;
    FDCAN_HandleTypeDef*    hfdcan;
    IOIF_FDCAN_RxCallback_t rx_callback;
    
#if defined(USE_FREERTOS)
    /* ===== RTOS Mode ===== */
  #if !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    SemaphoreHandle_t       rx_semaphore;   /**< ISR → RxTask 신호용 (V4.0) */
    TaskHandle_t            rx_task_handle; /**< IOIF 내부 RxTask (V4.0, ISR-Direct 시 제거) */
  #endif
    SemaphoreHandle_t       tx_mutex;       /**< Tx 경로 Thread-Safety 보호 */
#endif
    
    /* Software Tx Queue (Bus Off 방지) */
    CanTxQueueItem_t        tx_queue[SW_TX_QUEUE_SIZE];
    uint8_t                 tx_queue_head;
    uint8_t                 tx_queue_tail;
    uint8_t                 tx_queue_count;
} IOIF_FDCAN_Inst_t;

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

// 인스턴스들을 관리할 정적 배열
static IOIF_FDCAN_Inst_t s_fdcan_instances[IOIF_FDCAN_MAX_INSTANCES] = {0};
static uint32_t s_instance_count = 0;

/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static IOIF_FDCAN_Inst_t* _FindInstanceByHandle(FDCAN_HandleTypeDef* hfdcan);
static uint32_t _Len2DLC(uint8_t len);
static uint8_t _DLC2Len(uint8_t dlc);

#if defined(USE_FREERTOS) && !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/**
 * @brief [V4.0 IOIF 내부] FDCAN RxTask (세마포어 Give/Take 동일 소스 원칙)
 * @note V5.0 ISR-Direct 모드에서는 컴파일 제외 (ISR에서 직접 콜백)
 */
static void _IOIF_FDCAN_RxTask(void* argument);
#endif

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/* [REMOVED] IOIF_FDCAN_GetRxSemaphore 
 * 세마포어 Give/Take 동일 소스 원칙에 따라 제거됨.
 * RxTask가 IOIF 내부로 이동되어 semaphore 외부 노출 불필요.
 */

 // Init 함수에서 rx_queue 인자 제거
AGRBStatusDef IOIF_FDCAN_AssignInstance(IOIF_FDCANx_t* id, FDCAN_HandleTypeDef* hfdcan)
{
    if (s_instance_count >= IOIF_FDCAN_MAX_INSTANCES || hfdcan == NULL || id == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }
    // 이미 등록된 핸들인지 확인
    if (_FindInstanceByHandle(hfdcan) != NULL) {
        return AGRBStatus_BUSY;
    }

    // 새 인스턴스에 정보 할당
    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[s_instance_count];
    memset(inst, 0, sizeof(IOIF_FDCAN_Inst_t));
    inst->is_assigned = true;
    inst->hfdcan = hfdcan;
    inst->rx_callback = NULL; // 콜백을 NULL로 초기화
    
#if defined(USE_FREERTOS)
  #if !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* ===== V4.0: Rx Semaphore 생성 (ISR → RxTask 신호) ===== */
    inst->rx_semaphore = xSemaphoreCreateBinary();
    if (inst->rx_semaphore == NULL) {
        return AGRBStatus_SEMAPHORE_ERROR;
    }
    /* ⚠️ 초기 상태: 대기 (Give 안함, ISR에서 첫 신호) */
  #endif
    /* V5.0 ISR-Direct: Rx Semaphore 불필요 (ISR에서 직접 콜백) */

    /* ===== Tx Mutex 생성 (Priority Inheritance 지원) =====
     * - 문제: UserTask(54), PnP(25), SDO Handler(51)가 동시에 Transmit 호출 가능
     * - 해결: Mutex로 Tx 경로 전체 보호 (HW Tx FIFO + SW Tx Queue)
     */
    inst->tx_mutex = xSemaphoreCreateMutex();
    if (inst->tx_mutex == NULL) {
        return AGRBStatus_SEMAPHORE_ERROR;
    }
#endif
    
    *id = s_instance_count; // 사용자에게 핸들(ID) 반환
    s_instance_count++;

    return AGRBStatus_OK;
}

AGRBStatusDef IOIF_FDCAN_Start(IOIF_FDCANx_t id)
{
    /* ✅ Task 불필요 (ISR에서 직접 콜백 호출) */
    
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return AGRBStatus_NOT_INITIALIZED;
    }
    
    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[id];
    FDCAN_HandleTypeDef* hfdcan = inst->hfdcan;

    /* ✅ RTOS/Bare-metal 공통: Task 불필요, ISR에서 직접 콜백 호출 */

    /* 
     * [FDCAN Hardware Filter 설정 제거]
     * 
     * [설계 변경]
     * - IOIF는 범용 모듈이므로 기본 필터를 설정하지 않습니다.
     * - System Layer에서 IOIF_FDCAN_ConfigFilter()를 호출하여 모듈에 맞는 필터를 설정해야 합니다.
     * - 이유: System Layer의 필터 설정이 덮어써지는 문제 방지
     * 
     * [초기화 순서]
     * 1. System Layer: IOIF_FDCAN_ConfigFilter() (필터 설정)
     * 2. System Layer: IOIF_FDCAN_START() (FDCAN 시작)
     * 3. IOIF Layer: Rx FIFO Overwrite + Global Filter + TDC + Start (하드웨어 활성화)
     */
    
    // 2. Rx FIFO Overwrite Mode 활성화 (최신 메시지 우선)
    if (HAL_FDCAN_ConfigRxFifoOverwrite(hfdcan, FDCAN_RX_FIFO0, 
                                        FDCAN_RX_FIFO_OVERWRITE) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    
    // 3. 글로벌 필터 설정
    if (HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK) return AGRBStatus_ERROR;
    
    // 4. 수신 인터럽트 활성화
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return AGRBStatus_ERROR;

    // 4-1. [V5.0] RxFIFO0 Message Lost 모니터링 (overflow 검출)
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_MESSAGE_LOST, 0) != HAL_OK) return AGRBStatus_ERROR;

    // Bus Off Auto Recovery 활성화 (STM32G4/H7 공통)
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_BUS_OFF, 0) != HAL_OK) return AGRBStatus_ERROR;
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_ERROR_PASSIVE, 0) != HAL_OK) return AGRBStatus_ERROR;

    // 5. Tx Complete Interrupt 활성화 (CAN Bus Off 방지)
#if defined(STM32H743xx)
    // 모든 Tx 버퍼 (STM32H743: 32개)에 ISR 활성화
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_TX_COMPLETE, 0xFFFFFFFF) != HAL_OK) return AGRBStatus_ERROR;
#elif defined(STM32G474xx)
    // 모든 Tx 버퍼 (STM32G474: 3개)에 ISR 활성화
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_TX_COMPLETE, 0x00000007) != HAL_OK) return AGRBStatus_ERROR;
#endif
    
    // 6. 송신 지연 보상(TDC) 설정
    if (HAL_FDCAN_ConfigTxDelayCompensation(hfdcan, hfdcan->Init.DataPrescaler * hfdcan->Init.DataTimeSeg1, IOIF_TDC_FILTER) != HAL_OK) return AGRBStatus_ERROR;
    
    // 7. 송신 지연 보상(TDC) 활성화
    if (HAL_FDCAN_EnableTxDelayCompensation(hfdcan) != HAL_OK) return AGRBStatus_ERROR;

    // 8. FDCAN 시작
    if (HAL_FDCAN_Start(hfdcan) != HAL_OK) return AGRBStatus_ERROR;

#if defined(USE_FREERTOS) && !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /**
     * 9. [V4.0 RTOS] IOIF 내부 RxTask 생성
     * - 세마포어 Give/Take 동일 소스 원칙 적용
     * - ISR(Give) → RxTask(Take) → Batch Read → Callback
     * - V5.0 ISR-Direct 모드에서는 RxTask 불필요 (ISR에서 직접 콜백)
     */
    if (inst->rx_task_handle == NULL) {
        BaseType_t ret = xTaskCreate(
            _IOIF_FDCAN_RxTask,
            (id == 0) ? "IOIF_FDCRx0" : "IOIF_FDCRx1",
            FDCAN_RX_TASK_STACK_SIZE / sizeof(StackType_t),
            (void*)(uintptr_t)id,  /* 인스턴스 ID 전달 */
            FDCAN_RX_TASK_PRIORITY,
            &inst->rx_task_handle
        );
        if (ret != pdPASS) {
            HAL_FDCAN_Stop(hfdcan);  /* HW 상태 복구 — 재시도 가능하도록 */
            return AGRBStatus_INITIAL_FAILED;
        }
    }
#endif

    return AGRBStatus_OK;
}

/**
 * @brief 지정된 FDCAN 인스턴스를 통해 메시지를 전송합니다.
 * 
 * [변경 이유] .cursorrules "통신 아키텍처 철학" 섹션 기반
 * - AS-IS: Mutex 없이 HAL_FDCAN_AddMessageToTxFifoQ 호출 (Race Condition)
 * - TO-BE: Tx Mutex로 보호하여 Thread-Safety 보장
 * 
 * [Race Condition 시나리오]
 * 1. UserTask(54): Tx FIFO Put Index=5 읽음
 * 2. PnP Task(25): 선점 → 같은 Put Index=5 읽음 → RAM[5]에 Heartbeat 쓰기
 * 3. UserTask: RAM[5]에 토크 PDO 덮어쓰기 → CAN 프레임 손상
 * 
 * @note Thread-Safe: 내부 Tx Mutex 보호
 */
AGRBStatusDef IOIF_FDCAN_Transmit(IOIF_FDCANx_t id, uint32_t can_id, const uint8_t* txData, uint8_t len)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return AGRBStatus_NOT_INITIALIZED;
    }

    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[id];
    AGRBStatusDef result = AGRBStatus_ERROR;
    
    /* ===== Tx Mutex Lock ===== */
    if (!TX_LOCK(inst)) {
        return AGRBStatus_TIMEOUT;  /* Mutex 획득 실패 */
    }
    
    FDCAN_TxHeaderTypeDef TxHeader = {
        .Identifier = can_id,
        .IdType = FDCAN_STANDARD_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = _Len2DLC(len),
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch = FDCAN_BRS_ON,
        .FDFormat = FDCAN_FD_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
        .MessageMarker = 0
    };

    if (HAL_FDCAN_AddMessageToTxFifoQ(inst->hfdcan, &TxHeader, txData) == HAL_OK) {
        result = AGRBStatus_OK;
    }
    
    /* ===== Tx Mutex Unlock ===== */
    TX_UNLOCK(inst);
    
    return result;
}

/**
 * @brief Tx FIFO 여유 공간 확인 (HW 레지스터 Read-Only)
 * @details CAN Bus Off 방지를 위해 전송 전 FIFO 상태를 확인합니다.
 * @return 여유 공간 (0~3, 0=Full, 3=Empty)
 * 
 * @note Mutex 불필요: HW 레지스터 Read-Only (Atomic, Side-effect 없음)
 */
uint32_t IOIF_FDCAN_GetTxFifoFreeLevel(IOIF_FDCANx_t id)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return 0;  /* 인스턴스 없음 → Full로 간주 */
    }

    FDCAN_HandleTypeDef* hfdcan = s_fdcan_instances[id].hfdcan;

    /* Queue 모드: TFFL은 항상 0이므로 TFQF (Full flag)로 판단 */
    if (hfdcan->Init.TxFifoQueueMode == FDCAN_TX_QUEUE_OPERATION) {
        return ((hfdcan->Instance->TXFQS & FDCAN_TXFQS_TFQF) != 0U) ? 0U : 1U;
    }
    /* FIFO 모드: TFFL 필드 사용 (기존 동작) */
    return HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);
}

/**
 * @brief Rx FIFO0 채움 수준 확인 (HW 레지스터 Read-Only)
 */
uint32_t IOIF_FDCAN_GetRxFifo0FillLevel(IOIF_FDCANx_t id)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return 0;
    }
    return HAL_FDCAN_GetRxFifoFillLevel(s_fdcan_instances[id].hfdcan, FDCAN_RX_FIFO0);
}

/**
 * @brief FDCAN 에러 카운터 조회 (ECR 레지스터)
 */
AGRBStatusDef IOIF_FDCAN_GetErrorCounters(IOIF_FDCANx_t id, uint8_t* tec, uint8_t* rec)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return AGRBStatus_ERROR;
    }
    FDCAN_ErrorCountersTypeDef err_cnt;
    if (HAL_FDCAN_GetErrorCounters(s_fdcan_instances[id].hfdcan, &err_cnt) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    if (tec) *tec = (uint8_t)err_cnt.TxErrorCnt;
    if (rec) *rec = (uint8_t)err_cnt.RxErrorCnt;
    return AGRBStatus_OK;
}

/**
 * @brief FDCAN 프로토콜 상태 조회 (PSR 레지스터)
 */
AGRBStatusDef IOIF_FDCAN_GetBusStatus(IOIF_FDCANx_t id, uint8_t* lec, uint8_t* bus_status)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return AGRBStatus_ERROR;
    }
    FDCAN_ProtocolStatusTypeDef proto_sts;
    if (HAL_FDCAN_GetProtocolStatus(s_fdcan_instances[id].hfdcan, &proto_sts) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    if (lec) *lec = (uint8_t)proto_sts.LastErrorCode;
    if (bus_status) {
        *bus_status =
            ((proto_sts.BusOff)       ? 0x01 : 0) |
            ((proto_sts.Warning)      ? 0x02 : 0) |
            ((proto_sts.ErrorPassive) ? 0x04 : 0);
    }
    return AGRBStatus_OK;
}

/**
 * @brief Software Tx Queue에 메시지 추가 (FIFO)
 * @details Tx FIFO Full 시 Software Queue에 저장 (Bus Off 방지)
 * 
 * [변경 이유]
 * - 우선순위 기반 중간 삽입 로직이 Circular Queue 상태를 망가뜨림
 * - Boot-up 시 Queue Full 오류 발생
 * - CM-XM처럼 단순 FIFO로 변경 (검증된 방식)
 * 
 * @note priority 파라미터는 호환성을 위해 유지하지만 사용하지 않음
 */
/**
 * @brief Software Tx Queue에 메시지 추가 (Thread-Safe)
 * 
 * @note Thread-Safe: 내부 Tx Mutex 보호 (head/tail/count 무결성 보장)
 */
AGRBStatusDef IOIF_FDCAN_QueueMessage(IOIF_FDCANx_t id, uint32_t can_id, const uint8_t* data, uint8_t len, uint8_t priority)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned || data == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }
    if (len > 64) return AGRBStatus_PARAM_ERROR;

    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[id];
    AGRBStatusDef result = AGRBStatus_ERROR;
    
    /* ===== Tx Mutex Lock ===== */
    if (!TX_LOCK(inst)) {
        return AGRBStatus_TIMEOUT;
    }
    
    /* Queue Full 체크 */
    if (inst->tx_queue_count >= SW_TX_QUEUE_SIZE) {
        TX_UNLOCK(inst);
        return AGRBStatus_ERROR;  /* Queue Full (Drop) */
    }
    
    /* ✅ 단순 FIFO: tail 위치에 삽입 (CM-XM 방식) */
    inst->tx_queue[inst->tx_queue_tail].can_id = can_id;
    memcpy(inst->tx_queue[inst->tx_queue_tail].data, data, len);
    inst->tx_queue[inst->tx_queue_tail].len = len;
    inst->tx_queue[inst->tx_queue_tail].priority = priority;  /* 저장만 하고 사용 안 함 */
    inst->tx_queue[inst->tx_queue_tail].valid = true;
    
    inst->tx_queue_tail = (inst->tx_queue_tail + 1) % SW_TX_QUEUE_SIZE;
    inst->tx_queue_count++;
    result = AGRBStatus_OK;
    
    /* ===== Tx Mutex Unlock ===== */
    TX_UNLOCK(inst);
    
    return result;
}

/**
 * @brief Software Tx Queue 처리 (Main Loop에서 주기 호출, Thread-Safe)
 * @details Queue에서 Tx FIFO로 전송 (FIFO 순서)
 * @return 처리된 메시지 개수
 * 
 * @note Thread-Safe: 내부 Tx Mutex 보호 (Queue 조작 + HAL Tx 원자적 수행)
 */
uint32_t IOIF_FDCAN_ProcessQueue(IOIF_FDCANx_t id, uint8_t reserved_slots)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return 0;
    }
    
    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[id];
    uint32_t sent_count = 0;
    
    /* ===== Tx Mutex Lock ===== */
    if (!TX_LOCK(inst)) {
        return 0;  /* Mutex 획득 실패 → 다음 주기에 재시도 */
    }
    
    /* Queue에서 Tx FIFO로 전송 시도 */
    while (inst->tx_queue_count > 0) {
        /* Tx FIFO 여유 확인 */
        uint32_t free_level = IOIF_FDCAN_GetTxFifoFreeLevel(id);
        if (free_level <= reserved_slots) {
            break;  /* Tx FIFO Full → 다음 주기에 재시도 */
        }
        
        /* Queue에서 최우선 메시지 가져오기 (head) */
        CanTxQueueItem_t* item = &inst->tx_queue[inst->tx_queue_head];
        if (!item->valid) {
            break;  /* Invalid item */
        }
        
        /* Tx Header 설정 */
        FDCAN_TxHeaderTypeDef TxHeader = {
            .Identifier = item->can_id,
            .IdType = FDCAN_STANDARD_ID,
            .TxFrameType = FDCAN_DATA_FRAME,
            .DataLength = _Len2DLC(item->len),
            .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
            .BitRateSwitch = FDCAN_BRS_ON,
            .FDFormat = FDCAN_FD_CAN,
            .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
            .MessageMarker = 0
        };
        
        /* Tx FIFO에 추가 */
        if (HAL_FDCAN_AddMessageToTxFifoQ(inst->hfdcan, &TxHeader, item->data) == HAL_OK) {
            /* ✅ 전송 성공 → Queue에서 제거 */
            item->valid = false;
            inst->tx_queue_head = (inst->tx_queue_head + 1) % SW_TX_QUEUE_SIZE;
            inst->tx_queue_count--;
            sent_count++;
        } else {
            /* ❌ 전송 실패 (Tx FIFO Full?) → 다음 주기에 재시도 */
            break;
        }
    }
    
    /* ===== Tx Mutex Unlock ===== */
    TX_UNLOCK(inst);
    
    return sent_count;
}

/**
 * @brief 모든 Pending Tx 요청 취소 (Thread-Safe)
 * @details Master 온라인 감지 시 stuck TX 방지를 위해 호출
 * @param id IOIF_FDCANx_t 핸들
 */
void IOIF_FDCAN_AbortAllTx(IOIF_FDCANx_t id)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return;
    }
    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[id];
    if (!TX_LOCK(inst)) {
        return;
    }
    HAL_FDCAN_AbortTxRequest(inst->hfdcan,
                              FDCAN_TX_BUFFER0 | FDCAN_TX_BUFFER1 | FDCAN_TX_BUFFER2);

    /* SW queue flush — abort된 메시지가 ProcessQueue에서 재전송되는 것 방지 */
    for (uint8_t i = 0; i < SW_TX_QUEUE_SIZE; i++) {
        inst->tx_queue[i].valid = false;
    }
    inst->tx_queue_head  = 0;
    inst->tx_queue_tail  = 0;
    inst->tx_queue_count = 0;

    TX_UNLOCK(inst);
}

AGRBStatusDef IOIF_FDCAN_ConfigFilter(IOIF_FDCANx_t id, FDCAN_FilterTypeDef* filter_config)
{
    if (id >= s_instance_count || !s_fdcan_instances[id].is_assigned) {
        return AGRBStatus_NOT_INITIALIZED;
    }
    
    if (filter_config == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }
    
    FDCAN_HandleTypeDef* hfdcan = s_fdcan_instances[id].hfdcan;
    
    if (HAL_FDCAN_ConfigFilter(hfdcan, filter_config) != HAL_OK) {
        return AGRBStatus_ERROR;
    }
    
    return AGRBStatus_OK;
}

// 수신 콜백 등록 함수 구현
void IOIF_FDCAN_RegisterRxCallback(IOIF_FDCANx_t id, IOIF_FDCAN_RxCallback_t callback)
{
    if (id < s_instance_count && s_fdcan_instances[id].is_assigned) {
        s_fdcan_instances[id].rx_callback = callback;
    }
}

/**
 *------------------------------------------------------------
 *                        HAL CALLBACKS
 *------------------------------------------------------------
 */

/**
 * @brief FDCAN Error Status Callback (Bus Off Auto Recovery)
 * @details HAL에서 Bus Off 또는 Error Passive 발생 시 호출
 */
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    /* Bus Off 자동 복구 */
    if ((ErrorStatusITs & FDCAN_IT_BUS_OFF) != RESET) {
        /* Bus Off 복구: INIT 비트 클리어 (즉시 재시작) */
        CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_INIT);
        
        /* TODO: Bus Off 이벤트 로깅 (향후 Debug 시스템에서 처리) */
    }
    
    /* Error Passive 경고 */
    if ((ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE) != RESET) {
        /* TEC >= 128: Error Passive 상태 (경고) */
        uint32_t tec = (hfdcan->Instance->ECR & FDCAN_ECR_TEC_Msk) >> FDCAN_ECR_TEC_Pos;
        
        /* TODO: Error Passive 이벤트 로깅 (향후 Debug 시스템에서 처리) */
        (void)tec;
    }
}

/**
 * @brief FDCAN 수신 FIFO 0에 새 메시지가 도착했을 때 HAL 라이브러리에 의해 호출되는 콜백 함수.
 * @details 
 * [V3.7 최적화 - While Loop 완전 제거]
 * - HW FIFO에서 **1개만** 읽고 즉시 ISR 종료
 * - ISR 체류 시간 최소화 (~2µs)
 * - 남은 메시지는 HW가 자동으로 재호출 (인터럽트 재발생)
 * 
 * [이유]
 * - While Loop 위험 완전 제거 (무한 루프 불가능)
 * - 고우선순위 인터럽트 차단 최소화
 * - Real-Time 보장 (예측 가능한 ISR 시간)
 * 
 * [Trade-off]
 * - FIFO에 3개 쌓이면 ISR 3번 호출 (Context Switching 증가)
 * - 하지만 안전성이 더 중요 (총 시간은 거의 동일)
 */
/**
 * @brief FDCAN 수신 FIFO 0에 새 메시지가 도착했을 때 HAL 라이브러리에 의해 호출되는 콜백 함수.
 * @details 
 * [RTOS Mode - Semaphore Give ONLY] ⭐ V4.0
 * - xSemaphoreGiveFromISR()만 호출 (~0.5µs)
 * - HW FIFO 읽기는 System Task에서 처리 (Batch Processing)
 * - ISR Latency 최소화
 * 
 * [BareMetal Mode - Direct Callback]
 * - HW FIFO에서 1개만 읽고 Callback 호출
 * - HW가 자동으로 인터럽트 재발생
 * 
 * [성능 (STM32H743, 480MHz, Cache ON)]
 * - RTOS: ~0.5µs (Semaphore Give)
 * - BareMetal: ~5µs (GetRxMessage + Callback)
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs)
{
    if (!(RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) {
        return;
    }
    
    IOIF_FDCAN_Inst_t* instance = _FindInstanceByHandle(hfdcan);
    if (instance == NULL || !instance->is_assigned) {
        return;
    }

#if defined(USE_FREERTOS) && defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
    /* ===== V5.0 ISR-Direct: Batch drain + callback (NVIC 4, no FreeRTOS API) =====
     * - FDCAN ISR가 configMAX_SYSCALL(5) 위(NVIC 4)이므로 FreeRTOS API 호출 금지
     * - ISR에서 직접 FIFO 전체 drain + 콜백 호출 (~5-10µs/msg)
     * - f_sync/f_write 중에도 PDO 수신 보장 → Real Gap = 0
     */
    {
        IOIF_FDCAN_Msg_t msg;
        FDCAN_RxHeaderTypeDef rxHeader;

        while (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0,
                                       &rxHeader, msg.data) == HAL_OK)
        {
            msg.id  = rxHeader.Identifier;
            msg.len = _DLC2Len(rxHeader.DataLength);

            if (instance->rx_callback != NULL) {
                instance->rx_callback(&msg);
            }
        }
    }

#elif defined(USE_FREERTOS)
    /* ===== V4.0 RTOS: Semaphore Give ONLY ===== */
    if (instance->rx_semaphore != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(instance->rx_semaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

#else
    /* ===== BareMetal Mode: Direct Callback ===== */
    /* [방어 로직] Callback NULL 체크 */
    if (instance->rx_callback == NULL) {
        // 주인 없는 데이터는 1개만 읽고 버림
        FDCAN_RxHeaderTypeDef dummyHeader;
        uint8_t dummyData[64];
        HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &dummyHeader, dummyData);
        return;
    }
    
    IOIF_FDCAN_Msg_t received_msg;
    FDCAN_RxHeaderTypeDef rxHeader;

    /* 1개만 읽고 즉시 ISR 종료 (HW가 자동 재발생) */
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxHeader, received_msg.data) == HAL_OK)
    {
        received_msg.id = rxHeader.Identifier;
        received_msg.len = _DLC2Len(rxHeader.DataLength);
        
        /* Callback 호출 (Device Layer) */
        instance->rx_callback(&received_msg);
    }
#endif
}

/**
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 */

static IOIF_FDCAN_Inst_t* _FindInstanceByHandle(FDCAN_HandleTypeDef* hfdcan)
{
    for (uint32_t i = 0; i < s_instance_count; ++i) {
        if (s_fdcan_instances[i].hfdcan == hfdcan) {
            return &s_fdcan_instances[i];
        }
    }
    return NULL;
}

static uint32_t _Len2DLC(uint8_t len)
{
    // CAN-FD Length to DLC conversion table
    static const uint32_t len_to_dlc[] = {
        FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2, FDCAN_DLC_BYTES_3, FDCAN_DLC_BYTES_4, FDCAN_DLC_BYTES_5, FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7, FDCAN_DLC_BYTES_8
    };
    if (len <= 8) return len_to_dlc[len];
    if (len <= 12) return FDCAN_DLC_BYTES_12;
    if (len <= 16) return FDCAN_DLC_BYTES_16;
    if (len <= 20) return FDCAN_DLC_BYTES_20;
    if (len <= 24) return FDCAN_DLC_BYTES_24;
    if (len <= 32) return FDCAN_DLC_BYTES_32;
    if (len <= 48) return FDCAN_DLC_BYTES_48;
    return FDCAN_DLC_BYTES_64;
}

static uint8_t _DLC2Len(uint8_t dlc)
{
    static const uint8_t dlc_to_len[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    if (dlc > 15) return 64;
    return dlc_to_len[dlc];
}


#if defined(USE_FREERTOS) && !defined(IOIF_FDCAN_ISR_DIRECT_ENABLE)
/**
 * @brief [V4.0 IOIF 내부] FDCAN RxTask (세마포어 Give/Take 동일 소스 원칙)
 * @note V5.0 ISR-Direct 모드에서는 컴파일 제외
 */
static void _IOIF_FDCAN_RxTask(void* argument)
{
    IOIF_FDCANx_t id = (IOIF_FDCANx_t)(uintptr_t)argument;

    if (id >= IOIF_FDCAN_MAX_INSTANCES) {
        vTaskDelete(NULL);
        return;
    }

    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[id];
    IOIF_FDCAN_Msg_t msg;
    FDCAN_RxHeaderTypeDef rxHeader;

    for (;;) {
        if (xSemaphoreTake(inst->rx_semaphore, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        while (HAL_FDCAN_GetRxMessage(inst->hfdcan, FDCAN_RX_FIFO0,
                                       &rxHeader, msg.data) == HAL_OK)
        {
            msg.id  = rxHeader.Identifier;
            msg.len = _DLC2Len(rxHeader.DataLength);

            if (inst->rx_callback != NULL) {
                inst->rx_callback(&msg);
            }
        }
    }
}
#endif /* USE_FREERTOS && !ISR_DIRECT */

#if defined(USE_FREERTOS)
/**
 * @brief [RTOS Only] FDCAN 메시지 수신 (Non-Blocking) - 공개 API
 * @param id FDCAN 인스턴스 ID
 * @param msg 수신 메시지 구조체 포인터
 * @return AGRBStatus_OK (메시지 수신), AGRBStatus_ERROR (FIFO 비어있음)
 * 
 * @note IOIF 내부 RxTask가 기본 처리. 필요시 외부에서도 호출 가능.
 */
AGRBStatusDef IOIF_FDCAN_Receive(IOIF_FDCANx_t id, IOIF_FDCAN_Msg_t* msg)
{
    if (id >= s_instance_count || msg == NULL) {
        return AGRBStatus_ERROR;
    }

    IOIF_FDCAN_Inst_t* inst = &s_fdcan_instances[id];
    if (inst->hfdcan == NULL) {
        return AGRBStatus_ERROR;
    }

    /* HW FIFO에서 메시지 1개 읽기 (Non-Blocking) */
    FDCAN_RxHeaderTypeDef rxHeader;
    HAL_StatusTypeDef status = HAL_FDCAN_GetRxMessage(
        inst->hfdcan, 
        FDCAN_RX_FIFO0, 
        &rxHeader, 
        msg->data
    );

    if (status != HAL_OK) {
        /* HW FIFO 비어있음 */
        return AGRBStatus_ERROR;
    }

    /* CAN-ID 및 길이 변환 */
    msg->id = rxHeader.Identifier;
    msg->len = _DLC2Len(rxHeader.DataLength);

    return AGRBStatus_OK;
}
#endif

#endif /* AGRB_IOIF_FDCAN_ENABLE */
