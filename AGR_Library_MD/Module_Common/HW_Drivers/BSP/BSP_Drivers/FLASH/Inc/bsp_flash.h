/**
 * @file bsp_flash.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for FLASH functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef FLASH_INC_BSP_FLASH_H_
#define FLASH_INC_BSP_FLASH_H_

#include "main.h"
#include "module.h"

/** @defgroup FLASH FLASH
  * @brief FLASH HAL BSP module driver
  * @
  */
#ifdef HAL_FLASH_MODULE_ENABLED
#define BSP_FLASH_MODULE_ENABLED

#include "bsp_common.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/** @defgroup FLASHEx_Type_Erase FLASH Type Erase
  * @{
  */
//#define BSP_FLASH_TYPEERASE_SECTORS FLASH_TYPEERASE_SECTORS      ///< 0x00U  /*!< Sectors erase only          */
#define BSP_FLASH_TYPEERASE_MASSERASE FLASH_TYPEERASE_MASSERASE  ///< 0x01U  /*!< Flash Mass erase activation */

#if defined (FLASH_CR_PSIZE)
/** @defgroup FLASHEx_Voltage_Range FLASH Voltage Range
  * @{
  */
#define BSP_FLASH_VOLTAGE_RANGE_1 FLASH_VOLTAGE_RANGE_1   ///< 0x00000000U       /*!< Flash program/erase by 8 bits  */
#define BSP_FLASH_VOLTAGE_RANGE_2 FLASH_VOLTAGE_RANGE_2   ///< FLASH_CR_PSIZE_0  /*!< Flash program/erase by 16 bits */
#define BSP_FLASH_VOLTAGE_RANGE_3 FLASH_VOLTAGE_RANGE_3   ///< FLASH_CR_PSIZE_1  /*!< Flash program/erase by 32 bits */
#define BSP_FLASH_VOLTAGE_RANGE_4 FLASH_VOLTAGE_RANGE_4   ///< FLASH_CR_PSIZE    /*!< Flash program/erase by 64 bits */
#endif /* FLASH_CR_PSIZE */

/** @defgroup FLASHEx_Banks FLASH Banks
  * @{
  */
#define BSP_FLASH_BANK_1 FLASH_BANK_1       ///< 0x01U /*!< Bank 1   */
//#if defined (DUAL_BANK)
#define BSP_FLASH_BANK_2 FLASH_BANK_2       ///< 0x02U /*!< Bank 2   */
#define BSP_FLASH_BANK_BOTH FLASH_BANK_BOTH       ///< (FLASH_BANK_1 | FLASH_BANK_2) /*!< Bank1 and Bank2 */
//#endif /* DUAL_BANK */

/** @defgroup FLASH_Type_Program FLASH Type Program
  * @{
  */
#define BSP_FLASH_TYPEPROGRAM_FLASHWORD FLASH_TYPEPROGRAM_FLASHWORD  ///< 0x01U /*!< Program a flash word at a specified address */
#if defined (FLASH_OPTCR_PG_OTP)
#define BSP_FLASH_TYPEPROGRAM_OTPWORD FLASH_TYPEPROGRAM_OTPWORD      ///< 0x02U /*!< Program an OTP word at a specified address  */
#endif /* FLASH_OPTCR_PG_OTP */

/** @defgroup FLASH_Error_Code FLASH Error Code
  * @brief    FLASH Error Code
  * @{
  */
#define BSP_FLASH_ERROR_NONE        HAL_FLASH_ERROR_NONE               ///< No error set
#define BSP_FLASH_ERROR_WRP_BANK1   HAL_FLASH_ERROR_WRP_BANK1     ///< Write Protection Error on Bank 1
#define BSP_FLASH_ERROR_PGS_BANK1   HAL_FLASH_ERROR_PGS_BANK1     ///< Program Sequence Error on Bank 1
#define BSP_FLASH_ERROR_STRB_BANK1  HAL_FLASH_ERROR_STRB_BANK1   ///< Strobe Error on Bank 1
#define BSP_FLASH_ERROR_INC_BANK1   HAL_FLASH_ERROR_INC_BANK1     ///< Inconsistency Error on Bank 1
#if defined (FLASH_SR_OPERR)
#define BSP_HAL_FLASH_ERROR_OPE_BANK1 HAL_FLASH_ERROR_OPE_BANK1     ///< Operation Error on Bank 1
#endif /* FLASH_SR_OPERR */
#define BSP_FLASH_ERROR_RDP_BANK1   HAL_FLASH_ERROR_RDP_BANK1     ///< Read Protection Error on Bank 1
#define BSP_FLASH_ERROR_RDS_BANK1   HAL_FLASH_ERROR_RDS_BANK1     ///< Read Secured Error on Bank 1
#define BSP_FLASH_ERROR_SNECC_BANK1 HAL_FLASH_ERROR_SNECC_BANK1 ///< ECC Single Correction Error on Bank 1
#define BSP_FLASH_ERROR_DBECC_BANK1 HAL_FLASH_ERROR_DBECC_BANK1 ///< ECC Double Detection Error on Bank 1
#define BSP_FLASH_ERROR_CRCRD_BANK1 HAL_FLASH_ERROR_CRCRD_BANK1 ///< CRC Read Error on Bank 1
#define BSP_FLASH_ERROR_WRP_BANK2   HAL_FLASH_ERROR_WRP_BANK2     ///< Write Protection Error on Bank 2
#define BSP_FLASH_ERROR_PGS_BANK2   HAL_FLASH_ERROR_PGS_BANK2     ///< Program Sequence Error on Bank 2
#define BSP_FLASH_ERROR_STRB_BANK2  HAL_FLASH_ERROR_STRB_BANK2   ///< Strobe Error on Bank 2
#define BSP_FLASH_ERROR_INC_BANK2   HAL_FLASH_ERROR_INC_BANK2     ///< Inconsistency Error on Bank 2
#if defined (FLASH_SR_OPERR)
#define BSP_HAL_FLASH_ERROR_OPE_BANK2 HAL_FLASH_ERROR_OPE_BANK2     ///< Operation Error on Bank 2
#endif /* FLASH_SR_OPERR */
#define BSP_FLASH_ERROR_RDP_BANK2   HAL_FLASH_ERROR_RDP_BANK2     ///< Read Protection Error on Bank 2
#define BSP_FLASH_ERROR_RDS_BANK2   HAL_FLASH_ERROR_RDS_BANK2     ///< Read Secured Error on Bank 2
#define BSP_FLASH_ERROR_SNECC_BANK2 HAL_FLASH_ERROR_SNECC_BANK2 ///< SNECC Error on Bank 2
#define BSP_FLASH_ERROR_DBECC_BANK2 HAL_FLASH_ERROR_DBECC_BANK2 ///< Double Detection ECC on Bank 2
#define BSP_FLASH_ERROR_CRCRD_BANK2 HAL_FLASH_ERROR_CRCRD_BANK2 ///< CRC Read Error on Bank 2

/** @defgroup FLASH_Flag_definition FLASH Flag definition
  * @brief Flag definition
  * @{
  */
#define BSP_FLASH_FLAG_BSY      FLASH_FLAG_BSY             ///< FLASH_SR_BSY !< FLASH Busy flag
#define BSP_FLASH_FLAG_WBNE     FLASH_FLAG_WBNE            ///< FLASH_SR_WBNE !< Write Buffer Not Empty flag
#define BSP_FLASH_FLAG_QW       FLASH_FLAG_QW              ///< FLASH_SR_QW !< Wait Queue on flag
#define BSP_FLASH_FLAG_CRC_BUSY FLASH_FLAG_CRC_BUSY        ///< FLASH_SR_CRC_BUSY !< CRC Busy flag
#define BSP_FLASH_FLAG_EOP      FLASH_FLAG_EOP             ///< FLASH_SR_EOP !< End Of Program on flag
#define BSP_FLASH_FLAG_OPERR    FLASH_FLAG_OPERR
#define BSP_FLASH_FLAG_WRPERR   FLASH_FLAG_WRPERR          ///< FLASH_SR_WRPERR !< Write Protection Error on flag
#define BSP_FLASH_FLAG_PGSERR   FLASH_FLAG_PGSERR          ///< FLASH_SR_PGSERR !< Program Sequence Error on flag
#define BSP_FLASH_FLAG_STRBERR  FLASH_FLAG_STRBERR         ///< FLASH_SR_STRBERR !< Strobe Error flag
#define BSP_FLASH_FLAG_INCERR   FLASH_FLAG_INCERR          ///< FLASH_SR_INCERR !< Inconsistency Error on flag
#if defined (FLASH_SR_OPERR)
#define BSP_ FLASH_FLAG_OPERR                 ///< FLASH_SR_OPERR !< Operation Error on flag
#endif /* FLASH_SR_OPERR */
#define BSP_FLASH_FLAG_RDPERR   FLASH_FLAG_RDPERR          ///< FLASH_SR_RDPERR !< Read Protection Error on flag
#define BSP_FLASH_FLAG_RDSERR   FLASH_FLAG_RDSERR          ///< FLASH_SR_RDSERR !< Read Secured Error on flag
#define BSP_FLASH_FLAG_SNECCERR FLASH_FLAG_SNECCERR        ///< FLASH_SR_SNECCERR !< Single ECC Error Correction on flag
#define BSP_FLASH_FLAG_DBECCERR FLASH_FLAG_DBECCERR        ///< FLASH_SR_DBECCERR !< Double Detection ECC Error on flag
#define BSP_FLASH_FLAG_CRCEND   FLASH_FLAG_CRCEND          ///< FLASH_SR_CRCEND !< CRC End of Calculation flag
#define BSP_FLASH_FLAG_CRCRDERR FLASH_FLAG_CRCRDERR        ///< FLASH_SR_CRCRDERR !< CRC Read Error on bank flag

#define BSP_FLASH_FLAG_BSY_BANK1      FLASH_FLAG_BSY_BANK1       ///< FLASH_SR_BSY !< FLASH Bank 1 Busy flag
#define BSP_FLASH_FLAG_WBNE_BANK1     FLASH_FLAG_WBNE_BANK1      ///< FLASH_SR_WBNE !< Write Buffer Not Empty on Bank 1 flag
#define BSP_FLASH_FLAG_QW_BANK1       FLASH_FLAG_QW_BANK1        ///< FLASH_SR_QW !< Wait Queue on Bank 1 flag
#define BSP_FLASH_FLAG_CRC_BUSY_BANK1 FLASH_FLAG_CRC_BUSY_BANK1  ///< FLASH_SR_CRC_BUSY !< CRC Busy on Bank 1 flag
#define BSP_FLASH_FLAG_EOP_BANK1      FLASH_FLAG_EOP_BANK1       ///< FLASH_SR_EOP !< End Of Program on Bank 1 flag
#define BSP_FLASH_FLAG_WRPERR_BANK1   FLASH_FLAG_WRPERR_BANK1    ///< FLASH_SR_WRPERR !< Write Protection Error on Bank 1 flag
#define BSP_FLASH_FLAG_PGSERR_BANK1   FLASH_FLAG_PGSERR_BANK1    ///< FLASH_SR_PGSERR !< Program Sequence Error on Bank 1 flag
#define BSP_FLASH_FLAG_STRBERR_BANK1  FLASH_FLAG_STRBERR_BANK1   ///< FLASH_SR_STRBERR !< Strobe Error on Bank 1 flag
#define BSP_FLASH_FLAG_INCERR_BANK1   FLASH_FLAG_INCERR_BANK1    ///< FLASH_SR_INCERR !< Inconsistency Error on Bank 1 flag
#if defined (FLASH_SR_OPERR)
#define BSP_FLASH_FLAG_OPERR_BANK1    FLASH_FLAG_OPERR_BANK1     ///< FLASH_SR_OPERR !< Operation Error on Bank 1 flag
#endif /* FLASH_SR_OPERR */
#define BSP_FLASH_FLAG_RDPERR_BANK1   FLASH_FLAG_RDPERR_BANK1    ///< FLASH_SR_RDPERR !< Read Protection Error on Bank 1 flag
#define BSP_FLASH_FLAG_RDSERR_BANK1   FLASH_FLAG_RDSERR_BANK1    ///< FLASH_SR_RDSERR !< Read Secured Error on Bank 1 flag
#define BSP_FLASH_FLAG_SNECCERR_BANK1 FLASH_FLAG_SNECCERR_BANK1  ///< FLASH_SR_SNECCERR !< Single ECC Error Correction on Bank 1 flag
#define BSP_FLASH_FLAG_DBECCERR_BANK1 FLASH_FLAG_DBECCERR_BANK1  ///< FLASH_SR_DBECCERR !< Double Detection ECC Error on Bank 1 flag
#define BSP_FLASH_FLAG_CRCEND_BANK1   FLASH_FLAG_CRCEND_BANK1    ///< FLASH_SR_CRCEND !< CRC End of Calculation on Bank 1 flag
#define BSP_FLASH_FLAG_CRCRDERR_BANK1 FLASH_FLAG_CRCRDERR_BANK1  ///< FLASH_SR_CRCRDERR !< CRC Read error on Bank 1 flag

#if defined (FLASH_SR_OPERR)
#define BSP_FLASH_FLAG_ALL_ERRORS_BANK1 FLASH_FLAG_ALL_ERRORS_BANK1 /* < (FLASH_FLAG_WRPERR_BANK1   | FLASH_FLAG_PGSERR_BANK1   | \
                                                                          FLASH_FLAG_STRBERR_BANK1  | FLASH_FLAG_INCERR_BANK1   | \
                                                                          FLASH_FLAG_OPERR_BANK1    | FLASH_FLAG_RDPERR_BANK1   | \
                                                                          FLASH_FLAG_RDSERR_BANK1   | FLASH_FLAG_SNECCERR_BANK1 | \
                                                                          FLASH_FLAG_DBECCERR_BANK1 | FLASH_FLAG_CRCRDERR_BANK1) */
