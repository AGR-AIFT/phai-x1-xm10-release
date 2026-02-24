/**
 ******************************************************************************
 * @file    agr_nmt.h
 * @author  HyundoKim
 * @brief   AGR Network Management - 통신 무관 연결 상태 관리
 * @version 1.0
 * @date    Dec 2, 2025
 *
 * @details
 * AGR-NMT는 CANopen NMT를 참고하여 설계된 통신 프로토콜 무관한 연결 상태 관리 모듈입니다.
 * CAN, UART, SPI, I2C 등 모든 통신 방식에서 사용할 수 있습니다.
 * 
 * [주요 기능]
 * - NMT 상태 관리 (BOOT_UP → PRE_OPERATIONAL → OPERATIONAL ↔ STOPPED)
 * - 타임아웃 기반 연결 감지 (Heartbeat 대용)
 * - 상태 변경 콜백
 * 
 * [사용 예시 - CAN 모듈]
 * ```c
 * static AGR_NMT_Inst_t s_nmt;
 * 
 * void CM_Init(void) {
 *     AGR_NMT_Init(&s_nmt, 1000);  // 1초 타임아웃
 * }
 * 
 * void CM_OnPacketReceived(void) {
 *     AGR_NMT_UpdateActivity(&s_nmt, GetTick());
 * }
 * 
 * void CM_RunPeriodic(void) {
 *     AGR_NMT_CheckTimeout(&s_nmt, GetTick());
 * }
 * ```
 * 
 * [사용 예시 - UART 센서]
 * ```c
 * static AGR_NMT_Inst_t s_nmt_L, s_nmt_R;
 * 
 * void GRF_Init(void) {
 *     AGR_NMT_Init(&s_nmt_L, 500);  // 500ms 타임아웃
 *     AGR_NMT_Init(&s_nmt_R, 500);
 * }
 * ```
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_NMT_H
#define AGR_NMT_H

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * NMT STATE (CANopen 호환)
 *-----------------------------------------------------------
 * @brief 네트워크 관리 상태 (CiA 301 호환)
 * @note  상태 값은 CANopen Heartbeat 프로토콜과 호환됩니다.
 */
typedef enum {
    AGR_NMT_BOOT_UP         = 0x00,  /**< 부팅 중 (Boot-up 메시지 전송 대기) */
    AGR_NMT_STOPPED         = 0x04,  /**< 정지 (에러 또는 명령에 의해) */
    AGR_NMT_OPERATIONAL     = 0x05,  /**< 정상 동작 (PDO 송수신 가능) */
    AGR_NMT_PRE_OPERATIONAL = 0x7F,  /**< 설정 중 (SDO만 가능, PDO Mapping 등) */
} AGR_NMT_State_t;

/**
 *-----------------------------------------------------------
 * NMT COMMAND (CANopen 호환)
 *-----------------------------------------------------------
 * @brief NMT 명령어 (Master → Slave)
 */
typedef enum {
    AGR_NMT_CMD_START       = 0x01,  /**< Enter OPERATIONAL */
    AGR_NMT_CMD_STOP        = 0x02,  /**< Enter STOPPED */
    AGR_NMT_CMD_PRE_OP      = 0x80,  /**< Enter PRE-OPERATIONAL */
    AGR_NMT_CMD_RESET_NODE  = 0x81,  /**< Reset Node (Application Reset) */
    AGR_NMT_CMD_RESET_COMM  = 0x82,  /**< Reset Communication */
} AGR_NMT_Cmd_t;

/**
 *-----------------------------------------------------------
 * NMT INSTANCE
 *-----------------------------------------------------------
 * @brief 각 연결(노드)별로 생성하는 NMT 인스턴스
 * @note  volatile 사용: ISR과 메인 루프에서 공유될 수 있음
 */
typedef struct AGR_NMT_Inst AGR_NMT_Inst_t;

