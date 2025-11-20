#pragma once // 현대 컴파일러를 위한 최적화

#ifndef SYSTEM_LINKS_CONTROL_MODULE_INC_CM_XM_LINK_H_
#define SYSTEM_LINKS_CONTROL_MODULE_INC_CM_XM_LINK_H_

#include "link_interface.h"

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 *            PUBLIC DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 */
/**
 *-----------------------------------------------------------
 *                       PUBLIC TYPES
 *-----------------------------------------------------------
 */

// NMT(Network Management) 상태 정의
typedef enum {
    NMT_STATE_INITIALISING = 0,
    NMT_STATE_PRE_OPERATIONAL,
    NMT_STATE_OPERATIONAL,
    NMT_STATE_STOPPED
} LinkNmtState_t;

/**
 *------------------------------------------------------------
 *                   PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief CM-XM 링크 관리자 모듈을 초기화합니다.
 * @details BSP 계층의 메인 초기화 함수(BSP_Init)에서 호출됩니다.
 * 하위 계층인 cm_api 모듈에 콜백을 등록하고 의존성을 주입합니다.
 */
void CM_XM_Link_Init(void);

/**
 * @brief CM-XM 링크의 주기적 상태 관리를 수행합니다.
 * @details PnP 매니저 태스크에 의해 10ms 주기로 호출됩니다.
 * Heartbeat 전송, 타임아웃 검사 등 CM과의 연결 상태를 관리하는
 * 메인 상태 머신 로직을 포함합니다.
 */
void CM_XM_Link_RunPeriodic(void);

/**
 * @brief CM-XM 링크 모듈의 LinkModule_t 인터페이스를 반환합니다.
 * @details PnP 매니저가 이 모듈을 등록하고 제어할 수 있도록,
 * 자신의 정보와 함수 포인터들이 담긴 구조체를 외부에 제공합니다.
 * @return 이 모듈의 LinkModule_t 구조체 포인터.
 */
LinkModule_t* CM_XM_Link_GetModule(void);

/**
 * @brief 이 링크가 담당하는 CAN 메시지를 처리합니다.
 * @details CAN 라우터에서 CM으로부터 온 메시지일 경우 호출됩니다.
 * 수신된 메시지를 하위 cm_api 모듈로 전달하는 역할을 합니다.
 * @param[in] canId   수신된 메시지의 CAN ID.
 * @param[in] data    수신된 데이터 버퍼 포인터.
 * @param[in] len     수신된 데이터의 길이.
 */
void CM_XM_Link_ProcessMessage(uint16_t canId, uint8_t* data, uint8_t len);

/**
 * @brief CM과의 PnP 통신 연결 상태를 반환합니다.
 * @return 연결이 OPERATIONAL 상태이면 `true`, 그렇지 않으면 `false`.
 */
bool CM_XM_Link_IsConnected(void);

/**
 * @brief CM과의 PnP NMT 상태 변수를 반환합니다.
 * @return 연결이 OPERATIONAL 상태이면 `2`, NMT_STATE_PRE_OPERATIONAL이면 `1`.
 * @param[out] LinkNmtState_t XM의 NMT(Network Management) 상태.
 */
LinkNmtState_t CM_XM_Link_GetXMNmtState(void);

#endif /* SYSTEM_LINKS_CONTROL_MODULE_INC_CM_XM_LINK_H_ */
