/**
 ******************************************************************************
 * @file    xm_api_canbus.h
 * @author  HyundoKim
 * @brief   CAN-FD · 클래식 CAN 으로 연결한 내 장치와 주고받기 (Rev 2.0)
 * @details 두 가지 방법이 있습니다. 둘을 함께 써도 됩니다.
 *          ① 장치 API — 장치 번호를 등록해 두면(XM_CanIsDeviceIdAllowed), 장치가 보내는 주기 데이터 1 · 2 와 살아있음을
 *             XM10 이 알아서 나눠 담습니다(최신값). 명령도 보낼 수 있습니다.
 *          ② 자유 송수신 — 프로토콜 없이 ID 와 데이터만으로 보내고 받습니다(받은 순서 큐 · ID 별 최신값 우편함).
 *          클래식 CAN 전용 장치(모터 드라이버 등)는 XM_CanBusMode() 로 포트를 클래식 모드로 정해서 씁니다.
 *          프로토콜 · 장치 설정 · 기동 순서는 내 코드에서 정합니다 — XM10 은 막을 이유가 있는 ID 만 막습니다.
 * @note    호출 자리: 모든 함수는 Control_Setup() · Control_Loop() 안에서만 부르세요. 다른 태스크 · 인터럽트에서 부르면
 *          거부되고(bool 은 false, 결과 값은 XM_CAN_NOT_OWNER) owner_violation_count 로 셉니다. 등록 함수
 *          (XM_CanAttachDevice · XM_CanSubscribe)는 Control_Setup() 안에서만 됩니다. 어느 함수도 기다리지 않습니다.
 *          XM_CanIsIdAllowed · XM_CanIsDeviceIdAllowed · XM_CanFitLength · XM_CanSeqIsNewer 는 어디서나 부를 수 있습니다.
 * @note    보내기 결과는 꼭 `== XM_CAN_OK` 로 확인하세요. XM_CAN_OK 가 0 이라 `if (XM_CanSend(...))` 는 실패를 성공으로 읽습니다.
 * @note    한 포트에서 지킬 것:
 *          - 클래식 장치와 CAN-FD 장치를 함께 연결하지 마세요.
 *          - 모든 장치는 같은 속도여야 합니다.
 *          - 두 장치가 같은 ID 를 보내면 안 됩니다 — 번호를 겹치지 마세요.
 *          - 보낼 수 있는 양에는 한계가 있습니다 — 메시지 시간을 더해 버스 시간 안에 드는지 계산하세요.
 *          - 데이터 속도는 최대 5 Mbit/s 입니다. CAN XL 은 지원하지 않습니다.
 *          - 11비트 ID 만 지원합니다. 리모트 프레임은 지원하지 않습니다.
 *          - 같은 ID 를 보낼 때 앞 메시지가 아직 대기 중이면 순서가 바뀔 수 있습니다 — 앞 메시지가 나간 뒤에 보내세요.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_INC_XM_API_CANBUS_H_
#define XM_API_INC_XM_API_CANBUS_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define XM_CAN_MAX_DEVICES       (4U)           /**< 등록할 수 있는 장치 수 — 동시에 빠르게 돌릴 수 있는 대수의 보장은 아닙니다 */
#define XM_CAN_MAX_DATA          (64U)          /**< 한 번에 싣는 데이터 최대 바이트(클래식 모드는 8) */
#define XM_CAN_ALIVE_TIMEOUT_MS  (3000U)        /**< 살아있음이 이만큼 없으면 '끊김' */
#define XM_CAN_LENGTH_INVALID    (0xFFU)        /**< XM_CanFitLength — 이 모드에서 못 싣는 길이 */
#define XM_CAN_AGE_NONE          (0xFFFFFFFFUL) /**< XM_CanAgeMs — 받은 적 없음 · 너무 오래됨 · 등록 안 함 */

