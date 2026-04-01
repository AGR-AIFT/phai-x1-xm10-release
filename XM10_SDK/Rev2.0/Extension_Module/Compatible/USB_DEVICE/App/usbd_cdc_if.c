/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.c
  * @version        : v1.0_Cube
  * @brief          : Usb device for Virtual Com Port.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include "main.h" // for SCB functions if needed
#include "stm32h7xx_compatible.h" // NON_CACHE_SECTION 정의 사용
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */
#include "boot_ftp_trigger.h"
#include "cdc_handler.h"
#include <string.h>
#include <stdbool.h>
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_CDC_IF
  * @{
  */

/** @defgroup USBD_CDC_IF_Private_TypesDefinitions USBD_CDC_IF_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Defines USBD_CDC_IF_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Macros USBD_CDC_IF_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Variables USBD_CDC_IF_Private_Variables
  * @brief Private variables.
  * @{
  */
/* Create buffer for reception and transmission           */
/* It's up to user to redefine and/or remove those define */
/** Received data over USB are stored in this buffer      */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

/** Data to send over USB CDC are stored in this buffer   */
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* USER CODE BEGIN PRIVATE_VARIABLES */
/* 버퍼를 Non-Cacheable 영역으로 이동 */
__attribute__((section(D3_NON_CACHE_SECTION), aligned(32)))
static uint8_t _UserRxRingBufferFS[APP_RX_DATA_SIZE];

__attribute__((section(D3_NON_CACHE_SECTION), aligned(32)))
static uint8_t _UserTxRingBufferFS[APP_TX_DATA_SIZE];

#if ((APP_RX_DATA_SIZE-1) & APP_RX_DATA_SIZE) != 0
#error "APP_RX_DATA_SIZE is not a power of 2"
#endif
#if ((APP_TX_DATA_SIZE-1) & APP_TX_DATA_SIZE) != 0
#error "APP_TX_DATA_SIZE is not a power of 2"
#endif

/* [신규] IOIF로 이벤트를 전달할 함수 포인터 (ISR에서 읽으므로 volatile) */
static void (* volatile s_tx_cplt_cb)(void) = NULL;
static void (* volatile s_rx_cplt_cb)(uint8_t*, uint32_t) = NULL;
static void (* volatile s_dtr_change_cb)(uint8_t) = NULL;

/* [FTP] Deferred TX: CDC_Receive_FS(ISR)에서 TX BUSY 시 저장, TxComplete에서 우선 전송 */
__attribute__((section(D3_NON_CACHE_SECTION), aligned(32)))
static uint8_t s_ftp_deferred_buf[64];
static volatile uint32_t s_ftp_deferred_len = 0;

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Variables USBD_CDC_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */
//void CDC_Assign_Tx_Complete_Callback(void (*callback)(void*));
/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_FunctionPrototypes USBD_CDC_IF_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */
static int32_t _Boot_FTP_CdcTxWrapper(const uint8_t* data, uint32_t len);
/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitCplt_FS
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the CDC media low layer over the FS USB IP
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Init_FS(void)
{
  /* USER CODE BEGIN 3 */
  // _UserRxRingBufferFS_Head = 0;
  // _UserRxRingBufferFS_Tail = 0;

  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, _UserTxRingBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, _UserRxRingBufferFS);

  /* Why: QUERY_INFO response requires a TX callback to send data back to host.
   *      Without this, _SendQueryInfoResponse() exits early (s_tx_func == NULL).
   * What: Register CDC TX wrapper so App can respond to FTP commands in App mode.
   * Impact: Enables PhAI Studio device detection without entering BL mode first. */
  Boot_FTP_Init(_Boot_FTP_CdcTxWrapper);

  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  DeInitializes the CDC media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_DeInit_FS(void)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  Manage the CDC class requests
  * @param  cmd: Command code
  * @param  pbuf: Buffer containing command data (request parameters)
  * @param  length: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */
  switch(cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

    case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

    case CDC_SET_COMM_FEATURE:

    break;

    case CDC_GET_COMM_FEATURE:

    break;

    case CDC_CLEAR_COMM_FEATURE:

    break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
    case CDC_SET_LINE_CODING:

    break;

    case CDC_GET_LINE_CODING:

    break;

    case CDC_SET_CONTROL_LINE_STATE:
    {
      /*
       * wLength=0인 요청이므로 pbuf는 USBD_SetupReqTypedef* 포인터.
       * DTR = wValue bit 0, RTS = wValue bit 1 (USB CDC PSTN spec)
       */
      USBD_SetupReqTypedef* setup = (USBD_SetupReqTypedef*)pbuf;
      uint8_t dtr = (setup != NULL) ? (setup->wValue & 0x01) : 0;
      if (s_dtr_change_cb != NULL) {
          s_dtr_change_cb(dtr);
      }
    }
    break;

    case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  *
  *         @note
  *         This function will issue a NAK packet on any OUT packet received on
  *         USB endpoint until exiting this function. If you exit this function
  *         before transfer is complete on CDC interface (ie. using DMA controller)
  *         it will result in receiving more data while previous ones are still
  *         not sent.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */

  /* Phase 3: Scan for ENTER_BOOTLOADER command before normal processing.
   * If detected, device resets into BL FTP mode (function never returns).
   * If not detected, returns immediately — zero overhead for normal traffic. */
  Boot_FTP_CheckForEnterBL(Buf, *Len);

  /* 수신된 데이터를 상위 계층(IOIF)으로 전달 */
  if (s_rx_cplt_cb != NULL) {
      s_rx_cplt_cb(Buf, *Len);
  }

  // 다음 수신을 위해 반드시 다시 설정해야 합니다.
  /* 다음 패킷 수신 준비 (이걸 안 하면 한 번 받고 멈춤) */
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, _UserRxRingBufferFS);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  CDC_Transmit_FS
  *         Data to send over USB IN endpoint are sent over CDC interface
  *         through this function.
  *         @note
  *
  *
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
  */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* USER CODE END 7 */
  return result;
}

