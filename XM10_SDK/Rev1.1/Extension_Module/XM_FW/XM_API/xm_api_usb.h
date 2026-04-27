/**
 ******************************************************************************
 * @file    xm_api_usb.h
 * @author  HyundoKim
 * @brief   XM10 USB 데이터 로깅 및 스트리밍 통신 API (PhAI V2)
 * @details 
 * 이 모듈은 두 가지 핵심 기능을 제공합니다.
 * 1. [MSC] USB 메모리에 센서 데이터 로깅 (Start/Stop)
 * 2. [CDC] PhAI 프로토콜 패킷으로 래핑된 실시간 데이터 스트리밍
 *
 * @note    USB 케이블이 연결되어 있어야 동작하며, 
 * 데이터 로깅의 경우 USB 메모리(Flash Drive)가 인식되어야 합니다.
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
 * @brief 로거의 현재 상태
 */
typedef enum {
    XM_LOG_STATUS_IDLE,              /**< 중지됨 (초기 상태) */
    XM_LOG_STATUS_LOGGING,           /**< 정상 로깅 중 */
    XM_LOG_STATUS_WARNING_QUEUE_FULL,/**< 버퍼 사용률 높음 (f_write 지연 발생 중) */
    XM_LOG_STATUS_WARNING_DISK_LOW,  /**< USB 디스크 잔여 용량 50MB 미만 */
    XM_LOG_STATUS_ERROR_STOPPED,     /**< 에러로 로깅이 강제 중지됨 */
} XmLogStatus_e;

/**
 * @brief 로깅 세션 실시간 통계 (사용자 조회용)
 * @details XM_GetUsbLogStats()로 현재 세션의 통계를 조회할 수 있습니다.
 */
typedef struct {
    uint32_t total_bytes;          /**< 총 기록 바이트 수 */
    uint32_t total_records;        /**< 총 레코드 수 */
    uint32_t dropped_records;      /**< 누락된 레코드 수 (버퍼 오버플로) */
    uint32_t write_errors;         /**< 쓰기 실패 횟수 */
    uint32_t duration_ms;          /**< 세션 경과 시간 (ms) */
    uint8_t  hot_buffer_percent;   /**< Hot Buffer 피크 사용률 (0~100) */
    uint8_t  cold_buffer_percent;  /**< Cold Buffer 피크 사용률 (0~100) */
    uint32_t disk_free_mb;         /**< USB 잔여 용량 (MB) */
    uint32_t disk_total_mb;        /**< USB 전체 용량 (MB) */
} XmLogStats_t;

/**
 * @brief 이벤트 마커 타입
 * @details 로깅 중 특정 시점(모드 전환, 이상 감지, 수동 마킹)을 표시하여
 *          후처리 시 시간축에서 해당 이벤트를 빠르게 찾을 수 있습니다.
 */
typedef enum {
    XM_LOG_MARKER_USER   = 0x01,  /**< 수동 마킹 (버튼/명령) */
    XM_LOG_MARKER_MODE   = 0x02,  /**< 모드 전환 */
    XM_LOG_MARKER_ERROR  = 0x03,  /**< 에러 발생 */
    XM_LOG_MARKER_SYNC   = 0x04,  /**< 시간 동기점 */
} XmLogMarkerType_e;

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
 * @deprecated Total Data(0x20) 자동 전송으로 대체됨. 추가 채널은
 *             XM_SetUsbCustomMeta() + XM_SendUsbDataWithId() 사용.
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
 *                        NULL 또는 빈 문자열 전달 시 "S_001", "S_002", ... 자동 생성.
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

/**
 * @brief 자동 타임스탬프(4-byte tick_ms)를 활성화/비활성화합니다.
 * @param[in] enabled  true: 매 패킷 앞에 tick 자동 삽입 (기본값: true)
 *                     false: User 구조체에 이미 tick 포함 시 비활성화
 * @note User_Setup()에서 XM_StartUsbDataLog() 호출 전에 설정하세요.
 */
void XM_SetUsbLogAutoTimestamp(bool enabled);

/**
 * @brief 파일 롤링 크기를 설정합니다.
 * @param[in] size_mb  파일 분할 크기 (MB). 1~100, 기본값: 10.
 * @note User_Setup()에서 XM_StartUsbDataLog() 호출 전에 설정하세요.
 */
void XM_SetUsbLogRollingSize(uint32_t size_mb);

/* --------------------------------------------------------------------------
 * 상태 모니터링 (Status & Monitoring)
 * -------------------------------------------------------------------------- */

