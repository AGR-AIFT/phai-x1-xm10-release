/*
 * ioif_ebimu_9dofv5_r3.c
 *
 *  Created on: Jan 2, 2025
 *      Author: INVINCIBLE_1NE
 */


#include "ioif_ebimu_9dofv5_r3.h"

/** @defgroup UART
  * @brief UART IMU EBIMU_9DOFV5_R3 module driver
  * @{
  */
#ifdef IOIF_EBIMU_9DOFV5_R3_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */

#ifdef EXPANSION_BOARD_ENABLED
static uint8_t uart7DmaRx1Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx1Buff"))) = {0};
static uint8_t uart7DmaRx2Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx2Buff"))) = {0};
static uint8_t uart7DmaRx3Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx3Buff"))) = {0};
static uint8_t uart7DmaRx4Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart7DmaRx4Buff"))) = {0};

static uint8_t uart8DmaRx1Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx1Buff"))) = {0};
static uint8_t uart8DmaRx2Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx2Buff"))) = {0};
static uint8_t uart8DmaRx3Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx3Buff"))) = {0};
static uint8_t uart8DmaRx4Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx4Buff"))) = {0};
static uint8_t uart8DmaRx5Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx5Buff"))) = {0};
static uint8_t uart8DmaRx6Buff[IOIF_EBIMU_RX_BUFFER_LENGTH] __attribute__((section(".uart8DmaRx6Buff"))) = {0};
#endif

/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */

/* Current EBIMU Object pointer */
IOIF_EBIMU_Obj_t* ebimuCurrObj;


/*------------------------------------------------- COMMAND for EBIMU -------------------------------------------------*/
uint8_t responseSize_default = 5;		// For "<ok>" and "<er>"
uint8_t ringBufferSize = 0;
uint8_t ringBufferLastIdx = 0;
uint8_t pollingRxToken = 0;
uint8_t cmdRxToken = 0;
uint8_t cmdResponseCheck = 0;
uint8_t firstCmdTx = 0;
uint8_t uartDataSize = 0;
uint8_t uartCurrIdx = 0;
uint32_t timeCheck = 0;


char cmdRxResponse[IOIF_EBIMU_RESPONSE_MAX_NUM] = "";
char RxBuf[IOIF_EBIMU_RX_BUFFER_LENGTH] = "";
char tempData1[10] = "";
char tempData2[10] = "";
char tempData3[10] = "";
char tempData4[10] = "";
char tempData5[10] = "";
char tempData6[10] = "";

char cmdStart 		= '<';
char cmdEnd	  		= '>';
char cmdPolling 	= '*';
char CR				= '\r';
char LF				= '\n';
char SOL			= '*';
char SP				= ',';		// Separator


char cmd_SET_BAUDRATE[3] 				= "sb";
char cmd_SET_OUTPUT_RATE[4] 			= "sor";
char cmd_SET_OUTPUT_CODE[4] 			= "soc";
char cmd_SET_OUTPUT_FORMAT[4] 			= "sof";
char cmd_SET_OUTPUT_GYRO[4] 			= "sog";
char cmd_SET_OUTPUT_ACC[4] 				= "soa";
char cmd_SET_OUTPUT_MAG[4] 				= "som";
char cmd_SET_OUTPUT_DISTANCE[4] 		= "sod";
char cmd_SET_OUTPUT_TEMPERATURE[4]		= "sot";
char cmd_SET_OUTPUT_TIMESTAMP[5] 		= "sots";
char cmd_SET_ENABLE_MAG[4] 				= "sem";
char cmd_SET_GYRO_SENS[4] 				= "ssg";
char cmd_SET_ACC_SENS[4] 				= "ssa";
char cmd_SET_ACC_LPF[5] 				= "lpfa";
char cmd_SET_FF_BOTH[4] 				= "sff";
char cmd_SET_FF_ACC[5] 					= "sffa";
char cmd_SET_FF_MAG[5] 					= "sffm";
char cmd_SET_RAA_LEVEL[6]				= "raa_l";
char cmd_SET_RAA_TIMEOUT[6] 			= "raa_t";
char cmd_SET_RHA_LEVEL[6] 				= "rha_l";
char cmd_SET_RHA_TIMEOUT[6] 			= "rha_t";
char cmd_SET_RHA_CLEAR[8] 				= "rha_clr";
char cmd_SET_RHA_FINDHEAD[7] 			= "rha_fh";
char cmd_SET_AGC_ENABLE[6] 				= "agc_e";
char cmd_SET_AGC_THRESHOLD[6] 			= "agc_t";
char cmd_SET_AGC_DRIFT[6] 				= "agc_d";
char cmd_SET_AVC_ENABLE[7] 				= "avca_e";
char cmd_SET_PF_PARAM[8] 				= "posf_sl";
char cmd_SET_POS_ZERO[5] 				= "posz";
char cmd_CALIB_GYRO[3] 					= "cg";
char cmd_CALIB_ACC_FREE[4] 				= "caf";
char cmd_CALIB_ACC_SIMPLE[4]			= "cas";
char cmd_CALIB_MAG_FREE[4]				= "cmf";
char cmd_CALIB_MAG_XY[5]				= "cnxy";
char cmd_CALIB_MAG_XY_COMP[6]			= "+cnxy";
char cmd_CALIB_MAG_Z[4]					= "cnz";
char cmd_CALIB_MAG_Z_COMP[5]			= "+cnz";
char cmd_SET_MOTION_OFFSET_RPY[4]		= "cmo";
char cmd_SET_MOTION_OFFSET_ROLL[5]		= "cmox";
char cmd_SET_MOTION_OFFSET_PITCH[5]		= "cmoy";
char cmd_SET_MOTION_OFFSET_YAW[5]		= "cmoz";
char cmd_SET_MOTION_OFFSET_RP[6]		= "cmoxy";
char cmd_SET_MOTION_OFFSET_RPY_ROT[5]	= "cmo2";
char cmd_SET_MOTION_OFFSET_YAW_ROT[5]	= "cmoh";
char cmd_CLEAR_MOTION_OFFSET[5]			= "cmco";
char cmd_GET_CONFIG[4]					= "cfg";
char cmd_POWER_ON_START[5]				= "pons";
char cmd_START[6]						= "start";
char cmd_STOP[5]						= "stop";
char cmd_LOAD_FACTORY_SETTINGS[3]		= "lf";
char cmd_RESET[6]						= "reset";
char cmd_VERSION_CHECK[4]				= "ver";

