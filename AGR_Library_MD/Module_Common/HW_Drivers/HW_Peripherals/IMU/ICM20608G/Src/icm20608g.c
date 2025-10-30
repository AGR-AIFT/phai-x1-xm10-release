/**
 *-----------------------------------------------------------
 *       ICM20608G ACCELEROMETER AND GYROSCOPE DRIVER           
 *-----------------------------------------------------------
 * @file icm20608g.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Driver code for the ICM20608G accelerometer and gyroscope.
 *
 * This source file provides functionality to interface
 * with the ICM20608G accelerometer and gyroscope, including initialization,
 * data retrieval, and control register configurations.
 * 
 * Refer to the ICM20608G datasheet and related documents for more information.
 *
 * @ref ICM20608G Datasheet
 */

#include "icm20608g.h"

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

// static uint8_t ReadReg(ICM20608GObj_t* icm20608gObj, uint16_t reg, uint8_t* pData, uint32_t size);
static uint8_t WritReg(ICM20608GObj_t* icm20608gObj, uint16_t reg, uint8_t* pData, uint32_t size);
static uint8_t SetReg(ICM20608GObj_t* icm20608gObj, uint16_t reg, uint8_t* pData, uint32_t size);

static void CalValues(ICM20608GObj_t* icm20608gObj);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/**
 * @brief Set the I/O context for the ICM20608G object.
 * @param icm20608gObj Pointer to the ICM20608G object.
 * @param icm20608gIOctx Pointer to the I/O context structure.
 * @return ICM20608G status code.
 */
uint8_t ICM20608G_SetIoctx(ICM20608GObj_t* icm20608gObj, ICM20608GIOctx_t* icm20608gIOctx)
{
    if (!icm20608gObj || !icm20608gIOctx->IsDevReady || !icm20608gIOctx->ReadReg || !icm20608gIOctx->WriteReg) {
        return ICM20608G_STATUS_ERROR; // Return error if any pointer is NULL or essential functions are missing
    }

    // Assign IO context functions
    icm20608gObj->io.Init       = icm20608gIOctx->Init;
    icm20608gObj->io.DeInit     = icm20608gIOctx->DeInit;
    icm20608gObj->io.IsDevReady = icm20608gIOctx->IsDevReady;
    icm20608gObj->io.ReadReg    = icm20608gIOctx->ReadReg;
    icm20608gObj->io.WriteReg   = icm20608gIOctx->WriteReg;
    icm20608gObj->io.Wait       = icm20608gIOctx->Wait;

    return ICM20608G_STATUS_OK;
}

/**
 * @brief Initialize the ICM20608G object by setting control registers.
 * @param icm20608gObj Pointer to the ICM20608G object.
 * @return ICM20608G status code.
 */
uint8_t ICM20608G_Init(ICM20608GObj_t* icm20608gObj)
{
    if (!icm20608gObj) {
        return ICM20608G_STATUS_ERROR; // Return error if any pointer is NULL or essential functions are missing
    }

    uint8_t status = ICM20608G_STATUS_ERROR;
    icm20608gObj->devAddr = ICM20608G_DEV_ADDR_HIGH;

    // Check device readiness
    if (icm20608gObj->io.IsDevReady(icm20608gObj->devAddr) == 0){
        status = ICM20608G_STATUS_OK;
    }

    // Configure control registers if the device is ready
    if (status == ICM20608G_STATUS_OK){
        uint8_t conf_1 = ICM20608G_PWR_MGMT_1_DATA;
        status = SetReg(icm20608gObj, ICM20608G_PWR_MGMT_1, &conf_1, ICM20608G_CONTROL_SIZE);
        uint8_t conf_2 = ICM20608G_PWR_MGMT_2_DATA;
        status = SetReg(icm20608gObj, ICM20608G_PWR_MGMT_2, &conf_2, ICM20608G_CONTROL_SIZE);
        uint8_t conf_3 = ICM20608G_GYR_CONFIG_500dps;
        status = SetReg(icm20608gObj, ICM20608G_GYRO_CONFIG, &conf_3, ICM20608G_CONTROL_SIZE);
        uint8_t conf_4 = ICM20608G_ACC_CONFIG_4g;
        status = SetReg(icm20608gObj, ICM20608G_ACCEL_CONFIG, &conf_4, ICM20608G_CONTROL_SIZE);
    }

    return status;
}

/**
 * @brief Retrieves the raw data from the ICM20608G sensor and converts it into scaled acceleration and gyroscope values.
 *
 * This function reads the raw data bytes from the ICM20608G sensor using the provided ReadReg function pointer.
 * The raw data is then passed to the CalValues function to be converted into scaled values for acceleration
 * and gyroscope, which are assigned to the corresponding members of the ICM20608G object.
 *
 * @param icm20608gObj Pointer to the ICM20608G object containing the configuration, IO context, and data structure.
 * @return ICM20608G_STATUS_OK if the operation is successful; ICM20608G_STATUS_ERROR otherwise.
 */
