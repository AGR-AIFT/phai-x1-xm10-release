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

/** @brief BL → App 리셋 원인 전달 (BL v1.1.0+).
 *  BL 은 진입 직후 RCC->RSR 원본을 RTC->BKP4R 에 보관한다. BL 의 JumpToApp 이
 *  호출하는 HAL_RCC_DeInit() 이 RSR 을 지우므로(RMVF), App 은 RSR 대신 이 값을
 *  읽어야 직전 리셋 원인을 알 수 있다. 0 = 미기록(구 BL 또는 백업 도메인 리셋).
 *  비트 배치는 RCC_RSR 그대로: 16 RMVF 17 CPURST 19 D1RST 20 D2RST 21 BORRST
 *  22 PINRST 23 PORRST 24 SFTRST 26 IWDG1RST 28 WWDG1RST 30 LPWRRST. */
#define AGR_BOOT_RSR_BKP_REG_INDEX    (4U)  /**< Uses RTC->BKP4R */

/** @brief 연속 부팅 시도 가드 (BL v1.1.0+) — RTC->BKP5R = magic(31:8) | count(7:0).
 *  BL: magic 이 있을 때만 진입마다 count++ 하고, count >= AGR_BOOT_ATTEMPT_MAX 면
 *      App 으로 점프하지 않고 FTP 대기(복구 가능 상태)에 머문다.
 *      Active FW 가 바뀌면(FTP COMMIT 완료, 롤백 성공) BKP5R 을 0 으로 지워 해제한다 —
 *      무장은 지금 올라간 App 의 속성이다(가드를 모르는 구 App 으로 복귀해도 안전).
 *  App: ① ConfirmBoot 시점에 미무장(magic 불일치)이면 (magic | 0) 을 써서 무장한다.
 *         이미 무장돼 있으면 건드리지 않는다 — 매 부팅 0 으로 쓰면 가드가 무력해진다.
 *       ② 안정 동작 확인 시점(권장: 스케줄러 시작 후 5 s 이상)에 (magic | 0) 으로
 *         count 를 초기화한다.
 *  magic 이 없으면(구 App, 또는 백업 도메인 리셋) BL 동작은 바뀌지 않는다. */
#define AGR_BOOT_ATTEMPT_BKP_REG_INDEX (5U)  /**< Uses RTC->BKP5R */
#define AGR_BOOT_ATTEMPT_MAGIC         (0xB007A000U)
#define AGR_BOOT_ATTEMPT_MAGIC_MASK    (0xFFFFFF00U)
#define AGR_BOOT_ATTEMPT_COUNT_MASK    (0x000000FFU)
#define AGR_BOOT_ATTEMPT_MAX           (10U)

/** @brief BL 진입 카운터 + 단계 마커 (BL v1.1.0+, 순수 계측 — 동작에 영향 없음).
 *  RTC->BKP6R = magic(31:16) 0xB1E7 | stage(15:12) | entry_count(11:0, 4095 포화).
 *  BL 진입마다 count++ 와 stage=ENTRY, 이후 단계를 지날 때마다 stage 만 갱신한다.
 *  재현이 어려운 리셋 루프 중 Halt 해서 읽으면 "리셋이 BL 의 어느 단계에서 났는가"가
 *  남는다: JUMP(0x7) 이면 BL 은 일을 끝내고 App 으로 넘어간 뒤 리셋된 것, 그 전 값이면
 *  BL 안에서 리셋된 것. count 가 늘지 않고 1 에 머물면 매 사이클 백업 도메인이 지워지는
 *  것(= 전원이 완전히 떨어짐). 0 = 미기록(구 BL 또는 백업 도메인 리셋). */