char response_OK[5]						= "<ok>";
char response_ERROR[5]					= "<er>";
/*-----------------------------------------------------------------------------------------------------------------------*/


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
static void IOIF_EBIMU_MakeCMD_8bit(IOIF_EBIMU_Obj_t* ebimuObj, char* cmd, uint8_t cmd_param);
static void IOIF_EBIMU_MakeCMD_16bit(IOIF_EBIMU_Obj_t* ebimuObj, char* cmd, uint16_t cmd_param);
static void IOIF_EBIMU_MakeCMD_32bit(IOIF_EBIMU_Obj_t* ebimuObj, char* cmd, uint32_t cmd_param);

static void IOIF_EBIMU_InitUartRxHdlr(IOIF_EBIMU_Obj_t* ebimuObj);
//static void AlignRxBuffer(uint8_t realRxByte);
//static void AssignRxData(void);
/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */





/*-------------------------------------------------------------------------------------------------------------------------*/

static void IOIF_EBIMU_MakeCMD_8bit(IOIF_EBIMU_Obj_t* ebimuObj, char* cmd, uint8_t cmd_param)
{
	static char paramStr[4] = "";

	memset(ebimuObj->cmdTxBufChar, 0, sizeof(ebimuObj->cmdTxBufChar));		// Reset the Tx buffer
	memset(ebimuObj->cmdRxResponse, 0, sizeof(ebimuObj->cmdRxResponse));	// Reset the Rx buffer

	sprintf(paramStr, "%u", cmd_param);
	strncat(ebimuObj->cmdTxBufChar, &cmdStart, 1);			// "<"
	strcat(ebimuObj->cmdTxBufChar, cmd);					// "cmd"
	strcat(ebimuObj->cmdTxBufChar, paramStr);				// "cmd_param"
	strncat(ebimuObj->cmdTxBufChar, &cmdEnd, 1);			// ">"

	ebimuObj->cmdTxBufSize = strlen(ebimuObj->cmdTxBufChar);
}

static void IOIF_EBIMU_MakeCMD_16bit(IOIF_EBIMU_Obj_t* ebimuObj, char* cmd, uint16_t cmd_param)
{
	static char paramStr[6] = "";

	memset(ebimuObj->cmdTxBufChar, 0, sizeof(ebimuObj->cmdTxBufChar));		// Reset the Tx buffer
	memset(ebimuObj->cmdRxResponse, 0, sizeof(ebimuObj->cmdRxResponse));	// Reset the Rx buffer

	sprintf(paramStr, "%u", cmd_param);
	strncat(ebimuObj->cmdTxBufChar, &cmdStart, 1);			// "<"
	strcat(ebimuObj->cmdTxBufChar, cmd);					// "cmd"
	strcat(ebimuObj->cmdTxBufChar, paramStr);				// "cmd_param"
	strncat(ebimuObj->cmdTxBufChar, &cmdEnd, 1);			// ">"

	ebimuObj->cmdTxBufSize = strlen(ebimuObj->cmdTxBufChar);
}

static void IOIF_EBIMU_MakeCMD_32bit(IOIF_EBIMU_Obj_t* ebimuObj, char* cmd, uint32_t cmd_param)
{
	static char paramStr[11] = "";

	memset(ebimuObj->cmdTxBufChar, 0, sizeof(ebimuObj->cmdTxBufChar));		// Reset the Tx buffer
	memset(ebimuObj->cmdRxResponse, 0, sizeof(ebimuObj->cmdRxResponse));	// Reset the Rx buffer

	sprintf(paramStr, "%lu", cmd_param);
	strncat(ebimuObj->cmdTxBufChar, &cmdStart, 1);			// "<"
	strcat(ebimuObj->cmdTxBufChar, cmd);					// "cmd"
	strcat(ebimuObj->cmdTxBufChar, paramStr);				// "cmd_param"
	strncat(ebimuObj->cmdTxBufChar, &cmdEnd, 1);			// ">"

	ebimuObj->cmdTxBufSize = strlen(ebimuObj->cmdTxBufChar);
}

//void IOIF_EBIMU_RingBuffer(IOIF_EBIMU_Obj_t* ebimuObj, char* originBuffer)
//{
//	uint8_t cursor = 0;
//	uint8_t nextCursor = 0;
//
//	for (uint8_t i = 0; i < IOIF_EBIMU_RX_BUFFER_LENGTH; i++) {
//		if ((char)(originBuffer[i]) == SOL) {								// [1] Find the SOL '*'
//			cursor = i+1;
//
//			for (uint8_t j = 0; j < IOIF_EBIMU_RX_BUFFER_LENGTH; j++) {   	// [2] Assign & Algin the data
//				ebimuObj->RxBuf[j] = (char)(originBuffer[cursor++]);
//
//				if (cursor == IOIF_EBIMU_RX_BUFFER_LENGTH) {			  	// [3] Handle the overflow of current cursor
//					cursor = 0;
//				}
//
//				nextCursor = cursor + 1;									// [4] Handle the overflow of next cursor
//				if (nextCursor == IOIF_EBIMU_RX_BUFFER_LENGTH) {
//					nextCursor = 0;
//				}
//
//				if ((char)(originBuffer[cursor]) == CR && (char)(originBuffer[nextCursor]) == LF) {
//					ringBufferLastIdx = j + 1;								// [5] Index of 'CR' in Ring Buffer
//					for (uint8_t k = ringBufferLastIdx; k < IOIF_EBIMU_RX_BUFFER_LENGTH; k++) {
//						ebimuObj->RxBuf[k] = '\0';							// [6] Reset all the other elements
//					}
//					return;
//				}
//			}
//
//			return;
//		}
//	}
//}



