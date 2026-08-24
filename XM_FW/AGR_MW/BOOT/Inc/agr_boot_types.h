/**
 * @file  agr_boot_types.h
 * @author Angel Robotics
 * @brief  AGR_BOOT V2 — Core data types, address defines, enumerations
 * @version 1.0.0
 * @date  2026-03-12
 * @details
 *   Defines AGR_FwInfo_t (64B, Active Slot header), AGR_BootConfig_t (32B,
 *   Bank2 S6), flash address constants, and update-state / target-module enums.
 *   Pure C, no HAL/RTOS dependency — shared between BL and App.
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#ifndef AGR_BOOT_CORE_INC_AGR_BOOT_TYPES_H_
#define AGR_BOOT_CORE_INC_AGR_BOOT_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* *---------------------------------------------------------------------
 * FLASH MEMORY LAYOUT CONSTANTS
 *---------------------------------------------------------------------*/

/** @brief Bootloader occupies Bank1 Sector 0-1 (256 KB). */
#define AGR_BOOT_BL_BASE_ADDR          (0x08000000U)
#define AGR_BOOT_BL_SIZE               (0x00040000U)  /* 256 KB */

/** @brief Active Slot: Bank1 Sector 2-7 (768 KB). fw_info at base, App at +0x400. */
#define AGR_BOOT_ACTIVE_BASE_ADDR      (0x08040000U)
#define AGR_BOOT_FW_INFO_SIZE          (0x00000400U)  /* 1 KB header */
#define AGR_BOOT_APP_VTOR_ADDR         (0x08040400U)  /* Active + 1 KB */

/** @brief Backup Slot: Bank2 Sector 0-5 (768 KB). Byte-for-byte copy of Active. */
#define AGR_BOOT_BACKUP_BASE_ADDR      (0x08100000U)

/** @brief Boot Config: Bank2 Sector 6 (128 KB). Stores AGR_BootConfig_t. */
#define AGR_BOOT_CONFIG_BASE_ADDR      (0x081C0000U)

/** @brief Reserved: Bank2 Sector 7 (128 KB). App persistent data / calibration. */
#define AGR_BOOT_RESERVED_BASE_ADDR    (0x081E0000U)

/** @brief H7 sector geometry (uniform 128 KB). */
#define AGR_BOOT_SECTOR_SIZE           (0x00020000U)  /* 128 KB */

/** @brief Active/Backup slot = 6 sectors each. */
#define AGR_BOOT_SLOT_SECTOR_COUNT     (6U)
#define AGR_BOOT_SLOT_SIZE             (AGR_BOOT_SECTOR_SIZE * AGR_BOOT_SLOT_SECTOR_COUNT) /* 768 KB */

/** @brief Maximum app binary size = Slot – fw_info header. */
#define AGR_BOOT_MAX_FW_SIZE           (AGR_BOOT_SLOT_SIZE - AGR_BOOT_FW_INFO_SIZE) /* 767 KB */

/** @brief Flash write granularity on H7 (256-bit = 32 bytes). */
#define AGR_BOOT_FLASH_WRITE_SIZE      (32U)

/** @brief Signature string embedded in AGR_FwInfo_t. */
#define AGR_BOOT_FW_SIGNATURE          {'A','G','R','B','O','O','T',0x01}
#define AGR_BOOT_FW_SIGNATURE_SIZE     (8U)

/** @brief Magic value for valid AGR_BootConfig_t. */
#define AGR_BOOT_CONFIG_MAGIC          (0xB007CF96U)

/** @brief RTC Backup Register magic for App→BL mode transition (Phase 3).
 *  App writes this to RTC->BKP0R + NVIC_SystemReset → BL detects and enters FTP. */
#define AGR_BOOT_RTC_BKP_MAGIC        (0xB00710ADU)
#define AGR_BOOT_RTC_BKP_REG_INDEX    (0U)  /**< Uses RTC->BKP0R */

/** @brief BL firmware version — recorded in AGR_BootConfig_t.bl_ver fields.
 *  BL writes these into Boot Config on every config update.
 *  App reads them to report BL version in QUERY_INFO response. */
#define AGR_BOOT_BL_VER_MAJOR          (1U)
#define AGR_BOOT_BL_VER_MINOR          (0U)
#define AGR_BOOT_BL_VER_PATCH          (0U)

/** @brief App mode indicator for QUERY_INFO ftp_state field.
 *  When App responds to QUERY_INFO, ftp_state=0xFF means "App is running, not in BL". */
#define AGR_FTP_STATE_APP_MODE         (0xFFU)

/* *---------------------------------------------------------------------
 * TARGET MODULE ENUMERATION
 *---------------------------------------------------------------------*/

typedef enum {
    AGR_BOOT_MODULE_XM  = 0x10,
    AGR_BOOT_MODULE_CM  = 0x20,
    AGR_BOOT_MODULE_MD  = 0x30,
    AGR_BOOT_MODULE_SM  = 0x40,
    AGR_BOOT_MODULE_UNKNOWN = 0xFF,
} AGR_Boot_TargetModule_t;

/* *---------------------------------------------------------------------
 * UPDATE STATE ENUMERATION
 *---------------------------------------------------------------------*/

typedef enum {
    AGR_BOOT_STATE_NONE             = 0x00, /**< No pending update */
    AGR_BOOT_STATE_BACKUP_DONE      = 0x01, /**< Active -> Backup copy complete */
    AGR_BOOT_STATE_ERASED           = 0x02, /**< Active slot erased, ready for write */
    AGR_BOOT_STATE_FW_RECEIVED      = 0x03, /**< New FW written + CRC verified */
    AGR_BOOT_STATE_PENDING_CONFIRM  = 0x04, /**< Boot Config committed, App must confirm */
} AGR_Boot_UpdateState_t;

