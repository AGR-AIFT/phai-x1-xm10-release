/**
 ******************************************************************************
 * @file    xm_api_usb.c
 * @author  HyundoKim
 * @brief   USB 기능 통합 관리 구현부
 * @details 
 * 1. MSC (로깅): 사용자가 등록한 구조체를 Binary 형태로 파일 저장
 * 2. CDC (디버그): PC 터미널과의 양방향 통신 지원
 * 이 모든 기능은 core_process에 의해 2ms 주기로 자동 처리됩니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api_usb.h"
#include "xm_api_data.h"  // Default Source (XM)
// System Layer 내부 모듈 포함
#include "data_logger.h"  // DataLogger_Start, DataLogger_Stop
#include "cdc_handler.h"  // CdcStream_Send, CdcStream_SetState
#include "usb_mode_handler.h"
#include <stddef.h>
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

// MSC (Logging) Source
static void* s_log_src_ptr = NULL;  // 기본값: NULL
static uint32_t s_log_src_size = 0;

// CDC (Streaming) Source
static void* s_stream_src_ptr = NULL;  // 기본값: NULL
static uint32_t s_stream_src_size = 0;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/* ==========================================================================
 * Registration Functions
 * ========================================================================== */

void XM_SetUsbLogSource(void* data_ptr, uint32_t size)
{
    if (data_ptr != NULL && size > 0) {
        s_log_src_ptr = data_ptr;
        s_log_src_size = size;
    }
}

void XM_SetUsbStreamSource(void* data_ptr, uint32_t size)
{
    if (data_ptr != NULL && size > 0) {
        s_stream_src_ptr = data_ptr;
        s_stream_src_size = size;
    }
}

/* ==========================================================================
 * Data Logger Implementation
 * ========================================================================== */

/**
 * @brief USB 로깅 준비 상태를 확인합니다.
 */
bool XM_IsUsbLogReady(void)
{
    // Facade 패턴: 실제 구현은 DataLogger 모듈에 위임
    return DataLogger_IsReady(); // (g_isUsbHostReady & fs_ready) 확인
}

/**
 * @brief 로깅 시작 명령을 System Layer(DataLogger)로 전달합니다.
 */
bool XM_StartUsbDataLog(const char* sessionName, const char* metadata)
{
    if (!XM_IsUsbLogReady()) return false;
    // Facade 패턴: 실제 구현은 DataLogger 모듈에 위임합니다.
    // 이 함수가 내부 상태를 LOG_STATUS_LOGGING로 변경함
    return DataLogger_Start(sessionName, metadata);
}

/**
 * @brief 로깅 중지 명령을 System Layer(DataLogger)로 전달합니다.
 */
void XM_StopUsbDataLog(void)
{
    if (XM_IsUsbLogReady()) DataLogger_Stop();
}

// /**
//  * @brief [실시간] 2ms 루프의 로그 데이터를 System Layer(DataLogger)로 전달합니다.
//  */
// bool LogUsbData(const void* logPacket, uint32_t packetSize)
// {
//     if (!IsUsbLogReady()) return false;
//     // Facade 패턴: 실제 구현은 DataLogger 모듈에 위임합니다.
//     // [핵심] 이 함수는 링 버퍼에 memcpy 후 즉시 반환됩니다. (Non-Blocking)
//     return DataLogger_Log(logPacket, packetSize);
// }

/**
 * @brief [실시간] 로거의 현재 상태를 가져옵니다.
 */
XmLogStatus_e XM_GetUsbLogStatus(void)
{
	if (!XM_IsUsbLogReady()) return XM_LOG_STATUS_IDLE;
    // [실시간] System Layer의 상태 함수 호출
    // (DataLogger_Status_e와 XmLogStatus_e는 값이 동일하게 정의되어야 함)
    return (DataLogger_Status_e)DataLogger_GetStatus();
}

/* ============================================================================
 * USB 스트리밍 API (USB Streaming API) - Device/CDC Mode
 * ==========================================================================*/

bool XM_IsUsbStreamConnected(void)
{
    return g_isUsbDeviceReady;
}

bool XM_IsUsbStreamingActive(void)
{
    // Facade: System Layer에 위임
    return CdcStream_IsStreamingActive();
}

// 구조체 전송 API
bool XM_SendUsbData(const void* data, uint32_t len)
{
    if (!XM_IsUsbStreamConnected()) return false;
    // 비동기 링버퍼에 밀어넣기 (Non-Blocking, Lock-free)
    return CdcStream_Send(data, len);
}

// 문자열 전송도 지원 (내부적으로 CdcStream_Send 호출)
bool XM_SendUsbDebugMessage(const char* message)
{
    if (!XM_IsUsbStreamConnected()) return false;
    return CdcStream_Send(message, strlen(message));
}

// 데이터 수신
uint32_t XM_GetUsbData(void* buffer, uint32_t max_len)
{
    if (!XM_IsUsbStreamConnected()) return 0;
    return CdcStream_Read(buffer, max_len);
}

/* ==========================================================================
 * System Interface (Called by Core Process)
 * ========================================================================== */

void XM_USB_ProcessPeriodic(void)
{
    /* 1. MSC Data Logging */
	DataLogger_Status_e log_status = DataLogger_GetStatus();
	// 정상 로깅 중이거나(LOGGING), 큐가 조금 찼더라도(WARNING) 데이터 저장을 계속 시도.
    if (log_status == LOG_STATUS_LOGGING || log_status == LOG_STATUS_WARNING_QUEUE_FULL) {
        // [수정] 데이터 소스가 유효한 경우에만 저장 수행
        if (s_log_src_ptr != NULL && s_log_src_size > 0) {
            DataLogger_Log(s_log_src_ptr, s_log_src_size);
        }
    }

    /* 2. CDC Data Streaming */
    // 스트리밍이 켜져 있고, 소스가 등록되어 있고, 연결되어 있다면 전송
    bool is_hw_ready = XM_IsUsbStreamConnected();
    bool is_logic_active = XM_IsUsbStreamingActive();
    if (is_hw_ready && is_logic_active) {
        // CDC 버퍼 오버플로우 방지를 위해 전송 가능 여부 체크(옵션) 후 전송
        // 데이터 소스가 유효한 경우에만 전송 수행
        if (s_stream_src_ptr != NULL && s_stream_src_size > 0) {
            CdcStream_Send((uint8_t*)s_stream_src_ptr, s_stream_src_size);
        }
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */
