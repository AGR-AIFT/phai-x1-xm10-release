/*
 * ms5607-02ba03.h
 *
 *  Created on: Dec 31, 2024
 *      Author: INVINCIBLE_1NE
 */

#ifndef MS5607_02BA03_INC_IOIF_MS5607_02BA03_H_
#define MS5607_02BA03_INC_IOIF_MS5607_02BA03_H_


#include "module.h"

#ifdef IOIF_PMMGSENSOR_ENABLED

#include <string.h>
#include "ioif_spi_common.h"
#include "ioif_gpio_common.h"

/* SPI Commands */
#define RESET_CMD							0x1E
#define PROM_READ(address)          		(0xA0 | ((address) << 1)) 		// Macro to change values for the 8 PROM addresses

#define CONVERT_D1_OSR_DEFAULT_CMD			0x40
#define CONVERT_D1_OSR256_CMD            	0x40
#define CONVERT_D1_OSR512_CMD            	0x42
#define CONVERT_D1_OSR1024_CMD            	0x44
#define CONVERT_D1_OSR2048_CMD            	0x46
#define CONVERT_D1_OSR4096_CMD            	0x48

#define CONVERT_D2_OSR_DEFAULT_CMD			0x50
#define CONVERT_D2_OSR256_CMD            	0x50
#define CONVERT_D2_OSR512_CMD            	0x52
#define CONVERT_D2_OSR1024_CMD            	0x54
#define CONVERT_D2_OSR2048_CMD            	0x56
#define CONVERT_D2_OSR4096_CMD            	0x58

#define READ_ADC_CMD              			0x00



#define sysMHz								480

/* Oversampling ratio */
typedef enum _IOIF_pMMG_OSR_t {
	IOIF_OSR_256  = 0x00,
	IOIF_OSR_512  = 0x02,
	IOIF_OSR_1024 = 0x04,
	IOIF_OSR_2048 = 0x06,
	IOIF_OSR_4096 = 0x08
} IOIF_pMMG_OSR_t;


/* System states */
typedef enum _IOIF_pMMG_State_t {
	IOIF_pMMG_STATE_OK,
	IOIF_pMMG_STATE_ERROR,
	IOIF_pMMG_STATE_ERROR_1,
	IOIF_pMMG_STATE_ERROR_2,
	IOIF_pMMG_STATE_ERROR_3
} IOIF_pMMG_State_t;


/* PROM data structure */
typedef struct _IOIF_pMMG_PROMData_t {
  uint16_t reserved;
  uint16_t sens;		// Typ : 46372
  uint16_t off;			// Typ : 43981
  uint16_t tcs;			// Typ : 29059
  uint16_t tco;			// Typ : 27842
  uint16_t tref;		// Typ : 31553
  uint16_t tempsens;	// Typ : 28165
  uint16_t crc;
} IOIF_pMMG_PROMData_t;

typedef struct _IOIF_pMMG_UncompData_t {
	uint32_t uncompPressure;		// Typ : 6465444
	uint32_t uncompTemperature;		// Typ : 8077636
} IOIF_pMMG_UncompData_t;

typedef struct _IOIF_pMMG_Data_t {
	int32_t pressure;
	int32_t temperature;

	double pressureKPa;
	double temperatureC;
} IOIF_pMMG_Data_t;

/* pMMG data structure */
typedef struct _IOIF_pMMG_Obj_t {
	BSP_SPI_t pMMG_hspi;
	BSP_GPIOPort_t pMMG_CS_GPIO_Port;

	uint16_t pMMG_CS_Pin;
	IOIF_pMMG_PROMData_t promData;
	IOIF_pMMG_UncompData_t uncompData;
	IOIF_pMMG_Data_t pMMGData;
} IOIF_pMMG_Obj_t;



/* Declaration of functions */
IOIF_pMMG_State_t IOIF_pMMG_Init(IOIF_pMMG_Obj_t* pMMG_Obj, BSP_SPI_t hspi, BSP_GPIOPort_t GPIOx, uint16_t GPIO_Pin);
void IOIF_pMMG_ReadPROM(IOIF_pMMG_Obj_t* pMMG_Obj);
void IOIF_pMMG_ReadUncompValue(IOIF_pMMG_Obj_t* pMMG_Obj);
void IOIF_pMMG_Convert(IOIF_pMMG_Obj_t* pMMG_Obj);
void IOIF_pMMG_Update(IOIF_pMMG_Obj_t* pMMG_Obj);
void IOIF_pMMG_EnableCS(IOIF_pMMG_Obj_t* pMMG_Obj);
void IOIF_pMMG_DisableCS(IOIF_pMMG_Obj_t* pMMG_Obj);
void IOIF_pMMG_us_Delay(uint32_t us_delay);

void IOIF_pMMG_ReadUncompValue_multiple_3(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3);
void IOIF_pMMG_Update_multiple_3(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3);
void IOIF_pMMG_ReadUncompValue_multiple_2(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2);
void IOIF_pMMG_Update_multiple_2(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2);
void IOIF_pMMG_ReadUncompValue_multiple_4(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3, IOIF_pMMG_Obj_t* pMMG_Obj4);
void IOIF_pMMG_Update_multiple_4(IOIF_pMMG_Obj_t* pMMG_Obj1, IOIF_pMMG_Obj_t* pMMG_Obj2, IOIF_pMMG_Obj_t* pMMG_Obj3, IOIF_pMMG_Obj_t* pMMG_Obj4);

#endif /* IOIF_MS5607_02BA03_ENABLED */

#endif /* MS5607_02BA03_INC_MS5607_02BA03_H_ */
