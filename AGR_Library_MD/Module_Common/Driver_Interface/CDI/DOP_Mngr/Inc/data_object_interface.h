
#ifndef DOP_MNGR_INC_DATA_OBJECT_INTERFACE_H_
#define DOP_MNGR_INC_DATA_OBJECT_INTERFACE_H_

#if __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "error_dictionary.h"
#include "data_object.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define DOP_STATUS_SUCCESS		 		0
#define DOP_STATUS_SDO_SET_FAIL			-1
#define DOP_STATUS_PDO_SET_FAIL			-2
#define DOP_STATUS_RECV_FAIL			-3
#define DOP_STATUS_RECV_INVALID_MSG		-4
#define DOP_STATUS_SDO_RECV_FAIL		-5
#define DOP_STATUS_PDO_RECV_FAIL		-6

#define DOP_SUCCESS     0
#define DOP_PDO_FAULT   -1
#define DOP_SDO_FAULT   -2

#define DOP_CHAR_PDO 0x50
#define DOP_CHAR_SDO 0x53

/* PACKET SIZE*/
#define DOP_CALLER_ID_SIZE		1
#define DOP_DEVICE_ID_SIZE		1
#define DOP_NODE_ID_SIZE		1
#define DOP_CMD_SIZE			1
#define DOP_MSG_1BYTE			1
#define DOP_ERR_CODE_SIZE		4
#define DOP_OBJ_CHAR_SIZE		1
#define DOP_OBJ_NUMS_SIZE		1

#define DOP_SDO_SET_PDO_TO_SYNC 3

#define DOP_D10_TASK_NUM		4

#ifdef MD_MODULE
#define D10_BUFFER_ARRAY_MAX_SIZE		10000
#define D10_BUFFER_ARRAY_SUB_SIZE		2000

#define D10_TRAJECTORY_TOTAL_LENGTH		10000
#define D10_TRAJECTORY_ELEMENT_NUMBER	10

#endif

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef int32_t DOPI_Status_t;

typedef int (*DOPI_RecvMsgHdlr_t) (uint16_t, uint8_t*);		// not used //wasp id, rx Buffer

/* Mode of Operation*/
typedef enum _DOPI_OpMode_t{
	DOPI_CURRENT_MODE,
	DOPI_VELOCITY_MODE,
	DOPI_POSITION_MODE
} DOPI_OpMode_t;

/* Input Method */
typedef enum _DOPI_InMethod_t{
	DOPI_COMMUNICATION,
	DOPI_PWM,
	DOPI_ANALOG,
	DOPI_GUI
} DOPI_InMethod_t;

typedef struct _DOPI_CurrVerif_t{
   float mag;
   float freq;
   uint8_t type;
} DOPI_CurrVerif_t;

typedef struct _DOPI_InInfo_t{
	DOPI_InMethod_t	inputMethod;
	float inputMin;
	float inputMax;
	float outputMin;
	float outputMax;
} DOPI_InInfo_t;

typedef struct _DOPI_SDOParam_t{
	DOP_SDOStatus_t SDOStatus;
	uint8_t numOfData;
	void* data;
} DOPI_SDOParam_t;

typedef void (*DOPI_SDOCB_t)(DOPI_SDOParam_t*, DOPI_SDOParam_t*);

typedef struct _DOPI_SDOUnit_t{
	uint8_t	taskID;
	uint16_t SDOID;
	DOPI_SDOParam_t param;
	DOPI_SDOCB_t callback;
} DOPI_SDOUnit_t;

typedef struct _DOPI_PDOUnit_t{
	uint8_t	taskID;
	uint16_t PDOID;
	void*	addr;
} DOPI_PDOUnit_t;

typedef struct _DOPI_SDOMsg_t{
	uint8_t numOfSDO;
	uint8_t msgLength;
	uint8_t txBuf[64];
} DOPI_SDOMsg_t;

typedef struct _DOPI_PDOMsg_t{
	uint8_t numOfPDO;
	uint8_t msgLength;
	uint8_t txBuf[64];
} DOPI_PDOMsg_t;

