/**
 ******************************************************************************
 * @file    mcp79510.h
 * @author  HyundoKim
 * @brief   [Device Layer] MCP79510 Real-Time Clock SPI Driver
 * @version 0.1
 * @date    Mar 02, 2026
 *
 * @note    IOC Configuration:
 *   SPI: Full-Duplex Master, ≤10MHz, SPI Mode 0 (CPOL=0, CPHA=0)
 *   NSS: Software (IOIF manages CS via GPIO)
 *   DMA: TX + RX, Normal mode
 *   Interrupt: Enable (Priority: lower than control timer)
 *   XM10 ref: SPI2, PI0~PI3, DMA1_S5(TX)/S6(RX), Priority 6
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef MCP79510_H
#define MCP79510_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ioif_agrb_spi.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* MCP79510 SPI Command */
#define MCP79510_CMD_READ       (0x13U)  /* EEREAD — Timekeeping Read */
#define MCP79510_CMD_WRITE      (0x12U)  /* EEWRITE — Timekeeping Write */
#define MCP79510_CMD_CLRWDT     (0x44U)  /* Clear Watchdog Timer */
#define MCP79510_CMD_IDWRITE    (0x32U)  /* ID Write */
#define MCP79510_CMD_IDREAD     (0x33U)  /* ID Read */
#define MCP79510_CMD_SRREAD     (0x05U)  /* Status Register Read */
#define MCP79510_CMD_SRWRITE    (0x01U)  /* Status Register Write */

/* Timekeeping Register Addresses */
#define MCP79510_REG_HUNDREDTHS (0x00U)
#define MCP79510_REG_SECONDS    (0x01U)
#define MCP79510_REG_MINUTES    (0x02U)
#define MCP79510_REG_HOURS      (0x03U)
#define MCP79510_REG_WEEKDAY    (0x04U)
#define MCP79510_REG_DATE       (0x05U)
#define MCP79510_REG_MONTH      (0x06U)
#define MCP79510_REG_YEAR       (0x07U)
#define MCP79510_REG_CONTROL    (0x08U)
#define MCP79510_REG_OSCTRIM    (0x09U)

/* Bit Masks */
#define MCP79510_ST_BIT         (0x80U)  /* RTCSEC[7]: Oscillator Start */
#define MCP79510_VBATEN_BIT     (0x08U)  /* RTCWKDAY[3]: Battery Backup Enable */
#define MCP79510_OSCRUN_BIT     (0x20U)  /* RTCWKDAY[5]: Oscillator Running */
#define MCP79510_LPYR_BIT       (0x20U)  /* RTCMTH[5]: Leap Year */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief RTC 날짜/시간 구조체
 */
typedef struct {
    uint8_t year;       /**< 0~99 (2000 + year) */
    uint8_t month;      /**< 1~12 */
    uint8_t day;        /**< 1~31 */
    uint8_t weekday;    /**< 1~7 (Mon~Sun) */
    uint8_t hour;       /**< 0~23 */
    uint8_t minute;     /**< 0~59 */
    uint8_t second;     /**< 0~59 */
} MCP79510_DateTime_t;

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief MCP79510 초기화 (Singleton + DI)
 * @param[in] spi_id IOIF SPI 인스턴스 ID (system_startup에서 assign 후 전달)
 * @return AGRBStatus_OK: 성공
 */
AGRBStatusDef MCP79510_Init(IOIF_SPIx_t spi_id);

/**
 * @brief 날짜/시간 설정
 * @param[in] dt 설정할 날짜/시간
 * @return AGRBStatus_OK: 성공
 */
AGRBStatusDef MCP79510_SetDateTime(const MCP79510_DateTime_t *dt);

/**
 * @brief 날짜/시간 읽기
 * @param[out] dt 읽어온 날짜/시간
 * @return AGRBStatus_OK: 성공
 */
AGRBStatusDef MCP79510_GetDateTime(MCP79510_DateTime_t *dt);

/**
 * @brief 오실레이터 동작 여부 확인
 * @return true: 동작 중, false: 정지
 */
bool MCP79510_IsRunning(void);

/**
 * @brief [DIAG] SPI raw 바이트 캡처 (duplex 3-byte 트랜잭션)
 * @param[in]  addr    레지스터 주소
 * @param[out] raw_rx  3바이트 rx 버퍼 (rx[0]=echo, rx[1]=echo, rx[2]=data)
 * @param[in]  rx_len  버퍼 크기 (최소 3)
 * @return duplex 리턴값
 */
AGRBStatusDef MCP79510_DiagReadRaw(uint8_t addr, uint8_t *raw_rx, uint8_t rx_len);

/**
 * @brief 배터리 백업 활성화 (VBATEN bit 설정)
 * @return AGRBStatus_OK: 성공
 */
AGRBStatusDef MCP79510_EnableBatteryBackup(void);

#ifdef __cplusplus
}
#endif

#endif /* MCP79510_H */
