/**
 ******************************************************************************
 * @file    mdaf-25-6850.c
 * @author  HyundoKim
 * @brief   
 * @version 2.0 (Multi-Instance Auto-Sense + DataLake)
 * @date    Feb 11, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "mdaf-25-6850.h"

#include <string.h>

#include "ioif_agrb_tim.h" // IOIF_TIM_GetTick() 사용

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define QUEUE_SIZE          (16)    // 16개 패킷 버퍼링
#define FSR_HEADER_0        (0xFF)
#define FSR_HEADER_1        (0xFF)
#define FSR_FOOTER          (0xFE)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief FSR 패킷 프레이밍(Framing) 상태
 */
typedef enum {
    STATE_WAIT_HEADER_0,    // 0xFF 대기
    STATE_WAIT_HEADER_1,    // 0xFF 대기
    STATE_COLLECT_PAYLOAD   // 나머지 26바이트 수집
} ParseState_t;

/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static IOIF_UARTx_t _instance[MAX_SENSOR_NUM]; // [0]: 왼발, [1]: 오른발

/* System Layer가 등록한 콜백 (파싱된 패킷 전달용) */
static FsrPacketCallback_t s_packet_callback = NULL;

// 각 센서(UART 인스턴스)별 파싱 상태를 독립적으로 관리
static ParseState_t _parseState[MAX_SENSOR_NUM] = {STATE_WAIT_HEADER_0, STATE_WAIT_HEADER_0};
static MarvelDex_raw_packet_t _rx_buffer[MAX_SENSOR_NUM];
static uint8_t _rx_index[MAX_SENSOR_NUM] = {0, 0};

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static bool initialize(IOIF_UARTx_t id0, IOIF_UARTx_t id1, FsrPacketCallback_t packet_cb);
static int32_t get_sensor_index(IOIF_UARTx_t id);
static void callback(uint8_t* rx_buf, uint32_t size, uint32_t id);
static bool parsing(MarvelDex_raw_packet_t* raw_packet, MarvelDex_packet_t* output);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

MarvelDexFSR_t marvelDexFSR = {
    .init = initialize,
};

//초기화 함수. 태스크가 시작하기 전 또는 크리티컬 섹션 등 안전한 상태에서 호출 권장
/**
 * @brief 드라이버 초기화 (System Layer에서 호출)
 */
/**
 * @brief 드라이버 초기화 (System Layer에서 호출)
 */
static bool initialize(IOIF_UARTx_t id0, IOIF_UARTx_t id1, FsrPacketCallback_t packet_cb)
{
    _instance[0] = id0;
    _instance[1] = id1;

    // System Layer 콜백 저장
    s_packet_callback = packet_cb;

    // IOIF UART 계층에 'callback' 함수를 등록
    IOIF_UART_SetRxIdleCallback(id0, callback);
    IOIF_UART_SetRxIdleCallback(id1, callback);

    return true;
}

/**
 * @brief IOIF UARTx_t ID를 내부 인덱스(0 또는 1)로 변환
 */
static int32_t get_sensor_index(IOIF_UARTx_t id)
{
    if (id == _instance[0]) return 0;
    if (id == _instance[1]) return 1;
    return -1; // 등록되지 않은 ID
}

/**
 * @brief IOIF UART 태스크에 의해 호출되는 콜백 (패킷 프레이밍)
 */
