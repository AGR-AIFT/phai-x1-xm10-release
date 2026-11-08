/**
 ******************************************************************************
 * @file    xm_api_canopen.h
 * @author  HyundoKim
 * @brief   CANopen 장치 도움 함수 (Rev 2.0 · 클래식 모드 전용)
 * @details CANopen(CiA 301) 모터 드라이버 등을 XM10 이 직접 다루도록 돕는 함수입니다 — NMT 보내기 · 노드 하트비트 감시 ·
 *          SDO 빠른 읽기/쓰기(4 B 이하). 포트를 클래식 모드로 정했을 때만 동작합니다(xm_api_canbus.h 의 XM_CanBusMode).
 *          FD 모드에서는 보내기 · 등록이 XM_CAN_REJECTED / false 입니다. SYNC 는 XM_CanEnableSync(클래식 = 길이 0)를,
 *          PDO 같은 그 밖의 메시지는 XM_CanSendFrame · XM_CanReadFrame · XM_CanSubscribe 를 쓰세요.
 * @note    호출 자리 · 결과 확인 규칙은 xm_api_canbus.h 와 같습니다 — Control_Setup() · Control_Loop() 안에서만,
 *          결과는 `== XM_CAN_OK` 로. 어느 함수도 기다리지 않습니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_INC_XM_API_CANOPEN_H_
#define XM_API_INC_XM_API_CANOPEN_H_

#include <stdint.h>
#include <stdbool.h>

#include "xm_api_canbus.h"

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* NMT 명령 — 0x000 [명령, 노드 ID] 2 B */
#define XM_CANOPEN_NMT_START             (0x01U)
#define XM_CANOPEN_NMT_STOP              (0x02U)
#define XM_CANOPEN_NMT_PRE_OPERATIONAL   (0x80U)
#define XM_CANOPEN_NMT_RESET_NODE        (0x81U)
#define XM_CANOPEN_NMT_RESET_COMM        (0x82U)

#define XM_CANOPEN_MAX_WATCH_NODES       (8U)     /**< 하트비트를 감시할 수 있는 노드 수 */
#define XM_CANOPEN_SDO_TIMEOUT_MS        (500U)   /**< SDO 응답을 기다리는 시간 — 이 API 의 정책입니다(모든 장치의 응답 시간 보장이 아님) */
#define XM_CANOPEN_SDO_QUARANTINE_MS     (1500U)  /**< 시간 초과 뒤 새 SDO 요청을 받지 않는 시간(아래 SDO 설명) */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 감시 노드 상태 */
typedef enum {
    XM_CANOPEN_NODE_UNKNOWN = 0,     /**< 아직 하트비트를 못 받음 */
    XM_CANOPEN_NODE_BOOTUP,          /**< 0x00 — 방금 켜짐(부팅 알림) */
    XM_CANOPEN_NODE_STOPPED,         /**< 0x04 */
    XM_CANOPEN_NODE_OPERATIONAL,     /**< 0x05 */
    XM_CANOPEN_NODE_PRE_OPERATIONAL, /**< 0x7F */
    XM_CANOPEN_NODE_LOST,            /**< timeout_ms 동안 못 받음 — 다음 하트비트까지 유지 */
    XM_CANOPEN_NODE_NOT_WATCHED,     /**< 감시 등록 안 함 · FD 모드 · 다른 태스크에서 부름 */
} XmCanOpenNodeState_t;

/** @brief SDO 진행 상태 */
typedef enum {
    XM_CANOPEN_SDO_IDLE = 0,         /**< 진행 중인 요청 없음 */
    XM_CANOPEN_SDO_BUSY,             /**< 응답을 기다리는 중 */
    XM_CANOPEN_SDO_DONE,             /**< 끝남 — 읽기면 value · size 에 값 */
    XM_CANOPEN_SDO_ABORTED,          /**< 장치가 중단(SDO abort)을 보냄 — abort_code 에 그 코드 */
    XM_CANOPEN_SDO_TIMEOUT,          /**< XM_CANOPEN_SDO_TIMEOUT_MS 안에 응답이 없음 */
    XM_CANOPEN_SDO_UNSUPPORTED,      /**< 장치가 분할 전송(4 B 초과 등)으로 답함 — 이 API 는 지원하지 않습니다 */
} XmCanOpenSdoStatus_t;

/** @brief 끝난 SDO 요청의 결과 */
typedef struct {
    uint8_t  node_id;
    uint16_t index;
    uint8_t  subindex;
    uint32_t value;                  /**< 읽기 결과(작은 쪽 바이트부터) */
    uint8_t  size;                   /**< 읽은 바이트 수 1~4 — 장치가 크기를 알려 주지 않으면 4 */
    uint32_t abort_code;             /**< ABORTED 일 때 장치가 보낸 중단 코드 */
    bool     abort_queued;           /**< TIMEOUT · UNSUPPORTED 때 XM10 이 보낸 중단 프레임이 실렸는가(한 번만 시도) */
} XmCanOpenSdoResult_t;

