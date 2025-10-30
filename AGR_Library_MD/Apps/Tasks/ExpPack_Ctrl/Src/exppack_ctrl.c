/*
 * exppack_ctrl.c
 *
 *  Created on: Dec 31, 2024
 *      Author: INVINCIBLE_1NE
 */

#include "exppack_ctrl.h"

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */


/**
 *------------------------------------------------------------
 *                           VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */

TaskObj_t exppackCtrlTask;

Exppack_Data_t ExpPackDataObj;
ScaledData_t ScaledDataObj;
ReceivedDataFromCM_t ReceivedDataObj;

CMDataObj_t CMDataObj;
uint8_t CM_connect_signal = 0;
uint8_t CM_disconnect_signal = 0;
uint8_t SUIT_State_curr;


// Loop Time Count //
static uint32_t exppackCtrlLoopCnt;
static float exppackCtrlTimeElap;

static uint32_t RTbreak;

// Force Plate Synchronization //
uint8_t fp_sync = 0;


/*-------------- ADC buffer for DMA -------------*/
static uint16_t* adc1buff = {0};
static uint16_t* adc2buff = {0};
static uint16_t* adc3buff = {0};
/*-----------------------------------------------*/


/*------------ Individual PMMG (1~8) ------------*/
IOIF_pMMG_Obj_t pMMGObj_1A;
IOIF_pMMG_Obj_t pMMGObj_1B;
IOIF_pMMG_Obj_t pMMGObj_2A;
IOIF_pMMG_Obj_t pMMGObj_2B;
IOIF_pMMG_Obj_t pMMGObj_3A;
IOIF_pMMG_Obj_t pMMGObj_3B;
IOIF_pMMG_Obj_t pMMGObj_4A;
IOIF_pMMG_Obj_t pMMGObj_4B;

uint8_t state_1A = 0;
uint8_t state_1B = 0;
uint8_t state_2A = 0;
uint8_t state_2B = 0;
uint8_t state_3A = 0;
uint8_t state_3B = 0;
uint8_t state_4A = 0;
uint8_t state_4B = 0;
/*-----------------------------------------------*/


/*--------------------- IMU ---------------------*/
#ifdef IOIF_EBIMU_9DOFV5_R3_ENABLED
IOIF_EBIMU_Obj_t EbimuObj1;
IOIF_EBIMU_Obj_t EbimuObj2;
IOIF_EBIMU_Obj_t EbimuObj3;
IOIF_EBIMU_Obj_t EbimuObj4;
IOIF_EBIMU_Obj_t EbimuObj5;
IOIF_EBIMU_Obj_t EbimuObj6;
IOIF_EBIMU_Obj_t EbimuObj7;
IOIF_EBIMU_Obj_t EbimuObj8;
IOIF_EBIMU_Obj_t EbimuObj9;
IOIF_EBIMU_Obj_t EbimuObj10;
#endif

#ifdef IOIF_MTI630_ENABLED
IOIF_MTI630_Obj_t MTI630Obj1;
IOIF_MTI630_Obj_t MTI630Obj2;
IOIF_MTI630_Obj_t MTI630Obj3;
IOIF_MTI630_Obj_t MTI630Obj4;
IOIF_MTI630_Obj_t MTI630Obj5;
IOIF_MTI630_Obj_t MTI630Obj6;
IOIF_MTI630_Obj_t MTI630Obj7;
IOIF_MTI630_Obj_t MTI630Obj8;
IOIF_MTI630_Obj_t MTI630Obj9;
IOIF_MTI630_Obj_t MTI630Obj10;
#endif
/*-----------------------------------------------*/

/*--------------------- TIMER ---------------------*/
uint16_t TIMER_INTERRUPT_PERIOD = 1000-1;		// Tick
/*-------------------------------------------------*/

/* ------------- USB CDC FS Transmit ------------- */
uint8_t usbTxBuf[USB_CDC_BUFFER_SIZE] = {0};
uint16_t usbTxBufSize = 0;
uint8_t usbTxBufCursor = 0;
uint8_t firstDataCheck = 0;
uint32_t timeUSBCDC = 0;
uint8_t usbTxUpdate = 0;
uint16_t checkSum = 0;
uint8_t checkSumMSB = 0;
uint8_t checkSumLSB = 0;
uint8_t totalUSBCDCTxDataSize = 0;

#if defined(IMU_QUATERNION_GYR_ACC_VERSION)
	#define PROTOCOL_SIZE	122
#elif defined(IMU_EULER_GYR_ACC_VERSION)
	#define PROTOCOL_SIZE	112
#endif



// User should change this (JUST WRITE TOTAL DATASET) //
USBTxData_t usbTxDataSet[PROTOCOL_SIZE] = {
	{DS_TIMESTAMP,				DT_UINT32,		DT_UINT32},

#ifdef IMU_QUATERNION_VERSION
	{DS_IMU1_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU2_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU3_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU4_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU5_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU6_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU7_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU8_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU9_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},

	{DS_IMU10_QUATERNION_W, 	DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_QUATERNION_X, 	DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_QUATERNION_Y, 	DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_QUATERNION_Z, 	DT_FLOAT32, 	DT_INT16},
#endif

#ifdef IMU_QUATERNION_GYR_ACC_VERSION
	{DS_IMU1_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU2_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU3_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU4_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU5_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU6_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU7_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU8_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU9_QUATERNION_W, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_QUATERNION_X, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_QUATERNION_Y, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_QUATERNION_Z, 		DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU10_QUATERNION_W, 	DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_QUATERNION_X, 	DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_QUATERNION_Y, 	DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_QUATERNION_Z, 	DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_ACC_Z, 			DT_FLOAT32, 	DT_INT16},
#endif

#ifdef IMU_EULER_GYR_ACC_VERSION
	{DS_IMU1_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU1_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU2_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU2_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU3_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU3_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU4_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU4_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU5_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU5_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU6_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU6_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU7_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU7_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU8_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU8_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU9_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU9_ACC_Z, 			DT_FLOAT32, 	DT_INT16},

	{DS_IMU10_EULER_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_EULER_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_EULER_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_GYR_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_GYR_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_GYR_Z, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_ACC_X, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_ACC_Y, 			DT_FLOAT32, 	DT_INT16},
	{DS_IMU10_ACC_Z, 			DT_FLOAT32, 	DT_INT16},
#endif

#ifndef FSR_ALL_MODE
#ifdef EMG_NORM_VERSION
	{DS_EMGL1_NORM,		   		DT_FLOAT32, 	DT_UINT16},
	{DS_EMGL2_NORM, 			DT_FLOAT32, 	DT_UINT16},
	{DS_EMGL3_NORM, 			DT_FLOAT32, 	DT_UINT16},
	{DS_EMGL4_NORM, 			DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR1_NORM, 			DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR2_NORM, 			DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR3_NORM, 			DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR4_NORM, 			DT_FLOAT32, 	DT_UINT16},
#endif
#ifdef EMG_RAW_VERSION
	{DS_EMGL1_RAW,		   		DT_FLOAT32, 	DT_INT16},
	{DS_EMGL2_RAW, 				DT_FLOAT32, 	DT_INT16},
	{DS_EMGL3_RAW, 				DT_FLOAT32, 	DT_INT16},
	{DS_EMGL4_RAW, 				DT_FLOAT32, 	DT_INT16},
	{DS_EMGR1_RAW, 				DT_FLOAT32, 	DT_INT16},
	{DS_EMGR2_RAW, 				DT_FLOAT32, 	DT_INT16},
	{DS_EMGR3_RAW, 				DT_FLOAT32, 	DT_INT16},
	{DS_EMGR4_RAW, 				DT_FLOAT32, 	DT_INT16},
#endif
#endif
#ifdef FSR_ALL_MODE
	{DS_EMGL1_RAW,		   		DT_FLOAT32, 	DT_UINT16},
	{DS_EMGL2_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_EMGL3_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_EMGL4_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR1_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR2_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR3_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_EMGR4_RAW, 				DT_FLOAT32, 	DT_UINT16},
#endif

	{DS_FSRL1_RAW,		   		DT_FLOAT32, 	DT_UINT16},
	{DS_FSRL2_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_FSRL3_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_FSRL4_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_FSRR1_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_FSRR2_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_FSRR3_RAW, 				DT_FLOAT32, 	DT_UINT16},
	{DS_FSRR4_RAW, 				DT_FLOAT32, 	DT_UINT16},

#ifdef CM_CONNECT_MODE
	{DS_CM_INC_POS_RH,		   	DT_FLOAT32, 	DT_INT16},
	{DS_CM_INC_POS_LH, 			DT_FLOAT32, 	DT_INT16},
	{DS_CM_CURRENT_RH,		   	DT_FLOAT32, 	DT_INT16},
	{DS_CM_CURRENT_LH, 			DT_FLOAT32, 	DT_INT16},
#endif

#ifdef FP_SYNC_MODE
	{DS_FP_SYNC,		   		DT_UINT8, 		DT_UINT8},
#endif
};

/* ----------------------------------------------- */

/* ---------------- For GPIO EXTI ---------------- */
uint8_t GPIO_EXTI_FLAG = 0;
uint8_t BUZZER_STATE_FLAG = 0;
uint32_t DEBOUNCE_CNT = 0;
uint8_t USB_CDC_STOP_FLAG = 0;

uint16_t buttonPin = GPIO_PIN_8;		// Used button
uint32_t buttonHighAccumulation = 0;
uint32_t buttonLowAccumulation = 0;
uint8_t buttonStateToken = 0;
/* ----------------------------------------------- */


/* ---------------- For Code Time Check ---------------- */
static uint32_t EXTDEVcodeStartTick = 0;
static uint32_t EXTDEVcodeEndTick = 0;
/* ----------------------------------------------------- */


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

/* ------------------- STATE FUNCTION ------------------- */
static void StateOff_Run(void);

static void StateStandby_Ent(void);
static void StateStandby_Run(void);

static void StateEnable_Ent(void);
static void StateEnable_Run(void);
static void StateEnable_Ext(void);

static void StateError_Run(void);

/* ------------------- INITIALIZATION ------------------- */
static void ScalingForPDO(ScaledData_t* Scaled2ByteData, Exppack_Data_t* exppackDataObj);
/* ------------------- ROUTINE ------------------- */
//static int Ent_P_Vector_Decoder_RH();
//static int Run_P_Vector_Decoder_RH();
//static int Ext_P_Vector_Decoder_RH();
//static int Ent_P_Vector_Decoder_LH();
//static int Run_P_Vector_Decoder_LH();
//static int Ext_P_Vector_Decoder_LH();

/* ------------------- STATIC FUNCTIONS ------------------- */
static int InitPMMG(void);
static int GetRawPMMG_1sample(Exppack_Data_t* exppackDataObj, pMMG_Idx_t pMMG_idx);
static int GetRawPMMG_A_ALL(Exppack_Data_t* exppackDataObj);
static int GetRawPMMG_A(Exppack_Data_t* exppackDataObj);
static int GetRawPMMG_B(Exppack_Data_t* exppackDataObj);
static void GetRawFSR(Exppack_Data_t* exppackDataObj);
static int16_t signingEMG(uint16_t input);
static float signingEMG_float(float input);
static void GetRawEMG(Exppack_Data_t* exppackDataObj);

static float RectificationEMG(float EMGval);
static void RectifyEMG_ALL(Exppack_Data_t* exppackDataObj);
static float NormalizeEMG(float EMG_rect_val);
static void NormalizeEMG_ALL(Exppack_Data_t* exppackDataObj);
static float LowPassFilter(uint8_t* isInitialized, float* prevOutput, float currInput);
static float BandPassFilter(uint8_t* isInitialized, float* prevOutput, float* prev_prevOutput, int16_t currInput, int16_t prevInput, int16_t prev_prevInput);
static void LowPassFilteringEMG_ALL(Exppack_Data_t* exppackDataObj);
static void BandPassFilteringEMG_ALL(Exppack_Data_t* exppackDataObj);
static float MovingAverage(uint16_t* MA_Buffer, uint8_t* ind, uint8_t* cnt, float* sum, uint16_t newVal);
static void MovingAverageEMG_ALL(Exppack_Data_t* exppackDataObj);
static void ProcessEMG(Exppack_Data_t* exppackDataObj);

static void ActivateIMU(uint8_t imuIdx);
static void ResetUARTMuxPins(void);
static void InitIMU(uint8_t settingOn);
static void GetIMU(Exppack_Data_t* exppackDataObj);


/*------------------------------------- USB_CDC communication -------------------------------------*/
static void ResetUSBCDCTxBuffer(void);
static void AppendUSBCDCTxDataProtocol(void);
static void SendUSBCDCTxDataProtocol(void);
static void AppendUSBCDCTxData(void* dataPtr, DataType_t originalDataType, DataType_t scaledDataType, uint32_t scalingFactor);
static void GetCheckSum_USBCDC(void);
static void EndCMD_USBCDCTxData(void);
static void GPIO_EXTI_8_CALLBACK(uint16_t gpioPins);
static void ButtonSequence(void);
static void PrepareUSBCDCTxData_TEST(Exppack_Data_t* exppackDataObj);
static void TerminateUSBCDC(void);
/*-------------------------------------------------------------------------------------------------*/


/* ------------------- SDO CALLBACK ------------------- */
static void GetThetaRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void GetThetaLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void Get_SUIT_State_curr(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void GetCurrentRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
static void GetCurrentLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
//static void GetAccXRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
//static void GetAccXLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
//static void GetAccYRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
//static void GetAccYLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

DOP_COMMON_SDO_CB(exppackCtrlTask)

void InitExppackCtrl(void)
{
    InitTask(&exppackCtrlTask);

	/* Start DMA ADC1 for FSR Sensor */
	if(IOIF_StartADCDMA(IOIF_ADC1, &adc1buff, IOIF_ADC1_BUFFER_LENGTH)) {
		//TODO: Error Process
	}
	if(IOIF_StartADCDMA(IOIF_ADC2, &adc2buff, IOIF_ADC2_BUFFER_LENGTH)) {
		//TODO: Error Process
	}
	if(IOIF_StartADCDMA(IOIF_ADC3, &adc3buff, IOIF_ADC3_BUFFER_LENGTH)) {
		//TODO: Error Process
	}

	/* Reset the object */
	memset(&ExpPackDataObj, 0, sizeof(ExpPackDataObj));

	/* Initialize pMMG Sensor */
#ifdef PMMG_USED
	InitPMMG();
#endif
	/* Initialize IMU Sensor */
#ifdef IMU_USED
	InitIMU(1); 	// Change This
#endif
	/* Check Sensor Detection */
//#ifdef AUTO_SENSOR_DETECTION_ON
//	CheckActivatedSensors(&ExpPackDataObj);
//#endif

	/*------------------------------------------- Setting Timer Interrupt Period according to Sensor Usage -------------------------------------------*/
//	if (ExpPackDataObj.pMMG_activated_A == 1) {
//		TIMER_INTERRUPT_PERIOD += 1000;
//	}
//	if (ExpPackDataObj.pMMG_activated_B == 1) {
//		TIMER_INTERRUPT_PERIOD += 1000;
//	}

	TIMER_INTERRUPT_PERIOD = 10000-1;			// 10msec default (change this)
#ifdef FSR_ALL_MODE
	TIMER_INTERRUPT_PERIOD = 10000-1;			// 1msec for PC version
#endif
	HAL_TIM_Base_Stop(&htim2);
	htim2.Init.Period = TIMER_INTERRUPT_PERIOD;
	HAL_TIM_Base_Init(&htim2);
	/*------------------------------------------------------------------------------------------------------------------------------------------------*/


//#ifdef BUTTON_MODE
//	/* GPIO EXTI Setting */
//	IOIF_GPIOCBPtr_t Button_EXTI_Callback = GPIO_EXTI_8_CALLBACK;
//	IOIF_SetGPIOCB(GPIO_PIN_8, IOIF_GPIO_EXTI_CALLBACK, Button_EXTI_Callback);
//#endif

	/* State Definition */
	TASK_CREATE_STATE(&exppackCtrlTask, TASK_STATE_OFF,      NULL,				StateOff_Run,       NULL,         		 true);
	TASK_CREATE_STATE(&exppackCtrlTask, TASK_STATE_STANDBY,  StateStandby_Ent,	StateStandby_Run,	NULL,         		 false);
	TASK_CREATE_STATE(&exppackCtrlTask, TASK_STATE_ENABLE,   StateEnable_Ent,	StateEnable_Run, 	StateEnable_Ext,	 false);
	TASK_CREATE_STATE(&exppackCtrlTask, TASK_STATE_ERROR,    NULL,				StateError_Run,    	NULL,				 false);

	/* Routine Definition */
//	TASK_CREATE_ROUTINE(&exppackCtrlTask,  ROUTINE_ID_EXTDEV_P_VECTOR_DECODER_RH, Ent_P_Vector_Decoder_RH, Run_P_Vector_Decoder_RH, Ext_P_Vector_Decoder_RH);
//	TASK_CREATE_ROUTINE(&exppackCtrlTask,  ROUTINE_ID_EXTDEV_P_VECTOR_DECODER_LH, Ent_P_Vector_Decoder_LH, Run_P_Vector_Decoder_LH, Ext_P_Vector_Decoder_LH);

	/* DOD Definition */
	// DOD
	DOP_CreateDOD(TASK_ID_EXTDEV);

	// PDO
	/* For PDO setting */
	#ifdef PMMG_USED
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_1A,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_1A_scaled);
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_1B,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_1B_scaled);
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_2A,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_2A_scaled);
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_2B,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_2B_scaled);
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_3A,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_3A_scaled);
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_3B,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_3B_scaled);
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_4A,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_4A_scaled);
		DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_PMMG_4B,				DOP_UINT16,	    	1,    &ScaledDataObj.pMMG_4B_scaled);
	#endif
//
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_L1,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_L1_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_L2,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_L2_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_L3,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_L3_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_L4,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_L4_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_R1,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_R1_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_R2,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_R2_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_R3,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_R3_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_R4,				DOP_UINT16,	    	1,    &ScaledDataObj.EMG_R4_scaled);
//
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_L1,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_L1_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_L2,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_L2_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_L3,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_L3_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_L4,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_L4_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_R1,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_R1_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_R2,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_R2_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_R3,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_R3_scaled);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_FSR_R4,				DOP_UINT16,	    	1,    &ScaledDataObj.FSR_R4_scaled);

	/* For HAR_Demo_4 */
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_HARDEMO_1,		DOP_INT16,	    	1,    &ScaledDataObj.EMG_RAWSIGN);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_HARDEMO_2,		DOP_UINT16,	    	1,    &ScaledDataObj.EMG_ENVELOPE);
//	DOP_CreatePDO(TASK_ID_EXTDEV, 	 PDO_ID_EXTDEV_EMG_HARDEMO_3,		DOP_UINT16,	    	1,    &ScaledDataObj.EMG_MA);



	// SDO
	DOP_COMMON_SDO_CREATE(TASK_ID_EXTDEV)
	DOP_CreateSDO(TASK_ID_EXTDEV,	SDO_ID_EXTDEV_GET_THETA_RH,			DOP_INT16,	GetThetaRH);
	DOP_CreateSDO(TASK_ID_EXTDEV,	SDO_ID_EXTDEV_GET_THETA_LH,			DOP_INT16,	GetThetaLH);
	DOP_CreateSDO(TASK_ID_EXTDEV,	SDO_ID_EXTDEV_FSM_CURR,    			DOP_UINT8,	Get_SUIT_State_curr);
	DOP_CreateSDO(TASK_ID_EXTDEV,	SDO_ID_EXTDEV_ACTUAL_CURRENT_RH,	DOP_INT16,	GetCurrentRH);
	DOP_CreateSDO(TASK_ID_EXTDEV,	SDO_ID_EXTDEV_ACTUAL_CURRENT_LH,	DOP_INT16,	GetCurrentLH);


	/* Timer Callback Allocation */
	if (IOIF_StartTimIT(IOIF_TIM2) > 0) {
		//TODO: ERROR PROCESS
	}
	IOIF_SetTimCB(IOIF_TIM2, IOIF_TIM_PERIOD_ELAPSED_CALLBACK, RunExppackCtrl, NULL);
}

void RunExppackCtrl(void* params)
{
	/* Loop Start Time Check */							// pMMG DAQ에서 DWT->CYCCNT 사용하므로, 이 부분 사용 금지
//	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
//	DWT->CYCCNT = 0;
//	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	EXTDEVcodeStartTick = DWT->CYCCNT;

	/* Run Device */
	RunTask(&exppackCtrlTask);

	/* Elapsed Time Check */
	EXTDEVcodeEndTick = DWT->CYCCNT;
	if (EXTDEVcodeEndTick < EXTDEVcodeStartTick) {
		exppackCtrlTimeElap = ((4294967295 - EXTDEVcodeStartTick) + EXTDEVcodeEndTick + 1) / 480;	// in microsecond (Roll-over)
	}
	else {
		exppackCtrlTimeElap = (EXTDEVcodeEndTick - EXTDEVcodeStartTick) / 480;				// in microsecond
	}

	/* RT Check */
	if (exppackCtrlTimeElap > TIMER_INTERRUPT_PERIOD) {
		RTbreak++;
	}
}



/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

static void StateOff_Run(void)
{
	StateTransition(&exppackCtrlTask.stateMachine, TASK_STATE_STANDBY);
}

static void StateStandby_Ent(void)
{

}

static void StateStandby_Run(void)
{
//	if (CM_connect_signal == 1) {
//		Send_ExtensionBoardEnable();
//		CM_connect_signal = 0;
//	}

#ifdef BUTTON_MODE
	if (GPIO_EXTI_FLAG == BUTTON_STATE_SEND_PROTOCOL) {
	#ifdef USB_CDC_ACTIVATE
		SendUSBCDCTxDataProtocol();
	#endif
		GPIO_EXTI_FLAG = BUTTON_STATE_AFTER_PROTOCOL;
		BUZZER_STATE_FLAG = BUZZER_STATE_COMPLETE_SEND_PROTOCOL;
	}
	else if (GPIO_EXTI_FLAG == BUTTON_STATE_CONT_TX_START) {
		BUZZER_STATE_FLAG = BUZZER_STATE_START_TX;
		StateTransition(&exppackCtrlTask.stateMachine, TASK_STATE_ENABLE);
	}

//	ButtonAccumulate();		// Keep button click
	ButtonSequence();

	DEBOUNCE_CNT++;
#endif

#ifndef BUTTON_MODE
	StateTransition(&exppackCtrlTask.stateMachine, TASK_STATE_ENABLE);
#endif
}

static void StateEnable_Ent(void)
{
	EntRoutines(&exppackCtrlTask.routine);

	exppackCtrlLoopCnt = 0;
}

static void StateEnable_Run(void)
{
	RunRoutines(&exppackCtrlTask.routine);

	fp_sync = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_15);

#ifdef PMMG_USED
	/* Get pMMG data (1) A,B are activated(about 2.6ms), (2) A or B is activated(about 1.3ms) */
	if ( (ExpPackDataObj.pMMG_activated_A == 1) && (ExpPackDataObj.pMMG_activated_B == 1) ) {
		if (exppackCtrlLoopCnt % 3 == 0) {		// Sampling with 3ms
//			GetRawPMMG_A(&ExpPackDataObj);
//			GetRawPMMG_B(&ExpPackDataObj);
		}
	}
	else if ( (ExpPackDataObj.pMMG_activated_A == 1) && (ExpPackDataObj.pMMG_activated_B == 0) ) {
		if (exppackCtrlLoopCnt % 2 == 0) {		// Sampling with 2ms
			GetRawPMMG_1sample(&ExpPackDataObj, PMMG_1A);
//			GetRawPMMG_A(&ExpPackDataObj);
		}
	}
	else if ( (ExpPackDataObj.pMMG_activated_A == 0) && (ExpPackDataObj.pMMG_activated_B == 1) ) {
		if (exppackCtrlLoopCnt % 2 == 0) {		// Sampling with 2ms
//			GetRawPMMG_B(&ExpPackDataObj);
		}
	}
//	GetRawPMMG_A_ALL(&ExpPackDataObj);
#endif

#ifdef FSR_USED
	GetRawFSR(&ExpPackDataObj);
#endif
#ifdef EMG_USED
	GetRawEMG(&ExpPackDataObj);

	/* Processing Part */
//	ProcessEMG(&ExpPackDataObj);
#endif


#ifdef IMU_USED
//	IOIF_EBIMU_GetIMUData(&EbimuObj1, IOIF_EBIMU_RXMODE_EULER_ACC_GYR);
//	if (exppackCtrlLoopCnt % 10 == 0) {
//		GetIMU(&ExpPackDataObj);
//	}
	GetIMU(&ExpPackDataObj);
#endif

	/* Scaling for PDO sending */
//	ScalingForPDO(&ScaledDataObj, &ExpPackDataObj);


#ifdef BUTTON_MODE
#ifdef USB_CDC_ACTIVATE
	/* For X[msec] sampling */
	uint32_t USB_CDC_TX_SamplingPeriod_msec = PERIOD_USB_CDC_MULTIPLY * (TIMER_INTERRUPT_PERIOD + 1) / 1000;

	if ((exppackCtrlLoopCnt) % PERIOD_USB_CDC_MULTIPLY == 0) {
		PrepareUSBCDCTxData_TEST(&ExpPackDataObj);
		timeUSBCDC = timeUSBCDC + USB_CDC_TX_SamplingPeriod_msec;
	}
#endif
	DEBOUNCE_CNT++;
#endif


//	ButtonAccumulate();				// Keep button click
	ButtonSequence();
	exppackCtrlLoopCnt++;
}

static void StateEnable_Ext(void)
{
    ExtRoutines(&exppackCtrlTask.routine);
}

static void StateError_Run(void)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int InitPMMG(void)
{
	uint8_t pMMG_activate_A = 0;
	uint8_t pMMG_activate_B = 0;

#ifdef PMMG_1A_ENABLE
	state_1A = IOIF_pMMG_Init(&pMMGObj_1A, IOIF_SPI1, IOIF_GPIO_PORT_G, IOIF_GPIO_PIN_10);
	pMMG_activate_A = 1;
#endif
#ifdef PMMG_1B_ENABLE
	state_1B = IOIF_pMMG_Init(&pMMGObj_1B, IOIF_SPI1, IOIF_GPIO_PORT_G, IOIF_GPIO_PIN_8);
	pMMG_activate_B = 1;
#endif
#ifdef PMMG_2A_ENABLE
	state_2A = IOIF_pMMG_Init(&pMMGObj_2A, IOIF_SPI2, IOIF_GPIO_PORT_B, IOIF_GPIO_PIN_12);
	pMMG_activate_A = 1;
#endif
#ifdef PMMG_2B_ENABLE
	state_2B = IOIF_pMMG_Init(&pMMGObj_2B, IOIF_SPI2, IOIF_GPIO_PORT_H, IOIF_GPIO_PIN_5);
	pMMG_activate_B = 1;
#endif
#ifdef PMMG_3A_ENABLE
	state_3A = IOIF_pMMG_Init(&pMMGObj_3A, IOIF_SPI3, IOIF_GPIO_PORT_A, IOIF_GPIO_PIN_15);
	pMMG_activate_A = 1;
#endif
#ifdef PMMG_3B_ENABLE
	state_3B = IOIF_pMMG_Init(&pMMGObj_3B, IOIF_SPI3, IOIF_GPIO_PORT_D, IOIF_GPIO_PIN_14);
	pMMG_activate_B = 1;
#endif
#ifdef PMMG_4A_ENABLE
	state_4A = IOIF_pMMG_Init(&pMMGObj_4A, IOIF_SPI4, IOIF_GPIO_PORT_E, IOIF_GPIO_PIN_4);
	pMMG_activate_A = 1;
#endif
#ifdef PMMG_4B_ENABLE
	state_4B = IOIF_pMMG_Init(&pMMGObj_4B, IOIF_SPI4, IOIF_GPIO_PORT_D, IOIF_GPIO_PIN_15);
	pMMG_activate_B = 1;
#endif

	ExpPackDataObj.pMMG_activated_A = pMMG_activate_A;
	ExpPackDataObj.pMMG_activated_B = pMMG_activate_B;

	return 0;
}


static int GetRawPMMG_1sample(Exppack_Data_t* exppackDataObj, pMMG_Idx_t pMMG_idx)
{
	switch (pMMG_idx) {
#ifdef PMMG_1A_ENABLE
		case (PMMG_1A):
			IOIF_pMMG_Update(&pMMGObj_1A);
			exppackDataObj->pMMG_press.pMMG1A_press = (float)pMMGObj_1A.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG1A_temp = (float)pMMGObj_1A.pMMGData.temperatureC;
			break;
#endif
#ifdef PMMG_1B_ENABLE
		case (PMMG_1B):
			IOIF_pMMG_Update(&pMMGObj_1B);
			exppackDataObj->pMMG_press.pMMG1B_press = (float)pMMGObj_1B.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG1B_temp = (float)pMMGObj_1B.pMMGData.temperatureC;
			break;
#endif
#ifdef PMMG_2A_ENABLE
		case (PMMG_2A):
			IOIF_pMMG_Update(&pMMGObj_2A);
			exppackDataObj->pMMG_press.pMMG2A_press = (float)pMMGObj_2A.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG2A_temp = (float)pMMGObj_2A.pMMGData.temperatureC;
			break;
#endif
#ifdef PMMG_2B_ENABLE
		case (PMMG_2B):
			IOIF_pMMG_Update(&pMMGObj_2B);
			exppackDataObj->pMMG_press.pMMG2B_press = (float)pMMGObj_2B.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG2B_temp = (float)pMMGObj_2B.pMMGData.temperatureC;
			break;
#endif
#ifdef PMMG_3A_ENABLE
		case (PMMG_3A):
			IOIF_pMMG_Update(&pMMGObj_3A);
			exppackDataObj->pMMG_press.pMMG3A_press = (float)pMMGObj_3A.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG3A_temp = (float)pMMGObj_3A.pMMGData.temperatureC;
			break;
#endif
#ifdef PMMG_3B_ENABLE
		case (PMMG_3B):
			IOIF_pMMG_Update(&pMMGObj_3B);
			exppackDataObj->pMMG_press.pMMG3B_press = (float)pMMGObj_3B.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG3B_temp = (float)pMMGObj_3B.pMMGData.temperatureC;
			break;
#endif
#ifdef PMMG_4A_ENABLE
		case (PMMG_4A):
			IOIF_pMMG_Update(&pMMGObj_4A);
			exppackDataObj->pMMG_press.pMMG4A_press = (float)pMMGObj_4A.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG4A_temp = (float)pMMGObj_4A.pMMGData.temperatureC;
			break;
#endif
#ifdef PMMG_4B_ENABLE
		case (PMMG_4B):
			IOIF_pMMG_Update(&pMMGObj_4B);
			exppackDataObj->pMMG_press.pMMG4B_press = (float)pMMGObj_4B.pMMGData.pressureKPa;
			exppackDataObj->pMMG_temp.pMMG4B_temp = (float)pMMGObj_4B.pMMGData.temperatureC;
			break;
#endif
		default:
			break;
	}

	return 0;
}


static int GetRawPMMG_A_ALL(Exppack_Data_t* exppackDataObj)
{
#if defined(PMMG_1A_ENABLE) && defined(PMMG_2A_ENABLE) && defined(PMMG_3A_ENABLE) && defined(PMMG_4A_ENABLE)
	IOIF_pMMG_Update_multiple_4(&pMMGObj_1A, &pMMGObj_2A, &pMMGObj_3A, &pMMGObj_4A);
	exppackDataObj->pMMG_press.pMMG1A_press = (float)pMMGObj_1A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG1A_temp = (float)pMMGObj_1A.pMMGData.temperatureC;
	exppackDataObj->pMMG_press.pMMG2A_press = (float)pMMGObj_2A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG2A_temp = (float)pMMGObj_2A.pMMGData.temperatureC;
	exppackDataObj->pMMG_press.pMMG3A_press = (float)pMMGObj_3A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG3A_temp = (float)pMMGObj_3A.pMMGData.temperatureC;
	exppackDataObj->pMMG_press.pMMG4A_press = (float)pMMGObj_4A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG4A_temp = (float)pMMGObj_4A.pMMGData.temperatureC;
#endif

	return 0;
}


static int GetRawPMMG_A(Exppack_Data_t* exppackDataObj)
{
	IOIF_pMMG_Update_multiple_2(&pMMGObj_1A, &pMMGObj_2A);

	// Mapping //
#ifdef PMMG_1A_ENABLE
	exppackDataObj->pMMG_press.pMMG1A_press = (float)pMMGObj_1A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG1A_temp = (float)pMMGObj_1A.pMMGData.temperatureC;
#endif
#ifdef PMMG_2A_ENABLE
	exppackDataObj->pMMG_press.pMMG2A_press = (float)pMMGObj_2A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG2A_temp = (float)pMMGObj_2A.pMMGData.temperatureC;
#endif
#ifdef PMMG_3A_ENABLE
	exppackDataObj->pMMG_press.pMMG3A_press = (float)pMMGObj_3A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG3A_temp = (float)pMMGObj_3A.pMMGData.temperatureC;
#endif
#ifdef PMMG_4A_ENABLE
	exppackDataObj->pMMG_press.pMMG4A_press = (float)pMMGObj_4A.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG4A_temp = (float)pMMGObj_4A.pMMGData.temperatureC;
#endif

	return 0;
}

static int GetRawPMMG_B(Exppack_Data_t* exppackDataObj)
{
	IOIF_pMMG_Update_multiple_2(&pMMGObj_1B, &pMMGObj_2B);

	// Mapping //
#ifdef PMMG_1B_ENABLE
	exppackDataObj->pMMG_press.pMMG1B_press = (float)pMMGObj_1B.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG1B_temp = (float)pMMGObj_1B.pMMGData.temperatureC;
#endif
#ifdef PMMG_2B_ENABLE
	exppackDataObj->pMMG_press.pMMG2B_press = (float)pMMGObj_2B.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG2B_temp = (float)pMMGObj_2B.pMMGData.temperatureC;
#endif
#ifdef PMMG_3B_ENABLE
	exppackDataObj->pMMG_press.pMMG3B_press = (float)pMMGObj_3B.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG3B_temp = (float)pMMGObj_3B.pMMGData.temperatureC;
#endif
#ifdef PMMG_4B_ENABLE
	exppackDataObj->pMMG_press.pMMG4B_press = (float)pMMGObj_4B.pMMGData.pressureKPa;
	exppackDataObj->pMMG_temp.pMMG4B_temp = (float)pMMGObj_4B.pMMGData.temperatureC;
#endif

	return 0;
}


static void GetRawFSR(Exppack_Data_t* exppackDataObj)
{
	/* FSR */
#ifdef FSR_L1_ENABLE
	if (exppackDataObj->sensor_detection.FSR_L1_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_L1_raw = adc1buff[2];
		exppackDataObj->fsr_data.fsr_L1_raw_send = (float)(adc1buff[2]);
	}
#endif
#ifdef FSR_L2_ENABLE
	if (exppackDataObj->sensor_detection.FSR_L2_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_L2_raw = adc1buff[3];
		exppackDataObj->fsr_data.fsr_L2_raw_send = (float)(adc1buff[3]);
	}
#endif
#ifdef FSR_L3_ENABLE
	if (exppackDataObj->sensor_detection.FSR_L3_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_L3_raw = adc3buff[0];
		exppackDataObj->fsr_data.fsr_L3_raw_send = (float)(adc3buff[0]);
	}
#endif
#ifdef FSR_L4_ENABLE
	if (exppackDataObj->sensor_detection.FSR_L4_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_L4_raw = adc3buff[1];
		exppackDataObj->fsr_data.fsr_L4_raw_send = (float)(adc3buff[1]);
	}
#endif
#ifdef FSR_R1_ENABLE
	if (exppackDataObj->sensor_detection.FSR_R1_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_R1_raw = adc3buff[5];
		exppackDataObj->fsr_data.fsr_R1_raw_send = (float)(adc3buff[5]);
	}
#endif
#ifdef FSR_R2_ENABLE
	if (exppackDataObj->sensor_detection.FSR_R2_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_R2_raw = adc3buff[9];
		exppackDataObj->fsr_data.fsr_R2_raw_send = (float)(adc3buff[9]);
	}
#endif
#ifdef FSR_R3_ENABLE
	if (exppackDataObj->sensor_detection.FSR_R3_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_R3_raw = adc3buff[4];
		exppackDataObj->fsr_data.fsr_R3_raw_send = (float)(adc3buff[4]);
	}
#endif
#ifdef FSR_R4_ENABLE
	if (exppackDataObj->sensor_detection.FSR_R4_detected == SENSOR_DETECTED) {
		exppackDataObj->fsr_data.fsr_R4_raw = adc3buff[8];
		exppackDataObj->fsr_data.fsr_R4_raw_send = (float)(adc3buff[8]);
	}
#endif
}


