

#ifndef SIGNAL_PROCESSING_H_
#define SIGNAL_PROCESSING_H_

#include <math.h>
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

typedef struct _FirstOrderFilter {
	/* filter law: Y(k) = w_filt_f * Y(k-1) + w_raw * U(k) + w_raw_f * U(k-1) */
	/* U(k); k'th measurement */
	/* Y(k): k'th filtered value */
	float w_filt_f;
	float w_raw;
	float w_raw_f;
	float y;
	float u;
	uint8_t on_off;
} FirstOrderFilter;

typedef struct _SecondOrderFilter {
	/* filter law: Y(k) = w_filt_f * Y(k-1) + w_filt_ff * Y(k-2) + w_raw * U(k) + w_raw_f * U(k-1) */
	/* U(k); k'th measurement */
	/* Y(k): k'th filtered value */
	float w_filt_f;
	float w_filt_ff;
	float w_raw;
	float w_raw_f;
	float y;
	float y_f;
	float u;
	uint8_t on_off;
} SecondOrderFilter;


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

void Init_1st_Order_LPF(FirstOrderFilter *t_filter, float t_cutoff, float t_Ts);
void Init_1st_Order_HPF(FirstOrderFilter *t_filter, float t_cutoff, float t_Ts);
void Init_2nd_Order_LPF(SecondOrderFilter *t_filter, float t_cutoff, float t_Ts);

void Run_1st_Order_LPF(FirstOrderFilter *t_filter, float t_u);
void Run_1st_Order_HPF(FirstOrderFilter *t_filter, float t_u);
void Run_2nd_Order_LPF(SecondOrderFilter *t_filter, float t_u);


#endif /* SIGNAL_PROCESSING_H_ */
