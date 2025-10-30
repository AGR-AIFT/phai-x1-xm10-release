/**
 * @file bsp_i2c.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for I2C functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#include "bsp_i2c.h"

/** @defgroup I2C I2C
  * @brief I2C HAL BSP module driver
  * @{
  */
#ifdef HAL_I2C_MODULE_ENABLED

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
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, &hi2c2},       // I2C 2
    {BSP_I2C3, &hi2c3},       // I2C 3
    {BSP_I2C4, &hi2c4},       // I2C 4
};

#ifdef _USE_SEMAPHORE
static BSP_I2CSemMap_t bsp_i2cSemMap[BSP_I2C_COUNT]  = {
		{BSP_I2C_COUNT, 0, 0},
		{BSP_I2C1, &BinSem_I2C1Handle, BinSemI2C1_TIMEOUT},
		{BSP_I2C2, &BinSem_I2C2Handle, BinSemI2C2_TIMEOUT},
		{BSP_I2C3, &BinSem_I2C3Handle, BinSemI2C3_TIMEOUT},
		{BSP_I2C4, &BinSem_I2C4Handle, BinSemI2C4_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* WALKON5_CM_ENABLED */

#ifdef L30_CM_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, &hi2c2},       // I2C 2
    {BSP_I2C3, &hi2c3},       // I2C 3
    {BSP_I2C4, &hi2c4},       // I2C 4
};

#ifdef _USE_SEMAPHORE
static BSP_I2CSemMap_t bsp_i2cSemMap[BSP_I2C_COUNT]  = {
		{BSP_I2C_COUNT, 0, 0},
		{BSP_I2C1, &BinSem_I2C1Handle, BinSemI2C1_TIMEOUT},
		{BSP_I2C2, &BinSem_I2C2Handle, BinSemI2C2_TIMEOUT},
		{BSP_I2C3, &BinSem_I2C3Handle, BinSemI2C3_TIMEOUT},
		{BSP_I2C4, &BinSem_I2C4Handle, BinSemI2C4_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* L30_CM_ENABLED */

#ifdef SUIT_MINICM_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, &hi2c2},       // I2C 2
    {BSP_I2C3, NULL},         // I2C not defined
    {BSP_I2C4, &hi2c4},       // I2C 4
};

#ifdef _USE_SEMAPHORE
static BSP_I2CSemMap_t bsp_i2cSemMap[BSP_I2C_COUNT]  = {
		{BSP_I2C_COUNT, 0, 0},
		{BSP_I2C1, &BinSem_I2C1Handle, BinSemI2C1_TIMEOUT},
		{BSP_I2C2, &BinSem_I2C2Handle, BinSemI2C2_TIMEOUT},
		{BSP_I2C3, &BinSem_I2C3Handle, BinSemI2C3_TIMEOUT},
		{BSP_I2C4, &BinSem_I2C4Handle, BinSemI2C4_TIMEOUT},
};
#endif /* _USE_SEMAPHORE */
#endif /* SUIT_MINICM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, &hi2c2},       // I2C 2
    {BSP_I2C3, NULL},         // I2C not defined
    {BSP_I2C4, NULL},         // I2C not defined
};
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, &hi2c2},       // I2C 2
    {BSP_I2C3, &hi2c3},       // I2C 3
    {BSP_I2C4, NULL},         // I2C not defined
};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, &hi2c2},       // I2C 2
    {BSP_I2C3, &hi2c3},       // I2C 3
    {BSP_I2C4, NULL},         // I2C not defined
};
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MD_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, &hi2c2},       // I2C 2
    {BSP_I2C3, NULL},         // I2C not defined
    {BSP_I2C4, NULL},         // I2C not defined
};
#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, NULL},         // I2C not defined
    {BSP_I2C3, &hi2c3},       // I2C 3
    {BSP_I2C4, NULL},         // I2C not defined
};
#endif /* WIDM_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
static BSP_I2CMap_t bsp_i2cMap[BSP_I2C_COUNT] = {
    {BSP_I2C_COUNT, NULL},    // Dummy entry for index 0
    {BSP_I2C1, &hi2c1},       // I2C 1
    {BSP_I2C2, NULL},         // I2C not defined
    {BSP_I2C3, NULL},         // I2C not defined
    {BSP_I2C4, NULL},         // I2C not defined
};
#endif /* EXPANSION_BOARD_BOARD_ENABLED */

