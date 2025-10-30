/**
 * @file bsp_spi.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for SPI functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#include "bsp_spi.h"

/** @defgroup SPI SPI
  * @brief SPI HAL BSP module driver
  * @{
  */
#ifdef HAL_SPI_MODULE_ENABLED

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
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, NULL},         // SPI not defined
    {BSP_SPI2, NULL},         // SPI not defined
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, &hspi4},       // SPI 4
    {BSP_SPI5, &hspi5},       // SPI 5
    {BSP_SPI6, NULL},         // SPI not defined
};

#ifdef _USE_SEMAPHORE
static BSP_SPISemMap_t bsp_spiSemMap[BSP_SPI_COUNT]  = {
		{BSP_SPI_COUNT, 0, 0},
		{BSP_SPI1, &BinSem_SPI1Handle, BinSemSPI1_TIMEOUT},
		{BSP_SPI2, &BinSem_SPI2Handle, BinSemSPI2_TIMEOUT},
		{BSP_SPI3, &BinSem_SPI3Handle, BinSemSPI3_TIMEOUT},
		{BSP_SPI4, &BinSem_SPI4Handle, BinSemSPI4_TIMEOUT},
		{BSP_SPI5, &BinSem_SPI5Handle, BinSemSPI5_TIMEOUT},
		{BSP_SPI6, &BinSem_SPI6Handle, BinSemSPI6_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, NULL},         // SPI not defined
    {BSP_SPI2, NULL},         // SPI not defined
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, &hspi4},       // SPI 4
    {BSP_SPI5, &hspi5},       // SPI 5
    {BSP_SPI6, NULL},         // SPI not defined
};

#ifdef _USE_SEMAPHORE
static BSP_SPISemMap_t bsp_spiSemMap[BSP_SPI_COUNT]  = {
		{BSP_SPI_COUNT, 0, 0},
		{BSP_SPI1, &BinSem_SPI1Handle, BinSemSPI1_TIMEOUT},
		{BSP_SPI2, &BinSem_SPI2Handle, BinSemSPI2_TIMEOUT},
		{BSP_SPI3, &BinSem_SPI3Handle, BinSemSPI3_TIMEOUT},
		{BSP_SPI4, &BinSem_SPI4Handle, BinSemSPI4_TIMEOUT},
		{BSP_SPI5, &BinSem_SPI5Handle, BinSemSPI5_TIMEOUT},
		{BSP_SPI6, &BinSem_SPI6Handle, BinSemSPI6_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, &hspi1},       // SPI 1
    {BSP_SPI2, NULL},         // SPI not defined
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, NULL},         // SPI not defined
    {BSP_SPI5, NULL},         // SPI not defined
    {BSP_SPI6, NULL},         // SPI not defined
};

#ifdef _USE_SEMAPHORE
static BSP_SPISemMap_t bsp_spiSemMap[BSP_SPI_COUNT]  = {
		{BSP_SPI_COUNT, 0, 0},
		{BSP_SPI1, &BinSem_SPI1Handle, BinSemSPI1_TIMEOUT},
		{BSP_SPI2, &BinSem_SPI2Handle, BinSemSPI2_TIMEOUT},
		{BSP_SPI3, &BinSem_SPI3Handle, BinSemSPI3_TIMEOUT},
		{BSP_SPI4, &BinSem_SPI4Handle, BinSemSPI4_TIMEOUT},
		{BSP_SPI5, &BinSem_SPI5Handle, BinSemSPI5_TIMEOUT},
		{BSP_SPI6, &BinSem_SPI6Handle, BinSemSPI6_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* SUIT_MINICM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, &hspi1},       // SPI 1
    {BSP_SPI2, NULL},         // SPI not defined
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, NULL},         // SPI not defined
    {BSP_SPI5, NULL},         // SPI not defined
    {BSP_SPI6, NULL},         // SPI not defined
};
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, &hspi1},       // SPI 1
    {BSP_SPI2, NULL},         // SPI not defined
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, NULL},         // SPI not defined
    {BSP_SPI5, NULL},         // SPI not defined
    {BSP_SPI6, NULL},         // SPI not defined
};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, &hspi1},       // SPI 1
    {BSP_SPI2, NULL},         // SPI not defined
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, NULL},         // SPI not defined
    {BSP_SPI5, NULL},         // SPI not defined
    {BSP_SPI6, NULL},         // SPI not defined
};
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MD_ENABLED
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, &hspi1},       // SPI 1
    {BSP_SPI2, NULL},         // SPI not defined
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, NULL},         // SPI not defined
    {BSP_SPI5, NULL},         // SPI not defined
    {BSP_SPI6, NULL},         // SPI not defined
};
#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED

