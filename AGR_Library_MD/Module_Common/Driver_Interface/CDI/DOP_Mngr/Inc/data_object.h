
#ifndef DOP_MNGR_INC_DATA_OBJECT_H_
#define DOP_MNGR_INC_DATA_OBJECT_H_

#if __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "cvector.h"
#include "data_object_dictionaries.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define DOP_SDO_NOTHING 1

#define DOP_SDO_IDLE  	2
#define DOP_SDO_REQU  	1
#define DOP_SDO_SUCC  	0
#define DOP_SDO_FAIL 	-1

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
#define DOP_DEFAULT_DOD 		0
#define DOP_SDO_GET_DOD_LIST 	0
#define DOP_SDO_GET_PDO_LIST 	1
#define DOP_SDO_GET_SDO_LIST 	2

#endif

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef struct _DOP_Header_t{
	uint8_t dictID;
	uint8_t objID;
} DOP_Header_t;

typedef struct _DOP_DataTypeInfo_t{
    char* typeName;
    uint8_t typeSize;
} DOP_DataTypeInfo_t;

typedef struct _DOP_SDOArgs_t{
    void* data;
    uint8_t dataSize;
	uint16_t typeSize;
    int8_t status;
} DOP_SDOArgs_t;

typedef void (*DOP_SDOCB_t) (DOP_SDOArgs_t*, DOP_SDOArgs_t*);

typedef struct _DOP_SDO_t{
    uint16_t objID;
    DOP_DataType_t dataType;
    DOP_SDOCB_t callback;
    DOP_SDOArgs_t args;
    #if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
	char* name;

	#endif
} DOP_SDO_t;

typedef struct _DOP_PDO_t{
    uint16_t objID;
    DOP_DataType_t dataType;
	uint8_t dataSize;
    void* addr;
    void* lastPub;
    uint16_t objSize;
    #if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
	char* name;

	#endif
} DOP_PDO_t;

typedef struct _DOP_Dict_t{
	uint8_t dictID;
    DOP_PDO_t PDOs[DOP_PDO_MAX_NUM];
    DOP_SDO_t SDOs[DOP_SDO_MAX_NUM];
    #if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
	char* name;

	#endif
} DOP_Dict_t;

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

DOP_DataTypeInfo_t DOP_GetDataTypeInfo(DOP_DataType_t t_type);

void DOP_CreateDOD(uint8_t t_dictID);
void DOP_CreateSDO(uint8_t t_dictID, uint16_t t_objID, DOP_DataType_t t_type, DOP_SDOCB_t t_callback);
void DOP_CreatePDO(uint8_t t_dictID, uint16_t t_objID, DOP_DataType_t t_type, uint8_t t_size, void* t_addr);

DOP_SDO_t* DOP_FindSDO(uint8_t t_dictID, uint16_t t_objID);
DOP_PDO_t* DOP_FindPDO(uint8_t t_dictID, uint16_t t_objID);

void DOP_SetSDOReq(uint8_t t_dictID, uint16_t t_objID, void* t_data, uint8_t t_size);
DOP_SDOArgs_t* DOP_GetSDORes(uint8_t t_dictID, uint16_t t_objID);
uint16_t DOP_CallSDO(DOP_SDO_t* t_SDO, DOP_SDOArgs_t* t_req);
uint16_t DOP_SetSDOArgs(DOP_SDO_t* t_SDO, DOP_SDOArgs_t* t_args);

uint16_t DOP_SetPDO(DOP_PDO_t* t_PDO, void* t_data);
uint16_t DOP_GetPDO(DOP_PDO_t* t_PDO, void* t_data);

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
int DataObject_DODlist(char** list_str, uint16_t* list_len);
int DataObject_POD2CSV(char** csv_str, uint16_t* csv_len, uint8_t t_dictID);
int DataObject_SOD2CSV(char** csv_str, uint16_t* csv_len, uint8_t t_dictID);

void DataObject_FreeDODs();

void DefaultDOD_GetDODs(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
void DefaultDOD_GetPDOs(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);
void DefaultDOD_GetSDOs(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res);

#endif

#if __cplusplus
}
#endif


#endif /* DOP_MNGR_INC_DATA_OBJECT_H_ */