static BSP_I2CCB_t bsp_i2cCB[BSP_I2C_COUNT] = {0};


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static I2C_HandleTypeDef* FindI2CHandle(BSP_I2C_t i2c);
static BSP_I2C_t FindI2CEnum(I2C_HandleTypeDef* hi2c);
static void ExecuteI2CCB(I2C_HandleTypeDef* hi2c, BSP_I2CCBType_t callbackType);
#ifdef _USE_SEMAPHORE
static BSP_I2CSemMap_t FindI2CSemaphoreHandle(BSP_I2C_t i2c);
#endif /* _USE_SEMAPHORE */

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- INITIALIZATION & DE-INITIALIZATION ------------------- */
/**
 * @brief Initialize the I2C peripheral.
 * @param i2c Identifier for the I2C peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_InitI2C(BSP_I2C_t i2c) 
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    return (BSP_StatusTypeDef_t)HAL_I2C_Init(hi2c);
}

/**
 * @brief Deinitialize the I2C peripheral.
 * @param i2c Identifier for the I2C peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_DeInitI2C(BSP_I2C_t i2c) 
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    return (BSP_StatusTypeDef_t)HAL_I2C_DeInit(hi2c);
}

/* ------------------- I2C DEVICE CHECK ------------------- */
/**
 * @brief Check if the I2C device is ready.
 * @param i2c Identifier for the I2C peripheral.
 * @param devAddress The device address on the I2C bus.
 * @param trials The number of trials to check if the device is ready.
 * @param timeout The timeout duration for the operation. in millisecond.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_IsDevReadyI2C(BSP_I2C_t i2c, uint16_t devAddress, uint32_t trials, uint32_t timeout)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    return (BSP_StatusTypeDef_t)HAL_I2C_IsDeviceReady(hi2c, devAddress, trials, timeout);
}

/**
 * @brief Retrieve the current I2C state.
 * @param i2c Identifier for the I2C peripheral.
 * @return HAL status.
 */
BSP_I2CStateTypeDef_t BSP_GetStateI2C(BSP_I2C_t i2c)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    return (BSP_I2CStateTypeDef_t)HAL_I2C_GetState(hi2c);
}


/**
 * @brief Retrieve the current I2C mode.
 * @param i2c Identifier for the I2C peripheral.
 * @return HAL status.
 */
BSP_I2CModeTypeDef_t BSP_GetModeI2C(BSP_I2C_t i2c)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    return (BSP_I2CModeTypeDef_t)HAL_I2C_GetMode(hi2c);
}

/**
 * @brief Retrieve the current I2C error state.
 * @param i2c Identifier for the I2C peripheral.
 * @return HAL status.
 */
uint32_t BSP_GetErrorI2C(BSP_I2C_t i2c)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    return HAL_I2C_GetError(hi2c);
}

