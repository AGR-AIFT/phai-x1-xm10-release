/**
 * @file bsp_fdcan.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for FDCAN functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#include "bsp_fdcan.h"

/** @defgroup FDCAN FDCAN
  * @brief FDCAN HAL BSP module driver
  * @{
  */
#ifdef HAL_FDCAN_MODULE_ENABLED

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
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, &hfdcan2},   // FDCAN 2
};

#ifdef _USE_SEMAPHORE
static BSP_FDCANSemMap_t bsp_fdcanSemMap[BSP_FDCAN_COUNT]  = {
		{BSP_FDCAN_COUNT, 0, 0},
		{BSP_FDCAN1, &BinSem_FDCAN1Handle, BinSemFDCAN1_TIMEOUT},
		{BSP_FDCAN2, &BinSem_FDCAN2Handle, BinSemFDCAN2_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, &hfdcan2},   // FDCAN 2
};

#ifdef _USE_SEMAPHORE
static BSP_FDCANSemMap_t bsp_fdcanSemMap[BSP_FDCAN_COUNT]  = {
		{BSP_FDCAN_COUNT, 0, 0},
		{BSP_FDCAN1, &BinSem_FDCAN1Handle, BinSemFDCAN1_TIMEOUT},
		{BSP_FDCAN2, &BinSem_FDCAN2Handle, BinSemFDCAN2_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};

#ifdef _USE_SEMAPHORE
static BSP_FDCANSemMap_t bsp_fdcanSemMap[BSP_FDCAN_COUNT]  = {
		{BSP_FDCAN_COUNT, 0, 0},
		{BSP_FDCAN1, &BinSem_FDCAN1Handle, BinSemFDCAN1_TIMEOUT},
		{BSP_FDCAN2, &BinSem_FDCAN2Handle, BinSemFDCAN2_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* SUIT_MINICM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MD_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};
#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};
#endif /* WIDM_ENABLED */

#ifdef KAIST_GRF_BOARD_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};
#endif /* KAIST_GRF_BOARD_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
static BSP_FDCANMap_t bsp_fdcanMap[BSP_FDCAN_COUNT] = {
    {BSP_FDCAN_COUNT, NULL},  // Dummy entry for index 0
    {BSP_FDCAN1, &hfdcan1},   // FDCAN 1
    {BSP_FDCAN2, NULL},       // FDCAN not defined
};
#endif /* EXPANSION_BOARD_ENABLED */

static BSP_FDCANCB_t bsp_fdcanCB[BSP_FDCAN_COUNT];


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static FDCAN_HandleTypeDef* FindFdcanHandle(BSP_FDCAN_t fdcan);
static BSP_FDCAN_t FindFdcanEnum(FDCAN_HandleTypeDef* fdcan);
static void ExecuteFDCANCB(FDCAN_HandleTypeDef* hfdcan, BSP_FDCANCBType_t callbackType);
#ifdef _USE_SEMAPHORE
static BSP_FDCANSemMap_t FindFDCANSemaphoreHandle(BSP_FDCAN_t fdcan);
#endif /* _USE_SEMAPHORE */

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
/**
 * @brief Initialize the FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_InitFDCAN(BSP_FDCAN_t fdcan)
{   
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_Init(hfdcan);
}

/**
 * @brief Deinitialize the FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_DeInitFDCAN(BSP_FDCAN_t fdcan)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_DeInit(hfdcan);

}

/* ------------------- FDCAN CHECK ------------------- */
/**
 * @brief Retrieve the current FDCAN state.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_GetStateFDCAN(BSP_FDCAN_t fdcan)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_GetState(hfdcan);
}

/**
 * @brief Retrieve the current FDCAN error state.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_GetErrorFDCAN(BSP_FDCAN_t fdcan)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_GetError(hfdcan);

}

/* ------------------- CONFIGURATION FUNCTIONS ------------------- */
/**
 * @brief Configures a filter on the specified FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @param sFilterConfig Pointer to a BSPFDCAN_FilterTypeDef structure that contains the configuration information for the specified filter.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_ConfigFDCANFilter(BSP_FDCAN_t fdcan, BSP_FDCANFilterTypeDef_t* sFilterConfig)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_ConfigFilter(hfdcan, (FDCAN_FilterTypeDef*)sFilterConfig);
}

/**
 * @brief Configures the FDCAN global filter.
 * @param hfdcan pointer to a FDCAN_HandleTypeDef structure that contains the configuration information for the specified FDCAN.
 * @param nonMatchingStd Defines how received messages with 11-bit IDs that do not match any element of the filter list are treated. This parameter can be a value of FDCAN_Non_Matching_Frames.
 * @param nonMatchingExt Defines how received messages with 29-bit IDs that do not match any element of the filter list are treated. This parameter can be a value of FDCAN_Non_Matching_Frames.
 * @param rejectRemoteStd Filter or reject all the remote 11-bit IDs frames. This parameter can be a value of FDCAN_Reject_Remote_Frames.
 * @param rejectRemoteExt Filter or reject all the remote 29-bit IDs frames. This parameter can be a value of FDCAN_Reject_Remote_Frames.
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_ConfigFDCANGlobalFilter(BSP_FDCAN_t fdcan, uint32_t nonMatchingStd, uint32_t nonMatchingExt, uint32_t rejectRemoteStd, uint32_t rejectRemoteExt)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_ConfigGlobalFilter(hfdcan, nonMatchingStd, nonMatchingExt, rejectRemoteStd, rejectRemoteExt);
}

/**
 * @brief Configures the FDCAN transmitter delay compensation.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @param tdcOffset Transmitter Delay Compensation Offset. This parameter must be a number between 0x00 and 0x7F.
 * @param tdcFilter Transmitter Delay Compensation Filter Window Length. This parameter must be a number between 0x00 and 0x7F.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_ConfigFDCANTxDelay(BSP_FDCAN_t fdcan, uint32_t tdcFilter)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_ConfigTxDelayCompensation(hfdcan, hfdcan->Init.DataPrescaler * hfdcan->Init.DataTimeSeg1, tdcFilter);
}

/**
 * @brief Enables the transmitter delay compensation on the specified FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_EnableFDCANTxDelay(BSP_FDCAN_t fdcan)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_EnableTxDelayCompensation(hfdcan);
}

/* ------------------- INTERRUPTS MANAGEMENT ------------------- */
/**
 * @brief Enables the specified interrupts on the specified FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @param activeIT Indicates which interrupts will be enabled. This parameter can be any combination of FDCAN_Interrupts.
 * @param buffIndex Tx Buffer Indexes. This parameter can be any combination of FDCAN_Tx_location. This parameter is ignored if activeIT does not include one of the following: FDCAN_IT_TX_COMPLETE, FDCAN_IT_TX_ABORT_COMPLETE.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_ActivateFDCANNotification(BSP_FDCAN_t fdcan, uint32_t activeIT, uint32_t buffIndex)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_ActivateNotification(hfdcan, activeIT, buffIndex);
}

/* ------------------- CONTROL FUNCTIONS ------------------- */
/**
 * @brief Starts the FDCAN module on the specified FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_StartFDCAN(BSP_FDCAN_t fdcan)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_Start(hfdcan);
}

/**
 * @brief Stops the FDCAN module on the specified FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_StopFDCAN(BSP_FDCAN_t fdcan)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

    return (BSP_StatusTypeDef_t)HAL_FDCAN_Stop(hfdcan);
}

/**
 * @brief Adds a message to the Tx FIFO/Queue and activates the corresponding transmission request on the specified FDCAN peripheral.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @param pTxHeader Pointer to a BSPFDCAN_TxHeaderTypeDef structure.
 * @param pTxData Pointer to a buffer containing the payload of the Tx frame.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_AddFDCANMsgToTxQ(BSP_FDCAN_t fdcan, BSP_FDCANTxHeaderTypeDef_t* pTxHeader, uint8_t* pTxData)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

//#ifdef _USE_SEMAPHORE
//    BSP_FDCANSemMap_t BinSemFDCAN = FindFDCANSemaphoreHandle(fdcan);
//    if(osSemaphoreAcquire(*BinSemFDCAN.BinSemHdlr, BinSemFDCAN.timeout) == osOK)		//Semaphore Wait
//        status = (HAL_StatusTypeDef)HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, (FDCAN_TxHeaderTypeDef*)pTxHeader, pTxData);
//    else
//        return status;
//#else
//    status = (HAL_StatusTypeDef)HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, (FDCAN_TxHeaderTypeDef*)pTxHeader, pTxData);
//#endif /* _USE_SEMAPHORE */

    /* Debug - No Semaphore */
    return (BSP_StatusTypeDef_t)HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, (FDCAN_TxHeaderTypeDef*)pTxHeader, pTxData);
}

