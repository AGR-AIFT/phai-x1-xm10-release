/**
 ******************************************************************************
 * @file    ioif_agrb_usb.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] USB 3-mode 추상화 (CDC_ONLY / COMPOSITE / DRP)
 * @details
 * 모듈은 AGRB_IOIF_USB_ENABLE 활성 시 아래 3개 중 정확히 하나를 선택해야 한다.
 *   - AGRB_IOIF_USB_MODE_CDC_ONLY   : Device CDC-ACM 단일 (SM 계열)
 *   - AGRB_IOIF_USB_MODE_COMPOSITE  : Device MSC+CDC Composite (CM — SD 드라이브 노출)
 *   - AGRB_IOIF_USB_MODE_DRP        : Host MSC ↔ Device CDC 런타임 스위칭 (XM)
 *
 * 모든 모드의 Device 초기화 경로는 CubeMX가 생성한 MX_USB_DEVICE_Init()에 위임된다.
 * IOIF는 HW reset / CRS(H7) / 콜백 shim / 모드 전환 상호배제만 담당한다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef IOIF_INC_IOIF_AGRB_USB_H_
#define IOIF_INC_IOIF_AGRB_USB_H_

#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_USB_ENABLE)

#include <stdint.h>
#include <stdbool.h>

/*
 *-----------------------------------------------------------
 * MODE VALIDATION — 정확히 하나만 정의되어야 함
 *-----------------------------------------------------------
 */
#define _IOIF_USB_MODE_COUNT   (defined(AGRB_IOIF_USB_MODE_CDC_ONLY)  + \
                                defined(AGRB_IOIF_USB_MODE_COMPOSITE) + \
                                defined(AGRB_IOIF_USB_MODE_DRP))
#if _IOIF_USB_MODE_COUNT == 0
#error "AGRB_IOIF_USB_ENABLE is set but no mode selected. Define exactly one of: AGRB_IOIF_USB_MODE_CDC_ONLY | AGRB_IOIF_USB_MODE_COMPOSITE | AGRB_IOIF_USB_MODE_DRP"
#elif _IOIF_USB_MODE_COUNT > 1
#error "Multiple AGRB_IOIF_USB_MODE_* defined. Select exactly one."
#endif
#undef _IOIF_USB_MODE_COUNT

/* COMPOSITE / DRP 는 H7 OTG FS 전용 (G474 미지원) */
#if defined(AGRB_IOIF_USB_MODE_COMPOSITE) || defined(AGRB_IOIF_USB_MODE_DRP)
  #if !defined(STM32H743xx) && !defined(STM32H750xx)
    #error "AGRB_IOIF_USB_MODE_COMPOSITE / _DRP requires STM32H7 (OTG FS). Use MODE_CDC_ONLY on other chips."
  #endif
#endif

/* COMPOSITE 은 usbd_conf.h 에서 USE_USBD_COMPOSITE 정의되어야 함 */
#if defined(AGRB_IOIF_USB_MODE_COMPOSITE)
  #include "usbd_conf.h"
  #if !defined(USE_USBD_COMPOSITE)
    #error "MODE_COMPOSITE requires USE_USBD_COMPOSITE (regenerate CubeMX with Composite builder)."
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS
 *-----------------------------------------------------------
 */

#define IOIF_USB_CDC_TX_TIMEOUT_MS  (100)

/*
 *-----------------------------------------------------------
 * CALLBACK TYPES (공통)
 *-----------------------------------------------------------
 */

/** @brief CDC TX 완료 콜백 */
typedef void (*IOIF_USB_CDC_TxCallback_t)(void);

/** @brief CDC RX 수신 콜백 */
typedef void (*IOIF_USB_CDC_RxCallback_t)(uint8_t *data, uint32_t length);

/** @brief CDC DTR 상태 변경 콜백 (ISR context) */
typedef void (*IOIF_USB_CDC_DtrCallback_t)(uint8_t dtr);

/*
 * COMPOSITE SD 소유권 타입 (UsbService_SdOwner_e) 은
 * System/Comm/USB/usb_composite_service.h 로 이전되었다 (Plan v6 Step 3.2).
 */

/*
 *-----------------------------------------------------------
 * DRP 전용 타입 — Host 이벤트 콜백
 *-----------------------------------------------------------
 */
