

#include "data_object.h"

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
DOP_Dict_t** dods;
int dods_size;

#endif

/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */

static DOP_Dict_t DODs[TASK_NUM] = {0};


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
static int CompareID(const void* lt, const void* rt);
static DOP_Dict_t* FindDOD(uint8_t dictID);
static void FreePDO(uint8_t dictID);
static void FreeSDO(uint8_t dictID);
static int PDO_CSV_Row(uint8_t dictID, int row, char* csv_str, uint16_t csv_len);
static int SDO_CSV_Row(uint8_t dictID, int row, char* csv_str, uint16_t csv_len);

#endif




/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

DOP_DataTypeInfo_t DOP_GetDataTypeInfo(DOP_DataType_t t_type)
{
	DOP_DataTypeInfo_t t_res;

    switch (t_type) {
    case DOP_CHAR   :   t_res.typeName = "char"   ;     t_res.typeSize = sizeof(char);      break;
    case DOP_UINT8  :   t_res.typeName = "uint8"  ;     t_res.typeSize = sizeof(uint8_t);   break;
    case DOP_UINT16 :   t_res.typeName = "uint16" ;     t_res.typeSize = sizeof(uint16_t);  break;
    case DOP_UINT32 :   t_res.typeName = "uint32" ;     t_res.typeSize = sizeof(uint32_t);  break;
    case DOP_INT8   :   t_res.typeName = "int8"   ;     t_res.typeSize = sizeof(int8_t);    break;
    case DOP_INT16  :   t_res.typeName = "int16"  ;     t_res.typeSize = sizeof(int16_t);   break;
    case DOP_INT32  :   t_res.typeName = "int32"  ;     t_res.typeSize = sizeof(int32_t);   break;
    case DOP_FLOAT32:   t_res.typeName = "float32";     t_res.typeSize = sizeof(float);     break;
    case DOP_FLOAT64:   t_res.typeName = "float64";     t_res.typeSize = sizeof(double);    break;
    case DOP_STRING10:	t_res.typeName = "string10";    t_res.typeSize = 32*sizeof(char);   break;
    default:                                                                                break;
    }
    return t_res;
}

/* ------------------- DATA OBJECT DICTIONARY ------------------- */
// Create Data Object & Dictionary
void DOP_CreateDOD(uint8_t t_dictID)
{
	DODs[t_dictID].dictID = t_dictID;
}

void DOP_CreateSDO(uint8_t t_dictID, uint16_t t_objID, DOP_DataType_t t_type, DOP_SDOCB_t t_callback)
{
	DODs[t_dictID].SDOs[t_objID].objID          = t_objID;
	DODs[t_dictID].SDOs[t_objID].dataType       = t_type;
	DODs[t_dictID].SDOs[t_objID].callback       = t_callback;
	DODs[t_dictID].SDOs[t_objID].args.status    = DOP_SDO_IDLE;
	DODs[t_dictID].SDOs[t_objID].args.dataSize  = 0;
	DODs[t_dictID].SDOs[t_objID].args.data      = NULL;
	DODs[t_dictID].SDOs[t_objID].args.typeSize  = DOP_GetDataTypeInfo(t_type).typeSize;
}

void DOP_CreatePDO(uint8_t t_dictID, uint16_t t_objID, DOP_DataType_t t_type, uint8_t t_size, void* t_addr)
{
    DODs[t_dictID].PDOs[t_objID].objID          = t_objID;
    DODs[t_dictID].PDOs[t_objID].dataType       = t_type;
    DODs[t_dictID].PDOs[t_objID].dataSize       = t_size;
    DODs[t_dictID].PDOs[t_objID].addr           = t_addr;
    DODs[t_dictID].PDOs[t_objID].objSize       	= DOP_GetDataTypeInfo(t_type).typeSize * t_size;
    DODs[t_dictID].PDOs[t_objID].lastPub        = malloc(DODs[t_dictID].PDOs[t_objID].objSize);

    memset(DODs[t_dictID].PDOs[t_objID].lastPub, 0xFF, DODs[t_dictID].PDOs[t_objID].objSize);
}

DOP_SDO_t* DOP_FindSDO(uint8_t t_dictID, uint16_t t_objID)
{
	return &DODs[t_dictID].SDOs[t_objID];
}

DOP_PDO_t* DOP_FindPDO(uint8_t t_dictID, uint16_t t_objID)
{
	return &DODs[t_dictID].PDOs[t_objID];
}

uint16_t DOP_CallSDO(DOP_SDO_t* t_SDO, DOP_SDOArgs_t* t_req)
{
    if (t_SDO->args.data != NULL){
        free(t_SDO->args.data);
        t_SDO->args.data = NULL;
    }

    t_SDO->args.status = DOP_SDO_IDLE;
// t_req->typeSize = DOP_GetDataTypeInfo(t_SDO->dataType).typeSize;
    t_SDO->callback(t_req, &t_SDO->args);
    return t_req->dataSize * t_SDO->args.typeSize;
}

