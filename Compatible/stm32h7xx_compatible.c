#include "stm32h7xx_compatible.h"
#include "main.h"
// #include "ioif_agrb_usb.h" // [신규] 전역 플래그를 가져오기 위해 포함

#include <stdbool.h>

extern HCD_HandleTypeDef hhcd_USB_OTG_FS;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* [수정] IOIF 계층의 전역 상태 변수 참조 */
// extern volatile bool g_is_host_initialized;
// extern volatile bool g_is_device_initialized;
/**
  * @brief This function handles USB On The Go FS global interrupt.
  */
/**
  * @brief This function handles USB On The Go FS End Point 1 Out global interrupt.
  */
void OTG_FS_EP1_OUT_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_FS_EP1_OUT_IRQn 0 */
  /* [수정] 현재 활성화된 핸들러만 선택적으로 호출 */
  /* [핵심 수정] 하드웨어 레지스터(GINTSTS)의 CMOD 비트를 확인합니다.
   * CMOD == 1 : Host Mode
   * CMOD == 0 : Device Mode
   */
  if ((USB_OTG_FS->GINTSTS & USB_OTG_GINTSTS_CMOD) == USB_OTG_GINTSTS_CMOD)
  {
      // Host Mode Interrupt
      // (방어 코드: 핸들이 초기화되었을 때만 호출)
      if (hhcd_USB_OTG_FS.pData != NULL) {
          HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS);
      }
  }
  else
  {
      // Device Mode Interrupt
      // (방어 코드: 핸들이 초기화되었을 때만 호출)
      if (hpcd_USB_OTG_FS.pData != NULL) {
          HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
      }
  }
  /* USER CODE END OTG_FS_EP1_OUT_IRQn 0 */
  /* [삭제] HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS); */
  /* [삭제] HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); */
  /* USER CODE BEGIN OTG_FS_EP1_OUT_IRQn 1 */

  /* USER CODE END OTG_FS_EP1_OUT_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go FS End Point 1 In global interrupt.
  */
void OTG_FS_EP1_IN_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_FS_EP1_IN_IRQn 0 */
  /* [수정] 현재 활성화된 핸들러만 선택적으로 호출 */
  /* [핵심 수정] 하드웨어 레지스터(GINTSTS)의 CMOD 비트를 확인합니다.
   * CMOD == 1 : Host Mode
   * CMOD == 0 : Device Mode
   */
  if ((USB_OTG_FS->GINTSTS & USB_OTG_GINTSTS_CMOD) == USB_OTG_GINTSTS_CMOD)
  {
      // Host Mode Interrupt
      // (방어 코드: 핸들이 초기화되었을 때만 호출)
      if (hhcd_USB_OTG_FS.pData != NULL) {
          HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS);
      }
  }
  else
  {
      // Device Mode Interrupt
      // (방어 코드: 핸들이 초기화되었을 때만 호출)
      if (hpcd_USB_OTG_FS.pData != NULL) {
          HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
      }
  }
  /* USER CODE END OTG_FS_EP1_IN_IRQn 0 */
  /* [삭제] HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS); */
  /* [삭제] HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); */
  /* USER CODE BEGIN OTG_FS_EP1_IN_IRQn 1 */

  /* USER CODE END OTG_FS_EP1_IN_IRQn 1 */
}

/**
  * @brief This function handles USB On The Go FS global interrupt.
  */
void OTG_FS_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_FS_IRQn 0 */
  /* [수정] 현재 활성화된 핸들러만 선택적으로 호출 */
  /* [핵심 수정] 하드웨어 레지스터(GINTSTS)의 CMOD 비트를 확인합니다.
   * CMOD == 1 : Host Mode
   * CMOD == 0 : Device Mode
   */
  if ((USB_OTG_FS->GINTSTS & USB_OTG_GINTSTS_CMOD) == USB_OTG_GINTSTS_CMOD)
  {
      // Host Mode Interrupt
      // (방어 코드: 핸들이 초기화되었을 때만 호출)
      if (hhcd_USB_OTG_FS.pData != NULL) {
          HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS);
      }
  }
  else
  {
      // Device Mode Interrupt
      // (방어 코드: 핸들이 초기화되었을 때만 호출)
      if (hpcd_USB_OTG_FS.pData != NULL) {
          HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
      }
  }
  /* USER CODE END OTG_FS_IRQn 0 */
  /* [삭제] HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS); */
  /* [삭제] HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); */
  /* USER CODE BEGIN OTG_FS_IRQn 1 */

  /* USER CODE END OTG_FS_IRQn 1 */
}
 