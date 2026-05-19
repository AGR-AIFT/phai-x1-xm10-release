/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usbh_diskio.h (based on usbh_diskio_template.h v2.0.2)
  * @brief   Header for usbh_diskio.c module
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_DISKIO_H
#define __USBH_DISKIO_H

/* USER CODE BEGIN firstSection */
/* can be used to modify / undefine following code or add new definitions */
/* USER CODE END firstSection */

/* Includes ------------------------------------------------------------------*/
#include "usbh_core.h"
#include "usbh_msc.h"
#include "ff_gen_drv.h"
#include <stdint.h>
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
extern const Diskio_drvTypeDef  USBH_Driver;

/* USER CODE BEGIN lastSection */
/**
 * @brief USBH_MSC polling/write 진단 통계.
 * @details
 *  polling_entries   : usbh_diskio polling loop 진입 횟수 (0 이면 dead code 확정)
 *  polling_iters_max : 단일 loop 최대 iteration 수
 *  write_us_max      : USBH_MSC_Write 1회 최대 소요 시간 [µs] (tight spin 실측)
 *  write_us_sum      : USBH_MSC_Write 누적 시간 [µs] (CPU 독점 총합)
 *  write_call_count  : USBH_MSC_Write 호출 횟수 (평균 계산용)
 */
typedef struct {
    uint32_t polling_entries;
    uint32_t polling_iters_max;
    uint32_t write_us_max;
    uint64_t write_us_sum;
    uint32_t write_call_count;
    /* [2026-04-18] 연속 write burst 지속시간 — Hot Buffer margin 요구치.
     * burst = 직전 write end ~ 다음 write start gap < 5ms 로 묶인 연속 구간.
     * max = session 내 가장 긴 burst. C안(Cold 제거) 결정 근거. */
    uint32_t write_burst_max_us;
} USBH_DiskIO_Diag_t;

void USBH_DiskIO_GetDiag(USBH_DiskIO_Diag_t* diag);
void USBH_DiskIO_ResetDiag(void);
/* USER CODE END lastSection */

#endif /* __USBH_DISKIO_H */