/* ------------------- BLOCKING MODE ------------------- */
/**
 * @brief Handle master-side I2C operations in blocking mode.
 * @param i2c Identifier for the I2C peripheral.
 * @param devAddress The device address on the I2C bus.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param timeout The timeout duration for the operation. in millisecond.
 * @param operation The I2C operation to be performed (transmit or receive).
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CMasterBlock(BSP_I2C_t i2c, uint16_t devAddress, uint8_t* pData, uint16_t size, uint32_t timeout, BSP_I2COP_t operation)
{
     I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_I2C_MASTER_TRANSMIT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Transmit(hi2c, devAddress, pData, size, timeout);
        case BSP_I2C_MASTER_RECEIVE:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Receive(hi2c, devAddress, pData, size, timeout);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief Handle slave-side I2C operations in blocking mode.
 * @param i2c Identifier for the I2C peripheral.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param timeout The timeout duration for the operation. in millisecond.
 * @param operation The I2C operation to be performed (transmit or receive).
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CSlaveBlock(BSP_I2C_t i2c, uint8_t* pData, uint16_t size, uint32_t timeout, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_I2C_SLAVE_TRANSMIT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Transmit(hi2c, pData, size, timeout);
            break;
        case BSP_I2C_SLAVE_RECEIVE:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Receive(hi2c, pData, size, timeout);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/**
 * @brief Handle memory-based I2C operations in blocking mode.
 * @param i2c        Identifier for the I2C peripheral.
 * @param devAddress Device address for the I2C peripheral.
 * @param memAddress Memory address to perform operation on.
 * @param memAddSize size of the memory address.
 * @param pData      Pointer to data to be written/read.
 * @param size       size of the data to be written/read.
 * @param timeout    timeout duration for the operation. in millisecond.
 * @param operation  Type of operation (read/write).
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CMemBlock(BSP_I2C_t i2c, uint16_t devAddress, uint16_t memAddress, uint16_t memAddSize, uint8_t* pData, uint16_t size, uint32_t timeout, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
    switch (operation) {
        case BSP_I2C_MEM_WRITE:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Write(hi2c, devAddress, memAddress, memAddSize, pData, size, timeout);
            break;
        case BSP_I2C_MEM_READ:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Read(hi2c, devAddress, memAddress, memAddSize, pData, size, timeout);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }

    return status;
}

/* ------------------- NON-BLOCKING MODE WITH IT ------------------- */

/**
 * @brief Initializes the I2C in Master mode using interrupt.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param devAddress Target device address.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation I2C operation type (transmit/receive).
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CMasterIT(BSP_I2C_t i2c, uint16_t devAddress, uint8_t* pData, uint16_t size, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;

#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_MASTER_TRANSMIT_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Transmit_IT(hi2c, devAddress, pData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_MASTER_RECEIVE_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Receive_IT(hi2c, devAddress, pData, size);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else

    switch (operation) {
        case BSP_I2C_MASTER_TRANSMIT_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Transmit_IT(hi2c, devAddress, pData, size);
            break;
        case BSP_I2C_MASTER_RECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Receive_IT(hi2c, devAddress, pData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Initializes the I2C in Master mode using interrupt for sequential transfer.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param devAddress Target device address.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param xferOptions Transfer options.
 * @param operation I2C operation type (sequential transmit/receive).
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CMasterSeqIT(BSP_I2C_t i2c, uint16_t devAddress, uint8_t* pData, uint16_t size, uint32_t xferOptions, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_MASTER_SEQ_TRANSMIT_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Transmit_IT(hi2c, devAddress, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_MASTER_SEQ_RECEIVE_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Receive_IT(hi2c, devAddress, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_MASTER_SEQ_TRANSMIT_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Transmit_IT(hi2c, devAddress, pData, size, xferOptions);
            break;
        case BSP_I2C_MASTER_SEQ_RECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Receive_IT(hi2c, devAddress, pData, size, xferOptions);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Aborts a master I2C process communication in interrupt mode. 
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param devAddress Target device address.
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_AbortI2CMasterIT(BSP_I2C_t i2c, uint16_t devAddress)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    return (BSP_StatusTypeDef_t)HAL_I2C_Master_Abort_IT(hi2c, devAddress);
}

/**
 * @brief Initializes the I2C in Slave mode using interrupt.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation I2C operation type (transmit/receive).
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CSlaveIT(BSP_I2C_t i2c, uint8_t* pData, uint16_t size, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_SLAVE_TRANSMIT_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Transmit_IT(hi2c, pData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_SLAVE_RECEIVE_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Receive_IT(hi2c, pData, size);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_SLAVE_TRANSMIT_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Transmit_IT(hi2c, pData, size);
            break;
        case BSP_I2C_SLAVE_RECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Receive_IT(hi2c, pData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Initializes the I2C in Slave mode using interrupt for sequential transfer.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param xferOptions Transfer options.
 * @param operation I2C operation type (sequential transmit/receive).
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CSlaveSeqIT(BSP_I2C_t i2c, uint8_t* pData, uint16_t size, uint32_t xferOptions, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;

#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_SLAVE_SEQ_TRANSMIT_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Transmit_IT(hi2c, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_SLAVE_SEQ_RECEIVE_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Receive_IT(hi2c, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_SLAVE_SEQ_TRANSMIT_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Transmit_IT(hi2c, pData, size, xferOptions);
            break;
        case BSP_I2C_SLAVE_SEQ_RECEIVE_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Receive_IT(hi2c, pData, size, xferOptions);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Initializes the I2C memory access using interrupt.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param devAddress Target device address.
 * @param memAddress Internal memory address.
 * @param memAddSize size of internal memory address.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation I2C operation type (read/write).
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CMemIT(BSP_I2C_t i2c, uint16_t devAddress, uint16_t memAddress, uint16_t memAddSize, uint8_t* pData, uint16_t size, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;

#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_MEM_WRITE_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Write_IT(hi2c, devAddress, memAddress, memAddSize, pData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_MEM_READ_IT:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Read_IT(hi2c, devAddress, memAddress, memAddSize, pData, size);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else

    switch (operation) {
        case BSP_I2C_MEM_WRITE_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Write_IT(hi2c, devAddress, memAddress, memAddSize, pData, size);
        case BSP_I2C_MEM_READ_IT:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Read_IT(hi2c, devAddress, memAddress, memAddSize, pData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/* ------------------- NON-BLOCKING MODE WITH DMA ------------------- */

