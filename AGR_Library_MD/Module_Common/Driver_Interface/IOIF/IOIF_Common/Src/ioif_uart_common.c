

#include "ioif_uart_common.h"

/** @defgroup UART UART
  * @brief UART BSP module driver
  * @{
  */
#ifdef BSP_UART_MODULE_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */




/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */


DMA_HandleTypeDef* UART7_RX_DMA = &hdma_uart7_rx; //Todo
BSP_UARTMap_t UART_Handle;

/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */
static RingBufferStruct uart_rx_buff[IOIF_ASSIGN_UART_PORT_COUNT];
//uint8_t 				rx_packet[IOIF_UART_BUFFER_LENGTH] __attribute__((section(".uart7_dma_rx_buff")));	// MPU area in RAM D3
//uint8_t 				rx_packet[IOIF_UART_BUFFER_LENGTH];
//uint8_t 				rx_packet[30];
uint8_t rx_packet[IOIF_UART_BUFFER_LENGTH] __attribute__((section(".uart7dmaRxBuff")));



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

BSP_UART_t IOIF_UART_CH(uint8_t ch)
{

	BSP_UART_t uart_port;


//	UART_HandleTypeDef* ret = {0,};
//
	//if (ch == 1) return huart1;
	//if (ch == 2) return huart2;
	//if (ch == 3) return huart3;
 //	if (ch == 4) return &huart4;
	//if (ch == 5) return huart5;
	//if (ch == 6) return huart6;
	if (ch == 7) uart_port = BSP_UART7;

	return uart_port;
}

USART_TypeDef* IOIF_UART_CH_Inst(uint8_t ch)
{

	USART_TypeDef* ret = NULL;

	if (ch == 1) return USART1;
	if (ch == 2) return USART2;
	if (ch == 3) return USART3;
	if (ch == 4) return UART4;
	if (ch == 5) return UART5;

	return ret;
}



bool IOIF_UART_Init(uint8_t ch, uint32_t baudrate)
{
	bool ret = false;

//	UART_HandleTypeDef* bsp_uart_ch = IOIF_UART_CH(ch);
//	USART_TypeDef* 		bsp_uart_instance = IOIF_UART_CH_Inst(ch);

	BSP_UART_t uart_port = IOIF_UART_CH(ch);

	/* UART HW initialization */


//	bsp_uart_ch->Instance = bsp_uart_instance;
//	bsp_uart_ch->Init.BaudRate = baudrate;

#ifdef _USE_MCU_ST_H7
	/*
	bsp_uart_ch->Init.WordLength = UART_WORDLENGTH_8B;
	bsp_uart_ch->Init.StopBits = UART_STOPBITS_1;
	bsp_uart_ch->Init.Parity = UART_PARITY_NONE;
	bsp_uart_ch->Init.Mode = UART_MODE_TX_RX;
	bsp_uart_ch->Init.HwFlowCtl = UART_HWCONTROL_NONE;
	bsp_uart_ch->Init.OverSampling = UART_OVERSAMPLING_16;
	bsp_uart_ch->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	bsp_uart_ch->Init.ClockPrescaler = UART_PRESCALER_DIV1;
	bsp_uart_ch->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	*/


	/*BSP_InitUART(uart_port);
	BSP_SetFifoThUARTEx(uart_port, UART_TXFIFO_THRESHOLD_1_8, BSP_UARTEX_SETTXFIFOTHRESHOLD);
	BSP_SetFifoThUARTEx(uart_port, UART_TXFIFO_THRESHOLD_1_8, BSP_UARTEX_SETRXFIFOTHRESHOLD);
	BSP_FifoUARTEx(uart_port, BSP_UARTEX_DISABLEFIFOMODE);*/


	if(BSP_InitUART(uart_port) != BSP_OK)
		ret = false;
	else
		ret = true;
	if (BSP_SetFifoThUARTEx(uart_port, UART_TXFIFO_THRESHOLD_1_8, BSP_UARTEX_SETTXFIFOTHRESHOLD) != BSP_OK)
		ret = false;
	else
		ret = true;
	if (BSP_SetFifoThUARTEx(uart_port, UART_TXFIFO_THRESHOLD_1_8, BSP_UARTEX_SETRXFIFOTHRESHOLD) != BSP_OK)
			ret = false;
	else
		ret = true;
	if (BSP_FifoUARTEx(uart_port, BSP_UARTEX_DISABLEFIFOMODE) != BSP_OK)
		ret = false;
	else
		ret = true;
#endif

#ifdef _USE_MCU_ST_L5

	/*
	  huart4.Init.WordLength = UART_WORDLENGTH_8B;
	  huart4.Init.StopBits = UART_STOPBITS_1;
	  huart4.Init.Parity = UART_PARITY_NONE;
	  huart4.Init.Mode = UART_MODE_TX_RX;
	  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
	  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	  */
	  if (BSP_InitUART(&huart4) != HAL_OK)
	  {
		  ret = false;
	  }
	  if (BSP_SetFifoThUARTEx(&huart4, UART_TXFIFO_THRESHOLD_1_8, BSP_UARTEX_SETTXFIFOTHRESHOLD) != HAL_OK)
	  {
		  ret = false;
	  }
	  if (BSP_SetFifoThUARTEx(&huart4, UART_RXFIFO_THRESHOLD_1_8, BSP_UARTEX_SETRXFIFOTHRESHOLD) != HAL_OK)
	  {
		  ret = false;
	  }
	  if (BSP_FifoUARTEx(&huart4, BSP_UARTEX_DISABLEFIFOMODE) != HAL_OK)
	  {
		  ret = false;
	  }
	  else
		  ret = true;
#endif

	return ret;
}


