/*
 * IOIF_Audio_WavPlay_SAI.h
 *
 *  Created on: Sep 19, 2023
 *      Author: Angelrobotics
 */

#ifndef WAVPLAYER_SAI_INC_IOIF_AUDIO_WAVPLAY_SAI_H_
#define WAVPLAYER_SAI_INC_IOIF_AUDIO_WAVPLAY_SAI_H_

#include "module.h"

#ifdef IOIF_AUDIO_WAV_SAI_ENABLED

#include "stdbool.h"
#include "string.h"
#include "bsp_sai.h"
#include "fatfs.h"
#include "bsp_sdmmc.h"
#include "ioif_usb_common.h"

/** @defgroup AUDIO WAV SAI IOIF
  * @brief AUDIO WAV SAI driver
  * @{
  */


/**
 *-----------------------------------------------------------
 *              MACROS AND PREPROCESSOR DIRECTIVES
 *-----------------------------------------------------------
 * @brief Directives and macros for readability and efficiency.
 */

#define SAI_BUF_LEN   (1024*4)										// SAI Buffer length : 1024 * 4 bytes
#define SAI_BUF_MS    (10)											// SAI sampling duration : 10ms
#define SAI_SAMPLE_FREQ		16000



#ifdef _USE_STM32H7_DCACHE
#define _CACHE_FORCEWT		(1<<2)
#define _CACHE_CACR			(*(volatile uint32_t*) (0xE000EF9C))
#endif

/**
 *------------------------------------------------------------
 *                     TYPE DECLARATIONS
 *------------------------------------------------------------
 * @brief Custom data types and structures for the module.
 */

typedef struct _IOIF_wav_audio_ch_t
{
  int16_t left;
  int16_t right;
} audio_ch_t;

typedef struct wavfile_header_s
{
   /* RIFF */
   uint8_t ChunkID[4];       //4		"RIFF"
   int32_t ChunkSize;        //8		File Size = Chunk Size + 8byte (need to add Chunk ID 4byte, Chunk size 4byte)
   uint8_t Format[4];        //12		"WAVE"

   /* FMT Sub-Chunk */
   uint8_t Subchunk1ID[4];   //16      "FMT + 'space'"
   int32_t Subchunk1Size;    //20		FMT sub-chunk size
   int16_t AudioFormat;      //22   	1: PCM/Uncompressed, 2: MS ADPCM, ... , 80: MPEG, etc..
   int16_t NumChannels;      //24   	1: MONO, 2: STEREO, 3: left, center, right, 4:..., etc...
   int32_t SampleRate;       //28      Audio Sampling Rate
   int32_t ByteRate;         //32		Average Bytes Per Second
   int16_t BlockAlign;       //34  	Sample Frame size
   int16_t BitsPerSample;    //36		Bit Per Sample, 8bit/16bit/32bit...

   /* Data Sub-Chunk */
   uint8_t Subchunk2ID[4];	  //40		"data"
   int32_t Subchunk2Size;    //44 		Data Size
} wavfile_header_t;		  // Total : 44 bytes

typedef enum _IOIF_WavPlayState_t {
    IOIF_WAVPLAY_STATUS_OK = 0,
    IOIF_WAVPLAY_STATUS_ERROR,
} IOIF_WavPlayState_t;


typedef enum _IOIF_WavPlayerType_t {
	IOIF_WAVPLAY_SD = 0,
	IOIF_WAVPLAY_USB = 1,
	IOIF_WAVPLAY_RAM = 2
} IOIF_WavPlayerType_t;



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


/* Player Init. & Deinit. */
IOIF_WavPlayState_t WavAudio_FS_Init(uint8_t* DrivePath, IOIF_WavPlayerType_t IoType);
IOIF_WavPlayState_t WavAudio_FS_DeInit(uint8_t* DrivePath);

/* Get Wavefile Header Info. */
IOIF_WavPlayState_t GetWavFileInfo(uint8_t* DrivePath, uint8_t* filename, wavfile_header_t* wav_header);

/* Play Wavefile */
IOIF_WavPlayState_t PlayWaveFile(uint8_t* DrivePath, uint8_t* filename);


/* SAI TX Complete Callback */
void SaiAudioDMATxCB(void* param);

/* D-Cache Write Through Forced Mode for Cache Coherence */


#endif /* MODULE : IOIF_AUDIO_WAV_SAI_ENABLED */

#endif /* WAVPLAYER_SAI_INC_IOIF_AUDIO_WAVPLAY_SAI_H_ */
