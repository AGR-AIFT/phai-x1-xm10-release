/**
 ******************************************************************************
 * @file    agr_dop_types.h
 * @author  HyundoKim
 * @brief   AGR-DOP V2 Core Type Definitions
 * @version 2.0
 * @date    Dec 2, 2025
 *
 * @details
 * AGR-DOP (Angel Robotics Data Object Protocol)은 CANopen 표준(CiA 301/402)을
 * 참고하여 설계된 경량 통신 프로토콜입니다.
 * 
 * [OD Index 관리 정책]
 * - OD Index는 **노드(모듈) 단위**로 관리됩니다.
 * - 같은 Index(예: 0x6000)가 다른 모듈에서 다른 의미로 사용될 수 있습니다.
 * - 통신 시 **Node ID**로 대상을 구분합니다.
 * 
 * [Index 범위 가이드라인] (CANopen 호환)
 * - 0x1000 ~ 0x1FFF : Communication Profile (표준 통신 파라미터)
 * - 0x2000 ~ 0x5FFF : Manufacturer Specific (제조사 전용)
 * - 0x6000 ~ 0x9FFF : Device Profile (장치별 데이터)
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_TYPES_H
#define AGR_DOP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "agr_dop_config.h"  /* AGR_PDO_MAP_MAX_ENTRIES 등 config define 참조 */

/**
 *-----------------------------------------------------------
 * CONFIGURATION
 *-----------------------------------------------------------
 */

/** @brief CANFD 최대 페이로드 크기 (64 bytes) */
#define AGR_CANFD_MAX_PAYLOAD       64

/**
 * @brief SDO 데이터 버퍼 크기 (RAM knob). 기본 60.
 * @note  와이어 size 필드는 uint16(≤65535) 이지만 실제 버퍼는 이 값으로 제한.
 *        UDP/Serial 대용량이 필요한 모듈은 agr_mw_conf.h 에서 override.
 *        수신 시 declared > 이 값 → abort + diag (oversize).
 *        주의: 0x21/0x41 헤더는 6B → CAN-FD 단일프레임 data 실효 상한 = 64-6 = 58B.
 */
#ifndef AGR_SDO_MAX_DATA_SIZE
#define AGR_SDO_MAX_DATA_SIZE       60
#endif

/**
 * @brief SDO Master 동시 in-flight transaction 풀 크기 (per context).
 * @note  Master(요청 개시) 역할 ctx 만 실제 사용. Slave-only 모듈은
 *        agr_mw_conf.h 에서 1 로 override 하여 RAM 절약 가능
 *        (ctx 에 풀이 임베드되므로). 기본 8 = 1 master 가 다수 slave 에
 *        병렬 요청(각 slave 1-in-flight) 하는 케이스 커버.
 */
#ifndef AGR_SDO_MASTER_MAX_PENDING
#define AGR_SDO_MASTER_MAX_PENDING  8
#endif

/* AGR_PDO_MAP_MAX_ENTRIES: agr_dop_config.h에서 정의 (default 32, agr_mw_conf.h에서 override 가능) */

/* AGR_OD_MAX_ENTRIES: agr_dop_config.h에서 정의 (512, 참고 상한 — 버퍼 사이징에 미사용) */

/**
 *-----------------------------------------------------------
 * DATA TYPES
 *-----------------------------------------------------------
 */

typedef enum {
    AGR_TYPE_UINT8   = 0,   /**< Unsigned 8-bit integer */
    AGR_TYPE_UINT16  = 1,   /**< Unsigned 16-bit integer */
    AGR_TYPE_UINT32  = 2,   /**< Unsigned 32-bit integer */
    AGR_TYPE_INT8    = 3,   /**< Signed 8-bit integer */
    AGR_TYPE_INT16   = 4,   /**< Signed 16-bit integer */
    AGR_TYPE_INT32   = 5,   /**< Signed 32-bit integer */
    AGR_TYPE_FLOAT32 = 6,   /**< 32-bit floating point */
    AGR_TYPE_BLOB    = 7,   /**< Variable length binary data */
} AGR_DataType_t;

typedef enum {
    AGR_ACCESS_RO = 1,      /**< Read Only */
    AGR_ACCESS_WO = 2,      /**< Write Only */
    AGR_ACCESS_RW = 3,      /**< Read/Write */
} AGR_Access_t;

/**
 *-----------------------------------------------------------
 * OD ENTRY (Object Dictionary Entry)
 *-----------------------------------------------------------
 * @brief 하나의 데이터 객체를 정의합니다.
 * @note  각 드라이버(xxx_drv.c)에서 static const 배열로 정의합니다.
 * 
 * @example
 * ```c
 * static int16_t s_imu_acc_x;
 * 
 * static const AGR_OD_Entry_t s_od_entries[] = {
 *     // Index,  SubIdx, Type,            Size, Access,       DataPtr,        WriteCb,  Name,          Unit
 *     { 0x6000,  0x00,   AGR_TYPE_INT16,  2,    AGR_ACCESS_RO, &s_imu_acc_x,  NULL,     "accel_x",     "m/s2" },
 * };
 * ```
 */
