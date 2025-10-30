/**
 *-----------------------------------------------------------
 *                BM1422AGMV MAGNETOMETER DRIVER
 *-----------------------------------------------------------
 * @file bm1422agmv.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Driver code for the BM1422AGMV magnetometer.
 * @reference BM1422AGMV Datasheet
 *
 * The following source file provides functionality to interface
 * with the BM1422AGMV magnetometer, including initialization,
 * data retrieval, and control register configurations.
 * 
 * Refer to the BM1422AGMV datasheet and related documents for more information.
 *
 * @ref BM1422AGMV Datasheet
 */

#include "bm1422agmv.h"

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





/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

// static uint8_t ReadReg(BM1422AGMVObj_t* bm1422agmvObj, uint16_t reg, uint8_t* pData, uint32_t size);
static uint8_t WriteReg(BM1422AGMVObj_t* bm1422agmvObj, uint16_t reg, uint8_t* pData, uint32_t size);
static uint8_t SetReg(BM1422AGMVObj_t* bm1422agmvObj, uint16_t reg, uint8_t* pData, uint32_t size);

static void CalValues(BM1422AGMVObj_t* bm1422agmvObj, uint8_t* dataBuff);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/**
 * @brief Set the I/O context for the BM1422AGMV object.
 * @param bm1422agmvObj Pointer to the BM1422AGMV object.
 * @param bm1422agmvIOctx Pointer to the I/O context structure.
 * @return BM1422AGMV status code.
 */
uint8_t BM1422AGMV_SetIoctx(BM1422AGMVObj_t* bm1422agmvObj, BM1422AGMVIOctx_t* bm1422agmvIOctx)
{
    // Check for valid pointers and functions
    if (!bm1422agmvObj || !bm1422agmvIOctx->IsDevReady || !bm1422agmvIOctx->ReadReg || !bm1422agmvIOctx->WriteReg) {
        return BM1422AGMV_STATUS_ERROR;
    }

    // Assign IO context functions
    bm1422agmvObj->io.Init       = bm1422agmvIOctx->Init;
    bm1422agmvObj->io.DeInit     = bm1422agmvIOctx->DeInit;
    bm1422agmvObj->io.IsDevReady = bm1422agmvIOctx->IsDevReady;
    bm1422agmvObj->io.ReadReg    = bm1422agmvIOctx->ReadReg;
    bm1422agmvObj->io.WriteReg   = bm1422agmvIOctx->WriteReg;
    bm1422agmvObj->io.Wait       = bm1422agmvIOctx->Wait;

    return BM1422AGMV_STATUS_OK;
}

/**
 * @brief Initialize the BM1422AGMV object.
 * @param bm1422agmvObj Pointer to the BM1422AGMV object.
 * @return BM1422AGMV status code.
 */
uint8_t BM1422AGMV_Init(BM1422AGMVObj_t* bm1422agmvObj)		// Setting control register
{
    // Check for valid pointers and functions
    if (!bm1422agmvObj) {
        return BM1422AGMV_STATUS_ERROR;
    }

    uint8_t status = BM1422AGMV_STATUS_ERROR;
    bm1422agmvObj->devAddr = BM1422AGMV_DEV_ADDR;

    // Check device readiness
    if (bm1422agmvObj->io.IsDevReady(bm1422agmvObj->devAddr) == 0){
        status = BM1422AGMV_STATUS_OK;
    }

    // Configure control registers if the device is ready
    if (status == BM1422AGMV_STATUS_OK) {
    	uint8_t conf_1 = BM1422AGMV_CNTL1_VALUE;
        status = SetReg(bm1422agmvObj, BM1422AGMV_CNTL1_REG, &conf_1, BM1422AGMV_CONTROL_SIZE);
        uint8_t conf_2 = BM1422AGMV_CNTL2_VALUE;
        status = SetReg(bm1422agmvObj, BM1422AGMV_CNTL2_REG, &conf_2, BM1422AGMV_CONTROL_SIZE);
        uint8_t conf_4 = BM1422AGMV_CNTL4_H_VALUE;
        status = SetReg(bm1422agmvObj, BM1422AGMV_CNTL4_REG_H, &conf_4, BM1422AGMV_CONTROL_SIZE);
        uint8_t conf_5 = BM1422AGMV_CNTL4_L_VALUE;
        status = SetReg(bm1422agmvObj, BM1422AGMV_CNTL4_REG_L, &conf_5, BM1422AGMV_CONTROL_SIZE);
        // CNTL3 should be set at LAST //
        uint8_t conf_3 = BM1422AGMV_CNTL3_VALUE;
        status = SetReg(bm1422agmvObj, BM1422AGMV_CNTL3_REG, &conf_3, BM1422AGMV_CONTROL_SIZE);
    }

    return status;
}

