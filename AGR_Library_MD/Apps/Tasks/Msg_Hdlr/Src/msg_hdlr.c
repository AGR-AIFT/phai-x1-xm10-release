/**
 * @file msg_hdlr_task.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics FW Team
 * @brief 
 * @ref 
 */

// TODO : refactoring!!
#include "msg_hdlr.h"

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

MainSequence_Enum MS_enum;
TaskObj_t msg_hdlr_task;
uint8_t exppack_node_id;


/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */
#ifdef FDCAN_WALKONSUIT_ENABLED
static WS5_ExtBoard_Transmit_Data_16BIT WS5_ExtBoard_Transmit_Data_16bit;
static WS5_ExtBoard_Transmit_Data_32BIT WS5_ExtBoard_Transmit_Data_32bit;
uint16_t TIMER_INTERRUPT_PERIOD_MSG = 1000-1;
#endif

static cvector_vector_type(DOP_Header_t) pdo_send_list;
static cvector_vector_type(DOP_Header_t) sdo_req_list;
static cvector_vector_type(DOP_Header_t) sdo_res_list;

static COMMType comm_type;

static uint8_t GUI_onoff;
static uint8_t GUI_command;

//static uint8_t exppack_node_id;
static uint8_t ori_node;
static uint32_t fnc_code;
static uint32_t err_code;
static uint8_t fdcanTxData[64];
static uint8_t fdcanRxData[64];
static int comm_loop_cnt;

static int32_t test_dummy[10];

static float msgTimeElap;

/* ---------------- For Code Time Check ---------------- */
static uint32_t MSGcodeStartTick = 0;
static uint32_t MSGcodeEndTick = 0;
/* ----------------------------------------------- */


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

/* ------------------- STATE FUNCTION ------------------- */
static void StateOff_Ent();

static void StateStandby_Ent();
static void StateStandby_Run();
static void StateStandby_Ext();

static void StateEnable_Ent();
static void StateEnable_Run();
static void StateEnable_Ext();

static void StateError_Run();

/* ------------------- READ NODE ID ------------------- */
static uint8_t Read_Node_ID();

/* ------------------- CONVERT BYTE TO LENGTH ------------------- */
static DOP_Header_t Get_Header(uint8_t* t_byte_arr);

/* ------------------- EMCY ------------------- */
void Send_EMCY(uint32_t* t_err_code);
static void Recv_EMCY(uint8_t* t_byte_arr, uint32_t* t_err_code);
static DOP_SDOArgs_t Convert_Bytes_to_SDO_req(uint8_t* t_byte_arr, uint16_t *t_byte_len);

/* ------------------- SDO RX ------------------- */
static int Read_SDO(uint8_t* t_byte_arr);
static int Unpack_SDO(uint8_t* t_byte_arr);
static int Convert_SDOres_to_Bytes(DOP_Header_t* t_header, uint8_t* t_byte_arr);

/* ------------------- SDO TX ------------------- */
static int Pack_SDO(uint8_t* t_byte_arr, uint8_t* t_byte_len);
static int Send_SDO(uint8_t t_dest_node);
static int Convert_Bytes_to_PDO(uint8_t* t_byte_arr);
static int Unpack_PDO(uint8_t* t_byte_arr);

/* ------------------- PDO RX ------------------- */
static int Convert_PDO_to_Bytes(DOP_Header_t* t_header, uint8_t* t_byte_arr);
static int Pack_PDO(uint8_t* t_byte_arr, uint8_t* t_byte_len);
static int Run_Send_PDO();

/* ------------------- PDO TX ------------------- */
static int Ext_Send_PDO();
static int Set_PDO_Dummy();

/* ------------------- LIST MANAGEMENT ------------------- */
static void Add_PDO_to_Send(uint8_t t_dictID, uint8_t t_objID);
static void Add_SDO_to_Send(uint8_t t_dictID, uint8_t t_objID);
static void Clear_SDO_to_Send();

/* ------------------- MSG HANDLER ------------------- */
static int Fdcan_Rx_Hdlr(uint16_t t_wasp_id, uint8_t* t_rx_data);

/* ------------------- SDO CALLBACK ------------------- */
static void Set_Send_PDO_List(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res);
static void Set_MS_Enum(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res);
static void Set_GUI_COMM_OnOff(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res);
static void Set_GUI_COMM_Command(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res);

#ifdef FDCAN_WALKONSUIT_ENABLED
static int Init_Encoding_WS5_Dataset(void);
static int Run_Encoding_WS5_Dataset(void);
#endif
/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