/** @brief CANopen 도움 함수 통계 — 각 값은 온전하지만 서로 같은 순간의 값은 아닙니다 */
typedef struct {
    uint32_t sdo_late_response_count;    /**< 시간 초과 뒤 늦게 와서 버린 응답 + 기다리던 요청과 맞지 않는 응답(종류가 다르거나 길이가 모자람) */
    uint32_t sdo_rx_busy_drop_count;     /**< 응답을 넣는 중 겹쳐 놓친 수(대개 TIMEOUT 으로 끝남) */
    uint32_t sdo_clear_hold_count;       /**< 시간 초과 뒤 송신 대기를 비우려고 새 송신을 막은 tick 수 */
    uint32_t sdo_last_clear_wait_ticks;  /**< 마지막 시간 초과에서 송신 대기가 비기까지 걸린 tick 수 */
} XmCanOpenStats_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief NMT 명령을 보냅니다 — 0x000 [command, node_id].
 * @param command XM_CANOPEN_NMT_* 다섯 값 중 하나(그 밖은 XM_CAN_REJECTED)
 * @param node_id 0 = 모든 노드, 1~127
 */
XmCanResult_t XM_CanOpenSendNmt(uint8_t command, uint8_t node_id);

/**
 * @brief 노드의 하트비트(0x700 + 노드 ID)를 감시합니다 — Control_Setup() 안에서만, 최대 8 노드.
 * @param timeout_ms 이만큼 하트비트가 없으면 LOST — 장치가 하트비트를 주기적으로 내도록 설정돼 있어야 뜻이 있습니다
 *                   (장치의 하트비트 송신 주기는 장치 설명서 · SDO 0x1017 로)
 */
bool XM_CanOpenWatchNode(uint8_t node_id, uint16_t timeout_ms);

/** @brief 감시 노드 상태 — LOST 는 XM10 의 판단이지 장치가 보낸 값이 아닙니다 */
XmCanOpenNodeState_t XM_CanOpenGetNodeState(uint8_t node_id);

/**
 * @brief 감시 노드의 부팅 알림(0x00)을 받은 수 — 장치가 켜지거나 재설정될 때마다 늘어납니다.
 * @note  받은 알림의 수일 뿐 특정 NMT 요청의 응답이 아닙니다. 장치가 있는지 확인하는 데 쓰세요. 감시 안 함이면 0.
 */
uint32_t XM_CanOpenGetBootupCount(uint8_t node_id);

/**
 * @brief SDO 빠른 읽기를 시작합니다 — 기다리지 않습니다. Control_Loop 에서 XM_CanOpenSdoGetResult 로 결과를 보세요.
 * @details 한 번에 한 요청입니다(모든 노드 공통). 진행 중 · 결과를 아직 안 가져감 · 아래 쉬는 시간이면 XM_CAN_BUSY.
 *          이 노드의 응답 ID(0x580 + 노드 ID)를 XM_CanSubscribe 로 등록했으면 XM_CAN_REJECTED 입니다.
 * @note  시간 초과(TIMEOUT) · 분할 전송(UNSUPPORTED)으로 끝나면 XM10 은 중단 프레임을 한 번 보내고, 그 요청이 송신 대기를
 *        떠난 것을 확인한 뒤 1.5 초 동안 새 SDO 요청을 받지 않습니다(XM_CAN_BUSY). 그사이 그 노드의 늦은 응답은 버립니다 —
 *        다음 요청의 결과로 잘못 읽지 않기 위해서입니다. 이 약속은 장치가 요청을 받고 1.4 초 안에 응답을 내보낼 때만
 *        성립합니다(장치 설명서로 확인하세요). 송신 대기가 빌 때까지는 이 포트의 새 송신이 잠깐 XM_CAN_BUSY 이고 SYNC 가
 *        빠질 수 있습니다(가벼운 부하면 수 ms, 길어도 연속 20 ms 뒤 80 ms 는 정상). 다른 장치가 버스를 쉬지 않고 쓰면
 *        다음 SDO 요청이 오래 BUSY 일 수 있습니다 — 버스 부하를 줄이면 풀립니다. 장치가 중단(ABORTED)을 보낸 경우와
 *        정상(DONE)에는 쉬는 시간이 없습니다.
 */
XmCanResult_t XM_CanOpenSdoRead(uint8_t node_id, uint16_t index, uint8_t subindex);

/** @brief SDO 빠른 쓰기를 시작합니다(size 1~4 B, 작은 쪽 바이트부터) — 규칙은 XM_CanOpenSdoRead 와 같습니다 */
XmCanResult_t XM_CanOpenSdoWrite(uint8_t node_id, uint16_t index, uint8_t subindex, uint32_t value, uint8_t size);

/**
 * @brief SDO 결과를 봅니다 — 끝난 결과(DONE · ABORTED · TIMEOUT · UNSUPPORTED)는 한 번 돌려준 뒤 IDLE 이 됩니다.
 * @return IDLE · BUSY 면 out 은 그대로입니다. FD 모드 · 다른 태스크에서 부르면 IDLE(out 그대로).
 */
XmCanOpenSdoStatus_t XM_CanOpenSdoGetResult(XmCanOpenSdoResult_t* out);

/** @brief CANopen 도움 함수 통계 — out 이 NULL 이면 false */
bool XM_CanOpenGetStats(XmCanOpenStats_t* out);

#endif /* XM_API_INC_XM_API_CANOPEN_H_ */