static int16_t signingEMG(uint16_t input)
{
	/* Consider the sign of EMG signal (zero mean) */
	int16_t output = 0;

	output = input - 2048;

	if (output < -2048) {
		output = -2048;
	}
	else if (output > 2047) {
		output = 2047;
	}

	return output;
}

static float signingEMG_float(float input)
{
	/* Consider the sign of EMG signal (zero mean) */
	float output = 0.0f;

	output = input - 2048.0f;

	if (output < -2048.0f) {
		output = -2048.0f;
	}
	else if (output > 2047.0f) {
		output = 2047.0f;
	}

	return output;
}


static void GetRawEMG(Exppack_Data_t* exppackDataObj)
{
	/* EMG */
#ifdef EMG_L1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L1_raw = adc3buff[3];
		exppackDataObj->emg_data.emg_L1_rawSign[2] = exppackDataObj->emg_data.emg_L1_rawSign[1];
		exppackDataObj->emg_data.emg_L1_rawSign[1] = exppackDataObj->emg_data.emg_L1_rawSign[0];
		exppackDataObj->emg_data.emg_L1_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_L1_raw);
		exppackDataObj->emg_data.emg_L1_raw_send = (float)(exppackDataObj->emg_data.emg_L1_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L1_raw_send = (float)(adc3buff[3]);
#endif
	}
#endif
#ifdef EMG_L2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L2_raw = adc3buff[7];
		exppackDataObj->emg_data.emg_L2_rawSign[2] = exppackDataObj->emg_data.emg_L2_rawSign[1];
		exppackDataObj->emg_data.emg_L2_rawSign[1] = exppackDataObj->emg_data.emg_L2_rawSign[0];
		exppackDataObj->emg_data.emg_L2_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_L2_raw);
		exppackDataObj->emg_data.emg_L2_raw_send = (float)(exppackDataObj->emg_data.emg_L2_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L2_raw_send = (float)(adc3buff[7]);
#endif
	}
#endif
#ifdef EMG_L3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L3_raw = adc3buff[2];
		exppackDataObj->emg_data.emg_L3_rawSign[2] = exppackDataObj->emg_data.emg_L3_rawSign[1];
		exppackDataObj->emg_data.emg_L3_rawSign[1] = exppackDataObj->emg_data.emg_L3_rawSign[0];
		exppackDataObj->emg_data.emg_L3_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_L3_raw);
		exppackDataObj->emg_data.emg_L3_raw_send = (float)(exppackDataObj->emg_data.emg_L3_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L3_raw_send = (float)(adc3buff[2]);
#endif
	}
#endif
#ifdef EMG_L4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L4_raw = adc3buff[6];
		exppackDataObj->emg_data.emg_L4_rawSign[2] = exppackDataObj->emg_data.emg_L4_rawSign[1];
		exppackDataObj->emg_data.emg_L4_rawSign[1] = exppackDataObj->emg_data.emg_L4_rawSign[0];
		exppackDataObj->emg_data.emg_L4_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_L4_raw);
		exppackDataObj->emg_data.emg_L4_raw_send = (float)(exppackDataObj->emg_data.emg_L4_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_L4_raw_send = (float)(adc3buff[6]);
#endif
	}
#endif
#ifdef EMG_R1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R1_raw = adc1buff[0];
		exppackDataObj->emg_data.emg_R1_rawSign[2] = exppackDataObj->emg_data.emg_R1_rawSign[1];
		exppackDataObj->emg_data.emg_R1_rawSign[1] = exppackDataObj->emg_data.emg_R1_rawSign[0];
		exppackDataObj->emg_data.emg_R1_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_R1_raw);
		exppackDataObj->emg_data.emg_R1_raw_send = (float)(exppackDataObj->emg_data.emg_R1_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R1_raw_send = (float)(adc1buff[0]);
#endif
	}
#endif
#ifdef EMG_R2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R2_raw = adc1buff[1];
		exppackDataObj->emg_data.emg_R2_rawSign[2] = exppackDataObj->emg_data.emg_R2_rawSign[1];
		exppackDataObj->emg_data.emg_R2_rawSign[1] = exppackDataObj->emg_data.emg_R2_rawSign[0];
		exppackDataObj->emg_data.emg_R2_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_R2_raw);
		exppackDataObj->emg_data.emg_R2_raw_send = (float)(exppackDataObj->emg_data.emg_R2_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R2_raw_send = (float)(adc1buff[1]);
#endif
	}
#endif
#ifdef EMG_R3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R3_raw = adc2buff[0];
		exppackDataObj->emg_data.emg_R3_rawSign[2] = exppackDataObj->emg_data.emg_R3_rawSign[1];
		exppackDataObj->emg_data.emg_R3_rawSign[1] = exppackDataObj->emg_data.emg_R3_rawSign[0];
		exppackDataObj->emg_data.emg_R3_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_R3_raw);
		exppackDataObj->emg_data.emg_R3_raw_send = (float)(exppackDataObj->emg_data.emg_R3_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R3_raw_send = (float)(adc2buff[0]);
#endif
	}
#endif
#ifdef EMG_R4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
#ifndef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R4_raw = adc2buff[1];
		exppackDataObj->emg_data.emg_R4_rawSign[2] = exppackDataObj->emg_data.emg_R4_rawSign[1];
		exppackDataObj->emg_data.emg_R4_rawSign[1] = exppackDataObj->emg_data.emg_R4_rawSign[0];
		exppackDataObj->emg_data.emg_R4_rawSign[0] = signingEMG(exppackDataObj->emg_data.emg_R4_raw);
		exppackDataObj->emg_data.emg_R4_raw_send = (float)(exppackDataObj->emg_data.emg_R4_rawSign[0]);
#endif
#ifdef FSR_ALL_MODE
		exppackDataObj->emg_data.emg_R4_raw_send = (float)(adc2buff[1]);
#endif
	}
#endif
}


/* ------------------- SCALING DATA FOR PDO COMMUNICATION ------------------- */
// Function to scale a float to a 16-bit integer type(int16)
int16_t ScaleFloatToInt16(float value, float scaleFactor)
{
	// Scale the float value (Ex. float variable has -70.5 ~ +70.5 range, scaleFactor = 70.5)
	int16_t scaledInt16Value = (int16_t)(value * DATA_CONV_CONST_INT16 / scaleFactor);

	return scaledInt16Value;
}

// Function to scale a float to a 16-bit unsigned integer type(uint16)
uint16_t ScaleFloatToUInt16(float value, float scaleFactor)
{
	// Scale the float value (Ex. float variable has +70 ~ +220 range, scaleFactor = 150)
	uint16_t scaledUint16Value = (uint16_t)(value * DATA_CONV_CONST_UINT16 / scaleFactor);

	return scaledUint16Value;
}

// Function to scale int16 to a float type
float ScaleInt16ToFloat(int16_t value, float scaleFactor)
{
    // Scale the float value
    float scaledValue = (float)(value * scaleFactor / DATA_CONV_CONST_INT16);

    return scaledValue;
}



static void ScalingForPDO(ScaledData_t* Scaled2ByteData, Exppack_Data_t* exppackDataObj)
{
	// Scaling for PDO Data(2Byte)
	Scaled2ByteData->pMMG_1A_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG1A_press, PMMG_SCALING_FACTOR);
	Scaled2ByteData->pMMG_1B_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG1B_press, PMMG_SCALING_FACTOR);
	Scaled2ByteData->pMMG_2A_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG2A_press, PMMG_SCALING_FACTOR);
	Scaled2ByteData->pMMG_2B_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG2B_press, PMMG_SCALING_FACTOR);
	Scaled2ByteData->pMMG_3A_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG3A_press, PMMG_SCALING_FACTOR);
	Scaled2ByteData->pMMG_3B_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG3B_press, PMMG_SCALING_FACTOR);
	Scaled2ByteData->pMMG_4A_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG4A_press, PMMG_SCALING_FACTOR);
	Scaled2ByteData->pMMG_4B_scaled = ScaleFloatToUInt16(exppackDataObj->pMMG_press.pMMG4B_press, PMMG_SCALING_FACTOR);

//	Scaled2ByteData->EMG_L1_scaled = exppackDataObj->emg_data.emg_L1_raw;
//	Scaled2ByteData->EMG_L2_scaled = exppackDataObj->emg_data.emg_L2_raw;
//	Scaled2ByteData->EMG_L3_scaled = exppackDataObj->emg_data.emg_L3_raw;
//	Scaled2ByteData->EMG_L4_scaled = exppackDataObj->emg_data.emg_L4_raw;
//	Scaled2ByteData->EMG_R1_scaled = exppackDataObj->emg_data.emg_R1_raw;
//	Scaled2ByteData->EMG_R2_scaled = exppackDataObj->emg_data.emg_R2_raw;
//	Scaled2ByteData->EMG_R3_scaled = exppackDataObj->emg_data.emg_R3_raw;
//	Scaled2ByteData->EMG_R4_scaled = exppackDataObj->emg_data.emg_R4_raw;

//	Scaled2ByteData->FSR_L1_scaled = exppackDataObj->fsr_data.fsr_L1_raw;
//	Scaled2ByteData->FSR_L2_scaled = exppackDataObj->fsr_data.fsr_L2_raw;
//	Scaled2ByteData->FSR_L3_scaled = exppackDataObj->fsr_data.fsr_L3_raw;
//	Scaled2ByteData->FSR_L4_scaled = exppackDataObj->fsr_data.fsr_L4_raw;
//	Scaled2ByteData->FSR_R1_scaled = exppackDataObj->fsr_data.fsr_R1_raw;
//	Scaled2ByteData->FSR_R2_scaled = exppackDataObj->fsr_data.fsr_R2_raw;
//	Scaled2ByteData->FSR_R3_scaled = exppackDataObj->fsr_data.fsr_R3_raw;
//	Scaled2ByteData->FSR_R4_scaled = exppackDataObj->fsr_data.fsr_R4_raw;

	/* [0,1] normalized & rectified */
//	Scaled2ByteData->EMG_L1_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_L1_norm, EMG_NORM_SCALING_FACTOR);
//	Scaled2ByteData->EMG_L2_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_L2_norm, EMG_NORM_SCALING_FACTOR);
//	Scaled2ByteData->EMG_L3_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_L3_norm, EMG_NORM_SCALING_FACTOR);
//	Scaled2ByteData->EMG_L4_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_L4_norm, EMG_NORM_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R1_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_R1_norm, EMG_NORM_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R2_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_R2_norm, EMG_NORM_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R3_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_R3_norm, EMG_NORM_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R4_scaled = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_R4_norm, EMG_NORM_SCALING_FACTOR);

	/* [-1,1] normalized & signing */
//	Scaled2ByteData->EMG_L1_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_L1_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);
//	Scaled2ByteData->EMG_L2_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_L2_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);
//	Scaled2ByteData->EMG_L3_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_L3_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);
//	Scaled2ByteData->EMG_L4_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_L4_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R1_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_R1_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R2_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_R2_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R3_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_R3_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);
//	Scaled2ByteData->EMG_R4_scaled = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_R4_rawSign / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);

	/* Send raw & processed EMG data (for Demo4) */
//	Scaled2ByteData->EMG_RAWSIGN = ScaleFloatToInt16((float)(exppackDataObj->emg_data.emg_R2_rawSign[0] / 2048.0f), EMG_RAWSIGN_SCALING_FACTOR);	// [-1,1]
//	Scaled2ByteData->EMG_ENVELOPE = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_R2_LPF[0], EMG_NORM_SCALING_FACTOR);							// [0, 1]
//	Scaled2ByteData->EMG_MA = ScaleFloatToUInt16(exppackDataObj->emg_data.emg_R2_MA, EMG_NORM_SCALING_FACTOR);										// [0, 1]

	/* Send PDO data (for KW_Univ) */
//	Scaled2ByteData->control_input_RH = ScaleFloatToInt16((float)(totalDataObj.u_RH), CONTROL_INPUT_SCALING_FACTOR);
//	Scaled2ByteData->control_input_LH = ScaleFloatToInt16((float)(totalDataObj.u_LH), CONTROL_INPUT_SCALING_FACTOR);
//	Scaled2ByteData->theta_ref_RH = ScaleFloatToInt16((float)(totalDataObj.theta_RH_ref), DEG_SCALING_FACTOR);
//	Scaled2ByteData->theta_ref_LH = ScaleFloatToInt16((float)(totalDataObj.theta_LH_ref), DEG_SCALING_FACTOR);
//	Scaled2ByteData->EMG_RAW = ScaleFloatToInt16((float)(totalDataObj.EMG_raw), EMG_RAWSIGN_SCALING_FACTOR);					// [-1,1]
//	Scaled2ByteData->EMG_PROCESSED = ScaleFloatToUInt16((float)(totalDataObj.EMG_processed), EMG_NORM_SCALING_FACTOR);			// [0,1]
//
//	Scaled2ByteData->Fvector_trigger_1 = totalDataObj.fvector_trigger_1;
//	Scaled2ByteData->Fvector_trigger_2 = totalDataObj.fvector_trigger_2;
//	Scaled2ByteData->Fvector_trigger_3 = totalDataObj.fvector_trigger_3;
//	Scaled2ByteData->Fvector_trigger_4 = totalDataObj.fvector_trigger_4;
//	Scaled2ByteData->Fvector_trigger_5 = totalDataObj.fvector_trigger_5;
//
//	Scaled2ByteData->Pvector_test_RH = ScaleFloatToInt16((float)(posCtrl_RH.ref), DEG_SCALING_FACTOR);
//	Scaled2ByteData->Pvector_test_LH = ScaleFloatToInt16((float)(posCtrl_LH.ref), DEG_SCALING_FACTOR);
//	Scaled2ByteData->Pvector_test_RH = posCtrl_RH.ref;
//	Scaled2ByteData->Pvector_test_LH = posCtrl_LH.ref;
}


static float RectificationEMG(float EMGval)
{
	/* Rectification */
	float rectifiedSignal = 0;
	if (EMGval >= 0) {
		rectifiedSignal = EMGval;
	}
	else {
		rectifiedSignal = (-1)*EMGval;
	}

	return rectifiedSignal;
}


static void RectifyEMG_ALL(Exppack_Data_t* exppackDataObj)
{
	/* EMG */
#ifdef EMG_L1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_L1_rect = RectificationEMG(exppackDataObj->emg_data.emg_L1_BPF[0]);
		exppackDataObj->emg_data.emg_L1_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_L1_rawSign[0]));
	}
#endif
#ifdef EMG_L2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_L2_rect = RectificationEMG(exppackDataObj->emg_data.emg_L2_BPF[0]);
		exppackDataObj->emg_data.emg_L2_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_L2_rawSign[0]));
	}
#endif
#ifdef EMG_L3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_L3_rect = RectificationEMG(exppackDataObj->emg_data.emg_L3_BPF[0]);
		exppackDataObj->emg_data.emg_L3_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_L3_rawSign[0]));
	}
#endif
#ifdef EMG_L4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_L4_rect = RectificationEMG(exppackDataObj->emg_data.emg_L4_BPF[0]);
		exppackDataObj->emg_data.emg_L4_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_L4_rawSign[0]));
	}
#endif
#ifdef EMG_R1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_R1_rect = RectificationEMG(exppackDataObj->emg_data.emg_R1_BPF[0]);
		exppackDataObj->emg_data.emg_R1_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_R1_rawSign[0]));
	}
#endif
#ifdef EMG_R2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_R2_rect = RectificationEMG(exppackDataObj->emg_data.emg_R2_BPF[0]);
		exppackDataObj->emg_data.emg_R2_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_R2_rawSign[0]));
	}
#endif
#ifdef EMG_R3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_R3_rect = RectificationEMG(exppackDataObj->emg_data.emg_R3_BPF[0]);
		exppackDataObj->emg_data.emg_R3_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_R3_rawSign[0]));
	}
#endif
#ifdef EMG_R4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
//		exppackDataObj->emg_data.emg_R4_rect = RectificationEMG(exppackDataObj->emg_data.emg_R4_BPF[0]);
		exppackDataObj->emg_data.emg_R4_rect = RectificationEMG((float)(exppackDataObj->emg_data.emg_R4_rawSign[0]));
	}
#endif
}


static float NormalizeEMG(float EMG_rect_val)
{
	/* Normalization */
	float normalizedSignal = EMG_rect_val/2048.0f;

	return normalizedSignal;
}

static void NormalizeEMG_ALL(Exppack_Data_t* exppackDataObj)
{
	/* EMG */
#ifdef EMG_L1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L1_norm = NormalizeEMG(exppackDataObj->emg_data.emg_L1_rect);
	}
#endif
#ifdef EMG_L2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L2_norm = NormalizeEMG(exppackDataObj->emg_data.emg_L2_rect);
	}
#endif
#ifdef EMG_L3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L3_norm = NormalizeEMG(exppackDataObj->emg_data.emg_L3_rect);
	}
#endif
#ifdef EMG_L4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L4_norm = NormalizeEMG(exppackDataObj->emg_data.emg_L4_rect);
	}
#endif
#ifdef EMG_R1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R1_norm = NormalizeEMG(exppackDataObj->emg_data.emg_R1_rect);
	}
#endif
#ifdef EMG_R2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R2_norm = NormalizeEMG(exppackDataObj->emg_data.emg_R2_rect);
	}
#endif
#ifdef EMG_R3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R3_norm = NormalizeEMG(exppackDataObj->emg_data.emg_R3_rect);
	}
#endif
#ifdef EMG_R4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R4_norm = NormalizeEMG(exppackDataObj->emg_data.emg_R4_rect);
	}
#endif
}


static float LowPassFilter(uint8_t* isInitialized, float* prevOutput, float currInput)
{
	if (*isInitialized == 0) {
		*prevOutput = currInput;
		*isInitialized = 1;
	}

//	float currOutput = 0.644150443975408*(*prevOutput) + 0.355849556024592*(currInput);		// 70Hz
//	float currOutput = 0.828204181306860*(*prevOutput) + 0.171795818693140*(currInput);		// 30Hz
	float currOutput = 0.828204181306860*(*prevOutput) + 0.171795818693140*(currInput);		// 20Hz
//	float currOutput = 0.969072426304811*(*prevOutput) + 0.030927573695189*(currInput);		// 5Hz

	*prevOutput = currOutput;

	return currOutput;
}


