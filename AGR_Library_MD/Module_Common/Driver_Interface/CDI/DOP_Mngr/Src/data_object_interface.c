

#include "data_object_interface.h"

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

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
cvector_vector_type(DOP_Header_t) pdo_to_send;
cvector_vector_type(DOP_Header_t) pdo_to_recv;

cvector_vector_type(DOP_Header_t) sdos_to_req;
cvector_vector_type(DOP_Header_t) sdos_to_res;

#endif

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

static int ReadSDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr);
static int ReadPDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr);
static SDOInfo_t GetSDOInfo(uint8_t t_taskID, uint16_t t_SDOID);
static PDOInfo_t* GetPDOInfo(uint8_t t_taskID, uint16_t t_PDOID);
static void AllocateTask(DOPI_TaskObj_t* t_task, uint8_t t_taskID);
static int CheckSDO(uint8_t t_taskID, uint16_t t_SDOID);
static int CheckPDO(uint8_t t_taskID, uint16_t t_PDOID);

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
static DOP_Header_t GetHeader(uint8_t* byte_arr);
static int SendPDO_Protocol(DOP_Header_t* header, uint8_t* byte_arr);
static int RecvPDO_Protocol(uint8_t* byte_arr);
static DOP_SDOArgs_t Bytes2SDOreq(uint8_t* byte_arr, uint16_t *byte_len);
static int SDOres2Bytes(DOP_Header_t* header, uint8_t* byte_arr);
static int RecvSDO_Protocol(uint8_t* byte_arr);
static void Tx_SendPDO_List();
static void SetSendPDO_List(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);

#endif

/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- INIT ------------------- */
void DOPI_InitDevObj(DOPI_DevObj_t* t_obj, uint8_t t_nodeID)
{
	static uint8_t t_temp = 0;

	if(t_temp == 0){
		DOP_CreateSDOTable();
		DOP_CreatePDOTable();
		t_temp++;
	}

	/* Device Object Initialization */
	t_obj->nodeID = t_nodeID;
	t_obj->errCode = NO_ERROR;
	t_obj->numOfTask = TASK_NUM;

	/* Task Allocation */
	for(int i=0; i < TASK_NUM; ++i){
		AllocateTask(&t_obj->tasks[i], i);
	}
}

/* ------------------- SDO Object ------------------- */
DOPI_SDOUnit_t DOPI_CreateSDOUnit(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID, DOP_SDOStatus_t t_SDOStatus, uint8_t t_numOfData)
{
	DOPI_SDOUnit_t t_SDOUnit;

	t_SDOUnit.taskID = t_taskID;
	t_SDOUnit.SDOID = t_SDOID;
	t_SDOUnit.param.SDOStatus = t_SDOStatus;
	t_SDOUnit.param.numOfData = t_numOfData;
	t_SDOUnit.param.data = t_obj->tasks[t_taskID].SDOsAddr[t_SDOID];

	return t_SDOUnit;
}

DOPI_Status_t DOPI_AppendSDO(DOPI_SDOUnit_t* t_SDOUnit, DOPI_SDOMsg_t* t_SDOMsg)
{
	uint8_t t_cursor;

	/* Validation Check */
	if(CheckSDO(t_SDOUnit->taskID, t_SDOUnit->SDOID) != 0)	return DOP_STATUS_SDO_SET_FAIL;

	/* Appending SDO and Packing */
	/* 0th txbuf is for a number of SDO */
	if(t_SDOMsg->msgLength == 0) {t_cursor = 1;}
	else						 {t_cursor = t_SDOMsg->msgLength;}

	t_SDOMsg->txBuf[t_cursor++] = t_SDOUnit->taskID;
	t_SDOMsg->txBuf[t_cursor++] = t_SDOUnit->SDOID;
	t_SDOMsg->txBuf[t_cursor++] = t_SDOUnit->param.SDOStatus;
	t_SDOMsg->txBuf[t_cursor++] = t_SDOUnit->param.numOfData;

	SDOInfo_t dataSize = GetSDOInfo(t_SDOUnit->taskID, t_SDOUnit->SDOID);
	uint8_t t_size = t_SDOUnit->param.numOfData*dataSize;

	memcpy(&t_SDOMsg->txBuf[t_cursor], t_SDOUnit->param.data, t_size);
	t_cursor += t_size;

	t_SDOMsg->numOfSDO++;
	t_SDOMsg->txBuf[0] = t_SDOMsg->numOfSDO;
	t_SDOMsg->msgLength = t_cursor;

	return DOP_STATUS_SUCCESS;
}