#endif /* WIDM_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
static BSP_SPIMap_t bsp_spiMap[BSP_SPI_COUNT] = {
    {BSP_SPI_COUNT, NULL},    // Dummy entry for index 0
    {BSP_SPI1, &hspi1},       // SPI 1
    {BSP_SPI2, &hspi2},       // SPI 2
    {BSP_SPI3, &hspi3},       // SPI 3
    {BSP_SPI4, &hspi4},       // SPI 4
    {BSP_SPI5, NULL},         // SPI not defined
    {BSP_SPI6, NULL},         // SPI not defined
};
#endif /* EXPANSION_BOARD_ENABLED */

static BSP_SPICB_t bsp_spiCB[BSP_SPI_COUNT];


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static SPI_HandleTypeDef* FindSPIHandle(BSP_SPI_t spi);
static BSP_SPI_t FindSPIEnum(SPI_HandleTypeDef* hspi);
static void ExecuteSPICB(SPI_HandleTypeDef* hspi, BSP_SPICBType_t callbackType);

#ifdef _USE_SEMAPHORE
static BSP_SPISemMap_t FindSPISemaphoreHandle(BSP_SPI_t spi);
#endif /* _USE_SEMAPHORE */


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
/**
 * @brief Initialize the SPI peripheral.
 * @param spi Identifier for the SPI peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_InitSPI(BSP_SPI_t spi) 
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_StatusTypeDef_t)HAL_SPI_Init(hspi);
}

/**
 * @brief Deinitialize the SPI peripheral.
 * @param spi Identifier for the SPI peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_DeInitSPI(BSP_SPI_t spi) 
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_StatusTypeDef_t)HAL_SPI_DeInit(hspi);
}

/* ------------------- SPI DEVICE CHECK ------------------- */
/**
 * @brief Retrieve the current SPI state.
 * @param spi Identifier for the SPI peripheral.
 * @return HAL status.
 */
BSP_SPIStateTypeDef_t BSP_GetStateSPI(BSP_SPI_t spi)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_SPIStateTypeDef_t)HAL_SPI_GetState(hspi);
}

/**
 * @brief Retrieve the current SPI error state.
 * @param spi Identifier for the SPI peripheral.
 * @return HAL status.
 */
uint32_t BSP_GetErrorSPI(BSP_SPI_t spi)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return HAL_SPI_GetError(hspi);
}

