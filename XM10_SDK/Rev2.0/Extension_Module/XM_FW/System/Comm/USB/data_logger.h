/**
 ******************************************************************************
 * @file    data_logger.h
 * @author  HyundoKim
 * @brief   실시간 Binary 데이터 로깅 서비스 (USB 메모리)
 * @details 제어 주기에서 DataLogger_Log() 로 넘긴 데이터를 내부 버퍼에 담아 두고,
 * 백그라운드에서 USB 메모리에 씁니다.
 * @version 0.1
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

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS (.bin 파일 형식)
 *-----------------------------------------------------------
 */

/** @brief .bin 파일 매직넘버: 0xA1 'L' 'O' 'G' */
#define LOG_FILE_MAGIC              0xA14C4F47
/** @brief 파일 포맷 버전
 *    v1: data block = 4096B + CRC 4B = 4100B (섹터 경계에 맞지 않음)
 *    v2: data block = 4092B + CRC 4B = 4096B (섹터 경계에 맞음) */
#define LOG_FILE_FORMAT_VERSION     2
/** @brief .bin 파일 풋터 매직넘버 (헤더 역순) */
#define LOG_FILE_FOOTER_MAGIC       0x474F4CA1

/** @brief 헤더 flags 비트 정의 */
#define LOG_FILE_FLAG_AUTO_TIMESTAMP  (1 << 0)
#define LOG_FILE_FLAG_BLOCK_CRC       (1 << 1)

/** @brief 이벤트 마커 매직 (packetSize 상위 24비트로 마커 식별) */
#define LOG_MARKER_MAGIC              0xFFFFFF00

/**
 * @brief 마커 종류의 상한 — 0xFF 는 쓰지 않는다.
 * @details 종류 0xFF 의 마커 헤더는 0xFFFFFFFF(모든 비트가 1 — 지워진 칸 · -1 값과 같은 모양)가 되어 PC 디코더가 마커로 읽지 않는다.
 *          DataLogger_InsertMarker 가 이보다 큰 종류를 거절한다.
 */
#define LOG_MARKER_TYPE_MAX           (0xFEU)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 로거의 현재 상태
 * @details 이 enum 은 data_logger.h 가 정의합니다. 시스템 내부(USB LED 표시)만 같은 값으로 읽으며 SDK API 가 아닙니다.
 */
typedef enum {
    LOG_STATUS_IDLE,
    LOG_STATUS_LOGGING,
    LOG_STATUS_WARNING_QUEUE_FULL,
    LOG_STATUS_WARNING_DISK_LOW,     /**< 디스크 잔여 50MB 미만 */
    LOG_STATUS_ERROR_STOPPED,
} DataLogger_Status_e;

typedef struct {
    uint32_t total_bytes_written;
    uint32_t total_packets_logged;
    uint32_t dropped_packets;
    uint32_t write_errors;
    uint32_t sync_count;
    uint32_t start_tick;
    uint32_t end_tick;
    /* 확장 진단 필드 */
    uint8_t  hot_buffer_peak_percent;   /**< 내부 버퍼 최대 사용률 (0~100) */
    uint8_t  cold_buffer_peak_percent;  /**< 쓰지 않음(항상 0) — 예전 구조와 호환용 */
    uint32_t disk_free_mb;              /**< USB 잔여 용량 (MB, 캐시) */
    uint32_t disk_total_mb;             /**< USB 전체 용량 (MB) */
    uint32_t psram_write_retries;       /**< 쓰지 않음(항상 0) — 예전 구조와 호환용 */
    /* USB 쓰기 진단 */
    uint32_t usbh_polling_entries;      /**< USB 쓰기 완료 대기 루프 진입 횟수 */
    uint32_t usbh_polling_iters_max;    /**< 대기 루프 1회의 최대 반복 수 */
    uint32_t usbh_write_us_max;         /**< USB 쓰기 1회 최대 시간 [µs] */
    uint64_t usbh_write_us_sum;         /**< USB 쓰기 누적 시간 [µs] */
    /* CAN-FD 버스 통계 (세션 끝 값) */
    uint8_t  fdcan_rec_end;             /**< 세션 종료 시 Rx Error Counter (0~255) */
    uint8_t  fdcan_tec_end;             /**< 세션 종료 시 Tx Error Counter (0~255) */
    uint8_t  fdcan_lec_last;            /**< 마지막 Last Error Code (0=None~7) */
    uint8_t  fdcan_rx_fifo0_fill_max;   /**< RxFIFO0 최대 fill (overflow 근접 지표) */
    uint32_t fdcan_bus_off_events;      /**< Bus-off 전이 발생 횟수 */
    /* offload_* 두 필드는 쓰지 않음(항상 0) — 예전 구조와 호환용 */
    uint32_t offload_success_count;     /**< 쓰지 않음(항상 0) */
    uint32_t offload_psram_errors;      /**< 쓰지 않음(항상 0) */
    uint32_t usb_write_errors;          /**< USB 메모리 쓰기(f_write) 실패 횟수 */
} DataLogger_Stats_t;

