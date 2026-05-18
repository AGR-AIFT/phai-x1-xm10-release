/**
 ******************************************************************************
 * @file    xm_imu_diag.h
 * @author  HyundoKim
 * @brief   [System/Diag] XM ↔ IMU Hub CAN-FD baseline 진단 변수 (Live Expression)
 * @date    2026-04-21
 *
 * @details
 * STM32CubeIDE "Live Expression" 창에서 실시간 관찰할 XM(Master) 측 진단 카운터.
 * 대응되는 IMU(Slave) 측 카운터는 imu_hub_apps.h 의 PerfStats_t g_perf_stats.
 *
 * 분류 (IMU 측 Live Expression 변수 정리 문서 α/β/γ/δ 대응):
 *   α) Per-slave RX message count (tpdo1/tpdo2/hb/sdo_rsp/emcy)
 *   β) ctrl_tick gap detection — IMU TPDO1 Metadata timestamp(24-bit ms) 기반
 *      IMU OD 0x6050 UINT32 ctrl_tick_ms 는 등록만 되고 TPDO mapping 에는
 *      포함되지 않으므로, XM 은 PDO Metadata(4B) = Timestamp(3B)+ValidMask(1B)
 *      의 24-bit ms 값으로 gap 판정 (wrap 주기 ≈ 4.66 시간).
 *   γ) XM 자체 Tx/Err (sync_tx/sdo_req/sdo_timeout/bus_off/err_passive/tec/rec)
 *   δ) Slave 연결 상태 (last_hb_rx_ms/hb_timeout_count/nmt_state)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef SYSTEM_DIAG_XM_IMU_DIAG_H_
#define SYSTEM_DIAG_XM_IMU_DIAG_H_

#include <stdint.h>
#include <stdbool.h>

/* ===== Slave Index =====
 * IMU Hub 펌웨어는 단일 Node 0x0D 로 TPDO1(0x18D) + TPDO2(0x28D) 를 모두 송출 (CiA 301).
 * 6ch IMU 120B 를 TPDO1(IMU0/1/2) + TPDO2(IMU3/4/5) 로 분할. CAN-FD 64B 상한 대응.
 *
 * xm_node_id.h 의 IMU_HUB_B(0x0E) 는 초기 dual-hub 설계안 예약 흔적. 현재 FW 는
 * 단일 hub 로 통합 → 진단 slot 도 1개. Live Expression 은 g_xm_imu_diag[0] 만 관측. */
#define XM_IMU_SLAVE_COUNT       1
#define XM_IMU_SLAVE_IDX_A       0

/* 24-bit timestamp wrap mask (PDO Metadata Timestamp 필드) */
#define XM_IMU_TS_MASK_24BIT     0x00FFFFFFu

/**
 * @brief Per-slave RX/ctrl_tick/NMT 진단 카운터 (α + β + δ)
 */
typedef struct {
    /* ----- α) RX message counters ----- */
    uint32_t tpdo1_count;
    uint32_t tpdo2_count;
    uint32_t hb_count;
    uint32_t sdo_rsp_count;
    uint32_t emcy_count;

    /* ----- β) ctrl_tick_ms (24-bit) gap detection ----- */
    uint32_t last_ctrl_tick_ms;     /**< 하위 24-bit (0~0x00FFFFFF) */
    uint32_t ctrl_tick_gap_count;   /**< delta != 1 발생 수 */
    uint32_t ctrl_tick_gap_max;     /**< 관측된 최대 gap (ms) */
    uint32_t ctrl_tick_gap_sum;     /**< 누락 tick 총합 Σ(delta-1) */
    uint8_t  ctrl_tick_initialized; /**< 0 → 첫 수신 전 */

    /* ----- δ) Slave connection ----- */
    uint32_t last_hb_rx_ms;         /**< osKernelGetTickCount() 마지막 HB 수신 */
    uint32_t hb_timeout_count;      /**< _PnP_OnOffline 호출 횟수 */
    uint8_t  nmt_state;             /**< AGR_NMT_State_t (0/4/5/0x7F) */
} XM_ImuSlaveDiag_t;

