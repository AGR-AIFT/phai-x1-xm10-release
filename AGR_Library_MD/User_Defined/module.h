#ifndef USER_DEFINED_MODULE_H_
#define USER_DEFINED_MODULE_H_

/* ------------------- Module Enable/Disable ------------------- */
//******* IF CM *******//
//#define WALKON5_CM_ENABLED
// #define L30_CM_ENABLED
//#define SUIT_MINICM_ENABLED

//******* IF MD *******//
//#define L30_MD_REV06_ENABLED
// #define L30_MD_REV07_ENABLED
//#define L30_MD_REV08_ENABLED
//#define SUIT_MD_ENABLED

//******* IF WIDM *******//
// #define WIDM_ENABLED

//******* IF GRFB *******//
//#define KAIST_GRF_BOARD_ENABLED
//#define GRF_LEFT
//#define GRF_RIGHT

//******* IF EXPANSION_BOARD *******//
#define EXPANSION_BOARD_ENABLED

/* ------------------- Select CM or MD or WIDM For WIDM(Gait Ctrl) ------------------- */
#if defined(WALKON5_CM_ENABLED) || defined(L30_CM_ENABLED) || defined(SUIT_MINICM_ENABLED)
#define CM_MODULE
#endif
#if defined(L30_MD_REV06_ENABLED) || defined(L30_MD_REV07_ENABLED) || defined(L30_MD_REV08_ENABLED) || defined(SUIT_MD_ENABLED)
#define MD_MODULE
#endif
#if defined(WIDM_ENABLED)
#define WIDM_MODULE
#endif

/* ------------------- WALKON5_CM_ENABLED ------------------- */
#ifdef WALKON5_CM_ENABLED

#define _USE_OS_RTOS_BSP
#define _USE_OS_RTOS
#define _USE_CMSISV1
#define _USE_SEMAPHORE
#define IOIF_LTC2944_ENABLED
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_NZRLED_ENABLED
#define IOIF_RMB30SC_ENABLED
#define IOIF_THINPOT_LS_ENABLED
#define IOIF_TB67H450FNGEL_ENABLED
#define IOIF_BUZZER_ENABLED
#define IOIF_BATTERYLED_ENABLED
#define BUS_VOLTAGE 48
#define VBUS2DUTY_RATIO 100
#define FATFS_USB_ENABLE

#endif /* WALKON5_CM_ENABLED */

/* ------------------- L30_CM_ENABLED ------------------- */
#ifdef L30_CM_ENABLED

#define _USE_OS_RTOS_BSP
#define _USE_OS_RTOS
#define _USE_CMSISV1
#define _USE_SEMAPHORE
#define IOIF_LTC2944_ENABLED
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_NZRLED_ENABLED
#define IOIF_RMB30SC_ENABLED
#define IOIF_THINPOT_LS_ENABLED
#define IOIF_TB67H450FNGEL_ENABLED
#define IOIF_BUZZER_ENABLED
#define IOIF_BATTERYLED_ENABLED
#define IOIF_FATFS_ENABLED
#define BUS_VOLTAGE 48
#define VBUS2DUTY_RATIO 100
#define FATFS_USB_ENABLE

#endif /* L30_CM_ENABLED */

/* ------------------- SUIT_MINICM_ENABLED ------------------- */
#ifdef SUIT_MINICM_ENABLED

#define SUIT_H10
// #define SUIT_K10
// #define SUIT_A10
#define _USE_OS_RTOS_BSP
#define _USE_OS_RTOS
#define _USE_CMSISV2
#define _USE_SEMAPHORE
#define IOIF_LTC2944_ENABLED
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_PCA9957HNMP_ENABLED
#define IOIF_MDBT42Q_AT_ENABLED
#define IOIF_FATFS_ENABLED
#define IOIF_AUDIO_WAV_SAI_ENABLED
#define BUS_VOLTAGE 24
#define VBUS2DUTY_RATIO 200
#define FATFS_SD_ENABLE
#define FATFS_USB_ENABLE

#endif /* SUIT_MINICM_ENABLED */

/* ------------------- L30_MD_REV06_ENABLED ------------------- */
#ifdef L30_MD_REV06_ENABLED

#define _USE_BAREMETAL
#define IOIF_LTC2944_ENABLED
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_TMCS1100A2_ENABLED
#define IOIF_FSR_SZH_HWS004_ENABLED
#define IOIF_LINEAR_POTEN_ENABLED
#define IOIF_503NTC_ENABLED
#define IOIF_THINPOT_LS_ENABLED
#define IOIF_TB67H450FNGEL_ENABLED
#define IOIF_RMB20SC_ENABLED
#define IOIF_RMB20IC_ENABLED
#define IOIF_NZRLED_ENABLED
#define BUS_VOLTAGE 48
#define VBUS2DUTY_RATIO 100

