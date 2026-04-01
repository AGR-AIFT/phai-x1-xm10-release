/**
 ******************************************************************************
 * @file    usb_mode_handler.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 5, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_USB_USB_MODE_HANDLER_H_
#define SYSTEM_COMM_USB_USB_MODE_HANDLER_H_

#include "ioif_agrb_gpio.h"
#include "ioif_agrb_adc.h"
#include "ioif_agrb_usb.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define IOIF_USB_HOST_MAX_SUPPORTED_CLASS   (1) //현재는 MSC 하나만 지원
#define IOIF_USB_DEVICE_MAX_SUPPORTED_CLASS (1) //현재는 CDC 하나만 지원

#define USB_MODE_CHANGE_DEBOUNCE_COUNT   (3)

#define IOIF_USB_CC_PULLDOWN_THRESHOLD(x)    ( (( 1 << x ) - 1) / 5 ) /* 20% */
#define IOIF_USB_CC_PULLUP_THRESHOLD(x)      ( (( 1 << x ) * 4 ) / 5 ) /* 80% */

#define IOIF_USB_DEVICE_VID 0x0483
#define IOIF_USB_DEVICE_PID 0x5740
#define IOIF_USB_DEVICE_MANUFACTURER_STRING "Angel Robotics Inc."
#define IOIF_USB_DEVICE_PRODUCT_STRING "Angel Robotics XM"

#define IOIF_LONG_DATA_DEBUG_MESSAGE "[IOIF-USB] THIS IS A LONG DATA MESSAGE FOR DEBUG PURPOSE. PLEASE IGNORE THIS MESSAGE."
#define IOIF_LONG_DATA_DEBUG_MESSAGE_LENGTH (sizeof(IOIF_LONG_DATA_DEBUG_MESSAGE))

#define IOIF_USB_HOST_CLASS (IOIF_USB_Host_SupportedClass_MSC)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

typedef struct {
    IOIF_ADCx_t cc_id;

    IOIF_GPIOx_t enable_id;
    IOIF_GPIOx_t vbus_id;
    IOIF_GPIOx_t ufp_id;

    IOIF_GPIOx_t overcurrent_id;

    bool enable_host_mode;
    bool enable_device_mode;
} TaskUSBControlTask_Init_t;

extern volatile bool g_isUsbHostReady;
extern volatile bool g_isUsbDeviceReady;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief [신규] USB 제어 모듈을 초기화하고 백그라운드 태스크를 생성합니다.
 * @details system_startup.c에서 호출됩니다.
 * @param[in] init_params IOIF 핸들 등 태스크에 필요한 의존성 정보
 */
void USBControl_Init(TaskUSBControlTask_Init_t* init_params);

// StartUSBControlTask는 __weak가 아닌, 이 모듈의 내부 구현이 됩니다.
// void StartUSBControlTask(void *argument); //Argument is pointer to TaskUSBControlTask_Init_t structure main.h 또는 PFP에서 제거

void EventUSBNotWorkDetected(void); //Event when USB is not working (Unplugged or Overcurrent)

#endif /* SYSTEM_COMM_USB_USB_MODE_HANDLER_H_ */
