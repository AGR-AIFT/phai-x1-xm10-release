/**
 * @file    canfd_sensor.h
 * @brief   XM10 예제 Ex.45(CAN-FD My Sensor)의 상대 장치 — XM10 과 주고받는 규칙
 * @details 이 파일과 canfd_sensor.c 는 MCU 에 묶이지 않습니다(HAL 을 쓰지 않습니다).
 *          다른 MCU 로 옮길 때는 아래 'MCU 가 채울 함수' 세 개와, 받은 메시지를 Sensor_OnFrame() 으로 넘기는 부분만
 *          새로 만들면 됩니다. 이 예제는 main.c 에서 STM32G474 HAL 로 채웁니다.
 *
 *  [XM10 과 맞출 것 — 장치 번호 n = SENSOR_NODE_ID]
 *    버스        CAN-FD, 11비트 ID, 데이터 구간 고속 전송(BRS), 1 Mbit/s / 5 Mbit/s, 샘플 포인트 75 %
 *    주기 데이터 ID 0x180 + n, 16 바이트, 1 ms 마다
 *                  바이트 0~3 카운터(uint32, 작은 쪽 바이트부터 — 보낼 때마다 +1)
 *                  바이트 4~5 ADC 0(uint16) · 6~7 ADC 1(uint16) · 8~15 은 0
 *    살아있음    ID 0x700 + n, 1 바이트 0x05, 1 초마다. 켜질 때 0x00 을 한 번 보냅니다(재시작 알림)
 *    명령(받기)  ID 0x200 + n, [순번, LED(0 = 끔 · 1 = 켬)] — 순번이 지난번보다 1~127 앞서면 새 명령
 *    동기 신호   ID 0x080(받기, 선택 — SENSOR_USE_SYNC)
 */

#ifndef CANFD_SENSOR_H_
#define CANFD_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>

#ifndef SENSOR_NODE_ID
#define SENSOR_NODE_ID          (0x20U)                       /**< 장치 번호 — XM10 예제의 MY_DEVICE_ID 와 같게(-D 로도 바꿀 수 있음) */
#endif
#define SENSOR_DATA_ID          (0x180U + SENSOR_NODE_ID)     /**< 주기 데이터(보내기) */
#define SENSOR_CMD_ID           (0x200U + SENSOR_NODE_ID)     /**< 명령(받기) */
#define SENSOR_ALIVE_ID         (0x700U + SENSOR_NODE_ID)     /**< 살아있음(보내기) */
#define SENSOR_SYNC_ID          (0x080U)                      /**< 동기 신호(받기) */

#define SENSOR_DATA_LEN         (16U)      /**< 주기 데이터 길이 */
#define SENSOR_DATA_PERIOD_MS   (1U)       /**< 주기 데이터 1 kHz */
#define SENSOR_ALIVE_PERIOD_MS  (1000U)    /**< 살아있음 1 초 — XM10 은 3 초 동안 없으면 '끊김' 으로 봅니다 */
#define SENSOR_ALIVE_OK         (0x05U)    /**< 살아있음 값 */
#define SENSOR_ALIVE_REBOOT     (0x00U)    /**< 재시작 알림 값 */
#define SENSOR_CMD_LEN          (2U)       /**< 명령 [순번, LED] */
#define SENSOR_CMD_REBASE_MS    (1000U)    /**< 명령이 이만큼 없었으면 다음 순번을 새 기준으로 삼습니다(XM10 · 장치 재시작 대비) */
#ifndef SENSOR_USE_SYNC
#define SENSOR_USE_SYNC         (0)        /**< 1 = 동기 신호 직후에 주기 데이터를 보냅니다(신호가 없으면 자기 주기로, -D 로도 바꿀 수 있음) */
#endif
#define SENSOR_SYNC_LOST_MS     (5U)       /**< 동기 신호가 이만큼 없으면 자기 주기로 돌아갑니다 */

/** @brief 장치 쪽 통계 — 디버거로 볼 수 있습니다 */
typedef struct {
    uint32_t data_sent;     /**< 주기 데이터를 보낸 수(= 다음 카운터 값) */
    uint32_t send_fail;     /**< 보내기 큐가 가득해 못 보낸 수 */
    uint32_t cmd_new;       /**< 새 명령으로 받아들인 수 */
    uint32_t cmd_ignored;   /**< 중복 · 옛 순번이라 버린 수 */
    uint32_t sync_count;    /**< 받은 동기 신호 수 */
} SensorStats_t;

/* ---- MCU 가 채울 함수 — 이 예제는 main.c 에서 HAL 로 채웁니다 ---- */

/** @brief 메시지 하나를 보내기 큐에 싣습니다 — 기다리지 않습니다. 실었으면 true */
bool Sensor_PortSend(uint16_t id, const uint8_t* data, uint8_t len);
/** @brief ADC 두 채널을 읽습니다(0 ~ 4095) */
void Sensor_PortReadAdc(uint16_t* adc0, uint16_t* adc1);
/** @brief 보드 LED 를 켜고 끕니다 */
void Sensor_PortSetLed(bool on);

/* ---- XM10 과 주고받는 규칙 ---- */

/** @brief 처음 한 번 — 재시작 알림(0x00)을 보냅니다 */
void Sensor_Init(uint32_t now_ms);
/** @brief 메인 루프에서 계속 부릅니다 — 주기 데이터와 살아있음을 보냅니다 */
void Sensor_Poll(uint32_t now_ms);
/** @brief 받은 메시지 하나를 넘깁니다(명령 · 동기 신호) */
void Sensor_OnFrame(uint16_t id, const uint8_t* data, uint8_t len, uint32_t now_ms);
/** @brief 통계 */
const SensorStats_t* Sensor_GetStats(void);

#endif /* CANFD_SENSOR_H_ */
