/**
 * @file bsp_uart.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for UART functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#include "bsp_uart.h"

/** @defgroup UART UART
  * @brief UART HAL BSP module driver
  * @{
  */
#ifdef HAL_UART_MODULE_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */




/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */




/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */


#ifdef WALKON5_CM_ENABLED

#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED

#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED
static BSP_UARTMap_t bsp_uartMap[BSP_UART_COUNT] = {
    {BSP_UART_COUNT, NULL},    // Dummy entry for index 0
    {BSP_UART1, NULL},       // UART 1
    {BSP_UART2, NULL},         // UART not defined
    {BSP_UART3, NULL},       // UART 3
    {BSP_UART4, NULL},         // UART not defined
    {BSP_UART5, NULL},         // UART not defined
    {BSP_UART6, NULL},         // UART not defined
    {BSP_UART7, &huart7},         // UART not defined
    {BSP_UART8, NULL},         // UART not defined
};

#ifdef _USE_SEMAPHORE
static BSP_UARTSemMap_t bsp_uartSemMap[BSP_UART_COUNT]  = {
    {BSP_UART_COUNT, 0, 0},
    {BSP_UART1, NULL, BinSemUART1_TIMEOUT},
    {BSP_UART2, NULL, BinSemUART2_TIMEOUT},
    {BSP_UART3, NULL, BinSemUART3_TIMEOUT},
    {BSP_UART4, NULL, BinSemUART4_TIMEOUT},
    {BSP_UART5, NULL, BinSemUART5_TIMEOUT},
    {BSP_UART6, NULL, BinSemUART6_TIMEOUT},
    {BSP_UART7, &BinSem_UART7Handle, BinSemUART7_TIMEOUT},
    {BSP_UART8, NULL, BinSemUART8_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */

#endif /* SUIT_MINICM_ENABLED */

#ifdef L30_MD_REV06_ENABLED

#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED

#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV07_ENABLED

#endif /* L30_MD_REV07_ENABLED */

#ifdef SUIT_MD_ENABLED

#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED

#endif /* WIDM_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
static BSP_UARTMap_t bsp_uartMap[BSP_UART_COUNT] = {
    {BSP_UART_COUNT, NULL},    // Dummy entry for index 0
    {BSP_UART1, NULL},         // UART not defined
    {BSP_UART2, NULL},         // UART not defined
    {BSP_UART3, NULL},         // UART not defined
    {BSP_UART4, NULL},         // UART not defined
    {BSP_UART5, NULL},         // UART not defined
    {BSP_UART6, NULL},         // UART not defined
    {BSP_UART7, &huart7},      // UART 7
    {BSP_UART8, NULL},         // UART not defined
};
#endif /* EXPANSION_BOARD_ENABLED */

static BSP_UARTCB_t bsp_uartCB[BSP_UART_COUNT] = {0};


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static UART_HandleTypeDef* FindUARTHandle(BSP_UART_t uart);
static BSP_UART_t FindUARTEnum(UART_HandleTypeDef* huart);
static void ExecuteUARTCB(UART_HandleTypeDef* huart, BSP_UARTCBType_t callbackType);
#ifdef _USE_SEMAPHORE
static BSP_UARTSemMap_t FindUARTSemaphoreHandle(BSP_UART_t spi);
#endif /* _USE_SEMAPHORE */


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
/**
 * @brief Initialize the UART peripheral.
 * @param uart Identifier for the UART peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_InitUART(BSP_UART_t uart) 
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    return (BSP_StatusTypeDef_t)HAL_UART_Init(huart);
}

/**
 * @brief Deinitialize the UART peripheral.
 * @param uart Identifier for the UART peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_DeInitUART(BSP_UART_t uart) 
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    return (BSP_StatusTypeDef_t)HAL_UART_DeInit(huart);
}

/* ------------------- UART CHECK ------------------- */
/**
 * @brief Retrieve the current UART state.
 * @param uart Identifier for the UART peripheral.
 * @return HAL status.
 */
BSP_UARTStateTypeDef_t BSP_GetStateUART(BSP_UART_t uart)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    return (BSP_UARTStateTypeDef_t)HAL_UART_GetState(huart);
}

/**
 * @brief Retrieve the current UART error state.
 * @param uart Identifier for the UART peripheral.
 * @return HAL status.
 */
uint32_t BSP_GetErrorUART(BSP_UART_t uart)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    return HAL_UART_GetError(huart);
}