static float BandPassFilter(uint8_t* isInitialized, float* prevOutput, float* prev_prevOutput, int16_t currInput, int16_t prevInput, int16_t prev_prevInput)
{
	if (*isInitialized == 0) {
		*prevOutput = currInput;
		*prev_prevOutput = currInput;

		*isInitialized = 1;
	}

//	float currOutput = 0.710362863191271*(*prevOutput) + 0.185761652639405*(*prev_prevOutput) + (0.0001949032802507652 * currInput) - (0.0001949032802507652 * prev_prevInput);
	float currOutput = 0.941075889592254*(*prevOutput) - 0.052177855701698*(*prev_prevOutput) + (0.861014163143824 * prevInput) - (0.861014163143824 * prev_prevInput);

	*prev_prevOutput = *prevOutput;
	*prevOutput = currOutput;

	return currOutput;
}


static void LowPassFilteringEMG_ALL(Exppack_Data_t* exppackDataObj)
{
#ifdef EMG_L1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L1_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_L1_LPFstart, &exppackDataObj->emg_data.emg_L1_LPF[1], exppackDataObj->emg_data.emg_L1_norm);
	}
#endif
#ifdef EMG_L2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L2_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_L2_LPFstart, &exppackDataObj->emg_data.emg_L2_LPF[1], exppackDataObj->emg_data.emg_L2_norm);
	}
#endif
#ifdef EMG_L3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L3_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_L3_LPFstart, &exppackDataObj->emg_data.emg_L3_LPF[1], exppackDataObj->emg_data.emg_L3_norm);
	}
#endif
#ifdef EMG_L4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L4_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_L4_LPFstart, &exppackDataObj->emg_data.emg_L4_LPF[1], exppackDataObj->emg_data.emg_L4_norm);
	}
#endif
#ifdef EMG_R1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R1_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_R1_LPFstart, &exppackDataObj->emg_data.emg_R1_LPF[1], exppackDataObj->emg_data.emg_R1_norm);
	}
#endif
#ifdef EMG_R2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R2_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_R2_LPFstart, &exppackDataObj->emg_data.emg_R2_LPF[1], exppackDataObj->emg_data.emg_R2_norm);
	}
#endif
#ifdef EMG_R3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R3_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_R3_LPFstart, &exppackDataObj->emg_data.emg_R3_LPF[1], exppackDataObj->emg_data.emg_R3_norm);
	}
#endif
#ifdef EMG_R4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R4_LPF[0] = LowPassFilter(&exppackDataObj->emg_data.emg_R4_LPFstart, &exppackDataObj->emg_data.emg_R4_LPF[1], exppackDataObj->emg_data.emg_R4_norm);
	}
#endif
}

static void BandPassFilteringEMG_ALL(Exppack_Data_t* exppackDataObj)
{
#ifdef EMG_L1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L1_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_L1_BPFstart, &exppackDataObj->emg_data.emg_L1_BPF[1], &exppackDataObj->emg_data.emg_L1_BPF[2], exppackDataObj->emg_data.emg_L1_rawSign[0], exppackDataObj->emg_data.emg_L1_rawSign[1], exppackDataObj->emg_data.emg_L1_rawSign[2]);
	}
#endif
#ifdef EMG_L2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L2_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_L2_BPFstart, &exppackDataObj->emg_data.emg_L2_BPF[1], &exppackDataObj->emg_data.emg_L2_BPF[2], exppackDataObj->emg_data.emg_L2_rawSign[0], exppackDataObj->emg_data.emg_L2_rawSign[1], exppackDataObj->emg_data.emg_L2_rawSign[2]);
	}
#endif
#ifdef EMG_L3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L3_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_L3_BPFstart, &exppackDataObj->emg_data.emg_L3_BPF[1], &exppackDataObj->emg_data.emg_L3_BPF[2], exppackDataObj->emg_data.emg_L3_rawSign[0], exppackDataObj->emg_data.emg_L3_rawSign[1], exppackDataObj->emg_data.emg_L3_rawSign[2]);
	}
#endif
#ifdef EMG_L4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L4_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_L4_BPFstart, &exppackDataObj->emg_data.emg_L4_BPF[1], &exppackDataObj->emg_data.emg_L4_BPF[2], exppackDataObj->emg_data.emg_L4_rawSign[0], exppackDataObj->emg_data.emg_L4_rawSign[1], exppackDataObj->emg_data.emg_L4_rawSign[2]);
	}
#endif
#ifdef EMG_R1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R1_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_R1_BPFstart, &exppackDataObj->emg_data.emg_R1_BPF[1], &exppackDataObj->emg_data.emg_R1_BPF[2], exppackDataObj->emg_data.emg_R1_rawSign[0], exppackDataObj->emg_data.emg_R1_rawSign[1], exppackDataObj->emg_data.emg_R1_rawSign[2]);
	}
#endif
#ifdef EMG_R2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R2_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_R2_BPFstart, &exppackDataObj->emg_data.emg_R2_BPF[1], &exppackDataObj->emg_data.emg_R2_BPF[2], exppackDataObj->emg_data.emg_R2_rawSign[0], exppackDataObj->emg_data.emg_R2_rawSign[1], exppackDataObj->emg_data.emg_R2_rawSign[2]);
	}
#endif
#ifdef EMG_R3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R3_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_R3_BPFstart, &exppackDataObj->emg_data.emg_R3_BPF[1], &exppackDataObj->emg_data.emg_R3_BPF[2], exppackDataObj->emg_data.emg_R3_rawSign[0], exppackDataObj->emg_data.emg_R3_rawSign[1], exppackDataObj->emg_data.emg_R3_rawSign[2]);
	}
#endif
#ifdef EMG_R4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R4_BPF[0] = BandPassFilter(&exppackDataObj->emg_data.emg_R4_BPFstart, &exppackDataObj->emg_data.emg_R4_BPF[1], &exppackDataObj->emg_data.emg_R4_BPF[2], exppackDataObj->emg_data.emg_R4_rawSign[0], exppackDataObj->emg_data.emg_R4_rawSign[1], exppackDataObj->emg_data.emg_R4_rawSign[2]);
	}
#endif
}


/* newVal = EMG_rect */
static float MovingAverage(uint16_t* MA_Buffer, uint8_t* ind, uint8_t* cnt, float* sum, uint16_t newVal)
{
	float resultedMAval = 0.0;

	*sum = *sum - MA_Buffer[*ind] / (EMG_SCALING_FACTOR / 2.0f);
	MA_Buffer[*ind] = newVal;
	*sum = *sum + ( newVal / (EMG_SCALING_FACTOR / 2.0f) );

	*ind = (*ind + 1) % EMG_MA_BUFF_SIZE;

	if (*cnt < EMG_MA_BUFF_SIZE) {
		*cnt = *cnt + 1;
		resultedMAval = *sum / *cnt;
	}
	else {
		resultedMAval = *sum / EMG_MA_BUFF_SIZE;
	}

	return resultedMAval;
}


static void MovingAverageEMG_ALL(Exppack_Data_t* exppackDataObj)
{
	/* EMG for 70Hz*/
#ifdef EMG_L1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L1_MA = MovingAverage(exppackDataObj->emg_data.emg_L1_MA_Buff, &exppackDataObj->emg_data.emg_L1_MA_index, &exppackDataObj->emg_data.emg_L1_MA_count, &exppackDataObj->emg_data.emg_L1_MA_sum, exppackDataObj->emg_data.emg_L1_rect);
	}
#endif
#ifdef EMG_L2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L2_MA = MovingAverage(exppackDataObj->emg_data.emg_L2_MA_Buff, &exppackDataObj->emg_data.emg_L2_MA_index, &exppackDataObj->emg_data.emg_L2_MA_count, &exppackDataObj->emg_data.emg_L2_MA_sum, exppackDataObj->emg_data.emg_L2_rect);
	}
#endif
#ifdef EMG_L3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L3_MA = MovingAverage(exppackDataObj->emg_data.emg_L3_MA_Buff, &exppackDataObj->emg_data.emg_L3_MA_index, &exppackDataObj->emg_data.emg_L3_MA_count, &exppackDataObj->emg_data.emg_L3_MA_sum, exppackDataObj->emg_data.emg_L3_rect);
	}
#endif
#ifdef EMG_L4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_L4_MA = MovingAverage(exppackDataObj->emg_data.emg_L4_MA_Buff, &exppackDataObj->emg_data.emg_L4_MA_index, &exppackDataObj->emg_data.emg_L4_MA_count, &exppackDataObj->emg_data.emg_L4_MA_sum, exppackDataObj->emg_data.emg_L4_rect);
	}
#endif
#ifdef EMG_R1_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R1_MA = MovingAverage(exppackDataObj->emg_data.emg_R1_MA_Buff, &exppackDataObj->emg_data.emg_R1_MA_index, &exppackDataObj->emg_data.emg_R1_MA_count, &exppackDataObj->emg_data.emg_R1_MA_sum, exppackDataObj->emg_data.emg_R1_rect);
	}
#endif
#ifdef EMG_R2_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R2_MA = MovingAverage(exppackDataObj->emg_data.emg_R2_MA_Buff, &exppackDataObj->emg_data.emg_R2_MA_index, &exppackDataObj->emg_data.emg_R2_MA_count, &exppackDataObj->emg_data.emg_R2_MA_sum, exppackDataObj->emg_data.emg_R2_rect);
	}
#endif
#ifdef EMG_R3_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R3_MA = MovingAverage(exppackDataObj->emg_data.emg_R3_MA_Buff, &exppackDataObj->emg_data.emg_R3_MA_index, &exppackDataObj->emg_data.emg_R3_MA_count, &exppackDataObj->emg_data.emg_R3_MA_sum, exppackDataObj->emg_data.emg_R3_rect);
	}
#endif
#ifdef EMG_R4_ENABLE
	if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
		exppackDataObj->emg_data.emg_R4_MA = MovingAverage(exppackDataObj->emg_data.emg_R4_MA_Buff, &exppackDataObj->emg_data.emg_R4_MA_index, &exppackDataObj->emg_data.emg_R4_MA_count, &exppackDataObj->emg_data.emg_R4_MA_sum, exppackDataObj->emg_data.emg_R4_rect);
	}
#endif
}



static void ProcessEMG(Exppack_Data_t* exppackDataObj)
{
//	BandPassFilteringEMG_ALL(exppackDataObj);		// 20~450Hz			// EMG board implements already 10~200Hz BPF
	RectifyEMG_ALL(exppackDataObj);					// Absolute
	NormalizeEMG_ALL(exppackDataObj);				// Normalize [0,1]
	LowPassFilteringEMG_ALL(exppackDataObj);		// Enveloping
//	MovingAverageEMG_ALL(exppackDataObj);
}



/*----------------------------------------------------------------------------- [IMU] ------------------------------------------------------------------------------*/
static void ActivateIMU(uint8_t imuIdx)
{
	switch (imuIdx) {
		/* Lower Limb */
		case (1):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			break;

		case (2):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
			break;

		case (3):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			break;

		case (4):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);
			break;

		/* Upper Limb */
		case (5):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			break;

		case (6):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			break;

		case (7):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			break;

		case (8):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
			break;

		case (9):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			break;

		case (10):
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET);
			break;

		default:
			break;
	}
}


static void ResetUARTMuxPins(void)
{
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET);
}


