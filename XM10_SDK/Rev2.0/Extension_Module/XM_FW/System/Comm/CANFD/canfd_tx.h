/**
 ******************************************************************************
 * @file    canfd_tx.h
 * @author  HyundoKim
 * @brief   CAN-FD 송신 경로 — Ch1(CM) 송신 · Ch2(센서 버스) 송신 상태머신 · SYNC · bus-off 주기 서비스
 * @details system_startup.h 에서 선언을 옮겨 왔다(2026-10-29). system_startup.h 가 이 헤더를 포함하므로
 *          기존 호출부는 그대로 컴파일된다.
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_CANFD_CANFD_TX_H_
#define SYSTEM_COMM_CANFD_CANFD_TX_H_

#include <stdint.h>
#include <stdbool.h>

#include "ioif_agrb_fdcan.h"

/** @brief System_Fdcan2_TransmitUser() 결과 — 어느 값이든 함수는 기다리지 않고 돌아온다 */
typedef enum {
    FDCAN2_USER_TX_OK = 0,      /**< 하드웨어 송신 큐에 실렸다(수락) — 전달 완료가 아니다 */
    FDCAN2_USER_TX_BUSY,        /**< 상태 락 · TX 락 경합, 송신 큐 가득, HAL 거절 — 이번 호출만 실패 */
    FDCAN2_USER_TX_RECOVERY,    /**< 복구 대기 — flush 요청 · 이번 tick 비우는 중 · 백오프 · bus-off 세대 불일치 */
    FDCAN2_USER_TX_INVALID,     /**< 11비트 밖 ID · 길이 > 0 인데 NULL · 모드 상한 초과(FD 64 · 클래식 8) · 사용 불가 모드 · 채널 미할당 */
} Fdcan2UserTxResult_t;

/** @brief System_Fdcan2_SetClassicTiming() 결과 — 어느 값이든 기다리지 않고 돌아온다 */
typedef enum {
    FDCAN2_TIMING_OK = 0,       /**< 새 속도로 달린다(이미 그 값이었으면 멈추지 않았다) */
    FDCAN2_TIMING_BUSY,         /**< 상태 · TX 락 경합, 송신 대기 있음, 쓰기가 먹지 않아 옛 속도로 되돌아감 — 다음 tick 에 다시 */
    FDCAN2_TIMING_RECOVERY,     /**< 복구 대기 — 사용자 송신과 같은 게이트 */
    FDCAN2_TIMING_REJECTED,     /**< 클래식 모드가 아님 · 고장 표시 · 이번 호출이 고장을 냄 · IOIF 가 인자를 거부 */
} Fdcan2TimingResult_t;

/** @brief 클래식 Nominal 비트 타이밍 값 — 공개 XmCanBitTiming_t 와 같은 칸(값 범위는 CanfdUserBridge_IsBitTimingValid) */
typedef struct {
    uint16_t prescaler;
    uint16_t seg1;
    uint8_t  seg2;
    uint8_t  sjw;
} CanfdTxBitTiming_t;

/** @brief Ch2 버스 모드 — 부팅 때 한 번 정하는 상수. 실행 중에 바뀌지 않는다 */
typedef enum {
    CANFD_TX_CH2_MODE_FD = 0,          /**< 기본 — 허브(FD+BRS)와 사용자 장치. 지금까지의 동작 */
    CANFD_TX_CH2_MODE_CLASSIC,         /**< 클래식 전용 — AGR 송신 거부 · SYNC 길이 0 · 사용자 송신 ≤ 8 B · 상태 관리는 PnP_Task */
    CANFD_TX_CH2_MODE_UNAVAILABLE,     /**< 클래식을 요청했지만 설정하지 못함 — Ch2 송신 전부 거부(채널도 START 하지 않는다) */
} CanfdTxCh2Mode_t;

/**
 * @brief Ch2 송신 경로 통계 — 필드별 약한 스냅숏(각 32비트 값은 온전, 서로 같은 순간은 아님)
 * @note  flush_request_count · link_flush_count · busoff_flush_count 는 '요청 · 시도' 수다 — 취소 완료가 아니다.
 *        완료는 tx_pending_count(하드웨어 대기 수, TXBRP)가 0 이 되는 것으로 보되, 0 은 '대기 없음' 일 뿐
 *        개별 프레임이 취소됐는지 송신됐는지는 구분하지 못한다.
 */
typedef struct {
    uint32_t flush_request_count;     /**< 빈 버스 · BUS 복구 flush 요청 수 */
    uint32_t link_flush_count;        /**< 채널 취소(AbortAllTx)가 요청을 처리한 수(OK) — 완료 수는 아니다 */
    uint32_t abort_fail_count;        /**< 채널 취소가 TX 락을 못 잡아 아무것도 비우지 못한 수 — 미뤄 둔 요청은 남겨 다시 한다 */
    uint32_t classic_agr_tx_refused_count; /**< 클래식 · 사용 불가 모드에서 거부한 AGR 송신(PnP HB · NMT · SDO · 허브 명령) 수 */
    uint32_t recovery_count;          /**< bus-off 복구 진입 수 */
    uint32_t busoff_flush_count;      /**< IOIF 주기 서비스가 bus-off 뒤 채널을 비운 수 */
    uint32_t tx_pending_count;        /**< 지금 하드웨어 송신 대기 수(호출 시점) */
    uint32_t sync_skip_state_lock;    /**< SYNC 생략 — 상태 락 경합(구 바이너리에도 있는 카운터) */
    uint32_t sync_skip_quiet;         /**< SYNC 생략 — 허브 설정 조용 기간 */
    uint32_t sync_skip_backoff;       /**< SYNC 생략 — 복구 백오프 */
    uint32_t sync_skip_recovery;      /**< SYNC 생략 — bus-off 감지로 복구 요청 */
    uint32_t sync_skip_tx_busy;       /**< SYNC 생략 — TX 락 경합 · 큐 가득 · HAL 거절(예전에는 기다렸다) */
    uint32_t user_tx_ok_count;
    uint32_t user_tx_busy_count;
    uint32_t user_tx_recovery_count;
    uint32_t user_tx_invalid_count;
    uint32_t timing_write_fail_count; /**< 실행 중 속도 변경 실패 수(옛 값으로 되돌아감 · 고장) */
} CanfdTxCh2Stats_t;

/**
 * @brief Ch2 송신 상태 mutex 를 만든다 — System_Startup() 에서 가장 먼저 1회(FDCAN RxTask 기동 · START 전).
 */
void CanfdTx_Init(void);

/**
 * @brief Ch1 · Ch2 의 IOIF 채널 ID 를 넘긴다 — 두 채널 ASSIGN 뒤 · PnP_Task_Init · START 전 1회.
 * @note  넘기기 전에는 송신 함수가 잘못된 ID 로 IOIF 에서 거절된다(옮기기 전과 같은 동작).
 */
void CanfdTx_BindChannels(IOIF_FDCANx_t ch1_id, IOIF_FDCANx_t ch2_id);

/**
 * @brief Ch2 버스 모드를 정한다 — StartupTask 가 FDCAN2 ASSIGN · (클래식이면) IOIF 클래식 설정 뒤 · START 전에 한 번
 *        (CanfdTx_BindChannels 와 같은 자리). 그 뒤에는 읽기만 하는 부팅 상수다. 부르지 않으면 FD.
 */
void CanfdTx_SetCh2BusMode(CanfdTxCh2Mode_t mode);

/** @brief 지금 Ch2 버스 모드 — 어느 태스크에서나 부를 수 있다(부팅 상수) */
CanfdTxCh2Mode_t CanfdTx_GetCh2BusMode(void);

/** @brief 부팅 때 정한 클래식 비트 타이밍을 적어 둔다 — StartupTask, 레지스터에 쓴 뒤 · START 전에 한 번 */
void CanfdTx_SetCh2ClassicTiming(uint16_t prescaler, uint16_t seg1, uint8_t seg2, uint8_t sjw);

/** @brief 지금 클래식 비트 타이밍 — 클래식 모드가 아니거나 고장 표시면 false(out 그대로). UserTask 전용 */
bool CanfdTx_GetCh2ClassicTiming(CanfdTxBitTiming_t* out);

/**
 * @brief 실행 중에 클래식 속도를 바꾼다 — 기다리지 않는다. UserTask 전용(우선순위가 FDCAN RxTask 보다 낮아야 한다, IOIF 계약)
 * @details 하드웨어를 멈춘 채 값을 쓰고 다시 시작한다. 성공하면 그 tick 의 다음 송신부터 새 속도다. 수신 FIFO 에 남은 프레임은
 *          사라지고 오류 카운터는 그대로다. 멈출 수 없거나 값을 알 수 없게 되면 고장 표시를 세운다 —
 *          그 뒤로 사용자 송신 · SYNC · 상태 관리가 멈추고, 복구 코드는 없다(재시작).
 * @param prescaler · seg1 · seg2 · sjw 값 범위는 호출자가 먼저 확인한다(CanfdUserBridge_IsBitTimingValid)
 */
Fdcan2TimingResult_t System_Fdcan2_SetClassicTiming(uint16_t prescaler, uint16_t seg1, uint8_t seg2, uint8_t sjw);