#else
#define BSP_FLASH_FLAG_ALL_ERRORS_BANK1 FLASH_FLAG_ALL_ERRORS_BANK1 /* < (FLASH_FLAG_WRPERR_BANK1   | FLASH_FLAG_PGSERR_BANK1   | \
                                                                          FLASH_FLAG_STRBERR_BANK1  | FLASH_FLAG_INCERR_BANK1   | \
                                                                          FLASH_FLAG_RDPERR_BANK1   | FLASH_FLAG_RDSERR_BANK1   | \
                                                                          FLASH_FLAG_SNECCERR_BANK1 | FLASH_FLAG_DBECCERR_BANK1 | \
                                                                          FLASH_FLAG_CRCRDERR_BANK1) */
#endif /* FLASH_SR_OPERR */

#define BSP_FLASH_FLAG_ALL_BANK1 FLASH_FLAG_ALL_BANK1  /* < (FLASH_FLAG_BSY_BANK1      | FLASH_FLAG_WBNE_BANK1     | \
                                                             FLASH_FLAG_QW_BANK1       | FLASH_FLAG_CRC_BUSY_BANK1 | \
                                                             FLASH_FLAG_EOP_BANK1      | FLASH_FLAG_CRCEND_BANK1   | \
                                                             FLASH_FLAG_ALL_ERRORS_BANK1) */

