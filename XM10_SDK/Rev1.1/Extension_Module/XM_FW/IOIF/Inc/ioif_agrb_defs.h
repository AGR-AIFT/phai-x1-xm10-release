/**
 ******************************************************************************
 * @file    ioif_agrb_defs.h
 * @author  Angel Robotics Firmware Team
 * @brief   [IOIF] AGR Common Definitions - IOIF Layer 전용
 * @version 4.0 (Common Library - Git Submodule)
 * @date    Feb 11, 2026
 *
 * @details
 * IOIF (I/O Interface) Layer의 공통 정의를 관리합니다.
 * 
 * [적용 범위]
 * - IOIF Layer 전용 (ioif_agrb_*.c, ioif_agrb_*.h)
 * - IOIF 모듈 개발자를 위한 헤더
 * 
 * [System Layer는 사용 금지]
 * - System Layer는 module.h 사용
 * - IOIF는 System 설정에 의존하지 않음 (독립성)
 * 
 * [원칙]
 * - Hardware Abstraction에 필요한 최소한의 정의만 포함
 * - MCU 정보 자동 감지
 * - RTOS 자동 감지
 * - Status 코드 정의
 * - Module Enable/Disable Guard
 *
 * [사용법]
 * - 각 Product Project에서 ioif_conf.h를 제공하여 AGRB_IOIF_XXX_ENABLE 매크로 정의
 * - IOIF Submodule이 자동으로 필요한 모듈만 빌드
 * - ioif_conf.h는 프로젝트의 include 경로에 위치 (예: System/Config/)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef IOIF_AGRB_DEFS_H_
#define IOIF_AGRB_DEFS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/**
 * [Project Configuration] - STM32 HAL의 hal_conf.h 패턴
 * 각 Product Project가 ioif_conf.h를 제공하여 사용할 IOIF 모듈을 활성화합니다.
 * 이 파일은 IOIF Submodule에 포함되지 않으며, 프로젝트 include 경로에 위치합니다.
 */
#include "ioif_conf.h"

/**
 *===========================================================================
 * EXECUTION MODE AUTO-DETECTION (SMART)
 *===========================================================================
 * 
 * IOIF는 제품 독립적이므로 module.h를 참조하지 않습니다.
 * 대신 FreeRTOS 헤더의 존재 여부로 환경을 자동 감지합니다.
 * 
 * [감지 전략]
 * 1. Compiler의 __has_include 기능으로 FreeRTOSConfig.h 존재 확인
 * 2. IOIF는 이를 감지하여 USE_FREERTOS 자동 정의
 * 3. 사용자 개입 불필요 (완전 자동 감지)
 * 
 * [Fallback]
 * - __has_include 미지원 컴파일러: FreeRTOS 매크로 감지
 */

/* --- FreeRTOS 존재 여부 확인 (Compiler Feature) --- */
#if defined(__has_include)
    #if __has_include("FreeRTOSConfig.h")
        #define IOIF_RTOS_DETECTED 1
    #else
        #define IOIF_RTOS_DETECTED 0
    #endif
#else
    /* Fallback: FreeRTOS 특징적 매크로로 간접 확인 */
    #if defined(configUSE_PREEMPTION) || defined(INCLUDE_vTaskDelay)
        #define IOIF_RTOS_DETECTED 1
    #else
        #define IOIF_RTOS_DETECTED 0
    #endif
#endif

/* --- 환경별 매크로 정의 --- */
#if IOIF_RTOS_DETECTED
    #ifndef USE_FREERTOS
        #define USE_FREERTOS  /**< RTOS 환경 (자동 감지) */
    #endif
    
    #include "FreeRTOS.h"
    #include "task.h"
    #include "semphr.h"
    
#else
    #ifndef USE_BAREMETAL
        #define USE_BAREMETAL     /**< BareMetal 환경 (자동 감지) */
    #endif
#endif

/**
 *===========================================================================
 * MCU INFORMATION (Auto-detect from Compiler Defines)
 *===========================================================================
 */