/**
 * @brief Get an FDCAN frame from the Rx Buffer/FIFO zone into the message RAM.
 * @param fdcan Identifier for the FDCAN peripheral.
 * @param rxLocation Location of the received message to be read. This parameter can be a value of FDCAN_Rx_location.
 * @param pRxHeader Pointer to a BSPFDCAN_RxHeaderTypeDef structure.
 * @param pRxData Pointer to a buffer where the payload of the Rx frame will be stored.
 * @return BSP_StatusTypeDef_t BSP status.
 */
BSP_StatusTypeDef_t BSP_GetFDCANRxMsg(BSP_FDCAN_t fdcan, uint32_t rxLocation, BSP_FDCANRxHeaderTypeDef_t* pRxHeader, uint8_t* pRxData)
{
    FDCAN_HandleTypeDef* hfdcan = FindFdcanHandle(fdcan);

//#ifdef _USE_SEMAPHORE
//    BSP_FDCANSemMap_t BinSemFDCAN = FindFDCANSemaphoreHandle(fdcan);
//    if(osSemaphoreAcquire(*BinSemFDCAN.BinSemHdlr, BinSemFDCAN.timeout) == osOK)		//Semaphore Wait
//        status = (HAL_StatusTypeDef)HAL_FDCAN_GetRxMessage(hfdcan, rxLocation, (FDCAN_RxHeaderTypeDef*)pRxHeader, pRxData);
//    else
//        return status;
//#else
//    status = (HAL_StatusTypeDef)HAL_FDCAN_GetRxMessage(hfdcan, rxLocation, (FDCAN_RxHeaderTypeDef*)pRxHeader, pRxData);
//#endif /* _USE_SEMAPHORE */

    /* Debug - No Semaphore */
    return (BSP_StatusTypeDef_t)HAL_FDCAN_GetRxMessage(hfdcan, rxLocation, (FDCAN_RxHeaderTypeDef*)pRxHeader, pRxData);
}