/**
 * @brief  I2C Master operations in DMA mode
 * 
 * @param i2c I2C enumeration
 * @param devAddress Device address
 * @param pData Pointer to data buffer
 * @param size size of data to be transmitted/received
 * @param operation Operation type (Transmit/Receive)
 * ! dma_buff logic is only for Normal mode
 * @return HAL status
 */
BSP_StatusTypeDef_t BSP_I2CMasterDMA(BSP_I2C_t i2c, uint16_t devAddress, uint8_t* pData, uint16_t size, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_MASTER_TRANSMIT_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
            	status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Transmit_DMA(hi2c, devAddress, pData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_MASTER_RECEIVE_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Receive_DMA(hi2c, devAddress, pData, size);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_MASTER_TRANSMIT_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Transmit_DMA(hi2c, devAddress, pData, size);
            break;
        case BSP_I2C_MASTER_RECEIVE_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Receive_DMA(hi2c, devAddress, pData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief  I2C Master operations in DMA mode for sequential transfer.
 * 
 * @param i2c I2C enumeration
 * @param devAddress Device address
 * @param pData Pointer to data buffer
 * @param size size of data to be transmitted/received
 * @param xferOptions Transfer options.
 * @param operation Operation type (Transmit/Receive)
 * ! dma_buff logic is only for Normal mode
 * @return HAL status
 */
BSP_StatusTypeDef_t BSP_I2CMasterSeqDMA(BSP_I2C_t i2c, uint16_t devAddress, uint8_t* pData, uint16_t size, uint32_t xferOptions, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_MASTER_SEQ_TRANSMIT_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Transmit_DMA(hi2c, devAddress, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_MASTER_SEQ_RECEIVE_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Receive_DMA(hi2c, devAddress, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_MASTER_SEQ_TRANSMIT_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Transmit_DMA(hi2c, devAddress, pData, size, xferOptions);
            break;
        case BSP_I2C_MASTER_SEQ_RECEIVE_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Master_Seq_Receive_DMA(hi2c, devAddress, pData, size, xferOptions);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Initializes the I2C in Slave mode using DMA.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation I2C operation type (transmit/receive).
 * ! dma_buff logic is only for Normal mode
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CSlaveDMA(BSP_I2C_t i2c, uint8_t* pData, uint16_t size, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_SLAVE_TRANSMIT_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Transmit_DMA(hi2c, pData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_SLAVE_RECEIVE_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Receive_DMA(hi2c, pData, size);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_SLAVE_TRANSMIT_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Transmit_DMA(hi2c, pData, size);
            break;
        case BSP_I2C_SLAVE_RECEIVE_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Receive_DMA(hi2c, pData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Initializes the I2C in Slave mode using DMA for sequential transfer.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param xferOptions Transfer options.
 * @param operation I2C operation type (sequential transmit/receive).
 * ! dma_buff logic is only for Normal mode
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CSlaveSeqDMA(BSP_I2C_t i2c, uint8_t* pData, uint16_t size, uint32_t xferOptions, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;

#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_SLAVE_SEQ_TRANSMIT_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Transmit_DMA(hi2c, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_SLAVE_SEQ_RECEIVE_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Receive_DMA(hi2c, pData, size, xferOptions);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_SLAVE_SEQ_TRANSMIT_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Transmit_DMA(hi2c, pData, size, xferOptions);
            break;
        case BSP_I2C_SLAVE_SEQ_RECEIVE_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Slave_Seq_Receive_DMA(hi2c, pData, size, xferOptions);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/**
 * @brief Initializes the I2C memory access using DMA.
 * 
 * @param i2c BSP_I2C_t type instance.
 * @param devAddress Target device address.
 * @param memAddress Internal memory address.
 * @param memAddSize size of internal memory address.
 * @param pData Pointer to data buffer.
 * @param size Amount of data to be sent/received.
 * @param operation I2C operation type (read/write).
 * ! dma_buff logic is only for Normal mode
 * @return BSP_StatusTypeDef_t HAL status.
 */
BSP_StatusTypeDef_t BSP_I2CMemDMA(BSP_I2C_t i2c, uint16_t devAddress, uint16_t memAddress, uint16_t memAddSize, uint8_t* pData, uint16_t size, BSP_I2COP_t operation)
{
    I2C_HandleTypeDef* hi2c = FindI2CHandle(i2c);

    BSP_StatusTypeDef_t status = BSP_OK;
#ifdef _USE_SEMAPHORE
    BSP_I2CSemMap_t BinSemI2C = FindI2CSemaphoreHandle(i2c);

    switch (operation) {
        case BSP_I2C_MEM_WRITE_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Write_DMA(hi2c, devAddress, memAddress, memAddSize, pData, size);
        	else
        		return BSP_ERROR;
            break;
        case BSP_I2C_MEM_READ_DMA:
        	if(osSemaphoreAcquire(*BinSemI2C.BinSemHdlr, BinSemI2C.timeout) == osOK)		//Semaphore Wait
        		status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Read_DMA(hi2c, devAddress, memAddress, memAddSize, pData, size);
        	else
        		return BSP_ERROR;
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#else
    switch (operation) {
        case BSP_I2C_MEM_WRITE_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Write_DMA(hi2c, devAddress, memAddress, memAddSize, pData, size);
            break;
        case BSP_I2C_MEM_READ_DMA:
            status = (BSP_StatusTypeDef_t)HAL_I2C_Mem_Read_DMA(hi2c, devAddress, memAddress, memAddSize, pData, size);
            break;
        default:
            return BSP_ERROR; // Invalid operation
    }
#endif /* _USE_SEMAPHORE */

    return status;
}

/* ------------------- I2C CALLBACKS ------------------- */
/**
 * @brief Registers a custom callback for a specified I2C port and callback type.
 *
 * @param i2c Enum value representing the I2C port.
 * @param callbackType Enum value representing the type of callback (e.g., transmit complete, receive complete).
 * @param callback Function pointer to the custom callback function.
 * @param params Pointer to any parameters that the custom callback function may need.
 */
void BSP_SetI2CCB(BSP_I2C_t i2c, BSP_I2CCBType_t callbackType, BSP_I2CCBPtr_t callback, void* params)
{
    if (i2c < BSP_I2C_COUNT && callbackType < BSP_I2C_CALLBACK_TYPE_COUNT && callback) {
        bsp_i2cCB[i2c].callbacks[callbackType] = callback;
        bsp_i2cCB[i2c].params[callbackType] = params;
    }
}

/**
 * @brief Callback executed when the Master tx completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_MASTER_TX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_I2C_t i2c = FindI2CEnum(hi2c);
    BSP_I2CSemMap_t i2cBinSem = FindI2CSemaphoreHandle(i2c);
    osSemaphoreRelease(*i2cBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief Callback executed when the Master rx completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_MASTER_RX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_I2C_t i2c = FindI2CEnum(hi2c);
    BSP_I2CSemMap_t i2cBinSem = FindI2CSemaphoreHandle(i2c);
    osSemaphoreRelease(*i2cBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief Callback executed when the Slave tx completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_SLAVE_TX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_I2C_t i2c = FindI2CEnum(hi2c);
    BSP_I2CSemMap_t i2cBinSem = FindI2CSemaphoreHandle(i2c);
    osSemaphoreRelease(*i2cBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief Callback executed when the Slave rx completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_SLAVE_RX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_I2C_t i2c = FindI2CEnum(hi2c);
    BSP_I2CSemMap_t i2cBinSem = FindI2CSemaphoreHandle(i2c);
    osSemaphoreRelease(*i2cBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief Callback executed when the Memory tx completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_MEM_TX_CPLT_CALLBACK);

#ifdef _USE_SEMAPHORE
    BSP_I2C_t i2c = FindI2CEnum(hi2c);
    BSP_I2CSemMap_t i2cBinSem = FindI2CSemaphoreHandle(i2c);
    osSemaphoreRelease(*i2cBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief Callback executed when the Memory rx completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_MEM_RX_CPLT_CALLBACK);
    
#ifdef _USE_SEMAPHORE
    BSP_I2C_t i2c = FindI2CEnum(hi2c);
    BSP_I2CSemMap_t i2cBinSem = FindI2CSemaphoreHandle(i2c);
    osSemaphoreRelease(*i2cBinSem.BinSemHdlr);
#endif /* _USE_SEMAPHORE */
}

/**
 * @brief Callback executed when the Listen completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_LISTEN_CPLT_CALLBACK);
}

/**
 * @brief Callback executed when an error occurs.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_ERROR_CALLBACK);
}

/**
 * @brief Callback executed when the Abort completed.
 * @param hi2c HAL i2c handle.
 */
void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef* hi2c)
{
    UNUSED(hi2c);

   	ExecuteI2CCB(hi2c, BSP_I2C_ABORT_CPLT_CALLBACK);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Executes a custom callback for a specified I2C port and callback type.
 *
 * @param hi2c HAL I2C handle, used to find the corresponding I2C port enum value.
 * @param callbackType Enum value representing the type of callback to execute.
 */
static void ExecuteI2CCB(I2C_HandleTypeDef* hi2c, BSP_I2CCBType_t callbackType)
{
    BSP_I2C_t i2c = FindI2CEnum(hi2c);
    if (i2c < BSP_I2C_COUNT) {
        BSP_I2CCBPtr_t callback = bsp_i2cCB[i2c].callbacks[callbackType];
        void* params = bsp_i2cCB[i2c].params[callbackType];
        if (callback) {
            callback(params); // Executes the custom callback with the specified parameters
        }
    }
}

/**
 * @brief Find the I2C handle corresponding to the specified I2C enumeration
 *
 * @param i2c I2C enumeration
 * @return Pointer to I2C handle or NULL if not found
 */
static I2C_HandleTypeDef* FindI2CHandle(BSP_I2C_t i2c)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_i2cMap); i++) {
        if (bsp_i2cMap[i].i2c == i2c) {
            return bsp_i2cMap[i].handle;
        }
    }
    return NULL;
}

static BSP_I2C_t FindI2CEnum(I2C_HandleTypeDef* hi2c)
{
    for (int i = 0; i < ARRAY_SIZE(bsp_i2cMap); i++) {
        if (bsp_i2cMap[i].handle == hi2c) {
            return bsp_i2cMap[i].i2c;
        }
    }
    return BSP_I2C_COUNT; // TODO: Change Status
}

#ifdef _USE_SEMAPHORE
static BSP_I2CSemMap_t FindI2CSemaphoreHandle(BSP_I2C_t i2c)
{
	BSP_I2CSemMap_t res = {BSP_I2C_COUNT, NULL, 0};

    for (uint8_t i = 0; i < ARRAY_SIZE(bsp_i2cSemMap); i++) {
        if (bsp_i2cSemMap[i].i2c == i2c) {
            return bsp_i2cSemMap[i];
        }
    }
    return res;
}
#endif /* _USE_SEMAPHORE */


#endif /* HAL_I2C_MODULE_ENABLED */
