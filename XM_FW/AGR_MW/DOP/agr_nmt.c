/**
 ******************************************************************************
 * @file    agr_nmt.c
 * @author  HyundoKim
 * @brief   AGR Network Management - 통신 무관 연결 상태 관리
 * @version 1.0
 * @date    Dec 2, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_nmt.h"
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

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

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _NotifyStateChange(AGR_NMT_Inst_t* inst, 
                               AGR_NMT_State_t old_state, 
                               AGR_NMT_State_t new_state);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void AGR_NMT_Init(AGR_NMT_Inst_t* inst, uint32_t timeout_ms)
{
    if (inst == NULL) {
        return;
    }
    
    memset(inst, 0, sizeof(AGR_NMT_Inst_t));
    inst->state = AGR_NMT_BOOT_UP;
    inst->timeout_ms = timeout_ms;
    inst->last_activity_ms = 0;
    inst->node_id = 0;
    inst->user_ctx = NULL;
    inst->on_state_changed = NULL;
    inst->on_timeout = NULL;
}

void AGR_NMT_InitEx(AGR_NMT_Inst_t* inst, 
                    uint32_t timeout_ms,
                    uint8_t node_id,
                    AGR_NMT_StateChangedCb_t on_state_changed,
                    AGR_NMT_TimeoutCb_t on_timeout,
                    void* user_ctx)
{
    if (inst == NULL) {
        return;
    }
    
    AGR_NMT_Init(inst, timeout_ms);
    inst->node_id = node_id;
    inst->on_state_changed = on_state_changed;
    inst->on_timeout = on_timeout;
    inst->user_ctx = user_ctx;
}


void AGR_NMT_UpdateActivity(AGR_NMT_Inst_t* inst, uint32_t current_ms)
{
    if (inst == NULL) {
        return;
    }
    
    AGR_NMT_State_t old_state = inst->state;
    
    /* ===== Activity 갱신 ===== */
    inst->last_activity_ms = current_ms;
    
    /* ===== CANopen 표준 상태 전이 ===== */
    switch (old_state) {
        case AGR_NMT_BOOT_UP:
            /* Boot-up 수신 → PRE_OPERATIONAL 진입 */
            /* CANopen 표준: 설정 단계를 거쳐야 함 (SDO로 PDO Mapping 등) */
            inst->state = AGR_NMT_PRE_OPERATIONAL;
            _NotifyStateChange(inst, old_state, inst->state);
            break;
            
        case AGR_NMT_STOPPED:
            /* 재연결 (Timeout 후 Heartbeat 수신) → PRE_OPERATIONAL로 복귀 */
            /* CANopen 표준: 재연결 시 설정 단계부터 시작 */
            inst->state = AGR_NMT_BOOT_UP;
            _NotifyStateChange(inst, old_state, inst->state);
            break;
            
        case AGR_NMT_PRE_OPERATIONAL:
        case AGR_NMT_OPERATIONAL:
            /* Heartbeat 수신 → 상태 유지 (활동 시간만 갱신) */
            break;
    }
}

void AGR_NMT_CheckTimeout(AGR_NMT_Inst_t* inst, uint32_t current_ms)
{
    if (inst == NULL) {
        return;
    }
    
    /* 타임아웃 비활성화된 경우 */
    if (inst->timeout_ms == 0) {
        return;
    }
    
    /* BOOT_UP 상태에서는 타임아웃 검사 안 함 (아직 연결 전) */
    if (inst->state == AGR_NMT_BOOT_UP) {
        return;
    }
    
    /* 이미 STOPPED 상태면 검사 안 함 */
    if (inst->state == AGR_NMT_STOPPED) {
        return;
    }
    
    /* 타임아웃 검사 */
    uint32_t elapsed = current_ms - inst->last_activity_ms;
    if (elapsed > inst->timeout_ms) {
        AGR_NMT_State_t old_state = inst->state;
        inst->state = AGR_NMT_STOPPED;
        
        /* 타임아웃 콜백 */
        if (inst->on_timeout != NULL) {
            inst->on_timeout(inst->user_ctx);
        }
        
        /* 상태 변경 콜백 */
        _NotifyStateChange(inst, old_state, inst->state);
    }
}

void AGR_NMT_SetState(AGR_NMT_Inst_t* inst, AGR_NMT_State_t new_state)
{
    if (inst == NULL) {
        return;
    }
    
    AGR_NMT_State_t old_state = inst->state;
    
    /* 
     * [수정] 상태를 항상 업데이트 (old == new이어도)
     * 
     * 이유:
     * - SetState는 강제 설정 함수이므로 항상 반영되어야 함
     * - 콜백은 실제 변경이 있을 때만 호출 (중복 호출 방지)
     */
    inst->state = new_state;
    
    if (old_state != new_state) {
        _NotifyStateChange(inst, old_state, new_state);
    }
}

