/*
 * ioif_ebimu_9dofv5_r3.h
 *
 *  Created on: April 17, 2025
 *      Author: INVINCIBLE_1NE
 */

#ifndef EBIMU_9DOFV5_R3_INC_IOIF_EBIMU_9DOFV5_R3_H_
#define EBIMU_9DOFV5_R3_INC_IOIF_EBIMU_9DOFV5_R3_H_

#include "module.h"

/** @defgroup UART
  * @brief UART IMU module driver
  * @{
  */
#ifdef IOIF_EBIMU_9DOFV5_R3_ENABLED

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

/*------------------------------------------- COMMAND CODE LIST -------------------------------------------*/
/* Set Baudrate [sb] */
#define SET_BAUDRATE_9600				1
#define SET_BAUDRATE_19200				2
#define SET_BAUDRATE_38400				3
#define SET_BAUDRATE_57600				4
#define SET_BAUDRATE_115200				5		// Default
#define SET_BAUDRATE_230400				6
#define SET_BAUDRATE_460800				7
#define SET_BAUDRATE_921600				8

/* Set Output Rate [sor] */
#define SET_OUTPUT_RATE_POLLING			0		// Polling mode
#define SET_OUTPUT_RATE_1MS				1       // 1ms sampling
#define SET_OUTPUT_RATE_USER			10		// [USER selection] Otherwise, you can choose 1~1000 value for 1~1000ms sampling period (You have to change this value if you use this)

/* Set Output Code [soc] */
#define SET_OUTPUT_CODE_ASCII			1		// ASCII mode (default)
#define SET_OUTPUT_CODE_HEX				2 		// HEX mode

/* Set Output Format [sof] */
#define SET_OUTPUT_FORMAT_EULER			1		// Euler Angle mode (default)
#define SET_OUTPUT_FORMAT_QUATERNION 	2 		// Quaternion mode

/* Set Output Gyro [sog] */
#define SET_OUTPUT_GYRO_OFF				0		// Do not output Gyroscope data (default)
#define SET_OUTPUT_GYRO_ON				1 		// Output Gyroscope data

/* Set Output Acc [soa] */
#define SET_OUTPUT_ACC_OFF				0		// Do not output Accelerometer data (default)
#define SET_OUTPUT_ACC_ON_RAW			1 		// Output Raw ACC data
#define SET_OUTPUT_ACC_ON_GC_LOCAL		2		// Output ACC data with removed gravity in local
#define SET_OUTPUT_ACC_ON_GC_GLOBAL		3 		// Output ACC data with removed gravity in global
#define SET_OUTPUT_ACC_ON_VEL_LOCAL		4		// Output velocity data in local
#define SET_OUTPUT_ACC_ON_VEL_GLOBAL	5 		// Output velocity data in global

/* Set Output Mag [som] */
#define SET_OUTPUT_MAG_OFF				0 		// Do not output Magnetometer data (default)
#define SET_OUTPUT_MAG_ON				1 		// Output Magnetometer data

/* Set Output Distance [sod] */
#define SET_OUTPUT_DISTANCE_OFF			0		// Do not output Distance data (default)
#define SET_OUTPUT_DISTANCE_ON_LOCAL	1		// Output Distance data in local
#define SET_OUTPUT_DISTANCE_ON_GLOBAL	2		// Output Distance data in global

/* Set Output Temperature [sot] */
#define SET_OUTPUT_TEMP_OFF				0 		// Do not output Temperature data (default)
#define SET_OUTPUT_TEMP_ON				1 		// Output Temperature data

/* Set Output Time Stamp [sots] */
#define SET_OUTPUT_TIMESTAMP_OFF		0		// Do not output Time Stamp data (default)
#define SET_OUTPUT_TIMESTAMP_ON			1 		// Output Time Stamp data

/* Set Enable Magnetometer [sem] */
#define SET_ENABLE_MAG_OFF				0		// Magnetometer OFF
#define SET_ENABLE_MAG_ON				1		// Magnetometer ON
#define SET_ENABLE_MAG_ON_2				2		// Magnetometer ON 2

