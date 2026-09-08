#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_FDCAN_ENABLE)

#pragma once // 현대 컴파일러를 위한 최적화

#ifndef IOIF_FDCAN_INC_IOIF_AGRB_FDCAN_H_
#define IOIF_FDCAN_INC_IOIF_AGRB_FDCAN_H_

#include <stdint.h>
#include <stdbool.h>

/* STM32 HAL Headers (MCU별 자동 선택) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h743xx.h"
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_fdcan.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx.h"
    #include "stm32g4xx_hal.h"
	#include "stm32g4xx_hal_fdcan.h"
#else
    #error "Unsupported MCU series for IOIF FDCAN"
#endif

#if defined(USE_FREERTOS)
#include "cmsis_os2.h"  // For osThreadAttr_t
#include "FreeRTOS.h"   // For SemaphoreHandle_t
#include "semphr.h"     // For xSemaphoreCreateBinary()
#endif

/**
 *-----------------------------------------------------------
 *            PUBLIC DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 */

#define IOIF_FDCAN_MAX_INSTANCES 2  // FDCAN1, FDCAN2 최대 2개 지원
// FDCAN 통신 채널을 식별하기 위한 핸들(ID)
#define IOIF_FDCAN_INVALID_ID (0xFFFFFFFF)

#define IOIF_TDC_OFFSET 0x0B // Transmitter Delay Compensation Offset. This parameter must be a number between 0x00 and 0x7F, 0x0B = 11 (DataPrescaler * DataTimeSeg1)
#define IOIF_TDC_FILTER 0x00 // tdcFilter Transmitter Delay Compensation Filter Window Length. This parameter must be a number between 0x00 and 0x7F.

#define IOIF_FDCAN_TX_BUFF_SIZE 64
#define IOIF_FDCAN_RX_BUFF_SIZE 64

// --- 사용 편의성을 위한 매크로 API ---
#define IOIF_FDCAN_ASSIGN(id, hfdcan)         IOIF_FDCAN_AssignInstance(&(id), (hfdcan))
#define IOIF_FDCAN_START(id)                  IOIF_FDCAN_Start(id)
#define IOIF_FDCAN_TRANSMIT(id, ...)          IOIF_FDCAN_Transmit((id), __VA_ARGS__)
#define IOIF_FDCAN_REGISTER_CALLBACK(id, cb)  IOIF_FDCAN_RegisterRxCallback((id), (cb))

/**
 *-----------------------------------------------------------
 *                       PUBLIC TYPES
 *-----------------------------------------------------------
 */

// FDCAN 통신 채널을 식별하기 위한 핸들(ID)
typedef uint32_t IOIF_FDCANx_t;

// FDCAN 수신 메시지를 담을 구조체
typedef struct {
    uint16_t id;
    uint32_t  len;
    uint8_t  data[IOIF_FDCAN_RX_BUFF_SIZE];
} IOIF_FDCAN_Msg_t;

typedef enum {
    IOIF_FDCAN_STATUS_OK = 0,
    IOIF_FDCAN_STATUS_ERROR,
} IOIF_FDCANState_t;

/** @brief RX FIFO 인덱스 (IOIF_FDCAN_ConfigRxFifoMode 인자) */
#define IOIF_FDCAN_RX_FIFO0     (0U)
#define IOIF_FDCAN_RX_FIFO1     (1U)

/**
 * @brief RX FIFO 운용 모드 — FIFO 가 가득 찬 순간의 동작 (그 전에는 두 모드가 동일).
 * @details 어떤 트래픽을 어느 FIFO 로 받을지는 System Layer 가 필터로 정하므로,
 *          그 FIFO 의 full 정책도 System Layer 가 선언한다 (IOIF 는 정책을 고르지 않는다 —
 *          IOIF_FDCAN_ConfigFilter 와 동일 원칙).
 */
