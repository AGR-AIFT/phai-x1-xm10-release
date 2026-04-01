/**
 ******************************************************************************
 * @file    rtl8201f.h
 * @author  Angel Robotics Firmware Team (HyundoKim)
 * @brief   [Device Layer] RTL8201F 10/100 Ethernet PHY 드라이버 헤더
 * @version 1.0
 * @date    Mar 04, 2026
 *
 * @details
 * WS5(KAIST_WALKON5) 프로젝트에서 검증된 RTL8201F 드라이버를
 * Arch V2 Device Driver 패턴(Singleton + IOIF ID DI)으로 리팩토링.
 *
 * [WS5 → XM V2 주요 변경]
 * - IOCtx 함수 포인터 → IOIF ETH MDIO Handle 직접 호출
 * - Object 인스턴스 → Singleton (static 변수)
 * - GetTick busy-wait → osDelay (RTOS)
 * - SoftReset timeout 버그 수정
 * - Include guard 수정, Doxygen 추가
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef RTL8201F_H
#define RTL8201F_H

#include "ioif_agrb_eth_mdio.h"

/**
 *===========================================================================
 * STATUS CODES
 *===========================================================================
 */

#define RTL8201F_STATUS_READ_ERROR            ((int32_t)-5)
#define RTL8201F_STATUS_WRITE_ERROR           ((int32_t)-4)
#define RTL8201F_STATUS_ADDRESS_ERROR         ((int32_t)-3)
#define RTL8201F_STATUS_RESET_TIMEOUT         ((int32_t)-2)
#define RTL8201F_STATUS_ERROR                 ((int32_t)-1)
#define RTL8201F_STATUS_OK                    ((int32_t) 0)
#define RTL8201F_STATUS_LINK_DOWN             ((int32_t) 1)
#define RTL8201F_STATUS_100MBITS_FULLDUPLEX   ((int32_t) 2)
#define RTL8201F_STATUS_100MBITS_HALFDUPLEX   ((int32_t) 3)
#define RTL8201F_STATUS_10MBITS_FULLDUPLEX    ((int32_t) 4)
#define RTL8201F_STATUS_10MBITS_HALFDUPLEX    ((int32_t) 5)
#define RTL8201F_STATUS_AUTONEGO_NOTDONE      ((int32_t) 6)

/**
 *===========================================================================
 * PARAMETERS
 *===========================================================================
 */

#define RTL8201F_SW_RESET_TIMEOUT  ((uint32_t)500U)   /**< SW Reset 완료 대기 (ms) */
#define RTL8201F_REG_SETTING_DELAY ((uint32_t)200U)   /**< 레지스터 설정 간 딜레이 (ms) */
#define RTL8201F_INIT_TIMEOUT      ((uint32_t)2000U)  /**< 초기화 완료 안정화 대기 (ms) */
#define RTL8201F_PHY_ADDR          ((uint32_t)0U)     /**< PHY 주소 (PHYAD[0]=0) */

/**
 *===========================================================================
 * PUBLIC API (Singleton — PCA9957, MCP79510 패턴 동일)
 *===========================================================================
 */

/**
 * @brief RTL8201F PHY 초기화
 * @details SW Reset + Page 7 RMII 타이밍/LED 설정 + 안정화 대기.
 *          HW Reset(GPIO)은 호출자(ethernetif.c)에서 사전 수행해야 합니다.
 * @param mdio_id  IOIF ETH MDIO 인스턴스 ID (ioif_eth_mdio.assign으로 획득)
 * @return RTL8201F_STATUS_OK 성공, 음수 에러 코드
 */
int32_t RTL8201F_Init(IOIF_ETH_MDIOx_t mdio_id);

/**
 * @brief PHY 링크 상태 조회
 * @details BSR 2회 읽기 (IEEE 802.3 latching 해제) + BCR auto-nego 확인.
 * @return RTL8201F_STATUS_LINK_DOWN / RTL8201F_STATUS_100MBITS_FULLDUPLEX / ...
 */
int32_t RTL8201F_GetLinkState(void);

#endif /* RTL8201F_H */
