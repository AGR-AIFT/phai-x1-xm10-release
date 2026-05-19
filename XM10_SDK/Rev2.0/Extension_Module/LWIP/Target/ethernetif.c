/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : ethernetif.c
  * Description        : This file provides code for the configuration
  *                      of the ethernetif.c MiddleWare.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "ethernetif.h"
/* USER CODE BEGIN Include for User BSP */
#include "ioif_agrb_eth_mdio.h"
#include "rtl8201f.h"
/* USER CODE END Include for User BSP */
#include <string.h>
#include "cmsis_os.h"
#include "lwip/tcpip.h"

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Private define ------------------------------------------------------------*/
/* The time to block waiting for input. */
#define TIME_WAITING_FOR_INPUT ( osWaitForever )
/* Time to block waiting for transmissions to finish */
#define ETHIF_TX_TIMEOUT (2000U)
/* USER CODE BEGIN OS_THREAD_STACK_SIZE_WITH_RTOS */
/* Stack size of the interface thread */
#define INTERFACE_THREAD_STACK_SIZE ( 2048 )
/* USER CODE END OS_THREAD_STACK_SIZE_WITH_RTOS */
/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

/* ETH Setting  */
#define ETH_DMA_TRANSMIT_TIMEOUT               ( 20U )
#define ETH_TX_BUFFER_MAX             ((ETH_TX_DESC_CNT) * 2U)
/* ETH_RX_BUFFER_SIZE parameter is defined in lwipopts.h */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Private variables ---------------------------------------------------------*/
/*
@Note: This interface is implemented to operate in zero-copy mode only:
        - Rx Buffers will be allocated from LwIP stack Rx memory pool,
          then passed to ETH HAL driver.
        - Tx Buffers will be allocated from LwIP stack memory heap,
          then passed to ETH HAL driver.

@Notes:
  1.a. ETH DMA Rx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_RX_DESC_CNT in ETH GUI (Rx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h
  1.b. ETH DMA Tx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_TX_DESC_CNT in ETH GUI (Tx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h

  2.a. Rx Buffers number must be between ETH_RX_DESC_CNT and 2*ETH_RX_DESC_CNT
  2.b. Rx Buffers must have the same size: ETH_RX_BUFFER_SIZE, this value must
       passed to ETH DMA in the init field (heth.Init.RxBuffLen)
  2.c  The RX Ruffers addresses and sizes must be properly defined to be aligned
       to L1-CACHE line size (32 bytes).
*/

/* Data Type Definitions */
typedef enum
{
  RX_ALLOC_OK       = 0x00,
  RX_ALLOC_ERROR    = 0x01
} RxAllocStatusTypeDef;

typedef struct
{
  struct pbuf_custom pbuf_custom;
  uint8_t buff[(ETH_RX_BUFFER_SIZE + 31) & ~31] __ALIGNED(32);
} RxBuff_t;

/* Memory Pool Declaration */
#define ETH_RX_BUFFER_CNT             12U
LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");

/* Variable Definitions */
static volatile uint8_t RxAllocStatus;
#if defined ( __ICCARM__ ) /*!< IAR Compiler */

#pragma location=0x30000000
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
#pragma location=0x30000080
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */

__attribute__((at(0x30000000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x30000080))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __GNUC__ ) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDescripSection"))); /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDescripSection")));   /* Ethernet Tx DMA Descriptors */

#endif

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma location = 0x30000100
extern u8_t memp_memory_RX_POOL_base[];

#elif defined ( __CC_ARM ) /* MDK ARM Compiler */
__attribute__((section(".Rx_PoolSection"))) extern u8_t memp_memory_RX_POOL_base[];

#elif defined ( __GNUC__ ) /* GNU */
__attribute__((section(".Rx_PoolSection"))) extern u8_t memp_memory_RX_POOL_base[];
#endif

/* USER CODE BEGIN 2 */
/* IOIF MDIO Instance ID (Singleton — ethernetif 전용) */
static IOIF_ETH_MDIOx_t s_mdio_id = IOIF_ETH_MDIO_NOT_ALLOCATED;
/* USER CODE END 2 */

