/*
 * ioif_mti630.c
 *
 *  Created on: Sep 23, 2025
 *      Author: INVINCIBLENESS
 */


#include "ioif_mti630.h"

/** @defgroup UART
  * @brief UART IMU MTi-630 module driver
  * @{
  */
#ifdef IOIF_MTI630_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */

#ifdef EXPANSION_BOARD_ENABLED
static uint8_t uart7DmaRx1Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx1Buff"))) = {0};
static uint8_t uart7DmaRx2Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx2Buff"))) = {0};
static uint8_t uart7DmaRx3Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx3Buff"))) = {0};
static uint8_t uart7DmaRx4Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx4Buff"))) = {0};

static uint8_t uart8DmaRx1Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx1Buff"))) = {0};
static uint8_t uart8DmaRx2Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx2Buff"))) = {0};
static uint8_t uart8DmaRx3Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx3Buff"))) = {0};
static uint8_t uart8DmaRx4Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx4Buff"))) = {0};
static uint8_t uart8DmaRx5Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx5Buff"))) = {0};
static uint8_t uart8DmaRx6Buff[IOIF_MTI630_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx6Buff"))) = {0};
#endif

/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */

/* Current MTi630 Object pointer */
IOIF_MTI630_Obj_t* mti630CurrObj;

uint8_t cmdReqData[5] = {0xFA, 0xFF, 0x34, 0x00, 0xCD};



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


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */


/* settingChange: 0(default mode), 1(custom setting) */
IOIF_MTI630_State_t IOIF_MTI630_Init(IOIF_MTI630_Obj_t* mti630Obj, UART_HandleTypeDef* huart, uint8_t settingChange)
{
	memset(uart7DmaRx1Buff, 0, sizeof(uart7DmaRx1Buff));
	memset(uart7DmaRx2Buff, 0, sizeof(uart7DmaRx2Buff));
	memset(uart7DmaRx3Buff, 0, sizeof(uart7DmaRx3Buff));
	memset(uart7DmaRx4Buff, 0, sizeof(uart7DmaRx4Buff));
	memset(uart8DmaRx1Buff, 0, sizeof(uart8DmaRx1Buff));
	memset(uart8DmaRx2Buff, 0, sizeof(uart8DmaRx2Buff));
	memset(uart8DmaRx3Buff, 0, sizeof(uart8DmaRx3Buff));
	memset(uart8DmaRx4Buff, 0, sizeof(uart8DmaRx4Buff));
	memset(uart8DmaRx5Buff, 0, sizeof(uart8DmaRx5Buff));
	memset(uart8DmaRx6Buff, 0, sizeof(uart8DmaRx6Buff));

	mti630Obj->MTI630_huart = huart;

	/*------------------------------------------------------ Default setting without Initialization ------------------------------------------------------*/
	if (settingChange == 0 || settingChange == 1) {
		mti630Obj->baudRate 						= IOIF_MTI630_BAUDRATE_921600;
		mti630Obj->outputConfig.outputRate 			= IOIF_MTI630_OUTPUT_RATE_POLLING;
		mti630Obj->outputConfig.outputCode 			= IOIF_MTI630_OUTPUT_CODE_HEX;
		mti630Obj->outputConfig.outputFormat 		= IOIF_MTI630_OUTPUT_FORMAT_EULER_ACC_GYR;
		mti630Obj->outputConfig.outputGyro 			= IOIF_MTI630_OUTPUT_GYRO_ON;
		mti630Obj->outputConfig.outputAcc 			= IOIF_MTI630_OUTPUT_ACC_ON_RAW;
		mti630Obj->outputConfig.outputMag 			= IOIF_MTI630_OUTPUT_MAG_OFF;

		mti630Obj->outputConfig.outputDataByte_HEX 	= IOIF_MTI630_RX_BUFFER_LENGTH;

		mti630Obj->firstRun = 0;

		return IOIF_MTI630_STATUS_OK;
	}

	return IOIF_MTI630_STATUS_OK;
}


IOIF_MTI630_State_t IOIF_MTI630_GetIMUData(IOIF_MTI630_Obj_t* mti630Obj, uint8_t RxMode)
{
	/*--------------------------------------------- Temporary variables for HEX data parsing ---------------------------------------------*/
	static uint32_t roll_temp = 0;
	static uint32_t pitch_temp = 0;
	static uint32_t yaw_temp = 0;

	static uint32_t gyrX_temp = 0;
	static uint32_t gyrY_temp = 0;
	static uint32_t gyrZ_temp = 0;

	static uint32_t accX_temp = 0;
	static uint32_t accY_temp = 0;
	static uint32_t accZ_temp = 0;

	static float roll_f = 0.0f;
	static float pitch_f = 0.0f;
	static float yaw_f = 0.0f;

	static float gyrX_f = 0.0f;
	static float gyrY_f = 0.0f;
	static float gyrZ_f = 0.0f;

	static float accX_f = 0.0f;
	static float accY_f = 0.0f;
	static float accZ_f = 0.0f;
	/*-------------------------------------------------------------------------------------------------------------------------------------*/

	mti630Obj->rxMode = RxMode;

	uint8_t* selectedBuff = uart7DmaRx1Buff;

	if (mti630Obj->sensorID == 1) {
		selectedBuff = uart7DmaRx1Buff;
	}
	else if (mti630Obj->sensorID == 2) {
		selectedBuff = uart7DmaRx2Buff;
	}
	else if (mti630Obj->sensorID == 3) {
		selectedBuff = uart7DmaRx3Buff;
	}
	else if (mti630Obj->sensorID == 4) {
		selectedBuff = uart7DmaRx4Buff;
	}
	else if (mti630Obj->sensorID == 5) {
		selectedBuff = uart8DmaRx1Buff;
	}
	else if (mti630Obj->sensorID == 6) {
		selectedBuff = uart8DmaRx2Buff;
	}
	else if (mti630Obj->sensorID == 7) {
		selectedBuff = uart8DmaRx3Buff;
	}
	else if (mti630Obj->sensorID == 8) {
		selectedBuff = uart8DmaRx4Buff;
	}
	else if (mti630Obj->sensorID == 9) {
		selectedBuff = uart8DmaRx5Buff;
	}
	else if (mti630Obj->sensorID == 10) {
		selectedBuff = uart8DmaRx6Buff;
	}


//	if ( mti630Obj->MTI630_huart->ErrorCode != 0 ) {
//		Reset_UART_mti630(mti630Obj->MTI630_huart);
//		return IOIF_MTI630_STATUS_ERROR;
//	}

	HAL_UART_Transmit_DMA(mti630Obj->MTI630_huart, (uint8_t*)cmdReqData, (uint16_t)(sizeof(cmdReqData)));
//	HAL_UART_Transmit(mti630Obj->MTI630_huart, (uint8_t*)cmdReqData, (uint16_t)(sizeof(cmdReqData)), 1);
	HAL_UART_Receive(mti630Obj->MTI630_huart, selectedBuff, IOIF_MTI630_RX_BUFFER_LENGTH, 10);

	uint8_t bufSize = IOIF_MTI630_RX_BUFFER_LENGTH;
	uint8_t cursor = 0;

	/* Find the SOL */
	for (uint8_t i = 0; i < bufSize; i++) {
		if (selectedBuff[i] == 0xFA && selectedBuff[(i+1) % bufSize] == 0xFF && selectedBuff[(i+2) % bufSize] == 0x36 && selectedBuff[(i+3) % bufSize] == 0x2D) {
			cursor = (i + 4) % bufSize;
			break;		// Necessary
		}
	}

	cursor = (cursor + 3) % bufSize;

	/* Data Parsing */
	switch (mti630Obj->rxMode) {
		case (IOIF_MTI630_RXMODE_EULER_ACC_GYR):
			roll_temp  	= ((uint32_t)selectedBuff[cursor] << 24) | ((uint32_t)selectedBuff[(cursor+1) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+2) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+3) % bufSize]);
			pitch_temp 	= ((uint32_t)selectedBuff[(cursor+4) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+5) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+6) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+7) % bufSize]);
			yaw_temp   	= ((uint32_t)selectedBuff[(cursor+8) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+9) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+10) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+11) % bufSize]);

			accX_temp 	= ((uint32_t)selectedBuff[(cursor+15) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+16) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+17) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+18) % bufSize]);
			accY_temp 	= ((uint32_t)selectedBuff[(cursor+19) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+20) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+21) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+22) % bufSize]);
			accZ_temp 	= ((uint32_t)selectedBuff[(cursor+23) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+24) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+25) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+26) % bufSize]);

			gyrX_temp 	= ((uint32_t)selectedBuff[(cursor+30) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+31) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+32) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+33) % bufSize]);
			gyrY_temp 	= ((uint32_t)selectedBuff[(cursor+34) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+35) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+36) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+37) % bufSize]);
			gyrZ_temp 	= ((uint32_t)selectedBuff[(cursor+38) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+39) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+40) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+41) % bufSize]);

			memcpy(&roll_f, &roll_temp, sizeof(roll_f));
			memcpy(&pitch_f, &pitch_temp, sizeof(pitch_f));
			memcpy(&yaw_f, &yaw_temp, sizeof(yaw_f));

			memcpy(&accX_f, &accX_temp, sizeof(accX_f));
			memcpy(&accY_f, &accY_temp, sizeof(accY_f));
			memcpy(&accZ_f, &accZ_temp, sizeof(accZ_f));

			memcpy(&gyrX_f, &gyrX_temp, sizeof(gyrX_f));
			memcpy(&gyrY_f, &gyrY_temp, sizeof(gyrY_f));
			memcpy(&gyrZ_f, &gyrZ_temp, sizeof(gyrZ_f));

			mti630Obj->eulerAngle.roll 		= roll_f;
			mti630Obj->eulerAngle.pitch 	= pitch_f;
			mti630Obj->eulerAngle.yaw 		= yaw_f;
			mti630Obj->sensorData.accX 		= accX_f;
			mti630Obj->sensorData.accY 		= accY_f;
			mti630Obj->sensorData.accZ 		= accZ_f;
			mti630Obj->sensorData.gyrX 		= gyrX_f;
			mti630Obj->sensorData.gyrY 		= gyrY_f;
			mti630Obj->sensorData.gyrZ 		= gyrZ_f;
			break;

		default:
			break;
	}


	return IOIF_MTI630_STATUS_OK;
}


