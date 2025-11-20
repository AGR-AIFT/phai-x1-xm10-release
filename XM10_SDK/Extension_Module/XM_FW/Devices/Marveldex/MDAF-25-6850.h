/**
 ******************************************************************************
 * @file    mdaf-25-6850.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_MARVELDEX_MDAF_25_6850_H_
#define DEVICES_MARVELDEX_MDAF_25_6850_H_

#include "ioif_agrb_uart.h"
// Singletone << 시스템에 하나 이상 있을수 없다

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define MAX_SENSOR_NUM              (2)     // 최대 2개 FSR (왼발/오른발)
#define MARVELDEX_RAW_PACKET_SIZE   (28)    // FSR 패킷 크기 (28바이트)
#define MARVELDEX_CHANNEL_SIZE      (14)    // FSR 센서 채널 (14개)
//     헤더(4)     : 0xff 0xff 0x00 0x01
//     타임스탬프(4): millis() 값 (빅엔디안)
//     센서공간(1)  : 1=왼발, 2=오른발
//     롤링인덱스(1): 0~199 순환
//     센서데이터(14): 14채널 매핑된 값 (0~0xEF)
//     푸터(2)     : 0x00 0xfe
// 전송 속도: 1kHz

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef enum {
    MARVELDEX_SENSOR_SPACE_LEFT = 1,
    MARVELDEX_SENSOR_SPACE_RIGHT,
    MARVELDEX_SENSOR_SPACE_UNKNOWN,
} MARVELDEX_SENSOR_SPACE_e;

/**
 * @brief FSR 패킷 포맷 (총 28바이트)
 * @details C-style 예제 코드의 FSRPacket 구조체를 반영합니다.
 */
typedef struct {
    uint32_t timestamp;      // [수정] 수신 시점의 시스템 틱 (ms)
    MARVELDEX_SENSOR_SPACE_e  sensorSpace;    // 1=왼발, 2=오른발
    uint8_t  rollingIndex;   // 0-199 패킷 시퀀스
    uint8_t  sensorData[MARVELDEX_CHANNEL_SIZE]; // 14개 센서 채널 값 (0-239)
    uint8_t  batteryLevel;   // 배터리 잔량 (0-100)
    uint8_t  statusFlags;    // 상태 플래그 (bit0: 충전중)
} MarvelDex_packet_t;

/**
 * @brief 원시 패킷 버퍼
 */
typedef struct {
    uint8_t raw[MARVELDEX_RAW_PACKET_SIZE];
} MarvelDex_raw_packet_t;

/**
 * @brief Devices 계층이 System 계층으로 파싱된 패킷을 전달하는 콜백
 */
typedef void (*FsrPacketCallback_t)(const MarvelDex_packet_t* packet);

/**
 * @brief 드라이버 인터페이스 (싱글톤)
 */
typedef struct {
    /**
     * @brief 드라이버를 초기화하고 System Layer의 콜백을 주입받음
     */
    bool (*init)(IOIF_UARTx_t id0, IOIF_UARTx_t id1, FsrPacketCallback_t packet_cb);
    
    // /**
    //  * @brief 큐에서 파싱 완료된 패킷을 가져옵니다. (Non-blocking) 전송, 제어, 수신처리, 설정 
    //  */
    // bool (*get)(MarvelDex_packet_t* output);
} MarvelDexFSR_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */

extern MarvelDexFSR_t marvelDexFSR;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


#endif /* DEVICES_MARVELDEX_MDAF_25_6850_H_ */
