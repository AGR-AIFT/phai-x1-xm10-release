/*
 * IOIF_Audio_WavPlay_SAI.c
 *
 *  Created on: Sep 19, 2023
 *      Author: Angelrobotics
 */

#include "IOIF_Audio_WavPlay_SAI.h"


#ifdef IOIF_AUDIO_WAV_SAI_ENABLED

/**
 *-----------------------------------------------------------
 *              TYPE DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 * @brief Enumerated types and structures central to this module.
 */
#define IOIF_SD_PORT	BSP_SD1


/**
 *------------------------------------------------------------
 *                      GLOBAL VARIABLES
 *------------------------------------------------------------
 * @brief Variables accessible throughout the application.
 */

/* Declaration : FATFS variable */

FATFS 	audio_fs   __attribute__((section(".FATFS_RAMD1_data")));		//FATFS data structure works in RAM_D1(AXIRAM) with D-cache
FIL	  	audio_file __attribute__((section(".FATFS_RAMD1_data")));		//FATFS data structure works in RAM_D1(AXIRAM) with D-cache

/* Extern : Link driver (USB,SD CARD) */
extern const Diskio_drvTypeDef SD_Driver;
extern const Diskio_drvTypeDef USBH_Driver;

/* Extern : Mutex */
//extern osMutexId_t SoundMutexHandle;

FRESULT sc_mount_res, sc_read_res, sc_write_res;

static uint32_t audio_r_byte = 0;
//static uint32_t audio_w_byte = 0;


/* Declaration : WaveFile Header */

wavfile_header_t w_header;

/* Declaration : SAI Buffer Parameter*/

volatile static uint32_t sai_sample_rate = 16000;		// SAI sample rate (can be changed by audio sample rate)
volatile static uint32_t sai_qbuf_in = 0;				// queue buffer index : head
volatile static uint32_t sai_qbuf_out = 0;				// queue buffer index : tail
volatile static uint32_t sai_q_buf_len = 0;			// sampling data queue buffer length calculation
volatile static uint32_t sai_qbuf_len = 0; 				// total size of queue buffer

static bool sai_stop = false;							// SAI DMA transmit start/stop (no use in DMA circular, only normal mode)

/**
 *------------------------------------------------------------
 *                      STATIC VARIABLES
 *------------------------------------------------------------
 * @brief Variables local to this module.
 */

#ifdef _USE_STM32H7_DCACHE
static void DcacheForcedWT_Enable(void);
static void DcacheForcedWT_Disable(void);
#endif

/**
 *------------------------------------------------------------
 *                 STATIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * @brief Static Function prototypes for this module.
 */

static int16_t sai_q_buf_zero[SAI_BUF_LEN*2] __attribute__((section(".Sai1_RxBuff")));		// queue buffer for SAI
static audio_ch_t sai_q_buf[SAI_BUF_LEN]	 __attribute__((section(".Sai1_RxBuff")));		// 2-channel queue buffer for SAI

static uint8_t sai_r_buf_dummy = 0;



/**
 *------------------------------------------------------------
 *                      PUBLIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions that interface with this module.
 */


