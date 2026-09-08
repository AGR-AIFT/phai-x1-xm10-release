/**
 ******************************************************************************
 * @file    uart_rx_handler.h
 * @author  HyundoKim
 * @brief   [System/Comm] UART RX 파이프라인 바인딩 (GRF + External UART)
 * @details
 *  - IOIF Idle Event 콜백 등록과 Device 파서 인스턴스 초기화를 담당.
 *  - 매핑: UART7=GRF L, UART8=GRF R, USART2=External UART(PD5/PD6).
 *  - USART2 의 주인은 XM_EXTERNAL_UART_XSENS_ENABLE(module.h)이 빌드 타임에 정한다:
 *      0 = 범용 External Serial (기본) / 1 = Xsens MTi-630.
 *    IOIF RX 콜백 슬롯이 포트당 1개라 둘은 배타적이다 — 자세한 근거는 module.h 참조.
 *  - 센서 미연결 상태에서도 호출 안전. 연결되면 자동으로 OPERATIONAL 전환.
 * @version 0.3
 * @date    Sep 08, 2026
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_COMM_UART_UART_RX_HANDLER_H_
#define SYSTEM_COMM_UART_UART_RX_HANDLER_H_

#include <stdbool.h>
#include "ioif_agrb_uart.h"
#include "module.h"     /* XM_EXTERNAL_UART_XSENS_ENABLE */

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief UartRxHandler_GetDiag() type 인자 — FSR(GRF) 진단 그룹. */
#define UART_RX_DIAG_TYPE_FSR  (0U)
/** @brief UartRxHandler_GetDiag() type 인자 — IMU 진단 그룹. */
#define UART_RX_DIAG_TYPE_IMU  (1U)

/**
 * @brief External Serial 1회 송신 최대 바이트.
 * @note IOIF TX DMA 버퍼 크기와 같아야 한다 — SSOT 는 `ioif_agrb_uart.c` 의
 *       `IOIF_UART_TX_DMA_BUFFER_SIZE`(현재 128U). 그 값은 IOIF 헤더로 노출되지
 *       않아 여기서 다시 적는다. IOIF 를 범프하면 이 값을 대조할 것.
 *       (`ioif_agrb_uart.h` 주석의 "최대 256 bytes" 는 stale 이다.)
 */
#define EXTERNAL_SERIAL_TX_MAX_BYTES  (128U)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief UART Rx 진단 구조체 (Live Expression 모니터링용)
 * @details cursorrule 13-comm-core-patterns 참조
 *
 * 모니터링 기준:
 *   - queue_full_count > 0   : Task 우선순위 낮거나 시스템 과부하
 *   - max_batch_seen > 1     : 정상적 지연 (1~2는 OK, 3+ 주의)
 *   - batch_overflow_count > 0 : 치명적 타이밍 문제
 */
typedef struct {
    volatile uint32_t queue_full_count;      /**< ISR: Queue Full 횟수 (0이어야 정상) */
    volatile uint32_t max_batch_seen;        /**< Task: while 루프 최대 batch 크기 */
    volatile uint32_t batch_overflow_count;  /**< Task: batch_max 초과 횟수 (0이어야 정상) */
    volatile uint32_t total_packets;         /**< ISR: 총 수신 패킷 수 */
} UartRxDiag_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief FSR(GRF) UART RX 파이프라인 바인딩 (UART7 = Left, UART8 = Right)
 * @details
 *  - MarvelDex MDAF-25-6850 드라이버 init + IOIF Idle 콜백 등록.
 *  - 센서 미장착 상태에서도 호출 안전.
 * @param[in] grf_left_id  왼발 FSR UART 인스턴스 ID
 * @param[in] grf_right_id 오른발 FSR UART 인스턴스 ID
 */
void UartRxHandler_Init(IOIF_UARTx_t grf_left_id, IOIF_UARTx_t grf_right_id);

/**
 *------------------------------------------------------------
 * External UART (USART2, PD5/PD6) — 주인이 둘 중 하나로 갈린다
 *   XM_EXTERNAL_UART_XSENS_ENABLE == 0 → 범용 External Serial (아래 ExternalSerial_*)
 *   XM_EXTERNAL_UART_XSENS_ENABLE == 1 → Xsens MTi-630     (아래 XsensMTi630_*)
 * 근거는 module.h 의 해당 매크로 주석.
 *------------------------------------------------------------
 */

#if !XM_EXTERNAL_UART_XSENS_ENABLE

