

#include "motor_controller.h"

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




/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */


void Init_PID(PIDObject* t_PID_obj, float t_Kp, float t_Ki, float t_Kd)
{
	memset(t_PID_obj, 0, sizeof(PIDObject));

	t_PID_obj->Kp = t_Kp;
	t_PID_obj->Ki = t_Ki;
	t_PID_obj->Kd = t_Kd;
}

void Run_PID_Control(PIDObject* t_PID_obj, float t_ref, float t_actual, float t_period)
{
	float t_err;

	t_PID_obj->ref = t_ref;
	t_PID_obj->act = t_actual;

	t_err = t_ref - t_actual;

	t_PID_obj->err_sum += t_err*t_period;
	t_PID_obj->err_diff_raw = (t_err - t_PID_obj->err)/t_period;

	t_PID_obj->err = t_err;

	t_PID_obj->err_diff = 1.561*t_PID_obj->err_diff_f - 0.6414*t_PID_obj->err_diff_ff   \
			             + 0.02008*t_PID_obj->err_diff_raw + 0.04017*t_PID_obj->err_diff_raw_f + 0.02008*t_PID_obj->err_diff_raw_ff;

	t_PID_obj->err_diff_ff     = t_PID_obj->err_diff_f;
	t_PID_obj->err_diff_f      = t_PID_obj->err_diff;
	t_PID_obj->err_diff_raw_ff = t_PID_obj->err_diff_raw_f;
	t_PID_obj->err_diff_raw_f  = t_PID_obj->err_diff_raw;

	t_PID_obj->control_input = t_PID_obj->Kp * t_PID_obj->err 			\
								+ t_PID_obj->Ki * t_PID_obj->err_sum 	\
								+ t_PID_obj->Kd * t_PID_obj->err_diff;
}

void Run_Impedance_Controller(PIDObject* t_PID_obj, float t_ref, float t_actual, float t_period)
{
	float t_err;

	t_PID_obj->ref = t_ref;
	t_PID_obj->act = t_actual;

	t_err = t_ref - t_actual;

	t_PID_obj->err_sum += t_err*t_period;
	t_PID_obj->err_diff_raw = (t_err - t_PID_obj->err)/t_period;

	t_PID_obj->err = t_err;

	t_PID_obj->err_diff = 1.561*t_PID_obj->err_diff_f - 0.6414*t_PID_obj->err_diff_ff   \
			             + 0.02008*t_PID_obj->err_diff_raw + 0.04017*t_PID_obj->err_diff_raw_f + 0.02008*t_PID_obj->err_diff_raw_ff;


	t_PID_obj->err_diff_ff     = t_PID_obj->err_diff_f;
	t_PID_obj->err_diff_f      = t_PID_obj->err_diff;
	t_PID_obj->err_diff_raw_ff = t_PID_obj->err_diff_raw_f;
	t_PID_obj->err_diff_raw_f  = t_PID_obj->err_diff_raw;

	t_PID_obj->control_input = t_PID_obj->Kp * t_PID_obj->err 			\
								+ t_PID_obj->Ki * t_PID_obj->err_sum 	\
								+ t_PID_obj->Kd * t_PID_obj->err_diff;
}

float Run_Feedback_Control(float t_fb_gain, float t_fb_value)
{
	return t_fb_gain*t_fb_value;
}

float Cal_2Dim_Determinant(float t_mat[2][2])
{
	return (t_mat[0][0]*t_mat[1][1] - t_mat[0][1]*t_mat[1][0]);
}

void Cal_2Dim_Multiplication(float t_mat_left[2][2], float t_mat_right[2][2], float t_result[2][2])
{
    for(int i = 0; i < 2; ++i)
    {
        for(int j = 0; j < 2; ++j)
        {
            for(int z = 0; z < 2; ++z)
            {
            	t_result[i][j] += t_mat_left[i][z] * t_mat_right[z][j];
            }
        }
    }
}

void Cal_2Dim_Addition(float t_mat_left[2][2], float t_mat_right[2][2], float t_result[2][2])
{
	t_result[0][0] = t_mat_left[0][0] + t_mat_right[0][0];
	t_result[0][1] = t_mat_left[0][1] + t_mat_right[0][1];
	t_result[1][0] = t_mat_left[1][0] + t_mat_right[1][0];
	t_result[1][1] = t_mat_left[1][1] + t_mat_right[1][1];
}

void Cal_2Dim_Subtraction(float t_mat_left[2][2], float t_mat_right[2][2], float t_result[2][2])
{
	t_result[0][0] = t_mat_left[0][0] - t_mat_right[0][0];
	t_result[0][1] = t_mat_left[0][1] - t_mat_right[0][1];
	t_result[1][0] = t_mat_left[1][0] - t_mat_right[1][0];
	t_result[1][1] = t_mat_left[1][1] - t_mat_right[1][1];
}

void Cal_2Dim_Inverse(float t_mat[2][2], float t_result[2][2])
{
	static float t_det;

	t_det = Cal_2Dim_Determinant(t_mat);

	t_result[0][0] = t_mat[1][1]/t_det;
	t_result[0][1] = -t_mat[0][1]/t_det;
	t_result[1][0] = -t_mat[1][0]/t_det;
	t_result[1][1] = t_mat[0][0]/t_det;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

