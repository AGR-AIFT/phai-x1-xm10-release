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
 *   are compile-time placeholders (0) and are filled AFTER linking:
 *     - CubeIDE post-build `tools/build/patch_fw_info.py` patches fw_size and
 *       fw_crc32 into the ELF (.fw_header) and regenerates .bin/.hex, so a
 *       CubeIDE build flashed over SWD boots through the BL.
 *     - `tools/build/fw_packager.py` writes fw_size/fw_crc32/build_timestamp
 *       into the FTP upload package (_app.bin).
 *   The BL rejects fw_size == 0 even on a normal boot where it skips the CRC
 *   (AGR_Boot_Core_ValidateActive -> ERR_NO_FW -> FTP wait). A plain CMake CLI
 *   build has no post-build patch, so its ELF/bin is NOT bootable through the
 *   BL — compile check only (harness A30). Comment corrected 2026-10-07: the
 *   earlier text claimed the BL accepts any signature for SWD debug.
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
 * Impact: SWD debug (CubeIDE post-build patch) and FTP upload both produce a
 *         valid App image. A CMake CLI build does not (fw_size stays 0).
 */
const AGR_FwInfo_t __attribute__((section(".fw_header"), used)) g_fw_info = {
    .signature       = AGR_BOOT_FW_SIGNATURE,
    .fw_size         = 0,       /* Placeholder — patch_fw_info.py (CubeIDE ELF) / fw_packager.py (FTP) fill */
    .fw_crc32        = 0,       /* Placeholder — patch_fw_info.py (CubeIDE ELF) / fw_packager.py (FTP) fill */
    .fw_start_offset = AGR_BOOT_FW_INFO_SIZE,  /* 0x400 = 1KB */
    .ver_major       = FW_VER_MAJOR,
    .ver_minor       = FW_VER_MINOR,
    .ver_patch       = FW_VER_PATCH,
    .ver_debug       = FW_VER_DEBUG,
    .build_timestamp = 0,       /* Placeholder — fw_packager.py fills (FTP); CubeIDE ELF keeps 0 */
    .target_module   = (uint8_t)AGR_BOOT_MODULE_XM,
    .hw_rev_min      = AGR_BOOT_HWREV_REV11,
    .hw_rev_max      = AGR_BOOT_HWREV_REV20,
    .flags           = 0,
    .reserved        = {0},
};