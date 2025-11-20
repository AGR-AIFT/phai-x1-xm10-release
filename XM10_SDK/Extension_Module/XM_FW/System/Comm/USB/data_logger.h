/**
 ******************************************************************************
 * @file    data_logger.h
 * @author  HyundoKim
 * @brief   실시간 Binary 데이터 로깅 서비스 (Facade API)
 * @details UserTask가 LogUsbData()를 호출하면,
 * 별도의 저순위 태스크(DataLoggerTask)가 USB에 씁니다.
 * @version 0.1
 * @date    Nov 10, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_USB_DATA_LOGGER_H_
#define SYSTEM_COMM_USB_DATA_LOGGER_H_

#include "module.h"
#include <stdint.h>
#include <stdbool.h>

// FreeRTOS 및 CMSIS-OS 관련 헤더
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief 1회 쓰기(Log)의 최대 크기.
 * @details Application Layer는 이 크기보다 작은 struct를 정의해야 합니다.(300byte)
 */
#define MAX_LOG_PACKET_SIZE     (512) // 1회 쓰기 최대 크기

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief [수정] 로거의 현재 상태 (xm_api.h와 공유)
 * @details 이 enum은 data_logger.h가 소유하며, xm_api.h에 의해 외부에 노출됩니다.
 */
typedef enum {
    LOG_STATUS_IDLE,      // 중지됨 (초기 상태)
    LOG_STATUS_LOGGING,   // 정상 로깅 중
    LOG_STATUS_WARNING_QUEUE_FULL, // 큐가 90% 참 (f_write 멈춤 발생 중)
    LOG_STATUS_ERROR_STOPPED,    // 큐 오버플로우로 로깅이 강제 중지됨
} DataLogger_Status_e;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(EXTERN)
 *-----------------------------------------------------------
 */

extern QueueHandle_t g_logCmdQueue; // 2단계 큐 (LoggerCommand_t)

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief 데이터 로거 서비스를 초기화하고 저순위 태스크를 생성합니다.
 * @details system_startup에서 호출됩니다.
 */
void DataLogger_Init(void);

/**
 * @brief USB 저장 장치(MSC)가 연결되고 준비되었는지 확인합니다.
 * @return 준비되었으면 true, 아니면 false.
 */
bool DataLogger_IsReady(void);

/**
 * @brief [비실시간] 데이터 로깅 세션을 시작합니다.
 * @details 백그라운드 태스크에게 폴더 생성 및 메타데이터 저장을 요청합니다.
 * @param[in] sessionName 저장할 폴더명 (예: "S_001")
 * @param[in] metadata    데이터 구조를 설명하는 텍스트 (파일로 저장됨)
 * @return 요청 성공 시 true.
 */
bool DataLogger_Start(const char* sessionName, const char* metadata);

/**
 * @brief [비실시간] 데이터 로깅을 중지합니다.
 * @details 백그라운드 태스크에게 파일 닫기를 요청합니다.
 */
void DataLogger_Stop(void);

/**
 * @brief [실시간] 2ms UserTask에서 로그 데이터를 전송합니다.
 * @details 이 함수는 비차단(Non-blocking)이며, raw data를 큐에 복사합니다.
 * @param[in] logPacket  저장할 사용자 정의 struct 포인터 (void*)
 * @param[in] packetSize logPacket의 크기 (예: sizeof(MyLogData_t))
 * @return 큐 전송 성공 시 true. (큐가 꽉 차면 false)
 */
bool DataLogger_Log(const void* logPacket, uint32_t packetSize);

/**
 * @brief [실시간] 현재 로깅 상태를 반환합니다 (원자적).
 */
DataLogger_Status_e DataLogger_GetStatus(void);

#endif /* SYSTEM_COMM_USB_DATA_LOGGER_H_ */
