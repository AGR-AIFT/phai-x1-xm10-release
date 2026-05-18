/**
 ******************************************************************************
 * @file    xm_periph_stimulus.h
 * @author  HyundoKim
 * @brief   페리페럴 신호 stimulus 생성기 — 회로 SI 측정용.
 * @details
 *  회로 설계팀이 오실로스코프로 SPI/UART/MDIO/RMII/FDCAN 신호 타이밍을
 *  측정할 때, 해당 페리페럴이 의미있는 신호를 *지속적으로* 발생시키도록
 *  하는 stimulus generator.
 *
 *  각 stimulus 는 OD 0x7E00:N (UINT8 RW) 1바이트 SDO Write 로 ON/OFF.
 *  sensor-studio SI Toggle 패널 또는 OD Browser 에서 직접 조작.
 *
 *  자가검증(PASS/FAIL) 과는 분리된 개념 — 자가검증 OD 는 0x2010~ 범위로
 *  추후 Phase 4 에서 별도 정의.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#ifndef XM_PERIPH_STIMULUS_H_
#define XM_PERIPH_STIMULUS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief stimulus 식별자. OD 0x7E00 의 subindex 와 1:1 매핑.
 *        값 0 은 sentinel (NONE), 카운트는 _COUNT_.
 */
typedef enum {
    XM_STIM_NONE        = 0,
    XM_STIM_FDCAN1      = 1,  /**< FDCAN CH1 1ms loopback frame TX */
    XM_STIM_FDCAN2      = 2,  /**< FDCAN CH2 1ms loopback frame TX */
    XM_STIM_SPI_PSRAM   = 3,  /**< APS6404L QUADSPI Read loop */
    XM_STIM_SPI_RTC     = 4,  /**< MCP79510 SPI2 Read loop */
    XM_STIM_SPI_LED     = 5,  /**< PCA9957 SPI5 PWM write loop */
    XM_STIM_UART_GRF    = 6,  /**< USART2 0x55/0xAA pattern TX */
    XM_STIM_MDIO        = 7,  /**< RTL8201F MDIO register Read loop */
    XM_STIM_RMII        = 8,  /**< Ethernet frame TX loop */
    XM_STIM_COUNT_      = 9,
} XM_Stimulus_e;

/**
 * @brief stimulus 모듈 초기화. system_startup 에서 1회 호출.
 *        각 slot 의 step 함수 포인터를 등록 (미구현 stimulus 는 NULL).
 */
void XM_Stimulus_Init(void);

/**
 * @brief 100ms 주기 호출. 활성화된 stimulus 의 step 함수 일괄 실행.
 *        별도 StimulusTask 본체 (xm_periph_stimulus.c) 가 자기 주기로 호출.
 *        외부에서 직접 호출하지 말 것 — task wrapper 전용.
 */
void XM_Stimulus_Process(void);

/**
 * @brief USB CDC connect rising edge hook — StimulusTask resume.
 * @details xm_api_usb.c 의 `is_hw_ready && !s_prev_connected` 분기에서 호출.
 *          sensor-studio 가 DTR 토글로 연결 시 stimulus task 가 dorman 에서
 *          깨어나 100ms 주기 step 실행 시작.
 *
 *          disconnect 는 별도 hook 없음 — task 가 자가 polling 으로 감지하고
 *          모든 stimulus OFF + vTaskSuspend(NULL) 로 자가 suspend.
 */
void XM_Stimulus_OnUsbConnect(void);

/**
 * @brief Connected status mirror update — UserTask 1ms 주기에서 호출.
 * @details g_xm_status_connected[] 를 USB CDC / CM / SM Hub 의 IsConnected
 *          최신 값으로 갱신. stimulus task 의 suspend 상태와 무관하게 항상
 *          동작 → sensor-studio 가 USB 연결 즉시 "● connected" 표시 가능.
 *
 *          stimulus task 안에서 갱신하던 v2 설계는 task suspend 시 update 가
 *          중단되는 모순이 있었음 (사용자 보고 2026-05-13: "USB stand-by 유지").
 *          이 함수로 분리 후 UserTask 의 XM_USB_ProcessPeriodic 이 직접 호출.
 */
void XM_Stimulus_UpdateConnectedStatus(void);

/**
 * @brief stimulus ON/OFF 토글. OD 0x7E00:N write callback 에서 호출.
 * @return true: 적용 성공, false: 잘못된 id 또는 미구현 stimulus
 */
bool XM_Stimulus_SetEnabled(XM_Stimulus_e id, bool enabled);

/**
 * @brief 현재 enabled 상태 조회.
 */
bool XM_Stimulus_IsEnabled(XM_Stimulus_e id);

/**
 * @brief stimulus 이름 (디버그 로그용). NULL 시 "?".
 */
const char* XM_Stimulus_GetName(XM_Stimulus_e id);

/**
 * @brief stimulus 진단 카운터 — sensor-studio 가 OD 0x7E10 로 polling 가능.
 *
 *  alloc_failed         : PSRAM lazy alloc 실패 (1 시도, 0 또는 1)
 *  err_count[id]        : 각 stimulus 의 TX 실패 누적 (1ms 마다 누적)
 *
 *  CPU 비용은 무시할 수준 (atomic load + 비교).
 */
typedef struct {
    uint8_t  psram_alloc_failed;
    uint32_t err_count[XM_STIM_COUNT_];
} XM_Stimulus_Diag_t;