/** @brief 속도 변경 고장 표시 — 어느 태스크에서나 부를 수 있다(1 byte, UserTask 단일 Writer) */
bool CanfdTx_IsCh2Faulted(void);

/**
 * @brief 클래식 모드의 Ch2 상태 관리 — PnP_Task 100 ms 주기에서 부른다(FD · 사용 불가 모드에서는 바로 돌아온다).
 * @details 클래식 모드에는 AGR 송신(PnP HB 등)이 없어 그 경로가 하던 일(미뤄 둔 flush 소비 · bus-off 복구)을 여기서 한다.
 *          정리 규칙: bus-off → 복구(백오프 + 채널 취소) · 오류 증거(TEC ≥ 128 · 오류 수동 ·
 *          대기 32 가득)이면서 bus-live 0 → 정리. 대기만 있는 것으로는 정리하지 않고(받기만 하는 장치로 가는 명령을
 *          지우지 않게), 살아있음이 내려가는 전이로도 비우지 않는다.
 * @note  상태 mutex 를 기다려 잡는다(PnP_Task 규약 — osWaitForever). UserTask 에서 부르지 않는다.
 */
void CanfdTx_ServiceCh2Housekeeping(void);

/**
 * @brief FDCAN1 채널(Ch1)을 통해 CAN 메시지를 전송하는 래퍼 함수.
 * @note  CM(DOP V1) 전용 채널. AGR_TxFunc_t / CM_TxFunc_t 타입과 호환.
 * @param[in] can_id CAN ID (11-bit 또는 29-bit).
 * @param[in] data  전송할 데이터의 포인터 (const).
 * @param[in] len   전송할 데이터의 길이 (0~64 bytes).
 * @return 0 on success, <0 on error.
 */
int System_Fdcan1_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief FDCAN2 채널(Ch2)을 통해 CAN 메시지를 전송하는 래퍼 함수.
 * @note  Sensor Module(DOP V2) 전용 채널. AGR_TxFunc_t 타입과 호환.
 * @param[in] can_id CAN ID (11-bit 또는 29-bit).
 * @param[in] data  전송할 데이터의 포인터 (const).
 * @param[in] len   전송할 데이터의 길이 (0~64 bytes).
 * @return 0 on success, <0 on error.
 */
int System_Fdcan2_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief CiA 301 SYNC를 FDCAN1(Ch1, XM↔CM)으로 전송합니다.
 * @details 1-byte rolling counter payload. UserTask(1kHz)에서 호출.
 *          ISR-Safe: IOIF Tx Mutex 우회, HAL 직접 호출.
 *          PnP Operational 진입 후에만 호출할 것 (CM_Drv_IsConnected() 체크).
 */
void System_SendSync_Ch1(void);

/**
 * @brief CiA 301 SYNC를 FDCAN2(Ch2, XM↔SM)으로 전송합니다.
 * @details 1-byte rolling counter payload. UserTask(1kHz)에서 호출.
 *          IOIF Tx 사용 (Ch2는 PnP/SDO 등 다중 Tx 소스, Mutex 보호).
 *          최소 1개 SM이 Online일 때만 호출할 것.
 * @note 어느 분기도 기다리지 않는다 — 상태 락 TryLock(0) · IOIF TransmitTry · bus-off 는
 *       flush 요청 + 백오프로 PnP/HB 경로에 맡긴다. 바쁜 tick 의 SYNC 는 건너뛴다(사유별 카운터: CanfdTx_GetCh2Stats).
 */
void System_SendSync_Ch2(void);