IOIF_WavPlayState_t WavAudio_FS_Init(uint8_t* DrivePath, IOIF_WavPlayerType_t IoType)
{
	IOIF_WavPlayState_t res = IOIF_WAVPLAY_STATUS_ERROR;
	BSP_SD_Status_t sd_status = BSP_MSD_ERROR;
	IOIF_USBState usbh_status = IOIF_USB_FAIL;

#ifdef _USE_STM32H7_DCACHE
	DcacheForcedWT_Enable();
#endif

	if(IoType == IOIF_WAVPLAY_SD)
	{
		sd_status = BSP_InitSD(IOIF_SD_PORT); 				// SD driver Init.
		if(sd_status != BSP_MSD_OK)
			return res;
		/* Linked SD Driver */
		FATFS_LinkDriver(&SD_Driver, (char*)DrivePath);		// IO driver link
	}
	else if(IoType == IOIF_WAVPLAY_USB)
	{
		usbh_status = IOIF_InitUSB(IOIF_USBH_MSC, IOIF_USB_TIM_NULL);
		if(usbh_status != IOIF_USB_OK)
			return res;
		/* Linked USB Driver */
		FATFS_LinkDriver(&USBH_Driver, (char*)DrivePath);		// IO driver link
	}
	else if(IoType == IOIF_WAVPLAY_RAM)
	{
		//Todo : will be implemented
	}
	else
		return res;


//	sc_mount_res = f_mount(&audio_fs, (const TCHAR*)DrivePath, 1);									// File-system Mount
//	if(sc_mount_res != FR_OK)
//		return res;


	memset(sai_q_buf_zero, 0, sizeof(sai_q_buf_zero));												//SAI buffer Init.
	memset(sai_q_buf,0, sizeof(sai_q_buf));															//SAI buffer Init.

	sai_q_buf_len = (sai_sample_rate * 1) / (1000/SAI_BUF_MS);      								//SAI buffer length calculation
	sai_qbuf_len = SAI_BUF_LEN / sai_q_buf_len;														//SAI buffer length calculation

	BSP_SetSAICB(BSP_SAI1, BSP_SAI_TX_CPLT_CALLBACK, SaiAudioDMATxCB, NULL);						//SAI Transmit Complete Callback Register

	//if(HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*) sai_q_buf_zero, sai_q_buf_len*2) == HAL_OK)	//SAI DMA start
	if(BSP_RunSAIDMA(BSP_SAI1, (uint8_t*) sai_q_buf_zero, &sai_r_buf_dummy, sai_q_buf_len*2, BSP_SAI_TRANSMIT_DMA) == BSP_OK)
		res = IOIF_WAVPLAY_STATUS_OK;

#ifdef _USE_STM32H7_DCACHE
	DcacheForcedWT_Disable();
#endif

	return res;
}


IOIF_WavPlayState_t WavAudio_FS_DeInit(uint8_t* DrivePath)
{
	IOIF_WavPlayState_t res = IOIF_WAVPLAY_STATUS_ERROR;
	//bool sd_status;
	BSP_SD_Status_t sd_status;

	//sd_status = BSP_SD_DeInit();										// SD driver Init.
	sd_status = BSP_DeInitSD(IOIF_SD_PORT);
	if(sd_status != BSP_MSD_OK)
		return res;

	sc_mount_res = f_mount(NULL, (const TCHAR*)DrivePath, 0);				// File-system Unmount
	if(sc_mount_res != FR_OK)
		return res;

	return res;
}


IOIF_WavPlayState_t GetWavFileInfo(uint8_t* DrivePath, uint8_t* filename, wavfile_header_t* wav_header)
{
	IOIF_WavPlayState_t res = IOIF_WAVPLAY_STATUS_ERROR;

	if(!filename ||!wav_header)			//if filename or received header structure is not assigned,
		return res;

	FRESULT wavfile_open_res, wavfile_read_res;

	wavfile_open_res = f_open(&audio_file, (const TCHAR*)filename, FA_READ);
	if(wavfile_open_res == FR_OK)
	{
		wavfile_read_res = f_read(&audio_file, wav_header, sizeof(wavfile_header_t), (unsigned int*)&audio_r_byte);		// reading header file
		if(wavfile_read_res == FR_OK)
			return res = IOIF_WAVPLAY_STATUS_OK;
	}
	return res;
}


