/**
 ******************************************************************************
 * @file    agr_od_comm_profile.h
 * @author  HyundoKim
 * @brief   CANopen Communication Profile - Mandatory OD Definitions
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * CiA 301 Communication Profile 필수 Object Dictionary 정의입니다.
 *
 * [필수 OD 목록]
 * - 0x1000: Device Type
 * - 0x1001: Error Register
 * - 0x1017: Producer Heartbeat Time
 * - 0x1018: Identity Object (Vendor ID, Product Code, Revision, Serial)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_OD_COMM_PROFILE_H
#define AGR_OD_COMM_PROFILE_H

#include "agr_dop_types.h"

/**
 *-----------------------------------------------------------
 * COMMUNICATION PROFILE OD INDICES (CiA 301 Mandatory)
 *-----------------------------------------------------------
 */

/** @brief 0x1000 - Device Type (UNSIGNED32, RO) */
#define AGR_OD_IDX_DEVICE_TYPE              0x1000

/** @brief 0x1001 - Error Register (UNSIGNED8, RO) */
#define AGR_OD_IDX_ERROR_REGISTER           0x1001

/** @brief 0x1017 - Producer Heartbeat Time (UNSIGNED16, RW, unit: ms) */
#define AGR_OD_IDX_HEARTBEAT_TIME           0x1017

/** @brief 0x1018 - Identity Object (RECORD) */
#define AGR_OD_IDX_IDENTITY                 0x1018

/**
 *-----------------------------------------------------------
 * 0x1018 IDENTITY OBJECT SUB-INDICES
 *-----------------------------------------------------------
 */

#define AGR_OD_SUB_IDENTITY_COUNT           0x00  /**< Number of entries (UNSIGNED8, RO) = 4 */
#define AGR_OD_SUB_VENDOR_ID                0x01  /**< Vendor ID (UNSIGNED32, RO) */
#define AGR_OD_SUB_PRODUCT_CODE             0x02  /**< Product Code (UNSIGNED32, RO) */
#define AGR_OD_SUB_REVISION_NUMBER          0x03  /**< Revision Number (UNSIGNED32, RO) */
#define AGR_OD_SUB_SERIAL_NUMBER            0x04  /**< Serial Number (UNSIGNED32, RO) */

/**
 *-----------------------------------------------------------
 * ERROR REGISTER BIT DEFINITIONS (0x1001)
 *-----------------------------------------------------------
 */

#define AGR_ERR_REG_GENERIC                 (1U << 0)  /**< Bit 0: Generic error */
#define AGR_ERR_REG_CURRENT                 (1U << 1)  /**< Bit 1: Current */
#define AGR_ERR_REG_VOLTAGE                 (1U << 2)  /**< Bit 2: Voltage */
#define AGR_ERR_REG_TEMPERATURE             (1U << 3)  /**< Bit 3: Temperature */
#define AGR_ERR_REG_COMMUNICATION           (1U << 4)  /**< Bit 4: Communication */
#define AGR_ERR_REG_DEVICE_PROFILE          (1U << 5)  /**< Bit 5: Device profile specific */
#define AGR_ERR_REG_MANUFACTURER            (1U << 7)  /**< Bit 7: Manufacturer specific */

/**
 *-----------------------------------------------------------
 * DEVICE TYPE ENCODING (0x1000)
 *-----------------------------------------------------------
 * Upper 16 bits: Additional information
 * Lower 16 bits: Device Profile number (CiA 4xx)
 *
 * @example AGR_DEVICE_TYPE(0x0000, 402) → 0x00000192 (CiA 402 Drive)
 */
#define AGR_DEVICE_TYPE(additional, profile) \
    ((uint32_t)(((uint32_t)(additional) << 16) | ((uint32_t)(profile) & 0xFFFF)))

/**
 *-----------------------------------------------------------
 * ANGEL ROBOTICS VENDOR ID
 *-----------------------------------------------------------
 */
#define AGR_VENDOR_ID                       0x414E474C  /**< "ANGL" in ASCII */

