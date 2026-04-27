/**
 ******************************************************************************
 * @file    rtl8201f_regmap.h
 * @author  Angel Robotics Firmware Team (HyundoKim)
 * @brief   [Device Layer] RTL8201F PHY 레지스터 맵 정의
 * @version 1.0
 * @date    Mar 04, 2026
 *
 * @details
 * IEEE 802.3 표준 레지스터 (Reg 0~6, 13~14) +
 * RTL8201F 전용 레지스터 (Page 7: RMII, LED)
 *
 * @note WS5 프로젝트(KAIST_WALKON5)에서 검증된 레지스터 정의를 포팅.
 *       Include guard 수정 (#define → #ifndef).
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef RTL8201F_REGMAP_H
#define RTL8201F_REGMAP_H

/* ===== Basic Control & Status Register (IEEE 802.3) ===== */

#define RTL8201F_BCR ((uint16_t)0x00U)
#define RTL8201F_BCR_SOFT_RST      ((uint16_t)0x8000U)
#define RTL8201F_BCR_LOOPBACK      ((uint16_t)0x4000U)
#define RTL8201F_BCR_SPEED_SELECT  ((uint16_t)0x2000U)
#define RTL8201F_BCR_AUTONEGO_EN   ((uint16_t)0x1000U)
#define RTL8201F_BCR_POWER_DOWN    ((uint16_t)0x0800U)
#define RTL8201F_BCR_ISOLATE       ((uint16_t)0x0400U)
#define RTL8201F_BCR_AUTONEGO_RST  ((uint16_t)0x0200U)
#define RTL8201F_BCR_DUPLEX_MODE   ((uint16_t)0x0100U)

#define RTL8201F_BSR ((uint16_t)0x01U)
#define RTL8201F_BSR_100BASE_T4       ((uint16_t)0x8000U)
#define RTL8201F_BSR_100BASE_TX_FD    ((uint16_t)0x4000U)
#define RTL8201F_BSR_100BASE_TX_HD    ((uint16_t)0x2000U)
#define RTL8201F_BSR_10BASE_T_FD      ((uint16_t)0x1000U)
#define RTL8201F_BSR_10BASE_T_HD      ((uint16_t)0x0800U)
#define RTL8201F_BSR_AUTONEGO_CPLT    ((uint16_t)0x0020U)
#define RTL8201F_BSR_REMOTE_FAULT     ((uint16_t)0x0010U)
#define RTL8201F_BSR_AUTONEGO_ABILITY ((uint16_t)0x0008U)
#define RTL8201F_BSR_LINK_STATUS      ((uint16_t)0x0004U)
#define RTL8201F_BSR_JABBER_DETECT    ((uint16_t)0x0002U)
#define RTL8201F_BSR_EXTENDED_CAP     ((uint16_t)0x0001U)

/* ===== PHY Identifier Register ===== */

#define RTL8201F_PHYI1R ((uint16_t)0x02U)
#define RTL8201F_PHYI1R_OUI_HIGH     ((uint16_t)0x001CU)

#define RTL8201F_PHYI2R ((uint16_t)0x03U)
#define RTL8201F_PHYI2R_OUI_LOW      ((uint16_t)0x3200U)
#define RTL8201F_PHYI2R_MODEL_NBR    ((uint16_t)0x0010U)
#define RTL8201F_PHYI2R_REVISION_NBR ((uint16_t)0x0003U)

/* ===== Auto-Negotiation Register ===== */

#define RTL8201F_ANAR ((uint16_t)0x04U)
#define RTL8201F_ANAR_NEXT_PAGE            ((uint16_t)0x8000U)
#define RTL8201F_ANAR_ACK                  ((uint16_t)0x4000U)
#define RTL8201F_ANAR_REMOTE_FAULT         ((uint16_t)0x2000U)
#define RTL8201F_ANAR_PAUSE_OPERATION      ((uint16_t)0x0C00U)
#define RTL8201F_ANAR_PO_NOPAUSE           ((uint16_t)0x0000U)
#define RTL8201F_ANAR_PO_SYMMETRIC_PAUSE   ((uint16_t)0x0400U)
#define RTL8201F_ANAR_PO_ASYMMETRIC_PAUSE  ((uint16_t)0x0800U)
#define RTL8201F_ANAR_PO_ADVERTISE_SUPPORT ((uint16_t)0x0C00U)
#define RTL8201F_ANAR_100BASE_T4           ((uint16_t)0x0200U)
#define RTL8201F_ANAR_100BASE_TX_FD        ((uint16_t)0x0100U)
#define RTL8201F_ANAR_100BASE_TX           ((uint16_t)0x0080U)
#define RTL8201F_ANAR_10BASE_T_FD          ((uint16_t)0x0040U)
#define RTL8201F_ANAR_10BASE_T             ((uint16_t)0x0020U)
#define RTL8201F_ANAR_SELECTOR_FIELD       ((uint16_t)0x000FU)