typedef enum {
    /** full 시 **새로 온** 프레임을 버리고 RF0L/RF1L(유실 카운터)을 세운다.
     *  줄 서 있던 프레임이 보존되므로 트랜잭션 트래픽(NMT/SDO/HB/EMCY)에 맞다.
     *  HW 리셋값이자 IOIF 기본값 — 미선언 시 이 모드. */
    IOIF_FDCAN_RXFIFO_BLOCKING  = 0,
    /** full 시 **가장 오래된** 프레임을 덮어쓴다. "최신 하나만 의미 있는" 스트림
     *  (TPDO 텔레메트리, SYNC)에 맞다.
     *  - ST HAL 의 overwrite+full 읽기 경로(인덱스를 모듈로가 아닌 마스크로 계산 —
     *    G4 v1.2.6 hal_fdcan.c:2254 / H7 v1.11.6 :2994)는 IOIF 가 읽기 직전 full 을
     *    감지해 oldest 를 직접 ack 로 버리는 방식(RM 권고 순서)으로 **우회한다**.
     *  - RFxL 이 서지 않으므로 유실은 IOIF_FDCAN_GetRxFifoDiscardCounts() 로 본다. */
    IOIF_FDCAN_RXFIFO_OVERWRITE = 1,
} IOIF_FDCAN_RxFifoMode_t;

// FDCAN 수신 콜백 함수의 타입 정의
typedef void (*IOIF_FDCAN_RxCallback_t)(IOIF_FDCAN_Msg_t* msg);

/**
 *------------------------------------------------------------
 *                   PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief 새로운 FDCAN 인스턴스를 생성하고 ID를 발급합니다.
 * @details system_startup에서 각 FDCAN 채널에 대해 호출되어야 합니다.
 * 
 * [RTOS Mode]
 * - Rx Binary Semaphore 생성 (ISR → Task 신호용)
 * - Tx Mutex 생성 (Priority Inheritance, Thread-Safe 전송)
 * 
 * [BareMetal Mode]
 * - Semaphore/Mutex 생성 안함
 * - ISR 우선순위 차별화로 보호
 * 
 * @param id 생성된 인스턴스의 ID가 저장될 포인터.
 * @param hfdcan 사용할 FDCAN의 HAL 핸들 포인터.
 * @return AGRBStatus_OK on success.
 */
AGRBStatusDef IOIF_FDCAN_AssignInstance(IOIF_FDCANx_t* id, FDCAN_HandleTypeDef* hfdcan);

/**
 * @brief FDCAN 인스턴스의 통신을 시작합니다. (필터 설정 및 인터럽트 활성화 포함)
 * @param id IOIF_FDCAN_AssignInstance를 통해 발급받은 ID.
 * @return AGRBStatus_OK on success.
 */
AGRBStatusDef IOIF_FDCAN_Start(IOIF_FDCANx_t id);

/**
 * @brief FDCAN Restricted Operation Mode 전환 (CCCR.ASM 비트 토글)
 * @param id         IOIF_FDCANx_t 핸들
 * @param restricted true=Restricted Operation Mode 진입, false=Normal Mode 복원
 * @return AGRBStatus_OK 성공 / AGRBStatus_ERROR invalid id / AGRBStatus_FAILED Stop·Start 실패
 * @details ASM=1 이면 외부 라인 정상 + TEC 누적·Bus-Off 차단(peer 미연결 SI 측정 안전).
 *          Stop→CCCR.ASM→Start 시퀀스를 캡슐화. filter/notification 보존. 진단/stimulus 전용.
 * @note 단일 컨텍스트 가정 — Stop/Start 윈도우 중 동일 인스턴스 Transmit 금지.
 */
AGRBStatusDef IOIF_FDCAN_SetRestrictedMode(IOIF_FDCANx_t id, bool restricted);

