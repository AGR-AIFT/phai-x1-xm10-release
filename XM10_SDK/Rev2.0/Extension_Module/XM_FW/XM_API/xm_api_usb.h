/**
 ******************************************************************************
 * @file    xm_api_usb.h
 * @author  HyundoKim
 * @brief   XM10 USB-CDC 실시간 데이터 스트리밍 통신 API (PhAI V2)
 * @details
 * PhAI 프로토콜 패킷으로 래핑된 실시간 데이터를 USB-CDC로 스트리밍합니다.
 * 자동 전송되는 Total Data(0x20)는 PhAI Studio 로 볼 수 있습니다.
 * PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선
 * xm10 도구로 보고 저장하세요.
 *   xm10 PC 도구 (docs/getting-started/04-pc-data-tool.md)
 * PC 프로그램은 한 번에 하나만 연결하세요.
 *
 * @note    USB 케이블이 연결되어 있어야 동작합니다.
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
#include "phai_packet_builder.h"  /* 내부 Module ID 정의 — 사용자 데이터는 0xF0~0xFE 를 쓰세요 */

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

/* USB-MSC 파일 로깅 API는 v2.5.0에서 제거되었습니다. 데이터 캡처는 USB-CDC 스트리밍
 * (XM_SetUsbCustomMeta + XM_SendUsbDataWithId -> PC 의 xm10 도구)을 사용하세요.
 * 단, XmLogStatus_e / XM_GetUsbLogStatus() 는 XM10 내부의 USB LED 상태 표시가
 * 계속 사용하므로 예외적으로 유지합니다 (사용자 API 아님). */

/**
 * @brief 로거의 현재 상태
 * @note  사용자 API 가 아닙니다 — XM10 내부 USB LED 상태 표시 전용으로 유지됩니다.
 */
typedef enum {
    XM_LOG_STATUS_IDLE,              /**< 중지됨 (초기 상태) */
    XM_LOG_STATUS_LOGGING,           /**< 정상 로깅 중 */
    XM_LOG_STATUS_WARNING_QUEUE_FULL,/**< 버퍼 사용률 높음 (f_write 지연 발생 중) */
    XM_LOG_STATUS_WARNING_DISK_LOW,  /**< USB 디스크 잔여 용량 50MB 미만 */
    XM_LOG_STATUS_ERROR_STOPPED,     /**< 에러로 로깅이 강제 중지됨 */
} XmLogStatus_e;

/**
 * @brief USB-CDC 호스트 프로파일 (사용자 선택 — 2가지).
 * @details 한 보드/한 케이블에 붙는 PC 앱의 종류를 사용자가 명시합니다. 기본은
 *          PHAI_STUDIO 이므로 미설정 시 현재 동작(실시간 스트리밍)이 그대로 유지됩니다.
 */
typedef enum {
    XM_USB_HOST_PHAI_STUDIO = 0,  /**< 기본: PhAI Studio 실시간 스트리밍 */
    XM_USB_HOST_TERMINAL    = 1,  /**< 일반 시리얼 터미널용 — Total Data 자동 전송 OFF (직접 보낸 데이터 패킷은 그대로 전송) */
} XM_USB_HostProfile_e;

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
 * @brief [실시간] 현재 로거의 상태를 확인합니다.
 * @note  사용자 API 가 아닙니다 — XM10 내부 USB LED 상태 표시 전용으로 유지됩니다.
 * @return XmLogStatus_e 열거형 값
 */
XmLogStatus_e XM_GetUsbLogStatus(void);

/* ============================================================================
 * USB 모니터링 API (USB Monitoring API) - Device/CDC Mode
 * ============================================================================
 * PhAI V2 프로토콜: User payload를 SOF + LEN + SEQ_ID + MODULE_ID + CRC16으로 자동 래핑.
 * 
 * [사용법]
 *   아래 "User Custom Data API" 블록의 방법(XM_SetUsbCustomMeta + XM_SendUsbDataWithId)을 쓰세요.
 *   XM_SetUsbStreamSource / XM_SetUsbStreamModuleId / XM_SendUsbData 는 구 방식(deprecated)이라
 *   권장하지 않습니다.
 *
 * [Auto-Stream]
 *   기본 ON — PC 프로그램이 포트를 열면 자동 스트리밍 시작 (케이블을 꽂는 것만으로는 시작되지 않음).
 *   XM_SetUsbAutoStream(false) 호출 시 "AGRB MON START" 대기 모드 (Legacy).
 * ==========================================================================*/

