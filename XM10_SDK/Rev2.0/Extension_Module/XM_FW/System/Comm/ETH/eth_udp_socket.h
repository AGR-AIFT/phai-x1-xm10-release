/**
  ******************************************************************************
  * @file    eth_udp_socket.h
  * @author  HyundoKim
  * @brief   AGR_DOP over UDP — LwIP UDP 소켓 인프라 (Rev2.0 Ethernet)
  *
  *          3개의 UDP 포트를 관리:
  *          - PDO  (Port 5001): 실시간 프로세스 데이터 교환
  *          - SDO  (Port 5002): 서비스(설정/읽기) 요청-응답
  *          - EMCY (Port 5003): 비상 알림
  *
  *          AM(Application Module, Jetson) 측 IP: 192.168.10.20
  *          XM(Extension Module) 측 IP:          192.168.10.10 (LwIP Static)
  *
  * @version 1.0
  * @date    2026-03-02
  *
  * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
  ******************************************************************************
  */

#ifndef ETH_UDP_SOCKET_H
#define ETH_UDP_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported constants --------------------------------------------------------*/

/** @brief UDP Port Definitions for AGR_DOP over Ethernet */
#define ETH_UDP_PORT_PDO    5001U   /**< Process Data Objects (cyclic, real-time) */
#define ETH_UDP_PORT_SDO    5002U   /**< Service Data Objects (request-response) */
#define ETH_UDP_PORT_EMCY   5003U   /**< Emergency Messages */

/** @brief Default AM (Jetson) IP Address */
#define ETH_UDP_AM_IP_0     192U
#define ETH_UDP_AM_IP_1     168U
#define ETH_UDP_AM_IP_2     10U
#define ETH_UDP_AM_IP_3     20U

/* Exported types ------------------------------------------------------------*/

/** @brief UDP 채널 식별자 */
typedef enum {
    ETH_UDP_CH_PDO  = 0,
    ETH_UDP_CH_SDO  = 1,
    ETH_UDP_CH_EMCY = 2,
    ETH_UDP_CH_COUNT
} EthUdpChannel_t;

/**
  * @brief  UDP 수신 콜백 함수 타입
  * @param  channel: 수신된 채널 (PDO/SDO/EMCY)
  * @param  data: 수신 데이터 포인터 (콜백 종료 후 무효)
  * @param  len: 수신 데이터 길이
  * @param  src_ip: 송신자 IP (network byte order)
  * @param  src_port: 송신자 포트
  */
typedef void (*EthUdpRxCallback_t)(EthUdpChannel_t channel,
                                    const uint8_t *data, uint16_t len,
                                    uint32_t src_ip, uint16_t src_port);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  UDP 소켓 인프라 초기화 (3개 포트 바인드)
  * @note   MX_LWIP_Init() 이후에 호출해야 합니다.
  *         tcpip_thread 컨텍스트에서 실행되므로 RTOS 활성 상태 필요.
  * @retval true: 성공, false: 실패
  */
bool EthUdpSocket_Init(void);

/**
  * @brief  UDP 수신 콜백 등록
  * @param  channel: 대상 채널 (PDO/SDO/EMCY)
  * @param  callback: 수신 콜백 함수 (NULL로 해제)
  */
void EthUdpSocket_RegisterRxCallback(EthUdpChannel_t channel,
                                      EthUdpRxCallback_t callback);

/**
  * @brief  UDP 데이터 전송 (기본 AM IP로 전송)
  * @param  channel: 전송 채널 (PDO/SDO/EMCY, 포트 자동 결정)
  * @param  data: 전송 데이터 포인터
  * @param  len: 전송 데이터 길이 (최대 1472 bytes)
  * @retval 0: 성공, <0: 에러
  */
int EthUdpSocket_Send(EthUdpChannel_t channel,
                       const uint8_t *data, uint16_t len);

/**
  * @brief  UDP 데이터 전송 (지정 IP:Port로 전송)
  * @param  channel: 전송 채널
  * @param  data: 전송 데이터 포인터
  * @param  len: 전송 데이터 길이
  * @param  dest_ip: 목적지 IP (network byte order)
  * @param  dest_port: 목적지 포트
  * @retval 0: 성공, <0: 에러
  */
int EthUdpSocket_SendTo(EthUdpChannel_t channel,
                         const uint8_t *data, uint16_t len,
                         uint32_t dest_ip, uint16_t dest_port);

/**
  * @brief  UDP 소켓 상태 조회
  * @retval true: 초기화 완료 및 사용 가능, false: 미초기화
  */
bool EthUdpSocket_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* ETH_UDP_SOCKET_H */
