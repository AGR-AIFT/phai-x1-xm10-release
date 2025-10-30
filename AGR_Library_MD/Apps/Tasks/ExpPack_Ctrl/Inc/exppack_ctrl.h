/*
 * exppack_ctrl.h
 *
 *  Created on: Dec 31, 2024
 *      Author: INVINCIBLE_1NE
 */

#ifndef EXPPACK_CTRL_INC_EXPPACK_CTRL_H_
#define EXPPACK_CTRL_INC_EXPPACK_CTRL_H_

#include "module.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ioif_tim_common.h"
#include "error_dictionary.h"

#include "msg_hdlr.h"
//#include "algorithm_ctrl.h"
#include "data_object_common.h"
#include "ioif_adc_common.h"
#include "ioif_ms5607_02ba03.h"
#include "ioif_ra30p.h"
#include "ioif_psl_iemg2.h"
#include "ioif_ebimu_9dofv5_r3.h"
#include "ioif_mti630.h"
#include "ioif_sm1205c.h"


/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/* Define the sensors to use */
/* (1) pMMG sensor */
#define PMMG_1A_ENABLE
//#define PMMG_1B_ENABLE
//#define PMMG_2A_ENABLE
//#define PMMG_2B_ENABLE
//#define PMMG_3A_ENABLE
//#define PMMG_3B_ENABLE
//#define PMMG_4A_ENABLE
//#define PMMG_4B_ENABLE

/* (2) FSR sensor */
//#define FSR_L1_ENABLE
//#define FSR_L2_ENABLE
//#define FSR_L3_ENABLE
//#define FSR_L4_ENABLE
//#define FSR_R1_ENABLE
//#define FSR_R2_ENABLE
//#define FSR_R3_ENABLE
//#define FSR_R4_ENABLE

/* (3) EMG sensor */
//#define EMG_L1_ENABLE
//#define EMG_L2_ENABLE
//#define EMG_L3_ENABLE
//#define EMG_L4_ENABLE
//#define EMG_R1_ENABLE
//#define EMG_R2_ENABLE
//#define EMG_R3_ENABLE
//#define EMG_R4_ENABLE

/* (4) IMU sensor */
//#define IMU_1_ENABLE
//#define IMU_2_ENABLE
//#define IMU_3_ENABLE
//#define IMU_4_ENABLE
//#define IMU_5_ENABLE
//#define IMU_6_ENABLE
//#define IMU_7_ENABLE
//#define IMU_8_ENABLE
//#define IMU_9_ENABLE
//#define IMU_10_ENABLE


/* Check the Usage of Sensor */
#if defined(PMMG_1A_ENABLE) || defined(PMMG_1B_ENABLE) || defined(PMMG_2A_ENABLE) || defined(PMMG_2B_ENABLE) || defined(PMMG_3A_ENABLE) || defined(PMMG_3B_ENABLE) || defined(PMMG_4A_ENABLE) || defined(PMMG_4B_ENABLE)
	#define PMMG_USED
#endif
#if defined(FSR_L1_ENABLE) || defined(FSR_L2_ENABLE) || defined(FSR_L3_ENABLE) || defined(FSR_L4_ENABLE) || defined(FSR_R1_ENABLE) || defined(FSR_R2_ENABLE) || defined(FSR_R3_ENABLE) || defined(FSR_R4_ENABLE)
	#define FSR_USED
#endif
#if defined(EMG_L1_ENABLE) || defined(EMG_L2_ENABLE) || defined(EMG_L3_ENABLE) || defined(EMG_L4_ENABLE) || defined(EMG_R1_ENABLE) || defined(EMG_R2_ENABLE) || defined(EMG_R3_ENABLE) || defined(EMG_R4_ENABLE)
	#define EMG_USED
#endif
#if defined(IMU_1_ENABLE) || defined(IMU_2_ENABLE) || defined(IMU_3_ENABLE) || defined(IMU_4_ENABLE) || defined(IMU_5_ENABLE) || defined(IMU_6_ENABLE) || defined(IMU_7_ENABLE) || defined(IMU_8_ENABLE) || defined(IMU_9_ENABLE) || defined(IMU_10_ENABLE)
	#define IMU_USED
#endif


/* Mode Selection (Button Mode: for independently using of Expansion Board) */
//#define BUTTON_MODE								// Activate this if you use the button HW