/** @brief 상태 변경 콜백 타입 */
typedef void (*AGR_NMT_StateChangedCb_t)(AGR_NMT_State_t old_state, 
                                          AGR_NMT_State_t new_state, 
                                          void* user_ctx);

/** @brief 타임아웃 콜백 타입 */
typedef void (*AGR_NMT_TimeoutCb_t)(void* user_ctx);

struct AGR_NMT_Inst {
    /* ===== CANopen NMT 상태 ===== */
    volatile AGR_NMT_State_t state;             /**< 현재 NMT 상태 (BOOT_UP/PRE_OP/OPERATIONAL/STOPPED) */
    volatile uint32_t        last_activity_ms;  /**< 마지막 Heartbeat 수신 시간 (Timeout 체크용) */
    uint32_t                 timeout_ms;        /**< Heartbeat Timeout (ms), 0=비활성화 */
    uint8_t                  node_id;           /**< CANopen Node ID (1~127) */
    
    /* ===== 콜백 (AGR_PnP가 설정) ===== */
    AGR_NMT_StateChangedCb_t on_state_changed;  /**< 상태 변경 시 호출 (BOOT_UP → PRE_OP → OPERATIONAL) */
    AGR_NMT_TimeoutCb_t      on_timeout;        /**< Heartbeat Timeout 시 호출 (OPERATIONAL → STOPPED) */
    void*                    user_ctx;          /**< 콜백에 전달할 컨텍스트 (AGR_PnP_Device_t*) */
};

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

/**
 * @brief NMT 인스턴스 초기화
 * @param inst       인스턴스 포인터
 * @param timeout_ms 타임아웃 (ms), 0이면 타임아웃 검사 비활성화
 * @note  초기 상태는 AGR_NMT_BOOT_UP입니다.
 */
void AGR_NMT_Init(AGR_NMT_Inst_t* inst, uint32_t timeout_ms);

/**
 * @brief 확장 초기화 (Node ID, 콜백 설정)
 * @param inst              NMT 인스턴스
 * @param timeout_ms        Heartbeat Timeout (ms)
 * @param node_id           CANopen Node ID (1~127)
 * @param on_state_changed  상태 변경 콜백 (NULL 가능)
 * @param on_timeout        Heartbeat Timeout 콜백 (NULL 가능)
 * @param user_ctx          콜백에 전달될 컨텍스트 (AGR_PnP_Device_t*)
 * 
 * @details
 * CANopen 표준 PnP 전용 (Protocol-based만 사용)
 * - BOOT_UP → PRE_OPERATIONAL → OPERATIONAL
 * - 명시적인 NMT START 명령 필요
 * 
 * @note UART 센서는 PnP를 사용하지 않으므로 AGR_NMT 불필요
 */
void AGR_NMT_InitEx(AGR_NMT_Inst_t* inst, 
                    uint32_t timeout_ms,
                    uint8_t node_id,
                    AGR_NMT_StateChangedCb_t on_state_changed,
                    AGR_NMT_TimeoutCb_t on_timeout,
                    void* user_ctx);

/**
 * @brief 활동 갱신 (Boot-up/Heartbeat 수신 시 호출)
 * @param inst       NMT 인스턴스
 * @param current_ms 현재 시간 (ms)
 * 
 * @details
 * [CANopen 표준 상태 전이]
 * - BOOT_UP → PRE_OPERATIONAL (Boot-up 수신 시)
 * - STOPPED → PRE_OPERATIONAL (재연결 시)
 * - PRE_OP/OPERATIONAL → 상태 유지 (Heartbeat 수신 시)
 * 
 * @note AGR_NMT_ProcessMessage()에서 내부적으로 호출됨
 */
void AGR_NMT_UpdateActivity(AGR_NMT_Inst_t* inst, uint32_t current_ms);

