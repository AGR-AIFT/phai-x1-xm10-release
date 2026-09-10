/**
 ******************************************************************************
 * @file    agr_vector.h
 * @author  HyundoKim
 * @brief   AGR DOP Command Vector — deterministic 제어입력 채널 (Core)
 * @version 1.0
 * @date    Jul 2, 2026
 *
 * @details
 * P/F/I trajectory, ES vector 등 "제어 입력" 클래스 메시지를 SDO(main-loop
 * deferred)가 아니라 **단일 atomic 프레임 + 소비 모듈의 제어 tick**에서
 * 결정적으로 처리하기 위한 채널. FES(ES Vector)에서 검증된 와이어 계약을
 * Core 로 승격한 것 (2026-07-02, 전 모듈 compat 검토 후).
 *
 * [와이어 계약]
 *   Request(0x100+Node): [vector_type u8][seq u8][len u8][payload…]
 *   ACK    (0x680+Node): [echo_seq u8][status u8]   — 항상 2B (정확 DLC)
 *   EVENT  (0x680+Node): [event_code u8]            — 항상 1B, code = 0x80|type
 *   ACK 채널의 frame 종별은 **길이로 판별** (2B=ACK / 1B=EVENT). 두 frame 모두
 *   8B 이하이므로 CAN(-FD) DLC 가 정확 길이를 보존한다 — 패딩 금지.
 *
 * [seq 멱등 계약]
 *   - OK 적용된 seq 만 last_seq 로 기록. 동일 seq 재수신 = 재실행 없이 재-ACK.
 *   - 에러 status 는 last_seq 를 전진시키지 않음 → master 가 payload 수정 후
 *     같은 seq 로 재시도 가능.
 *
 * [Core 역할 경계]
 *   Core = 프레임 파싱 + seq 멱등 + 길이 검증 + mailbox stage + on_apply 호출.
 *   BUSY/RANGE/WRONG_STATE(NMT) 판정과 tick 스케줄링(예: MD 의 ISR-ring →
 *   mid-level 1kHz drain)은 소비 모듈 책임. Core 는 순수 C / Context 기반 /
 *   reentrant(per-ctx) / non-blocking / no HAL·RTOS.
 *
 * 설계 SSOT: SAM3x_FW
 * `MD_FW/Devices/AGR/Control_Module/Doc/md_command_vector_design.md`
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_VECTOR_H
#define AGR_VECTOR_H

#include "agr_dop_types.h"

/**
 *-----------------------------------------------------------
 * SLAVE — 등록 / Request 처리
 *-----------------------------------------------------------
 */

/**
 * @brief Vector type 등록 (slave, Init 단계 전용 — opt-in 스위치)
 * @param ctx      DOP Context
 * @param type     vector_type 코드 (예: 0x10 P / 0x11 I / 0x12 F / 0x13 F-zero)
 * @param mailbox  staging 버퍼 (모듈 소유, aligned struct 권장). size==0 이면 NULL 허용
 * @param size     기대 payload 길이 (0 ~ AGR_VECTOR_MAX_PAYLOAD). 수신 len 과
 *                 정확 일치해야 apply (불일치 = BAD_LEN ACK)
 * @param on_apply 적용 콜백 (필수). 반환값 = AGR_VectorStatus_t
 * @return 0=성공, -1=인자 오류(mailbox/size/on_apply/oversize), -2=테이블 가득,
 *         -3=type 중복
 * @note  하나라도 등록되면 이 ctx 는 자기 노드 앞 0x100+N 프레임을 소비(ACK)
 *        하기 시작한다. 등록 0 = 채널 완전 관성 (기존 pass-through 동일).
 */
int32_t AGR_Vector_RegisterType(AGR_DOP_Ctx_t* ctx,
                                uint8_t type,
                                void* mailbox,
                                uint16_t size,
                                AGR_VectorApply_fn on_apply);

