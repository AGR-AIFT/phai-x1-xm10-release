/**
 ******************************************************************************
 * @file    mti-630.h
 * @author  HyundoKim
 * @brief   [Devices Layer] Xsens MTi-630 IMU 드라이버 헤더
 * @details 
 * - IOIF 의존 싱글톤 인터페이스 (AGR 아키텍처 표준)
 * - 파싱: Multi-instance (파서 객체 외부 관리, ISR-safe)
 * - 설정: ConfigureOutput, Cmd_* (IOIF UART 사용)
 * - IMU Hub Module / XM 공통 사용 가능 (Device + IOIF 복사 이식)
 * 
 * [의존성] Device → IOIF (00-core-architecture.mdc 준수)
 * 
 * @version 4.0 (Multi-Instance Auto-Sense + DataLake)
 * @date    Feb 11, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_XSENS_MTI_630_H_
#define DEVICES_XSENS_MTI_630_H_

#include <stdint.h>
#include <stdbool.h>
#include "ioif_agrb_uart.h"  /* IOIF_UARTx_t, IOIF_UART_Write_Polling */

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* Xsens MTData2 프로토콜 정의 */
#define XSENS_PREAMBLE          (0xFA) // 패킷 시작
#define XSENS_BID               (0xFF) // 버스 ID (Master)
#define XSENS_MID_MTDATA2       (0x36) // 메시지 ID (MTData2)
#define XSENS_MID_SETOUTPUTCFG  (0xC0) // 출력 설정 명령 ID

// MTData2 Data IDs (XDI) - 1kHz 요청 데이터
#define XDI_QUATERNION       (0x2010) // 4x float (w,x,y,z) - 16 bytes
#define XDI_ACCELERATION     (0x4020) // 3x float (x,y,z) - 12 bytes
#define XDI_GYROSCOPE_DATA   (0x8020) // 3x float (x,y,z) - 12 bytes

#define XSENS_MAX_PACKET_SIZE   (256)  // MTData2 패킷 최대 크기

/**
 * @brief 최대 XSENS 인스턴스 수 (module.h에서 오버라이드 가능)
 * @details XM: 1, IMU Hub: 6 — module.h에서 #define 후 이 헤더 include
 */
#ifndef XSENS_MAX_INSTANCES
#define XSENS_MAX_INSTANCES     (1)
#endif

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief Xsens 파싱 데이터 구조체 (단일 센서용)
 */
typedef struct {
    uint32_t timestamp;  // 수신 시각 (ms) - caller가 설정
    uint8_t  imu_index;  // 센서 인덱스 (0 ~ 5) - caller가 설정
    
    // Orientation (Quaternion)
    float q_w, q_x, q_y, q_z;
    
    // Acceleration (m/s²)
    float acc_x, acc_y, acc_z;
    
    // Gyroscope (rad/s or deg/s, 센서 설정에 따라)
    float gyr_x, gyr_y, gyr_z;
    
} XsensMTi_packet_t;

/**
 * @brief Xsens 파싱 상태 (EBIMU 패턴)
 */
typedef enum {
    XSENS_STATE_WAIT_PREAMBLE,  // 0xFA
    XSENS_STATE_WAIT_BID,       // 0xFF
    XSENS_STATE_WAIT_MID,       // Message ID
    XSENS_STATE_WAIT_LEN,       // Length
    XSENS_STATE_WAIT_LEN_EXT_H, // Extended Length High
    XSENS_STATE_WAIT_LEN_EXT_L, // Extended Length Low
    XSENS_STATE_COLLECT_PAYLOAD,// Payload
    XSENS_STATE_WAIT_CHECKSUM,  // Checksum
} XsensParseState_t;

/**
 * @brief Xsens 파서 구조체 (인스턴스화)
 * @details EBIMU의 EbimuParser_t와 동일한 패턴
 */
typedef struct {
    XsensParseState_t state;
    uint8_t           buffer[XSENS_MAX_PACKET_SIZE];
    uint8_t           mid;
    uint16_t          len;       // Extended Length 지원
    uint16_t          buf_idx;
    uint8_t           checksum;  // 1-complement
} XsensParser_t;

/**
 * @brief Xsens 드라이버 인터페이스 (싱글톤)
 * @details
 * [파싱] Multi-instance: InitParser, ParseByte, IsConnected
 *   - IMU Hub: 6개 파서 인스턴스 동시 관리
 *   - XM: 1개 파서 인스턴스
 *   - timestamp, imu_index는 caller(System Layer)가 설정
 * 
 * [설정] IOIF 의존: ConfigureOutput, Cmd_*
 *   - 센서 1kHz Quat+Acc+Gyro 모드 설정
 *   - 재시도 로직 내장 (5회)
 *   - 초기화 시 1회 호출
 */