IOIF_MTI630_State_t IOIF_MTI630_GetIMUData_continuous_euler(IOIF_MTI630_Obj_t* mti630Obj, uint8_t RxMode)
{
	/*--------------------------------------------- Temporary variables for HEX data parsing ---------------------------------------------*/
	static uint32_t roll_temp = 0;
	static uint32_t pitch_temp = 0;
	static uint32_t yaw_temp = 0;

	static uint32_t gyrX_temp = 0;
	static uint32_t gyrY_temp = 0;
	static uint32_t gyrZ_temp = 0;

	static uint32_t accX_temp = 0;
	static uint32_t accY_temp = 0;
	static uint32_t accZ_temp = 0;

	static float roll_f = 0.0f;
	static float pitch_f = 0.0f;
	static float yaw_f = 0.0f;

	static float gyrX_f = 0.0f;
	static float gyrY_f = 0.0f;
	static float gyrZ_f = 0.0f;

	static float accX_f = 0.0f;
	static float accY_f = 0.0f;
	static float accZ_f = 0.0f;
	/*-------------------------------------------------------------------------------------------------------------------------------------*/

	mti630Obj->rxMode = RxMode;

	uint8_t* selectedBuff = uart7DmaRx1Buff;

	if (mti630Obj->sensorID == 1) {
		selectedBuff = uart7DmaRx1Buff;
	}
	else if (mti630Obj->sensorID == 2) {
		selectedBuff = uart7DmaRx2Buff;
	}
	else if (mti630Obj->sensorID == 3) {
		selectedBuff = uart7DmaRx3Buff;
	}
	else if (mti630Obj->sensorID == 4) {
		selectedBuff = uart7DmaRx4Buff;
	}
	else if (mti630Obj->sensorID == 5) {
		selectedBuff = uart8DmaRx1Buff;
	}
	else if (mti630Obj->sensorID == 6) {
		selectedBuff = uart8DmaRx2Buff;
	}
	else if (mti630Obj->sensorID == 7) {
		selectedBuff = uart8DmaRx3Buff;
	}
	else if (mti630Obj->sensorID == 8) {
		selectedBuff = uart8DmaRx4Buff;
	}
	else if (mti630Obj->sensorID == 9) {
		selectedBuff = uart8DmaRx5Buff;
	}
	else if (mti630Obj->sensorID == 10) {
		selectedBuff = uart8DmaRx6Buff;
	}

	if (mti630Obj->firstRun == 0) {
		HAL_UART_Receive_DMA(mti630Obj->MTI630_huart, selectedBuff, IOIF_MTI630_RX_BUFFER_LENGTH);
		mti630Obj->firstRun = 1;
	}

	uint8_t bufSize = IOIF_MTI630_RX_BUFFER_LENGTH;
	uint8_t cursor = 0;

	/* Find the SOL */
	for (uint8_t i = 0; i < bufSize; i++) {
		if (selectedBuff[i] == 0xFA && selectedBuff[(i+1) % bufSize] == 0xFF && selectedBuff[(i+2) % bufSize] == 0x36 && selectedBuff[(i+3) % bufSize] == 0x2D) {
			cursor = (i + 4) % bufSize;
			break;		// Necessary
		}
	}

	cursor = (cursor + 3) % bufSize;

	/* Data Parsing */
	switch (mti630Obj->rxMode) {
		case (IOIF_MTI630_RXMODE_EULER_ACC_GYR):
			roll_temp  	= ((uint32_t)selectedBuff[cursor] << 24) | ((uint32_t)selectedBuff[(cursor+1) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+2) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+3) % bufSize]);
			pitch_temp 	= ((uint32_t)selectedBuff[(cursor+4) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+5) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+6) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+7) % bufSize]);
			yaw_temp   	= ((uint32_t)selectedBuff[(cursor+8) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+9) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+10) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+11) % bufSize]);

			accX_temp 	= ((uint32_t)selectedBuff[(cursor+15) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+16) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+17) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+18) % bufSize]);
			accY_temp 	= ((uint32_t)selectedBuff[(cursor+19) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+20) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+21) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+22) % bufSize]);
			accZ_temp 	= ((uint32_t)selectedBuff[(cursor+23) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+24) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+25) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+26) % bufSize]);

			gyrX_temp 	= ((uint32_t)selectedBuff[(cursor+30) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+31) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+32) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+33) % bufSize]);
			gyrY_temp 	= ((uint32_t)selectedBuff[(cursor+34) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+35) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+36) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+37) % bufSize]);
			gyrZ_temp 	= ((uint32_t)selectedBuff[(cursor+38) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+39) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+40) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+41) % bufSize]);

			memcpy(&roll_f, &roll_temp, sizeof(roll_f));
			memcpy(&pitch_f, &pitch_temp, sizeof(pitch_f));
			memcpy(&yaw_f, &yaw_temp, sizeof(yaw_f));

			memcpy(&accX_f, &accX_temp, sizeof(accX_f));
			memcpy(&accY_f, &accY_temp, sizeof(accY_f));
			memcpy(&accZ_f, &accZ_temp, sizeof(accZ_f));

			memcpy(&gyrX_f, &gyrX_temp, sizeof(gyrX_f));
			memcpy(&gyrY_f, &gyrY_temp, sizeof(gyrY_f));
			memcpy(&gyrZ_f, &gyrZ_temp, sizeof(gyrZ_f));

			mti630Obj->eulerAngle.roll 		= roll_f;
			mti630Obj->eulerAngle.pitch 	= pitch_f;
			mti630Obj->eulerAngle.yaw 		= yaw_f;
			mti630Obj->sensorData.accX 		= accX_f;
			mti630Obj->sensorData.accY 		= accY_f;
			mti630Obj->sensorData.accZ 		= accZ_f;
			mti630Obj->sensorData.gyrX 		= gyrX_f;
			mti630Obj->sensorData.gyrY 		= gyrY_f;
			mti630Obj->sensorData.gyrZ 		= gyrZ_f;
			break;

		default:
			break;
	}


	return IOIF_MTI630_STATUS_OK;
}


