/**
 ******************************************************************************
 * @file    core_process.h
 * @author  HyundoKim
 * @brief   XM10 Main Control Loop Engine (IPO Model Enforcer)
 * @details 
 * 이 모듈은 User Task의 실행 환경을 구축하고 구동하는 '숨겨진 엔진'입니다.
 * * [역할]
 * 1. 실시간성 보장: 2ms 주기의 정밀한 타이밍 제어
 * 2. 데이터 동기화: 하위 드라이버(Link)와 상위 API(Facade) 간의 데이터 매핑
 * 3. IPO 모델 강제: Input Gathering -> User Process -> Output Flushing 순서 보장
 * * @note    이 파일은 System Layer의 Core에 위치하지만, 
 * 논리적으로는 XM API 인터페이스의 'Backend Implementation'입니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef SYSTEM_CORE_CORE_PROCESS_H_
#define SYSTEM_CORE_CORE_PROCESS_H_

#ifdef __cplusplus
extern "C" {
#endif

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
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief  User Task의 진입점 (RTOS Task Function)
 * @details 
 * 1. User_Setup()을 1회 호출하여 사용자 초기화를 수행합니다.
 * 2. 무한 루프에 진입하여 다음 과정을 2ms마다 반복합니다.
 * - [Input]  모든 센서/모듈 데이터 최신화 (_FetchAllInputs)
 * - [Logic]  User_Loop() 호출 (사용자 알고리즘)
 * - [Output] 제어 명령 물리적 전송 (_FlushAllOutputs)
 * * @param argument RTOS 태스크 생성 시 전달되는 인자 (사용 안 함)
 */
void StartUserTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CORE_CORE_PROCESS_H_ */