/* Set Gyroscope Sensitivity [ssg] */
#define SET_GYRO_SENS_125				1		// 125dps
#define SET_GYRO_SENS_250				2		// 250dps
#define SET_GYRO_SENS_500				3		// 500dps
#define SET_GYRO_SENS_1000				4		// 1000dps
#define SET_GYRO_SENS_2000				5		// 2000dps (default)

/* Set Accelerometer Sensitivity [ssa] */
#define SET_ACC_SENS_2g					1		// 2g
#define SET_ACC_SENS_4g					2		// 4g
#define SET_ACC_SENS_8g					3		// 8g
#define SET_ACC_SENS_16g				4		// 16g (default)

/* Set Low Pass Filter for Accelerometer [lpfa] */
#define SET_ACC_LPF_21					0		// Cutoff Frequency = 21Hz
#define SET_ACC_LPF_48					1		// 48Hz
#define SET_ACC_LPF_59					2		// 59Hz
#define SET_ACC_LPF_97					3		// 97Hz	(default)
#define SET_ACC_LPF_117					4		// 117Hz
#define SET_ACC_LPF_191					5		// 191Hz
#define SET_ACC_LPF_230					6		// 230Hz
#define SET_ACC_LPF_262					7		// 262Hz
#define SET_ACC_LPF_493					8		// 493Hz
#define SET_ACC_LPF_OFF					9		// No LPF

/* Set Filter Factor for [sff], [sffa], [sffm] */
#define SET_FF_BOTH						10		// [USER selection]:: [sff]: ACC/MAG filter factor  -> (default=10) you can choose the value between 1~50
#define SET_FF_ACC						10		// [USER selection]:: [sffa]: ACC filter factor     -> (default=10) you can choose the value between 1~50
#define SET_FF_MAG						10		// [USER selection]:: [sffm]: MAG filter factor     -> (default=10) you can choose the value between 1~50

/* Robust Attitude Algorithm Parameters [raa_l], [raa_t] */
#define RAA_PARAM_LEVEL					0.1		// [USER selection]:: [raa_l]: 0.00 ~ 100.00 (default=0.1)
#define RAA_PARAM_TIMEOUT				20000	// [USER selection]:: [raa_t]: 0 ~ 2000000000 (default=20000)

/* Robust Heading Algorithm Parameters [rha_l], [rha_t] */
#define RHA_PARAM_LEVEL					0.075	// [USER selection]:: [rha_l]: 0.00 ~ 100.00 (default=0.075)
#define RHA_PARAM_TIMEOUT				10000	// [USER selection]:: [rha_t]: 0 ~ 2000000000 (default=10000)

/* RHA Heading Clear [rha_clr] */
#define RHA_CLEAR								// No need any command (Just use "<rha_clr>")

/* RHA Find Heading [rha_fh] */
#define RHA_FH_NOAUTO					0		// No auto-alignment (default)
#define RHA_FH_AUTO						50		// Auto-alignment

/* Auto Gyroscope Calibration Parameters [agc_e], [agc_t], [agc_d] */
#define AUTO_GYRO_CALIB_DISABLE			0		// AGC Disabled
#define AUTO_GYRO_CALIB_ENABLE			1		// AGC Enabled (default)
#define AUTO_GYRO_CALIB_THRESHOLD		0.5		// [USER selection]:: AGC Threshold (default=0.5) : 0.00 ~ 100.00
#define AUTO_GYRO_CALIB_DRIFT			0.3		// [USER selection]:: AGC Drift (default=0.3) : 0.00 ~ 10.00

/* Active Vibration Cancellation Parameters [avca_e] */
#define AVC_ENABLE						0		// AVC Disabled (default)
#define AVC_DISABLE						1 		// AVC Enabled

/* Position Filter Parameters [posf_sl] */
#define PFP_PARAM						0.02 	// [USER selection]:: PFP parameter (default=0.02) : 0.0000 ~ 1.0000

/* Position Zero [posz] */
#define POS_ZERO						 		// No need any command (Just use "<posz>")

/* Calibration Gyroscope [cg] */
#define CALIB_GYRO								// No need any command (Just use "<cg>")

/* Calibration Accelerometer Free [caf] */
#define CALIB_ACC_FREE							// No need any command (Just use "<caf>")

/* Calibration Accelerometer Simple [cas] */
#define CALIB_ACC_SIMPLE						// No need any command (Just use "<cas>")

