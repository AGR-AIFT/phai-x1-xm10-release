/**
 *-----------------------------------------------------------
 *                 FDCAN Communication Driver
 *-----------------------------------------------------------
 * @date Created on: Aug 23, 2023
 * @author AngelRobotics HW Team
 * @brief Driver code for the FDCAN communication.
 *
 * This source file provides functionality to interface
 *
 * @ref FDCAN reference
 */

#include "ioif_fdcan_common.h"

/** @defgroup FDCAN FDCAN
  * @brief FDCAN BSP module driver
  * @{
  */
#ifdef BSP_FDCAN_MODULE_ENABLED

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

static IOIF_FDCANUserCBPtr_t userRxCBPtr1;
static IOIF_FDCANUserCBPtr_t userRxCBPtr2;


/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */
static IOIF_FDCANObj_t* fdcanPtr = NULL;

static IOIF_FDCANObj_t fdcan1Obj = {0};
static IOIF_FDCANObj_t fdcan2Obj = {0};

static BSP_FDCANFilterTypeDef_t IOIF_FDCANsFilter = {
	.IdType = BSP_FDCAN_STANDARD_ID,
	.FilterIndex = 0,
	.FilterType = BSP_FDCAN_FILTER_RANGE,
	.FilterConfig = BSP_FDCAN_FILTER_TO_RXFIFO0,
	.FilterID1 = 0x000,
	.FilterID2 = 0x7FF,
	.RxBufferIndex = 0,			// ignored when (FilterConfig != FDCAN_FILTER_TO_RXBUFFER
	.IsCalibrationMsg = 0		// ignored when (FilterConfig != FDCAN_FILTER_TO_RXBUFFER
};

static BSP_FDCANTxHeaderTypeDef_t IOIF_FDCANTxHeader = {
	.Identifier = 0,
	.IdType = BSP_FDCAN_STANDARD_ID,
	.TxFrameType = BSP_FDCAN_DATA_FRAME,
	.DataLength = 0,
	.ErrorStateIndicator = BSP_FDCAN_ESI_ACTIVE,
	.BitRateSwitch = BSP_FDCAN_BRS_ON,
	.FDFormat = BSP_FDCAN_FD_CAN,
	.TxEventFifoControl = BSP_FDCAN_NO_TX_EVENTS,
	.MessageMarker = 0
};

// Define the static global constant lookup table
static const IOIF_DLCLookupTable_t lookupTable[] = {
    {4,  BSP_FDCAN_DLC_BYTES_4},
    {8,  BSP_FDCAN_DLC_BYTES_8},
    {12, BSP_FDCAN_DLC_BYTES_12},
    {16, BSP_FDCAN_DLC_BYTES_16},
    {20, BSP_FDCAN_DLC_BYTES_20},
    {24, BSP_FDCAN_DLC_BYTES_24},
    {32, BSP_FDCAN_DLC_BYTES_32},
    {48, BSP_FDCAN_DLC_BYTES_48},
    {64, BSP_FDCAN_DLC_BYTES_64}
};


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static void IOIF_AllocateFDCANRxCB(IOIF_FDCAN_t fdcan, IOIF_FDCANUserCBPtr_t funcPtr);
static void RunFDCAN1RxCB(void* params);
static void RunFDCAN2RxCB(void* params);
static uint32_t Len2DLC(uint32_t len);

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/**
 * @brief Initialize FDCAN.
 */
