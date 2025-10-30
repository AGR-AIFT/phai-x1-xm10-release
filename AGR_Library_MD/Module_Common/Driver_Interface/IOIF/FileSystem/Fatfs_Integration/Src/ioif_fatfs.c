/*
 * ioif_fatfs_sd.c
 *
 *  Created on: Sep 19, 2023
 *      Author: Angelrobotics
 */


#include "ioif_fatfs.h"

#ifdef IOIF_FATFS_ENABLED

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

#ifdef FATFS_SD_ENABLE
extern const Diskio_drvTypeDef SD_Driver;
#endif

#ifdef FATFS_USB_ENABLE
extern const Diskio_drvTypeDef USBH_Driver;
#endif

/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */


static uint64_t IOIF_FreeSpaceSize;


/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */



/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */

#ifdef FATFS_SD_ENABLE

IOIF_SD_Status_t IOIF_FATFS_SD_Init(IOIF_SD_t SDPortNum, uint8_t* DrivePath)
{
	BSP_SD_Status_t sd_status = BSP_MSD_ERROR;

	sd_status = BSP_InitSD((BSP_SD_t)SDPortNum);
	FATFS_LinkDriver(&SD_Driver, (char*)DrivePath);

	return (IOIF_SD_Status_t) sd_status;
}

#endif

#ifdef FATFS_USB_ENABLE
IOIF_USB_Status_t IOIF_FATFS_USB_Init(IOIF_FATFS_USB_t usbmode, uint8_t* DrivePath)
{
	IOIF_USB_Status_t status = IOIF_FATFS_USB_OK;
	bool usb_status = false;

	/* USB driver init. code */
	if(usbmode ==  IOIF_FATFS_USB_MSC)
	{
		usb_status = BSP_USBInit(BSP_USBH_MSC);
		if(usb_status != true)
			status = IOIF_FATFS_USB_FAIL;

	}
	else		/* Todo : Will be adding CDC Mass Storage mode */
	{
		status = IOIF_FATFS_USB_FAIL;
	}

	/* Linked Driver */
	FATFS_LinkDriver(&USBH_Driver, (char*)DrivePath);

	return status;
}
#endif


IOIF_fCMD_res_t IOIF_FileMount(IOIF_FATFS_t* FileSysObject, uint8_t* DrivePath)
{
	return f_mount(FileSysObject, (const TCHAR*)DrivePath, 1);
}


IOIF_fCMD_res_t IOIF_FileUnmount(uint8_t* DrivePath)
{
	return f_mount(NULL, (const TCHAR*)DrivePath, 0);
}


IOIF_fCMD_res_t IOIF_FileOpen(IOIF_FILE_t* FileObject, uint8_t* Filename)
{
	/* File open with read/write function, if file is not exist, will return failure */
	return f_open(FileObject, (const TCHAR*)Filename, FA_READ | FA_WRITE | FA_OPEN_EXISTING);
}