typedef struct {
    uint16_t       index;       /**< Object Index (0x0000 ~ 0xFFFF) */
    uint8_t        subindex;    /**< Sub-Index (0x00 ~ 0xFF, 보통 0x00 사용) */
    AGR_DataType_t type;        /**< 데이터 타입 */
    uint8_t        size;        /**< 바이트 크기 (스칼라 1/2/4, BLOB 는 가변) */
    AGR_Access_t   access;      /**< 접근 권한 */
    void*          data_ptr;    /**< 바인딩된 변수 주소 */
    void (*on_write)(void);     /**< 쓰기 완료 시 콜백 (Optional, NULL 가능) */
    const char*    name;        /**< Human-readable 이름 (OD Discovery, NULL 허용) */
    const char*    unit;        /**< 물리 단위 문자열 (OD Discovery, NULL 허용) */
} AGR_OD_Entry_t;

/**
 *-----------------------------------------------------------
 * OD TABLE (Object Dictionary Table)
 *-----------------------------------------------------------
 * @brief OD Entry 배열과 메타데이터를 묶은 구조체
 * @note  드라이버 초기화 시 한 번 설정하면 변경되지 않습니다.
 */
typedef struct {
    const AGR_OD_Entry_t* entries;      /**< OD Entry 배열 포인터 (const) */
    uint16_t              entry_count;  /**< Entry 개수 */
    const uint16_t*       sorted_idx;   /**< 정렬 인덱스 배열 (Optional). NULL = 선형 탐색.
                                             AGR_OD_BuildSortedIndex()로만 설정 (직접 대입 금지).
                                             zero-init 호환 — 기존 모듈은 무변경으로 선형 유지 */
    uint16_t*             sort_buf;     /**< (Optional) 호출자 제공 정렬 버퍼 (수명=od, 원소>=entry_count).
                                             NULL 아니면 transport Init(AGR_CANFD/Serial_Init)이
                                             AGR_OD_BuildSortedIndex 를 자동 실행(lookup O(log n) +
                                             중복 index 검출). AGR_OD_TABLE_WITH_SORT 매크로 권장.
                                             zero-init 호환 — 기존/Master(od=NULL) 무영향. */
} AGR_OD_Table_t;

/**
 *-----------------------------------------------------------
 * PDO MAP ENTRY (정적 PDO 매핑 - 컴파일 타임 정의)
 *-----------------------------------------------------------
 * @brief PDO 페이로드 내 하나의 신호를 정의합니다.
 * @note  고정된 PDO 구조를 정의할 때 사용합니다.
 */
typedef struct {
    uint16_t od_index;      /**< 매핑된 OD Entry의 Index */
    uint8_t  od_subindex;   /**< 매핑된 OD Entry의 Sub-Index */
    uint16_t byte_offset;   /**< 페이로드 내 바이트 오프셋 (uint16: USB/EtherCAT 大 PDO >255B 지원).
                             *   런타임 encode 는 entry 순서로 누적 — 본 필드는 문서/정렬 참조용. */
    uint8_t  byte_length;   /**< 바이트 길이 (1, 2, 4) */
} AGR_PDO_MapEntry_t;

/**
 *-----------------------------------------------------------
 * PDO DEFINITION (정적 PDO 정의 - 컴파일 타임)
 *-----------------------------------------------------------
 * @brief 송신/수신 PDO의 정적 정의
 */
typedef struct {
    uint32_t                   cob_id;        /**< CAN Object ID (11-bit or 29-bit) */
    const AGR_PDO_MapEntry_t*  map_entries;   /**< 매핑 엔트리 배열 (const) */
    uint8_t                    map_count;     /**< 매핑 개수 (≤255) */
    uint16_t                   payload_size;  /**< 총 페이로드 바이트 크기 (uint16: USB/EtherCAT 大 PDO >255B.
                                               *   CAN-FD 는 transport 가 64B enforce). 런타임 미참조 — 정보용. */
} AGR_PDO_Def_t;

/**
 *-----------------------------------------------------------
 * PDO MAPPING TABLE (동적 PDO 매핑 - 런타임 설정)
 *-----------------------------------------------------------
 * @brief Master가 SetPDOMapping SDO로 런타임에 설정하는 PDO 리스트
 * @details
 * - Master가 Slave(CM, IMU/EMG/FES Hub)에게 "이 OD Entry들을 PDO로 보내라"고 요청
 * - Slave는 이 테이블에 따라 PDO 페이로드를 구성하여 전송
 * - Wire format (SDO 0x1A00.xx) 은 Legacy 4B (CiA 301) / Extended 8B (AGR) 양쪽을 지원.
 *   자세한 포맷은 docs/pdo_mapping_format.md 와 agr_pdo_engine.h 참조.
 * - 런타임 내부 보관은 항상 AGR_PDO_MapItem_t 로 통일.
 *
 * @example
 * ```c
 * // Master 에서 Slave 로 TPDO 매핑 요청 (Extended 8B, 단일 37B entry)
 * // Header(0x81) + [IdxL, IdxH, Sub, Flags, Len0..Len3]
 * static const uint8_t pdo_list[] = {
 *     0x81,
 *     0x18, 0x70, 0x00, 0x00, 37, 0, 0, 0
 * };
 * AGR_SDO_Write(FES_HUB_NODE_ID, 0x1A00, pdo_list, sizeof(pdo_list));
 * ```
 */
