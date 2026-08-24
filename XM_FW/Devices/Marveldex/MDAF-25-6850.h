/**
 ******************************************************************************
 * @file    mdaf-25-6850.h
 * @author  HyundoKim
 * @brief   
 * @version 2.0 (Multi-Instance Auto-Sense + DataLake)
 * @date    Feb 11, 2026
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

/**
 * @brief 최대 MarvelDex 인스턴스 수 (module.h에서 오버라이드 가능)
 * @details XM: 2 (왼발/오른발), 기본값 2
 */
#ifndef MARVELDEX_MAX_INSTANCES
#define MARVELDEX_MAX_INSTANCES     (2)
#endif

/**
 * @brief 채널 인덱스 매크로 (sensorSpace → ch 전환)
 * @details 기존 MARVELDEX_SENSOR_SPACE_LEFT(1)/RIGHT(2) → ch 0/1
 */
#define MARVELDEX_CH_LEFT   (0)
#define MARVELDEX_CH_RIGHT  (1)
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

/* 싱글톤 인스턴스 (파싱 + UART 콜백 관리) */
extern MarvelDexFSR_t marvelDexFSR;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ================================================================
 * Auto-Sense + DataLake API (Multi-Instance)
 * ================================================================
 * - Auto-Sense: 데이터 타임아웃 기반 연결 감지 (PnP Task에서 호출)
 * - DataLake: Task-Task 간 데이터 공유 (Mutex + Snapshot)
 * - Multi-Instance: ch 파라미터로 인스턴스 구분
 *   - ch=0: 왼발 (MARVELDEX_CH_LEFT)
 *   - ch=1: 오른발 (MARVELDEX_CH_RIGHT)
 *
 * RTOS: Mutex + Snapshot / BareMetal: volatile 직접 접근
 * ================================================================ */

/**
 * @brief Auto-Sense + DataLake 초기화
 * @param ch 채널 인덱스 (0 ~ MARVELDEX_MAX_INSTANCES-1)
 */
void MarvelDex_StateInit(uint8_t ch);

/**
 * @brief [Writer] DataLake에 최신 패킷 업데이트 (UartRxTask에서 호출)
 * @param ch 채널 인덱스
 * @param packet 파싱 완료된 FSR 데이터 패킷
 */
void MarvelDex_UpdateData(uint8_t ch, const MarvelDex_packet_t* packet);

/**
 * @brief [Reader] DataLake에서 최신 스냅샷 가져오기 (Core Process에서 호출)
 * @param ch 채널 인덱스
 * @param out 데이터를 복사받을 구조체 포인터
 * @return true: 데이터 유효 (OPERATIONAL), false: 연결 끊김
 */
bool MarvelDex_GetLatest(uint8_t ch, MarvelDex_packet_t* out);

/**
 * @brief Auto-Sense 연결 상태 확인
 * @param ch 채널 인덱스
 * @return true: OPERATIONAL (데이터 수신 중), false: STOPPED (타임아웃)
 */
bool MarvelDex_IsOnline(uint8_t ch);

/**
 * @brief Auto-Sense 주기적 실행 (PnP Task에서 호출, 100ms 주기)
 * @param ch 채널 인덱스
 * @details 데이터 타임아웃 체크. 500ms 동안 데이터 없으면 STOPPED 전환.
 */
void MarvelDex_RunPeriodic(uint8_t ch);

#endif /* DEVICES_MARVELDEX_MDAF_25_6850_H_ */
