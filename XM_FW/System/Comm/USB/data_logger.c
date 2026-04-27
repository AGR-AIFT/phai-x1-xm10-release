/**
 ******************************************************************************
 * @file    data_logger.c
 * @author  HyundoKim
 * @brief   [System Layer] USB MSC 로깅 태스크 (Consumer) 구현 - 양산 품질
 * @details
 * - 2-Stage: UserTask(Producer) -> D2 RAM Ring Buffer -> DataLoggerTask(Consumer) -> USB
 * - Lock-Free SPSC 링 버퍼 (280KB, D2 RAM)
 * - Self-describing binary format: 32B Header + Block CRC32 + 12B Footer
 * - Emergency Stop: 5종 StopReason + deferred stop (UserTask FATFS 접근 방지)
 * - Event Marker: 로깅 스트림 내 이벤트 마커 삽입 (12B)
 * - Disk Monitoring: 10초 주기 디스크 용량 캐시 + LOW 경고
 * - Auto Session Naming: boot count 기반 B%03lu_%03lu 자동 넘버링
 * - No PSRAM, No RTC (Rev1.1)
 * @version 1.0
 * @date    Nov 10, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "data_logger.h"
#include "usb_mode_handler.h" // USB 상태 플래그(g_isUsbHostReady) 사용
#include "ioif_agrb_fs.h"     // IOIF 파일 시스템 API
#include "ioif_agrb_dwt.h"   // DWT cycle counter (f_sync 소요 시간 측정)
#include "stm32h7xx_hal_crc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

extern CRC_HandleTypeDef hcrc;

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* --- 1. RAM 링 버퍼 (댐) 설정 ---
 * D2 RAM (288KB) 중 280KB를 로깅 버퍼로 사용
 * 필요 대역폭 300Bytes x 500Hz = 150kB/s
 * 버퍼 용량 280kB의 USB write시의 GC에 의한 멈춤 시간을 버티는 시간 280/150 = ~1.86초
 * USB 스틱의 일반적인 최대 지연이 0.5초 미만이므로 1.86초는 안정적인 마진 */
#define LOG_BUFFER_SIZE             (280 * 1024)

/* --- 2. 로깅 태스크 주기 --- */
#define LOGGER_TASK_PERIOD_MS       (100)

/* --- 3. 링 버퍼 위험 수위 --- */
#define BUFFER_WARNING_THRESHOLD_PERCENT (90)
#define BUFFER_STOP_THRESHOLD_PERCENT    (98)

/* --- 4. 배치 쓰기(f_write) 단위 --- */
#define MAX_WRITE_CHUNK_SIZE        (32 * 1024)

/* --- 5. 명령 큐 설정 --- */
#define CMD_QUEUE_SIZE              (5)
#define MAX_METADATA_SIZE           (2048)
#define MAX_PATH_SIZE               (512)
#define MAX_SESSION_NAME_SIZE       (100)
#define MAX_FULL_PATH_SIZE          (MAX_PATH_SIZE + 64)

#define LOG_FILE_ROLLING_SIZE_MB    (10)
#define LOG_DIR_PATH                "/LOGS"

/* --- 6. Block CRC 설정 --- */
#define CRC_BLOCK_SIZE              4096U
#define DISK_CHECK_INTERVAL         100U     /* 100 cycles x 100ms = 10초 */
#define DISK_LOW_THRESHOLD_MB       50U

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 링 버퍼에 저장될 패킷 헤더 */
typedef struct {
    uint32_t packetSize;
} LogPacketHeader_t;

typedef enum {
    LOG_CMD_START_SESSION,
    LOG_CMD_STOP_SESSION,
} LoggerCommand_e;

typedef struct {
    LoggerCommand_e command;
    char sessionName[MAX_SESSION_NAME_SIZE + 1];
    char metadata[MAX_METADATA_SIZE];
} LoggerCommand_t;

/** @brief 비정상 종료 원인 (private) */
typedef enum {
    STOP_REASON_NORMAL,
    STOP_REASON_HOT_OVERFLOW,
    STOP_REASON_USB_DISCONNECT,
    STOP_REASON_WRITE_FAILURE,
    STOP_REASON_ROLLING_FAILURE,
} StopReason_e;

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

extern volatile bool g_isUsbHostReady;
QueueHandle_t g_logCmdQueue;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/* --- 태스크 --- */
static osThreadId_t s_loggerTaskHandle;
const osThreadAttr_t s_loggerTask_attributes = {
    .name = "DataLoggerTask",
    .stack_size = TASK_STACK_USB_SAVE,
    .priority = (osPriority_t) TASK_PRIO_USB_SAVE,
};

/* --- Lock-Free SPSC 링 버퍼 (D2 RAM) --- */
__attribute__((section(IOIF_FS_SECTION)))
static uint8_t s_logBuffer[LOG_BUFFER_SIZE];

static volatile atomic_uint s_logHead = 0;
static volatile uint32_t s_logTail = 0;

/* --- 상태 변수 --- */
static volatile atomic_bool s_isLoggingActive = false;
static volatile atomic_uint s_logStatus = LOG_STATUS_IDLE;

/* --- 로깅 통계 --- */
static DataLogger_Stats_t s_stats = {0};
static volatile uint32_t s_fsync_last_us = 0;   /**< 직전 f_sync 소요 시간 (DWT) */
static volatile uint32_t s_fsync_max_us  = 0;   /**< 세션 내 f_sync 최대 소요 시간 */

/* --- 편의 기능 설정 --- */
static volatile bool s_auto_timestamp_enabled = true;
static uint32_t s_rolling_size_mb = LOG_FILE_ROLLING_SIZE_MB;
static uint32_t s_user_packet_size = 0;

/* --- 파일 관리 --- */
static IOIF_FILEx_t s_currentFileId;
static char s_currentSessionPath[MAX_PATH_SIZE];
static uint32_t s_currentSessionIndex = 0;
static uint32_t s_currentSplitIndex = 0;

/* --- Block CRC 관련 --- */
static uint32_t s_crc_block_offset = 0;
static uint8_t s_write_buf[MAX_WRITE_CHUNK_SIZE];
static uint32_t s_part_record_count = 0;
static uint32_t s_part_data_bytes = 0;

/* --- Deferred Emergency Stop (UserTask -> DataLoggerTask) --- */
static volatile StopReason_e s_pending_stop_reason = STOP_REASON_NORMAL;
static volatile bool s_pending_stop = false;

