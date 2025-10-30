/*
 * algorithm_ctrl.c
 *
 *  Created on: Mar 18, 2024
 *      Author: INVINCIBLE_1NE
 */

#include "algorithm_ctrl.h"


TaskObj_t algorithmCtrlTask;
TotalData_t totalDataObj;

//extern Exppack_Data_t ExpPackDataObj;
//extern StudentsData_t StudentsDataObj;

/* ------------------ For Code Time Check ----------------- */
static uint32_t STUDENTcodeStartTick = 0;
static uint32_t STUDENTcodeEndTick = 0;
static uint32_t algorithmCtrlLoopCnt;
static float algorithmCtrlTimeElap;
/* -------------------------------------------------------- */


/* -------------------- STATE FUNCTION -------------------- */
static void StateOff_Run(void);

static void StateStandby_Run(void);

static void StateEnable_Ent(void);
static void StateEnable_Run(void);
static void StateEnable_Ext(void);

static void StateError_Run(void);
/* -------------------------------------------------------- */


/* ------------------- Default Variables ------------------ */
uint8_t motionMap_selection = 99;
uint8_t startPvector_decoding = 0;
/* -------------------------------------------------------- */

/* test */
float Kp = 0.067;
float Kd = 0.01;
//////////


/*---------- (1) START of STUDENT CODE (Declare the functions to use) -----------*/




/*---------- (1) END of STUDENT CODE (Declare the functions to use) -------------*/





DOP_COMMON_SDO_CB(algorithmCtrlTask)

void InitAlgorithmCtrl(void)
{
    InitTask(&algorithmCtrlTask);

	/* State Definition */
	TASK_CREATE_STATE(&algorithmCtrlTask, TASK_STATE_OFF,      NULL,				StateOff_Run,       NULL,         		 true);
	TASK_CREATE_STATE(&algorithmCtrlTask, TASK_STATE_STANDBY,  NULL,				StateStandby_Run,	NULL,         		 false);
	TASK_CREATE_STATE(&algorithmCtrlTask, TASK_STATE_ENABLE,   StateEnable_Ent,		StateEnable_Run, 	StateEnable_Ext,	 false);
	TASK_CREATE_STATE(&algorithmCtrlTask, TASK_STATE_ERROR,    NULL,				StateError_Run,    	NULL,				 false);

	/* Routine Definition */

	/* DOD Definition */
	// DOD
	DOP_CreateDOD(TASK_ID_STUDENTS);

	// PDO
	/* For PDO setting */

	// SDO
	DOP_COMMON_SDO_CREATE(TASK_ID_STUDENTS)


	/* Timer Callback Allocation */
	if (IOIF_StartTimIT(IOIF_TIM3) > 0) {
		//TODO: ERROR PROCESS
	}
	IOIF_SetTimCB(IOIF_TIM3, IOIF_TIM_PERIOD_ELAPSED_CALLBACK, RunAlgorithmCtrl, NULL);
}

void RunAlgorithmCtrl(void* params)
{
	/* Loop Start Time Check */
	STUDENTcodeStartTick = DWT->CYCCNT;

	/* Run Device */
	RunTask(&algorithmCtrlTask);

	/* Elapsed Time Check */
	STUDENTcodeEndTick = DWT->CYCCNT;
	if (STUDENTcodeEndTick < STUDENTcodeStartTick) {
		algorithmCtrlTimeElap = ((4294967295 - STUDENTcodeStartTick) + STUDENTcodeEndTick) / 480;	// in microsecond (Roll-over)
	}
	else {
		algorithmCtrlTimeElap = (DWT->CYCCNT - STUDENTcodeStartTick) / 480;							// in microsecond
	}
}

/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

static void StateOff_Run(void)
{
	StateTransition(&algorithmCtrlTask.stateMachine, TASK_STATE_STANDBY);
}

static void StateStandby_Run(void)
{
	StateTransition(&algorithmCtrlTask.stateMachine, TASK_STATE_ENABLE);
}

static void StateEnable_Ent(void)
{
	EntRoutines(&algorithmCtrlTask.routine);

	algorithmCtrlLoopCnt = 0;
}

static void StateEnable_Run(void)
{
	RunRoutines(&algorithmCtrlTask.routine);

	/*---------------------------- Data gathering (DO NOT CHANGE THIS) ----------------------------*/
	totalDataObj.theta_RH_act = StudentsDataObj.theta_RH;
	totalDataObj.theta_LH_act = StudentsDataObj.theta_LH;
	totalDataObj.accX_RH = StudentsDataObj.accX_RH;
	totalDataObj.accX_LH = StudentsDataObj.accX_LH;
	totalDataObj.accY_RH = StudentsDataObj.accY_RH;
	totalDataObj.accY_LH = StudentsDataObj.accY_LH;
	totalDataObj.EMG_raw = ExpPackDataObj.emg_data.emg_R2_rawSign[0] / 2048.0f;
	/*---------------------------------------------------------------------------------------------*/





	/*--------------------- (2) START of STUDENT CODE (Write your code in this section) --------------------*/

	/* Process the Raw EMG data */

	// Test code (1NE) //
//	totalDataObj.EMG_processed = totalDataObj.EMG_raw + 1;
	totalDataObj.EMG_processed = ExpPackDataObj.emg_data.emg_R2_LPF[0];



	/* Calculate the control input */

//	totalDataObj.e_RH = (posCtrl_RH.ref*500) - totalDataObj.theta_RH_act;
//	totalDataObj.e_LH = (posCtrl_LH.ref*500) - totalDataObj.theta_LH_act;

//	totalDataObj.e_dot_RH = (totalDataObj.e_RH - totalDataObj.e_prev_RH)/0.001;
//	totalDataObj.e_dot_LH = (totalDataObj.e_LH - totalDataObj.e_prev_LH)/0.001;

//	totalDataObj.e_prev_RH =  totalDataObj.e_RH;
//	totalDataObj.e_prev_LH =  totalDataObj.e_LH;

//	totalDataObj.u_RH = Kp*totalDataObj.e_RH + Kd*totalDataObj.e_dot_RH;
//	totalDataObj.u_LH = Kp*totalDataObj.e_LH + Kd*totalDataObj.e_dot_LH;


//	if (totalDataObj.u_RH > 9) {
//		totalDataObj.u_RH = 9;
//	}
//	else if (totalDataObj.u_RH < -9) {
//		totalDataObj.u_RH = -9;
//	}
//
//	if (totalDataObj.u_LH > 9) {
//		totalDataObj.u_LH = 9;
//	}
//	else if (totalDataObj.u_LH < -9) {
//		totalDataObj.u_LH = -9;
//	}


	// Test code (1NE) //
	if (algorithmCtrlLoopCnt % 2 == 0) {
		totalDataObj.u_RH = totalDataObj.u_RH + 1;
		totalDataObj.u_LH = totalDataObj.u_LH + 2;
		totalDataObj.theta_RH_ref = totalDataObj.theta_RH_ref + 3;
		totalDataObj.theta_LH_ref = totalDataObj.theta_LH_ref + 4;

		totalDataObj.fvector_trigger_1 = totalDataObj.fvector_trigger_1 + 11;
		totalDataObj.fvector_trigger_2 = totalDataObj.fvector_trigger_2 + 22;
		totalDataObj.fvector_trigger_3 = totalDataObj.fvector_trigger_3 + 33;
		totalDataObj.fvector_trigger_4 = totalDataObj.fvector_trigger_4 + 44;
		totalDataObj.fvector_trigger_5 = totalDataObj.fvector_trigger_5 + 55;
	}
	else {
		totalDataObj.u_RH = totalDataObj.u_RH - 1;
		totalDataObj.u_LH = totalDataObj.u_LH - 2;
		totalDataObj.theta_RH_ref = totalDataObj.theta_RH_ref - 3;
		totalDataObj.theta_LH_ref = totalDataObj.theta_LH_ref - 4;

		totalDataObj.fvector_trigger_1 = totalDataObj.fvector_trigger_1 - 11;
		totalDataObj.fvector_trigger_2 = totalDataObj.fvector_trigger_2 - 22;
		totalDataObj.fvector_trigger_3 = totalDataObj.fvector_trigger_3 - 33;
		totalDataObj.fvector_trigger_4 = totalDataObj.fvector_trigger_4 - 44;
		totalDataObj.fvector_trigger_5 = totalDataObj.fvector_trigger_5 - 55;
	}


	/*------------------------------------ (2) END of STUDENT CODE-----------------------------------------*/


	algorithmCtrlLoopCnt++;
}

static void StateEnable_Ext(void)
{
    ExtRoutines(&algorithmCtrlTask.routine);
}

static void StateError_Run(void)
{

}






/*----------- (3) START of STUDENT CODE (Define the functions to use) ------------*/





/*------------ (3) END of STUDENT CODE (Define the functions to use) -------------*/