void DOPI_ClearSDO(DOPI_SDOMsg_t* t_SDOMsg)
{
	memset(t_SDOMsg, 0, sizeof(DOPI_SDOMsg_t));
}

/* ------------------- PDO OBJECT ------------------- */
DOPI_PDOUnit_t DOPI_CreatePDOUnit(uint8_t t_taskID, uint16_t t_PDOID, void* t_addr)
{
	DOPI_PDOUnit_t t_PDOUnit;

	t_PDOUnit.taskID = t_taskID;
	t_PDOUnit.PDOID = t_PDOID;
	t_PDOUnit.addr = t_addr;

	return t_PDOUnit;
}

DOPI_Status_t DOPI_AppendPDO(DOPI_PDOUnit_t* t_PDOUnit, DOPI_PDOMsg_t* t_PDOMsg)
{
	uint8_t t_cursor;

	/* Validation Check */
	if(CheckPDO(t_PDOUnit->taskID, t_PDOUnit->PDOID) != 0)	return DOP_STATUS_PDO_SET_FAIL;

	/* Appending PDO and Packing */
	/* 0th txbuf is for a number of PDO */
	if(t_PDOMsg->msgLength == 0) {t_cursor = 1;}
	else						 {t_cursor = t_PDOMsg->msgLength;}

	t_PDOMsg->txBuf[t_cursor++] = t_PDOUnit->taskID;
	t_PDOMsg->txBuf[t_cursor++] = t_PDOUnit->PDOID;

	PDOInfo_t* t_PDOInfo = GetPDOInfo(t_PDOUnit->taskID, t_PDOUnit->PDOID);
	uint8_t t_size = (*t_PDOInfo[0])*(*t_PDOInfo[1]);

	memcpy(&t_PDOMsg->txBuf[t_cursor], t_PDOUnit->addr, t_size);
	t_cursor += t_size;

	t_PDOMsg->numOfPDO++;
	t_PDOMsg->txBuf[0] = t_PDOMsg->numOfPDO;
	t_PDOMsg->msgLength = t_cursor;

	return DOP_STATUS_SUCCESS;
}

void DOPI_ClearPDO(DOPI_PDOMsg_t* t_PDOMsg)
{
	memset(t_PDOMsg, 0, sizeof(DOPI_PDOMsg_t));
}

/* ------------------- GET & SET ------------------- */
void DOPI_SetSDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID, void* t_addr)
{
	t_obj->tasks[t_taskID].SDOsAddr[t_SDOID] = t_addr;
}

void* DOPI_GetSDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID)
{
	return t_obj->tasks[t_taskID].SDOsAddr[t_SDOID];
}

void DOPI_SetPDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_PDOID, void* t_addr)
{
	t_obj->tasks[t_taskID].PDOsAddr[t_PDOID] = t_addr;
}

void* DOPI_GetPDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_PDOID)
{
	return t_obj->tasks[t_taskID].PDOsAddr[t_PDOID];
}

void DOPI_SetRoutOnOff(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint8_t t_routID, DOP_RoutineOnOff_t t_OnOff)
{
	t_obj->tasks[t_taskID].routines[t_routID] = t_OnOff;
}

