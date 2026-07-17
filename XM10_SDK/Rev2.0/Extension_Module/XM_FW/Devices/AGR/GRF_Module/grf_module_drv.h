/**
 ******************************************************************************
 * @file    grf_module_drv.h
 * @author  HyundoKim
 * @brief   [Devices/AGR] SM-GRF Module 수신 드라이버 — fixed UART frame
 * @version 1.0
 * @date    2026-07-04
 *
 * @details
 *  XM(Master) ← SM-GRF(Slave) 링크. SM-GRF 는 발당 1개씩 장착되는 지면반력
 *  센서 모듈(24ch FSR + 6축 IMU + NN)이며, **동일 펌웨어**가 좌/우에 올라간다.
 *  좌/우 구분은 **물리 포트**로 한다 (사람이 오른발 GRF 를 XM 오른쪽 포트에):
 *      ch=0 LEFT  ← UART7 (system_startup 에서 배선)
 *      ch=1 RIGHT ← UART8
 *  두 GRF 는 point-to-point UART 라 **동일 Node ID(0x11)** 여도 충돌 없음
 *  (포트가 곧 신원). 공유 FDCAN PnP master 미사용 → 포트별 독립 컨텍스트.
 *
 * [MarvelDex(MDAF-25-6850) 미러 — 5-함수 DataLake + Auto-Sense]
 *  GRF UART 경로는 1kHz fixed binary frame 단일 스트림이다. XM 은
 *  IOIF UART RxTask 컨텍스트에서 sync/length/
 *  checksum 을 확인한 뒤 DataLake 로 스냅샷한다. 생존감지는 frame freshness
 *  기준 — 500ms 무수신이면 STOPPED.
 *
 * [Fixed frame wire 계약 — SM-GRF xm_link_drv.c 와 정합 (74B frame, LE)]
 *      [ 0 ]     sync0       0xA5
 *      [ 1 ]     sync1       0x5A
 *      [ 2 ]     version     0x01
 *      [ 3 ]     type        0x01 (GRF data)
 *      [ 4 ]     sequence    uint8, wrap allowed
 *      [ 5 ]     payload_len 66
 *      [ 0..47 ] 0x6020 FSR  uint16[24]  = 48B
 *      [48..61 ] 0x6021 IMU   int16[7]   = 14B (acc[3],gyr[3],temp)
 *      [62..65 ] 0x6050 tick  uint32     =  4B (GRF 1kHz 제어틱, gap 검출용)
 *      [72..73 ] checksum    uint16 LE, CRC-16/CCITT-FALSE(frame[2..71])
 *
 * [스레드 모델]
 *  Writer = IOIF UART 공유 RxTask (RxIdle 콜백 → fixed frame parser), prio 54.
 *  Reader = Core Process (GetLatest), prio 53. / PnP Task(LED), prio 25.
 *  Mutex + Snapshot (13-comm-core-patterns). osMutex(PrioInherit).
 *  timeout 은 XM 표준대로 역할별 분리 — **Reader 0 / Writer 1 tick**.
 *   - Reader 0: 획득 실패 시 false 반환 → 직전 snapshot 유지 (제어 루프 블로킹 금지).
 *   - Writer 1: Reader 가 mutex 를 쥔 채 선점당한 경우(RxTask 54 > UserTask 53 →
 *     priority inversion) PrioInherit 가 Reader 를 boost 해 즉시 해제시키게 한다.
 *     timeout=0(trylock)이면 '대기'가 없어 PI 가 영원히 발동 못 하고, inversion 마다
 *     프레임을 조용히 버린다 (2026-07-17 실측 6667 drop = 0.17%). 임계구역이 76B 구조체
 *     복사뿐이라 실 대기는 sub-us — 1 tick 은 도달할 일 없는 상한이며, 도달 시 거동은
 *     trylock 과 동일(drop)하다.
 *  RxTask 는 태스크 컨텍스트(ISR 아님) → mutex 사용 안전.
 *  [drop-free 관측] g_dbg_grf_last_parsed[] / g_dbg_grf_parsed_count[] 는 mutex '전'에
 *  무조건 갱신 → snapshot drop 과 무관하게 모든 CRC-valid 프레임을 담는다.
 *  관계식: g_dbg_grf_parsed_count == frame_count + snapshot_drop_count.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_AGR_GRF_MODULE_GRF_MODULE_DRV_H_
#define DEVICES_AGR_GRF_MODULE_GRF_MODULE_DRV_H_

#include <stdint.h>
#include <stdbool.h>

#include "ioif_agrb_uart.h"   /* IOIF_UARTx_t */

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief 최대 인스턴스 수 (좌/우 발). module.h 에서 오버라이드 가능. */
#ifndef GRF_MODULE_MAX_INSTANCES
#define GRF_MODULE_MAX_INSTANCES    (2)
#endif

