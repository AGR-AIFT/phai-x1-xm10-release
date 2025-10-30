
/**
 *-----------------------------------------------------------
 *    ICM20608G ACCELEROMETER AND GYROSCOPE DRIVER HEADER          
 *-----------------------------------------------------------
 * @file ICM20608G.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Main header file for the ICM20608G IMU sensor.
 * 
 * This header file includes macros, type declarations, global variables,
 * and function prototypes necessary for interfacing and working with
 * the ICM20608G sensor.
 * 
 * Refer to the ICM20608G datasheet and related documents for more information.
 *
 * @ref ICM20608G Datasheet
 */

#ifndef ICM20608G_INC_ICM20608G_H_
#define ICM20608G_INC_ICM20608G_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "icm20608g_regmap.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define ICM20608G_DATA_BUFF_SIZE    32
#define ICM20608G_READ_DATA_SIZE    14

#define ICM20608G_TOTAL_SENSORS     7

#define ICM20608G_TRIALS            10
#define ICM20608G_STRAT_UP_DELAY    10
#define ICM20608G_TIMEOUT           1

#define ICM20608G_ACCEL_SCALE_FACTOR_2g			16384.0f	// 16384 LSB/g
#define ICM20608G_ACCEL_SCALE_FACTOR_4g    		8192.0f		// 8192  LSB/g
#define ICM20608G_ACCEL_SCALE_FACTOR_8g			4096.0f		// 4096	 LSB/g
#define ICM20608G_ACCEL_SCALE_FACTOR_16g		2048.0f		// 2048  LSB/g

#define ICM20608G_GYRO_SCALE_FACTOR_250dps		131.0f		// 131  LSB/dps
#define ICM20608G_GYRO_SCALE_FACTOR_500dps     	65.5f		// 65.5 LSB/dps
#define ICM20608G_GYRO_SCALE_FACTOR_1000dps		32.8f		// 32.8 LSB/dps
#define ICM20608G_GYRO_SCALE_FACTOR_2000dps		16.2		// 16.4 LSB/dps

#define ICM20608G_TEMP_SCALE_FACTOR     326.8f
#define ICM20608G_ROOM_TEMP_OFFSET      25.0f


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

// Function pointer types for initialization, de-initialization, readiness, etc.
typedef uint8_t (*ICM20608G_InitFunc)       (void);
typedef uint8_t (*ICM20608G_DeInitFunc)     (void);
typedef uint8_t (*ICM20608G_IsDevReadyFunc) (uint16_t);                               // DevAddr
typedef uint8_t (*ICM20608G_ReadRegFunc)    (uint16_t, uint16_t, uint8_t*, uint16_t); // DevAddr, RegAddr, Data, DataSize
typedef uint8_t (*ICM20608G_WriteRegFunc)   (uint16_t, uint16_t, uint8_t*, uint16_t); // DevAddr, RegAddr, Data, DataSize
typedef uint8_t (*ICM20608G_WaitFunc)       (uint32_t);			                      // ms to Wait

// Enumeration for ICM20608G status codes.
typedef enum _ICM20608GState_t {
    ICM20608G_STATUS_OK = 0,
    ICM20608G_STATUS_ERROR,
    ICM20608G_STATUS_BUSY,
    ICM20608G_STATUS_TIMEOUT,
} ICM20608GState_t;

// Structure for IO context
typedef struct _ICM20608GIOctx_t {
    ICM20608G_InitFunc       Init;
    ICM20608G_DeInitFunc     DeInit;
    ICM20608G_IsDevReadyFunc IsDevReady;
    ICM20608G_ReadRegFunc    ReadReg;
    ICM20608G_WriteRegFunc   WriteReg;
    ICM20608G_WaitFunc       Wait;
} ICM20608GIOctx_t;

// Structure for sensor values
typedef struct _ICM20608GData_t {
    float accX;
    float accY;
    float accZ;
    float gyrX;
    float gyrY;
    float gyrZ;
    float temp;
} ICM20608GData_t;

// Object handle structure
typedef struct _ICM20608GObj_t {
    uint16_t devAddr;               // Device address
    uint8_t isInit;                 // Initialization status
    uint8_t* dataBuff;              // Data Buffer
    ICM20608GData_t IMUData;        // Acc, Gyro, Temp Data
    ICM20608GIOctx_t io;            // IO context
} ICM20608GObj_t;


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

// Set the I/O context for the ICM20608G object.
uint8_t ICM20608G_Init(ICM20608GObj_t* icm20608g_obj);

// Initialize the ICM20608G object.
uint8_t ICM20608G_SetIoctx(ICM20608GObj_t* icm20608g_obj, ICM20608GIOctx_t* icm20608g_ioctx);

// Retrieve the acclerometer and gyroscope data values from the ICM20608G object.
uint8_t ICM20608G_GetValue(ICM20608GObj_t* icm20608g_obj);


#endif /* ICM20608G_INC_ICM20608G_H_ */