DOP_COMMON_SDO_CB(msg_hdlr_task)

void InitMsgHdlr()
{
    /*Task Init*/
    InitTask(&msg_hdlr_task);

    exppack_node_id = Read_Node_ID();		// select correct ID
    ori_node = 0x00;

    /* Communication Init */
    comm_type = COMM_TYPE_FDCAN; // Must be Change if use USB
    // comm_type = COMM_TYPE_USB;

    // TODO : Error Handle!
    IOIF_InitFDCAN1(exppack_node_id);

	/* State Definition */
	TASK_CREATE_STATE(&msg_hdlr_task, TASK_STATE_OFF,      StateOff_Ent,        NULL,    			NULL,               false);
	TASK_CREATE_STATE(&msg_hdlr_task, TASK_STATE_STANDBY,  StateStandby_Ent,    StateStandby_Run,	StateStandby_Ext,   true);
	TASK_CREATE_STATE(&msg_hdlr_task, TASK_STATE_ENABLE,   StateEnable_Ent,     StateEnable_Run, 	StateEnable_Ext,   	false);
	TASK_CREATE_STATE(&msg_hdlr_task, TASK_STATE_ERROR,    NULL,   			    StateError_Run,     NULL,   	        false);

    TASK_CREATE_ROUTINE(&msg_hdlr_task,  ROUTINE_ID_MSG_PDO_SEND,           NULL,   Run_Send_PDO,   Ext_Send_PDO);
    TASK_CREATE_ROUTINE(&msg_hdlr_task,  ROUTINE_ID_MSG_PDO_DUMMY_TEST,     NULL,   Set_PDO_Dummy,  NULL);
#ifdef FDCAN_WALKONSUIT_ENABLED
    TASK_CREATE_ROUTINE(&msg_hdlr_task,  ROUTINE_ID_MSG_WS5_PDO_PACK,       Init_Encoding_WS5_Dataset, Run_Encoding_WS5_Dataset, NULL);
#endif

	/* Data Object Definition */
    DOP_CreateDOD(TASK_ID_MSG);

    DOP_COMMON_SDO_CREATE(TASK_ID_MSG)
    DOP_CreateSDO(TASK_ID_MSG, SDO_ID_MSG_PDO_LIST, 				DOP_UINT16, Set_Send_PDO_List);
    DOP_CreateSDO(TASK_ID_MSG, SDO_ID_MSG_MS_ENUM,  				DOP_UINT8,  Set_MS_Enum);
    DOP_CreateSDO(TASK_ID_MSG, SDO_ID_MSG_GUI_COMM_ONOFF,  			DOP_UINT8,  Set_GUI_COMM_OnOff);
    DOP_CreateSDO(TASK_ID_MSG, SDO_ID_MSG_GUI_COMM_COMMAND,  		DOP_UINT8,  Set_GUI_COMM_Command);

	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST1, 					DOP_INT32, 1, &test_dummy[0]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST2, 					DOP_INT32, 1, &test_dummy[1]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST3, 					DOP_INT32, 1, &test_dummy[2]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST4, 					DOP_INT32, 1, &test_dummy[3]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST5, 					DOP_INT32, 1, &test_dummy[4]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST6, 					DOP_INT32, 1, &test_dummy[5]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST7, 					DOP_INT32, 1, &test_dummy[6]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST8, 					DOP_INT32, 1, &test_dummy[7]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST9, 					DOP_INT32, 1, &test_dummy[8]);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_MSG_TEST10, 					DOP_INT32, 1, &test_dummy[9]);
#ifdef FDCAN_WALKONSUIT_ENABLED
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_WS5_EXTBOARD_TRANSMIT_16BIT,  DOP_INT16,  16, &WS5_ExtBoard_Transmit_Data_16bit);
	DOP_CreatePDO(TASK_ID_MSG, PDO_ID_WS5_EXTBOARD_TRANSMIT_32BIT,  DOP_UINT32, 1,  &WS5_ExtBoard_Transmit_Data_32bit);