/**
 * @brief 지정된 FDCAN 인스턴스를 통해 메시지를 전송합니다. (Thread-Safe)
 * @param id IOIF_FDCANx_t 핸들.
 * @param can_id 전송할 메시지의 CAN ID (11-bit Standard or 29-bit Extended).
 * @param txData 전송할 데이터의 포인터.
 * @param len 전송할 데이터의 길이 (바이트, 최대 64).
 * @return AGRBStatus_OK: 성공, AGRBStatus_TIMEOUT: Mutex 실패, AGRBStatus_ERROR: 전송 실패
 * 
 * @note Thread-Safe: RTOS 환경에서 내부 Tx Mutex로 보호 (Priority Inheritance)
 * @note BareMetal: ISR 우선순위 차별화로 보호 (Mutex 없음)
 */
AGRBStatusDef IOIF_FDCAN_Transmit(IOIF_FDCANx_t id, uint32_t can_id, const uint8_t* txData, uint8_t len);

/**
 * @brief RT(1kHz) 경로용 논블로킹 송신 — Transmit 과 동일하되 Tx Mutex try-lock(0).
 * @return AGRBStatus_OK: 성공, AGRBStatus_BUSY: Tx Mutex 경합(대기 0, 미송신),
 *         AGRBStatus_ERROR: HW FIFO 전송 실패
 * @note RTOS: 블로킹 0 보장 — 1ms 주기 task 의 5ms TX_LOCK 대기 제거 (2026-07-23).
 *       경합/실패 시 호출자가 드롭 계측 후 다음 주기 최신값으로 대체(latest-wins).
 * @note BareMetal: Transmit 과 동일 동작 (try-lock 이 항상 성공).
 */
AGRBStatusDef IOIF_FDCAN_TransmitTry(IOIF_FDCANx_t id, uint32_t can_id, const uint8_t* txData, uint8_t len);

/**
 * @brief Classic-CAN(비-FD, BRS off) 프레임 전송 — IOIF_FDCAN_Transmit 과 동일 Tx Mutex.
 * @param len 전송할 데이터 길이 (바이트, Classic CAN 최대 8).
 * @return AGRBStatus_OK: 성공, AGRBStatus_TIMEOUT: Mutex 실패, AGRBStatus_PARAM_ERROR: len>8
 * @note Ch1 SYNC 처럼 Classic-CAN 와이어 포맷을 유지하면서 Tx Mutex 보호가 필요한 경로용.
 */
AGRBStatusDef IOIF_FDCAN_TransmitClassic(IOIF_FDCANx_t id, uint32_t can_id, const uint8_t* txData, uint8_t len);

/**
 * @brief Tx FIFO 여유 공간 확인 (HW 레지스터 Read-Only)
 * @param id IOIF_FDCANx_t 핸들
 * @return 여유 공간 (0=Full, max=TxFifoQueueElmtsNbr)
 * @note Thread-Safe: HW 레지스터 Read-Only (Mutex 불필요)
 */
uint32_t IOIF_FDCAN_GetTxFifoFreeLevel(IOIF_FDCANx_t id);

/**
 * @brief 현재 HW Tx 에 pending 된 메시지 개수 (TXBRP popcount)
 * @param id IOIF_FDCANx_t 핸들
 * @return pending buffer 개수 (0 = idle, max = Tx FIFO/Queue 크기). invalid id 시 0
 * @details TXBRP (Tx Buffer Request Pending) 레지스터의 set bit 수를 반환. Queue 모드에서
 *          `GetTxFifoFreeLevel` 이 full flag 만 노출하므로, backpressure 진단 및 Live
 *          Expression 관측에 적합. STM32H7/G4 FDCAN 공통.
 * @note Thread-Safe: HW 레지스터 Read-Only (Mutex 불필요)
 */
uint32_t IOIF_FDCAN_GetTxInFlightCount(IOIF_FDCANx_t id);

/**
 * @brief Rx FIFO0 채움 수준 확인 (HW 레지스터 Read-Only)
 * @param id IOIF_FDCANx_t 핸들
 * @return 채움 수준 (0=Empty, max=RxFifo0ElmtsNbr)
 * @note Thread-Safe: HW 레지스터 Read-Only (Mutex 불필요)
 */