/**
 * @brief .bin 파일 헤더 (28 bytes, self-describing format)
 * @details 각 .bin 파일 시작에 기록. metadata.txt 없이도 디코더가 레코드 크기 파악 가능.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;              /**< LOG_FILE_MAGIC (0xA14C4F47) */
    uint8_t  format_version;     /**< LOG_FILE_FORMAT_VERSION */
    uint8_t  flags;              /**< bit0: auto_timestamp, bit1: has_block_crc */
    uint16_t header_size;        /**< sizeof(DataLogFileHeader_t) — forward compat */
    uint32_t record_total_bytes; /**< header(4) + ts(0|4) + user_payload */
    uint32_t user_payload_bytes; /**< user struct size */
    uint32_t creation_fattime;   /**< FATFS DWORD timestamp at file creation */
    uint32_t reserved[2];        /**< 향후 확장 (block_size 등) */
} DataLogFileHeader_t;           /* 28 bytes (4+1+1+2+4+4+4+8) */

/**
 * @brief .bin 파일 풋터 (12 bytes)
 * @details 파일 정상 종료 시 기록. 풋터 없음 = 비정상 종료.
 */
typedef struct __attribute__((packed)) {
    uint32_t record_count;       /**< 이 파트 파일의 레코드 수 */
    uint32_t data_bytes;         /**< 데이터 바이트 수 (헤더/풋터 제외) */
    uint32_t footer_magic;       /**< LOG_FILE_FOOTER_MAGIC (0x474F4CA1) */
} DataLogFileFooter_t;           /* 12 bytes */

/**
 * @brief 이벤트 마커 레코드 (12 bytes)
 * @details 일반 데이터와 같은 경로로 기록됩니다.
 *          LogPacketHeader_t.packetSize = LOG_MARKER_MAGIC | marker_type 로 식별.
 *          디코더: packetSize 상위 24비트 == 0xFFFFFF → 마커 레코드.
 */
typedef struct __attribute__((packed)) {
    uint32_t marker_header;   /**< LOG_MARKER_MAGIC | marker_type */
    uint32_t tick_ms;         /**< 이벤트 발생 시각 (osKernelGetTickCount) */
    uint16_t marker_data;     /**< context (에러 코드, 모드 ID 등) */
    uint16_t reserved;
} LogMarkerRecord_t;          /* 12 bytes */

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(EXTERN)
 *-----------------------------------------------------------
 */

extern osMessageQueueId_t g_logCmdQueue; // 로거 명령 큐 (내부용)

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief 데이터 로거 서비스를 초기화하고 백그라운드 저장 태스크를 만듭니다.
 * @details 시스템 시작 때 한 번 호출됩니다. 사용자 코드에서는 부르지 않습니다.
 */
void DataLogger_Init(void);

/**
 * @brief USB 저장 장치(MSC)가 연결되고 준비되었는지 확인합니다.
 * @return 준비되었으면 true, 아니면 false.
 */
bool DataLogger_IsReady(void);

/**
 * @brief [비실시간] 데이터 로깅 세션을 시작합니다. 기다리지 않고 바로 돌아옵니다.
 * @details 폴더 생성 · 메타데이터 저장은 백그라운드에서 이어서 하므로 true 는 "시작을 접수했다" 는 뜻입니다.
 *          다음 경우에는 접수하지 않고 false 를 돌려줍니다: 세션이 기록 중이거나 이전 시작이 아직 처리 중임 ·
 *          USB 저장 장치가 준비되지 않음(DataLogger_IsReady() 가 false) · 사용자 구조체 크기를 정하지 않았거나 너무 큼
 *          (내부 버퍼 여유가 모자라는 경우 포함) · metadata 가 NULL 이거나 너무 김 · 세션 이름이 잘못됨.
 *          접수한 뒤 백그라운드에서 실패하면(저장 공간 부족 등) 세션이 열리지 않을 뿐 이 함수의 반환값은 바뀌지 않습니다.
 *          구조체 크기 · 자동 타임스탬프 · 롤링 크기는 이 호출 시점의 값이 세션 내내 쓰입니다.
 * @param[in] sessionName 저장할 폴더명 (예: "S_001"). NULL 이거나 "" 이면 RTC 시각(YYMMDD_HHMMSS)으로 만듭니다.
 * @param[in] metadata    데이터 구조를 설명하는 텍스트 (파일로 저장됨). NULL 이면 안 됩니다.
 * @return true: 시작을 접수했습니다. false: 접수하지 않았습니다.
 */
bool DataLogger_Start(const char* sessionName, const char* metadata);

/**
 * @brief [비실시간] 데이터 로깅을 중지합니다. 기다리지 않고 바로 돌아옵니다.
 * @details 파일 닫기는 백그라운드에서 이어서 합니다. 시작을 접수했지만 백그라운드가 아직 처리를 시작하기 전이면 그 시작을 취소합니다
 *          (파일을 만들지 않습니다). 이미 파일을 만드는 중이었다면 만든 뒤 곧바로 닫으므로 기록이 없는 세션이 남습니다.
 *          기록 차단과 시작 취소 표시는 호출 즉시 적용되고, 파일을 닫으라는 명령만 큐에 넣습니다(기다리지 않음).
 * @return true: 중지 명령을 큐에 넣었습니다. false: 큐가 가득 차 넣지 못했습니다 — 기록 차단은 이미 적용됐으므로 다시 부릅니다.
 */
