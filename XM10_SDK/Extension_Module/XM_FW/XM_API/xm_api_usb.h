/**
 ******************************************************************************
 * @file    xm_api_usb.h
 * @author  HyundoKim
 * @brief   XM10 USB 데이터 로깅 및 디버그 통신 API
 * @details 
 * 이 모듈은 두 가지 핵심 기능을 제공합니다.
 * 1. [MSC] USB 메모리에 센서 데이터 로깅 (Start/Stop)
 * 2. [CDC] PC 터미널(TeraTerm 등)로 디버그 메시지 전송
 * * @note    USB 케이블이 연결되어 있어야 동작하며, 
 * 데이터 로깅의 경우 USB 메모리(Flash Drive)가 인식되어야 합니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_USB_H_
#define XM_API_XM_API_USB_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 로거의 현재 상태
 */
typedef enum {
    XM_LOG_STATUS_IDLE,      // 중지됨 (초기 상태)
    XM_LOG_STATUS_LOGGING,   // 정상 로깅 중
    XM_LOG_STATUS_WARNING_QUEUE_FULL, // 큐가 90% 참 (f_write 멈춤 발생 중)
    XM_LOG_STATUS_ERROR_STOPPED,    // 큐 오버플로우로 로깅이 강제 중지됨
} XmLogStatus_e;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ==========================================================================
 * 1. DATA SOURCE REGISTRATION (데이터 등록)
 * ========================================================================== */

/**
 * @brief  [MSC] USB 메모리에 저장할 데이터 소스를 등록합니다.
 * @param  data_ptr : 저장할 구조체의 주소 (&myData)
 * @param  size     : 구조체의 크기 (sizeof(myData))
 */
void XM_SetUsbLogSource(void* data_ptr, uint32_t size);

/**
 * @brief  [CDC] PC로 실시간 스트리밍할 데이터 소스를 등록합니다.
 * @param  data_ptr : 전송할 구조체의 주소 (&myData)
 * @param  size     : 구조체의 크기 (sizeof(myData))
 */
void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);

/**
 * ============================================================================
 * USB 데이터 로깅 API (USB Data Logging API) - Host/MSC Mode
 * ============================================================================
 * @brief UserTask(2ms)의 실시간성을 보장하면서 대용량 데이터를 USB에 저장합니다.
 * @details
 * 이 API는 3단계 비동기 큐 방식으로 동작합니다:
 * 1. [Prio 53, 2ms] UserTask: LogUsbData()를 호출하여 struct를 1단계 큐에 넣습니다. (Non-Blocking)
 * 2. [Prio 24, 100ms] DataLoggerTask: 1단계 큐에서 struct를 꺼내 Binary로 변환(sprintf 아님), 2단계 큐에 넣습니다.
 * 3. [Prio 16, 저순위] UsbMscSaveTask: 2단계 큐에서 데이터를 꺼내 실제 f_write()를 수행합니다.
 */

/**
 * @brief [실시간] USB 저장 장치(MSC)가 연결되고 로깅이 준비되었는지 확인합니다.
 * @details
 * 이 함수는 2ms 실시간 루프에서 안전하게 호출할 수 있습니다.
 * 내부적으로 System Layer의 usb_mode_handler가 관리하는 상태 플래그를 읽습니다.
 * @return USB 저장 장치가 준비되었으면 true, 아니면 false.
 */
bool XM_IsUsbLogReady(void);

/**
 * @brief [비실시간] USB 데이터 로깅 세션을 시작합니다.
 * @warning
 * 이 함수는 저순위 로깅 태스크(DataLoggerTask)에게 명령을 전송하며,
 * 큐가 꽉 찼을 경우 최대 100ms까지 **블로킹(Blocking)될 수 있습니다.**
 * **절대 2ms 실시간 루프(UserTask) 안에서 호출하지 마십시오.**
 * (예: `EnterActive` 같은 상태 진입 함수에서 1회만 호출)
 * @details
 * 저순위 태스크가 "/LOGS/[sessionName]" 폴더를 생성하고,
 * "metadata.txt" 파일을 생성한 뒤, "data_000.bin" 파일 쓰기를 준비합니다.
 *
 * @param[in] sessionName 저장할 세션(폴더)의 이름입니다. (예: "S_001_TestRun")
 * @param[in] metadata    저장될 Binary 데이터를 설명하는 메타데이터 문자열입니다.
 * (예: "Offset 0: uint32_t timestamp...")
 * 이 내용은 'metadata.txt' 파일에 저장됩니다.
 * @return 명령 큐 전송에 성공하면 true, 큐가 꽉 찼거나 USB가 준비되지 않았으면 false.
 */