static void callback(uint8_t* rx_buf, uint32_t size, uint32_t id)
{
    int32_t idx = get_sensor_index(id);
    if (idx == -1) return; // 알 수 없는 UART 인스턴스

    // 이 인스턴스(센서)의 상태 변수들을 가져옴
    ParseState_t* state = &_parseState[idx];
    uint8_t* buffer = _rx_buffer[idx].raw;
    uint8_t* count = &_rx_index[idx];

    for (uint32_t i = 0; i < size; i++) {
        uint8_t byte = rx_buf[i];
        
        switch (*state) {
            case STATE_WAIT_HEADER_0:
                // 1. 헤더 0 (0xFF) 대기
                if (byte == FSR_HEADER_0) {
                    buffer[0] = byte;
                    *count = 1;
                    *state = STATE_WAIT_HEADER_1;
                }
                break;

            case STATE_WAIT_HEADER_1:
                // 2. 헤더 1 (0xFF) 대기
                if (byte == FSR_HEADER_1) {
                    buffer[1] = byte;
                    *count = 2;
                    *state = STATE_COLLECT_PAYLOAD;
                } else {
                    // 0xFF 다음 0xFF가 아니면 리셋
                    *state = STATE_WAIT_HEADER_0;
                    *count = 0;
                }
                break;

            case STATE_COLLECT_PAYLOAD:
                // 3. 나머지 26바이트 수집
                buffer[*count] = byte;
                (*count)++;

                if (*count >= MARVELDEX_RAW_PACKET_SIZE) {
                    // 4. 28바이트가 모두 모이면 파싱 시도
                    MarvelDex_packet_t parsed_packet;
                    if (parsing(&_rx_buffer[idx], &parsed_packet)) {
                        // [!! 핵심 수정 !!] 큐에 넣는 대신, System Layer 콜백 호출
                        if (s_packet_callback != NULL) {
                            s_packet_callback(&parsed_packet);
                        }
                    }
                    // 6. 상태 리셋 (파싱 성공/실패 무관)
                    // 파싱 성공/실패 여부와 관계없이
                    // 상태, 카운트, 그리고 버퍼를 모두 리셋해야 합니다.
                    *state = STATE_WAIT_HEADER_0;
                    *count = 0;
                    
                    // (필수) 'Stale Footer' 버그를 막기 위한 가장 효율적인 방법
                    // 버퍼 전체를 0으로 닦는 대신,
                    // 파싱에 사용되는 푸터 바이트(27번 인덱스)만
                    // 유효하지 않은 값(예: 0x00)으로 덮어씁니다.
                    // _rx_buffer[idx].raw[MARVELDEX_RAW_PACKET_SIZE - 1] = 0x00;
                    // Or (선택 사항) 버퍼 전체를 0으로 클리어
                    memset(buffer, 0, MARVELDEX_RAW_PACKET_SIZE);
                }
                break;
        }
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 28바이트 원시 패킷을 구조체로 파싱 (FSR 예제 코드 반영)
 */
static bool parsing(MarvelDex_raw_packet_t* raw_packet, MarvelDex_packet_t* output)
{
    // 1. 푸터 검증
    if (raw_packet->raw[27] != FSR_FOOTER) {
        return false; // 잘못된 푸터 싱크워드
    }
    
    // (헤더는 callback 함수에서 이미 검증되었음)

    // 2. 타임스탬프
    // FSR 예제 코드의 주석 권장 사항대로, 수신 시점의 XM10 마스터의 시스템 틱으로 덮어씁니다.
    output->timestamp = IOIF_TIM_GetTick();
    
    // (참고) FSR 패킷 내부의 타임스탬프 (Big-endian) __REV()와 동일한 동작
    // XM10 Master의 시간이 아닌 센서의 시간이 필요하다면 아래와 같이 파싱하면 됨.
    // uint32_t packet_timestamp = ((uint32_t)raw_packet->raw[4] << 24) |
    //                             ((uint32_t)raw_packet->raw[5] << 16) |
    //                             ((uint32_t)raw_packet->raw[6] << 8)  |
    //                             ((uint32_t)raw_packet->raw[7]);

    // 3. 센서 공간 및 롤링 인덱스 파싱
    output->sensorSpace = raw_packet->raw[8];
    output->rollingIndex = raw_packet->raw[9];

    // 4. 14개 센서 채널 파싱 (memcpy로 최적화)
    memcpy(output->sensorData, &raw_packet->raw[10], MARVELDEX_CHANNEL_SIZE);

    // 5. 배터리 레벨 및 상태 플래그 파싱
    output->batteryLevel = raw_packet->raw[24];
    output->statusFlags = raw_packet->raw[25];

    return true; // 파싱 성공
}

/* ================================================================
 * Auto-Sense + DataLake 구현 (Multi-Instance)
 * ================================================================
 * - DataLake: Mutex + Snapshot (Task-Task 표준 패턴)
 * - Auto-Sense: 데이터 타임아웃 기반 STOPPED ↔ OPERATIONAL 전환
 * - Multi-Instance: ch 파라미터로 인스턴스 구분
 *   ch=0: 왼발 (MARVELDEX_CH_LEFT)
 *   ch=1: 오른발 (MARVELDEX_CH_RIGHT)
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
#define GRF_AUTO_SENSE_TIMEOUT_MS  (500U)

typedef enum {
    GRF_STATE_STOPPED,
    GRF_STATE_OPERATIONAL,
} GrfAutoSenseState_t;

typedef struct {
    volatile GrfAutoSenseState_t state;
    volatile uint32_t            lastPacketTick;
} GrfAutoSenseInst_t;

/* Auto-Sense + DataLake (인스턴스별 배열) */
static GrfAutoSenseInst_t s_auto_sense[MARVELDEX_MAX_INSTANCES];
static MarvelDex_packet_t s_grf_datalake[MARVELDEX_MAX_INSTANCES];

#if defined(USE_FREERTOS)
static SemaphoreHandle_t  s_grf_mutex[MARVELDEX_MAX_INSTANCES];
#endif

/* ===== Public Functions ===== */

void MarvelDex_StateInit(uint8_t ch)
{
    if (ch >= MARVELDEX_MAX_INSTANCES) return;

    memset(&s_grf_datalake[ch], 0, sizeof(s_grf_datalake[ch]));
    s_auto_sense[ch].state = GRF_STATE_STOPPED;
    s_auto_sense[ch].lastPacketTick = 0;

#if defined(USE_FREERTOS)
    if (s_grf_mutex[ch] == NULL) {
        s_grf_mutex[ch] = xSemaphoreCreateMutex();
    }
#endif
}

void MarvelDex_UpdateData(uint8_t ch, const MarvelDex_packet_t* packet)
{
    if (ch >= MARVELDEX_MAX_INSTANCES) return;
    if (packet == NULL) return;

    /* Auto-Sense: 데이터 수신 → OPERATIONAL */
    s_auto_sense[ch].state = GRF_STATE_OPERATIONAL;
    s_auto_sense[ch].lastPacketTick = IOIF_TIM_GetTick();

#if defined(USE_FREERTOS)
    /* Mutex + DataLake: Writer (UartRxTask) */
    xSemaphoreTake(s_grf_mutex[ch], portMAX_DELAY);
    s_grf_datalake[ch] = *packet;
    xSemaphoreGive(s_grf_mutex[ch]);
#else
    /* BareMetal: volatile 직접 쓰기 (ISR Priority 보호) */
    s_grf_datalake[ch] = *packet;
#endif
}

bool MarvelDex_GetLatest(uint8_t ch, MarvelDex_packet_t* out)
{
    if (ch >= MARVELDEX_MAX_INSTANCES) return false;
    if (out == NULL) return false;
    if (s_auto_sense[ch].state != GRF_STATE_OPERATIONAL) return false;

#if defined(USE_FREERTOS)
    /* Mutex + Snapshot: Reader (Core Process) */
    xSemaphoreTake(s_grf_mutex[ch], portMAX_DELAY);
    *out = s_grf_datalake[ch];
    xSemaphoreGive(s_grf_mutex[ch]);
#else
    /* BareMetal: volatile 직접 읽기 */
    *out = s_grf_datalake[ch];
#endif

    return true;
}

bool MarvelDex_IsOnline(uint8_t ch)
{
    if (ch >= MARVELDEX_MAX_INSTANCES) return false;
    return (s_auto_sense[ch].state == GRF_STATE_OPERATIONAL);
}

void MarvelDex_RunPeriodic(uint8_t ch)
{
    if (ch >= MARVELDEX_MAX_INSTANCES) return;

    if (s_auto_sense[ch].state == GRF_STATE_OPERATIONAL) {
        if (IOIF_TIM_GetTick() - s_auto_sense[ch].lastPacketTick > GRF_AUTO_SENSE_TIMEOUT_MS) {
            s_auto_sense[ch].state = GRF_STATE_STOPPED;
        }
    }
}
