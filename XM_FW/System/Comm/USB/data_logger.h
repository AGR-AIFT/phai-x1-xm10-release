/**
 ******************************************************************************
 * @file    data_logger.h
 * @author  HyundoKim
 * @brief   실시간 Binary 데이터 로깅 서비스 (Facade API)
 * @details UserTask가 DataLogger_Log()를 호출하면,
 * 별도의 저순위 태스크(DataLoggerTask)가 USB에 씁니다.
 *
 * [양산 품질 업그레이드]
 * - Self-describing binary format: 32B Header + 12B Footer per .bin file
 * - Block CRC32: 4KB 블록마다 HW CRC32 4-byte append (partial recovery 가능)
 * - Emergency Stop: 5종 StopReason + deferred stop (UserTask FATFS 접근 방지)
 * - Event Marker: 로깅 스트림 내 이벤트 마커 삽입 (12B)
 * - Disk Monitoring: 10초 주기 디스크 용량 캐시 + LOW 경고
 * - Auto Session Naming: boot count 기반 B%03lu_%03lu 자동 넘버링
 *
 * @version 1.0
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
 * @brief 1회 쓰기(Log)의 최대 크기 (타임스탬프 제외한 User payload 부분).
 */
#define MAX_LOG_PACKET_SIZE     (512)

/**
 * @brief 자동 타임스탬프 헤더 크기 (활성화 시 매 패킷 앞에 4바이트 tick 자동 삽입)
 */
#define LOG_TIMESTAMP_SIZE      (sizeof(uint32_t))

/* ===== Self-Describing Binary Format (양산 품질) ===== */

/** @brief .bin 파일 매직넘버: 0xA1 'L' 'O' 'G' */
#define LOG_FILE_MAGIC              0xA14C4F47U
/** @brief 파일 포맷 버전 */
#define LOG_FILE_FORMAT_VERSION     1
/** @brief .bin 파일 풋터 매직넘버 (헤더 역순) */
#define LOG_FILE_FOOTER_MAGIC       0x474F4CA1U

/** @brief 헤더 flags 비트 정의 */
#define LOG_FILE_FLAG_AUTO_TS       (1 << 0)
#define LOG_FILE_FLAG_BLOCK_CRC     (1 << 1)

/** @brief 이벤트 마커 식별: packetSize 상위 24비트가 0xFFFFFF이면 마커 */
#define LOG_MARKER_MAGIC            0xFFFFFF00U

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 로거의 현재 상태 (xm_api_usb.h의 XmLogStatus_e와 1:1 대응)
 */
typedef enum {
    LOG_STATUS_IDLE,
    LOG_STATUS_LOGGING,
    LOG_STATUS_WARNING_QUEUE_FULL,  /**< 링 버퍼 90% 이상 */
    LOG_STATUS_WARNING_DISK_LOW,    /**< 디스크 잔여 50MB 미만 */
    LOG_STATUS_ERROR_STOPPED,       /**< 오류로 로깅 강제 중단 */
} DataLogger_Status_e;

/**
 * @brief 로깅 세션 통계 (확장)
 */
typedef struct {
    uint32_t total_bytes_written;
    uint32_t total_packets_logged;
    uint32_t dropped_packets;
    uint32_t write_errors;
    uint32_t sync_count;
    uint32_t start_tick;
    uint32_t end_tick;
    /* 확장 진단 필드 */
    uint8_t  hot_buffer_peak_percent;   /**< 링 버퍼 피크 사용률 (0~100) */
    uint32_t disk_free_mb;              /**< USB 잔여 용량 (MB, 캐시) */
    uint32_t disk_total_mb;             /**< USB 전체 용량 (MB, 캐시) */
} DataLogger_Stats_t;

/**
 * @brief .bin 파일 헤더 (32 bytes, self-describing format)
 * @details 각 .bin 파일 시작에 기록. metadata.txt 없이도 디코더가 레코드 크기 파악 가능.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;              /**< LOG_FILE_MAGIC (0xA14C4F47) */
    uint8_t  format_version;     /**< LOG_FILE_FORMAT_VERSION */
    uint8_t  flags;              /**< bit0: auto_timestamp, bit1: has_block_crc */
    uint16_t header_size;        /**< sizeof(DataLogFileHeader_t) — forward compat */
    uint32_t record_total_bytes; /**< header(4) + ts(0|4) + user_payload */
    uint32_t user_payload_bytes; /**< user struct size */
    uint32_t creation_fattime;   /**< 0 (Rev1.1 — RTC 없음) */
    uint32_t reserved[2];        /**< 향후 확장 */
} DataLogFileHeader_t;           /* 32 bytes */