#if defined(STM32H743xx) || defined(STM32H750xx)
    #define IOIF_MCU_SERIES_H7          1
    #define IOIF_CPU_FREQ_HZ            480000000UL /**< 480 MHz (XM10, CM, MD) */
    #define IOIF_HAS_DCACHE             1           /**< D-Cache 존재, DMA 영역은 MPU Non-cacheable 설정 → D-Cache 연산 불필요 */
    
#elif defined(STM32G474xx) || defined(STM32G431xx)
    #define IOIF_MCU_SERIES_G4          1
    #define IOIF_CPU_FREQ_HZ            160000000UL /**< 160 MHz (Sensor Modules) */
    #define IOIF_HAS_DCACHE             0           /**< D-Cache 없음 (캐시 문제 없음) */
    
#else
    #warning "Unknown MCU. Using default configuration."
    #define IOIF_CPU_FREQ_HZ            100000000UL
    #define IOIF_HAS_DCACHE             0
#endif

/**
 *===========================================================================
 * MEMORY SECTION ATTRIBUTES
 *===========================================================================
 * 
 * STM32H7: 다중 RAM 영역 (DTCM, AXI, SRAM1-4, Backup SRAM)
 *   - DMA 버퍼는 MPU Non-cacheable 영역에 배치 → D-Cache Clean/Invalidate 불필요
 *   - __attribute__((section("...")))으로 변수를 해당 RAM 영역에 지정
 * STM32G4/F4: 단일 SRAM (Section/D-Cache 불필요)
 *   - DMA 변수는 일반 전역 변수로 선언, DMA 함수에 입력으로 전달
 */

#if defined(IOIF_MCU_SERIES_H7)
    /*
     * STM32H7 DMA 도메인 제약 (하드웨어 고정):
     *   DMA1/DMA2 → RAM_D2 (0x30000000, AHB1/AHB2) 만 접근 가능
     *   BDMA      → RAM_D3 (0x38000000, AHB4)       만 접근 가능
     *   MDMA      → 전체 메모리 접근 가능
     *
     * 각 풀은 반드시 해당 컨트롤러가 접근 가능한 RAM에 배치해야 함.
     * 위반 시 Bus Fault 또는 데이터 무응답 발생.
     * 프로젝트별 링커 섹션이 다를 경우 ioif_conf.h에서 override 가능.
     */
    #ifndef IOIF_DMA_SECTION
        #define IOIF_DMA_SECTION     ".RAM_D2_data"     /* DMA1/DMA2 → RAM_D2 */
    #endif
    #ifndef IOIF_BDMA_SECTION
        #define IOIF_BDMA_SECTION    ".RAM_D3_data"     /* BDMA → RAM_D3 */
    #endif
    #ifndef IOIF_MDMA_SECTION
        #define IOIF_MDMA_SECTION    ".RAM_D3_data"     /* MDMA → 제약 없음, D3 기본 */
    #endif

    /* USB/FS 전용 RAM 영역 */
    #ifndef IOIF_FS_SECTION
        #define IOIF_FS_SECTION      ".RAM_D2_data"
    #endif
    #ifndef IOIF_USB_CDC_SECTION
        #define IOIF_USB_CDC_SECTION ".RAM_D3_data"
    #endif
#else
    /*
     * STM32G4/F4: 단일 SRAM → Section 속성 불필요.
     * DMA Pool Manager(AGRB_IOIF_DMA_ENABLE)는 H7 전용이므로
     * G4/F4에서는 비활성 상태. 빈 매크로로 정의하여 컴파일 호환성 유지.
     */
    #define IOIF_DMA_SECTION
    #define IOIF_BDMA_SECTION
    #define IOIF_MDMA_SECTION
    #define IOIF_FS_SECTION
    #define IOIF_USB_CDC_SECTION
#endif