#if defined(AGRB_IOIF_USB_MODE_DRP)
/**
 * @brief USB Host 이벤트 콜백 (usbh_core.h 의 HOST_USER_* id 전달)
 */
typedef void (*IOIF_USB_HostCallback_t)(uint8_t event_id);
#endif

/*
 *-----------------------------------------------------------
 * PUBLIC VARIABLES
 *-----------------------------------------------------------
 */

/** @brief USB Device 초기화 완료 플래그 (ISR 참조용) */
extern volatile bool g_is_device_initialized;

#if defined(AGRB_IOIF_USB_MODE_DRP)
/** @brief USB Host 초기화 완료 플래그 (DRP 전환 상호배제용) */
extern volatile bool g_is_host_initialized;
#endif

/*
 *-----------------------------------------------------------
 * PUBLIC API — Device (공통)
 *-----------------------------------------------------------
 */

/**
 * @brief  USB Device 초기화
 * @details HW reset + CRS(H7) + MX_USB_DEVICE_Init() + NVIC enable + CDC 콜백 등록.
 *          모드별 클래스 구성은 CubeMX 생성 코드에 의해 결정된다.
 *          - COMPOSITE : MSC + CDC
 *          - CDC_ONLY / DRP : CDC 단일
 * @param[in] tx_cb  CDC TX 완료 콜백 (NULL 허용)
 * @param[in] rx_cb  CDC RX 수신 콜백 (NULL 허용)
 * @return AGRBStatus_OK | AGRBStatus_BUSY (DRP 모드에서 Host 동작 중) | AGRBStatus_INITIAL_FAILED
 */
AGRBStatusDef ioif_usb_device_init(IOIF_USB_CDC_TxCallback_t tx_cb,
                                    IOIF_USB_CDC_RxCallback_t rx_cb);

/**
 * @brief  USB Device 종료 + HW 강제 리셋
 */
AGRBStatusDef ioif_usb_device_deinit(void);

/**
 * @brief  DTR 콜백 등록 (COM 포트 열림/닫힘 감지)
 * @param[in] callback  DTR 변경 콜백 (ISR context — ISR-safe 함수만 등록)
 */
void ioif_usb_device_register_dtr_callback(IOIF_USB_CDC_DtrCallback_t callback);

/*
 *-----------------------------------------------------------
 * PUBLIC API — COMPOSITE 전용 (HAL 얇은 어댑터만)
 *-----------------------------------------------------------
 * SD 소유권 state machine / DTR deferred / CDC active 조회 / FW 회수 등
 * 정책·상태 API 는 전부 System/Comm/USB/usb_composite_service.h 로 이전.
 */
#if defined(AGRB_IOIF_USB_MODE_COMPOSITE)

/**
 * @brief  MSC SCSI media ready 상태 제어 (PC 측 드라이브 노출 on/off)
 * @details 직접 USBD_Storage_SetMediaActive() 호출 대신 사용할 것.
 *          고수준 정책(예: 로깅 중 MSC 차단)은 usb_composite_service 가 담당하고,
 *          이 함수는 그 서비스가 호출하는 얇은 래퍼이다.
 * @param[in] active true: 드라이브 보임, false: NOT_READY (드라이브 숨김)
 */
void ioif_usb_set_media_active(bool active);

#endif /* MODE_COMPOSITE */

/*
 *-----------------------------------------------------------
 * PUBLIC API — DRP 전용 (XM)
 *-----------------------------------------------------------
 */
#if defined(AGRB_IOIF_USB_MODE_DRP)

/**
 * @brief  USB Host 스택 초기화 (USBH_MSC 클래스 등록)
 * @param[in] user_callback Host 이벤트 콜백 (NULL 허용)
 * @return AGRBStatus_OK | AGRBStatus_BUSY (Device 동작 중) | AGRBStatus_INITIAL_FAILED
 */
AGRBStatusDef ioif_usb_host_init(IOIF_USB_HostCallback_t user_callback);

/**
 * @brief  USB Host 스택 종료 + HW 강제 리셋
 */
AGRBStatusDef ioif_usb_host_deinit(void);

/**
 * @brief  Host 포트 활성(Reset 완료) 여부
 */
bool ioif_usb_host_is_port_enabled(void);

#endif /* MODE_DRP */

#ifdef __cplusplus
}
#endif

#endif /* AGRB_IOIF_USB_ENABLE */

#endif /* IOIF_INC_IOIF_AGRB_USB_H_ */