/**
 *-----------------------------------------------------------
 * COMM PROFILE OD ENTRY HELPER MACROS
 *-----------------------------------------------------------
 * @brief Communication Profile OD Entry를 빠르게 선언하는 매크로
 *
 * @example
 * ```c
 * static uint32_t s_device_type  = AGR_DEVICE_TYPE(0, 402);
 * static uint8_t  s_error_reg    = 0;
 * static uint16_t s_hb_time_ms   = 1000;
 * static uint8_t  s_id_count     = 4;
 * static uint32_t s_vendor_id    = AGR_VENDOR_ID;
 * static uint32_t s_product_code = 0x00010001;
 * static uint32_t s_revision     = 0x00030000;
 * static uint32_t s_serial       = 0x00000001;
 *
 * static const AGR_OD_Entry_t s_comm_od[] = {
 *     AGR_OD_COMM_DEVICE_TYPE(&s_device_type),
 *     AGR_OD_COMM_ERROR_REGISTER(&s_error_reg),
 *     AGR_OD_COMM_HEARTBEAT_TIME(&s_hb_time_ms),
 *     AGR_OD_COMM_IDENTITY_COUNT(&s_id_count),
 *     AGR_OD_COMM_VENDOR_ID(&s_vendor_id),
 *     AGR_OD_COMM_PRODUCT_CODE(&s_product_code),
 *     AGR_OD_COMM_REVISION(&s_revision),
 *     AGR_OD_COMM_SERIAL(&s_serial),
 * };
 * ```
 */

/** @brief 0x1000 Device Type (UINT32, RO) */
#define AGR_OD_COMM_DEVICE_TYPE(ptr) \
    { AGR_OD_IDX_DEVICE_TYPE, 0x00, AGR_TYPE_UINT32, 4, AGR_ACCESS_RO, \
      (void*)(ptr), NULL, "device_type", "" }

/** @brief 0x1001 Error Register (UINT8, RO) */
#define AGR_OD_COMM_ERROR_REGISTER(ptr) \
    { AGR_OD_IDX_ERROR_REGISTER, 0x00, AGR_TYPE_UINT8, 1, AGR_ACCESS_RO, \
      (void*)(ptr), NULL, "error_register", "" }

/** @brief 0x1017 Producer Heartbeat Time (UINT16, RW) */
#define AGR_OD_COMM_HEARTBEAT_TIME(ptr) \
    { AGR_OD_IDX_HEARTBEAT_TIME, 0x00, AGR_TYPE_UINT16, 2, AGR_ACCESS_RW, \
      (void*)(ptr), NULL, "heartbeat_time", "ms" }

/** @brief 0x1018 sub 0 - Number of entries (UINT8, RO) */
#define AGR_OD_COMM_IDENTITY_COUNT(ptr) \
    { AGR_OD_IDX_IDENTITY, AGR_OD_SUB_IDENTITY_COUNT, AGR_TYPE_UINT8, 1, AGR_ACCESS_RO, \
      (void*)(ptr), NULL, "identity_count", "" }

/** @brief 0x1018 sub 1 - Vendor ID (UINT32, RO) */
#define AGR_OD_COMM_VENDOR_ID(ptr) \
    { AGR_OD_IDX_IDENTITY, AGR_OD_SUB_VENDOR_ID, AGR_TYPE_UINT32, 4, AGR_ACCESS_RO, \
      (void*)(ptr), NULL, "vendor_id", "" }

/** @brief 0x1018 sub 2 - Product Code (UINT32, RO) */
#define AGR_OD_COMM_PRODUCT_CODE(ptr) \
    { AGR_OD_IDX_IDENTITY, AGR_OD_SUB_PRODUCT_CODE, AGR_TYPE_UINT32, 4, AGR_ACCESS_RO, \
      (void*)(ptr), NULL, "product_code", "" }

/** @brief 0x1018 sub 3 - Revision Number (UINT32, RO) */
#define AGR_OD_COMM_REVISION(ptr) \
    { AGR_OD_IDX_IDENTITY, AGR_OD_SUB_REVISION_NUMBER, AGR_TYPE_UINT32, 4, AGR_ACCESS_RO, \
      (void*)(ptr), NULL, "revision", "" }

/** @brief 0x1018 sub 4 - Serial Number (UINT32, RO) */
#define AGR_OD_COMM_SERIAL(ptr) \
    { AGR_OD_IDX_IDENTITY, AGR_OD_SUB_SERIAL_NUMBER, AGR_TYPE_UINT32, 4, AGR_ACCESS_RO, \
      (void*)(ptr), NULL, "serial_number", "" }

#endif /* AGR_OD_COMM_PROFILE_H */