void DOP_SetSDOReq(uint8_t t_dictID, uint16_t t_objID, void* t_data, uint8_t t_size)
{
    DOP_SDOArgs_t t_req;

    DOP_SDO_t* t_SDO = DOP_FindSDO(t_dictID, t_objID);
    if (t_SDO == NULL) {
        return;
    }

    t_req.status = DOP_SDO_REQU;
    t_req.data = t_data;
    t_req.dataSize = t_size;
    DOP_SetSDOArgs(t_SDO, &t_req);
}

DOP_SDOArgs_t* DOP_GetSDORes(uint8_t t_dictID, uint16_t t_objID)
{
    DOP_SDO_t* t_SDO = DOP_FindSDO(t_dictID, t_objID);
    if (t_SDO == NULL) {
        return NULL;
    }

    return &t_SDO->args;
}

uint16_t DOP_SetSDOArgs(DOP_SDO_t* t_SDO, DOP_SDOArgs_t* t_args)
{
    // Copy status
	t_SDO->args.status = t_args->status;
    
    // Copy size
    int t_totalSize = t_SDO->args.typeSize * t_args->dataSize;
    t_SDO->args.dataSize = t_args->dataSize;
    if (t_totalSize <= 0) {
        return 0;
    }

    // Copy data
    if (t_SDO->args.data != NULL) {
        free(t_SDO->args.data);
        t_SDO->args.data = NULL;
    }
    t_SDO->args.data = malloc(t_totalSize);
    memcpy(t_SDO->args.data, t_args->data, t_totalSize);

    return t_totalSize;
}

uint16_t DOP_SetPDO(DOP_PDO_t* t_PDO, void* t_data)
{    
    memcpy(t_data, t_PDO->addr, t_PDO->objSize);
    memcpy(t_PDO->lastPub, t_PDO->addr, t_PDO->objSize);
    return t_PDO->objSize;
}

uint16_t DOP_GetPDO(DOP_PDO_t* t_PDO, void* t_data)
{
    memcpy(t_PDO->addr, t_data, t_PDO->objSize);
    return t_PDO->objSize;
}

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
void DataObject_FreeDODs()
{
    for (int i = 0; i < dods_size; ++i) {
        if (dods[i] != NULL) {
            FreePDO(i);
            FreeSDO(i);
            free(dods[i]->name);
            free(dods[i]);
            dods[i] = NULL;
        }
    }
    free(dods);
    dods = NULL;
    dods_size = 0;
}

int DataObject_DODlist(char** list_str, uint16_t* list_len)
{
    *list_len = 0;
    if (*list_str != NULL) {
        free(*list_str);
    }
    *list_str = NULL;

    if (dods_size <= 0) {
        char* str = "No DOD found.\n";
        *list_len = strlen(str)+1;
        *list_str = malloc(*list_len);
        strcpy(*list_str, str);
        return -1;
    }
    char sep;
    int name_len[dods_size];
    int cursor = 0;
    for (int i = 0; i < dods_size; ++i) {
        sep = (i == dods_size-1) ? '\n' : ',';
        name_len[i] = snprintf(NULL, 0, "%s%c",dods[i]->name, sep);


        cursor += name_len[i];
    }
    cursor += 1; // string end

    *list_len = cursor;
    *list_str = (char*)malloc(cursor);

    cursor = 0;
    for (int i = 0; i < dods_size; ++i) {
        sep = (i == dods_size-1) ? '\n' : ',';
        snprintf(*list_str + cursor, name_len[i]+1, "%s%c",dods[i]->name, sep);
        cursor += name_len[i];
    }

    return 0;
}

int DataObject_PDO2CSV(char** csv_str, uint16_t* csv_len, uint8_t dictID)
{
    *csv_len = 0;
    if (*csv_str != NULL) {
        free(*csv_str);
    }
    *csv_str = NULL;

    DOP_Dict_t* dod = FindDOD(dictID);
    if (dod == NULL) {
        // TODO: return fail reason
        char* str = "Exporting csv failed.\n";
        *csv_len = strlen(str)+1;
        *csv_str = malloc(*csv_len);
        strcpy(*csv_str, str);
        return -1;
    }

    int cursor = 0;
    int rows = cvector_size(dod->PDOs);
    if (rows <= 0) {
        char* str = "No PDO found.\n";
        *csv_len = strlen(str)+1;
        *csv_str = malloc(*csv_len);
        strcpy(*csv_str, str);
        return -1;
    }

    int row_len[rows];
    for (int i = 0; i < rows; ++i) {
        row_len[i] = PDO_CSV_Row(dictID, i, NULL, 0);
        cursor += row_len[i];
    }
    cursor += 1; // string end
    
    *csv_str = malloc(cursor);
    *csv_len = cursor;
    
    cursor = 0;
    for (int i = 0; i < rows; ++i) {
        cursor += PDO_CSV_Row(dictID, i, *csv_str + cursor, row_len[i]+1);
    }

    return 0;
}