/* Define the Sampling Period for USB CDC */
//#define USB_CDC_ACTIVATE						// Activate this for USB_CDC communication with other modules(PC/AM)
#define PERIOD_USB_CDC_MULTIPLY    		1		// [Period of USB_CDC TX] = (Timer interrupt period) x (this value)
#define USB_CDC_TERMINATE_PYTHON		78		// can be changed
#define USB_CDC_BUFFER_SIZE				150 	// bytes [Protocol: 1 + 3x(dataNum) + 1]
#define USB_CDC_PROTOCOL_DATA			0xAB 	// First transmission of Protocol Data Set
#define USB_CDC_START_DATA				0xAC	// SOL (Should be matched with PC code)
#define USB_CDC_END_DATA				0xAD	// EOL (Should be matched with PC code)


/* For Data Save To Hex (for 2Byte Data Processing) */
#define DATA_CONV_CONST_UINT16              65535       // uint16 conversion constant
#define DATA_CONV_CONST_INT16               32767       // int16 conversion constant
#define PMMG_SCALING_FACTOR					200 		// 0~200kPa (It can be changed)
#define EMG_SCALING_FACTOR					4095		// ADC 12Bit Resolution
#define EMG_NORM_SCALING_FACTOR				1			// Normalized EMG [0,1]
#define EMG_RAWSIGN_SCALING_FACTOR			2			// Normalized EMG [-1,1]
#define EMG_RAW_12BIT_SCALING_FACTOR		4095		// Raw EMG [-2048,2047]
#define FSR_RAW_12BIT_SCALING_FACTOR		4095		// Raw FSR [0,4095]
#define IMU_QUATERNION_SCALING_FACTOR		2			// [-1,1]
#define IMU_EULER_ROLL_SCALING_FACTOR		360			// [-180,180]deg
#define IMU_EULER_PITCH_SCALING_FACTOR		180			// [-90,90]deg
#define IMU_EULER_YAW_SCALING_FACTOR		360			// [-180,180]deg
#define IMU_GYR_SCALING_FACTOR				4000		// [-2000,2000]
#define IMU_ACC_SCALING_FACTOR				320			// [-156.9,156.9]
#define TIMESTAMP_SCALING_FACTOR			1

// Have to change //
#define CONTROL_INPUT_SCALING_FACTOR		30			// [-15,15]


// For MD->CM->EXTboard //
#define DEG_SCALING_FACTOR              	720         // (deg) -360 ~ +360
#define ACC_SCALING_FACTOR              	78.4532     // (m/s^2) 8 * g(9.80665) -39.24 ~ +39.24
#define CURRENT_RANGE_MAX                   360         // (-180, 180)A

/* For processing */
#define EMG_MA_BUFF_SIZE					100

/* For Sensor Detection */
#define AUTO_SENSOR_DETECTION_ON						// ON: 1st checker(#define) + 2nd checker(sensor detection), OFF: 1st checker(#define)
#define ADC_DETECTION_THRESHOLD				3
#define NO_DETECTION_CNT					150
#define NO_DETECTION_CNT_IMU				150

/* For Buzzer Usage */
#define BUZZER_MODE

/* IMU output (Please Select this) */
//#define IMU_QUATERNION_VERSION
//#define IMU_QUATERNION_GYR_ACC_VERSION
#define IMU_EULER_GYR_ACC_VERSION

/* EMG output (Please Select this) */
//#define EMG_NORM_VERSION				// [0,1]
#define EMG_RAW_VERSION					// [-2048,2047]

/* FSR mode (For using FSR sensor from EMG ports) */
#define FSR_ALL_MODE

/* CM connect mode */
#define CM_CONNECT_MODE

/* Force Plate SYNC mode */
#define FP_SYNC_MODE


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */
typedef enum _ButtonCMD_t {
	BUTTON_STATE_IDLE,
	BUTTON_STATE_SENSOR_DETECTION,
	BUTTON_STATE_SEND_PROTOCOL,
	BUTTON_STATE_AFTER_PROTOCOL,
	BUTTON_STATE_CONT_TX_START,
} ButtonCMD_t;

typedef enum _BuzzerState_t {
	BUZZER_STATE_IDLE,
	BUZZER_STATE_COMPLETE_SENSOR_DETECTION,
	BUZZER_STATE_COMPLETE_SEND_PROTOCOL,
	BUZZER_STATE_START_TX,
	BUZZER_STATE_TERMINATION,
} BuzzerState_t;