#define BSP_FLASH_FLAG_BSY_BANK2      FLASH_FLAG_BSY_BANK2      ///< (FLASH_SR_BSY      | 0x80000000U) !< FLASH Bank 2 Busy flag
#define BSP_FLASH_FLAG_WBNE_BANK2     FLASH_FLAG_WBNE_BANK2     ///< (FLASH_SR_WBNE     | 0x80000000U) !< Write Buffer Not Empty on Bank 2 flag
#define BSP_FLASH_FLAG_QW_BANK2       FLASH_FLAG_QW_BANK2       ///< (FLASH_SR_QW       | 0x80000000U) !< Wait Queue on Bank 2 flag
#define BSP_FLASH_FLAG_CRC_BUSY_BANK2 FLASH_FLAG_CRC_BUSY_BANK2 ///< (FLASH_SR_CRC_BUSY | 0x80000000U) !< CRC Busy on Bank 2 flag
#define BSP_FLASH_FLAG_EOP_BANK2      FLASH_FLAG_EOP_BANK2      ///< (FLASH_SR_EOP      | 0x80000000U) !< End Of Program on Bank 2 flag
#define BSP_FLASH_FLAG_WRPERR_BANK2   FLASH_FLAG_WRPERR_BANK2   ///< (FLASH_SR_WRPERR   | 0x80000000U) !< Write Protection Error on Bank 2 flag
#define BSP_FLASH_FLAG_PGSERR_BANK2   FLASH_FLAG_PGSERR_BANK2   ///< (FLASH_SR_PGSERR   | 0x80000000U) !< Program Sequence Error on Bank 2 flag
#define BSP_FLASH_FLAG_STRBERR_BANK2  FLASH_FLAG_STRBERR_BANK2  ///< (FLASH_SR_STRBERR  | 0x80000000U) !< Strobe Error on Bank 2 flag
#define BSP_FLASH_FLAG_INCERR_BANK2   FLASH_FLAG_INCERR_BANK2   ///< (FLASH_SR_INCERR   | 0x80000000U) !< Inconsistency Error on Bank 2 flag
#if defined (FLASH_SR_OPERR)
#define BSP_FLASH_FLAG_OPERR_BANK2 FLASH_FLAG_OPERR_BANK2           ///< (FLASH_SR_OPERR    | 0x80000000U)        /*!< Operation Error on Bank 2 flag
#endif /* FLASH_SR_OPERR */
#define BSP_FLASH_FLAG_RDPERR_BANK2   FLASH_FLAG_RDPERR_BANK2   ///< (FLASH_SR_RDPERR   | 0x80000000U) !< Read Protection Error on Bank 2 flag
#define BSP_FLASH_FLAG_RDSERR_BANK2   FLASH_FLAG_RDSERR_BANK2   ///< (FLASH_SR_RDSERR   | 0x80000000U) !< Read Secured Error on Bank 2 flag
#define BSP_FLASH_FLAG_SNECCERR_BANK2 FLASH_FLAG_SNECCERR_BANK2 ///< (FLASH_SR_SNECCERR | 0x80000000U) !< Single ECC Error Correction on Bank 2 flag
#define BSP_FLASH_FLAG_DBECCERR_BANK2 FLASH_FLAG_DBECCERR_BANK2 ///< (FLASH_SR_DBECCERR | 0x80000000U) !< Double Detection ECC Error on Bank 2 flag
#define BSP_FLASH_FLAG_CRCEND_BANK2   FLASH_FLAG_CRCEND_BANK2   ///< (FLASH_SR_CRCEND   | 0x80000000U) !< CRC End of Calculation on Bank 2 flag
#define BSP_FLASH_FLAG_CRCRDERR_BANK2 FLASH_FLAG_CRCRDERR_BANK2 ///< (FLASH_SR_CRCRDERR | 0x80000000U) !< CRC Read error on Bank 2 flag

