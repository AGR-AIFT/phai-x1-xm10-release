/**
 * @file  agr_boot_core.h
 * @author Angel Robotics
 * @brief  AGR_BOOT V2 — Common App-side boot API (confirm + request update + boot diag)
 * @version 2.2.0
 * @date  2026-10-07
 * @details
 *   Common boot infrastructure used by ALL AGR modules:
 *   - AGR_Boot_ConfirmBoot()       : Reset rollback counter after successful boot
 *   - AGR_Boot_RequestUpdate()     : Write RTC BKP magic + NVIC_SystemReset
 *   - AGR_Boot_GetResetCause()     : BL v1.1.0+ 가 BKP4R 에 보관한 리셋 원인 (없으면 RSR)
 *   - AGR_Boot_MarkAppStage()      : BKP6R 단계 마커에 App 단계(0xE main / 0xF stable) 기록
 *   - AGR_Boot_ArmAttemptGuard()   : BL 연속 부팅 시도 가드(BKP5R) 무장 — ConfirmBoot 직후
 *   - AGR_Boot_ClearAttemptCount() : 안정 도달 시 가드 횟수 0 (BKP5R = magic|0)
 *
 *   BKP4R~7R 계약과 동작은 agr_boot_types.h 의 AGR_BOOT_RSR_BKP_REG_INDEX /
 *   AGR_BOOT_ATTEMPT_* / AGR_BOOT_DIAG_* 주석과 부트로더 레포
 *   `docs/plans/PLAN-20261006-bl-bootloop-guard.md` 를 따른다.
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
#include "agr_boot_types.h"   /* AGR_Boot_Stage_t, BKP 계약 상수 */

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

/**
 * @brief 직전 리셋 원인 — 부트로더(v1.1.0+)가 진입 직후 RTC->BKP4R 에 보관한 RSR 원본.
 *
 * 부트로더의 HAL_RCC_DeInit() 이 RSR 을 지우기 때문에, BL 을 거쳐 기동한 App 이 직접 읽는
 * RCC->RSR 은 항상 0 이었다. 이 함수는 BKP6R 단계 마커가 "JumpToApp 직전(0x7)" 이면
 * 부트로더가 방금 돌았다고 보고 BKP4R 을 돌려주고, 아니면(구 BL, 디버거 직접 기동 등)
 * 호출자가 넘긴 현재 RSR 을 그대로 돌려준다.
 *
 * @param rsr_now 호출 시점의 RCC->RSR (fallback).
 * @return 직전 리셋 원인 (RCC_RSR 비트 배치 그대로).
 * @note main() 최상단, HAL_Init 이전에도 안전 (PWR/RCC/RTC 레지스터만 접근).
 */
uint32_t AGR_Boot_GetResetCause(uint32_t rsr_now);

/**
 * @brief BKP6R 단계 마커에 App 단계를 기록한다 — 부팅 횟수(11:0)는 보존, 단계(15:12)만 갱신.
 *
 * 리셋 루프 중 SWD 로 BKP6R 을 읽으면 "App 이 main 에 도달했는가(0xE), 안정 도달까지
 * 갔는가(0xF), 부트로더 점프 직전(0x7)에서 멈추는가" 가 한 자리로 보인다.
 *
 * @param stage AGR_BOOT_STAGE_APP_MAIN 또는 AGR_BOOT_STAGE_APP_STABLE.
 */
void AGR_Boot_MarkAppStage(uint8_t stage);

/**
 * @brief 연속 부팅 시도 가드(RTC->BKP5R) 무장 — ConfirmBoot 직후 1회 호출.
 *
 * 이미 무장돼 있으면(magic 일치) 아무것도 하지 않는다 — 횟수를 건드리면 가드가 무력해진다.
 * 미무장이면 (magic | 0) 을 써서 "이 App 은 안정 도달 시 횟수를 0 으로 되돌릴 줄 안다" 고
 * 부트로더에 알린다. 이후 부트로더는 진입마다 횟수를 올리고 AGR_BOOT_ATTEMPT_MAX 에서
 * App 점프를 멈춘다(FTP 대기). 부트로더는 FW 교체(FTP 완료·롤백) 시 무장을 해제한다.
 */
void AGR_Boot_ArmAttemptGuard(void);

/**
 * @brief 안정 동작 확인 시점에 호출 — 가드 횟수를 0 으로(BKP5R = magic|0, 무장 포함).
 *
 * 권장 시점: 스케줄러 시작 후 제어 루프가 5 s 이상 살아 있을 때 1회. 너무 이르면(예: 매 부팅
 * main 에서) 가드가 세는 "안정 도달 실패" 를 가릴 수 없게 된다.
 */
void AGR_Boot_ClearAttemptCount(void);

#ifdef __cplusplus
}
#endif

#endif /* AGR_MW_BOOT_ENABLE */

#endif /* AGR_MW_BOOT_INC_AGR_BOOT_CORE_H_ */