/* ------------------- PERIPHERAL CONTROL FUNCTIONS ------------------- */
/**
 * @brief  Enables or disables the FIFO mode for a specific UART module.
 * @param  uart: BSP_UART_t type that identifies the UART module to be operated on.
 * @param  operation: BSP_UARTOP_t type that specifies the operation to be performed (enable or disable FIFO mode).
 * @return BSP_StatusTypeDef_t: The status of the operation.
 *          - HAL_OK if the operation was successful.
 *          - BSP_ERROR if the UART handle is not valid or if an invalid operation was specified.
 */
BSP_StatusTypeDef_t BSP_FifoUARTEx(BSP_UART_t uart, BSP_UARTOP_t operation)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_UARTEX_ENABLEFIFOMODE:
            status = (BSP_StatusTypeDef_t)HAL_UARTEx_EnableFifoMode(huart);
            break;
        case BSP_UARTEX_DISABLEFIFOMODE:
            status = (BSP_StatusTypeDef_t)HAL_UARTEx_DisableFifoMode(huart);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief  Sets the FIFO threshold for the specified UART module.
 * @param  uart: BSP_UART_t type that identifies the UART module to be operated on.
 * @param  threshold: uint32_t type that specifies the threshold value.
 * @param  operation: BSP_UARTOP_t type that specifies the operation to be performed (set Tx or Rx FIFO threshold).
 * @return BSP_StatusTypeDef_t: The status of the operation.
 *          - HAL_OK if the operation was successful.
 *          - BSP_ERROR if the UART handle is not valid or if an invalid operation was specified.
 */
BSP_StatusTypeDef_t BSP_SetFifoThUARTEx(BSP_UART_t uart, uint32_t threshold, BSP_UARTOP_t operation)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_UARTEX_SETTXFIFOTHRESHOLD:
            status = (BSP_StatusTypeDef_t)HAL_UARTEx_SetTxFifoThreshold(huart, threshold);
            break;
        case BSP_UARTEX_SETRXFIFOTHRESHOLD:
            status = (BSP_StatusTypeDef_t)HAL_UARTEx_SetRxFifoThreshold(huart, threshold);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- BLOCKING MODE ------------------- */
/**
 * @brief Perform a blocking UART operation.
 * @param uart UART device identifier.
 * @param pTxData Pointer to transmission data.
 * @param pRxData Pointer to receive data buffer.
 * @param size Amount of data to be sent/received.
 * @param timeout timeout duration. in millisecond.
 * @param operation UART operation type.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunUARTBlock(BSP_UART_t uart, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, uint32_t timeout, BSP_UARTOP_t operation)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_UART_TRANSMIT:
            status = (BSP_StatusTypeDef_t)HAL_UART_Transmit(huart, pTxData, size, timeout);
            break;
        case BSP_UART_RECEIVE:
            status = (BSP_StatusTypeDef_t)HAL_UART_Receive(huart, pRxData, size, timeout);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief  Aborts a specific UART operation.
 * @param  huart: Pointer to a UART_HandleTypeDef structure that contains
 *                the configuration information for the specified UART module.
 * @return BSP_StatusTypeDef_t: The status of the operation abort.
 *          - HAL_OK if the abort operation was successful.
 *          - BSP_ERROR if the UART handle is not valid or if an invalid operation was specified.
 */
BSP_StatusTypeDef_t BSP_AbortUARTBlock(BSP_UART_t uart, BSP_UARTOP_t operation)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_UART_ABORT:
            status = (BSP_StatusTypeDef_t)HAL_UART_Abort(huart);
            break;
        case BSP_UART_ABORTTRANSMIT:
            status = (BSP_StatusTypeDef_t)HAL_UART_AbortTransmit(huart);
            break;
        case BSP_UART_ABORTRECEIVE:
            status = (BSP_StatusTypeDef_t)HAL_UART_AbortReceive(huart);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- NON-BLOCKING MODE WITH IT ------------------- */
