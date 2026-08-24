/**
 ******************************************************************************
 * @file    cdc_handler.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 13, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_USB_CDC_HANDLER_H_
#define SYSTEM_COMM_USB_CDC_HANDLER_H_

#include "ioif_agrb_defs.h"
#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */
// 2ms마다 300바이트가 쏟아져도 100ms를 버티는 크기 (300B*50 = 15KB, 16kB)
// 버퍼 사이즈 설정 (D3 RAM 여유분에 따라 조절)
// CDC 전송용 링버퍼 크기 (넉넉하게 16KB ~ 32KB 추천)
#define CDC_TX_RING_BUFFER_SIZE (32 * 1024) // 32kB Tx Ring Buffer
#define CDC_RX_RING_BUFFER_SIZE (4 * 1024)  // 4kB Rx Ring Buffer

// PC에서 보낼 명령어 정의 [Deprecated — PhAI V2에서는 Auto-Stream이 기본]
#define CDC_CMD_STREAMING_START   "AGRB MON START"
#define CDC_CMD_STREAMING_STOP    "AGRB MON STOP"

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief CDC 스트림 핸들러를 초기화합니다. (Tx/Rx 링버퍼/큐 생성)
 */
void CdcStream_Init(void);

/**
 * @brief [Facade -> System] 구조체 데이터 전송 (Non-blocking, Lock-Free)
 * @param[in] data  저장할 사용자 정의 struct 포인터 (void*)
 * @param[in] len   data Packet의 크기 (예: sizeof(MyUSBStreamData_t))
 * @return 링버퍼에 추가 성공 시 true, 꽉 찼으면 false
 */
bool CdcStream_Send(const void* data, uint32_t len);

/**
 * @brief [Facade용] Rx. (Non-Blocking)
 * @param[out] buf      데이터를 저장할 버퍼
 * @param[in]  max_len  버퍼의 최대 크기
 * @return 
 */
uint32_t CdcStream_Read(void* buf, uint32_t max_len);

/**
 * @brief 스트리밍이 활성화되었는지 확인
 * @details Auto-Stream 모드에서는 USB CDC 연결 시 자동 true.
 *          Legacy 모드에서는 "AGRB MON START" 수신 시 true.
 */
bool CdcStream_IsStreamingActive(void);

/**
 * @brief Auto-Stream 모드를 설정합니다.
 * @param[in] enabled  true: USB 연결 시 자동 스트리밍 (PhAI Studio 기본)
 *                     false: "AGRB MON START" 대기 (Legacy Python 호환)
 * @note 기본값은 true (PhAI Studio 기본 동작)
 */
void CdcStream_SetAutoStreamEnabled(bool enabled);

/**
 * @brief USB CDC 연결 상태 변경 시 호출 (usb_mode_handler에서 호출)
 * @details Auto-Stream 모드에서 연결/해제 시 스트리밍을 자동 제어합니다.
 */
void CdcStream_OnConnectionChanged(bool connected);

/**
 * @brief Host DTR 상태 변경 시 호출 (CDC_SET_CONTROL_LINE_STATE 콜백에서 호출)
 * @param[in] dtr  1: Host가 COM 포트 열음, 0: Host가 COM 포트 닫음
 * @details Host가 COM 포트를 닫을 때(dtr=0) Tx 상태를 리셋하여
 *          다음 연결 시 정상 동작하도록 합니다.
 */
void CdcStream_OnHostDtrChanged(uint8_t dtr);

/**
 * @brief 호스트 disconnect 감지 (TX drop 기반 fallback)
 * @details 100ms 주기로 호출. 연속 N 사이클 drop 증가 시 streaming 자동 해제.
 *          DTR=0을 보내지 않는 호스트(WebSerial 등) 대응용.
 *          DTR=0이 정상 동작하는 환경에서는 이 함수보다 DTR이 먼저 처리됨.
 * @note pnp_task.c에서 _UpdateUsbLed() 전에 호출.
 */
void CdcStream_CheckForDisconnect(void);

/**
 * @brief Tx 실패(드롭) 누적 횟수를 반환합니다.
 * @return 링버퍼 풀로 인해 드롭된 패킷 수
 */
uint32_t CdcStream_GetTxDropCount(void);

/**
 * @brief Tx 드롭 카운터를 0으로 리셋합니다.
 */
void CdcStream_ResetTxDropCount(void);

/**
 * @brief [IOIF->System] Tx 완료 콜백. (ISR 컨텍스트에서 호출됨)
 */
void CdcStream_OnTxComplete(void);

/**
 * @brief [IOIF->System] Rx 완료 콜백. (ISR 컨텍스트에서 호출됨)
 */
void CdcStream_OnRxReceived(uint8_t* data, uint32_t len);

#endif /* SYSTEM_COMM_USB_CDC_HANDLER_H_ */
