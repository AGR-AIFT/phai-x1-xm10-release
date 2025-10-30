/**
 * @file bsp_spi.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for SPI functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef SPI_INC_BSP_SPI_H_
#define SPI_INC_BSP_SPI_H_

#include "main.h"
#include "module.h"

/** @defgroup SPI SPI
  * @brief SPI HAL BSP module driver
  * @{
  */
#ifdef HAL_SPI_MODULE_ENABLED
#define BSP_SPI_MODULE_ENABLED

#include "spi.h"
#include "bsp_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */




/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Function pointer type for SPI callback functions.
 */
typedef void (*BSP_SPICBPtr_t)(void* params);

/**
  * @brief  HAL SPI State structure definition
  */
typedef enum _BSP_SPIStateTypeDef_t{
    BSP_SPI_STATE_RESET      = HAL_SPI_STATE_RESET,         /*!< Peripheral not Initialized                         */
    BSP_SPI_STATE_READY      = HAL_SPI_STATE_READY,         /*!< Peripheral Initialized and ready for use           */
    BSP_SPI_STATE_BUSY       = HAL_SPI_STATE_BUSY,          /*!< an internal process is ongoing                     */
    BSP_SPI_STATE_BUSY_TX    = HAL_SPI_STATE_BUSY_TX,       /*!< Data Transmission process is ongoing               */
    BSP_SPI_STATE_BUSY_RX    = HAL_SPI_STATE_BUSY_RX,       /*!< Data Reception process is ongoing                  */
    BSP_SPI_STATE_BUSY_TX_RX = HAL_SPI_STATE_BUSY_TX_RX,    /*!< Data Transmission and Reception process is ongoing */
    BSP_SPI_STATE_ERROR      = HAL_SPI_STATE_ERROR,         /*!< SPI error state                                    */
    BSP_SPI_STATE_ABORT      = HAL_SPI_STATE_ABORT          /*!< SPI abort is ongoing                               */
} BSP_SPIStateTypeDef_t;


/**
 * @brief Enumeration for BSP SPI operations.
 *
 * Represents the various spi-related operations available
 * within the BSP layer, abstracting HAL functions.
 */
typedef enum _BSP_SPIOP_t {
    // I2S Blocking Mode
    BSP_SPI_TRANSMIT,
    BSP_SPI_RECEIVE,
    BSP_SPI_TRANSMIT_RECEIVE,

    // I2S Non-Blocking Mode with IT
    BSP_SPI_TRANSMIT_IT,
    BSP_SPI_RECEIVE_IT,
    BSP_SPI_TRANSMIT_RECEIVE_IT,

    // I2S Non-Blocking Mode with DMA
    BSP_SPI_TRANSMIT_DMA,
    BSP_SPI_RECEIVE_DMA,
    BSP_SPI_TRANSMIT_RECEIVE_DMA,
} BSP_SPIOP_t;

/**
 * @brief Enumeration for BSP SPI identifiers.
 * Starts from 1 to align with common STM32 naming (SPI1, SPI2, ...)
 */
typedef enum _BSPSPI_t {
    BSP_SPI1 = 1,  ///< SPI 1 Identifier
    BSP_SPI2,      ///< SPI 2 Identifier
    BSP_SPI3,      ///< SPI 3 Identifier
    BSP_SPI4,      ///< SPI 4 Identifier
    BSP_SPI5,      ///< SPI 5 Identifier
    BSP_SPI6,      ///< SPI 6 Identifier
    BSP_SPI_COUNT
} BSP_SPI_t;

/**
 * @enum BSP_SPICBType_t
 * @brief SPI callback types for various spi events.
 */
typedef enum _BSP_SPICBType_t {
    BSP_SPI_TX_CPLT_CALLBACK,
    BSP_SPI_RX_CPLT_CALLBACK,
    BSP_SPI_TXRX_CPLT_CALLBACK,
    BSP_SPI_TX_HALF_CPLT_CALLBACK,
    BSP_SPI_RX_HALF_CPLT_CALLBACK,
    BSP_SPI_TXRX_HALF_CPLT_CALLBACK,
    BSP_SPI_ERROR_CALLBACK,
    BSP_SPI_ABORT_CPLT_CALLBACK,
    BSP_SPI_SUSPEND_CALLBACK,
    BSP_SPI_CALLBACK_TYPE_COUNT,
} BSP_SPICBType_t;

/**
 * @struct BSP_SPIMap_t
 * @brief Maps BSP SPI enumerations to their corresponding HAL SPI handles.
 */
typedef struct _BSP_SPIMap_t {
    BSP_SPI_t spi;       ///< Enumeration of the spier
    SPI_HandleTypeDef* handle;  ///< Pointer to the HAL SPI handle
} BSP_SPIMap_t;

#ifdef _USE_SEMAPHORE
typedef struct _BSP_SPISemMap_t {
	BSP_SPI_t spi;
	osSemaphoreId_t* BinSemHdlr;
	uint32_t timeout;
} BSP_SPISemMap_t;
#endif /* _USE_SEMAPHORE */

/**
 * @struct BSP_SPICB_t
 * @brief Manager BSP SPI Custom CBs and Parameters.
 */
typedef struct _BSP_SPICB_t {
    BSP_SPICBPtr_t callbacks[BSP_SPI_CALLBACK_TYPE_COUNT];
    void* params[BSP_SPI_CALLBACK_TYPE_COUNT];
} BSP_SPICB_t;


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
BSP_StatusTypeDef_t BSP_InitSPI(BSP_SPI_t spi);
BSP_StatusTypeDef_t BSP_DeInitSPI(BSP_SPI_t spi);

/* ------------------- SPI DEVICE CHECK ------------------- */
BSP_SPIStateTypeDef_t BSP_GetStateSPI(BSP_SPI_t spi);
uint32_t BSP_GetErrorSPI(BSP_SPI_t spi);

/* ------------------- BLOCKING MODE ------------------- */
BSP_StatusTypeDef_t BSP_RunSPIBlock(BSP_SPI_t spi, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, uint32_t timeout, BSP_SPIOP_t operation);
BSP_StatusTypeDef_t BSP_AbortSPIBlock(BSP_SPI_t spi);

/* ------------------- NON-BLOCKING MODE WITH IT ------------------- */
BSP_StatusTypeDef_t BSP_RunSPIIT(BSP_SPI_t spi, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_SPIOP_t operation);
BSP_StatusTypeDef_t BSP_AbortSPIIT(BSP_SPI_t spi);

/* ------------------- NON-BLOCKING MODE WITH DMA ------------------- */
BSP_StatusTypeDef_t BSP_RunSPIDMA(BSP_SPI_t spi, const uint8_t* pTxData, uint8_t* pRxData, uint16_t size, BSP_SPIOP_t operation);
BSP_StatusTypeDef_t BSP_PauseSPIDMA(BSP_SPI_t spi);
BSP_StatusTypeDef_t BSP_ResumeSPIDMA(BSP_SPI_t spi);
BSP_StatusTypeDef_t BSP_StopSPIDMA(BSP_SPI_t spi);

/* ------------------- SPI CALLBACKS ------------------- */
void BSP_SetSPICB(BSP_SPI_t spi, BSP_SPICBType_t callbackType, BSP_SPICBPtr_t callback, void* params);


#endif /* HAL_SPI_MODULE_ENABLED */

#endif /* SPI_INC_BSP_SPI_H_ */