typedef struct {
    uint16_t od_index;       /**< OD Entry Index */
    uint8_t  od_subindex;    /**< OD Entry Sub-Index */
    uint8_t  flags;          /**< bit 0: signed, bit 1..7: reserved */
    uint32_t length_bytes;   /**< 매핑 크기 (bytes), Extended 포맷 */
} AGR_PDO_MapItem_t;

typedef struct {
    AGR_PDO_MapItem_t items[AGR_PDO_MAP_MAX_ENTRIES];  /**< 매핑 항목 배열 */
    uint8_t           count;                           /**< 현재 매핑된 항목 수 */
} AGR_PDO_MapTable_t;

/**
 *-----------------------------------------------------------
 * SDO MESSAGE (요청/응답 구조) - CANopen 표준 준수
 *-----------------------------------------------------------
 * @brief SDO 메시지 구조 (CANFD 지원, 최대 60바이트 데이터)
 * 
 * [SDO 프레임 구조 - CANopen 표준 (CiA 301)]
 * | Byte 0  | Byte 1-2 | Byte 3   | Byte 4~7(CAN) / 4~63(CANFD) |
 * | CS      | Index    | SubIndex | Data                         |
 * 
 * CS (Command Specifier) Bit Fields:
 * - Bit 7-5: Command Code (CCS/SCS)
 * - Bit 4: Reserved (0)
 * - Bit 3-2: n (bytes not containing data, for expedited)
 * - Bit 1: e (expedited transfer, 1=data in frame)
 * - Bit 0: s (size indicator, 1=size is indicated)
 * 
 * @note CANFD 확장 (AGR): >4B 는 expedited(e=1) 가 아니라 **비-expedited(e=0, s=1)
 *       명시 길이** 로 전송합니다. cs=0x21(write)/0x41(read-resp) + byte4-5 의 2B 길이.
 *       DLC 패딩과 무관하게 송신측이 실제 데이터 길이를 명시. (구형 expedited/frame-len 미지원)
 */

/**
 * @brief SDO Command Specifier (CANopen 표준 - CiA 301)
 * @details
 * CCS (Client Command Specifier) - Master → Slave
 * SCS (Server Command Specifier) - Slave → Master
 */
typedef enum {
    /* ===== Download (Write) - Initiate ===== */
    AGR_SDO_CS_DOWNLOAD_INIT_REQ       = 0x20,  /**< [CCS] [DEPRECATED] frame-len Write Req — 미지원(수신 시 abort) */
    AGR_SDO_CS_DOWNLOAD_INIT_REQ_SIZED = 0x21,  /**< [CCS] Write Req — e=0,s=1, byte4-5 의 2B 명시 길이 (AGR 표준) */
    AGR_SDO_CS_DOWNLOAD_INIT_RSP       = 0x60,  /**< [SCS] Download Initiate Response (8B 고정) */

    /* ===== Upload (Read) - Initiate ===== */
    AGR_SDO_CS_UPLOAD_INIT_REQ         = 0x40,  /**< [CCS] Upload Initiate Request (데이터 0) */
    AGR_SDO_CS_UPLOAD_INIT_RSP         = 0x40,  /**< [SCS] Upload Initiate Response (base) */
    AGR_SDO_CS_UPLOAD_INIT_RSP_SIZED   = 0x41,  /**< [SCS] Read Resp — e=0,s=1, byte4-5 의 2B 명시 길이 (AGR 표준) */
    
    /* ===== Abort ===== */
    AGR_SDO_CS_ABORT                = 0x80,  /**< Abort Transfer (Error) */
    
    /* ===== Segmented Transfer (추후 구현) ===== */
    AGR_SDO_CS_DOWNLOAD_SEG_REQ     = 0x00,  /**< [CCS] Download Segment Request */
    AGR_SDO_CS_DOWNLOAD_SEG_RSP     = 0x20,  /**< [SCS] Download Segment Response */
    AGR_SDO_CS_UPLOAD_SEG_REQ       = 0x60,  /**< [CCS] Upload Segment Request */
    AGR_SDO_CS_UPLOAD_SEG_RSP       = 0x00,  /**< [SCS] Upload Segment Response */
    
    /* ===== Block Transfer (추후 구현) ===== */
    AGR_SDO_CS_BLOCK_DOWNLOAD       = 0xC0,  /**< Block Download */
    AGR_SDO_CS_BLOCK_UPLOAD         = 0xA0,  /**< Block Upload */
} AGR_SDO_CS_t;

/**
 * @brief SDO Command Specifier 헬퍼 매크로
 */

/** @brief Expedited Transfer 여부 확인 (e bit) */
#define AGR_SDO_IS_EXPEDITED(cs)    (((cs) & 0x02) != 0)

/** @brief Size Indicator 여부 확인 (s bit) */
#define AGR_SDO_HAS_SIZE(cs)        (((cs) & 0x01) != 0)

/** @brief n 값 추출 (bytes not containing data, 0~3) */
#define AGR_SDO_GET_N(cs)           (((cs) >> 2) & 0x03)

/** @brief [DEPRECATED] Expedited Download — 미지원. 0x21 명시 길이로 통일 (수신 시 abort INVALID_CS) */
#define AGR_SDO_CS_DOWNLOAD_EXP(n)  (0x23 | (((n) & 0x03) << 2))

