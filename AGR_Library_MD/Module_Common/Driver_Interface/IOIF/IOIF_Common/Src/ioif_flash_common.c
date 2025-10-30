

#include "ioif_flash_common.h"

/** @defgroup FLASH FLASH
  * @brief FLASH BSP module driver
  * @{
  */
#ifdef BSP_FLASH_MODULE_ENABLED

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




/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static uint32_t GetSector(uint32_t addr);


/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

/**
  * @brief  Initialize the Flash memory.
  * 
  * This function clears any pending flags related to the Flash memory operations.
  * It should be called before any Flash memory operations to ensure that the 
  * Flash is in a clean state.
  *
  * @retval None
  */
void IOIF_InitFlash(void)
{
    /* Clear pending flags (if any) */
    __BSP_FLASH_CLEAR_FLAG(BSP_FLASH_FLAG_EOP | BSP_FLASH_FLAG_OPERR | BSP_FLASH_FLAG_WRPERR |
                            BSP_FLASH_FLAG_PGSERR | BSP_FLASH_FLAG_WRPERR);
}

/**
  * @brief  Erase the specified flash sector or all flash sectors.
  * @param  startSector: The starting sector to erase.
  * @param  eraseAll: If true, erase all flash sectors; otherwise, erase only the specified sector.
  * @retval IOIF_FLASH_STATUS_OK if successful, IOIF_FLASH_STATUS_ERASEKO otherwise.
  */
IOIF_FLASHState_t IOIF_EraseFlash(uint32_t startSector, bool eraseAll)
{
    uint32_t userStartSector;
    uint32_t sectorError;
    BSP_FLASHEraseInitTypeDef_t eraseInit;

    /* Unlock the Flash for write/erase operations */
    uint8_t status = BSP_UnlockFlash();
    if (status != BSP_OK) {
        return IOIF_FLASH_STATUS_ERASEKO;
    }

    IOIF_InitFlash();

    if (eraseAll) {
        eraseInit.TypeErase = BSP_FLASH_TYPEERASE_MASSERASE;
        eraseInit.Banks = BSP_FLASH_BANK_BOTH; // 전체 Bank를 지울 경우 해당 설정 필요
    } else {
        /* Calculate the sector index */
        userStartSector = GetSector(startSector);

        eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
        eraseInit.Page = userStartSector;
        eraseInit.NbPages = IOIF_NUM_SECTORS;

        if (startSector < IOIF_FLASH_SECTOR_0_BANK2_ADDR) {
            eraseInit.Banks = BSP_FLASH_BANK_1;
        } else {
            eraseInit.Banks = BSP_FLASH_BANK_2;
        }
    }

    /* Perform the erase operation */
    if (BSP_EraseFlashEx(&eraseInit, &sectorError) != BSP_OK) {
        /* Error occurred while erasing the sector */
        BSP_LockFlash();
        return IOIF_FLASH_STATUS_ERASEKO;
    }

    /* If we're erasing everything, we also need to erase the second bank */
    if (eraseAll) {
        eraseInit.Banks = BSP_FLASH_BANK_2;
        if (BSP_EraseFlashEx(&eraseInit, &sectorError) != BSP_OK) {
            /* Error occurred while erasing the sector */
            BSP_LockFlash();
            return IOIF_FLASH_STATUS_ERASEKO;
        }
    }

    status = BSP_LockFlash();
    if (status != BSP_OK) {
        return IOIF_FLASH_STATUS_ERASEKO;
    }

    return IOIF_FLASH_STATUS_OK;
}

/**
  * @brief  Write a data buffer to Flash memory.
  * @note   Ensure to erase the flash sector before writing to it.
  * @param  flashAddr: Start address in Flash for writing data
  * @param  pData: Pointer to the data buffer to be written
  * @param  length: Actual length of the data to be written
  * @retval None
  */
IOIF_FLASHState_t IOIF_WriteFlash(uint32_t flashAddr, void* pData)
{
    BSP_UnlockFlash();  // Unlock the flash memory for writing
    uint8_t status = BSP_ProgramFlash(FLASH_TYPEPROGRAM_QUADWORD, flashAddr, (uint32_t)pData);  // Write the data to flash memory
    if (status != IOIF_FLASH_STATUS_OK) {
    	return status;
    }
    BSP_LockFlash();  // Lock the flash memory after writing

    return IOIF_FLASH_STATUS_OK;
}

/**
  * @brief  Read a data buffer from Flash memory.
  * @param  flashAddr: Start address in Flash from where data is to be read
  * @param  pData: Pointer to the buffer where the read data will be stored
  * @param  length: Actual length of the data to be read
  * @retval None
  */
IOIF_FLASHState_t IOIF_ReadFlash(uint32_t flashAddr, void* pData, uint32_t length)
{
    if(length > IOIF_FLASH_BUFFER_SIZE) {
        return IOIF_FLASH_BUFFER_OVERFLOW;
    }

    // Read data from the flash memory into the buffer
    memcpy(pData, (uint32_t*)flashAddr, length);

    return IOIF_FLASH_STATUS_OK;
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

/**
  * @brief  Determines the sector of a given flash address.
  * @param  addr: Flash address to determine the sector for.
  * @retval The sector corresponding to the given address.
  */
static uint32_t GetSector(uint32_t addr)
{
    const uint32_t sectorsBank1[] = {
        IOIF_FLASH_SECTOR_0_BANK1_ADDR, IOIF_FLASH_SECTOR_1_BANK1_ADDR, IOIF_FLASH_SECTOR_2_BANK1_ADDR, IOIF_FLASH_SECTOR_3_BANK1_ADDR,
        IOIF_FLASH_SECTOR_4_BANK1_ADDR, IOIF_FLASH_SECTOR_5_BANK1_ADDR, IOIF_FLASH_SECTOR_6_BANK1_ADDR, IOIF_FLASH_SECTOR_7_BANK1_ADDR
    };

    const uint32_t sectorsBank2[] = {
        IOIF_FLASH_SECTOR_0_BANK2_ADDR, IOIF_FLASH_SECTOR_1_BANK2_ADDR, IOIF_FLASH_SECTOR_2_BANK2_ADDR, IOIF_FLASH_SECTOR_3_BANK2_ADDR,
        IOIF_FLASH_SECTOR_4_BANK2_ADDR, IOIF_FLASH_SECTOR_5_BANK2_ADDR, IOIF_FLASH_SECTOR_6_BANK2_ADDR, IOIF_FLASH_SECTOR_7_BANK2_ADDR
    };

    for (uint32_t i = 0; i < 7; i++) {
        if (addr < sectorsBank1[i + 1]) {
            return i;
        }
    }

    for (uint32_t i = 0; i < 7; i++) {
        if (addr < sectorsBank2[i + 1]) {
            return i;
        }
    }

    return BSP_FLASH_SECTOR_7; // Default return value if no matching sector is found.
}


#endif /* BSP_FLASH_MODULE_ENABLED */