/**
 * @brief XM 자체 FDCAN Tx/Err + 공용 RX 진단 (γ + 추가 제안)
 */
typedef struct {
    /* ----- γ) XM Tx counters ----- */
    uint32_t sync_tx_count;             /**< 0x080 SYNC 송신 누적 (1kHz) */
    uint32_t sdo_req_count;             /**< SDO Req 송신 누적 */
    uint32_t sdo_timeout_count;         /**< SDO 응답 timeout 누적 */

    /* ----- γ) FDCAN 에러/상태 (1Hz poll) ----- */
    uint32_t fdcan_bus_off_count;       /**< Bus-Off 진입 에지 카운트 */
    uint32_t fdcan_err_passive_count;   /**< Error-Passive 진입 에지 카운트 */
    uint8_t  fdcan_tec;                 /**< Transmit Error Counter (0~255) */
    uint8_t  fdcan_rec;                 /**< Receive Error Counter (0~127) */
    uint8_t  fdcan_bus_status;          /**< bit0:BusOff bit1:Warning bit2:ErrorPassive */
    uint8_t  fdcan_lec;                 /**< Last Error Code (PSR.LEC) */

    /* ----- 추가 제안 ----- */
    uint32_t rx_total_count;            /**< _OnFdcanRxMessage 총 진입 수 */
    uint32_t rx_fifo0_peak;             /**< Rx FIFO0 drain 직후 잔여 fill peak
                                         *   (IOIF RxTask 가 따라잡지 못해 쌓인 backlog 지표,
                                         *    0 = IOIF 가 HW FIFO 를 완벽히 비우고 있음) */
    uint32_t non_realtime_queue_peak;   /**< NRT queue depth peak (enqueue 직후 기준) */
    uint32_t classify_unknown_drop;     /**< _ClassifyAndRoute default drop */
} XM_BusDiag_t;

/* ===== Live Expression 타겟 (volatile 필수) ===== */
extern volatile XM_ImuSlaveDiag_t g_xm_imu_diag[XM_IMU_SLAVE_COUNT];
extern volatile XM_BusDiag_t      g_xm_bus_diag;

/* ===== Helpers ===== */

/**
 * @brief Node ID → slave_idx 변환
 * @return 0=IMU_HUB_A, -1=그 외 (진단 대상 아님)
 * @note 0x0E 는 legacy dual-hub 예약. 실제 송출 없음 → -1 로 drop.
 *       만약 0x0E 가 관측되면 비정상 (classify_unknown_drop 으로 집계되지는 않고
 *       Node-mismatch 이므로 _RouteRealtimeV2 의 unknown_drop_count 에 쌓임).
 */
__attribute__((unused))
static inline int XM_Diag_NodeToSlaveIdx(uint8_t node_id)
{
    if (node_id == 0x0D) return XM_IMU_SLAVE_IDX_A;
    return -1;
}

/**
 * @brief ctrl_tick (24-bit ms) gap 판정 (TPDO1 디코딩 지점에서 호출)
 * @param slave_idx  0 또는 1
 * @param tick_24bit PDO Metadata 의 3B timestamp (이미 XM 측에서 조립된 값)
 * @note wrap 주기 4.66h. delta = (current - last) & 0x00FFFFFF.
 *       정상 = 1, gap = delta > 1.
 */
void XM_Diag_UpdateCtrlTick(int slave_idx, uint32_t tick_24bit);

/** @brief HB 수신 시 호출 (0x700 + node_id RX 경로) */
void XM_Diag_MarkHbRx(int slave_idx, uint32_t now_ms);

/** @brief HB timeout 발생 (PnP Master _OnOffline 콜백) */
void XM_Diag_MarkHbTimeout(int slave_idx);

/** @brief NMT 상태 업데이트 (on_slave_state_changed) */
void XM_Diag_UpdateNmtState(int slave_idx, uint8_t nmt_state);

/** @brief 1Hz 주기 호출 — TEC/REC/BusStatus poll + Bus-Off/Passive 에지 감지 */
void XM_Diag_PollBusStatus(void);

#endif /* SYSTEM_DIAG_XM_IMU_DIAG_H_ */