IOIF_FDCANState_t IOIF_InitFDCAN1(uint32_t nodeId)
{
	fdcanPtr = &fdcan1Obj;
#ifdef FDCAN_WALKONSUIT_ENABLED
	fdcanPtr->maskWindow 	 	= nodeId;
#endif
#ifdef FDCAN_H10_ENABLED
	fdcanPtr->maskWindow 	 	= 0x00F;
#endif
	fdcanPtr->userFilter1 	 	= 0;
	fdcanPtr->userFilter2 	 	= nodeId;
	fdcanPtr->sFilterConfig  	= IOIF_FDCANsFilter;
	fdcanPtr->TxHeader			= IOIF_FDCANTxHeader;

	if (BSP_ConfigFDCANGlobalFilter(BSP_FDCAN1, BSP_FDCAN_REJECT, BSP_FDCAN_REJECT, BSP_FDCAN_FILTER_REMOTE, BSP_FDCAN_FILTER_REMOTE) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_ConfigFDCANFilter(BSP_FDCAN1, &fdcanPtr->sFilterConfig) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_ActivateFDCANNotification(BSP_FDCAN1, BSP_FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_ConfigFDCANTxDelay(BSP_FDCAN1, IOIF_TDC_FILTER) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_EnableFDCANTxDelay(BSP_FDCAN1) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_StartFDCAN(BSP_FDCAN1) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	return IOIF_FDCAN_STATUS_OK;
}

IOIF_FDCANState_t IOIF_InitFDCAN2(uint32_t nodeId)
{
	fdcanPtr = &fdcan2Obj;

	fdcanPtr->maskWindow 	 	= 0x00F;
	fdcanPtr->userFilter1 	 	= 0;
	fdcanPtr->userFilter2 	 	= nodeId;
	fdcanPtr->sFilterConfig  	= IOIF_FDCANsFilter;
	fdcanPtr->TxHeader			= IOIF_FDCANTxHeader;

	if (BSP_ConfigFDCANGlobalFilter(BSP_FDCAN2, BSP_FDCAN_REJECT, BSP_FDCAN_REJECT, BSP_FDCAN_FILTER_REMOTE, BSP_FDCAN_FILTER_REMOTE) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_ConfigFDCANFilter(BSP_FDCAN2, &fdcanPtr->sFilterConfig) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_ActivateFDCANNotification(BSP_FDCAN2, BSP_FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_ConfigFDCANTxDelay(BSP_FDCAN2, IOIF_TDC_FILTER) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_EnableFDCANTxDelay(BSP_FDCAN2) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	if (BSP_StartFDCAN(BSP_FDCAN2) != BSP_OK) {
		return IOIF_FDCAN_STATUS_ERROR;
	}

	return IOIF_FDCAN_STATUS_OK;
}


/**
 * @brief Transmit Data with FDCAN
 * @param msgId is identifier, len is message's length, txData is a pointer of data which you want to transmit
 * @return Status of the data transmission
 */
uint8_t IOIF_TransmitFDCAN1(uint16_t msgId, uint8_t* txData, uint32_t len)
{
	fdcanPtr = &fdcan1Obj;

	fdcanPtr->TxHeader.Identifier = msgId;	// msgId = (0x"Msg Type"/"ori_node"/"dest_node")
	fdcanPtr->TxHeader.DataLength = Len2DLC(len);

	return BSP_AddFDCANMsgToTxQ(BSP_FDCAN1, &fdcanPtr->TxHeader, txData);
}

uint8_t IOIF_TransmitFDCAN2(uint16_t msgId, uint8_t* txData, uint32_t len)
{
	fdcanPtr = &fdcan2Obj;

	fdcanPtr->TxHeader.Identifier = msgId;	// msgId = (0x"Msg Type"/"ori_node"/"dest_node")
	fdcanPtr->TxHeader.DataLength = Len2DLC(len);

	return BSP_AddFDCANMsgToTxQ(BSP_FDCAN2, &fdcanPtr->TxHeader, txData);
}


/**
 * @brief Allocating RxFifo0Callback function
 */
static void IOIF_AllocateFDCANRxCB(IOIF_FDCAN_t fdcan, IOIF_FDCANUserCBPtr_t funcPtr)
{
	if (fdcan == IOIF_FDCAN1) {
		userRxCBPtr1 = funcPtr;
	} else if (fdcan == IOIF_FDCAN2) {
		userRxCBPtr2 = funcPtr;
	}
}

void IOIF_SetFDCANRxCB(IOIF_FDCAN_t fdcan, IOIF_FDCANCBType_t callbackType, IOIF_FDCANUserCBPtr_t funcPtr)
{
	if (fdcan == IOIF_FDCAN1) {
		BSP_SetFDCANCB(fdcan, callbackType, RunFDCAN1RxCB, NULL);
	} else if (fdcan == IOIF_FDCAN2) {
		BSP_SetFDCANCB(fdcan, callbackType, RunFDCAN2RxCB, NULL);
	}

	IOIF_AllocateFDCANRxCB(fdcan, funcPtr);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

static void RunFDCAN1RxCB(void* params)
{
	fdcanPtr = &fdcan1Obj;

	if (BSP_GetFDCANRxMsg(IOIF_FDCAN1, BSP_FDCAN_RX_FIFO0, &fdcanPtr->RxHeader, fdcanPtr->RxData) != BSP_OK) {
		// TODO : Handle Error !
		return;
	}

	// For Checking Origin Node
	if (((fdcanPtr->RxHeader.Identifier & fdcanPtr->maskWindow) != fdcanPtr->userFilter1) && \
		((fdcanPtr->RxHeader.Identifier & fdcanPtr->maskWindow) != fdcanPtr->userFilter2)) {
		// TODO : Handle Error !
		return;
	}

	if (userRxCBPtr1 != NULL) {
		userRxCBPtr1(fdcanPtr->RxHeader.Identifier, fdcanPtr->RxData);
	}
}

static void RunFDCAN2RxCB(void* params)
{
	fdcanPtr = &fdcan2Obj;

	if (BSP_GetFDCANRxMsg(IOIF_FDCAN2, BSP_FDCAN_RX_FIFO0, &fdcanPtr->RxHeader, fdcanPtr->RxData) != BSP_OK) {
		// TODO : Handle Error !
		return;
	}

	// For Checking Origin Node
	if (((fdcanPtr->RxHeader.Identifier & fdcanPtr->maskWindow) != fdcanPtr->userFilter1) && \
		((fdcanPtr->RxHeader.Identifier & fdcanPtr->maskWindow) != fdcanPtr->userFilter2)) {
		// TODO : Handle Error !
		return;
	}

	if (userRxCBPtr2 != NULL) {
		userRxCBPtr2(fdcanPtr->RxHeader.Identifier, fdcanPtr->RxData);
	}
}

/**
 * @brief Allocate FDCAN Object according to the FDCAN Port Number.
 */
// Your function using the global lookup table
static uint32_t Len2DLC(uint32_t len)
{
    for(size_t i = 0; i < sizeof(lookupTable) / sizeof(lookupTable[0]); i++) {
        if (len <= lookupTable[i].maxLen) {
            return lookupTable[i].dlcValue;
        }
    }

    return BSP_FDCAN_DLC_BYTES_64;
}


#endif /* BSP_FDCAN_MODULE_ENABLED */