/* *---------------------------------------------------------------------
 * HW REVISION CONSTANTS
 *---------------------------------------------------------------------*/

#define AGR_BOOT_HWREV_UNKNOWN  (0x00U)
#define AGR_BOOT_HWREV_REV11   (0x11U)  /**< PCB Rev 1.1 — HWREV[2:0] = 001 */
#define AGR_BOOT_HWREV_REV20   (0x20U)  /**< PCB Rev 2.0 — HWREV[2:0] = 010 */

/* *---------------------------------------------------------------------
 * AGR_FwInfo_t — Firmware Information Header (64 bytes)
 *
 * Stored at Active Slot base (0x08040000). First 1 KB of packaged.bin
 * (64B useful + 960B padding = 1 KB).
 *---------------------------------------------------------------------*/

typedef struct __attribute__((packed)) {
    uint8_t  signature[AGR_BOOT_FW_SIGNATURE_SIZE]; /**< Must be AGR_BOOT_FW_SIGNATURE */
    uint32_t fw_size;            /**< App binary size in bytes (excludes this header) */
    uint32_t fw_crc32;           /**< CRC-32 of app binary */
    uint32_t fw_start_offset;    /**< App entry offset from slot base (0x400 = 1 KB) */
    uint8_t  ver_major;          /**< Semantic version: major */
    uint8_t  ver_minor;          /**< Semantic version: minor */
    uint8_t  ver_patch;          /**< Semantic version: patch */
    uint8_t  ver_debug;          /**< Debug/build number */
    uint32_t build_timestamp;    /**< Build time (Unix epoch) */
    uint8_t  target_module;      /**< AGR_Boot_TargetModule_t */
    uint8_t  hw_rev_min;         /**< Minimum supported hw_rev (inclusive) */
    uint8_t  hw_rev_max;         /**< Maximum supported hw_rev (inclusive) */
    uint8_t  flags;              /**< bit0: encrypted, bit1: signed */
    uint8_t  reserved[32];       /**< Future expansion (zeroed) */
} AGR_FwInfo_t;

_Static_assert(sizeof(AGR_FwInfo_t) == 64, "AGR_FwInfo_t must be 64 bytes");

/* *---------------------------------------------------------------------
 * AGR_BootConfig_t — Boot Configuration (32 bytes)
 *
 * Stored at Bank2 Sector 6 (0x081C0000). Tracks update state, boot
 * counter for rollback, and detected hw_rev.
 *---------------------------------------------------------------------*/

typedef struct __attribute__((packed)) {
    uint32_t magic;              /**< Must be AGR_BOOT_CONFIG_MAGIC (0xB007CF96) */
    uint8_t  update_state;       /**< AGR_Boot_UpdateState_t */
    uint8_t  boot_count;         /**< Incremented each boot, reset by ConfirmBoot */
    uint8_t  max_boot_attempts;  /**< Default 3 — rollback threshold */
    uint8_t  backup_valid;       /**< 1 if backup slot has valid FW copy */
    uint32_t active_crc;         /**< CRC-32 of current active FW binary */
    uint32_t backup_crc;         /**< CRC-32 of backup FW binary */
    uint32_t active_version;     /**< Packed (major<<24 | minor<<16 | patch<<8 | debug) */
    uint32_t backup_version;     /**< Packed backup FW version */
    uint8_t  hw_rev;             /**< Detected hw_rev from HWREV GPIO */
    uint8_t  bl_ver_major;       /**< BL firmware version major (0 if written by old BL) */
    uint8_t  bl_ver_minor;       /**< BL firmware version minor */
    uint8_t  bl_ver_patch;       /**< BL firmware version patch */
    uint32_t config_crc;         /**< CRC-32 of bytes 0..27 (everything before this field) */
} AGR_BootConfig_t;

_Static_assert(sizeof(AGR_BootConfig_t) == 32, "AGR_BootConfig_t must be 32 bytes");

/* *---------------------------------------------------------------------
 * UTILITY MACROS
 *---------------------------------------------------------------------*/

/** @brief Pack version fields into a single uint32_t. */
#define AGR_BOOT_PACK_VERSION(maj, min, pat, dbg) \
    ((uint32_t)(((uint32_t)(maj) << 24) | ((uint32_t)(min) << 16) | \
                ((uint32_t)(pat) << 8) | (uint32_t)(dbg)))

/** @brief Check if fw_info signature matches expected AGR_BOOT_FW_SIGNATURE. */
static inline bool AGR_Boot_IsSignatureValid(const AGR_FwInfo_t* info)
{
    const uint8_t expected[] = AGR_BOOT_FW_SIGNATURE;
    for (uint8_t i = 0; i < AGR_BOOT_FW_SIGNATURE_SIZE; i++) {
        if (info->signature[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

/** @brief Check hw_rev compatibility: hw_rev_min <= device_rev <= hw_rev_max. */
static inline bool AGR_Boot_IsHwRevCompatible(const AGR_FwInfo_t* info, uint8_t device_hw_rev)
{
    return (device_hw_rev >= info->hw_rev_min) && (device_hw_rev <= info->hw_rev_max);
}

#ifdef __cplusplus
}
#endif

#endif /* AGR_BOOT_CORE_INC_AGR_BOOT_TYPES_H_ */
