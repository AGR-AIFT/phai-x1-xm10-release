/**
 ******************************************************************************
 * @file    mti-630.h
 * @author  HyundoKim
 * @brief   [Devices Layer] Xsens MTi-630 IMU 드라이버
 * @details ioif_agrb_uart의 콜백을 받아 Xsens MTi 패킷을 파싱하고,
 * System Layer에 큐(Queue)로 전달합니다.
 * @version 0.1
 * @date    Nov 12, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_XSENS_MTI_630_H_
#define DEVICES_XSENS_MTI_630_H_

#include "ioif_agrb_uart.h"
#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* ... (Xsens MTData2 정의) ... */
#define XSENS_PREAMBLE          (0xFA) // 패킷 시작
#define XSENS_BID               (0xFF) // 버스 ID (Master)
#define XSENS_MID_MTDATA2       (0x36) // 메시지 ID (MTData2)
#define XSENS_MID_SETOUTPUTCFG  (0xC0) // 출력 설정 명령 ID

// MTData2 Data IDs (XDI) - 1kHz 요청 데이터
#define XDI_QUATERNION       (0x2010) // 4x float (w,x,y,z) - 16 bytes
#define XDI_ACCELERATION     (0x4020) // 3x float (x,y,z) - 12 bytes
#define XDI_GYROSCOPE_DATA   (0x8020) // 3x float (x,y,z) - 12 bytes

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief [수정] Xsens MTData2 패킷 (1kHz 요청 데이터)
 */
typedef struct {
    uint32_t timestamp;   // 수신 시점의 시스템 틱 (ms)
    
    // 1. Quaternion (4-float)
    float q_w, q_x, q_y, q_z;
    
    // 2. Calibrated Accelerometer (3-float)
    float acc_x, acc_y, acc_z;
    
    // 3. Calibrated Gyroscope (3-float)
    float gyr_x, gyr_y, gyr_z;

} XsensMTi_packet_t;

/**
 * @brief Devices 계층이 System 계층으로 파싱된 패킷을 전달하는 콜백
 */
typedef void (*ImuPacketCallback_t)(const XsensMTi_packet_t* packet);

/**
 * @brief 드라이버 인터페이스 (싱글톤)
 */
typedef struct {
    /**
     * @brief 드라이버를 초기화합니다 (큐 생성, 콜백 등록).
     */
    bool (*init)(IOIF_UARTx_t id, ImuPacketCallback_t packet_cb);
    
    /**
     * @brief IMU에게 1kHz로 데이터 전송을 요청하는 설정 메시지를 보냅니다.
     * @details UartRxHandler가 init() 직후 호출해야 합니다.
     */
    bool (*ConfigureOutput)(void);
} XsensMTi630_t;

extern XsensMTi630_t xsensMTi630;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


#endif /* DEVICES_XSENS_MTI_630_H_ */
