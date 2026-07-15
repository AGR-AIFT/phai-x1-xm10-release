/**
 ******************************************************************************
 * @file    xm_production_diag.h
 * @author  HyundoKim
 * @brief   PhAI-X1 생산검사 OD 확장 — Button/CM Link/GRF Detail/LED Command
 * @details
 *   xm_production_od.c 의 OD 테이블(SSOT)이 바인딩하는 volatile 미러 변수와,
 *   그 값을 1ms tick 마다 갱신/적용하는 로직을 제공한다. OD 엔트리 등록 자체는
 *   xm_production_od.c 에 그대로 두고(SSOT 단일화), 갱신 로직만 이 파일에 둔다
 *   — 기존 xm_periph_stimulus.h/.c ↔ xm_production_od.c 분리 패턴과 동일 구조.
 *
 *   그룹 정의 (docs/implementation-plan-2026-07-02.md §5.1 SSOT):
 *     0x7E40 Button Status   — :00=count(3, RO), :01~03=raw level(RO, non-consuming)
 *     0x7E50 CM Link Detail  — :01=nmt_state(RO), :02=link_drop_count(RO)
 *     0x7E70 GRF Detail      — :01=total_packets(RO, L+R 합산),
 *                               :02/:03=port L/R 최근 수신 식별자(RO, D9 스왑 검출)
 *     0x7EA0 LED Command     — :00=all_on_test_mode(RW), :01=applied_status(RO)
 *
 *   스레드 안전 계약: 모든 mirror 변수는 volatile 단일 word(uint8/uint32).
 *   writer = UserTask(1kHz, XM_ProductionDiag_Update()) 또는 PnP_Task(~100ms,
 *   cm_drv.c 내부 카운터 — 이 파일은 getter 로 폴링만). reader = USB SDO 핸들러
 *   (동일 UserTask 컨텍스트, cdc_dop_router.c 참조). 멀티워드 일관성이 필요한
 *   구조는 두지 않는다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_PRODUCTION_DIAG_H_
#define XM_PRODUCTION_DIAG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 온보드 기능 버튼 개수 (button_manager 1~3 고정). OD 0x7E40:00. */
#define XM_DIAG_BUTTON_COUNT     (3u)

/** @brief GRF 포트 개수 (L/R). MARVELDEX_MAX_INSTANCES 와 동일 값. */
#define XM_DIAG_GRF_PORT_COUNT   (2u)

/**
 * @brief 1ms tick 갱신 — 버튼/CM Link/GRF Detail 미러 갱신 + LED ALL-ON 적용.
 * @details XM_USB_ProcessPeriodic() 안 XM_Stimulus_UpdateConnectedStatus()
 *          직후 호출 (UserTask 1kHz 컨텍스트, core_process.c 참조). Control_Loop
 *          이 PRODUCTION 모드에서 스킵되는 것과 무관하게 항상 실행되어야
 *          검사 채널이 최신 상태를 본다.
 */
void XM_ProductionDiag_Update(void);

/**
 * @brief production exit/DTR loss 시 LED ALL-ON latch 를 default/off 상태로 복귀.
 */
void XM_ProductionDiag_ForceLedDefault(void);

/**
 * @brief OD 0x7E40:01~03 Button raw level mirror (non-consuming).
 * @details ButtonManager_GetState(1..3) 를 그대로 미러 — PopEvent() 는 절대
 *          사용하지 않는다(사용자 앱의 이벤트 큐를 소비해 오염시키지 않기 위함).
 *          idx 0=BTN1 .. 2=BTN3. 0=뗌, 1=눌림.
 */
extern volatile uint8_t g_xm_diag_btn_raw_level[XM_DIAG_BUTTON_COUNT];

/**
 * @brief OD 0x7E50:01 CM NMT state mirror (CM_NmtState_t 를 uint8 로 캐스팅).
 * @details writer=UserTask 1ms(CM_Drv_GetNmtState() 폴링). 0=INITIALISING,
 *          1=PRE_OPERATIONAL, 2=OPERATIONAL, 3=STOPPED.
 */
extern volatile uint8_t g_xm_diag_cm_nmt_state;

/**
 * @brief OD 0x7E50:02 CM PnP 링크 drop 누적 카운터 mirror.
 * @details writer=UserTask 1ms(CM_Drv_GetLinkDropCount() 폴링). 원본 카운터는
 *          cm_drv.c 가 PnP_Task(~100ms) 컨텍스트에서 증가시키는 단일 word
 *          volatile — 이 mirror 는 그 값을 그대로 복사한 atomic read 결과다.
 */
extern volatile uint32_t g_xm_diag_cm_link_drop_count;

/**
 * @brief OD 0x7E70:01 GRF UART 총 수신 패킷 수 mirror (L+R 합산).
 * @details UartRxHandler_GetDiag(UART_RX_DIAG_TYPE_FSR, ...).total_packets 재사용.
 */
extern volatile uint32_t g_xm_diag_grf_total_packets;

/**
 * @brief OD 0x7E70:02/03 GRF 포트별 최근 수신 식별자 mirror (D9 스왑 검출용).
 * @details idx 는 물리 포트 기준 MARVELDEX_CH_LEFT(0, UART7)/MARVELDEX_CH_RIGHT(1,
 *          UART8). 값은 MarvelDex_packet_t.sensorSpace 그대로 미러한다
 *          (1=MARVELDEX_SENSOR_SPACE_LEFT, 2=..._RIGHT, 3=..._UNKNOWN,
 *          0=아직 유효 패킷 수신 없음/미연결).
 *          포트 L(idx 0) 미러가 2(RIGHT) 를 보고하면 L/R 커넥터가 뒤바뀐 것 —
 *          GUI 가 "포트 idx" 와 "값" 을 비교해 스왑 여부를 판정한다.
 */
extern volatile uint8_t g_xm_diag_grf_port_identity[XM_DIAG_GRF_PORT_COUNT];

/**
 * @brief OD 0x7EA0:00 LED 일괄 ON 테스트 커맨드 (RW). 0=평시 복원, 1=전체 ON.
 * @details write 는 AGR-DOP OD 인프라(agr_od.c)가 이 변수에 직접 memcpy 한다
 *          (단일 byte, Cortex-M aligned write 는 atomic). on_write 콜백은 두지
 *          않는다 — 실제 적용(PCA9957 SPI ≈840us 포함)은 XM_ProductionDiag_Update()
 *          가 이 값의 변화(edge)를 감지해 수행한다. SDO 디코드 콜스택 한복판에서
 *          SPI 트랜잭션을 직접 부르지 않기 위한 설계(§5.2 참조).
 */
extern volatile uint8_t g_xm_diag_led_all_on_cmd;

/**
 * @brief OD 0x7EA0:01 LED 일괄 ON 적용 완료 readback (RO).
 * @details 0=평시 상태로 복원됨(또는 아직 ON 커맨드 미적용), 1=ALL-ON 적용
 *          완료(PCA9957 SPI 갱신 성공까지 확인). GUI 는 :00 write 후 이 값이
 *          1 로 바뀌는 것을 확인한 다음에만 육안검사 단계로 진입해야 한다
 *          (write-then-readback 원칙, false-PASS 방지).
 */
extern volatile uint8_t g_xm_diag_led_applied_status;

#ifdef __cplusplus
}
#endif

#endif /* XM_PRODUCTION_DIAG_H_ */
