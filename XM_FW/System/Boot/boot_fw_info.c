/**
 * @file  boot_fw_info.c
 * @author Angel Robotics
 * @brief  XM Boot FW Info — Embedded firmware header for SWD flash compatibility
 * @version 1.0.0
 * @date  2026-03-24
 * @details
 *   Places AGR_FwInfo_t at 0x08040000 (.fw_header section) so that SWD
 *   flashing includes the header in the same erase/write cycle.
 *   BL reads this header to validate and jump to the App.
 *
 *   Without this, SWD erase destroys the FwInfo (written previously by
 *   FTP upload), leaving the BL unable to find a valid App.
 *
 *   Fields that require runtime values (fw_size, fw_crc32, build_timestamp)
 *   use compile-time placeholders. fw_packager.py overwrites these when
 *   creating a packaged binary for FTP upload. For SWD debug, the BL
 *   accepts any valid signature — fw_size/crc are only checked during
 *   FTP update flow.
 *
 * @note  Module-specific: target_module = XM, hw_rev = Rev1.1~Rev2.0
 *
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#include "agr_boot_types.h"
#include "version.h"

/**
 * @brief Embedded FW Info header — placed at 0x08040000 by linker (.fw_header section).
 *
 * Why:  SWD flash erases the entire sector (0x08040000-0x0805FFFF), destroying
 *       any previously written FwInfo. Embedding it in the ELF ensures BL always
 *       finds a valid signature after SWD flash.
 * What: const struct in .fw_header section -> linker places at FW_HEADER region.
 * Impact: SWD debug and FTP upload both produce a valid App image.
 */
const AGR_FwInfo_t __attribute__((section(".fw_header"), used)) g_fw_info = {
    .signature       = AGR_BOOT_FW_SIGNATURE,
    .fw_size         = 0,       /* Placeholder — fw_packager.py fills for FTP upload */
    .fw_crc32        = 0,       /* Placeholder — fw_packager.py fills for FTP upload */
    .fw_start_offset = AGR_BOOT_FW_INFO_SIZE,  /* 0x400 = 1KB */
    .ver_major       = FW_VER_MAJOR,
    .ver_minor       = FW_VER_MINOR,
    .ver_patch       = FW_VER_PATCH,
    .ver_debug       = FW_VER_DEBUG,
    .build_timestamp = 0,       /* Placeholder — fw_packager.py fills for FTP upload */
    .target_module   = (uint8_t)AGR_BOOT_MODULE_XM,
    .hw_rev_min      = AGR_BOOT_HWREV_REV11,
    .hw_rev_max      = AGR_BOOT_HWREV_REV20,
    .flags           = 0,
    .reserved        = {0},
};