osSemaphoreId_t RxPktSemaphore = NULL;   /* Semaphore to signal incoming packets */
osSemaphoreId_t TxPktSemaphore = NULL;   /* Semaphore to signal transmit packet complete */

/* Global Ethernet handle */
ETH_HandleTypeDef heth;

/* Private function prototypes -----------------------------------------------*/

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */

/* Private functions ---------------------------------------------------------*/
void pbuf_free_custom(struct pbuf *p);

/**
  * @brief  Ethernet Rx Transfer completed callback
  * @param  handlerEth: ETH handler
  * @retval None
  */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *handlerEth)
{
  { extern volatile uint32_t g_diag_eth_rx_cnt; g_diag_eth_rx_cnt++; }
  if (RxPktSemaphore != NULL) {
    osSemaphoreRelease(RxPktSemaphore);
  }
}
/**
  * @brief  Ethernet Tx Transfer completed callback
  * @param  handlerEth: ETH handler
  * @retval None
  */
void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *handlerEth)
{
  { extern volatile uint32_t g_diag_eth_tx_cnt; g_diag_eth_tx_cnt++; }
  if (TxPktSemaphore != NULL) {
    osSemaphoreRelease(TxPktSemaphore);
  }
}
/**
  * @brief  Ethernet DMA transfer error callback
  * @param  handlerEth: ETH handler
  * @retval None
  */
void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *handlerEth)
{
  { extern volatile uint32_t g_diag_eth_err_cnt; g_diag_eth_err_cnt++; }
  if((HAL_ETH_GetDMAError(handlerEth) & ETH_DMACSR_RBU) == ETH_DMACSR_RBU)
  {
    if (RxPktSemaphore != NULL) {
      osSemaphoreRelease(RxPktSemaphore);
    }
  }
}

/* USER CODE BEGIN 4 */
/* SI stimulus 용 MDIO ID getter — xm_periph_stimulus.c 가 평시 path 와 동일
 * IOIF MDIO 인스턴스를 공유 (별도 assign 불필요). low_level_init 미완 / 실패
 * 시 IOIF_ETH_MDIO_NOT_ALLOCATED 반환 → caller 가 skip. */
IOIF_ETH_MDIOx_t XM_Ethernetif_GetMdioId(void) { return s_mdio_id; }

/* [2026-05-14] LAN ping/ARP/평시 트래픽 수신 카운터 — RMII/MDIO stimulus 가
 * polling 으로 "LAN 연결 + 트래픽 활동" 표시. ethernetif_input task 가 매번
 * RX pbuf 처리 시 ++ (single writer). sensor-studio 가 1Hz polling 으로
 * 이전 값 대비 증가 여부 확인 → "ping 응답 중" 직관 표시. */
volatile uint32_t g_eth_rx_packet_count = 0u;
/* USER CODE END 4 */

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH)
*******************************************************************************/
/**
 * @brief In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
  HAL_StatusTypeDef hal_eth_init_status = HAL_OK;
/* USER CODE BEGIN OS_THREAD_ATTR_CMSIS_RTOS_V2 */
  osThreadAttr_t attributes;
/* USER CODE END OS_THREAD_ATTR_CMSIS_RTOS_V2 */
/* USER CODE BEGIN low_level_init Variables Initialization for User BSP */

/* USER CODE END low_level_init Variables Initialization for User BSP */
  /* Start ETH HAL Init */

   uint8_t MACAddr[6] ;
  heth.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  heth.Init.RxBuffLen = 1536;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  hal_eth_init_status = HAL_ETH_Init(&heth);

  /* [진단] MspInit→Init 직후 PMCR 캡처 */
  extern volatile uint32_t g_diag_pmcr_after_mspinit;
  g_diag_pmcr_after_mspinit = SYSCFG->PMCR;

  /* End ETH HAL Init */

  /* Initialize the RX POOL */
  LWIP_MEMPOOL_INIT(RX_POOL);

