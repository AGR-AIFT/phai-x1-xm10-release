/**
 * @file  boot_ftp_trigger.h
 * @author Angel Robotics
 * @brief  XM Boot FTP Trigger — USB CDC-based FTP command handler
 * @version 1.0.0
 * @date  2026-03-24
 * @details
 *   XM-specific boot trigger that scans USB CDC Rx for PhAI FTP commands:
 *   - Boot_FTP_Init()              : Set CDC TX callback for QUERY_INFO response
 *   - Boot_FTP_CheckForEnterBL()   : Parse COBS+PhAI FTP commands (QUERY_INFO + ENTER_BL)
 *
 *   This module handles the transport-specific (USB CDC) part of the boot flow.
 *   Common boot operations (ConfirmBoot, RequestUpdate) are in AGR_MW/BOOT/agr_boot_core.h.
 *
 *   Other modules may implement different triggers:
 *   - CM: UART-FTP trigger
 *   - SM: CAN-FD boot trigger (via master command)
 *
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#ifndef XM_FW_SYSTEM_BOOT_BOOT_FTP_TRIGGER_H_
#define XM_FW_SYSTEM_BOOT_BOOT_FTP_TRIGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief CDC Tx function pointer for sending FTP responses from App.
 * @param[in] data  Complete packet bytes (COBS-encoded + 0x00 delimiter)
 * @param[in] len   Number of bytes to transmit
 * @return 0 on success, non-zero if CDC busy
 */
typedef int32_t (*Boot_FTP_TxFunc_t)(const uint8_t* data, uint32_t len);

/**
 * @brief Initialize XM boot FTP trigger with CDC TX callback.
 *
 * Must be called once at startup (after USB CDC init) to enable
 * QUERY_INFO response capability. Without this call, QUERY_INFO
 * commands are silently ignored (backward compatible).
 *
 * @param[in] tx_func  CDC TX function (e.g., wrapping CDC_Transmit_FS)
 */
void Boot_FTP_Init(Boot_FTP_TxFunc_t tx_func);

/**
 * @brief Scan raw USB CDC Rx data for FTP commands (QUERY_INFO + ENTER_BOOTLOADER).
 *
 * Call this from CDC_Receive_FS() BEFORE normal PhAI protocol processing.
 *
 * Handles two FTP commands:
 *   - QUERY_INFO (0x01): Sends device info response (BL ver, App ver, hw_rev, etc.)
 *                        Requires Boot_FTP_Init() to have been called.
 *   - ENTER_BOOTLOADER (0x02): Calls AGR_Boot_RequestUpdate() and NEVER RETURNS.
 *
 * If no FTP command is detected, returns immediately (zero overhead for normal traffic).
 *
 * @param[in] data  Raw CDC Rx buffer
 * @param[in] len   Number of bytes received
 *
 * @note Handles COBS decoding and CRC16 validation internally.
 */
void Boot_FTP_CheckForEnterBL(const uint8_t* data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* XM_FW_SYSTEM_BOOT_BOOT_FTP_TRIGGER_H_ */