/**
 * @brief 타임아웃 검사 (주기적 태스크에서 호출)
 * @param inst       인스턴스 포인터
 * @param current_ms 현재 시간 (ms)
 * @details 
 * - timeout_ms 동안 활동이 없으면 STOPPED 상태로 전환
 * - on_timeout 콜백 호출 (설정된 경우)
 */
void AGR_NMT_CheckTimeout(AGR_NMT_Inst_t* inst, uint32_t current_ms);

/**
 * @brief 상태 변경 (직접 설정)
 * @param inst      인스턴스 포인터
 * @param new_state 새 상태
 * @details 상태 변경 시 on_state_changed 콜백이 호출됩니다.
 */
void AGR_NMT_SetState(AGR_NMT_Inst_t* inst, AGR_NMT_State_t new_state);

/**
 * @brief CANopen 메시지 처리 (NMT, Boot-up, Heartbeat)
 * @param inst       인스턴스 포인터
 * @param can_id     CAN ID
 * @param data       데이터 버퍼
 * @param len        데이터 길이
 * @param current_ms 현재 시간 (ms) - Heartbeat Timeout 체크용
 * @return 0: 처리 완료, -1: 해당 없음 (다른 프로토콜)
 * 
 * @details
 * CANopen 표준 메시지를 파싱하고 처리합니다:
 * - CAN ID 0x000: NMT Command (Master → Slave)
 * - CAN ID 0x700 + Node ID: Boot-up / Heartbeat
 *   - data[0] == 0x00: Boot-up
 *   - data[0] == 0x05: Heartbeat (Operational)
 * 
 * @example
 * ```c
 * // ISR에서 호출
 * void FDCAN_RxCallback(uint16_t can_id, uint8_t* data, uint8_t len) {
 *     uint32_t now = GetTick();
 *     
 *     if (AGR_NMT_ProcessMessage(&s_nmt, can_id, data, len, now) == 0) {
 *         return;  // NMT 메시지 처리 완료
 *     }
 *     
 *     // 다른 프로토콜 처리 (SDO, PDO)
 *     AGR_DOP_ProcessRxMessage(&s_dop_ctx, can_id, data, len);
 * }
 * ```
 */
int AGR_NMT_ProcessMessage(AGR_NMT_Inst_t* inst, uint16_t can_id, const uint8_t* data, uint8_t len, uint32_t current_ms);

/**
 * @brief NMT 명령 처리 (Master로부터 명령 수신 시)
 * @param inst 인스턴스 포인터
 * @param cmd  NMT 명령
 * @details CANopen NMT 명령을 처리하여 상태를 변경합니다.
 * @note AGR_NMT_ProcessMessage()에서 내부적으로 호출됩니다.
 */
void AGR_NMT_ProcessCommand(AGR_NMT_Inst_t* inst, AGR_NMT_Cmd_t cmd);

/**
 * @brief 현재 상태 조회
 * @param inst 인스턴스 포인터
 * @return 현재 NMT 상태
 */
AGR_NMT_State_t AGR_NMT_GetState(const AGR_NMT_Inst_t* inst);

/**
 * @brief 연결 여부 확인 (OPERATIONAL 상태인지)
 * @param inst 인스턴스 포인터
 * @return OPERATIONAL 상태이면 true
 */
bool AGR_NMT_IsConnected(const AGR_NMT_Inst_t* inst);

/**
 * @brief 통신 가능 여부 확인 (PRE_OPERATIONAL 또는 OPERATIONAL)
 * @param inst 인스턴스 포인터
 * @return SDO 통신이 가능한 상태이면 true
 */
bool AGR_NMT_IsReady(const AGR_NMT_Inst_t* inst);

/**
 * @brief 상태를 문자열로 변환 (디버그용)
 * @param state NMT 상태
 * @return 상태 문자열 (예: "OPERATIONAL")
 */
const char* AGR_NMT_StateToString(AGR_NMT_State_t state);

#endif /* AGR_NMT_H */

