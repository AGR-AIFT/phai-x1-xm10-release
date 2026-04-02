/**
 ******************************************************************************
 * @file    xm_api_memory.h
 * @author  HyundoKim
 * @brief   XM10 메모리 접근 API
 * @details
 * 사용자 코드(XM_Apps)에서 다양한 메모리 영역에 접근하기 위한 API입니다.
 *
 * 메모리 영역별 특성:
 *   - RAM_D1 Workspace : 범용 (Cacheable, 고속)
 *   - PSRAM User       : 대용량 (Write-Through, 중속 ~30MB/s)
 *   - DTCM User        : 실시간 제어 변수 (Zero-Wait-State, DMA 불가)
 *   - Flash User NV    : 비휘발성 저장 (전원 차단 시 보존, 쓰기 느림)
 *
 * @version 1.0
 * @date    2026-03-02
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_MEMORY_H_
#define XM_API_MEMORY_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *-----------------------------------------------------------
 * SECTION PLACEMENT MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief 함수를 ITCMRAM에 배치하여 Zero-Wait-State로 실행합니다.
 * @note  Flash 대비 분기 예측 실패 시 지연이 없어 ISR, 제어 루프에 적합합니다.
 *        Startup 코드가 Flash → ITCM으로 자동 복사합니다.
 *
 * @code
 * XM_RAMFUNC void MyControlLoop(void) {
 *     // 이 함수는 ITCMRAM에서 실행됨 (1-cycle fetch @ 480MHz)
 * }
 * @endcode
 */
#define XM_RAMFUNC    __attribute__((section(".itcm_text")))

/**
 * @brief 변수를 DTCMRAM에 배치하여 Zero-Wait-State로 접근합니다.
 * @note  D-Cache를 경유하지 않아 결정론적 접근 보장. DMA 접근 불가.
 *        제어 알고리즘의 상태 변수, Lookup Table에 적합합니다.
 *
 * @code
 * XM_DTCM_VAR static float s_pid_state[6];
 * XM_DTCM_VAR static float s_lookup_table[256];
 * @endcode
 */
#define XM_DTCM_VAR  __attribute__((section(".dtcm_data")))

/**
 *-----------------------------------------------------------
 * RAM_D1 USER WORKSPACE
 *-----------------------------------------------------------
 */

/**
 * @brief RAM_D1 사용자 워크스페이스 포인터를 반환합니다.
 * @return 연속 메모리 블록의 시작 주소
 * @note Cacheable (D-Cache 경유), 전원 차단 시 소멸.
 *       알고리즘 변수, 센서 데이터 배열, 연산 버퍼 등 범용 목적.
 */
void*    XM_GetUserWorkspace(void);

/** @brief 사용자 워크스페이스 크기 (바이트) */
uint32_t XM_GetUserWorkspaceSize(void);

/**
 *-----------------------------------------------------------
 * PSRAM USER AREA
 *-----------------------------------------------------------
 */

/**
 * @brief PSRAM 사용자 영역 포인터를 반환합니다.
 * @return 연속 메모리 블록의 시작 주소 (0x90700000)
 * @note Write-Through Cacheable, 전원 차단 시 소멸.
 *       QSPI Memory-Mapped 초기화 후 사용 가능.
 *       AI/ML 모델 가중치, 대용량 Lookup Table, 장기 데이터 저장에 적합.
 * @warning PSRAM 초기화 전 접근 시 HardFault 발생.
 *          System Startup에서 자동 초기화되므로 User_Setup() 이후 안전.
 */
void*    XM_GetUserPSRAM(void);

/** @brief PSRAM 사용자 영역 크기 (바이트) */
uint32_t XM_GetUserPSRAMSize(void);

/**
 *-----------------------------------------------------------
 * DTCM USER AREA
 *-----------------------------------------------------------
 */

/**
 * @brief DTCM 사용자 변수 영역 포인터를 반환합니다.
 * @return 연속 메모리 블록의 시작 주소
 * @note Zero-Wait-State @ 480MHz, D-Cache 미경유, DMA 접근 불가.
 *       개별 변수는 XM_DTCM_VAR 매크로로 배치하는 것이 더 편리합니다.
 *       이 API는 큰 연속 블록이 필요한 경우(LUT 등)에 사용하세요.
 */
void*    XM_GetUserDTCM(void);

/** @brief DTCM 사용자 영역 크기 (바이트) */
uint32_t XM_GetUserDTCMSize(void);

/**
 *-----------------------------------------------------------
 * FLASH USER NV (Non-Volatile Storage)
 *-----------------------------------------------------------
 */

/** @brief User NV 영역 크기 (바이트) — Flash Bank 2, Sector 7 (128KB) */
uint32_t XM_UserNV_GetSize(void);

/**
 * @brief Flash User NV에서 데이터를 읽습니다.
 * @param offset  User NV 시작부터의 바이트 오프셋 (0 ~ Size-1)
 * @param data    읽은 데이터를 저장할 버퍼
 * @param size    읽을 바이트 수
 * @retval  0  성공
 * @retval -1  파라미터 오류 (offset + size > NV 크기)
 */
int32_t XM_UserNV_Read(uint32_t offset, void *data, uint32_t size);

/**
 * @brief Flash User NV에 데이터를 기록합니다.
 * @param offset  User NV 시작부터의 바이트 오프셋 (32-byte 정렬 권장)
 * @param data    기록할 데이터 포인터
 * @param size    기록할 바이트 수
 * @retval  0  성공
 * @retval -1  파라미터 오류
 * @retval -2  Flash 프로그래밍 실패
 *
 * @note Flash는 Erase 후에만 Write 가능 (1→0만 가능, 0→1 불가).
 *       새 데이터를 쓰려면 먼저 XM_UserNV_Erase()를 호출하세요.
 *       STM32H7 Flash는 32바이트(256-bit) 단위로 기록됩니다.
 *       offset이 32-byte 정렬되지 않으면 자동 정렬 후 기록합니다.
 *
 * @warning 빈번한 Write는 Flash 수명에 영향 (최소 10,000 erase 사이클).
 *          부팅 시 1회 읽기, 종료/설정변경 시 1회 쓰기를 권장합니다.
 */
int32_t XM_UserNV_Write(uint32_t offset, const void *data, uint32_t size);

/**
 * @brief Flash User NV 전체 영역을 지웁니다 (0xFF로 초기화).
 * @retval  0  성공
 * @retval -2  Flash Erase 실패
 *
 * @note 128KB 전체 섹터가 지워집니다. 약 1~2초 소요.
 *       실시간 제어 루프에서 호출하지 마세요.
 */
int32_t XM_UserNV_Erase(void);

/**
 * @brief Flash User NV 영역이 비어있는지(Erased) 확인합니다.
 * @retval true   전체 영역이 0xFF (Erase 직후 상태)
 * @retval false  데이터가 존재하거나 부분적으로 기록됨
 */
bool XM_UserNV_IsErased(void);

#ifdef __cplusplus
}
#endif

#endif /* XM_API_MEMORY_H_ */