#if LWIP_ARP || LWIP_ETHERNET
  /* set MAC hardware address length */
  netif->hwaddr_len = ETH_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] =  heth.Init.MACAddr[0];
  netif->hwaddr[1] =  heth.Init.MACAddr[1];
  netif->hwaddr[2] =  heth.Init.MACAddr[2];
  netif->hwaddr[3] =  heth.Init.MACAddr[3];
  netif->hwaddr[4] =  heth.Init.MACAddr[4];
  netif->hwaddr[5] =  heth.Init.MACAddr[5];

  /* maximum transfer unit */
  netif->mtu = ETH_MAX_PAYLOAD;

  /* Accept broadcast address and ARP traffic */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  #if LWIP_ARP
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
  #else
    netif->flags |= NETIF_FLAG_BROADCAST;
  #endif /* LWIP_ARP */

  /* create a binary semaphore used for informing ethernetif of frame reception */
  RxPktSemaphore = osSemaphoreNew(1, 0, NULL);

  /* create a binary semaphore used for informing ethernetif of frame transmission */
  TxPktSemaphore = osSemaphoreNew(1, 0, NULL);

  /* create the task that handles the ETH_MAC */
/* USER CODE BEGIN OS_THREAD_NEW_CMSIS_RTOS_V2 */
  memset(&attributes, 0x0, sizeof(osThreadAttr_t));
  attributes.name = "EthIf";
  attributes.stack_size = INTERFACE_THREAD_STACK_SIZE;
  attributes.priority = osPriorityRealtime;
  osThreadNew(ethernetif_input, netif, &attributes);
/* USER CODE END OS_THREAD_NEW_CMSIS_RTOS_V2 */

/* USER CODE BEGIN low_level_init Code 1 for User BSP */

    /* === [V2] RTL8201F PHY Init via IOIF ETH MDIO ===
     *
     * Init 순서:
     *   1. HAL_ETH_Init (위, CubeMX 자동생성) — PHY 전원ON 기본 REF_CLK로 DMA SWR 성공
     *   2. IOIF MDIO assign (HAL Handle DI)
     *   3. RTL8201F_Init — SW Reset + Page 7 RMII 타이밍 + 2s 안정화
     *   4. Link state → MAC config → Start (Code 2에서 수행)
     *
     * Note: PHY HW Reset 불필요 — 전원ON 시 strap pin 기반 기본 설정으로 충분.
     *       HAL_ETH_Init 후 HW Reset 수행 시 REF_CLK 중단 → MAC RX 불능. */

    /* 1. IOIF ETH MDIO assign */
    IOIF_ETH_MDIO_Initialize_t mdio_init = {
        .heth = &heth,
        .timeout = 1000,
    };
    ioif_eth_mdio.assign(&s_mdio_id, &mdio_init);

    /* 2. RTL8201F Init: SW Reset + Page 7 RMII 타이밍 + 2s 안정화 */
    int32_t phy_init_status = RTL8201F_Init(s_mdio_id);
    { extern volatile int32_t g_diag_phy_init_status;
      g_diag_phy_init_status = phy_init_status; }

/* USER CODE END low_level_init Code 1 for User BSP */

  if (hal_eth_init_status == HAL_OK)
  {
/* USER CODE BEGIN low_level_init Code 2 for User BSP */

    /* PHY Init 실패 시 link down 처리 (MDIO 통신 오류 / PHYID 불일치 / Reset Timeout) */
    if (phy_init_status != RTL8201F_STATUS_OK) {
        netif_set_link_down(netif);
        netif_set_down(netif);
        /* 진단: g_diag_phy_init_status를 디버거로 확인 */
    } else {

    /* Link state 확인 + MAC 설정 + ETH Start */
    int32_t PHYLinkState = RTL8201F_GetLinkState();

    if (PHYLinkState > RTL8201F_STATUS_LINK_DOWN) {
        ETH_MACConfigTypeDef MACConf = {0};
        uint32_t duplex, speed;

        switch (PHYLinkState) {
        case RTL8201F_STATUS_100MBITS_FULLDUPLEX:
            duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_100M; break;
        case RTL8201F_STATUS_100MBITS_HALFDUPLEX:
            duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_100M; break;
        case RTL8201F_STATUS_10MBITS_FULLDUPLEX:
            duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_10M; break;
        case RTL8201F_STATUS_10MBITS_HALFDUPLEX:
        default:
            duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_10M; break;
        }

        HAL_ETH_GetMACConfig(&heth, &MACConf);
        MACConf.DuplexMode = duplex;
        MACConf.Speed = speed;
        HAL_ETH_SetMACConfig(&heth, &MACConf);

        /* PMCR 재보장 — RMII + PA1 Close + Booster */
        SYSCFG->PMCR |= SYSCFG_PMCR_EPIS_SEL_2;
        SYSCFG->PMCR &= ~((uint32_t)SYSCFG_PMCR_PA1SO);
        SYSCFG->PMCR |= SYSCFG_PMCR_BOOSTEN;

        HAL_ETH_Start_IT(&heth);

        { extern volatile uint32_t g_diag_pmcr_after_start;
          g_diag_pmcr_after_start = SYSCFG->PMCR; }

        netif_set_up(netif);
        netif_set_link_up(netif);
    } else {
        netif_set_link_down(netif);
        netif_set_down(netif);
    }

    } /* end: phy_init_status == OK */

/* USER CODE END low_level_init Code 2 for User BSP */

  }
  else
  {
    Error_Handler();
  }