#if defined (FLASH_SR_OPERR)
#define BSP_FLASH_FLAG_ALL_ERRORS_BANK1 FLASH_FLAG_ALL_ERRORS_BANK1 /* < (FLASH_FLAG_WRPERR_BANK1   | FLASH_FLAG_PGSERR_BANK1   | \
                                                                          FLASH_FLAG_STRBERR_BANK1  | FLASH_FLAG_INCERR_BANK1   | \
                                                                          FLASH_FLAG_OPERR_BANK1    | FLASH_FLAG_RDPERR_BANK1   | \
                                                                          FLASH_FLAG_RDSERR_BANK1   | FLASH_FLAG_SNECCERR_BANK1 | \
                                                                          FLASH_FLAG_DBECCERR_BANK1 | FLASH_FLAG_CRCRDERR_BANK1) */
#else
#define BSP_FLASH_FLAG_ALL_ERRORS_BANK2 FLASH_FLAG_ALL_ERRORS_BANK2  /* < (FLASH_FLAG_WRPERR_BANK2   | FLASH_FLAG_PGSERR_BANK2   | \
                                                                           FLASH_FLAG_STRBERR_BANK2  | FLASH_FLAG_INCERR_BANK2   | \
                                                                           FLASH_FLAG_RDPERR_BANK2   | FLASH_FLAG_RDSERR_BANK2   | \
                                                                           FLASH_FLAG_SNECCERR_BANK2 | FLASH_FLAG_DBECCERR_BANK2 | \
                                                                           FLASH_FLAG_CRCRDERR_BANK2) */
