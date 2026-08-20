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

/* DMA 버퍼용 D-Cache 라인(32B) 경계 정렬 — SCB_Clean/InvalidateDCache_by_Addr 가
 * 캐시 라인 단위로 동작하므로, 인접 변수의 라인 공유(오염) 방지를 위해 DMA 버퍼는
 * 이 매크로로 정렬한다. (섹션 속성과 결합 시: __attribute__((section(X))) IOIF_DMA_ALIGNED)
 * XM10 = H7 고정이라 단일 정의. IOIF 서브모듈 내부 정의(ioif_agrb_defs.h 예정)와의
 * 일원화는 별도 IOIF 세션 사안 — 이름을 맞춰 충돌 없이 수렴 가능하도록 ifndef 가드. */
#ifndef IOIF_DMA_ALIGNED
#define IOIF_DMA_ALIGNED __attribute__((aligned(32)))
#endif


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