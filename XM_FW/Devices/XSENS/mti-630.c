/**
 ******************************************************************************
 * @file    mti-630.c
 * @author  HyundoKim
 * @brief   [Devices Layer] Xsens MTi-630 IMU 드라이버 구현부
 * @details
 * - Xbus 프로토콜 바이트 단위 파싱 (ISR-safe)
 * - 센서 설정 (ConfigureOutput) - IOIF UART 사용
 * - IMU Hub Module / XM 공통 사용 가능 (Device + IOIF 복사 이식)
 * 
 * [의존성] Device → IOIF (00-core-architecture.mdc 준수)
 * - ioif_agrb_uart.h: UART Tx (센서 명령 전송)
 * - ioif_agrb_tim.h: 딜레이 (센서 부팅/응답 대기)
 * 
 * @version 4.0 (Multi-Instance Auto-Sense + DataLake)
 * @date    Feb 11, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "mti-630.h"
#include "ioif_agrb_tim.h"   /* IOIF_TIM_Delay, IOIF_TIM_GetTick */
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief UART 센서 타임아웃 (ms) - 1초간 데이터 없으면 끊김 */
#define XSENS_TIMEOUT_MS    1000

/** @brief 명령 전송 최대 재시도 횟수 */
#define XSENS_CMD_MAX_RETRY 5

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* 파싱 (Pure C, ISR-safe) */
static void _InitParser(XsensParser_t* parser);
static bool _ParseByte(XsensParser_t* parser, uint8_t byte, XsensMTi_packet_t* output);
static bool _IsConnected(uint8_t imu_id, uint32_t last_rx_time, uint32_t current_time);

static bool _ProcessPacket(XsensParser_t* parser, XsensMTi_packet_t* output);
static void _ParseMTData2(XsensParser_t* parser, XsensMTi_packet_t* output);
static float _ReverseFloat(const uint8_t* pData);

/* 센서 설정 (IOIF 의존) */
static void _ConfigureOutput(IOIF_UARTx_t uart_id);
static void _Cmd_Reset(IOIF_UARTx_t uart_id);
static void _Cmd_GoToConfig(IOIF_UARTx_t uart_id);
static void _Cmd_GoToMeasurement(IOIF_UARTx_t uart_id);
static void _TxCmd(IOIF_UARTx_t uart_id, const uint8_t* cmd, uint16_t len);

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/** @brief [디버깅] 센서 설정 명령 전송 통계 */
static volatile uint32_t s_config_cmd_sent = 0;
static volatile uint32_t s_config_cmd_failed = 0;

/**
 *------------------------------------------------------------
 * PUBLIC VARIABLES
 *------------------------------------------------------------
 */

XSENS_Driver_t xsensMTi630 = {
    /* 파싱 */
    .InitParser         = _InitParser,
    .ParseByte          = _ParseByte,
    .IsConnected        = _IsConnected,
    /* 센서 설정 */
    .ConfigureOutput    = _ConfigureOutput,
    .Cmd_Reset          = _Cmd_Reset,
    .Cmd_GoToConfig     = _Cmd_GoToConfig,
    .Cmd_GoToMeasurement = _Cmd_GoToMeasurement,
};

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief [디버깅] 센서 설정 명령 전송 통계 가져오기
 */