#ifdef CM_MODULE
typedef struct _DOPI_SYNCMsg_t{
	uint8_t numOfNode;
	uint8_t nodeMsgLengths[3];
	uint8_t nodeIDs[3];
	DOPI_PDOMsg_t PDOMsgs[3];
} DOPI_SYNCMsg_t;

#endif

typedef struct _DOPI_TaskObj_t{
	uint8_t taskID;
	uint8_t taskState;
	DOP_RoutineOnOff_t routines[DOP_ROUTINE_MAX_NUM];
	void* SDOsAddr[DOP_SDO_MAX_NUM];
	void* PDOsAddr[DOP_PDO_MAX_NUM];
} DOPI_TaskObj_t;

typedef struct _DOPI_DevObj_t{
	uint8_t  nodeID;
	uint16_t errCode;
	uint8_t  numOfTask;
	DOPI_TaskObj_t tasks[TASK_NUM];
} DOPI_DevObj_t;

#ifdef MD_MODULE
typedef struct _DOPI_SharedBuff_t{
	float buff1[D10_BUFFER_ARRAY_MAX_SIZE];
	float buff2[D10_BUFFER_ARRAY_MAX_SIZE];
	float buff3[D10_BUFFER_ARRAY_MAX_SIZE];
	float buff4[D10_BUFFER_ARRAY_MAX_SIZE];
} DOPI_SharedBuff_t;

typedef struct _DOPI_TrajBuff_t{
	float buff[D10_TRAJECTORY_TOTAL_LENGTH];
	uint16_t frameIDX;
} DOPI_TrajBuff_t;

#endif

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

void DOPI_InitDevObj(DOPI_DevObj_t* t_obj, uint8_t t_nodeID);

DOPI_SDOUnit_t DOPI_CreateSDOUnit(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID, DOP_SDOStatus_t t_SDOStatus, uint8_t t_numOfData);
DOPI_Status_t DOPI_AppendSDO(DOPI_SDOUnit_t* t_SDOUnit, DOPI_SDOMsg_t* t_SDOMsg);
void DOPI_ClearSDO(DOPI_SDOMsg_t* t_SDOMsg);

DOPI_PDOUnit_t DOPI_CreatePDOUnit(uint8_t t_taskID, uint16_t t_PDOID, void* t_addr);
DOPI_Status_t DOPI_AppendPDO(DOPI_PDOUnit_t* t_PDOUnit, DOPI_PDOMsg_t* t_PDOMsg);
void DOPI_ClearPDO(DOPI_PDOMsg_t* t_PDOMsg);

void DOPI_SetSDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID, void* t_addr);
void* DOPI_GetSDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_SDOID);

void DOPI_SetPDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_PDOID, void* t_addr);
void* DOPI_GetPDOAddr(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint16_t t_PDOID);

void DOPI_SetRoutOnOff(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint8_t t_routID, DOP_RoutineOnOff_t t_OnOff);
DOP_RoutineOnOff_t DOPI_GetRoutOnOff(DOPI_DevObj_t* t_obj, uint8_t t_taskID, uint8_t t_routID);

int DOPI_UnpackSDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr);
int DOPI_UnpackPDO(DOPI_DevObj_t* t_obj, uint8_t* t_byteArr);

#ifdef CM_MODULE
void DOPI_UnpackEMCY(uint32_t* t_errCode, uint8_t* t_byteArr);

#endif /* CM Modules */

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
//void DOP_Init();
//void DOP_InitDefaultDOD();
// TxRx Protocols
int DOP_Tx(uint8_t* byte_arr, uint16_t* byte_len);
int DOP_Rx(uint8_t* byte_arr, uint16_t byte_len);

// PDO Helper
void DOP_AddPDOtoSend(uint8_t dictID, uint16_t objID);
void DOP_ClearPDOtoSend();

void DOP_AddPDOtoRecv(uint8_t dictID, uint16_t objID);

// SDO Helper
void DOP_AddSDOtoReq(uint8_t dictID, uint16_t objID, void* data, uint16_t size);

#endif

#if __cplusplus
}
#endif

#endif /* DOP_MNGR_INC_DATA_OBJECT_INTERFACE_H_ */