/**
 *===========================================================================
 * IOIF HARDWARE MODULES ENABLE/DISABLE
 *===========================================================================
 * 
 * [설계 철학] ENABLE 패턴 (기본 비활성)
 * - 각 Product Project의 ioif_conf.h에서 사용할 모듈만 ENABLE
 * - 새 모듈 추가 시 기존 프로젝트에 영향 없음 (안전한 기본값)
 * 
 * [설정 방법] ioif_conf.h (프로젝트별 제공)
 * #define AGRB_IOIF_FDCAN_ENABLE
 * #define AGRB_IOIF_UART_ENABLE
 * #define AGRB_IOIF_GPIO_ENABLE
 * 
 * [사용 가능 모듈]
 * AGRB_IOIF_FDCAN_ENABLE       - FDCAN (CAN FD)
 * AGRB_IOIF_UART_ENABLE        - UART (RS-232/485)
 * AGRB_IOIF_SPI_ENABLE         - SPI
 * AGRB_IOIF_I2C_ENABLE         - I2C
 * AGRB_IOIF_GPIO_ENABLE        - GPIO (General Purpose I/O)
 * AGRB_IOIF_TIM_ENABLE         - Timer (Hardware Timer)
 * AGRB_IOIF_ADC_ENABLE         - ADC (Analog-to-Digital)
 * AGRB_IOIF_DWT_ENABLE         - DWT (Performance Profiling)
 * AGRB_IOIF_USB_ENABLE         - USB (CDC, MSC)
 * AGRB_IOIF_FILESYSTEM_ENABLE  - FatFs (USB MSC, SD Card)
 * AGRB_IOIF_DMA_ENABLE         - DMA Pool Manager
 * AGRB_IOIF_SAI_ENABLE         - SAI (Audio)
 * AGRB_IOIF_PSRAM_ENABLE       - PSRAM (External SRAM)
 */

/* 
 * [IMPORTANT] ENABLE 매크로는 여기서 정의하지 않습니다!
 * 각 Product Project의 ioif_conf.h에서 정의합니다.
 * (STM32 HAL의 stm32xx_hal_conf.h 패턴과 동일)
 */

/**
 *===========================================================================
 * STATUS CODES
 *===========================================================================
 * 
 * [설계 철학]
 * - XM의 간결한 핵심 코드 + aeat의 세분화된 에러 통합
 * - 총 20개 이내로 제한
 * - AGRBStatus_ToString() 유틸리티 제공
 */

typedef enum {
    /* --- Success --- */
    AGRBStatus_OK = 0,                  /**< 성공 */
    
    /* --- General Errors --- */
    AGRBStatus_ERROR,                   /**< 일반 오류 */
    AGRBStatus_FAILED,                  /**< 실패 */
    
    /* --- Resource Errors --- */
    AGRBStatus_NO_RESOURCE,             /**< 리소스 부족 (Instance Pool Full) */
    AGRBStatus_INSTANCE_FULL,           /**< 인스턴스 풀 가득 참 */
    AGRBStatus_BUSY,                    /**< 장치 사용 중 */
    AGRBStatus_TIMEOUT,                 /**< 타임아웃 */
    
    /* --- Initialization Errors --- */
    AGRBStatus_NOT_INITIALIZED,         /**< 초기화 안 됨 */
    AGRBStatus_INITIAL_FAILED,          /**< 초기화 실패 */
    AGRBStatus_ALREADY_INITIALIZED,     /**< 이미 초기화됨 */
    
    /* --- Parameter Errors --- */
    AGRBStatus_PARAM_ERROR,             /**< 파라미터 오류 (NULL, 범위 초과) */
    AGRBStatus_NOT_ALLOWED,             /**< 허용 안 됨 */
    AGRBStatus_NOT_SUPPORTED,           /**< 지원 안 됨 */
    
    /* --- Data Errors --- */
    AGRBStatus_BUFFER_OVERFLOW,         /**< 버퍼 오버플로 */
    AGRBStatus_NOT_FOUND,               /**< 찾을 수 없음 */
    AGRBStatus_LENGTH_MISMATCH,         /**< 길이 불일치 */
    
    /* --- Integrity & Security --- */
    AGRBStatus_INTEGRITY_ERROR,         /**< 데이터 무결성 오류 */
    AGRBStatus_SECURITY_ERROR,          /**< 보안 오류 */
    
    /* --- RTOS / Synchronization --- */
    AGRBStatus_SEMAPHORE_ERROR,         /**< 세마포어 오류 (RTOS Only) */
    
    /* --- DMA --- */
    AGRBStatus_DMA_ALLOCATION_ERROR,    /**< DMA 버퍼 할당 실패 */

    /* --- State --- */
    AGRBStatus_INVALID_STATE,           /**< 현재 상태에서 허용되지 않는 동작 */
    AGRBStatus_EMPTY,                   /**< 데이터 없음 (큐/버퍼 비어있음) */

    /* --- Communication / Connection --- */
    AGRBStatus_NOT_CONNECTED,           /**< 통신 연결 안 됨 (USB CDC 등) */
    AGRBStatus_QUEUE_FULL,              /**< 메시지 큐 가득 참 */

} AGRBStatusDef;