/* 클래식 CAN 속도 프리셋 — XmCanBitTiming_t 의 초기값으로 씁니다. 표본 위치 87.5 % */
#define XM_CAN_TIMING_1M         { 5U, 13U, 2U, 2U}   /**< 1 Mbit/s(기본) */
#define XM_CAN_TIMING_500K       {10U, 13U, 2U, 2U}   /**< 500 kbit/s */
#define XM_CAN_TIMING_250K       {20U, 13U, 2U, 2U}   /**< 250 kbit/s */
#define XM_CAN_TIMING_125K       {40U, 13U, 2U, 2U}   /**< 125 kbit/s */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 모든 '보내기' 함수의 결과 */
typedef enum {
    XM_CAN_OK = 0,              /**< 보내기 큐에 실렸습니다(수락) — 상대가 받았다는 뜻은 아닙니다 */
    XM_CAN_BUSY,                /**< 이번엔 못 실었습니다(큐 가득 · 다른 송신과 겹침) — 다음 주기에 다시 */
    XM_CAN_RECOVERY,            /**< 버스 복구 중입니다(버스 오류 뒤 · 막힌 송신을 비우는 중) — 잠시 뒤 다시 */
    XM_CAN_NOT_CONNECTED,       /**< (장치 API 만) 장치가 연결돼 있지 않습니다 */
    XM_CAN_REJECTED,            /**< 보낼 수 없는 ID · 길이 · 인자이거나, 이 모드에서 지원하지 않습니다 */
    XM_CAN_NOT_OWNER,           /**< Control_Setup() · Control_Loop() 밖에서 불렀습니다 */
} XmCanResult_t;

/** @brief CAN-FD #2 포트의 버스 모드 — 전원을 켤 때 한 번 정합니다 */
typedef enum {
    XM_CAN_BUS_FD = 0,          /**< 기본 — CAN-FD(1 / 5 Mbit/s). IMU · EMG · FES 허브와 함께 쓸 수 있습니다 */
    XM_CAN_BUS_CLASSIC,         /**< 클래식 CAN 전용(기본 1 Mbit/s — XM_CanClassicBitTiming) — 모터 드라이버 등. 이 포트에 허브를 연결하지 마세요 */
    XM_CAN_BUS_UNAVAILABLE,     /**< (읽기 전용) 클래식을 요청했지만 설정하지 못했습니다 — 이 포트로 아무것도 보내지 않습니다 */
} XmCanBusMode_t;

/**
 * @brief 클래식 CAN 비트 타이밍 — 속도 = 80 MHz ÷ prescaler ÷ (1 + seg1 + seg2)
 * @details 표본 위치 = (1 + seg1) ÷ (1 + seg1 + seg2). 상대 장치와 맞는 값을 쓰세요. 프리셋: XM_CAN_TIMING_1M 등
 */
typedef struct {
    uint16_t prescaler;         /**< 분주 1~512 */
    uint16_t seg1;              /**< 표본 위치 앞 구간 1~256 */
    uint8_t  seg2;              /**< 표본 위치 뒤 구간 1~128 */
    uint8_t  sjw;               /**< 재동기 폭 1~seg2 */
} XmCanBitTiming_t;

/** @brief 받은 메시지 하나 */
typedef struct {
    uint32_t seq;               /**< 큐: 큐로 들어온 순서 번호(넘쳐서 버린 것도 번호를 씁니다 — 빠진 번호 = 버린 수)
                                     · 최신값(장치 API · 우편함): 그 ID 를 받을 때마다 +1 — 지난번과 같으면 새 메시지가 아닙니다 */
    uint32_t tick_ms;           /**< 받은 시각(XM_GetTick 기준, 1 ms) */
    uint16_t id;                /**< 메시지 ID(11비트) */
    uint8_t  len;               /**< 버스에 실린 길이 — FD: 0~8 · 12 · 16 · 20 · 24 · 32 · 48 · 64 / 클래식: 0~8 */
    uint8_t  data[XM_CAN_MAX_DATA];
} XmCanFrame_t;

/** @brief 장치별 통계(등록한 장치만) — 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다 */
typedef struct {
    uint32_t rx_count[2];               /**< 주기 데이터 1 · 2 를 받은 수 */
    uint32_t rx_busy_drop_count;        /**< 읽는 중이라 못 쓰고 버린 수 */
    uint32_t unread_overwrite_count;    /**< 아직 읽지 않은 값을 새 값이 덮은 수 */
    uint32_t alive_count;               /**< 살아있음(0x05)을 받은 수 */
    uint32_t alive_invalid_count;       /**< 살아있음 자리로 온 다른 값 · 빈 메시지 수 */
    uint32_t reboot_count;              /**< 재시작 알림(0x00)을 받은 수 */
    uint32_t last_rx_tick;              /**< 이 장치의 메시지를 마지막으로 받은 시각 */
    uint32_t last_alive_tick;           /**< 살아있음(0x05)을 마지막으로 받은 시각 */
    uint32_t send_accept_count;         /**< XM_CanSend 가 수락된 수 */
    uint32_t send_fail_count;           /**< XM_CanSend 가 실패한 수 */
    uint32_t read_busy_count;           /**< 쓰는 중이라 XM_CanReadLatest 가 이번엔 못 읽은 수 */
    uint8_t  last_send_fail;            /**< 마지막으로 실패한 XM_CanSend 결과(XmCanResult_t) — 0 = 아직 없음 */
} XmCanDeviceStats_t;