/**
 * @brief .bin 파일 풋터 (12 bytes)
 * @details 파일 정상 종료 시 기록. 풋터 없음 = 비정상 종료.
 *          data_bytes는 순수 레코드 데이터만 포함 (CRC 오버헤드 제외).
 */
typedef struct __attribute__((packed)) {
    uint32_t record_count;       /**< 이 파트 파일의 레코드 수 */
    uint32_t data_bytes;         /**< 순수 데이터 바이트 수 (CRC/헤더/풋터 제외) */
    uint32_t footer_magic;       /**< LOG_FILE_FOOTER_MAGIC (0x474F4CA1) */
} DataLogFileFooter_t;           /* 12 bytes */

/**
 * @brief 이벤트 마커 레코드 (12 bytes)
 * @details Hot Buffer에 일반 데이터와 동일 경로로 삽입됨.
 *          LogPacketHeader_t.packetSize = LOG_MARKER_MAGIC | marker_type 로 식별.
 *          디코더: packetSize & 0xFFFFFF00 == 0xFFFFFF00 → 마커 레코드.
 */
typedef struct __attribute__((packed)) {
    uint32_t marker_header;   /**< LOG_MARKER_MAGIC | marker_type */
    uint32_t tick_ms;         /**< 이벤트 발생 시각 (xTaskGetTickCount) */
    uint16_t marker_data;     /**< context (에러 코드, 모드 ID 등) */
    uint16_t reserved;
} LogMarkerRecord_t;          /* 12 bytes */

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
 * @param[in] sessionName 저장할 폴더명 (예: "S_001"). NULL 시 자동 넘버링 (B%03lu_%03lu).
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
 * @brief [실시간] 1ms UserTask에서 로그 데이터를 전송합니다.
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

/**
 * @brief 로깅 통계를 반환합니다.
 * @param[out] stats 통계 정보가 복사될 구조체 포인터
 */
void DataLogger_GetStats(DataLogger_Stats_t* stats);

/**
 * @brief 직전 f_sync() 소요 시간을 반환합니다 (DWT 측정).
 * @return 소요 시간 [µs]. f_sync 미실행 시 0.
 */
uint32_t DataLogger_GetLastFsyncUs(void);

/**
 * @brief 세션 내 f_sync() 최대 소요 시간을 반환합니다.
 */
uint32_t DataLogger_GetMaxFsyncUs(void);

/**
 * @brief User 패킷 크기를 등록합니다 (메타데이터 자동 생성에 사용).
 * @details XM_SetUsbLogSource() 호출 시 내부적으로 호출됩니다.
 */
void DataLogger_SetPacketSize(uint32_t size);

/**
 * @brief 자동 타임스탬프 삽입을 활성화/비활성화합니다.
 * @param[in] enabled true: 매 패킷 앞에 4-byte tick_ms 자동 삽입 (기본값: true)
 * @note User 구조체에 이미 tick을 포함하는 경우 false로 설정
 */
void DataLogger_SetAutoTimestamp(bool enabled);

/**
 * @brief 파일 롤링 크기를 런타임에 설정합니다.
 * @param[in] size_mb 파일 롤링 크기 (MB). 최소 1, 최대 100. 기본값: 10.
 */
void DataLogger_SetRollingSize(uint32_t size_mb);

/**
 * @brief [실시간] 이벤트 마커를 로그 스트림에 삽입합니다.
 * @param[in] marker_type 마커 타입 (0x01~0xFF)
 * @param[in] data        컨텍스트 데이터 (에러 코드, 모드 ID 등)
 * @return true: 성공, false: 로깅 비활성 또는 버퍼 부족
 * @note UserTask(1ms) 컨텍스트에서 안전하게 호출 가능 (Non-blocking).
 */
bool DataLogger_InsertMarker(uint8_t marker_type, uint16_t data);

/**
 * @brief 캐시된 USB 디스크 잔여 용량(MB) 반환.
 * @return 잔여 용량 (MB). USB 미연결 시 0.
 */
uint32_t DataLogger_GetDiskFreeMB(void);

/**
 * @brief 캐시된 USB 디스크 전체 용량(MB) 반환.
 * @return 전체 용량 (MB). USB 미연결 시 0.
 */
uint32_t DataLogger_GetDiskTotalMB(void);

#endif /* SYSTEM_COMM_USB_DATA_LOGGER_H_ */