/* Calibration Magnetometer Free [cmf] */
#define CALIB_MAG_FREE							// No need any command (Just use "<cmf>")

/* Calibration Magnetometer XY [cnxy], [+cnxy] */
#define CALIB_MAG_XY							// No need any command (Just use "<cnxy>" or "<+cnxy>")

/* Calibration Magnetometer Z [cnz], [+cnz] */
#define CALIB_MAG_Z								// No need any command (Just use "<cnz>" or "<+cnz>")

/* Set Motion Offset [cmo], [cmox], [cmoy], [cmoz], [cmoxy], [cmo2], [cnmoh] */
#define SET_MOTION_OFFSET_RPY					// No need any command (Just use "<cmo>")
#define SET_MOTION_OFFSET_ROLL					// No need any command (Just use "<cmox>")
#define SET_MOTION_OFFSET_PITCH					// No need any command (Just use "<cmoy>")
#define SET_MOTION_OFFSET_YAW					// No need any command (Just use "<cmoz>")
#define SET_MOTION_OFFSET_RP					// No need any command (Just use "<cmoxy>")
#define SET_MOTION_OFFSET_RPY_ROT				// No need any command (Just use "<cmo2>")
#define SET_MOTION_OFFSET_YAW_ROT				// No need any command (Just use "<cmoh>")

/* Output Configuration [cfg] */
#define OUTPUT_CONFIGURATION					// No need any command (Just use "<cfg>")

/* Power ON Start [pons] */
#define POWER_ON_START_OFF				0		// Sensor is not activated after power is supplied
#define POWER_ON_START_ON				1		// Sensor is activated after power is supplied (default)

/* START [start] */
#define EBIMU_START								// No need any command (Just use "<start>")

/* STOP [stop] */
#define EBIMU_STOP								// No need any command (Just use "<stop>")

/* Load Factory Settings [lf] */
#define LOAD_FS									// No need any command (Just use "<lf>")

/* RESET [reset] */
#define EBIMU_RESET								// No need any command (Just use "<reset>")

/* Version Check [ver] */
#define EBIMU_VERSION_CHECK						// No need any command (Just use "<ver>")

/* Response of EBIMU */
#define EBIMU_OK								// "ok"
#define EBIMU_ERROR								// "er"

/* Scaler for HEX output */
#define EBIMU_HEX_DATA_SIZE				2		// 2bytes for each data in HEX mode
#define EBIMU_HEX_DATA_SOP				2 		// 2bytes SOP = 0x5555
#define EBIMU_HEX_DATA_SIZE_CHECKSUM	2		// 2bytes for Check Sum
#define EBIMU_HEX_SCALE_EULERANGLE		100
#define EBIMU_HEX_SCALE_QUATERNION		10000
#define EBIMU_HEX_SCALE_GYRO			10
#define EBIMU_HEX_SCALE_ACC				1000
#define EBIMU_HEX_SCALE_MAG				10
#define EBIMU_HEX_SCALE_DISTANCE		1000
#define EBIMU_HEX_SCALE_TEMPERATURE		10
#define EBIMU_HEX_SCALE_TIMESTAMP		1

/* MAX bytes for ASCII output */
#define EBIMU_ASCII_DATA_SIZE			1		// 1byte for each character in ASCII mode
#define EBIMU_ASCII_DATA_SOL			1 		// 1bytes SOL = '*'
#define EBIMU_ASCII_DATA_EOL			2		// 2bytes EOL = '\r\n'
#define EBIMU_ASCII_BYTES_EULERANGLE 	20		// roll(7) + pitch(6) + yaw(7) = 20bytes
#define EBIMU_ASCII_BYTES_QUATERNION	28		// w(7) + x(7) + y(7) + z(7) = 28bytes
#define EBIMU_ASCII_BYTES_GYRO			24		// x(8) + y(8) + z(8) = 24bytes
#define EBIMU_ASCII_BYTES_ACC			24		// x(8) + y(8) + z(8) = 24bytes
#define EBIMU_ASCII_BYTES_MAG			21		// x(7) + y(7) + z(7) = 21bytes
#define EBIMU_ASCII_BYTES_DISTANCE		21		// Not accurate
#define EBIMU_ASCII_BYTES_TEMPERATURE	5		// Not accurate
#define EBIMU_ASCII_BYTES_TIMESTAMP		5