/** @brief 포트 공통 통계 — 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다 */
typedef struct {
    uint32_t bad_call_count;            /**< 미등록 번호 · 잘못된 인자 · Control_Setup() 밖 등록 */
    uint32_t owner_violation_count;     /**< 다른 태스크 · 인터럽트에서 부른 수 */
    uint32_t flush_request_count;       /**< 받아 줄 장치가 없어 대기 중 메시지 취소를 '요청' 한 수 — 완료 수가 아닙니다 */
    uint32_t busoff_flush_count;        /**< 버스 오류(bus-off) 뒤 대기 중 메시지를 비운 수 */
    uint32_t tx_pending_count;          /**< 지금 하드웨어에 남아 있는 송신 수(부른 순간) */
    uint32_t sync_skip_count;           /**< SYNC 를 보내지 못한 tick 수 — 아래 네 사유의 합 */
    uint32_t sync_skip_busy;            /**< 다른 송신과 겹침 · 송신 큐 가득 */
    uint32_t sync_skip_hub_setup;       /**< 센서 허브를 설정하는 동안 */
    uint32_t sync_skip_recovery;        /**< 버스 복구 중 */
    uint32_t sync_skip_clear;           /**< (클래식) 장치 요청이 시간 초과로 끝난 뒤 송신 대기를 비우는 동안 */
    uint32_t rx_frame_count;            /**< 이 API 로 들어온 메시지 수 */
    uint32_t rx_queue_drop_count;       /**< 받은 순서 큐가 가득해 버린 수 */
    uint32_t rx_queue_last_seq;         /**< 큐로 보낸 마지막 메시지의 seq — 큐를 다 비운 뒤 drop 수와 대조합니다 */
    uint32_t rx_queue_peak;             /**< 큐에 쌓였던 최대 개수 */
    uint32_t rejected_id_count;         /**< 허용 밖 ID 로 보내려 한 수 */
    uint32_t classic_hub_tx_refused_count; /**< (클래식 · 사용 불가) XM10 이 이 포트로 내보내지 않고 막은 허브용 송신 수 */
    uint32_t timing_write_fail_count;   /**< XM_CanSetClassicBitTiming 이 속도를 쓰지 못한 수 */
    uint32_t heartbeat_sent_count;      /**< XM_CanEnableHeartbeat 의 하트비트가 수락된 수 */
    uint32_t heartbeat_fail_count;      /**< 하트비트를 못 실어 다음 주기로 미룬 수 */
    uint8_t  heartbeat_last_fail;       /**< 마지막으로 실패한 하트비트 결과(XmCanResult_t) — 0 = 아직 없음 */
    uint8_t  bus_mode;                  /**< 지금 버스 모드(XmCanBusMode_t) */
    uint8_t  tec;                       /**< 송신 오류 카운터(부른 순간) */
    uint8_t  rec;                       /**< 수신 오류 카운터(부른 순간) */
    uint8_t  bus_status;                /**< bit0 bus-off · bit1 오류 경고 · bit2 오류 수동 */
} XmCanBusStats_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ===== 버스 모드 ===== */

/**
 * @brief 포트의 버스 모드를 정하는 함수 — 사용자가 만듭니다(선택). 만들지 않으면 FD 입니다.
 * @details 전원을 켤 때 한 번 불립니다. 부팅 중이라 다른 XM 함수는 아직 준비 전입니다 — 값만 돌려주세요.
 *          클래식으로 정하면 이 포트는 클래식 CAN 으로만 보내고 받으며, 허브는 이 포트에서 동작하지 않습니다.
 * @code
 * XmCanBusMode_t XM_CanBusMode(void) { return XM_CAN_BUS_CLASSIC; }
 * @endcode
 */
XmCanBusMode_t XM_CanBusMode(void);

/** @brief 지금 버스 모드 — 언제나 부를 수 있습니다. 클래식을 설정하지 못했거나 포트가 고장 났으면 XM_CAN_BUS_UNAVAILABLE */
XmCanBusMode_t XM_CanGetBusMode(void);