#ifdef IOIF_EBIMU_9DOFV5_R3_ENABLED
static void InitIMU(uint8_t settingOn)
{
	EbimuObj1.sensorID = 1;
	EbimuObj2.sensorID = 2;
	EbimuObj3.sensorID = 3;
	EbimuObj4.sensorID = 4;
	EbimuObj5.sensorID = 5;
	EbimuObj6.sensorID = 6;
	EbimuObj7.sensorID = 7;
	EbimuObj8.sensorID = 8;
	EbimuObj9.sensorID = 9;
	EbimuObj10.sensorID = 10;

#ifdef IMU_1_ENABLE
	ActivateIMU(1);
	IOIF_EBIMU_Init(&EbimuObj1, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_2_ENABLE
	ActivateIMU(2);
	IOIF_EBIMU_Init(&EbimuObj2, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_3_ENABLE
	ActivateIMU(3);
	IOIF_EBIMU_Init(&EbimuObj3, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_4_ENABLE
	ActivateIMU(4);
	IOIF_EBIMU_Init(&EbimuObj4, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_5_ENABLE
	ActivateIMU(5);
	IOIF_EBIMU_Init(&EbimuObj5, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_6_ENABLE
	ActivateIMU(6);
	IOIF_EBIMU_Init(&EbimuObj6, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_7_ENABLE
	ActivateIMU(7);
	IOIF_EBIMU_Init(&EbimuObj7, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_8_ENABLE
	ActivateIMU(8);
	IOIF_EBIMU_Init(&EbimuObj8, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_9_ENABLE
	ActivateIMU(9);
	IOIF_EBIMU_Init(&EbimuObj9, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_10_ENABLE
	ActivateIMU(10);
	IOIF_EBIMU_Init(&EbimuObj10, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
}

static void GetIMU(Exppack_Data_t* exppackDataObj)
{
#ifdef IMU_1_ENABLE
	if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
		ActivateIMU(1);
//		IOIF_EBIMU_GetIMUData(&EbimuObj1, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj1, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj1, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_2_ENABLE
	if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
		ActivateIMU(2);
//		IOIF_EBIMU_GetIMUData(&EbimuObj2, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj2, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj2, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_3_ENABLE
	if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
		ActivateIMU(3);
//		IOIF_EBIMU_GetIMUData(&EbimuObj3, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj3, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj3, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_4_ENABLE
	if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
		ActivateIMU(4);
//		IOIF_EBIMU_GetIMUData(&EbimuObj4, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj4, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj4, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_5_ENABLE
	if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
		ActivateIMU(5);
//		IOIF_EBIMU_GetIMUData(&EbimuObj5, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj5, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj5, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_6_ENABLE
	if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
		ActivateIMU(6);
//		IOIF_EBIMU_GetIMUData(&EbimuObj6, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj6, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj6, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_7_ENABLE
	if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
		ActivateIMU(7);
//		IOIF_EBIMU_GetIMUData(&EbimuObj7, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj7, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj7, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_8_ENABLE
	if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
		ActivateIMU(8);
//		IOIF_EBIMU_GetIMUData(&EbimuObj8, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj8, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj8, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_9_ENABLE
	if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
		ActivateIMU(9);
//		IOIF_EBIMU_GetIMUData(&EbimuObj9, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj9, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj9, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_10_ENABLE
	if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
		ActivateIMU(10);
//		IOIF_EBIMU_GetIMUData(&EbimuObj10, IOIF_EBIMU_RXMODE_EULER_ONLY);
//		IOIF_EBIMU_GetIMUData(&EbimuObj10, IOIF_EBIMU_RXMODE_QUATERNION_ONLY);
		IOIF_EBIMU_GetIMUData(&EbimuObj10, IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC);
		ResetUARTMuxPins();
	}
#endif


	/* Copy ALL data to Exppack Object */
#ifdef IMU_1_ENABLE
	if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj1.roll 	= EbimuObj1.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj1.pitch 	= EbimuObj1.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj1.yaw 	= EbimuObj1.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj1.w 		= EbimuObj1.quaternion.w;
		exppackDataObj->imu_data.imuObj1.x 		= EbimuObj1.quaternion.x;
		exppackDataObj->imu_data.imuObj1.y		= EbimuObj1.quaternion.y;
		exppackDataObj->imu_data.imuObj1.z		= EbimuObj1.quaternion.z;
		exppackDataObj->imu_data.imuObj1.gyrX 	= EbimuObj1.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj1.gyrY 	= EbimuObj1.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj1.gyrZ 	= EbimuObj1.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj1.accX 	= EbimuObj1.sensorData.accX;
		exppackDataObj->imu_data.imuObj1.accY 	= EbimuObj1.sensorData.accY;
		exppackDataObj->imu_data.imuObj1.accZ 	= EbimuObj1.sensorData.accZ;
	}
#endif
#ifdef IMU_2_ENABLE
	if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj2.roll 	= EbimuObj2.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj2.pitch 	= EbimuObj2.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj2.yaw 	= EbimuObj2.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj2.w 		= EbimuObj2.quaternion.w;
		exppackDataObj->imu_data.imuObj2.x 		= EbimuObj2.quaternion.x;
		exppackDataObj->imu_data.imuObj2.y		= EbimuObj2.quaternion.y;
		exppackDataObj->imu_data.imuObj2.z		= EbimuObj2.quaternion.z;
		exppackDataObj->imu_data.imuObj2.gyrX 	= EbimuObj2.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj2.gyrY 	= EbimuObj2.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj2.gyrZ 	= EbimuObj2.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj2.accX 	= EbimuObj2.sensorData.accX;
		exppackDataObj->imu_data.imuObj2.accY 	= EbimuObj2.sensorData.accY;
		exppackDataObj->imu_data.imuObj2.accZ 	= EbimuObj2.sensorData.accZ;
	}
#endif
#ifdef IMU_3_ENABLE
	if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj3.roll 	= EbimuObj3.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj3.pitch 	= EbimuObj3.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj3.yaw 	= EbimuObj3.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj3.w 		= EbimuObj3.quaternion.w;
		exppackDataObj->imu_data.imuObj3.x 		= EbimuObj3.quaternion.x;
		exppackDataObj->imu_data.imuObj3.y		= EbimuObj3.quaternion.y;
		exppackDataObj->imu_data.imuObj3.z		= EbimuObj3.quaternion.z;
		exppackDataObj->imu_data.imuObj3.gyrX 	= EbimuObj3.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj3.gyrY 	= EbimuObj3.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj3.gyrZ 	= EbimuObj3.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj3.accX 	= EbimuObj3.sensorData.accX;
		exppackDataObj->imu_data.imuObj3.accY 	= EbimuObj3.sensorData.accY;
		exppackDataObj->imu_data.imuObj3.accZ 	= EbimuObj3.sensorData.accZ;
	}
#endif
#ifdef IMU_4_ENABLE
	if (exppackDataObj->sensor_detection.IMU_4_detected == 0) {
	//	exppackDataObj->imu_data.imuObj4.roll 	= EbimuObj4.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj4.pitch 	= EbimuObj4.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj4.yaw 	= EbimuObj4.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj4.w 		= EbimuObj4.quaternion.w;
		exppackDataObj->imu_data.imuObj4.x 		= EbimuObj4.quaternion.x;
		exppackDataObj->imu_data.imuObj4.y		= EbimuObj4.quaternion.y;
		exppackDataObj->imu_data.imuObj4.z		= EbimuObj4.quaternion.z;
		exppackDataObj->imu_data.imuObj4.gyrX 	= EbimuObj4.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj4.gyrY 	= EbimuObj4.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj4.gyrZ 	= EbimuObj4.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj4.accX 	= EbimuObj4.sensorData.accX;
		exppackDataObj->imu_data.imuObj4.accY 	= EbimuObj4.sensorData.accY;
		exppackDataObj->imu_data.imuObj4.accZ 	= EbimuObj4.sensorData.accZ;
	}
#endif
#ifdef IMU_5_ENABLE
	if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj5.roll 	= EbimuObj5.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj5.pitch 	= EbimuObj5.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj5.yaw 	= EbimuObj5.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj5.w 		= EbimuObj5.quaternion.w;
		exppackDataObj->imu_data.imuObj5.x 		= EbimuObj5.quaternion.x;
		exppackDataObj->imu_data.imuObj5.y		= EbimuObj5.quaternion.y;
		exppackDataObj->imu_data.imuObj5.z		= EbimuObj5.quaternion.z;
		exppackDataObj->imu_data.imuObj5.gyrX 	= EbimuObj5.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj5.gyrY 	= EbimuObj5.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj5.gyrZ 	= EbimuObj5.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj5.accX 	= EbimuObj5.sensorData.accX;
		exppackDataObj->imu_data.imuObj5.accY 	= EbimuObj5.sensorData.accY;
		exppackDataObj->imu_data.imuObj5.accZ 	= EbimuObj5.sensorData.accZ;
	}
#endif
#ifdef IMU_6_ENABLE
	if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj6.roll 	= EbimuObj6.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj6.pitch 	= EbimuObj6.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj6.yaw 	= EbimuObj6.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj6.w 		= EbimuObj6.quaternion.w;
		exppackDataObj->imu_data.imuObj6.x 		= EbimuObj6.quaternion.x;
		exppackDataObj->imu_data.imuObj6.y		= EbimuObj6.quaternion.y;
		exppackDataObj->imu_data.imuObj6.z		= EbimuObj6.quaternion.z;
		exppackDataObj->imu_data.imuObj6.gyrX 	= EbimuObj6.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj6.gyrY 	= EbimuObj6.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj6.gyrZ 	= EbimuObj6.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj6.accX 	= EbimuObj6.sensorData.accX;
		exppackDataObj->imu_data.imuObj6.accY 	= EbimuObj6.sensorData.accY;
		exppackDataObj->imu_data.imuObj6.accZ 	= EbimuObj6.sensorData.accZ;
	}
#endif
#ifdef IMU_7_ENABLE
	if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj7.roll 	= EbimuObj7.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj7.pitch 	= EbimuObj7.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj7.yaw 	= EbimuObj7.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj7.w 		= EbimuObj7.quaternion.w;
		exppackDataObj->imu_data.imuObj7.x 		= EbimuObj7.quaternion.x;
		exppackDataObj->imu_data.imuObj7.y		= EbimuObj7.quaternion.y;
		exppackDataObj->imu_data.imuObj7.z		= EbimuObj7.quaternion.z;
		exppackDataObj->imu_data.imuObj7.gyrX 	= EbimuObj7.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj7.gyrY 	= EbimuObj7.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj7.gyrZ 	= EbimuObj7.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj7.accX 	= EbimuObj7.sensorData.accX;
		exppackDataObj->imu_data.imuObj7.accY 	= EbimuObj7.sensorData.accY;
		exppackDataObj->imu_data.imuObj7.accZ 	= EbimuObj7.sensorData.accZ;
	}
#endif
#ifdef IMU_8_ENABLE
	if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj8.roll 	= EbimuObj8.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj8.pitch 	= EbimuObj8.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj8.yaw 	= EbimuObj8.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj8.w 		= EbimuObj8.quaternion.w;
		exppackDataObj->imu_data.imuObj8.x 		= EbimuObj8.quaternion.x;
		exppackDataObj->imu_data.imuObj8.y		= EbimuObj8.quaternion.y;
		exppackDataObj->imu_data.imuObj8.z		= EbimuObj8.quaternion.z;
		exppackDataObj->imu_data.imuObj8.gyrX 	= EbimuObj8.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj8.gyrY 	= EbimuObj8.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj8.gyrZ 	= EbimuObj8.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj8.accX 	= EbimuObj8.sensorData.accX;
		exppackDataObj->imu_data.imuObj8.accY 	= EbimuObj8.sensorData.accY;
		exppackDataObj->imu_data.imuObj8.accZ 	= EbimuObj8.sensorData.accZ;
	}
#endif
#ifdef IMU_9_ENABLE
	if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj9.roll 	= EbimuObj9.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj9.pitch 	= EbimuObj9.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj9.yaw 	= EbimuObj9.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj9.w 		= EbimuObj9.quaternion.w;
		exppackDataObj->imu_data.imuObj9.x 		= EbimuObj9.quaternion.x;
		exppackDataObj->imu_data.imuObj9.y		= EbimuObj9.quaternion.y;
		exppackDataObj->imu_data.imuObj9.z		= EbimuObj9.quaternion.z;
		exppackDataObj->imu_data.imuObj9.gyrX 	= EbimuObj9.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj9.gyrY 	= EbimuObj9.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj9.gyrZ 	= EbimuObj9.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj9.accX 	= EbimuObj9.sensorData.accX;
		exppackDataObj->imu_data.imuObj9.accY 	= EbimuObj9.sensorData.accY;
		exppackDataObj->imu_data.imuObj9.accZ 	= EbimuObj9.sensorData.accZ;
	}
#endif
#ifdef IMU_10_ENABLE
	if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
	//	exppackDataObj->imu_data.imuObj10.roll 	= EbimuObj10.eulerAngle.roll;
	//	exppackDataObj->imu_data.imuObj10.pitch = EbimuObj10.eulerAngle.pitch;
	//	exppackDataObj->imu_data.imuObj10.yaw 	= EbimuObj10.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj10.w 	= EbimuObj10.quaternion.w;
		exppackDataObj->imu_data.imuObj10.x 	= EbimuObj10.quaternion.x;
		exppackDataObj->imu_data.imuObj10.y		= EbimuObj10.quaternion.y;
		exppackDataObj->imu_data.imuObj10.z		= EbimuObj10.quaternion.z;
		exppackDataObj->imu_data.imuObj10.gyrX 	= EbimuObj10.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj10.gyrY 	= EbimuObj10.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj10.gyrZ 	= EbimuObj10.sensorData.gyrZ;
		exppackDataObj->imu_data.imuObj10.accX 	= EbimuObj10.sensorData.accX;
		exppackDataObj->imu_data.imuObj10.accY 	= EbimuObj10.sensorData.accY;
		exppackDataObj->imu_data.imuObj10.accZ 	= EbimuObj10.sensorData.accZ;
	}
#endif
}
#endif


#ifdef IOIF_MTI630_ENABLED
static void InitIMU(uint8_t settingOn)
{
	MTI630Obj1.sensorID = 1;
	MTI630Obj2.sensorID = 2;
	MTI630Obj3.sensorID = 3;
	MTI630Obj4.sensorID = 4;
	MTI630Obj5.sensorID = 5;
	MTI630Obj6.sensorID = 6;
	MTI630Obj7.sensorID = 7;
	MTI630Obj8.sensorID = 8;
	MTI630Obj9.sensorID = 9;
	MTI630Obj10.sensorID = 10;

#ifdef IMU_1_ENABLE
	ActivateIMU(1);
	IOIF_MTI630_Init(&MTI630Obj1, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_2_ENABLE
	ActivateIMU(2);
	IOIF_MTI630_Init(&MTI630Obj2, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_3_ENABLE
	ActivateIMU(3);
	IOIF_MTI630_Init(&MTI630Obj3, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_4_ENABLE
	ActivateIMU(4);
	IOIF_MTI630_Init(&MTI630Obj4, &huart7, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_5_ENABLE
	ActivateIMU(5);
	IOIF_MTI630_Init(&MTI630Obj5, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_6_ENABLE
	ActivateIMU(6);
	IOIF_MTI630_Init(&MTI630Obj6, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_7_ENABLE
	ActivateIMU(7);
	IOIF_MTI630_Init(&MTI630Obj7, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_8_ENABLE
	ActivateIMU(8);
	IOIF_MTI630_Init(&MTI630Obj8, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_9_ENABLE
	ActivateIMU(9);
	IOIF_MTI630_Init(&MTI630Obj9, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
#ifdef IMU_10_ENABLE
	ActivateIMU(10);
	IOIF_MTI630_Init(&MTI630Obj10, &huart8, settingOn);
	ResetUARTMuxPins();
#endif
}

static void GetIMU(Exppack_Data_t* exppackDataObj)
{
#ifdef IMU_1_ENABLE
	if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
		ActivateIMU(1);
//		IOIF_MTI630_GetIMUData(&MTI630Obj1, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj1, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj1, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_2_ENABLE
	if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
		ActivateIMU(2);
//		IOIF_MTI630_GetIMUData(&MTI630Obj2, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj2, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj2, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_3_ENABLE
	if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
		ActivateIMU(3);
//		IOIF_MTI630_GetIMUData(&MTI630Obj3, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj3, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj3, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_4_ENABLE
	if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
		ActivateIMU(4);
//		IOIF_MTI630_GetIMUData(&MTI630Obj4, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj4, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj4, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_5_ENABLE
	if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
		ActivateIMU(5);
//		IOIF_MTI630_GetIMUData(&MTI630Obj5, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj5, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj5, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_6_ENABLE
	if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
		ActivateIMU(6);
//		IOIF_MTI630_GetIMUData(&MTI630Obj6, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj6, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj6, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_7_ENABLE
	if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
		ActivateIMU(7);
//		IOIF_MTI630_GetIMUData(&MTI630Obj7, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj7, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj7, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_8_ENABLE
	if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
		ActivateIMU(8);
//		IOIF_MTI630_GetIMUData(&MTI630Obj8, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj8, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj8, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_9_ENABLE
	if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
		ActivateIMU(9);
//		IOIF_MTI630_GetIMUData(&MTI630Obj9, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj9, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj9, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif
#ifdef IMU_10_ENABLE
	if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
		ActivateIMU(10);
//		IOIF_MTI630_GetIMUData(&MTI630Obj10, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
		IOIF_MTI630_GetIMUData_continuous_euler(&MTI630Obj10, IOIF_MTI630_RXMODE_EULER_ACC_GYR);
//		IOIF_MTI630_GetIMUData_continuous_quaternion(&MTI630Obj10, IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR);
		ResetUARTMuxPins();
	}
#endif


	/* Copy ALL data to Exppack Object */
#ifdef IMU_1_ENABLE
	if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj1.roll 	= MTI630Obj1.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj1.pitch 	= MTI630Obj1.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj1.yaw 	= MTI630Obj1.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj1.w 		= MTI630Obj1.quaternion.w;
		exppackDataObj->imu_data.imuObj1.x 		= MTI630Obj1.quaternion.x;
		exppackDataObj->imu_data.imuObj1.y		= MTI630Obj1.quaternion.y;
		exppackDataObj->imu_data.imuObj1.z		= MTI630Obj1.quaternion.z;
		exppackDataObj->imu_data.imuObj1.accX 	= MTI630Obj1.sensorData.accX;
		exppackDataObj->imu_data.imuObj1.accY 	= MTI630Obj1.sensorData.accY;
		exppackDataObj->imu_data.imuObj1.accZ 	= MTI630Obj1.sensorData.accZ;
		exppackDataObj->imu_data.imuObj1.gyrX 	= MTI630Obj1.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj1.gyrY 	= MTI630Obj1.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj1.gyrZ 	= MTI630Obj1.sensorData.gyrZ;
	}
#endif
#ifdef IMU_2_ENABLE
	if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj2.roll 	= MTI630Obj2.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj2.pitch 	= MTI630Obj2.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj2.yaw 	= MTI630Obj2.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj2.w 		= MTI630Obj2.quaternion.w;
		exppackDataObj->imu_data.imuObj2.x 		= MTI630Obj2.quaternion.x;
		exppackDataObj->imu_data.imuObj2.y		= MTI630Obj2.quaternion.y;
		exppackDataObj->imu_data.imuObj2.z		= MTI630Obj2.quaternion.z;
		exppackDataObj->imu_data.imuObj2.accX 	= MTI630Obj2.sensorData.accX;
		exppackDataObj->imu_data.imuObj2.accY 	= MTI630Obj2.sensorData.accY;
		exppackDataObj->imu_data.imuObj2.accZ 	= MTI630Obj2.sensorData.accZ;
		exppackDataObj->imu_data.imuObj2.gyrX 	= MTI630Obj2.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj2.gyrY 	= MTI630Obj2.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj2.gyrZ 	= MTI630Obj2.sensorData.gyrZ;
	}
#endif
#ifdef IMU_3_ENABLE
	if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj3.roll 	= MTI630Obj3.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj3.pitch 	= MTI630Obj3.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj3.yaw 	= MTI630Obj3.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj3.w 		= MTI630Obj3.quaternion.w;
		exppackDataObj->imu_data.imuObj3.x 		= MTI630Obj3.quaternion.x;
		exppackDataObj->imu_data.imuObj3.y		= MTI630Obj3.quaternion.y;
		exppackDataObj->imu_data.imuObj3.z		= MTI630Obj3.quaternion.z;
		exppackDataObj->imu_data.imuObj3.accX 	= MTI630Obj3.sensorData.accX;
		exppackDataObj->imu_data.imuObj3.accY 	= MTI630Obj3.sensorData.accY;
		exppackDataObj->imu_data.imuObj3.accZ 	= MTI630Obj3.sensorData.accZ;
		exppackDataObj->imu_data.imuObj3.gyrX 	= MTI630Obj3.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj3.gyrY 	= MTI630Obj3.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj3.gyrZ 	= MTI630Obj3.sensorData.gyrZ;
	}
#endif
#ifdef IMU_4_ENABLE
	if (exppackDataObj->sensor_detection.IMU_4_detected == 0) {
		exppackDataObj->imu_data.imuObj4.roll 	= MTI630Obj4.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj4.pitch 	= MTI630Obj4.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj4.yaw 	= MTI630Obj4.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj4.w 		= MTI630Obj4.quaternion.w;
		exppackDataObj->imu_data.imuObj4.x 		= MTI630Obj4.quaternion.x;
		exppackDataObj->imu_data.imuObj4.y		= MTI630Obj4.quaternion.y;
		exppackDataObj->imu_data.imuObj4.z		= MTI630Obj4.quaternion.z;
		exppackDataObj->imu_data.imuObj4.accX 	= MTI630Obj4.sensorData.accX;
		exppackDataObj->imu_data.imuObj4.accY 	= MTI630Obj4.sensorData.accY;
		exppackDataObj->imu_data.imuObj4.accZ 	= MTI630Obj4.sensorData.accZ;
		exppackDataObj->imu_data.imuObj4.gyrX 	= MTI630Obj4.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj4.gyrY 	= MTI630Obj4.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj4.gyrZ 	= MTI630Obj4.sensorData.gyrZ;
	}
#endif
#ifdef IMU_5_ENABLE
	if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj5.roll 	= MTI630Obj5.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj5.pitch 	= MTI630Obj5.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj5.yaw 	= MTI630Obj5.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj5.w 		= MTI630Obj5.quaternion.w;
		exppackDataObj->imu_data.imuObj5.x 		= MTI630Obj5.quaternion.x;
		exppackDataObj->imu_data.imuObj5.y		= MTI630Obj5.quaternion.y;
		exppackDataObj->imu_data.imuObj5.z		= MTI630Obj5.quaternion.z;
		exppackDataObj->imu_data.imuObj5.accX 	= MTI630Obj5.sensorData.accX;
		exppackDataObj->imu_data.imuObj5.accY 	= MTI630Obj5.sensorData.accY;
		exppackDataObj->imu_data.imuObj5.accZ 	= MTI630Obj5.sensorData.accZ;
		exppackDataObj->imu_data.imuObj5.gyrX 	= MTI630Obj5.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj5.gyrY 	= MTI630Obj5.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj5.gyrZ 	= MTI630Obj5.sensorData.gyrZ;
	}
#endif
#ifdef IMU_6_ENABLE
	if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj6.roll 	= MTI630Obj6.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj6.pitch 	= MTI630Obj6.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj6.yaw 	= MTI630Obj6.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj6.w 		= MTI630Obj6.quaternion.w;
		exppackDataObj->imu_data.imuObj6.x 		= MTI630Obj6.quaternion.x;
		exppackDataObj->imu_data.imuObj6.y		= MTI630Obj6.quaternion.y;
		exppackDataObj->imu_data.imuObj6.z		= MTI630Obj6.quaternion.z;
		exppackDataObj->imu_data.imuObj6.accX 	= MTI630Obj6.sensorData.accX;
		exppackDataObj->imu_data.imuObj6.accY 	= MTI630Obj6.sensorData.accY;
		exppackDataObj->imu_data.imuObj6.accZ 	= MTI630Obj6.sensorData.accZ;
		exppackDataObj->imu_data.imuObj6.gyrX 	= MTI630Obj6.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj6.gyrY 	= MTI630Obj6.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj6.gyrZ 	= MTI630Obj6.sensorData.gyrZ;
	}
#endif
#ifdef IMU_7_ENABLE
	if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj7.roll 	= MTI630Obj7.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj7.pitch 	= MTI630Obj7.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj7.yaw 	= MTI630Obj7.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj7.w 		= MTI630Obj7.quaternion.w;
		exppackDataObj->imu_data.imuObj7.x 		= MTI630Obj7.quaternion.x;
		exppackDataObj->imu_data.imuObj7.y		= MTI630Obj7.quaternion.y;
		exppackDataObj->imu_data.imuObj7.z		= MTI630Obj7.quaternion.z;
		exppackDataObj->imu_data.imuObj7.accX 	= MTI630Obj7.sensorData.accX;
		exppackDataObj->imu_data.imuObj7.accY 	= MTI630Obj7.sensorData.accY;
		exppackDataObj->imu_data.imuObj7.accZ 	= MTI630Obj7.sensorData.accZ;
		exppackDataObj->imu_data.imuObj7.gyrX 	= MTI630Obj7.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj7.gyrY 	= MTI630Obj7.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj7.gyrZ 	= MTI630Obj7.sensorData.gyrZ;
	}
#endif
#ifdef IMU_8_ENABLE
	if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj8.roll 	= MTI630Obj8.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj8.pitch 	= MTI630Obj8.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj8.yaw 	= MTI630Obj8.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj8.w 		= MTI630Obj8.quaternion.w;
		exppackDataObj->imu_data.imuObj8.x 		= MTI630Obj8.quaternion.x;
		exppackDataObj->imu_data.imuObj8.y		= MTI630Obj8.quaternion.y;
		exppackDataObj->imu_data.imuObj8.z		= MTI630Obj8.quaternion.z;
		exppackDataObj->imu_data.imuObj8.accX 	= MTI630Obj8.sensorData.accX;
		exppackDataObj->imu_data.imuObj8.accY 	= MTI630Obj8.sensorData.accY;
		exppackDataObj->imu_data.imuObj8.accZ 	= MTI630Obj8.sensorData.accZ;
		exppackDataObj->imu_data.imuObj8.gyrX 	= MTI630Obj8.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj8.gyrY 	= MTI630Obj8.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj8.gyrZ 	= MTI630Obj8.sensorData.gyrZ;
	}
#endif
#ifdef IMU_9_ENABLE
	if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj9.roll 	= MTI630Obj9.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj9.pitch 	= MTI630Obj9.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj9.yaw 	= MTI630Obj9.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj9.w 		= MTI630Obj9.quaternion.w;
		exppackDataObj->imu_data.imuObj9.x 		= MTI630Obj9.quaternion.x;
		exppackDataObj->imu_data.imuObj9.y		= MTI630Obj9.quaternion.y;
		exppackDataObj->imu_data.imuObj9.z		= MTI630Obj9.quaternion.z;
		exppackDataObj->imu_data.imuObj9.accX 	= MTI630Obj9.sensorData.accX;
		exppackDataObj->imu_data.imuObj9.accY 	= MTI630Obj9.sensorData.accY;
		exppackDataObj->imu_data.imuObj9.accZ 	= MTI630Obj9.sensorData.accZ;
		exppackDataObj->imu_data.imuObj9.gyrX 	= MTI630Obj9.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj9.gyrY 	= MTI630Obj9.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj9.gyrZ 	= MTI630Obj9.sensorData.gyrZ;
	}
#endif
#ifdef IMU_10_ENABLE
	if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
		exppackDataObj->imu_data.imuObj10.roll 	= MTI630Obj10.eulerAngle.roll;
		exppackDataObj->imu_data.imuObj10.pitch = MTI630Obj10.eulerAngle.pitch;
		exppackDataObj->imu_data.imuObj10.yaw 	= MTI630Obj10.eulerAngle.yaw;
		exppackDataObj->imu_data.imuObj10.w 	= MTI630Obj10.quaternion.w;
		exppackDataObj->imu_data.imuObj10.x 	= MTI630Obj10.quaternion.x;
		exppackDataObj->imu_data.imuObj10.y		= MTI630Obj10.quaternion.y;
		exppackDataObj->imu_data.imuObj10.z		= MTI630Obj10.quaternion.z;
		exppackDataObj->imu_data.imuObj10.accX 	= MTI630Obj10.sensorData.accX;
		exppackDataObj->imu_data.imuObj10.accY 	= MTI630Obj10.sensorData.accY;
		exppackDataObj->imu_data.imuObj10.accZ 	= MTI630Obj10.sensorData.accZ;
		exppackDataObj->imu_data.imuObj10.gyrX 	= MTI630Obj10.sensorData.gyrX;
		exppackDataObj->imu_data.imuObj10.gyrY 	= MTI630Obj10.sensorData.gyrY;
		exppackDataObj->imu_data.imuObj10.gyrZ 	= MTI630Obj10.sensorData.gyrZ;
	}