#define IOIF_EBIMU_RX_BUFFER_LENGTH 	24		// 22bytes for [EULER_GYR_ACC], 10bytes for [EULER], 12bytes for [QUATERNION]  // 24bytes for [QUATERNION_GYR_ACC]
#define IOIF_EBIMU_RX_LENGTH_DEFAULT 	24      // 22bytes for [EULER_GYR_ACC], 10bytes for [EULER], 12bytes for [QUATERNION]  // 24bytes for [QUATERNION_GYR_ACC]
#define IOIF_EBIMU_RX_DELAY				100 	// For DMA Rx Waiting Delay (it is needed for large data RX)
#define IOIF_EBIMU_TX_TIMEOUT			1
#define IOIF_EBIMU_RX_TIMEOUT			1
#define IOIF_EBIMU_CMD_MAX_NUM			20
#define IOIF_EBIMU_RESPONSE_MAX_NUM		20
#define IOIF_EBIMU_RESPONSE_NUM			4

#define systickMHz						480
#define IOIF_EBIMU_CMD_TX_DELAY			1000000  	// [usec]

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */
typedef enum _IOIF_EBIMU_State_t {
    IOIF_EBIMU_STATUS_OK = 0,
	IOIF_EBIMU_STATUS_ERROR,
} IOIF_EBIMU_State_t;


typedef enum _IOIF_EBIMU_Baudrate_t {
    IOIF_EBIMU_BAUDRATE_9600 = 1,
    IOIF_EBIMU_BAUDRATE_19200,
    IOIF_EBIMU_BAUDRATE_38400,
    IOIF_EBIMU_BAUDRATE_57600,
    IOIF_EBIMU_BAUDRATE_115200,
    IOIF_EBIMU_BAUDRATE_230400,
    IOIF_EBIMU_BAUDRATE_460800,
    IOIF_EBIMU_BAUDRATE_921600,
	IOIF_EBIMU_BAUDRATE_ERROR,
} IOIF_EBIMU_Baudrate_t;

typedef enum _IOIF_EBIMU_OutputRate_t {
    IOIF_EBIMU_OUTPUT_RATE_POLLING = 0,
    IOIF_EBIMU_OUTPUT_RATE_1ms,
	IOIF_EBIMU_OUTPUT_RATE_ERROR,
} IOIF_EBIMU_OutputRate_t;

typedef enum _IOIF_EBIMU_OutputCode_t {
	IOIF_EBIMU_OUTPUT_CODE_ASCII = 1,
	IOIF_EBIMU_OUTPUT_CODE_HEX,
	IOIF_EBIMU_OUTPUT_CODE_ERROR,
} IOIF_EBIMU_OutputCode_t;

typedef enum _IOIF_EBIMU_OutputFormat_t {
	IOIF_EBIMU_OUTPUT_FORMAT_EULER = 1,
	IOIF_EBIMU_OUTPUT_FORMAT_QUATERNION,
	IOIF_EBIMU_OUTPUT_FORMAT_QUATERNION_GYR_ACC,
	IOIF_EBIMU_OUTPUT_FORMAT_ERROR,
} IOIF_EBIMU_OutputFormat_t;

typedef enum _IOIF_EBIMU_OutputGyro_t {
	IOIF_EBIMU_OUTPUT_GYRO_OFF = 0,
	IOIF_EBIMU_OUTPUT_GYRO_ON,
	IOIF_EBIMU_OUTPUT_GYRO_ERROR,
} IOIF_EBIMU_OutputGyro_t;

typedef enum _IOIF_EBIMU_OutputAcc_t {
	IOIF_EBIMU_OUTPUT_ACC_OFF = 0,
	IOIF_EBIMU_OUTPUT_ACC_ON_RAW,
	IOIF_EBIMU_OUTPUT_ACC_ON_GC_LOCAL,
	IOIF_EBIMU_OUTPUT_ACC_ON_GC_GLOBAL,
	IOIF_EBIMU_OUTPUT_ACC_ON_VEL_LOCAL,
	IOIF_EBIMU_OUTPUT_ACC_ON_VEL_GLOBAL,
	IOIF_EBIMU_OUTPUT_ACC_ERROR,
} IOIF_EBIMU_OutputAcc_t;