/** @brief [DEPRECATED] Expedited Upload — 미지원. 0x41 명시 길이로 통일 */
#define AGR_SDO_CS_UPLOAD_EXP(n)    (0x43 | (((n) & 0x03) << 2))

/**
 * @brief SDO 메시지 구조체 (CANopen 표준 호환)
 */
typedef struct {
    uint8_t       cs;                           /**< Command Specifier (CANopen 표준) */
    uint16_t      index;                        /**< Object Index */
    uint8_t       subindex;                     /**< Sub-Index */
    uint8_t       data[AGR_SDO_MAX_DATA_SIZE];  /**< Data (CANFD: 최대 60바이트) */
    uint16_t      data_len;                     /**< 실제 데이터 길이 (uint16: UDP/Serial >255B 대비) */
} AGR_SDO_Msg_t;

/**
 *-----------------------------------------------------------
 * SDO ABORT CODES (에러 코드) - CANopen 표준 (CiA 301)
 *-----------------------------------------------------------
 */
typedef enum {
    AGR_SDO_ABORT_NONE              = 0x00000000,  /**< No error */
    
    /* ===== Protocol Errors (0x0503xxxx, 0x0504xxxx) ===== */
    AGR_SDO_ABORT_TOGGLE_BIT        = 0x05030000,  /**< Toggle bit not alternated */
    AGR_SDO_ABORT_TIMEOUT           = 0x05040000,  /**< SDO protocol timed out */
    AGR_SDO_ABORT_INVALID_CS        = 0x05040001,  /**< Invalid command specifier */
    AGR_SDO_ABORT_INVALID_BLOCK     = 0x05040002,  /**< Invalid block size */
    AGR_SDO_ABORT_INVALID_SEQ       = 0x05040003,  /**< Invalid sequence number */
    AGR_SDO_ABORT_CRC_ERROR         = 0x05040004,  /**< CRC error */
    AGR_SDO_ABORT_OUT_OF_MEMORY     = 0x05040005,  /**< Out of memory */
    
    /* ===== Access Errors (0x0601xxxx) ===== */
    AGR_SDO_ABORT_UNSUPPORTED       = 0x06010000,  /**< Unsupported access to an object */
    AGR_SDO_ABORT_WRITE_ONLY        = 0x06010001,  /**< Attempt to read a write only object */
    AGR_SDO_ABORT_READ_ONLY         = 0x06010002,  /**< Attempt to write a read only object */
    
    /* ===== Object Dictionary Errors (0x0602xxxx) ===== */
    AGR_SDO_ABORT_NOT_EXIST         = 0x06020000,  /**< Object does not exist in OD */
    
    /* ===== PDO Mapping Errors (0x0604xxxx) ===== */
    AGR_SDO_ABORT_NO_MAP            = 0x06040041,  /**< Object cannot be mapped to PDO */
    AGR_SDO_ABORT_MAP_LEN           = 0x06040042,  /**< PDO length exceeded */
    AGR_SDO_ABORT_PARAM_INCOMPAT    = 0x06040043,  /**< General parameter incompatibility */
    AGR_SDO_ABORT_INTERNAL          = 0x06040047,  /**< General internal incompatibility */
    
    /* ===== Hardware Errors (0x0606xxxx) ===== */
    AGR_SDO_ABORT_HW_ERROR          = 0x06060000,  /**< Access failed due to hardware error */
    
    /* ===== Data Type Errors (0x0607xxxx) ===== */
    AGR_SDO_ABORT_TYPE_MISMATCH     = 0x06070010,  /**< Data type mismatch */
    AGR_SDO_ABORT_DATA_LONG         = 0x06070012,  /**< Data type length too high */
    AGR_SDO_ABORT_DATA_SHORT        = 0x06070013,  /**< Data type length too low */
    
    /* ===== Sub-Index Errors (0x0609xxxx) ===== */
    AGR_SDO_ABORT_SUB_UNKNOWN       = 0x06090011,  /**< Sub-index does not exist */
    AGR_SDO_ABORT_VALUE_RANGE       = 0x06090030,  /**< Value range of parameter exceeded */
    AGR_SDO_ABORT_VALUE_HIGH        = 0x06090031,  /**< Value of parameter written too high */
    AGR_SDO_ABORT_VALUE_LOW         = 0x06090032,  /**< Value of parameter written too low */
    AGR_SDO_ABORT_MAX_MIN           = 0x06090036,  /**< Maximum value is less than minimum value */
    
    /* ===== General Errors (0x0800xxxx) ===== */
    AGR_SDO_ABORT_GENERAL           = 0x08000000,  /**< General error */
    AGR_SDO_ABORT_DATA_STORE        = 0x08000020,  /**< Data cannot be transferred or stored to app */
    AGR_SDO_ABORT_DATA_LOCAL        = 0x08000021,  /**< Data cannot be transferred (local control) */
    AGR_SDO_ABORT_DATA_STATE        = 0x08000022,  /**< Data cannot be transferred (device state) */
    AGR_SDO_ABORT_NO_OD             = 0x08000023,  /**< Object dictionary not present */
} AGR_SDO_AbortCode_t;

/**
 *-----------------------------------------------------------
 * SYNC CONTROL (CANFD 동기화 객체)
 *-----------------------------------------------------------
 * @brief SYNC 메시지 제어 구조체 (CANopen 표준)
 * @details
 * SYNC 메시지(CAN-ID: 0x080)는 모든 노드를 동기화하는 브로드캐스트 메시지입니다.
 * Master가 SYNC를 전송하면, Slave들은 동기화된 시점에 PDO를 전송합니다.
 * 
 * [SYNC 사용 시나리오]
 * - Master(XM10)가 1ms마다 SYNC 전송
 * - Slave(IMU Hub)는 SYNC 수신 시 즉시 TPDO 전송
 * - 모든 센서 데이터가 동일한 시간 기준으로 동기화됨
 */
typedef struct {
    bool     sync_enabled;           /**< SYNC 기능 활성화 여부 */
    uint8_t  sync_counter;           /**< SYNC 카운터 (0~255 순환) */
    uint32_t sync_period_us;         /**< SYNC 주기 (μs, Master용) */
    uint32_t last_sync_tick;         /**< 마지막 SYNC 수신 시각 (ms) */
    void (*on_sync)(void* ctx);      /**< SYNC 수신 시 콜백 (ISR에서 호출) */
} AGR_SYNC_Ctrl_t;

/**
 *-----------------------------------------------------------
 * EMERGENCY CONTROL (긴급 메시지)
 *-----------------------------------------------------------
 * @brief Emergency 메시지 제어 구조체 (CANopen 표준)
 * @details
 * Emergency 메시지(CAN-ID: 0x080 + Node ID)는 즉시 처리가 필요한 에러를 알립니다.
 * 
 * [Emergency Error Code 분류]
 * - 0x0000: Error Reset
 * - 0x10xx: Generic Error
 * - 0x20xx: Current Error
 * - 0x30xx: Voltage Error
 * - 0x81xx~0x8Fxx: Manufacturer Specific
 */
typedef struct {
    bool     emergency_enabled;      /**< Emergency 기능 활성화 여부 */
    uint16_t last_error_code;        /**< 마지막 에러 코드 */
    uint8_t  last_error_register;    /**< 마지막 Error Register (OD 0x1001) */
    uint32_t last_emcy_sent_ms;      /**< 마지막 Emergency 전송 시각 */
    /** Emergency 수신 콜백 (CiA 301: error_code + error_register) */
    void (*on_emergency)(uint16_t error_code, uint8_t error_register, void* ctx);
} AGR_EMCY_Ctrl_t;

/**
 *-----------------------------------------------------------
 * PDO INHIBIT TIME CONTROL (전송 간격 제한)
 *-----------------------------------------------------------
 * @brief PDO Inhibit Time 제어 구조체 (CANopen 표준)
 * @details
 * PDO Inhibit Time은 동일한 PDO가 연속으로 전송될 때 최소 간격을 강제합니다.
 * 이를 통해 CAN 버스 부하를 제어하고 과도한 전송을 방지합니다.
 * 
 * [사용 시나리오]
 * - IMU 데이터가 빠르게 변경될 때 (1kHz 샘플링)
 * - PDO Inhibit Time = 1ms 설정 시
 * - 최소 1ms 간격으로만 PDO 전송됨
 * - 버스 부하 감소 및 전송 효율 향상
 * 
 * [Inhibit Time 단위]
 * - CANopen 표준: 100μs 단위 (0~65535 → 0~6.5535초)
 * - AGR-DOP V2: μs 단위로 직접 지정 (더 직관적)
 */
typedef struct {
    bool     inhibit_enabled;        /**< Inhibit Time 기능 활성화 여부 */
    uint32_t inhibit_time_us;        /**< 최소 전송 간격 (μs) */
    uint32_t last_tx_tick_us;        /**< 마지막 전송 시각 (μs) */
} AGR_PDO_Inhibit_t;

/**
 *-----------------------------------------------------------
 * SDO DIAGNOSTICS (수신 거부 / 에러 카운터)
 *-----------------------------------------------------------
 * @brief SDO 수신 디코드 실패 / oversize 관측용. Risk Manager 가 폴링 → EMCY 발행.
 * @note  Decode/Process 가 0 이외 반환 시 transport 가 증가시킴.
 * @note  [ISR↔main 계약] transport 의 Rx 처리(AGR_*_ProcessRxMessage)가 ISR 에서
 *        호출될 수 있고(예: IMU Hub G4 BareMetal — FDCAN Rx ISR 직접 호출), Risk
 *        Manager 는 main/task 에서 폴링한다. 따라서 카운터는 volatile — 가시성 보장용
 *        (증분 원자성은 보장 아님; 단일 ISR-writer + main-reader 전제, Cortex-M aligned
 *        16-bit 접근은 원자적). 소비 모듈이 SDO 를 main 으로 defer(R13: EMG/FES/SAM3x)
 *        하면 ISR-write 는 없으나, lib 은 소비자별 defer 를 가정할 수 없으므로 기본적으로
 *        ISR/main 안전을 보장한다. CMW-08 (2026-06-19 전 소비모듈 전수조사로 확정). */
typedef struct {
    volatile uint16_t sdo_oversize_count;     /**< declared > MAX_DATA_SIZE 로 거부한 횟수 */
    volatile uint16_t sdo_decode_err_count;   /**< 디코드 실패(잘못된 CS / 짧은 프레임) 횟수 */
    volatile uint16_t last_bad_index;         /**< 마지막 거부된 OD index (진단) */
} AGR_DOP_SdoDiag_t;