#endif
}
#endif



void CheckActivatedSensors(Exppack_Data_t* exppackDataObj)
{
	static uint8_t FSR_L1_NOTdetectedCnt = 0;
	static uint8_t FSR_L2_NOTdetectedCnt = 0;
	static uint8_t FSR_L3_NOTdetectedCnt = 0;
	static uint8_t FSR_L4_NOTdetectedCnt = 0;
	static uint8_t FSR_R1_NOTdetectedCnt = 0;
	static uint8_t FSR_R2_NOTdetectedCnt = 0;
	static uint8_t FSR_R3_NOTdetectedCnt = 0;
	static uint8_t FSR_R4_NOTdetectedCnt = 0;

	static uint8_t EMG_L1_NOTdetectedCnt = 0;
	static uint8_t EMG_L2_NOTdetectedCnt = 0;
	static uint8_t EMG_L3_NOTdetectedCnt = 0;
	static uint8_t EMG_L4_NOTdetectedCnt = 0;
	static uint8_t EMG_R1_NOTdetectedCnt = 0;
	static uint8_t EMG_R2_NOTdetectedCnt = 0;
	static uint8_t EMG_R3_NOTdetectedCnt = 0;
	static uint8_t EMG_R4_NOTdetectedCnt = 0;

	static uint8_t IMU_1_NOTdetectedCnt = 0;
	static uint8_t IMU_2_NOTdetectedCnt = 0;
	static uint8_t IMU_3_NOTdetectedCnt = 0;
	static uint8_t IMU_4_NOTdetectedCnt = 0;
	static uint8_t IMU_5_NOTdetectedCnt = 0;
	static uint8_t IMU_6_NOTdetectedCnt = 0;
	static uint8_t IMU_7_NOTdetectedCnt = 0;
	static uint8_t IMU_8_NOTdetectedCnt = 0;
	static uint8_t IMU_9_NOTdetectedCnt = 0;
	static uint8_t IMU_10_NOTdetectedCnt = 0;


	for (uint8_t i = 0; i < NO_DETECTION_CNT + 50; i++) {
		GetRawFSR(exppackDataObj);
		GetRawEMG(exppackDataObj);
		GetIMU(exppackDataObj);

		if (exppackDataObj->fsr_data.fsr_L1_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_L1_NOTdetectedCnt++;
		}
		if (exppackDataObj->fsr_data.fsr_L2_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_L2_NOTdetectedCnt++;
		}
		if (exppackDataObj->fsr_data.fsr_L3_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_L3_NOTdetectedCnt++;
		}
		if (exppackDataObj->fsr_data.fsr_L4_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_L4_NOTdetectedCnt++;
		}
		if (exppackDataObj->fsr_data.fsr_R1_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_R1_NOTdetectedCnt++;
		}
		if (exppackDataObj->fsr_data.fsr_R2_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_R2_NOTdetectedCnt++;
		}
		if (exppackDataObj->fsr_data.fsr_R3_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_R3_NOTdetectedCnt++;
		}
		if (exppackDataObj->fsr_data.fsr_R4_raw <= ADC_DETECTION_THRESHOLD) {
			FSR_R4_NOTdetectedCnt++;
		}

		if (exppackDataObj->emg_data.emg_L1_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_L1_NOTdetectedCnt++;
		}
		if (exppackDataObj->emg_data.emg_L2_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_L2_NOTdetectedCnt++;
		}
		if (exppackDataObj->emg_data.emg_L3_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_L3_NOTdetectedCnt++;
		}
		if (exppackDataObj->emg_data.emg_L4_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_L4_NOTdetectedCnt++;
		}
		if (exppackDataObj->emg_data.emg_R1_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_R1_NOTdetectedCnt++;
		}
		if (exppackDataObj->emg_data.emg_R2_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_R2_NOTdetectedCnt++;
		}
		if (exppackDataObj->emg_data.emg_R3_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_R3_NOTdetectedCnt++;
		}
		if (exppackDataObj->emg_data.emg_R4_raw <= ADC_DETECTION_THRESHOLD) {
			EMG_R4_NOTdetectedCnt++;
		}

#ifdef IMU_1_ENABLE
		if (exppackDataObj->imu_data.imuObj1.w == 0 && exppackDataObj->imu_data.imuObj1.x == 0 && exppackDataObj->imu_data.imuObj1.y == 0 && exppackDataObj->imu_data.imuObj1.z == 0) {
			IMU_1_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj1.w > 1 || exppackDataObj->imu_data.imuObj1.w < -1 || exppackDataObj->imu_data.imuObj1.x > 1 || exppackDataObj->imu_data.imuObj1.x < -1 || exppackDataObj->imu_data.imuObj1.y > 1 || exppackDataObj->imu_data.imuObj1.y < -1 || exppackDataObj->imu_data.imuObj1.z > 1 || exppackDataObj->imu_data.imuObj1.z < -1) {
			IMU_1_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_2_ENABLE
		if (exppackDataObj->imu_data.imuObj2.w == 0 && exppackDataObj->imu_data.imuObj2.x == 0 && exppackDataObj->imu_data.imuObj2.y == 0 && exppackDataObj->imu_data.imuObj2.z == 0) {
			IMU_2_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj2.w > 1 || exppackDataObj->imu_data.imuObj2.w < -1 || exppackDataObj->imu_data.imuObj2.x > 1 || exppackDataObj->imu_data.imuObj2.x < -1 || exppackDataObj->imu_data.imuObj2.y > 1 || exppackDataObj->imu_data.imuObj2.y < -1 || exppackDataObj->imu_data.imuObj2.z > 1 || exppackDataObj->imu_data.imuObj2.z < -1) {
			IMU_2_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_3_ENABLE
		if (exppackDataObj->imu_data.imuObj3.w == 0 && exppackDataObj->imu_data.imuObj3.x == 0 && exppackDataObj->imu_data.imuObj3.y == 0 && exppackDataObj->imu_data.imuObj3.z == 0) {
			IMU_3_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj3.w > 1 || exppackDataObj->imu_data.imuObj3.w < -1 || exppackDataObj->imu_data.imuObj3.x > 1 || exppackDataObj->imu_data.imuObj3.x < -1 || exppackDataObj->imu_data.imuObj3.y > 1 || exppackDataObj->imu_data.imuObj3.y < -1 || exppackDataObj->imu_data.imuObj3.z > 1 || exppackDataObj->imu_data.imuObj3.z < -1) {
			IMU_3_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_4_ENABLE
		if (exppackDataObj->imu_data.imuObj4.w == 0 && exppackDataObj->imu_data.imuObj4.x == 0 && exppackDataObj->imu_data.imuObj4.y == 0 && exppackDataObj->imu_data.imuObj4.z == 0) {
			IMU_4_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj4.w > 1 || exppackDataObj->imu_data.imuObj4.w < -1 || exppackDataObj->imu_data.imuObj4.x > 1 || exppackDataObj->imu_data.imuObj4.x < -1 || exppackDataObj->imu_data.imuObj4.y > 1 || exppackDataObj->imu_data.imuObj4.y < -1 || exppackDataObj->imu_data.imuObj4.z > 1 || exppackDataObj->imu_data.imuObj4.z < -1) {
			IMU_4_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_5_ENABLE
		if (exppackDataObj->imu_data.imuObj5.w == 0 && exppackDataObj->imu_data.imuObj5.x == 0 && exppackDataObj->imu_data.imuObj5.y == 0 && exppackDataObj->imu_data.imuObj5.z == 0) {
			IMU_5_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj5.w > 1 || exppackDataObj->imu_data.imuObj5.w < -1 || exppackDataObj->imu_data.imuObj5.x > 1 || exppackDataObj->imu_data.imuObj5.x < -1 || exppackDataObj->imu_data.imuObj5.y > 1 || exppackDataObj->imu_data.imuObj5.y < -1 || exppackDataObj->imu_data.imuObj5.z > 1 || exppackDataObj->imu_data.imuObj5.z < -1) {
			IMU_5_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_6_ENABLE
		if (exppackDataObj->imu_data.imuObj6.w == 0 && exppackDataObj->imu_data.imuObj6.x == 0 && exppackDataObj->imu_data.imuObj6.y == 0 && exppackDataObj->imu_data.imuObj6.z == 0) {
			IMU_6_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj6.w > 1 || exppackDataObj->imu_data.imuObj6.w < -1 || exppackDataObj->imu_data.imuObj6.x > 1 || exppackDataObj->imu_data.imuObj6.x < -1 || exppackDataObj->imu_data.imuObj6.y > 1 || exppackDataObj->imu_data.imuObj6.y < -1 || exppackDataObj->imu_data.imuObj6.z > 1 || exppackDataObj->imu_data.imuObj6.z < -1) {
			IMU_6_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_7_ENABLE
		if (exppackDataObj->imu_data.imuObj7.w == 0 && exppackDataObj->imu_data.imuObj7.x == 0 && exppackDataObj->imu_data.imuObj7.y == 0 && exppackDataObj->imu_data.imuObj7.z == 0) {
			IMU_7_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj7.w > 1 || exppackDataObj->imu_data.imuObj7.w < -1 || exppackDataObj->imu_data.imuObj7.x > 1 || exppackDataObj->imu_data.imuObj7.x < -1 || exppackDataObj->imu_data.imuObj7.y > 1 || exppackDataObj->imu_data.imuObj7.y < -1 || exppackDataObj->imu_data.imuObj7.z > 1 || exppackDataObj->imu_data.imuObj7.z < -1) {
			IMU_7_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_8_ENABLE
		if (exppackDataObj->imu_data.imuObj8.w == 0 && exppackDataObj->imu_data.imuObj8.x == 0 && exppackDataObj->imu_data.imuObj8.y == 0 && exppackDataObj->imu_data.imuObj8.z == 0) {
			IMU_8_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj8.w > 1 || exppackDataObj->imu_data.imuObj8.w < -1 || exppackDataObj->imu_data.imuObj8.x > 1 || exppackDataObj->imu_data.imuObj8.x < -1 || exppackDataObj->imu_data.imuObj8.y > 1 || exppackDataObj->imu_data.imuObj8.y < -1 || exppackDataObj->imu_data.imuObj8.z > 1 || exppackDataObj->imu_data.imuObj8.z < -1) {
			IMU_8_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_9_ENABLE
		if (exppackDataObj->imu_data.imuObj9.w == 0 && exppackDataObj->imu_data.imuObj9.x == 0 && exppackDataObj->imu_data.imuObj9.y == 0 && exppackDataObj->imu_data.imuObj9.z == 0) {
			IMU_9_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj9.w > 1 || exppackDataObj->imu_data.imuObj9.w < -1 || exppackDataObj->imu_data.imuObj9.x > 1 || exppackDataObj->imu_data.imuObj9.x < -1 || exppackDataObj->imu_data.imuObj9.y > 1 || exppackDataObj->imu_data.imuObj9.y < -1 || exppackDataObj->imu_data.imuObj9.z > 1 || exppackDataObj->imu_data.imuObj9.z < -1) {
			IMU_9_NOTdetectedCnt++;
		}
#endif
#ifdef IMU_10_ENABLE
		if (exppackDataObj->imu_data.imuObj10.w == 0 && exppackDataObj->imu_data.imuObj10.x == 0 && exppackDataObj->imu_data.imuObj10.y == 0 && exppackDataObj->imu_data.imuObj10.z == 0) {
			IMU_10_NOTdetectedCnt++;
		}
		else if (exppackDataObj->imu_data.imuObj10.w > 1 || exppackDataObj->imu_data.imuObj10.w < -1 || exppackDataObj->imu_data.imuObj10.x > 1 || exppackDataObj->imu_data.imuObj10.x < -1 || exppackDataObj->imu_data.imuObj10.y > 1 || exppackDataObj->imu_data.imuObj10.y < -1 || exppackDataObj->imu_data.imuObj10.z > 1 || exppackDataObj->imu_data.imuObj10.z < -1) {
			IMU_10_NOTdetectedCnt++;
		}
#endif
	}

	/* Final Check */
	if (FSR_L1_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_L1_detected = SENSOR_NOT_DETECTED;
	}
	if (FSR_L2_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_L2_detected = SENSOR_NOT_DETECTED;
	}
	if (FSR_L3_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_L3_detected = SENSOR_NOT_DETECTED;
	}
	if (FSR_L4_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_L4_detected = SENSOR_NOT_DETECTED;
	}
	if (FSR_R1_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_R1_detected = SENSOR_NOT_DETECTED;
	}
	if (FSR_R2_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_R2_detected = SENSOR_NOT_DETECTED;
	}
	if (FSR_R3_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_R3_detected = SENSOR_NOT_DETECTED;
	}
	if (FSR_R4_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.FSR_R4_detected = SENSOR_NOT_DETECTED;
	}

	if (EMG_L1_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_L1_detected = SENSOR_NOT_DETECTED;
	}
	if (EMG_L2_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_L2_detected = SENSOR_NOT_DETECTED;
	}
	if (EMG_L3_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_L3_detected = SENSOR_NOT_DETECTED;
	}
	if (EMG_L4_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_L4_detected = SENSOR_NOT_DETECTED;
	}
	if (EMG_R1_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_R1_detected = SENSOR_NOT_DETECTED;
	}
	if (EMG_R2_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_R2_detected = SENSOR_NOT_DETECTED;
	}
	if (EMG_R3_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_R3_detected = SENSOR_NOT_DETECTED;
	}
	if (EMG_R4_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.EMG_R4_detected = SENSOR_NOT_DETECTED;
	}

#ifdef IMU_1_ENABLE
	if (IMU_1_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_1_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_2_ENABLE
	if (IMU_2_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_2_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_3_ENABLE
	if (IMU_3_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_3_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_4_ENABLE
	if (IMU_4_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_4_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_5_ENABLE
	if (IMU_5_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_5_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_6_ENABLE
	if (IMU_6_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_6_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_7_ENABLE
	if (IMU_7_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_7_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_8_ENABLE
	if (IMU_8_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_8_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_9_ENABLE
	if (IMU_9_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_9_detected = SENSOR_NOT_DETECTED;
	}
#endif
#ifdef IMU_10_ENABLE
	if (IMU_10_NOTdetectedCnt > NO_DETECTION_CNT) {
		exppackDataObj->sensor_detection.IMU_10_detected = SENSOR_NOT_DETECTED;
	}
#endif


	// Forced Activation of certain sensors //
	ForcedActivation(exppackDataObj);

	// Forced Deactivation of certain sensors //
	ForcedDeactivation(exppackDataObj);

	// Reset ALL Counter for Next Sensor Detection //
	FSR_L1_NOTdetectedCnt = 0;
	FSR_L2_NOTdetectedCnt = 0;
	FSR_L3_NOTdetectedCnt = 0;
	FSR_L4_NOTdetectedCnt = 0;
	FSR_R1_NOTdetectedCnt = 0;
	FSR_R2_NOTdetectedCnt = 0;
	FSR_R3_NOTdetectedCnt = 0;
	FSR_R4_NOTdetectedCnt = 0;

	EMG_L1_NOTdetectedCnt = 0;
	EMG_L2_NOTdetectedCnt = 0;
	EMG_L3_NOTdetectedCnt = 0;
	EMG_L4_NOTdetectedCnt = 0;
	EMG_R1_NOTdetectedCnt = 0;
	EMG_R2_NOTdetectedCnt = 0;
	EMG_R3_NOTdetectedCnt = 0;
	EMG_R4_NOTdetectedCnt = 0;

	IMU_1_NOTdetectedCnt = 0;
	IMU_2_NOTdetectedCnt = 0;
	IMU_3_NOTdetectedCnt = 0;
	IMU_4_NOTdetectedCnt = 0;
	IMU_5_NOTdetectedCnt = 0;
	IMU_6_NOTdetectedCnt = 0;
	IMU_7_NOTdetectedCnt = 0;
	IMU_8_NOTdetectedCnt = 0;
	IMU_9_NOTdetectedCnt = 0;
	IMU_10_NOTdetectedCnt = 0;


	/* Filter Out the Not Detected Sensor Data from USB_CDC PROTOCOL */
	for (uint8_t i = 0; i < (sizeof(usbTxDataSet) / sizeof(usbTxDataSet[0])); i++) {
#ifdef IMU_QUATERNION_GYR_ACC_VERSION
		if ( exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU1_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU1_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU1_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU1_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU1_GYR_X || usbTxDataSet[i].dataName == DS_IMU1_GYR_Y || usbTxDataSet[i].dataName == DS_IMU1_GYR_Z || usbTxDataSet[i].dataName == DS_IMU1_ACC_X || usbTxDataSet[i].dataName == DS_IMU1_ACC_Y || usbTxDataSet[i].dataName == DS_IMU1_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU2_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU2_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU2_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU2_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU2_GYR_X || usbTxDataSet[i].dataName == DS_IMU2_GYR_Y || usbTxDataSet[i].dataName == DS_IMU2_GYR_Z || usbTxDataSet[i].dataName == DS_IMU2_ACC_X || usbTxDataSet[i].dataName == DS_IMU2_ACC_Y || usbTxDataSet[i].dataName == DS_IMU2_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU3_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU3_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU3_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU3_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU3_GYR_X || usbTxDataSet[i].dataName == DS_IMU3_GYR_Y || usbTxDataSet[i].dataName == DS_IMU3_GYR_Z || usbTxDataSet[i].dataName == DS_IMU3_ACC_X || usbTxDataSet[i].dataName == DS_IMU3_ACC_Y || usbTxDataSet[i].dataName == DS_IMU3_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU4_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU4_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU4_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU4_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU4_GYR_X || usbTxDataSet[i].dataName == DS_IMU4_GYR_Y || usbTxDataSet[i].dataName == DS_IMU4_GYR_Z || usbTxDataSet[i].dataName == DS_IMU4_ACC_X || usbTxDataSet[i].dataName == DS_IMU4_ACC_Y || usbTxDataSet[i].dataName == DS_IMU4_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU5_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU5_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU5_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU5_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU5_GYR_X || usbTxDataSet[i].dataName == DS_IMU5_GYR_Y || usbTxDataSet[i].dataName == DS_IMU5_GYR_Z || usbTxDataSet[i].dataName == DS_IMU5_ACC_X || usbTxDataSet[i].dataName == DS_IMU5_ACC_Y || usbTxDataSet[i].dataName == DS_IMU5_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU6_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU6_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU6_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU6_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU6_GYR_X || usbTxDataSet[i].dataName == DS_IMU6_GYR_Y || usbTxDataSet[i].dataName == DS_IMU6_GYR_Z || usbTxDataSet[i].dataName == DS_IMU6_ACC_X || usbTxDataSet[i].dataName == DS_IMU6_ACC_Y || usbTxDataSet[i].dataName == DS_IMU6_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU7_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU7_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU7_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU7_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU7_GYR_X || usbTxDataSet[i].dataName == DS_IMU7_GYR_Y || usbTxDataSet[i].dataName == DS_IMU7_GYR_Z || usbTxDataSet[i].dataName == DS_IMU7_ACC_X || usbTxDataSet[i].dataName == DS_IMU7_ACC_Y || usbTxDataSet[i].dataName == DS_IMU7_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU8_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU8_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU8_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU8_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU8_GYR_X || usbTxDataSet[i].dataName == DS_IMU8_GYR_Y || usbTxDataSet[i].dataName == DS_IMU8_GYR_Z || usbTxDataSet[i].dataName == DS_IMU8_ACC_X || usbTxDataSet[i].dataName == DS_IMU8_ACC_Y || usbTxDataSet[i].dataName == DS_IMU8_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU9_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU9_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU9_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU9_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU9_GYR_X || usbTxDataSet[i].dataName == DS_IMU9_GYR_Y || usbTxDataSet[i].dataName == DS_IMU9_GYR_Z || usbTxDataSet[i].dataName == DS_IMU9_ACC_X || usbTxDataSet[i].dataName == DS_IMU9_ACC_Y || usbTxDataSet[i].dataName == DS_IMU9_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU10_QUATERNION_W || usbTxDataSet[i].dataName == DS_IMU10_QUATERNION_X || usbTxDataSet[i].dataName == DS_IMU10_QUATERNION_Y || usbTxDataSet[i].dataName == DS_IMU10_QUATERNION_Z || usbTxDataSet[i].dataName == DS_IMU10_GYR_X || usbTxDataSet[i].dataName == DS_IMU10_GYR_Y || usbTxDataSet[i].dataName == DS_IMU10_GYR_Z || usbTxDataSet[i].dataName == DS_IMU10_ACC_X || usbTxDataSet[i].dataName == DS_IMU10_ACC_Y || usbTxDataSet[i].dataName == DS_IMU10_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}
#endif

#ifdef IMU_EULER_GYR_ACC_VERSION
		if ( exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU1_EULER_X || usbTxDataSet[i].dataName == DS_IMU1_EULER_Y || usbTxDataSet[i].dataName == DS_IMU1_EULER_Z || usbTxDataSet[i].dataName == DS_IMU1_GYR_X || usbTxDataSet[i].dataName == DS_IMU1_GYR_Y || usbTxDataSet[i].dataName == DS_IMU1_GYR_Z || usbTxDataSet[i].dataName == DS_IMU1_ACC_X || usbTxDataSet[i].dataName == DS_IMU1_ACC_Y || usbTxDataSet[i].dataName == DS_IMU1_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU2_EULER_X || usbTxDataSet[i].dataName == DS_IMU2_EULER_Y || usbTxDataSet[i].dataName == DS_IMU2_EULER_Z || usbTxDataSet[i].dataName == DS_IMU2_GYR_X || usbTxDataSet[i].dataName == DS_IMU2_GYR_Y || usbTxDataSet[i].dataName == DS_IMU2_GYR_Z || usbTxDataSet[i].dataName == DS_IMU2_ACC_X || usbTxDataSet[i].dataName == DS_IMU2_ACC_Y || usbTxDataSet[i].dataName == DS_IMU2_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU3_EULER_X || usbTxDataSet[i].dataName == DS_IMU3_EULER_Y || usbTxDataSet[i].dataName == DS_IMU3_EULER_Z || usbTxDataSet[i].dataName == DS_IMU3_GYR_X || usbTxDataSet[i].dataName == DS_IMU3_GYR_Y || usbTxDataSet[i].dataName == DS_IMU3_GYR_Z || usbTxDataSet[i].dataName == DS_IMU3_ACC_X || usbTxDataSet[i].dataName == DS_IMU3_ACC_Y || usbTxDataSet[i].dataName == DS_IMU3_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU4_EULER_X || usbTxDataSet[i].dataName == DS_IMU4_EULER_Y || usbTxDataSet[i].dataName == DS_IMU4_EULER_Z || usbTxDataSet[i].dataName == DS_IMU4_GYR_X || usbTxDataSet[i].dataName == DS_IMU4_GYR_Y || usbTxDataSet[i].dataName == DS_IMU4_GYR_Z || usbTxDataSet[i].dataName == DS_IMU4_ACC_X || usbTxDataSet[i].dataName == DS_IMU4_ACC_Y || usbTxDataSet[i].dataName == DS_IMU4_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU5_EULER_X || usbTxDataSet[i].dataName == DS_IMU5_EULER_Y || usbTxDataSet[i].dataName == DS_IMU5_EULER_Z || usbTxDataSet[i].dataName == DS_IMU5_GYR_X || usbTxDataSet[i].dataName == DS_IMU5_GYR_Y || usbTxDataSet[i].dataName == DS_IMU5_GYR_Z || usbTxDataSet[i].dataName == DS_IMU5_ACC_X || usbTxDataSet[i].dataName == DS_IMU5_ACC_Y || usbTxDataSet[i].dataName == DS_IMU5_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU6_EULER_X || usbTxDataSet[i].dataName == DS_IMU6_EULER_Y || usbTxDataSet[i].dataName == DS_IMU6_EULER_Z || usbTxDataSet[i].dataName == DS_IMU6_GYR_X || usbTxDataSet[i].dataName == DS_IMU6_GYR_Y || usbTxDataSet[i].dataName == DS_IMU6_GYR_Z || usbTxDataSet[i].dataName == DS_IMU6_ACC_X || usbTxDataSet[i].dataName == DS_IMU6_ACC_Y || usbTxDataSet[i].dataName == DS_IMU6_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU7_EULER_X || usbTxDataSet[i].dataName == DS_IMU7_EULER_Y || usbTxDataSet[i].dataName == DS_IMU7_EULER_Z || usbTxDataSet[i].dataName == DS_IMU7_GYR_X || usbTxDataSet[i].dataName == DS_IMU7_GYR_Y || usbTxDataSet[i].dataName == DS_IMU7_GYR_Z || usbTxDataSet[i].dataName == DS_IMU7_ACC_X || usbTxDataSet[i].dataName == DS_IMU7_ACC_Y || usbTxDataSet[i].dataName == DS_IMU7_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU8_EULER_X || usbTxDataSet[i].dataName == DS_IMU8_EULER_Y || usbTxDataSet[i].dataName == DS_IMU8_EULER_Z || usbTxDataSet[i].dataName == DS_IMU8_GYR_X || usbTxDataSet[i].dataName == DS_IMU8_GYR_Y || usbTxDataSet[i].dataName == DS_IMU8_GYR_Z || usbTxDataSet[i].dataName == DS_IMU8_ACC_X || usbTxDataSet[i].dataName == DS_IMU8_ACC_Y || usbTxDataSet[i].dataName == DS_IMU8_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU9_EULER_X || usbTxDataSet[i].dataName == DS_IMU9_EULER_Y || usbTxDataSet[i].dataName == DS_IMU9_EULER_Z || usbTxDataSet[i].dataName == DS_IMU9_GYR_X || usbTxDataSet[i].dataName == DS_IMU9_GYR_Y || usbTxDataSet[i].dataName == DS_IMU9_GYR_Z || usbTxDataSet[i].dataName == DS_IMU9_ACC_X || usbTxDataSet[i].dataName == DS_IMU9_ACC_Y || usbTxDataSet[i].dataName == DS_IMU9_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_NOT_DETECTED && (usbTxDataSet[i].dataName == DS_IMU10_EULER_X || usbTxDataSet[i].dataName == DS_IMU10_EULER_Y || usbTxDataSet[i].dataName == DS_IMU10_EULER_Z || usbTxDataSet[i].dataName == DS_IMU10_GYR_X || usbTxDataSet[i].dataName == DS_IMU10_GYR_Y || usbTxDataSet[i].dataName == DS_IMU10_GYR_Z || usbTxDataSet[i].dataName == DS_IMU10_ACC_X || usbTxDataSet[i].dataName == DS_IMU10_ACC_Y || usbTxDataSet[i].dataName == DS_IMU10_ACC_Z) ) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}
#endif

#ifdef EMG_NORM_VERSION
		if ( exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL1_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL2_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL3_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL4_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR1_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR2_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR3_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR4_NORM) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}
#endif
#ifdef EMG_RAW_VERSION
		if ( exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL1_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL2_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL3_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGL4_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR1_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR2_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR3_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_EMGR4_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}
#endif

		if ( exppackDataObj->sensor_detection.FSR_L1_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRL1_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.FSR_L2_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRL2_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.FSR_L3_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRL3_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.FSR_L4_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRL4_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.FSR_R1_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRR1_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.FSR_R2_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRR2_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.FSR_R3_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRR3_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}

		if ( exppackDataObj->sensor_detection.FSR_R4_detected == SENSOR_NOT_DETECTED && usbTxDataSet[i].dataName == DS_FSRR4_RAW) {
			usbTxDataSet[i].dataName = DS_NOT_DETECTED_SENSOR;
		}
	}
}


/* Manually selects the sensors YOU want to activate */
void ForcedActivation(Exppack_Data_t* exppackDataObj)
{
	exppackDataObj->sensor_detection.FSR_L1_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.FSR_L2_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.FSR_L3_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.FSR_L4_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.FSR_R1_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.FSR_R2_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.FSR_R3_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.FSR_R4_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_L1_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_L2_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_L3_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_L4_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_R1_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_R2_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_R3_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.EMG_R4_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.IMU_1_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_2_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_3_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_4_detected = SENSOR_DETECTED;
	exppackDataObj->sensor_detection.IMU_5_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_6_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_7_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_8_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_9_detected = SENSOR_DETECTED;
//	exppackDataObj->sensor_detection.IMU_10_detected = SENSOR_DETECTED;
}



/* Manually selects the sensors YOU want to deactivate */
void ForcedDeactivation(Exppack_Data_t* exppackDataObj)
{
//	exppackDataObj->sensor_detection.FSR_L1_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.FSR_L2_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.FSR_L3_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.FSR_L4_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.FSR_R1_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.FSR_R2_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.FSR_R3_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.FSR_R4_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_L1_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_L2_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_L3_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_L4_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_R1_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_R2_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_R3_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.EMG_R4_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.IMU_1_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_2_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_3_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_4_detected = SENSOR_NOT_DETECTED;
//	exppackDataObj->sensor_detection.IMU_5_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_6_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_7_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_8_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_9_detected = SENSOR_NOT_DETECTED;
	exppackDataObj->sensor_detection.IMU_10_detected = SENSOR_NOT_DETECTED;
}




#ifdef USB_CDC_ACTIVATE
/*---------------------------------------------------------------------- USB_CDC communication ----------------------------------------------------------------------*/
/*---------------------------------------------------------------------- [Data Protocol] ----------------------------------------------------------------------*/
// PACKET: (0xAB)->(dataName1)->(originalDataType1)->(scaledDataType1)->(dataName2)->(originalDataType2)->(scaledDataType2)->...->(dataNameN)->(originalDataTypeN)->(scaledDataTypeN)->(totalDataSize)->(0xAD) //

static void ResetUSBCDCTxBuffer(void)
{
	memset(usbTxBuf, 0, sizeof(usbTxBuf));
	usbTxBufCursor = 0;
}


static void AppendUSBCDCTxDataProtocol(void)
{
	totalUSBCDCTxDataSize = 0;

	for (uint8_t i = 0; i < (sizeof(usbTxDataSet) / sizeof(usbTxDataSet[0])); i++) {

		if (usbTxDataSet[i].dataName == DS_NOT_DETECTED_SENSOR) {
			continue;			// Not Detected Sensor -> Neglect
		}
		usbTxBuf[usbTxBufCursor++] = usbTxDataSet[i].dataName;
		usbTxBuf[usbTxBufCursor++] = usbTxDataSet[i].originalDataType;
		usbTxBuf[usbTxBufCursor++] = usbTxDataSet[i].scaledDataType;

		if ((usbTxDataSet[i].scaledDataType == DT_UINT8) || (usbTxDataSet[i].scaledDataType == DT_INT8)) {
			totalUSBCDCTxDataSize += 1;
		}
		else if ((usbTxDataSet[i].scaledDataType == DT_UINT16) || (usbTxDataSet[i].scaledDataType == DT_INT16)) {
			totalUSBCDCTxDataSize += 2;
		}
		else if ((usbTxDataSet[i].scaledDataType == DT_UINT32) || (usbTxDataSet[i].scaledDataType == DT_INT32)) {
			totalUSBCDCTxDataSize += 4;
		}
	}
}


static void SendUSBCDCTxDataProtocol(void)
{
	ResetUSBCDCTxBuffer();

	/* Add SOL */
	usbTxBuf[usbTxBufCursor++] = USB_CDC_PROTOCOL_DATA;

	/* Append ALL information about Tx DataSet */
	AppendUSBCDCTxDataProtocol();

	/* Add TOTAL TxDataByteSize except for SOL */
	usbTxBuf[usbTxBufCursor++] = totalUSBCDCTxDataSize;

	/* Add EOL */
	usbTxBuf[usbTxBufCursor++] = USB_CDC_END_DATA;

	/* Calculate Buffer size & Send data */
	usbTxBufSize = usbTxBufCursor;
	usbTxUpdate = 1;
}



/*----------------------------------------------------------------- [Continuous Data Sending] -----------------------------------------------------------------*/
// PACKET: (0xAC)->(data1)->(data2)->(data3)->...->(dataN)->(totalDataSize)->(0xAD) //

static void AppendUSBCDCTxSOL(void) {
	usbTxBufCursor = 0;
	usbTxBuf[usbTxBufCursor++] = USB_CDC_START_DATA;
}


static void AppendUSBCDCTxData(void* dataPtr, DataType_t originalDataType, DataType_t scaledDataType, uint32_t scalingFactor)
{
	switch (originalDataType) {
		case DT_UINT8:
			uint8_t tempVar_1 = *(uint8_t*)dataPtr;
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_1);
			break;
		case DT_UINT16:
			uint16_t tempVar_2 = *(uint16_t*)dataPtr;
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_2 >> 8);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_2 & 0xFF);
			break;
		case DT_UINT32:
			uint32_t tempVar_3 = *(uint32_t*)dataPtr;
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_3 >> 24);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_3 >> 16);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_3 >> 8);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_3 & 0xFF);
			break;
		case DT_INT8:
			int8_t tempVar_4 = *(int8_t*)dataPtr;
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_4);
			break;
		case DT_INT16:
			uint16_t tempVar_5 = *(uint16_t*)dataPtr;
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_5 >> 8);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_5 & 0xFF);
			break;
		case DT_INT32:
			uint32_t tempVar_6 = *(uint32_t*)dataPtr;
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_6 >> 24);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_6 >> 16);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_6 >> 8);
			usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_6 & 0xFF);
			break;
		case DT_FLOAT32:
			if (scaledDataType == DT_UINT16) {
				uint16_t tempVar_7 = (uint16_t)(ScaleFloatToUInt16(*(float*)(dataPtr), scalingFactor));
				usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_7 >> 8);
				usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_7 & 0xFF);
			}
			else if (scaledDataType == DT_INT16) {
				int16_t tempVar_8 = (int16_t)(ScaleFloatToInt16(*(float*)(dataPtr), scalingFactor));
				usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_8 >> 8);
				usbTxBuf[usbTxBufCursor++] = (uint8_t)(tempVar_8 & 0xFF);
			}
			break;

		default:
			return;		// Terminate the whole function here
	}