IOIF_MTI630_State_t IOIF_MTI630_GetIMUData_continuous_quaternion(IOIF_MTI630_Obj_t* mti630Obj, uint8_t RxMode)
{
	/*--------------------------------------------- Temporary variables for HEX data parsing ---------------------------------------------*/
	static uint32_t w_temp = 0;
	static uint32_t x_temp = 0;
	static uint32_t y_temp = 0;
	static uint32_t z_temp = 0;

	static uint32_t gyrX_temp = 0;
	static uint32_t gyrY_temp = 0;
	static uint32_t gyrZ_temp = 0;

	static uint32_t accX_temp = 0;
	static uint32_t accY_temp = 0;
	static uint32_t accZ_temp = 0;

	static float w_f = 0.0f;
	static float x_f = 0.0f;
	static float y_f = 0.0f;
	static float z_f = 0.0f;

	static float gyrX_f = 0.0f;
	static float gyrY_f = 0.0f;
	static float gyrZ_f = 0.0f;

	static float accX_f = 0.0f;
	static float accY_f = 0.0f;
	static float accZ_f = 0.0f;
	/*-------------------------------------------------------------------------------------------------------------------------------------*/

	mti630Obj->rxMode = RxMode;

	uint8_t* selectedBuff = uart7DmaRx1Buff;

	if (mti630Obj->sensorID == 1) {
		selectedBuff = uart7DmaRx1Buff;
	}
	else if (mti630Obj->sensorID == 2) {
		selectedBuff = uart7DmaRx2Buff;
	}
	else if (mti630Obj->sensorID == 3) {
		selectedBuff = uart7DmaRx3Buff;
	}
	else if (mti630Obj->sensorID == 4) {
		selectedBuff = uart7DmaRx4Buff;
	}
	else if (mti630Obj->sensorID == 5) {
		selectedBuff = uart8DmaRx1Buff;
	}
	else if (mti630Obj->sensorID == 6) {
		selectedBuff = uart8DmaRx2Buff;
	}
	else if (mti630Obj->sensorID == 7) {
		selectedBuff = uart8DmaRx3Buff;
	}
	else if (mti630Obj->sensorID == 8) {
		selectedBuff = uart8DmaRx4Buff;
	}
	else if (mti630Obj->sensorID == 9) {
		selectedBuff = uart8DmaRx5Buff;
	}
	else if (mti630Obj->sensorID == 10) {
		selectedBuff = uart8DmaRx6Buff;
	}

	if (mti630Obj->firstRun == 0) {
		HAL_UART_Receive_DMA(mti630Obj->MTI630_huart, selectedBuff, IOIF_MTI630_RX_BUFFER_LENGTH);
		mti630Obj->firstRun = 1;
	}

	uint8_t bufSize = IOIF_MTI630_RX_BUFFER_LENGTH;
	uint8_t cursor = 0;

	/* Find the SOL */
	for (uint8_t i = 0; i < bufSize; i++) {
		if (selectedBuff[i] == 0xFA && selectedBuff[(i+1) % bufSize] == 0xFF && selectedBuff[(i+2) % bufSize] == 0x36 && selectedBuff[(i+3) % bufSize] == 0x31) {
			cursor = (i + 4) % bufSize;
			break;		// Necessary
		}
	}

	cursor = (cursor + 3) % bufSize;

	/* Data Parsing */
	switch (mti630Obj->rxMode) {
		case (IOIF_MTI630_RXMODE_QUATERNION_ACC_GYR):
			w_temp  	= ((uint32_t)selectedBuff[cursor] << 24) | ((uint32_t)selectedBuff[(cursor+1) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+2) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+3) % bufSize]);
			x_temp	 	= ((uint32_t)selectedBuff[(cursor+4) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+5) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+6) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+7) % bufSize]);
			y_temp   	= ((uint32_t)selectedBuff[(cursor+8) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+9) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+10) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+11) % bufSize]);
			z_temp   	= ((uint32_t)selectedBuff[(cursor+12) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+13) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+14) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+15) % bufSize]);

			accX_temp 	= ((uint32_t)selectedBuff[(cursor+19) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+20) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+21) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+22) % bufSize]);
			accY_temp 	= ((uint32_t)selectedBuff[(cursor+23) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+24) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+25) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+26) % bufSize]);
			accZ_temp 	= ((uint32_t)selectedBuff[(cursor+27) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+28) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+29) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+30) % bufSize]);

			gyrX_temp 	= ((uint32_t)selectedBuff[(cursor+34) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+35) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+36) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+37) % bufSize]);
			gyrY_temp 	= ((uint32_t)selectedBuff[(cursor+38) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+39) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+40) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+41) % bufSize]);
			gyrZ_temp 	= ((uint32_t)selectedBuff[(cursor+42) % bufSize] << 24) | ((uint32_t)selectedBuff[(cursor+43) % bufSize] << 16) | ((uint32_t)selectedBuff[(cursor+44) % bufSize] << 8) | ((uint32_t)selectedBuff[(cursor+45) % bufSize]);

			memcpy(&w_f, &w_temp, sizeof(w_f));
			memcpy(&x_f, &x_temp, sizeof(x_f));
			memcpy(&y_f, &y_temp, sizeof(y_f));
			memcpy(&z_f, &z_temp, sizeof(z_f));

			memcpy(&accX_f, &accX_temp, sizeof(accX_f));
			memcpy(&accY_f, &accY_temp, sizeof(accY_f));
			memcpy(&accZ_f, &accZ_temp, sizeof(accZ_f));

			memcpy(&gyrX_f, &gyrX_temp, sizeof(gyrX_f));
			memcpy(&gyrY_f, &gyrY_temp, sizeof(gyrY_f));
			memcpy(&gyrZ_f, &gyrZ_temp, sizeof(gyrZ_f));

			mti630Obj->quaternion.w 		= w_f;
			mti630Obj->quaternion.x 		= x_f;
			mti630Obj->quaternion.y 		= y_f;
			mti630Obj->quaternion.z 		= z_f;
			mti630Obj->sensorData.accX 		= accX_f;
			mti630Obj->sensorData.accY 		= accY_f;
			mti630Obj->sensorData.accZ 		= accZ_f;
			mti630Obj->sensorData.gyrX 		= gyrX_f;
			mti630Obj->sensorData.gyrY 		= gyrY_f;
			mti630Obj->sensorData.gyrZ 		= gyrZ_f;
			break;

		default:
			break;
	}


	return IOIF_MTI630_STATUS_OK;
}


void Reset_UART_mti630(UART_HandleTypeDef *huart)
{
    HAL_UART_Abort(huart);

    __HAL_UART_CLEAR_PEFLAG(huart);
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);

    huart->ErrorCode = HAL_UART_ERROR_NONE;
    huart->gState = HAL_UART_STATE_READY;
    huart->RxState = HAL_UART_STATE_READY;

    HAL_UART_Init(huart);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */


#endif /* IOIF_MTI630_ENABLED */