/**
 * @brief 진단 카운터 스냅샷 조회. polling 안전 (atomic 단순 read).
 */
void XM_Stimulus_GetDiag(XM_Stimulus_Diag_t* out);

/**
 * @brief OD 0x7E10 노출용 raw 변수.
 *  - g_stim_psram_alloc_failed : uint8, 1 = 첫 alloc 실패
 *  - g_stim_err_count[N]       : uint32, N = XM_Stimulus_e
 *
 *  xm_production_od.c 가 이 주소를 OD entry pointer 로 등록한다. 1ms tick
 *  context 에서만 write, sensor-studio polling 단일 reader → race 없음.
 */
extern volatile uint8_t  g_stim_psram_alloc_failed;
extern volatile uint32_t g_stim_err_count[XM_STIM_COUNT_];

/**
 * @brief FDCAN Ch2 stimulus enable 거부 사유 — 마지막 SetEnabled(FDCAN2, true) 결과.
 *   0 = 거부 없음 (정상 진입 또는 시도 없음)
 *   1 = SM 연결 중 (Imu/Emg/FesHub IsConnected)
 *   2 = TEC 누적 (≥ XM_STIM_FDCAN_TEC_AUTODISABLE)
 *   3 = Restricted Mode 진입 실패 (HAL_FDCAN_Stop/Start 실패)
 * OD 0x7E40:02 로 노출 — sensor-studio polling 으로 "starting → OFF" 시 즉시 원인 확인.
 */
extern volatile uint8_t  g_stim_fdcan2_reject_reason;

/**
 * @brief stimulus step 실행 카운터 — 1ms 마다 enable 된 slot 의 step 호출 시 ++.
 *  sensor-studio polling 으로 이전 값 대비 증가 여부를 확인하면 "실행 중"
 *  검증 가능 (오실로 없이 GUI 만으로 동작 확인). 1Hz polling 기준 약 1000씩
 *  증가하는 것이 정상.
 */
extern volatile uint32_t g_stim_run_count[XM_STIM_COUNT_];

/**
 * @brief 평시 트래픽 connected 상태 — 1ms tick 마다 외부 IsConnected API 결과를
 *  mirror. OD 0x7E30:N 으로 노출, sensor-studio GUI 가 polling 으로 "연결 OK"
 *  / "미연결" 가시화 (stimulus 가 의미 없는 FDCAN/UART/USB row 의 status).
 *
 *  0x7E30 subindex (XM_Stimulus_e 와 의도적으로 동일 mapping):
 *    0 = USB CDC                    (XM_IsUsbStreamConnected, boolean)
 *    1 = FDCAN1 (CM)                (CM_Drv_IsConnected, boolean)
 *    2 = FDCAN2 (SM)                (bitmap: bit0=IMU, bit1=EMG, bit2=FES Hub)
 *                                    non-zero = connected (boolean 호환 유지)
 *                                    [2026-05-14 v2] 어떤 SM 인지 GUI 표시용
 *    6 = UART GRF (Xsens MTi)       (Xsens attach 상태 — 미구현 시 0)
 *  나머지 subindex 는 stimulus row 와 무관, 사용 안 함.
 */
extern volatile uint8_t g_xm_status_connected[XM_STIM_COUNT_];

/**
 * @brief 각 stimulus 의 마지막 송신/수신 데이터 (uint32, OD 0x7E60:N).
 *
 * GUI 가 polling 으로 값 변화를 시각화 → "진짜 데이터가 흐르고 있구나" 직관적
 * 검증 (오실로 없이도). 각 sub 의 의미:
 *   1 = FDCAN1  : payload[0..3] (tick LSB 4B) — 매 TX 시 ++
 *   2 = FDCAN2  : payload[0..3] (tick LSB 4B)
 *   3 = SPI_PSRAM: memory-mapped read 첫 4B (값이 매번 다르면 PSRAM 응답 OK)
 *   4 = SPI_RTC : MCP79510 status register 값 (low byte)
 *   5 = SPI_LED : 현재 PWM 값 (0xFF ON phase / 0x00 OFF phase)
 *   6 = UART_GRF: pattern 0x55AA (상위 16비트) | tick LSB (하위 16비트)
 *   7 = MDIO    : PHYIDR1 register 값 (RTL8201F = 0x001C 정상)
 *   8 = RMII    : BMSR register 값 (bit2 = link up)
 */
extern volatile uint32_t g_stim_last_data[XM_STIM_COUNT_];

/**
 * @brief FDCAN Ch2 stimulus enable 거부 사유 (OD 0x7E10:09 노출, 2026-05-14).
 *
 *  0 = 거부 없음 (정상 진입 또는 시도 없음)
 *  1 = SM(IMU/EMG/FES Hub) 연결 중 — 평시 PDO 통신 보호 위해 거부
 *  2 = TEC 이미 임계 (>= XM_STIM_FDCAN_TEC_AUTODISABLE) 누적 — 보드 재부팅 필요
 *  3 = Restricted Operation Mode 진입 실패 (HAL_FDCAN_Stop/Start 실패)
 *
 * sensor-studio 가 "FDCAN Ch2 ON 후 즉시 OFF" 시 이 값을 polling 해 원인 표시.
 */
extern volatile uint8_t g_stim_fdcan2_reject_reason;

#ifdef __cplusplus
}
#endif

#endif /* XM_PERIPH_STIMULUS_H_ */