uint32_t IOIF_FDCAN_GetRxFifo0FillLevel(IOIF_FDCANx_t id);

/**
 * @brief RX FIFO0/FIFO1 유실(RF0L/RF1L) 누적 카운터 조회
 * @details IOIF_FDCAN_Start() 는 양쪽 FIFO 의 NEW_MESSAGE/LOST IT 를 켜고, full 정책은
 *          System Layer 가 IOIF_FDCAN_ConfigRxFifoMode() 로 선언한 값을 적용한다(미선언=BLOCKING).
 *          System Layer 가 IOIF_FDCAN_ConfigFilter() 에서 FDCAN_FILTER_TO_RXFIFO1 로 라우팅하면
 *          FIFO1 이 활성화된다 (예: SM 의 SYNC 전용 FIFO — 1kHz SYNC 가 PnP/SDO 프레임을 밀어내지
 *          못하게 격리). 두 FIFO 모두 동일 rx_callback 으로 전달된다.
 * @param id IOIF_FDCANx_t 핸들
 * @param[out] fifo0_lost RF0L 누적 (NULL 허용)
 * @param[out] fifo1_lost RF1L 누적 (NULL 허용)
 * @return AGRBStatus_OK / AGRBStatus_NOT_INITIALIZED
 * @note Thread-Safe: volatile 32-bit 단일 read
 */
AGRBStatusDef IOIF_FDCAN_GetRxFifoLostCounts(IOIF_FDCANx_t id, uint32_t* fifo0_lost, uint32_t* fifo1_lost);

/**
 * @brief OVERWRITE FIFO 에서 full 상태로 버린 oldest 프레임 수 조회
 * @details overwrite 모드는 RFxL(유실 IT)을 세우지 않아 GetRxFifoLostCounts 가 0 을 유지한다.
 *          IOIF 가 읽기 직전 full 을 감지해 oldest 를 직접 ack 로 버릴 때마다 이 값을 올리므로,
 *          OVERWRITE FIFO 의 유실은 이 카운터로 본다 (BLOCKING FIFO 는 항상 0).
 * @param id IOIF_FDCANx_t 핸들
 * @param[out] fifo0_discard FIFO0 누적 (NULL 허용)
 * @param[out] fifo1_discard FIFO1 누적 (NULL 허용)
 * @return AGRBStatus_OK / AGRBStatus_NOT_INITIALIZED
 * @note Thread-Safe: volatile 32-bit 단일 read
 */
AGRBStatusDef IOIF_FDCAN_GetRxFifoDiscardCounts(IOIF_FDCANx_t id, uint32_t* fifo0_discard, uint32_t* fifo1_discard);

/**
 * @brief FDCAN 에러 카운터 조회 (HW 레지스터 Read-Only)
 * @param id IOIF_FDCANx_t 핸들
 * @param[out] tec Transmit Error Counter (0~255)
 * @param[out] rec Receive Error Counter (0~127)
 * @return AGRBStatus_OK on success
 * @note Thread-Safe: HW 레지스터 Read-Only (Mutex 불필요)
 */
AGRBStatusDef IOIF_FDCAN_GetErrorCounters(IOIF_FDCANx_t id, uint8_t* tec, uint8_t* rec);

/**
 * @brief IOIF 관측 FDCAN 이벤트 누적 통계
 * @details `IOIF_FDCAN_GetErrorCounters` 가 실시간 HW TEC/REC snapshot 을 반환하는 반면,
 *          본 구조는 `HAL_FDCAN_ErrorStatusCallback` 이 관측한 bus-off / error-passive
 *          이벤트의 누적 카운트를 담는다. 텔레메트리/진단 용도.
 */
typedef struct {
    uint32_t bus_off_count;              /**< Bus Off 이벤트 누적 횟수 */
    uint32_t error_passive_count;        /**< Error Passive 이벤트 누적 횟수 */
    uint32_t rx_fifo0_lost_count;        /**< RxFIFO0 overflow(RF0L) 프레임 유실 누적 (2026-07-13) */
    uint8_t  last_tec_at_error_passive;  /**< 가장 최근 Error Passive 발생 시 TEC 값 */
} IOIF_FDCAN_ErrorStats_t;

