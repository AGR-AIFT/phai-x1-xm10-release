/**
 ******************************************************************************
 * @file    xm_total_data.h
 * @author  HyundoKim
 * @brief   [System Layer] Total Data Packet 스냅샷 모듈 헤더
 * @details
 * 모든 센서의 raw 데이터를 1ms마다 스냅샷으로 수집하여
 * USB CDC Total Data Packet(Module ID 0x20)으로 자동 전송합니다.
 *
 * - 스냅샷 수집: _FetchAllInputs() 직후 호출
 * - 데이터 제공: XM_USB_ProcessPeriodic()에서 PhAI_PacketBuild()로 전송
 * - 사용자 코드 불필요 (System-Managed)
 *
 * @version 2.0
 * @date    Mar 23, 2026
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_USB_XM_TOTAL_DATA_H_
#define SYSTEM_COMM_USB_XM_TOTAL_DATA_H_

#include <stdint.h>
#include "xm_total_data_packet.h"  /* AUTO-GENERATED: XM_TotalDataPacket_t */

/**
 * @brief 모든 센서 raw 데이터를 스냅샷으로 수집합니다.
 * @details core_process.c의 _FetchAllInputs() 직후 호출됩니다.
 *          각 센서 드라이버의 DataLake API를 통해 최신 데이터를 복사합니다.
 *          device_online_mask 비트플래그로 연결된 디바이스를 표시합니다.
 * @note Thread-Safe: 각 드라이버 내부 Mutex로 보호됨
 */
void XM_TotalData_Snapshot(void);

/**
 * @brief 최신 스냅샷 데이터의 포인터와 크기를 반환합니다.
 * @param[out] size  패킷 크기 (bytes)
 * @return 최신 스냅샷 데이터 포인터 (static 버퍼, 복사 불필요)
 */
const XM_TotalDataPacket_t* XM_TotalData_GetLatest(uint32_t* size);

#endif /* SYSTEM_COMM_USB_XM_TOTAL_DATA_H_ */