IOIF_EBIMU_State_t IOIF_EBIMU_CheckResponse(IOIF_EBIMU_Obj_t* ebimuObj)
{
	for (uint8_t i = 0; i < 4; i++){
		if ( ebimuObj->cmdRxResponse[i] != response_OK[i] ) {
			return IOIF_EBIMU_STATUS_ERROR;
		}
	}

	return IOIF_EBIMU_STATUS_OK;
}


/* settingChange: 0(default mode), 1(custom setting) */
IOIF_EBIMU_State_t IOIF_EBIMU_Init(IOIF_EBIMU_Obj_t* ebimuObj, UART_HandleTypeDef* huart, uint8_t settingChange)
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

	ebimuObj->EBIMU_huart = huart;

	/*------------------------------------------------------ Default setting without Initialization (QUATERNION version) ------------------------------------------------------*/
	if (settingChange == 0) {
		ebimuObj->baudRate 							= IOIF_EBIMU_BAUDRATE_921600;
		ebimuObj->outputConfig.outputRate 			= IOIF_EBIMU_OUTPUT_RATE_POLLING;
		ebimuObj->outputConfig.outputCode 			= IOIF_EBIMU_OUTPUT_CODE_HEX;
		ebimuObj->outputConfig.outputFormat 		= IOIF_EBIMU_OUTPUT_FORMAT_QUATERNION;
		ebimuObj->outputConfig.outputGyro 			= IOIF_EBIMU_OUTPUT_GYRO_OFF;
		ebimuObj->outputConfig.outputAcc 			= IOIF_EBIMU_OUTPUT_ACC_OFF;
		ebimuObj->outputConfig.outputMag 			= IOIF_EBIMU_OUTPUT_MAG_OFF;
		ebimuObj->outputConfig.outputDistance 		= IOIF_EBIMU_OUTPUT_DISTANCE_OFF;
		ebimuObj->outputConfig.outputTemperature 	= IOIF_EBIMU_OUTPUT_TEMPERATURE_OFF;
		ebimuObj->outputConfig.outputTimeStamp 		= IOIF_EBIMU_OUTPUT_TIMESTAMP_OFF;

		ebimuObj->sensorConfig.sensorEnableMag		= IOIF_EBIMU_SENSOR_ENABLE_MAG_ON2;
		ebimuObj->sensorConfig.sensorGyroSens 		= IOIF_EBIMU_SENSOR_GYRO_SENS_2000;
		ebimuObj->sensorConfig.sensorAccSens 		= IOIF_EBIMU_SENSOR_ACC_SENS_16g;
		ebimuObj->sensorConfig.sensorAccLPF 		= IOIF_EBIMU_SENSOR_ACC_LPF_97Hz;
		ebimuObj->sensorConfig.sensorAccFF 			= SET_FF_BOTH;
		ebimuObj->sensorConfig.sensorMagFF 			= SET_FF_BOTH;

		ebimuObj->outputConfig.outputDataByte_HEX 	= IOIF_EBIMU_RX_LENGTH_DEFAULT;

		return IOIF_EBIMU_STATUS_OK;
	}

	/*------------------------------------------------ Default setting without Initialization (QUATERNION + GYR + ACC version) ------------------------------------------------*/
	if (settingChange == 1) {
		ebimuObj->baudRate 							= IOIF_EBIMU_BAUDRATE_921600;
		ebimuObj->outputConfig.outputRate 			= IOIF_EBIMU_OUTPUT_RATE_POLLING;
		ebimuObj->outputConfig.outputCode 			= IOIF_EBIMU_OUTPUT_CODE_HEX;
		ebimuObj->outputConfig.outputFormat 		= IOIF_EBIMU_OUTPUT_FORMAT_QUATERNION_GYR_ACC;
		ebimuObj->outputConfig.outputGyro 			= IOIF_EBIMU_OUTPUT_GYRO_ON;
		ebimuObj->outputConfig.outputAcc 			= IOIF_EBIMU_OUTPUT_ACC_ON_RAW;
		ebimuObj->outputConfig.outputMag 			= IOIF_EBIMU_OUTPUT_MAG_OFF;
		ebimuObj->outputConfig.outputDistance 		= IOIF_EBIMU_OUTPUT_DISTANCE_OFF;
		ebimuObj->outputConfig.outputTemperature 	= IOIF_EBIMU_OUTPUT_TEMPERATURE_OFF;
		ebimuObj->outputConfig.outputTimeStamp 		= IOIF_EBIMU_OUTPUT_TIMESTAMP_OFF;

		ebimuObj->sensorConfig.sensorEnableMag		= IOIF_EBIMU_SENSOR_ENABLE_MAG_ON2;
		ebimuObj->sensorConfig.sensorGyroSens 		= IOIF_EBIMU_SENSOR_GYRO_SENS_2000;
		ebimuObj->sensorConfig.sensorAccSens 		= IOIF_EBIMU_SENSOR_ACC_SENS_16g;
		ebimuObj->sensorConfig.sensorAccLPF 		= IOIF_EBIMU_SENSOR_ACC_LPF_97Hz;
		ebimuObj->sensorConfig.sensorAccFF 			= SET_FF_BOTH;
		ebimuObj->sensorConfig.sensorMagFF 			= SET_FF_BOTH;

		ebimuObj->outputConfig.outputDataByte_HEX 	= IOIF_EBIMU_RX_LENGTH_DEFAULT;

		return IOIF_EBIMU_STATUS_OK;
	}


	/*------------------------------------------------------------------ Custom Setting with Initialization ------------------------------------------------------------------*/
	/*----------------------------------- USER DEFINITION -----------------------------------*/
	uint8_t EBIMU_baudRate 			= SET_BAUDRATE_921600;
	uint16_t EBIMU_outputRate 		= SET_OUTPUT_RATE_POLLING;
	uint8_t EBIMU_outputCode		= SET_OUTPUT_CODE_HEX;
	uint8_t EBIMU_outputFormat		= SET_OUTPUT_FORMAT_QUATERNION;
	uint8_t EBIMU_outputGyro		= SET_OUTPUT_GYRO_OFF;
	uint8_t EBIMU_outputAcc			= SET_OUTPUT_ACC_OFF;
	uint8_t EBIMU_outputMag			= SET_OUTPUT_MAG_OFF;
	uint8_t EBIMU_outputDistance	= SET_OUTPUT_DISTANCE_OFF;
	uint8_t EBIMU_outputTemp		= SET_OUTPUT_TEMP_OFF;
	uint8_t EBIMU_outputTimeStamp	= SET_OUTPUT_TIMESTAMP_OFF;

	uint8_t EBIMU_enableMag			= SET_ENABLE_MAG_ON_2;
	uint8_t EBIMU_GyroSENS			= SET_GYRO_SENS_2000;
	uint8_t EBIMU_AccSENS			= SET_ACC_SENS_16g;
	uint8_t EBIMU_AccLPF			= SET_ACC_LPF_97;
	uint8_t EBIMU_SFF				= SET_FF_BOTH;
	/*----------------------------------------------------------------------------------------*/

	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_BAUDRATE, EBIMU_baudRate);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->baudRate = EBIMU_baudRate;}
	else 											{ebimuObj->baudRate = IOIF_EBIMU_BAUDRATE_ERROR;}


	IOIF_EBIMU_MakeCMD_16bit(ebimuObj, cmd_SET_OUTPUT_RATE, EBIMU_outputRate);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputRate = EBIMU_outputRate;}
	else 											{ebimuObj->outputConfig.outputRate = IOIF_EBIMU_OUTPUT_RATE_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_CODE, EBIMU_outputCode);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputCode = EBIMU_outputCode;}
	else 											{ebimuObj->outputConfig.outputCode = IOIF_EBIMU_OUTPUT_CODE_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_FORMAT, EBIMU_outputFormat);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputFormat = EBIMU_outputFormat;}
	else 											{ebimuObj->outputConfig.outputFormat = IOIF_EBIMU_OUTPUT_FORMAT_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_GYRO, EBIMU_outputGyro);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputGyro = EBIMU_outputGyro;}
	else 											{ebimuObj->outputConfig.outputGyro = IOIF_EBIMU_OUTPUT_GYRO_ERROR;}



	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_ACC, EBIMU_outputAcc);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputAcc = EBIMU_outputAcc;}
	else 											{ebimuObj->outputConfig.outputAcc = IOIF_EBIMU_OUTPUT_ACC_ERROR;}



	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_MAG, EBIMU_outputMag);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputMag = EBIMU_outputMag;}
	else 											{ebimuObj->outputConfig.outputMag = IOIF_EBIMU_OUTPUT_MAG_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_DISTANCE, EBIMU_outputDistance);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputDistance = EBIMU_outputDistance;}
	else 											{ebimuObj->outputConfig.outputDistance = IOIF_EBIMU_OUTPUT_DISTANCE_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_TEMPERATURE, EBIMU_outputTemp);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputTemperature = EBIMU_outputTemp;}
	else 											{ebimuObj->outputConfig.outputTemperature = IOIF_EBIMU_OUTPUT_TEMPERATURE_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_OUTPUT_TIMESTAMP, EBIMU_outputTimeStamp);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->outputConfig.outputTimeStamp = EBIMU_outputTimeStamp;}
	else 											{ebimuObj->outputConfig.outputTimeStamp = IOIF_EBIMU_OUTPUT_TIMESTAMP_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_ENABLE_MAG, EBIMU_enableMag);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->sensorConfig.sensorEnableMag = EBIMU_enableMag;}
	else 											{ebimuObj->sensorConfig.sensorEnableMag = IOIF_EBIMU_SENSOR_ENABLE_MAG_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_GYRO_SENS, EBIMU_GyroSENS);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->sensorConfig.sensorGyroSens = EBIMU_GyroSENS;}
	else 											{ebimuObj->sensorConfig.sensorEnableMag = IOIF_EBIMU_SENSOR_GYRO_SENS_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_ACC_SENS, EBIMU_AccSENS);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->sensorConfig.sensorAccSens = EBIMU_AccSENS;}
	else 											{ebimuObj->sensorConfig.sensorAccSens = IOIF_EBIMU_SENSOR_ACC_SENS_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_ACC_LPF, EBIMU_AccLPF);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->sensorConfig.sensorAccLPF = EBIMU_AccLPF;}
	else 											{ebimuObj->sensorConfig.sensorAccLPF = IOIF_EBIMU_SENSOR_ACC_LPF_ERROR;}


	IOIF_EBIMU_MakeCMD_8bit(ebimuObj, cmd_SET_FF_BOTH, EBIMU_SFF);
	HAL_UART_Receive_IT(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdRxResponse, IOIF_EBIMU_RESPONSE_NUM);
	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)ebimuObj->cmdTxBufChar, ebimuObj->cmdTxBufSize, IOIF_EBIMU_TX_TIMEOUT);
	IOIF_EBIMU_us_Delay(IOIF_EBIMU_CMD_TX_DELAY);
	if (IOIF_EBIMU_CheckResponse(ebimuObj) == 0) 	{ebimuObj->sensorConfig.sensorAccFF = EBIMU_SFF;    ebimuObj->sensorConfig.sensorMagFF = EBIMU_SFF;}
	else 											{}


	/*--------------------------------------------------------------------- Total Output Data Size Check ---------------------------------------------------------------------*/
	if (ebimuObj->outputConfig.outputCode == IOIF_EBIMU_OUTPUT_CODE_ASCII) {
		if (ebimuObj->outputConfig.outputFormat == IOIF_EBIMU_OUTPUT_FORMAT_EULER) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 3;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_EULERANGLE;
		}
		else if (ebimuObj->outputConfig.outputFormat == IOIF_EBIMU_OUTPUT_FORMAT_QUATERNION) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 4;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_QUATERNION;
		}

		if (ebimuObj->outputConfig.outputAcc > 0) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 3;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_ACC;
		}

		if (ebimuObj->outputConfig.outputGyro > 0) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 3;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_GYRO;
		}

		if (ebimuObj->outputConfig.outputMag > 0) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 3;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_MAG;
		}

		if (ebimuObj->outputConfig.outputDistance > 0) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 3;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_DISTANCE;
		}

		if (ebimuObj->outputConfig.outputTemperature > 0) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 1;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_TEMPERATURE;
		}

		if (ebimuObj->outputConfig.outputTimeStamp > 0) {
			ebimuObj->outputConfig.outputDataNum_ASCII += 1;
			ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_BYTES_TIMESTAMP;
		}

		/* Total Received Data Bytes = (SOL: '*') + (ASCII data) + (separator ',') + (EOL: '\r\n') */
		ebimuObj->outputConfig.outputDataByte_ASCII += EBIMU_ASCII_DATA_SOL + (ebimuObj->outputConfig.outputDataNum_ASCII - 1) * EBIMU_ASCII_DATA_SIZE + EBIMU_ASCII_DATA_EOL;
