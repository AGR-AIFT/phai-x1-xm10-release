/**
 ******************************************************************************
 * @file    uart_rx_handler.c
 * @author  HyundoKim
 * @brief   [System/Comm] UART 센서 데이터 수신 핸들러 (IOIF RxTask 콜백 패턴)
 * @details
 * UART 센서(FSR, IMU) 데이터를 IOIF RxTask에서 파싱 후
 * Device Layer DataLake에 직접 업데이트하는 구조입니다.
 *
 * 데이터 흐름 (V2.0 - ISR 최소화 아키텍처):
 *   DMA Ring Buffer → ISR(StreamBuffer Send) → IOIF RxTask(StreamBuffer Recv)
 *     → Device.ParseByte (Task 컨텍스트)
 *       → Device.UpdateData (DataLake, Mutex 보호)
 *         → Main Task → Device.GetLatest (Snapshot)
 *
 * [V1.0 → V2.0 변경 사유]
 * - ISR 최소화 원칙: ISR에서 파싱/Queue 전달 → ISR에서 StreamBuffer 전달만
 * - Queue/QueueSet/UartRxTask 제거: IOIF 공유 RxTask가 대체
 * - DataLake 직접 업데이트: 콜백이 Task 컨텍스트이므로 Queue 불필요
 *
 * @version 2.0
 * @date    Feb 11, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "uart_rx_handler.h"
#include <string.h>
#include <stdbool.h>

/* RTOS */
#include "FreeRTOS.h"
#include "module.h"     /* TASK_STACK_*, TASK_PRIO_* */
#include "cmsis_os2.h"

/* Devices Layer */
#include "mdaf-25-6850.h"
#include "mti-630.h"

/* System Layer */
#include "ioif_agrb_tim.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* [V2.0] Queue 관련 매크로 제거됨
 * - IOIF RxTask에서 콜백이 Task 컨텍스트로 호출되므로 Queue 불필요
 * - DataLake 직접 업데이트 (Mutex 보호)
 */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 진단 타입 (공개 API용) */
#define DIAG_TYPE_FSR  (0U)
#define DIAG_TYPE_IMU  (1U)

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

/* [V2.0] UartRxTask, Queue, QueueSet 제거됨
 * - IOIF 공유 RxTask가 대체 (ioif_agrb_uart.c 내부)
 * - 콜백이 Task 컨텍스트에서 호출되므로 Queue 불필요
 */

/* ===== Xsens 파서 인스턴스 (System Layer에서 관리) ===== */
static XsensParser_t  s_xsens_parser;
static IOIF_UARTx_t   s_xsens_uart_id;

/* ===== 진단 (Live Expression 모니터링용) ===== */
static UartRxDiag_t s_diag_fsr;
static UartRxDiag_t s_diag_imu;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* [V2.0] StartUartRxTask 제거됨 - IOIF RxTask가 대체 */
static void _OnFsrPacketReceived(const MarvelDex_packet_t* packet);
static void _OnImuPacketReceived(const XsensMTi_packet_t* packet);
static void _OnXsensUartReceive(uint8_t* rx_buf, uint32_t size, uint32_t id);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief FSR UART 및 UartRxTask 초기화
 */
void UartRxHandler_Init(IOIF_UARTx_t grf_left_id, IOIF_UARTx_t grf_right_id)
{
    /* 진단 구조체 초기화 */
    memset(&s_diag_fsr, 0, sizeof(s_diag_fsr));
    memset(&s_diag_imu, 0, sizeof(s_diag_imu));

    /* [V2.0] Queue/QueueSet/UartRxTask 제거됨
     * - IOIF 공유 RxTask가 StreamBuffer를 통해 데이터를 수신
     * - 콜백(_OnFsrPacketReceived, _OnXsensUartReceive)이 Task 컨텍스트에서 호출됨
     * - DataLake 직접 업데이트 가능 (Mutex 보호)
     */

    /* GRF Device 드라이버 초기화 (IOIF RxTask 컨텍스트에서 호출될 콜백 주입) */
    if (marvelDexFSR.init(grf_left_id, grf_right_id, _OnFsrPacketReceived) == false) {
        /* TODO: 치명적 오류 처리 */
    }
}

