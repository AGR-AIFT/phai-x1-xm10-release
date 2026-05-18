/**
 * @file  agr_mw_conf.h
 * @brief  AGR_MW Configuration — XM (Extension Module, STM32H743XI)
 * @details
 *   AGR_MW submodule configuration for XM module.
 *   Pattern identical to ioif_conf.h — placed in System/Config/.
 *
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#ifndef AGR_MW_CONF_H_
#define AGR_MW_CONF_H_

/* -----------------------------------------------------------------------
 * MCU Series Auto-Detection (from CubeMX-generated defines)
 * ----------------------------------------------------------------------- */
#if defined(STM32H743xx) || defined(STM32H750xx)
    #define AGR_MW_MCU_SERIES_H7    1
#elif defined(STM32G474xx) || defined(STM32G431xx)
    #define AGR_MW_MCU_SERIES_G4    1
#else
    #error "AGR_MW: Unsupported MCU — add detection in agr_mw_conf.h"
#endif

/* -----------------------------------------------------------------------
 * Module Enable/Disable (XM — STM32H743XI)
 * ----------------------------------------------------------------------- */

/** BOOT — App-side bootloader integration (ConfirmBoot, RequestUpdate)
 *  XM has AGR_BOOT V2 bootloader deployed. */
#define AGR_MW_BOOT_ENABLE

/* -----------------------------------------------------------------------
 * DOP Transport Selection (XM Rev2.0 — CAN-FD + UDP + SERIAL)
 * -----------------------------------------------------------------------
 * agr_dop_config.h가 이 파일을 include하므로 #ifndef 가드보다 먼저 적용.
 * - CAN-FD: IMU Hub, EMG Hub, FES Hub (PnP Master)
 * - UDP:    AM(Jetson) ↔ XM Ethernet 통신 (Rev2.0 전용)
 * - SERIAL: USB-CDC 위 sensor-studio GUI (HW 양산/SI 검증, 0x7000 Test Mode)
 */
#define AGR_DOP_TRANSPORT_CANFD     1
#define AGR_DOP_TRANSPORT_UDP       1
#define AGR_DOP_TRANSPORT_SERIAL    1

/* -----------------------------------------------------------------------
 * API Contract — DO NOT MODIFY
 * -----------------------------------------------------------------------
 * These definitions implement the AGR_MW error return contract.
 * See: docs/agr_mw_error_contract.md
 *
 * Convention:
 *   int32_t rc = AGR_Module_DoThing(...);
 *   if (!AGR_IsOk(rc)) { ... handle error ... }
 *
 *   rc == 0 : OK
 *   rc <  0 : error (module-local typed enum negative values)
 *   rc >  0 : reserved for future "ok-with-info"
 *
 * This macro must stay identical across all consumer modules. When syncing
 * this template into your System/Config/agr_mw_conf.h, preserve this block
 * verbatim.
 * ----------------------------------------------------------------------- */
#define AGR_IsOk(rc)  ((int32_t)(rc) >= 0)

#endif /* AGR_MW_CONF_H_ */
