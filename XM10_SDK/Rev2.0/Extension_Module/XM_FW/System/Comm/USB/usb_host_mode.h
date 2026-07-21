/**
 ******************************************************************************
 * @file    usb_host_mode.h
 * @author  HyundoKim
 * @brief   USB-CDC 호스트 모드 SSOT (PHAI / TERMINAL / PRODUCTION) — System 소유.
 * @details
 *  한 보드/한 케이블에서 붙는 PC 호스트의 종류를 분리하는 상태머신.
 *  기존에 facade(xm_api_usb)에 있던 로직을 System 계층으로 이전 — facade 는
 *  XM_USB_SetHostProfile() 한 줄로 위임만 하고, 상태·전이 로직은 여기가 소유한다.
 *
 *  ── 내부 mode ───────────────────────────────────────────────────────────
 *   사용자가 고를 수 있는 두 모드를 0·1 로 연속 배치하고, 자동 전용인
 *   PRODUCTION 을 맨 뒤(2)에 둔다 → 사용자 관점에선 "0·1 만 존재, 2 는 없는 값".
 *   또한 UsbHostProfile_e 와 값이 1:1 정렬(PHAI↔PHAI_STUDIO=0, TERMINAL=1).
 *   - PHAI (0)       : PhAI Studio 실시간 telemetry (Total Data auto-pump). 선택 가능.
 *   - TERMINAL (1)   : 일반 터미널 / 커스텀 프로그램 — auto-pump OFF. 선택 가능.
 *   - PRODUCTION (2) : Extension_Module_GUI_ForProduction HW 검증 / SI / 양산 (COBS+CRC DOP).
 *                      ⚠ 사용자 API 로 선택 불가. cdc_dop_router 가 첫 유효 DOP frame
 *                      수신 시 자동 latch 하는 전용 모드.
 *
 *  ── 사용자 프로파일 (2-값, PRODUCTION 은닉) ─────────────────────────────
 *   - PHAI_STUDIO (기본) : streaming baseline. PRODUCTION 자동 latch 허용.
 *   - TERMINAL           : 깨끗한 텍스트/커스텀 IO. auto-pump·PRODUCTION latch 차단.
 *
 *  ── DTR=0 (호스트 분리) 복귀 정책 ───────────────────────────────────────
 *   하드코딩 PHAI 가 아니라 *사용자 기본 프로파일* 로 복귀한다. 따라서 TERMINAL
 *   프로파일은 터미널 close/open (DTR 재토글) 재접속에도 유지된다.
 *   (DETERMINISTIC > OBVIOUS: 복귀 목적지를 프로파일 하나로 일원화)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_USB_USB_HOST_MODE_H_
#define SYSTEM_COMM_USB_USB_HOST_MODE_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 내부 mode. 선택 가능한 PHAI(0)·TERMINAL(1) 을 앞에 연속 배치,
 *        자동 전용 PRODUCTION(2) 은 맨 뒤 — 사용자 관점에서 "없는 값".
 */
typedef enum {
    USB_HOST_MODE_PHAI       = 0,   /**< 선택 가능 (PHAI_STUDIO 프로파일) */
    USB_HOST_MODE_TERMINAL   = 1,   /**< 선택 가능 (TERMINAL 프로파일) */
    USB_HOST_MODE_PRODUCTION = 2,   /**< 자동 latch 전용 — 사용자 비노출 */
} UsbHostMode_e;

/** @brief 사용자 기본 프로파일 (DTR-lost 복귀 목적지). PRODUCTION 은닉. */
typedef enum {
    USB_HOST_PROFILE_PHAI_STUDIO = 0,  /**< 기본: streaming baseline (+ PRODUCTION auto-latch 허용) */
    USB_HOST_PROFILE_TERMINAL    = 1,  /**< 터미널/커스텀 — auto-pump·latch OFF, 재접속 유지 */
} UsbHostProfile_e;

/** @brief 내부 mode 변경 알림 콜백 (OD/LED 동기화 등). 짧고 비차단. */
typedef void (*UsbHostMode_ChangeCb_t)(UsbHostMode_e prev, UsbHostMode_e next);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/** @brief 현재 모드 조회 (atomic, ISR-safe read). */
UsbHostMode_e UsbHostMode_Get(void);

/**
 * @brief 사용자 기본 프로파일 설정 — facade XM_USB_SetHostProfile() 위임 대상.
 * @details 즉시 해당 모드로 전환하고, 이후 DTR-lost 복귀 목적지로 기억한다.
 * @param[in] profile PHAI_STUDIO(기본) 또는 TERMINAL.
 */
void UsbHostMode_SetDefaultProfile(UsbHostProfile_e profile);

/**
 * @brief [cdc_dop_router 전용] 첫 유효 DOP frame 수신 시 PRODUCTION latch 요청.
 * @details 기본 프로파일이 PHAI_STUDIO 일 때만 성공(TERMINAL 사용자 보호).
 *          idempotent — 이미 PRODUCTION 이면 무동작.
 * @return true = 이 호출이 실제 전환을 일으킴, false = 무변화.
 */
bool UsbHostMode_RequestProductionLatch(void);

/**
 * @brief [TASK 문맥 전용] DTR=0 (호스트 분리) 시 기본 프로파일로 복귀.
 * @warning heavy·non-ISR-safe (mode 전환·LED·ProductionOD·콜백). ISR 은
 *          UsbHostMode_NotifyDtrLostFromISR() 를 대신 사용 [P1-G].
 */
void UsbHostMode_OnDtrLost(void);

/**
 * @brief [ISR-safe] ISR 문맥에서 DTR-lost 를 표시(atomic set)만 하고 즉시 복귀.
 * @details 실제 heavy 복귀는 UsbHostMode_ProcessDeferred()(UserTask)가 소비 [P1-G].
 */
void UsbHostMode_NotifyDtrLostFromISR(void);

/**
 * @brief [TASK 문맥] 지연된 DTR-lost 를 소비(test-and-clear)하고 복귀 실행.
 * @return true = 이번 호출에서 복귀가 실행됨.
 */
bool UsbHostMode_ProcessDeferred(void);

/** @brief 내부 mode 변경 콜백 등록 (마지막 등록자 우선, NULL = 해제). */
void UsbHostMode_RegisterChangeCallback(UsbHostMode_ChangeCb_t cb);

#endif /* SYSTEM_COMM_USB_USB_HOST_MODE_H_ */