#define RTL8201F_ANLPAR ((uint16_t)0x05U)
#define RTL8201F_ANLPAR_NEXT_PAGE            ((uint16_t)0x8000U)
#define RTL8201F_ANLPAR_REMOTE_FAULT         ((uint16_t)0x2000U)
#define RTL8201F_ANLPAR_PAUSE_OPERATION      ((uint16_t)0x0C00U)
#define RTL8201F_ANLPAR_PO_NOPAUSE           ((uint16_t)0x0000U)
#define RTL8201F_ANLPAR_PO_SYMMETRIC_PAUSE   ((uint16_t)0x0400U)
#define RTL8201F_ANLPAR_PO_ASYMMETRIC_PAUSE  ((uint16_t)0x0800U)
#define RTL8201F_ANLPAR_PO_ADVERTISE_SUPPORT ((uint16_t)0x0C00U)
#define RTL8201F_ANLPAR_100BASE_T4           ((uint16_t)0x0200U)
#define RTL8201F_ANLPAR_100BASE_TX_FD        ((uint16_t)0x0100U)
#define RTL8201F_ANLPAR_100BASE_TX           ((uint16_t)0x0080U)
#define RTL8201F_ANLPAR_10BASE_T_FD          ((uint16_t)0x0040U)
#define RTL8201F_ANLPAR_10BASE_T             ((uint16_t)0x0020U)
#define RTL8201F_ANLPAR_SELECTOR_FIELD       ((uint16_t)0x000FU)

#define RTL8201F_ANER ((uint16_t)0x06U)
#define RTL8201F_ANER_PARALLEL_DETECT_FAULT  ((uint16_t)0x0010U)
#define RTL8201F_ANER_LP_NP_ABLE             ((uint16_t)0x0008U)
#define RTL8201F_ANER_NP_ABLE                ((uint16_t)0x0004U)
#define RTL8201F_ANER_PAGE_RECEIVED          ((uint16_t)0x0002U)
#define RTL8201F_ANER_LP_AUTONEG_ABLE        ((uint16_t)0x0001U)

/* ===== MMD Access Control & Data Register ===== */

#define RTL8201F_MMDACR  ((uint16_t)0x0DU)
#define RTL8201F_MMDACR_MMD_FUNCTION      ((uint16_t)0xC000U)
#define RTL8201F_MMDACR_MMD_FUNCTION_ADDR ((uint16_t)0x0000U)
#define RTL8201F_MMDACR_MMD_FUNCTION_DATA ((uint16_t)0x4000U)
#define RTL8201F_MMDACR_MMD_DEV_ADDR      ((uint16_t)0x001FU)

#define RTL8201F_MMDAADR ((uint16_t)0x0EU)

/* ===== Power Saving Mode Register ===== */

#define RTL8201F_PSM  ((uint16_t)0x18U)
#define RTL8201F_PSM_MASK     ((uint16_t)0x8000U)
#define RTL8201F_PSM_SET_PSM  ((uint16_t)0x8000U)

/* ===== Page Setting Register ===== */

#define RTL8201F_PSR ((uint16_t)0x1FU)
#define RTL8201F_PSR_MASK ((uint16_t)0x00FFU)
#define RTL8201F_PSR_P0   ((uint16_t)0x0000U)
#define RTL8201F_PSR_P7   ((uint16_t)0x0007U)

/* ===== Page 7: RMII Mode Setting Register ===== */

#define RTL8201F_P7_RMSR ((uint16_t)0x10U)
#define RTL8201F_P7_RMSR_RMII_MODE_MASK ((uint16_t)0x0008U)
#define RTL8201F_P7_RMSR_TX_TIMING_MASK ((uint16_t)0x0F00U)
#define RTL8201F_P7_RMSR_RX_TIMING_MASK ((uint16_t)0x00F0U)
#define RTL8201F_P7_RMSR_SET_RMII_MODE  ((uint16_t)0x0008U)
#define RTL8201F_P7_RMSR_TX_TIMING      ((uint16_t)0x0500U)
#define RTL8201F_P7_RMSR_RX_TIMING      ((uint16_t)0x0040U)

/* ===== Page 7: LED Mode Setting Register ===== */

#define RTL8201F_P7_LED ((uint16_t)0x13U)
#define RTL8201F_P7_LED_MASK             ((uint16_t)0x0030U)
#define RTL8201F_P7_LED_ACK_ALL_LINK_100 ((uint16_t)0x0000U)
#define RTL8201F_P7_LED_ACT_LINK_10_100  ((uint16_t)0x0030U)

#endif /* RTL8201F_REGMAP_H */