DOP_RoutineOnOff_t DOPI_GetRoutOnOff(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint8_t t_routID)
{
	return t_obj->tasks[t_taskID].routines[t_routID];
}

/* ------------------- RECEIVE SDO ------------------- */
int DOPI_UnpackSDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr)
{
    int t_cursor = 0;

    // Get # of SDOs
    uint8_t t_numOfSDO = 0;
    memcpy(&t_numOfSDO, &t_byteArr[t_cursor++], 1);

    // Call & Respond SDOs
    if (t_numOfSDO > 0) {
        for (int i = 0; i < t_numOfSDO; ++i) {
            int t_tempCursor = ReadSDO(t_obj, &t_byteArr[t_cursor]);
            if (t_tempCursor > 0) {
                t_cursor += t_tempCursor;
            } else if (t_tempCursor < 0) {
                //TODO: Unpack SDO ERROR
                return DOP_STATUS_SDO_RECV_FAIL;
            }
        }
    }
    return DOP_STATUS_SUCCESS;
}

/* ------------------- RECEIVE PDO ------------------- */
int DOPI_UnpackPDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr)
{
    int t_cursor = 0;

    // Get # of PDOs
    uint8_t t_numOfPDO = 0;
    memcpy(&t_numOfPDO, &t_byteArr[t_cursor++], 1);

    if (t_numOfPDO > 0) {
        for (int i = 0; i < t_numOfPDO; ++i) {
            int t_tempCursor = ReadPDO(t_obj, &t_byteArr[t_cursor]);
            if (t_tempCursor > 0) {
                t_cursor += t_tempCursor;
            } else if (t_tempCursor < 0) {
                //TODO: Unpack PDO Error
                return DOP_STATUS_PDO_RECV_FAIL;
            }
        }
    }
    return DOP_STATUS_SUCCESS;
}

#ifdef CM_MODULE
void DOPI_UnpackEMCY(uint32_t* t_errCode, uint8_t* t_byteArr)
{
    memcpy(t_errCode, t_byteArr, DOP_ERR_CODE_SIZE);
}

#endif

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
//void DOP_Init()
//{
//	DOP_InitDefaultDOD();
//    DOP_CreateSDO(DATA_OBJECT_DEFAULT_DOD, DOP_SDO_SET_PDO_TO_SYNC, "set_pdo_to_send", DOP_UINT16, SetSendPDO_List);
//}
//
//void DOP_InitDefaultDOD()
//{
//    DOP_Create_DOD(DATA_OBJECT_DEFAULT_DOD, "default_dod");
//    DOP_CreateSDO(DATA_OBJECT_DEFAULT_DOD, DATA_OBJECT_SDO_GET_DOD_LIST, "dod_list", DOP_CHAR,  DefaultDOD_GetDODs);
//    DOP_CreateSDO(DATA_OBJECT_DEFAULT_DOD, DATA_OBJECT_SDO_GET_PDO_LIST, "pdo_list", DOP_CHAR, DefaultDOD_GetPDOs);
//    DOP_CreateSDO(DATA_OBJECT_DEFAULT_DOD, DATA_OBJECT_SDO_GET_SDO_LIST, "sdo_list", DOP_CHAR, DefaultDOD_GetSDOs);
//}

