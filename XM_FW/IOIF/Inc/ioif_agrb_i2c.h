/**
 ******************************************************************************
 * @file    ioif_agrb_i2c.h
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] I2C 하드웨어 추상화 계층 헤더 (aeat_9955 origin)
 * @version 3.0 (Common Library - H7/G4 Dual Platform)
 * @date    Feb 12, 2026
 *
 * @details
 * - Handle-based API: ioif_i2c.mem_write(), ioif_i2c.mem_read() 등
 * - RTOS: DMA + Semaphore 기반
 * - BareMetal: Polling 기반
 *
 * @note aeat_9955 원본 기반, H7/G4 듀얼 플랫폼 + ENABLE 가드 적용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef INC_IOIF_AGRB_I2C_H_
#define INC_IOIF_AGRB_I2C_H_
#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_I2C_ENABLE)

#include <stdint.h>
#include <stdbool.h>

/* HAL Includes (MCU-specific) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_i2c.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx_hal.h"
    #include "stm32g4xx_hal_i2c.h"
#else
    #error "Unsupported MCU series for IOIF I2C module"
#endif

#include "ioif_agrb_gpio.h"

#define IOIF_I2C_NOT_INITIALIZED    (0xFFFFFFFF)

#define IOIF_I2C_MAX_INSTANCES    (4)

#define IOIF_I2C_DEFAULT_TIMEOUT        (1000U) //in ms

/** @brief GPIO bit-bang 버스 복구에 필요한 SCL 토글 횟수 */
#define IOIF_I2C_BUS_RECOVERY_SCL_TOGGLES  (18U)

/** @brief GPIO bit-bang 딜레이 루프 카운트 (~5us @480MHz) */
#define IOIF_I2C_BUS_RECOVERY_DELAY_LOOPS  (100U)

typedef uint32_t IOIF_I2Cx_t;

typedef enum {
    IOIF_I2C_MemAddrSize_8BIT = I2C_MEMADD_SIZE_8BIT,
    IOIF_I2C_MemAddrSize_16BIT = I2C_MEMADD_SIZE_16BIT,

} IOIF_I2C_MemAddrSize_e;

/**
 * @brief I2C 버스 복구용 GPIO 핀 맵 (Optional)
 * @details reset() 호출 시 SDA Stuck Low 상태를 GPIO bit-bang으로 복구합니다.
 *          pin_map.sda.port == NULL이면 GPIO 복구 없이 HAL DeInit/Init만 수행합니다.
 */
typedef struct {
    struct { GPIO_TypeDef* port; uint16_t pin; } sda;
    struct { GPIO_TypeDef* port; uint16_t pin; } scl;
    uint32_t alternate_function;  /**< GPIO_AF4_I2C1 등 (HAL GPIO AF 매크로) */
} IOIF_I2C_PinMap_t;

typedef struct {
    I2C_HandleTypeDef* hi2c;

    uint32_t timeout;

    /** @brief (Optional) GPIO 버스 복구용 핀 맵. sda.port=NULL이면 미사용. */
    IOIF_I2C_PinMap_t pin_map;

    #if defined(USE_FREERTOS)
    struct {
        size_t tx_size;
        size_t rx_size;
    } dma;
    #endif
} IOIF_I2C_Initialize_t;

typedef struct {
    AGRBStatusDef (*assign)(IOIF_I2Cx_t* id, const IOIF_I2C_Initialize_t* init);
    AGRBStatusDef (*reset)(IOIF_I2Cx_t id);

    AGRBStatusDef (*master_transmit)(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size);
    AGRBStatusDef (*master_receive)(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size);

    AGRBStatusDef (*mem_write)(IOIF_I2Cx_t id, uint16_t dev_address, uint16_t mem_address, IOIF_I2C_MemAddrSize_e mem_address_size, void* data, size_t size);
    AGRBStatusDef (*mem_read)(IOIF_I2Cx_t id, uint16_t dev_address, uint16_t mem_address, IOIF_I2C_MemAddrSize_e mem_address_size, void* data, size_t size);

    //원본 데이터 전송용 함수. DMA 버퍼를 거치지 않음
    AGRBStatusDef (*raw_transmit)(IOIF_I2Cx_t id, uint16_t dev_address, const void* data, size_t size);
    AGRBStatusDef (*raw_receive)(IOIF_I2Cx_t id, uint16_t dev_address, void* data, size_t size);

    size_t (*get_tx_buffer_size)(IOIF_I2Cx_t id);

    /**
     * @brief 마지막 HAL I2C 에러 코드 조회
     * @return HAL_I2C_ERROR_NONE, HAL_I2C_ERROR_BERR, HAL_I2C_ERROR_ARLO,
     *         HAL_I2C_ERROR_AF (NACK), HAL_I2C_ERROR_OVR, HAL_I2C_ERROR_DMA,
     *         HAL_I2C_ERROR_TIMEOUT, HAL_I2C_ERROR_SIZE
     */
    uint32_t (*get_last_error)(IOIF_I2Cx_t id);
} IOIF_I2C_Handle_t;

extern IOIF_I2C_Handle_t ioif_i2c;

#endif /* AGRB_IOIF_I2C_ENABLE */

#endif /* INC_IOIF_AGRB_I2C_H_ */