#endif /* LWIP_ARP || LWIP_ETHERNET */

/* USER CODE BEGIN LOW_LEVEL_INIT */

/* USER CODE END LOW_LEVEL_INIT */
}

/**
 * @brief This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  uint32_t i = 0U;
  struct pbuf *q = NULL;
  err_t errval = ERR_OK;
  ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT] = {0};
  ETH_TxPacketConfig tx_config;

  memset(Txbuffer, 0 , ETH_TX_DESC_CNT*sizeof(ETH_BufferTypeDef));

  /* Set Tx packet config common parameters */
  memset(&tx_config, 0 , sizeof(ETH_TxPacketConfig));
  tx_config.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  tx_config.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  tx_config.CRCPadCtrl = ETH_CRC_PAD_INSERT;

  for(q = p; q != NULL; q = q->next)
  {
    if(i >= ETH_TX_DESC_CNT)
      return ERR_IF;

    Txbuffer[i].buffer = q->payload;
    Txbuffer[i].len = q->len;

    if(i>0)
    {
      Txbuffer[i-1].next = &Txbuffer[i];
    }

    if(q->next == NULL)
    {
      Txbuffer[i].next = NULL;
    }

    i++;
  }

  tx_config.Length = p->tot_len;
  tx_config.TxBuffer = Txbuffer;
  tx_config.pData = p;

  pbuf_ref(p);

  do
  {
    if(HAL_ETH_Transmit_IT(&heth, &tx_config) == HAL_OK)
    {
      errval = ERR_OK;
    }
    else
    {

      if(HAL_ETH_GetError(&heth) & HAL_ETH_ERROR_BUSY)
      {
        /* Wait for descriptors to become available */
        osSemaphoreAcquire(TxPktSemaphore, ETHIF_TX_TIMEOUT);
        HAL_ETH_ReleaseTxPacket(&heth);
        errval = ERR_BUF;
      }
      else
      {
        /* Other error */
        pbuf_free(p);
        errval =  ERR_IF;
      }
    }
  }while(errval == ERR_BUF);

  return errval;
}

/**
 * @brief Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
   */
static struct pbuf * low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL;

  if(RxAllocStatus == RX_ALLOC_OK)
  {
    HAL_ETH_ReadData(&heth, (void **)&p);
    if (p != NULL) {
      /* [2026-05-14] 정상 RX frame 카운트 — RMII stimulus 의 PC LAN 트래픽
       * 활동 표시. USER CODE 4 의 g_eth_rx_packet_count 변수 사용. */
      extern volatile uint32_t g_eth_rx_packet_count;
      g_eth_rx_packet_count++;
    }
  }

  return p;
}

