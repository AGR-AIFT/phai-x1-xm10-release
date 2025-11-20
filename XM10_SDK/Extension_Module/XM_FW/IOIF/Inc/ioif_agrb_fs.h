/**
 ******************************************************************************
 * @file    ioif_agrb_fs.h
 * @author  HyundoKim
 * @brief   [IOIF Layer] 범용 FATFS 추상화 드라이버 (USB Host & SD Card)
 * @details
 * 이 파일은 원본 ioif_agrb_fs.c의 의도를 보존하며 리팩토링되었습니다:
 * 1. 원본의 USB/SD 멀티 스토리지, 파일 핸들 풀, API 추상화 의도를 보존합니다.
 * 2. [FIX] FATFS API 호출부를 Mutex로 보호하여 스레드 안전성(Thread-Safe)을 확보합니다.
 * 3. [ADD] data_logger.c의 세션/롤링/용량 확인 기능을 지원하기 위해
 * mkdir, get_free_space_mb, get_size_mb 함수를 추가합니다.
 * 4. [FIX] usb_host.c와의 빌드 충돌을 피하기 위해,
 * ioif_filesystem_HandleHostEvent를 제공하여 의존성을 역전시킵니다.
 * @version 0.1
 * @date    Nov 10, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_defs.h"
#if defined(AGRB_IOIF_FILESYSTEM_ENABLE)

#pragma once

#ifndef IOIF_INC_IOIF_AGRB_FS_H_
#define IOIF_INC_IOIF_AGRB_FS_H_

#include "ff.h" // FRESULT 타입을 위해

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
 * @brief [유지] 파일 열기 접근 모드 (읽기/쓰기/이어쓰기)
 */
typedef enum {
    IOIF_FileSystem_AccessMode_READONLY = 1,
    IOIF_FileSystem_AccessMode_WRITE,
    IOIF_FileSystem_AccessMode_APPEND,
} IOIF_FileSystem_AccessMode_e;

/**
 * @brief [유지] 파일 쓰기 시 생성 모드 (덮어쓰기/생성/증가)
 */
typedef enum {
    IOIF_FileSystem_CreateMode_EXCLUSIVE, // 파일이 없으면 생성 (FA_CREATE_NEW)
    IOIF_FileSystem_CreateMode_APPEND,    // 파일 끝에 이어쓰기 (FA_OPEN_APPEND)
    IOIF_FileSystem_CreateMode_OVERWRITE, // 항상 덮어쓰기 (FA_CREATE_ALWAYS)
    IOIF_FileSystem_CreateMode_INCREMENT, // data(1).bin, data(2).bin 자동 증가
} IOIF_FileSystem_CreateMode_e;

/**
 * @brief [유지] 초기화 시 선택할 스토리지 드라이버 타입
 */
typedef enum {
    IOIF_FileSystem_DeviceType_SDCard,      // SD 카드
    IOIF_FileSystem_DeviceType_USBH_MSC,    // USB Host Mass Storage Class
} IOIF_FileSystem_DeviceType_e;

/**
 * @brief [수정] AGRB 표준 상태 반환 코드 (레거시 스타일 기준 + 신규 코드 병합)
 * @details ioif_agrb_fs_legacy.h의 IOIF_FileSystem_Error_e를 기반으로,
 * 리팩토링에 필요한 신규 상태(NOT_READY, FS_ERROR 등)를 통합합니다.
 */
typedef enum {
    IOIF_FileSystem_OK = 0,                     // 성공
    IOIF_FileSystem_ERROR,                      // 일반 오류 (레거시 INTERNAL과 유사)
    IOIF_FileSystem_BUSY,                       // 드라이버 사용 중 (신규)
    IOIF_FileSystem_TIMEOUT,                    // Mutex 또는 작업 시간 초과 (레거시/신규)
    IOIF_FileSystem_NOT_INITIALIZED,            // 드라이버가 초기화되지 않음 (신규)
    IOIF_FileSystem_PARAM_ERROR,                // 잘못된 파라미터 (레거시/신규)
    IOIF_FileSystem_FS_ERROR,                   // FATFS 라이브러리 오류 (신규)
    IOIF_FileSystem_NOT_READY,                  // USB/SD가 마운트되지 않음 (신규)
    IOIF_FileSystem_SEMAPHORE_ERROR,            // Mutex 생성/반환 실패 (레거시/신규)
    IOIF_FileSystem_BUFFER_OVERFLOW,            // 버퍼 크기 초과 (신규, 레거시 SIZE_EXCEED 대체)
    IOIF_FileSystem_DRIVER_ERROR,               // USBH_Driver/SD_Driver 연결 실패 (레거시/신규)

    // --- 레거시 상세 오류 코드 ---
    IOIF_FileSystem_PATH_INVALID,               // 잘못된 경로명 (레거시)
    IOIF_FileSystem_INTERNAL,                   // 내부 로직 오류 (레거시)
    IOIF_FileSystem_SEMAPHORE_NOT_INITIALIZED,  // 세마포어가 생성되지 않음 (레거시)
    IOIF_FileSystem_OVERWRITE_PROTECTED,        // 덮어쓰기 방지 (레거시)
    IOIF_FileSystem_NOT_EXIST,                  // 파일/경로 없음 (레거시)
    IOIF_FileSystem_FILECOUNT_EXCEED,           // 최대 파일 개수 초과 (레거시)
    IOIF_FileSystem_FILEINSTANCE_FULL,          // 파일 핸들 풀 가득 참 (레거시)
    IOIF_FileSystem_FILEINSTANCE_INVALID,       // 잘못된 파일 핸들 (레거시)
    IOIF_FileSystem_ACCESS_MODE_INVALID,        // 잘못된 접근 모드 (레거시)
    IOIF_FileSystem_ACCESS_ALREADY_OPENED       // 이미 열려있음 (레거시)
} AGRBFileSystemStatusDef;

