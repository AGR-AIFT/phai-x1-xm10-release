/**
 ******************************************************************************
 * @file    agr_sdo_master.h
 * @author  HyundoKim
 * @brief   AGR SDO Master (Client) - response/readback/abort/timeout tracking
 * @version 1.0
 * @date    Jun 19, 2026
 *
 * @details
 * Master 측 SDO request 의 완료(write-ack / read-back / abort / timeout)를
 * transaction 풀로 추적하는 transport-agnostic Core API.
 * Slave 측 `AGR_SDO_ProcessRequest` 와 대칭: 여기는 `AGR_SDO_Master_ProcessResponse`.
 *
 * Types (AGR_SDO_Master_t / AGR_SDO_Transaction_t / AGR_SDO_CompletionCb_t /
 * AGR_SDO_Tx_State_e) 는 순환 include 회피를 위해 agr_dop_types.h 에 정의됨.
 * 본 헤더는 함수 API 만 보유. Design SSOT: docs/plan_sdo_master.md.
 *
 * WHY CALLBACK-ONLY (no polling, no blocking)
 * ===========================================
 * (1) Per-request callback (not global hook):
 *     Slave-side on_sdo_request 는 들어오는 요청의 OD index 를 모르므로 global.
 *     Master-side 는 반대 — 우리가 각 요청을 개시하고 context 를 안다. Per-request
 *     callback 으로 요청 코드 근처에 완료 처리를 두고, request 별 user_ctx 를 구분.
 * (2) No polling mode:
 *     on_done==NULL 허용 시 caller 가 tx->state 를 직접 peek → middleware 내부
 *     결합. callback 이 caller 소유 flag 를 세팅하고 main-loop state machine 이
 *     그 flag 를 보는 패턴(Write→ack flag→next Write)이 PnP 시퀀스의 자연스러운 길.
 * (3) No blocking primitive:
 *     OS 세마포어를 AGR_MW 에 넣으면 RTOS 의존 → 이식성 훼손. Blocking 필요 시
 *     caller 가 10줄로 wrap (osSemaphoreNew → release_sem_cb → osSemaphoreAcquire).
 *     BareMetal 은 flag + state-machine 패턴 사용.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_SDO_MASTER_H
#define AGR_SDO_MASTER_H

#include <stdint.h>
#include "agr_dop_types.h"   /* AGR_SDO_Master_t, AGR_SDO_Transaction_t, AGR_SDO_Msg_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SDO Master 초기화 (풀 zero-init + tick source 주입).
 * @param m            Master 인스턴스 (보통 &ctx->sdo_master).
 * @param get_tick_ms  ms tick DI (timeout 계측용). NULL 이면 timeout 미동작.
 */
void AGR_SDO_Master_Init(AGR_SDO_Master_t* m, uint32_t (*get_tick_ms)(void));

/**
 * @brief Transaction 슬롯 할당 + 즉시 PENDING 클레임 (single-in-flight enforce).
 * @return NULL if (1) 같은 slave 에 이미 in-flight 존재, (2) 풀 가득 참.
 * @note  반환 슬롯은 이미 state=PENDING, slave_id 세팅됨. 호출자(transport
 *        Request)가 index/subindex/data/on_done/sent_tick 을 채운다. 송신 실패
 *        시 호출자가 state=AGR_SDO_TX_IDLE 로 되돌려 슬롯 반환.
 */
AGR_SDO_Transaction_t* AGR_SDO_Master_AllocTx(AGR_SDO_Master_t* m, uint8_t slave_id);

/**
 * @brief 응답 처리 — Transport 가 SDO response/abort 수신 시 호출.
 * @details (from_slave, rsp.index, rsp.subindex) 매칭으로 PENDING transaction
 *          탐색. 매칭 시 CS 로 결과 분류(write-ack/read/abort) → on_done 호출 →
 *          슬롯 반환(IDLE).
 * @return 0 매칭/처리 성공, -1 인자 오류, -2 orphan(매칭 없음, 폐기).
 */
int32_t AGR_SDO_Master_ProcessResponse(AGR_SDO_Master_t*    m,
                                       uint8_t              from_slave,
                                       const AGR_SDO_Msg_t* rsp);

/**
 * @brief Timeout 체크 — comm task / main loop 에서 주기 호출 (AllocTx 와 동일 컨텍스트).
 * @param current_ms  현재 tick (ms). get_tick_ms 와 동일 시계.
 */
void AGR_SDO_Master_Tick(AGR_SDO_Master_t* m, uint32_t current_ms);

#ifdef __cplusplus
}
#endif

#endif /* AGR_SDO_MASTER_H */