/**
 * @brief FDCAN 누적 에러 이벤트 통계 조회
 * @param id IOIF_FDCANx_t 핸들
 * @param[out] stats 누적 통계 구조체
 * @return AGRBStatus_OK on success
 * @note Thread-Safe: ISR 에서 volatile 증가, Task 에서 읽기. 필드별 uint32_t 는 ARM M4/M7
 *       atomic 읽기 보장. struct 복사 중 ISR 개입 시 필드간 부분 최신 가능 (통계 용도 허용).
 */
AGRBStatusDef IOIF_FDCAN_GetErrorStats(IOIF_FDCANx_t id, IOIF_FDCAN_ErrorStats_t* stats);

/**
 * @brief SW Tx Queue 사용량 통계 조회
 * @param id IOIF_FDCANx_t 핸들
 * @param[out] now  현재 큐잉된 메시지 개수 (0 ~ IOIF_FDCAN_SW_TX_QUEUE_SIZE). NULL 허용
 * @param[out] peak 관측된 최대 큐 사용량 (high-water mark). NULL 허용
 * @param[out] drop Queue Full 로 인한 drop 누적 횟수. NULL 허용
 * @return AGRBStatus_OK / AGRBStatus_ERROR (invalid id)
 * @details `IOIF_FDCAN_GetTxInFlightCount` 가 HW Tx pending 을 보는 반면, 본 API 는
 *          IOIF 내부의 SW queue 사용량을 본다. 두 값의 합 = 총 outstanding TX.
 * @note Thread-Safe: volatile read. Task context 에서 호출.
 */
AGRBStatusDef IOIF_FDCAN_GetQueueStats(IOIF_FDCANx_t id,
                                        uint8_t* now,
                                        uint8_t* peak,
                                        uint32_t* drop);

/**
 * @brief FDCAN 프로토콜 상태 조회 (HW 레지스터 Read-Only)
 * @param id IOIF_FDCANx_t 핸들
 * @param[out] lec Last Error Code (0~7, PSR.LEC)
 * @param[out] bus_status Bus status flags (bit0:BusOff, bit1:Warning, bit2:ErrorPassive)
 * @return AGRBStatus_OK on success
 * @note Thread-Safe: HW 레지스터 Read-Only (Mutex 불필요)
 */
AGRBStatusDef IOIF_FDCAN_GetBusStatus(IOIF_FDCANx_t id, uint8_t* lec, uint8_t* bus_status);

/**
 * @brief Software Tx Queue에 메시지 추가 (Thread-Safe)
 * @param id IOIF_FDCANx_t 핸들
 * @param can_id CAN ID
 * @param data 전송 데이터
 * @param len 데이터 길이
 * @param priority 우선순위 (0=최고, 255=최저, 현재 미사용)
 * @return AGRBStatus_OK: 성공, AGRBStatus_TIMEOUT: Mutex 실패, AGRBStatus_ERROR: Queue Full
 * 
 * @note Thread-Safe: RTOS 환경에서 내부 Tx Mutex로 보호
 * @note Tx FIFO Full 시 Queue에 저장, IOIF_FDCAN_ProcessQueue에서 자동 전송
 */
AGRBStatusDef IOIF_FDCAN_QueueMessage(IOIF_FDCANx_t id, uint32_t can_id, const uint8_t* data, uint8_t len, uint8_t priority);

/**
 * @brief Software Tx Queue 처리 (Thread-Safe, Main Loop에서 주기 호출)
 * @param id IOIF_FDCANx_t 핸들
 * @return 처리된 메시지 개수
 * 
 * @note Thread-Safe: RTOS 환경에서 내부 Tx Mutex로 보호
 * @note Queue에서 FIFO 순서로 Tx FIFO에 전송 (여유 공간만큼)
 */