// TxRx Protocols
int DOP_Tx(uint8_t* byte_arr, uint16_t* byte_len)
{
    /* Pre-process */
    Tx_SendPDO_List();

    int cursor = 0;
    
    /* PDO */
    // Set PDO character
    memcpy(&byte_arr[cursor], &(char){DOP_CHAR_PDO}, DOP_OBJ_CHAR_SIZE);
    cursor += DOP_OBJ_CHAR_SIZE;

    // Pub PDO
    int n_pdo_cursor = cursor;
    cursor += DOP_OBJ_NUMS_SIZE;
    uint8_t numOfPDO = 0;
    if (pdo_to_send != NULL) {
        for(int i = 0; i < cvector_size(pdo_to_send); ++i) {
            int temp_cursor = SendPDO_Protocol(&pdo_to_send[i], &byte_arr[cursor]);
            if (temp_cursor > 0) {
                cursor += temp_cursor;
                ++numOfPDO;
            } else if (temp_cursor < 0) {
                return temp_cursor;
            }
        }
    }

    // Set # of PDOs
    memcpy(&byte_arr[n_pdo_cursor], &numOfPDO, DOP_OBJ_NUMS_SIZE);

    /* SDO */
    // Set SDO character
    memcpy(&byte_arr[cursor], &(char){DOP_CHAR_SDO}, DOP_OBJ_CHAR_SIZE);
    cursor += DOP_OBJ_CHAR_SIZE;

    // Res SDOs
    int n_sdo_cursor = cursor;
    cursor += DOP_OBJ_NUMS_SIZE;
    uint8_t numOfSDO = 0;
    if (sdos_to_res != NULL) {
        for(int i = 0; i < cvector_size(sdos_to_res); ++i) {
            int temp_cursor = SDOres2Bytes(&sdos_to_res[i], &byte_arr[cursor]);
            if (temp_cursor > 0) {
                cursor += temp_cursor;
                ++numOfSDO;
            } else if (temp_cursor < 0) {
                return DOP_PDO_FAULT;
            }
        }
        // TODO: use more efficient way than vector
        cvector_free(sdos_to_res);
        sdos_to_res = NULL;
    }

    // Req SDOs
    if (sdos_to_req != NULL) {
        for(int i = 0; i < cvector_size(sdos_to_req); ++i) {
            int temp_cursor = SDOres2Bytes(&sdos_to_req[i], &byte_arr[cursor]);
            if (temp_cursor > 0) {
                cursor += temp_cursor;
                ++numOfSDO;
            } else if (temp_cursor < 0) {
                return DOP_SDO_FAULT;
            }
        }
        cvector_free(sdos_to_req);
        sdos_to_req = NULL;
    }

    // Set # of PDOs
    memcpy(&byte_arr[n_sdo_cursor], &numOfSDO, DOP_OBJ_NUMS_SIZE);

    *byte_len = cursor;

    return DOP_SUCCESS;
}

int DOP_Rx(uint8_t* byte_arr, uint16_t byte_len)
{
    int cursor = 0;

    /* PDO */
    // Check PDO character
    if (byte_arr[cursor] == DOP_CHAR_PDO) {
        cursor += DOP_OBJ_CHAR_SIZE;

        // Get # of PDOs
        uint8_t numOfPDO = 0;
        memcpy(&numOfPDO, &byte_arr[cursor], DOP_OBJ_NUMS_SIZE);
        cursor += DOP_OBJ_NUMS_SIZE;

        // Sync PDOs
        if (numOfPDO > 0) {
            for (int i = 0; i < numOfPDO; ++i) {
                int temp_cursor = RecvPDO_Protocol(&byte_arr[cursor]);
                if (temp_cursor > 0) {
                    cursor += temp_cursor;
                } else if (temp_cursor < 0) {
                    return DOP_PDO_FAULT;
                }
            }
        }
    }

    /* SDO */
    // Check SDO character
    if (byte_arr[cursor] == DOP_CHAR_SDO) {
        cursor += DOP_OBJ_CHAR_SIZE;

        // Get # of SDOs
        uint16_t numOfSDO = 0;
        memcpy(&numOfSDO, &byte_arr[cursor], DOP_OBJ_NUMS_SIZE);
        cursor += DOP_OBJ_NUMS_SIZE;

        // Call & Respond SDOs
        if (numOfSDO > 0) {
            for (int i = 0; i < numOfSDO; ++i) {
                int temp_cursor = RecvSDO_Protocol(&byte_arr[cursor]);
                if (temp_cursor > 0) {
                    cursor += temp_cursor;
                } else if (temp_cursor < 0) {
                    return DOP_SDO_FAULT;
                }
            }
        }
    }

    return DOP_SUCCESS;
}