#endif /* FLASH_SR_OPERR */

#define BSP_FLASH_FLAG_ALL_BANK2 FLASH_FLAG_ALL_BANK2  /*< (FLASH_FLAG_BSY_BANK2      | FLASH_FLAG_WBNE_BANK2     | \
                                                             FLASH_FLAG_QW_BANK2       | FLASH_FLAG_CRC_BUSY_BANK2 | \
                                                             FLASH_FLAG_EOP_BANK2      | FLASH_FLAG_CRCEND_BANK2   | \
                                                             FLASH_FLAG_ALL_ERRORS_BANK2) */

/**
  * @brief  Clear the specified FLASH flag.
  * @param  __FLAG__: specifies the FLASH flags to clear.
  *    In case of Bank 1, this parameter can be any combination of the following values:
  *     @arg FLASH_FLAG_EOP_BANK1        : End Of Program on Bank 1 flag
  *     @arg FLASH_FLAG_WRPERR_BANK1     : Write Protection Error on Bank 1 flag
  *     @arg FLASH_FLAG_PGSERR_BANK1     : Program Sequence Error on Bank 1 flag
  *     @arg FLASH_FLAG_STRBER_BANK1     : Program Alignment Error on Bank 1 flag
  *     @arg FLASH_FLAG_INCERR_BANK1     : Inconsistency Error on Bank 1 flag
  *     @arg FLASH_FLAG_OPERR_BANK1      : Operation Error on Bank 1 flag
  *     @arg FLASH_FLAG_RDPERR_BANK1     : Read Protection Error on Bank 1 flag
  *     @arg FLASH_FLAG_RDSERR_BANK1     : Read secure  Error on Bank 1 flag
  *     @arg FLASH_FLAG_SNECCE_BANK1     : Single ECC Error Correction on Bank 1 flag
  *     @arg FLASH_FLAG_DBECCE_BANK1     : Double Detection ECC Error on Bank 1 flag
  *     @arg FLASH_FLAG_CRCEND_BANK1     : CRC End on Bank 1 flag
  *     @arg FLASH_FLAG_CRCRDERR_BANK1   : CRC Read error on Bank 1 flag
  *     @arg FLASH_FLAG_ALL_ERRORS_BANK1 : All Bank 1 error flags
  *     @arg FLASH_FLAG_ALL_BANK1        : All Bank 1 flags
  *
  *   In case of Bank 2, this parameter can be any combination of the following values :
  *     @arg FLASH_FLAG_EOP_BANK2        : End Of Program on Bank 2 flag
  *     @arg FLASH_FLAG_WRPERR_BANK2     : Write Protection Error on Bank 2 flag
  *     @arg FLASH_FLAG_PGSERR_BANK2     : Program Sequence Error on Bank 2 flag
  *     @arg FLASH_FLAG_STRBER_BANK2     : Program Alignment Error on Bank 2 flag
  *     @arg FLASH_FLAG_INCERR_BANK2     : Inconsistency Error on Bank 2 flag
  *     @arg FLASH_FLAG_OPERR_BANK2      : Operation Error on Bank 2 flag
  *     @arg FLASH_FLAG_RDPERR_BANK2     : Read Protection Error on Bank 2 flag
  *     @arg FLASH_FLAG_RDSERR_BANK2     : Read secure  Error on Bank 2 flag
  *     @arg FLASH_FLAG_SNECCE_BANK2     : Single ECC Error Correction on Bank 2 flag
  *     @arg FLASH_FLAG_DBECCE_BANK2     : Double Detection ECC Error on Bank 2 flag
  *     @arg FLASH_FLAG_CRCEND_BANK2     : CRC End on Bank 2 flag
  *     @arg FLASH_FLAG_CRCRDERR_BANK2   : CRC Read error on Bank 2 flag
  *     @arg FLASH_FLAG_ALL_ERRORS_BANK2 : All Bank 2 error flags
  *     @arg FLASH_FLAG_ALL_BANK2        : All Bank 2 flags
  * @retval none
  */