uint32_t IOIF_FDCAN_ProcessQueue(IOIF_FDCANx_t id, uint8_t reserved_slots);

/**
 * @brief ServicePeriodic 결과 — 호출자가 no-pending 과 pending-잔존(busy)을 구분해
 *        bus-off flush 완료 전 신규 송신을 억제할 수 있게 한다 (2026-07-22 Codex
 *        재리뷰 P2-1: bool 반환은 세 상태를 뭉개 stale 프레임 '뒤에' 새 프레임이
 *        큐잉되는 순서 역전을 호출자가 감지할 수 없었다).
 */
typedef enum {
    IOIF_FDCAN_SVC_NO_PENDING = 0,   /**< flush 할 것 없음 (정상 상태 / invalid id 포함) */
    IOIF_FDCAN_SVC_FLUSHED,          /**< 이번 호출에서 bus-off flush 수행 완료 */
    IOIF_FDCAN_SVC_PENDING_BUSY,     /**< flush 필요하나 TX lock 경합으로 미수행 —
                                          호출자는 이 채널의 신규 송신을 억제하고
                                          다음 주기에 재시도할 것 */
} IOIF_FDCAN_SvcResult_t;

/**
 * @brief [필수 주기 서비스] Bus-Off 복구 flush 전용 경량 서비스 (SW queue drain 없음)
 * @details direct-transmit 모듈(XM10/CM-WH 등 IOIF_FDCAN_Transmit 직접 호출·SW Tx
 *          Queue 미사용)이 IOIF_FDCAN_ProcessQueue 를 주기 호출하지 않아도, bus-off
 *          복구 직후 stale HW Tx 버퍼가 복구된 버스로 버스트 송신되는 것을 막을 수 있도록
 *          bus-off flush(test-and-clear + HW Tx abort + SW queue flush)만 떼어낸 필수
 *          서비스. **모든 FDCAN 사용 모듈은 ProcessQueue 또는 본 함수 중 하나를 반드시
 *          주기 호출**해야 bus-off 보호를 받는다.
 * @param id IOIF_FDCANx_t 핸들
 * @return IOIF_FDCAN_SvcResult_t — PENDING_BUSY 면 stale 프레임이 남아 있으므로
 *         호출자는 이번 주기 해당 채널의 신규 송신을 억제해야 순서 역전이 없다.
 * @note ProcessQueue 를 이미 주기 호출하는 모듈(IMU/EMG 등)은 그 안에서 동일 처리가
 *       일어나므로 본 함수를 추가로 부를 필요 없음. 설령 둘 다 불러도 mutex 안
 *       test-and-clear 라 flush 는 1회만 수행됨(idempotent, double-flush 없음).
 * @note Thread-Safe: flush 는 TX try-lock(0) 하에 수행 — RT(1kHz) 주기 컨텍스트에서
 *       블로킹 0 보장 (구 TX_LOCK 5ms 대기 제거, 경합 시 PENDING_BUSY 반환 후
 *       다음 주기 재시도). Task context 에서 주기 호출.
 * @note 평상시(flush 불필요) 경로는 lockless(volatile flag pre-check) — 1kHz 주기
 *       호출에도 뮤텍스 오버헤드가 없다. 실제 flush 발생 시에만 lock 시도.
 */
IOIF_FDCAN_SvcResult_t IOIF_FDCAN_ServicePeriodic(IOIF_FDCANx_t id);

/**
 * @brief 모든 Pending Tx 요청 취소 (Thread-Safe)
 * @param id IOIF_FDCANx_t 핸들
 */
void IOIF_FDCAN_AbortAllTx(IOIF_FDCANx_t id);

/**
 * @brief FDCAN 수신 인터럽트 발생 시 호출될 콜백 함수를 등록합니다.
 * @param id IOIF_FDCANx_t 핸들.
 * @param callback 등록할 콜백 함수 포인터. NULL로 전달 시 콜백 비활성화.
 */
void IOIF_FDCAN_RegisterRxCallback(IOIF_FDCANx_t id, IOIF_FDCAN_RxCallback_t callback);

