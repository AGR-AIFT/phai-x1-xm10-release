/**
 * @file bsp_uart.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for UART functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef UART_INC_BSP_UART_H_
#define UART_INC_BSP_UART_H_

#include "main.h"
#include "module.h"

/** @defgroup UART UART
  * @brief UART HAL BSP module driver
  * @
  */
#ifdef HAL_UART_MODULE_ENABLED
#define BSP_UART_MODULE_ENABLED

#include "usart.h"
#include "bsp_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/** @defgroup UARTEx_Word_Length UARTEx Word Length
  * @{
  */
#define BSP_UART_WORDLENGTH_7B UART_WORDLENGTH_7B   ///< USART_CR1_M1   /*!< 7-bit long UART frame */
#define BSP_UART_WORDLENGTH_8B UART_WORDLENGTH_8B   ///< 0x00000000U    /*!< 8-bit long UART frame */
#define BSP_UART_WORDLENGTH_9B UART_WORDLENGTH_9B   ///< USART_CR1_M0   /*!< 9-bit long UART frame */

/** @defgroup UARTEx_WakeUp_Address_Length UARTEx WakeUp Address Length
  * @{
  */
#define BSP_UART_ADDRESS_DETECT_4B UART_ADDRESS_DETECT_4B   ///< 0x00000000U      /*!< 4-bit long wake-up address */
#define BSP_UART_ADDRESS_DETECT_7B UART_ADDRESS_DETECT_7B   ///< USART_CR2_ADDM7  /*!< 7-bit long wake-up address */

/** @defgroup UARTEx_FIFO_mode UARTEx FIFO mode
  * @brief    UART FIFO mode
  * @{
  */
#define BSP_UART_FIFOMODE_DISABLE  UART_FIFOMODE_DISABLE    ///< 0x00000000U       /*!< FIFO mode disable */
#define BSP_UART_FIFOMODE_ENABLE   UART_FIFOMODE_ENABLE     ///< USART_CR1_FIFOEN  /*!< FIFO mode enable  */

/** @defgroup UARTEx_TXFIFO_threshold_level UARTEx TXFIFO threshold level
  * @brief    UART TXFIFO threshold level
  * @{
  */
#define BSP_UART_TXFIFO_THRESHOLD_1_8 UART_TXFIFO_THRESHOLD_1_8   ///< 0x00000000U                               /*!< TX FIFO reaches 1/8 of its depth */
#define BSP_UART_TXFIFO_THRESHOLD_1_4 UART_TXFIFO_THRESHOLD_1_4   ///< USART_CR3_TXFTCFG_0                       /*!< TX FIFO reaches 1/4 of its depth */
#define BSP_UART_TXFIFO_THRESHOLD_1_2 UART_TXFIFO_THRESHOLD_1_2   ///< USART_CR3_TXFTCFG_1                       /*!< TX FIFO reaches 1/2 of its depth */
#define BSP_UART_TXFIFO_THRESHOLD_3_4 UART_TXFIFO_THRESHOLD_3_4   ///< (USART_CR3_TXFTCFG_0|USART_CR3_TXFTCFG_1) /*!< TX FIFO reaches 3/4 of its depth */
#define BSP_UART_TXFIFO_THRESHOLD_7_8 UART_TXFIFO_THRESHOLD_7_8   ///< USART_CR3_TXFTCFG_2                       /*!< TX FIFO reaches 7/8 of its depth */
#define BSP_UART_TXFIFO_THRESHOLD_8_8 UART_TXFIFO_THRESHOLD_8_8   ///< (USART_CR3_TXFTCFG_2|USART_CR3_TXFTCFG_0) /*!< TX FIFO becomes empty            */

/** @defgroup UARTEx_RXFIFO_threshold_level UARTEx RXFIFO threshold level
  * @brief    UART RXFIFO threshold level
  * @{
  */
#define BSP_UART_RXFIFO_THRESHOLD_1_8 UART_RXFIFO_THRESHOLD_1_8   ///< 0x00000000U                               /*!< RX FIFO reaches 1/8 of its depth */
#define BSP_UART_RXFIFO_THRESHOLD_1_4 UART_RXFIFO_THRESHOLD_1_4   ///< USART_CR3_RXFTCFG_0                       /*!< RX FIFO reaches 1/4 of its depth */
#define BSP_UART_RXFIFO_THRESHOLD_1_2 UART_RXFIFO_THRESHOLD_1_2   ///< USART_CR3_RXFTCFG_1                       /*!< RX FIFO reaches 1/2 of its depth */
#define BSP_UART_RXFIFO_THRESHOLD_3_4 UART_RXFIFO_THRESHOLD_3_4   ///< (USART_CR3_RXFTCFG_0|USART_CR3_RXFTCFG_1) /*!< RX FIFO reaches 3/4 of its depth */
#define BSP_UART_RXFIFO_THRESHOLD_7_8 UART_RXFIFO_THRESHOLD_7_8   ///< USART_CR3_RXFTCFG_2                       /*!< RX FIFO reaches 7/8 of its depth */
#define BSP_UART_RXFIFO_THRESHOLD_8_8 UART_RXFIFO_THRESHOLD_8_8   ///< (USART_CR3_RXFTCFG_2|USART_CR3_RXFTCFG_0) /*!< RX FIFO becomes full             */

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Function pointer type for UART callback functions.
 */
typedef void (*BSP_UARTCBPtr_t)(uint16_t size);

/**
  * @brief HAL UART State definition
  * @note  HAL UART State value is a combination of 2 different substates:
  *        gState and RxState (see @ref UART_State_Definition).
  *        - gState contains UART state information related to global Handle management
  *          and also information related to Tx operations.
  *          gState value coding follow below described bitmap :
  *          b7-b6  Error information
  *             00 : No Error
  *             01 : (Not Used)
  *             10 : Timeout
  *             11 : Error
  *          b5     Peripheral initialization status
  *             0  : Reset (Peripheral not initialized)
  *             1  : Init done (Peripheral initialized. HAL UART Init function already called)
  *          b4-b3  (not used)
  *             xx : Should be set to 00
  *          b2     Intrinsic process state
  *             0  : Ready
  *             1  : Busy (Peripheral busy with some configuration or internal operations)
  *          b1     (not used)
  *             x  : Should be set to 0
  *          b0     Tx state
  *             0  : Ready (no Tx operation ongoing)
  *             1  : Busy (Tx operation ongoing)
  *        - RxState contains information related to Rx operations.
  *          RxState value coding follow below described bitmap :
  *          b7-b6  (not used)
  *             xx : Should be set to 00
  *          b5     Peripheral initialization status
  *             0  : Reset (Peripheral not initialized)
  *             1  : Init done (Peripheral initialized)
  *          b4-b2  (not used)
  *            xxx : Should be set to 000
  *          b1     Rx state
  *             0  : Ready (no Rx operation ongoing)
  *             1  : Busy (Rx operation ongoing)
  *          b0     (not used)
  *             x  : Should be set to 0.
  */
typedef uint32_t BSP_UARTStateTypeDef_t;

/**
 * @brief Enumeration for BSP UART operations.
 *
 * Represents the various uart-related operations available
 * within the BSP layer, abstracting HAL functions.
 */
typedef enum _BSP_UARTOP_t {
    // Peripheral Control functions
    BSP_UART_RECEIVERTIMEOUT_CONFIG,
    BSP_UART_ENABLERECEIVERTIMEOUT,
    BSP_UART_DISABLERECEIVERTIMEOUT,
    BSP_MULTIPROCESSOR_ENABLEMUTEMODE,
    BSP_MULTIPROCESSOR_DISABLEMUTEMODE,
    BSP_MULTIPROCESSOR_ENTERMUTEMODE,
    BSP_HALFDUPLEX_ENABLETRANSMITTER,
    BSP_HALFDUPLEX_ENABLERECEIVER,
    BSP_LIN_SENDBREAK,
    BSP_MULTIPROCESSOREX_ADDRESSLENGTH_SET,
    BSP_UARTEX_STOPMODEWAKEUPSOURCECONFIG,
    BSP_UARTEX_ENABLESTOPMODE,
    BSP_UARTEX_DISABLESTOPMODE,
    BSP_UARTEX_ENABLEFIFOMODE,
    BSP_UARTEX_DISABLEFIFOMODE,
    BSP_UARTEX_SETTXFIFOTHRESHOLD,
    BSP_UARTEX_SETRXFIFOTHRESHOLD,
    BSP_UARTEX_RECEIVETOIDLE,
    BSP_UARTEX_RECEIVETOIDLE_IT,
    BSP_UARTEX_RECEIVETOIDLE_DMA,
    BSP_UARTEX_GETRXEVENTTYPE,

    // IO operation functions
    BSP_UART_TRANSMIT,
    BSP_UART_RECEIVE,
    BSP_UART_TRANSMIT_IT,
    BSP_UART_RECEIVE_IT,
    BSP_UART_TRANSMIT_DMA,
    BSP_UART_RECEIVE_DMA,
    BSP_UART_DMAPAUSE,
    BSP_UART_DMARESUME,
    BSP_UART_DMASTOP,
    BSP_UART_ABORT,
    BSP_UART_ABORTTRANSMIT,
    BSP_UART_ABORTRECEIVE,
    BSP_UART_ABORT_IT,
    BSP_UART_ABORTTRANSMIT_IT,
    BSP_UART_ABORTRECEIVE_IT,
} BSP_UARTOP_t;

/**
 * @brief Enumeration for BSP UART identifiers.
 * Starts from 1 to align with common STM32 naming (UART1, UART2, ...)
 */
typedef enum _BSP_UART_t {
    BSP_UART1 = 1,  ///< UART 1 Identifier
    BSP_UART2,      ///< UART 2 Identifier
    BSP_UART3,      ///< UART 3 Identifier
    BSP_UART4,      ///< UART 4 Identifier
    BSP_UART5,      ///< UART 5 Identifier
    BSP_UART6,      ///< UART 6 Identifier
    BSP_UART7,      ///< UART 7 Identifier
    BSP_UART8,      ///< UART 8 Identifier
    BSP_UART_COUNT
} BSP_UART_t;

/**
 * @enum BSP_UARTCBType_t
 * @brief UART callback types for various uart events.
 */
typedef enum _BSP_UARTCBType_t {
    BSP_UART_TX_CPLT_CALLBACK,
    BSP_UART_TX_HALF_CPLT_CALLBACK,
    BSP_UART_RX_CPLT_CALLBACK,
    BSP_UART_RX_HALF_CPLT_CALLBACK,
    BSP_UART_ERROR_CALLBACK,
    BSP_UART_ABORT_CPLT_CALLBACK,
    BSP_UART_ABORT_TRANSMIT_CPLT_CALLBACK,
    BSP_UART_ABORT_RECEIVE_CPLT_CALLBACK,
    BSP_UARTEX_RX_EVENT_CALLBACK,
    BSP_UARTEX_WAKEUP_CALLBACK,
    BSP_UARTEX_RX_FIFO_FULL_CALLBACK,
    BSP_UARTEX_TX_FIFO_EMPTY_CALLBACK,
    BSP_UART_CALLBACK_TYPE_COUNT,
} BSP_UARTCBType_t;

/**
 * @struct BSP_UARTMap_t
 * @brief Maps BSP UART enumerations to their corresponding HAL UART handles.
 */
typedef struct _BSP_UARTMap_t {
    BSP_UART_t uart;       ///< Enumeration of the uarter
    UART_HandleTypeDef* handle;  ///< Pointer to the HAL UART handle
} BSP_UARTMap_t;

#ifdef _USE_SEMAPHORE
typedef struct _BSP_UARTSemMap_t {
	BSP_UART_t uart;
	osSemaphoreId_t* BinSemHdlr;
	uint32_t timeout;
} BSP_UARTSemMap_t;
#endif

/**
 * @struct BSP_UARTCB_t
 * @brief Manager BSP UART Custom Callbacks and Parameters.
 */
typedef struct _BSP_UARTCB_t {
    BSP_UARTCBPtr_t callbacks[BSP_UART_CALLBACK_TYPE_COUNT];
    uint16_t sizeParams[BSP_UART_CALLBACK_TYPE_COUNT];
} BSP_UARTCB_t;


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */




/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
BSP_StatusTypeDef_t BSP_InitUART(BSP_UART_t uart);
BSP_StatusTypeDef_t BSP_DeInitUART(BSP_UART_t uart);

/* ------------------- UART DEVICE CHECK ------------------- */
BSP_UARTStateTypeDef_t BSP_GetStateUART(BSP_UART_t uart);
uint32_t BSP_GetErrorUART(BSP_UART_t uart);

/* ------------------- PERIPHERAL CONTROL FUNCTIONS ------------------- */
BSP_StatusTypeDef_t BSP_FifoUARTEx(BSP_UART_t uart, BSP_UARTOP_t operation);
BSP_StatusTypeDef_t BSP_SetFifoThUARTEx(BSP_UART_t uart, uint32_t threshold, BSP_UARTOP_t operation);

/* ------------------- BLOCKING MODE ------------------- */
BSP_StatusTypeDef_t BSP_RunUARTBlock(BSP_UART_t uart, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, uint32_t timeout, BSP_UARTOP_t operation);
BSP_StatusTypeDef_t BSP_AbortUARTBlock(BSP_UART_t uart, BSP_UARTOP_t operation);

/* ------------------- NON-BLOCKING MODE WITH IT ------------------- */
BSP_StatusTypeDef_t BSP_RunUARTIT(BSP_UART_t uart, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_UARTOP_t operation);
BSP_StatusTypeDef_t BSP_AbortUARTIT(BSP_UART_t uart, BSP_UARTOP_t operation);

/* ------------------- NON-BLOCKING MODE WITH DMA ------------------- */
BSP_StatusTypeDef_t BSP_RunUARTDMA(BSP_UART_t uart, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_UARTOP_t operation);
BSP_StatusTypeDef_t BSP_PauseUARTDMA(BSP_UART_t uart);
BSP_StatusTypeDef_t BSP_ResumeUARTDMA(BSP_UART_t uart);
BSP_StatusTypeDef_t BSP_StopUARTDMA(BSP_UART_t uart);

/* ------------------- UART CALLBACKS ------------------- */
void BSP_SetUARTCB(BSP_UART_t uart, BSP_UARTCBType_t callbackType, BSP_UARTCBPtr_t callback, uint16_t sizeParams);


#endif /* HAL_UART_MODULE_ENABLED */

#endif /* UART_INC_BSP_UART_H_ */