/**
 *-----------------------------------------------------------
 * SDO MASTER (Client) — Transaction tracking
 *-----------------------------------------------------------
 * @brief Master 측 SDO request 의 응답(write-ack / read-back / abort / timeout)
 *        추적. Function API: agr_sdo_master.h. Design SSOT: docs/plan_sdo_master.md.
 *
 * @note  타입을 (agr_sdo_master.h 가 아니라) 여기 둔 이유: AGR_DOP_Ctx_t 가
 *        AGR_SDO_Master_t 를 by-value 임베드하려면 완전 타입이 보여야 하는데,
 *        agr_sdo_master.h ↔ agr_dop_types.h 순환 include 가 생긴다. SDO 의
 *        다른 타입(AGR_SDO_Msg_t / AGR_SDO_AbortCode_t)도 이미 여기 있으므로
 *        일관. 함수 선언만 agr_sdo_master.h 가 보유.
 *
 * @note  [동시성 계약] Transaction 풀은 단일 컨텍스트 모델: AllocTx/Request 와
 *        Tick 은 같은 컨텍스트(예: comm task)에서 호출. ProcessResponse 가
 *        다른 컨텍스트(예: FDCAN Rx ISR)에서 호출될 수 있으면, 소비자가 응답을
 *        defer 하거나 Tick 과의 상호배타를 보장해야 한다 (AGR_MW 는 RTOS 독립 —
 *        락을 넣지 않음, sdo_diag 와 동일 계약).
 *
 * Callback-only completion, single-in-flight per (master, slave). 자세한
 * rationale(폴링/블로킹 기각)는 agr_sdo_master.h 헤더 주석.
 */
typedef enum {
    AGR_SDO_TX_IDLE = 0,    /**< 빈 슬롯 (free) */
    AGR_SDO_TX_RESERVED,    /**< AllocTx 가 클레임, 아직 미충전/미송신 — ProcessResponse/Tick 가 skip
                                 (half-filled 슬롯 오매칭 방지: PENDING 은 모든 필드 채운 뒤 마지막 store) */
    AGR_SDO_TX_PENDING,     /**< 송신됨, 응답 대기 */
    AGR_SDO_TX_DONE,        /**< 정상 완료 (write-ack 또는 read data) */
    AGR_SDO_TX_ABORTED,     /**< Slave abort 또는 방향/CS 불일치 */
    AGR_SDO_TX_TIMEOUT,     /**< 응답 없음 (timeout) */
} AGR_SDO_Tx_State_e;

struct AGR_SDO_Transaction;  /* fwd-decl for callback typedef */

/**
 * @brief Request 완료 콜백 — per-request 단위 (필수, NULL 금지).
 * @param tx          완료된 transaction (tx->state 로 결과 판정).
 * @param data        read 성공 시 수신 데이터, 그 외 NULL.
 * @param data_len    data 바이트 수.
 * @param abort_code  tx->state==ABORTED 일 때만 유효 (그 외 0).
 * @param user_ctx    Request 시 전달한 caller context.
 *
 * @warning tx 는 콜백 실행 중에만 유효. 콜백 리턴 직후 슬롯이 IDLE 로 반환되어
 *          재사용된다. tx 포인터를 저장하지 말 것 — 필요한 값은 콜백 내에서
 *          caller 소유 저장소로 복사(예: dev->last_state = tx->state). ISR-deferred
 *          패턴(콜백이 main-loop 로 미룸)에서 특히 주의.
 */
typedef void (*AGR_SDO_CompletionCb_t)(struct AGR_SDO_Transaction* tx,
                                       const uint8_t*      data,
                                       uint8_t             data_len,
                                       AGR_SDO_AbortCode_t abort_code,
                                       void*               user_ctx);

typedef struct AGR_SDO_Transaction {
    /* 매칭 필드 (전 transport 공통: slave_id + index + subindex) */
    uint8_t                slave_id;
    uint16_t               index;
    uint8_t                subindex;
    bool                   is_write;

    /* 상태 / 타이밍 — state 는 volatile (ProcessResponse 가 ISR, Tick/Alloc 가 main
     * 컨텍스트일 수 있어 가시성 보장; sdo_diag 와 동일 계약. 원자성/락은 비보장 —
     * 동시성 계약은 위 구조체 주석 참조). */
    volatile AGR_SDO_Tx_State_e state;
    uint32_t               sent_tick_ms;
    uint32_t               timeout_ms;

    /* 완료 통지 (callback-only) */
    AGR_SDO_CompletionCb_t on_done;
    void*                  user_ctx;

    /* 데이터 (read 결과) */
    uint8_t                data[AGR_SDO_MAX_DATA_SIZE];
    uint8_t                data_len;
    AGR_SDO_AbortCode_t    abort_code;
} AGR_SDO_Transaction_t;

typedef struct {
    AGR_SDO_Transaction_t pool[AGR_SDO_MASTER_MAX_PENDING];
    uint32_t            (*get_tick_ms)(void);   /**< DI tick source (timeout 계측) */
} AGR_SDO_Master_t;