void DOP_AddPDOtoSend(uint8_t dictID, uint16_t objID)
{
    DOP_Header_t pdo = {dictID, objID};
    cvector_push_back(pdo_to_send, pdo);
}

void DOP_ClearPDOtoSend()
{
    cvector_free(pdo_to_send);
    pdo_to_send = NULL;
}

void DOP_AddPDOtoRecv(uint8_t dictID, uint16_t objID)
{
    DOP_Header_t pdo = {dictID, objID};
    cvector_push_back(pdo_to_recv, pdo);
}

void DOP_AddSDOtoReq(uint8_t dictID, uint16_t objID, void* data, uint16_t size)
{
    DOP_Header_t sdo = {dictID, objID};
    cvector_push_back(sdos_to_req, sdo);
    
    DOP_SetSDOReq(dictID, objID, data, size);
}

#endif

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

static int ReadSDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr)
{
    uint8_t t_cursor = 0;
    uint8_t t_taskID, t_numOfData;
	uint16_t t_SDOID;
    int8_t t_SDOStatus;

    memcpy(&t_taskID, &t_byteArr[t_cursor++], 1);
    memcpy(&t_SDOID, &t_byteArr[t_cursor++], 1);
    memcpy(&t_SDOStatus, &t_byteArr[t_cursor++], 1);
    memcpy(&t_numOfData, &t_byteArr[t_cursor++], 1);

    if(CheckSDO(t_taskID, t_SDOID) != 0 )	return DOP_STATUS_RECV_FAIL;

    uint8_t t_size = t_numOfData*SDOTable[t_taskID][t_SDOID];
    t_cursor += t_size;

    switch(t_SDOStatus){

    case DOP_SDO_IDLE:
    	break;

    case DOP_SDO_SUCC:
    	break;

    case DOP_SDO_FAIL:
    	return DOP_STATUS_SDO_RECV_FAIL;
    	break;

    default:
    	return DOP_STATUS_SDO_RECV_FAIL;
    	break;
    }

    return t_cursor;
}

static int ReadPDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr)
{
    int t_cursor = 0;
    uint8_t t_taskID;
	uint16_t t_PDOID;

    memcpy(&t_taskID, &t_byteArr[t_cursor++], 1);
    memcpy(&t_PDOID, &t_byteArr[t_cursor++], 1);

    if(CheckPDO(t_taskID, t_PDOID) != 0 )	return DOP_STATUS_RECV_FAIL;

    uint8_t t_size = DOP_ConvertDataSize(PDOTable[t_taskID][t_PDOID][0]) * PDOTable[t_taskID][t_PDOID][1];
    memcpy(t_obj->tasks[t_taskID].PDOsAddr[t_PDOID], &t_byteArr[t_cursor], t_size);
    t_cursor += t_size;

    return t_cursor;
}

static SDOInfo_t GetSDOInfo(uint8_t t_taskID, uint16_t t_SDOID)
{
	return DOP_ConvertDataSize(SDOTable[t_taskID][t_SDOID]);
}

static PDOInfo_t* GetPDOInfo(uint8_t t_taskID, uint16_t t_PDOID)
{
	return &PDOTable[t_taskID][t_PDOID];
}

static void AllocateTask(DOPI_TaskObj_t* t_task, uint8_t t_taskID)
{
	t_task->taskID = t_taskID;
	t_task->taskState = STATE_IDLE;
}