IOIF_fCMD_res_t IOIF_FileCreate(IOIF_FILE_t* FileObject, uint8_t* Filename)
{
	/* File create with read/write function, if file is existing, it will be truncated and overwritten */
	return f_open(FileObject, (const TCHAR*)Filename, FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
}

IOIF_fCMD_res_t IOIF_FileCreateNew(IOIF_FILE_t* FileObject, uint8_t* Filename)
{
	/* File create with read/write function, if file is existing, will return failure */
	return f_open(FileObject, (const TCHAR*)Filename, FA_READ | FA_WRITE | FA_CREATE_NEW);
}

IOIF_fCMD_res_t IOIF_FileOpenCreate(IOIF_FILE_t* FileObject, uint8_t* Filename)
{
	/* File open with read/write function, if file is not exist, a new file will be created */
	return f_open(FileObject, (const TCHAR*)Filename, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
}

IOIF_fCMD_res_t IOIF_FileOpenCreateAppend(IOIF_FILE_t* FileObject, uint8_t* Filename)
{
	/* File open with read/append write function, if file is not exist, a new file will be created */
	return f_open(FileObject, (const TCHAR*)Filename, FA_READ | FA_WRITE | FA_OPEN_APPEND);
}


IOIF_fCMD_res_t IOIF_FileClose(IOIF_FILE_t* FileObject)
{
	/* If file system is open, must be closed after access */
	return f_close(FileObject);
}


IOIF_fCMD_res_t IOIF_fWrite(IOIF_FILE_t* FileObject, void* WriteBuff, uint32_t WriteSize, uint32_t* WriteByte)
{
	/*
		FileObject : [IN] Pointer to the file object structure
		WriteBuff  : [IN] Pointer to the data to be written
		WriteSize  : [IN] Number of bytes to write
		WriteByte  : [OUT] Pointer to the variable to return number of bytes written
	*/
	return f_write(FileObject, WriteBuff, (unsigned int)WriteSize, (unsigned int*) WriteByte);
}


IOIF_fCMD_res_t IOIF_fRead(IOIF_FILE_t* FileObject, void* ReadBuff, uint32_t ReadSize, uint32_t* ReadByte)
{
	/*
	FileObject : [IN] File object
	ReadBuff   : [OUT] Buffer to store read data
	ReadSize   : [IN] Number of bytes to read
	ReadByte   : [OUT] Number of bytes read
	*/
	return f_read(FileObject, ReadBuff, (unsigned int) ReadSize, (unsigned int*) ReadByte);
}


/* File 1-cycle write operation with file open and write then close */

IOIF_fCMD_res_t IOIF_FileWrite(IOIF_FILE_t* FileObject, uint8_t* Filename, void* WriteBuff, uint32_t WriteSize, uint32_t* WriteByte)
{
	IOIF_fCMD_res_t status;

	if (!FileObject|| !Filename) {
		return status = FR_INVALID_PARAMETER;
	}

	status = f_open (FileObject, (const TCHAR*)Filename, FA_WRITE | FA_OPEN_ALWAYS); // File 'always' open with write only

	if (status == FR_OK) {
		status = f_write(FileObject, WriteBuff, (unsigned int)WriteSize, (unsigned int*) WriteByte);
		if (status == FR_OK) {
			f_close(FileObject);
			return status = FR_OK;
		} else {
			return status;
		} 
	} else {
		return status;
	}

	return status;
}


IOIF_fCMD_res_t IOIF_FileWriteAppend(IOIF_FILE_t* FileObject, uint8_t* Filename, void* WriteBuff, uint32_t WriteSize, uint32_t* WriteByte)
{
	IOIF_fCMD_res_t status;

	if (!FileObject|| !Filename) {
		return status = FR_INVALID_PARAMETER;
	}

	status = f_open(FileObject, (const TCHAR*)Filename, FA_WRITE | FA_OPEN_APPEND); // File 'append' open with write only

	if (status == FR_OK) {
		status = f_write(FileObject, WriteBuff, (unsigned int)WriteSize, (unsigned int*) WriteByte);
		if (status == FR_OK) {
			f_close(FileObject);
			return status = FR_OK;
		} else {
			return status;
		}
	} else {
		return status;
	}

	return status;
}


/* File 1-cycle read operation with file open and read then close */

IOIF_fCMD_res_t IOIF_FileRead(IOIF_FILE_t* FileObject, uint8_t* Filename, void* ReadBuff, uint32_t ReadSize, uint32_t* ReadByte)
{
	IOIF_fCMD_res_t status;

	if (!FileObject|| !Filename)
	{
		return status = FR_INVALID_PARAMETER;
	}

	status = f_open (FileObject, (const TCHAR*)Filename, FA_READ | FA_OPEN_ALWAYS); // File 'always' open with read only

	if (status == FR_OK)
	{
		status = f_read(FileObject, ReadBuff, (unsigned int)ReadSize, (unsigned int*)ReadByte);
		if (status == FR_OK)
		{
			f_close(FileObject);
			return status = FR_OK;
		} else return status;
	} else return status;

	return status;
}


/* File Synchronization with safe save */

IOIF_fCMD_res_t IOIF_FileSync(IOIF_FILE_t* FileObject)
{
	return f_sync(FileObject);
}


float IOIF_Disk_GetFreeSpace(uint8_t* DrivePath, IOIF_FATFS_t* FileSysObject)
{
	IOIF_fCMD_res_t status;

	status = f_getfree((const TCHAR*)DrivePath, (DWORD*)&IOIF_FreeSpaceSize, &FileSysObject);

	if(status == FR_OK)
		return (float)(((uint32_t)((uint64_t)IOIF_FreeSpaceSize * (uint64_t)FileSysObject->csize/ 2 )) / 1024);  // calculate free-space, unit : MB
	else
		return -1;		// return -1 means calculation error

	return -1;
}


uint32_t IOIF_Disk_TotalSpace(IOIF_FATFS_t* FileSysObject)
{
	return (uint32_t)((uint64_t)(FileSysObject->n_fatent - 2) * (uint64_t)FileSysObject->csize / 2 / 1024);	// calculate total space, unit : MB
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */




#endif /* IOIF_FATFS_SD_ENABLED */
