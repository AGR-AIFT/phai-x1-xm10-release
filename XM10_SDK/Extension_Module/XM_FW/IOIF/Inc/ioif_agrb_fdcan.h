#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_FDCAN_ENABLE)

#pragma once // 현대 컴파일러를 위한 최적화

#ifndef IOIF_FDCAN_INC_IOIF_AGRB_FDCAN_H_
#define IOIF_FDCAN_INC_IOIF_AGRB_FDCAN_H_

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_fdcan.h"

#include <stdint.h>
#include <stdbool.h>

#if defined(USE_FREERTOS_DMA)
#include "cmsis_os2.h" // For osThreadAttr_t
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
#if defined(USE_FREERTOS_DMA)
#define IOIF_FDCAN_START(id, attr)            IOIF_FDCAN_Start((id), (attr))
#else // BareMetal
#define IOIF_FDCAN_START(id)                  IOIF_FDCAN_Start(id)
#endif
#define IOIF_FDCAN_TRANSMIT(id, ...)          IOIF_FDCAN_Transmit((id), __VA_ARGS__)
#define IOIF_FDCAN_REGISTER_FILTER_CALLBACK(id, cb)  IOIF_FDCAN_RegisterFilterCallback((id), (cb))
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

// 필터 콜백 함수의 타입 정의: CAN ID를 받아 통과시킬지(true) 말지(false) 결정
typedef bool (*IOIF_FDCAN_FilterCallback_t)(uint16_t canId);
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
 * @param id 생성된 인스턴스의 ID가 저장될 포인터.
 * @param hfdcan_ptr 사용할 FDCAN의 HAL 핸들 포인터 (예: (void*)&hfdcan1).
 * 상위 계층은 이것이 무엇인지 알 필요 없이 전달만 합니다.
 * @return AGRBStatus_OK on success.
 */
AGRBStatusDef IOIF_FDCAN_AssignInstance(IOIF_FDCANx_t* id, FDCAN_HandleTypeDef* hfdcan);


#if defined(USE_FREERTOS_DMA)
/**
 * @brief FDCAN (RTOS Mode): 필터, 인터럽트, Rx 태스크 생성을 포함하여 시작합니다.
 * @param id IOIF_FDCAN_AssignInstance를 통해 발급받은 ID.
 * @param rx_task_attr .
 * @return AGRBStatus_OK on success.
 */
AGRBStatusDef IOIF_FDCAN_Start(IOIF_FDCANx_t id, const osThreadAttr_t* rx_task_attr);
#else // BareMetal
/**
 * @brief FDCAN 인스턴스의 통신을 시작합니다. (필터 설정 및 인터럽트 활성화 포함)
 * @param id IOIF_FDCAN_AssignInstance를 통해 발급받은 ID.
 * @return AGRBStatus_OK on success.
 */
AGRBStatusDef IOIF_FDCAN_Start(IOIF_FDCANx_t id);
#endif

/**
 * @brief 지정된 FDCAN 인스턴스를 통해 메시지를 전송합니다.
 * @param id IOIF_FDCANx_t 핸들.
 * @param msgId 전송할 메시지의 CAN ID.
 * @param txData 전송할 데이터의 포인터.
 * @param len 전송할 데이터의 길이 (바이트).
 * @return AGRBStatus_OK on success.
 */
AGRBStatusDef IOIF_FDCAN_Transmit(IOIF_FDCANx_t id, uint16_t msgId, uint8_t* txData, uint32_t len);

// 
/**
 * @brief FDCAN 수신 인터럽트 발생 시 호출될 소프트웨어 필터 콜백 함수를 등록합니다.
 * @param id IOIF_FDCANx_t 핸들.
 * @param callback 등록할 콜백 함수 포인터. NULL로 전달 시 콜백 비활성화.
 */
void IOIF_FDCAN_RegisterFilterCallback(IOIF_FDCANx_t id, IOIF_FDCAN_FilterCallback_t callback);

/**
 * @brief FDCAN 수신 인터럽트 발생 시 호출될 콜백 함수를 등록합니다.
 * @param id IOIF_FDCANx_t 핸들.
 * @param callback 등록할 콜백 함수 포인터. NULL로 전달 시 콜백 비활성화.
 */
void IOIF_FDCAN_RegisterRxCallback(IOIF_FDCANx_t id, IOIF_FDCAN_RxCallback_t callback);

#endif /* IOIF_FDCAN_INC_IOIF_AGRB_FDCAN_H_ */

#endif /* AGRB_IOIF_FDCAN_ENABLE */