static int CheckSDO(uint8_t t_taskID, uint16_t t_SDOID)
{
	/* Check Error */
	switch(t_taskID){
	case TASK_ID_LOWLEVEL:
		if(t_SDOID >= SDO_ID_LOWLEVEL_NUM)	return DOP_STATUS_SDO_SET_FAIL;
		break;
	case TASK_ID_MIDLEVEL:
		if(t_SDOID >= SDO_ID_MIDLEVEL_NUM)	return DOP_STATUS_SDO_SET_FAIL;
		break;
	case TASK_ID_MSG:
		if(t_SDOID >= SDO_ID_MSG_NUM)		return DOP_STATUS_SDO_SET_FAIL;
		break;
	case TASK_ID_IMU:
		if(t_SDOID >= SDO_ID_IMU_NUM)		return DOP_STATUS_SDO_SET_FAIL;
		break;
	case TASK_ID_GAIT:
		if(t_SDOID >= SDO_ID_GAIT_NUM)		return DOP_STATUS_SDO_SET_FAIL;
		break;
	case TASK_ID_SYSMNGT:
		if(t_SDOID >= SDO_ID_SYSMNGT_NUM)	return DOP_STATUS_SDO_SET_FAIL;
		break;
	case TASK_ID_EXTDEV:
		if(t_SDOID >= SDO_ID_EXTDEV_NUM)	return DOP_STATUS_SDO_SET_FAIL;
		break;
	default:
		return DOP_STATUS_SDO_SET_FAIL;
		break;
	}

	return DOP_STATUS_SUCCESS;
}

static int CheckPDO(uint8_t t_taskID, uint16_t t_PDOID)
{
	switch(t_taskID){
	case TASK_ID_LOWLEVEL:
		if(t_PDOID >= PDO_ID_LOWLEVEL_NUM)	return DOP_STATUS_PDO_SET_FAIL;
		break;
	case TASK_ID_MIDLEVEL:
		if(t_PDOID >= PDO_ID_MIDLEVEL_NUM)	return DOP_STATUS_PDO_SET_FAIL;
		break;
	case TASK_ID_MSG:
		if(t_PDOID >= PDO_ID_MSG_NUM)		return DOP_STATUS_PDO_SET_FAIL;
		break;
	case TASK_ID_IMU:
		if(t_PDOID >= PDO_ID_IMU_NUM)		return DOP_STATUS_PDO_SET_FAIL;
		break;
	case TASK_ID_GAIT:
		if(t_PDOID >= PDO_ID_GAIT_NUM)		return DOP_STATUS_PDO_SET_FAIL;
		break;
	case TASK_ID_SYSMNGT:
		if(t_PDOID >= PDO_ID_SYSMNGT_NUM)	return DOP_STATUS_PDO_SET_FAIL;
		break;
	case TASK_ID_EXTDEV:
		if(t_PDOID >= PDO_ID_EXTDEV_NUM)	return DOP_STATUS_PDO_SET_FAIL;
		break;
	default:
		return DOP_STATUS_PDO_SET_FAIL;
		break;
	}

	return DOP_STATUS_SUCCESS;
}

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
static DOP_Header_t GetHeader(uint8_t* byte_arr)
{
    DOP_Header_t header;
    memcpy(&header, byte_arr, sizeof(DOP_Header_t));
    // TODO: Header validation
    return header;
}

static int SendPDO_Protocol(DOP_Header_t* header, uint8_t* byte_arr)
{
    int header_size = sizeof(DOP_Header_t);
    // Publish PDO    
    DOP_PDO_t* pdo = DOP_FindPDO(header->dictID, header->objID);
    if (pdo == NULL) {
        return -2;
    }

    uint16_t n_bytes = DOP_SetPDO(pdo, byte_arr + header_size);
    if (n_bytes < 0) { // Publish error
        return -1;
    } else if (n_bytes == 0) { // Nothing to publish
        return 0;
    }

    // Set PDO Header
    memcpy(byte_arr, header, header_size);
    return header_size + n_bytes; // written bytes
}


static int RecvPDO_Protocol(uint8_t* byte_arr)
{    
    int byte_read = 0;

    DOP_Header_t header = GetHeader(byte_arr);
    byte_read += sizeof(DOP_Header_t);

    DOP_PDO_t* pdo = DOP_FindPDO(header.dictID, header.objID);
    if (pdo == NULL) {
        return -2;
    }
    
    uint16_t n_bytes = DOP_GetPDO(pdo, (void*)(byte_arr + byte_read));
    if (n_bytes < 0) {
        return -1;
    }
    byte_read += n_bytes;
    return byte_read;
}

