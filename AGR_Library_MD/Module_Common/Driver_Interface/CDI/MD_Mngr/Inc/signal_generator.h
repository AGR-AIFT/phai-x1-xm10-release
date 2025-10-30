

#ifndef SIGNAL_GENERATOR_H_
#define SIGNAL_GENERATOR_H_

#include <stdint.h>
#include <math.h>

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define CHIRP_ARRAY_SIZE				6377
#define COS_SIG_ARRAY_SIZE              4096
#define SIN_SIG_ARRAY_SIZE              4096
#define BW_CHECK_ARRAY_SIZE				6895
#define RGS_INPUT_ARRAY_SIZE            2000


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

extern float chirp_freq[CHIRP_ARRAY_SIZE];
extern float cosine_signal[COS_SIG_ARRAY_SIZE];
extern float sine_signal[SIN_SIG_ARRAY_SIZE];
extern float current_bandwidth[BW_CHECK_ARRAY_SIZE];
extern float rgs_input[RGS_INPUT_ARRAY_SIZE];


/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

float Generate_Sine(float t_A, float t_offset, float t_f, uint32_t t_k, float t_T, int32_t t_phase_lead_index);
float Generate_Rectangle_tanh(float t_amp, float t_offset, float t_freq, uint32_t t_k, float t_T, int32_t t_phase_lead_index);
float Generate_Rectangle(float t_A, float t_offset, float t_f, float t_ratio, uint32_t t_k, float t_T, int8_t t_mode);
float Generate_Stair(float t_A1, float t_A2, uint16_t t_N, float t_dT, uint32_t t_k, float t_T);
float Generate_Ramp(float t_A1, float t_ym, float t_offset, uint32_t t_k, float t_T);


#endif /* SIGNAL_GENERATOR_H_ */
