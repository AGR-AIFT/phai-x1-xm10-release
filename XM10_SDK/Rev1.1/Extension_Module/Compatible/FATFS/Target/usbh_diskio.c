/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usbh_diskio.c (based on usbh_diskio_template.c v2.0.2)
  * @brief   USB Host Disk I/O driver
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
#include "main.h" // [신규] SCB_InvalidateDCache_by_Addr 등을 사용하기 위해 추가
#include "usbh_msc.h"  // USBH_MSC_GetState 사용
#include "ioif_agrb_dwt.h"  // [Diag] IOIF_DWT_GetCycles / CyclesToUs
#include <stdbool.h>       // [2026-04-18] burst tracking
/* USER CODE END Header */
/* USER CODE BEGIN firstSection */
/* can be used to modify / undefine following code or add new definitions */
/* USER CODE END firstSection */

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "usbh_diskio.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define USB_DEFAULT_BLOCK_SIZE 512
#define USBH_DISKIO_TIMEOUT_MS 5000

/**
 * @brief [Diag] polling loop 진입/iteration + USBH_MSC_Write DWT 실측 통계.
 * @details
 *   DataLoggerTask(Prio Normal) 단일 컨텍스트에서만 업데이트되므로 atomic 불필요.
 *   GetDiag/ResetDiag는 세션 종료 시점에만 호출 → race 없음.
 */
static volatile uint32_t s_diag_polling_entries   = 0;
static volatile uint32_t s_diag_polling_iters_max = 0;
static volatile uint32_t s_diag_write_us_max      = 0;
static volatile uint64_t s_diag_write_us_sum      = 0;
static volatile uint32_t s_diag_write_call_count  = 0;

/* [2026-04-18] Write burst 추적 — 연속 write (gap < 5ms) 의 최대 지속시간.
 * Hot Buffer margin 요구치 도출용. DataLoggerTask 단일 컨텍스트 → atomic 불필요. */
#define USBH_BURST_GAP_THRESHOLD_US  (5000U)
static volatile uint32_t s_diag_burst_start_cyc   = 0;
static volatile uint32_t s_diag_burst_last_end_cyc = 0;
static volatile uint32_t s_diag_burst_max_us      = 0;
static volatile bool     s_diag_burst_active      = false;

/** write 1회 완료 시점에 호출. burst 연장 또는 갱신. */
static inline void _usbh_diag_update_burst(uint32_t write_start_cyc, uint32_t write_end_cyc)
{
    if (s_diag_burst_active) {
        uint32_t gap_us = IOIF_DWT_CyclesToUs(write_start_cyc - s_diag_burst_last_end_cyc);
        if (gap_us >= USBH_BURST_GAP_THRESHOLD_US) {
            /* Gap 커서 burst 종료. 새 burst 시작. */
            s_diag_burst_start_cyc = write_start_cyc;
        }
    } else {
        s_diag_burst_start_cyc = write_start_cyc;
        s_diag_burst_active = true;
    }
    s_diag_burst_last_end_cyc = write_end_cyc;
    uint32_t burst_us = IOIF_DWT_CyclesToUs(write_end_cyc - s_diag_burst_start_cyc);
    if (burst_us > s_diag_burst_max_us) s_diag_burst_max_us = burst_us;
}

/**
 * @brief polling loop 진입 시 카운터 증분 + iter 최대값 갱신.
 * @note osDelay(1) 도달이 dead code 가설 검증용. entries==0 이면 확정.
 */
#define USBH_DISKIO_POLL_DIAG_BEGIN()   uint32_t _diag_iter = 0;
#define USBH_DISKIO_POLL_DIAG_TICK()    do { \
    if (_diag_iter == 0) { s_diag_polling_entries++; } \
    _diag_iter++; \
} while (0)
#define USBH_DISKIO_POLL_DIAG_END()     do { \
    if (_diag_iter > s_diag_polling_iters_max) { \
        s_diag_polling_iters_max = _diag_iter; \
    } \
} while (0)

/* Private variables ---------------------------------------------------------*/
/*
 * D-Cache strategy: scratch buffer uses aligned(32) + manual Cache ops.
 * Ideally placed in Non-Cacheable region (.dma_noncache) to eliminate Cache ops,
 * but current approach (SCB_CleanDCache_by_Addr / SCB_InvalidateDCache_by_Addr
 * + 32-byte aligned) is functionally correct. Consider Non-Cacheable placement
 * if MPU configuration is revisited.
 */
