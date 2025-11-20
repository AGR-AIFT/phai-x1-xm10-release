/**
 ******************************************************************************
 * @file    ioif_agrb_usb.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] Angel Robotics 펌웨어 공통 정의
 * @details
 * - ioif_agrb_fs, ioif_agrb_usb 등 모든 IOIF 모듈에서 공유하는
 * 기본 상태(AGRBStatusDef)와 매크로를 정의합니다.
 * @version 0.1
 * @date    Nov 10, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_USB_ENABLE)

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_USB_H_
#define IOIF_INC_IOIF_AGRB_USB_H_

#include "usbh_core.h"      // USBH_UserProcess ID enum을 위해
#include "usbd_cdc.h"       // USBD_CDC_HandleTypeDef

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// CDC TX 완료 대기 최대 시간 (ms)
#define IOIF_USB_CDC_TX_TIMEOUT_MS  (100)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief USB Host 이벤트 콜백 함수 포인터 타입
 * @details System Layer(usb_mode_handler)가 이벤트를 수신하기 위해 등록합니다.
 * @param event_id (usbh_core.h에 정의된 HOST_USER_... 값)
 */
typedef void (*IOIF_USB_HostCallback_t)(uint8_t event_id);

/**
 * @brief USB Device CDC 송신 콜백 함수 포인터 타입
 * @details System Layer(cdc_handler)가 데이터를 송신하기 위해 등록합니다.
 */
typedef void (*IOIF_USB_CDC_TxCallback_t)(void); // Tx Cplt

/**
 * @brief USB Device CDC 수신 콜백 함수 포인터 타입
 * @details System Layer(cdc_handler)가 데이터를 수신하기 위해 등록합니다.
 * @param data 수신된 데이터 버퍼 포인터
 * @param length 수신된 데이터 길이
 */
typedef void (*IOIF_USB_CDC_RxCallback_t)(uint8_t* data, uint32_t length);

/**
 * @brief [신규] ISR이 현재 활성화된 스택을 참조하기 위한 전역 플래그
 * @details 이 변수들은 ioif_agrb_usb.c가 "소유"하고 정의합니다.
 */
extern volatile bool g_is_host_initialized;
extern volatile bool g_is_device_initialized;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ===================================================================
 * HOST 모드 API
 * =================================================================== */

/**
 * @brief USB Host 스택을 초기화하고 콜백을 등록합니다.
 * @details System Layer(usb_mode_handler)가 Host 모드로 전환 시 호출합니다.
 * @param[in] user_callback Host 이벤트를 수신할 콜백 함수
 * @return AGRBStatusDef (ioif_agrb_defs.h 기준)
 */
AGRBStatusDef ioif_usb_host_init(IOIF_USB_HostCallback_t user_callback);

/**
 * @brief USB Host 스택을 중지하고 비활성화합니다.
 * @return AGRBStatusDef (ioif_agrb_defs.h 기준)
 */
AGRBStatusDef ioif_usb_host_deinit(void);

/**
 * @brief Host 포트가 활성화(Reset 완료)되었는지 확인합니다.
 * @return true (활성화됨), false (비활성화)
 */
bool ioif_usb_host_is_port_enabled(void);

/* ===================================================================
 * DEVICE 모드 API
 * =================================================================== */

/**
 * @brief USB Device 스택(CDC)을 초기화하고 콜백을 등록합니다.
 * @details System Layer(usb_mode_handler)가 Device 모드로 전환 시 호출합니다.
 * @param[in] tx_callback CDC 데이터를 송신할 콜백 함수
 * @param[in] rx_callback CDC 데이터를 수신할 콜백 함수
 * @return AGRBStatusDef (ioif_agrb_defs.h 기준)
 */
AGRBStatusDef ioif_usb_device_init(IOIF_USB_CDC_TxCallback_t user_tx_callback, IOIF_USB_CDC_RxCallback_t user_rx_callback);

/**
 * @brief USB Device 스택을 중지하고 비활성화합니다.
 * @return AGRBStatusDef (ioif_agrb_defs.h 기준)
 */
AGRBStatusDef ioif_usb_device_deinit(void);

#endif /* IOIF_INC_IOIF_AGRB_USB_H_ */

#endif /* AGRB_IOIF_USB_ENABLE */
