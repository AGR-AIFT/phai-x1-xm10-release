

#ifndef MOTOR_CONTROLLER_H_
#define MOTOR_CONTROLLER_H_

#include <string.h>
#include <stdint.h>

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

typedef enum _VelocityEstimator_t {
	e_VE_RAW = 0,
	e_VE_LPF_300,
	e_VE_LPF_400,
	e_VE_LPF_500,
	e_VE_MOVING_AVR,
	e_VE_KALMAN,
} VelocityEstimator_t;
typedef uint8_t VelocityEstimator;

typedef struct _KFObject {
   float kf_A;
   float kf_B;
   float kf_C;
   float kf_Q;
   float kf_R;
   float kf_P;
   float kf_K;
   float kf_y;
} KFObject;

typedef struct _VelocityEstObject {
	float pos_save[9];
	KFObject kf_obj;
	float vel_save[2];
	VelocityEstimator type;
	float lpf_a, lpf_b;

	float velocity;

	float leadlag_a;
	float leadlag_b;

	uint8_t masking_size;
} VelocityEstObject;

typedef struct _FFObject {
	float num[4];
	float den[4];
	float in[4];
	float out[4];

	float num_length;
	float den_length;

	float control_input;

	uint8_t diff;
} FFObject;

typedef struct _PIDObject {
	float ref;  // yd(k)
	float ref1; // yd(k+1)
	float ref2;	// yd(k+2)

	float act;
	float Ctrl_BW_Hz;

	float Kp;
	float Ki;
	float Kd;

	float R;		// Input penalty in LQ,   q1=1, q2=0

	float control_input;

	float err;
	float err_sum;
	float err_diff;
	float err_diff_f;
	float err_diff_ff;
	float err_diff_raw;
	float err_diff_raw_f;
	float err_diff_raw_ff;
} PIDObject;

typedef struct _DOBObject {
	uint32_t transition_time; // unit: ms
	uint32_t time_stamp;

	float gq_num[6];
	float gq_den[6];
	float gq_out[6];
	float gq_in[6];
	float q_out[4];
	float q_num[4];
	float q_den[4];
	float q_in[4];
	float wc_Q;    // Q-filter cutoff frequency (unit: rad/s)
	float disturbance;
	float gq_num_length;
	float gq_den_length;
	float q_num_length;
	float q_den_length;
	float saturation;
	float control_input;
	float initial_pos;
} DOBObject;

typedef struct _ImpedanceReductionCtrl {
	float irc_num[6];
	float irc_den[6];
	float irc_input[6];
	float irc_output[6];
	float saturation;
	uint8_t numerator_length;
	uint8_t denominator_length;
} ImpedanceReductionCtrl;


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

void Init_PID(PIDObject* t_PID_obj, float t_Kp, float t_Ki, float t_Kd);
void Run_PID_Control(PIDObject* t_PID_obj, float t_ref, float t_actual, float t_period);
float Run_Feedback_Control(float t_fb_gain, float t_fb_value);

float Cal_2Dim_Determinant(float t_mat[2][2]);
void Cal_2Dim_Multiplication(float t_mat_left[2][2], float t_mat_right[2][2], float t_result[2][2]);
void Cal_2Dim_Addition(float t_mat_left[2][2], float t_mat_right[2][2], float t_result[2][2]);
void Cal_2Dim_Subtraction(float t_mat_left[2][2], float t_mat_right[2][2], float t_result[2][2]);
void Cal_2Dim_Inverse(float t_mat[2][2], float t_result[2][2]);

void Run_Impedance_Controller(PIDObject* t_PID_obj, float t_ref, float t_actual, float t_period);


#endif /* MOTOR_CONTROLLER_H_ */