//		ebimuObj->outputConfig.outputDataByte_ASCII = IOIF_EBIMU_RX_LENGTH_DEFAULT;
	}
	else if (ebimuObj->outputConfig.outputCode == IOIF_EBIMU_OUTPUT_CODE_HEX) {
		if (ebimuObj->outputConfig.outputFormat == IOIF_EBIMU_OUTPUT_FORMAT_EULER) {
			ebimuObj->outputConfig.outputDataNum_HEX += 3;
		}
		else if (ebimuObj->outputConfig.outputFormat == IOIF_EBIMU_OUTPUT_FORMAT_QUATERNION) {
			ebimuObj->outputConfig.outputDataNum_HEX += 4;
		}

		if (ebimuObj->outputConfig.outputAcc > 0) {
			ebimuObj->outputConfig.outputDataNum_HEX += 3;
		}

		if (ebimuObj->outputConfig.outputGyro > 0) {
			ebimuObj->outputConfig.outputDataNum_HEX += 3;
		}

		if (ebimuObj->outputConfig.outputMag > 0) {
			ebimuObj->outputConfig.outputDataNum_HEX += 3;
		}

		if (ebimuObj->outputConfig.outputDistance > 0) {
			ebimuObj->outputConfig.outputDataNum_HEX += 3;
		}

		if (ebimuObj->outputConfig.outputTemperature > 0) {
			ebimuObj->outputConfig.outputDataNum_HEX += 1;
		}

		if (ebimuObj->outputConfig.outputTimeStamp > 0) {
			ebimuObj->outputConfig.outputDataNum_HEX += 1;
		}

		/* Total Received Data Bytes = (HEX data) + (separator ' ') + (SOL '*') */
//		ebimuObj->outputConfig.outputDataByte_HEX = EBIMU_HEX_DATA_SOP + ((ebimuObj->outputConfig.outputDataNum_HEX) + (ebimuObj->outputConfig.outputDataNum_HEX - 1)) * EBIMU_HEX_DATA_SIZE + EBIMU_HEX_DATA_SIZE_CHECKSUM;
		ebimuObj->outputConfig.outputDataByte_HEX = IOIF_EBIMU_RX_LENGTH_DEFAULT;
	}


	return IOIF_EBIMU_STATUS_OK;
}