int AGR_NMT_ProcessMessage(AGR_NMT_Inst_t* inst, uint16_t can_id, const uint8_t* data, uint8_t len, uint32_t current_ms)
{
    if (inst == NULL || data == NULL || len == 0) {
        return -1;
    }
    
    /* ===== NMT Command (CAN ID 0x000) ===== */
    if (can_id == 0x000 && len >= 2) {
        uint8_t cmd_byte = data[0];
        uint8_t target_node_id = data[1];
        
        /* 브로드캐스트(0) 또는 자신의 Node ID */
        if (target_node_id == 0 || target_node_id == inst->node_id) {
            AGR_NMT_Cmd_t cmd = (AGR_NMT_Cmd_t)cmd_byte;
            AGR_NMT_ProcessCommand(inst, cmd);
            AGR_NMT_UpdateActivity(inst, current_ms);  /* Activity 갱신 */
            return 0;  /* 처리 완료 */
        }
    }
    
    /* ===== Boot-up / Heartbeat (CAN ID 0x700 + Node ID) ===== */
    uint16_t fnc_code = can_id & 0x780;  /* Function Code 추출 */
    if (fnc_code == 0x700 && len >= 1) {
        uint8_t nmt_state_byte = data[0];
        
        /* Boot-up (0x00) */
        if (nmt_state_byte == 0x00) {
            /* ✅ Boot-up 감지: 상태를 BOOT_UP으로 리셋 */
            /* 
             * [중요] Boot-up은 "재시작" 신호이므로 현재 상태와 무관하게 
             * BOOT_UP으로 리셋한 후 UpdateActivity에서 전이 처리
             */
            AGR_NMT_State_t old_state = inst->state;
            inst->state = AGR_NMT_BOOT_UP;
            AGR_NMT_UpdateActivity(inst, current_ms);  /* Activity 갱신 + 상태 전이 */
            
            /* 상태가 변경되었다면 콜백 호출 */
            if (old_state != AGR_NMT_BOOT_UP) {
                _NotifyStateChange(inst, old_state, AGR_NMT_BOOT_UP);
            }
            return 0;  /* 처리 완료 */
        }
        
        /* Heartbeat (0x04: STOPPED, 0x05: OPERATIONAL, 0x7F: PRE_OPERATIONAL) */
        else if (nmt_state_byte == AGR_NMT_STOPPED || 
                 nmt_state_byte == AGR_NMT_OPERATIONAL || 
                 nmt_state_byte == AGR_NMT_PRE_OPERATIONAL) {
            AGR_NMT_State_t old_state = inst->state;
            AGR_NMT_State_t new_state = (AGR_NMT_State_t)nmt_state_byte;
            
            inst->state = new_state;
            AGR_NMT_UpdateActivity(inst, current_ms);  /* Activity 갱신 */
            
            /* ✅ Heartbeat 수신 시 항상 콜백 호출 (상태 변경 여부 무관) */
            /* 이유: Slave가 Master Heartbeat를 받을 때 시간 갱신 필요 */
            _NotifyStateChange(inst, old_state, new_state);
            
            return 0;  /* 처리 완료 */
        }
    }
    
    return -1;  /* 해당 없음 (다른 프로토콜) */
}

void AGR_NMT_ProcessCommand(AGR_NMT_Inst_t* inst, AGR_NMT_Cmd_t cmd)
{
    if (inst == NULL) {
        return;
    }
    
    AGR_NMT_State_t old_state = inst->state;
    AGR_NMT_State_t new_state = old_state;
    
    switch (cmd) {
        case AGR_NMT_CMD_START:
            /* PRE_OPERATIONAL 또는 STOPPED에서 OPERATIONAL로 전환 */
            /* 재연결 시나리오 지원 */
            if (old_state == AGR_NMT_PRE_OPERATIONAL || old_state == AGR_NMT_STOPPED) {
                new_state = AGR_NMT_OPERATIONAL;
            }
            break;
            
        case AGR_NMT_CMD_STOP:
            /* 어떤 상태에서든 STOPPED로 전환 가능 */
            new_state = AGR_NMT_STOPPED;
            break;
            
        case AGR_NMT_CMD_PRE_OP:
            /* CiA 301: 어떤 상태에서든 PRE_OPERATIONAL로 전환 가능 */
            if (old_state != AGR_NMT_PRE_OPERATIONAL) {
                new_state = AGR_NMT_PRE_OPERATIONAL;
            }
            break;
            
        case AGR_NMT_CMD_RESET_NODE:
        case AGR_NMT_CMD_RESET_COMM:
            /* 리셋 → BOOT_UP으로 전환 (재초기화 필요) */
            new_state = AGR_NMT_BOOT_UP;
            break;
    }
    
    if (old_state != new_state) {
        inst->state = new_state;
        _NotifyStateChange(inst, old_state, new_state);
    }
}

AGR_NMT_State_t AGR_NMT_GetState(const AGR_NMT_Inst_t* inst)
{
    if (inst == NULL) {
        return AGR_NMT_STOPPED;
    }
    return inst->state;
}

bool AGR_NMT_IsConnected(const AGR_NMT_Inst_t* inst)
{
    if (inst == NULL) {
        return false;
    }
    return (inst->state == AGR_NMT_OPERATIONAL);
}

bool AGR_NMT_IsReady(const AGR_NMT_Inst_t* inst)
{
    if (inst == NULL) {
        return false;
    }
    return (inst->state == AGR_NMT_PRE_OPERATIONAL || 
            inst->state == AGR_NMT_OPERATIONAL);
}

const char* AGR_NMT_StateToString(AGR_NMT_State_t state)
{
    switch (state) {
        case AGR_NMT_BOOT_UP:         return "BOOT_UP";
        case AGR_NMT_STOPPED:         return "STOPPED";
        case AGR_NMT_OPERATIONAL:     return "OPERATIONAL";
        case AGR_NMT_PRE_OPERATIONAL: return "PRE_OPERATIONAL";
        default:                      return "UNKNOWN";
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 상태 변경 콜백 호출
 */
static void _NotifyStateChange(AGR_NMT_Inst_t* inst, 
                               AGR_NMT_State_t old_state, 
                               AGR_NMT_State_t new_state)
{
    if (inst->on_state_changed != NULL) {
        inst->on_state_changed(old_state, new_state, inst->user_ctx);
    }
}