/* --- 파일 이중 close 방지 (Q6) --- */
static bool s_file_open = false;

/* --- 디스크 모니터링 캐시 --- */
static uint32_t s_cached_disk_free_mb = 0;
static uint32_t s_cached_disk_total_mb = 0;
static uint32_t s_disk_check_counter = 0;

/* --- Boot count 기반 자동 넘버링 --- */
static uint32_t s_boot_count = 0;
static bool s_boot_count_loaded = false;

/* --- 명령 큐 버퍼 (스택 2.1KB -> static으로 이동, F14) --- */
static LoggerCommand_t s_cmd_buffer;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void StartDataLoggerTask(void* argument);
static bool _CheckUsbSpace(void);
static bool _InitSensorLogSession(const char* sessionName, const char* metadata);
static bool _CheckFileRolling(void);
static void _HandleCommandQueue(void);
static void _ProcessRingBuffer(void);
static uint32_t _GetBufferUsedSize(uint32_t head, uint32_t tail);
static void _RingBufferWrite(uint32_t* idx, const uint8_t* data, uint32_t size);

/* --- 신규 헬퍼 (양산 품질) --- */
static bool _WriteFileHeader(void);
static bool _WriteFileFooter(void);
static bool _WriteDataWithBlockCRC(const uint8_t* data, uint32_t size);
static bool _FlushPartialBlockCRC(void);
static void _EmergencyStop(StopReason_e reason);
static void _RequestEmergencyStop(StopReason_e reason);
static void _WriteSummary_WithStatus(StopReason_e reason);
static const char* _StopReasonToString(StopReason_e reason);
static void _UpdateDiskFreeCache(void);
static void _LoadBootCount(void);
static bool _AutoFindSessionName(char* out_name, uint32_t out_size);
static bool _ValidateSessionName(const char* name);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void DataLogger_Init(void)
{
    g_logCmdQueue = xQueueCreate(CMD_QUEUE_SIZE, sizeof(LoggerCommand_t));

    atomic_store(&s_logHead, 0);
    s_logTail = 0;
    atomic_store(&s_isLoggingActive, false);
    atomic_store(&s_logStatus, LOG_STATUS_IDLE);
    memset(&s_stats, 0, sizeof(s_stats));

    s_crc_block_offset = 0;
    s_part_record_count = 0;
    s_part_data_bytes = 0;
    s_pending_stop = false;
    s_file_open = false;
    s_cached_disk_free_mb = 0;
    s_cached_disk_total_mb = 0;
    s_disk_check_counter = 0;
    s_boot_count = 0;
    s_boot_count_loaded = false;

    s_loggerTaskHandle = osThreadNew(StartDataLoggerTask, NULL, &s_loggerTask_attributes);
}

bool DataLogger_IsReady(void)
{
    return (ioif_filesystem_is_ready() & g_isUsbHostReady);
}

/**
 * @brief [비실시간] 데이터 로깅 세션을 시작합니다.
 * @param[in] sessionName 세션명. NULL 시 자동 넘버링 (B%03lu_%03lu).
 * @param[in] metadata    데이터 구조를 설명하는 텍스트
 * @return 요청 성공 시 true
 */
