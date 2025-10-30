/*
 * ms5607-02ba03.c
 *
 *  Created on: Dec 31, 2024
 *      Author: INVINCIBLE_1NE
 */


#include "ioif_ms5607_02ba03.h"


/* SPI Transmission Data */
static uint8_t SPITxData;

/* OSR setting */
static uint8_t pressureOSR = IOIF_OSR_256;
static uint8_t temperatureOSR = IOIF_OSR_256;


IOIF_pMMG_State_t IOIF_pMMG_Init(IOIF_pMMG_Obj_t* pMMG_Obj, BSP_SPI_t hspi, BSP_GPIOPort_t GPIOx, uint16_t GPIO_Pin) {
	pMMG_Obj->pMMG_hspi = hspi;
	pMMG_Obj->pMMG_CS_GPIO_Port = GPIOx;
	pMMG_Obj->pMMG_CS_Pin = GPIO_Pin;

	IOIF_pMMG_EnableCS(pMMG_Obj);
	SPITxData = RESET_CMD;
	BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
//	HAL_Delay(3);
	IOIF_pMMG_us_Delay(3000);
	IOIF_pMMG_DisableCS(pMMG_Obj);

	IOIF_pMMG_ReadPROM(pMMG_Obj);

	if (pMMG_Obj->promData.reserved == 0x00 || pMMG_Obj->promData.reserved == 0xFF) {
		return IOIF_pMMG_STATE_ERROR;
	}
	else {
		return IOIF_pMMG_STATE_OK;
	}
}


/* Reading the PROM data */
void IOIF_pMMG_ReadPROM(IOIF_pMMG_Obj_t* pMMG_Obj) {
	uint8_t address;
	uint8_t promPtr[16];
	uint8_t* cursor = promPtr;

	for (address = 0; address < 8; address++) {
		SPITxData = PROM_READ(address);
		IOIF_pMMG_EnableCS(pMMG_Obj);
		BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
		BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, NULL, cursor, 2, 10, BSP_SPI_RECEIVE);
		IOIF_pMMG_DisableCS(pMMG_Obj);

		cursor += 2;
	}

//	/* Byte swap on 16-bit integers */
//	cursor = promPtr;
//	for (address = 0; address < 8; address++) {
//		uint8_t* toSwap = cursor;
//		uint8_t secondByte = toSwap[0];
//		toSwap[0] = toSwap[1];
//		toSwap[1] = secondByte;
//
//		cursor += 2;
//	}

	/* Set corresponding data */
	pMMG_Obj->promData.reserved = (uint16_t)( ((uint16_t)promPtr[0] << 8) + (uint16_t)promPtr[1] );
	pMMG_Obj->promData.sens 	= (uint16_t)( ((uint16_t)promPtr[2] << 8) + (uint16_t)promPtr[3] );			// Typ : 46372
	pMMG_Obj->promData.off 		= (uint16_t)( ((uint16_t)promPtr[4] << 8) + (uint16_t)promPtr[5] );			// Typ : 43981
	pMMG_Obj->promData.tcs 		= (uint16_t)( ((uint16_t)promPtr[6] << 8) + (uint16_t)promPtr[7] );			// Typ : 29059
	pMMG_Obj->promData.tco 		= (uint16_t)( ((uint16_t)promPtr[8] << 8) + (uint16_t)promPtr[9] );			// Typ : 27842
	pMMG_Obj->promData.tref 	= (uint16_t)( ((uint16_t)promPtr[10] << 8) + (uint16_t)promPtr[11] );		// Typ : 31553
	pMMG_Obj->promData.tempsens = (uint16_t)( ((uint16_t)promPtr[12] << 8) + (uint16_t)promPtr[13] );		// Typ : 28165
	pMMG_Obj->promData.crc  	= (uint16_t)( ((uint16_t)promPtr[14] << 8) + (uint16_t)promPtr[15] );
}




