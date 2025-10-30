/**
 * @file msg_hdlr_task.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics FW Team
 * @brief 
 * @ref 
 */

#ifndef APPS_MSG_HDLR_INC_MSG_HDLR_H_
#define APPS_MSG_HDLR_INC_MSG_HDLR_H_

#include "task_mngr.h"
#include "error_dictionary.h"
//#include "usbd_cdc_if.h"

#include "ioif_fdcan_common.h"
#include "ioif_tim_common.h"
#include "ioif_gpio_common.h"
//#include "ioif_usb_common.h"

#include "module.h"
#include "cvector.h"
#include "data_object_common.h"
#include "data_object_interface.h"

#include "exppack_ctrl.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */


#define MEMORY_SECOND_HAND_CHECK	1

#define USB_CDC_MSG_TX_TIMEOUT_MS 	1


/* By 1NE (Temporary setting) */
#define EXPANSION_BOARD_NODEID		8		// MD3(RK) for H10 connection


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef enum {
	FIRST_USE = 0x01,
	SECOND_USE,
	MEMORY_UI_MOTOR_PROPERTIES,
	MEMORY_UI_SENSOR_SETTING,
	MEMORY_UI_ELECTRICAL_PROPERTIES,
	MEMORY_UI_MECHANICAL_PROPERTIES,
	MEMORY_UI_CONTROL_PARAMETERS1,
	MEMORY_UI_CONTROL_PARAMETERS2,
	MEMORY_UI_ADDITIONAL_FUNCTION_PARAMETERS,

	E_SYS_BATCH,
	E_SYS_BEMF,
	BEMF_ID_OVER_CURRENT,
	BW_CHECK,
	FRICTION_ID_RAW_DATA,
	FRICTION_ID_AVERAGED_DATA,
	FRICTION_ID_DONE,
	FRICTION_COMPENSATOR_VERIFICATION,

	MECH_SYS_ID_SBS_RAW_DATA,
	MECH_SYS_ID_SBS_DONE,

	IRC_VERIFICATION,

	GET_IMPEDANCE_SINE_CTRL,
	GET_IMPEDANCE_REC_CTRL,

	GAIN_TUNER,
	GET_VELOCITY_CTRL,
	GET_POSITION_CTRL,

	GET_HALLSENSOR,
	GET_INCENCODER,
	GET_ABSENCODER1,
	GET_ABSENCODER2,

	GET_VSD_UPPER_LIMIT,
	GET_VSD_LOWER_LIMIT,

	VSD_VERIFICATION_DATA,

	GET_BACKLASH_TEST,
	GET_DOB_DATA,

	GET_DIRECTION_SET_DATA,
	GET_DIRECTION_SET_DONE,

	SAVE_DONE,
	GET_VE_TEST_DATA,
	GET_SYSTEM_ID_VERIFY,
	VE_KF_SETTING_ERROR,
	GET_FF_CTRL,
	GET_TOTAL_CTRL,

	ADV_FRICTION_ID_DATA,
	ADV_FRICTION_ID_DONE,
} GUISequence_Enum;

typedef enum {
	IDLE,
	UPLOAD_PROPERTIES,
	SAVE_PROPERTIES,
	DOWNLOAD_PROPERTIES,
	ELECTRICAL_SYSTEM_ID,
	BEMF_ID,
	CURRENT_BANDWIDTH_CHECK,
	AUTO_TUNING,
	ADV_FRICTION_ID,
	CAL_FRICTION_LUT,
} MainSequence_Enum;

typedef enum _COMMType {
	COMM_TYPE_FDCAN,
	COMM_TYPE_USB
} COMMType;


#ifdef FDCAN_WALKONSUIT_ENABLED
typedef struct _WS5_ExtBoard_Transmit_Data_16BIT{
	int16_t IMU1_W;
	int16_t IMU1_X;
	int16_t IMU1_Y;
	int16_t IMU1_Z;
	int16_t IMU2_W;
	int16_t IMU2_X;
	int16_t IMU2_Y;
	int16_t IMU2_Z;
	int16_t IMU3_W;
	int16_t IMU3_X;
	int16_t IMU3_Y;
	int16_t IMU3_Z;
	int16_t IMU4_W;
	int16_t IMU4_X;
	int16_t IMU4_Y;
	int16_t IMU4_Z;

//int16_t pMMG_R_Thigh;
//int16_t pMMG_L_Thigh;
//int16_t pMMG_R_Shank;
//int16_t pMMG_L_Shank;

	float K_pMMG;
	float K_IMU;

} WS5_ExtBoard_Transmit_Data_16BIT;

typedef struct _WS5_ExtBoard_Transmit_Data_32BIT{
	uint32_t loop_cnt;
} WS5_ExtBoard_Transmit_Data_32BIT;
#endif


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */

extern TaskObj_t			msg_hdlr_task;
extern MainSequence_Enum 	MS_enum;
extern uint8_t           	MD_nodeID;

/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

void Send_EMCY(uint32_t* f_err_code);
int Send_MSG(uint16_t t_COB_ID, uint8_t* t_tx_data, uint32_t t_len);
void SendDurationCompletedFlag_RH(void);
void SendDurationCompletedFlag_LH(void);
void Send_ExtensionBoardEnable(void);
void Send_ExtensionBoardDisable(void);

void InitMsgHdlr(void);
void RunMsgHdlr(void* params);


#endif /* APPS_MSG_HDLR_TASK_INC_MSG_HDLR_H_ */