typedef enum _DataType_t {
	DT_CHAR = 0,
	DT_UINT8,
	DT_UINT16,
	DT_UINT32,
	DT_INT8,
	DT_INT16,
	DT_INT32,
	DT_FLOAT32,
	DT_FLOAT64,
	DT_STRING10,
} DataType_t;

typedef enum _DataSet_t {
	DS_TIMESTAMP,

#ifdef IMU_QUATERNION_VERSION
	DS_IMU1_QUATERNION_W,
	DS_IMU1_QUATERNION_X,
	DS_IMU1_QUATERNION_Y,
	DS_IMU1_QUATERNION_Z,

	DS_IMU2_QUATERNION_W,
	DS_IMU2_QUATERNION_X,
	DS_IMU2_QUATERNION_Y,
	DS_IMU2_QUATERNION_Z,

	DS_IMU3_QUATERNION_W,
	DS_IMU3_QUATERNION_X,
	DS_IMU3_QUATERNION_Y,
	DS_IMU3_QUATERNION_Z,

	DS_IMU4_QUATERNION_W,
	DS_IMU4_QUATERNION_X,
	DS_IMU4_QUATERNION_Y,
	DS_IMU4_QUATERNION_Z,

	DS_IMU5_QUATERNION_W,
	DS_IMU5_QUATERNION_X,
	DS_IMU5_QUATERNION_Y,
	DS_IMU5_QUATERNION_Z,

	DS_IMU6_QUATERNION_W,
	DS_IMU6_QUATERNION_X,
	DS_IMU6_QUATERNION_Y,
	DS_IMU6_QUATERNION_Z,

	DS_IMU7_QUATERNION_W,
	DS_IMU7_QUATERNION_X,
	DS_IMU7_QUATERNION_Y,
	DS_IMU7_QUATERNION_Z,

	DS_IMU8_QUATERNION_W,
	DS_IMU8_QUATERNION_X,
	DS_IMU8_QUATERNION_Y,
	DS_IMU8_QUATERNION_Z,

	DS_IMU9_QUATERNION_W,
	DS_IMU9_QUATERNION_X,
	DS_IMU9_QUATERNION_Y,
	DS_IMU9_QUATERNION_Z,

	DS_IMU10_QUATERNION_W,
	DS_IMU10_QUATERNION_X,
	DS_IMU10_QUATERNION_Y,
	DS_IMU10_QUATERNION_Z,
#endif

#ifdef IMU_QUATERNION_GYR_ACC_VERSION
	DS_IMU1_QUATERNION_W,
	DS_IMU1_QUATERNION_X,
	DS_IMU1_QUATERNION_Y,
	DS_IMU1_QUATERNION_Z,
	DS_IMU1_GYR_X,
	DS_IMU1_GYR_Y,
	DS_IMU1_GYR_Z,
	DS_IMU1_ACC_X,
	DS_IMU1_ACC_Y,
	DS_IMU1_ACC_Z,

	DS_IMU2_QUATERNION_W,
	DS_IMU2_QUATERNION_X,
	DS_IMU2_QUATERNION_Y,
	DS_IMU2_QUATERNION_Z,
	DS_IMU2_GYR_X,
	DS_IMU2_GYR_Y,
	DS_IMU2_GYR_Z,
	DS_IMU2_ACC_X,
	DS_IMU2_ACC_Y,
	DS_IMU2_ACC_Z,

	DS_IMU3_QUATERNION_W,
	DS_IMU3_QUATERNION_X,
	DS_IMU3_QUATERNION_Y,
	DS_IMU3_QUATERNION_Z,
	DS_IMU3_GYR_X,
	DS_IMU3_GYR_Y,
	DS_IMU3_GYR_Z,
	DS_IMU3_ACC_X,
	DS_IMU3_ACC_Y,
	DS_IMU3_ACC_Z,

	DS_IMU4_QUATERNION_W,
	DS_IMU4_QUATERNION_X,
	DS_IMU4_QUATERNION_Y,
	DS_IMU4_QUATERNION_Z,
	DS_IMU4_GYR_X,
	DS_IMU4_GYR_Y,
	DS_IMU4_GYR_Z,
	DS_IMU4_ACC_X,
	DS_IMU4_ACC_Y,
	DS_IMU4_ACC_Z,

	DS_IMU5_QUATERNION_W,
	DS_IMU5_QUATERNION_X,
	DS_IMU5_QUATERNION_Y,
	DS_IMU5_QUATERNION_Z,
	DS_IMU5_GYR_X,
	DS_IMU5_GYR_Y,
	DS_IMU5_GYR_Z,
	DS_IMU5_ACC_X,
	DS_IMU5_ACC_Y,
	DS_IMU5_ACC_Z,

	DS_IMU6_QUATERNION_W,
	DS_IMU6_QUATERNION_X,
	DS_IMU6_QUATERNION_Y,
	DS_IMU6_QUATERNION_Z,
	DS_IMU6_GYR_X,
	DS_IMU6_GYR_Y,
	DS_IMU6_GYR_Z,
	DS_IMU6_ACC_X,
	DS_IMU6_ACC_Y,
	DS_IMU6_ACC_Z,

	DS_IMU7_QUATERNION_W,
	DS_IMU7_QUATERNION_X,
	DS_IMU7_QUATERNION_Y,
	DS_IMU7_QUATERNION_Z,
	DS_IMU7_GYR_X,
	DS_IMU7_GYR_Y,
	DS_IMU7_GYR_Z,
	DS_IMU7_ACC_X,
	DS_IMU7_ACC_Y,
	DS_IMU7_ACC_Z,

	DS_IMU8_QUATERNION_W,
	DS_IMU8_QUATERNION_X,
	DS_IMU8_QUATERNION_Y,
	DS_IMU8_QUATERNION_Z,
	DS_IMU8_GYR_X,
	DS_IMU8_GYR_Y,
	DS_IMU8_GYR_Z,
	DS_IMU8_ACC_X,
	DS_IMU8_ACC_Y,
	DS_IMU8_ACC_Z,

	DS_IMU9_QUATERNION_W,
	DS_IMU9_QUATERNION_X,
	DS_IMU9_QUATERNION_Y,
	DS_IMU9_QUATERNION_Z,
	DS_IMU9_GYR_X,
	DS_IMU9_GYR_Y,
	DS_IMU9_GYR_Z,
	DS_IMU9_ACC_X,
	DS_IMU9_ACC_Y,
	DS_IMU9_ACC_Z,

	DS_IMU10_QUATERNION_W,
	DS_IMU10_QUATERNION_X,
	DS_IMU10_QUATERNION_Y,
	DS_IMU10_QUATERNION_Z,
	DS_IMU10_GYR_X,
	DS_IMU10_GYR_Y,
	DS_IMU10_GYR_Z,
	DS_IMU10_ACC_X,
	DS_IMU10_ACC_Y,
	DS_IMU10_ACC_Z,
#endif

#ifdef IMU_EULER_GYR_ACC_VERSION
	DS_IMU1_EULER_X,
	DS_IMU1_EULER_Y,
	DS_IMU1_EULER_Z,
	DS_IMU1_GYR_X,
	DS_IMU1_GYR_Y,
	DS_IMU1_GYR_Z,
	DS_IMU1_ACC_X,
	DS_IMU1_ACC_Y,
	DS_IMU1_ACC_Z,

	DS_IMU2_EULER_X,
	DS_IMU2_EULER_Y,
	DS_IMU2_EULER_Z,
	DS_IMU2_GYR_X,
	DS_IMU2_GYR_Y,
	DS_IMU2_GYR_Z,
	DS_IMU2_ACC_X,
	DS_IMU2_ACC_Y,
	DS_IMU2_ACC_Z,

	DS_IMU3_EULER_X,
	DS_IMU3_EULER_Y,
	DS_IMU3_EULER_Z,
	DS_IMU3_GYR_X,
	DS_IMU3_GYR_Y,
	DS_IMU3_GYR_Z,
	DS_IMU3_ACC_X,
	DS_IMU3_ACC_Y,
	DS_IMU3_ACC_Z,

	DS_IMU4_EULER_X,
	DS_IMU4_EULER_Y,
	DS_IMU4_EULER_Z,
	DS_IMU4_GYR_X,
	DS_IMU4_GYR_Y,
	DS_IMU4_GYR_Z,
	DS_IMU4_ACC_X,
	DS_IMU4_ACC_Y,
	DS_IMU4_ACC_Z,

	DS_IMU5_EULER_X,
	DS_IMU5_EULER_Y,
	DS_IMU5_EULER_Z,
	DS_IMU5_GYR_X,
	DS_IMU5_GYR_Y,
	DS_IMU5_GYR_Z,
	DS_IMU5_ACC_X,
	DS_IMU5_ACC_Y,
	DS_IMU5_ACC_Z,

	DS_IMU6_EULER_X,
	DS_IMU6_EULER_Y,
	DS_IMU6_EULER_Z,
	DS_IMU6_GYR_X,
	DS_IMU6_GYR_Y,
	DS_IMU6_GYR_Z,
	DS_IMU6_ACC_X,
	DS_IMU6_ACC_Y,
	DS_IMU6_ACC_Z,

	DS_IMU7_EULER_X,
	DS_IMU7_EULER_Y,
	DS_IMU7_EULER_Z,
	DS_IMU7_GYR_X,
	DS_IMU7_GYR_Y,
	DS_IMU7_GYR_Z,
	DS_IMU7_ACC_X,
	DS_IMU7_ACC_Y,
	DS_IMU7_ACC_Z,

	DS_IMU8_EULER_X,
	DS_IMU8_EULER_Y,
	DS_IMU8_EULER_Z,
	DS_IMU8_GYR_X,
	DS_IMU8_GYR_Y,
	DS_IMU8_GYR_Z,
	DS_IMU8_ACC_X,
	DS_IMU8_ACC_Y,
	DS_IMU8_ACC_Z,

	DS_IMU9_EULER_X,
	DS_IMU9_EULER_Y,
	DS_IMU9_EULER_Z,
	DS_IMU9_GYR_X,
	DS_IMU9_GYR_Y,
	DS_IMU9_GYR_Z,
	DS_IMU9_ACC_X,
	DS_IMU9_ACC_Y,
	DS_IMU9_ACC_Z,

	DS_IMU10_EULER_X,
	DS_IMU10_EULER_Y,
	DS_IMU10_EULER_Z,
	DS_IMU10_GYR_X,
	DS_IMU10_GYR_Y,
	DS_IMU10_GYR_Z,
	DS_IMU10_ACC_X,
	DS_IMU10_ACC_Y,
	DS_IMU10_ACC_Z,
#endif

#ifdef EMG_NORM_VERSION
	DS_EMGL1_NORM,
	DS_EMGL2_NORM,
	DS_EMGL3_NORM,
	DS_EMGL4_NORM,
	DS_EMGR1_NORM,
	DS_EMGR2_NORM,
	DS_EMGR3_NORM,
	DS_EMGR4_NORM,
#endif

#ifdef EMG_RAW_VERSION
	DS_EMGL1_RAW,
	DS_EMGL2_RAW,
	DS_EMGL3_RAW,
	DS_EMGL4_RAW,
	DS_EMGR1_RAW,
	DS_EMGR2_RAW,
	DS_EMGR3_RAW,
	DS_EMGR4_RAW,
#endif

	DS_FSRL1_RAW,
	DS_FSRL2_RAW,
	DS_FSRL3_RAW,
	DS_FSRL4_RAW,
	DS_FSRR1_RAW,
	DS_FSRR2_RAW,
	DS_FSRR3_RAW,
	DS_FSRR4_RAW,

#ifdef CM_CONNECT_MODE
	DS_CM_INC_POS_RH,
	DS_CM_INC_POS_LH,
	DS_CM_CURRENT_RH,
	DS_CM_CURRENT_LH,
#endif

#ifdef FP_SYNC_MODE
	DS_FP_SYNC,
#endif

	DS_NOT_DETECTED_SENSOR,
} DataSet_t;