uint8_t ICM20608G_GetValue(ICM20608GObj_t* icm20608gObj)
{
    // Check for NULL pointer
    if (!icm20608gObj) {
        return ICM20608G_STATUS_ERROR; // Return error if any pointer is NULL or essential functions are missing
    }

    // Read the raw data from the sensor
    uint8_t status = icm20608gObj->io.ReadReg(icm20608gObj->devAddr, ICM20608G_ACCEL_XOUT_H, icm20608gObj->dataBuff, ICM20608G_READ_DATA_SIZE);

    // If the read operation was successful, scale and assign the data
    if (status == ICM20608G_STATUS_OK) {
        CalValues(icm20608gObj);
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
 * @brief Read data from the specified register of the ICM20608G device.
 * @param icm20608gObj Pointer to the ICM20608G object.
 * @param reg Register devAddress to read.
 * @param data Pointer to store the read data.
 * @param size Size of the data to be read.
 * @return ICM20608G status code.
 */
// static uint8_t ReadReg(ICM20608GObj_t* icm20608gObj, uint16_t reg, uint8_t* pData, uint32_t size)
// {
//    return icm20608gObj->io.ReadReg(icm20608gObj->devAddr, reg, pData, size);
// }

/**
 * @brief Write data to the specified register of the ICM20608G device.
 * @param icm20608gObj Pointer to the ICM20608G object.
 * @param reg Register devAddress to write.
 * @param data Pointer to the data to write.
 * @param size Size of the data to be written.
 * @return ICM20608G status code.
 */
static uint8_t WritReg(ICM20608GObj_t* icm20608gObj, uint16_t reg, uint8_t* pData, uint32_t size)
{
    return icm20608gObj->io.WriteReg(icm20608gObj->devAddr, reg, pData, size);
}

/**
 * @brief Set and verify the specified register of the ICM20608G device.
 * @param icm20608gObj Pointer to the ICM20608G object.
 * @param reg Register devAddress to set.
 * @param data Pointer to the data to set.
 * @param size Size of the data to be set.
 * @return ICM20608G status code.
 */
static uint8_t SetReg(ICM20608GObj_t* icm20608gObj, uint16_t reg, uint8_t* pData, uint32_t size)
{
	uint8_t status = ICM20608G_STATUS_OK;
//    uint8_t readVal[size];

    status = WritReg(icm20608gObj, reg, pData, size);

    // if (status == ICM20608G_STATUS_OK) {
    //     status = ReadReg(icm20608gObj, reg, readVal, size);
    // }

    // if (status == ICM20608G_STATUS_OK) {
    //     if (memcmp(pData, &readVal, size) != 0) {
    //         status = ICM20608G_STATUS_ERROR;
    //     }
    // }

    return status;
}

/**
 * @brief Scales the raw data from the ICM20608G sensor and assigns it to the appropriate structure members.
 * 
 * This function takes the raw data bytes from the ICM20608G sensor, converts them to 16-bit signed integers,
 * scales them according to the defined scale factors for acceleration and gyroscope, and assigns them
 * to the corresponding members of the ICM20608G object.
 * 
 * @param icm20608gObj Pointer to the ICM20608G object containing the data structure to be updated.
 * @param dataBuff Pointer to the buffer containing the raw data bytes from the sensor.
 */
static void CalValues(ICM20608GObj_t* icm20608gObj)
{
    // Array to hold the raw 16-bit values for each sensor reading
    int16_t rawValues[ICM20608G_TOTAL_SENSORS] = {0};

    // Convert the raw bytes to 16-bit signed integers
    for (int i = 0; i < ICM20608G_TOTAL_SENSORS; i++) {
        rawValues[i] = (int16_t)(icm20608gObj->dataBuff[i * 2] << 8 | icm20608gObj->dataBuff[i * 2 + 1]);
    }

    // Scale the raw values and assign them to the appropriate structure members
    icm20608gObj->IMUData.accX = (float)(rawValues[0] / ICM20608G_ACCEL_SCALE_FACTOR_4g);
    icm20608gObj->IMUData.accY = (float)(rawValues[1] / ICM20608G_ACCEL_SCALE_FACTOR_4g);
    icm20608gObj->IMUData.accZ = (float)(rawValues[2] / ICM20608G_ACCEL_SCALE_FACTOR_4g);
    icm20608gObj->IMUData.temp = (float)(rawValues[3] / ICM20608G_TEMP_SCALE_FACTOR) + ICM20608G_ROOM_TEMP_OFFSET;
    icm20608gObj->IMUData.gyrX = (float)(rawValues[4] / ICM20608G_GYRO_SCALE_FACTOR_500dps);
    icm20608gObj->IMUData.gyrY = (float)(rawValues[5] / ICM20608G_GYRO_SCALE_FACTOR_500dps);
    icm20608gObj->IMUData.gyrZ = (float)(rawValues[6] / ICM20608G_GYRO_SCALE_FACTOR_500dps);
}
