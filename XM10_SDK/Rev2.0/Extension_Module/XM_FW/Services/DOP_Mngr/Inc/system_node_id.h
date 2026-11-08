/**
 ******************************************************************************
 * @file    system_node_id.h
 * @brief   관절 · 장치 Node ID(SystemNodeID_t) — 공개 API(xm_api_data.h 의 벡터 · 관절 설정 함수)가 쓰는 타입
 * @details data_object_dictionaries.h 에서 이 enum 만 옮겼다(이름 · 값 그대로). 공개 헤더가 내부 사전 전체를
 *          사용자 코드로 끌고 오지 않게 하려는 분리다. data_object_dictionaries.h 가 이 헤더를 include 하므로
 *          내부 코드는 바꿀 것이 없다.
 ******************************************************************************
 */

#pragma once

#ifndef DOP_MNGR_INC_SYSTEM_NODE_ID_H_
#define DOP_MNGR_INC_SYSTEM_NODE_ID_H_

/**
 * @brief XM 시스템 네트워크의 장치 Node ID
 * @note  XM 프로젝트에 필요한 ID만 명확하게 재정의하여 사용합니다.
 * 
 * [DOP V1 vs DOP V2 Node ID]
 * - DOP V1 (Legacy): CM 코드와 일치
 *   - CM Node ID = 0x1 (CM 코드: NODE_ID_CM = 1)
 *   - XM Node ID = 0x2 (CM 코드: NODE_ID_EXTPACK = 2)
 *   - MD_RH/LH Node ID = 0x6/0x7
 * 
 * - DOP V2 (CANopen): AGR_DOP_Node_ID와 일치 (Node ID v3.0, xm_node_id.h 기준)
 *   - XM Node ID = 0x02 (AGR_NODE_ID_XM)
 *   - FES Hub = 0x0C, IMU Hub = 0x0D (단일 노드), EMG Hub = 0x0F
 */
typedef enum {
    SYS_NODE_ID_ALL   = 0x0,
    
    /* DOP V1 (Legacy) - CM, XM, MD */
    SYS_NODE_ID_CM    = 0x1,  // Control Module (CM 코드와 일치)
    SYS_NODE_ID_XM    = 0x2,  // Extension Module (This Module)
    
    // SUIT Actuators (관절 모듈)
    SYS_NODE_ID_RH    = 0x6, // Right Hip(Sagittal)
    SYS_NODE_ID_LH    = 0x7, // Left Hip(Sagittal)
    SYS_NODE_ID_RK    = 0x8, // Right Knee(Sagittal)
    SYS_NODE_ID_LK    = 0x9, // Left Knee(Sagittal)
    SYS_NODE_ID_RA    = 0xA, // Right Ankle (10) (Sagittal)
    SYS_NODE_ID_LA    = 0xB, // Left Ankle  (11) (Sagittal)

    /* DOP V2 (CANopen) - Sensor Modules */
    // @deprecated (2026-08-19) SSOT 불일치 — 실제 통신에는 System/Config/xm_node_id.h 의
    //   AGR_NODE_ID_IMU_HUB_A(0x0D) / AGR_NODE_ID_EMG_HUB(0x0F) 를 사용할 것.
    //   아래 두 값(0x08/0x09)은 이 enum 안에서 SYS_NODE_ID_RK/LK(관절 노드)와 숫자가
    //   겹치는 미사용(참조 0건) 정의 — 신규 코드에서 절대 사용 금지 (오사용 시
    //   실제로는 MD Knee 노드로 잘못 송신됨). 값 충돌이라 제거는 별도 승인 사안.
    SYS_NODE_ID_IMU_HUB = 0x08,  // IMU Hub Module
    SYS_NODE_ID_EMG_HUB = 0x09,  // EMG Hub Module (향후)
    
    // Other Devices (향후 확장용)
    // NODE_ID_SENSOR_HUB_EMG = 0xD,
} SystemNodeID_t;

#endif /* DOP_MNGR_INC_SYSTEM_NODE_ID_H_ */