/**
 * @brief Retrieve the magnet field values from the BM1422AGMV object.
 * @param bm1422agmvObj Pointer to the BM1422AGMV object.
 * @param bm1422agmv_data Pointer to store the retrieved data.
 * @return BM1422AGMV status code.
 */
uint8_t BM1422AGMV_GetValue(BM1422AGMVObj_t* bm1422agmvObj)
{
    // Check for valid data pointer
	if (!bm1422agmvObj) {
        return BM1422AGMV_STATUS_ERROR;
    }

    // Read the raw data from the sensor
    uint8_t status = bm1422agmvObj->io.ReadReg(bm1422agmvObj->devAddr, BM1422AGMV_DATAX_REG, bm1422agmvObj->dataBuff, BM1422AGMV_READ_DATA_SIZE);

    // If the read operation was successful, scale and assign the data
    if (status == BM1422AGMV_STATUS_OK) {
        CalValues(bm1422agmvObj, bm1422agmvObj->dataBuff);
    }

	return status;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Read the specified register of the BM1422AGMV device.
 * @param bm1422agmvObj Pointer to the BM1422AGMV object.
 * @param reg Register address to read.
 * @param data Pointer to store the read data.
 * @param size Size of the data to be read.
 * @return BM1422AGMV status code.
 */
// static uint8_t ReadReg(BM1422AGMVObj_t* bm1422agmvObj, uint16_t reg, uint8_t* pData, uint32_t size)
// {
//     return bm1422agmvObj->io.ReadReg(bm1422agmvObj->devAddr, reg, pData, size);
// }

/**
 * @brief Write data to the specified register of the BM1422AGMV device.
 * @param bm1422agmvObj Pointer to the BM1422AGMV object.
 * @param reg Register address to write.
 * @param data Pointer to the data to write.
 * @param size Size of the data to be written.
 * @return BM1422AGMV status code.
 */
static uint8_t WriteReg(BM1422AGMVObj_t* bm1422agmvObj, uint16_t reg, uint8_t* pData, uint32_t size)
{
    return bm1422agmvObj->io.WriteReg(bm1422agmvObj->devAddr, reg, pData, size);
}

/**
 * @brief Set and verify the specified register of the BM1422AGMV device.
 * @param bm1422agmvObj Pointer to the BM1422AGMV object.
 * @param reg Register address to set.
 * @param data Pointer to the data to set.
 * @param size Size of the data to be set.
 * @return BM1422AGMV status code.
 */
static uint8_t SetReg(BM1422AGMVObj_t* bm1422agmvObj, uint16_t reg, uint8_t* pData, uint32_t size)
{
    uint8_t status = BM1422AGMV_STATUS_OK;
//    uint8_t readVal[size];

    status = WriteReg(bm1422agmvObj, reg, pData, size);

    // if (status == BM1422AGMV_STATUS_OK) {
    //     status = ReadReg(bm1422agmvObj, reg, readVal, size);
    // }

    // if (status == BM1422AGMV_STATUS_OK) {
    //     if (memcmp(pData, &readVal, size) != 0) {
    //         status = BM1422AGMV_STATUS_ERROR;
    //     }
    // }

    return status;
}

/**
 * @brief Scales the raw data from the BM1422AGMV sensor and assigns it to the appropriate structure members.
 * 
 * This function takes the raw data bytes from the BM1422AGMV sensor, converts them to 16-bit signed integers,
 * scales them according to the defined scale factors for acceleration and gyroscope, and assigns them
 * to the corresponding members of the BM1422AGMV object.
 * 
 * @param bm1422agmvObj Pointer to the BM1422AGMV object containing the data structure to be updated.
 * @param dataBuff Pointer to the buffer containing the raw data bytes from the sensor.
 */
static void CalValues(BM1422AGMVObj_t* bm1422agmvObj, uint8_t* dataBuff)
{
    // Array to hold the raw 16-bit values for each sensor reading
    int16_t rawValues[BM1422AGMV_TOTAL_SENSORS] = {0};

    // Convert the raw bytes to 16-bit signed integers
    for (int i = 0; i < BM1422AGMV_TOTAL_SENSORS; i++) {
        rawValues[i] = (int16_t)(dataBuff[i * 2 + 1] << 8 | dataBuff[i * 2]);
    }

    // Scale the raw values and assign them to the appropriate structure members
    bm1422agmvObj->magData.magX = (float)rawValues[0] * BM1422AGMV_SCALE_FACTOR;
    bm1422agmvObj->magData.magY = (float)rawValues[1] * BM1422AGMV_SCALE_FACTOR;
    bm1422agmvObj->magData.magZ = (float)rawValues[2] * BM1422AGMV_SCALE_FACTOR;
}