#define __BSP_FLASH_CLEAR_FLAG_BANK1(__FLAG__) __HAL_FLASH_CLEAR_FLAG_BANK1(__FLAG__)    ///< WRITE_REG(FLASH->CCR1, (__FLAG__))

#define __BSP_FLASH_CLEAR_FLAG_BANK2(__FLAG__) __HAL_FLASH_CLEAR_FLAG_BANK2(__FLAG__)    ///< WRITE_REG(FLASH->CCR2, ((__FLAG__) & 0x7FFFFFFFU))

#if defined (DUAL_BANK)
#define __BSP_FLASH_CLEAR_FLAG(__FLAG__) __HAL_FLASH_CLEAR_FLAG(__FLAG__) ///< (IS_FLASH_FLAG_BANK1(__FLAG__) ?  __HAL_FLASH_CLEAR_FLAG_BANK1(__FLAG__) : __HAL_FLASH_CLEAR_FLAG_BANK2(__FLAG__))
#else
#define __BSP_FLASH_CLEAR_FLAG(__FLAG__) __HAL_FLASH_CLEAR_FLAG(__FLAG__) ///< __HAL_FLASH_CLEAR_FLAG_BANK1(__FLAG__)
#endif /* DUAL_BANK */

/** @defgroup FLASH_Sectors FLASH Sectors
  * @{
  */