IOIF_WavPlayState_t PlayWaveFile(uint8_t* DrivePath, uint8_t* filename)
{
	IOIF_WavPlayState_t res = IOIF_WAVPLAY_STATUS_ERROR;

	if(!filename)			//filename is not assigned,
		return res;

	FRESULT wavfile_open_res, wavfile_read_res, wavfile_seek_res;

	volatile uint32_t r_len = sai_q_buf_len;
	int16_t sound_buf_frame[sai_q_buf_len*2] ;

#ifdef _USE_STM32H7_DCACHE
	DcacheForcedWT_Enable();
#endif

	//osMutexAcquire(SoundMutexHandle, 3000);			// Mutex Lock

	sc_mount_res = f_mount(&audio_fs, (const TCHAR*)DrivePath, 1);					//mount driver

	wavfile_open_res = f_open(&audio_file, (const TCHAR*)filename, FA_READ);
	if(wavfile_open_res == FR_OK)
	{
		wavfile_read_res = f_read(&audio_file, &w_header, sizeof(wavfile_header_t), (unsigned int*)&audio_r_byte);		// reading header file
		wavfile_seek_res = f_lseek(&audio_file, sizeof(wavfile_header_t));												// move the file pointer to start of voice data

		if(wavfile_read_res != FR_OK || wavfile_seek_res !=FR_OK)
			return res;

		while(audio_r_byte)
		{
		  	uint32_t buf_len;
		  	int32_t len;
		  	uint32_t q_offset;

		  	buf_len = ((sai_qbuf_len + sai_qbuf_in - sai_qbuf_out) % sai_qbuf_len);											//check the queue buffer available space
		  	buf_len = (sai_qbuf_len - buf_len) - 1;

		  	if (buf_len > 0)
		  	{
		  		wavfile_read_res = f_read(&audio_file, &sound_buf_frame, r_len *(2*w_header.NumChannels), (unsigned int*)&audio_r_byte);		//wave file read

		  		len = audio_r_byte;																								//length : number of read byte from file

		  		if (len != r_len*(2*w_header.NumChannels))																	//if not match the real length with calculated length
		  		{
		  			break;
		  		}

		  		q_offset = sai_qbuf_in*sai_q_buf_len;																		//queue buffer offset : head * queue buffer length

		  		for (uint32_t i=0; i<r_len; i++)
		  		{
		  			if (w_header.NumChannels == 2)																			//number of channel check (mono or stereo)
		  			{
		  				sai_q_buf[q_offset + i].left  = sound_buf_frame[i*2 + 0];
		  			    sai_q_buf[q_offset + i].right = sound_buf_frame[i*2 + 1];
		  			}
		  			else {
		  			    sai_q_buf[q_offset + i].left  = sound_buf_frame[i];
		  			    sai_q_buf[q_offset + i].right = sound_buf_frame[i];
		  			}
		  		}

		  		if (((sai_qbuf_in + 1) % sai_qbuf_len) != sai_qbuf_out)														//queue buffer overflow check
		  		{
		  			sai_qbuf_in = (sai_qbuf_in+1) % sai_qbuf_len;
		  		}
		  	}
		 }
		 sai_stop = true;				// SAI TX complete callback stop
	}

	wavfile_open_res = f_close(&audio_file);
	if(wavfile_open_res == FR_OK)
		res = IOIF_WAVPLAY_STATUS_OK;

	sc_mount_res = f_mount(NULL, (const TCHAR*)DrivePath, 0);				// File-system Unmount

	sai_stop = false;					// SAI TX complete callback ready

#ifdef _USE_STM32H7_DCACHE
	DcacheForcedWT_Disable();
#endif

	//osMutexRelease(SoundMutexHandle);

	return res;
}


void SaiAudioDMATxCB(void* param)
{
	uint32_t len;

	if (sai_stop == true)																						// if sai transmit is completed
		return;

	len = (sai_qbuf_len + sai_qbuf_in - sai_qbuf_out) % sai_qbuf_len;											// sai queue buffer length check

	if (len > 0)																								// if sound data exists in sai queue buffer,
	{
		//HAL_SAI_Transmit_DMA(hsai, (uint8_t*)&sai_q_buf[sai_qbuf_out*sai_q_buf_len], sai_q_buf_len * 2);		// sai DMA trasmit start from next frame
		BSP_RunSAIDMA(BSP_SAI1, (uint8_t*)&sai_q_buf[sai_qbuf_out*sai_q_buf_len], &sai_r_buf_dummy,  sai_q_buf_len * 2, BSP_SAI_TRANSMIT_DMA);
		if (sai_qbuf_out != sai_qbuf_in)
		   sai_qbuf_out = (sai_qbuf_out + 1) % sai_qbuf_len;													// queue buffer index increase
	}
	else
		//HAL_SAI_Transmit_DMA(hsai, (uint8_t*)sai_q_buf_zero, sai_q_buf_len * 2);								// first transmit
		BSP_RunSAIDMA(BSP_SAI1, (uint8_t*)sai_q_buf_zero, &sai_r_buf_dummy,  sai_q_buf_len * 2, BSP_SAI_TRANSMIT_DMA);
}


/**
 *------------------------------------------------------------
 *                      STATIC FUNCTIONS
 *------------------------------------------------------------
 * @brief Functions intended for internal use within this module.
 */

#ifdef _USE_STM32H7_DCACHE
static void DcacheForcedWT_Enable(void)
{
    _CACHE_CACR |= _CACHE_FORCEWT;
}

static void DcacheForcedWT_Disable(void)
{
    _CACHE_CACR &= ~(_CACHE_FORCEWT);
}
#endif

#endif