static void IOIF_EBIMU_InitUartRxHdlr(IOIF_EBIMU_Obj_t* ebimuObj)
{
	if (ebimuObj->firstRun == 0) {
//		__HAL_UART_ENABLE_IT(ebimuObj->EBIMU_huart, UART_IT_IDLE);
		HAL_UART_Receive_DMA(ebimuObj->EBIMU_huart, uart7DmaRx1Buff, ebimuObj->outputConfig.outputDataByte_HEX);
		uartCurrIdx = 0;
		ebimuObj->firstRun = 1;

		for (uint8_t i = 0; i < IOIF_EBIMU_RX_BUFFER_LENGTH; i++) {
			RxBuf[i] = '\0';
		}
	}
}


IOIF_EBIMU_State_t IOIF_EBIMU_GetIMUData(IOIF_EBIMU_Obj_t* ebimuObj, uint8_t RxMode) {
	/*--------------------------------------------- Temporary variables for HEX data parsing ---------------------------------------------*/
	static int16_t roll_temp = 0;
	static int16_t pitch_temp = 0;
	static int16_t yaw_temp = 0;

	static int16_t w_temp = 0;
	static int16_t x_temp = 0;
	static int16_t y_temp = 0;
	static int16_t z_temp = 0;

	static int16_t gyrX_temp = 0;
	static int16_t gyrY_temp = 0;
	static int16_t gyrZ_temp = 0;

	static int16_t accX_temp = 0;
	static int16_t accY_temp = 0;
	static int16_t accZ_temp = 0;
	/*-------------------------------------------------------------------------------------------------------------------------------------*/

	ebimuObj->rxMode = RxMode;
//	IOIF_EBIMU_InitUartRxHdlr(ebimuObj);

	uint8_t* selectedBuff = uart7DmaRx1Buff;

	if (ebimuObj->sensorID == 1) {
		selectedBuff = uart7DmaRx1Buff;
	}
	else if (ebimuObj->sensorID == 2) {
		selectedBuff = uart7DmaRx2Buff;
	}
	else if (ebimuObj->sensorID == 3) {
		selectedBuff = uart7DmaRx3Buff;
	}
	else if (ebimuObj->sensorID == 4) {
		selectedBuff = uart7DmaRx4Buff;
	}
	else if (ebimuObj->sensorID == 5) {
		selectedBuff = uart8DmaRx1Buff;
	}
	else if (ebimuObj->sensorID == 6) {
		selectedBuff = uart8DmaRx2Buff;
	}
	else if (ebimuObj->sensorID == 7) {
		selectedBuff = uart8DmaRx3Buff;
	}
	else if (ebimuObj->sensorID == 8) {
		selectedBuff = uart8DmaRx4Buff;
	}
	else if (ebimuObj->sensorID == 9) {
		selectedBuff = uart8DmaRx5Buff;
	}
	else if (ebimuObj->sensorID == 10) {
		selectedBuff = uart8DmaRx6Buff;
	}


	if ( ebimuObj->EBIMU_huart->ErrorCode != 0 ) {
		Reset_UART(ebimuObj->EBIMU_huart);
		return IOIF_EBIMU_STATUS_ERROR;
	}


	HAL_UART_Transmit_DMA(ebimuObj->EBIMU_huart, (uint8_t*)&cmdPolling, 1);
//	HAL_UART_Transmit(ebimuObj->EBIMU_huart, (uint8_t*)&cmdPolling, 1, 1);
//	HAL_UART_Receive_DMA(ebimuObj->EBIMU_huart, selectedBuff, ebimuObj->outputConfig.outputDataByte_HEX);
	HAL_UART_Receive(ebimuObj->EBIMU_huart, selectedBuff, ebimuObj->outputConfig.outputDataByte_HEX, 1);

//	HAL_UART_Receive_DMA(ebimuObj->EBIMU_huart, uart7DmaRx1Buff, ebimuObj->outputConfig.outputDataByte_ASCII);
//	HAL_UART_Receive(ebimuObj->EBIMU_huart, uart7DmaRx1Buff, ebimuObj->outputConfig.outputDataByte_ASCII, IOIF_EBIMU_TX_TIMEOUT);
//	memcpy(ebimuObj->RxBufOriginal, (char*)uart7DmaRx1Buff, ebimuObj->outputConfig.outputDataByte_ASCII);

//	IOIF_EBIMU_us_Delay(IOIF_EBIMU_RX_DELAY);
	/*------------------------------------------------------ UART Ring Buffer parsing ------------------------------------------------------*/
	uint8_t bufSize = IOIF_EBIMU_RX_BUFFER_LENGTH;
	uint8_t cursor = 0;

	/* Find the SOL */
	for (uint8_t i = 0; i < bufSize; i++) {
		if (selectedBuff[i] == 0x55 && selectedBuff[(i+1) % bufSize] == 0x55) {
			cursor = (i + 2) % bufSize;
			break;		// Necessary
		}
	}

	/* Data Parsing */
	switch (ebimuObj->rxMode) {
		case (IOIF_EBIMU_RXMODE_EULER_ONLY):
			roll_temp  	= (selectedBuff[cursor] << 8) | selectedBuff[(cursor+1) % bufSize];
			pitch_temp 	= (selectedBuff[(cursor+2) % bufSize] << 8) | selectedBuff[(cursor+3) % bufSize];
			yaw_temp   	= (selectedBuff[(cursor+4) % bufSize] << 8) | selectedBuff[(cursor+5) % bufSize];

			ebimuObj->eulerAngle.roll 		= (float)(roll_temp) / 100.0f;
			ebimuObj->eulerAngle.pitch 		= (float)(pitch_temp) / 100.0f;
			ebimuObj->eulerAngle.yaw 		= (float)(yaw_temp) / 100.0f;
			break;

		case (IOIF_EBIMU_RXMODE_EULER_ACC):
			roll_temp  	= (selectedBuff[cursor] << 8) | selectedBuff[(cursor+1) % bufSize];
			pitch_temp 	= (selectedBuff[(cursor+2) % bufSize] << 8) | selectedBuff[(cursor+3) % bufSize];
			yaw_temp   	= (selectedBuff[(cursor+4) % bufSize] << 8) | selectedBuff[(cursor+5) % bufSize];
			accX_temp 	= (selectedBuff[(cursor+6) % bufSize] << 8) | selectedBuff[(cursor+7) % bufSize];
			accY_temp 	= (selectedBuff[(cursor+8) % bufSize] << 8) | selectedBuff[(cursor+9) % bufSize];
			accZ_temp 	= (selectedBuff[(cursor+10) % bufSize] << 8) | selectedBuff[(cursor+11) % bufSize];

			ebimuObj->eulerAngle.roll 		= (float)(roll_temp) / 100.0f;
			ebimuObj->eulerAngle.pitch 		= (float)(pitch_temp) / 100.0f;
			ebimuObj->eulerAngle.yaw 		= (float)(yaw_temp) / 100.0f;
			ebimuObj->sensorData.accX 		= (float)(accX_temp) / 1000.0f;
			ebimuObj->sensorData.accY 		= (float)(accY_temp) / 1000.0f;
			ebimuObj->sensorData.accZ 		= (float)(accZ_temp) / 1000.0f;
			break;

		case (IOIF_EBIMU_RXMODE_EULER_GYR):
			roll_temp  	= (selectedBuff[cursor] << 8) | selectedBuff[(cursor+1) % bufSize];
			pitch_temp 	= (selectedBuff[(cursor+2) % bufSize] << 8) | selectedBuff[(cursor+3) % bufSize];
			yaw_temp   	= (selectedBuff[(cursor+4) % bufSize] << 8) | selectedBuff[(cursor+5) % bufSize];
			gyrX_temp 	= (selectedBuff[(cursor+6) % bufSize] << 8) | selectedBuff[(cursor+7) % bufSize];
			gyrY_temp 	= (selectedBuff[(cursor+8) % bufSize] << 8) | selectedBuff[(cursor+9) % bufSize];
			gyrZ_temp 	= (selectedBuff[(cursor+10) % bufSize] << 8) | selectedBuff[(cursor+11) % bufSize];

			ebimuObj->eulerAngle.roll 		= (float)(roll_temp) / 100.0f;
			ebimuObj->eulerAngle.pitch 		= (float)(pitch_temp) / 100.0f;
			ebimuObj->eulerAngle.yaw 		= (float)(yaw_temp) / 100.0f;
			ebimuObj->sensorData.gyrX 		= (float)(gyrX_temp) / 10.0f;
			ebimuObj->sensorData.gyrY 		= (float)(gyrY_temp) / 10.0f;
			ebimuObj->sensorData.gyrZ 		= (float)(gyrZ_temp) / 10.0f;
			break;

		case (IOIF_EBIMU_RXMODE_EULER_GYR_ACC):
			roll_temp  	= (selectedBuff[cursor] << 8) | selectedBuff[(cursor+1) % bufSize];
			pitch_temp 	= (selectedBuff[(cursor+2) % bufSize] << 8) | selectedBuff[(cursor+3) % bufSize];
			yaw_temp   	= (selectedBuff[(cursor+4) % bufSize] << 8) | selectedBuff[(cursor+5) % bufSize];
			gyrX_temp 	= (selectedBuff[(cursor+6) % bufSize] << 8) | selectedBuff[(cursor+7) % bufSize];
			gyrY_temp 	= (selectedBuff[(cursor+8) % bufSize] << 8) | selectedBuff[(cursor+9) % bufSize];
			gyrZ_temp 	= (selectedBuff[(cursor+10) % bufSize] << 8) | selectedBuff[(cursor+11) % bufSize];
			accX_temp 	= (selectedBuff[(cursor+12) % bufSize] << 8) | selectedBuff[(cursor+13) % bufSize];
			accY_temp 	= (selectedBuff[(cursor+14) % bufSize] << 8) | selectedBuff[(cursor+15) % bufSize];
			accZ_temp 	= (selectedBuff[(cursor+16) % bufSize] << 8) | selectedBuff[(cursor+17) % bufSize];

			ebimuObj->eulerAngle.roll 		= (float)(roll_temp) / 100.0f;
			ebimuObj->eulerAngle.pitch 		= (float)(pitch_temp) / 100.0f;
			ebimuObj->eulerAngle.yaw 		= (float)(yaw_temp) / 100.0f;
			ebimuObj->sensorData.gyrX 		= (float)(gyrX_temp) / 10.0f;
			ebimuObj->sensorData.gyrY 		= (float)(gyrY_temp) / 10.0f;
			ebimuObj->sensorData.gyrZ 		= (float)(gyrZ_temp) / 10.0f;
			ebimuObj->sensorData.accX 		= (float)(accX_temp) / 1000.0f;
			ebimuObj->sensorData.accY 		= (float)(accY_temp) / 1000.0f;
			ebimuObj->sensorData.accZ 		= (float)(accZ_temp) / 1000.0f;
			break;

		case (IOIF_EBIMU_RXMODE_QUATERNION_ONLY):
			z_temp  	= (selectedBuff[cursor] << 8) | selectedBuff[(cursor+1) % bufSize];
			y_temp 		= (selectedBuff[(cursor+2) % bufSize] << 8) | selectedBuff[(cursor+3) % bufSize];
			x_temp   	= (selectedBuff[(cursor+4) % bufSize] << 8) | selectedBuff[(cursor+5) % bufSize];
			w_temp 		= (selectedBuff[(cursor+6) % bufSize] << 8) | selectedBuff[(cursor+7) % bufSize];

			ebimuObj->quaternion.z 			= (float)(z_temp) / 10000.0f;
			ebimuObj->quaternion.y 			= (float)(y_temp) / 10000.0f;
			ebimuObj->quaternion.x			= (float)(x_temp) / 10000.0f;
			ebimuObj->quaternion.w			= (float)(w_temp) / 10000.0f;
			break;

		case (IOIF_EBIMU_RXMODE_QUATERNION_GYR_ACC):
			z_temp  	= (selectedBuff[cursor] << 8) | selectedBuff[(cursor+1) % bufSize];
			y_temp 		= (selectedBuff[(cursor+2) % bufSize] << 8) | selectedBuff[(cursor+3) % bufSize];
			x_temp   	= (selectedBuff[(cursor+4) % bufSize] << 8) | selectedBuff[(cursor+5) % bufSize];
			w_temp 		= (selectedBuff[(cursor+6) % bufSize] << 8) | selectedBuff[(cursor+7) % bufSize];
			gyrX_temp 	= (selectedBuff[(cursor+8) % bufSize] << 8) | selectedBuff[(cursor+9) % bufSize];
			gyrY_temp 	= (selectedBuff[(cursor+10) % bufSize] << 8) | selectedBuff[(cursor+11) % bufSize];
			gyrZ_temp 	= (selectedBuff[(cursor+12) % bufSize] << 8) | selectedBuff[(cursor+13) % bufSize];
			accX_temp 	= (selectedBuff[(cursor+14) % bufSize] << 8) | selectedBuff[(cursor+15) % bufSize];
			accY_temp 	= (selectedBuff[(cursor+16) % bufSize] << 8) | selectedBuff[(cursor+17) % bufSize];
			accZ_temp 	= (selectedBuff[(cursor+18) % bufSize] << 8) | selectedBuff[(cursor+19) % bufSize];

			ebimuObj->quaternion.z 			= (float)(z_temp) / 10000.0f;
			ebimuObj->quaternion.y 			= (float)(y_temp) / 10000.0f;
			ebimuObj->quaternion.x			= (float)(x_temp) / 10000.0f;
			ebimuObj->quaternion.w			= (float)(w_temp) / 10000.0f;
			ebimuObj->sensorData.gyrX 		= (float)(gyrX_temp) / 10.0f;
			ebimuObj->sensorData.gyrY 		= (float)(gyrY_temp) / 10.0f;
			ebimuObj->sensorData.gyrZ 		= (float)(gyrZ_temp) / 10.0f;
			ebimuObj->sensorData.accX 		= (float)(accX_temp) / 1000.0f;
			ebimuObj->sensorData.accY 		= (float)(accY_temp) / 1000.0f;
			ebimuObj->sensorData.accZ 		= (float)(accZ_temp) / 1000.0f;
			break;

		default:
			break;
	}


//	HAL_UART_Receive_DMA(ebimuObj->EBIMU_huart, uart7DmaRxBuff, ebimuObj->outputConfig.outputDataByte_ASCII);
//	memcpy(ebimuObj->RxBufOriginal, uart7DmaRxBuff, IOIF_EBIMU_RX_BUFFER_LENGTH);
//	IOIF_EBIMU_RingBuffer(ebimuObj, ebimuObj->RxBufOriginal);
//	if (ebimuObj->eulerAngle.roll > 200 || ebimuObj->eulerAngle.roll < -200 || ebimuObj->eulerAngle.pitch > 100 || ebimuObj->eulerAngle.pitch < -100 || ebimuObj->eulerAngle.yaw > 200 || ebimuObj->eulerAngle.yaw < -200) {
//		errCnt++;
//	}


	timeCheck++;

	return IOIF_EBIMU_STATUS_OK;
};



