/**
 ******************************************************************************
 * @file    ioif_agrb_spi.h
 * @author  Angel Robotics Firmware Team (KimJinwoo)
 * @brief   [IOIF Layer] SPI 하드웨어 추상화 계층 헤더 (aeat_9955 origin)
 * @version 3.0 (Common Library - H7/G4 Dual Platform)
 * @date    Feb 12, 2026
 *
 * @details
 * - Handle-based API: ioif_spi.write(), ioif_spi.read() 등
 * - Resource Pool + Instance 2계층 아키텍처
 * - ISR 기반 SPI 모드 지원
 *
 * @note aeat_9955 원본 기반, H7/G4 듀얼 플랫폼 + ENABLE 가드 적용
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef _IOIF_AGRB_SPI_H_
#define _IOIF_AGRB_SPI_H_

#include "ioif_agrb_defs.h"

#if defined(AGRB_IOIF_SPI_ENABLE)

#include <stdint.h>
#include <stdbool.h>

/* HAL Includes (MCU-specific) */
#if defined(IOIF_MCU_SERIES_H7)
    #include "stm32h7xx_hal.h"
    #include "stm32h7xx_hal_spi.h"
    #include "stm32h7xx_hal_gpio.h"
#elif defined(IOIF_MCU_SERIES_G4)
    #include "stm32g4xx_hal.h"
    #include "stm32g4xx_hal_spi.h"
    #include "stm32g4xx_hal_gpio.h"
#else
    #error "Unsupported MCU series for IOIF SPI module"
#endif

#include "ioif_agrb_gpio.h"

#define IOIF_SPI_ID_NOT_ALLOCATED  (0xFFFFFFFF)

#define IOIF_SPI_MAX_INSTANCES              (16) //Max 16 devices can be handled
#define IOIF_SPI_MAX_HANDLERS               (4) //Max 4 SPI instances (SPI1, SPI2, SPI3, SPI4, ...)
#define IOIF_SPI_DEFAULT_TIMEOUT            (1000U)
#define IOIF_SPI_DEFAULT_DMA_TX_SIZE        (128)
#define IOIF_SPI_DEFAULT_DMA_RX_SIZE        (128)
#define IOIF_SPI_DEFAULT_DUMMY_SIZE         (256U) //BareMetal TransmitReceive용 스택 더미 RX 버퍼 크기
#define IOIF_SPI_ISR_MQ_MAX_CAPACITY        (256U) //ISR에서 관리하는 메시지 큐의 최대 크기 (메시지 수)

#define IOIF_SPI_ISR_MQ_UNIT_SIZE           (16U) //ISR에서 관리하는 메시지 큐의 단위 크기 (바이트)

typedef uint32_t IOIF_SPIx_t;
#define IOIF_SPI_NOT_ALLOCATED_ID           ((IOIF_SPIx_t)(-1))

typedef struct {
    SPI_HandleTypeDef* hspi;
    IOIF_GPIOx_t ss; //SS/CS pin as GPIO

    #if defined(USE_FREERTOS)
    struct {
        size_t tx_size;
        size_t rx_size;
    } dma;
    #endif

    struct {
        struct {
            bool enable; //ISR 기반 모드 활성화 여부
            size_t queue_length; //ISR 기반 모드에서 사용하는 메시지 큐 길이 (단위: 메시지 수)
        } isr_mode;
    } options;

    uint32_t timeout; //in ms
} IOIF_SPI_Initialize_t;

typedef struct {
    AGRBStatusDef (*assign)(IOIF_SPIx_t* id, IOIF_SPI_Initialize_t* init);
    AGRBStatusDef (*write)(IOIF_SPIx_t id, void* tx_buffer, uint16_t size);
    AGRBStatusDef (*read)(IOIF_SPIx_t id, void* tx_buffer, uint16_t tx_size, void* rx_buffer, uint16_t rx_size);
    AGRBStatusDef (*duplex)(IOIF_SPIx_t id, void* tx_buffer, void* rx_buffer, uint16_t size);

    /// @brief ISR 기반 모드 시작 함수 포인터.
    AGRBStatusDef (*start_isr_mode)(IOIF_SPIx_t id, bool enable_overwrite);
    AGRBStatusDef (*stop_isr_mode)(IOIF_SPIx_t id);
    AGRBStatusDef (*change_isr_queue_capacity)(IOIF_SPIx_t id, size_t size);

    /// @brief ISR 기반 송신 처리 함수 포인터.
    AGRBStatusDef (*write_isr)(IOIF_SPIx_t id, void* tx_buffer, uint16_t total_size);
    /// @brief ISR 기반 수신 처리 함수 포인터.
    AGRBStatusDef (*read_isr)(IOIF_SPIx_t id, void* rx_buffer, uint16_t size);

    IOIF_GPIOx_t (*get_ss_pin)(IOIF_SPIx_t id);
    AGRBStatusDef (*set_ss_pin)(IOIF_SPIx_t id, IOIF_GPIOx_t ss_pin);

    AGRBStatusDef (*mode_update)(IOIF_SPIx_t id, bool cpol, bool cpha);

    AGRBStatusDef (*reset)(IOIF_SPIx_t id);

} IOIF_SPI_Handle_t;

extern IOIF_SPI_Handle_t ioif_spi;

#endif /* AGRB_IOIF_SPI_ENABLE */

#endif /* _IOIF_AGRB_SPI_H_ */
