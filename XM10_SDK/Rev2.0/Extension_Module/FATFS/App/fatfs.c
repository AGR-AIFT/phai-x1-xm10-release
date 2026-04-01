/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "fatfs.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  // [?��?��] 기존?�� ?���? ?��결해 ?��?���?(?��: USER_Driver), 강제�? ?��?��?�� ?��리�?? 만듭?��?��.
  FATFS_UnLinkDriver(USERPath);
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  /* [Phase 3b M1] Cached Time 패턴: DataLoggerTask가 10초 주기로 RTC 읽어 캐시.
   * get_fattime()은 캐시된 값 즉시 반환 (<1us, SPI 호출 없음).
   * 캐시 미초기화 시(0) fallback: 2025-01-01 00:00:00 */
  extern DWORD DataLogger_GetCachedFatTime(void);
  DWORD t = DataLogger_GetCachedFatTime();
  return (t != 0) ? t : ((DWORD)(2025 - 1980) << 25 | 1 << 21 | 1 << 16);
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */
