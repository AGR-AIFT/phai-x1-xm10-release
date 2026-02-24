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

/**
 *-----------------------------------------------------------
 * CONFIGURATION
 *-----------------------------------------------------------
 */

/** @brief CANFD 최대 페이로드 크기 (64 bytes) */
#define AGR_CANFD_MAX_PAYLOAD       64

/** @brief SDO 최대 데이터 크기 (CANFD 페이로드 - 헤더 4바이트) */
#define AGR_SDO_MAX_DATA_SIZE       60

/** @brief PDO Mapping Table 최대 Entry 개수 */
#define AGR_PDO_MAP_MAX_ENTRIES     32

/** @brief OD Entry 최대 개수 (드라이버별) */
#define AGR_OD_MAX_ENTRIES          64

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
 *     // Index,  SubIdx, Type,            Size, Access,       DataPtr,        WriteCb
 *     { 0x6000,  0x00,   AGR_TYPE_INT16,  2,    AGR_ACCESS_RO, &s_imu_acc_x,  NULL },
 * };
 * ```
 */
typedef struct {
    uint16_t       index;       /**< Object Index (0x0000 ~ 0xFFFF) */
    uint8_t        subindex;    /**< Sub-Index (0x00 ~ 0xFF, 보통 0x00 사용) */
    AGR_DataType_t type;        /**< 데이터 타입 */
    uint8_t        size;        /**< 바이트 크기 (1, 2, 4) */
    AGR_Access_t   access;      /**< 접근 권한 */
    void*          data_ptr;    /**< 바인딩된 변수 주소 */
    void (*on_write)(void);     /**< 쓰기 완료 시 콜백 (Optional, NULL 가능) */
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
    uint8_t  byte_offset;   /**< 페이로드 내 바이트 오프셋 */
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
    uint8_t                    map_count;     /**< 매핑 개수 */
    uint8_t                    payload_size;  /**< 총 페이로드 바이트 크기 */
} AGR_PDO_Def_t;

/**
 *-----------------------------------------------------------
 * PDO MAPPING TABLE (동적 PDO 매핑 - 런타임 설정)
 *-----------------------------------------------------------
 * @brief Master가 SetPDOMapping SDO로 런타임에 설정하는 PDO 리스트
 * @details 
 * - Master(XM10)가 Slave(CM, IMU Hub)에게 "이 OD Entry들을 PDO로 보내라"고 요청
 * - Slave는 이 테이블에 따라 PDO 페이로드를 구성하여 전송
 * 
 * @example
 * ```c
 * // XM10에서 IMU Hub에게 전송 요청
 * static const uint8_t pdo_list[] = {
 *     0x60, 0x00,  // Index 0x6000 (IMU0_AccX)
 *     0x60, 0x01,  // Index 0x6001 (IMU0_AccY)
 *     0x60, 0x02,  // Index 0x6002 (IMU0_AccZ)
 * };
 * AGR_SDO_Write(IMU_HUB_NODE_ID, 0x1600, pdo_list, sizeof(pdo_list));
 * ```
 */
typedef struct {
    uint16_t od_index;      /**< OD Entry Index */
    uint8_t  od_subindex;   /**< OD Entry Sub-Index */
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
 * @note CANFD 확장: CANopen 표준은 4바이트까지 expedited transfer를 정의하지만,
 *       CANFD에서는 60바이트까지 확장하여 사용합니다 (e=1 유지).
 */

/**
 * @brief SDO Command Specifier (CANopen 표준 - CiA 301)
 * @details
 * CCS (Client Command Specifier) - Master → Slave
 * SCS (Server Command Specifier) - Slave → Master
 */
typedef enum {
    /* ===== Download (Write) - Initiate ===== */
    AGR_SDO_CS_DOWNLOAD_INIT_REQ    = 0x20,  /**< [CCS] Download Initiate Request */
    AGR_SDO_CS_DOWNLOAD_INIT_RSP    = 0x60,  /**< [SCS] Download Initiate Response */
    
    /* ===== Upload (Read) - Initiate ===== */
    AGR_SDO_CS_UPLOAD_INIT_REQ      = 0x40,  /**< [CCS] Upload Initiate Request */
    AGR_SDO_CS_UPLOAD_INIT_RSP      = 0x40,  /**< [SCS] Upload Initiate Response (base) */
    
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

/** @brief Expedited Download Initiate with size (쓰기 요청, 크기 지정) */
#define AGR_SDO_CS_DOWNLOAD_EXP(n)  (0x23 | (((n) & 0x03) << 2))

/** @brief Expedited Upload Initiate with size (읽기 응답, 크기 지정) */
#define AGR_SDO_CS_UPLOAD_EXP(n)    (0x43 | (((n) & 0x03) << 2))

/**
 * @brief SDO 메시지 구조체 (CANopen 표준 호환)
 */
typedef struct {
    uint8_t       cs;                           /**< Command Specifier (CANopen 표준) */
    uint16_t      index;                        /**< Object Index */
    uint8_t       subindex;                     /**< Sub-Index */
    uint8_t       data[AGR_SDO_MAX_DATA_SIZE];  /**< Data (CANFD: 최대 60바이트) */
    uint8_t       data_len;                     /**< 실제 데이터 길이 */
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
    uint32_t last_emcy_sent_ms;      /**< 마지막 Emergency 전송 시각 */
    void (*on_emergency)(uint16_t error_code, void* ctx);  /**< Emergency 수신 콜백 */
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
