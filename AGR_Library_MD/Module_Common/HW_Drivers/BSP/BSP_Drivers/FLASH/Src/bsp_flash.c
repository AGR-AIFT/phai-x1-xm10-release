/**
 * @file bsp_flash.c
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for FLASH functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#include "bsp_flash.h"

/** @defgroup FLASH FLASH
  * @brief FLASH HAL BSP module driver
  * @{
  */
#ifdef HAL_FLASH_MODULE_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */




/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */




/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */

static BSP_FLASHCB_t bsp_flashCB;


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static void ExecuteFLASHCB(uint32_t ReturnVal, BSP_FLASHCBType_t callbackType);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/* ------------------- UNLOCK & LOCK ------------------- */

/**
 * @brief Unlock the FLASH peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_UnlockFlash(void)
{    
    return (BSP_StatusTypeDef_t)HAL_FLASH_Unlock();
}

/**
 * @brief Lock the FLASH peripheral.
 * @return HAL status.
 */
BSP_StatusTypeDef_t BSP_LockFlash(void)
{    
    return (BSP_StatusTypeDef_t)HAL_FLASH_Lock();
}

/* ------------------- FLASH CHECK ------------------- */
/**
 * @brief Get Error Code the FLASH peripheral.
 * @return HAL_FLASH_ERRORCode.
 */
uint32_t BSP_GetFlashError(void)
{
    return HAL_FLASH_GetError();
}

/* ------------------- FLASH PROGRAMMING OPERATION ------------------- */
/**
 * @brief  Program a flash word at a specified address
 * @param  typeProgram Indicate the way to program at a specified address.
 *         This parameter can be a value of @ref FLASH_Type_Program
 * @param  flashAddress specifies the address to be programmed.
 *         This parameter shall be aligned to the Flash word:
 *          - 256 bits for STM32H74x/5X devices (8x 32bits words)
 *          - 128 bits for STM32H7Ax/BX devices (4x 32bits words)
 *          - 256 bits for STM32H72x/3X devices (8x 32bits words)
 * @param  dataAddress specifies the address of data to be programmed.
 *         This parameter shall be 32-bit aligned
 * @retval BSP_StatusTypeDef_t HAL Status
 */
BSP_StatusTypeDef_t BSP_ProgramFlash(uint32_t typeProgram, uint32_t flashAddress, uint32_t dataAddress)
{
    return (BSP_StatusTypeDef_t)HAL_FLASH_Program(typeProgram, flashAddress, dataAddress);
}

/**
 * @brief  Program a flash word at a specified address with interrupt enabled.
 * @param  typeProgram Indicate the way to program at a specified address.
 *                      This parameter can be a value of @ref FLASH_Type_Program
 * @param  flashAddress specifies the address to be programmed.
 *         This parameter shall be aligned to the Flash word:
 *          - 256 bits for STM32H74x/5X devices (8x 32bits words)
 *          - 128 bits for STM32H7Ax/BX devices (4x 32bits words)
 *          - 256 bits for STM32H72x/3X devices (8x 32bits words)
 * @param  dataAddress specifies the address of data to be programmed.
 *         This parameter shall be 32-bit aligned
 * @retval HAL Status
 */
BSP_StatusTypeDef_t BSP_ProgramFlashIT(uint32_t typeProgram, uint32_t flashAddress, uint32_t dataAddress)
{
    return (BSP_StatusTypeDef_t)HAL_FLASH_Program_IT(typeProgram, flashAddress, dataAddress);
}

/**
 * @brief  Perform a mass erase or erase the specified FLASH memory sectors
 * @param[in]  pEraseInit pointer to an FLASH_EraseInitTypeDef structure that
 *         contains the configuration information for the erasing.
 * @param[out]  pSectorError pointer to variable  that contains the configuration
 *          information on faulty sector in case of error (0xFFFFFFFF means that all
 *          the sectors have been correctly erased)
 * @retval HAL Status
 */
BSP_StatusTypeDef_t BSP_EraseFlashEx(BSP_FLASHEraseInitTypeDef_t* pEraseInit, uint32_t* pSectorError)
{
    return (BSP_StatusTypeDef_t)HAL_FLASHEx_Erase((FLASH_EraseInitTypeDef*)pEraseInit, pSectorError);
}

/**
 * @brief  Perform a mass erase or erase the specified FLASH memory sectors with interrupt enabled
 * @param  pEraseInit pointer to an FLASH_EraseInitTypeDef structure that contains the configuration information for the erasing.
 * @retval HAL Status
 */
BSP_StatusTypeDef_t BSP_EraseFlashExIT(BSP_FLASHEraseInitTypeDef_t* pEraseInit)
{
    BSP_StatusTypeDef_t status = BSP_OK;
    status = (BSP_StatusTypeDef_t)HAL_FLASHEx_Erase_IT((FLASH_EraseInitTypeDef*)pEraseInit);

    return status;
}

/* ------------------- FLASH CALLBACKS ------------------- */
/**
 * @brief Registers a custom callback for a specified FLASH port and callback type.
 *
 * @param flashEnum Enum value representing the FLASH port.
 * @param callbackType Enum value representing the type of callback (e.g., transmit complete, receive complete).
 * @param callback Function pointer to the custom callback function.
 * @param params Pointer to any parameters that the custom callback function may need.
 */
void BSP_SetFLASHCB(BSP_FLASHCBType_t callbackType, BSP_FLASHCBPtr_t callback, uint32_t returnValparams)
{
    if (callbackType < BSP_FLASH_CALLBACK_TYPE_COUNT && callback) {
        bsp_flashCB.callbacks[callbackType] = callback;
        bsp_flashCB.returnValparams[callbackType] = returnValparams;
    }
}

/**
 * @brief  FLASH end of operation interrupt callback
 * @param  ReturnVal The value saved in this parameter depends on the ongoing procedure
 *                  Mass Erase: Bank number which has been requested to erase
 *                  Sectors Erase: Sector which has been erased
 *                    (if 0xFFFFFFFF, it means that all the selected sectors have been erased)
 *                  Program: Address which was selected for data program
 * @retval None
 */
void HAL_FLASH_EndOfOperationCallback(uint32_t ReturnVal)
{
    UNUSED(ReturnVal);

    ExecuteFLASHCB(ReturnVal, BSP_FLASH_ENDOFOPERATIONCALLBACK);
}

/**
 * @brief  FLASH operation error interrupt callback
 * @param  ReturnVal The value saved in this parameter depends on the ongoing procedure
 *                 Mass Erase: Bank number which has been requested to erase
 *                 Sectors Erase: Sector number which returned an error
 *                 Program: Address which was selected for data program
 * @retval None
 */
void HAL_FLASH_OperationErrorCallback(uint32_t ReturnVal)
{
    UNUSED(ReturnVal);

    ExecuteFLASHCB(ReturnVal, BSP_FLASH_OPERATIONERRORCALLBACK);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
 * @brief Executes a custom callback for a specified FLASH port and callback type.
 *
 * @param hflash HAL FLASH handle, used to find the corresponding FLASH port enum value.
 * @param callbackType Enum value representing the type of callback to execute.
 */
static void ExecuteFLASHCB(uint32_t ReturnVal, BSP_FLASHCBType_t callbackType)
{
    BSP_FLASHCBPtr_t callback = bsp_flashCB.callbacks[callbackType];
    uint32_t params = bsp_flashCB.returnValparams[callbackType];
    if (callback) {
        callback(params); // Executes the custom callback with the specified parameters
    }
}


#endif /* HAL_FLASH_MODULE_ENABLED */