typedef enum _pMMG_Idx_t {
	PMMG_1A,
	PMMG_1B,
	PMMG_2A,
	PMMG_2B,
	PMMG_3A,
	PMMG_3B,
	PMMG_4A,
	PMMG_4B,
} pMMG_Idx_t;


typedef enum _SensorDetection_State_t {
	SENSOR_DETECTED,
	SENSOR_NOT_DETECTED,
} SensorDetection_State_t;


typedef struct _USBTxData_t {
	DataSet_t 	dataName;
	DataType_t 	originalDataType;
	DataType_t 	scaledDataType;
}USBTxData_t;


// 0: Detected, 1: Not Detected //
typedef struct _SensorDetection_t {
	/* (1) pMMG sensor */
	SensorDetection_State_t pMMG_1A_detected;
	SensorDetection_State_t pMMG_1B_detected;
	SensorDetection_State_t pMMG_2A_detected;
	SensorDetection_State_t pMMG_2B_detected;
	SensorDetection_State_t pMMG_3A_detected;
	SensorDetection_State_t pMMG_3B_detected;
	SensorDetection_State_t pMMG_4A_detected;
	SensorDetection_State_t pMMG_4B_detected;

	/* (2) FSR sensor */
	SensorDetection_State_t FSR_L1_detected;
	SensorDetection_State_t FSR_L2_detected;
	SensorDetection_State_t FSR_L3_detected;
	SensorDetection_State_t FSR_L4_detected;
	SensorDetection_State_t FSR_R1_detected;
	SensorDetection_State_t FSR_R2_detected;
	SensorDetection_State_t FSR_R3_detected;
	SensorDetection_State_t FSR_R4_detected;

	/* (3) EMG sensor */
	SensorDetection_State_t EMG_L1_detected;
	SensorDetection_State_t EMG_L2_detected;
	SensorDetection_State_t EMG_L3_detected;
	SensorDetection_State_t EMG_L4_detected;
	SensorDetection_State_t EMG_R1_detected;
	SensorDetection_State_t EMG_R2_detected;
	SensorDetection_State_t EMG_R3_detected;
	SensorDetection_State_t EMG_R4_detected;

	/* (4) IMU sensor */
	SensorDetection_State_t IMU_1_detected;
	SensorDetection_State_t IMU_2_detected;
	SensorDetection_State_t IMU_3_detected;
	SensorDetection_State_t IMU_4_detected;
	SensorDetection_State_t IMU_5_detected;
	SensorDetection_State_t IMU_6_detected;
	SensorDetection_State_t IMU_7_detected;
	SensorDetection_State_t IMU_8_detected;
	SensorDetection_State_t IMU_9_detected;
	SensorDetection_State_t IMU_10_detected;

	// Check whether the detection is done //
	uint8_t SENSOR_DETECTION_DONE;
} SensorDetection_t;