#define BSP_FLASH_SECTOR_0  0U      /*!< Sector Number 0   */
#define BSP_FLASH_SECTOR_1  1U       /*!< Sector Number 1   */
#define BSP_FLASH_SECTOR_2  2U       /*!< Sector Number 2   */
#define BSP_FLASH_SECTOR_3  3U       /*!< Sector Number 3   */
#define BSP_FLASH_SECTOR_4  4U       /*!< Sector Number 4   */
#define BSP_FLASH_SECTOR_5  5U       /*!< Sector Number 5   */
#define BSP_FLASH_SECTOR_6  6U       /*!< Sector Number 6   */
#define BSP_FLASH_SECTOR_7  7U       /*!< Sector Number 7   */

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
 * @brief Function pointer type for FLASH callback functions.
 */
typedef void (*BSP_FLASHCBPtr_t)(uint32_t ReturnValue);

/**
 * @brief Enumeration for BSP FLASH operations.
 *
 * Represents the various flash-related operations available
 * within the BSP layer, abstracting HAL functions.
 */
typedef enum _BSP_FLASHOperation_t {
    /* FLASH Peripheral Control functions */
    BSP_FLASH_UNLOCK,
    BSP_FLASH_LOCK,
    BSP_FLASH_OB_UNLOCK,
    BSP_FLASH_OB_LOCK,
    BSP_FLASH_OB_LAUNCH,

    /* FLASH Peripheral Errors functions */
    BSP_FLASH_GETERROR,

    /* FLASH Programming operation functions */
    BSP_FLASH_PROGRAM,
    BSP_FLASH_PROGRAM_IT,

    /* FLASH Extended programming operation functions */
    BSP_FLASHEx_ERASE,
    BSP_FLASHEx_ERASE_IT,
    BSP_FLASHEx_OBPROGRAM,
    BSP_FLASHEx_OBGETCONFIG,
    BSP_FLASHEx_UNLOCK_BANK1,
    BSP_FLASHEx_LOCK_BANK1,
    BSP_FLASHEx_UNLOCK_BANK2,
    BSP_FLASHEx_LOCK_BANK2,
    BSP_FLASHEx_COMPUTECRC,
} BSP_FLASHOperation_t;

