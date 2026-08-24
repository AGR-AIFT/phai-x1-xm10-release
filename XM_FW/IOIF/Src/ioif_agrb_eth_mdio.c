/**
 ******************************************************************************
 * @file    ioif_agrb_eth_mdio.c
 * @author  Angel Robotics Firmware Team (HyundoKim)
 * @brief   [IOIF Layer] Ethernet MDIO 하드웨어 추상화 계층 구현
 * @version 1.0 (Common Library - H7 Platform)
 * @date    Mar 04, 2026
 *
 * @details
 * HAL_ETH_ReadPHYRegister / WritePHYRegister를 IOIF Handle 패턴으로 래핑.
 * MDIO는 Polling 기반이므로 DMA 불필요. RTOS 환경에서는 Mutex로 동시 접근 방지.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_eth_mdio.h"

#if defined(AGRB_IOIF_ETH_MDIO_ENABLE)

#include <string.h>

/**
 *===========================================================================
 * INTERNAL DATA STRUCTURES
 *===========================================================================
 */

/** @brief MDIO Instance (SPI Instance 패턴 준용, DMA 불필요) */
typedef struct {
    IOIF_ETH_MDIO_Initialize_t init;    /**< 초기화 파라미터 사본 */
    #if defined(USE_FREERTOS)
    SemaphoreHandle_t device;           /**< MDIO 버스 Mutex (동시 접근 방지) */
    #endif
} _IOIF_ETH_MDIO_Instance_t;

/**
 *===========================================================================
 * STATIC VARIABLES
 *===========================================================================
 */

static _IOIF_ETH_MDIO_Instance_t _instances[IOIF_ETH_MDIO_MAX_INSTANCES];
static uint32_t _instance_count = 0;

/**
 *===========================================================================
 * STATIC FUNCTION PROTOTYPES
 *===========================================================================
 */

static AGRBStatusDef _AssignInstance(IOIF_ETH_MDIOx_t* id, const IOIF_ETH_MDIO_Initialize_t* init);
static AGRBStatusDef _ReadReg(IOIF_ETH_MDIOx_t id, uint32_t phy_addr, uint32_t reg_addr,
                              uint32_t* value);
static AGRBStatusDef _WriteReg(IOIF_ETH_MDIOx_t id, uint32_t phy_addr, uint32_t reg_addr,
                               uint32_t value);
static AGRBStatusDef _SetClockRange(IOIF_ETH_MDIOx_t id);

/**
 *===========================================================================
 * GLOBAL HANDLE INITIALIZATION
 *===========================================================================
 */

IOIF_ETH_MDIO_Handle_t ioif_eth_mdio = {
    .assign          = _AssignInstance,
    .read            = _ReadReg,
    .write           = _WriteReg,
    .set_clock_range = _SetClockRange,
};

/**
 *===========================================================================
 * STATIC FUNCTION IMPLEMENTATIONS
 *===========================================================================
 */

/**
 * @brief MDIO 인스턴스 할당 (heth 핸들 주입 + RTOS Mutex 생성)
 */
static AGRBStatusDef _AssignInstance(IOIF_ETH_MDIOx_t* id, const IOIF_ETH_MDIO_Initialize_t* init)
{
    /* 파라미터 검증 */
    if (id == NULL || init == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }
    if (init->heth == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }

    /* 중복 등록 체크 (동일 heth 핸들) */
    for (uint32_t i = 0; i < _instance_count; i++) {
        if (_instances[i].init.heth == init->heth) {
            return AGRBStatus_ALREADY_INITIALIZED;
        }
    }

    /* Instance 풀 오버플로우 */
    if (_instance_count >= IOIF_ETH_MDIO_MAX_INSTANCES) {
        return AGRBStatus_INSTANCE_FULL;
    }

    /* 새 Instance 생성 */
    _IOIF_ETH_MDIO_Instance_t* instance = &_instances[_instance_count];
    memset(instance, 0, sizeof(_IOIF_ETH_MDIO_Instance_t));
    memcpy(&instance->init, init, sizeof(IOIF_ETH_MDIO_Initialize_t));

    /* 기본값 적용 */
    if (instance->init.timeout == 0) {
        instance->init.timeout = IOIF_ETH_MDIO_DEFAULT_TIMEOUT;
    }

    #if defined(USE_FREERTOS)
    /* MDIO 버스 Mutex 생성 (Priority Inheritance) */
    instance->device = xSemaphoreCreateMutex();
    if (instance->device == NULL) {
        memset(instance, 0, sizeof(_IOIF_ETH_MDIO_Instance_t));
        return AGRBStatus_SEMAPHORE_ERROR;
    }
    /* Mutex는 생성 시 이미 available (Give 불필요) */
    #endif

    /* ID 반환 */
    *id = _instance_count++;

    return AGRBStatus_OK;
}


/**
 * @brief PHY 레지스터 읽기 (Mutex 보호 + HAL 래핑)
 */
static AGRBStatusDef _ReadReg(IOIF_ETH_MDIOx_t id, uint32_t phy_addr, uint32_t reg_addr,
                              uint32_t* value)
{
    /* 파라미터 검증 */
    if (id >= _instance_count) {
        return AGRBStatus_PARAM_ERROR;
    }
    if (value == NULL) {
        return AGRBStatus_PARAM_ERROR;
    }

    _IOIF_ETH_MDIO_Instance_t* instance = &_instances[id];

    #if defined(USE_FREERTOS)
    /* MDIO 버스 독점 */
    if (xSemaphoreTake(instance->device, pdMS_TO_TICKS(instance->init.timeout)) != pdTRUE) {
        return AGRBStatus_TIMEOUT;
    }
    #endif

    /* HAL API 호출 */
    HAL_StatusTypeDef hal_status = HAL_ETH_ReadPHYRegister(
        instance->init.heth, phy_addr, reg_addr, value);

    #if defined(USE_FREERTOS)
    xSemaphoreGive(instance->device);
    #endif

    return IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
}


/**
 * @brief PHY 레지스터 쓰기 (Mutex 보호 + HAL 래핑)
 */
static AGRBStatusDef _WriteReg(IOIF_ETH_MDIOx_t id, uint32_t phy_addr, uint32_t reg_addr,
                               uint32_t value)
{
    /* 파라미터 검증 */
    if (id >= _instance_count) {
        return AGRBStatus_PARAM_ERROR;
    }

    _IOIF_ETH_MDIO_Instance_t* instance = &_instances[id];

    #if defined(USE_FREERTOS)
    if (xSemaphoreTake(instance->device, pdMS_TO_TICKS(instance->init.timeout)) != pdTRUE) {
        return AGRBStatus_TIMEOUT;
    }
    #endif

    HAL_StatusTypeDef hal_status = HAL_ETH_WritePHYRegister(
        instance->init.heth, phy_addr, reg_addr, value);

    #if defined(USE_FREERTOS)
    xSemaphoreGive(instance->device);
    #endif

    return IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_status);
}


/**
 * @brief MDIO 클럭 레인지 설정
 * @details HAL_ETH_SetMDIOClockRange 래핑.
 *          AHB 클럭에 따라 MDIO MDC 분주비를 자동 설정합니다.
 */
static AGRBStatusDef _SetClockRange(IOIF_ETH_MDIOx_t id)
{
    if (id >= _instance_count) {
        return AGRBStatus_PARAM_ERROR;
    }

    _IOIF_ETH_MDIO_Instance_t* instance = &_instances[id];

    HAL_ETH_SetMDIOClockRange(instance->init.heth);

    return AGRBStatus_OK;
}

#endif /* AGRB_IOIF_ETH_MDIO_ENABLE */