/**
 * @brief Xsens IMU UART 초기화
 */
void Uart4Rx_XsensIMU_Init(IOIF_UARTx_t imu_id)
{
    /* 1. Xsens 파서 초기화 (Device Layer) */
    s_xsens_uart_id = imu_id;
    xsensMTi630.InitParser(&s_xsens_parser);

    /* 2. UART Rx Idle 콜백 등록 */
    IOIF_UART_SetRxIdleCallback(imu_id, _OnXsensUartReceive);

    /* 3. Xsens IMU 부팅 대기 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 4. Xsens IMU 출력 설정 (사전 설정된 경우 생략) */
    /* xsensMTi630.ConfigureOutput(imu_id); */
}

/**
 * @brief UART Rx 진단 정보를 반환합니다.
 * @param[in] type 0=FSR, 1=IMU
 * @param[out] out_diag 진단 구조체 포인터
 */
void UartRxHandler_GetDiag(uint8_t type, UartRxDiag_t* out_diag)
{
    if (out_diag == NULL) return;
    if (type == DIAG_TYPE_FSR) {
        *out_diag = s_diag_fsr;
    } else if (type == DIAG_TYPE_IMU) {
        *out_diag = s_diag_imu;
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* [V2.0] StartUartRxTask 제거됨 - IOIF 공유 RxTask가 대체 */

/**
 * @brief [IOIF RxTask 컨텍스트] FSR 패킷 수신 콜백 (MarvelDex Device Layer가 호출)
 * @details
 * [V2.0 변경] ISR → Task 컨텍스트로 이동
 * - ISR에서 Queue 전달 → Task에서 DataLake 직접 업데이트
 * - Device Layer의 Mutex가 Main Task와의 동시 접근 보호
 */
static void _OnFsrPacketReceived(const MarvelDex_packet_t* packet)
{
    s_diag_fsr.total_packets++;

    /* [V2.0] DataLake 직접 업데이트 (Task 컨텍스트, Mutex 보호) */
    uint8_t grf_ch = (packet->sensorSpace == MARVELDEX_SENSOR_SPACE_LEFT)
                     ? MARVELDEX_CH_LEFT : MARVELDEX_CH_RIGHT;
    MarvelDex_UpdateData(grf_ch, packet);
}

/**
 * @brief [IOIF RxTask 컨텍스트] IMU 패킷 수신 콜백
 * @details
 * [V2.0 변경] ISR → Task 컨텍스트로 이동
 * - Queue 전달 제거, DataLake 직접 업데이트
 */
static void _OnImuPacketReceived(const XsensMTi_packet_t* packet)
{
    s_diag_imu.total_packets++;

    /* [V2.0] DataLake 직접 업데이트 (Task 컨텍스트, Mutex 보호) */
    XsensMTi_UpdateData(0, packet);  /* XM: ch=0 고정 */
}

/**
 * @brief [IOIF RxTask 컨텍스트] Xsens UART Rx 콜백
 * @details
 * [V2.0 변경] ISR → Task 컨텍스트로 이동
 * - IOIF RxTask가 StreamBuffer에서 읽은 raw bytes로 이 콜백 호출
 * - 바이트 파싱 후 완성된 패킷을 DataLake에 직접 업데이트
 * - ISR 부하 제거 (파싱이 Task에서 수행)
 */
static void _OnXsensUartReceive(uint8_t* rx_buf, uint32_t size, uint32_t id)
{
    if (id != s_xsens_uart_id) return;

    XsensMTi_packet_t parsed_packet;

    for (uint32_t i = 0; i < size; i++) {
        if (xsensMTi630.ParseByte(&s_xsens_parser, rx_buf[i], &parsed_packet)) {
            /* 메타데이터 설정 (Device 파서는 타임스탬프를 설정하지 않음) */
            parsed_packet.timestamp = IOIF_TIM_GetTick();
            parsed_packet.imu_index = 0;  /* XM은 XSENS IMU 1개 */

            /* [V2.0] DataLake 직접 업데이트 (Queue 제거) */
            _OnImuPacketReceived(&parsed_packet);
        }
    }
}