/**
 * @brief USB 가상 시리얼 포트(CDC)가 사용 가능한 상태인지 확인합니다.
 * @details USB 장치가 준비되면(케이블 연결 후) PC 프로그램이 포트를 열기 전에도 true 입니다.
 *          그래서 전송 함수(XM_SendUsbDataWithId() 등)가 true 를 반환해도
 *          받는 프로그램이 없을 수 있습니다.
 * @return USB 장치가 준비되었으면 true, 아니면 false.
 */
bool XM_IsUsbStreamConnected(void);

/**
 * @brief 스트리밍이 활성화되었는지 확인합니다.
 * @details Auto-Stream 모드에서는 PC 프로그램이 포트를 열면 자동으로 true.
 *          Legacy 모드에서는 "AGRB MON START" 수신 시 true.
 * @return true: 스트리밍 활성 (데이터 전송 중), false: 대기
 */
bool XM_IsUsbStreamingActive(void);

/**
 * @brief Auto-Stream 모드를 설정합니다.
 * @param[in] enabled  true: PC 프로그램이 포트를 열면 자동 스트리밍 (기본값)
 *                     false: "AGRB MON START" 명령 대기 (Legacy Python 호환)
 */
void XM_SetUsbAutoStream(bool enabled);

/**
 * @brief USB-CDC 호스트 프로파일을 설정합니다. (예제 Control_Setup 에서 1회)
 * @details 미호출 시 기본 PHAI_STUDIO — PC 프로그램이 포트를 열면 1kHz Total Data 가
 *          자동 전송됩니다 (XM_SetUsbCustomMeta() 로 등록한 채널 이름도 이 프로파일에서만
 *          전송됩니다).
 *          일반 시리얼 터미널(Tera Term · VS Code Serial Monitor 등)로 깨끗한 텍스트만 보려면
 *          setup 에서 XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL) 을 1회 호출하세요.
 *          1kHz Total Data 자동 전송과 채널 이름 전송이 꺼지고, 포트를 닫았다 다시 열어도
 *          유지됩니다.
 *          다만 TERMINAL 이어도 XM_SendUsbDataWithId() 로 직접 보낸 데이터 패킷은 그대로
 *          전송되어 터미널에 깨진 글자로 섞여 보입니다. 텍스트만 볼 때는
 *          XM_SendUsbDataWithId() 를 쓰지 마세요.
 *          xm10 도구처럼 데이터 패킷을 해석하는 프로그램을 쓸 때는 기본값(PHAI_STUDIO)을
 *          그대로 두세요.
 * @param[in] profile XM_USB_HOST_PHAI_STUDIO(기본) 또는 XM_USB_HOST_TERMINAL.
 */
void XM_USB_SetHostProfile(XM_USB_HostProfile_e profile);

/**
 * @brief [실시간] USB CDC로 데이터를 PhAI 패킷으로 래핑하여 전송합니다.
 * @deprecated XM_SendUsbDataWithId()로 대체됨. Module ID를 명시적으로 지정하세요.
 * @details 1ms 주기 내에서 안전하게 호출 가능 (Non-blocking).
 *          내부적으로 SOF(0xAA) + LEN + SEQ_ID + MODULE_ID + CRC16을 자동 생성합니다.
 * @param[in] data  전송할 User 구조체 포인터 (float 배열 또는 4-byte 정렬 struct)
 * @param[in] len   데이터 바이트 수
 * @return true: 전송 버퍼에 적재됨 (받는 PC 프로그램이 없어도 true 일 수 있음),
 *         false: 버퍼 풀 또는 USB 장치 미준비
 */
bool XM_SendUsbData(const void* data, uint32_t len);

/**
 * @brief 스트리밍 데이터의 Module ID를 설정합니다.
 * @deprecated XM_SendUsbDataWithId()로 대체됨. Module ID를 전송 시 직접 지정.
 * @param[in] module_id  Module ID (기본값 0x10 = COMBINED)
 */
void XM_SetUsbStreamModuleId(uint8_t module_id);