//	totalUSBCDCTxDataSize = (usbTxBufCursor - 1);
}


static void GetCheckSum_USBCDC(void)
{
	checkSum = 0;

	for (uint8_t i = 1; i < usbTxBufCursor; i++) {
		checkSum += usbTxBuf[i];
	}

	checkSumMSB = (uint8_t)((checkSum & (0xFF00)) >> 8);
	checkSumLSB = (uint8_t)(checkSum & 0x00FF);

	usbTxBuf[usbTxBufCursor++] = checkSumMSB;
	usbTxBuf[usbTxBufCursor++] = checkSumLSB;
}


static void EndCMD_USBCDCTxData(void)
{
	/* Add EOL in the Last part of Tx Buffer */
	usbTxBuf[usbTxBufCursor] = USB_CDC_END_DATA;

	/* Calculate Buffer size & Send data */
	usbTxBufSize = (usbTxBufCursor + 1);
	usbTxUpdate = 1;
}



/* For UART TEST code */
static void PrepareUSBCDCTxData_TEST(Exppack_Data_t* exppackDataObj)
{
	/* Reset the buffers */
	ResetUSBCDCTxBuffer();

	/* Add SOL in the start of data */
	AppendUSBCDCTxSOL();

	/* Assign the Data to send through USB CDC FS [Only Change This] */
	for (uint8_t i = 0; i < (uint8_t)(sizeof(usbTxDataSet) / sizeof(usbTxDataSet[0])); i++) {
		DataSet_t targetData = usbTxDataSet[i].dataName;
		DataType_t originalType = usbTxDataSet[i].originalDataType;
		DataType_t scaledType = usbTxDataSet[i].scaledDataType;

		switch (targetData) {
			/* Time */
			case (DS_TIMESTAMP):
				AppendUSBCDCTxData(&timeUSBCDC, originalType, scaledType, TIMESTAMP_SCALING_FACTOR);
				break;

			/* IMU */
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU1_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU1_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU1_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU1_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU2_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU2_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU2_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU2_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU3_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU3_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU3_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU3_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU4_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU4_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU4_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU4_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU5_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU5_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU5_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU5_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_5_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj5.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU6_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU6_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU6_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU6_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_6_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj6.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU7_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU7_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU7_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU7_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_7_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj7.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU8_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU8_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU8_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU8_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_8_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj8.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU9_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU9_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU9_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU9_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_9_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj9.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
#if defined (IMU_QUATERNION_VERSION) || defined (IMU_QUATERNION_GYR_ACC_VERSION)
			case (DS_IMU10_QUATERNION_W):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.w, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_QUATERNION_X):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.x, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_QUATERNION_Y):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.y, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_QUATERNION_Z):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.z, originalType, scaledType, IMU_QUATERNION_SCALING_FACTOR);
				}
				break;