/**
  * @brief  CDC_TransmitCplt_FS
  *         Data transmitted callback
  *
  *         @note
  *         This function is IN transfer complete callback used to inform user that
  *         the submitted Data is successfully sent over USB.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 13 */
  UNUSED(Buf); UNUSED(Len); UNUSED(epnum);

  /* [FTP] Deferred FTP response — 스트리밍보다 우선 전송.
   * CDC_Receive_FS(ISR)에서 TX BUSY로 실패한 FTP 응답을 여기서 재시도.
   * 성공 시 이번 사이클은 스트리밍 TxComplete를 스킵하고,
   * 다음 TxComplete에서 스트리밍이 재개됨 (최대 1ms 지연). */
  if (s_ftp_deferred_len > 0) {
      if (CDC_Transmit_FS(s_ftp_deferred_buf, (uint16_t)s_ftp_deferred_len) == USBD_OK) {
          s_ftp_deferred_len = 0;
          return USBD_OK;
      }
  }

  // 등록된 Tx 완료 콜백을 파라미터와 함께 호출합니다.
  /* 전송 완료 사실을 상위 계층(IOIF)에 알림 -> 다음 데이터 전송 트리거 */
  if (s_tx_cplt_cb != NULL) {
      s_tx_cplt_cb();
  }

  /* USER CODE END 13 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
 * @brief AGR_Boot_App TX wrapper — adapts CDC_Transmit_FS signature for FTP response.
 * @note  Called from CDC_Receive_FS() ISR context via Boot_FTP_CheckForEnterBL().
 */
static int32_t _Boot_FTP_CdcTxWrapper(const uint8_t* data, uint32_t len)
{
    if (len > sizeof(s_ftp_deferred_buf)) { return -1; }

    /* Copy to Non-Cacheable static buffer (D3 SRAM).
     * Why: 'data' points to a stack buffer in _SendQueryInfoResponse().
     *      On H7 with D-Cache, USB DMA reads stale physical RAM.
     *      D3 is Non-Cacheable — always coherent with USB peripheral. */
    memcpy(s_ftp_deferred_buf, data, len);

    /* Suspend streaming — prevents new sensor data from being queued.
     * Existing data in ring buffer may still be DMA'd by _TryTransmit. */
    CdcStream_OnConnectionChanged(false);

    /* Try immediate TX. If streaming TX is in-flight, TxState != 0 → BUSY.
     * In that case, store in deferred buffer. CDC_TransmitCplt_FS will send
     * the deferred FTP response as soon as the current streaming TX completes.
     * Why no force-clear: overriding TxState during active DMA corrupts the
     * in-flight transfer, causing both streaming and FTP data loss.
     * With streaming active, TxComplete is guaranteed to fire (~1ms max). */
    if (CDC_Transmit_FS(s_ftp_deferred_buf, (uint16_t)len) != USBD_OK) {
        s_ftp_deferred_len = len;
    }

    return 0;
}

/* [신규] IOIF에서 콜백을 등록하기 위한 함수 */
void CDC_Register_Callbacks(void (*tx_cb)(void), void (*rx_cb)(uint8_t*, uint32_t))
{
    s_tx_cplt_cb = tx_cb;
    s_rx_cplt_cb = rx_cb;
}

void CDC_Register_DTR_Callback(void (*dtr_cb)(uint8_t))
{
    s_dtr_change_cb = dtr_cb;
}

// //Need Optimization
// uint32_t CDC_Receive_Rx_Data(uint8_t* pbuf, uint32_t len)
// {
//   if (len == 0 || pbuf == NULL ) return;

//   uint32_t available = ((_UserRxRingBufferFS_Head - _UserRxRingBufferFS_Tail) > 0) ?
//                        (_UserRxRingBufferFS_Head - _UserRxRingBufferFS_Tail) :
//                        (APP_RX_DATA_SIZE - (_UserRxRingBufferFS_Tail - _UserRxRingBufferFS_Head));

//   if (len > available) len = available;

//   for (uint32_t i = 0; i < len; i++) {
//       pbuf[i] = _UserRxRingBufferFS[_UserRxRingBufferFS_Tail];
//       _UserRxRingBufferFS_Tail = (_UserRxRingBufferFS_Tail + 1) & (APP_RX_DATA_SIZE - 1);
//   }

//   return len;
// }
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */
