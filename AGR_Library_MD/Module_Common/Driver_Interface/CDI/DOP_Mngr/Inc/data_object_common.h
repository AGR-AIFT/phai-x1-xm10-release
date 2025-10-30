
#ifndef DOP_MNGR_INC_DATA_OBJECT_COMMON_H_
#define DOP_MNGR_INC_DATA_OBJECT_COMMON_H_

#include <stdio.h>
#include <stdlib.h>

#include "task_mngr.h"
#include "data_object_interface.h"

#ifdef _USE_OS_RTOS
#include "cmsis_os.h"
#endif

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#ifdef CM_MODULE
/* General Constants */
#define CARTESIAN_AXIS_NUM 3 // xyz

#ifdef WALKON5_CM_ENABLED
#define ACTUATOR_NUM 16
#else
#define ACTUATOR_NUM 4

#endif /* WALKON5_CM_ENABLED */

/* Task Index */
#define TASK_NUMS                   10
#define TASK_IDX_AM_COMM_HDLR   	0
#define TASK_IDX_BLE_COMM_HDLR  	1
#define TASK_IDX_DATA_CTRL    	    2
#define TASK_IDX_DEBUG_CTRL 		3
#define TASK_IDX_DEV_COMM_HDLR  	4
#define TASK_IDX_EXT_DEV_CTRL	    5
#define TASK_IDX_GAIT_CTRL   	    6
#define TASK_IDX_IMU_CTRL   	    7
#define TASK_IDX_SYSTEM_CTRL 	    8
#define TASK_IDX_WHOLE_BODY_CTRL	9

/* DOD ID */
#define DOD_ID_AM_COMM_HDLR   	0
#define DOD_ID_BLE_COMM_HDLR  	1
#define DOD_ID_DATA_CTRL    	2
#define DOD_ID_DEBUG_CTRL 		3
#define DOD_ID_DEV_COMM_HDLR 	4
#define DOD_ID_EXT_DEV_CTRL	    5
#define DOD_ID_GAIT_CTRL   	    6
#define DOD_ID_IMU_CTRL   	    7
#define DOD_ID_SYSTEM_CTRL 	    8
#define DOD_ID_WHOLE_BODY_CTRL	9

/* Common PDO */
#define DOP_COMMON_PDO_CREATE(t_dictID, t_task)                                                                 \
    DOP_CreatePDO(t_dictID, 1, DOP_UINT8,  1, (uint8_t*)&((t_task).stateMachine.currState));                    \
    DOP_CreatePDO(t_dictID, 2, DOP_UINT32, 1, &(t_task).errCode);

#endif

/* Common SDO */
#define DOP_COMMON_SDO_CB(t_task)                                                                               \
    static void GetTaskStateCB(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)                                          \
    {                                                                                                           \
        res->dataSize = 1;                                                                                      \
        res->data = malloc(1);                                                                                  \
        ((uint8_t*)res->data)[0] = (uint8_t)((t_task).stateMachine.currState);                                  \
        res->status = DOP_SDO_SUCC;                                                                             \
    }                                                                                                           \
                                                                                                                \
    static void SetTaskStateCB(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)                                          \
    {                                                                                                           \
        StateEnum_t state_cmd = (StateEnum_t)(((uint8_t*)req->data)[0]);                                        \
        StateTransition(&(t_task).stateMachine, state_cmd);                                                     \
        res->data = NULL;                                                                                       \
        res->dataSize = 0;                                                                                      \
        res->status = DOP_SDO_SUCC;                                                                             \
    }                                                                                                           \
                                                                                                                \
    static void GetTaskRoutineCB(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)                                        \
    {                                                                                                           \
        res->status = DOP_SDO_SUCC;                                                                             \
        res->dataSize = ((t_task).routine.numOfRoutineID);                                                      \
        if (res->dataSize == 0) {                                                                               \
            return;                                                                                             \
        }                                                                                                       \
        res->data = malloc(req->typeSize * res->dataSize);                                                      \
        for (int i = 0; i < res->dataSize; ++i){                                                                \
            ((uint8_t*)res->data)[i] = (uint8_t)((t_task).routine.id[i]);                                       \
        }                                                                                                       \
    }                                                                                                           \
                                                                                                                \
    static void SetTaskRoutineCB(DOP_SDOArgs_t* req, DOP_SDOArgs_t* res)                                        \
    {                                                                                                           \
        res->dataSize = 0;                                                                                      \
        ClearRoutines(&(t_task).routine);                                                                       \
        if (req->dataSize == 0) {                                                                               \
            res->status = DOP_SDO_SUCC;                                                                         \
            return;                                                                                             \
        }                                                                                                       \
        res->data = malloc(req->typeSize * req->dataSize);                                                      \
        for (int i = 0; i < req->dataSize; ++i){                                                                \
            uint8_t id = ((uint8_t*)req->data)[i];                                                              \
            if (PushRoutine(&(t_task).routine, id) == 0) {                                                      \
                ((uint8_t*)res->data)[i] = ((uint8_t*)req->data)[i];                                            \
                ++res->dataSize;                                                                                \
            }                                                                                                   \
        }                                                                                                       \
        res->status = (res->dataSize != req->dataSize) ? DOP_SDO_FAIL : DOP_SDO_SUCC;                           \
    }

// Create all common SDOs
#define DOP_COMMON_SDO_CREATE(t_taskID)                                                                         \
	DOP_CreateSDO(t_taskID, 0, DOP_UINT8, GetTaskStateCB);                                                      \
    DOP_CreateSDO(t_taskID, 1, DOP_UINT8, SetTaskStateCB);                                                      \
    DOP_CreateSDO(t_taskID, 2, DOP_UINT8, GetTaskRoutineCB);                                                    \
    DOP_CreateSDO(t_taskID, 3, DOP_UINT8, SetTaskRoutineCB);                                                    \

#ifdef CM_MODULE
#define DOP_COMMON_PARAM_CHECK_SIZE                                                                             \
        if (req->dataSize == 0) {                                                                               \
            res->status = DOP_SDO_FAIL;                                                                         \
            return;                                                                                             \
        }                                                                                                       \

#define DOP_COMMON_PARAM_COPY_VALUES(dest)                                                                      \
        memcpy((dest), req->data, req->typeSize * req->dataSize);                                               \
        res->status = DOP_SDO_SUCC;                                                                             \

#endif

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

#ifdef CM_MODULE
void DOPC_AssignTaskID(TaskObj_t* p_taskObj, int t_IDX);
uint8_t DOPC_GetTaskState(int t_IDX);
void DOPC_SetTaskState(int t_IDX, uint8_t t_state);

#endif

#endif /* DOP_MNGR_INC_DATA_OBJECT_COMMON_H_ */