/**
 * @brief Request 프레임 처리 (파싱 + 멱등 + 검증 + stage + apply)
 * @param ctx     DOP Context
 * @param data    수신 payload ([type][seq][len][payload…])
 * @param len     수신 길이 (CAN-FD DLC 패딩 허용 — frame 내 len 필드가 진실)
 * @param ack_out [out] 2B ACK ([echo_seq][status]) — 반환 0 일 때만 유효
 * @return 0=ack_out 유효(전송할 것), 1=미등록 ctx (pass-through, ACK 없음),
 *         -1=인자 오류, -2=malformed (len<3 — seq 없어 ACK 불가, diag 카운트)
 * @note  호출 컨텍스트는 모듈이 정한다 (MD: mid-level tick 의 ring drain —
 *        on_apply 가 그 tick 에서 실행되어 결정성 확보). ctx 당 단일 컨텍스트
 *        호출 계약 (agr_dop_types.h Command Vector 주석 참조).
 */
int32_t AGR_Vector_HandleRequest(AGR_DOP_Ctx_t* ctx,
                                 const uint8_t* data,
                                 uint8_t len,
                                 uint8_t ack_out[2]);

/**
 *-----------------------------------------------------------
 * MASTER — ACK/EVENT 소비 (Optional)
 *-----------------------------------------------------------
 */

/**
 * @brief Master 측 ACK/EVENT 콜백 등록 (Init 단계 전용)
 * @param ctx      DOP Context
 * @param on_ack   ACK([echo_seq][status]) 수신 콜백 (NULL 허용)
 * @param on_event EVENT([event_code]) 수신 콜백 (NULL 허용, 예: P-Done 0x90)
 * @return 0=성공, -1=인자 오류
 * @note  둘 다 NULL 이면 fnc 0x0D 는 pass-through 유지 (관성).
 */
int32_t AGR_Vector_SetMasterCallbacks(AGR_DOP_Ctx_t* ctx,
                                      void (*on_ack)(uint8_t source_node,
                                                     uint8_t echo_seq,
                                                     uint8_t status,
                                                     void* user_ctx),
                                      void (*on_event)(uint8_t source_node,
                                                       uint8_t event_code,
                                                       void* user_ctx));

/**
 * @brief ACK 채널(0x680+N) 프레임 처리 — 길이로 ACK/EVENT 판별 후 콜백 라우팅
 * @param ctx         DOP Context
 * @param source_node 송신 slave node (can_id & 0x7F — 필터링은 콜백 측 책임)
 * @param data        수신 payload
 * @param len         수신 길이 (2=ACK, 1=EVENT, 그 외=malformed)
 * @return 0=콜백 소비, 1=콜백 미등록/해당 없음 (pass-through), -1=인자 오류,
 *         -2=malformed 길이 (diag 카운트)
 */
int32_t AGR_Vector_HandleAckFrame(AGR_DOP_Ctx_t* ctx,
                                  uint8_t source_node,
                                  const uint8_t* data,
                                  uint8_t len);

/**
 *-----------------------------------------------------------
 * ENCODE (transport 공용 헬퍼)
 *-----------------------------------------------------------
 */

/**
 * @brief Request 프레임 인코드 ([type][seq][len][payload…])
 * @param type     vector_type 코드
 * @param seq      시퀀스 번호 (per-node 단조 증가는 master 책임)
 * @param payload  payload (len==0 이면 NULL 허용)
 * @param len      payload 길이 (≤ AGR_VECTOR_MAX_PAYLOAD)
 * @param buf      [out] 인코드 버퍼
 * @param buf_size 버퍼 크기
 * @return 프레임 길이(len+3) 또는 <0 (-1=인자/oversize, -2=버퍼 부족)
 */
int32_t AGR_Vector_BuildRequest(uint8_t type,
                                uint8_t seq,
                                const void* payload,
                                uint8_t len,
                                uint8_t* buf,
                                uint16_t buf_size);

#endif /* AGR_VECTOR_H */