typedef struct _pMMG_pressure_t {
	float pMMG1A_press;
	float pMMG1B_press;
	float pMMG2A_press;
	float pMMG2B_press;
	float pMMG3A_press;
	float pMMG3B_press;
	float pMMG4A_press;
	float pMMG4B_press;
} pMMG_pressure_t;

typedef struct _pMMG_temperature_t {
	float pMMG1A_temp;
	float pMMG1B_temp;
	float pMMG2A_temp;
	float pMMG2B_temp;
	float pMMG3A_temp;
	float pMMG3B_temp;
	float pMMG4A_temp;
	float pMMG4B_temp;
} pMMG_temperature_t;

typedef struct _pMMG_err_t {
	uint32_t err1A;
	uint32_t err1B;
	uint32_t err2A;
	uint32_t err2B;
	uint32_t err3A;
	uint32_t err3B;
	uint32_t err4A;
	uint32_t err4B;
	uint32_t totalErr;
} pMMG_err_t;

typedef struct _fsr_data_t {
	uint16_t fsr_L1_raw;
	uint16_t fsr_L2_raw;
	uint16_t fsr_L3_raw;
	uint16_t fsr_L4_raw;
	uint16_t fsr_R1_raw;
	uint16_t fsr_R2_raw;
	uint16_t fsr_R3_raw;
	uint16_t fsr_R4_raw;

	float fsr_L1_raw_send;
	float fsr_L2_raw_send;
	float fsr_L3_raw_send;
	float fsr_L4_raw_send;
	float fsr_R1_raw_send;
	float fsr_R2_raw_send;
	float fsr_R3_raw_send;
	float fsr_R4_raw_send;
} fsr_data_t;