/**
 * @brief This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void ethernetif_input(void* argument)
{
  struct pbuf *p = NULL;
  struct netif *netif = (struct netif *) argument;

  for( ;; )
  {
    if (osSemaphoreAcquire(RxPktSemaphore, TIME_WAITING_FOR_INPUT) == osOK)
    {
      do
      {
        p = low_level_input( netif );
        if (p != NULL)
        {
          /* [2026-05-14] SI stimulus RX packet 카운터 (RMII/MDIO row 표시용).
           * single writer (ethernetif_input task), single reader (sensor-studio
           * polling) → 4B atomic, race 없음. ARP/ICMP/IP 모든 RX 포함. */
          g_eth_rx_packet_count++;
          if (netif->input( p, netif) != ERR_OK )
          {
            pbuf_free(p);
          }
        }
      } while(p!=NULL);
    }
  }
}

#if !LWIP_ARP
/**
 * This function has to be completed by user in case of ARP OFF.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if ...
 */
static err_t low_level_output_arp_off(struct netif *netif, struct pbuf *q, const ip4_addr_t *ipaddr)
{
  err_t errval;
  errval = ERR_OK;

/* USER CODE BEGIN 5 */

/* USER CODE END 5 */

  return errval;

}
#endif /* LWIP_ARP */

/**
 * @brief Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "XM10-Rev2.0";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  // MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */

#if LWIP_IPV4
#if LWIP_ARP || LWIP_ETHERNET
#if LWIP_ARP
  netif->output = etharp_output;
#else
  /* The user should write its own code in low_level_output_arp_off function */
  netif->output = low_level_output_arp_off;
#endif /* LWIP_ARP */
#endif /* LWIP_ARP || LWIP_ETHERNET */
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

/**
  * @brief  Custom Rx pbuf free callback
  * @param  pbuf: pbuf to be freed
  * @retval None
  */
void pbuf_free_custom(struct pbuf *p)
{
  struct pbuf_custom* custom_pbuf = (struct pbuf_custom*)p;
  LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

  /* If the Rx Buffer Pool was exhausted, signal the ethernetif_input task to
   * call HAL_ETH_GetRxDataBuffer to rebuild the Rx descriptors. */

  if (RxAllocStatus == RX_ALLOC_ERROR)
  {
    RxAllocStatus = RX_ALLOC_OK;
    osSemaphoreRelease(RxPktSemaphore);
  }
}

/* USER CODE BEGIN 6 */

/**
* @brief  Returns the current time in milliseconds
*         when LWIP_TIMERS == 1 and NO_SYS == 1
* @param  None
* @retval Current Time value
*/
u32_t sys_now(void)
{
  return HAL_GetTick();
}

/* USER CODE END 6 */

/* USER CODE BEGIN PHI IO Functions for User BSP */

