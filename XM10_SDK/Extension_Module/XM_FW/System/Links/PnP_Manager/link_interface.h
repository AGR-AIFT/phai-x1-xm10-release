/**
 ******************************************************************************
 * @file    link_interface.h
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Oct 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_LINKS_PNP_MNGR_INC_LINK_INTERFACE_H_
#define SYSTEM_LINKS_PNP_MNGR_INC_LINK_INTERFACE_H_

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 모든 링크(Link) 모듈이 구현해야 하는 표준 인터페이스 구조체.
 * @details 이 구조체는 PnP 매니저가 각기 다른 링크 모듈(CM, EMG, IMU 등)을
 * 동일한 방식으로 관리할 수 있도록 하는 '설계도' 또는 '규격(Contract)' 역할을 합니다.
 * 각 링크 모듈은 자신의 기능에 맞는 함수들을 이 구조체의 함수 포인터에 등록해야 합니다.
 */
typedef struct {
    /**
     * @brief 이 링크 모듈이 담당하는 상대방 장치의 CAN Node ID.
     * @details PnP 매니저는 수신된 CAN 메시지의 발신자 ID와 이 값을 비교하여
     * 메시지를 처리할 올바른 모듈을 찾습니다.
     */
    uint8_t      nodeId;

    /**
     * @brief 링크 모듈의 초기화 함수 포인터.
     * @details 시스템 시작 시 PnP 매니저에 의해 단 한 번 호출됩니다.
     * 주로 하위 계층(cm_drv 등)의 콜백을 등록하고 내부 상태 변수를 초기화합니다.
     */
    void       (*Init)(void);

    /**
     * @brief 링크 모듈의 주기적 실행 함수 포인터.
     * @details PnP 매니저 태스크에 의해 주기적으로 (예: 10ms) 호출됩니다.
     * Heartbeat 전송, 타임아웃 검사 등 주기적인 상태 관리를 수행합니다.
     * @param currentTime 현재 시스템 시간 (ms). GetTick() 값입니다.
     */
    void       (*RunPeriodic)(void);

    /**
     * @brief CAN 메시지 처리 함수 포인터.
     * @details CAN 라우터가 메시지를 수신했을 때, PnP 매니저를 통해 호출됩니다.
     * 이 모듈이 담당하는 nodeId로부터 온 메시지만 전달됩니다.
     * @param canId 수신된 메시지의 전체 CAN ID.
     * @param data  수신된 데이터 버퍼 포인터.
     * @param len   수신된 데이터의 길이.
     */
    void       (*ProcessMessage)(uint16_t canId, uint8_t* data, uint8_t len);

    /**
     * @brief 링크의 최종 연결 상태를 반환하는 함수 포인터.
     * @return PnP 절차가 모두 완료되어 데이터 통신이 가능한 'Operational' 상태이면서
     * PDO data가 전송되기 시작한 순간부터 true, 아니면 false.
     */
    bool       (*IsConnected)(void);

} LinkModule_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


#endif /* SYSTEM_LINKS_PNP_MNGR_INC_LINK_INTERFACE_H_ */
