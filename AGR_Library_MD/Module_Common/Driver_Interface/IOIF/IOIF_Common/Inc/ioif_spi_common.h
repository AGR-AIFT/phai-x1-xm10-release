

#ifndef IOIF_COMMON_INC_IOIF_SPI_COMMON_H_
#define IOIF_COMMON_INC_IOIF_SPI_COMMON_H_

#include "bsp_spi.h"
#include "module.h"

/** @defgroup SPI SPI
  * @brief SPI BSP module driver
  * @{
  */
#ifdef BSP_SPI_MODULE_ENABLED

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
typedef void (*IOIF_SPICBPtr_t)(void* params);

typedef enum _IOIF_SPIState_t {
    IOIF_SPI_STATUS_OK = 0,
    IOIF_SPI_STATUS_ERROR,
    IOIF_SPI_STATUS_BUSY,
    IOIF_SPI_STATUS_TIMEOUT,
} IOIF_SPIState_t;

/**
 * @brief Enumeration for IOIF SPI identifiers.
 * Starts from 1 to align with common STM32 naming (SPI1, SPI2, ...)
 */
typedef enum _IOIF_SPI_t {
    IOIF_SPI1 = 1,  ///< SPI 1 Identifier
    IOIF_SPI2,      ///< SPI 2 Identifier
    IOIF_SPI3,      ///< SPI 3 Identifier
    IOIF_SPI4,      ///< SPI 4 Identifier
    IOIF_SPI_COUNT
} IOIF_SPI_t;

/**
 * @enum IOIF_SPICBType_t
 * @brief SPI callback types for various spi events.
 */
typedef enum _IOIF_SPICBType_t {
    IOIF_SPI_TX_CPLT_CALLBACK,
    IOIF_SPI_RX_CPLT_CALLBACK,
    IOIF_SPI_TXRX_CPLT_CALLBACK,
    IOIF_SPI_TX_HALF_CPLT_CALLBACK,
    IOIF_SPI_RX_HALF_CPLT_CALLBACK,
    IOIF_SPI_TXRX_HALF_CPLT_CALLBACK,
    IOIF_SPI_ERROR_CALLBACK,
    IOIF_SPI_ABORT_CPLT_CALLBACK,
    IOIF_SPI_SUSPEND_CALLBACK,
    IOIF_SPI_CALLBACK_TYPE_COUNT,
} IOIF_SPICBType_t;


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */

#ifdef WALKON5_CM_ENABLED
extern uint8_t spi3DmaTxBuff[256] __attribute__((section(".spi3TxBuff"))); // AM SPI Communication Tx
extern uint8_t spi3DmaTxTestBuff[256] __attribute__((section(".spi3TxTestBuff"))); // AM SPI Communication Tx Test 

extern uint8_t  spi3DmaRxBuff[256] __attribute__((section(".spi3RxBuff"))); // AM SPI Communication Rx
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
extern uint8_t spi3DmaTxBuff[256] __attribute__((section(".spi3TxBuff"))); // AM SPI Communication Tx
extern uint8_t spi3DmaTxTestBuff[256] __attribute__((section(".spi3TxTestBuff"))); // AM SPI Communication Tx Test 

extern uint8_t  spi3DmaRxBuff[256] __attribute__((section(".spi3RxBuff"))); // AM SPI Communication Rx
#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED 
// TODO : Please Update!
extern uint16_t spi1DmaRxBuff __attribute__((section(".spi1RxBuff")));  // RTC Module
extern uint16_t spi2DmaRxBuff __attribute__((section(".spi2RxBuff")));  // not in use
extern uint16_t spi3DmaRxBuff __attribute__((section(".spi3RxBuff")));  // LED Driver
extern uint16_t spi4DmaRxBuff __attribute__((section(".spi4RxBuff")));  // not in use
#endif /* SUIT_MINICM_ENABLED */


/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

uint8_t IOIF_SetSPICB(IOIF_SPI_t spi, IOIF_SPICBType_t callbackType, IOIF_SPICBPtr_t callback, void* params);


#endif /* BSP_SPI_MODULE_ENABLED */

#endif /* IOIF_COMMON_INC_IOIF_SPI_COMMON_H_ */