/**
 *-----------------------------------------------------------
 * COMMAND VECTOR — deterministic 제어입력 채널
 *-----------------------------------------------------------
 * @brief P/F/I trajectory, ES vector 등 "제어 입력" 클래스 메시지 전용 와이어.
 * @details
 * Request(0x100+Node, fnc 0x02): [vector_type u8][seq u8][len u8][payload…]
 * ACK    (0x680+Node, fnc 0x0D): [echo_seq u8][status u8]  (2B 고정)
 * EVENT  (0x680+Node, fnc 0x0D): [event_code u8]           (1B, code = 0x80|type)
 *
 * SDO(main-loop deferred)와 달리 단일 atomic 프레임을 소비 모듈이 정한
 * 제어 tick 에서 결정적으로 처리한다 (Core 는 stage+검증만 — tick 소유 안 함).
 * Function API: Core/agr_vector.h. 설계 SSOT: SAM3x_FW
 * `MD_FW/Devices/AGR/Control_Module/Doc/md_command_vector_design.md`.
 *
 * @note  타입을 (agr_vector.h 가 아니라) 여기 둔 이유: AGR_DOP_Ctx_t 가
 *        AGR_Vector_Ctrl_t 를 by-value 임베드하려면 완전 타입이 보여야 함
 *        (AGR_SDO_Master_t 와 동일한 순환 include 회피 배치).
 *
 * @note  [동시성 계약] Slave 상태(last_seq/reg[])는 단일 컨텍스트 모델:
 *        HandleRequest 호출은 ctx 당 한 컨텍스트에서만 (예: MD = mid-level
 *        1kHz tick 의 ring drain, IMU G4 = FDCAN Rx ISR 직접). 등록은 Init
 *        단계에서만. AGR_MW 는 RTOS 독립 — 락 없음 (sdo_master 와 동일 계약).
 */

/** @brief Vector type 코드 공간 (모듈 예약 — Core 는 값을 강제하지 않음) */
/*  0x0X = FES (0x01 ES) / 0x1X = Motor (0x10 P, 0x11 I, 0x12 F, 0x13 F-zero) */

typedef enum {
    AGR_VECTOR_STATUS_OK          = 0,  /**< 적용 완료 */
    AGR_VECTOR_STATUS_BUSY        = 1,  /**< routine 실행 중 — 나중에 재시도 */
    AGR_VECTOR_STATUS_RANGE       = 2,  /**< payload 값 범위 초과 */
    AGR_VECTOR_STATUS_WRONG_STATE = 3,  /**< NMT OPERATIONAL 아님 등 상태 부적합 */
    AGR_VECTOR_STATUS_UNSUPPORTED = 4,  /**< 미등록 vector_type */
    AGR_VECTOR_STATUS_BAD_LEN     = 5,  /**< len ≠ 등록 payload 크기 / 프레임 잘림 */
} AGR_VectorStatus_t;

/**
 * @brief Slave apply 콜백 — 소비 모듈이 mailbox 내용을 자체 상태로 반영.
 * @param vector_type  수신 frame 의 type 코드.
 * @param payload      staging mailbox (등록 size>0) 또는 NULL (size==0 type).
 * @param len          payload 길이 (== 등록 size, Core 가 검증 후 호출).
 * @param user_ctx     ctx->user_ctx.
 * @return AGR_VectorStatus_t — OK 만 last_seq 를 전진시킴 (에러는 동일 seq
 *         재시도 허용). BUSY/RANGE/WRONG_STATE 판정은 모듈 책임 (Core 는
 *         UNSUPPORTED/BAD_LEN 만 자체 판정).
 */
typedef uint8_t (*AGR_VectorApply_fn)(uint8_t vector_type, const void* payload,
                                      uint8_t len, void* user_ctx);

/** @brief 등록된 vector type 하나 (slave 측) */
typedef struct {
    uint8_t            type;      /**< vector_type 코드 */
    uint16_t           size;      /**< 기대 payload 길이 (0 허용 — mailbox 불필요) */
    void*              mailbox;   /**< staging 버퍼 (모듈 소유, aligned). size==0 이면 NULL 허용 */
    AGR_VectorApply_fn on_apply;  /**< 적용 콜백 (필수) */
} AGR_VectorReg_t;

/** @brief Command Vector 상태 (ctx 임베드, zero-init = 비활성/관성) */
typedef struct {
    /* ===== Slave (Request 수신) ===== */
    AGR_VectorReg_t reg[AGR_VECTOR_MAX_TYPES];  /**< 등록 테이블 */
    uint8_t         reg_count;                  /**< 0 = 미등록 → 채널 완전 관성 (pass-through) */
    uint8_t         last_seq;                   /**< 마지막 OK 적용 seq (멱등 dedupe) */
    uint8_t         last_status;                /**< last_seq 의 ACK status (재-ACK 용) */
    bool            seq_seen;                   /**< false = 아직 OK 적용 없음 (zero-init 시 seq 0 오인 방지) */

    /* ===== Master (ACK/EVENT 수신, Optional) ===== */
    void (*on_ack)(uint8_t source_node, uint8_t echo_seq, uint8_t status, void* user_ctx);
    void (*on_event)(uint8_t source_node, uint8_t event_code, void* user_ctx);

    /* ===== 진단 (sdo_diag 와 동일한 volatile 계약 — CMW-08) ===== */
    volatile uint16_t bad_frame_count;          /**< ACK 불가 malformed frame (len<3, ACK-ch 길이 위반) */
} AGR_Vector_Ctrl_t;