/**
 * @brief ETH MSP 초기화 (HAL_ETH_Init 내부에서 콜백 호출)
 * @details CubeMX Driver_PHY=undefined 모드에서는 이 함수가 자동 생성되지 않음.
 *          __weak 기본값(빈 함수)을 오버라이드하여 ETH 클럭 + GPIO + 인터럽트 설정.
 *
 * [Root Cause] Driver_PHY=LAN8742 → undefined 변경 시 CubeMX가 제거.
 *              HAL_ETH_MspInit 미구현 → ETH 레지스터 언클럭 → 모든 읽기 0 → MDIO 불통.
 *
 * ETH RMII Pin Map (XM10 Rev2.0 = WS5 동일):
 *   PC1  → ETH_MDC          PA1  → ETH_REF_CLK
 *   PA2  → ETH_MDIO         PA7  → ETH_CRS_DV
 *   PC4  → ETH_RXD0         PC5  → ETH_RXD1
 *   PB11 → ETH_TX_EN        PG13 → ETH_TXD0        PG14 → ETH_TXD1
 */
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (heth->Instance == ETH) {
        /* 1. ETH Peripheral Clocks Enable */
        __HAL_RCC_ETH1MAC_CLK_ENABLE();
        __HAL_RCC_ETH1TX_CLK_ENABLE();
        __HAL_RCC_ETH1RX_CLK_ENABLE();

        /* 2. GPIO Clocks Enable */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();

        /* 1.5 PA1_C ↔ PA1 아날로그 스위치 Close (REF_CLK 경로 연결)
        * Why: XM10 Rev2.0 BGA240에서 REF_CLK이 PA1_C 볼에 연결됨.
        *      PA1SO=1(기본값)이면 PA1_C↔PA1 단절 → 50MHz 미도달 → DMA SWR timeout.
        *      PA1SO=0으로 스위치 닫아야 REF_CLK이 ETH AF11까지 도달. */
        __HAL_RCC_SYSCFG_CLK_ENABLE();
        HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA1, SYSCFG_SWITCH_PA1_CLOSE);

        /* 3. GPIOC: PC1(MDC) + PC4(RXD0) + PC5(RXD1) */
        GPIO_InitStruct.Pin = MCU_PHY_MDC_Pin | MCU_RMII_RXD0_Pin | MCU_RMII_RXD1_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* 4. GPIOA: PA1(REF_CLK) + PA2(MDIO) + PA7(CRS_DV) */
        GPIO_InitStruct.Pin = MCU_RMII_REF_CLK_Pin | MCU_PHY_MDIO_Pin | MCU_RMII_CRS_DV_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* 5. GPIOB: PB11(TX_EN) */
        GPIO_InitStruct.Pin = MCU_RMII_TX_EN_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
        HAL_GPIO_Init(MCU_RMII_TX_EN_GPIO_Port, &GPIO_InitStruct);

        /* 6. GPIOG: PG13(TXD0) + PG14(TXD1) */
        GPIO_InitStruct.Pin = MCU_RMII_TXD0A_Pin | MCU_RMII_TXD1A_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        /* 7. ETH Interrupt (WS5 동일 — priority 5) */
        HAL_NVIC_SetPriority(ETH_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(ETH_IRQn);

        /* 8. PMCR 강제 재설정 — RMII + PA1 스위치 Close + Booster
         * 다른 HAL 초기화(I2C FMP 등)가 PMCR을 덮어쓸 수 있으므로
         * MspInit 마지막에 한번 더 보장 */
        SYSCFG->PMCR |= SYSCFG_PMCR_EPIS_SEL_2;               /* bit 23: RMII 모드 */
        SYSCFG->PMCR &= ~((uint32_t)SYSCFG_PMCR_PA1SO);       /* bit 25: PA1_C↔PA1 Close */
        SYSCFG->PMCR |= SYSCFG_PMCR_BOOSTEN;                  /* bit 8: 아날로그 스위치 부스터 — 50MHz REF_CLK via PA1_C */
    }
}

/* [진단] 디버거 Watch용 — 부팅 후 상태 확인 */
volatile uint32_t g_diag_pmcr_after_mspinit = 0;
volatile uint32_t g_diag_pmcr_after_start   = 0;
volatile int32_t  g_diag_phy_init_status    = -99;   /* RTL8201F_Init 리턴값 (0=OK) */
volatile int32_t  g_diag_phy_link_state     = -1;    /* GetLinkState 리턴값 */
volatile uint32_t g_diag_eth_rx_cnt         = 0;     /* RX 인터럽트 카운터 */
volatile uint32_t g_diag_eth_tx_cnt         = 0;     /* TX 인터럽트 카운터 */
volatile uint32_t g_diag_eth_err_cnt        = 0;     /* Error 인터럽트 카운터 */
volatile uint32_t g_diag_mmcrx_crc_err      = 0;     /* MMC RX CRC Error 카운터 */

/* USER CODE END PHI IO Functions for User BSP */

/**
  * @brief  Check the ETH link state then update ETH driver and netif link accordingly.
  * @retval None
  */