/**
 * @brief 클래식 CAN 속도를 정하는 함수 — 사용자가 만듭니다(선택). 만들지 않으면 1 Mbit/s 입니다.
 * @details XM_CanBusMode() 가 클래식이면 전원을 켤 때 한 번 불립니다. 부팅 중이라 다른 XM 함수는 아직 준비 전입니다 — 값만
 *          채우세요. 틀린 속도로 시작하면 XM10 이 오류 메시지로 버스를 깨므로 시작 전에 정합니다. 값이 범위 밖이면 포트를
 *          쓰지 못합니다(XM_CAN_BUS_UNAVAILABLE).
 * @code
 * void XM_CanClassicBitTiming(XmCanBitTiming_t* timing)
 * {
 *     const XmCanBitTiming_t t = XM_CAN_TIMING_500K;
 *     *timing = t;
 * }
 * @endcode
 */
void XM_CanClassicBitTiming(XmCanBitTiming_t* timing);

/**
 * @brief 실행 중에 클래식 CAN 속도를 바꿉니다 — 기다리지 않습니다. 대부분은 XM_CanClassicBitTiming 이면 충분합니다
 * @details 공장 속도가 다른 장치를 새 속도로 설정한 뒤 함께 옮길 때 씁니다. 바꾸는 동안 받아 두고 아직 읽지 않은 메시지는
 *          사라지고, 오류 카운터(tec · rec)는 그대로입니다. 같은 값이면 아무것도 멈추지 않고 XM_CAN_OK 입니다.
 *          장치에 속도 변경을 보낸 뒤에는 다른 메시지를 보내지 말고 바로 이 함수를 부르세요. 장치가 먼저 옮겨 가면 답을
 *          못 받은 메시지가 남아 몇 초 동안 XM_CAN_BUSY 일 수 있습니다.
 * @param t 새 속도(XM_CAN_TIMING_* 또는 직접 쓴 값)
 * @return XM_CAN_OK = 바뀜, 다음 보내기부터 새 속도.
 *         XM_CAN_BUSY = 보낼 메시지가 남음(SYNC 를 켰으면 먼저 끄세요) · XM_CanOpenSdoRead · Write 진행 중 · 잠깐 겹침 — 다음 주기에 다시.
 *         XM_CAN_RECOVERY = 버스 복구 중.
 *         XM_CAN_REJECTED = NULL · 값 범위 밖 · 클래식 모드가 아님 · 포트 고장(재시작하세요 — XM_CanGetBusMode 가 XM_CAN_BUS_UNAVAILABLE).
 *         XM_CAN_NOT_OWNER
 * @note  버스의 모든 장치와 같은 속도여야 합니다. 어긋나면 오류가 쌓이고 버스 복구로 보입니다(tec · bus_status).
 */
XmCanResult_t XM_CanSetClassicBitTiming(const XmCanBitTiming_t* t);

/** @brief 지금 클래식 CAN 속도 — 클래식 모드가 아니거나 포트 고장이면 false(out 은 그대로) */
bool XM_CanGetClassicBitTiming(XmCanBitTiming_t* out);

/* ===== 장치 API ===== */

/**
 * @brief 장치 번호를 등록합니다 — Control_Setup() 안에서만.
 * @details 등록하면 그 장치의 주기 데이터 1(ID 0x180 + 번호) · 주기 데이터 2(0x280 + 번호) · 살아있음(0x700 + 번호)을
 *          XM10 이 받아 둡니다. 같은 버스에서 번호는 장치마다 달라야 합니다(XM10 은 중복을 알아채지 못합니다).
 * @return 쓸 수 없는 번호(XM_CanIsDeviceIdAllowed) · 중복 · 칸 초과 · Control_Setup() 밖이면 false(bad_call_count +1)
 */
bool XM_CanAttachDevice(uint8_t device_id);

/** @brief 3 초 안에 살아있음(0x05)을 받았는가 — 재시작 알림(0x00) 뒤에는 다음 0x05 까지 false */
bool XM_CanIsConnected(uint8_t device_id);

/**
 * @brief 그 장치의 주기 데이터 최신값을 읽습니다.
 * @param stream 1 = 주기 데이터 1, 2 = 주기 데이터 2
 * @return true = out 에 값이 있습니다. 같은 seq 를 다시 읽어도 true 입니다(새 값인지는 seq 를 비교).
 *         끊긴 뒤에도 옛 값은 true 로 읽힙니다(tick_ms · XM_CanIsConnected 로 판단).
 *         false = 아직 못 받음 · 재시작 알림 뒤 새 값 전 · 쓰는 중이라 이번엔 못 읽음 · 잘못된 인자 — out 은 그대로입니다.
 */