static DOP_SDOArgs_t Bytes2SDOreq(uint8_t* byte_arr, uint16_t *byte_len)
{
    DOP_SDOArgs_t req;
    *byte_len = 0;

    int idx = sizeof(req.status);
    int len = sizeof(req.dataSize);

    memcpy(&req.dataSize, &byte_arr[idx], len);
    *byte_len += len;

    req.data = &byte_arr[idx + len];


    req.status = byte_arr[0];
    *byte_len += 1;

    return req;
}

static int SDOres2Bytes(DOP_Header_t* header, uint8_t* byte_arr)
{
    int byte_written = 0;
    // Set SDO Header
    memcpy(byte_arr, header, sizeof(DOP_Header_t));
    byte_written += sizeof(DOP_Header_t);

    // Return Response
    DOP_SDO_t* sdo = DOP_FindSDO(header->dictID, header->objID);
    if (sdo == NULL) {
        return -2;
    }

    memcpy(byte_arr + byte_written, &sdo->args.status, sizeof(sdo->args.status));
    byte_written += sizeof(sdo->args.status);
    memcpy(byte_arr + byte_written, &sdo->args.dataSize,   sizeof(sdo->args.dataSize));
    byte_written += sizeof(sdo->args.dataSize);

    int data_len = sdo->args.dataSize * sdo->args.typeSize;
    memcpy(byte_arr + byte_written, sdo->args.data, data_len);
    byte_written += data_len;

    return byte_written;
}

static int RecvSDO_Protocol(uint8_t* byte_arr)
{
    int byte_read = 0;
    DOP_Header_t header = GetHeader(byte_arr);
    byte_read += sizeof(DOP_Header_t);
    DOP_SDO_t* sdo = DOP_FindSDO(header.dictID, header.objID);
    if (sdo == NULL) {
        return -2;
    }

    uint16_t req_bytes = 0;
    DOP_SDOArgs_t req = Bytes2SDOreq(byte_arr + byte_read, &req_bytes);
    byte_read += req_bytes;

    uint16_t n_bytes = 0;
    if (req.status == DOP_SDO_REQU) {
        n_bytes = DOP_CallSDO(sdo, &req);
        cvector_push_back(sdos_to_res, header); // Assign Response
    } else if(req.status == DOP_SDO_SUCC || req.status == DOP_SDO_FAIL) {
        n_bytes = DOP_SetSDOArgs(sdo, &req);
        if (n_bytes < 0) {
            return -1;
        }
    } else {
        return -1;
    }

    byte_read += n_bytes;
    return byte_read;
}

static void Tx_SendPDO_List()
{
    if (pdo_to_recv == NULL) {
        return;
    }

    int numOfPDO = cvector_size(pdo_to_recv);
    uint16_t req_data[numOfPDO * 2];
    for (int i = 0; i < numOfPDO; ++i) {
        req_data[2*i]   = pdo_to_recv[i].dictID;
        req_data[2*i+1] = pdo_to_recv[i].objID;
    }
    DOP_AddSDOtoReq(DOP_DEFAULT_DOD, DOP_SDO_SET_PDO_TO_SYNC, req_data, numOfPDO * 2);
    cvector_free(pdo_to_recv);
    pdo_to_recv = NULL;
}

// Init
static void SetSendPDO_List(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    int cursor = 0;
    uint16_t* ids = (uint16_t*)req->data;
    while (cursor < req->dataSize) {
        uint8_t  dictID = (uint8_t)ids[cursor++];
        uint16_t objID = ids[cursor++];
        DOP_AddPDOtoSend(dictID, objID);
    }

    res->status = DOP_SDO_SUCC;
}

#endif