/**
 *===========================================================================
 * HELPER MACROS
 *===========================================================================
 */

#define AGRB_IS_OK(status)              ((status) == AGRBStatus_OK)
#define AGRB_IS_ERROR(status)           ((status) != AGRBStatus_OK)

/**
 * @brief HAL Status → AGRB Status 변환 매크로
 * @details HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT을 AGRB 상태 코드로 변환
 */
#define IOIF_HAL_ERROR_TO_AGRB_STATUSDEF(hal_res) \
    ( (hal_res) == HAL_OK      ? AGRBStatus_OK      : \
      (hal_res) == HAL_ERROR   ? AGRBStatus_ERROR    : \
      (hal_res) == HAL_BUSY    ? AGRBStatus_BUSY     : \
      (hal_res) == HAL_TIMEOUT ? AGRBStatus_TIMEOUT  : \
                                 AGRBStatus_ERROR )

/**
 * @brief Status 코드를 문자열로 변환 (디버깅용)
 */
static inline const char* AGRBStatus_ToString(AGRBStatusDef status)
{
    switch (status) {
        case AGRBStatus_OK:                     return "OK";
        case AGRBStatus_ERROR:                  return "ERROR";
        case AGRBStatus_FAILED:                 return "FAILED";
        case AGRBStatus_NO_RESOURCE:            return "NO_RESOURCE";
        case AGRBStatus_INSTANCE_FULL:          return "INSTANCE_FULL";
        case AGRBStatus_BUSY:                   return "BUSY";
        case AGRBStatus_TIMEOUT:                return "TIMEOUT";
        case AGRBStatus_NOT_INITIALIZED:        return "NOT_INITIALIZED";
        case AGRBStatus_INITIAL_FAILED:         return "INITIAL_FAILED";
        case AGRBStatus_ALREADY_INITIALIZED:    return "ALREADY_INITIALIZED";
        case AGRBStatus_PARAM_ERROR:            return "PARAM_ERROR";
        case AGRBStatus_NOT_ALLOWED:            return "NOT_ALLOWED";
        case AGRBStatus_NOT_SUPPORTED:          return "NOT_SUPPORTED";
        case AGRBStatus_BUFFER_OVERFLOW:        return "BUFFER_OVERFLOW";
        case AGRBStatus_NOT_FOUND:              return "NOT_FOUND";
        case AGRBStatus_LENGTH_MISMATCH:        return "LENGTH_MISMATCH";
        case AGRBStatus_INTEGRITY_ERROR:        return "INTEGRITY_ERROR";
        case AGRBStatus_SECURITY_ERROR:         return "SECURITY_ERROR";
        case AGRBStatus_SEMAPHORE_ERROR:        return "SEMAPHORE_ERROR";
        case AGRBStatus_DMA_ALLOCATION_ERROR:   return "DMA_ALLOCATION_ERROR";
        case AGRBStatus_INVALID_STATE:          return "INVALID_STATE";
        case AGRBStatus_EMPTY:                  return "EMPTY";
        case AGRBStatus_NOT_CONNECTED:          return "NOT_CONNECTED";
        case AGRBStatus_QUEUE_FULL:             return "QUEUE_FULL";
        default:                                return "UNKNOWN";
    }
}

#endif /* IOIF_AGRB_DEFS_H_ */