/* ------------------- BLOCKING MODE ------------------- */
/**
 * @brief Perform a blocking SPI operation.
 * @param spi SPI device identifier.
 * @param pTxData Pointer to transmission data.
 * @param pRxData Pointer to receive data buffer.
 * @param size Amount of data to be sent/received.
 * @param timeout timeout duration. in millisecond.
 * @param operation SPI operation type.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunSPIBlock(BSP_SPI_t spi, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, uint32_t timeout, BSP_SPIOP_t operation)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_SPI_TRANSMIT:
            status = (BSP_StatusTypeDef_t)HAL_SPI_Transmit(hspi, pTxData, size, timeout);
            break;
        case BSP_SPI_RECEIVE:
            status = (BSP_StatusTypeDef_t)HAL_SPI_Receive(hspi, pRxData, size, timeout);
            break;
        case BSP_SPI_TRANSMIT_RECEIVE:
            status = (BSP_StatusTypeDef_t)HAL_SPI_TransmitReceive(hspi, pTxData, pRxData, size, timeout);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief Abort a blocking SPI operation.
 * @param spi SPI device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_AbortSPIBlock(BSP_SPI_t spi)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_StatusTypeDef_t)HAL_SPI_Abort(hspi);
}

/* ------------------- NON-BLOCKING MODE WITH IT ------------------- */
/**
 * @brief Perform a non-blocking SPI operation with interrupts.
 * @param spi SPI device identifier.
 * @param pTxData Pointer to transmission data.
 * @param pRxData Pointer to receive data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation SPI operation type.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunSPIIT(BSP_SPI_t spi, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_SPIOP_t operation)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    BSP_StatusTypeDef_t status = BSP_OK;

#ifdef _USE_SEMAPHORE
    BSP_SPISemMap_t BinSemSPI = FindSPISemaphoreHandle(spi);

    switch (operation) {
        case BSP_SPI_TRANSMIT_IT:
        	if(osSemaphoreAcquire(*BinSemSPI.BinSemHdlr, BinSemSPI.timeout) == osOK)
        		status = (BSP_StatusTypeDef_t)HAL_SPI_Transmit_IT(hspi, pTxData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_SPI_RECEIVE_IT:
        	if(osSemaphoreAcquire(*BinSemSPI.BinSemHdlr, BinSemSPI.timeout) == osOK)
        		status = (BSP_StatusTypeDef_t)HAL_SPI_Receive_IT(hspi, pRxData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_SPI_TRANSMIT_RECEIVE_IT:
        	if(osSemaphoreAcquire(*BinSemSPI.BinSemHdlr, BinSemSPI.timeout) == osOK)
        		status = (BSP_StatusTypeDef_t)HAL_SPI_TransmitReceive_IT(hspi, pTxData, pRxData, size);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_SPI_TRANSMIT_IT:
            status = (BSP_StatusTypeDef_t)HAL_SPI_Transmit_IT(hspi, pTxData, size);
            break;
        case BSP_SPI_RECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_SPI_Receive_IT(hspi, pRxData, size);
            break;
        case BSP_SPI_TRANSMIT_RECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_SPI_TransmitReceive_IT(hspi, pTxData, pRxData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Abort a non-blocking SPI operation with interrupts.
 * @param spi SPI device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_AbortSPIIT(BSP_SPI_t spi)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_StatusTypeDef_t)HAL_SPI_Abort_IT(hspi);
}

/* ------------------- NON-BLOCKING MODE WITH DMA ------------------- */
/**
 * @brief Perform a non-blocking SPI operation with DMA.
 * @param spi SPI device identifier.
 * @param pTxData Pointer to transmission data.
 * @param pRxData Pointer to receive data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation SPI operation type.
 * ! dma_buff logic is only for Normal mode
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_RunSPIDMA(BSP_SPI_t spi, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_SPIOP_t operation)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    BSP_StatusTypeDef_t status = BSP_OK;

#ifdef _USE_SEMAPHORE
    BSP_SPISemMap_t BinSemSPI = FindSPISemaphoreHandle(spi);

    switch (operation) {
        case BSP_SPI_TRANSMIT_DMA:
            if (pTxData) {
            	if(osSemaphoreAcquire(*BinSemSPI.BinSemHdlr, BinSemSPI.timeout) == osOK)
            		status = (BSP_StatusTypeDef_t)HAL_SPI_Transmit_DMA(hspi, pTxData, size);
            	else
            		return BSP_ERROR;
            }
            else return BSP_ERROR;
            break;
        case BSP_SPI_RECEIVE_IT:
        	if (pRxData) {
        		if(osSemaphoreAcquire(*BinSemSPI.BinSemHdlr, BinSemSPI.timeout) == osOK)
        			status =  (BSP_StatusTypeDef_t)HAL_SPI_Receive_DMA(hspi, pRxData, size);
        		else
        			return BSP_ERROR;
        	}
        	else return BSP_ERROR;
            break;
        case BSP_SPI_TRANSMIT_RECEIVE_IT:
        	if(pTxData || pRxData)
        	{
        		if(osSemaphoreAcquire(*BinSemSPI.BinSemHdlr, BinSemSPI.timeout) == osOK)
        			status = (BSP_StatusTypeDef_t)HAL_SPI_TransmitReceive_DMA(hspi, pTxData, pRxData, size);
        		else
        			return BSP_ERROR;
        	}
        	else return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_SPI_TRANSMIT_DMA:
            if (!pTxData) {
                return BSP_ERROR; // pTxData cannot be NULL for transmit operation
            }
            status = (BSP_StatusTypeDef_t)HAL_SPI_Transmit_DMA(hspi, pTxData, size);
            break;
        case BSP_SPI_RECEIVE_DMA:
        	if (!pRxData) {
				return BSP_ERROR; // pRxData cannot be NULL for receive operation
			}
        	status =  (BSP_StatusTypeDef_t)HAL_SPI_Receive_DMA(hspi, pRxData, size);
            break;
        case BSP_SPI_TRANSMIT_RECEIVE_DMA:
        	 if (!pTxData || !pRxData) {
				return BSP_ERROR; // Both pTxData and pRxData cannot be NULL for transmit-receive operation
			}
        	 status = (BSP_StatusTypeDef_t)HAL_SPI_TransmitReceive_DMA(hspi, pTxData, pRxData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Pause a DMA-based SPI operation.
 * @param spi SPI device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_PauseSPIDMA(BSP_SPI_t spi)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_StatusTypeDef_t)HAL_SPI_DMAPause(hspi);
}

/**
 * @brief Resume a paused DMA-based SPI operation.
 * @param spi SPI device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_ResumeSPIDMA(BSP_SPI_t spi)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_StatusTypeDef_t)HAL_SPI_DMAResume(hspi);
}

/**
 * @brief Stop a DMA-based SPI operation.
 * @param spi SPI device identifier.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_StopSPIDMA(BSP_SPI_t spi)
{
    SPI_HandleTypeDef* hspi = FindSPIHandle(spi);

    return (BSP_StatusTypeDef_t)HAL_SPI_DMAStop(hspi);
}

/* ------------------- SPI CALLBACKS ------------------- */
/**
 * @brief Registers a custom callback for a specified SPI port and callback type.
 *
 * @param spi Enum value representing the SPI port.
 * @param callbackType Enum value representing the type of callback (e.g., transmit complete, receive complete).
 * @param callback Function pointer to the custom callback function.
 * @param params Pointer to any parameters that the custom callback function may need.
 */