void XsensMTi_GetConfigStats(volatile uint32_t* sent, volatile uint32_t* failed)
{
    if (sent) *sent = s_config_cmd_sent;
    if (failed) *failed = s_config_cmd_failed;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS - 파싱 (Pure C, ISR-safe)
 *------------------------------------------------------------
 */

static void _InitParser(XsensParser_t* parser)
{
    if (parser) {
        memset(parser, 0, sizeof(XsensParser_t));
        parser->state = XSENS_STATE_WAIT_PREAMBLE;
    }
}

/**
 * @brief 바이트 단위 파싱 (ISR Context)
 * @details Xbus 프로토콜: 0xFA 0xFF MID LEN [Payload] CS
 * @note timestamp, imu_index는 caller가 설정해야 함
 */
static bool _ParseByte(XsensParser_t* parser, uint8_t byte, XsensMTi_packet_t* output)
{
    bool completed = false;
    
    // 체크섬 계산 (Preamble 제외)
    if (parser->state > XSENS_STATE_WAIT_PREAMBLE) {
        parser->checksum += byte;
    }
    
    switch (parser->state) {
        case XSENS_STATE_WAIT_PREAMBLE:
            if (byte == XSENS_PREAMBLE) {
                parser->state = XSENS_STATE_WAIT_BID;
                parser->checksum = 0;
            }
            break;
            
        case XSENS_STATE_WAIT_BID:
            if (byte == XSENS_BID) {
                parser->state = XSENS_STATE_WAIT_MID;
            } else {
                parser->state = XSENS_STATE_WAIT_PREAMBLE;
            }
            break;
            
        case XSENS_STATE_WAIT_MID:
            parser->mid = byte;
            parser->state = XSENS_STATE_WAIT_LEN;
            break;
            
        case XSENS_STATE_WAIT_LEN:
            if (byte == 0xFF) {  // Extended Length
                parser->len = 0;
                parser->state = XSENS_STATE_WAIT_LEN_EXT_H;
            } else {
                parser->len = byte;
                parser->buf_idx = 0;
                parser->state = (parser->len > 0) ? XSENS_STATE_COLLECT_PAYLOAD : XSENS_STATE_WAIT_CHECKSUM;
            }
            break;
            
        case XSENS_STATE_WAIT_LEN_EXT_H:
            parser->len = (uint16_t)byte << 8;
            parser->state = XSENS_STATE_WAIT_LEN_EXT_L;
            break;
            
        case XSENS_STATE_WAIT_LEN_EXT_L:
            parser->len |= byte;
            parser->buf_idx = 0;
            parser->state = (parser->len > 0) ? XSENS_STATE_COLLECT_PAYLOAD : XSENS_STATE_WAIT_CHECKSUM;
            break;
            
        case XSENS_STATE_COLLECT_PAYLOAD:
            if (parser->buf_idx < XSENS_MAX_PACKET_SIZE) {
                parser->buffer[parser->buf_idx++] = byte;
            }
            
            /* 버퍼 오버플로우 시 파서 리셋 (동기화 복구) */
            if (parser->len > XSENS_MAX_PACKET_SIZE) {
                parser->state = XSENS_STATE_WAIT_PREAMBLE;
                break;
            }
            
            if (parser->buf_idx >= parser->len) {
                parser->state = XSENS_STATE_WAIT_CHECKSUM;
            }
            break;
            
        case XSENS_STATE_WAIT_CHECKSUM:
            // Checksum 검증 (1-complement: 합이 0x00이어야 함)
            if (parser->checksum == 0x00) {
                if (_ProcessPacket(parser, output)) {
                    completed = true;
                }
            }
            parser->state = XSENS_STATE_WAIT_PREAMBLE;
            break;
    }
    
    return completed;
}

/**
 * @brief 연결 상태 확인 (타임아웃 기반)
 */
static bool _IsConnected(uint8_t imu_id, uint32_t last_rx_time, uint32_t current_time)
{
    (void)imu_id;
    
    uint32_t elapsed = current_time - last_rx_time;
    return (elapsed < XSENS_TIMEOUT_MS);
}

/**
 * @brief 수신된 패킷 처리 (MID에 따라 분기)
 */
static bool _ProcessPacket(XsensParser_t* parser, XsensMTi_packet_t* output)
{
    if (parser->mid == XSENS_MID_MTDATA2) {  // 0x36
        _ParseMTData2(parser, output);
        return true;
    }
    return false;
}

/**
 * @brief MTData2 (0x36) 페이로드 파싱
 * @details Data ID 기반 파싱 (0x2010, 0x4020, 0x8020)
 * @note timestamp는 설정하지 않음 (caller 책임)
 */
static void _ParseMTData2(XsensParser_t* parser, XsensMTi_packet_t* output)
{
    // 센서 데이터 필드 초기화 (timestamp, imu_index 제외)
    output->q_w = 0.0f; output->q_x = 0.0f; output->q_y = 0.0f; output->q_z = 0.0f;
    output->acc_x = 0.0f; output->acc_y = 0.0f; output->acc_z = 0.0f;
    output->gyr_x = 0.0f; output->gyr_y = 0.0f; output->gyr_z = 0.0f;
    
    uint16_t idx = 0;
    while (idx < parser->len) {
        // 1. Data ID 읽기 (2 bytes, Big-endian)
        uint16_t data_id = ((uint16_t)parser->buffer[idx] << 8) | parser->buffer[idx+1];
        idx += 2;
        
        // 2. Data Length 읽기 (1 byte)
        uint8_t data_len = parser->buffer[idx];
        idx += 1;
        
        // 3. Data ID에 따라 파싱
        switch(data_id) {
            case XDI_QUATERNION:  // 0x2010 (16 bytes)
                output->q_w = _ReverseFloat(&parser->buffer[idx + 0]);
                output->q_x = _ReverseFloat(&parser->buffer[idx + 4]);
                output->q_y = _ReverseFloat(&parser->buffer[idx + 8]);
                output->q_z = _ReverseFloat(&parser->buffer[idx + 12]);
                break;
                
            case XDI_ACCELERATION:  // 0x4020 (12 bytes)
                output->acc_x = _ReverseFloat(&parser->buffer[idx + 0]);
                output->acc_y = _ReverseFloat(&parser->buffer[idx + 4]);
                output->acc_z = _ReverseFloat(&parser->buffer[idx + 8]);
                break;
                
            case XDI_GYROSCOPE_DATA:  // 0x8020 (12 bytes)
                output->gyr_x = _ReverseFloat(&parser->buffer[idx + 0]);
                output->gyr_y = _ReverseFloat(&parser->buffer[idx + 4]);
                output->gyr_z = _ReverseFloat(&parser->buffer[idx + 8]);
                break;
        }
        idx += data_len;
    }
}

/**
 * @brief Big-Endian float → Little-Endian float 변환
 */
static float _ReverseFloat(const uint8_t* pData)
{
    float f;
    uint8_t* pFloat = (uint8_t*)&f;
    // Xsens Big-Endian → STM32 Little-Endian
    pFloat[0] = pData[3];
    pFloat[1] = pData[2];
    pFloat[2] = pData[1];
    pFloat[3] = pData[0];
    return f;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS - 센서 설정 (IOIF 의존)
 *------------------------------------------------------------
 */

/**
 * @brief Xsens MTi-630 센서를 1kHz Quat+Acc+Gyro 모드로 설정
 * @param uart_id IOIF UART 인스턴스 ID
 * @details 
 * [시퀀스]
 * 1. 센서 부팅 대기 (500ms)
 * 2. GoToConfig (Config 모드 진입)
 * 3. SetOutputConfiguration (MID 0xC0)
 *    - Quaternion @ 1000Hz
 *    - Acceleration @ 1000Hz
 *    - Gyroscope @ 1000Hz
 * 4. GoToMeasurement (MTData2 전송 시작)
 */
static void _ConfigureOutput(IOIF_UARTx_t uart_id)
{
    /* [1] 센서 부팅 대기 */
    IOIF_TIM_Delay(500);
    
    /* [2] GoToConfig (Config 모드 진입) */
    _Cmd_GoToConfig(uart_id);
    IOIF_TIM_Delay(200);
    
    /* [3] MTData2 Output Configuration (MID 0xC0) */
    uint8_t payload[] = {
        0x20, 0x10, 0x03, 0xE8, /* Quaternion @ 1000Hz */
        0x40, 0x20, 0x03, 0xE8, /* Acceleration @ 1000Hz */
        0x80, 0x20, 0x03, 0xE8  /* Gyroscope @ 1000Hz */
    };
    uint8_t len = sizeof(payload);
    
    uint8_t tx_buffer[32];
    tx_buffer[0] = XSENS_PREAMBLE;
    tx_buffer[1] = XSENS_BID;
    tx_buffer[2] = XSENS_MID_SETOUTPUTCFG;
    tx_buffer[3] = len;
    memcpy(&tx_buffer[4], payload, len);
    
    /* Checksum */
    uint8_t checksum = 0;
    for (int i = 1; i < (4 + len); i++) {
        checksum += tx_buffer[i];
    }
    tx_buffer[4 + len] = (uint8_t)(-checksum);
    
    _TxCmd(uart_id, tx_buffer, 5 + len);
    IOIF_TIM_Delay(200);
    
    /* [4] GoToMeasurement (MTData2 전송 시작!) */
    _Cmd_GoToMeasurement(uart_id);
    IOIF_TIM_Delay(100);
}

/**
 * @brief Xsens 리셋 명령 (MID 0x40)
 */
static void _Cmd_Reset(IOIF_UARTx_t uart_id)
{
    uint8_t cmd[] = {XSENS_PREAMBLE, XSENS_BID, 0x40, 0x00, 0x00};
    uint8_t checksum = (uint8_t)(0xFF + 0x40 + 0x00);
    cmd[4] = (uint8_t)(-checksum);
    _TxCmd(uart_id, cmd, 5);
}

/**
 * @brief GoToConfig 명령 (MID 0x30)
 */
static void _Cmd_GoToConfig(IOIF_UARTx_t uart_id)
{
    uint8_t cmd[] = {XSENS_PREAMBLE, XSENS_BID, 0x30, 0x00, 0x00};
    uint8_t checksum = (uint8_t)(0xFF + 0x30 + 0x00);
    cmd[4] = (uint8_t)(-checksum);
    _TxCmd(uart_id, cmd, 5);
}

/**
 * @brief GoToMeasurement 명령 (MID 0x10)
 */
static void _Cmd_GoToMeasurement(IOIF_UARTx_t uart_id)
{
    uint8_t cmd[] = {XSENS_PREAMBLE, XSENS_BID, 0x10, 0x00, 0x00};
    uint8_t checksum = (uint8_t)(0xFF + 0x10 + 0x00);
    cmd[4] = (uint8_t)(-checksum);
    _TxCmd(uart_id, cmd, 5);
}

/**
 * @brief Xsens UART 명령 전송 (재시도 포함)
 * @param uart_id IOIF UART 인스턴스 ID
 * @param cmd 명령 바이트 배열
 * @param len 명령 길이
 */
static void _TxCmd(IOIF_UARTx_t uart_id, const uint8_t* cmd, uint16_t len)
{
    uint32_t retry = 0;
    bool success = false;
    
    while (retry < XSENS_CMD_MAX_RETRY) {
        AGRBStatusDef result = IOIF_UART_Write_Polling(uart_id, (uint8_t*)cmd, len);
        if (result == AGRBStatus_OK) {
            success = true;
            break;
        }
        
        IOIF_TIM_Delay(10);
        retry++;
    }
    
    if (success) {
        s_config_cmd_sent++;
    } else {
        s_config_cmd_failed++;
    }
    
    IOIF_TIM_Delay(20);
}

/* ================================================================
 * Auto-Sense + DataLake 구현 (Multi-Instance)
 * ================================================================
 * - DataLake: Mutex + Snapshot (Task-Task 표준 패턴)
 * - Auto-Sense: 데이터 타임아웃 기반 STOPPED ↔ OPERATIONAL 전환
 * - Multi-Instance: ch 파라미터로 인스턴스 구분
 *   XM: ch=0 (XSENS_MAX_INSTANCES=1)
 *   IMU Hub: ch=0~5 (XSENS_MAX_INSTANCES=6)
 *
 * 동기화 패턴 (cursorrule 13-comm-core-patterns):
 *   Task→Task, Latest Data Only → Mutex + Snapshot
 *   ❌ Double Buffer (불필요한 Lock-Free 복잡성)
 *   ❌ Queue (최신 값만 필요, 큐 관리 오버헤드)
 * ================================================================ */

#if defined(USE_FREERTOS)
#include "FreeRTOS.h"
#include "semphr.h"
#endif

/* ===== Auto-Sense 상태 ===== */
#define XSENS_AUTO_SENSE_TIMEOUT_MS  (500U)

typedef enum {
    XSENS_STATE_STOPPED,
    XSENS_STATE_OPERATIONAL,
} XsensAutoSenseState_t;

/* DataLake: Mutex + 단일 버퍼 (인스턴스별 배열) */
static XsensMTi_packet_t  s_imu_datalake[XSENS_MAX_INSTANCES];

#if defined(USE_FREERTOS)
static SemaphoreHandle_t  s_imu_mutex[XSENS_MAX_INSTANCES];
#endif

/* Auto-Sense 상태 (인스턴스별 배열) */
static volatile XsensAutoSenseState_t s_auto_sense_state[XSENS_MAX_INSTANCES];
static volatile uint32_t              s_last_packet_tick[XSENS_MAX_INSTANCES];

/* ===== Public Functions ===== */

void XsensMTi_StateInit(uint8_t ch)
{
    if (ch >= XSENS_MAX_INSTANCES) return;

    memset(&s_imu_datalake[ch], 0, sizeof(s_imu_datalake[ch]));
    s_auto_sense_state[ch] = XSENS_STATE_STOPPED;
    s_last_packet_tick[ch] = 0;

#if defined(USE_FREERTOS)
    if (s_imu_mutex[ch] == NULL) {
        s_imu_mutex[ch] = xSemaphoreCreateMutex();
    }
#endif
}

void XsensMTi_UpdateData(uint8_t ch, const XsensMTi_packet_t* packet)
{
    if (ch >= XSENS_MAX_INSTANCES) return;
    if (packet == NULL) return;

#if defined(USE_FREERTOS)
    /* Mutex + DataLake: Writer (UartRxTask) */
    xSemaphoreTake(s_imu_mutex[ch], portMAX_DELAY);
    s_imu_datalake[ch] = *packet;
    xSemaphoreGive(s_imu_mutex[ch]);
#else
    /* BareMetal: volatile 직접 쓰기 (ISR Priority 보호) */
    s_imu_datalake[ch] = *packet;
#endif

    /* Auto-Sense: 데이터 수신 → OPERATIONAL */
    s_last_packet_tick[ch] = IOIF_TIM_GetTick();
    s_auto_sense_state[ch] = XSENS_STATE_OPERATIONAL;
}

bool XsensMTi_GetLatest(uint8_t ch, XsensMTi_packet_t* out)
{
    if (ch >= XSENS_MAX_INSTANCES) return false;
    if (out == NULL) return false;
    if (s_auto_sense_state[ch] != XSENS_STATE_OPERATIONAL) return false;

#if defined(USE_FREERTOS)
    /* Mutex + Snapshot: Reader (Core Process) */
    xSemaphoreTake(s_imu_mutex[ch], portMAX_DELAY);
    *out = s_imu_datalake[ch];
    xSemaphoreGive(s_imu_mutex[ch]);
#else
    /* BareMetal: volatile 직접 읽기 */
    *out = s_imu_datalake[ch];
#endif

    return true;
}

bool XsensMTi_IsOnline(uint8_t ch)
{
    if (ch >= XSENS_MAX_INSTANCES) return false;
    return (s_auto_sense_state[ch] == XSENS_STATE_OPERATIONAL);
}

void XsensMTi_RunPeriodic(uint8_t ch)
{
    if (ch >= XSENS_MAX_INSTANCES) return;

    if (s_auto_sense_state[ch] == XSENS_STATE_OPERATIONAL) {
        if (IOIF_TIM_GetTick() - s_last_packet_tick[ch] > XSENS_AUTO_SENSE_TIMEOUT_MS) {
            s_auto_sense_state[ch] = XSENS_STATE_STOPPED;
        }
    }
}
