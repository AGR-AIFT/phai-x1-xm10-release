/**
 ******************************************************************************
 * @file    xm_production_ctrl_test.h
 * @author  HyundoKim
 * @brief   PhAI-X1 생산검사 실동작(Ctrl Test) 검사 루틴 — passive_mode.c 이식
 * @details
 *   Examples/11_Passive_Mode/passive_mode.c 의 Homing/Passive 왕복 로직을
 *   XM_API 를 전혀 경유하지 않고 cm_drv.h(Device 계층) 직접 호출로 이식한
 *   System 내부 검사 루틴이다.
 *
 *   설계 근거: Extension_Module_GUI_ForProduction/docs/
 *   fw-single-build-mode-switch-feasibility-2026-07-03.md §F1~F3, §4.
 *
 *   F1 — XM_API 완전 미경유: 이 헤더/구현부 어디에도 xm_api.h 또는
 *        xm_api_*.h 를 include 하지 않는다. PRODUCTION 모드 판정은
 *        core_process.c 가 UsbHostMode_Get() 를 1회 평가해 bool 로 주입한다
 *        (XM_ProductionCtrlTest_Poll 의 인자) — 타입 레벨에서 XM_API 의존을
 *        차단하기 위한 의도적 설계.
 *   F2 — DTR 손실 = fault: core_process.c 의 게이트는 USB 모드가 아니라
 *        XM_ProductionCtrlTest_IsControlling() 을 기준으로 Control_Loop()
 *        스킵 여부를 결정한다 — 모터 구동 중 케이블이 뽑혀도 안전 정지
 *        시퀀스(STOPPING/FAULT)를 완주할 때까지 App 이 재개되지 않는다.
 *   F3 — RAM_D1 배치: 이 모듈의 정적 컨텍스트(s_ctx, cm_drv.h 참조)는 어떤
 *        section attribute 도 쓰지 않는 plain static 이므로 링커스크립트
 *        기본 배치(.bss → RAM_D1, STM32H743XIHX_FLASH.ld)를 그대로 따른다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_PRODUCTION_CTRL_TEST_H_
#define XM_PRODUCTION_CTRL_TEST_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 검사 루틴 상태 (OD 0x7EB0:01 로 그대로 캐스트되어 노출됨).
 */
typedef enum {
    XM_CTRLTEST_STATE_IDLE = 0,        /**< 평상시 — Control_Loop 정상 실행 대상 */
    XM_CTRLTEST_STATE_ARMED,           /**< ARM 커맨드 수신, START 대기 */
    XM_CTRLTEST_STATE_HOMING,          /**< 원점 복귀 중 (모터 구동) */
    XM_CTRLTEST_STATE_ACTIVE_SWEEP,    /**< 왕복 구동 중 (모터 구동) */
    XM_CTRLTEST_STATE_STOPPING,        /**< 정상 종료 요청 → 안전 정지 시퀀스 진행 중 */
    XM_CTRLTEST_STATE_FAULT,           /**< 이상 이탈(DTR/워치독/CM끊김) → 안전 정지 시퀀스 진행 중 */
} XmCtrlTestState_e;

/**
 * @brief 1ms tick 마다 무조건 호출 — OD 커맨드 엣지 처리, 워치독, mode-loss 감지,
 *        모터 각도/토크 미러 갱신. 모터로 SDO 를 보내지 않는다(Step() 전용).
 * @param[in] is_production_mode  core_process.c 가 UsbHostMode_Get()==PRODUCTION
 *            여부를 미리 계산해 전달 (본 모듈이 xm_api_usb.h 를 include 하지
 *            않기 위한 의도적 설계 — F1 "XM_API 완전 미경유"를 타입 레벨까지
 *            강제).
 */
void XM_ProductionCtrlTest_Poll(bool is_production_mode);

/**
 * @brief 검사 루틴이 모터를 구동 중이거나(HOMING/ACTIVE_SWEEP) 안전 정지 시퀀스를
 *        진행 중(STOPPING/FAULT)인지 여부. core_process.c 의 게이트 판정 기준
 *        (F2 — USB 모드가 아니라 이 값을 기준으로 Control_Loop() 스킵 여부 결정).
 */
bool XM_ProductionCtrlTest_IsControlling(void);

/**
 * @brief production exit/DTR loss/fault cleanup hook.
 * @details ARMED 는 즉시 IDLE, 구동/정지/FAULT 상태는 STOPPING 경로를 유지한다.
 */
void XM_ProductionCtrlTest_RequestSafeExit(void);

/**
 * @brief IsControlling()==true 인 tick 에만 core_process.c 가 호출.
 *        현재 상태의 서브 FSM 을 1 스텝 진행하고 필요 시 CM 으로 P/I-Vector 를
 *        전송한다(cm_drv.h 직접 호출).
 */
void XM_ProductionCtrlTest_Step(void);