bool XM_CanReadLatest(uint8_t device_id, uint8_t stream, XmCanFrame_t* out);

/**
 * @brief 장치에 명령을 보냅니다(ID 0x200 + 번호).
 * @details 길이 1~64(클래식 1~8) — 허용 길이(1~8 · 12 · 16 · 20 · 24 · 32 · 48 · 64)가 아니면 다음 허용 길이까지 0 으로
 *          채워 나갑니다. XM10 이 최근 살아있음으로 '연결' 이라 판단할 때만 수락합니다(끊긴 직후 최대 3 초는 수락될 수 있음).
 *          수락된 명령은 하드웨어가 보낼 때까지 다시 시도하고, XM10 이 소프트웨어로 다시 보내지는 않습니다. 끊긴 동안
 *          하드웨어에 남아 있던 명령은 다시 연결될 때 나갈 수 있습니다 — 명령 첫 바이트를 순번으로 써서 장치가
 *          중복 · 순서 뒤바뀜을 거르게 하세요(XM_CanSeqIsNewer). 순번은 유효기간이 아닙니다.
 * @return XM_CAN_OK = 큐에 실림(전달 완료가 아님). XM_CAN_NOT_CONNECTED · BUSY · RECOVERY · REJECTED · NOT_OWNER
 */
XmCanResult_t XM_CanSend(uint8_t device_id, const void* data, uint8_t len);

/** @brief 장치별 통계 — 등록하지 않은 번호 · out 이 NULL 이면 false */
bool XM_CanGetStats(uint8_t device_id, XmCanDeviceStats_t* out);

/** @brief 포트 공통 통계 — out 이 NULL 이면 false */
bool XM_CanGetBusStats(XmCanBusStats_t* out);

/**
 * @brief 1 kHz 동기 신호(SYNC, ID 0x080)를 내 장치에게도 보냅니다 — 기본 꺼짐.
 * @details FD 모드: 켜도 받아 줄 장치가 3 초 안에 하나도 안 보이면 보내지 않습니다. 센서 허브가 동작 중이면 끄더라도 SYNC 는
 *          그대로 나갑니다(끄기는 내 요청만 끕니다). FD 모드의 SYNC 는 1 B(카운터) · 클래식 모드는 길이 0 입니다.
 *          SYNC 는 잠깐 멈추거나 한 번씩 빠질 수 있습니다 — 장치는 샘플링 정렬에만 쓰고, 없으면 자기 주기로 계속 보내게 하세요.
 *          클래식 모드에서 켠 동안에는 XM_CanSendFrame 으로 ID 0x080 을 보낼 수 없습니다.
 */
void XM_CanEnableSync(bool on);

/**
 * @brief XM10 의 하트비트를 내 장치에게 보냅니다 — ID 0x700 + node_id, 데이터 1 바이트 0x05. 기본 꺼짐.
 * @param node_id   1~127 — 상대 장치가 기다리는 번호
 * @param period_ms 보내는 주기 1~65535 ms. 0 = 끔(node_id 는 보지 않습니다)
 * @details Control_Loop() 가 끝난 직후에, 주기가 됐으면 보냅니다. 그래서 Control_Loop() 가 돌지 않으면 새로 보내지 않습니다 —
 *          상대가 하트비트 끊김으로 스스로 멈추게 할 때 씁니다. 다만 이미 송신 대기에 든 메시지는 그 뒤에도 나갈 수 있고,
 *          상대가 실제로 멈추는지는 상대 설정에 달렸습니다 — 보드에서 확인하세요.
 *          못 실으면 다음 주기가 아니라 다음 tick 에 다시 시도합니다(주기는 성공한 때부터). 버스 복구 중에는 간격이 길어질 수 있습니다.
 * @return XM_CAN_OK = 켬 · 끔 · 주기 변경 완료. XM_CAN_REJECTED = 범위 밖 번호 · 사용 불가 모드 · FD 모드에서 0x02 또는
 *         0x0C · 0x0D · 0x0F · 그 ID 를 XM_CanAttachDevice · XM_CanSubscribe · 감시로 이미 받는 중(반대 순서도 거부됩니다).
 *         XM_CAN_NOT_OWNER
 */
XmCanResult_t XM_CanEnableHeartbeat(uint8_t node_id, uint16_t period_ms);

