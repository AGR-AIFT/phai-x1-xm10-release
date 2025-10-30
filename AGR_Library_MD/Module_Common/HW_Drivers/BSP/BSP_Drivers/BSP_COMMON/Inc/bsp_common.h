/**
 * @file bsp_common.h
 * @date Created on: Jul 28, 2023
 * @author AngelRobotics HW Team
 * @brief Board Support Package for BSP common functionalities.
 * @ref um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf
 */

#ifndef BSP_COMMON_INC_BSP_COMMON_H_
#define BSP_COMMON_INC_BSP_COMMON_H_

#include "main.h"
#include "module.h"

/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

/**
 * @brief Calculates the number of elements in an array.
 * @param arr The array for which to calculate the size.
 * @warning Use only with actual arrays. Incorrect results with pointers.
 * @return Number of elements in the array.
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

/**
  * @brief  BSP Status structures definition
  */
typedef enum _BSP_StatusTypeDef_t {
    BSP_OK       = 0x00,
    BSP_ERROR    = 0x01,
    BSP_BUSY     = 0x02,
    BSP_TIMEOUT  = 0x03
} BSP_StatusTypeDef_t;

#ifdef _USE_OS_RTOS_BSP

#ifdef _USE_CMSISV1
#include "cmsis_os.h"
#endif

#ifdef _USE_CMSISV2
#include "cmsis_os2.h"
#endif

#ifdef 	_USE_SEMAPHORE
extern osSemaphoreId_t BinSem_I2C1Handle;
extern osSemaphoreId_t BinSem_I2C2Handle;
extern osSemaphoreId_t BinSem_I2C3Handle;
extern osSemaphoreId_t BinSem_I2C4Handle;
extern osSemaphoreId_t BinSem_SPI1Handle;
extern osSemaphoreId_t BinSem_SPI2Handle;
extern osSemaphoreId_t BinSem_SPI3Handle;
extern osSemaphoreId_t BinSem_SPI4Handle;
extern osSemaphoreId_t BinSem_SPI5Handle;
extern osSemaphoreId_t BinSem_SPI6Handle;
extern osSemaphoreId_t BinSem_UART7Handle;
extern osSemaphoreId_t BinSem_SDMMC1Handle;
extern osSemaphoreId_t BinSem_SDMMC2Handle;
extern osSemaphoreId_t BinSem_SAI1Handle;
extern osSemaphoreId_t BinSem_SAI2Handle;
extern osSemaphoreId_t BinSem_FDCAN1Handle;
extern osSemaphoreId_t BinSem_FDCAN2Handle;

#define BinSemI2C1_TIMEOUT				1000
#define BinSemI2C2_TIMEOUT				1000
#define BinSemI2C3_TIMEOUT				1000
#define BinSemI2C4_TIMEOUT				1000
#define BinSemSPI1_TIMEOUT				1000
#define BinSemSPI2_TIMEOUT				1000
#define BinSemSPI3_TIMEOUT				1000
#define BinSemSPI4_TIMEOUT				1000
#define BinSemSPI5_TIMEOUT				1000
#define BinSemSPI6_TIMEOUT				1000
#define BinSemUART1_TIMEOUT				1000
#define BinSemUART2_TIMEOUT				1000
#define BinSemUART3_TIMEOUT				1000
#define BinSemUART4_TIMEOUT				1000
#define BinSemUART5_TIMEOUT				1000
#define BinSemUART6_TIMEOUT				1000
#define BinSemUART7_TIMEOUT				1000
#define BinSemUART8_TIMEOUT				1000
#define BinSemSDMMC1_TIMEOUT			2000
#define BinSemSDMMC2_TIMEOUT			2000
#define BinSemSAI1_TIMEOUT				1000
#define BinSemSAI2_TIMEOUT				1000
#define BinSemFDCAN1_TIMEOUT			1000
#define BinSemFDCAN2_TIMEOUT			1000
#endif /* _USE_SEMAPHORE */

#endif /* _USE_OS_RTOS_BSP */


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




#endif /* BSP_COMMON_INC_BSP_COMMON_H_ */
