/**
 ******************************************************************************
 * @file    cdc_dop_router.h
 * @author  HyundoKim
 * @brief   USB-CDC ↔ AGR DOP Serial 라우터 (XM Production / SI GUI)
 * @details
 *   PC Extension_Module_GUI_ForProduction GUI 가 USB-CDC 위로 COBS-framed DOP V3 프레임을 보내올 때,
 *   본 모듈이 CdcStream RX 를 AGR_Serial_ProcessRxData() 로 흘리고,
 *   응답/PDO TX 는 AGR_Serial_*Send*() 가 호출하는 tx_func 을 CdcStream_Send()
 *   로 래핑하여 송신한다.
 *
 *   동작 모드:
 *     - Normal     : PhAI V2 스트리밍 유지. RX 만 DOP 파서에 공급.
 *                    유효 SDO Read/Write 프레임이 들어오면 SDO 응답이 자동 회신됨.
 *     - Test Mode  : 0x7000:0 = 1 SDO Write 로 진입. PhAI 자동 스트리밍 차단
 *                    (CdcStream_SetAutoStreamEnabled(false)), 페리페럴 stimulus 허용.
 *
 *   FW 변경 흐름:
 *     - System_Startup() :  CdcStream_Init() 후 CdcDopRouter_Init()
 *     - core_process 1ms :  XM_USB_ProcessPeriodic() 내부에서 CdcDopRouter_Process()
 *     - Host DTR off    :  CdcStream 콜백이 호출되어 사이드이펙트 자동 정리
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */
#pragma once

#ifndef SYSTEM_COMM_USB_CDC_DOP_ROUTER_H_
#define SYSTEM_COMM_USB_CDC_DOP_ROUTER_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief CDC-DOP 라우터 초기화.
 *        XM_ProductionOD_Init() 이후, CdcStream_Init() 이후에 호출.
 */
void CdcDopRouter_Init(void);

/** @brief 라우터 초기화 성공 여부 — startup 결과 비트맵(System_GetStartupResultBitmap)용 */
bool CdcDopRouter_IsReady(void);

/**
 * @brief CDC RX 버퍼를 비우면서 DOP 프레임 파서에 공급한다.
 *        1ms 주기로 XM_USB_ProcessPeriodic() 에서 호출.
 *
 * @return 이번 호출에서 처리한 바이트 수
 */
uint32_t CdcDopRouter_Process(void);

/**
 * @brief 현재 호스트에서 유효한 DOP 프레임을 수신한 적이 있는지.
 *        true 동안 PhAI 스트리밍은 자동 보류된다.
 */
bool CdcDopRouter_IsDopHostActive(void);

/**
 * @brief [P1 2026-08-19] 링크 리셋(DTR 토글 등) 통지 — Boot FTP carry 무효화.
 * @details 연결이 FTP 프레임 중간에 끊기면 carry 잔여가 새 연결의 첫 명령과
 *          잘못 결합될 수 있어, DTR 이벤트 시 carry 를 지연 리셋한다.
 * @note ISR-safe (atomic flag set 만 수행 — 소비는 CdcDopRouter_Process).
 */
void CdcDopRouter_NotifyLinkReset(void);

#endif /* SYSTEM_COMM_USB_CDC_DOP_ROUTER_H_ */