/**
 * @brief [실시간] 로깅 세션의 실시간 통계를 조회합니다.
 * @details 로깅 중 또는 로깅 종료 후 세션의 상세 통계를 확인할 수 있습니다.
 *          Hot/Cold 버퍼 피크 사용률, 디스크 잔여 용량 등 진단 정보를 포함합니다.
 * @param[out] out_stats 통계가 복사될 구조체 포인터
 * @return true: 성공, false: 파라미터 오류 또는 USB 미연결
 *
 * @code
 * XmLogStats_t stats;
 * if (XM_GetUsbLogStats(&stats)) {
 *     printf("Records: %lu, Dropped: %lu, Disk: %lu MB\n",
 *            stats.total_records, stats.dropped_records, stats.disk_free_mb);
 * }
 * @endcode
 */
bool XM_GetUsbLogStats(XmLogStats_t* out_stats);

/**
 * @brief [실시간] USB 디스크 잔여 용량(MB)을 반환합니다.
 * @details 10초 주기로 캐시된 값을 즉시 반환합니다 (Non-blocking).
 * @return 잔여 용량 (MB). USB 미연결 시 0.
 */
uint32_t XM_GetUsbDiskFreeMB(void);

/**
 * @brief [실시간] USB 디스크 전체 용량(MB)을 반환합니다.
 * @return 전체 용량 (MB). USB 미연결 시 0.
 */
uint32_t XM_GetUsbDiskTotalMB(void);


/* --------------------------------------------------------------------------
 * 이벤트 마커 (Event Markers)
 * -------------------------------------------------------------------------- */

/**
 * @brief [실시간] 로깅 중 이벤트 마커를 삽입합니다.
 * @details 모드 전환, 에러 발생, 수동 마킹 등 특정 시점을 기록합니다.
 *          마커는 일반 데이터와 동일한 파이프라인으로 저장되며,
 *          Python 디코더가 자동으로 마커를 분리하여 이벤트 로그를 생성합니다.
 * @param[in] type 마커 타입 (XmLogMarkerType_e)
 * @param[in] data 컨텍스트 데이터 (에러 코드, 모드 ID 등. 불필요 시 0)
 * @return true: 성공, false: 로깅 비활성 또는 버퍼 부족
 *
 * @code
 * // 모드 전환 시
 * XM_InsertUsbLogMarker(XM_LOG_MARKER_MODE, newModeId);
 *
 * // 에러 감지 시
 * XM_InsertUsbLogMarker(XM_LOG_MARKER_ERROR, errorCode);
 *
 * // 수동 마킹
 * XM_InsertUsbLogMarker(XM_LOG_MARKER_USER, 0);
 * @endcode
 */
bool XM_InsertUsbLogMarker(XmLogMarkerType_e type, uint16_t data);


/* ============================================================================
 * USB 모니터링 API (USB Monitoring API) - Device/CDC Mode
 * ============================================================================
 * PhAI V2 프로토콜: User payload를 SOF + SEQ_ID + MODULE_ID + CRC8으로 자동 래핑.
 * 
 * [사용법]
 *   1. User_Setup()에서 소스 등록:
 *      XM_SetUsbStreamSource(&myData, sizeof(myData));
 *   2. (선택) Module ID 변경:
 *      XM_SetUsbStreamModuleId(0xF0);
 *   3. User_Loop()에서 직접 전송 또는 자동 전송:
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
 *   1. User_Setup()에서 메타데이터 등록:
 *      XM_SetUsbCustomMeta(0xF0, "[{\"name\":\"Target\",\"unit\":\"deg\"}]");
 *   2. User_Loop()에서 데이터 전송:
 *      float data[4] = { target, current, error, torque };
 *      XM_SendUsbDataWithId(data, sizeof(data), 0xF0);
 * ========================================================================== */

/**
 * @brief User Custom 채널 메타데이터를 등록합니다.
 * @details USB 연결 시 Module ID 0xEF로 자동 전송됩니다.
 *          User_Setup()에서 1회 호출하면 됩니다.
 * @param[in] module_id  대상 Module ID (0xF0~0xFE)
 * @param[in] json_str   채널 정의 JSON string (NULL-terminated, 문자열 리터럴 권장)
 * @note json_str 포인터는 프로그램 수명 동안 유효해야 합니다 (복사하지 않음).
 */
void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);

/**
 * @brief User Custom float[] 데이터를 지정된 Module ID로 전송합니다.
 * @param[in] data       float 배열 포인터 (4-byte aligned)
 * @param[in] len        바이트 수 (sizeof(float) × 채널수)
 * @param[in] module_id  Module ID (0xF0~0xFE)
 * @return true: 전송 성공, false: 버퍼 풀 또는 연결 없음
 * @note User_Loop() 내에서 호출. Non-blocking.
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
 */
uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);


/* ==========================================================================
 * System Interface (Called by Core Process)
 * ========================================================================== */

void XM_USB_ProcessPeriodic(void);

#endif /* XM_API_XM_API_USB_H_ */