bool IOIF_UART_Deinit(uint8_t ch)
{
	bool ret = true;

	BSP_UART_t uart_port = IOIF_UART_CH(ch);
	BSP_DeInitUART(uart_port);

	return ret;
}


uint8_t IOIF_UART_Start(uint8_t ch, uint8_t mode, uint32_t baudrate)
{
	uint8_t 	ret = IOIF_UART_START_OK;
	uint32_t 	i=0;
	uint32_t 	ring_buff_index=IOIF_ASSIGN_UART_PORT_COUNT % ch - 1;		//사용중인 UART 의 최대 갯수와 실제 사용하는 UART channel 번호와 ring buffer index 맞추기
	BSP_UART_t uart_port = IOIF_UART_CH(ch);
    uint8_t tx_data  = 0;

//	UART_HandleTypeDef* bsp_uart_ch = _UART_CH(ch);

	memset(&rx_packet[0], 0, sizeof(rx_packet));				//Receive Buffer initialization

	for (i=0 ; i<=ring_buff_index; i++)							//UART 수신용 Ring Buffer 생성, UART DMA circular mode 를 사용함.
	{
		/* Ring Buffer 생성 시 buffer 용량은 충분히 크게 할 것!	\
			Push(인입) 와 pop(인출) 의 속도 차이에 따라 data overrap 현상이 발생할 수 있음! */
		RingBufferCreate(&uart_rx_buff[i], &rx_packet[0], IOIF_UART_BUFFER_LENGTH);
	}

	if(IOIF_UART_Init(ch, baudrate) != true)
	{
		return IOIF_UART_INIT_FAIL;
	}

	switch(mode)
	{
		case IOIF_UART_MODE_POLLING:
			/* TODO : implement, 하지만 polling 은 부적절함. */
		break;

		case IOIF_UART_MODE_IT:	/* Received IT 의 경우 수신받는 데이터가 많을 경우 지속적인 인터럽트가 발생함으로써 concurrent 성능을 저하시키고, \
						UART Transmit 를 할 때 Received IT disable 해주지 않으면 데이터 송신 시 장애를 일으킬 수 있음!*/
			if(BSP_RunUARTIT(uart_port, &tx_data, (uint8_t *)&rx_packet[0], sizeof(rx_packet), BSP_UART_RECEIVE_IT) != BSP_OK)							//1byte receive
				ret = IOIF_UART_START_FAIL;
		break;

		case IOIF_UART_MODE_DMA:
			if(BSP_RunUARTDMA(uart_port, &tx_data, (uint8_t *)&rx_packet[0], sizeof(rx_packet), BSP_UART_RECEIVE_DMA) != BSP_OK)
				ret = IOIF_UART_START_FAIL;
			//Todo:
//			if (HAL_UART_Receive_DMA(&huart7, (uint8_t *)&rx_packet[0], sizeof(rx_packet)) != HAL_OK) {ret = IOIF_UART_START_FAIL;}
			/* DMA buffer 와 ring buffer 의 synchronization 필요, DMA 의 CNTDR (STM32H743 의 경우 SxNDTR 임!) 의 경우 index 가 decrease 함!\
				Normal 인 경우 index 가 0이 될 시 stop, circular mode 에서는 auto reloaded 되어 다시 전송을 시작한다 */

			uart_rx_buff[ring_buff_index].head = uart_rx_buff[ring_buff_index].length - ((BDMA_Channel_TypeDef*)UART7_RX_DMA->Instance)->CNDTR;
			uart_rx_buff[ring_buff_index].tail = uart_rx_buff[ring_buff_index].head;	// 초기 수신 DMA 실행 시 dummy data 가 들어오면 flush 처리
		break;
	}
	return ret;
}


uint8_t	IOIF_UART_Stop(uint8_t ch, uint8_t mode, uint32_t baudrate)
{
	uint8_t ret = 0;

	switch(mode)
		{
			case IOIF_UART_MODE_POLLING:
				/* TODO : implement */
			break;

			case IOIF_UART_MODE_IT:
				/* TODO : implement */
			break;

			case IOIF_UART_MODE_DMA:
				/* TODO : implement */
			break;
		}

	return ret;
}


uint8_t IOIF_UART_Read(uint8_t ch)
{
	uint8_t  ret = 0;
	uint32_t ring_buff_index = IOIF_ASSIGN_UART_PORT_COUNT % ch - 1;		//사용중인 UART 의 최대 갯수와 실제 사용하는 UART channel 번호와 ring buffer index 맞추기

	RingBufferPop(&uart_rx_buff[ring_buff_index], &ret, 1);		//ring buffer 로부터 1바이트 읽기

	return ret;
}


