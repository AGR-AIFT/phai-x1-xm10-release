/**
 ******************************************************************************
 * @file    cdc_dop_router.c
 * @author  HyundoKim
 * @brief   USB-CDC ↔ AGR DOP Serial 양방향 라우터 + 호스트 모드 자동 전환.
 * @details
 *  ┌─────────────────────────────────────────────────────────────────────┐
 *  │  USB-CDC 호스트 종류 분리 (XM 전용):                                │
 *  │   PHAI       — PhAI Studio 실시간 telemetry (Total Data auto-pump)  │
 *  │   PRODUCTION — sensor-studio HW 검증/SI/양산 (COBS+CRC DOP)         │
 *  │   TERMINAL   — 예제 / Raw 텍스트 (사용자 명시)                       │
 *  │                                                                     │
 *  │  자동 전환 규칙:                                                    │
 *  │   default (DTR=1) → PHAI                                            │
 *  │   첫 유효 DOP frame(SDO Request) 도달 → PRODUCTION latch            │
 *  │   DTR=0 → PHAI 자동 복귀 + lock 해제 (cdc_handler 가 처리)          │
 *  │                                                                     │
 *  │  PRODUCTION 진입 5단계 시퀀스 (race 방지):                          │
 *  │   1) on_sdo_request 콜백 안에서 latch (검증된 frame 시점 확보)       │
 *  │   2) PhAI auto-pump 즉시 차단 (CdcStream_ForceStopStreaming)         │
 *  │   3) auto-stream 게이트도 잠금 (DTR re-trigger 차단)                 │
 *  │   4) sentinel 0x00 1B 송신 — 잘린 PhAI frame 의 COBS 경계 강제 종료 │
 *  │   5) Heartbeat 즉시 1회 + 100ms 주기 송신 시작                       │
 *  └─────────────────────────────────────────────────────────────────────┘
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "cdc_dop_router.h"
#include "cdc_handler.h"
#include "agr_dop_serial.h"
#include "xm_api_usb.h"
#include "xm_production_od.h"

#include <stdatomic.h>

/* RX chunk 크기 — non-blocking 분할 처리. 크게 잡지 않음 (latency). */
#define CDC_DOP_RX_CHUNK            128u

/* Heartbeat 주기 (단위: ProcessPeriodic tick = 1ms 가정).
 * 100ms 가 적당 — sensor-studio default heartbeat timeout 은 1s. 여유 10×.
 */
#define CDC_DOP_HB_PERIOD_TICKS     100u

/* sensor-studio AGR DOP Heartbeat state byte: 0x05 = Operational (CiA 301). */
#define CDC_DOP_HB_STATE_OPERATIONAL  0x05u

static bool                 s_router_ready = false;
static volatile atomic_uint s_hb_tick      = 0;

static int32_t _TxToCdc(const uint8_t* data, uint32_t len)
{
    if (data == NULL || len == 0u) return 0;
    return CdcStream_Send(data, len) ? 0 : -1;
}

/* ── on_sdo_request: 첫 유효 DOP frame 도달 신호 ───────────────────────
 * agr_dop_serial.c::_OnFrameReceived 가 SDO Request 수신 + Decode 성공 시
 * 호출. CRC + COBS 통과한 *검증된* frame 시점이라 PRODUCTION latch 안전.
 * 응답 자체는 rsp.cs = 0 유지하여 default OD 핸들러로 fallthrough.
 */
static void _OnFirstSdoRequest(const AGR_SDO_Msg_t* req, AGR_SDO_Msg_t* rsp)
{
    (void)req;
    (void)rsp;  /* cs 미설정 → core SDO 핸들러가 처리 */

    if (XM_USB_RequestProductionLatch()) {
        /* 실제 모드 전환 발생 (PHAI → PRODUCTION).
         * XM_USB_RequestProductionLatch 가 ForceStopStreaming + 게이트 잠금
         * 까지 처리. 여기서는 wire-level cleanup 만 추가:
         *   - sentinel 0x00 1B 송신 — 잘린 PhAI frame 의 COBS 경계 강제 종료.
         *     sensor-studio COBS 파서가 garbage frame 1개만 버리고 깨끗히
         *     다음 frame 부터 정상 파싱.
         */
        const uint8_t sentinel = 0x00u;
        CdcStream_Send(&sentinel, 1);

        /* Heartbeat 즉시 1회 송신 — sensor-studio 1s timeout 안에 응답.
         * 이후 주기 송신은 CdcDopRouter_Process tick 카운터에서.
         */
        AGR_Serial_Ctx_t* sctx = XM_ProductionOD_GetSerialCtx();
        if (sctx != NULL) {
            AGR_Serial_SendHeartbeat(sctx, CDC_DOP_HB_STATE_OPERATIONAL);
        }
        atomic_store(&s_hb_tick, 0);
    }
}

void CdcDopRouter_Init(void)
{
    if (s_router_ready) return;

    XM_ProductionOD_Init();

    AGR_Serial_Ctx_t* sctx = XM_ProductionOD_GetSerialCtx();
    AGR_DOP_Ctx_t*    dctx = XM_ProductionOD_GetDopCtx();
    if (sctx == NULL || dctx == NULL) return;

    /* on_sdo_request 콜백 등록 — AGR_MW 무수정으로 first-frame detection.
     * 콜백은 latch 만 trigger, rsp 손대지 않음 → core OD 핸들러가 응답 처리.
     */
    dctx->on_sdo_request = _OnFirstSdoRequest;

    if (AGR_Serial_Init(sctx, dctx, _TxToCdc) != 0) {
        return;
    }

    s_router_ready = true;
}

uint32_t CdcDopRouter_Process(void)
{
    if (!s_router_ready) return 0u;

    AGR_Serial_Ctx_t* sctx = XM_ProductionOD_GetSerialCtx();
    if (sctx == NULL) return 0u;

    uint8_t  buf[CDC_DOP_RX_CHUNK];
    uint32_t total = 0u;

    /* RX: 누적 chunk 분할 처리. AGR_Serial_ProcessRxData 가 COBS 디코딩 +
     * CRC 검증 + 라우팅까지 수행. 유효 SDO frame 도달 시 _OnFirstSdoRequest
     * 가 자동 호출되어 PRODUCTION latch.
     */
    for (;;) {
        uint32_t n = CdcStream_Read(buf, sizeof(buf));
        if (n == 0u) break;
        AGR_Serial_ProcessRxData(sctx, buf, n);
        total += n;
        if (n < sizeof(buf)) break;
    }

    /* TX: PRODUCTION 모드일 때만 Heartbeat 주기 송신.
     * sensor-studio 가 "지원 모듈" 로 인식하기 위한 필수 신호.
     */
    if (XM_USB_GetMode() == XM_USB_MODE_PRODUCTION) {
        unsigned int tick = atomic_fetch_add(&s_hb_tick, 1) + 1u;
        if (tick >= CDC_DOP_HB_PERIOD_TICKS) {
            atomic_store(&s_hb_tick, 0);
            AGR_Serial_SendHeartbeat(sctx, CDC_DOP_HB_STATE_OPERATIONAL);
        }
    } else {
        /* PHAI / TERMINAL 에서는 HB 카운터 정지 (재진입 시 0 부터). */
        atomic_store(&s_hb_tick, 0);
    }

    return total;
}

bool CdcDopRouter_IsDopHostActive(void)
{
    return XM_USB_GetMode() == XM_USB_MODE_PRODUCTION;
}