void BSP_SetSPICB(BSP_SPI_t spi, BSP_SPICBType_t callbackType, BSP_SPICBPtr_t callback, void* params)
{
    if (spi < BSP_SPI_COUNT && callbackType < BSP_SPI_CALLBACK_TYPE_COUNT && callback) {
        bsp_spiCB[spi].callbacks[callbackType] = callback;
        bsp_spiCB[spi].params[callbackType] = params;
    }
}

/**
 * @brief  Callback function to be executed upon the completion of an SPI transmit operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

    ExecuteSPICB(hspi, BSP_SPI_TX_CPLT_CALLBACK);
    
#ifdef _USE_SEMAPHORE
    BSP_SPI_t spi = FindSPIEnum(hspi);
    BSP_SPISemMap_t spiBinSem = FindSPISemaphoreHandle(spi);
    osSemaphoreRelease(*spiBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief  Callback function to be executed upon the completion of an SPI receive operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_RX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_SPI_t spi = FindSPIEnum(hspi);
    BSP_SPISemMap_t spiBinSem = FindSPISemaphoreHandle(spi);
    osSemaphoreRelease(*spiBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief  Callback function to be executed upon the completion of an SPI transmit and receive operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_TXRX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_SPI_t spi = FindSPIEnum(hspi);
    BSP_SPISemMap_t spiBinSem = FindSPISemaphoreHandle(spi);
    osSemaphoreRelease(*spiBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief  Callback function to be executed upon the half completion of an SPI transmit operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_TxHalfCpltCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_TX_HALF_CPLT_CALLBACK);
}

/**
 * @brief  Callback function to be executed upon the half completion of an SPI receive operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_RX_HALF_CPLT_CALLBACK);
}

/**
 * @brief  Callback function to be executed upon the half completion of an SPI transmit and receive operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_TxRxHalfCpltCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_TXRX_HALF_CPLT_CALLBACK);
}

/**
 * @brief  Callback function to be executed when an error occurs during an SPI operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_ERROR_CALLBACK);
}

/**
 * @brief  Callback function to be executed upon the completion of an SPI abort operation.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_ABORT_CPLT_CALLBACK);
}

/**
 * @brief  Callback function to be executed when an SPI operation is suspended.
 * @param  hspi: Pointer to a SPI_HandleTypeDef structure that contains the configuration information for the specified SPI module.
 */
void HAL_SPI_SuspendCallback(SPI_HandleTypeDef* hspi)
{
    UNUSED(hspi);

   	ExecuteSPICB(hspi, BSP_SPI_SUSPEND_CALLBACK);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Executes a custom callback for a specified SPI port and callback type.
 *
 * @param hspi HAL SPI handle, used to find the corresponding SPI port enum value.
 * @param callbackType Enum value representing the type of callback to execute.
 */
static void ExecuteSPICB(SPI_HandleTypeDef* hspi, BSP_SPICBType_t callbackType)
{
    BSP_SPI_t spi = FindSPIEnum(hspi);
    if (spi < BSP_SPI_COUNT) {
        BSP_SPICBPtr_t callback = bsp_spiCB[spi].callbacks[callbackType];
        void* params = bsp_spiCB[spi].params[callbackType];
        if (callback) {
            callback(params); // Executes the custom callback with the specified parameters
        }
    }
}

/**
 * @brief Find the SPI handle corresponding to the specified SPI enumeration
 * 
 * @param spi SPI enumeration
 * @return Pointer to SPI handle or NULL if not found
 */
static SPI_HandleTypeDef* FindSPIHandle(BSP_SPI_t spi) 
{
    for (int i = 0; i < ARRAY_SIZE(bsp_spiMap); i++) {
        if (bsp_spiMap[i].spi == spi) {
            return bsp_spiMap[i].handle;
        }
    }
    return NULL;
}

static BSP_SPI_t FindSPIEnum(SPI_HandleTypeDef* hspi)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_spiMap); i++) {
        if (bsp_spiMap[i].handle == hspi) {
            return bsp_spiMap[i].spi;
        }
    }
    return BSP_SPI_COUNT; // TODO: Change Status
}

#ifdef _USE_SEMAPHORE
static BSP_SPISemMap_t FindSPISemaphoreHandle(BSP_SPI_t spi)
{
	BSP_SPISemMap_t res = {BSP_SPI_COUNT, NULL, 0};

    for (uint8_t i = 0; i < ARRAY_SIZE(bsp_spiSemMap); i++) {
        if (bsp_spiSemMap[i].spi == spi) {
            return bsp_spiSemMap[i];
        }
    }
    return res;
}
#endif /* _USE_SEMAPHORE */

#endif /* HAL_SPI_MODULE_ENABLED */
