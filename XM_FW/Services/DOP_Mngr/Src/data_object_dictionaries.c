

#include "data_object_dictionaries.h"

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

SDOInfo_t SDOTable[TASK_NUM][DOP_SDO_MAX_NUM] = {0};
PDOInfo_t PDOTable[TASK_NUM][DOP_PDO_MAX_NUM] = {0};


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

static void AssembleSDO(SDOInfo_t* t_addr, uint8_t t_dataType);
static void AssemblePDO(PDOInfo_t* t_addr, uint8_t t_dataType, uint8_t t_numOfData);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- ASSEMBLE ------------------- */
uint8_t DOP_ConvertDataSize(int t_dataType)
{
	switch(t_dataType) {
		case DOP_CHAR: 			return 1; 		break;
		case DOP_UINT8: 		return 1;		break;
		case DOP_UINT16:		return 2;		break;
		case DOP_UINT32:		return 4;		break;
		case DOP_INT8:			return 1;		break;
		case DOP_INT16:			return 2;		break;
		case DOP_INT32:			return 4;		break;
		case DOP_FLOAT32:		return 4;		break;
		case DOP_FLOAT64:		return 8;		break;
		case DOP_STRING10:		return 32;		break;
		default:				return 0;		break;
	}
}

/* ------------------- SDO TABLE ------------------- */
void DOP_CreateSDOTable(void)
{
	//*******************************************************************************************************************//
	//						|	  Task_ID	  |			SDO_ID			|									 DATA_TYPE | //
	//*******************************************************************************************************************//
	/* Low Level Ctrl Task */
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_GET_STATE],  	    						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_STATE],  	    						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_GET_ROUTINE],  								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_ROUTINE],  								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_NAME],  										DOP_STRING10);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_POLE_PAIR],  								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_ENCODER_RESOLUTION],  						DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_GEAR_RATIO],  								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_TORQUE_CONSTANT],  							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_VELOCITY_CONSTANT],  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_PEAK_CURRENT_LIMIT],  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_CONTINUOUS_CURRENT_LIMIT],  					DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_MAX_VELOCITY],		  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_COMMUTATION_DUTY],  							DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_USER_DIRECTION],  							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_ELEC_SYSTEM_ID_MAG],  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_TERMINAL_RESISTANCE],  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_TERMINAL_INDUCTANCE],  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_BEMF_ID_VELOCITY],  							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_BEMF_ID_GAIN_PCTG],  						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_CURRENT_CTRL_BW_RAD],  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_INERTIA],  									DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_DAMPING_COEF],  								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_MECH_MODEL_A],								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_MECH_MODEL_B],  								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_FRICTION_ID_INFO],  							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_FRICTION_LUT_INFO],  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_COMMUTATION_SENSOR],    					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_POS_FEEDBACK_SENSOR],  					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_E_ANGLE_HOMING_SENSOR],  				DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_M_ANGLE_HOMING_SENSOR],  				DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_SENSOR_USAGE],  	        				DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_LOWLEVEL] [SDO_ID_LOWLEVEL_SET_AUX_INPUT],								DOP_FLOAT32);

	/* Mid Level Ctrl Task */
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_GET_STATE],  								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_SET_STATE],  								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_GET_ROUTINE], 								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_SET_ROUTINE], 								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_IRC_NUMERATOR_LENGTH],	 					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_IRC_DENOMINATOR_LENGTH],						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_IRC_NUMERATOR],								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_IRC_DENOMINATOR],	  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_IRC_SATURATION],	  							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_IMP_VIRTUAL_STIFFNESS],	  					DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_IMP_VIRTUAL_DAMPER],	  						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VELOCITY_PERIODIC_SIG_INFO],					DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_POSITION_PERIODIC_SIG_INFO],					DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_DOB_Q_BW],									DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_DOB_GQ_NUM],									DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_DOB_GQ_DEN],									DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_DOB_Q_NUM],									DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_DOB_Q_DEN],									DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_DOB_SATURATION],								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VELOCITY_CTRL_BW],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VELOCITY_CTRL_P_GAIN],						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VELOCITY_CTRL_I_GAIN],						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_POSITION_CTRL_INPUT_PENALTY],				DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_POSITION_CTRL_P_GAIN],						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_POSITION_CTRL_D_GAIN],						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_MID_CTRL_SATURATION],						DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_INCENCODER_SET_OFFSET],						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_ABSENCODER1_SET_OFFSET],						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_ABSENCODER1_CHANGE_DIRECTION],				DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_ABSENCODER2_SET_OFFSET],						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_ABSENCODER2_CHANGE_DIRECTION],				DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VSD_STIFFNESS],								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VSD_DAMPER],									DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VSD_DAMPED_RANGE],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VSD_STIFF_RANGE],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_SET_VSD_UPPER_LIMIT],						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_SET_VSD_LOWER_LIMIT],						DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VSD_SATURATION],								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_FEEDFORWARD_NUM],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_FEEDFORWARD_DEN],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VELOCITY_ESTIMATOR],							DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_VELOCITY_ESTIMATOR_LEAD_LAG],				DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_ENCODER_RESOLUTION],							DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_SYSTEM_ID_SBS_INFO],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_SYSTEM_ID_VERIFICATION_MAG],					DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_P_VECTOR_YD],								DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_P_VECTOR_L],									DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_P_VECTOR_S0],								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_P_VECTOR_SD],								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_F_VECTOR_MODE_IDX],	       					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_F_VECTOR_TMAX],			   					DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_F_VECTOR_DELAY],			   					DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_F_VECTOR_ZERO],			   					DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_I_VECTOR_EPSILON],							DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_I_VECTOR_KP],	           					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_I_VECTOR_KD],	           					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_I_VECTOR_LAMBDA],							DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_I_VECTOR_DURATION],							DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_I_VECTOR_KP_MAX],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_I_VECTOR_KD_MAX],							DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_MIDLEVEL] [SDO_ID_MIDLEVEL_DESIRED_MECH_ANGLE],	       					DOP_FLOAT32);

	/* MSG Handler Task */
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_GET_STATE],  	    									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_SET_STATE],  	    									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_GET_ROUTINE],  										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_SET_ROUTINE],  										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_CLEARALL_ROUTINE],										DOP_UINT8);

	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_PDO_LIST], 											DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_MS_ENUM],  											DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_GUI_COMM_ONOFF],  										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_GUI_COMM_COMMAND], 									DOP_UINT8);

	// AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_SYNCH_MODE_ONOFF],										DOP_UINT16);
	// AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_GUI_QT_DETECT],										DOP_UINT8);
	// AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_GUI_QT_ID_RECEIVE],									DOP_UINT8);
	// AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_MD_PDO_ENABLE],										DOP_UINT8);
	// AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_MD_SEND_TRAJ],											DOP_UINT8);
	// AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_MD_DC_LENGTH_COMMAND],									DOP_UINT8);

	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_READ_NODE_ID], 										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_SDO_LIST], 											DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_MSG] [SDO_ID_MSG_SET_FDCAN_TEST_SWITCH], 								DOP_UINT32);

	/* IMU Task */
	AssembleSDO( &SDOTable [TASK_ID_IMU] [SDO_ID_IMU_GET_STATE],  											DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_IMU] [SDO_ID_IMU_SET_STATE],  											DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_IMU] [SDO_ID_IMU_GET_ROUTINE],											DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_IMU] [SDO_ID_IMU_SET_ROUTINE],											DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_IMU] [SDO_ID_IMU_FOR_TEST],												DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_IMU] [SDO_ID_IMU_MAG_INVA],												DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_IMU] [SDO_ID_IMU_MAG_IRON_ERROR],  										DOP_FLOAT32);

	/* Gait Task */
	AssembleSDO( &SDOTable [TASK_ID_GAIT] [SDO_ID_GAIT_GET_STATE],  										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_GAIT] [SDO_ID_GAIT_SET_STATE],  										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_GAIT] [SDO_ID_GAIT_GET_ROUTINE],										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_GAIT] [SDO_ID_GAIT_SET_ROUTINE],										DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_GAIT] [SDO_ID_GAIT_FOR_TEST],											DOP_UINT16);

	/* System Ctrl Task */
	AssembleSDO( &SDOTable [TASK_ID_SYSMNGT_MD] [SDO_ID_SYSMNGT_GET_STATE],  									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_SYSMNGT_MD] [SDO_ID_SYSMNGT_SET_STATE],  									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_SYSMNGT_MD] [SDO_ID_SYSMNGT_GET_ROUTINE],									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_SYSMNGT_MD] [SDO_ID_SYSMNGT_SET_ROUTINE],									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_SYSMNGT_MD] [SDO_ID_SYSMNGT_FOR_TEST],  									DOP_UINT16);

	/* Ext Dev Ctrl Task */
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_STATE],  									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_SET_STATE],  									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_ROUTINE],  									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_SET_ROUTINE],  									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_CLEARALL_ROUTINE],								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_DC_SET_LENGTH],  								DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_DC_SET_DIRECT],  								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_ROBOWEAR_MONITOR],								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_THIGH_THETA_RH],								DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_THIGH_THETA_LH],								DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_THETA_RH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_THETA_LH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_ACCX_RH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_ACCX_LH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_ACCY_RH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_ACCY_LH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_GYRZ_RH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_GYRZ_LH],  									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_GET_ASSIST_LEVEL],  								DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_MOTIONMAP_ID],  									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_YD_RH],									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_L_RH],									DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_S0_RH],									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_SD_RH],									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_YD_LH],									DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_L_LH],									DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_S0_LH],									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_P_VECTOR_SD_LH],									DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_RH_P_VECTOR_DURATION_COMPLETED],					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_LH_P_VECTOR_DURATION_COMPLETED],					DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_FSM_CURR],										DOP_UINT8);

	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_MODEID_RH],								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_COEFFICIENT_RH],						DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_GLOBALID_RH],							DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_DELAY_RH],								DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_MODEID_LH],								DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_COEFFICIENT_LH],						DOP_INT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_GLOBALID_LH],							DOP_UINT16);
	AssembleSDO( &SDOTable [TASK_ID_EXTDEV] [SDO_ID_EXTDEV_F_VECTOR_DELAY_LH],								DOP_UINT16);

	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_RH_NeutralPosture],			DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_LH_NeutralPosture],			DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_RK_NeutralPosture],			DOP_FLOAT32);
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_LK_NeutralPosture],			DOP_FLOAT32);

	// AssembleSDO( &SDOTable [TASK_ID_SYSMNGT_CM] [SDO_ID_SYSMNGT_CM_FLASH_WRITE_NOTI],  			DOP_UINT8);

	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_RH_P_VECTOR_DURATION_COMPLETED],	DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_LH_P_VECTOR_DURATION_COMPLETED],	DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_RK_P_VECTOR_DURATION_COMPLETED],	DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_LK_P_VECTOR_DURATION_COMPLETED],	DOP_UINT8);

	// Extension Board
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_EXTENSION_BOARD_ENABLE],			DOP_UINT8);
	AssembleSDO( &SDOTable [TASK_ID_WHOLEBODY] [SDO_ID_WHOLEBODY_EXTENSION_BOARD_DISABLE],			DOP_UINT8);
}