/**
 * @brief FDCAN1/FDCAN2 의 bus-off 복구 flush 를 서비스합니다 (IOIF ServicePeriodic wrapper).
 * @details bus-off ISR(HAL_FDCAN_ErrorStatusCallback)은 busoff_flush_pending 플래그만 세트하고,
 *          실제 flush(HW Tx abort + SW queue clear)는 task context 에서 소비해야 한다.
 *          IOIF 는 그 소비를 IOIF_FDCAN_ProcessQueue 에서 수행하는데, XM 은 direct-transmit
 *          구조(IOIF_FDCAN_Transmit 직접 호출·SW Tx Queue 미사용)라 ProcessQueue 를 호출하지
 *          않는다 → 이 wrapper 가 XM 의 유일한 소비 지점이다.
 *
 *          Ch1(FDCAN1/CM): 별도 recovery 상태머신이 없어 이 호출이 유일한 bus-off 보호.
 *          Ch2(FDCAN2/SM): s_fdcan2_* recovery 상태머신과 동일 플래그를 mutex 안 test-and-clear
 *                          로 공유 → 겹쳐도 flush 1회(idempotent).
 *
 * @note UserTask(1kHz) 등에서 **연결/모드 게이트와 무관하게 매 tick 무조건** 호출할 것.
 *       bus-off 는 링크가 끊긴 상태에서도 발생하므로 게이트 안에 넣으면 안 된다.
 * @note System_Startup() 완료 전(FDCAN ID 미할당)에 호출돼도 안전 — IOIF 내부 invalid-id
 *       가드가 크래시 없이 false 를 반환한다.
 * @note 평상시(flush 미대기)에는 lockless — 뮤텍스 오버헤드 0.
 * @note Ch2 는 서비스 직전 bus-off 세대와 서비스 결과를 남긴다 — 같은 tick 의 System_Fdcan2_TransmitUser 가 본다.
 *       그래서 이 함수와 System_Fdcan2_TransmitUser 는 **같은 태스크(UserTask)** 에서만 부른다.
 * @warning ISR 에서 호출 금지 — IOIF 내부 TX_LOCK 이 FreeRTOS mutex(task context 전용).
 */
void System_Fdcan_ServicePeriodic(void);

/**
 * @brief Notify FDCAN2 bus manager that a sensor-module bootup/heartbeat was seen.
 * @details Called from the FDCAN Rx routing task. This is non-blocking and only
 *          marks Tx cleanup for the next task-context FDCAN2 transmit.
 */
void System_Fdcan2_NotifySensorHeartbeat(uint8_t node_id, uint8_t nmt_state);

/**
 * @brief 사용자 장치용 FDCAN2 송신 — 기다리지 않는다(UserTask 전용).
 * @details 상태 락 TryLock(0) → 복구 대기(백오프 · flush 요청 · 이번 tick 의 IOIF 결과 · bus-off 세대 불일치 ·
 *          bus-off 반영 대기)면 거부 → IOIF_FDCAN_TransmitTry(11비트 — FD 모드는 FD+BRS, 클래식 모드는 IOIF 가 클래식으로).
 *          presence · flush 서비스와 채널 취소는 부르지 않는다. 사용 불가 모드면 INVALID.
 * @param[in] can_id 11비트 표준 ID — **어떤 ID 를 보내도 되는지는 호출자(사용자 장치 브리지)가 검사한다**
 * @param[in] data   보낼 데이터 — 길이 0 이면 NULL 이어도 된다(길이 > 0 인데 NULL 이면 INVALID)
 * @param[in] len    FD 0~64 (허용 길이가 아니면 IOIF 가 다음 허용 길이까지 0 으로 채운다) · 클래식 0~8
 * @return Fdcan2UserTxResult_t — OK 는 '큐 수락' 이다. 세대를 읽은 뒤 적재 전에 새 bus-off 가 나면 수락한 명령도
 *         후속 서비스의 취소 대상이 될 수 있다(시간 상한 · 취소 완료 시점 비보장).
 */
Fdcan2UserTxResult_t System_Fdcan2_TransmitUser(uint32_t can_id, const uint8_t* data, uint8_t len);

/**
 * @brief 살아 있는 사용자 장치가 있는지 알린다 — 빈 버스 flush 판정(bus-live)에 쓴다. UserTask 단일 Writer.
 * @note  기본값 false 면 Ch2 동작은 사용자 장치가 없던 때와 같다.
 */
void CanfdTx_SetUserBusLive(bool live);

/** @brief Ch2 송신 경로 통계를 복사한다(필드별 약한 스냅숏). out 이 NULL 이면 아무것도 안 한다. */
void CanfdTx_GetCh2Stats(CanfdTxCh2Stats_t* out);

/**
 * @brief 지금 Ch2 하드웨어 송신 대기 수(TXBRP 비트 수) — 락 없는 레지스터 읽기, 어느 태스크에서나.
 * @note  비트는 송신 완료 또는 취소 완료에서만 내려간다(M_CAN) — 0 이면 그 앞에 넣은 프레임은 모두 나갔거나 취소됐다.
 *        사용자 장치 브리지의 송신 정리(T_clear) 관측이 쓴다. 채널 미할당이면 0.
 */
uint32_t CanfdTx_GetCh2TxPending(void);

/** @brief Ch2 오류 카운터 · 버스 상태(호출 시 레지스터 읽기). 읽지 못하면 false 이고 출력은 0 */
bool CanfdTx_GetCh2Health(uint8_t* tec, uint8_t* rec, uint8_t* bus_status);

#endif /* SYSTEM_COMM_CANFD_CANFD_TX_H_ */
