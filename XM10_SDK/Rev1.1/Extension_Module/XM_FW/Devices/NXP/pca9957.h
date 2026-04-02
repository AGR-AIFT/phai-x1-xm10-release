/**
 ******************************************************************************
 * @file    pca9957.h
 * @author  HyundoKim
 * @brief   [Device Layer] PCA9957 24-Channel LED Driver SPI Driver
 * @version 0.1
 * @date    Mar 02, 2026
 *
 * @note    IOC Configuration:
 *   SPI: Full-Duplex Master, ≤10MHz, SPI Mode 0 (CPOL=0, CPHA=0), MSB First
 *   NSS: Software (IOIF manages CS via GPIO)
 *   Control: nRESET (Pull-up, Active Low), nOE (Pull-up, Active Low)
 *   DMA: TX + RX, Normal mode
 *   Interrupt: Enable (Priority: lower than control timer)
 *   XM10 ref: SPI5, PK0/PK1/PJ10/PJ11, DMA1_S7(TX)/DMA2_S0(RX), Priority 6
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef PCA9957_H
#define PCA9957_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ioif_agrb_spi.h"
#include "ioif_agrb_gpio.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define PCA9957_NUM_CHANNELS    (24U)

/* PCA9957 SPI Command Byte Format (7-bit addressing):
 * [A6 | A5 | A4 | A3 | A2 | A1 | A0 | R/W]
 * A6:A0 = Register address (7-bit, shifted left by 1)
 * R/W   = 0: Write, 1: Read
 * Auto-Increment: MODE1.AI2 비트로 제어 (커맨드 바이트 무관)
 *
 * [AS-IS] 5-bit mask (0x1F) + AI flag in cmd byte (0x80) — PCA9955B 포맷 오적용
 * [TO-BE] 7-bit mask (0x7F), AI는 MODE1 레지스터로 제어 — PCA9957 datasheet 준수
 * [근거] NXP 공식 데모앱 + Arduino 라이브러리 + 커뮤니티 예제 교차 검증
 */
#define PCA9957_CMD_WRITE(reg)      ((uint8_t)(((reg) & 0x7F) << 1))
#define PCA9957_CMD_READ(reg)       ((uint8_t)((((reg) & 0x7F) << 1) | 0x01))

/* AI 매크로: MODE1.AI2가 활성화된 상태에서 multi-byte 전송 시 자동 증가.
 * 커맨드 바이트는 WRITE/READ와 동일하며, 의도 문서화용 별칭. */
#define PCA9957_CMD_AI_WRITE(reg)   PCA9957_CMD_WRITE(reg)
#define PCA9957_CMD_AI_READ(reg)    PCA9957_CMD_READ(reg)

/* Register Addresses (7-bit, NXP PCA9957 datasheet 기준)
 *
 * [AS-IS] PCA9955B 기준 5-bit 주소 (PWM0=0x0A, IREF0=0x22, IREFALL=0x3C)
 * [TO-BE] PCA9957 실제 7-bit 주소 (PWM0=0x10, IREF0=0x28, IREFALL=0x6B)
 */
#define PCA9957_REG_MODE1       (0x00U)
#define PCA9957_REG_MODE2       (0x01U)
#define PCA9957_REG_EFLAG0      (0x02U)
#define PCA9957_REG_EFLAG1      (0x03U)
#define PCA9957_REG_EFLAG2      (0x04U)
#define PCA9957_REG_EFLAG3      (0x05U)
#define PCA9957_REG_EFLAG4      (0x06U)
#define PCA9957_REG_EFLAG5      (0x07U)
#define PCA9957_REG_LEDOUT0     (0x08U)
#define PCA9957_REG_LEDOUT1     (0x09U)
#define PCA9957_REG_LEDOUT2     (0x0AU)
#define PCA9957_REG_LEDOUT3     (0x0BU)
#define PCA9957_REG_LEDOUT4     (0x0CU)
#define PCA9957_REG_LEDOUT5     (0x0DU)
#define PCA9957_REG_GRPPWM      (0x0EU)
#define PCA9957_REG_GRPFREQ     (0x0FU)
#define PCA9957_REG_PWM0        (0x10U)  /* PWM0~PWM23: 0x10~0x27 */
#define PCA9957_REG_IREF0       (0x28U)  /* IREF0~IREF23: 0x28~0x3F */
#define PCA9957_REG_OFFSET      (0x69U)
#define PCA9957_REG_PWMALL      (0x6AU)
#define PCA9957_REG_IREFALL     (0x6BU)