//static void AlignRxBuffer(uint8_t realRxByte)
//{
//	uint8_t cursor = 0;
//
//	for (uint8_t i = 0; i < realRxByte; i++) {
//		if ((char)(uart7DmaRx1Buff[i]) == SOL) {
//			cursor = i + 1;
//
//			for (uint8_t j = 0; i < realRxByte; j++) {
//				ebimuCurrObj->RxBuf[j] = (char)(uart7DmaRx1Buff[cursor++]);
//				if ((char)(uart7DmaRx1Buff[cursor]) == CR && (char)(uart7DmaRx1Buff[cursor+1]) == LF) {
//					break;
//				}
//			}
//
//			break;
//		}
//	}
//}
//
//
//static void AssignRxData(void)
//{
//	char* tempData = "";
//	uint8_t rxMode = ebimuCurrObj->rxMode;
//
//	switch (rxMode) {
//		case (IOIF_EBIMU_RXMODE_EULER_ONLY):
//			tempData = strtok(ebimuCurrObj->RxBuf, &SP);
//			ebimuCurrObj->eulerAngle.roll = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.pitch = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.yaw = atof(tempData);
//			break;
//
//		case (IOIF_EBIMU_RXMODE_QUATERNION_ONLY):
//			tempData = strtok(ebimuCurrObj->RxBuf, &SP);
//			ebimuCurrObj->quaternion.z = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->quaternion.y = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->quaternion.x = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->quaternion.w = atof(tempData);
//			break;
//
//		case (IOIF_EBIMU_RXMODE_EULER_ACC):
//			tempData = strtok(ebimuCurrObj->RxBuf, &SP);
//			ebimuCurrObj->eulerAngle.roll = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.pitch = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.yaw = atof(tempData);
//
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accX = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accY = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accZ = atof(tempData);
//			break;
//
//		case (IOIF_EBIMU_RXMODE_EULER_GYR):
//			tempData = strtok(ebimuCurrObj->RxBuf, &SP);
//			ebimuCurrObj->eulerAngle.roll = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.pitch = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.yaw = atof(tempData);
//
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrX = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrY = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrZ = atof(tempData);
//			break;
//
//
//		case (IOIF_EBIMU_RXMODE_EULER_ACC_GYR):
//			tempData = strtok(ebimuCurrObj->RxBuf, &SP);
//			ebimuCurrObj->eulerAngle.roll = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.pitch = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->eulerAngle.yaw = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrX = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrY = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrZ = atof(tempData);
//
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accX = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accY = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accZ = atof(tempData);
//			break;
//
//		case (IOIF_EBIMU_RXMODE_QUATERNION_ACC_GYR):
//			tempData = strtok(ebimuCurrObj->RxBuf, &SP);
//			ebimuCurrObj->quaternion.z = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->quaternion.y = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->quaternion.x = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->quaternion.w = atof(tempData);
//
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrX = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrY = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.gyrZ = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accX = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accY = atof(tempData);
//			tempData = strtok(NULL, &SP);
//			ebimuCurrObj->sensorData.accZ = atof(tempData);
//			break;
//
//		default:
//			break;
//	}
//}
//
//