/* ===== 자유 송수신 — 프로토콜 없이 ID 와 데이터만 ===== */

/**
 * @brief 메시지 하나를 보냅니다.
 * @details 보낼 수 있는 ID — FD 모드: 0x000~0x7FF 중 아래 7비트가 0x0C · 0x0D · 0x0F 인 ID(센서 허브 자리) · 0x080(SYNC 는
 *          XM_CanEnableSync 로) · 0x702(XM10 시스템 하트비트)를 뺀 전부입니다. ID 0x000 은 길이 2 이고 둘째 바이트가 대상
 *          번호일 때만 보냅니다(0 = 전체 · 센서 허브 번호는 거부).
 *          클래식 모드: 0x000~0x7FF 전부(SYNC 를 켠 동안의 0x080 제외). 11비트만 됩니다.
 *          길이 FD 0~64(다음 허용 길이까지 0 으로 채움) · 클래식 0~8. data 는 길이 0 일 때만 NULL 이어도 됩니다.
 *          받는 장치가 없어도 수락될 수 있습니다. 수락 ≠ 전달입니다(XM_CanSend 와 같은 규칙).
 */
XmCanResult_t XM_CanSendFrame(uint16_t id, const void* data, uint8_t len);

/**
 * @brief 받은 순서대로 하나를 꺼냅니다 — 없으면 false.
 * @details 우편함 · 장치 API 가 가져가지 않은 메시지가 깊이 64 의 큐로 들어옵니다 — 등록하지 않은 ID 도 모두 옵니다(FD 모드의
 *          센서 허브 자리 제외). 가득 차면 새 메시지를 버리고 rx_queue_drop_count 를 셉니다. Control_Loop 에서
 *          `while (XM_CanReadFrame(&f)) { ... }` 로 매 주기 비우세요.
 *          서로 다른 종류의 메시지(살아있음 · 응답 · 오류 알림 대 그 밖) 사이의 버스 순서는 보장하지 않습니다.
 */
bool XM_CanReadFrame(XmCanFrame_t* out);

/**
 * @brief 이 ID 는 큐 대신 최신값 우편함으로 받습니다 — Control_Setup() 안에서만. 1 kHz 주기 데이터에 알맞습니다.
 * @details 우편함은 장치 API 의 스트림과 합쳐 16 칸입니다. 11비트 밖 ID · FD 모드의 센서 허브 자리(아래 7비트 0x0C · 0x0D · 0x0F) ·
 *          등록한 장치의 ID · 이미 등록한 ID · XM_CanEnableHeartbeat 로 내가 보내는 ID 는 거부합니다.
 */
bool XM_CanSubscribe(uint16_t id);

/** @brief 우편함 ID 의 최신값 — XM_CanReadLatest 와 같은 규칙(같은 seq 도 true, 실패면 out 그대로) */
bool XM_CanReadLatestById(uint16_t id, XmCanFrame_t* out);

/** @brief 우편함 ID 를 마지막으로 받은 뒤 지난 ms — 받은 적 없음 · 2^31 ms 이상 · 등록 안 함이면 XM_CAN_AGE_NONE */
uint32_t XM_CanAgeMs(uint16_t id);

/**
 * @brief 도움 — 이 모드의 ID 규칙으로 보낼 수 있는 ID 인가.
 * @details ID 만 봅니다. ID 0x000 의 데이터 검사 · SYNC 를 켠 동안의 0x080 거부 · BUSY · RECOVERY 는 XM_CanSendFrame 이 따로 알려 줍니다.
 */
bool XM_CanIsIdAllowed(uint16_t id);

/** @brief 도움 — 장치 API 에 등록할 수 있는 번호인가. 1~127 중 예약 번호(0x01 · 0x02 · 0x0C · 0x0D · 0x0F · 0x10 · 0x11)를 뺀 나머지 */
bool XM_CanIsDeviceIdAllowed(uint8_t id);

/** @brief 도움 — 버스에 실릴 길이로 올립니다(0 → 0). 이 모드에서 못 실으면 XM_CAN_LENGTH_INVALID */
uint8_t XM_CanFitLength(uint8_t len);

/** @brief 도움 — 8비트 순번: now 가 last 보다 1~127 앞서면 새 값입니다(0 · 128~255 차이는 중복 · 옛 값) */
bool XM_CanSeqIsNewer(uint8_t last, uint8_t now);

#endif /* XM_API_INC_XM_API_CANBUS_H_ */
