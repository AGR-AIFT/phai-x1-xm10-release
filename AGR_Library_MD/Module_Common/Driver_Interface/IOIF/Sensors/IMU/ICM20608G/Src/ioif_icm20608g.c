/*
 * ioif_icm20608g.c
 *
 *  Created on: Jul 28, 2023
 *      Author: INVINCIBLE_1NE
 */


#include "ioif_icm20608g.h"

/** @defgroup I2C I2C
  * @brief I2C ICM20608G module driver
  * @{
  */
#ifdef IOIF_ICM20608G_ENABLED

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
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c3RxBuff"))) = {0};
#endif /* L30_CM_ENABLED */

#ifdef L30_CM_ENABLED
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c3RxBuff"))) = {0};
#endif /* L30_CM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c3RxBuff"))) = {0};
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c3RxBuff"))) = {0};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c3RxBuff"))) = {0};
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MINICM_ENABLED
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};
#endif /* SUIT_MINICM_ENABLED */

#ifdef SUIT_MD_ENABLED
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c2RxBuff"))) = {0};
#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED
// TODO : Port Change!! U5!!
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};
#endif /* SUIT_WIDM_ENABLED */

#ifdef KAIST_GRF_BOARD_ENABLED
// TODO : Port Change!! U5!!
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c2RxBuff"))) = {0};
#endif /* KAIST_GRF_BOARD_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
// TODO : Port Change!! U5!!
static uint8_t icmDmaRxBuff[IOIF_ICM20608G_BUFF_SIZE] __attribute__((section(".i2c2RxBuff"))) = {0};
#endif /* EXPANSION_BOARD_ENABLED */


static ICM20608GObj_t icm20608gObj;
static ICM20608GIOctx_t icm20608gIOctx;
static IOIF_I2C_t i2cHandle;


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static uint8_t IsDevReady(uint16_t devAddr);
static uint8_t ReadReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size);
static uint8_t WriteReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/**
 * @brief Initialize the 6-axis IMU sensor.
 * @param i2c Enumeration representing the I2C channel to use.
 * @return Status of the initialization operation.
 */
IOIF_6AxisState_t IOIF_Init6Axis(IOIF_I2C_t i2c)
{
    icm20608gIOctx.IsDevReady = IsDevReady;
    icm20608gIOctx.ReadReg = ReadReg;
    icm20608gIOctx.WriteReg = WriteReg;
    
    i2cHandle = i2c;
    // for DMA Read
    icm20608gObj.dataBuff = icmDmaRxBuff;

    uint8_t status = ICM20608G_SetIoctx(&icm20608gObj, &icm20608gIOctx);
    if (status != ICM20608G_STATUS_OK) {
        return status; // Error handling
    }

    // Initialize the ICM20608G object
    // You should have a function for initialization
	status = ICM20608G_Init(&icm20608gObj);
    if (status != ICM20608G_STATUS_OK) {
        return status; // Error handling
    }

    return status;
}

/**
 * @brief Retrieve the current values from the 6-axis IMU sensor.
 * @param imuData Pointer to a structure to store the retrieved data.
 * @return Status of the data retrieval operation.
 */
IOIF_6AxisState_t IOIF_Get6AxisValue(IOIF_6AxisData_t* imuData)
{
    // Check for NULL pointer
    if (imuData == NULL) {
        return IOIF_IMU6AXIS_STATUS_ERROR;
    }

    // Initialize acceleration and gyroscope data
	memset(&icm20608gObj.IMUData, 0, sizeof(icm20608gObj.IMUData));

    // Get the value from the hardware object and check for errors
    uint8_t status = ICM20608G_GetValue(&icm20608gObj);
    if (status != IOIF_IMU6AXIS_STATUS_OK) {
        return status;
    }

    memcpy(imuData, &icm20608gObj.IMUData, sizeof(icm20608gObj.IMUData));

    return status;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

// Checks if the IMU 6-Axis device is ready
static uint8_t IsDevReady(uint16_t devAddr)
{
    return BSP_IsDevReadyI2C((BSP_I2C_t)i2cHandle, devAddr, IOIF_ICM20608G_TRIALS, IOIF_ICM20608G_TIMEOUT);
}

// Reads a register from the IMU 6-Axis device using DMA
static uint8_t ReadReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size)
{
    return BSP_I2CMemDMA((BSP_I2C_t)i2cHandle, devAddr, regAddr, BSP_I2C_MEMADD_SIZE_8BIT, pData, size, BSP_I2C_MEM_READ_DMA);
}

// Writes to a register on the IMU 6-Axis device using blocking method
static uint8_t WriteReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size)
{
	return BSP_I2CMemBlock((BSP_I2C_t)i2cHandle, devAddr, regAddr, BSP_I2C_MEMADD_SIZE_8BIT, pData, size, IOIF_ICM20608G_TIMEOUT, BSP_I2C_MEM_WRITE);
}


#endif /* IOIF_ICM20608G_ENABLED */