typedef enum _IOIF_EBIMU_OutputMag_t {
	IOIF_EBIMU_OUTPUT_MAG_OFF = 0,
	IOIF_EBIMU_OUTPUT_MAG_ON,
	IOIF_EBIMU_OUTPUT_MAG_ERROR,
} IOIF_EBIMU_OutputMag_t;

typedef enum _IOIF_EBIMU_OutputDistance_t {
	IOIF_EBIMU_OUTPUT_DISTANCE_OFF = 0,
	IOIF_EBIMU_OUTPUT_DISTANCE_ON_LOCAL,
	IOIF_EBIMU_OUTPUT_DISTANCE_ON_GLOBAL,
	IOIF_EBIMU_OUTPUT_DISTANCE_ERROR,
} IOIF_EBIMU_OutputDistance_t;

typedef enum _IOIF_EBIMU_OutputTemperature_t {
	IOIF_EBIMU_OUTPUT_TEMPERATURE_OFF = 0,
	IOIF_EBIMU_OUTPUT_TEMPERATURE_ON,
	IOIF_EBIMU_OUTPUT_TEMPERATURE_ERROR,
} IOIF_EBIMU_OutputTemperature_t;

typedef enum _IOIF_EBIMU_OutputTimeStamp_t {
	IOIF_EBIMU_OUTPUT_TIMESTAMP_OFF = 0,
	IOIF_EBIMU_OUTPUT_TIMESTAMP_ON,
	IOIF_EBIMU_OUTPUT_TIMESTAMP_ERROR,
} IOIF_EBIMU_OutputTimeStamp_t;

typedef enum _IOIF_EBIMU_SensorEnableMag_t {
	IOIF_EBIMU_SENSOR_ENABLE_MAG_OFF = 0,
	IOIF_EBIMU_SENSOR_ENABLE_MAG_ON,
	IOIF_EBIMU_SENSOR_ENABLE_MAG_ON2,
	IOIF_EBIMU_SENSOR_ENABLE_MAG_ERROR,
} IOIF_EBIMU_SensorEnableMag_t;

typedef enum _IOIF_EBIMU_SensorAccSens_t {
    IOIF_EBIMU_SENSOR_ACC_SENS_2g = 1,
    IOIF_EBIMU_SENSOR_ACC_SENS_4g,
    IOIF_EBIMU_SENSOR_ACC_SENS_8g,
    IOIF_EBIMU_SENSOR_ACC_SENS_16g,
	IOIF_EBIMU_SENSOR_ACC_SENS_ERROR,
} IOIF_EBIMU_SensorAccSens_t;


typedef enum _IOIF_EBIMU_SensorGyroSens_t {
    IOIF_EBIMU_SENSOR_GYRO_SENS_125 = 1,
    IOIF_EBIMU_SENSOR_GYRO_SENS_250,
    IOIF_EBIMU_SENSOR_GYRO_SENS_500,
    IOIF_EBIMU_SENSOR_GYRO_SENS_1000,
    IOIF_EBIMU_SENSOR_GYRO_SENS_2000,
	IOIF_EBIMU_SENSOR_GYRO_SENS_ERROR,
} IOIF_EBIMU_SensorGyroSens_t;

typedef enum _IOIF_EBIMU_SensorAccLPF_t {
    IOIF_EBIMU_SENSOR_ACC_LPF_21Hz = 0,
	IOIF_EBIMU_SENSOR_ACC_LPF_48Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_59Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_97Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_117Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_191Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_230Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_262Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_493Hz,
	IOIF_EBIMU_SENSOR_ACC_LPF_NONE,
	IOIF_EBIMU_SENSOR_ACC_LPF_ERROR,
} IOIF_EBIMU_SensorAccLPF_t;

typedef enum _IOIF_EBIMU_RxMode_t {
	IOIF_EBIMU_RXMODE_EULER_ONLY,
	IOIF_EBIMU_RXMODE_EULER_ACC,
	IOIF_EBIMU_RXMODE_EULER_GYR,
	IOIF_EBIMU_RXMODE_EULER_GYR_ACC,
	IOIF_EBIMU_RXMODE_QUATERNION_ONLY,
	IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC,
} IOIF_EBIMU_RxMode_t;

