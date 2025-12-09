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

// PC에서 보낼 명령어 정의 (터미널에서 입력하기 쉬운 문자열)
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

// 현재 모니터링(Scenario 1) 모드가 활성화되었는지 확인
bool CdcStream_IsStreamingActive(void);

/**
 * @brief [IOIF->System] Tx 완료 콜백. (ISR 컨텍스트에서 호출됨)
 */
void CdcStream_OnTxComplete(void);

/**
 * @brief [IOIF->System] Rx 완료 콜백. (ISR 컨텍스트에서 호출됨)
 */
void CdcStream_OnRxReceived(uint8_t* data, uint32_t len);

#endif /* SYSTEM_COMM_USB_CDC_HANDLER_H_ */
