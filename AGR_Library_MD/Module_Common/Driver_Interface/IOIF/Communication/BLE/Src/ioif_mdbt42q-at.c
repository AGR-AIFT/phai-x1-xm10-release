/**
 *-----------------------------------------------------------
 *                MDBT48Q-AT BLE Slave Device
 *-----------------------------------------------------------
 * @file ioif_mdbt42q-at.c
 * @date Created on: Sep 19, 2023
 * @author AngelRobotics HW Team
 * @brief Code for the MDBT48Q-AT BLE Slave Device.
 *
 *
 * Refer to the MDBT48Q-AT datasheet and related documents for more information.
 * @ref MDBT48Q-AT Datasheet
 */

#include "ioif_mdbt42q-at.h"

/** @defgroup UART UART
  * @brief I2C ICM20608G module driver
  * @{
  */
#ifdef IOIF_MDBT42Q_AT_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */

BLE_IO_CallbackStruct BLE_CallbackStruct = {
		NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */


/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */
static IO_state* _BLE_IO_State;
static uint32_t* hdlr_loop_time = NULL;

static uint8_t  BLE_ACK			 [CMD_BUFFER_INDEX];
static uint8_t	BLE_DataBuff	 [DATA_BUFFER_INDEX];
static uint8_t 	BLE_ATCMDBuff	 [CMD_BUFFER_INDEX];
static uint8_t  BLE_SlaveName	 [NAME_LENGTH_MAX];
static uint8_t 	BLE_SlaveSerialNo[SERIAL_LENGTH_MAX];


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */
bool IsBLEConnected(void);
void sync_time_counter(uint32_t* task_looptime);

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* BLE Device Identifier */
bool IOIF_BLE_Device_Identifier(void)
{
	bool ret = true;

	/* Register BLE Callback Functions */
	BLE_CallbackStruct.BLE_IO_Wait 			 = IOIF_BLE_Async_IO_Wait;
	BLE_CallbackStruct.BLE_UART_Get_Baudrate = IOIF_BLE_UART_GetBaudrate;
	BLE_CallbackStruct.BLE_UART_PD_GPIOWrite = IOIF_BLE_Write_GPIO_State;
	BLE_CallbackStruct.BLE_UART_Read		 = IOIF_BLE_UART_Read;
	BLE_CallbackStruct.BLE_UART_Write		 = IOIF_BLE_UART_Write;
	BLE_CallbackStruct.BLE_UART_Ready		 = IOIF_BLE_UART_IsReady;
	BLE_CallbackStruct.BLE_UART_Set_Baudrate = IOIF_BLE_UART_SetBaudrate;
	BLE_CallbackStruct.BLE_HW_Default		 = IOIF_BLE_HWDefault;

	if (BLE_IO_RegisterCallback(&BLE_CallbackStruct) != true)
		return ret = false;

	/* BLE Module Identifier : 이름 및 Serial Number 설정 */

	MDBT42Q_SetNodeName((uint8_t*)IOIF_BLE_SLAVE_NODENAME, BLE_SlaveName);
	MDBT42Q_SetNodeSerial((uint8_t*)IOIF_BLE_SLAVE_SERIAL, BLE_SlaveSerialNo);

	/* Buffer Init. */

	memset(&BLE_ACK[0], 0, CMD_BUFFER_INDEX);
	memset(&BLE_DataBuff[0], 0, DATA_BUFFER_INDEX);
	memset(&BLE_ATCMDBuff[0], 0, CMD_BUFFER_INDEX);

	/* UART Start */
	if (IOIF_UART_Start(IOIF_UART_PORT_7, IOIF_UART_MODE_DMA, 9600) != IOIF_UART_START_OK)			// Transmit : Normal, Received : DMA
		return ret = false;

	return ret;
}

/* BLE connection check */

bool IsBLEConnected(void)
{
	bool ret = false;

	ret = Is_BT_connected();

	return ret;
}

/* Asynchronous Wait Timer init. */

void sync_time_counter(uint32_t* task_looptime)
{
	hdlr_loop_time = task_looptime;
}


/* BLE Data_Read */

int IOIF_BLE_DataRead(void)
{

//	MDBT42Q_ReceivedData(BSP_UART7, &BLE_DataBuff[0], DATA_BUFFER_INDEX);
#ifdef _NOUSE_OS
	static uint32_t current_time=0;

	if(*hdlr_loop_time - current_time >= 5)		//통신 주기 handling
	{
		MDBT42Q_ReceivedData(BSP_UART7, &BLE_DataBuff[0], DATA_BUFFER_INDEX);
		current_time = *hdlr_loop_time;
	}
#endif

#ifdef _USE_OS
	MDBT42Q_ReceivedData(BSP_UART7, &BLE_DataBuff[0], DATA_BUFFER_INDEX);
#endif
	return 0;
}


int IOIF_BLE_DataWrite_Test(void)
{

#ifdef _NOUSE_OS
	static uint32_t ctime=0;

	if(*hdlr_loop_time - ctime >= 200)  //!!주기 없이 데이터 전송 시 다른 task들 hold 상태됨
	{
	/* BT device 간 연결 상태가 아니라면 BLE data 송신하지 않음*/
	if(IsBLEConnected() != false)
		MDBT42Q_TransmitData((uint8_t*)TEST_SEND_PACKET, sizeof(TEST_SEND_PACKET));
	else
		return 0;				// BT 가 연결 중이 아니라면 데이터 송신 불가..
	ctime = *hdlr_loop_time;

	}
#endif

#ifdef _USE_OS
	if(IsBLEConnected() != false)
			MDBT42Q_TransmitData((uint8_t*)TEST_SEND_PACKET, sizeof(TEST_SEND_PACKET));
	else
		return 0;
#endif

	return 0;
}


bool IOIF_BLE_Device_Init(void)
{
	bool ret = false;

	static bool 	IsBaud9600 = false;		// 현재 BLE module 의 baudrate 가 9600 임을 체크하는 변수
	static uint8_t  state = 0;				// Init 함수의 state machine 변수
	static int32_t  buffer_index=0;			// UART RX Ring buffer 의 index
	static int32_t  start_index=0;			// Ring Buffer 에서 AT CMD 로 값을 copy 할 때의 start index
	static uint8_t  timeout_trial=0;		// Timeout Trial 변수 (최대 3회)


	switch(state)
	{
		case 0:		/* 1. BLE Module 의 현재 baudrate 확인 */
			_BLE_IO_State = MDBT42Q_AT_Read((uint8_t*)&BLE_SLAVE_READ_BAUDRATE[0], sizeof(BLE_SLAVE_READ_BAUDRATE));
			if(_BLE_IO_State->_BLE_state == BLE_IO_COMPLETE)
			{
				buffer_index = MDBT42Q_ReceivedData(BSP_UART7, &BLE_ATCMDBuff[0], CMD_BUFFER_INDEX);
				if ((buffer_index - 1) < 0)																		// AT CMD 를 다 읽으면,
				{
					_BLE_IO_State->_BLE_state = BLE_IO_INIT;
					memcpy(&BLE_ACK[0], &BLE_ATCMDBuff[0], CMD_BUFFER_INDEX);
					if(strncmp((char*)BLE_ACK, (char*) ACK_BAUDRATE9600, sizeof(ACK_BAUDRATE9600)) == 0)		// baud 가 9600 일 경우 state 1로 전환
					{
						memset(&BLE_ATCMDBuff[0], 0, CMD_BUFFER_INDEX);											//CMD buffer clear
						memset(&BLE_ACK[0], 0, CMD_BUFFER_INDEX);												//CMD buffer clear
						state = 1;
					}
					else state = 2;																				// 아무런 응답이 없을 경우 BLE baudrate 가 115200 임을 가정하고 MCU baud rate 설정을 115200 으로 설정하는 state = 2 으로 transition
					//그 외는 알 수 없는 통신 오류 등으로 timeout 처리됨
				}
			}
			break;

		case 1:		/* 2. Baud rate 가 9600 일 경우 BLE baudrate 를 115200 으로 세팅 */
			_BLE_IO_State = MDBT42Q_AT_Write((uint8_t*)&BLE_SLAVE_WRITE_BAUD115200[0], sizeof(BLE_SLAVE_WRITE_BAUD115200), true);
			if(_BLE_IO_State->_BLE_state == BLE_IO_COMPLETE)
			{
				buffer_index = MDBT42Q_ReceivedData(BSP_UART7, &BLE_ATCMDBuff[0], CMD_BUFFER_INDEX);
				if ((buffer_index - 1) < 0)																		// AT CMD 를 다 읽으면,
				{
					_BLE_IO_State->_BLE_state = BLE_IO_INIT;
					memset(&BLE_ATCMDBuff[0], 0, CMD_BUFFER_INDEX);											//CMD buffer clear
					IsBaud9600 = true;
					state = 2;
				}
			}
			break;

		case 2:		/* 3. MCU 의 UART Baudrate 를 115200 으로 변경 */

			if(IOIF_UART_SetBaudrate(BSP_UART7, 115200) == true)
			{
				state = 3;
			}
			else state = BLE_ERROR_STATE;
			break;

		case 3:		/* 4. MCU - BLE Module 간 baudrate 가 115200 인지 확인 */
			_BLE_IO_State = MDBT42Q_AT_Read((uint8_t*)&BLE_SLAVE_READ_BAUDRATE[0], sizeof(BLE_SLAVE_READ_BAUDRATE));
			if(_BLE_IO_State->_BLE_state == BLE_IO_COMPLETE)
				{

					buffer_index = MDBT42Q_ReceivedData(BSP_UART7, &BLE_ATCMDBuff[0], CMD_BUFFER_INDEX);

					if((IsBaud9600 == true) && (start_index == 0))
					{
						start_index = buffer_index - 4;																	// 이전에 저정된 ATCMD 를 load
					}
					else if((IsBaud9600 != true) && (start_index != 0))
						start_index = 0;

					if ((buffer_index - 1) < 0)																			// AT CMD 를 다 읽으면,
					{
						_BLE_IO_State->_BLE_state = BLE_IO_INIT;
						memcpy(&BLE_ACK[0], &BLE_ATCMDBuff[start_index], CMD_BUFFER_INDEX-start_index+4);				// 읽을 때의 index 는, start index 에서 -2 한 값을 를 보간하여 +4 로 설정
						if(strncmp((char*)BLE_ACK, (char*)ACK_BAUDRATE115200, sizeof(ACK_BAUDRATE115200)) == 0)			// baud 가 115200 일 경우 state 4로 전환
						{
							memset(&BLE_ATCMDBuff[0], 0, CMD_BUFFER_INDEX);												//CMD buffer clear
							memset(&BLE_ACK[0], 0, CMD_BUFFER_INDEX);													//CMD buffer clear
							start_index = 0;
							state = 4;
						}
						else state = BLE_ERROR_STATE;																	//Baudrate 가 115200 이 아니라면, error state 로 진입
					}
				}

			break;

		case 4:		/* 5. BLE Module 의 Name 설정 */
			_BLE_IO_State = MDBT42Q_AT_Write((uint8_t*)&BLE_SlaveName, NAME_LENGTH_MAX, false);
			if(_BLE_IO_State->_BLE_state == BLE_IO_COMPLETE)
			{
				_BLE_IO_State->_BLE_state = BLE_IO_INIT;
				state = 5;
			}
			break;

		case 5:		/* 6. BLE Module Serial Number 설정 */
			_BLE_IO_State = MDBT42Q_AT_Write((uint8_t*)&BLE_SlaveSerialNo, SERIAL_LENGTH_MAX, true);
			if(_BLE_IO_State->_BLE_state == BLE_IO_COMPLETE)
			{
				_BLE_IO_State->_BLE_state = BLE_IO_INIT;
				state = 6;
			}
			break;

		case 6:		/* 8. BLE Advertising Stop */

//			_BLE_IO_State = MDBT42Q_AT_Write((uint8_t*)&BLE_SLAVE_WRITE_ADV_STOP, sizeof(BLE_SLAVE_WRITE_ADV_STOP), false);
//			if(_BLE_IO_State->_BLE_state == BLE_IO_COMPLETE)
//			{
//				_BLE_IO_State->_BLE_state = BLE_IO_INIT;
//				CDC_Transmit_FS((uint8_t*)BLE_INIT_OK, 12);
//				state = 0;
				ret = true;
//			}
			break;


		case BLE_ERROR_STATE:	/* Error State : Timeout */
								/* Do nothing */
			break;

		default : break;
	}

	/* 모든 Initialization 과정이 Timeout 내에 실행되지 않으면 Recovery, Default 로 복구 :
	 * 1. HW Default 수행, 2. MCU UART 를 9600 으로 변경
	 * 2. Timeout Trial 은 최대 3회까지
	 * 3. 3회 이후는 BLE device initialization fail 을 return 하고 BLE 기능을 사용하지 않는다 */


	if (*hdlr_loop_time > BLE_INIT_TIMEOUT)
	{
		state = BLE_ERROR_STATE;												// Transition to Error state
		_BLE_IO_State = MDBT42Q_HW_Default();									// HW default 수행
		if(_BLE_IO_State->_BLE_state == BLE_IO_COMPLETE)
		{
			_BLE_IO_State->_BLE_state = BLE_IO_INIT;
			if(IOIF_UART_SetBaudrate(BSP_UART7, 9600) != true)
				timeout_trial++;
			state = 0;															// State Machine 초기화
			*hdlr_loop_time = 0;												// loop time count 초기화
			IOIF_UART_RX_BufferFlush(BSP_UART7);									// UART RX Buffer Flush
			memset(&BLE_ATCMDBuff[0], 0, CMD_BUFFER_INDEX);						// CMD buffer clear
			memset(&BLE_ACK[0], 0, CMD_BUFFER_INDEX);							// CMD buffer clear
		}
	}

	if (timeout_trial > 2)
		return false;

	return ret;
}




/* Callback Functions */

bool IOIF_BLE_Write_GPIO_State (_GPIO_PIN_STATE GPIO_STATE)
{
	bool ret = true;

	BSP_WriteGPIOPin(BSP_GPIO_PORT_E, BLE_UART_EN_Pin, GPIO_STATE);

	return ret;
}

bool IOIF_BLE_HWDefault (_GPIO_PIN_STATE GPIO_STATE)
{
	bool ret = true;

	BSP_WriteGPIOPin(BSP_GPIO_PORT_E, BLE_nDEFAULT_Pin, GPIO_STATE);

	return ret;
}


uint32_t IOIF_BLE_UART_GetBaudrate (void)
{
	uint32_t baud;
	return baud = IOIF_UART_GetBaudrate(BSP_UART7);
}


bool IOIF_BLE_UART_SetBaudrate (uint32_t baudrate)
{
	bool ret = true;

	if(IOIF_UART_SetBaudrate(BSP_UART7, baudrate) != true)
		return false;

	return ret;
}


uint8_t IOIF_BLE_UART_Read(void)
{
	uint8_t rx_buffer;
	return rx_buffer = IOIF_UART_Read(BSP_UART7);
}


bool IOIF_BLE_UART_Write(uint8_t* data, uint32_t length)
{
	bool ret = true;

	if(IOIF_UART_Write(BSP_UART7, IOIF_UART_MODE_POLLING, data, length) != true)
		return false;

	return ret;
}


uint32_t IOIF_BLE_UART_IsReady(void)
{
	return IOIF_UART_IsReady(BSP_UART7);
}


uint8_t IOIF_BLE_Async_IO_Wait(uint32_t ms_wait)
{
	/* Timer - Counter 를 이용한 Asynchronous Wait 함수 */
	static uint8_t state = BLE_IO_WAITING;
	static uint32_t current_time=0;
	static bool start = true;

	if(start == true)
	{
		current_time = *hdlr_loop_time;
		state = BLE_IO_WAITING;
		start = false;
	}

	if((*hdlr_loop_time - current_time) >= ms_wait)
	{
		current_time = *hdlr_loop_time;
		state = BLE_IO_WAIT_DONE;
		start = true;
	}

	/* Get_HALTick() 함수 사용
	 *
	current_time = BSP_millis();
	if(BSP_millis() - current_time > ms_wait)
	{
		current_time = BSP_millis();
		state = BLE_IO_WAIT_DONE;
	}
	*
	*/

	return state;
}


uint8_t IOIF_BLE_Sync_IO_Wait(uint32_t ms_wait)
{
	/* Synchronous Wait 의 경우 일반적인 delay 함수라 별도의 scheduler 없이 사용하지 말 것 */
	uint8_t state = BLE_IO_WAITING;

	//BSP_delay_ms(ms_wait);
	state = BLE_IO_WAIT_DONE;

	return state;
}



/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

#endif /* IOIF_MDBT42Q_AT_ENABLED */