/**
 * @brief Perform a non-blocking UART operation with interrupts.
 * @param uart UART device identifier.
 * @param pTxData Pointer to transmission data.
 * @param pRxData Pointer to receive data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation UART operation type.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunUARTIT(BSP_UART_t uart, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_UARTOP_t operation)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    BSP_StatusTypeDef_t status = BSP_OK;

#ifdef _USE_SEMAPHORE
    BSP_UARTSemMap_t BinSemUART = FindUARTSemaphoreHandle(uart);

    switch (operation) {
    	case BSP_UART_TRANSMIT_IT:
    		if(osSemaphoreAcquire(*BinSemUART.BinSemHdlr, BinSemUART.timeout) == osOK)
    			status = (BSP_StatusTypeDef_t)HAL_UART_Transmit_IT(huart, pTxData, size);
    		else
    			return BSP_ERROR;
    		break;
    	case BSP_UART_RECEIVE_IT:
    		if(osSemaphoreAcquire(*BinSemUART.BinSemHdlr, BinSemUART.timeout) == osOK)
    			status = (BSP_StatusTypeDef_t)HAL_UART_Receive_IT(huart, pRxData, size);
    		else
    			return BSP_ERROR;
    		break;
    	default:
    		return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_UART_TRANSMIT_IT:
            status = (BSP_StatusTypeDef_t)HAL_UART_Transmit_IT(huart, pTxData, size);
            break;
        case BSP_UART_RECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_UART_Receive_IT(huart, pRxData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

BSP_StatusTypeDef_t BSP_AbortUARTIT(BSP_UART_t uart, BSP_UARTOP_t operation)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_UART_ABORT_IT:
            status = (BSP_StatusTypeDef_t)HAL_UART_Abort_IT(huart);
            break;
        case BSP_UART_ABORTTRANSMIT_IT:
            status = (BSP_StatusTypeDef_t)HAL_UART_AbortTransmit_IT(huart);
            break;
        case BSP_UART_ABORTRECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_UART_AbortReceive_IT(huart);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- NON-BLOCKING MODE WITH DMA ------------------- */
/**
 * @brief Perform a non-blocking UART operation with DMA.
 * @param uart UART device identifier.
 * @param pTxData Pointer to transmission data.
 * @param pRxData Pointer to receive data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation UART operation type.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunUARTDMA(BSP_UART_t uart, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_UARTOP_t operation)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    BSP_StatusTypeDef_t status = BSP_OK;
#ifdef _USE_SEMAPHORE
    BSP_UARTSemMap_t BinSemUART = FindUARTSemaphoreHandle(uart);

    switch (operation) {
    	case BSP_UART_TRANSMIT_DMA:
    		if(osSemaphoreAcquire(*BinSemUART.BinSemHdlr, BinSemUART.timeout) == osOK)
                status = (BSP_StatusTypeDef_t)HAL_UART_Transmit_DMA(huart, pTxData, size);
    		else
    			return BSP_ERROR;
    		break;
    	case BSP_UART_RECEIVE_DMA:
    		if(osSemaphoreAcquire(*BinSemUART.BinSemHdlr, BinSemUART.timeout) == osOK)
                status = (BSP_StatusTypeDef_t)HAL_UART_Receive_DMA(huart, pRxData, size);
    		else
    			return BSP_ERROR;
    		break;
    	default:
    		return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_UART_TRANSMIT_DMA:
            status = (BSP_StatusTypeDef_t)HAL_UART_Transmit_DMA(huart, pTxData, size);
            break;
        case BSP_UART_RECEIVE_DMA:
            status = (BSP_StatusTypeDef_t)HAL_UART_Receive_DMA(huart, pRxData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE*/

    return status;
}

/**
 * @brief Pause a DMA-based UART operation.
 * @param uart UART device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_PauseUARTDMA(BSP_UART_t uart)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    return (BSP_StatusTypeDef_t)HAL_UART_DMAPause(huart);
}

/**
 * @brief Resume a paused DMA-based UART operation.
 * @param uart UART device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_ResumeUARTDMA(BSP_UART_t uart)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    return (BSP_StatusTypeDef_t)HAL_UART_DMAResume(huart);
}

/**
 * @brief Stop a DMA-based UART operation.
 * @param uart UART device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_StopUARTDMA(BSP_UART_t uart)
{
    UART_HandleTypeDef* huart = FindUARTHandle(uart);

    return (BSP_StatusTypeDef_t)HAL_UART_DMAStop(huart);
}

/* ------------------- UART CALLBACKS ------------------- */
/**
 * @brief Registers a custom callback for a specified UART port and callback type.
 *
 * @param uart Enum value representing the UART port.
 * @param callbackType Enum value representing the type of callback (e.g., transmit complete, receive complete).
 * @param callback Function pointer to the custom callback function.
 * @param params Pointer to any parameters that the custom callback function may need.
 * ! This is RxEvent Callback!!
 */