/** @brief 채널 인덱스 (MarvelDex 규약과 동일: 0=좌, 1=우) */
#define GRF_MODULE_CH_LEFT          (0)
#define GRF_MODULE_CH_RIGHT         (1)

/** @brief FSR 채널 수 (SM-GRF: ADC1 15ch + ADC3 9ch = 24). wire 계약 고정값. */
#define GRF_MODULE_FSR_CH_TOTAL     (24)

/** @brief Fixed-frame payload 크기 (FSR 48B + IMU 14B + tick 4B). */
#define GRF_MODULE_FIXED_PAYLOAD_BYTES  (48 + 14 + 4)   /* = 66 */

/** @brief Fixed binary frame constants. */
#define GRF_MODULE_FRAME_SYNC0       (0xA5U)
#define GRF_MODULE_FRAME_SYNC1       (0x5AU)
#define GRF_MODULE_FRAME_VERSION     (0x01U)
#define GRF_MODULE_FRAME_TYPE_DATA   (0x01U)
#define GRF_MODULE_FIXED_FRAME_BYTES (2U + 1U + 1U + 1U + 1U + GRF_MODULE_FIXED_PAYLOAD_BYTES + 2U)

/** @brief Auto-Sense 타임아웃 (ms). GRF 1kHz 스트림 → 500ms 무수신 = 단선. */
#ifndef GRF_MODULE_AUTOSENSE_TIMEOUT_MS
#define GRF_MODULE_AUTOSENSE_TIMEOUT_MS (500U)
#endif

/**
 * @brief update_gap_cycles_max 통계 상한 (DWT cycle). 초과 간격은 max 갱신에서 제외.
 * @details 480MHz × 10ms = 4,800,000. 상한의 유일한 목적은 "정상 주기 지터"와 "통신 공백"의
 *          분리다. 500ms 이상 단선은 GrfModule_RunPeriodic 의 s_last_update_cycles=0 리셋이
 *          이미 구조적으로 배제하므로, 이 상한은 타임아웃에 못 미치는 짧은 순단만 걸러내는
 *          backstop 이다.
 *          ⚠️ 구값 960000(2ms)은 snapshot drop 1회의 간격(≈2×485000=970000cyc)보다 낮아
 *          정작 관측 대상인 drop 흔적을 max 통계에서 전부 배제했다 (2026-07-17 실측:
 *          drop 6667 건에도 max 가 캡 경계 960000 에 붙어 있었음). 10ms 로 올려 drop
 *          최대 ~9 연속까지 관측 가능하게 한다.
 */
#ifndef GRF_MODULE_UPDATE_GAP_MAX_CAP_CYC
#define GRF_MODULE_UPDATE_GAP_MAX_CAP_CYC (4800000UL)   /* 10ms @480MHz */
#endif

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief IMU raw 블록 (14B) — SM-GRF GrfOd_ImuRaw_t 바이트 미러 (host int16 = LE).
 */
typedef struct __attribute__((packed)) {
    int16_t acc[3];    /**< X/Y/Z 가속도 raw */
    int16_t gyr[3];    /**< X/Y/Z 자이로 raw */
    int16_t temp;      /**< 다이 온도 raw */
} GrfModule_Imu_t;     /* 14 bytes */

/**
 * @brief GRF 모듈 1개(한 발)의 최신 수신 데이터 (DataLake 스냅샷 단위).
 */
typedef struct {
    uint16_t         fsr[GRF_MODULE_FSR_CH_TOTAL];  /**< 24ch FSR raw (ADC LSB) */
    GrfModule_Imu_t  imu;                            /**< 6축 IMU + temp raw */
    uint32_t         grf_tick_ms;                    /**< GRF 제어틱(ms) — gap/freshness 검출 */
    uint32_t         rx_tick_ms;                     /**< XM 로컬 수신 시각(ms, IOIF_TIM_GetTick) */
    uint8_t          rolling_index;                  /**< fixed frame sequence */
} GrfModule_Data_t;