void ethernet_link_thread(void* argument)
{

/* USER CODE BEGIN ETH link init */
  struct netif *netif = (struct netif *) argument;
/* USER CODE END ETH link init */

  for(;;)
  {

/* USER CODE BEGIN ETH link Thread core code for User BSP */

    /* RTL8201F Link state 폴링 (100ms 주기 — osDelay 아래)
     * IEEE 802.3 BSR latching-low → RTL8201F_GetLinkState 내부에서 2회 읽기 처리 */
    int32_t PHYLinkState = RTL8201F_GetLinkState();

    /* [진단] link state + MMC CRC 에러 카운터 갱신 */
    { extern volatile int32_t  g_diag_phy_link_state;
      extern volatile uint32_t g_diag_mmcrx_crc_err;
      g_diag_phy_link_state = PHYLinkState;
      g_diag_mmcrx_crc_err  = heth.Instance->MMCRCRCEPR; }

    if (netif_is_link_up(netif) && (PHYLinkState <= RTL8201F_STATUS_LINK_DOWN)) {
        /* Link was UP → now DOWN */
        HAL_ETH_Stop_IT(&heth);
        netif_set_down(netif);
        netif_set_link_down(netif);
    } else if (!netif_is_link_up(netif) && (PHYLinkState > RTL8201F_STATUS_LINK_DOWN)) {
        /* Link was DOWN → now UP */
        ETH_MACConfigTypeDef MACConf = {0};
        uint32_t duplex, speed;

        switch (PHYLinkState) {
        case RTL8201F_STATUS_100MBITS_FULLDUPLEX:
            duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_100M; break;
        case RTL8201F_STATUS_100MBITS_HALFDUPLEX:
            duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_100M; break;
        case RTL8201F_STATUS_10MBITS_FULLDUPLEX:
            duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_10M; break;
        case RTL8201F_STATUS_10MBITS_HALFDUPLEX:
        default:
            duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_10M; break;
        }

        HAL_ETH_GetMACConfig(&heth, &MACConf);
        MACConf.DuplexMode = duplex;
        MACConf.Speed = speed;
        HAL_ETH_SetMACConfig(&heth, &MACConf);

        /* PMCR 재보장 — RMII + PA1 Close + Booster */
        SYSCFG->PMCR |= SYSCFG_PMCR_EPIS_SEL_2;
        SYSCFG->PMCR &= ~((uint32_t)SYSCFG_PMCR_PA1SO);
        SYSCFG->PMCR |= SYSCFG_PMCR_BOOSTEN;

        HAL_ETH_Start_IT(&heth);

        { extern volatile uint32_t g_diag_pmcr_after_start;
          g_diag_pmcr_after_start = SYSCFG->PMCR; }

        netif_set_up(netif);
        netif_set_link_up(netif);
    }

/* USER CODE END ETH link Thread core code for User BSP */

    osDelay(100);
  }
}

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
/* USER CODE BEGIN HAL ETH RxAllocateCallback */
  struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
  if (p)
  {
    /* Get the buff from the struct pbuf address. */
    *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
    p->custom_free_function = pbuf_free_custom;
    /* Initialize the struct pbuf.
    * This must be performed whenever a buffer's allocated because it may be
    * changed by lwIP or the app, e.g., pbuf_free decrements ref. */
    pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
  }
  else
  {
    RxAllocStatus = RX_ALLOC_ERROR;
    *buff = NULL;
  }
/* USER CODE END HAL ETH RxAllocateCallback */
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
/* USER CODE BEGIN HAL ETH RxLinkCallback */

  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd = (struct pbuf **)pEnd;
  struct pbuf *p = NULL;

  /* Get the struct pbuf from the buff address. */
  p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
  p->next = NULL;
  p->tot_len = 0;
  p->len = Length;

  /* Chain the buffer. */
  if (!*ppStart)
  {
    /* The first buffer of the packet. */
    *ppStart = p;
  }
  else
  {
    /* Chain the buffer to the end of the packet. */
    (*ppEnd)->next = p;
  }
  *ppEnd  = p;

  /* Update the total length of all the buffers of the chain. Each pbuf in the chain should have its tot_len
   * set to its own length, plus the length of all the following pbufs in the chain. */
  for (p = *ppStart; p != NULL; p = p->next)
  {
    p->tot_len += Length;
  }

  /* Invalidate data cache because Rx DMA's writing to physical memory makes it stale. */
  SCB_InvalidateDCache_by_Addr((uint32_t *)buff, Length);

/* USER CODE END HAL ETH RxLinkCallback */
}

void HAL_ETH_TxFreeCallback(uint32_t * buff)
{
/* USER CODE BEGIN HAL ETH TxFreeCallback */

  pbuf_free((struct pbuf *)buff);

/* USER CODE END HAL ETH TxFreeCallback */
}

/* USER CODE BEGIN 8 */

/* USER CODE END 8 */
