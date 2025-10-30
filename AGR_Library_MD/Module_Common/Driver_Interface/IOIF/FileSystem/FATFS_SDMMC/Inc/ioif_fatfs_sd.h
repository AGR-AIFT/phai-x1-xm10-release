/*
 * ioif_fatfs_sd.h
 *
 *  Created on: Sep 19, 2023
 *      Author: Angelrobotics
 */

#ifndef FATFS_SDMMC_INC_IOIF_FATFS_SD_H_
#define FATFS_SDMMC_INC_IOIF_FATFS_SD_H_

#include "module.h"

#ifdef IOIF_FATFS_SD_ENABLED

#include "bsp_sdmmc.h"
#include "fatfs.h"

/** @defgroup FATFS IOIF
  * @brief FATFS IOIF driver
  * @{
  */


/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */


/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

 typedef enum _IOIF_SD_Status_t {
 	 IOIF_MSD_OK    			   = BSP_MSD_OK,
	 IOIF_MSD_ERROR 			   = BSP_MSD_ERROR,
	 IOIF_MSD_ERROR_SD_NOT_PRESENT = BSP_MSD_ERROR_SD_NOT_PRESENT,
  }IOIF_SD_Status_t;

  typedef enum _IOIF_SD_Transfer_Status_t {
	 IOIF_SD_TRANSFER_OK   = BSP_SD_TRANSFER_OK,
	 IOIF_SD_TRANSFER_BUSY = BSP_SD_TRANSFER_BUSY,
	 IOIF_SD_PRESENT 	   = BSP_SD_PRESENT,
	 IOIF_SD_NOT_PRESENT   = BSP_SD_NOT_PRESENT,
	 IOIF_SD_DATATIMEOUT   = BSP_SD_DATATIMEOUT,
  }IOIF_SD_Transfer_Status_t;

  typedef enum _IOIF_SD_t {
	 IOIF_SD1 	   = BSP_SD1,
	 IOIF_SD2 	   = BSP_SD2,
	 IOIF_SD_COUNT = BSP_SD_COUNT,
  }IOIF_SD_t;



typedef FATFS 	IOIF_FATFS_t;
typedef FATFS*	IOIF_FATFS_ptr_t;
typedef FIL	  	IOIF_FILE_t;
typedef FRESULT IOIF_fCMD_res_t;




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

 /* File System Init.*/
 IOIF_SD_Status_t IOIF_SD_Init(IOIF_SD_t SDPortNum);

 /* File System Mount on Device */
 IOIF_fCMD_res_t IOIF_FileMount(IOIF_FATFS_t* FileSysObject, uint8_t* DrivePath);
 IOIF_fCMD_res_t IOIF_FileUnmount(uint8_t* DrivePath);

 /* File Open & Creates */
 IOIF_fCMD_res_t IOIF_FileOpen(IOIF_FILE_t* FileObject, uint8_t* Filename);
 IOIF_fCMD_res_t IOIF_FileOpenCreate(IOIF_FILE_t* FileObject, uint8_t* Filename);
 IOIF_fCMD_res_t IOIF_FileCreateNew(IOIF_FILE_t* FileObject, uint8_t* Filename);
 IOIF_fCMD_res_t IOIF_FileCreate(IOIF_FILE_t* FileObject, uint8_t* Filename);
 IOIF_fCMD_res_t IOIF_FileClose(IOIF_FILE_t* FileObject);

 /* File Single Write & Read */
 IOIF_fCMD_res_t IOIF_fWrite(IOIF_FILE_t* FileObject, void* WriteBuff, uint32_t WriteSize, uint32_t* WriteByte);
 IOIF_fCMD_res_t IOIF_fRead(IOIF_FILE_t* FileObject, void* ReadBuff, uint32_t ReadSize, uint32_t* ReadByte);

 /* File Open & Create and Write & Read -> Close */
 IOIF_fCMD_res_t IOIF_FileWrite(IOIF_FILE_t* FileObject, uint8_t* Filename, void* WriteBuff, uint32_t WriteSize, uint32_t* WriteByte);
 IOIF_fCMD_res_t IOIF_FileRead(IOIF_FILE_t* FileObject, uint8_t* Filename, void* ReadBuff, uint32_t ReadSize, uint32_t* ReadByte);


 /* Disk Information */
 float 	  IOIF_Disk_GetFreeSpace(uint8_t* DrivePath, IOIF_FATFS_t* FileSysObject);
 uint32_t IOIF_Disk_TotalSpace(IOIF_FATFS_t* FileSysObject);



#endif /* MODULE : IOIF_FATFS_SD_ENABLED */

#endif /* FATFS_SDMMC_INC_IOIF_FATFS_SD_H_ */