typedef struct _IOIF_EBIMU_EulerAngle_t {
	float roll;
	float pitch;
	float yaw;
} IOIF_EBIMU_EulerAngle_t;


typedef struct _IOIF_EBIMU_Quaternion_t {
	float w;
	float x;
	float y;
	float z;
} IOIF_EBIMU_Quaternion_t;


typedef struct _IOIF_EBIMU_9DOF_Data_t {
	float accX;
	float accY;
	float accZ;

	float gyrX;
	float gyrY;
	float gyrZ;

	float magX;
	float magY;
	float magZ;
} IOIF_EBIMU_9DOF_Data_t;


typedef struct _IOIF_EBIMU_OutputConfig_t {
	IOIF_EBIMU_OutputRate_t outputRate;
	IOIF_EBIMU_OutputCode_t	outputCode;
	IOIF_EBIMU_OutputFormat_t outputFormat;

	IOIF_EBIMU_OutputAcc_t outputAcc;
	IOIF_EBIMU_OutputGyro_t	outputGyro;
	IOIF_EBIMU_OutputMag_t outputMag;
	IOIF_EBIMU_OutputDistance_t outputDistance;
	IOIF_EBIMU_OutputTemperature_t outputTemperature;
	IOIF_EBIMU_OutputTimeStamp_t outputTimeStamp;


	uint8_t outputDataNum_ASCII;
	uint8_t outputDataByte_ASCII;
	uint8_t outputDataNum_HEX;
	uint8_t outputDataByte_HEX;
} IOIF_EBIMU_OutputConfig_t;


typedef struct _IOIF_EBIMU_SensorConfig_t {
	IOIF_EBIMU_SensorEnableMag_t sensorEnableMag;

	IOIF_EBIMU_SensorAccSens_t sensorAccSens;
	IOIF_EBIMU_SensorGyroSens_t	sensorGyroSens;
	IOIF_EBIMU_SensorAccLPF_t sensorAccLPF;

	uint8_t sensorAccFF;
	uint8_t sensorMagFF;
} IOIF_EBIMU_SensorConfig_t;


typedef struct _IOIF_EBIMU_Obj_t {
	UART_HandleTypeDef* EBIMU_huart;

	IOIF_EBIMU_Baudrate_t baudRate;

	IOIF_EBIMU_EulerAngle_t	eulerAngle;
	IOIF_EBIMU_Quaternion_t quaternion;
	IOIF_EBIMU_9DOF_Data_t sensorData;

	IOIF_EBIMU_OutputConfig_t outputConfig;
	IOIF_EBIMU_SensorConfig_t sensorConfig;



	char cmdTxBufChar[IOIF_EBIMU_CMD_MAX_NUM];
	char cmdRxResponse[IOIF_EBIMU_RESPONSE_MAX_NUM];

	char RxSOLCheck;
	char RxBufOriginal[IOIF_EBIMU_RX_BUFFER_LENGTH];
//	char RxBuf[IOIF_EBIMU_RX_BUFFER_LENGTH];
	uint16_t cmdTxBufSize;

	uint8_t firstRun;
	uint8_t rxMode;
	uint8_t sensorID;

} IOIF_EBIMU_Obj_t;


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
IOIF_EBIMU_State_t IOIF_EBIMU_Init(IOIF_EBIMU_Obj_t* ebimuObj, UART_HandleTypeDef* huart, uint8_t settingChange);
IOIF_EBIMU_State_t IOIF_EBIMU_CheckResponse(IOIF_EBIMU_Obj_t* ebimuObj);
IOIF_EBIMU_State_t IOIF_EBIMU_GetIMUData(IOIF_EBIMU_Obj_t* ebimuObj, uint8_t RxMode);
void IOIF_EBIMU_RingBuffer(IOIF_EBIMU_Obj_t* ebimuObj, char* originBuffer);
void IOIF_EBIMU_us_Delay(uint32_t us_delay);
void UART7_IRQHandler_forEBIMU(void);
void Reset_UART(UART_HandleTypeDef *huart);


#endif /* IOIF_EBIMU_9DOFV5_R3_ENABLED */

#endif /* EBIMU_9DOFV5_R3_INC_IOIF_EBIMU_9DOFV5_R3_H_ */
