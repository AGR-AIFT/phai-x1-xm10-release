

#ifndef IOIF_COMMON_INC_IOIF_FDCAN_COMMON_H_
#define IOIF_COMMON_INC_IOIF_FDCAN_COMMON_H_

#include "bsp_fdcan.h"

/** @defgroup FDCAN FDCAN
  * @brief FDCAN BSP module driver
  * @{
  */
#ifdef BSP_FDCAN_MODULE_ENABLED

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define IOIF_TDC_OFFSET 0x0B // Transmitter Delay Compensation Offset. This parameter must be a number between 0x00 and 0x7F, 0x0B = 11 (DataPrescaler * DataTimeSeg1)

#define IOIF_TDC_FILTER 0x00 // tdcFilter Transmitter Delay Compensation Filter Window Length. This parameter must be a number between 0x00 and 0x7F.

#define IOIF_FDCAN_TX_BUFF_SIZE 64
#define IOIF_FDCAN_RX_BUFF_SIZE 64


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef int (*IOIF_FDCANUserCBPtr_t)(uint16_t, uint8_t*);


/**
 * @brief Function pointer type for FDCAN callback functions.
 */
typedef void (*IOIF_FDCANRxFifoxCBPtr_t)(void* params);

typedef enum _IOIF_FDCANState_t {
    IOIF_FDCAN_STATUS_OK = 0,
    IOIF_FDCAN_STATUS_ERROR,
	IOIF_FDCANSTATUS_BUSY,
	IOIF_FDCANSTATUS_TIMEOUT
} IOIF_FDCANState_t;

/**
 * @brief Enumeration for IOIF FDCAN identifiers.
 * Starts from 1 to align with common STM32 naming (I2C1, I2C2, ...)
 */
typedef enum _IOIF_FDCAN_t {
	IOIF_FDCAN1 = 1,
	IOIF_FDCAN2,
	IOIF_FDCAN_COUNT
} IOIF_FDCAN_t;

/**
 * @enum IOIF_I2CCallbackType_t
 * @brief FDCAN callback types for various fdcan events.
 */
typedef enum _IOIF_FDCANCBType_t {
    IOIF_FDCAN_CLOCKCALIBRATIONCALLBACK,
    IOIF_FDCAN_TXEVENTFIFOCALLBACK,
    IOIF_FDCAN_RXFIFO0CALLBACK,
    IOIF_FDCAN_RXFIFO1CALLBACK,
    IOIF_FDCAN_TXFIFOEMPTYCALLBACK,
    IOIF_FDCAN_TXBUFFERCOMPLETECALLBACK,
    IOIF_FDCAN_TXBUFFERABORTCALLBACK,
    IOIF_FDCAN_RXBUFFERNEWMESSAGECALLBACK,
    IOIF_FDCAN_TIMESTAMPWRAPAROUNDCALLBACK,
    IOIF_FDCAN_TIMEOUTOCCURREDCALLBACK,
    IOIF_FDCAN_HIGHPRIORITYMESSAGECALLBACK,
    IOIF_FDCAN_ERRORCALLBACK,
    IOIF_FDCAN_ERRORSTATUSCALLBACK,
    IOIF_FDCAN_TT_SCHEDULESYNCCALLBACK,
    IOIF_FDCAN_TT_TIMEMARKCALLBACK,
    IOIF_FDCAN_TT_STOPWATCHCALLBACK,
    IOIF_FDCAN_TT_GLOBALTIMECALLBACK,
    IOIF_FDCAN_CALLBACK_TYPE_COUNT,
} IOIF_FDCANCBType_t;

// Define the structure type
typedef struct _IOIF_DLCLookupTable_t {
    uint32_t maxLen;
    uint32_t dlcValue;
} IOIF_DLCLookupTable_t;

/**
 * @brief Structure for FDCAN Object.
 */
typedef struct _IOIF_FDCANObj_t {
	BSP_FDCANFilterTypeDef_t sFilterConfig;
	BSP_FDCANTxHeaderTypeDef_t TxHeader;
	BSP_FDCANRxHeaderTypeDef_t RxHeader;

	uint32_t maskWindow;
	uint32_t userFilter1;
	uint32_t userFilter2;

	uint8_t RxData[IOIF_FDCAN_RX_BUFF_SIZE];
} IOIF_FDCANObj_t;


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

IOIF_FDCANState_t IOIF_InitFDCAN1(uint32_t nodeId);
IOIF_FDCANState_t IOIF_InitFDCAN2(uint32_t nodeId);
uint8_t IOIF_TransmitFDCAN1(uint16_t msgId, uint8_t* txData, uint32_t len);
uint8_t IOIF_TransmitFDCAN2(uint16_t msgId, uint8_t* txData, uint32_t len);
void IOIF_SetFDCANRxCB(IOIF_FDCAN_t fdcan, IOIF_FDCANCBType_t callbackType, IOIF_FDCANUserCBPtr_t funcPtr);


#endif /* BSP_FDCAN_MODULE_ENABLED */

#endif /* IOIF_COMMON_INC_IOIF_FDCAN_COMMON_H_ */
