/**
 ******************************************************************************
 * @file    agr_pnp_identify.h
 * @author  HyundoKim
 * @brief   AGR PnP IDENTIFY — Slave identity 비교검증 상태머신 (transport-agnostic)
 * @version 1.0.0
 * @date    2026-06-11
 *
 * @details
 * Master가 Pre-Op 진입 시 Slave의 0x1000(device_type)과 0x1018:01~03
 * (vendor/product/revision)을 SDO Read로 읽어 컴파일 타임 기대값과 비교한다.
 * CiA 302-2 boot-slave / EtherCAT SII↔ENI 검증과 동일 모델 — Master의
 * 사전지식(매뉴얼 하드코딩)을 물리 연결 시점에 비준(ratify)한다.
 *
 * [Revision = 와이어 계약 버전]
 * Slave는 OD/와이어 계약 변경 시 0x1018:03을 반드시 bump한다. Master의
 * expected.revision 비교가 매뉴얼 drift를 현장이 아닌 부팅 시점에 검출한다.
 *
 * [Transport 독립성 — 과한 추상화 금지 원칙]
 * 본 컴포넌트는 SDO 메시지 레벨(AGR_SDO_Msg_t)만 다룬다. 송신은 DI 함수
 * 포인터(send_read)로 위임 — CAN-FD는 AGR_CANFD_SendSDO/AGR_CANFD_Master_Request,
 * 보드 간 UART Serial DOP는 AGR_Serial_Master_Request 래퍼로 재사용한다.
 * EtherCAT은 SOES/SOEM 전담이다. USB-CDC GUI 세션은 host가 포트를 열고
 * 진단 SDO를 질의하는 모델이라 일반적으로 embedded PnP 대상이 아니지만,
 * UART 보드 간 링크는 bootup/HB/NMT/IDENTIFY PnP 의미론을 사용한다.
 *
 * [구동 계약 — Device Driver Pre-Op SM]
 * 1) Boot-up 콜백(Rx 컨텍스트)에서는 pending 마킹만 한다.
 * 2) 슬롯 획득(AGR_PnP_Master_TryAcquirePreOpSlot) + Start()는
 *    PnP Task(RunPeriodic) 단일 컨텍스트에서만 호출한다.
 * 3) SDO Upload 응답/Abort를 OnSdoResponse()로 주입한다.
 * 4) RunPeriodic()이 타임아웃/재시도를 처리한다.
 * 5) PASS 후에만 CONFIG(TPDO 매핑→NMT START)로 진행, MISMATCH면
 *    NMT START를 보류한다 (fail loud).
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_PNP_IDENTIFY_H
#define AGR_PNP_IDENTIFY_H

#include <stdint.h>
#include <stdbool.h>
#include "agr_dop_types.h"   /* AGR_SDO_Msg_t, AGR_SDO_CS_* */

/**
 *-----------------------------------------------------------
 * CONFIGURATION DEFAULTS
 *-----------------------------------------------------------
 */

#define AGR_IDENTIFY_TIMEOUT_MS_DEFAULT   5000U  /**< 단계별 응답 타임아웃 (기존 Pre-Op 패턴) */
#define AGR_IDENTIFY_MAX_RETRY_DEFAULT    3U     /**< 단계별 재시도 횟수 */

/**
 *-----------------------------------------------------------
 * TYPES
 *-----------------------------------------------------------
 */

/** @brief 검증 단계 (SDO Read 순서) */
typedef enum {
    AGR_IDENTIFY_STEP_DEVICE_TYPE = 0,  /**< 0x1000:00 */
    AGR_IDENTIFY_STEP_VENDOR_ID,        /**< 0x1018:01 */
    AGR_IDENTIFY_STEP_PRODUCT_CODE,     /**< 0x1018:02 */
    AGR_IDENTIFY_STEP_REVISION,         /**< 0x1018:03 — 와이어 계약 버전 */
    AGR_IDENTIFY_STEP_COUNT,
} AGR_Identify_Step_t;

/** @brief 진행/판정 결과 */
typedef enum {
    AGR_IDENTIFY_RESULT_IDLE = 0,       /**< Start 전 / Reset 후 */
    AGR_IDENTIFY_RESULT_IN_PROGRESS,    /**< 시퀀스 진행 중 */
    AGR_IDENTIFY_RESULT_PASS,           /**< 전 항목 일치 (또는 WARN 모드 통과) */
    AGR_IDENTIFY_RESULT_MISMATCH,       /**< 불일치 — NMT START 보류 대상 (STRICT) */
    AGR_IDENTIFY_RESULT_TIMEOUT,        /**< 재시도 소진 — Slave 무응답 */
    AGR_IDENTIFY_RESULT_ABORT,          /**< Slave가 SDO Abort 응답 (해당 OD 미지원 등) */
} AGR_Identify_Result_t;

/** @brief 불일치 처리 모드 */
typedef enum {
    AGR_IDENTIFY_MODE_STRICT = 0,       /**< 불일치 → MISMATCH (기본, fail loud) */
    AGR_IDENTIFY_MODE_WARN,             /**< 불일치 → 카운터만 증가, PASS 처리 */
} AGR_Identify_Mode_t;