typedef struct _emg_data_t {
	/* [0, 4095] */
	uint16_t emg_L1_raw;
	uint16_t emg_L2_raw;
	uint16_t emg_L3_raw;
	uint16_t emg_L4_raw;
	uint16_t emg_R1_raw;
	uint16_t emg_R2_raw;
	uint16_t emg_R3_raw;
	uint16_t emg_R4_raw;

	/* [-2048, 2047] */
	int16_t emg_L1_rawSign[3];
	int16_t emg_L2_rawSign[3];
	int16_t emg_L3_rawSign[3];
	int16_t emg_L4_rawSign[3];
	int16_t emg_R1_rawSign[3];
	int16_t emg_R2_rawSign[3];
	int16_t emg_R3_rawSign[3];
	int16_t emg_R4_rawSign[3];

	/* [-2048.0f, 2047.0f] for FSR / [0.0f, 4095.0f] for FSR*/
	float emg_L1_raw_send;
	float emg_L2_raw_send;
	float emg_L3_raw_send;
	float emg_L4_raw_send;
	float emg_R1_raw_send;
	float emg_R2_raw_send;
	float emg_R3_raw_send;
	float emg_R4_raw_send;

	/* [0, 2048] */
	float emg_L1_rect;
	float emg_L2_rect;
	float emg_L3_rect;
	float emg_L4_rect;
	float emg_R1_rect;
	float emg_R2_rect;
	float emg_R3_rect;
	float emg_R4_rect;

	/* [0, 1] */
	float emg_L1_norm;
	float emg_L2_norm;
	float emg_L3_norm;
	float emg_L4_norm;
	float emg_R1_norm;
	float emg_R2_norm;
	float emg_R3_norm;
	float emg_R4_norm;

	uint8_t emg_L1_LPFstart;
	uint8_t emg_L2_LPFstart;
	uint8_t emg_L3_LPFstart;
	uint8_t emg_L4_LPFstart;
	uint8_t emg_R1_LPFstart;
	uint8_t emg_R2_LPFstart;
	uint8_t emg_R3_LPFstart;
	uint8_t emg_R4_LPFstart;

	uint8_t emg_L1_BPFstart;
	uint8_t emg_L2_BPFstart;
	uint8_t emg_L3_BPFstart;
	uint8_t emg_L4_BPFstart;
	uint8_t emg_R1_BPFstart;
	uint8_t emg_R2_BPFstart;
	uint8_t emg_R3_BPFstart;
	uint8_t emg_R4_BPFstart;

	/* [-2048, 2047] */
	float emg_L1_LPF[2];
	float emg_L2_LPF[2];
	float emg_L3_LPF[2];
	float emg_L4_LPF[2];
	float emg_R1_LPF[2];
	float emg_R2_LPF[2];
	float emg_R3_LPF[2];
	float emg_R4_LPF[2];

	float emg_L1_BPF[3];
	float emg_L2_BPF[3];
	float emg_L3_BPF[3];
	float emg_L4_BPF[3];
	float emg_R1_BPF[3];
	float emg_R2_BPF[3];
	float emg_R3_BPF[3];
	float emg_R4_BPF[3];

	/* [0, 1] */
	float emg_L1_MA;
	float emg_L2_MA;
	float emg_L3_MA;
	float emg_L4_MA;
	float emg_R1_MA;
	float emg_R2_MA;
	float emg_R3_MA;
	float emg_R4_MA;

	float emg_L1_MA_sum;
	float emg_L2_MA_sum;
	float emg_L3_MA_sum;
	float emg_L4_MA_sum;
	float emg_R1_MA_sum;
	float emg_R2_MA_sum;
	float emg_R3_MA_sum;
	float emg_R4_MA_sum;

	uint16_t emg_L1_MA_Buff[EMG_MA_BUFF_SIZE];
	uint16_t emg_L2_MA_Buff[EMG_MA_BUFF_SIZE];
	uint16_t emg_L3_MA_Buff[EMG_MA_BUFF_SIZE];
	uint16_t emg_L4_MA_Buff[EMG_MA_BUFF_SIZE];
	uint16_t emg_R1_MA_Buff[EMG_MA_BUFF_SIZE];
	uint16_t emg_R2_MA_Buff[EMG_MA_BUFF_SIZE];
	uint16_t emg_R3_MA_Buff[EMG_MA_BUFF_SIZE];
	uint16_t emg_R4_MA_Buff[EMG_MA_BUFF_SIZE];

	uint8_t emg_L1_MA_index;
	uint8_t emg_L2_MA_index;
	uint8_t emg_L3_MA_index;
	uint8_t emg_L4_MA_index;
	uint8_t emg_R1_MA_index;
	uint8_t emg_R2_MA_index;
	uint8_t emg_R3_MA_index;
	uint8_t emg_R4_MA_index;

	uint8_t emg_L1_MA_count;
	uint8_t emg_L2_MA_count;
	uint8_t emg_L3_MA_count;
	uint8_t emg_L4_MA_count;
	uint8_t emg_R1_MA_count;
	uint8_t emg_R2_MA_count;
	uint8_t emg_R3_MA_count;
	uint8_t emg_R4_MA_count;
} emg_data_t;