/* Reading the uncompensated data */
void IOIF_pMMG_ReadUncompValue(IOIF_pMMG_Obj_t* pMMG_Obj) {

	/* Data buffer for sensor replies */
	uint8_t rxData[3];

	/* ---------------------------------------------------------------------------- */

	/* (1) Pressure part */
	IOIF_pMMG_EnableCS(pMMG_Obj);

	/* Setting the OSR */
	SPITxData = CONVERT_D1_OSR_DEFAULT_CMD | pressureOSR;
	BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);

	if (pressureOSR == 0x00) {				// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (pressureOSR == 0x02) {			// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (pressureOSR == 0x04) {			// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (pressureOSR == 0x06) {			// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {									// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, NULL, rxData, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj);

	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj->uncompData.uncompPressure = ( ((uint32_t) rxData[0] << 16) | ((uint32_t) rxData[1] << 8) | ((uint32_t) rxData[2]) );


	/* ---------------------------------------------------------------------------- */


	/* (2) Temperature part */
	IOIF_pMMG_EnableCS(pMMG_Obj);

	/* Setting the OSR */
	SPITxData = CONVERT_D2_OSR_DEFAULT_CMD | temperatureOSR;
	BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);

	if (temperatureOSR == 0x00) {			// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (temperatureOSR == 0x02) {		// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (temperatureOSR == 0x04) {		// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (temperatureOSR == 0x06) {		// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {									// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj->pMMG_hspi, NULL, rxData, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj);


	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj->uncompData.uncompTemperature = ( ((uint32_t) rxData[0] << 16) | ((uint32_t) rxData[1] << 8) | ((uint32_t) rxData[2]) );
}


/* Data Conversion */
void IOIF_pMMG_Convert(IOIF_pMMG_Obj_t* pMMG_Obj) {
	int32_t dT;
	int32_t TEMP;
	int64_t OFF;
	int64_t SENS;

	dT = pMMG_Obj->uncompData.uncompTemperature - ( (int32_t)(pMMG_Obj->promData.tref << 8) );
	TEMP = 2000 + ( ((int64_t)dT * pMMG_Obj->promData.tempsens) >> 23 );
	OFF = ( ((int64_t)pMMG_Obj->promData.off) << 17 ) + ( ((int64_t)pMMG_Obj->promData.tco * dT) >> 6 );
	SENS = ( ((int64_t)pMMG_Obj->promData.sens) << 16 ) + ( ((int64_t)pMMG_Obj->promData.tcs * dT) >> 7 );


	if (TEMP < 2000) {
		int32_t T2 = ( ((int64_t)dT * (int64_t)dT) >> 31 );
		int32_t TEMPM = TEMP - 2000;
		int64_t OFF2 = ( (61 * (int64_t)TEMPM * (int64_t)TEMPM) >> 4 );
		int64_t SENS2 = ( 2 * (int64_t)TEMPM * (int64_t)TEMPM );

		if (TEMP < -1500) {
			int32_t TEMPP = TEMP + 1500;
			int32_t TEMPP2 = TEMPP * TEMPP;
		    OFF2 = OFF2 + (int64_t)15 * TEMPP2;
		    SENS2 = SENS2 + (int64_t)8 * TEMPP2;
		}
	    TEMP -=  T2;
	    OFF  -=  OFF2;
	    SENS -=  SENS2;
	}

	pMMG_Obj->pMMGData.pressure = ( ((((int64_t)pMMG_Obj->uncompData.uncompPressure * SENS) >> 21) - OFF ) >> 15 );
	pMMG_Obj->pMMGData.temperature = TEMP;
}


/* Update the pMMG sensor reading */
void IOIF_pMMG_Update(IOIF_pMMG_Obj_t* pMMG_Obj) {
	IOIF_pMMG_ReadUncompValue(pMMG_Obj);
	IOIF_pMMG_Convert(pMMG_Obj);

	pMMG_Obj->pMMGData.pressureKPa = ( (double)(pMMG_Obj->pMMGData.pressure) / 1000.0 );
	pMMG_Obj->pMMGData.temperatureC = ( (double)(pMMG_Obj->pMMGData.temperature) / 100.0 );
}


/* Enable CS Pin */
void IOIF_pMMG_EnableCS(IOIF_pMMG_Obj_t* pMMG_Obj) {
	BSP_WriteGPIOPin(pMMG_Obj->pMMG_CS_GPIO_Port, pMMG_Obj->pMMG_CS_Pin, BSP_GPIO_PIN_RESET);
}

/* Disable CS Pin */
void IOIF_pMMG_DisableCS(IOIF_pMMG_Obj_t* pMMG_Obj) {
	BSP_WriteGPIOPin(pMMG_Obj->pMMG_CS_GPIO_Port, pMMG_Obj->pMMG_CS_Pin, BSP_GPIO_PIN_SET);
}