/** @brief 항목별 검사 on/off 비트 */
#define AGR_IDENTIFY_FLAG_DEVICE_TYPE   (1U << 0)
#define AGR_IDENTIFY_FLAG_VENDOR_ID     (1U << 1)
#define AGR_IDENTIFY_FLAG_PRODUCT_CODE  (1U << 2)
#define AGR_IDENTIFY_FLAG_REVISION      (1U << 3)
#define AGR_IDENTIFY_FLAG_ALL           (0x0FU)

/** @brief Master가 하드코딩하는 기대 identity (Slave 매뉴얼 기반) */
typedef struct {
    uint32_t device_type;    /**< OD 0x1000:00 */
    uint32_t vendor_id;      /**< OD 0x1018:01 */
    uint32_t product_code;   /**< OD 0x1018:02 */
    uint32_t revision;       /**< OD 0x1018:03 — 계약 버전 */
} AGR_Identify_Expected_t;

/** @brief 마지막 불일치 상세 (진단) */
typedef struct {
    AGR_Identify_Step_t failed_step;
    uint32_t            expected;
    uint32_t            actual;
} AGR_Identify_Mismatch_t;

/**
 * @brief SDO Read 송신 함수 (DI — transport 래퍼가 구현)
 * @param user  Init 시 전달한 user 포인터
 * @param idx   OD Index
 * @param sub   OD Sub-Index
 * @return 0=송신 성공, <0=에러 (실패 시 RunPeriodic 타임아웃 경로가 재시도)
 */
typedef int (*AGR_Identify_SendReadFn_t)(void* user, uint16_t idx, uint8_t sub);

/**
 * @brief IDENTIFY 컨텍스트 (Device Driver 인스턴스가 보유)
 * @note  런타임 필드는 직접 접근하지 말 것 — API 경유.
 */
typedef struct {
    /* DI */
    AGR_Identify_SendReadFn_t send_read;
    void*                     user;

    /* 설정 */
    AGR_Identify_Expected_t   expected;
    uint8_t                   check_flags;  /**< AGR_IDENTIFY_FLAG_* */
    AGR_Identify_Mode_t       mode;
    uint32_t                  timeout_ms;   /**< 기본 AGR_IDENTIFY_TIMEOUT_MS_DEFAULT */
    uint8_t                   max_retry;    /**< 기본 AGR_IDENTIFY_MAX_RETRY_DEFAULT */

    /* 런타임 상태 */
    AGR_Identify_Step_t       current_step;
    AGR_Identify_Result_t     result;
    uint32_t                  step_start_ms;
    uint8_t                   retry_count;
    uint32_t                  actual_values[AGR_IDENTIFY_STEP_COUNT];

    /* 진단 (누적 — Reset에도 유지) */
    AGR_Identify_Mismatch_t   last_mismatch;
    uint32_t                  mismatch_count;
    uint32_t                  timeout_count;
    uint32_t                  abort_count;
    uint32_t                  pass_count;
} AGR_Identify_Ctx_t;

/**
 *-----------------------------------------------------------
 * PUBLIC API
 *-----------------------------------------------------------
 */

/**
 * @brief 컨텍스트 초기화 (Device Driver Init에서 1회)
 * @param expected  NULL이면 check_flags=0으로 강제 (검사 skip — 즉시 PASS)
 */
void AGR_Identify_Init(AGR_Identify_Ctx_t* ctx,
                       AGR_Identify_SendReadFn_t send_read,
                       void* user,
                       const AGR_Identify_Expected_t* expected,
                       uint8_t check_flags,
                       AGR_Identify_Mode_t mode);

/**
 * @brief 시퀀스 시작 — 첫 SDO Read 즉시 송신
 * @note  PnP Task 컨텍스트에서만 호출 (Rx 콜백 금지 — 구동 계약 참조)
 * @return 진행 결과 (check_flags=0이면 즉시 PASS)
 */
AGR_Identify_Result_t AGR_Identify_Start(AGR_Identify_Ctx_t* ctx, uint32_t now_ms);

/**
 * @brief SDO 응답 주입 (Upload Response 0x40/0x41 또는 Abort 0x80)
 * @details 현재 단계의 index/sub와 불일치하는 응답은 무시한다.
 * @return 갱신된 진행 결과
 */
AGR_Identify_Result_t AGR_Identify_OnSdoResponse(AGR_Identify_Ctx_t* ctx,
                                                 const AGR_SDO_Msg_t* rsp,
                                                 uint32_t now_ms);

/**
 * @brief 타임아웃/재시도 처리 (PnP Task 주기 호출)
 * @return 갱신된 진행 결과 (재시도 소진 시 TIMEOUT)
 */
AGR_Identify_Result_t AGR_Identify_RunPeriodic(AGR_Identify_Ctx_t* ctx, uint32_t now_ms);

/** @brief 현재 결과 조회 */
AGR_Identify_Result_t AGR_Identify_GetResult(const AGR_Identify_Ctx_t* ctx);

/**
 * @brief 런타임 상태 리셋 (IDLE 복귀)
 * @details Boot-up 재수신(Slave 리셋)·Offline 시 호출. 설정/누적 진단은 유지.
 */
void AGR_Identify_Reset(AGR_Identify_Ctx_t* ctx);

#endif /* AGR_PNP_IDENTIFY_H */
