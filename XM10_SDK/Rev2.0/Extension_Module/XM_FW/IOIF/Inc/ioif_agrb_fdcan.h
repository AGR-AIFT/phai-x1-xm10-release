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