bool IOIF_UART_Write(uint8_t ch, uint8_t mode, uint8_t *data, uint32_t length)
{
	bool 	ret 	 = false;
	uint8_t time_out = 100;

//	UART_HandleTypeDef* bsp_uart_ch = _UART_CH(ch);
	BSP_UART_t uart_port = IOIF_UART_CH(ch);

	uint8_t rx_packet[0] = {};

	switch(mode)
	{
		case IOIF_UART_MODE_POLLING:
			if (BSP_RunUARTBlock(uart_port, data, (uint8_t *)&rx_packet[0], length, time_out, BSP_UART_TRANSMIT) == BSP_OK) {
				ret = true;}
			break;

		case IOIF_UART_MODE_IT:
			/* TODO : implement */
			break;

		case IOIF_UART_MODE_DMA:
			/* TODO : implement */
			BSP_RunUARTDMA(uart_port, data, (uint8_t *)&rx_packet[0], length, BSP_UART_TRANSMIT_DMA);
//			hal_status = BSP_RunUARTDMA(uart_port, data, (uint8_t *)&rx_packet[0], 1, BSP_UART_TRANSMIT_DMA);
			break;
	}

	return ret;
}


uint32_t IOIF_UART_IsReady(uint8_t ch)
{
	uint32_t ret = 0;

	uint32_t ring_buff_index = IOIF_ASSIGN_UART_PORT_COUNT % ch - 1;		//사용중인 UART 의 최대 갯수와 실제 사용하는 UART channel 번호와 ring buffer index 맞추기
	uint32_t buffer_length=0;

	uart_rx_buff[ring_buff_index].head = uart_rx_buff[ring_buff_index].length - ((BDMA_Channel_TypeDef*)UART7_RX_DMA->Instance)->CNDTR;	// index 업데이트

	buffer_length = RingBufferIsAvailable(&uart_rx_buff[ring_buff_index]);

	if(buffer_length > 0)
			return ret = buffer_length;

	return ret;
}


uint32_t IOIF_UART_GetBaudrate(uint8_t ch)
{
	uint32_t ret = 0;

//	UART_HandleTypeDef* bsp_uart_ch = _UART_CH(ch);

	return ret = UART_Handle.handle->Init.BaudRate;

}


bool IOIF_UART_SetBaudrate(uint8_t ch, uint32_t baudrate)
{
	bool ret = true;

	BSP_UART_t uart_port = IOIF_UART_CH(ch);
	uint32_t 			ring_buff_index=IOIF_ASSIGN_UART_PORT_COUNT % ch - 1;

	IOIF_UART_Deinit(ch);
	IOIF_UART_Init(ch, baudrate);

	uint8_t tx_data;

	/* Re-init. 후 다시 DMA RX start */

/*
	if(BSP_RunUARTDMA(uart_port, &tx_data, (uint8_t *)&rx_packet[0], 1, BSP_UART_RECEIVE_DMA) != HAL_OK)
			ret = IOIF_UART_START_FAIL;
*/
	BSP_RunUARTDMA(uart_port, &tx_data, (uint8_t *)&rx_packet[0], sizeof(rx_packet), BSP_UART_RECEIVE_DMA);

	uart_rx_buff[ring_buff_index].head = uart_rx_buff[ring_buff_index].length - ((BDMA_Channel_TypeDef*)UART7_RX_DMA->Instance)->CNDTR;
	uart_rx_buff[ring_buff_index].tail = uart_rx_buff[ring_buff_index].head;

	return ret;
}


bool IOIF_UART_RX_BufferFlush(uint8_t ch)
{
	bool ret = false;

	uint32_t ring_buff_index = IOIF_ASSIGN_UART_PORT_COUNT % ch - 1;

	if(RingBufferFlush(&uart_rx_buff[ring_buff_index]) == true)
		ret = true;

	return ret;
}

bool UART_TEST_EY (uint8_t ch, uint32_t baudrate, uint8_t *pData, uint16_t size){

	static bool ey = false;
	//static uint8_t mode = IOIF_UART_MODE_DMA;

	IOIF_UART_Deinit(ch);

//	ey = IOIF_UART_Init(ch, baudrate);
//	if (HAL_UART_Init(&huart7)== HAL_OK) {ey = true;}
//	memset(&rx_packet[0], 0, sizeof(rx_packet));

	if (IOIF_UART_Start(ch, IOIF_UART_MODE_DMA, baudrate) != 0) {ey = false;}
//	if (HAL_UART_Receive_DMA(&huart7, (uint8_t *)&rx_packet[0], sizeof(rx_packet)) != HAL_OK) {ey = false;}
	IOIF_UART_Read(ch);

	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET); //essential
	ey = IOIF_BLE_UART_Write(pData, size);
//	if (HAL_UART_Transmit(&huart7, pData, size, 100)!= HAL_OK) {ey = false;}



	return ey;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* BSP_UART_MODULE_ENABLED */