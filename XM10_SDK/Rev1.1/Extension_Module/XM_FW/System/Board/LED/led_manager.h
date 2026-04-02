/**
 ******************************************************************************
 * @file    led_manager.h
 * @author  HyundoKim
 * @brief   [System Layer] 온보드 LED + PCA9957 채널 LED 통합 관리
 * @details
 * - CM-XM Link Status LED (RGB GPIO, PC7/8/9)
 * - User Function LED (Single GPIO, PH13/14/15)
 * - Channel LED (PCA9957 24ch RGB, 7 device channels)
 * - CiA 303-3 표준 LED 패턴 지원
 * @version 2.0.0
 * @date    2026-03-02
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_BOARD_LED_LED_MANAGER_H_
#define SYSTEM_BOARD_LED_LED_MANAGER_H_

#include "ioif_agrb_gpio.h" // IOIF_GPIOx_t
#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/* ===== 1. System Link Status LED (RGB) ===== */

/**
 * @brief CM-XM 링크 상태 (RGB LED 색상/패턴 결정)
 * @warning 기존 0~3 순서 변경 금지 (cm_drv.c:660에서 ordinal cast 사용)
 */
typedef enum {
    /* 기존 4개 (순서 고정, cm_drv.c ordinal cast 호환) */
    LINK_STATE_INITIALISING,    // 0: White Solid (부팅)
    LINK_STATE_PRE_OPERATIONAL, // 1: Green Blinking (연결 대기)
    LINK_STATE_OPERATIONAL,     // 2: Green Solid (연결 완료)
    LINK_STATE_STOPPED,         // 3: Red Solid (정지)

    /* Rev2.0 확장 (반드시 뒤에 추가) */
    LINK_STATE_DETECTING,       // 4: Green Flickering (감지 중)
    LINK_STATE_HEARTBEAT_LOST,  // 5: Red Double Flash
    LINK_STATE_ERROR,           // 6: Red Blinking
    LINK_STATE_FW_UPDATE,       // 7: Blue Solid
} LinkState_t;

/* ===== 2. LED 동작 패턴 (User LED + Channel LED 공용) ===== */

/**
 * @brief LED 동작 모드 (CiA 303-3 확장)
 * @note xm_api_led_btn.h의 XmLedMode_t과 0~4 순서 호환
 */
typedef enum {
    /* 기존 5개 (User LED, XM_API 호환) */
    LED_MODE_OFF,           // 0: 꺼짐
    LED_MODE_SOLID,         // 1: 계속 켜짐
    LED_MODE_BLINK,         // 2: 50% Duty 깜빡임 (주기 설정 가능)
    LED_MODE_HEARTBEAT,     // 3: 두근-두근 (1초 주기)
    LED_MODE_ONE_SHOT,      // 4: 한 번 켜졌다 꺼짐

    /* CiA 303-3 표준 패턴 */
    LED_MODE_FLICKERING,    // 5: 50ms ON/OFF (10Hz) — 감지/열거 중
    LED_MODE_SINGLE_FLASH,  // 6: 200ms ON / 1000ms OFF — 경고
    LED_MODE_DOUBLE_FLASH,  // 7: 2x200ms ON (200ms gap) / 1000ms OFF — HB Lost
    LED_MODE_TRIPLE_FLASH,  // 8: 3x200ms / 1000ms OFF — FW Update 등
} LedMode_t;

/* ===== 3. Channel LED (PCA9957 RGB) ===== */

/**
 * @brief PCA9957 채널 LED 식별자
 * @note PCA9957 RGB Group 순서와 일치 (group 0~6)
 */
typedef enum {
    CH_LED_EMG   = 0,
    CH_LED_FES   = 1,
    CH_LED_IMU   = 2,
    CH_LED_HMMG  = 3,
    CH_LED_GRF_L = 4,
    CH_LED_GRF_R = 5,
    CH_LED_USB   = 6,
    CH_LED_COUNT = 7
} ChannelLedId_t;

/* ===== 4. Device State Enums ===== */

/**
 * @brief CAN-FD PnP 디바이스 상태 (EMG/FES/IMU/HMMG)
 */
typedef enum {
    CH_DEV_NOT_CONNECTED,   // 0: 연결 안됨 — Off
    CH_DEV_DETECTING,       // 1: 감지 중 — Green Flickering
    CH_DEV_PRE_OPERATIONAL, // 2: Pre-Op — Green Single Flash
    CH_DEV_OPERATIONAL,     // 3: 정상 동작 — Green Solid
    CH_DEV_HEARTBEAT_LOST,  // 4: HB 타임아웃 — Red Double Flash
    CH_DEV_DEVICE_ERROR,    // 5: 디바이스 에러 — Red Solid
    CH_DEV_CAN_WARNING,     // 6: CAN Warning — Amber Blinking
    CH_DEV_CAN_BUS_OFF,     // 7: CAN Bus Off — Red Blinking
    CH_DEV_WRONG_DEVICE,    // 8: 잘못된 디바이스 — Red Triple Flash
} ChannelDevState_t;