/* MODE1 Bits */
#define PCA9957_MODE1_AI2       (0x80U)  /* Auto-Increment bit 2 */
#define PCA9957_MODE1_SLEEP     (0x10U)  /* Sleep (oscillator off) */

/* LEDOUT Values (per channel, 2 bits) */
#define PCA9957_LEDOUT_OFF      (0x00U)  /* LED driver off */
#define PCA9957_LEDOUT_ON       (0x01U)  /* LED fully on */
#define PCA9957_LEDOUT_PWM      (0x02U)  /* Individual PWM */
#define PCA9957_LEDOUT_PWM_GRP  (0x03U)  /* PWM + Group dimming */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief PCA9957 채널 LED 그룹 (RGB 3ch × 7그룹 + Reserved)
 */
typedef enum {
    PCA9957_GRP_EMG = 0,    /**< CH0-2:   EMG RGB */
    PCA9957_GRP_FES,        /**< CH3-5:   FES RGB */
    PCA9957_GRP_IMU,        /**< CH6-8:   IMU RGB */
    PCA9957_GRP_HMMG,       /**< CH9-11:  HMMG RGB */
    PCA9957_GRP_GRF_L,      /**< CH12-14: GRF Left RGB */
    PCA9957_GRP_GRF_R,      /**< CH15-17: GRF Right RGB */
    PCA9957_GRP_USB,        /**< CH18-20: USB RGB */
    PCA9957_GRP_RESERVED,   /**< CH21-23: Reserved */
    PCA9957_GRP_COUNT       /**< 8 */
} PCA9957_Group_t;

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief PCA9957 초기화 (Singleton + DI)
 * @param[in] spi_id    IOIF SPI 인스턴스 ID
 * @param[in] nreset_id IOIF GPIO ID for nRESET pin
 * @param[in] noe_id    IOIF GPIO ID for nOE pin
 * @return AGRBStatus_OK: 성공
 */
AGRBStatusDef PCA9957_Init(IOIF_SPIx_t spi_id,
                           IOIF_GPIOx_t nreset_id,
                           IOIF_GPIOx_t noe_id);

/**
 * @brief 개별 채널 PWM 설정
 * @param[in] channel 채널 번호 (0~23)
 * @param[in] pwm     PWM 듀티 (0x00=OFF ~ 0xFF=Full)
 */
AGRBStatusDef PCA9957_SetChannelPWM(uint8_t channel, uint8_t pwm);

/**
 * @brief RGB 그룹 색상 설정
 * @param[in] group PCA9957 그룹 (EMG, FES, IMU 등)
 * @param[in] r     Red PWM (0~255)
 * @param[in] g     Green PWM (0~255)
 * @param[in] b     Blue PWM (0~255)
 */
AGRBStatusDef PCA9957_SetRGB(PCA9957_Group_t group, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 24채널 PWM 일괄 업데이트 (Auto-Increment, 1회 SPI 전송)
 * @param[in] pwm 24바이트 PWM 배열 (CH0~CH23 순서)
 */
AGRBStatusDef PCA9957_UpdateAll(const uint8_t pwm[PCA9957_NUM_CHANNELS]);

/**
 * @brief 개별 채널 전류 설정
 * @param[in] channel 채널 번호 (0~23)
 * @param[in] iref    전류 값 (0x00~0xFF, IREF 레지스터)
 */
AGRBStatusDef PCA9957_SetChannelCurrent(uint8_t channel, uint8_t iref);

/**
 * @brief LED 출력 활성화 (nOE → LOW)
 */
void PCA9957_Enable(void);

/**
 * @brief LED 출력 비활성화 (nOE → HIGH)
 */
void PCA9957_Disable(void);

/**
 * @brief 하드웨어 리셋 (nRESET pulse: LOW 10ms → HIGH)
 */
void PCA9957_Reset(void);

/**
 * @brief 레지스터 1바이트 읽기 (진단/디버깅용)
 * @param[in]  reg   레지스터 주소 (7-bit)
 * @param[out] value 읽은 값
 */
AGRBStatusDef PCA9957_ReadReg(uint8_t reg, uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif /* PCA9957_H */
