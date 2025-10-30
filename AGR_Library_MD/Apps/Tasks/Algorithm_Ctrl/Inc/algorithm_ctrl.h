/*
 * algorithm_ctrl.h
 *
 *  Created on: Mar 18, 2024
 *      Author: INVINCIBLE_1NE
 */

#ifndef ALGORITHM_CTRL_INC_ALGORITHM_CTRL_H_
#define ALGORITHM_CTRL_INC_ALGORITHM_CTRL_H_

#include "module.h"

#include <stdbool.h>
#include <math.h>

#include "ioif_tim_common.h"
#include "error_dictionary.h"

#include "msg_hdlr.h"
#include "exppack_ctrl.h"
#include "data_object_common.h"
#include "ioif_adc_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef struct _TotalData_t {
	float u_RH;				// control input for RH
	float u_LH;				// control input for LH

	float theta_RH_act;		// Actual position of RH (received [MD->MiniCM->EXTboard])
	float theta_LH_act;		// Actual position of LH (received [MD->MiniCM->EXTboard])
	float theta_RH_ref;		// Reference position of RH
	float theta_LH_ref;		// Reference position of LH

	float accX_RH;			// (received [MD->MiniCM->EXTboard])
	float accX_LH;			// (received [MD->MiniCM->EXTboard])
	float accY_RH;			// (received [MD->MiniCM->EXTboard])
	float accY_LH;			// (received [MD->MiniCM->EXTboard])

	float EMG_raw;			// raw EMG data [-1,1]	(received from EXTboard)
	float EMG_processed;	// processed EMG data

	uint8_t fvector_trigger_1;
	uint8_t fvector_trigger_2;
	uint8_t fvector_trigger_3;
	uint8_t fvector_trigger_4;
	uint8_t fvector_trigger_5;

	float e_RH;
	float e_LH;
	float e_prev_RH;
	float e_prev_LH;
	float e_dot_RH;
	float e_dot_LH;
} TotalData_t;


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */

extern TotalData_t totalDataObj;
extern uint8_t motionMap_selection;
extern uint8_t startPvector_decoding;

/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

void InitAlgorithmCtrl(void);
void RunAlgorithmCtrl(void* params);

#endif /* ALGORITHM_CTRL_INC_ALGORITHM_CTRL_H_ */
