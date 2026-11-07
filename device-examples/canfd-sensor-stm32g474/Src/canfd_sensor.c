/**
 * @file    canfd_sensor.c
 * @brief   XM10 예제 Ex.45 의 상대 장치 — 주기 데이터 · 살아있음 보내기, 명령 받기(MCU 와 무관한 부분)
 * @details 보내기와 ADC · LED 는 canfd_sensor.h 의 Sensor_Port* 함수로만 합니다.
 */

#include "canfd_sensor.h"

#include <string.h>

static SensorStats_t s_stats;
static uint32_t s_counter;          /* 주기 데이터 카운터 — 보낼 때마다 +1 */
static uint32_t s_last_data_ms;
static uint32_t s_last_alive_ms;

static bool     s_have_cmd;         /* 명령을 한 번이라도 받아들였나 */
static uint8_t  s_last_seq;         /* 마지막으로 받아들인 명령 순번 */
static uint32_t s_last_cmd_rx_ms;   /* 명령(중복 포함)을 마지막으로 받은 시각 */

static bool     s_sync_seen;
static bool     s_sync_pending;     /* 동기 신호를 받았고 아직 그 뒤 데이터를 안 보냄 */
static uint32_t s_last_sync_ms;

static void _PutU32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static void _PutU16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void _SendData(void)
{
    uint8_t  buf[SENSOR_DATA_LEN];
    uint16_t adc0 = 0U;
    uint16_t adc1 = 0U;

    memset(buf, 0, sizeof(buf));
    Sensor_PortReadAdc(&adc0, &adc1);
    _PutU32(&buf[0], s_counter);
    _PutU16(&buf[4], adc0);
    _PutU16(&buf[6], adc1);
    if (Sensor_PortSend(SENSOR_DATA_ID, buf, SENSOR_DATA_LEN)) {
        s_counter++;                         /* XM10 은 카운터의 빈 번호로 잃은 값을 셉니다 */
        s_stats.data_sent = s_counter;
    } else {
        s_stats.send_fail++;                 /* 이번 값은 건너뜁니다(다음 주기에 새 값) */
    }
}

static bool _SendAlive(uint8_t value)
{
    const uint8_t b = value;
    if (Sensor_PortSend(SENSOR_ALIVE_ID, &b, 1U)) {
        return true;
    }
    s_stats.send_fail++;
    return false;
}

void Sensor_Init(uint32_t now_ms)
{
    memset(&s_stats, 0, sizeof(s_stats));
    s_counter      = 0U;
    s_have_cmd     = false;
    s_sync_seen    = false;
    s_sync_pending = false;

    (void)_SendAlive(SENSOR_ALIVE_REBOOT);   /* 켜졌음 — XM10 은 그동안 받아 둔 값을 버리고 다음 0x05 를 기다립니다 */
    s_last_alive_ms = now_ms - SENSOR_ALIVE_PERIOD_MS;   /* 다음 Sensor_Poll() 에서 바로 0x05 */
    s_last_data_ms  = now_ms;
}

void Sensor_Poll(uint32_t now_ms)
{
    if ((now_ms - s_last_alive_ms) >= SENSOR_ALIVE_PERIOD_MS) {
        if (_SendAlive(SENSOR_ALIVE_OK)) {
            s_last_alive_ms = now_ms;        /* 못 실었으면 다음 루프에 다시 */
        }
    }

    /* 동기 신호를 쓰는 동안에는 신호 직후에, 신호가 끊기면 자기 주기로 보냅니다(신호는 가끔 빠질 수 있습니다) */
    const bool use_sync = (SENSOR_USE_SYNC != 0) && s_sync_seen && ((now_ms - s_last_sync_ms) < SENSOR_SYNC_LOST_MS);
    if (use_sync) {
        if (s_sync_pending) {
            s_sync_pending = false;
            s_last_data_ms = now_ms;
            _SendData();
        }
    } else if ((now_ms - s_last_data_ms) >= SENSOR_DATA_PERIOD_MS) {
        s_last_data_ms = now_ms;
        _SendData();
    }
}

void Sensor_OnFrame(uint16_t id, const uint8_t* data, uint8_t len, uint32_t now_ms)
{
    if (id == SENSOR_SYNC_ID) {
        s_stats.sync_count++;
        s_sync_seen    = true;
        s_sync_pending = true;
        s_last_sync_ms = now_ms;
        return;
    }
    if ((id != SENSOR_CMD_ID) || (len < SENSOR_CMD_LEN)) {
        return;
    }

    /* 순번 거르기 — 지난번보다 1~127 앞서면 새 명령, 같거나(중복) 뒤면(순서가 뒤바뀐 옛 명령) 버립니다.
     * 명령이 1 초 넘게 없었으면 XM10 이나 이 장치가 다시 켜졌을 수 있어 이번 순번을 새 기준으로 받아들입니다.
     * 그래서 연결이 끊긴 동안 XM10 쪽에 남아 있던 옛 명령이 먼저 오면 그 명령은 걸러지지 않습니다. */
    const uint8_t seq    = data[0];
    const bool    rebase = !s_have_cmd || ((now_ms - s_last_cmd_rx_ms) > SENSOR_CMD_REBASE_MS);
    const uint8_t diff   = (uint8_t)(seq - s_last_seq);
    s_last_cmd_rx_ms = now_ms;

    if (rebase || ((diff >= 1U) && (diff <= 127U))) {
        s_have_cmd = true;
        s_last_seq = seq;
        s_stats.cmd_new++;
        Sensor_PortSetLed(data[1] != 0U);
    } else {
        s_stats.cmd_ignored++;
    }
}

const SensorStats_t* Sensor_GetStats(void)
{
    return &s_stats;
}