typedef struct _imu_obj_t {
	// Euler Angle //
	float roll;
	float pitch;
	float yaw;

	// Quaternion //
	float w;
	float x;
	float y;
	float z;

	// Sensor data //
	float accX;
	float accY;
	float accZ;

	float gyrX;
	float gyrY;
	float gyrZ;

	float magX;
	float magY;
	float magZ;

	uint8_t imuIdx;
} imu_obj_t;

typedef struct _imu_data_t {
	imu_obj_t imuObj1;
	imu_obj_t imuObj2;
	imu_obj_t imuObj3;
	imu_obj_t imuObj4;
	imu_obj_t imuObj5;
	imu_obj_t imuObj6;
	imu_obj_t imuObj7;
	imu_obj_t imuObj8;
	imu_obj_t imuObj9;
	imu_obj_t imuObj10;
} imu_data_t;

typedef struct _Exppack_Data_t {
	fsr_data_t fsr_data;
	emg_data_t emg_data;
	imu_data_t imu_data;

	pMMG_pressure_t pMMG_press;
	pMMG_temperature_t pMMG_temp;
	pMMG_err_t pMMG_err;

	uint8_t pMMG_activated_A;
	uint8_t pMMG_activated_B;

	SensorDetection_t sensor_detection;
} Exppack_Data_t;