#endif /* L30_MD_REV06_ENABLED */

/* ------------------- L30_MD_REV07_ENABLED ------------------- */
#ifdef L30_MD_REV07_ENABLED

#define _USE_BAREMETAL
#define IOIF_LTC2944_ENABLED
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_TMCS1100A2_ENABLED
#define IOIF_FSR_SZH_HWS004_ENABLED
#define IOIF_LINEAR_POTEN_ENABLED   
#define IOIF_503NTC_ENABLED
#define IOIF_THINPOT_LS_ENABLED
#define IOIF_TB67H450FNGEL_ENABLED
#define IOIF_RMB20SC_ENABLED
#define IOIF_RMB20IC_ENABLED
#define IOIF_NZRLED_ENABLED
#define BUS_VOLTAGE 48
#define VBUS2DUTY_RATIO 100

#endif /* L30_MD_REV07_ENABLED */

/* ------------------- L30_MD_REV08_ENABLED ------------------- */
#ifdef L30_MD_REV08_ENABLED

#define _USE_BAREMETAL
#define IOIF_LTC2944_ENABLED
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_TMCS1100A2_ENABLED
#define IOIF_FSR_SZH_HWS004_ENABLED
#define IOIF_LINEAR_POTEN_ENABLED   
#define IOIF_503NTC_ENABLED
#define IOIF_THINPOT_LS_ENABLED
#define IOIF_TB67H450FNGEL_ENABLED
#define IOIF_RMB20SC_ENABLED
#define IOIF_RMB20IC_ENABLED
#define IOIF_NZRLED_ENABLED
#define BUS_VOLTAGE 48
#define VBUS2DUTY_RATIO 100

#endif /* L30_MD_REV08_ENABLED */

/* ------------------- SUIT_MD_ENABLED ------------------- */
#ifdef SUIT_MD_ENABLED

#define _USE_BAREMETAL
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_TMCS1100A3_ENABLED
#define IOIF_503NTC_ENABLED
#define IOIF_GRFSENSOR_ENABLED
#define IOIF_RMB20SC_ENABLED
#define IOIF_RMB20IC_ENABLED
#define BUS_VOLTAGE 24
#define VBUS2DUTY_RATIO 200

#endif /* SUIT_MD_ENABLED */

/* ------------------- WIDM_ENABLED ------------------- */
#ifdef WIDM_ENABLED

#define _USE_BAREMETAL
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED

#endif /* WIDM_ENABLED */

/* ------------------- GRFB_ENABLED ------------------- */
#ifdef KAIST_GRF_BOARD_ENABLED

#define _USE_BAREMETAL
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_GRFSENSOR_ENABLED

#endif /* GRFBoard_ENABLED */

/* ------------------- EXPANSION_BOARD_ENABLED ------------------- */
#ifdef EXPANSION_BOARD_ENABLED

#define _USE_BAREMETAL
#define IOIF_BM1422AGMV_ENABLED
#define IOIF_ICM20608G_ENABLED
#define IOIF_GRFSENSOR_ENABLED
#define IOIF_PMMGSENSOR_ENABLED
#define IOIF_RA30P_ENABLED
#define IOIF_PSL_IEMG2_ENABLED
//#define IOIF_EBIMU_9DOFV5_R3_ENABLED
#define IOIF_MTI630_ENABLED
#define IOIF_SM1205C_ENABLED

/* Select only ONE */
//#define FDCAN_WALKONSUIT_ENABLED
#define FDCAN_H10_ENABLED


/*------ Removed code after code generation ------*/
// SUBSTITUTE BELOW CODE IN "fdcan.c" after every .ioc update //

//#ifdef FDCAN_WALKONSUIT_ENABLED
//  hfdcan1.Init.NominalPrescaler = 1;
//  hfdcan1.Init.NominalSyncJumpWidth = 32;
//  hfdcan1.Init.NominalTimeSeg1 = 127;
//  hfdcan1.Init.NominalTimeSeg2 = 32;c
//#endif
//#ifdef FDCAN_H10_ENABLED
//  hfdcan1.Init.NominalPrescaler = 10;
//  hfdcan1.Init.NominalSyncJumpWidth = 2;
//  hfdcan1.Init.NominalTimeSeg1 = 5;
//  hfdcan1.Init.NominalTimeSeg2 = 2;
//#endif

#endif /* EXPANSION_BOARD_ENABLED */

#endif /* USER_DEFINED_MODULE_H_ */