bool DataLogger_Stop(void);

/**
 * @brief [실시간] 제어 주기마다 로그 데이터 1개를 기록합니다.
 * @details 이 함수는 비차단(Non-blocking)이며, 데이터를 내부 버퍼에 복사합니다. 기록하지 않았으면 false 를 돌려줍니다:
 *          기록 중이 아님(시작 전 · 시작을 처리하는 중 · 중지 뒤) ·
 *          packetSize 가 이번 세션의 레코드 크기(시작할 때 정해진 사용자 구조체 크기)와 다름 ·
 *          1 kHz 보다 낮은 주기로 저장하는 세션에서 이번 주기 칸에 이미 기록함 ·
 *          내부 버퍼가 가득 참(이 경우 세션은 오류로 중지됩니다).
 * @param[in] logPacket  저장할 사용자 정의 struct 포인터 (void*)
 * @param[in] packetSize logPacket의 크기 (예: sizeof(MyLogData_t))
 * @return true: 기록했습니다. false: 기록하지 않았습니다 (위 경우).
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
 * @brief 사용자 구조체(레코드 하나의 사용자 데이터) 크기를 등록합니다 (1 ~ MAX_LOG_PACKET_SIZE).
 * @param[in] size 구조체 크기 [B]
 * @note 다음 DataLogger_Start 부터 적용됩니다. 이미 접수했거나 기록 중인 세션의 레코드 크기는 바뀌지 않습니다.
 */
void DataLogger_SetPacketSize(uint32_t size);

/**
 * @brief 자동 타임스탬프 삽입을 활성화/비활성화합니다.
 * @param[in] enabled true: 매 패킷 앞에 4-byte tick_ms 자동 삽입 (기본값: true)
 * @note 다음 DataLogger_Start 부터 적용됩니다. 이미 접수했거나 기록 중인 세션은 그대로입니다.
 *       User 구조체에 이미 tick을 포함하는 경우 false로 설정
 */
void DataLogger_SetAutoTimestamp(bool enabled);

/**
 * @brief 파일 롤링 크기를 설정합니다.
 * @param[in] size_mb 파일 롤링 크기 (MB). 최소 1, 최대 100(범위 밖의 값은 무시). 기본값: 10.
 * @note 다음 DataLogger_Start 부터 적용됩니다. 이미 접수했거나 기록 중인 세션은 그대로입니다.
 */
void DataLogger_SetRollingSize(uint32_t size_mb);

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
 * @brief FATFS get_fattime() 용 캐시된 RTC 타임스탬프 반환.
 * @return FATFS DWORD 포맷 타임스탬프 (0이면 미초기화)
 * @note fatfs.c의 get_fattime()에서 호출. 10초 주기로 RTC에서 자동 갱신.
 */
uint32_t DataLogger_GetCachedFatTime(void);

/**
 * @brief 캐시된 USB 디스크 잔여 용량(MB) 반환.
 * @return 잔여 용량 (MB). USB 미연결 시 0.
 */
uint32_t DataLogger_GetDiskFreeMB(void);

/**
 * @brief USB 디스크 전체 용량(MB) 반환.
 * @return 전체 용량 (MB). USB 미연결 시 0.
 */
uint32_t DataLogger_GetDiskTotalMB(void);

/**
 * @brief 이벤트 마커를 기록합니다.
 * @param[in] marker_type 마커 타입 (0x01~LOG_MARKER_TYPE_MAX = 0xFE — 0xFF 는 디코더가 마커로 읽지 않아 거절합니다)
 * @param[in] data        컨텍스트 데이터 (에러 코드, 모드 ID 등)
 * @return true: 성공, false: 로깅 비활성 · 버퍼 부족 · 허용하지 않는 마커 타입(0xFF)
 * @note 제어 주기(Control_Loop)에서 불러도 됩니다 (비차단).
 */
bool DataLogger_InsertMarker(uint8_t marker_type, uint16_t data);

/**
 * @brief 현재(또는 마지막) 세션 이름을 반환합니다.
 * @details 오류로 멈춘 뒤 같은 세션 폴더에 이어 쓰려면 이 이름을 저장해 두었다가 DataLogger_Start() 에 다시 넘깁니다.
 *          같은 폴더에 data_001_* 처럼 다음 번호의 파일이 생깁니다(저장 주기 · 레코드 크기가 같아야 하고,
 *          metadata.txt 는 처음 것을 그대로 씁니다 — 새 metadata 와 비교하지 않습니다).
 * @param[out] out_buf  세션 이름이 복사될 버퍼 (문자열)
 * @param[in]  buf_size 버퍼 크기 (세션 이름은 100자까지이므로 101 이상 권장)
 * @note 활성 세션이 없으면 빈 문자열("")을 반환합니다.
 */
void DataLogger_GetActiveSessionName(char* out_buf, uint32_t buf_size);

#endif /* SYSTEM_COMM_USB_DATA_LOGGER_H_ */