bool DataLogger_Start(const char* sessionName, const char* metadata)
{
    if (!g_isUsbHostReady || metadata == NULL) return false;

    /* sessionName == NULL 또는 "" (빈 문자열): 자동 넘버링 (_HandleCommandQueue에서 처리)
     * Why: XM_StartUsbDataLog("", metadata) 호출 시 빈 문자열도 auto-numbering 트리거.
     *      _ValidateSessionName("")이 false를 반환하여 START 명령이 큐에 도달하지 못하는 버그 수정. */
    bool is_auto_naming = (sessionName == NULL || sessionName[0] == '\0');
    if (!is_auto_naming) {
        if (strlen(sessionName) > MAX_SESSION_NAME_SIZE) return false;
        if (!_ValidateSessionName(sessionName)) return false;
    }

    if (strlen(metadata) >= MAX_METADATA_SIZE) return false;

    LoggerCommand_t cmd = {0};
    cmd.command = LOG_CMD_START_SESSION;
    if (sessionName != NULL) {
        strncpy(cmd.sessionName, sessionName, MAX_SESSION_NAME_SIZE);
    }
    /* sessionName == NULL: cmd.sessionName[0] == '\0' (자동 넘버링 트리거) */
    strncpy(cmd.metadata, metadata, MAX_METADATA_SIZE - 1);

    return (xQueueSend(g_logCmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE);
}

void DataLogger_Stop(void)
{
    /* 즉시 로깅 차단 — DataLogger_Log() 호출을 바로 거부
     * Why: 큐 전송만 하면 DataLoggerTask(100ms)가 처리할 때까지
     *      최대 100ms간 stale 데이터가 링 버퍼에 계속 쌓임.
     *      atomic_store는 UserTask 컨텍스트에서 안전 (Lock-Free). */
    atomic_store(&s_isLoggingActive, false);

    LoggerCommand_t cmd = {0};
    cmd.command = LOG_CMD_STOP_SESSION;
    xQueueSend(g_logCmdQueue, &cmd, 0);
}

/**
 * @brief [실시간] 1ms UserTask가 링 버퍼에 직접 쓰기 (Lock-Free)
 * @details auto_timestamp 활성화 시 매 패킷 앞에 4-byte tick 자동 삽입.
 *          peak% 추적 + overflow 시 deferred EmergencyStop.
 */
bool DataLogger_Log(const void* logPacket, uint32_t packetSize)
{
    if (!atomic_load(&s_isLoggingActive)) return false;
    if (packetSize == 0 || packetSize > MAX_LOG_PACKET_SIZE) return false;

    uint32_t ts_size = s_auto_timestamp_enabled ? LOG_TIMESTAMP_SIZE : 0;
    uint32_t payload_size = ts_size + packetSize;

    uint32_t head = atomic_load(&s_logHead);
    uint32_t tail = s_logTail;

    uint32_t used_size = _GetBufferUsedSize(head, tail);
    uint32_t free_space = LOG_BUFFER_SIZE - used_size - 1;
    uint32_t total_write_size = sizeof(LogPacketHeader_t) + payload_size;

    /* peak% 추적 */
    uint32_t usage_pct = (used_size * 100) / LOG_BUFFER_SIZE;
    if (usage_pct > s_stats.hot_buffer_peak_percent) {
        s_stats.hot_buffer_peak_percent = (uint8_t)usage_pct;
    }

    if (total_write_size > free_space) {
        s_stats.dropped_packets++;
        _RequestEmergencyStop(STOP_REASON_HOT_OVERFLOW);
        return false;
    }

    uint32_t idx = head;
    LogPacketHeader_t header = { .packetSize = payload_size };

    _RingBufferWrite(&idx, (const uint8_t*)&header, sizeof(LogPacketHeader_t));

    if (s_auto_timestamp_enabled) {
        uint32_t tick = xTaskGetTickCount();
        _RingBufferWrite(&idx, (const uint8_t*)&tick, sizeof(uint32_t));
    }

    _RingBufferWrite(&idx, (const uint8_t*)logPacket, packetSize);

    atomic_store(&s_logHead, idx);
    s_stats.total_packets_logged++;
    s_part_record_count++;
    return true;
}

DataLogger_Status_e DataLogger_GetStatus(void)
{
    return (DataLogger_Status_e)atomic_load(&s_logStatus);
}

void DataLogger_GetStats(DataLogger_Stats_t* stats)
{
    if (stats != NULL) {
        *stats = s_stats;
    }
}

uint32_t DataLogger_GetLastFsyncUs(void)
{
    return s_fsync_last_us;
}

uint32_t DataLogger_GetMaxFsyncUs(void)
{
    return s_fsync_max_us;
}

void DataLogger_SetPacketSize(uint32_t size)
{
    s_user_packet_size = size;
}

void DataLogger_SetAutoTimestamp(bool enabled)
{
    s_auto_timestamp_enabled = enabled;
}

void DataLogger_SetRollingSize(uint32_t size_mb)
{
    if (size_mb >= 1 && size_mb <= 100) {
        s_rolling_size_mb = size_mb;
    }
}

/**
 * @brief [실시간] 이벤트 마커를 로그 스트림에 삽입합니다.
 * @param[in] marker_type 마커 타입 (0x01~0xFF)
 * @param[in] data        컨텍스트 데이터 (에러 코드, 모드 ID 등)
 * @return true: 성공, false: 로깅 비활성 또는 버퍼 부족
 */
bool DataLogger_InsertMarker(uint8_t marker_type, uint16_t data)
{
    if (!atomic_load(&s_isLoggingActive)) return false;

    /* LogPacketHeader_t.packetSize = LOG_MARKER_MAGIC | type (상위 24비트 식별) */
    uint32_t head = atomic_load(&s_logHead);
    uint32_t tail = s_logTail;
    uint32_t used_size = _GetBufferUsedSize(head, tail);
    uint32_t free_space = LOG_BUFFER_SIZE - used_size - 1;
    uint32_t total_size = sizeof(LogPacketHeader_t) + sizeof(LogMarkerRecord_t);

    if (total_size > free_space) {
        return false; /* silent drop - emergency stop 안 함 */
    }

    uint32_t idx = head;

    /* 헤더: packetSize의 상위 24비트가 LOG_MARKER_MAGIC → 디코더가 마커로 식별 */
    LogPacketHeader_t pkt_hdr = { .packetSize = LOG_MARKER_MAGIC | (uint32_t)marker_type };
    _RingBufferWrite(&idx, (const uint8_t*)&pkt_hdr, sizeof(LogPacketHeader_t));

    /* 마커 레코드 (12B) */
    LogMarkerRecord_t marker = {
        .marker_header = LOG_MARKER_MAGIC | (uint32_t)marker_type,
        .tick_ms = xTaskGetTickCount(),
        .marker_data = data,
        .reserved = 0,
    };
    _RingBufferWrite(&idx, (const uint8_t*)&marker, sizeof(LogMarkerRecord_t));

    atomic_store(&s_logHead, idx);
    return true;
}

/**
 * @brief 캐시된 USB 디스크 잔여 용량(MB) 반환.
 */
uint32_t DataLogger_GetDiskFreeMB(void)
{
    return s_cached_disk_free_mb;
}

/**
 * @brief 캐시된 USB 디스크 전체 용량(MB) 반환.
 */
uint32_t DataLogger_GetDiskTotalMB(void)
{
    return s_cached_disk_total_mb;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* ============================================================
 * 링 버퍼 유틸리티
 * ============================================================ */

/**
 * @brief 링 버퍼의 현재 사용량을 계산합니다.
 */
static uint32_t _GetBufferUsedSize(uint32_t head, uint32_t tail)
{
    if (head >= tail) {
        return head - tail;
    } else {
        return (LOG_BUFFER_SIZE - tail) + head;
    }
}

/**
 * @brief 링 버퍼에 데이터 쓰기 (Wrap-around 자동 처리)
 */
static void _RingBufferWrite(uint32_t* idx, const uint8_t* data, uint32_t size)
{
    uint32_t pos = *idx;
    uint32_t to_end = LOG_BUFFER_SIZE - pos;

    if (size <= to_end) {
        memcpy(&s_logBuffer[pos], data, size);
    } else {
        memcpy(&s_logBuffer[pos], data, to_end);
        memcpy(&s_logBuffer[0], data + to_end, size - to_end);
    }
    *idx = (pos + size) % LOG_BUFFER_SIZE;
}

/**
 * @brief 링 버퍼에서 데이터 읽기 (Wrap-around 자동 처리, s_write_buf로 복사)
 */
static void _RingBufferRead(uint32_t tail, uint8_t* dest, uint32_t size)
{
    uint32_t to_end = LOG_BUFFER_SIZE - tail;

    if (size <= to_end) {
        memcpy(dest, &s_logBuffer[tail], size);
    } else {
        memcpy(dest, &s_logBuffer[tail], to_end);
        memcpy(dest + to_end, &s_logBuffer[0], size - to_end);
    }
}

/* ============================================================
 * Self-Describing Binary Format: Header / Footer / CRC
 * ============================================================ */

/**
 * @brief 32B 파일 헤더를 기록하고 파트 카운터를 리셋합니다.
 */
static bool _WriteFileHeader(void)
{
    uint32_t ts_bytes = s_auto_timestamp_enabled ? LOG_TIMESTAMP_SIZE : 0;
    uint32_t record_payload = ts_bytes + s_user_packet_size;
    uint32_t record_total = sizeof(LogPacketHeader_t) + record_payload;

    DataLogFileHeader_t hdr = {
        .magic = LOG_FILE_MAGIC,
        .format_version = LOG_FILE_FORMAT_VERSION,
        .flags = (uint8_t)((s_auto_timestamp_enabled ? LOG_FILE_FLAG_AUTO_TS : 0)
                            | LOG_FILE_FLAG_BLOCK_CRC),
        .header_size = sizeof(DataLogFileHeader_t),
        .record_total_bytes = record_total,
        .user_payload_bytes = s_user_packet_size,
        .creation_fattime = 0, /* Rev1.1: RTC 없음 */
        .reserved = {0, 0},
    };

    FRESULT res = FR_OK;
    AGRBFileSystem.write(s_currentFileId, (const uint8_t*)&hdr, sizeof(hdr), &res);
    if (res != FR_OK) {
        s_stats.write_errors++;
        return false;
    }

    /* 파트 카운터 리셋 */
    s_part_record_count = 0;
    s_part_data_bytes = 0;
    s_crc_block_offset = 0;

    return true;
}

/**
 * @brief 12B 파일 풋터를 기록합니다.
 * @details data_bytes = s_part_data_bytes (순수 데이터만, CRC 제외 — Q4 fix)
 */
static bool _WriteFileFooter(void)
{
    DataLogFileFooter_t footer = {
        .record_count = s_part_record_count,
        .data_bytes = s_part_data_bytes,
        .footer_magic = LOG_FILE_FOOTER_MAGIC,
    };

    FRESULT res = FR_OK;
    AGRBFileSystem.write(s_currentFileId, (const uint8_t*)&footer, sizeof(footer), &res);
    if (res != FR_OK) {
        s_stats.write_errors++;
        return false;
    }
    return true;
}

/**
 * @brief 데이터를 파일에 쓰면서 4KB 블록 CRC32를 계산/삽입합니다.
 * @details
 *   - 블록 시작(s_crc_block_offset==0): __HAL_CRC_DR_RESET
 *   - 매 chunk: HAL_CRC_Accumulate (BYTES 모드, alignment 무관)
 *   - 블록 완성(s_crc_block_offset >= CRC_BLOCK_SIZE): CRC32 4B append
 *   - 실패 시 재시도 없이 즉시 return false (Q1: CRC 상태 보존 불가)
 *   - s_part_data_bytes += chunk (순수 데이터만 추적, CRC 제외 — Q4)
 */
static bool _WriteDataWithBlockCRC(const uint8_t* data, uint32_t size)
{
    uint32_t offset = 0;

    while (offset < size) {
        /* 블록 시작: HW CRC 리셋 */
        if (s_crc_block_offset == 0) {
            __HAL_CRC_DR_RESET(&hcrc);
        }

        /* 이번 chunk: 남은 데이터와 블록 잔여 공간 중 작은 값 */
        uint32_t remain_in_block = CRC_BLOCK_SIZE - s_crc_block_offset;
        uint32_t remain_in_data = size - offset;
        uint32_t chunk = (remain_in_data < remain_in_block) ? remain_in_data : remain_in_block;

        /* 파일에 쓰기 */
        FRESULT res = FR_OK;
        AGRBFileSystem.write(s_currentFileId, &data[offset], chunk, &res);
        if (res != FR_OK) {
            s_stats.write_errors++;
            return false; /* Q1: 재시도 없이 즉시 실패 */
        }

        /* HW CRC 누적 */
        HAL_CRC_Accumulate(&hcrc, (uint32_t*)&data[offset], chunk);

        /* Q4: 순수 데이터만 추적 (CRC 오버헤드 미포함) */
        s_part_data_bytes += chunk;
        s_stats.total_bytes_written += chunk;
        offset += chunk;
        s_crc_block_offset += chunk;

        /* 블록 완성: CRC32 4B append */
        if (s_crc_block_offset >= CRC_BLOCK_SIZE) {
            uint32_t crc_val = hcrc.Instance->DR;
            FRESULT crc_res = FR_OK;
            AGRBFileSystem.write(s_currentFileId, (const uint8_t*)&crc_val, sizeof(uint32_t), &crc_res);
            if (crc_res != FR_OK) {
                s_stats.write_errors++;
                return false;
            }
            /* CRC 4B는 s_part_data_bytes에 포함하지 않음 (Q4) */
            s_crc_block_offset = 0;
        }
    }

    return true;
}

/**
 * @brief 마지막 불완전 블록의 CRC를 flush합니다.
 */
static bool _FlushPartialBlockCRC(void)
{
    if (s_crc_block_offset == 0) return true; /* 불완전 블록 없음 */

    uint32_t crc_val = hcrc.Instance->DR;
    FRESULT res = FR_OK;
    AGRBFileSystem.write(s_currentFileId, (const uint8_t*)&crc_val, sizeof(uint32_t), &res);
    if (res != FR_OK) {
        s_stats.write_errors++;
        return false;
    }
    s_crc_block_offset = 0;
    return true;
}

/* ============================================================
 * Emergency Stop
 * ============================================================ */

/**
 * @brief 통합 에러 정리 (DataLoggerTask 컨텍스트에서만 호출)
 * @details
 *   1. isLoggingActive = false (새 데이터 유입 차단)
 *   2. _ProcessRingBuffer(): 가능한 만큼 drain (Q3)
 *   3. if s_file_open && IsReady: FlushCRC + Footer + close (Q6)
 *   4. end_tick 기록 → WriteSummary
 */
static void _EmergencyStop(StopReason_e reason)
{
    /* 1. 새 데이터 유입 차단 */
    atomic_store(&s_isLoggingActive, false);

    /* 2. Q3: 가능한 만큼 ring buffer drain */
    _ProcessRingBuffer();

    /* 3. Q6: s_file_open 가드로 이중 close 방지 */
    if (s_file_open) {
        if (DataLogger_IsReady()) {
            _FlushPartialBlockCRC();
            _WriteFileFooter();
            AGRBFileSystem.close(s_currentFileId, NULL);
        }
        s_file_open = false;
    }

    /* 4. 종료 정보 기록 */
    s_stats.end_tick = xTaskGetTickCount();

    /* USB Disconnect 시 FATFS 접근 불가 → summary 스킵 */
    if (reason != STOP_REASON_USB_DISCONNECT && DataLogger_IsReady()) {
        _WriteSummary_WithStatus(reason);
    }

    atomic_store(&s_logStatus, LOG_STATUS_ERROR_STOPPED);
}

/**
 * @brief UserTask용 deferred stop 요청 (flag만 설정, FATFS 접근 금지)
 */
static void _RequestEmergencyStop(StopReason_e reason)
{
    s_pending_stop_reason = reason;
    s_pending_stop = true;
}

/* ============================================================
 * Summary / Diagnostics
 * ============================================================ */

/**
 * @brief StopReason enum → 문자열 변환
 */
static const char* _StopReasonToString(StopReason_e reason)
{
    switch (reason) {
        case STOP_REASON_NORMAL:          return "NORMAL";
        case STOP_REASON_HOT_OVERFLOW:    return "HOT_OVERFLOW";
        case STOP_REASON_USB_DISCONNECT:  return "USB_DISCONNECT";
        case STOP_REASON_WRITE_FAILURE:   return "WRITE_FAILURE";
        case STOP_REASON_ROLLING_FAILURE: return "ROLLING_FAILURE";
        default:                          return "UNKNOWN";
    }
}

/**
 * @brief 세션 종료 시 summary.txt를 생성합니다 (기존 _WriteSummary 대체).
 * @param[in] reason 종료 원인
 */
static void _WriteSummary_WithStatus(StopReason_e reason)
{
    IOIF_FILEx_t file_id;
    char path[MAX_FULL_PATH_SIZE];
    snprintf(path, sizeof(path), "%s/summary.txt", s_currentSessionPath);

    if (AGRBFileSystem.open_write(&file_id, path, IOIF_FileSystem_CreateMode_OVERWRITE, NULL) != IOIF_FileSystem_OK) {
        return;
    }

    uint32_t duration_ms = s_stats.end_tick - s_stats.start_tick;
    const char* status_str = (reason == STOP_REASON_NORMAL) ? "COMPLETED" : "ERROR_STOPPED";

    char buf[768];
    int len = snprintf(buf, sizeof(buf),
        "=== Session Summary ===\n"
        "status=%s\n"
        "stop_reason=%s\n"
        "start_tick=%lu\n"
        "end_tick=%lu\n"
        "duration_ms=%lu\n"
        "total_packets=%lu\n"
        "total_bytes=%lu\n"
        "dropped_packets=%lu\n"
        "write_errors=%lu\n"
        "sync_count=%lu\n"
        "file_count=%lu\n"
        "hot_buffer_peak_percent=%u\n"
        "disk_free_mb=%lu\n"
        "disk_total_mb=%lu\n",
        status_str,
        _StopReasonToString(reason),
        (unsigned long)s_stats.start_tick,
        (unsigned long)s_stats.end_tick,
        (unsigned long)duration_ms,
        (unsigned long)s_stats.total_packets_logged,
        (unsigned long)s_stats.total_bytes_written,
        (unsigned long)s_stats.dropped_packets,
        (unsigned long)s_stats.write_errors,
        (unsigned long)s_stats.sync_count,
        (unsigned long)(s_currentSplitIndex + 1),
        (unsigned)s_stats.hot_buffer_peak_percent,
        (unsigned long)s_stats.disk_free_mb,
        (unsigned long)s_stats.disk_total_mb);

    AGRBFileSystem.write(file_id, (uint8_t*)buf, (uint32_t)len, NULL);
    AGRBFileSystem.close(file_id, NULL);
}

/* ============================================================
 * Disk Monitoring
 * ============================================================ */

/**
 * @brief 10초 주기로 디스크 용량 캐시를 갱신합니다.
 */
static void _UpdateDiskFreeCache(void)
{
    if (++s_disk_check_counter < DISK_CHECK_INTERVAL) return;
    s_disk_check_counter = 0;

    uint32_t free_mb = 0;
    uint32_t total_mb = 0;
    if (AGRBFileSystem.get_space_mb(&free_mb, &total_mb) == IOIF_FileSystem_OK) {
        s_cached_disk_free_mb = free_mb;
        s_cached_disk_total_mb = total_mb;
        s_stats.disk_free_mb = free_mb;
        s_stats.disk_total_mb = total_mb;
    }
}

/* ============================================================
 * Boot Count / Auto Session Naming
 * ============================================================ */

/**
 * @brief /LOGS/.boot_count 파일에서 부팅 횟수를 로드하고 +1 후 저장합니다.
 * @details 같은 부팅 내에서는 1회만 로드 (s_boot_count_loaded 가드).
 *          USB 교체 시 파일 없음 → 1부터 시작 (자연 리셋).
 */
static void _LoadBootCount(void)
{
    if (s_boot_count_loaded) return;

    IOIF_FILEx_t file_id;
    char path[] = "/LOGS/.boot_count";
    char buf[16] = {0};

    /* /LOGS 디렉터리 보장 */
    AGRBFileSystem.mkdir(LOG_DIR_PATH, NULL);

    /* 파일 읽기 시도 */
    if (AGRBFileSystem.open(&file_id, path, IOIF_FileSystem_AccessMode_READONLY, NULL) == IOIF_FileSystem_OK) {
        uint32_t bytes_read = 0;
        AGRBFileSystem.read(file_id, (uint8_t*)buf, sizeof(buf) - 1, &bytes_read, NULL);
        AGRBFileSystem.close(file_id, NULL);
        s_boot_count = (uint32_t)atol(buf) + 1;
    } else {
        /* 파일 없음 (새 USB): 1부터 시작 */
        s_boot_count = 1;
    }

    /* 증가된 값 저장 */
    if (AGRBFileSystem.open_write(&file_id, path, IOIF_FileSystem_CreateMode_OVERWRITE, NULL) == IOIF_FileSystem_OK) {
        int len = snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_boot_count);
        AGRBFileSystem.write(file_id, (const uint8_t*)buf, (uint32_t)len, NULL);
        AGRBFileSystem.close(file_id, NULL);
    }

    s_boot_count_loaded = true;
}

/**
 * @brief B%03lu_%03lu 형식으로 자동 세션명을 생성합니다 (binary search).
 * @param[out] out_name 결과 세션명 버퍼
 * @param[in]  out_size 버퍼 크기
 * @return true: 성공, false: 999개 초과
 */
static bool _AutoFindSessionName(char* out_name, uint32_t out_size)
{
    uint32_t low = 0;
    uint32_t high = 1000;
    IOIF_FILEx_t temp_id;
    char temp_path[MAX_FULL_PATH_SIZE];

    /* 해당 부팅 번호 내 다음 빈 순번 탐색 */
    while (low < high) {
        uint32_t mid = (low + high) / 2;

        /* B005_003/data_000_part_000.bin 형태로 존재 여부 확인 */
        snprintf(temp_path, sizeof(temp_path), "%s/B%03lu_%03lu",
                 LOG_DIR_PATH, (unsigned long)s_boot_count, (unsigned long)mid);

        /* 세션 디렉터리 존재 확인: 내부 data 파일로 판단 */
        char check_path[MAX_FULL_PATH_SIZE];
        snprintf(check_path, sizeof(check_path), "%s/data_000_part_000.bin", temp_path);

        if (AGRBFileSystem.open(&temp_id, check_path, IOIF_FileSystem_AccessMode_READONLY, NULL) == IOIF_FileSystem_OK) {
            AGRBFileSystem.close(temp_id, NULL);
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    if (low >= 1000) return false;

    snprintf(out_name, out_size, "B%03lu_%03lu",
             (unsigned long)s_boot_count, (unsigned long)low);
    return true;
}

/**
 * @brief FATFS 금지 문자 검증
 * @details 금지: < > : " / \ | ? * + 제어문자(< 0x20)
 */
static bool _ValidateSessionName(const char* name)
{
    if (name == NULL || name[0] == '\0') return false;

    for (const char* p = name; *p != '\0'; p++) {
        if ((uint8_t)*p < 0x20) return false;
        switch (*p) {
            case '<': case '>': case ':': case '"':
            case '/': case '\\': case '|': case '?': case '*':
                return false;
            default:
                break;
        }
    }
    return true;
}

/* ============================================================
 * USB Space Check
 * ============================================================ */

static bool _CheckUsbSpace(void)
{
    uint32_t free_mb = 0;
    if (AGRBFileSystem.get_free_space_mb(&free_mb) == IOIF_FileSystem_OK) {
        return (free_mb > s_rolling_size_mb);
    }
    return false;
}

/* ============================================================
 * Session Init
 * ============================================================ */

/**
 * @brief 세션 시작 시 디렉터리/메타데이터 생성 + 첫 파일 오픈
 */
static bool _InitSensorLogSession(const char* sessionName, const char* metadata)
{
    IOIF_FILEx_t file_id;
    char path[MAX_FULL_PATH_SIZE];

    /* 1. 기본 로그 디렉터리 생성 */
    AGRBFileSystem.mkdir(LOG_DIR_PATH, NULL);

    /* 2. 세션 디렉터리 생성 */
    snprintf(s_currentSessionPath, MAX_PATH_SIZE, "%s/%s", LOG_DIR_PATH, sessionName);
    AGRBFileSystem.mkdir(s_currentSessionPath, NULL);

    /* 3. 메타데이터 파일 저장 */
    snprintf(path, sizeof(path), "%s/metadata.txt", s_currentSessionPath);
    if (AGRBFileSystem.open_write(&file_id, path, IOIF_FileSystem_CreateMode_OVERWRITE, NULL) == IOIF_FileSystem_OK) {
        AGRBFileSystem.write(file_id, (uint8_t*)metadata, strlen(metadata), NULL);

        uint32_t ts_bytes = s_auto_timestamp_enabled ? LOG_TIMESTAMP_SIZE : 0;
        uint32_t record_payload = ts_bytes + s_user_packet_size;
        uint32_t record_total = sizeof(LogPacketHeader_t) + record_payload;

        char sys_meta[512];
        int len = snprintf(sys_meta, sizeof(sys_meta),
            "\n\n=== System Info ===\n"
            "file_format_version=%d\n"
            "auto_timestamp=%d\n"
            "timestamp_bytes=%lu\n"
            "user_payload_bytes=%lu\n"
            "record_header_bytes=%u\n"
            "record_total_bytes=%lu\n"
            "rolling_size_mb=%lu\n"
            "buffer_size_kb=%u\n"
            "logger_period_ms=%u\n"
            "block_crc_size=%u\n",
            LOG_FILE_FORMAT_VERSION,
            (int)s_auto_timestamp_enabled,
            (unsigned long)ts_bytes,
            (unsigned long)s_user_packet_size,
            (unsigned)sizeof(LogPacketHeader_t),
            (unsigned long)record_total,
            (unsigned long)s_rolling_size_mb,
            (unsigned)(LOG_BUFFER_SIZE / 1024),
            (unsigned)LOGGER_TASK_PERIOD_MS,
            (unsigned)CRC_BLOCK_SIZE);
        AGRBFileSystem.write(file_id, (uint8_t*)sys_meta, (uint32_t)len, NULL);

        AGRBFileSystem.close(file_id, NULL);
    } else {
        return false;
    }

    /* 4. 이진 탐색으로 빈 세션 번호(data_XXX) 찾기 */
    uint32_t low = 0;
    uint32_t high = 1000;
    IOIF_FILEx_t temp_id;
    char temp_path[MAX_FULL_PATH_SIZE];

    while (low < high) {
        uint32_t mid = (low + high) / 2;
        snprintf(temp_path, sizeof(temp_path), "%s/data_%03lu_part_000.bin",
                 s_currentSessionPath, (unsigned long)mid);
        if (AGRBFileSystem.open(&temp_id, temp_path, IOIF_FileSystem_AccessMode_READONLY, NULL) == IOIF_FileSystem_OK) {
            AGRBFileSystem.close(temp_id, NULL);
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    if (low >= 1000) return false;

    s_currentSessionIndex = low;
    s_currentSplitIndex = 0;

    /* 5. 첫 번째 파일 생성 */
    char final_path[MAX_FULL_PATH_SIZE];
    snprintf(final_path, sizeof(final_path), "%s/data_%03lu_part_%03lu.bin",
             s_currentSessionPath, (unsigned long)s_currentSessionIndex,
             (unsigned long)s_currentSplitIndex);

    if (AGRBFileSystem.open_write(&s_currentFileId, final_path, IOIF_FileSystem_CreateMode_EXCLUSIVE, NULL) != IOIF_FileSystem_OK) {
        return false;
    }

    return true;
}

/* ============================================================
 * File Rolling (with Header/Footer/CRC)
 * ============================================================ */

/**
 * @brief 파일 용량이 rolling_size_mb를 넘으면 다음 part 파일 생성
 * @details close 전: FlushCRC → Footer → close → s_file_open=false
 *          open 후: s_file_open=true → WriteFileHeader
 */
static bool _CheckFileRolling(void)
{
    uint32_t current_size_mb = 0;

    if (AGRBFileSystem.get_size_mb(s_currentFileId, &current_size_mb) != IOIF_FileSystem_OK) {
        return false;
    }

    if (current_size_mb < s_rolling_size_mb) {
        return true; /* 아직 롤링 불필요 */
    }

    /* --- 현재 파일 마무리 --- */
    _FlushPartialBlockCRC();
    _WriteFileFooter();
    AGRBFileSystem.close(s_currentFileId, NULL);
    s_file_open = false;

    /* --- 다음 파일 생성 --- */
    s_currentSplitIndex++;

    char next_path[MAX_FULL_PATH_SIZE];
    snprintf(next_path, sizeof(next_path), "%s/data_%03lu_part_%03lu.bin",
             s_currentSessionPath, (unsigned long)s_currentSessionIndex,
             (unsigned long)s_currentSplitIndex);

    if (AGRBFileSystem.open_write(&s_currentFileId, next_path, IOIF_FileSystem_CreateMode_EXCLUSIVE, NULL) != IOIF_FileSystem_OK) {
        /* Q6: s_file_open 이미 false → _EmergencyStop에서 이중 close 안전 */
        _EmergencyStop(STOP_REASON_ROLLING_FAILURE);
        return false;
    }

    s_file_open = true;
    _WriteFileHeader();

    return true;
}

/* ============================================================
 * Ring Buffer Processing (핵심 리팩토링)
 * ============================================================ */

/**
 * @brief 링 버퍼의 데이터를 s_write_buf로 복사 후 _WriteDataWithBlockCRC로 기록
 * @details
 *   - retry loop 제거 (CRC 상태 보존 — Q1)
 *   - 98% → EmergencyStop(HOT_OVERFLOW), 90% → WARNING
 *   - 마커 패킷: packetSize 상위 24비트가 LOG_MARKER_MAGIC이면 고정 12B payload
 */
static void _ProcessRingBuffer(void)
{
    uint32_t head = atomic_load(&s_logHead);
    uint32_t tail = s_logTail;
    uint32_t used_size = _GetBufferUsedSize(head, tail);

    /* --- 1. 버퍼 수위 감시 --- */
    uint32_t usage_percent = (used_size * 100) / LOG_BUFFER_SIZE;

    if (usage_percent > BUFFER_STOP_THRESHOLD_PERCENT) {
        /* isLoggingActive 가드: _EmergencyStop 내부 재귀 호출 방지 */
        if (atomic_load(&s_isLoggingActive)) {
            _EmergencyStop(STOP_REASON_HOT_OVERFLOW);
        }
        return;
    } else if (usage_percent > BUFFER_WARNING_THRESHOLD_PERCENT) {
        atomic_store(&s_logStatus, LOG_STATUS_WARNING_QUEUE_FULL);
    } else if (atomic_load(&s_isLoggingActive)) {
        atomic_store(&s_logStatus, LOG_STATUS_LOGGING);
    }

    /* --- 2. 패킷 경계 계산 → s_write_buf에 복사 --- */
    if (used_size == 0) return;

    uint32_t write_size = 0;
    uint32_t temp_tail = tail;

    while (write_size < MAX_WRITE_CHUNK_SIZE) {
        uint32_t remaining_in_buffer = _GetBufferUsedSize(head, temp_tail);
        if (remaining_in_buffer < sizeof(LogPacketHeader_t)) {
            break;
        }

        /* 헤더 읽기 (Wrap-around 고려) */
        LogPacketHeader_t pkt_hdr;
        uint8_t* hdr_ptr = (uint8_t*)&pkt_hdr;
        uint32_t hdr_tail = temp_tail;
        for (uint32_t i = 0; i < sizeof(LogPacketHeader_t); i++) {
            hdr_ptr[i] = s_logBuffer[hdr_tail];
            hdr_tail = (hdr_tail + 1) % LOG_BUFFER_SIZE;
        }

        /* 마커 패킷: 상위 24비트가 LOG_MARKER_MAGIC이면 고정 12B payload */
        uint32_t payload_size;
        if ((pkt_hdr.packetSize & 0xFFFFFF00U) == LOG_MARKER_MAGIC) {
            payload_size = sizeof(LogMarkerRecord_t); /* 12B 고정 */
        } else {
            payload_size = pkt_hdr.packetSize;
        }

        /* 패킷 전체가 버퍼에 있는지 확인 */
        if (payload_size > (remaining_in_buffer - sizeof(LogPacketHeader_t))) {
            break;
        }

        uint32_t total_packet_size = sizeof(LogPacketHeader_t) + payload_size;
        if (write_size + total_packet_size > MAX_WRITE_CHUNK_SIZE) {
            break;
        }

        write_size += total_packet_size;
        temp_tail = (temp_tail + sizeof(LogPacketHeader_t) + payload_size) % LOG_BUFFER_SIZE;
    }

    if (write_size == 0) return;

    /* --- 3. ring buffer → s_write_buf memcpy (wrap-around 자동 처리) --- */
    _RingBufferRead(tail, s_write_buf, write_size);

    /* --- 4. _WriteDataWithBlockCRC 1회 호출 --- */
    if (_WriteDataWithBlockCRC(s_write_buf, write_size)) {
        /* tail 진행 */
        s_logTail = (tail + write_size) % LOG_BUFFER_SIZE;
        _CheckFileRolling();
    } else {
        /* Q1: 쓰기 실패 → 즉시 EmergencyStop */
        _EmergencyStop(STOP_REASON_WRITE_FAILURE);
    }
}

/* ============================================================
 * Command Queue Handler
 * ============================================================ */

/**
 * @brief 2단계 큐(명령 큐)를 확인하고 처리합니다.
 * @details LoggerCommand_t를 static으로 이동 (F14: 스택 2.1KB 절감)
 */
static void _HandleCommandQueue(void)
{
    if (xQueueReceive(g_logCmdQueue, &s_cmd_buffer, 0) != pdTRUE) return;

    if (s_cmd_buffer.command == LOG_CMD_START_SESSION) {
        /* 이전 로깅이 비정상 종료된 경우 정리 */
        if (atomic_load(&s_isLoggingActive)) {
            if (s_file_open) {
                _FlushPartialBlockCRC();
                _WriteFileFooter();
                AGRBFileSystem.close(s_currentFileId, NULL);
                s_file_open = false;
            }
        }

        /* Boot count 로드 (같은 부팅 내 1회만) */
        _LoadBootCount();

        /* 자동 세션명 생성: sessionName[0] == '\0' → auto */
        char auto_name[MAX_SESSION_NAME_SIZE + 1];
        const char* session_name = s_cmd_buffer.sessionName;
        if (session_name[0] == '\0') {
            if (!_AutoFindSessionName(auto_name, sizeof(auto_name))) {
                atomic_store(&s_logStatus, LOG_STATUS_ERROR_STOPPED);
                return;
            }
            session_name = auto_name;
        }

        if (DataLogger_IsReady() && _CheckUsbSpace()) {
            if (_InitSensorLogSession(session_name, s_cmd_buffer.metadata)) {
                /* 링 버퍼 리셋 */
                atomic_store(&s_logHead, 0);
                s_logTail = 0;
                memset(&s_stats, 0, sizeof(s_stats));
                s_stats.start_tick = xTaskGetTickCount();

                /* 파일 헤더 기록 */
                s_file_open = true;
                _WriteFileHeader();

                /* 디스크 캐시 초기화 */
                s_disk_check_counter = DISK_CHECK_INTERVAL; /* 즉시 갱신 트리거 */
                _UpdateDiskFreeCache();

                /* Deferred stop 플래그 클리어 */
                s_pending_stop = false;

                atomic_store(&s_isLoggingActive, true);
                atomic_store(&s_logStatus, LOG_STATUS_LOGGING);
            } else {
                atomic_store(&s_logStatus, LOG_STATUS_ERROR_STOPPED);
            }
        } else {
            /* Why: DataLogger_IsReady() 또는 _CheckUsbSpace() 실패 시
             *      명령만 소비되고 상태가 IDLE로 남아 앱이 ACTIVE인데 로깅은 안 되는
             *      silent fail 버그. ERROR_STOPPED로 설정하여 앱이 감지 가능하게 함. */
            atomic_store(&s_logStatus, LOG_STATUS_ERROR_STOPPED);
        }
    }
    else if (s_cmd_buffer.command == LOG_CMD_STOP_SESSION) {
        /* Note: s_isLoggingActive는 DataLogger_Stop()에서 이미 false로 설정됨.
         * 파일 마무리는 s_file_open 기준으로 수행. */
        atomic_store(&s_isLoggingActive, false);  /* 방어적 중복 설정 */

        /* drain 잔여 데이터 */
        _ProcessRingBuffer();

        /* 파일 마무리 */
        if (s_file_open) {
            _FlushPartialBlockCRC();
            _WriteFileFooter();
            AGRBFileSystem.close(s_currentFileId, NULL);
            s_file_open = false;

            s_stats.end_tick = xTaskGetTickCount();
            _WriteSummary_WithStatus(STOP_REASON_NORMAL);
        }
        atomic_store(&s_logStatus, LOG_STATUS_IDLE);
    }
}

/* ============================================================
 * Main Task Loop
 * ============================================================ */

/**
 * @brief [핵심] 저순위(Prio 16) 데이터 저장 태스크 (100ms 주기)
 * @details
 *   - Deferred stop 체크 (UserTask에서 설정한 s_pending_stop 플래그)
 *   - USB disconnect 감지 → EmergencyStop
 *   - 디스크 모니터링 + DISK_LOW 경고 + recovery to LOGGING
 */
static void StartDataLoggerTask(void* argument)
{
    (void)argument;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(LOGGER_TASK_PERIOD_MS);
    uint32_t fsync_counter = 0;

    for (;;) {
        AGRBFileSystem.process_mount();

        /* --- USB Disconnect 감지 --- */
        if (!DataLogger_IsReady()) {
            if (atomic_load(&s_isLoggingActive)) {
                _EmergencyStop(STOP_REASON_USB_DISCONNECT);
            }
            fsync_counter = 0;
            vTaskDelayUntil(&xLastWakeTime, xPeriod);
            continue;
        }

        /* --- Deferred stop 체크 (UserTask에서 flag 설정) --- */
        if (s_pending_stop && atomic_load(&s_isLoggingActive)) {
            _EmergencyStop(s_pending_stop_reason);
            s_pending_stop = false;
            fsync_counter = 0;
            vTaskDelayUntil(&xLastWakeTime, xPeriod);
            continue;
        }

        _HandleCommandQueue();

        if (atomic_load(&s_isLoggingActive)) {
            _ProcessRingBuffer();

            /* [Sync Rev1.3] fsync 30초 주기 (300 x 100ms)
             * Why: f_sync → USB Host HAL I/O (22~35ms) + FreeRTOS 커널 내부 critical section
             *      → DataLoggerTask가 USB 트랜잭션 완료까지 점유
             *      → 경로 내 brief taskENTER_CRITICAL(BASEPRI=5) → Timer ISR(NVIC5) 마스킹
             *      → UserTask 1kHz 트리거 2~3ms 지연 → Real Gap 발생 (B002/B003 실증).
             *      30초로 확대하여 gap 발생 빈도를 1/3로 감소.
             * Trade-off: 비정상 전원 차단 시 최대 30초분 데이터 유실 (정상 종료 시 영향 없음). */
            if (++fsync_counter >= 300) {
                fsync_counter = 0;
                uint32_t dwt_before = IOIF_DWT_GetCycles();
                AGRBFileSystem.sync(s_currentFileId, NULL);
                uint32_t dwt_after  = IOIF_DWT_GetCycles();
                uint32_t dur_us = IOIF_DWT_CyclesToUs(dwt_after - dwt_before);
                s_fsync_last_us = dur_us;
                if (dur_us > s_fsync_max_us) s_fsync_max_us = dur_us;
                s_stats.sync_count++;
            }

            /* 디스크 모니터링 */
            _UpdateDiskFreeCache();

            /* DISK_LOW 경고 */
            if (s_cached_disk_free_mb > 0 && s_cached_disk_free_mb < DISK_LOW_THRESHOLD_MB) {
                atomic_store(&s_logStatus, LOG_STATUS_WARNING_DISK_LOW);
            } else {
                /* 경고 해제 시 LOGGING으로 복귀 (WARNING_QUEUE_FULL이 아닌 경우) */
                DataLogger_Status_e cur = (DataLogger_Status_e)atomic_load(&s_logStatus);
                if (cur == LOG_STATUS_WARNING_DISK_LOW) {
                    atomic_store(&s_logStatus, LOG_STATUS_LOGGING);
                }
            }
        } else {
            fsync_counter = 0;
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