/**
 * @brief [유지] 파일명/경로명 유효성 검사 결과
 */
typedef enum {
    IOIF_FileSystem_NamingRule_OK = 0,
    IOIF_FileSystem_NamingRule_PARAM_ERROR,
    IOIF_FileSystem_NamingRule_INVALID_LENGTH,
    IOIF_FileSystem_NamingRule_INVALID_CHARACTER,
    IOIF_FileSystem_NamingRule_CONSECUTIVE_SLASHES,
    IOIF_FileSystem_NamingRule_START_WITH_DOT,
    IOIF_FileSystem_NamingRule_END_WITH_DOT,
    IOIF_FileSystem_NamingRule_NOT_START_WITH_SLASH,
    IOIF_FileSystem_NamingRule_END_WITH_SLASH,
    IOIF_FileSystem_NamingRule_START_WITH_SPACE,
    IOIF_FileSystem_NamingRule_END_WITH_SPACE,
} IOIF_FileSystem_NamingRule_e;

/**
 * @brief [유지] 스토리지 용량 정보 구조체
 * @details (ioif_filesystem_get_inspect 함수가 이 구조체를 채움)
 */
typedef struct {
    uint32_t block_size; // 섹터 크기 (bytes)
    uint32_t total_blocks;
    uint32_t free_blocks;

    uint32_t total_size; // 총 용량 (bytes)
    uint32_t free_size;  // 남은 용량 (bytes)

    bool is_initialized; // 마운트 여부
    
} IOIF_FileSystem_Inspect_t;

/**
 * @brief [유지] 파일 핸들 ID 타입 정의
 */
#define IOIF_FILE_INVALID_ID (0xDEADF11E) // (기존 코드에서 이동)
typedef uint32_t IOIF_FILEx_t; // File handle ID type

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief [신규] USB MSC가 마운트되어 파일 시스템이 준비되었는지 확인합니다.
 * @details data_logger 또는 usb_mode_handler가 로깅 가능 여부를 판단하는 데 사용합니다.
 * @return 준비되었으면 true
 */
bool ioif_filesystem_is_ready(void);

/**
 * @brief [유지] FATFS 드라이버를 Link/Mount합니다.
 * @details 시스템 부팅 시 또는 ioif_agrb_usb.c가 Host 모드로 진입할 때 호출됩니다.
 * USB의 경우 f_mount는 즉시 실행되지 않고, HandleHostEvent에서 실행됩니다.
 */
AGRBFileSystemStatusDef ioif_filesystem_init(void);

/**
 * @brief [유지] FATFS 드라이버를 Unlink/Unmount하고 Mutex를 삭제합니다.
 */
AGRBFileSystemStatusDef ioif_filesystem_deinit(void);

/**
 * @brief [유지] 파일을 엽니다 (읽기/쓰기/추가).
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_open(IOIF_FILEx_t* id, const char* path, IOIF_FileSystem_AccessMode_e mode, FRESULT* fresult);

/**
 * @brief [유지] 쓰기 모드로 파일을 엽니다 (생성 옵션 포함).
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_open_write(IOIF_FILEx_t* id, const char* path, IOIF_FileSystem_CreateMode_e mode, FRESULT* fresult);

/**
 * @brief [유지] 파일을 닫습니다.
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_close(IOIF_FILEx_t id, FRESULT* fresult);

/**
 * @brief [유지] 파일에 데이터를 씁니다.
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 * 이 함수는 동기식(Blocking)이므로 저순위 태스크(DataLoggerTask)에서만 호출해야 합니다.
 */
AGRBFileSystemStatusDef ioif_filesystem_write(IOIF_FILEx_t id, const uint8_t* data, uint32_t size, FRESULT* fresult);

