/**
 ******************************************************************************
 * @file    xm_production_od.h
 * @author  HyundoKim
 * @brief   XM Production / SI 검증 GUI 용 DOP Slave Object Dictionary
 * @details
 *   XM이 USB-CDC + DOP Serial(COBS) 위에서 SDO Slave로 동작할 때 노출하는
 *   OD 엔트리 모음. sensor-studio GUI가 SDO Read/Write로 보드 식별, Test Mode
 *   전환, 페리페럴 stimulus/sense를 수행한다.
 *
 *   Phase 0 범위: Identity(0x1000/0x1008/0x1018) + Test Mode(0x7000) 만.
 *   페리페럴 OD (0x2010~0x20FF) 는 Phase 2 에서 확장.
 *
 *   Index 정책:
 *     0x1000~0x1FFF : CANopen Communication Profile
 *     0x2000~0x5FFF : Manufacturer Specific (XM 페리페럴 카탈로그 — Phase 2)
 *     0x7000        : Test Mode Enable (양산 검증 진입 게이트)
 *     0x7E00        : SI Toggle stimulus control (Phase 3)
 *     0x7F00        : Peripheral Test Runner (Phase 2, sensor-studio 표준 호환)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */
#pragma once

#ifndef XM_PRODUCTION_OD_H_
#define XM_PRODUCTION_OD_H_

#include "agr_dop_types.h"
#include "agr_dop_serial.h"
#include <stdbool.h>
#include <stdint.h>

#define XM_PRODUCTION_NODE_ID       0x01u

#define XM_DEVICE_TYPE              0x00010001u  /* AGR XM Extension Module */
#define XM_VENDOR_ID                0x00000A6Eu  /* Angel Robotics 'AGR' (placeholder) */
#define XM_PRODUCT_CODE             0x584D3130u  /* 'XM10' ASCII */
#define XM_REVISION_REV1_1          0x00010001u
#define XM_REVISION_REV2_0          0x00020000u

void     XM_ProductionOD_Init(void);

AGR_Serial_Ctx_t* XM_ProductionOD_GetSerialCtx(void);
AGR_DOP_Ctx_t*    XM_ProductionOD_GetDopCtx(void);

bool     XM_ProductionOD_IsTestModeEnabled(void);
void     XM_ProductionOD_SetSerialNumber(uint32_t serial);

#endif /* XM_PRODUCTION_OD_H_ */