void IOIF_pMMG_us_Delay(uint32_t us_delay)
{
	uint32_t tickStart = DWT->CYCCNT;
	uint32_t tickDelay = us_delay * sysMHz;

	if (tickStart > 4294967295 - (us_delay * sysMHz)) {
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











/* -----------------------------------------------------------------------  Multiple pMMG in multiple SPI ports version  ----------------------------------------------------------------------------------- */

void IOIF_pMMG_ReadUncompValue_multiple_3(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3) {

	/* Data buffer for sensor replies */
	uint8_t rxData1[3];
	uint8_t rxData2[3];
	uint8_t rxData3[3];

	/* ---------------------------------------------------------------------------- */

	/* (1) Pressure part */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);

	/* Setting the OSR */
	SPITxData = CONVERT_D1_OSR_DEFAULT_CMD | pressureOSR;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);

	if (pressureOSR == 0x00) {				// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (pressureOSR == 0x02) {			// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (pressureOSR == 0x04) {			// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (pressureOSR == 0x06) {			// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {									// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, NULL, rxData1, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, NULL, rxData2, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, NULL, rxData3, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);

	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj1->uncompData.uncompPressure = ( ((uint32_t) rxData1[0] << 16) | ((uint32_t) rxData1[1] << 8) | ((uint32_t) rxData1[2]) );
	pMMG_Obj2->uncompData.uncompPressure = ( ((uint32_t) rxData2[0] << 16) | ((uint32_t) rxData2[1] << 8) | ((uint32_t) rxData2[2]) );
	pMMG_Obj3->uncompData.uncompPressure = ( ((uint32_t) rxData3[0] << 16) | ((uint32_t) rxData3[1] << 8) | ((uint32_t) rxData3[2]) );
	/* ---------------------------------------------------------------------------- */


	/* (2) Temperature part */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);

	/* Setting the OSR */
	SPITxData = CONVERT_D2_OSR_DEFAULT_CMD | temperatureOSR;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);

	if (temperatureOSR == 0x00) {				// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (temperatureOSR == 0x02) {			// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (temperatureOSR == 0x04) {			// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (temperatureOSR == 0x06) {			// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {										// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, NULL, rxData1, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, NULL, rxData2, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, NULL, rxData3, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);


	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj1->uncompData.uncompTemperature = ( ((uint32_t) rxData1[0] << 16) | ((uint32_t) rxData1[1] << 8) | ((uint32_t) rxData1[2]) );
	pMMG_Obj2->uncompData.uncompTemperature = ( ((uint32_t) rxData2[0] << 16) | ((uint32_t) rxData2[1] << 8) | ((uint32_t) rxData2[2]) );
	pMMG_Obj3->uncompData.uncompTemperature = ( ((uint32_t) rxData3[0] << 16) | ((uint32_t) rxData3[1] << 8) | ((uint32_t) rxData3[2]) );
}




void IOIF_pMMG_Update_multiple_3(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3) {
	IOIF_pMMG_ReadUncompValue_multiple_3(pMMG_Obj1, pMMG_Obj2, pMMG_Obj3);
	IOIF_pMMG_Convert(pMMG_Obj1);
	IOIF_pMMG_Convert(pMMG_Obj2);
	IOIF_pMMG_Convert(pMMG_Obj3);

	pMMG_Obj1->pMMGData.pressureKPa = ( (double)(pMMG_Obj1->pMMGData.pressure) / 1000.0 );
	pMMG_Obj1->pMMGData.temperatureC = ( (double)(pMMG_Obj1->pMMGData.temperature) / 100.0 );

	pMMG_Obj2->pMMGData.pressureKPa = ( (double)(pMMG_Obj2->pMMGData.pressure) / 1000.0 );
	pMMG_Obj2->pMMGData.temperatureC = ( (double)(pMMG_Obj2->pMMGData.temperature) / 100.0 );

	pMMG_Obj3->pMMGData.pressureKPa = ( (double)(pMMG_Obj3->pMMGData.pressure) / 1000.0 );
	pMMG_Obj3->pMMGData.temperatureC = ( (double)(pMMG_Obj3->pMMGData.temperature) / 100.0 );
}



