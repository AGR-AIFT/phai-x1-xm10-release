/**
 ******************************************************************************
 * @file    ioif_agrb_eth_mdio.h
 * @author  Angel Robotics Firmware Team (HyundoKim)
 * @brief   [IOIF Layer] Ethernet MDIO 하드웨어 추상화 계층 헤더
 * @version 1.0 (Common Library - H7 Platform)
 * @date    Mar 04, 2026
 *
 * @details
 * - Handle-based API: ioif_eth_mdio.read(), ioif_eth_mdio.write()
 * - Instance Pool 아키텍처 (ioif_agrb_spi 패턴 준용)
 * - MDIO는 Polling 기반이므로 DMA 불필요, Mutex만 사용
 *
 * @note H7 전용 (G4 시리즈는 ETH 미지원)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef _IOIF_AGRB_ETH_MDIO_H_
#define _IOIF_AGRB_ETH_MDIO_H_

#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_ETH_MDIO_ENABLE)

#include <stdint.h>
#include <stdbool.h>

/* HAL Includes (MCU-specific) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_eth.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #error "ETH MDIO is not available on STM32G4 series"
#else
    #error "Unsupported MCU series for IOIF ETH MDIO module"
#endif

/**
 *===========================================================================
 * TYPE DEFINITIONS
 *===========================================================================
 */

/** @brief MDIO Instance ID (배열 인덱스) */
typedef uint32_t IOIF_ETH_MDIOx_t;

#define IOIF_ETH_MDIO_NOT_ALLOCATED     (0xFFFFFFFF)
#define IOIF_ETH_MDIO_MAX_INSTANCES     (2)      /**< 최대 MDIO 인스턴스 수 */
#define IOIF_ETH_MDIO_DEFAULT_TIMEOUT   (1000U)  /**< 기본 타임아웃 (ms) */

/**
 *===========================================================================
 * INITIALIZATION STRUCTURE
 *===========================================================================
 */

/** @brief MDIO 초기화 구조체 (HAL Handle 주입) */
typedef struct {
    ETH_HandleTypeDef* heth;    /**< HAL ETH 핸들 (필수) */
    uint32_t timeout;           /**< MDIO R/W timeout (ms), 0이면 기본값 1000ms */
} IOIF_ETH_MDIO_Initialize_t;

/**
 *===========================================================================
 * HANDLE STRUCTURE (API)
 *===========================================================================
 */

/**
 * @brief MDIO Handle 구조체 (ioif_spi, ioif_i2c 패턴 동일)
 * @details 사용자는 글로벌 핸들 `ioif_eth_mdio`를 통해 API에 접근합니다.
 *
 * @code
 * // 초기화
 * IOIF_ETH_MDIO_Initialize_t init = { .heth = &heth, .timeout = 1000 };
 * ioif_eth_mdio.assign(&mdio_id, &init);
 *
 * // PHY 레지스터 읽기
 * uint32_t value;
 * ioif_eth_mdio.read(mdio_id, phy_addr, reg_addr, &value);
 * @endcode
 */
typedef struct {
    /**
     * @brief MDIO 인스턴스 할당
     * @param id    [out] 할당된 인스턴스 ID
     * @param init  [in]  초기화 구조체 (heth 핸들 주입)
     * @return AGRBStatus_OK / AGRBStatus_PARAM_ERROR / AGRBStatus_INSTANCE_FULL
     */
    AGRBStatusDef (*assign)(IOIF_ETH_MDIOx_t* id, const IOIF_ETH_MDIO_Initialize_t* init);

    /**
     * @brief PHY 레지스터 읽기 (MDIO Clause 22)
     * @param id        MDIO 인스턴스 ID
     * @param phy_addr  PHY 주소 (0~31)
     * @param reg_addr  레지스터 주소 (0~31)
     * @param value     [out] 읽은 값 (uint32_t — HAL API 호환)
     * @return AGRBStatus_OK / AGRBStatus_ERROR / AGRBStatus_TIMEOUT
     */
    AGRBStatusDef (*read)(IOIF_ETH_MDIOx_t id, uint32_t phy_addr, uint32_t reg_addr,
                          uint32_t* value);

    /**
     * @brief PHY 레지스터 쓰기 (MDIO Clause 22)
     * @param id        MDIO 인스턴스 ID
     * @param phy_addr  PHY 주소 (0~31)
     * @param reg_addr  레지스터 주소 (0~31)
     * @param value     쓸 값
     * @return AGRBStatus_OK / AGRBStatus_ERROR / AGRBStatus_TIMEOUT
     */
    AGRBStatusDef (*write)(IOIF_ETH_MDIOx_t id, uint32_t phy_addr, uint32_t reg_addr,
                           uint32_t value);

    /**
     * @brief MDIO 클럭 레인지 설정 (HAL_ETH_SetMDIOClockRange 래핑)
     * @param id  MDIO 인스턴스 ID
     * @return AGRBStatus_OK / AGRBStatus_ERROR
     */
    AGRBStatusDef (*set_clock_range)(IOIF_ETH_MDIOx_t id);

} IOIF_ETH_MDIO_Handle_t;

/**
 *===========================================================================
 * GLOBAL HANDLE
 *===========================================================================
 */

/** @brief MDIO 글로벌 핸들 — 사용자는 이 핸들로만 API 접근 */
extern IOIF_ETH_MDIO_Handle_t ioif_eth_mdio;

#endif /* AGRB_IOIF_ETH_MDIO_ENABLE */

#endif /* _IOIF_AGRB_ETH_MDIO_H_ */