//void UART7_IRQHandler_forEBIMU(void)
//{
//    if (__HAL_UART_GET_FLAG(&huart7, UART_FLAG_IDLE)) {
//        __HAL_UART_CLEAR_IDLEFLAG(&huart7);
//
//        uartDataSize = IOIF_EBIMU_RX_BUFFER_LENGTH;
//        uint8_t rxByte = uartDataSize - __HAL_DMA_GET_COUNTER(huart7.hdmarx);
//
//        AlignRxBuffer(rxByte);
//
//        AssignRxData();
//    }
//}


//uint8_t RxStart = 0;
//uint8_t cnt = 0;
//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
//{
//	if (huart->Instance == UART7) {
//		if ((char)(uart7DmaRx1Buff[0]) == SOL) {
//			RxStart = 1;
//		}
//		else if (RxStart == 1) {
//			RxBuf[cnt] = (char)(uart7DmaRx1Buff[0]);
//			cnt++;
//			if ((char)(uart7DmaRx1Buff[0]) == '\n') {
//				cnt = 0;
//				RxStart = 0;
//			}
//		}
//	}
//}
//void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
//{
//	if (huart->Instance == UART7) {
//		static uint32_t a = 0;
//		a++;
//	}
//}



void IOIF_EBIMU_us_Delay(uint32_t us_delay)
{
	uint32_t tickStart = DWT->CYCCNT;
	uint32_t tickDelay = us_delay * systickMHz;

	if (tickStart > 4294967295 - (us_delay * systickMHz)) {
		uint32_t elapsed = 4294967295 - tickStart;
		uint32_t remainder = tickDelay - elapsed;
		while ( DWT->CYCCNT >= tickStart && DWT->CYCCNT <= 4294967295 )
		{
		}
		while ( DWT->CYCCNT <= remainder)
		{
		}
	}
	else {
	    while ( DWT->CYCCNT - tickStart <= tickDelay )
	    {
	    }
	}
}



void Reset_UART(UART_HandleTypeDef *huart)
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


#endif /* IOIF_EBIMU_9DOFV5_R3_ENABLED */