__attribute__((aligned(32)))
static DWORD scratch[_MAX_SS / 4];
extern USBH_HandleTypeDef  hUSB_Host;

/* Private function prototypes -----------------------------------------------*/
DSTATUS USBH_initialize (BYTE);
DSTATUS USBH_status (BYTE);
DRESULT USBH_read (BYTE, BYTE*, DWORD, UINT);

#if _USE_WRITE == 1
  DRESULT USBH_write (BYTE, const BYTE*, DWORD, UINT);
#endif /* _USE_WRITE == 1 */

#if _USE_IOCTL == 1
  DRESULT USBH_ioctl (BYTE, BYTE, void*);
#endif /* _USE_IOCTL == 1 */

const Diskio_drvTypeDef  USBH_Driver =
{
  USBH_initialize,
  USBH_status,
  USBH_read,
#if  _USE_WRITE == 1
  USBH_write,
#endif /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USBH_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* USER CODE BEGIN beforeFunctionSection */
/* can be used to modify / undefine following code or add new code */
/* USER CODE END beforeFunctionSection */

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  lun : lun id
  * @retval DSTATUS: Operation status
  */
DSTATUS USBH_initialize(BYTE lun)
{
  /* CAUTION : USB Host library has to be initialized in the application */

  return RES_OK;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : lun id
  * @retval DSTATUS: Operation status
  */
DSTATUS USBH_status(BYTE lun)
{
  DRESULT res = RES_ERROR;

  if(USBH_MSC_UnitIsReady(&hUSB_Host, lun))
  {
    res = RES_OK;
  }
  else
  {
    res = RES_ERROR;
  }

  return res;
}

/* USER CODE BEGIN beforeReadSection */
/* can be used to modify previous code / undefine following code / add new code */
/* USER CODE END beforeReadSection */

/**
  * @brief  Reads Sector(s)
  * @param  lun : lun id
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USBH_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
  MSC_LUNTypeDef info;
  USBH_StatusTypeDef  status = USBH_OK;

  if (((DWORD)buff & 3) && (((HCD_HandleTypeDef *)hUSB_Host.pData)->Init.dma_enable))
  {
    while ((count--)&&(status == USBH_OK))
    {
      status = USBH_MSC_Read(&hUSB_Host, lun, sector + count, (uint8_t *)scratch, 1);

      /* USER CODE BEGIN 4 */
      // [신규] 완료될 때까지 대기 (Blocking)
      {
          uint32_t timeout_count = USBH_DISKIO_TIMEOUT_MS;
          USBH_DISKIO_POLL_DIAG_BEGIN();
          while(status == USBH_OK && USBH_MSC_GetState(&hUSB_Host) != MSC_IDLE)
          {
              USBH_DISKIO_POLL_DIAG_TICK();
              osDelay(1);
              if (--timeout_count == 0) { status = USBH_FAIL; break; }
          }
          USBH_DISKIO_POLL_DIAG_END();
      }
      /* USER CODE END 4 */

      if(status == USBH_OK)
      {
        /* USER CODE BEGIN 5 */
        /* [핵심] DMA가 RAM(scratch)을 변경했으니, CPU가 읽기 전에 D-Cache를 무효화(Invalidate) */
        SCB_InvalidateDCache_by_Addr((uint32_t*)scratch, sizeof(scratch));
        /* USER CODE END 5 */
        memcpy (&buff[count * _MAX_SS] ,scratch, _MAX_SS);
      }
      else
      {
        break;
      }
    }
  }
  else
  {
    status = USBH_MSC_Read(&hUSB_Host, lun, sector, buff, count);
    /* USER CODE BEGIN 6 */
    // [핵심 수정] 작업이 완료될 때까지 대기 (Blocking)
    // MSC 상태가 'MSC_READ'에서 다시 'MSC_IDLE'로 돌아올 때까지 기다립니다.
    {
          uint32_t timeout_count = USBH_DISKIO_TIMEOUT_MS;
          USBH_DISKIO_POLL_DIAG_BEGIN();
          while(status == USBH_OK && USBH_MSC_GetState(&hUSB_Host) != MSC_IDLE)
          {
              USBH_DISKIO_POLL_DIAG_TICK();
              osDelay(1);
              if (--timeout_count == 0) { status = USBH_FAIL; break; }
          }
          USBH_DISKIO_POLL_DIAG_END();
      }
    /* USER CODE END 6 */
  }

  if(status == USBH_OK)
  {
    res = RES_OK;
  }
  else
  {
    /* [Fix] GetLUNInfo 반환값 확인: USB 분리 시 USBH_FAIL 반환 → info 미초기화 접근 방지 */
    if (USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info) == USBH_OK)
    {
      switch (info.sense.asc)
      {
      case SCSI_ASC_LOGICAL_UNIT_NOT_READY:
      case SCSI_ASC_MEDIUM_NOT_PRESENT:
      case SCSI_ASC_NOT_READY_TO_READY_CHANGE:
        USBH_ErrLog ("USB Disk is not ready!");
        res = RES_NOTRDY;
        break;

      default:
        res = RES_ERROR;
        break;
      }
    }
  }

  return res;
}

