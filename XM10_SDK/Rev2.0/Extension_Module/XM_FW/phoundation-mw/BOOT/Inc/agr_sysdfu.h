/**
 * @file  agr_sysdfu.h
 * @author Angel Robotics
 * @brief  STM32H7 내장 시스템 부트로더(USB DFU) 진입 헬퍼
 * @version 1.0.0
 * @details
 *   ST-Link 없이 USB로 FW를 굽기 위한 최소 구현.
 *   커스텀 부트로더(AGR_BOOT V2)가 완성되기 전까지의 중간 단계이며,
 *   FLASH 파티션 / 링커 스크립트 / VTOR 재배치를 전혀 건드리지 않는다.
 *
 *   동작 원리
 *   ---------
 *   1) 호스트가 SDO_ID_MSG_ENTER_DFU 를 쓴다.
 *   2) App 이 RTC->BKP1R 에 매직을 쓰고 (VBAT 도메인 = 시스템 리셋에도 보존)
 *      수백 ms 뒤 NVIC_SystemReset() 을 건다.
 *   3) 리셋 직후 main() 최상단에서 AGR_SysDFU_CheckAndJump() 가 매직을 보고
 *      0x1FF09800 (STM32H74x/75x 시스템 부트로더 벡터테이블) 로 점프한다.
 *   4) PC 에 "STM32 BOOTLOADER" DFU 장치가 나타난다 -> CubeProgrammer 로 굽기.
 *
 *   "리셋 후 점프" 를 쓰는 이유
 *   ---------------------------
 *   동작 중인 App 에서 곧바로 점프하면 클럭/캐시/DMA/USB 잔여 상태 때문에
 *   시스템 부트로더가 열거되지 않는 사례가 흔하다. 리셋 직후에는 모든 주변장치가
 *   리셋 상태라 별도 de-init 없이 확정적으로 동작한다.
 *
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#ifndef AGR_BOOT_INC_AGR_SYSDFU_H_
#define AGR_BOOT_INC_AGR_SYSDFU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/** @brief STM32H74x/H75x 시스템 부트로더 진입점 (시스템 메모리 base 와 다름!).
 *  H7A3=0x1FF0A800, H7B0=0x1FF0A000 이므로 파트 변경 시 반드시 재확인. */
#define AGR_SYSDFU_BOOT_ENTRY      (0x1FF09800UL)

/** @brief RTC 백업 레지스터 매직. AGR_BOOT V2 는 BKP0R 을 쓰므로 충돌 회피로 BKP1R 사용. */
#define AGR_SYSDFU_MAGIC           (0xDF05B007UL)
#define AGR_SYSDFU_BKP_REG_INDEX   (1U)

/** @brief SDO 오조작 방지용 키. 이 값이 아니면 요청을 거부한다. */
#define AGR_SYSDFU_REQUEST_KEY     (0xA5U)

/** @brief SDO 응답 코드. */
#define AGR_SYSDFU_ACK_ARMED       (0xA5U)  /**< 수락 — 곧 리셋됨 */
#define AGR_SYSDFU_ACK_BAD_KEY     (0x01U)  /**< 키 불일치 */
#define AGR_SYSDFU_ACK_BUSY        (0x02U)  /**< 구동 중이라 거부 */

/**
 * @brief  리셋 직후 매직을 확인하고 시스템 부트로더로 점프한다.
 * @note   ★ main() 최상단, MPU_Config()/SCB_EnableICache()/HAL_Init() 보다 먼저 호출할 것.
 *         매직이 없으면 즉시 반환하므로 일반 부팅에는 영향이 없다.
 *         매직은 점프 직전에 클리어하므로 DFU 세션이 끝나면 정상 부팅한다.
 */
void AGR_SysDFU_CheckAndJump(void);

/**
 * @brief  DFU 진입을 예약한다 (매직 기록 + 지연 리셋 카운터 시작).
 * @param  delay_ms  리셋까지 대기 시간 [ms]. SDO 응답이 호스트로 빠져나갈
 *                   시간을 확보하기 위함. 200~500 권장.
 * @note   ISR 컨텍스트(OTG_FS)에서 호출해도 안전하다 — 여기서 리셋하지 않는다.
 */
void AGR_SysDFU_ArmRequest(uint16_t delay_ms);

/** @brief 예약 취소 (매직 클리어 + 카운터 정지). */
void AGR_SysDFU_Cancel(void);

/** @brief 예약 상태 조회. */
bool AGR_SysDFU_IsArmed(void);

/**
 * @brief  지연 리셋 서비스. 1 kHz 틱(RunMsgHdlr)에서 호출할 것.
 * @note   카운터가 0 이 되는 순간 NVIC_SystemReset() 을 수행한다.
 */
void AGR_SysDFU_Service1kHz(void);

#ifdef __cplusplus
}
#endif

#endif /* AGR_BOOT_INC_AGR_SYSDFU_H_ */
