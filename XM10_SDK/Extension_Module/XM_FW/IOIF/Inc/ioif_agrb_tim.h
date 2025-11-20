/**
 ******************************************************************************
 * @file    ioif_agrb_tim.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Oct 30, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_TIM_H_
#define IOIF_INC_IOIF_AGRB_TIM_H_

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_tim.h"
#include "stm32h7xx_hal_tim_ex.h"

#include <stdint.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

uint32_t IOIF_GetTick(void);

#endif /* IOIF_INC_IOIF_AGRB_TIM_H_ */