/**
 * @brief GRF UART 센서 상태 (GRF_L/GRF_R)
 */
typedef enum {
    GRF_NOT_CONNECTED,      // 0: 연결 안됨 — Off
    GRF_DETECTING,          // 1: 감지 중 — Green Flickering
    GRF_DATA_ACTIVE,        // 2: 데이터 수신 중 — Green Solid
    GRF_DATA_TIMEOUT,       // 3: 데이터 타임아웃 — Amber Single Flash
    GRF_LOW_BATTERY,        // 4: 배터리 부족 — Amber Heartbeat
    GRF_CRITICAL_BATTERY,   // 5: 배터리 위험 — Red Blinking
    GRF_SENSOR_ERROR,       // 6: 센서 에러 — Red Solid
} GrfSensorState_t;

/**
 * @brief USB LED 상태 (CDC + MSC)
 * @details 설계 문서: 04_LED_Indication_Design.md Section 4.4
 *          MSC는 Blue 색상으로 CDC(Green)와 시각적 구분
 */
typedef enum {
    USB_LED_NOT_CONNECTED,      // 0: Off — USB 미연결
    USB_LED_CDC_CONNECTED,      // 1: Green Solid — CDC 연결 (idle)
    USB_LED_CDC_DATA_ACTIVE,    // 2: Green Flickering — CDC 데이터 전송 중
    USB_LED_MSC_READY,          // 3: Blue Solid — MSC 준비됨 (idle)
    USB_LED_MSC_LOGGING,        // 4: Blue Flickering — MSC 로깅 중
    USB_LED_MSC_NEARLY_FULL,    // 5: Blue Solid + Red Single Flash — 용량 경고
    USB_LED_ERROR,              // 6: Red Solid — USB 에러
} UsbLedState_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ===== Link Status LED (RGB GPIO) ===== */

void LedManager_InitLinkStatusLeds(IOIF_GPIOx_t r_id, IOIF_GPIOx_t g_id, IOIF_GPIOx_t b_id);
void LedManager_SetLinkState(LinkState_t state);

/* ===== User Function LED (Single GPIO) ===== */

void LedManager_InitUserLeds(IOIF_GPIOx_t led1_id, IOIF_GPIOx_t led2_id, IOIF_GPIOx_t led3_id);
void LedManager_SetUserLedMode(uint8_t led_index, LedMode_t mode, uint32_t period_ms);

/* ===== Channel LED (PCA9957 RGB) ===== */

/**
 * @brief PCA9957 채널 LED 시스템 초기화
 * @note PCA9957_Init() 이후 호출해야 함. system_startup.c에서 호출.
 */
void LedManager_InitChannelLeds(void);

/**
 * @brief 채널 LED의 Green/Red 패턴을 설정합니다.
 * @param[in] ch 채널 LED ID
 * @param[in] green Green LED 패턴 (LED_MODE_xxx)
 * @param[in] red Red LED 패턴 (LED_MODE_xxx)
 */
void LedManager_SetChannelPattern(ChannelLedId_t ch, LedMode_t green, LedMode_t red);

/**
 * @brief 채널 LED의 RGB 값을 직접 설정합니다 (패턴 무시).
 * @param[in] ch 채널 LED ID
 * @param[in] r Red PWM (0~255)
 * @param[in] g Green PWM (0~255)
 * @param[in] b Blue PWM (0~255)
 */
void LedManager_SetChannelRGB(ChannelLedId_t ch, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 채널 LED의 Green/Red/Blue 패턴을 설정합니다.
 * @param[in] ch 채널 LED ID
 * @param[in] green Green LED 패턴
 * @param[in] red   Red LED 패턴
 * @param[in] blue  Blue LED 패턴
 * @note Blue 패턴이 필요한 경우 사용 (MSC 등). 일반적으로 SetChannelPattern 사용.
 */
void LedManager_SetChannelFullPattern(ChannelLedId_t ch,
                                       LedMode_t green,
                                       LedMode_t red,
                                       LedMode_t blue);

/* ===== Device State → LED 자동 매핑 ===== */

void LedManager_SetCanDeviceState(ChannelLedId_t ch, ChannelDevState_t state);
void LedManager_SetGrfSensorState(ChannelLedId_t ch, GrfSensorState_t state);
void LedManager_SetUsbLedState(UsbLedState_t state);

/* ===== Common Tick Update ===== */

void LedManager_Update(uint32_t timestamp_ms);

#endif /* SYSTEM_BOARD_LED_LED_MANAGER_H_ */
