/**
 * @file  agr_boot_core.h
 * @author Angel Robotics
 * @brief  AGR_BOOT V2 — Common App-side boot API (confirm + request update)
 * @version 2.1.0
 * @date  2026-03-24
 * @details
 *   Common boot infrastructure used by ALL AGR modules:
 *   - AGR_Boot_ConfirmBoot()    : Reset rollback counter after successful boot
 *   - AGR_Boot_RequestUpdate()  : Write RTC BKP magic + NVIC_SystemReset
 *
 *   Module-specific boot triggers (CDC-FTP, UART-FTP, CAN-FD boot, etc.)
 *   live in each module's System/Boot/ directory.
 *
 *   Requires AGR_MW_BOOT_ENABLE in agr_mw_conf.h.
 *   Dependencies: STM32H7 HAL (flash, RTC BKP)
 *
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#ifndef AGR_MW_BOOT_INC_AGR_BOOT_CORE_H_
#define AGR_MW_BOOT_INC_AGR_BOOT_CORE_H_

#include "agr_mw_conf.h"

#if defined(AGR_MW_BOOT_ENABLE)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Confirm successful boot to the bootloader.
 *
 * Reads AGR_BootConfig_t from Boot Config sector (Bank2 S6, 0x081C0000).
 * If update_state == PENDING_CONFIRM, resets boot_count to 0 and clears
 * the update state. This prevents the bootloader from rolling back on
 * the next reboot.
 *
 * Safe to call without bootloader: returns 0 (no-op) if Boot Config
 * magic/CRC is invalid.
 *
 * @warning MUST be called before RTOS scheduler start or when no ISR/task
 *          can access Bank2 flash (0x08100000-0x081FFFFF). Flash erase/program
 *          on Bank2 stalls any concurrent Bank2 bus access. The HAL flash
 *          functions do NOT disable interrupts internally.
 *
 * @return 0 on success or no-op, negative on flash write error
 */
int32_t AGR_Boot_ConfirmBoot(void);

/**
 * @brief Request firmware update — transition to bootloader FTP mode.
 *
 * Writes AGR_BOOT_RTC_BKP_MAGIC to RTC Backup Register 0, then triggers
 * NVIC_SystemReset(). On reboot, the bootloader detects the magic and
 * enters FTP wait mode for firmware upload.
 *
 * @warning This function NEVER RETURNS. The device will reset immediately.
 * @note Safe from any context (Task, main loop). Does not require RTOS.
 */
void AGR_Boot_RequestUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* AGR_MW_BOOT_ENABLE */

#endif /* AGR_MW_BOOT_INC_AGR_BOOT_CORE_H_ */