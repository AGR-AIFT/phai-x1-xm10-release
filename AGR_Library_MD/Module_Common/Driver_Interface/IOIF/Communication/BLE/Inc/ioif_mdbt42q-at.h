/**
 *-----------------------------------------------------------
 *                MDBT48Q-AT BLE Slave Device
 *-----------------------------------------------------------
 * @file ioif_mdbt42q-at.h
 * @date Created on: Sep 19, 2023
 * @author AngelRobotics HW Team
 * @brief Code for the MDBT48Q-AT BLE Slave Device.
 *
 *
 * Refer to the MDBT48Q-AT datasheet and related documents for more information.
 * @ref MDBT48Q-AT Datasheet
 */

#ifndef BLE_INC_IOIF_BLEIF_MDBT42Q_AT_H_
#define BLE_INC_IOIF_BLEIF_MDBT42Q_AT_H_

#include "module.h"

/** @defgroup UART UART
  * @brief I2C ICM20608G module driver
  * @{
  */

#ifdef IOIF_MDBT42Q_AT_ENABLED

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "ioif_usb_common.h"
#include "ioif_uart_common.h"
#include "ioif_gpio_common.h"
#include "mdbt42q-at.h"

#define _NOUSE_OS

#define DATA_BUFFER_INDEX		1024
#define CMD_BUFFER_INDEX		30
#define NAME_LENGTH_MAX			28		//AT+NAME + 20Bytes
#define SERIAL_LENGTH_MAX		19		//AT+SERIALNO + 8bytes
#define ACK_BAUDRATE9600		"0 baudrate9600"
#define ACK_BAUDRATE115200		"4 baudrate115200"
#define BLE_INIT_OK				"Init OK!\r\n"
#define BLE_INIT_TIMEOUT		1000	//10ms * BLE_INIT_TIMEOUT = 10second
#define BLE_ERROR_STATE			99

#define TEST_SEND_PACKET		"Send Test Packet transmit, BLE is connected successfully!!\r\n"

#define BLE_BTN_PRESSED_TIME	300		// 3sec

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/* Basic H/W Definitions */
//Todo : Temporary ioif before the CDI layer made
#define IOIF_BLE_SLAVE_NODENAME				"WISE_CM_L5"		//Max length 20 characters
#define IOIF_BLE_SLAVE_SERIAL				"00000001"			//fixed 8-character width
#define IOIF_BLE_SLAVE_MAC					"000000000001"		//12byte(Hex), MAC address, Written order is from MSB byte to LSB

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */



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


bool IOIF_BLE_Device_Identifier(void);				// BLE Device 에 필요한 환경 변수 설정
bool IOIF_BLE_Device_Init(void);					// BLE Device Initialization 함수
int  IOIF_BLE_DataRead(void);						// Routine 에 사용할 함수, BLE Data Read
int  IOIF_BLE_DataWrite_Test(void);					// Routine 에 사용할 함수, BLE Data Write (Test)
int  IOIF_BLE_StartAdvertising(void);				// Routine 에 사용할 함수, BLE Advertising 처리 함수

/* BLE Callback Functions */
bool 			IOIF_BLE_Write_GPIO_State (_GPIO_PIN_STATE GPIO_STAT);
uint32_t 		IOIF_BLE_UART_GetBaudrate (void);
bool 			IOIF_BLE_UART_SetBaudrate (uint32_t baudrate);
uint8_t 		IOIF_BLE_UART_Read(void);
bool 			IOIF_BLE_UART_Write(uint8_t* data, uint32_t length);
uint32_t 		IOIF_BLE_UART_IsReady(void);
uint8_t 		IOIF_BLE_Async_IO_Wait(uint32_t ms_wait);
uint8_t 		IOIF_BLE_Sync_IO_Wait(uint32_t ms_wait);
bool 			IOIF_BLE_HWDefault (_GPIO_PIN_STATE GPIO_STAT);

#endif /*IOIF_MDBT42Q_AT_ENABLED*/
#endif /* BLE_INC_IOIF_BLEIF_MDBT42Q_AT_H_ */
