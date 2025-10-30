/**
 *-----------------------------------------------------------
 *            BM1422AGMV MAGNETOMETER DRIVER HEADER
 *-----------------------------------------------------------
 * @file bm1422agmv.h
 * @date Created on: Jul 27, 2023
 * @author AngelRobotics HW
 * @brief Hardware Interface for BM1422AGMV Magnetometer Sensor.
 * 
 * This header file provides the necessary types, structures, and functions
 * to interface with the BM1422AGMV magnetometer sensor. It defines the
 * sensor's communication methods, data types, and operational functions.
 * 
 * The BM1422AGMV is used to measure magnetic fields in three dimensions.
 * 
 * Refer to the BM1422AGMV datasheet and related documents for more information.
 *
 * @ref BM1422AGMV Datasheet
 */

#ifndef BM1422AGMV_INC_BM1422AGMV_H_
#define BM1422AGMV_INC_BM1422AGMV_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "bm1422agmv_regmap.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define BM1422AGMV_DATA_BUFF_SIZE   32
#define BM1422AGMV_READ_DATA_SIZE   6

#define BM1422AGMV_TOTAL_SENSORS    3

#define BM1422AGMV_TRIALS            10
#define BM1422AGMV_STRAT_UP_DELAY    10
#define BM1422AGMV_TIMEOUT           1


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

// Function pointer types for initialization, deinitialization, and I/O operations.
typedef uint8_t (*BM1422AGMV_InitFunc)			(void);
typedef uint8_t (*BM1422AGMV_DeInitFunc)		(void);
typedef uint8_t (*BM1422AGMV_IsDevReadyFunc)	(uint16_t);    								// DevAddr
typedef uint8_t (*BM1422AGMV_ReadRegFunc)		(uint16_t, uint16_t, uint8_t*, uint16_t);	// DevAddr, RegAddr, Data, DataSize
typedef uint8_t (*BM1422AGMV_WriteRegFunc)		(uint16_t, uint16_t, uint8_t*, uint16_t);	// DevAddr, RegAddr, Data, DataSize
typedef uint8_t (*BM1422AGMV_WaitFunc)			(uint32_t); 								// DevAddr, ms to wait

// Enumeration for BM1422AGMV status codes.
typedef enum _BM1422AGMVState_t {
    BM1422AGMV_STATUS_OK = 0,
    BM1422AGMV_STATUS_ERROR,
    BBM1422AGMV_STATUS_BUSY,
    BM1422AGMV_STATUS_TIMEOUT,
} BM1422AGMVState_t;

// I/O context structure for BM1422AGMV.
typedef struct _BM1422AGMVIOctx_t {
	BM1422AGMV_InitFunc 		Init;
	BM1422AGMV_DeInitFunc 		DeInit;
	BM1422AGMV_IsDevReadyFunc 	IsDevReady;
	BM1422AGMV_ReadRegFunc 		ReadReg;
	BM1422AGMV_WriteRegFunc 	WriteReg;
	BM1422AGMV_WaitFunc 		Wait;
} BM1422AGMVIOctx_t;

// Data structure for BM1422AGMV.
typedef struct _BM1422AGMVData_t {
    float magX;
    float magY;
    float magZ;
} BM1422AGMVData_t;

// Object handle structure for BM1422AGMV.
typedef struct _BM1422AGMVObj_t {
    uint16_t devAddr;               // Device address
    uint8_t isInit;                 // Initialization status
    uint8_t* dataBuff;
    BM1422AGMVData_t magData;		// User data
	BM1422AGMVIOctx_t io;			// IO context
} BM1422AGMVObj_t;


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

// Set the I/O context for the BM1422AGMV object.
uint8_t BM1422AGMV_Init(BM1422AGMVObj_t* bm1422agmv_obj);

// Initialize the BM1422AGMV object.
uint8_t BM1422AGMV_SetIoctx(BM1422AGMVObj_t* bm1422agmv_obj, BM1422AGMVIOctx_t* bm1422agmvIOctx);

// Retrieve the magnet data values from the BM1422AGMV object.
uint8_t BM1422AGMV_GetValue(BM1422AGMVObj_t* bm1422agmv_obj);


#endif /* BM1422AGMV_INC_BM1422AGMV_H_ */