void BSP_SetUARTCB(BSP_UART_t uart, BSP_UARTCBType_t callbackType, BSP_UARTCBPtr_t callback, uint16_t sizeParams)
{
    if (uart < BSP_UART_COUNT && callbackType < BSP_UART_CALLBACK_TYPE_COUNT && callback) {
        bsp_uartCB[uart].callbacks[callbackType] = callback;
        bsp_uartCB[uart].sizeParams[callbackType] = sizeParams;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_TX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_UART_t uart = FindUARTEnum(huart);
    BSP_UARTSemMap_t uartBinSem = FindUARTSemaphoreHandle(uart);
    osSemaphoreRelease(*uartBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_TX_HALF_CPLT_CALLBACK);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_RX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_UART_t uart = FindUARTEnum(huart);
    BSP_UARTSemMap_t uartBinSem = FindUARTSemaphoreHandle(uart);
    osSemaphoreRelease(*uartBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_RX_HALF_CPLT_CALLBACK);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_ERROR_CALLBACK);
}

void HAL_UART_AbortCpltCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_ABORT_CPLT_CALLBACK);
}

void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_ABORT_CPLT_CALLBACK);
}

void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UART_ABORT_CPLT_CALLBACK);
}

void HAL_UART_RxEventCallback(UART_HandleTypeDef* huart, uint16_t size)
{
    UNUSED(huart);

    ExecuteUARTCB(huart, BSP_UARTEX_RX_EVENT_CALLBACK);
}

void HAL_UARTEx_WakeupCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UARTEX_WAKEUP_CALLBACK);
}


void HAL_UARTEx_RxFifoFullCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UARTEX_RX_FIFO_FULL_CALLBACK);
}


void HAL_UARTEx_TxFifoEmptyCallback(UART_HandleTypeDef* huart)
{
    UNUSED(huart);

   	ExecuteUARTCB(huart, BSP_UARTEX_TX_FIFO_EMPTY_CALLBACK);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Executes a custom callback for a specified UART port and callback type.
 *
 * @param huart HAL UART handle, used to find the corresponding UART port enum value.
 * @param callbackType Enum value representing the type of callback to execute.
 */
static void ExecuteUARTCB(UART_HandleTypeDef* huart, BSP_UARTCBType_t callbackType)
{
    BSP_UART_t uart = FindUARTEnum(huart);
    if (uart < BSP_UART_COUNT) {
        BSP_UARTCBPtr_t callback = bsp_uartCB[uart].callbacks[callbackType];
        uint16_t params = bsp_uartCB[uart].sizeParams[callbackType];
        if (callback) {
            callback(params); // Executes the custom callback with the specified parameters
        }
    }
}

/**
 * @brief Find the UART handle corresponding to the specified UART enumeration
 * 
 * @param uart UART enumeration
 * @return Pointer to UART handle or NULL if not found
 */
static UART_HandleTypeDef* FindUARTHandle(BSP_UART_t uart) 
{
	for (int i = 0; i < ARRAY_SIZE(bsp_uartMap); i++) {
        if (bsp_uartMap[i].uart == uart) {
            return bsp_uartMap[i].handle;
        }
    }
    return NULL;
}

static BSP_UART_t FindUARTEnum(UART_HandleTypeDef* huart)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_uartMap); i++) {
        if (bsp_uartMap[i].handle == huart) {
            return bsp_uartMap[i].uart;
        }
    }
    return BSP_UART_COUNT; // TODO: Change Status
}

#ifdef _USE_SEMAPHORE
static BSP_UARTSemMap_t FindUARTSemaphoreHandle(BSP_UART_t uart)
{
	BSP_UARTSemMap_t res = {BSP_UART_COUNT, NULL, 0};

    for (uint8_t i = 0; i < ARRAY_SIZE(bsp_uartSemMap); i++) {
        if (bsp_uartSemMap[i].uart == uart) {
            return bsp_uartSemMap[i];
        }
    }
    return res;
}
#endif /* _USE_SEMAPHORE */


#endif /* HAL_UART_MODULE_ENABLED */