/* ------------------- FDCAN CALLBACKS ------------------- */
/**
 * @brief Registers a custom callback for a specified FDCAN port and callback type.
 *
 * @param fdcan Enum value representing the FDCAN port.
 * @param callbackType Enum value representing the type of callback (e.g., transmit complete, receive complete).
 * @param callback Function pointer to the custom callback function.
 * @param params Pointer to any parameters that the custom callback function may need.
 * ! This is Rx FIFO only 
 */
void BSP_SetFDCANCB(BSP_FDCAN_t fdcan, BSP_FDCANCBType_t callbackType, BSP_FDCANRxFifoxCBPtr_t callback, void* params)
{
    if (fdcan < BSP_FDCAN_COUNT && callbackType < BSP_FDCAN_CALLBACK_TYPE_COUNT && callback) {
        bsp_fdcanCB[fdcan].callbacks[callbackType] = callback;
        bsp_fdcanCB[fdcan].params[callbackType] = params;
    }
}

/**
 * @brief Callback function that is invoked when an interrupt is triggered from the Rx FIFO 0 of the specified FDCAN peripheral.
 * @param hfdcan Pointer to a FDCAN_HandleTypeDef structure that contains the configuration information for the specified FDCAN.
 * @param RxFifo0ITs Rx FIFO 0 Interrupts
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs)
{
    UNUSED(hfdcan);

    // TODO: Algorithm Update
    ExecuteFDCANCB(hfdcan, BSP_FDCAN_RXFIFO0CALLBACK);
}

/**
 * @brief Callback function that is invoked when an interrupt is triggered from the Rx FIFO 1 of the specified FDCAN peripheral.
 * @param hfdcan Pointer to a FDCAN_HandleTypeDef structure that contains the configuration information for the specified FDCAN.
 * @param RxFifo1ITs Rx FIFO 1 Interrupts
 */
void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo1ITs)
{
    UNUSED(hfdcan);

    // TODO: Algorithm Update
    ExecuteFDCANCB(hfdcan, BSP_FDCAN_RXFIFO1CALLBACK);
}

#ifdef _USE_SEMAPHORE
void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hfdcan)
{
    UNUSED(hfdcan);

    // TODO: Algorithm Update
    ExecuteFDCANCB(hfdcan, BSP_FDCAN_TXFIFOEMPTYCALLBACK);

    BSP_FDCAN_t fdcan = FindFdcanEnum(hfdcan);
    BSP_FDCANSemMap_t fdcanBinSem = FindFDCANSemaphoreHandle(fdcan);
    osSemaphoreRelease(*fdcanBinSem.BinSemHdlr);
}
#endif /* _USE_SEMAPHORE */


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Executes a custom callback for a specified FDCAN port and callback type.
 * @param hfdcan HAL FDCAN handle, used to find the corresponding FDCAN port enum value.
 * @param callbackType Enum value representing the type of callback to execute.
 * ! This is Rx FIFO only 
 */
static void ExecuteFDCANCB(FDCAN_HandleTypeDef* hfdcan, BSP_FDCANCBType_t callbackType)
{
    BSP_FDCAN_t fdcan = FindFdcanEnum(hfdcan);
    if (fdcan < BSP_FDCAN_COUNT) {
    	BSP_FDCANRxFifoxCBPtr_t callback = bsp_fdcanCB[fdcan].callbacks[callbackType];
        void* params = bsp_fdcanCB[fdcan].params[callbackType];
        if (callback) {
            callback(params); // Executes the custom callback with the specified parameters
        }
    }
}

/**
 * @brief Find the FDCAN handle corresponding to the specified FDCAN enumeration
 * 
 * @param fdcan FDCAN enumeration
 * @return Pointer to FDCAN handle or NULL if not found
 */
static FDCAN_HandleTypeDef* FindFdcanHandle(BSP_FDCAN_t fdcan)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_fdcanMap); i++) {
        if (bsp_fdcanMap[i].fdcan == fdcan) {
            return bsp_fdcanMap[i].handle;
        }
    }
    return NULL;
}

/**
 * @brief Find the FDCAN handle corresponding to the specified FDCAN enumeration
 * 
 * @param fdcan FDCAN enumeration
 * @return Pointer to FDCAN handle or NULL if not found
 */
static BSP_FDCAN_t FindFdcanEnum(FDCAN_HandleTypeDef* fdcan)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_fdcanMap); i++) {
        if (bsp_fdcanMap[i].handle == fdcan) {
            return bsp_fdcanMap[i].fdcan;
        }
    }
    return BSP_FDCAN_COUNT; // TODO: Change Status
}

#ifdef _USE_SEMAPHORE
static BSP_FDCANSemMap_t FindFDCANSemaphoreHandle(BSP_FDCAN_t fdcan)
{
	BSP_FDCANSemMap_t res = {BSP_FDCAN_COUNT, NULL, 0};

    for (uint8_t i = 0; i < ARRAY_SIZE(bsp_fdcanSemMap); i++) {
        if (bsp_fdcanSemMap[i].fdcan == fdcan) {
            return bsp_fdcanSemMap[i];
        }
    }
    return res;
}
#endif /* _USE_SEMAPHORE */

#endif /* HAL_FDCAN_MODULE_ENABLED */