int DataObject_SDO2CSV(char** csv_str, uint16_t* csv_len, uint8_t dictID)
{
    *csv_len = 0;
    if (*csv_str != NULL) {
        free(*csv_str);
    }
    *csv_str = NULL;

    DOP_Dict_t* dod = FindDOD(dictID);
    if (dod == NULL) {
        // TODO: return fail reason
        char* str = "Exporting csv failed.\n";
        *csv_len = strlen(str)+1;
        *csv_str = malloc(*csv_len);
        strcpy(*csv_str, str);
        return -1;
    }

    int cursor = 0;
    int rows = cvector_size(dod->SDOs);
    if (rows <= 0) {
        char* str = "No SDO found.\n";
        *csv_len = strlen(str)+1;
        *csv_str = malloc(*csv_len);
        strcpy(*csv_str, str);
        return -1;
    }

    int row_len[rows];
    for (int i = 0; i < rows; ++i) {
        row_len[i] = SDO_CSV_Row(dictID, i, NULL, 0);
        cursor += row_len[i];
    }
    cursor += 1; // string end
    
    *csv_str = malloc(cursor);
    *csv_len = cursor;
    
    cursor = 0;
    for (int i = 0; i < rows; ++i) {
        cursor += SDO_CSV_Row(dictID, i, *csv_str + cursor, row_len[i]+1);
    }

    return 0;
}

void DefaultDOD_GetDODs(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    if (DataObject_DODlist((char**)&res->data, &res->dataSize) < 0) {
        res->status = DOP_SDO_FAIL;
    } else {
        res->status = DOP_SDO_SUCC;
    }
    
}

void DefaultDOD_GetPDOs(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    if (DataObject_PDO2CSV((char**)&res->data, &res->dataSize, ((uint8_t*)req->data)[0]) < 0) {
        res->status = DOP_SDO_FAIL;
    } else {
        res->status = DOP_SDO_SUCC;
    }
}

void DefaultDOD_GetSDOs(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)
{
    if (DataObject_SDO2CSV((char**)&res->data, &res->dataSize, ((uint8_t*)req->data)[0]) < 0) {
        res->status = DOP_SDO_FAIL;
    } else {
        res->status = DOP_SDO_SUCC;
    }
}

#endif

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED)
static int CompareID(const void* lt, const void* rt)
{
    return (int)*(uint8_t *)lt - (int)*(uint8_t *)rt;
}

static DOP_Dict_t* FindDOD(uint8_t dictID)
{
    if (dods_size < dictID) {
        return NULL;
    }
    
    return dods[dictID];
}

static void FreePDO(uint8_t dictID)
{
    DOP_Dict_t* dod = dods[dictID];
    for (int i = 0; i < cvector_size(dod->PDOs); ++i) {
        if (dod->PDOs[i].name) {
            free(dod->PDOs[i].name);
            dod->PDOs[i].name = NULL;
        }

        if (dod->PDOs[i].lastPub) {
            free(dod->PDOs[i].lastPub);
            dod->PDOs[i].lastPub = NULL;
        }
    }
    cvector_free(dod->PDOs);
}

static void FreeSDO(uint8_t dictID)
{
    DOP_Dict_t* dod = dods[dictID];
    for (int i = 0; i < cvector_size(dod->SDOs); ++i) {
        if (dod->SDOs[i].name != NULL) {
            free(dod->SDOs[i].name);
            dod->SDOs[i].name = NULL;
        }

        if (dod->SDOs[i].args.data != NULL) {
            free(dod->SDOs[i].args.data);
            dod->SDOs[i].args.data = NULL;
        }
    }
    cvector_free(dod->SDOs);
}


static int PDO_CSV_Row(uint8_t dictID, int row, char* csv_str, uint16_t csv_len)
{
    DOP_PDO_t* pdo = &FindDOD(dictID)->PDOs[row];
    return snprintf(csv_str, csv_len, "%d,%d,%d,%s\n", pdo->objID, (uint8_t)pdo->dataType, pdo->dataSize, pdo->name);
}

static int SDO_CSV_Row(uint8_t dictID, int row, char* csv_str, uint16_t csv_len)
{
    DOP_SDO_t* sdo = &FindDOD(dictID)->SDOs[row];
    return snprintf(csv_str, csv_len, "%d,%d,%s\n", sdo->objID, (uint8_t)sdo->dataType, sdo->name);
}

#endif
