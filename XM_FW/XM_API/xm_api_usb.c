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
#include "data_logger.h"          // DataLogger_Start, DataLogger_Stop
#include "cdc_handler.h"          // CdcStream_Send, CdcStream_SetState
#include "phai_packet_builder.h"  // PhAI V2 프로토콜 패킷 빌더
#include "usb_mode_handler.h"
#include "xm_total_data.h"       // Total Data Packet (System-Managed)
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
static void* s_stream_src_ptr = NULL;
static uint32_t s_stream_src_size = 0;

// PhAI V2: 스트리밍 Module ID (User가 변경 가능, 기본 COMBINED)
static uint8_t s_stream_module_id = PHAI_MODULE_COMBINED;

// System Total Data (0x20) auto-stream. Enabled by default for dashboard compatibility.
static bool s_total_data_stream_enabled = true;

// User Custom Metadata (Module ID 0xEF, 연결 시 1회 전송)
static const char* s_custom_meta_json = NULL;
static uint8_t     s_custom_meta_target_id = 0;
static bool        s_meta_sent = false;

// 이전 연결 상태 추적 (재연결 감지 → Meta 재전송)
static bool s_prev_connected = false;

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
        DataLogger_SetPacketSize(size);
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
    return (XmLogStatus_e)DataLogger_GetStatus();
}

void XM_SetUsbLogAutoTimestamp(bool enabled)
{
    DataLogger_SetAutoTimestamp(enabled);
}

void XM_SetUsbLogRollingSize(uint32_t size_mb)
{
    DataLogger_SetRollingSize(size_mb);
}

bool XM_GetUsbLogStats(XmLogStats_t* out_stats)
{
    if (out_stats == NULL) return false;

    DataLogger_Stats_t internal;
    DataLogger_GetStats(&internal);

    out_stats->total_bytes     = internal.total_bytes_written;
    out_stats->total_records   = internal.total_packets_logged;
    out_stats->dropped_records = internal.dropped_packets;
    out_stats->write_errors    = internal.write_errors;
    out_stats->hot_buffer_percent = internal.hot_buffer_peak_percent;
    out_stats->disk_free_mb    = internal.disk_free_mb;
    out_stats->disk_total_mb   = internal.disk_total_mb;

    /* duration 계산: 로깅 중이면 현재 tick, 아니면 end_tick 기준 */
    if (internal.start_tick > 0) {
        uint32_t end = (internal.end_tick > 0) ? internal.end_tick : xTaskGetTickCount();
        out_stats->duration_ms = (end - internal.start_tick) * portTICK_PERIOD_MS;
    } else {
        out_stats->duration_ms = 0;
    }

    return (internal.start_tick > 0);
}

bool XM_InsertUsbLogMarker(XmLogMarkerType_e type, uint16_t data)
{
    return DataLogger_InsertMarker((uint8_t)type, data);
}

uint32_t XM_GetUsbDiskFreeMB(void)
{
    return DataLogger_GetDiskFreeMB();
}

uint32_t XM_GetUsbDiskTotalMB(void)
{
    return DataLogger_GetDiskTotalMB();
}

/* ============================================================================
 * USB 스트리밍 API (USB Streaming API) - Device/CDC Mode — PhAI V2
 * ==========================================================================*/

bool XM_IsUsbStreamConnected(void)
{
    return g_isUsbDeviceReady;
}

bool XM_IsUsbStreamingActive(void)
{
    return CdcStream_IsStreamingActive();
}

void XM_SetUsbAutoStream(bool enabled)
{
    CdcStream_SetAutoStreamEnabled(enabled);
}

void XM_SetUsbTotalDataStream(bool enabled)
{
    s_total_data_stream_enabled = enabled;
}

void XM_SetUsbStreamModuleId(uint8_t module_id)
{
    s_stream_module_id = module_id;
}

bool XM_SendUsbData(const void* data, uint32_t len)
{
    if (!XM_IsUsbStreamConnected()) return false;
    return PhAI_PacketBuild(data, len, s_stream_module_id);
}

bool XM_SendUsbDebugMessage(const char* message)
{
    if (!XM_IsUsbStreamConnected()) return false;
    /* 디버그 메시지는 Raw 텍스트 전송 (PhAI 패킷 래핑 없이) */
    return CdcStream_Send(message, strlen(message));
}

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
    /* 0. CDC Rx: Legacy command processing (AGRB MON START/STOP) */
    {
        uint8_t rxTmp[64];
        CdcStream_Read(rxTmp, sizeof(rxTmp));
    }

    /* 1. MSC Data Logging */
    DataLogger_Status_e log_status = DataLogger_GetStatus();
    if (log_status == LOG_STATUS_LOGGING || log_status == LOG_STATUS_WARNING_QUEUE_FULL
        || log_status == LOG_STATUS_WARNING_DISK_LOW) {
        if (s_log_src_ptr != NULL && s_log_src_size > 0) {
            DataLogger_Log(s_log_src_ptr, s_log_src_size);
        }
    }

    /* 2. CDC Data Streaming (PhAI V2 프로토콜) */
    bool is_hw_ready = XM_IsUsbStreamConnected();
    bool is_logic_active = XM_IsUsbStreamingActive();

    /* 재연결 감지 → Meta 재전송 플래그 리셋 */
    if (is_hw_ready && !s_prev_connected) {
        s_meta_sent = false;
    }
    s_prev_connected = is_hw_ready;

    if (is_hw_ready && is_logic_active) {
        /* 2-1. User Custom Meta — 연결 후 1회 전송 (Module ID 0xEF) */
        if (!s_meta_sent && s_custom_meta_json != NULL) {
            /* Wire format: [target_module_id:1B] [json_bytes...] */
            uint32_t json_len = strlen(s_custom_meta_json);
            uint8_t meta_buf[513]; /* 1B target + 512B max JSON */
            if (json_len <= 512) {
                meta_buf[0] = s_custom_meta_target_id;
                memcpy(&meta_buf[1], s_custom_meta_json, json_len);
                if (PhAI_PacketBuild(meta_buf, 1 + json_len, PHAI_MODULE_USER_META)) {
                    s_meta_sent = true;
                }
            }
        }

        /* 2-2. System Total Data — optional auto-stream (Module ID 0x20) */
        if (s_total_data_stream_enabled) {
            uint32_t total_size;
            const XM_TotalDataPacket_t* pkt = XM_TotalData_GetLatest(&total_size);
            PhAI_PacketBuild(pkt, total_size, PHAI_MODULE_TOTAL_DATA);
        }

        /* 2-3. Legacy user stream source (과도기 호환, deprecated) */
        if (s_stream_src_ptr != NULL && s_stream_src_size > 0) {
            PhAI_PacketBuild(s_stream_src_ptr, s_stream_src_size, s_stream_module_id);
        }
    }
}

/* ==========================================================================
 * User Custom Data API (신규)
 * ========================================================================== */

void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str)
{
    s_custom_meta_target_id = module_id;
    s_custom_meta_json = json_str;
    s_meta_sent = false; /* 재등록 시 재전송 */
}

bool XM_SendUsbDataWithId(const void* data, uint32_t len, uint8_t module_id)
{
    if (!XM_IsUsbStreamConnected()) return false;
    return PhAI_PacketBuild(data, len, module_id);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */
