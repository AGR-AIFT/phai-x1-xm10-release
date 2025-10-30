

#include "signal_processing.h"

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

/* 1st order filter */
void Init_1st_Order_LPF(FirstOrderFilter *t_filter, float t_cutoff, float t_Ts)
{
	/* zero order hold */
	float t_wc = 2 * 3.141592 * t_cutoff;
	float t_x = - t_wc * t_Ts;

	t_filter->w_filt_f = exp(t_x);
	t_filter->w_raw    = 1 - exp(t_x);
}

void Init_1st_Order_HPF(FirstOrderFilter *t_filter, float t_cutoff, float t_Ts)
{
	/* zero order hold */
	float t_wc = 2 * 3.141592 * t_cutoff;
	float t_x = - t_wc * t_Ts;

	t_filter->w_filt_f = exp(t_x);
	t_filter->w_raw    = 1;
	t_filter->w_raw_f  = -1;
}

/* 2nd order filter */
void Init_2nd_Order_LPF(SecondOrderFilter *t_filter, float t_cutoff, float t_Ts)
{
	/* zero order hold */
	float t_wc = 2 * 3.141592 * t_cutoff;
	float t_x = - t_wc * t_Ts;

	t_filter->w_filt_f = 2 * exp(t_x);
	t_filter->w_filt_ff = - exp(2*t_x);
	t_filter->w_raw = 1 - exp(t_x) + t_x * exp(t_x);
	t_filter->w_raw_f = exp(t_x) * (exp(t_x) - t_x - 1);
}


void Run_1st_Order_LPF(FirstOrderFilter *t_filter, float t_u)
{
    float t_y;
    t_y = t_filter->w_filt_f * t_filter->y + t_filter->w_raw * t_u;
    t_filter->y = t_y;
}

void Run_1st_Order_HPF(FirstOrderFilter *t_filter, float t_u)
{
	float t_y;
	t_y = t_filter->w_filt_f * t_filter->y + t_filter->w_raw * t_u + t_filter->w_raw_f * t_filter->u;
	t_filter->y = t_y;
	t_filter->u = t_u;
}

void Run_2nd_Order_LPF(SecondOrderFilter *t_filter, float t_u)
{
	float t_y;

	t_y = t_filter->w_filt_f * t_filter->y + t_filter->w_filt_ff * t_filter->y_f + t_filter->w_raw * t_u + t_filter->w_raw_f * t_filter->u;

	t_filter->y_f = t_filter->y;
	t_filter->y = t_y;
	t_filter->u = t_u;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