#endif
#if defined (IMU_EULER_GYR_ACC_VERSION)
			case (DS_IMU10_EULER_X):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.roll, originalType, scaledType, IMU_EULER_ROLL_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_EULER_Y):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.pitch, originalType, scaledType, IMU_EULER_PITCH_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_EULER_Z):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.yaw, originalType, scaledType, IMU_EULER_YAW_SCALING_FACTOR);
				}
				break;
#endif
			case (DS_IMU10_GYR_X):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.gyrX, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_GYR_Y):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.gyrY, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_GYR_Z):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.gyrZ, originalType, scaledType, IMU_GYR_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_ACC_X):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.accX, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_ACC_Y):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.accY, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;
			case (DS_IMU10_ACC_Z):
				if (exppackDataObj->sensor_detection.IMU_10_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj10.accZ, originalType, scaledType, IMU_ACC_SCALING_FACTOR);
				}
				break;


				/* EMG */
#ifdef EMG_NORM_VERSION
			case (DS_EMGL1_NORM):
				if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L1_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
			case (DS_EMGL2_NORM):
				if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L2_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
			case (DS_EMGL3_NORM):
				if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L3_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
			case (DS_EMGL4_NORM):
				if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L4_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR1_NORM):
				if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R1_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR2_NORM):
				if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R2_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR3_NORM):
				if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R3_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR4_NORM):
				if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R4_norm, originalType, scaledType, EMG_NORM_SCALING_FACTOR);
				}
				break;
#endif
#ifdef EMG_RAW_VERSION
			case (DS_EMGL1_RAW):
				if (exppackDataObj->sensor_detection.EMG_L1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L1_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_EMGL2_RAW):
				if (exppackDataObj->sensor_detection.EMG_L2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L2_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_EMGL3_RAW):
				if (exppackDataObj->sensor_detection.EMG_L3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L3_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_EMGL4_RAW):
				if (exppackDataObj->sensor_detection.EMG_L4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L4_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR1_RAW):
				if (exppackDataObj->sensor_detection.EMG_R1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R1_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR2_RAW):
				if (exppackDataObj->sensor_detection.EMG_R2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R2_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR3_RAW):
				if (exppackDataObj->sensor_detection.EMG_R3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R3_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_EMGR4_RAW):
				if (exppackDataObj->sensor_detection.EMG_R4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R4_raw_send, originalType, scaledType, EMG_RAW_12BIT_SCALING_FACTOR);
				}
				break;
#endif
			/* FSR */
			case (DS_FSRL1_RAW):
				if (exppackDataObj->sensor_detection.FSR_L1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_L1_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_FSRL2_RAW):
				if (exppackDataObj->sensor_detection.FSR_L2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_L2_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_FSRL3_RAW):
				if (exppackDataObj->sensor_detection.FSR_L3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_L3_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_FSRL4_RAW):
				if (exppackDataObj->sensor_detection.FSR_L4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_L4_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_FSRR1_RAW):
				if (exppackDataObj->sensor_detection.FSR_R1_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_R1_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_FSRR2_RAW):
				if (exppackDataObj->sensor_detection.FSR_R2_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_R2_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_FSRR3_RAW):
				if (exppackDataObj->sensor_detection.FSR_R3_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_R3_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;
			case (DS_FSRR4_RAW):
				if (exppackDataObj->sensor_detection.FSR_R4_detected == SENSOR_DETECTED) {
					AppendUSBCDCTxData(&exppackDataObj->fsr_data.fsr_R4_raw_send, originalType, scaledType, FSR_RAW_12BIT_SCALING_FACTOR);
				}
				break;

#ifdef CM_CONNECT_MODE
			/* CM & MD */
			case (DS_CM_INC_POS_RH):
				AppendUSBCDCTxData(&CMDataObj.theta_RH, originalType, scaledType, DEG_SCALING_FACTOR);
				break;
			case (DS_CM_INC_POS_LH):
				AppendUSBCDCTxData(&CMDataObj.theta_LH, originalType, scaledType, DEG_SCALING_FACTOR);
				break;
			case (DS_CM_CURRENT_RH):
				AppendUSBCDCTxData(&CMDataObj.current_RH, originalType, scaledType, CURRENT_RANGE_MAX);
				break;
			case (DS_CM_CURRENT_LH):
				AppendUSBCDCTxData(&CMDataObj.current_LH, originalType, scaledType, CURRENT_RANGE_MAX);
				break;
#endif
#ifdef FP_SYNC_MODE
			case (DS_FP_SYNC):
				AppendUSBCDCTxData(&fp_sync, originalType, scaledType, 1);
				break;
#endif
			default:
				break;
		}
	}

//	AppendUSBCDCTxData(&timeUSBCDC, DT_UINT32, DT_UINT32, TIMESTAMP_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.w, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.x, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.y, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj1.z, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.w, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.x, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.y, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj2.z, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.w, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.x, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.y, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj3.z, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.w, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.x, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.y, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->imu_data.imuObj4.z, DT_FLOAT32, DT_INT16, IMU_QUATERNION_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L1_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L2_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L3_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_L4_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R1_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R2_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R3_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);
//	AppendUSBCDCTxData(&exppackDataObj->emg_data.emg_R4_norm, DT_FLOAT32, DT_UINT16, EMG_NORM_SCALING_FACTOR);

	/* Add EOL in the end of data */
	EndCMD_USBCDCTxData();
}

static void TerminateUSBCDC(void)
{
	/* Reset the buffers */
	memset(usbTxBuf, 0, sizeof(usbTxBuf));

	/* Assign the Terminate Data to send through USB CDC FS */
	uint8_t terminateData = USB_CDC_TERMINATE_PYTHON;
	usbTxBuf[0] = terminateData;

	totalUSBCDCTxDataSize = 0;

	usbTxBufSize = 1;

	usbTxUpdate = 1;
}


#endif


/*---------------------------------------------------------- BUTTON EXTI CALLBACK ----------------------------------------------------------*/

static void GPIO_EXTI_8_CALLBACK(uint16_t gpioPins)
{
	uint16_t DebounceCheckCnt = 500 / ((TIMER_INTERRUPT_PERIOD + 1) / 1000);

	/* EXTI operation */
	if (gpioPins == buttonPin && GPIO_EXTI_FLAG == BUTTON_STATE_IDLE && DEBOUNCE_CNT > DebounceCheckCnt) {
		DEBOUNCE_CNT = 0;
		GPIO_EXTI_FLAG = BUTTON_STATE_SENSOR_DETECTION;
	}
	else if (gpioPins == buttonPin && GPIO_EXTI_FLAG == BUTTON_STATE_SENSOR_DETECTION && DEBOUNCE_CNT > DebounceCheckCnt) {
		DEBOUNCE_CNT = 0;
		GPIO_EXTI_FLAG = BUTTON_STATE_SEND_PROTOCOL;
	}
	else if (gpioPins == buttonPin && GPIO_EXTI_FLAG == BUTTON_STATE_AFTER_PROTOCOL && DEBOUNCE_CNT > DebounceCheckCnt) {
		timeUSBCDC = 0;
		DEBOUNCE_CNT = 0;
		GPIO_EXTI_FLAG = BUTTON_STATE_CONT_TX_START;
	}
	else if (gpioPins == buttonPin && GPIO_EXTI_FLAG == BUTTON_STATE_CONT_TX_START && DEBOUNCE_CNT > DebounceCheckCnt) {
		timeUSBCDC = 0;
		exppackCtrlLoopCnt = 0;
		DEBOUNCE_CNT = 0;
		GPIO_EXTI_FLAG = BUTTON_STATE_IDLE;

		#ifdef USB_CDC_ACTIVATE
			TerminateUSBCDC();
		#endif
		memset(&(ExpPackDataObj.sensor_detection), 0, sizeof(ExpPackDataObj.sensor_detection));				// Reset the sensor detection object

		BUZZER_STATE_FLAG = BUZZER_STATE_TERMINATION;

		StateTransition(&exppackCtrlTask.stateMachine, TASK_STATE_STANDBY);
	}
}


static void ButtonSequence(void)
{
	uint16_t buttonKeepingCnt = 2000 / ((TIMER_INTERRUPT_PERIOD + 1) / 1000);

	uint8_t buttonState = HAL_GPIO_ReadPin(GPIOE, buttonPin);			// Change this according to Button GPIO

	if (buttonState == GPIO_PIN_SET) {
		buttonHighAccumulation++;
	}
	else if (buttonState == GPIO_PIN_RESET && buttonStateToken == 1) {
		buttonHighAccumulation = 0;
		buttonLowAccumulation++;
		if (buttonLowAccumulation > 10) {
			buttonStateToken = 0;
			buttonLowAccumulation = 0;
		}
	}
	else {
		buttonHighAccumulation = 0;
	}


	if (buttonHighAccumulation > buttonKeepingCnt && buttonStateToken == 0) {
		if (GPIO_EXTI_FLAG == BUTTON_STATE_IDLE) {
			DEBOUNCE_CNT = 0;
			buttonHighAccumulation = 0;
			buttonStateToken = 1;
#ifdef CM_CONNECT_MODE
			Send_ExtensionBoardEnable();	// For CM
#endif
			GPIO_EXTI_FLAG = BUTTON_STATE_SENSOR_DETECTION;
		}
		else if (GPIO_EXTI_FLAG == BUTTON_STATE_SENSOR_DETECTION) {
			DEBOUNCE_CNT = 0;
			buttonHighAccumulation = 0;
			buttonStateToken = 1;
			GPIO_EXTI_FLAG = BUTTON_STATE_SEND_PROTOCOL;
		}
		else if (GPIO_EXTI_FLAG == BUTTON_STATE_AFTER_PROTOCOL) {
			timeUSBCDC = 0;
			DEBOUNCE_CNT = 0;
			buttonHighAccumulation = 0;
			buttonStateToken = 1;
			GPIO_EXTI_FLAG = BUTTON_STATE_CONT_TX_START;
		}
		else if (GPIO_EXTI_FLAG == BUTTON_STATE_CONT_TX_START) {
			timeUSBCDC = 0;
			exppackCtrlLoopCnt = 0;
			DEBOUNCE_CNT = 0;
			buttonHighAccumulation = 0;
			buttonStateToken = 1;
			GPIO_EXTI_FLAG = BUTTON_STATE_IDLE;
			#ifdef USB_CDC_ACTIVATE
				TerminateUSBCDC();
			#endif
			memset(&(ExpPackDataObj.sensor_detection), 0, sizeof(ExpPackDataObj.sensor_detection));				// Reset the sensor detection object

			BUZZER_STATE_FLAG = BUZZER_STATE_TERMINATION;

			StateTransition(&exppackCtrlTask.stateMachine, TASK_STATE_STANDBY);
		}
	}

}

/*--------------------------------------------------------------- SDO CALLBACK ---------------------------------------------------------------*/

static void GetThetaRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
	memcpy(&ReceivedDataObj.theta_RH_scaled, req->data, 2);

	CMDataObj.theta_RH = ScaleInt16ToFloat(ReceivedDataObj.theta_RH_scaled, DEG_SCALING_FACTOR);

    res->dataSize = 0;
    res->status = DOP_SDO_SUCC;
}

static void GetThetaLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
	memcpy(&ReceivedDataObj.theta_LH_scaled, req->data, 2);

	CMDataObj.theta_LH = ScaleInt16ToFloat(ReceivedDataObj.theta_LH_scaled, DEG_SCALING_FACTOR);

    res->dataSize = 0;
    res->status = DOP_SDO_SUCC;
}

static void Get_SUIT_State_curr(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
	ReceivedDataObj.SUIT_state_prev = SUIT_State_curr;
	memcpy(&ReceivedDataObj.SUIT_state_curr, req->data, 1);

	SUIT_State_curr = ReceivedDataObj.SUIT_state_curr;

	res->dataSize = 0;
	res->status = DOP_SDO_SUCC;
}

static void GetCurrentRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
	memcpy(&ReceivedDataObj.current_RH_scaled, req->data, 2);

	CMDataObj.current_RH = ScaleInt16ToFloat(ReceivedDataObj.current_RH_scaled, CURRENT_RANGE_MAX);

    res->dataSize = 0;
    res->status = DOP_SDO_SUCC;
}

static void GetCurrentLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
	memcpy(&ReceivedDataObj.current_LH_scaled, req->data, 2);

	CMDataObj.current_LH = ScaleInt16ToFloat(ReceivedDataObj.current_LH_scaled, CURRENT_RANGE_MAX);

    res->dataSize = 0;
    res->status = DOP_SDO_SUCC;
}


//static void GetAccXRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
//{
//	memcpy(&ReceivedDataObj.accX_RH_scaled, req->data, 2);
//
//	StudentsDataObj.accX_RH = ScaleInt16ToFloat(ReceivedDataObj.accX_RH_scaled, ACC_SCALING_FACTOR);
//
//    res->dataSize = 0;
//    res->status = DOP_SDO_SUCC;
//}
//
//static void GetAccXLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
//{
//	memcpy(&ReceivedDataObj.accX_LH_scaled, req->data, 2);
//
//	StudentsDataObj.accX_LH = ScaleInt16ToFloat(ReceivedDataObj.accX_LH_scaled, ACC_SCALING_FACTOR);
//
//    res->dataSize = 0;
//    res->status = DOP_SDO_SUCC;
//}
//
//static void GetAccYRH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
//{
//	memcpy(&ReceivedDataObj.accY_RH_scaled, req->data, 2);
//
//	StudentsDataObj.accY_RH = ScaleInt16ToFloat(ReceivedDataObj.accY_RH_scaled, ACC_SCALING_FACTOR);
//
//    res->dataSize = 0;
//    res->status = DOP_SDO_SUCC;
//}
//
//static void GetAccYLH(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
//{
//	memcpy(&ReceivedDataObj.accY_LH_scaled, req->data, 2);
//
//	StudentsDataObj.accY_LH = ScaleInt16ToFloat(ReceivedDataObj.accY_LH_scaled, ACC_SCALING_FACTOR);
//
//    res->dataSize = 0;
//    res->status = DOP_SDO_SUCC;
//}