/**
 * @brief [PUBLIC] FDCAN 필터 설정 (System Layer에서 호출)
 * @details IOIF_FDCAN_Start() 호출 후에 이 함수를 호출하여 특정 모듈에 맞는 필터를 설정할 수 있습니다.
 * @param[in] id            FDCAN 인스턴스 ID
 * @param[in] filter_config HAL FDCAN 필터 설정 구조체 포인터
 * @return AGRBStatus_OK (성공), AGRBStatus_ERROR (실패)
 * 
 * @example
 * // IMU Hub 전용 필터 설정 예시 (system_startup.c에서 호출)
 * FDCAN_FilterTypeDef filter = {
 *     .IdType = FDCAN_STANDARD_ID,
 *     .FilterIndex = 1,
 *     .FilterType = FDCAN_FILTER_MASK,
 *     .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
 *     .FilterID1 = 0x608,  // SDO Request
 *     .FilterID2 = 0x7FF,  // Exact match
 * };
 * IOIF_FDCAN_ConfigFilter(fdcan_id, &filter);
 */
AGRBStatusDef IOIF_FDCAN_ConfigFilter(IOIF_FDCANx_t id, FDCAN_FilterTypeDef* filter_config);

/**
 * @brief RX FIFO0/FIFO1 의 full 정책 선언 (System Layer 소유)
 * @details 필터로 트래픽을 FIFO 에 배분한 쪽이 그 FIFO 의 full 정책도 정한다.
 *          예) XM: FIFO0=TPDO 텔레메트리(OVERWRITE) / FIFO1=NMT·SDO·HB(BLOCKING)
 *              SM: FIFO0=NMT·SDO·HB(BLOCKING)      / FIFO1=SYNC(OVERWRITE)
 * @param id    IOIF_FDCANx_t 핸들
 * @param fifo  IOIF_FDCAN_RX_FIFO0 | IOIF_FDCAN_RX_FIFO1
 * @param mode  IOIF_FDCAN_RXFIFO_BLOCKING(기본) | IOIF_FDCAN_RXFIFO_OVERWRITE
 * @return AGRBStatus_OK / NOT_INITIALIZED / PARAM_ERROR / BUSY(Start 이후 호출 — 아래 note)
 * @note **IOIF_FDCAN_Start() 이전에 호출**해야 한다 — HAL 이 STATE_READY 에서만
 *       RXFxC.FxOM 을 받는다. Start 시점에 필터/TDC 와 함께 HW 에 적용된다.
 *       (미호출 = BLOCKING. AssignInstance 의 memset 0 이 기본값을 보장)
 */
AGRBStatusDef IOIF_FDCAN_ConfigRxFifoMode(IOIF_FDCANx_t id, uint8_t fifo, IOIF_FDCAN_RxFifoMode_t mode);

#if defined(USE_FREERTOS)
/**
 * @brief [RTOS Only] FDCAN 메시지 수신 (Non-Blocking)
 * @details 
 * - System Comm Layer (FDCAN_Rx_Task)에서 Batch Processing용으로 호출
 * - HW FIFO에서 메시지 1개 읽기
 * - HW FIFO 비어있으면 즉시 AGRBStatus_ERROR 반환
 * 
 * @param[in]  id  FDCAN 인스턴스 ID
 * @param[out] msg 수신 메시지 구조체 포인터
 * @return AGRBStatus_OK (메시지 수신), AGRBStatus_ERROR (FIFO 비어있음)
 * 
 * @note BareMetal 환경에서는 사용 불가 (Callback 방식만 사용)
 */
AGRBStatusDef IOIF_FDCAN_Receive(IOIF_FDCANx_t id, IOIF_FDCAN_Msg_t* msg);
#endif /* USE_FREERTOS */

#endif /* IOIF_FDCAN_INC_IOIF_AGRB_FDCAN_H_ */

#endif /* AGRB_IOIF_FDCAN_ENABLE */