/* USER CODE BEGIN beforeWriteSection */
/* can be used to modify previous code / undefine following code / add new code */
/* USER CODE END beforeWriteSection */

/**
  * @brief  Writes Sector(s)
  * @param  lun : lun id
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USBH_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
  MSC_LUNTypeDef info;
  USBH_StatusTypeDef  status = USBH_OK;

  if (((DWORD)buff & 3) && (((HCD_HandleTypeDef *)hUSB_Host.pData)->Init.dma_enable))
  {

    while (count--)
    {
      memcpy (scratch, &buff[count * _MAX_SS], _MAX_SS);
      /* USER CODE BEGIN 7 */
      /* [핵심] CPU가 D-Cache(scratch)에 썼으니, DMA가 RAM에서 읽기 전에 캐시를 청소(Clean) */
      SCB_CleanDCache_by_Addr((uint32_t*)scratch, sizeof(scratch));
      /* USER CODE END 7 */
      
      /* [Diag] USBH_MSC_Write DWT 실측 — tight spin 시간 측정 */
      uint32_t _diag_write_start = IOIF_DWT_GetCycles();
      status = USBH_MSC_Write(&hUSB_Host, lun, sector + count, (BYTE *)scratch, 1) ;
      uint32_t _diag_write_end = IOIF_DWT_GetCycles();
      uint32_t _diag_write_us = IOIF_DWT_CyclesToUs(_diag_write_end - _diag_write_start);
      s_diag_write_call_count++;
      s_diag_write_us_sum += _diag_write_us;
      if (_diag_write_us > s_diag_write_us_max) { s_diag_write_us_max = _diag_write_us; }
      _usbh_diag_update_burst(_diag_write_start, _diag_write_end);

      /* USER CODE BEGIN 8 */
      // [신규] 완료 대기
      {
          uint32_t timeout_count = USBH_DISKIO_TIMEOUT_MS;
          USBH_DISKIO_POLL_DIAG_BEGIN();
          while(status == USBH_OK && USBH_MSC_GetState(&hUSB_Host) != MSC_IDLE)
          {
              USBH_DISKIO_POLL_DIAG_TICK();
              osDelay(1);
              if (--timeout_count == 0) { status = USBH_FAIL; break; }
          }
          USBH_DISKIO_POLL_DIAG_END();
      }
      /* USER CODE END 8 */

      if(status == USBH_FAIL)
      {
        break;
      }
    }
  }
  else
  {
    /* [Diag] USBH_MSC_Write DWT 실측 — tight spin 시간 측정 */
    uint32_t _diag_write_start = IOIF_DWT_GetCycles();
    status = USBH_MSC_Write(&hUSB_Host, lun, sector, (BYTE *)buff, count);
    uint32_t _diag_write_end = IOIF_DWT_GetCycles();
    uint32_t _diag_write_us = IOIF_DWT_CyclesToUs(_diag_write_end - _diag_write_start);
    s_diag_write_call_count++;
    s_diag_write_us_sum += _diag_write_us;
    if (_diag_write_us > s_diag_write_us_max) { s_diag_write_us_max = _diag_write_us; }
    _usbh_diag_update_burst(_diag_write_start, _diag_write_end);

    /* USER CODE BEGIN 9 */
    // [핵심 수정] 작업 완료 대기 (Blocking)
    {
          uint32_t timeout_count = USBH_DISKIO_TIMEOUT_MS;
          USBH_DISKIO_POLL_DIAG_BEGIN();
          while(status == USBH_OK && USBH_MSC_GetState(&hUSB_Host) != MSC_IDLE)
          {
              USBH_DISKIO_POLL_DIAG_TICK();
              osDelay(1);
              if (--timeout_count == 0) { status = USBH_FAIL; break; }
          }
          USBH_DISKIO_POLL_DIAG_END();
      }
    /* USER CODE END 9 */
  }

  if(status == USBH_OK)
  {
    res = RES_OK;
  }
  else
  {
    /* [Fix] GetLUNInfo 반환값 확인: USB 분리 시 USBH_FAIL 반환 → info 미초기화 접근 방지 */
    if (USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info) == USBH_OK)
    {
      switch (info.sense.asc)
      {
      case SCSI_ASC_WRITE_PROTECTED:
        USBH_ErrLog("USB Disk is Write protected!");
        res = RES_WRPRT;
        break;

      case SCSI_ASC_LOGICAL_UNIT_NOT_READY:
      case SCSI_ASC_MEDIUM_NOT_PRESENT:
      case SCSI_ASC_NOT_READY_TO_READY_CHANGE:
        USBH_ErrLog("USB Disk is not ready!");
        res = RES_NOTRDY;
        break;

      default:
        res = RES_ERROR;
        break;
      }
    }
  }

  return res;
}
#endif /* _USE_WRITE == 1 */