void IOIF_pMMG_ReadUncompValue_multiple_2(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2) {

	/* Data buffer for sensor replies */
	uint8_t rxData1[3];
	uint8_t rxData2[3];

	/* ---------------------------------------------------------------------------- */

	/* (1) Pressure part */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);

	/* Setting the OSR */
	SPITxData = CONVERT_D1_OSR_DEFAULT_CMD | pressureOSR;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);


	if (pressureOSR == 0x00) {				// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (pressureOSR == 0x02) {			// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (pressureOSR == 0x04) {			// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (pressureOSR == 0x06) {			// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {									// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, NULL, rxData1, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, NULL, rxData2, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);

	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj1->uncompData.uncompPressure = ( ((uint32_t) rxData1[0] << 16) | ((uint32_t) rxData1[1] << 8) | ((uint32_t) rxData1[2]) );
	pMMG_Obj2->uncompData.uncompPressure = ( ((uint32_t) rxData2[0] << 16) | ((uint32_t) rxData2[1] << 8) | ((uint32_t) rxData2[2]) );
	/* ---------------------------------------------------------------------------- */


	/* (2) Temperature part */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);

	/* Setting the OSR */
	SPITxData = CONVERT_D2_OSR_DEFAULT_CMD | temperatureOSR;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);

	if (temperatureOSR == 0x00) {				// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (temperatureOSR == 0x02) {			// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (temperatureOSR == 0x04) {			// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (temperatureOSR == 0x06) {			// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {										// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, NULL, rxData1, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, NULL, rxData2, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);

	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj1->uncompData.uncompTemperature = ( ((uint32_t) rxData1[0] << 16) | ((uint32_t) rxData1[1] << 8) | ((uint32_t) rxData1[2]) );
	pMMG_Obj2->uncompData.uncompTemperature = ( ((uint32_t) rxData2[0] << 16) | ((uint32_t) rxData2[1] << 8) | ((uint32_t) rxData2[2]) );
}




void IOIF_pMMG_Update_multiple_2(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2) {
	IOIF_pMMG_ReadUncompValue_multiple_2(pMMG_Obj1, pMMG_Obj2);
	IOIF_pMMG_Convert(pMMG_Obj1);
	IOIF_pMMG_Convert(pMMG_Obj2);

	pMMG_Obj1->pMMGData.pressureKPa = ( (double)(pMMG_Obj1->pMMGData.pressure) / 1000.0 );
	pMMG_Obj1->pMMGData.temperatureC = ( (double)(pMMG_Obj1->pMMGData.temperature) / 100.0 );

	pMMG_Obj2->pMMGData.pressureKPa = ( (double)(pMMG_Obj2->pMMGData.pressure) / 1000.0 );
	pMMG_Obj2->pMMGData.temperatureC = ( (double)(pMMG_Obj2->pMMGData.temperature) / 100.0 );
}



void IOIF_pMMG_ReadUncompValue_multiple_4(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3, IOIF_pMMG_Obj_t* pMMG_Obj4) {

	/* Data buffer for sensor replies */
	uint8_t rxData1[3];
	uint8_t rxData2[3];
	uint8_t rxData3[3];
	uint8_t rxData4[3];

	/* ---------------------------------------------------------------------------- */

	/* (1) Pressure part */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);
	IOIF_pMMG_EnableCS(pMMG_Obj4);

	/* Setting the OSR */
	SPITxData = CONVERT_D1_OSR_DEFAULT_CMD | pressureOSR;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj4->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);


	if (pressureOSR == 0x00) {				// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (pressureOSR == 0x02) {			// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (pressureOSR == 0x04) {			// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (pressureOSR == 0x06) {			// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {									// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);
	IOIF_pMMG_DisableCS(pMMG_Obj4);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);
	IOIF_pMMG_EnableCS(pMMG_Obj4);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, NULL, rxData1, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, NULL, rxData2, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, NULL, rxData3, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj4->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj4->pMMG_hspi, NULL, rxData4, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);
	IOIF_pMMG_DisableCS(pMMG_Obj4);

	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj1->uncompData.uncompPressure = ( ((uint32_t) rxData1[0] << 16) | ((uint32_t) rxData1[1] << 8) | ((uint32_t) rxData1[2]) );
	pMMG_Obj2->uncompData.uncompPressure = ( ((uint32_t) rxData2[0] << 16) | ((uint32_t) rxData2[1] << 8) | ((uint32_t) rxData2[2]) );
	pMMG_Obj3->uncompData.uncompPressure = ( ((uint32_t) rxData3[0] << 16) | ((uint32_t) rxData3[1] << 8) | ((uint32_t) rxData3[2]) );
	pMMG_Obj4->uncompData.uncompPressure = ( ((uint32_t) rxData4[0] << 16) | ((uint32_t) rxData4[1] << 8) | ((uint32_t) rxData4[2]) );
	/* ---------------------------------------------------------------------------- */


	/* (2) Temperature part */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);
	IOIF_pMMG_EnableCS(pMMG_Obj4);

	/* Setting the OSR */
	SPITxData = CONVERT_D2_OSR_DEFAULT_CMD | temperatureOSR;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj4->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);


	if (temperatureOSR == 0x00) {				// 0.54ms
//		HAL_Delay(1);
		IOIF_pMMG_us_Delay(600);
	}
	else if (temperatureOSR == 0x02) {			// 1.06ms
//		HAL_Delay(2);
		IOIF_pMMG_us_Delay(1200);
	}
	else if (temperatureOSR == 0x04) {			// 2.08ms
//		HAL_Delay(3);
		IOIF_pMMG_us_Delay(2200);
	}
	else if (temperatureOSR == 0x06) {			// 4.13ms
//		HAL_Delay(5);
		IOIF_pMMG_us_Delay(4300);
	}
	else {										// 8.22ms
//		HAL_Delay(10);
		IOIF_pMMG_us_Delay(9000);
	}

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);
	IOIF_pMMG_DisableCS(pMMG_Obj4);

	/* Reading 24-bit ADC value */
	IOIF_pMMG_EnableCS(pMMG_Obj1);
	IOIF_pMMG_EnableCS(pMMG_Obj2);
	IOIF_pMMG_EnableCS(pMMG_Obj3);
	IOIF_pMMG_EnableCS(pMMG_Obj4);

	SPITxData = READ_ADC_CMD;
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj1->pMMG_hspi, NULL, rxData1, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj2->pMMG_hspi, NULL, rxData2, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj3->pMMG_hspi, NULL, rxData3, 3, 10, BSP_SPI_RECEIVE);
	BSP_RunSPIBlock(pMMG_Obj4->pMMG_hspi, &SPITxData, NULL, 1, 10, BSP_SPI_TRANSMIT);
	BSP_RunSPIBlock(pMMG_Obj4->pMMG_hspi, NULL, rxData4, 3, 10, BSP_SPI_RECEIVE);

	IOIF_pMMG_DisableCS(pMMG_Obj1);
	IOIF_pMMG_DisableCS(pMMG_Obj2);
	IOIF_pMMG_DisableCS(pMMG_Obj3);
	IOIF_pMMG_DisableCS(pMMG_Obj4);

	/* Convert the 24-bit raw data into a 32-bit useful integer data */
	pMMG_Obj1->uncompData.uncompTemperature = ( ((uint32_t) rxData1[0] << 16) | ((uint32_t) rxData1[1] << 8) | ((uint32_t) rxData1[2]) );
	pMMG_Obj2->uncompData.uncompTemperature = ( ((uint32_t) rxData2[0] << 16) | ((uint32_t) rxData2[1] << 8) | ((uint32_t) rxData2[2]) );
	pMMG_Obj3->uncompData.uncompTemperature = ( ((uint32_t) rxData3[0] << 16) | ((uint32_t) rxData3[1] << 8) | ((uint32_t) rxData3[2]) );
	pMMG_Obj4->uncompData.uncompTemperature = ( ((uint32_t) rxData4[0] << 16) | ((uint32_t) rxData4[1] << 8) | ((uint32_t) rxData4[2]) );
}




void IOIF_pMMG_Update_multiple_4(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3, IOIF_pMMG_Obj_t* pMMG_Obj4) {
	IOIF_pMMG_ReadUncompValue_multiple_4(pMMG_Obj1, pMMG_Obj2, pMMG_Obj3, pMMG_Obj4);
	IOIF_pMMG_Convert(pMMG_Obj1);
	IOIF_pMMG_Convert(pMMG_Obj2);
	IOIF_pMMG_Convert(pMMG_Obj3);
	IOIF_pMMG_Convert(pMMG_Obj4);

	pMMG_Obj1->pMMGData.pressureKPa = ( (double)(pMMG_Obj1->pMMGData.pressure) / 1000.0 );
	pMMG_Obj1->pMMGData.temperatureC = ( (double)(pMMG_Obj1->pMMGData.temperature) / 100.0 );

	pMMG_Obj2->pMMGData.pressureKPa = ( (double)(pMMG_Obj2->pMMGData.pressure) / 1000.0 );
	pMMG_Obj2->pMMGData.temperatureC = ( (double)(pMMG_Obj2->pMMGData.temperature) / 100.0 );

	pMMG_Obj3->pMMGData.pressureKPa = ( (double)(pMMG_Obj3->pMMGData.pressure) / 1000.0 );
	pMMG_Obj3->pMMGData.temperatureC = ( (double)(pMMG_Obj3->pMMGData.temperature) / 100.0 );

	pMMG_Obj4->pMMGData.pressureKPa = ( (double)(pMMG_Obj4->pMMGData.pressure) / 1000.0 );
	pMMG_Obj4->pMMGData.temperatureC = ( (double)(pMMG_Obj4->pMMGData.temperature) / 100.0 );
}
