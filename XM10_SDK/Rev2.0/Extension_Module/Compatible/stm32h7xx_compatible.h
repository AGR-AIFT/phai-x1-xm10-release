#ifndef _STM32H7XX_COMPATIBLE_H
#define _STM32H7XX_COMPATIBLE_H

// This file is for compatibility with older HAL drivers that may not have certain modules enabled.
#define HAL_HCD_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED

// Define the USBH handle as external, so that it can be used in other files.
#define hUSB_Host (hUsbHostFS)

// [수정] D2 Domain 섹션 이름으로 변경
#define D2_NON_CACHE_SECTION ".RAM_D2_data"
#define D3_NON_CACHE_SECTION ".RAM_D3_data"


//Interrupt Handler 
void OTG_FS_EP1_OUT_IRQHandler(void);
void OTG_FS_EP1_IN_IRQHandler(void);
void OTG_FS_IRQHandler(void);


#define USE_PCD_DEFAULT (0)

#if defined(HAL_PCD_MODULE_ENABLED) && (USE_PCD_DEFAULT == 0)

#define USBD_VID                        (0x1234)
#define USBD_LANGID_STRING              (1033)
#define USBD_MANUFACTURER_STRING        ("AngelRobotics Inc.")
#define USBD_PID_FS                     (0x5678)
#define USBD_PRODUCT_STRING_FS          ("AngelRobotics Virtual ComPort")
#define USBD_CONFIGURATION_STRING_FS    ("CDC Config")
#define USBD_INTERFACE_STRING_FS        ("CDC Interface")

#define USB_SIZ_BOS_DESC                (0x0C)

#endif //HAL_PCD_MODULE_ENABLED

#endif //_STM32H7XX_COMPATIBLE_H