/* USER CODE BEGIN beforeIoctlSection */
/* can be used to modify previous code / undefine following code / add new code */
/* USER CODE END beforeIoctlSection */

/**
  * @brief  I/O control operation
  * @param  lun : lun id
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USBH_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  MSC_LUNTypeDef info;

  switch (cmd)
  {
  /* Make sure that no pending write process */
  case CTRL_SYNC:
    res = RES_OK;
    break;

  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT :
    if(USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info) == USBH_OK)
    {
      *(DWORD*)buff = info.capacity.block_nbr;
      res = RES_OK;
    }
    else
    {
      res = RES_ERROR;
    }
    break;

  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE :
    if(USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info) == USBH_OK)
    {
      *(DWORD*)buff = info.capacity.block_size;
      res = RES_OK;
    }
    else
    {
      res = RES_ERROR;
    }
    break;

    /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE :

    if(USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info) == USBH_OK)
    {
      *(DWORD*)buff = info.capacity.block_size / USB_DEFAULT_BLOCK_SIZE;
      res = RES_OK;
    }
    else
    {
      res = RES_ERROR;
    }
    break;

  default:
    res = RES_PARERR;
  }

  return res;
}
#endif /* _USE_IOCTL == 1 */

/* USER CODE BEGIN lastSection */
/**
 * @brief [Diag] USBH_MSC polling/write 통계 스냅샷 읽기.
 * @note  DataLoggerTask 종료 경로에서만 호출 → atomic 불필요.
 */
void USBH_DiskIO_GetDiag(USBH_DiskIO_Diag_t* diag)
{
    if (diag == NULL) { return; }
    diag->polling_entries   = s_diag_polling_entries;
    diag->polling_iters_max = s_diag_polling_iters_max;
    diag->write_us_max      = s_diag_write_us_max;
    diag->write_us_sum      = s_diag_write_us_sum;
    diag->write_call_count  = s_diag_write_call_count;
    diag->write_burst_max_us = s_diag_burst_max_us;
}

/**
 * @brief [Diag] 통계 초기화. 세션 시작 시 호출.
 */
void USBH_DiskIO_ResetDiag(void)
{
    s_diag_polling_entries    = 0;
    s_diag_polling_iters_max  = 0;
    s_diag_write_us_max       = 0;
    s_diag_write_us_sum       = 0;
    s_diag_write_call_count   = 0;
    s_diag_burst_start_cyc    = 0;
    s_diag_burst_last_end_cyc = 0;
    s_diag_burst_max_us       = 0;
    s_diag_burst_active       = false;
}
/* USER CODE END lastSection */
