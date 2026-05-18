/**
 ******************************************************************************
 * @file    cdc_dop_router.h
 * @author  HyundoKim
 * @brief   USB-CDC ↔ AGR DOP Serial 라우터 (XM Production / SI GUI)
 * @details
 *   PC sensor-studio GUI 가 USB-CDC 위로 COBS-framed DOP V3 프레임을 보내올 때,
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
 *     - core_process 2ms :  XM_USB_ProcessPeriodic() 내부에서 CdcDopRouter_Process()
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

/**
 * @brief CDC RX 버퍼를 비우면서 DOP 프레임 파서에 공급한다.
 *        2ms 주기로 XM_USB_ProcessPeriodic() 에서 호출.
 *
 * @return 이번 호출에서 처리한 바이트 수
 */
uint32_t CdcDopRouter_Process(void);

/**
 * @brief 현재 호스트에서 유효한 DOP 프레임을 수신한 적이 있는지.
 *        true 동안 PhAI 스트리밍은 자동 보류된다.
 */
bool CdcDopRouter_IsDopHostActive(void);

#endif /* SYSTEM_COMM_USB_CDC_DOP_ROUTER_H_ */