typedef struct {
    /* ===== 파싱 (Multi-instance, ISR-safe) ===== */
    
    /**
     * @brief 파서 초기화
     */
    void (*InitParser)(XsensParser_t* parser);
    
    /**
     * @brief 바이트 단위 파싱 (ISR에서 호출)
     * @param parser 파서 객체 포인터
     * @param byte   수신된 1바이트
     * @param output 파싱 완료 시 데이터를 담을 구조체
     * @return true: 패킷 완성됨
     * @note timestamp, imu_index는 caller가 설정해야 함
     */
    bool (*ParseByte)(XsensParser_t* parser, uint8_t byte, XsensMTi_packet_t* output);
    
    /**
     * @brief 연결 상태 확인 (타임아웃 기반)
     * @param imu_id IMU 인덱스 (0~5)
     * @param last_rx_time 마지막 수신 시간 (ms)
     * @param current_time 현재 시간 (ms)
     * @return true: 연결됨, false: 타임아웃
     */
    bool (*IsConnected)(uint8_t imu_id, uint32_t last_rx_time, uint32_t current_time);
    
    /* ===== 센서 설정 (IOIF 의존, 초기화 시 1회 호출) ===== */
    
    /**
     * @brief 센서를 1kHz Quat+Acc+Gyro 모드로 설정
     * @param uart_id IOIF UART 인스턴스 ID
     * @details GoToConfig → SetOutputConfiguration → GoToMeasurement
     */
    void (*ConfigureOutput)(IOIF_UARTx_t uart_id);
    
    /**
     * @brief 센서 리셋 명령
     */
    void (*Cmd_Reset)(IOIF_UARTx_t uart_id);
    
    /**
     * @brief Config 모드 진입 (MID 0x30)
     */
    void (*Cmd_GoToConfig)(IOIF_UARTx_t uart_id);
    
    /**
     * @brief Measurement 모드 진입 (MID 0x10)
     */
    void (*Cmd_GoToMeasurement)(IOIF_UARTx_t uart_id);
    
} XSENS_Driver_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */

/* 싱글톤 인스턴스 (파싱 + 센서 설정) */
extern XSENS_Driver_t xsensMTi630;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief [디버깅] 센서 설정 명령 전송 통계 가져오기
 * @param sent 성공 전송 횟수 출력
 * @param failed 실패 전송 횟수 출력
 */
void XsensMTi_GetConfigStats(volatile uint32_t* sent, volatile uint32_t* failed);

/* ================================================================
 * Auto-Sense + DataLake API (Multi-Instance)
 * ================================================================
 * - Auto-Sense: 데이터 타임아웃 기반 연결 감지 (PnP Task에서 호출)
 * - DataLake: Task-Task 간 데이터 공유 (Mutex + Snapshot)
 * - Multi-Instance: ch 파라미터로 인스턴스 구분
 *   - XM: ch=0 고정 (XSENS_MAX_INSTANCES=1)
 *   - IMU Hub: ch=0~5 (XSENS_MAX_INSTANCES=6)
 *
 * RTOS: Mutex + Snapshot / BareMetal: volatile 직접 접근
 * ================================================================ */

/**
 * @brief Auto-Sense + DataLake 초기화
 * @param ch 채널 인덱스 (0 ~ XSENS_MAX_INSTANCES-1)
 */
void XsensMTi_StateInit(uint8_t ch);

/**
 * @brief [Writer] DataLake에 최신 패킷 업데이트 (UartRxTask에서 호출)
 * @param ch 채널 인덱스
 * @param packet 파싱 완료된 IMU 데이터 패킷
 */
void XsensMTi_UpdateData(uint8_t ch, const XsensMTi_packet_t* packet);

/**
 * @brief [Reader] DataLake에서 최신 스냅샷 가져오기 (Core Process에서 호출)
 * @param ch 채널 인덱스
 * @param out 데이터를 복사받을 구조체 포인터
 * @return true: 데이터 유효 (OPERATIONAL), false: 연결 끊김
 */
bool XsensMTi_GetLatest(uint8_t ch, XsensMTi_packet_t* out);

/**
 * @brief Auto-Sense 연결 상태 확인
 * @param ch 채널 인덱스
 * @return true: OPERATIONAL (데이터 수신 중), false: STOPPED (타임아웃)
 */
bool XsensMTi_IsOnline(uint8_t ch);

/**
 * @brief Auto-Sense 주기적 실행 (PnP Task에서 호출, 100ms 주기)
 * @param ch 채널 인덱스
 * @details 데이터 타임아웃 체크. 500ms 동안 데이터 없으면 STOPPED 전환.
 */
void XsensMTi_RunPeriodic(uint8_t ch);

#endif /* DEVICES_XSENS_MTI_630_H_ */