bool XM_StartUsbDataLog(const char* sessionName, const char* metadata);

/**
 * @brief [비실시간] USB 데이터 로깅 세션을 중지합니다.
 * @warning
 * 이 함수는 **2ms 실시간 루프(UserTask) 안에서 호출하지 마십시오.**
 * @details
 * 저순위 로깅 태스크에게 현재 열린 로그 파일을 닫고 로깅을 종료하라는
 * 명령을 비동기적으로 전송합니다.
 */
void XM_StopUsbDataLog(void);

/**
 * @brief [실시간] 2ms 제어 루프에서 로그 데이터를 링 버퍼에 씁니다.
 * @details 이 함수는 비차단(Non-Blocking)이며, 링 버퍼에 memcpy 후
 * 원자적(atomic) 포인터 연산을 수행합니다.
 * @param[in] logPacket  저장할 사용자 정의 struct의 포인터 (예: &myLogData)
 * @param[in] packetSize 전송할 struct의 크기 (예: sizeof(myLogData))
 * @return 쓰기 성공 시 true. (버퍼가 꽉 차서 로깅이 중단되면 false)
 */
// bool LogUsbData(const void* logPacket, uint32_t packetSize);

/**
 * @brief [실시간] 현재 로거의 상태를 확인합니다.
 * @details UserTask가 이 함수를 호출하여 로깅이 강제 중단되었는지(ERROR_STOPPED)
 * 또는 버퍼가 꽉 차고 있는지(WARNING_BUFFER_HIGH) 확인할 수 있습니다.
 * @return XmLogStatus_e 열거형 값
 */
XmLogStatus_e XM_GetUsbLogStatus(void);


/* ============================================================================
 * USB 모니터링 API (USB Monitoring API) - Device/CDC Mode
 * ==========================================================================*/

/**
 * @brief PC가 USB 가상 시리얼 포트(CDC)에 연결되었는지 확인합니다.
 * @return PC와 연결되었으면 true, 아니면 false.
 */
bool XM_IsUsbStreamConnected(void);

/**
 * @brief PC로부터 '모니터링 시작' 명령(Scenario 1)을 받았는지 확인합니다.
 * @details PC 터미널에서 "AGRB_MON_START"를 보내면 true가 됩니다.
 * @return true: 모니터링 활성 (데이터 전송 시작하세요), false: 대기
 */
bool XM_IsUsbStreamingActive(void);

/**
 * @brief [실시간] USB CDC로 구조체 데이터를 전송합니다.
 * @details 2ms 주기 내에서 안전하게 호출 가능 (Non-blocking)
 */
bool XM_SendUsbData(const void* data, uint32_t len);

/**
 * @brief PC로 메시지를 전송합니다. (비차단)
 * @details 이 함수는 메시지를 즉시 전송하지 않고, 내부의 비동기 큐에 저장합니다.
 * 메인 제어 루프(2ms)에서 printf처럼 안전하게 호출할 수 있습니다.
 *
 * @param[in] message 전송할 문자열. (자동으로 "\r\n"이 추가됩니다)
 * @return 큐에 메시지가 성공적으로 추가되면 true.
 */
bool XM_SendUsbDebugMessage(const char* message);

// 데이터 수신
uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);


/* ==========================================================================
 * System Interface (Called by Core Process)
 * ========================================================================== */

void XM_USB_ProcessPeriodic(void);

#endif /* XM_API_XM_API_USB_H_ */
