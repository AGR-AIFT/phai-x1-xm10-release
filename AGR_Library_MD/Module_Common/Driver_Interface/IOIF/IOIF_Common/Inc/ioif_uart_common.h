

#ifndef IOIF_COMMON_INC_IOIF_UART_COMMON_H_
#define IOIF_COMMON_INC_IOIF_UART_COMMON_H_

#include "bsp_uart.h"
#include "module.h"
#include "ring_buffer.h"
#include "ioif_mdbt42q-at.h"

/** @defgroup UART UART
  * @brief UART BSP module driver
  * @{
  */
#ifdef BSP_UART_MODULE_ENABLED

/*MCU Select*/
#define _USE_MCU_ST_H7
//#define _USE_MCU_ST_L5

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

//Todo : eunm change
#define IOIF_ASSIGN_UART_PORT_COUNT 1		// UART port 사용할 갯수 지정

#define IOIF_UART_MODE_POLLING	  1		    // UART 동작 모드: Polling 1, Interrupt 2, DMA 3
#define IOIF_UART_MODE_IT		  2
#define IOIF_UART_MODE_DMA		  3

#define IOIF_UART_PORT_1		  1		    // UART 채널 번호
#define IOIF_UART_PORT_2		  2
#define IOIF_UART_PORT_3		  3
#define IOIF_UART_PORT_4		  4
#define IOIF_UART_PORT_5		  5

#ifdef _USE_MCU_ST_H7
#define IOIF_UART_PORT_6		  6
#define IOIF_UART_PORT_7		  7
#endif

#define IOIF_UART_START_OK		0
#define IOIF_UART_INIT_FAIL		2
#define IOIF_UART_START_FAIL	3

#define IOIF_UART_BUFFER_LENGTH		2056


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

extern UART_HandleTypeDef huart7;
extern DMA_HandleTypeDef hdma_uart7_rx;

BSP_UART_t IOIF_UART_CH(uint8_t ch);
USART_TypeDef* IOIF_UART_CH_Inst(uint8_t ch);
DMA_HandleTypeDef IOIF_UART_DMA_RX(uint8_t ch);


/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

bool IOIF_UART_Init(uint8_t ch, uint32_t baudrate);
bool IOIF_UART_Deinit(uint8_t ch);
uint8_t IOIF_UART_Start(uint8_t ch, uint8_t mode, uint32_t baudrate);
uint8_t	IOIF_UART_Stop(uint8_t ch, uint8_t mode, uint32_t baudrate);

uint8_t IOIF_UART_Read(uint8_t ch);
bool IOIF_UART_Write(uint8_t ch, uint8_t mode, uint8_t *data, uint32_t length);
uint32_t IOIF_UART_IsReady(uint8_t ch);

uint32_t IOIF_UART_GetBaudrate(uint8_t ch);
bool IOIF_UART_SetBaudrate(uint8_t ch, uint32_t baudrate);

bool IOIF_UART_RX_BufferFlush(uint8_t ch);

bool UART_TEST_EY (uint8_t ch, uint32_t baudrate, uint8_t *pData, uint16_t size);


#endif /* BSP_UART_MODULE_ENABLED */

#endif /* IOIF_COMMON_INC_IOIF_UART_COMMON_H_ */