/**
 * @brief Live-expression용 fixed-frame 수신 진단.
 */
typedef struct {
    uint32_t rx_chunk_count;
    uint32_t rx_byte_count;
    uint32_t frame_count;
    uint32_t checksum_error_count;
    uint32_t sync_drop_count;
    uint32_t resync_count;        /**< 검증 실패 → 윈도 내부 재동기 횟수 (오정렬 궤도 검출) */
    uint32_t version_drop_count;
    uint32_t type_drop_count;
    uint32_t length_drop_count;
    uint32_t snapshot_drop_count;
    uint32_t seq_gap_count;
    uint32_t timeout_count;
    uint32_t last_seq;
    uint32_t last_grf_tick_ms;
    uint32_t last_rx_tick_ms;
    uint32_t parser_pos;
    uint32_t online;
    /* [F14 지연계측] DWT CYCCNT 기반(480MHz → 1us=480cyc). one-way 절대지연은 두 MCU
     * 클럭 비동기라 불가 — 여기선 XM 내부 처리비용과 프레임 도착주기 지터를 관측한다.
     * end-to-end 절대지연은 오실로 GPIO-토글(양 보드) 필요(F17). */
    uint32_t proc_cycles_last;        /**< _RxCallback 1회 처리 소요 cycle (CPU 비용) */
    uint32_t update_gap_cycles_last;  /**< DataLake 갱신 간 간격 cycle (≈480000=1ms) */
    uint32_t update_gap_cycles_max;   /**< 위 간격의 관측 최대 (지터 상한).
                                       *   GRF_MODULE_UPDATE_GAP_MAX_CAP_CYC 초과분은 제외 —
                                       *   값이 캡에 붙어 있으면 '초과 간격이 잘렸다'는 신호. */
} GrfModule_Diag_t;

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 드라이버 초기화 — 포트별 fixed-frame parser + DataLake + RxIdle 콜백 배선.
 * @param id_left   좌발 GRF UART 핸들 (UART7). 미사용 시 IOIF_UART_ID_NOT_ALLOCATED.
 * @param id_right  우발 GRF UART 핸들 (UART8).
 * @return true: 성공. Call context: system_startup (태스크 시작 전).
 * @note  UART AssignInstance/Start 는 호출자(system_startup)가 먼저 수행하고
 *        얻은 핸들을 넘긴다 (MarvelDex initialize 규약과 동일).
 */
bool GrfModule_Init(IOIF_UARTx_t id_left, IOIF_UARTx_t id_right);

/**
 * @brief [Reader] 최신 스냅샷 획득 (Core Process). Mutex + Snapshot.
 * @param ch   GRF_MODULE_CH_LEFT / _RIGHT
 * @param out  복사 대상 (NULL 무시)
 * @return true: 데이터 유효(OPERATIONAL), false: 단선/미수신
 */
bool GrfModule_GetLatest(uint8_t ch, GrfModule_Data_t* out);

/**
 * @brief Auto-Sense 연결 상태.
 * @return true: OPERATIONAL (수신 중), false: STOPPED (타임아웃/미연결)
 */
bool GrfModule_IsOnline(uint8_t ch);

/**
 * @brief 누적 유효 fixed-frame 수신 수 (포트별).
 * @param ch   GRF_MODULE_CH_LEFT / _RIGHT
 * @return 누적 RX 카운트 (미연결/무효 ch = 0).
 * @note  생산검사 진단(OD 0x7E70:01)이 XM_GRF_FIXED_FRAME_MODULE=1 일 때 MarvelDex
 *        total_packets 대신 이 값(L+R 합)으로 "GRF UART 링크 활성"을 본다.
 *        Writer=RxTask(_UpdateData), Reader=진단 task — 단일 word 카운터라
 *        별도 락 불필요(monotonic, Cortex-M7 32-bit 정렬 접근 원자적).
 */
uint32_t GrfModule_GetRxCount(uint8_t ch);

/**
 * @brief Auto-Sense 주기 실행 (~100ms). Fixed-frame 무수신 타임아웃 → STOPPED.
 */
void GrfModule_RunPeriodic(uint8_t ch);

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES
 *-----------------------------------------------------------
 */

extern volatile GrfModule_Diag_t g_dbg_grf_module_diag[GRF_MODULE_MAX_INSTANCES];

#endif /* DEVICES_AGR_GRF_MODULE_GRF_MODULE_DRV_H_ */