/* For PDO sending */
typedef struct _ScaledData_t {
	// 16bit Scaled Data (For Sending Large PDO Datas)
	uint16_t pMMG_1A_scaled;
	uint16_t pMMG_1B_scaled;
	uint16_t pMMG_2A_scaled;
	uint16_t pMMG_2B_scaled;
	uint16_t pMMG_3A_scaled;
	uint16_t pMMG_3B_scaled;
	uint16_t pMMG_4A_scaled;
	uint16_t pMMG_4B_scaled;

	uint16_t EMG_L1_scaled;
	uint16_t EMG_L2_scaled;
	uint16_t EMG_L3_scaled;
	uint16_t EMG_L4_scaled;
	uint16_t EMG_R1_scaled;
	uint16_t EMG_R2_scaled;
	uint16_t EMG_R3_scaled;
	uint16_t EMG_R4_scaled;

	uint16_t FSR_L1_scaled;
	uint16_t FSR_L2_scaled;
	uint16_t FSR_L3_scaled;
	uint16_t FSR_L4_scaled;
	uint16_t FSR_R1_scaled;
	uint16_t FSR_R2_scaled;
	uint16_t FSR_R3_scaled;
	uint16_t FSR_R4_scaled;

	/* For HAR_Demo_4 */
	int16_t EMG_RAWSIGN;
	uint16_t EMG_ENVELOPE;
	uint16_t EMG_MA;
} ScaledData_t;



/* Data for IIT */
typedef struct _ReceivedDataFromCM_t {
	int16_t theta_RH_scaled;
	int16_t theta_LH_scaled;
	uint8_t SUIT_state_curr;
	uint8_t SUIT_state_prev;
	int16_t current_RH_scaled;
	int16_t current_LH_scaled;
} ReceivedDataFromCM_t;

typedef struct _CMDataObj_t {
	float theta_RH;
	float theta_LH;
	float current_RH;
	float current_LH;
} CMDataObj_t;



/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */

extern Exppack_Data_t ExpPackDataObj;
extern uint8_t usbTxBuf[USB_CDC_BUFFER_SIZE];
extern uint16_t usbTxBufSize;
extern uint8_t usbTxUpdate;
extern uint8_t GPIO_EXTI_FLAG;
extern uint8_t BUZZER_STATE_FLAG;

extern uint8_t CM_connect_signal;
extern uint8_t CM_disconnect_signal;

/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

void InitExppackCtrl(void);
void RunExppackCtrl(void* params);

/* Data Size scaling for PDO */
int16_t ScaleFloatToInt16(float value, float scaleFactor);
uint16_t ScaleFloatToUInt16(float value, float scaleFactor);
float ScaleInt16ToFloat(int16_t value, float scaleFactor);

/* Used in main */
void CheckActivatedSensors(Exppack_Data_t* exppackDataObj);
void ForcedActivation(Exppack_Data_t* exppackDataObj);
void ForcedDeactivation(Exppack_Data_t* exppackDataObj);


#endif /* EXPPACK_CTRL_INC_EXPPACK_CTRL_H_ */
