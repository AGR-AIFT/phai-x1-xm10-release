/**
 * @file  agr_mw_conf.h  (TEMPLATE — copy to your module's System/Config/)
 * @brief  AGR_MW Configuration — MCU series + module enable/disable
 * @details
 *   Each module repo provides its own agr_mw_conf.h (like ioif_conf.h).
 *   This file is NOT compiled as part of AGR_MW submodule — it lives in
 *   each product's include path (e.g., System/Config/agr_mw_conf.h).
 *
 *   AGR_MW headers include this file to:
 *   1. Auto-detect MCU series (H7/G4) from CubeMX defines
 *   2. Enable/disable middleware modules per product
 *
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#ifndef AGR_MW_CONF_H_
#define AGR_MW_CONF_H_

/* -----------------------------------------------------------------------
 * MCU Series Auto-Detection (from CubeMX-generated defines)
 * ----------------------------------------------------------------------- */
#if defined(STM32H743xx) || defined(STM32H750xx)
    #define AGR_MW_MCU_SERIES_H7    1
#elif defined(STM32G474xx) || defined(STM32G431xx)
    #define AGR_MW_MCU_SERIES_G4    1
#else
    #error "AGR_MW: Unsupported MCU — add detection in agr_mw_conf.h"
#endif

/* -----------------------------------------------------------------------
 * Module Enable/Disable
 * ----------------------------------------------------------------------- */

/* BOOT — App-side bootloader integration (ConfirmBoot, RequestUpdate)
 * H7 only: requires dual-bank flash + RTC BKP
 * Enable for: XM, CM-WH (H7 with bootloader)
 * Disable for: G4 modules (SM-IMU, SM-EMG, SM-FES) */
#if defined(AGR_MW_MCU_SERIES_H7)
    #define AGR_MW_BOOT_ENABLE
#endif

#endif /* AGR_MW_CONF_H_ */