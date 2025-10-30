/*
 * ioif_mti630.h
 *
 *  Created on: Sep 23, 2025
 *      Author: INVINCIBLENESS
 */

#ifndef MTI_630_INC_IOIF_MTI630_H_
#define MTI_630_INC_IOIF_MTI630_H_

#include "module.h"

/** @defgroup UART
  * @brief UART IMU module driver
  * @{
  */
#ifdef IOIF_MTI630_ENABLED

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "usart.h"
#include "stm32h7xx_it.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define IOIF_MTI630_RX_BUFFER_LENGTH				50 		// For Euler Angles (4 + 45 + 1)
//#define IOIF_MTI630_RX_BUFFER_LENGTH	   			54		// For Quaternion (4 + 49 + 1)


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */
typedef enum _IOIF_MTI630_State_t {
    IOIF_MTI630_STATUS_OK = 0,
	IOIF_MTI630_STATUS_ERROR,
} IOIF_MTI630_State_t;

typedef enum _IOIF_MTI630_Baudrate_t {
    IOIF_MTI630_BAUDRATE_9600 = 1,
    IOIF_MTI630_BAUDRATE_19200,
    IOIF_MTI630_BAUDRATE_38400,
    IOIF_MTI630_BAUDRATE_57600,
    IOIF_MTI630_BAUDRATE_115200,
    IOIF_MTI630_BAUDRATE_230400,
    IOIF_MTI630_BAUDRATE_460800,
    IOIF_MTI630_BAUDRATE_921600,
	IOIF_MTI630_BAUDRATE_ERROR,
} IOIF_MTI630_Baudrate_t;

typedef enum _IOIF_MTI630_OutputRate_t {
    IOIF_MTI630_OUTPUT_RATE_POLLING = 0,
    IOIF_MTI630_OUTPUT_RATE_1ms,
	IOIF_MTI630_OUTPUT_RATE_ERROR,
} IOIF_MTI630_OutputRate_t;

typedef enum _IOIF_MTI630_OutputCode_t {
	IOIF_MTI630_OUTPUT_CODE_ASCII = 1,
	IOIF_MTI630_OUTPUT_CODE_HEX,
	IOIF_MTI630_OUTPUT_CODE_ERROR,
} IOIF_MTI630_OutputCode_t;

typedef enum _IOIF_MTI630_OutputFormat_t {
	IOIF_MTI630_OUTPUT_FORMAT_EULER = 1,
	IOIF_MTI630_OUTPUT_FORMAT_QUATERNION,
	IOIF_MTI630_OUTPUT_FORMAT_QUATERNION_GYR_ACC,
	IOIF_MTI630_OUTPUT_FORMAT_EULER_ACC_GYR,
	IOIF_MTI630_OUTPUT_FORMAT_ERROR,
} IOIF_MTI630_OutputFormat_t;

typedef enum _IOIF_MTI630_OutputGyro_t {
	IOIF_MTI630_OUTPUT_GYRO_OFF = 0,
	IOIF_MTI630_OUTPUT_GYRO_ON,
	IOIF_MTI630_OUTPUT_GYRO_ERROR,
} IOIF_MTI630_OutputGyro_t;

typedef enum _IOIF_MTI630_OutputAcc_t {
	IOIF_MTI630_OUTPUT_ACC_OFF = 0,
	IOIF_MTI630_OUTPUT_ACC_ON_RAW,
	IOIF_MTI630_OUTPUT_ACC_ON_GC_LOCAL,
	IOIF_MTI630_OUTPUT_ACC_ON_GC_GLOBAL,
	IOIF_MTI630_OUTPUT_ACC_ON_VEL_LOCAL,
	IOIF_MTI630_OUTPUT_ACC_ON_VEL_GLOBAL,
	IOIF_MTI630_OUTPUT_ACC_ERROR,
} IOIF_MTI630_OutputAcc_t;

typedef enum _IOIF_MTI630_OutputMag_t {
	IOIF_MTI630_OUTPUT_MAG_OFF = 0,
	IOIF_MTI630_OUTPUT_MAG_ON,
	IOIF_MTI630_OUTPUT_MAG_ERROR,
} IOIF_MTI630_OutputMag_t;

typedef enum _IOIF_MTI630_RxMode_t {
	IOIF_MTI630_RXMODE_EULER_ACC_GYR,
	IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR,
} IOIF_MTI630_RxMode_t;

typedef struct _IOIF_MTI630_EulerAngle_t {
	float roll;
	float pitch;
	float yaw;
} IOIF_MTI630_EulerAngle_t;

typedef struct _IOIF_MTI630_Quaternion_t {
	float w;
	float x;
	float y;
	float z;
} IOIF_MTI630_Quaternion_t;

typedef struct _IOIF_MTI630_9DOF_Data_t {
	float accX;
	float accY;
	float accZ;

	float gyrX;
	float gyrY;
	float gyrZ;

	float magX;
	float magY;
	float magZ;
} IOIF_MTI630_9DOF_Data_t;

typedef struct _IOIF_MTI630_OutputConfig_t {
	IOIF_MTI630_OutputRate_t 	outputRate;
	IOIF_MTI630_OutputCode_t	outputCode;
	IOIF_MTI630_OutputFormat_t 	outputFormat;

	IOIF_MTI630_OutputAcc_t 	outputAcc;
	IOIF_MTI630_OutputGyro_t	outputGyro;
	IOIF_MTI630_OutputMag_t 	outputMag;

	uint8_t outputDataNum_ASCII;
	uint8_t outputDataByte_ASCII;
	uint8_t outputDataNum_HEX;
	uint8_t outputDataByte_HEX;
} IOIF_MTI630_OutputConfig_t;

typedef struct _IOIF_MTI630_Obj_t {
	UART_HandleTypeDef* MTI630_huart;

	IOIF_MTI630_Baudrate_t baudRate;
	IOIF_MTI630_OutputConfig_t outputConfig;

	IOIF_MTI630_EulerAngle_t eulerAngle;
	IOIF_MTI630_Quaternion_t quaternion;
	IOIF_MTI630_9DOF_Data_t sensorData;

	uint8_t rxMode;
	uint8_t sensorID;

	uint8_t firstRun;
} IOIF_MTI630_Obj_t;

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
IOIF_MTI630_State_t IOIF_MTI630_Init(IOIF_MTI630_Obj_t* mti630Obj, UART_HandleTypeDef* huart, uint8_t settingChange);
IOIF_MTI630_State_t IOIF_MTI630_GetIMUData(IOIF_MTI630_Obj_t* mti630Obj, uint8_t RxMode);
IOIF_MTI630_State_t IOIF_MTI630_GetIMUData_continuous_euler(IOIF_MTI630_Obj_t* mti630Obj, uint8_t RxMode);
IOIF_MTI630_State_t IOIF_MTI630_GetIMUData_continuous_quaternion(IOIF_MTI630_Obj_t* mti630Obj, uint8_t RxMode);
void Reset_UART_mti630(UART_HandleTypeDef *huart);

#endif /* IOIF_MTI630_ENABLED */

#endif /* MTI_630_INC_IOIF_MTI630_H_ */