/**
 * @brief 부팅 시 1회 초기화 (system_startup.c 에서 호출).
 */
void XM_ProductionCtrlTest_Init(void);

/* ===== OD 0x7EB0 bound storage (xm_production_od.c 가 bind) =====
 * 스레드 안전 계약: xm_production_diag.h 와 동일 — 모든 mirror 변수는 volatile
 * 단일 word(uint8/uint16/int16). writer=UserTask(1kHz, Poll()/Step()),
 * reader=USB SDO 핸들러(동일 UserTask 컨텍스트, cdc_dop_router.c 참조,
 * XM_USB_ProcessPeriodic() 이 core_process.c Step4 에서 호출됨). 멀티워드
 * 일관성이 필요한 구조는 두지 않는다.
 */

/** @brief :00 RW, self-clearing pulse — 0xA5=ARM, 0x5A=START, 0x55=STOP. */
extern volatile uint8_t  g_xm_ctrltest_cmd_reg;
/** @brief :01 RO — XmCtrlTestState_e 캐스트 (0=IDLE..5=FAULT). */
extern volatile uint8_t  g_xm_ctrltest_status;
/** @brief :02 RW, write-only 워치독 pulse — HOMING/ACTIVE_SWEEP 중 이전 값과
 *         다른 값을 반복 write 해야 워치독이 갱신됨(토글 비트 권장, on_write
 *         콜백 부재로 인한 제약 — §Poll() 구현 주석 참조).
 *         계약: START(0x5A) 커맨드 수신 시 워치독 기준시각이 내부적으로
 *         리셋되므로(armed_tick/sub_state_timer 리셋과 동일 관례), GUI 는
 *         ARMED 구간(ARM 후 START 대기 중)에는 이 레지스터를 write 할 필요가
 *         없다 — HOMING/ACTIVE_SWEEP 진입 이후부터만 XM_CTRLTEST_KEEPALIVE_
 *         TIMEOUT_MS(1000ms) 보다 짧은 주기로 다른 값을 write 하면 된다. */
extern volatile uint8_t  g_xm_ctrltest_keepalive_reg;
/** @brief :03 RO — 우측 고관절 모터 각도(deg x100, 관찰 전용). */
extern volatile int16_t  g_xm_ctrltest_angle_rh_x100;
/** @brief :04 RO — 좌측 고관절 모터 각도(deg x100, 관찰 전용). */
extern volatile int16_t  g_xm_ctrltest_angle_lh_x100;
/** @brief :05 RO — 우측 고관절 토크(Nm x100, 관찰 전용). */
extern volatile int16_t  g_xm_ctrltest_torque_rh_x100;
/** @brief :06 RO — 좌측 고관절 토크(Nm x100, 관찰 전용). */
extern volatile int16_t  g_xm_ctrltest_torque_lh_x100;
/** @brief :07 RO — CM_Drv_IsConnected() mirror. */
extern volatile uint8_t  g_xm_ctrltest_cm_connected;
/** @brief :08 RO, latched — FAULT 완주(STOP_SUB_DONE) 시마다 누적. */
extern volatile uint16_t g_xm_ctrltest_fault_count;
/** @brief :09 RO, latched — 최근 FAULT 사유(XmCtrlTestFaultReason_e 캐스트). */
extern volatile uint8_t  g_xm_ctrltest_fault_reason;
/** @brief :0A RO — CM h10Mode(H10 전원버튼 모드) mirror: 0=대기(STANDBY)/1=보조
 *         (ASSIST). GUI ④ 가 "현재 SUIT 모드" 실시간 표시 + 물리버튼 트리거
 *         관찰용. HOMING 진입/안전정지는 FW Poll() 내부 엣지 감지가 구동한다. */
extern volatile uint8_t  g_xm_ctrltest_h10_mode;

/* 명령 레지스터 매직값.
 * 0xA5 = ENABLE(구 ARM): IDLE→ARMED "물리 전원버튼 대기" 진입. 실제 HOMING 진입은
 *        Poll() 의 h10Mode STANDBY→ASSIST 상승엣지가 자동 트리거(passive_mode).
 * 0x5A = START: [벤치 전용 폴백] 프로덕션 GUI 미사용 — 물리 H10 없이 강제 진입용.
 * 0x55 = STOP: ARMED/HOMING/ACTIVE_SWEEP 어디서든 즉시 안전 정지. */
#define XM_CTRLTEST_CMD_NONE            (0x00u)
#define XM_CTRLTEST_CMD_ARM_REQUEST     (0xA5u)   /* = ENABLE (물리버튼 대기 진입) */
#define XM_CTRLTEST_CMD_START_REQUEST   (0x5Au)   /* 벤치 전용 폴백 */
#define XM_CTRLTEST_CMD_STOP_REQUEST    (0x55u)

#ifdef __cplusplus
}
#endif

#endif /* XM_PRODUCTION_CTRL_TEST_H_ */