/**
 * @brief [유지] 파일에서 데이터를 읽습니다.
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_read(IOIF_FILEx_t id, uint8_t* read_buffer, uint32_t size_to_read, uint32_t* size_read, FRESULT* fresult);

/**
 * @brief [유지] 파일의 현재 크기를 바이트 단위로 가져옵니다.
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_get_size(IOIF_FILEx_t id, uint32_t* size);

/**
 * @brief [신규] 파일 크기를 MB 단위로 가져옵니다. (파일 롤링용)
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_get_size_mb(IOIF_FILEx_t id, uint32_t* size_mb);

/**
 * @brief [신규] 디렉터리(폴더)를 생성합니다. (세션 관리용)
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_mkdir(const char* path, FRESULT* fresult);

/**
 * @brief [신규] 남은 용량을 MB 단위로 가져옵니다. (용량 확인용)
 * @details [FIX] 스레드 안전합니다 (내부 Mutex로 보호됨).
 */
AGRBFileSystemStatusDef ioif_filesystem_get_free_space_mb(uint32_t* free_mb);

/**
 * @brief [신규] 파일 또는 빈 디렉터리를 삭제합니다.
 * @details 원본의 ioif_filesystem_delete_ex 기능을 대체합니다.
 * 스레드 안전합니다 (Mutex 포함).
 * @param[in] path 삭제할 파일 또는 디렉터리의 전체 경로
 * @param[out] fresult (Optional) FATFS의 FRESULT 반환값
 * @return AGRBFileSystemStatusDef 상태
 */
AGRBFileSystemStatusDef ioif_filesystem_delete(const char* path, FRESULT* fresult);

/**
 * @brief [신규] USB Host 이벤트 핸들러 (의존성 역전)
 * @details 3rd-party 미들웨어인 usb_host.c의 USER CODE 블록에서
 * 이 함수를 호출해야 합니다. (빌드 충돌 방지)
 * @param[in] isActive true시 HOST_USER_CLASS_ACTIVE, false시 HOST_USER_DISCONNECTION
 */
void ioif_filesystem_HandleHostEvent(bool isActive);

/**
 * @brief [신규] 저순위 태스크가 주기적으로 호출할 마운트 처리 함수
 */
AGRBFileSystemStatusDef ioif_filesystem_ProcessMount(void);

/* ===================================================================
 * [유지] AGRBFileSystem 구조체 (API 추상화)
 * =================================================================== */

/**
 * @brief 범용 파일 시스템 API의 추상화 인터페이스 구조체
 * @details
 * ioif_agrb_usb.c 또는 다른 모듈이
 * 실제 구현(f_write 등)을 몰라도 `AGRBFileSystem.write(...)`
 * 형식으로 파일 시스템을 사용할 수 있게 합니다. (전략 패턴)
 */
typedef struct {
    AGRBFileSystemStatusDef (*init)(void);
    AGRBFileSystemStatusDef (*deinit)(void);
    AGRBFileSystemStatusDef (*open)(IOIF_FILEx_t* id, const char* path, IOIF_FileSystem_AccessMode_e mode, FRESULT* fresult);
    AGRBFileSystemStatusDef (*open_write)(IOIF_FILEx_t* id, const char* path, IOIF_FileSystem_CreateMode_e mode, FRESULT* fresult);
    AGRBFileSystemStatusDef (*read)(IOIF_FILEx_t id, uint8_t* read_buffer, uint32_t size_to_read, uint32_t* size_read, FRESULT* fresult);
    AGRBFileSystemStatusDef (*write)(IOIF_FILEx_t id, const uint8_t* data, uint32_t size, FRESULT* fresult);
    AGRBFileSystemStatusDef (*close)(IOIF_FILEx_t id, FRESULT* fresult);
    AGRBFileSystemStatusDef (*get_size)(IOIF_FILEx_t id, uint32_t* size);
    
    // data_logger.c가 사용할 함수들
    AGRBFileSystemStatusDef (*mkdir)(const char* path, FRESULT* fresult);
    AGRBFileSystemStatusDef (*get_free_space_mb)(uint32_t* free_mb);
    AGRBFileSystemStatusDef (*get_size_mb)(IOIF_FILEx_t id, uint32_t* size_mb);
    AGRBFileSystemStatusDef (*delete)(const char* path, FRESULT* fresult);
    bool (*is_ready)(void);
    AGRBFileSystemStatusDef (*process_mount)(void);

} AGRBFileSystem_t;

/**
 * @brief AGRBFileSystem API의 전역 인스턴스
 */
extern AGRBFileSystem_t AGRBFileSystem;

#endif /* IOIF_INC_IOIF_AGRB_FS_H_ */

#endif /* AGRB_IOIF_FILESYSTEM_ENABLE */