/**
 * @enum BSP_FLASHCBType_t
 * @brief FLASH callback types for various flash events.
 */
typedef enum _BSP_FLASHCBType_t {
    BSP_FLASH_ENDOFOPERATIONCALLBACK,
    BSP_FLASH_OPERATIONERRORCALLBACK,
    BSP_FLASH_CALLBACK_TYPE_COUNT,
} BSP_FLASHCBType_t;

/**
 * @struct BSP_FLASHCB_t
 * @brief Manager BSP FLASH Custom Callbacks and Parameters.
 */
typedef struct _BSP_FLASHCB_t {
    BSP_FLASHCBPtr_t callbacks[BSP_FLASH_CALLBACK_TYPE_COUNT];
    uint32_t returnValparams[BSP_FLASH_CALLBACK_TYPE_COUNT];
} BSP_FLASHCB_t;

/**
  * @brief  FLASH Erase structure definition
  */
typedef struct _BSP_FLASH_EraseInitTypeDef_t {
  uint32_t TypeErase;   /*!< Mass erase or sector Erase.
                             This parameter can be a value of @ref FLASHEx_Type_Erase */
  uint32_t Banks;       /*!< Select banks to erase when Mass erase is enabled.
                             This parameter must be a value of @ref FLASHEx_Banks */
  uint32_t Page;      /*!< Initial FLASH sector to erase when Mass erase is disabled
                             This parameter must be a value of @ref FLASH_Sectors */
  uint32_t NbPages;   /*!< Number of sectors to be erased.
                             This parameter must be a value between 1 and (max number of sectors - value of Initial sector)*/
} BSP_FLASHEraseInitTypeDef_t;

/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Extern declarations for global variables.
 */




/**
 *------------------------------------------------------------
 *                     FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Function prototypes declaration for this module.
 */

/* ------------------- UNLOCK & LOCK ------------------- */
BSP_StatusTypeDef_t BSP_UnlockFlash(void);
BSP_StatusTypeDef_t BSP_LockFlash(void);

/* ------------------- FLASH CHECK ------------------- */
uint32_t BSP_GetFlashError(void);

/* ------------------- FLASH PROGRAMMING OPERATION ------------------- */
BSP_StatusTypeDef_t BSP_ProgramFlash(uint32_t typeProgram, uint32_t flashAddress, uint32_t dataAddress);
BSP_StatusTypeDef_t BSP_ProgramFlashIT(uint32_t typeProgram, uint32_t flashAddress, uint32_t dataAddress);
BSP_StatusTypeDef_t BSP_EraseFlashEx(BSP_FLASHEraseInitTypeDef_t* pEraseInit, uint32_t* pSectorError);
BSP_StatusTypeDef_t BSP_EraseFlashExIT(BSP_FLASHEraseInitTypeDef_t* pEraseInit);

/* ------------------- FDCAN CALLBACKS ------------------- */
void BSP_SetFLASHCB(BSP_FLASHCBType_t callbackType, BSP_FLASHCBPtr_t callback, uint32_t returnValparams);


#endif /* HAL_FLASH_MODULE_ENABLED */

#endif /* FLASH_INC_BSP_FLASH_H_ */