#endif

    DOP_SetSDOReq(TASK_ID_EXTDEV,  SDO_ID_EXTDEV_EXTENSION_BOARD_ENABLE,     &CM_connect_signal,     DOP_UINT8);
    DOP_SetSDOReq(TASK_ID_EXTDEV,  SDO_ID_EXTDEV_EXTENSION_BOARD_DISABLE,    &CM_disconnect_signal,  DOP_UINT8);

	/* Callback Allocation */
	if (comm_type == COMM_TYPE_FDCAN) {
	    IOIF_SetFDCANRxCB(IOIF_FDCAN1, IOIF_FDCAN_RXFIFO0CALLBACK, Fdcan_Rx_Hdlr);
	} else if (comm_type == COMM_TYPE_USB) {
//	    ioif_usbRxCBPtr = USB_Rx_Hdlr;
	}

	/* Timer 6 Callback Allocation */
	if (IOIF_StartTimIT(IOIF_TIM6) > 0) {
		//TODO: ERROR PROCESS
	}

	IOIF_SetTimCB(IOIF_TIM6, IOIF_TIM_PERIOD_ELAPSED_CALLBACK, RunMsgHdlr, NULL);
}

void RunMsgHdlr(void* params)
{	
    /* Loop Start Time Check */								// pMMG DAQ에서 DWT->CYCCNT 사용하므로, 이 부분 사용 금지
//	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
//	DWT->CYCCNT = 0;
//	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	MSGcodeStartTick = DWT->CYCCNT;

	/* Run Device */
	RunTask(&msg_hdlr_task);
	
	/* Elapsed Time Check */
	MSGcodeEndTick = DWT->CYCCNT;
	if (MSGcodeEndTick < MSGcodeStartTick) {
		msgTimeElap = ((4294967295 - MSGcodeStartTick) + MSGcodeEndTick + 1) / 480;	// in microsecond (Roll-over)
	}
	else {
		msgTimeElap = (MSGcodeEndTick - MSGcodeStartTick) / 480;				// in microsecond
	}
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/* ------------------- STATE FUNCTION ------------------- */
static void StateOff_Ent()
{
	GUI_onoff = 0;
	GUI_command = 0;

    StateTransition(&msg_hdlr_task.stateMachine, TASK_STATE_STANDBY);
}

static void StateStandby_Ent()
{
//	Send_ExtensionBoardDisable();
}

static void StateStandby_Run()
{

}

static void StateStandby_Ext()
{
    
}

static void StateEnable_Ent()
{
	comm_loop_cnt = 0;
	EntRoutines(&msg_hdlr_task.routine);
}

static void StateEnable_Run()
{
	if (comm_loop_cnt % 1000 == 0) {	// For Debug
		HAL_GPIO_TogglePin(GPIOD, LED_BLUE_Pin);
	}

#ifdef FDCAN_WALKONSUIT_ENABLED
//	Run_Send_PDO();
	Run_Encoding_WS5_Dataset();
//	GetRawPMMG_1sample(&ExpPackDataObj, PMMG_1A);
//	GetRawPMMG_1sample(&ExpPackDataObj, PMMG_2A);
//	GetRawPMMG_1sample(&ExpPackDataObj, PMMG_3A);
//	GetRawPMMG_1sample(&ExpPackDataObj, PMMG_4A);
#endif

	RunRoutines(&msg_hdlr_task.routine);
    comm_loop_cnt++;


}

static void StateEnable_Ext()
{
	ExtRoutines(&msg_hdlr_task.routine);

	GUI_onoff = 0;
	GUI_command = 0;
}

static void StateError_Run()
{

}

/* ------------------- READ NODE ID ------------------- */
static uint8_t Read_Node_ID(void)
{
#ifdef FDCAN_WALKONSUIT_ENABLED
	uint8_t nodeID = 20;
#endif
#ifdef FDCAN_H10_ENABLED
	uint8_t nodeID = EXPANSION_BOARD_NODEID;
#endif

	return nodeID;
}

/* ------------------- CONVERT BYTE TO LENGTH ------------------- */

static DOP_Header_t Get_Header(uint8_t* t_byte_arr)
{
	DOP_Header_t t_header = {0};
    memcpy(&t_header, t_byte_arr, sizeof(DOP_Header_t));
    return t_header;
}

/* ------------------- EMCY ------------------- */
void Send_EMCY(uint32_t* t_err_code)
{
    uint8_t* t_tx_data = malloc(sizeof(uint8_t)*4);
    uint16_t t_identifier = EMCY|(exppack_node_id<<4);

    memcpy(t_tx_data, t_err_code, DOP_ERR_CODE_SIZE);

    if(Send_MSG(t_identifier, t_tx_data, 4) != 0){
        //TODO: MSG TX ERROR
    }

    free(t_tx_data);
    t_tx_data = NULL;
}

static void Recv_EMCY(uint8_t* t_byte_arr, uint32_t* t_err_code)
{
    memcpy(t_err_code, t_byte_arr, DOP_ERR_CODE_SIZE);
}

/* ------------------- SDO RX ------------------- */
static DOP_SDOArgs_t Convert_Bytes_to_SDO_req(uint8_t* t_byte_arr, uint16_t *t_byte_len)
{
	DOP_SDOArgs_t t_req = {0};
    *t_byte_len = 0;

    int t_idx = sizeof(t_req.status);
    int t_len = sizeof(t_req.dataSize);

    memcpy(&t_req.dataSize, &t_byte_arr[t_idx], t_len);
    *t_byte_len += t_len;

    t_req.data = &t_byte_arr[t_idx + t_len];

    t_req.status = t_byte_arr[0];
    *t_byte_len += 1;

    return t_req;
}

static int Read_SDO(uint8_t* t_byte_arr)
{
    int t_byte_read = 0;
    DOP_Header_t t_header = Get_Header(t_byte_arr);
    t_byte_read += sizeof(DOP_Header_t);

    DOP_SDO_t* t_sdo = DOP_FindSDO(t_header.dictID, t_header.objID);
    if (t_sdo == NULL) {
        //TODO: Cannot Find SDO ERROR
        return -2;
    }

    uint16_t t_req_bytes = 0;
    DOP_SDOArgs_t t_req = Convert_Bytes_to_SDO_req(t_byte_arr + t_byte_read, &t_req_bytes);
    t_req.typeSize = t_sdo->args.typeSize; // Copy SDO info
    t_byte_read += t_req_bytes;

    uint16_t t_n_bytes = 0;
    if (t_req.status == DOP_SDO_REQU) {
    	t_n_bytes = DOP_CallSDO(t_sdo, &t_req);
        cvector_push_back(sdo_res_list, t_header); // Assign Response
    } else if(t_req.status == DOP_SDO_SUCC || t_req.status == DOP_SDO_FAIL) {
    	t_n_bytes = DOP_SetSDOArgs(t_sdo, &t_req);
        if (t_n_bytes < 0) {
            //TODO: Set SDO Argument ERROR
            return -1;
        }
    } else {
        //TODO: Read SDO Status ERROR
        return -1;
    }

    t_byte_read += t_n_bytes;
    return t_byte_read;
}

static int Unpack_SDO(uint8_t* t_byte_arr)
{
    int t_cursor = 0;

    // Get # of SDOs
    uint16_t t_numOfSDO = 0;
    memcpy(&t_numOfSDO, &t_byte_arr[t_cursor], DOP_OBJ_NUMS_SIZE);
    t_cursor += DOP_OBJ_NUMS_SIZE;

    // Call & Respond SDOs
    if (t_numOfSDO > 0) {
        for (int i = 0; i < t_numOfSDO; ++i) {
            int temp_cursor = Read_SDO(&t_byte_arr[t_cursor]);
            if (temp_cursor > 0) {
            	t_cursor += temp_cursor;
            } else if (temp_cursor < 0) {
                //TODO: Unpack SDO ERROR
                return DOP_SDO_FAULT;
            }
        }
    }

    return DOP_SUCCESS;
}

/* ------------------- SDO TX ------------------- */
static int Convert_SDOres_to_Bytes(DOP_Header_t* t_header, uint8_t* t_byte_arr)
{
    int t_byte_written = 0;
    // Set SDO Header
    memcpy(t_byte_arr, t_header, sizeof(DOP_Header_t));
    t_byte_written += sizeof(DOP_Header_t);

    // Return Response
    DOP_SDO_t* t_sdo = DOP_FindSDO(t_header->dictID, t_header->objID);
    if (t_sdo == NULL) {
        //TODO: Cannot Find SDO ERROR
        return -2;
    }

    memcpy(t_byte_arr + t_byte_written, &t_sdo->args.status, sizeof(t_sdo->args.status));
    t_byte_written += sizeof(t_sdo->args.status);
    memcpy(t_byte_arr + t_byte_written, &t_sdo->args.dataSize,   sizeof(t_sdo->args.dataSize));
    t_byte_written += sizeof(t_sdo->args.dataSize);

    int t_data_len = t_sdo->args.dataSize * t_sdo->args.typeSize;
    memcpy(t_byte_arr + t_byte_written, t_sdo->args.data, t_data_len);

    t_byte_written += t_data_len;

    return t_byte_written;
}

static int Pack_SDO(uint8_t* t_byte_arr, uint8_t* t_byte_len)
{
	// check send list whether these are empty or not 
	if ((sdo_res_list == NULL) && (sdo_req_list == NULL)){
		return DOP_SDO_NOTHING;
	}

	// Message Packaging
    int t_cursor = 0;

    // Res SDOs
    int t_numOfSDO_cursor = t_cursor;
    t_cursor += DOP_OBJ_NUMS_SIZE;

    uint8_t t_numOfSDO = 0;
    
    if (sdo_res_list != NULL) {
        for(int i = 0; i < cvector_size(sdo_res_list); ++i) {
            int temp_cursor = Convert_SDOres_to_Bytes(&sdo_res_list[i], &t_byte_arr[t_cursor]);
            if (temp_cursor > 0) {
            	t_cursor += temp_cursor;
                ++t_numOfSDO;
            } else if (temp_cursor < 0) {
                //TODO: Pack Response SDO Error
                return DOP_SDO_FAULT;
            }
        }
        cvector_free(sdo_res_list);
        sdo_res_list = NULL;
    }

    // Req SDOs
    if (sdo_req_list != NULL) {
        for(int i = 0; i < cvector_size(sdo_req_list); ++i) {
            int temp_cursor = Convert_SDOres_to_Bytes(&sdo_req_list[i], &t_byte_arr[t_cursor]);
            if (temp_cursor > 0) {
            	t_cursor += temp_cursor;
                ++t_numOfSDO;
            } else if (temp_cursor < 0) {
                //TODO: Pack Request SDO Error
                return DOP_SDO_FAULT;
            }
        }
        cvector_free(sdo_req_list);
        sdo_req_list = NULL;
    }

    // Set # of SDOs
    memcpy(&t_byte_arr[t_numOfSDO_cursor], &t_numOfSDO, DOP_OBJ_NUMS_SIZE);

    *t_byte_len = t_cursor;

    return DOP_SUCCESS;
}

static int Send_SDO(uint8_t t_dest_node)
{
    uint8_t t_byte_len = 0;
#ifdef FDCAN_WALKONSUIT_ENABLED
    uint16_t t_identifier = (0 << 10)|((exppack_node_id & 0x1F) << 5)|(t_dest_node & 0x1F);
#endif
#ifdef FDCAN_H10_ENABLED
    uint16_t t_identifier = SDO|(exppack_node_id<<4)|t_dest_node;
#endif


    int t_check = Pack_SDO(fdcanRxData, &t_byte_len);

    if(t_check < 0){
        //TODO: Send SDO Error
    	return t_check;
    } else if(t_check){
    	return t_check;
    }

    if (t_byte_len > 64) {
        //TODO: TX MESSAGE TOO LONG ERROR 
    }

    if(Send_MSG(t_identifier, fdcanRxData, t_byte_len) != 0){
        return t_check;
        //TODO: MSG TX ERROR
    }

    return t_check;
}

/* ------------------- PDO RX ------------------- */
static int Convert_Bytes_to_PDO(uint8_t* t_byte_arr)
{
    int t_byte_read = 0;

    DOP_Header_t t_header = Get_Header(t_byte_arr);
    t_byte_read += sizeof(DOP_Header_t);

    DOP_PDO_t* t_pdo = DOP_FindPDO(t_header.dictID, t_header.objID);
    if (t_pdo == NULL) {
        //TODO: Cannot Find PDO Error
        return -2;
    }

    uint16_t t_n_bytes = DOP_GetPDO(t_pdo, (void*)(t_byte_arr + t_byte_read));
    if (t_n_bytes < 0) {
        //TODO: Copy PDO to Receive Error
        return -1;
    }
    t_byte_read += t_n_bytes;

    return t_byte_read;
}

static int Unpack_PDO(uint8_t* t_byte_arr)
{
    int t_cursor = 0;

    // Get # of PDOs
    uint8_t t_numOfPDO = 0;
    memcpy(&t_numOfPDO, &t_byte_arr[t_cursor], DOP_OBJ_NUMS_SIZE);
    t_cursor += DOP_OBJ_NUMS_SIZE;

    if (t_numOfPDO > 0) {
        for (int i = 0; i < t_numOfPDO; ++i) {
            int temp_cursor = Convert_Bytes_to_PDO(&t_byte_arr[t_cursor]);
            if (temp_cursor > 0) {
            	t_cursor += temp_cursor;
            } else if (temp_cursor < 0) {
                //TODO: Unpack PDO Error
                return DOP_PDO_FAULT;
            }
        }
    }

    return DOP_SUCCESS;
}

/* ------------------- PDO TX ------------------- */
static int Convert_PDO_to_Bytes(DOP_Header_t* t_header, uint8_t* t_byte_arr)
{
    int t_header_size = sizeof(DOP_Header_t);
    // Publish PDO
    DOP_PDO_t* t_pdo = DOP_FindPDO(t_header->dictID, t_header->objID);
    if (t_pdo == NULL) {
        //TODO: Cannot Find PDO
        return -2;
    }

    uint16_t t_n_bytes = DOP_SetPDO(t_pdo, t_byte_arr + t_header_size);
    if (t_n_bytes < 0) {
        //TODO: Copy PDO to Send 
        return -1;
    } else if (t_n_bytes == 0) { // Nothing to publish
        return 0;
    }

    memcpy(t_byte_arr, t_header, t_header_size);
    return t_header_size + t_n_bytes;
}

static int Pack_PDO(uint8_t* t_byte_arr, uint8_t* t_byte_len)
{
	// check send list whether these are empty or not 
    if (pdo_send_list == NULL){
        return 0;
    }

    int t_cursor = 0;

    // Pub PDO
    int t_numOfPDO_cursor = t_cursor;
    t_cursor += DOP_OBJ_NUMS_SIZE;

    uint8_t t_numOfPDO = 0;

    if (pdo_send_list != NULL) {
        for(int i = 0; i < cvector_size(pdo_send_list); ++i) {

            int temp_cursor = Convert_PDO_to_Bytes(&pdo_send_list[i], &t_byte_arr[t_cursor]);
            if (temp_cursor > 0) {
            	t_cursor += temp_cursor;
                ++t_numOfPDO;
            } else if (temp_cursor < 0) {
                //TODO: Pack PDO Error
                return temp_cursor;
            }
        }
    }

    // Set # of PDOs
    memcpy(&t_byte_arr[t_numOfPDO_cursor], &t_numOfPDO, DOP_OBJ_NUMS_SIZE);

    *t_byte_len = t_cursor;

    return DOP_SUCCESS;
}

static int Run_Send_PDO()
{
    uint8_t t_byte_len = 0;
    uint8_t t_dest_node = NODE_ID_CM;
    uint16_t t_identifier;

    if(GUI_onoff)	{	t_identifier = GUI_SYNC|GUI_command;	}
#ifdef FDCAN_WALKONSUIT_ENABLED
    else            {   t_identifier = (1 << 10)|((exppack_node_id & 0x1F) << 5)|(t_dest_node & 0x1F); }
#endif
#ifdef FDCAN_H10_ENABLED
    else 			{	t_identifier = PDO|(exppack_node_id<<4)|t_dest_node;	}
#endif



    int t_check = Pack_PDO(fdcanTxData, &t_byte_len);

    if(t_check != 0){
        //TODO: Send PDO Error
    	return t_check;
    } else if(t_check){
    	return t_check;
    }

    if (t_byte_len != 1){
		if(Send_MSG(t_identifier, fdcanTxData, t_byte_len) == 0){
			return t_check;
			//TODO: MSG TX ERROR
		}
    }

	return t_check;
}

static int Ext_Send_PDO()
{
	if(GUI_command == GET_DIRECTION_SET_DATA){
		Send_MSG((uint16_t)(GUI_SYNC|GET_DIRECTION_SET_DONE), (uint8_t*)0, 1);
	}

	return 0;
}

static int Set_PDO_Dummy()
{
	static int t_count = 0;

	test_dummy[0] = comm_loop_cnt;
	test_dummy[1] = comm_loop_cnt;
	test_dummy[2] = comm_loop_cnt;
	test_dummy[3] = comm_loop_cnt;
	test_dummy[4] = comm_loop_cnt;
	test_dummy[5] = comm_loop_cnt;
	test_dummy[6] = comm_loop_cnt;
	test_dummy[7] = comm_loop_cnt;
	test_dummy[8] = comm_loop_cnt;
	test_dummy[9] = comm_loop_cnt;

	t_count++;

	return 0;
}


/* ------------------- LIST MANAGEMENT ------------------- */
static void Add_PDO_to_Send(uint8_t t_dictID, uint8_t t_objID)
{
	DOP_PDO_t* temp_pdo = DOP_FindPDO(t_dictID, t_objID);
    if (temp_pdo == NULL) {
        //TODO: Cannot Find PDO Error
        return;
    }

    DOP_Header_t t_pdo = {t_dictID, t_objID};

    for (int i = 0; i < cvector_size(pdo_send_list); ++i) {
        if ((pdo_send_list[i].dictID == t_dictID) && (pdo_send_list[i].objID == t_objID)){
            return;
        }
    }
    cvector_push_back(pdo_send_list, t_pdo);
}

static void Clear_PDO_to_Send()
{
    cvector_free(pdo_send_list);
    pdo_send_list = NULL;
}


/* ------------------- MSG HANDLER ------------------- */
int Send_MSG(uint16_t t_COB_ID, uint8_t* t_tx_data, uint32_t t_len)
{
	int t_check = 0;

	if (comm_type == COMM_TYPE_FDCAN) {
		if (IOIF_TransmitFDCAN1(t_COB_ID, t_tx_data, t_len) != 0) {
			return t_check;
			//TODO: MSG TX ERROR
		}
	}

	return -1;
}


static int Fdcan_Rx_Hdlr(uint16_t t_wasp_id, uint8_t* t_rx_data)
{
    fnc_code = t_wasp_id & 0x700;
    ori_node = (t_wasp_id & 0x0F0)>>4;

//    IOIF_WriteGPIOPin(IOIF_GPIO_PORT_E, IOIF_GPIO_PIN_13, IOIF_GPIO_PIN_RESET); // STATUS_LED_B_Pin

    switch(fnc_code) {
#ifdef FDCAN_WALKONSUIT_ENABLED
		case 0:
			if (Unpack_SDO(t_rx_data) < 0) {
				return SDO_RX_ERR;
			}
			else {
				Send_SDO(ori_node);
			}
			break;

		case 1:
			if (Unpack_PDO(t_rx_data) < 0) {
				return PDO_RX_ERR;
			}
			else {
				Run_Send_PDO(ori_node);
			}
			break;

		default: break;
#endif
#ifdef FDCAN_H10_ENABLED
        case EMCY:
            Recv_EMCY(t_rx_data, &err_code);
            // TODO: ERROR Process
            break;

        case SDO:
            if (Unpack_SDO(t_rx_data) < 0) {
                return SDO_RX_ERR;
            } else{
                Send_SDO(ori_node);
            }
            break;

        case PDO:
            if (Unpack_PDO(t_rx_data) < 0) {
                return PDO_RX_ERR;
            } else{
            	Run_Send_PDO(ori_node);
            }
            break;

        default: break;
#endif
    }

    return 0;
}

/* ------------------- SDO CALLBACK ------------------- */
static void Set_Send_PDO_List(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res)
{
	Clear_PDO_to_Send();

    int t_cursor = 0;
    uint8_t* t_ids = (uint8_t*)t_req->data;
    while (t_cursor < 2*t_req->dataSize) {
        uint8_t t_dictID = t_ids[t_cursor++];
        uint8_t t_objID = t_ids[t_cursor++];
        Add_PDO_to_Send(t_dictID, t_objID);
    }

    t_res->status = DOP_SDO_SUCC;
}

static void Set_MS_Enum(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res)
{
	memcpy(&MS_enum, t_req->data, 1);

	t_res->dataSize = 0;
	t_res->status = DOP_SDO_SUCC;
}

static void Set_GUI_COMM_OnOff(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res)
{
	memcpy(&GUI_onoff, t_req->data, 1);

	t_res->dataSize = 1;
	t_res->status = DOP_SDO_SUCC;
}

static void Set_GUI_COMM_Command(DOP_SDOArgs_t* t_req, DOP_SDOArgs_t* t_res)
{
	memcpy(&GUI_command, t_req->data, 1);

	t_res->dataSize = 1;
	t_res->status = DOP_SDO_SUCC;
}



/* For P vector */
static void Add_SDO_to_Send(uint8_t t_dictID, uint8_t t_objID)
{
	DOP_SDO_t* temp_sdo = DOP_FindSDO(t_dictID, t_objID);
    if (temp_sdo == NULL) {
        //TODO: Cannot Find PDO Error
        return;
    }

    DOP_Header_t t_sdo = {t_dictID, t_objID};

    for (int i = 0; i < cvector_size(sdo_res_list); ++i) {
        if ((sdo_res_list[i].dictID == t_dictID) && (sdo_res_list[i].objID == t_objID)){
            return;
        }
    }
    cvector_push_back(sdo_res_list, t_sdo);
}

static void Clear_SDO_to_Send()
{
    cvector_free(sdo_res_list);
    sdo_res_list = NULL;
}

void SendDurationCompletedFlag_RH(void)
{
	Clear_SDO_to_Send();
	Add_SDO_to_Send(TASK_ID_EXTDEV, SDO_ID_EXTDEV_RH_P_VECTOR_DURATION_COMPLETED);
	Send_SDO(NODE_ID_CM);
}

void SendDurationCompletedFlag_LH(void)
{
	Clear_SDO_to_Send();
	Add_SDO_to_Send(TASK_ID_EXTDEV, SDO_ID_EXTDEV_LH_P_VECTOR_DURATION_COMPLETED);
	Send_SDO(NODE_ID_CM);
}

void Send_ExtensionBoardEnable(void)
{
	Clear_SDO_to_Send();
    Add_SDO_to_Send(TASK_ID_EXTDEV, SDO_ID_EXTDEV_EXTENSION_BOARD_ENABLE);
	Send_SDO(NODE_ID_CM);
}

void Send_ExtensionBoardDisable(void)
{
	Clear_SDO_to_Send();
    Add_SDO_to_Send(TASK_ID_EXTDEV, SDO_ID_EXTDEV_EXTENSION_BOARD_DISABLE);
	Send_SDO(NODE_ID_CM);
}


#ifdef FDCAN_WALKONSUIT_ENABLED
static int Init_Encoding_WS5_Dataset(void)
{
	WS5_ExtBoard_Transmit_Data_16bit.K_pMMG         = 65536.0f / 800;
	WS5_ExtBoard_Transmit_Data_16bit.K_IMU          = 65536.0f / 3;

	return 0;
}

static int Run_Encoding_WS5_Dataset(void)
{
	WS5_ExtBoard_Transmit_Data_16bit.IMU1_W  = (int16_t) (ExpPackDataObj.imu_data.imuObj1.w  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU1_X  = (int16_t) (ExpPackDataObj.imu_data.imuObj1.x  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU1_Y  = (int16_t) (ExpPackDataObj.imu_data.imuObj1.y  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU1_Z  = (int16_t) (ExpPackDataObj.imu_data.imuObj1.z  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU2_W  = (int16_t) (ExpPackDataObj.imu_data.imuObj2.w  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU2_X  = (int16_t) (ExpPackDataObj.imu_data.imuObj2.x  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU2_Y  = (int16_t) (ExpPackDataObj.imu_data.imuObj2.y  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU2_Z  = (int16_t) (ExpPackDataObj.imu_data.imuObj2.z  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU3_W  = (int16_t) (ExpPackDataObj.imu_data.imuObj3.w  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU3_X  = (int16_t) (ExpPackDataObj.imu_data.imuObj3.x  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU3_Y  = (int16_t) (ExpPackDataObj.imu_data.imuObj3.y  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU3_Z  = (int16_t) (ExpPackDataObj.imu_data.imuObj3.z  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU4_W  = (int16_t) (ExpPackDataObj.imu_data.imuObj4.w  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU4_X  = (int16_t) (ExpPackDataObj.imu_data.imuObj4.x  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU4_Y  = (int16_t) (ExpPackDataObj.imu_data.imuObj4.y  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);
	WS5_ExtBoard_Transmit_Data_16bit.IMU4_Z  = (int16_t) (ExpPackDataObj.imu_data.imuObj4.z  * WS5_ExtBoard_Transmit_Data_16bit.K_IMU);


//	WS5_ExtBoard_Transmit_Data_16bit.pMMG_R_Thigh  = (int16_t) (ExpPackDataObj.pMMG_press.pMMG1A_press * WS5_ExtBoard_Transmit_Data_16bit.K_pMMG);
//	WS5_ExtBoard_Transmit_Data_16bit.pMMG_L_Thigh  = (int16_t) (ExpPackDataObj.pMMG_press.pMMG2A_press * WS5_ExtBoard_Transmit_Data_16bit.K_pMMG);
//	WS5_ExtBoard_Transmit_Data_16bit.pMMG_R_Shank  = (int16_t) (ExpPackDataObj.pMMG_press.pMMG3A_press * WS5_ExtBoard_Transmit_Data_16bit.K_pMMG);
//	WS5_ExtBoard_Transmit_Data_16bit.pMMG_L_Shank  = (int16_t) (ExpPackDataObj.pMMG_press.pMMG4A_press * WS5_ExtBoard_Transmit_Data_16bit.K_pMMG);

	WS5_ExtBoard_Transmit_Data_32bit.loop_cnt  = comm_loop_cnt;

	return 0;
}
#endif
