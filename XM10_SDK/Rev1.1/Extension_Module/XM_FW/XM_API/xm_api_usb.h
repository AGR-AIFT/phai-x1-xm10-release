/**
 ******************************************************************************
 * @file    xm_api_usb.h
 * @author  HyundoKim
 * @brief   XM10 USB-CDC 실시간 스트리밍 통신 API (PhAI V2)
 * @details
 * [CDC] PhAI 프로토콜 패킷으로 래핑된 실시간 데이터 스트리밍 (PC / PhAI Studio).
 *
 * @note    USB-MSC 파일 로깅 public API 는 v2.5.0 에서 제거(은닉)되었습니다.
 *          데이터 수집은 USB-CDC 실시간 스트리밍으로 하며, 온보드 저장(SD카드)은
 *          향후 HW 리비전에서 지원 예정입니다.
 * @version 2.0  (PhAI V2 프로토콜 적용)
 * @date    Feb 23, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_USB_H_
#define XM_API_XM_API_USB_H_

#include <stdint.h>
#include <stdbool.h>
#include "phai_packet_builder.h"  /* PHAI_MODULE_* 매크로 (User가 직접 사용) */

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
 * @brief 로거의 현재 상태 (System 내부 상태 조회용)
 */
typedef enum {
    XM_LOG_STATUS_IDLE,               // 중지됨 (초기 상태)
    XM_LOG_STATUS_LOGGING,            // 정상 로깅 중
    XM_LOG_STATUS_WARNING_QUEUE_FULL, // 링 버퍼 90% 이상 (f_write 멈춤 발생 중)
    XM_LOG_STATUS_WARNING_DISK_LOW,   // 디스크 잔여 50MB 미만
    XM_LOG_STATUS_ERROR_STOPPED,      // 오류로 로깅이 강제 중지됨
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
 * @brief  [CDC] PC로 실시간 스트리밍할 데이터 소스를 등록합니다.
 * @deprecated Total Data(0x20) 자동 전송으로 대체됨. 추가 채널은
 *             XM_SetUsbCustomMeta() + XM_SendUsbDataWithId() 사용.
 * @param  data_ptr : 전송할 구조체의 주소 (&myData)
 * @param  size     : 구조체의 크기 (sizeof(myData))
 */
void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);

/**
 * @brief [내부 전용] 로거의 현재 상태를 확인합니다. (USB 상태 LED 등 System 용)
 * @return XmLogStatus_e 열거형 값
 */
XmLogStatus_e XM_GetUsbLogStatus(void);


/* ==========================================================================
 * USB 모니터링 API (USB Monitoring API) - Device/CDC Mode
 * ==========================================================================
 * PhAI V2 프로토콜: User payload를 SOF + SEQ_ID + MODULE_ID + CRC8으로 자동 래핑.
 *
 * [사용법]
 *   1. Control_Setup()에서 소스 등록:
 *      XM_SetUsbStreamSource(&myData, sizeof(myData));
 *   2. (선택) Module ID 변경:
 *      XM_SetUsbStreamModuleId(0xF0);
 *   3. Control_Loop()에서 직접 전송 또는 자동 전송:
 *      XM_SendUsbData(&myData, sizeof(myData));
 *
 * [Auto-Stream]
 *   기본 ON — USB 연결 시 자동 스트리밍 시작 (PhAI Studio 기본 동작).
 *   XM_SetUsbAutoStream(false) 호출 시 "AGRB MON START" 대기 모드 (Legacy).
 * ==========================================================================*/

/**
 * @brief PC가 USB 가상 시리얼 포트(CDC)에 연결되었는지 확인합니다.
 * @return PC와 연결되었으면 true, 아니면 false.
 */
bool XM_IsUsbStreamConnected(void);

/**
 * @brief 스트리밍이 활성화되었는지 확인합니다.
 * @details Auto-Stream 모드에서는 USB 연결 시 자동 true.
 *          Legacy 모드에서는 "AGRB MON START" 수신 시 true.
 * @return true: 스트리밍 활성 (데이터 전송 중), false: 대기
 */
bool XM_IsUsbStreamingActive(void);

/**
 * @brief Auto-Stream 모드를 설정합니다.
 * @param[in] enabled  true: USB 연결 시 자동 스트리밍 (기본값, PhAI Studio)
 *                     false: "AGRB MON START" 명령 대기 (Legacy Python 호환)
 */
void XM_SetUsbAutoStream(bool enabled);

