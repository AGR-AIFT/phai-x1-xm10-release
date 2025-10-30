/*
 * ioif_bm1422agmv.c
 *
 *  Created on: Jul 28, 2023
 *      Author: INVINCIBLE_1NE
 */


#include "ioif_bm1422agmv.h"

/** @defgroup I2C I2C
  * @brief I2C BM1422AGMV module driver
  * @{
  */
#ifdef IOIF_BM1422AGMV_ENABLED

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
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c2RxBuff"))) = {0};
#endif /* L30_CM_ENABLED */

#ifdef L30_CM_ENABLED
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c2RxBuff"))) = {0};
#endif /* L30_CM_ENABLED */

#ifdef L30_MD_REV06_ENABLED
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0}; // not in use
#endif /* L30_MD_REV06_ENABLED */

#ifdef L30_MD_REV07_ENABLED
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};
#endif /* L30_MD_REV07_ENABLED */

#ifdef L30_MD_REV08_ENABLED
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};
#endif /* L30_MD_REV08_ENABLED */

#ifdef SUIT_MINICM_ENABLED
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c2RxBuff"))) = {0};
#endif /* SUIT_MINICM_ENABLED */

#ifdef SUIT_MD_ENABLED
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};
#endif /* SUIT_MD_ENABLED */

#ifdef WIDM_ENABLED
// TODO : Port Change!! U5!!
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c3RxBuff"))) = {0};
#endif /* SUIT_WIDM_ENABLED */

#ifdef KAIST_GRF_BOARD_ENABLED
// TODO : Port Change!! U5!!
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};
#endif /* KAIST_GRF_BOARD_ENABLED */

#ifdef EXPANSION_BOARD_ENABLED
// TODO : Port Change!! U5!!
static uint8_t bmDmaRxBuff[IOIF_BM1422AGMV_BUFF_SIZE] __attribute__((section(".i2c1RxBuff"))) = {0};
#endif /* EXPANSION_BOARD_ENABLED */

static BM1422AGMVObj_t bm1422agmvObj;
static BM1422AGMVIOctx_t bm1422agmvIOctx;
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
uint8_t IOIF_InitMag(IOIF_I2C_t i2c)
{
    bm1422agmvIOctx.IsDevReady = IsDevReady;
    bm1422agmvIOctx.ReadReg = ReadReg;
    bm1422agmvIOctx.WriteReg = WriteReg;

    i2cHandle = i2c;
    // for DMA Read
    bm1422agmvObj.dataBuff = bmDmaRxBuff;

    uint8_t status = BM1422AGMV_SetIoctx(&bm1422agmvObj, &bm1422agmvIOctx);
    if (status != BM1422AGMV_STATUS_OK) {
        return status; // Error handling
    }

    // Initialize the BM1422AGMV object
    // You should have a function for initialization
    status = BM1422AGMV_Init(&bm1422agmvObj);
    if (status != BM1422AGMV_STATUS_OK) {
        return status; // Error handling
    }

    return status;
}

/**
* @brief Retrieve the current values from the 6-axis IMU sensor.
* @param imuData Pointer to a structure to store the retrieved data.
* @return Status of the data retrieval operation.
*/
uint8_t IOIF_GetMagValue(IOIF_MagData_t* magData)
{
    // Check for NULL pointer
    if (magData == NULL) {
        return IOIF_IMU3AXIS_STATUS_ERROR;
    }

    // Initialize acceleration and gyroscope data
    memset(&bm1422agmvObj.magData, 0, sizeof(bm1422agmvObj.magData));

    // Get the value from the hardware object and check for errors
    uint8_t status = BM1422AGMV_GetValue(&bm1422agmvObj);
    if (status != IOIF_IMU3AXIS_STATUS_OK) {
        return status;
    }

    memcpy(magData, &bm1422agmvObj.magData, sizeof(bm1422agmvObj.magData));

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
    return BSP_IsDevReadyI2C((BSP_I2C_t)i2cHandle, devAddr, IOIF_BM1422AGMV_TRIALS, IOIF_BM1422AGMV_TIMEOUT);
}

// Reads a register from the IMU 6-Axis device using DMA
static uint8_t ReadReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size)
{
    return BSP_I2CMemDMA((BSP_I2C_t)i2cHandle, devAddr, regAddr, BSP_I2C_MEMADD_SIZE_8BIT, pData, size, BSP_I2C_MEM_READ_DMA);
}

// Writes to a register on the IMU 6-Axis device using blocking method
static uint8_t WriteReg(uint16_t devAddr, uint16_t regAddr, uint8_t* pData, uint16_t size)
{
    return BSP_I2CMemBlock((BSP_I2C_t)i2cHandle, devAddr, regAddr, BSP_I2C_MEMADD_SIZE_8BIT, pData, size, IOIF_BM1422AGMV_TIMEOUT, BSP_I2C_MEM_WRITE);
}


#endif /* IOIF_BM1422AGMV_ENABLED */