/* ==========================================================================
 * User Custom Data API (신규 — Total Data와 공존)
 * ==========================================================================
 * Total Data(0x20)는 XM10 이 자동 전송합니다. 사용자가 알고리즘 디버그
 * 데이터를 추가로 전송하고 싶을 때 아래 API를 사용합니다.
 *
 * [사용법]
 *   1. Control_Setup()에서 메타데이터 등록 (이름 개수 = 보내는 float 개수):
 *      XM_SetUsbCustomMeta(0xF0,
 *          "[{\"name\":\"Target\",\"unit\":\"deg\"},"
 *          "{\"name\":\"Current\",\"unit\":\"deg\"},"
 *          "{\"name\":\"Error\",\"unit\":\"deg\"},"
 *          "{\"name\":\"Torque\",\"unit\":\"Nm\"}]");
 *   2. Control_Loop()에서 데이터 전송:
 *      float data[4] = { target, current, error, torque };
 *      XM_SendUsbDataWithId(data, sizeof(data), 0xF0);
 * ========================================================================== */

/**
 * @brief User Custom 채널 메타데이터를 등록합니다.
 * @details PC 프로그램이 포트를 열어 스트리밍이 시작되면 Module ID 0xEF로 한 번 자동 전송됩니다
 *          (기본 프로파일에서만). 채널 이름이 안 보이면 USB 케이블을 다시 꽂은 뒤 연결하세요.
 *          Control_Setup()에서 1회 호출하면 됩니다.
 * @param[in] module_id  대상 Module ID (0xF0~0xFE)
 * @param[in] json_str   채널 정의 JSON string (NULL-terminated, 문자열 리터럴 권장)
 * @note json_str 포인터는 프로그램 수명 동안 유효해야 합니다 (복사하지 않음).
 * @note **하나의 Module ID 메타만 유지됩니다** (단일 슬롯 — 마지막 호출이 이전 것을
 *       덮어씀). 여러 채널 그룹을 라벨링하려면 채널을 하나의 Module ID 로 모아 등록하세요.
 * @note json_str 은 512바이트까지만 전송됩니다 (더 길면 아무 알림 없이 전송되지 않습니다).
 *       채널이 많으면 단위를 이름 안에 적어(예: "IMU0 Roll [deg]") 짧게 유지하세요.
 */
void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);

/**
 * @brief User Custom float[] 데이터를 지정된 Module ID로 전송합니다.
 * @param[in] data       float 배열 포인터 (4-byte aligned)
 * @param[in] len        바이트 수 (sizeof(float) × 채널수)
 * @param[in] module_id  Module ID (0xF0~0xFE)
 * @return true: 전송 버퍼에 적재됨 (받는 PC 프로그램이 없어도 true 일 수 있음),
 *         false: 버퍼 풀 또는 USB 장치 미준비
 * @note Control_Loop() 내에서 호출. Non-blocking.
 */
bool XM_SendUsbDataWithId(const void* data, uint32_t len, uint8_t module_id);

/**
 * @brief PC로 디버그 메시지를 전송합니다. (비차단, Raw 텍스트)
 * @details PhAI 패킷이 아닌 raw 텍스트로 전송됩니다 (패킷 구분 없음). PC 도구(PhAI Studio,
 *          xm10 도구)가 데이터 패킷을 해석하는 중에 보내면 메시지 1개마다 데이터 패킷 1개를
 *          잃습니다.
 *          텍스트만 쓰려면 Rev2.0 에서 XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL) 을 호출하고
 *          일반 시리얼 터미널로 연결하세요.
 * @param[in] message 전송할 문자열.
 * @return 전송 버퍼에 적재되면 true (받는 PC 프로그램이 없어도 true 일 수 있음).
 */
bool XM_SendUsbDebugMessage(const char* message);

/**
 * @brief PC로부터 데이터를 수신합니다. (Non-Blocking)
 * @warning 수신 버퍼는 시스템도 매 1ms tick(호스트 프로파일과 무관)마다, Control_Loop() 가
 *          끝난 뒤 같은 tick 안에서 끝까지 읽어 비웁니다. Control_Loop() 안에서 이 함수를
 *          호출해 그 tick 에 버퍼를 다 읽지 않으면, 남은 바이트는 시스템이 가져가 버려
 *          두 번 다시 받을 수 없습니다.
 *          PC 에서 보낸 raw 데이터를 받으려면 이 제약을 전제로 설계하세요.
 */
uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);


/* ==========================================================================
 * System Interface (XM10 내부용 — 사용자는 호출하지 않음)
 * ========================================================================== */

void XM_USB_ProcessPeriodic(void);

#endif /* XM_API_XM_API_USB_H_ */
