/**
 ******************************************************************************
 * @file    cm_nmt_state.h
 * @brief   CM 연결 상태(CM_NmtState_t) — 공개 API(XM_GetXMNmtState)와 CM 드라이버가 함께 쓰는 타입
 * @details cm_drv.h 에서 이 enum 만 옮겼다(이름 · 값 그대로). 공개 헤더가 CM 드라이버와 내부 사전 전체를
 *          사용자 코드로 끌고 오지 않게 하려는 분리다. cm_drv.h 가 이 헤더를 include 하므로 내부 코드는 바꿀 것이 없다.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_CONTROL_MODULE_INC_CM_NMT_STATE_H_
#define DEVICES_CONTROL_MODULE_INC_CM_NMT_STATE_H_

/* ===== PnP NMT 상태 정의 (cm_xm_link.h에서 이동) =====
 * [변경 이유] Link → Device 병합 (Phase 5)
 * - 값은 CM의 ExtLinkNmtState_t와 동일 (SDO 통신 호환성)
 */
typedef enum {
    CM_NMT_INITIALISING = 0,
    CM_NMT_PRE_OPERATIONAL,
    CM_NMT_OPERATIONAL,
    CM_NMT_STOPPED
} CM_NmtState_t;

#endif /* DEVICES_CONTROL_MODULE_INC_CM_NMT_STATE_H_ */