/**
 * @brief [범용 Serial] External UART RX 수신 콜백 타입
 * @param[in] data 수신 바이트 (IOIF 공유 RxTask 스택 버퍼)
 * @param[in] len  이번 호출로 전달된 바이트 수
 *
 * @warning 호출 컨텍스트 = IOIF **공유 RxTask**(prio 54). 이 태스크는 GRF UART7/8 과
 *          같은 순차 루프를 돈다 — 여기서 오래 끌면 GRF 1kHz 수신이 그만큼 밀린다.
 *          **자기 버퍼로 복사만 하고 즉시 리턴**하고, 파싱은 Control_Loop() 에서 한다.
 * @warning `data` 는 RxTask 스택 버퍼(128B)를 가리킨다 — **리턴 후 무효**다. 포인터를
 *          보관하지 말고 내용을 복사할 것.
 * @warning 한 번의 콜백 = 한 개의 프레임이 **아니다**. 스트림은 최대 128B 청크로 쪼개져
 *          여러 번 전달되고, 한 청크에 두 프레임이 붙어 올 수도 있다. 프레임 경계는
 *          호출자가 바이트 상태기계로 찾아야 한다.
 */
typedef void (*ExternalSerialRxFunc_t)(const uint8_t* data, uint32_t len);

/**
 * @brief [범용 Serial] External UART 에 수신 콜백을 결합한다.
 * @param[in] rx_callback  수신 콜백. NULL 이면 수신 해제.
 * @return true=등록됨 / false=포트 미할당(System_Startup 미완료)
 * @note 포트(USART2)는 System_GetExternalUartId() 로 내부에서 해석한다.
 */
bool ExternalSerial_Attach(ExternalSerialRxFunc_t rx_callback);

/**
 * @brief [범용 Serial] 통신 속도 변경 (상대 장비와 같은 값이어야 한다)
 * @return true=성공. 8N1 은 고정이며 런타임 변경 수단이 없다.
 */
bool ExternalSerial_SetBaudrate(IOIF_UART_Baudrate_e baudrate);

/**
 * @brief [범용 Serial] 논블로킹 송신 (1회 최대 128 bytes)
 * @return true=DMA 송신 시작됨 / false=이전 송신 진행 중이거나 인자 오류
 * @note 1kHz Control_Loop 에서 호출하도록 만든 경로다 — 절대 블로킹하지 않는다.
 */
bool ExternalSerial_Send(const void* data, uint32_t len);

/**
 * @brief [범용 Serial] 블로킹 송신 (Control_Setup() 전용)
 * @warning 최대 5초까지 블로킹될 수 있다. Control_Loop() 이나 RX 콜백에서 호출 금지.
 */
bool ExternalSerial_SendBlocking(const void* data, uint32_t len);

/**
 * @brief [범용 Serial] RX 재무장 보장 (프레이밍/노이즈/오버런 에러 후 침묵 방지)
 * @note 멱등. 정상 수신 중이면 상태 읽기 1회의 no-op. ~100ms 주기 호출 권장.
 */
void ExternalSerial_EnsureRxArmed(void);

#else  /* XM_EXTERNAL_UART_XSENS_ENABLE */

/**
 * @brief Xsens MTi-630 IMU를 지정 UART에 결합 (현재 매핑: USART2 = External UART PD5/PD6)
 * @details
 *  - 파서 인스턴스 초기화 + IOIF Idle Event 콜백 등록만 수행 (포트는 이미 AssignInstance 완료 가정)
 *  - 센서 미연결 상태에서도 호출 안전. 케이블 꽂으면 자동 OPERATIONAL 전환.
 *  - 센서 Output Configuration은 EEPROM 보존이 일반적이므로 여기서 호출하지 않음.
 *    신품/공장 초기화 센서는 XsensMTi630_ConfigureOutput()을 별도 호출.
 *  - 함수명에 모델(MTi-630)을 명시한 이유: 향후 USART2가 범용 Serial로 전환되어도 본 API는
 *    "Xsens MTi-630을 명시적 결합" 의미로 그대로 재사용 가능.
 * @param[in] imu_id  IOIF UART 인스턴스 ID
 */
void XsensMTi630_AttachUart(IOIF_UARTx_t imu_id);

/**
 * @brief Xsens MTi-630 Output Configuration 1회 송신 (1kHz Quat+Acc+Gyro)
 * @details
 *  - 신품/공장 초기화 센서에만 필요. EEPROM에 이미 저장된 센서면 호출 불필요.
 *  - 블로킹 ~1초 (GoToConfig + SetOutputCfg + GoToMeasurement, 각 200ms 대기 포함).
 *  - XsensMTi630_AttachUart() 선행 호출 필수. 미호출 시 무시.
 *  - Control_Setup() 또는 별도 명시 시점에 호출 권장 (1kHz 제어 루프 안에서는 호출 금지).
 */
void XsensMTi630_ConfigureOutput(void);

#endif /* XM_EXTERNAL_UART_XSENS_ENABLE */

/**
 * @brief UART Rx 진단 정보를 반환합니다. (Live Expression 모니터링용)
 * @param[in] type 0=FSR, 1=IMU
 * @param[out] out_diag 진단 구조체 포인터
 */
void UartRxHandler_GetDiag(uint8_t type, UartRxDiag_t* out_diag);

#endif /* SYSTEM_COMM_UART_UART_RX_HANDLER_H_ */