/* ------------------- PDO TABLE ------------------- */
void DOP_CreatePDOTable(void)
{
	//**********************************************************************************************************************************//
	//							      |	    Task_ID	    |				PDO_ID			     	     |      DATA_TYPE     |   #_of_DATA //
	//**********************************************************************************************************************************//
	/* Low Level Ctrl Task */
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_3PHASES_CURRENT_RAW],  			DOP_INT32, 	 		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_3PHASES_CURRENT_KF], 				DOP_FLOAT32,  		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_3PHASES_VOLTAGE_RAW],  			DOP_INT32, 	 		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_3PHASES_VOLTAGE_KF], 				DOP_FLOAT32,  		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_POSITION],  						DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_VELOCITY],  						DOP_INT32,  		2);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_CLARKE_OUT],  					DOP_INT32,  		2);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_PARK_OUT],  						DOP_FLOAT32,  		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_VOLTAGE_IN],	    				DOP_FLOAT32,  		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_ELEC_ANGLE],	    				DOP_UINT16,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_PRBS_DATA],	    				DOP_FLOAT32, 		2);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_TOTAL_CURRENT_INPUT],				DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_CURRENT_OUTPUT],  				DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_AUXILIARY_INPUT],  				DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_LOW_ID_PROCESS_INPUT],			DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_FRICTION_COMPENSATOR_INPUT],  	DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_MID_ID_PROCESS_INPUT],			DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_IRC_INPUT],						DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_MID_CTRL_INPUT],  				DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_ANALYZER_INPUT],					DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_LOWLEVEL] [PDO_ID_LOWLEVEL_COMMUTATION_STEP],				DOP_UINT8, 			1);

	/* Mid Level Ctrl Task */
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_LOOP_CNT],						DOP_UINT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_REF_POSITION],  					DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_REF_VELOCITY],  					DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_ACTUAL_POSITION], 				DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_ACTUAL_VELOCITY_RAW], 			DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_SYSTEM_ID_SBS_FREQ],				DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_IMP_CTRL_INPUT], 					DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_POS_PID_CTRL_INPUT], 				DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_VEL_PID_CTRL_INPUT],				DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_VSD_INPUT],						DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_UNIT_TRAJECTORY_BUFF_COUNT],		DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_F_VECTOR_INPUT],		        	DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_ABSENCODER1_POSITION],			DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_ABSENCODER2_POSITION],			DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_DOB_DISTURABNCE],					DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_DOB_INPUT],						DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_FF_INPUT],						DOP_FLOAT32, 		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MIDLEVEL] [PDO_ID_MIDLEVEL_VELOCITY_ESTIMATED],				DOP_FLOAT32, 		1);

	/* MSG Handler Task */
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST1],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST2],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST3],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST4],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST5],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST6],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST7],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST8],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST9],  									DOP_INT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_MSG] [PDO_ID_MSG_TEST10],  									DOP_INT32,  		1);

	/* IMU Task */
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_ROLL],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_PITCH],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_YAW],  										DOP_FLOAT32,  		1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_ACC_X],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_ACC_Y],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_ACC_Z],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_GYR_X],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_GYR_Y],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_GYR_Z],  									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_MAG_X],  									DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_MAG_Y],  									DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_MAG_Z],  									DOP_FLOAT32,		1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_ACC_XYZ],  									DOP_FLOAT32,  		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_GYR_XYZ],  									DOP_FLOAT32,  		3);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_MAG_XYZ],  									DOP_FLOAT32,  		3);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_ACC_GYR_XYZ],  								DOP_FLOAT32,  		6);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_IMU] [PDO_ID_IMU_QUATERNION],								DOP_INT16,  		4);

	/* Gait Task */
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_GAIT] [PDO_ID_GAIT_DEG],  									DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_GAIT] [PDO_ID_GAIT_VEL],  									DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_GAIT] [PDO_ID_GAIT_DEG_INC],  								DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_GAIT] [PDO_ID_GAIT_VEL_INC],  								DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_GAIT] [PDO_ID_GAIT_GYR_Z],  									DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_GAIT] [PDO_ID_GAIT_QUATERNION],								DOP_INT16,  		4);

	/* System Ctrl Task */
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_SYSMNGT_MD] [PDO_ID_SYSTEM_VOLT],								DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_SYSMNGT_MD] [PDO_ID_SYSTEM_CURR],								DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_SYSMNGT_MD] [PDO_ID_SYSTEM_TEMP],								DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_SYSMNGT_MD] [PDO_ID_SYSTEM_PCTG],								DOP_FLOAT32,  		1);

	/* Ext Dev Ctrl Task */				
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR],  								DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_LP],									DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_DC_LENGTH_REF],						DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_DC_DIRECTION_CMD],  					DOP_UINT8,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_DC_LENGTH_ACT],  						DOP_FLOAT32,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_DC_DIRECTION_ACT],  					DOP_UINT8,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_DC_BUTTON_STATE],  					DOP_UINT8,  		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_NTC_MOTOR_TEMP],						DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_MCU_TEMP],							DOP_UINT32,			1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_MIDDLE_TOE],						DOP_UINT16,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_BIG_TOE],							DOP_UINT16,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_LITTLE_TOE],						DOP_UINT16,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_HEEL],							DOP_UINT16,		1);

	/* Used PDO by 1NE */
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_1A],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_1B],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_2A],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_2B],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_3A],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_3B],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_4A],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PMMG_4B],								DOP_UINT16,			1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_L1],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_L2],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_L3],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_L4],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_R1],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_R2],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_R3],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_R4],								DOP_INT16,			1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_L1],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_L2],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_L3],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_L4],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_R1],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_R2],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_R3],								DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FSR_R4],								DOP_UINT16,			1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_HARDEMO_1],						DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_HARDEMO_2],						DOP_UINT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_HARDEMO_3],						DOP_UINT16,			1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_CONTROL_INPUT_RH],					DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_CONTROL_INPUT_LH],					DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_RAW],								DOP_INT16,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_EMG_PROCESSED],						DOP_UINT16,			1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PVECTOR_REF_RH],						DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_PVECTOR_REF_LH],						DOP_FLOAT32,		1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_CONTROL_MODE],						DOP_UINT8,			1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_ACK_SIGNAL],							DOP_UINT8,			1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FREE_VAR1],							DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_FREE_VAR2],							DOP_FLOAT32,		1);

	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_QUATERNION_W],						DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_QUATERNION_X],						DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_QUATERNION_Y],						DOP_FLOAT32,		1);
	AssemblePDO( (PDOInfo_t*)PDOTable [TASK_ID_EXTDEV] [PDO_ID_EXTDEV_QUATERNION_Z],						DOP_FLOAT32,		1);
	
	/* END */
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

static void AssembleSDO(SDOInfo_t* t_addr, uint8_t t_dataType)
{
	SDOInfo_t t_temp = t_dataType;
	memcpy(t_addr, &t_temp, sizeof(SDOInfo_t));
}

static void AssemblePDO(PDOInfo_t* t_addr, uint8_t t_dataType, uint8_t t_numOfData)
{
	PDOInfo_t t_temp = {t_dataType, t_numOfData};
	memcpy(t_addr, &t_temp, sizeof(PDOInfo_t));
}