/**
 *-----------------------------------------------------------
 * TRANSMISSION FUNCTION TYPE
 *-----------------------------------------------------------
 */
typedef int (*AGR_TxFunc_t)(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 *-----------------------------------------------------------
 * DOP CONTEXT (드라이버 인스턴스)
 *-----------------------------------------------------------
 * @brief 각 드라이버가 관리하는 DOP 통신 컨텍스트
 * @note  하나의 통신 상대(노드)당 하나의 Context를 생성합니다.
 * 
 * @example
 * ```c
 * // IMU Hub 드라이버에서 XM10과의 통신 Context
 * static AGR_DOP_Ctx_t s_xm_ctx;
 * 
 * void IMU_Hub_Drv_Init(AGR_TxFunc_t tx_func) {
 *     s_xm_ctx.od.entries = s_od_entries;
 *     s_xm_ctx.od.entry_count = sizeof(s_od_entries) / sizeof(s_od_entries[0]);
 *     s_xm_ctx.node_id = 0x10;  // IMU Hub의 Node ID
 *     s_xm_ctx.tx_func = tx_func;
 * }
 * ```
 */
typedef struct {
    /* ===== OD 정의 (Static, 드라이버에서 설정) ===== */
    AGR_OD_Table_t       od;              /**< Object Dictionary */
    
    /* ===== PDO 정의 (Static, 컴파일 타임) ===== */
    const AGR_PDO_Def_t* tx_pdo_defs;     /**< 송신 PDO 정의 배열 */
    uint8_t              tx_pdo_count;    /**< 송신 PDO 개수 */
    const AGR_PDO_Def_t* rx_pdo_defs;     /**< 수신 PDO 정의 배열 */
    uint8_t              rx_pdo_count;    /**< 수신 PDO 개수 */
    
    /* ===== PDO Mapping (Dynamic, 런타임 설정 by Master) ===== */
    AGR_PDO_MapTable_t   tx_pdo_map[4];   /**< 동적 TX PDO Mapping Table (TPDO1~4) */
    AGR_PDO_MapTable_t   rx_pdo_map[4];   /**< 동적 RX PDO Mapping Table (RPDO1~4) */
    
    /* ===== 통신 설정 (Runtime, 초기화 시 설정) ===== */
    uint8_t              node_id;         /**< 자신의 Node ID (0x01 ~ 0x7F) */
    uint8_t              target_node_id;  /**< 통신 대상 Node ID */
    AGR_TxFunc_t         tx_func;         /**< CAN 전송 함수 (Dependency Injection) */
    
    /* ===== Week 2: Advanced Features ===== */
    AGR_SYNC_Ctrl_t      sync;            /**< SYNC 제어 (CANopen SYNC Object) */
    AGR_EMCY_Ctrl_t      emcy;            /**< Emergency 제어 (CANopen Emergency) */
    AGR_PDO_Inhibit_t    pdo_inhibit[4];  /**< PDO Inhibit Time (최대 4개 TPDO) */
    
    /* ===== 콜백 (Optional) ===== */
    void (*on_pdo_received)(uint32_t cob_id, const uint8_t* data, uint8_t len);
    void (*on_sdo_request)(const AGR_SDO_Msg_t* req, AGR_SDO_Msg_t* rsp);
    void*                user_ctx;        /**< User Context (콜백에서 사용) */

    /* ===== SDO 진단 (수신 거부 / 에러 카운터) ===== */
    AGR_DOP_SdoDiag_t    sdo_diag;        /**< oversize / decode-err 카운터 (Risk Manager 폴링) */

    /* ===== SDO Master (Client) — 응답/readback/abort 추적 (C6) ===== */
    AGR_SDO_Master_t     sdo_master;      /**< Master 역할 시 사용. Slave-only ctx 는
                                               zero-init 풀 → ProcessResponse 가 매칭
                                               실패로 무해 폐기 (API: agr_sdo_master.h). */

    /* ===== Command Vector — deterministic 제어입력 채널 (opt-in) ===== */
    AGR_Vector_Ctrl_t    vector;          /**< zero-init = 미등록 → fnc 0x02/0x0D 는
                                               기존과 동일하게 pass-through (관성).
                                               API: Core/agr_vector.h. 필드는 항상 struct
                                               끝에 append (레이아웃 보수성). */

} AGR_DOP_Ctx_t;

/**
 *-----------------------------------------------------------
 * HELPER MACROS
 *-----------------------------------------------------------
 */

/** @brief 데이터 타입별 바이트 크기 */
#define AGR_TYPE_SIZE(type) \
    ((type) == AGR_TYPE_UINT8   ? 1 : \
     (type) == AGR_TYPE_UINT16  ? 2 : \
     (type) == AGR_TYPE_UINT32  ? 4 : \
     (type) == AGR_TYPE_INT8    ? 1 : \
     (type) == AGR_TYPE_INT16   ? 2 : \
     (type) == AGR_TYPE_INT32   ? 4 : \
     (type) == AGR_TYPE_FLOAT32 ? 4 : 0)

/** @brief OD Entry 배열 크기 계산 */
#define AGR_OD_ENTRY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/** @brief PDO Map Entry 배열 크기 계산 */
#define AGR_PDO_MAP_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif /* AGR_DOP_TYPES_H */