#define AGR_BOOT_DIAG_BKP_REG_INDEX    (6U)  /**< Uses RTC->BKP6R */
#define AGR_BOOT_DIAG_MAGIC            (0xB1E70000U)
#define AGR_BOOT_DIAG_MAGIC_MASK       (0xFFFF0000U)
#define AGR_BOOT_DIAG_STAGE_SHIFT      (12U)
#define AGR_BOOT_DIAG_STAGE_MASK       (0x0000F000U)
#define AGR_BOOT_DIAG_COUNT_MASK       (0x00000FFFU)

typedef enum {
    AGR_BOOT_STAGE_ENTRY        = 0x0, /**< Boot_Main 진입 (count++) */
    AGR_BOOT_STAGE_FLASH_INIT   = 0x1, /**< Flash 콜백 등록 완료 */
    AGR_BOOT_STAGE_CORE_RUN     = 0x2, /**< Core_Run 진입 */
    AGR_BOOT_STAGE_CFG_READ     = 0x3, /**< BootConfig 읽기(또는 재생성) 완료 */
    AGR_BOOT_STAGE_CFG_WRITE    = 0x4, /**< BootConfig erase+program 시작 — 여기서 멈추면 소거 중 리셋 */
    AGR_BOOT_STAGE_CFG_WRITE_OK = 0x5, /**< BootConfig 기록 완료 */
    AGR_BOOT_STAGE_VALIDATED    = 0x6, /**< Active FW 검증 통과 */
    AGR_BOOT_STAGE_JUMP         = 0x7, /**< JumpToApp 직전 — 이후 리셋은 App 쪽(또는 App 기동 중 전원) */
    AGR_BOOT_STAGE_FTP_WAIT     = 0x8, /**< FTP 대기 진입 */
    AGR_BOOT_STAGE_FTP_DONE     = 0x9, /**< FTP 완료 → NVIC_SystemReset 직전 */
    AGR_BOOT_STAGE_HOLD         = 0xA, /**< 부팅 시도 가드로 정지 */
    AGR_BOOT_STAGE_ERROR        = 0xB, /**< 오류 LED 정지 */
    AGR_BOOT_STAGE_T2_RECOVER   = 0xC, /**< T2 BootConfig 섹터 소거 → 리셋 직전 */
    AGR_BOOT_STAGE_ROLLBACK     = 0xD, /**< 롤백(Backup→Active) 진행 중 */
    /* 0xE/0xF 는 App 이 기록한다(agr_boot_core.c App 측 AGR_Boot_MarkAppStage) — BL 은 쓰지 않는다.
     * 리셋 루프 중 이 값이면 App 은 main 에 도달했다는 뜻이고, JUMP(0x7)에 머물면 App pre-main 사망. */
    AGR_BOOT_STAGE_APP_MAIN     = 0xE, /**< App main() 진입 직후 (리셋 원인 읽은 뒤) */
    AGR_BOOT_STAGE_APP_STABLE   = 0xF, /**< App 안정 도달 (1 kHz 루프 5 s 생존, 가드 count 0 으로 되돌림) */
} AGR_Boot_Stage_t;

/** @brief 누적 리셋 원인 (BL v1.1.0+): RTC->BKP7R |= RCC->RSR, BL 진입마다 OR.
 *  BKP4R 이 "마지막 한 번"이라면 이것은 "백업 도메인이 유지된 동안 본 모든 원인".
 *  루프 중 SFT 와 POR 이 섞였는지 한 번에 보인다. 0 = 미기록. */
#define AGR_BOOT_RSR_STICKY_BKP_REG_INDEX (7U)  /**< Uses RTC->BKP7R */

/** @brief BL firmware version — recorded in AGR_BootConfig_t.bl_ver fields.
 *  BL writes these into Boot Config when the config content changes.
 *  App reads them to report BL version in QUERY_INFO response.
 *  1.1.0 (2026-10-06): RSR→BKP4R 보관, BKP5R 부팅 시도 가드, BootConfig 변경 시에만 기록. */
#define AGR_BOOT_BL_VER_MAJOR          (1U)
#define AGR_BOOT_BL_VER_MINOR          (1U)
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