/**
 * @brief [실시간] USB CDC로 데이터를 PhAI 패킷으로 래핑하여 전송합니다.
 * @deprecated XM_SendUsbDataWithId()로 대체됨. Module ID를 명시적으로 지정하세요.
 * @details 1ms 주기 내에서 안전하게 호출 가능 (Non-blocking).
 *          내부적으로 SOF(0xAA) + LEN + SEQ_ID + MODULE_ID + CRC16을 자동 생성합니다.
 * @param[in] data  전송할 User 구조체 포인터 (float 배열 또는 4-byte 정렬 struct)
 * @param[in] len   데이터 바이트 수
 * @return true: 전송 성공, false: 버퍼 풀 또는 연결 안 됨
 */
bool XM_SendUsbData(const void* data, uint32_t len);

/**
 * @brief 스트리밍 데이터의 Module ID를 설정합니다.
 * @deprecated XM_SendUsbDataWithId()로 대체됨. Module ID를 전송 시 직접 지정.
 * @param[in] module_id  Module ID (기본값 0x10 = COMBINED)
 * @see phai_packet_builder.h: PHAI_MODULE_* 정의 참조
 */
void XM_SetUsbStreamModuleId(uint8_t module_id);

/* ==========================================================================
 * User Custom Data API (신규 — Total Data와 공존)
 * ==========================================================================
 * Total Data(0x20)는 System이 자동 전송합니다. 사용자가 알고리즘 디버그
 * 데이터를 추가로 전송하고 싶을 때 아래 API를 사용합니다.
 *
 * [사용법]
 *   1. Control_Setup()에서 메타데이터 등록:
 *      XM_SetUsbCustomMeta(0xF0, "[{\"name\":\"Target\",\"unit\":\"deg\"}]");
 *   2. Control_Loop()에서 데이터 전송:
 *      float data[4] = { target, current, error, torque };
 *      XM_SendUsbDataWithId(data, sizeof(data), 0xF0);
 * ========================================================================== */

/**
 * @brief User Custom 채널 메타데이터를 등록합니다.
 * @details USB 연결 시 Module ID 0xEF로 자동 전송됩니다.
 *          Control_Setup()에서 1회 호출하면 됩니다.
 * @param[in] module_id  대상 Module ID (0xF0~0xFE)
 * @param[in] json_str   채널 정의 JSON string (NULL-terminated, 문자열 리터럴 권장)
 * @note json_str 포인터는 프로그램 수명 동안 유효해야 합니다 (복사하지 않음).
 * @note **USB 연결당 하나의 Module ID 메타만 유지됩니다** (단일 슬롯 — 마지막 호출이 이전 것을
 *       덮어씀). 여러 채널 그룹을 라벨링하려면 채널을 하나의 Module ID 로 모아 등록하세요.
 */
void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);

/**
 * @brief User Custom float[] 데이터를 지정된 Module ID로 전송합니다.
 * @param[in] data       float 배열 포인터 (4-byte aligned)
 * @param[in] len        바이트 수 (sizeof(float) × 채널수)
 * @param[in] module_id  Module ID (0xF0~0xFE)
 * @return true: 전송 성공, false: 버퍼 풀 또는 연결 없음
 * @note Control_Loop() 내에서 호출. Non-blocking.
 */
bool XM_SendUsbDataWithId(const void* data, uint32_t len, uint8_t module_id);

/**
 * @brief PC로 디버그 메시지를 전송합니다. (비차단, Raw 텍스트)
 * @details PhAI 패킷이 아닌 raw 텍스트로 전송됩니다.
 * @param[in] message 전송할 문자열.
 * @return 전송 성공 시 true.
 */
bool XM_SendUsbDebugMessage(const char* message);

/**
 * @brief PC로부터 데이터를 수신합니다. (Non-Blocking)
 * @warning [2nd-consumer 경합] System 내부의 CdcDopRouter_Process()(매 1ms tick,
 *          호스트 프로파일과 무관하게 실행)가 동일한 CDC RX StreamBuffer 를
 *          drain-to-empty 로 소비합니다. Control_Loop() 안에서 이 함수를 호출해
 *          그 tick 에 버퍼를 다 비우지 않으면, 남은 바이트는 같은 tick 뒤쪽에서
 *          라우터가 가져가 DOP 프레임으로 해석을 시도하고 두 번 다시 수신할 수
 *          없습니다. raw RX 수신이 필요한 애플리케이션은 이 제약을 전제로
 *          설계하세요 (라우터 profile 게이트는 후속 검토 항목).
 */
uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);


/* ==========================================================================
 * System Interface (Called by Core Process)
 * ========================================================================== */

void XM_USB_ProcessPeriodic(void);

#endif /* XM_API_XM_API_USB_H_ */
