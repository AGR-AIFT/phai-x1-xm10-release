/**
 ******************************************************************************
 * @file    ioif_agrb_fs.c
 * @author  HyundoKim
 * @brief   [IOIF Layer] FATFS 추상화 드라이버 (USB Host & SD Card)
 * @details
 * - 원본의 USB/SD 멀티 스토리지, 파일 핸들 풀, API 추상화 의도 보존
 * - [FIX] .NOCACHE_DMA_RAM 섹션을 사용하도록 D2 RAM 타겟팅 수정
 * - [FIX] FATFS API 호출부를 Mutex로 보호하여 스레드 안전성 확보
 * - [ADD] data_logger를 위한 mkdir, get_free_space, get_size_mb 함수 추가
 * - [ADD] usb_host.c 콜백을 처리할 ioif_filesystem_HandleHostEvent 추가
 * @version 0.1
 * @date    Nov 10, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "ioif_agrb_fs.h"
#if defined(AGRB_IOIF_FILESYSTEM_ENABLE)

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ff.h"
#include "diskio.h"
#include "ff_gen_drv.h" // Diskio_drvTypeDef
#include "module.h"

// [유지] 스토리지 드라이버 포함
#if defined(HAL_HCD_MODULE_ENABLED)
#include "usb_host.h"       // hUsbHostFS
#include "usbh_core.h"
#include "usbh_msc.h"
#endif
#if defined(HAL_SD_MODULE_ENABLED)
#include "bsp_driver_sd.h" // (가정)
#endif

// [신규] FreeRTOS 및 CMSIS-OS 관련 헤더
#include "cmsis_os2.h"
#include "semphr.h" // Mutex 사용

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/**
 * @brief 기존 ioif_agrb_fs.c에서 복사
 */
#define IOIF_FILESYSTEM_DEFAULT_TIMEOUT         (pdMS_TO_TICKS(1000U))
//#define IOIF_FILESYSTEM_USE_DEBUG_LOGGING     //Enable debug logging for filesystem operations
#define IOIF_FILESYSTEM_DEFAULT_ROOTPATH        "0" // Default root path
#define IOIF_FILE_MAX_INSTANCES                 (5)
#define IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH     (256)
#define IOIF_FILESYSTEM_MAX_FULLPATH_LENGTH     (160)   // NULL 문자 포함
#define IOIF_FILESYSTEM_MAX_FOLDER_LENGTH       (128)   // NULL 문자 포함
#define IOIF_FILESYSTEM_MAX_FILENAME_LENGTH     (64)    // 확장자 제외, NULL 문자 포함
#define IOIF_FILESYSTEM_MAX_POSTFIX_LENGTH      (8)     // 파일명 뒤에 붙는 번호 등, NULL 문자 포함
#define IOIF_FILESYSTEM_MAX_EXTENSION_LENGTH    (16)    // NULL 문자 포함

#define IOIF_FILESYSTEM_READ_BOUNCE_BUFFER_SIZE    (4096) // 4KB
#define IOIF_FILESYSTEM_WRITE_BOUNCE_BUFFER_SIZE   (4096) // 4KB
#if (_FS_LOCK != 0 )
#define IOIF_FILESYSTEM_MAX_OPENED_FILES          (_FS_LOCK)   /* 최대 8개 파일까지 열 수 있음 */
#else
#define IOIF_FILESYSTEM_MAX_OPENED_FILES          (8)   /* 최대 8 */
#endif

#define IOIF_FILESYSTEM_BACKUPFILE_EXTENSION        "bak"   // 백업 파일 확장자
#define IOIF_FILESYSTEM_BACKUPFILE_EXTENSION_DOT    ".bak"  // 백업 파일 확장자 (점 포함)
#define IOIF_FILESYSTEM_BACKUPFILE_EXTENSION_LENGTH (5)     // NULL 문자 포함

#define IOIF_FILESYSTEM_MAX_FILECOUNT_PER_PATH  (128)   // 하나의 경로에 최대 N개 파일까지 생성가능
#define IOIF_FILESYSTEM_MAX_FILE_INCREMENT_TRY  (64)    // 파일 이름 자동 증가 시도 최대 횟수

/**
 * @brief [신규] FATFS 재진입(Re-entrancy)을 막기 위한 Mutex
 */
static SemaphoreHandle_t s_fileSystemMutex = NULL;

#define FATFS_MUTEX_TIMEOUT_DEFAULT_MS (1000)
#define FATFS_MUTEX_TIMEOUT_IO_MS      (2000)
#define FATFS_MUTEX_TIMEOUT_SYNC_MS    (3000)

/* Recursive Mutex: 같은 task 내 중첩 acquire 안전 (helper 함수 자체 lock 대비) */
#define FATFS_MUTEX_LOCK()         (s_fileSystemMutex != NULL && xSemaphoreTakeRecursive(s_fileSystemMutex, pdMS_TO_TICKS(FATFS_MUTEX_TIMEOUT_DEFAULT_MS)) == pdTRUE)
#define FATFS_MUTEX_LOCK_IO()      (s_fileSystemMutex != NULL && xSemaphoreTakeRecursive(s_fileSystemMutex, pdMS_TO_TICKS(FATFS_MUTEX_TIMEOUT_IO_MS)) == pdTRUE)
#define FATFS_MUTEX_LOCK_SYNC()    (s_fileSystemMutex != NULL && xSemaphoreTakeRecursive(s_fileSystemMutex, pdMS_TO_TICKS(FATFS_MUTEX_TIMEOUT_SYNC_MS)) == pdTRUE)
#define FATFS_MUTEX_UNLOCK()       xSemaphoreGiveRecursive(s_fileSystemMutex)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 기존 ioif_agrb_fs.c에서 복사 (파일 핸들 풀)
 */
typedef struct {
    bool allocated;
    IOIF_FILEx_t id;
    FIL file;
    FRESULT fresult;

    IOIF_FileSystem_AccessMode_e mode;
    char filename[IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH];
} IOIF_FileObject_t; // (원본의 IOIF_FileSystem_FileInstance_t와 동일)

/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

#if defined(HAL_HCD_MODULE_ENABLED)
extern const Diskio_drvTypeDef  USBH_Driver; // (usbh_diskio.h에 선언됨)
extern USBH_HandleTypeDef hUsbHostFS;
#endif
#if defined(HAL_SD_MODULE_ENABLED)
extern const Diskio_drvTypeDef  SD_Driver; // (sd_diskio.h에 선언됨)
#endif

/**
 * @brief [수정] API 구조체 정의 (ioif_agrb_fs.h와 일치)
 */
AGRBFileSystem_t AGRBFileSystem = {
    .init = ioif_filesystem_init,
    .deinit = ioif_filesystem_deinit,
    .open = ioif_filesystem_open,
    .open_write = ioif_filesystem_open_write,
    .read = ioif_filesystem_read,
    .write = ioif_filesystem_write,
    .close = ioif_filesystem_close,
    .get_size = ioif_filesystem_get_size,
    .mkdir = ioif_filesystem_mkdir,
    .get_free_space_mb = ioif_filesystem_get_free_space_mb,
    .get_space_mb = ioif_filesystem_get_space_mb,
    .get_size_mb = ioif_filesystem_get_size_mb,
    .delete = ioif_filesystem_delete,
    .sync = ioif_filesystem_sync,
    .seek = ioif_filesystem_seek,
    .truncate = ioif_filesystem_truncate,
    .is_ready = ioif_filesystem_is_ready,
    .process_mount = ioif_filesystem_ProcessMount // [신규] 함수 포인터 할당
};

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

// D2 영역에서처럼 DMA를 사용하는 MPU설정이 없어도(파일시스템 사용시 필요한 non-cachable 등의 설정)
// usbh_diskio.c의 USBH_read / USBH_write 함수에 수동 캐시 관리 함수(SCB_Clean..., SCB_Invalidate...)
// 가 존재함에 따라 FatFS 구조체와 Filesystem Object가 D1 영역(기본) 사용되어도 문제 없음(약 48kB)
static FATFS s_fs_drive;
static IOIF_FileObject_t s_file_pool[IOIF_FILE_MAX_INSTANCES];

/**
 * @brief 기존 ioif_agrb_fs.c에서 복사
 */
static char s_fs_path[4];   /* USBH_Path or SDPath */
static volatile bool s_is_fs_ready = false;
static volatile bool s_is_mount_pending = false;
static IOIF_FileSystem_DeviceType_e s_current_device_type; // [유지]
static const Diskio_drvTypeDef* s_storage_driver = NULL; // 범용 드라이버 포인터

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief 기존 ioif_agrb_fs.c에서 복사
 */
static IOIF_FileObject_t* _get_file_from_pool(IOIF_FILEx_t id);
static AGRBFileSystemStatusDef _release_file_to_pool(IOIF_FILEx_t id);
static IOIF_FileObject_t* _get_free_file_id(IOIF_FILEx_t* id);
static BYTE _IOIF_FileSystem_Mode_To_FatFs_Mode(IOIF_FileSystem_AccessMode_e mode);
static BYTE _IOIF_FileSystem_CreateMode_To_FatFs_Mode(IOIF_FileSystem_CreateMode_e mode);
static const Diskio_drvTypeDef* _ioif_filesystem_get_diskio_driver(IOIF_FileSystem_DeviceType_e device_type);
static bool _ioif_filesystem_check_pathname_vaildation(const char* path);
static FRESULT _ioif_filesystem_check_file_exist(const char* path);
static FRESULT _ioif_filesystem_check_path_exist(const char* path);
static FRESULT _ioif_filesystem_check_filecount(const char* path, uint32_t* count);
static FRESULT _ioif_filesystem_create_path_recursively(char* path);
static bool _build_full_path(const char* path, const char* filename, const char* ext, char* out_fullname, uint32_t max_len);
static bool _check_file_increment(IOIF_FileObject_t* file_obj, const char* path, const char* filename, const char* ext, FRESULT* out_fresult);
static bool _file_open_write_exclusive(IOIF_FileObject_t* file_obj, const char* path, FRESULT* out_fresult);
static bool _file_open_write_append(IOIF_FileObject_t* file_obj, const char* path, FRESULT* out_fresult);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

AGRBFileSystemStatusDef ioif_filesystem_init(IOIF_FileSystem_DeviceType_e device_type)
{
    if (s_fileSystemMutex == NULL) {
        s_fileSystemMutex = xSemaphoreCreateRecursiveMutex();
        if (s_fileSystemMutex == NULL) return IOIF_FileSystem_SEMAPHORE_ERROR;
    }

    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (device_type == IOIF_FileSystem_DeviceType_Auto) {
#if defined(HAL_HCD_MODULE_ENABLED)
        device_type = IOIF_FileSystem_DeviceType_USBH_MSC;
#elif defined(HAL_SD_MODULE_ENABLED)
        device_type = IOIF_FileSystem_DeviceType_SDCard;
#else
        return IOIF_FileSystem_DRIVER_ERROR;
#endif
    }

    s_current_device_type = device_type;

    switch (device_type) {
    case IOIF_FileSystem_DeviceType_USBH_MSC:
#if defined(HAL_HCD_MODULE_ENABLED)
        s_storage_driver = &USBH_Driver;
        strcpy(s_fs_path, "0:");
#else
        return IOIF_FileSystem_DRIVER_ERROR;
#endif
        break;
    case IOIF_FileSystem_DeviceType_SDCard:
#if defined(HAL_SD_MODULE_ENABLED)
        s_storage_driver = &SD_Driver;
        strcpy(s_fs_path, "0:"); /* xCM: Multi-Volume 시 "1:" 사용 */
#else
        return IOIF_FileSystem_DRIVER_ERROR;
#endif
        break;
    default:
        return IOIF_FileSystem_PARAM_ERROR;
    }

    // [유지] FATFS 드라이버 연결
    if (FATFS_LinkDriver(s_storage_driver, s_fs_path) != 0) {
        status = IOIF_FileSystem_DRIVER_ERROR;
    }
    // USBH의 경우 HOST_USER_CLASS_ACTIVATE 이벤트시 마운트(ioif_filesystem_HandleHostEvent)
    
    // [유지] SD 카드의 경우, 부팅 시 즉시 마운트
#if defined(HAL_SD_MODULE_ENABLED)
    if (s_current_device_type == IOIF_FileSystem_DeviceType_SDCard) {
        if (FATFS_MUTEX_LOCK()) {
            if (f_mount(&s_fs_drive, (TCHAR const*)s_fs_path, 1) == FR_OK) {
                s_is_fs_ready = true;
            } else {
                s_is_fs_ready = false;
                status = IOIF_FileSystem_FS_ERROR;
            }
            FATFS_MUTEX_UNLOCK();
        } else {
            status = IOIF_FileSystem_TIMEOUT;
        }
    }
#endif

    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_deinit(void)
{
    // [유지] 원본 deinit 로직
    if (FATFS_MUTEX_LOCK()) {
        f_mount(NULL, (TCHAR const*)s_fs_path, 0);
        FATFS_UnLinkDriver(s_fs_path);
        FATFS_MUTEX_UNLOCK();
    }
    s_is_fs_ready = false;

    // [수정] 생성된 Mutex를 삭제하고 핸들을 NULL로 초기화
    if (s_fileSystemMutex != NULL) {
        vSemaphoreDelete(s_fileSystemMutex);
        s_fileSystemMutex = NULL;
    }
    // (USBH_Stop/DeInit은 ioif_agrb_usb.c의 deinit이 담당)
    
    return IOIF_FileSystem_OK;
}
/**
 * @brief [신규] USB MSC가 마운트되어 파일 시스템이 준비되었는지 확인합니다.
 */
bool ioif_filesystem_is_ready(void)
{
    return s_is_fs_ready;
}

AGRBFileSystemStatusDef ioif_filesystem_open(IOIF_FILEx_t* id, const char* path, IOIF_FileSystem_AccessMode_e mode, FRESULT* fresult)
{
    if (id == NULL || path == NULL) return IOIF_FileSystem_PARAM_ERROR;
    if (strlen(path) >= IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    BYTE fatfs_mode = _IOIF_FileSystem_Mode_To_FatFs_Mode(mode);
    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK()) {
        IOIF_FileObject_t* file_obj = _get_free_file_id(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_open(&file_obj->file, path, fatfs_mode);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult == FR_OK) {
            strncpy(file_obj->filename, path, IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH - 1);
            file_obj->mode = mode;
        } else {
            _release_file_to_pool(*id);
            *id = IOIF_FILE_INVALID_ID;
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }
    
    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_open_write(IOIF_FILEx_t* id, const char* path, IOIF_FileSystem_CreateMode_e mode, FRESULT* fresult)
{
    if (id == NULL || path == NULL) return IOIF_FileSystem_PARAM_ERROR;
    if (strlen(path) >= IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    BYTE fatfs_mode = _IOIF_FileSystem_CreateMode_To_FatFs_Mode(mode);
    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK()) {
        IOIF_FileObject_t* file_obj = _get_free_file_id(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_open(&file_obj->file, path, fatfs_mode);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult == FR_OK) {
            strncpy(file_obj->filename, path, IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH - 1);
            file_obj->mode = IOIF_FileSystem_AccessMode_WRITE;
        } else {
            _release_file_to_pool(*id);
            *id = IOIF_FILE_INVALID_ID;
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }
    
    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_write(IOIF_FILEx_t id, const uint8_t* data, uint32_t size, FRESULT* fresult)
{
    if (id == IOIF_FILE_INVALID_ID || data == NULL || size == 0) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    UINT bytes_written = 0;
    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK_IO()) {
        IOIF_FileObject_t* file_obj = _get_file_from_pool(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_write(&file_obj->file, data, size, &bytes_written);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult != FR_OK || bytes_written != size) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }

    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_sync(IOIF_FILEx_t id, FRESULT* fresult)
{
    if (id == IOIF_FILE_INVALID_ID) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK_SYNC()) {
        IOIF_FileObject_t* file_obj = _get_file_from_pool(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_sync(&file_obj->file);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult != FR_OK) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }

    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_seek(IOIF_FILEx_t id, uint32_t offset, FRESULT* fresult)
{
    if (id == IOIF_FILE_INVALID_ID) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK_SYNC()) {
        IOIF_FileObject_t* file_obj = _get_file_from_pool(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_lseek(&file_obj->file, (FSIZE_t)offset);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult != FR_OK) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }

    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_truncate(IOIF_FILEx_t id, FRESULT* fresult)
{
    if (id == IOIF_FILE_INVALID_ID) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK_SYNC()) {
        IOIF_FileObject_t* file_obj = _get_file_from_pool(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_truncate(&file_obj->file);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult != FR_OK) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }

    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_read(IOIF_FILEx_t id, uint8_t* read_buffer, uint32_t size_to_read, uint32_t* size_read, FRESULT* fresult)
{
    if (id == IOIF_FILE_INVALID_ID || read_buffer == NULL || size_read == NULL) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK_IO()) {
        IOIF_FileObject_t* file_obj = _get_file_from_pool(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_read(&file_obj->file, read_buffer, size_to_read, (UINT*)size_read);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult != FR_OK) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }

    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_close(IOIF_FILEx_t id, FRESULT* fresult)
{
    if (id == IOIF_FILE_INVALID_ID) return IOIF_FileSystem_PARAM_ERROR;

    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK()) {
        IOIF_FileObject_t* file_obj = _get_file_from_pool(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        file_obj->fresult = f_close(&file_obj->file);
        if (fresult) *fresult = file_obj->fresult;

        if (file_obj->fresult != FR_OK) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        _release_file_to_pool(id);
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }

    return status;
}

AGRBFileSystemStatusDef ioif_filesystem_get_size(IOIF_FILEx_t id, uint32_t* size)
{
    if (id == IOIF_FILE_INVALID_ID || size == NULL) return IOIF_FileSystem_PARAM_ERROR;

    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK()) {
        IOIF_FileObject_t* file_obj = _get_file_from_pool(id);
        if (file_obj == NULL) {
            FATFS_MUTEX_UNLOCK();
            return IOIF_FileSystem_FILEINSTANCE_FULL;
        }

        *size = (uint32_t)f_size(&file_obj->file);
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }

    return status;
}

/**
 * @brief [신규] 파일 크기를 MB 단위로 가져옵니다.
 */
AGRBFileSystemStatusDef ioif_filesystem_get_size_mb(IOIF_FILEx_t id, uint32_t* size_mb)
{
    uint32_t size_bytes = 0;
    AGRBFileSystemStatusDef status = ioif_filesystem_get_size(id, &size_bytes);
    if (status == IOIF_FileSystem_OK) {
        *size_mb = size_bytes / (1024 * 1024);
    }
    return status;
}

/**
 * @brief [신규] 디렉터리(폴더)를 생성합니다. (CM의 로직 지원)
 */
AGRBFileSystemStatusDef ioif_filesystem_mkdir(const char* path, FRESULT* fresult)
{
    if (path == NULL) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;
    
    FRESULT res;
    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK()) { // [수정] Mutex
        res = f_mkdir(path);
        if (fresult) *fresult = res;
        if (res != FR_OK && res != FR_EXIST) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }
    return status;
}

/**
 * @brief [신규] USB 저장소의 남은 용량을 MB 단위로 가져옵니다. (CM의 로직 지원)
 */
AGRBFileSystemStatusDef ioif_filesystem_get_free_space_mb(uint32_t* free_mb)
{
    return ioif_filesystem_get_space_mb(free_mb, NULL);
}

/**
 * @brief [신규] 남은 용량과 전체 용량을 MB 단위로 가져옵니다.
 * @details f_getfree() 1회 호출로 free+total 동시 반환.
 *          free_mb 또는 total_mb가 NULL이면 해당 값은 무시됩니다.
 */
AGRBFileSystemStatusDef ioif_filesystem_get_space_mb(uint32_t* free_mb, uint32_t* total_mb)
{
    if (free_mb == NULL && total_mb == NULL) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;

    FATFS *pfs;
    DWORD fre_clust, fre_sect, tot_sect;
    FRESULT res;
    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK()) {
        res = f_getfree((TCHAR const*)s_fs_path, &fre_clust, &pfs);
        if (res == FR_OK) {
            tot_sect = (pfs->n_fatent - 2) * pfs->csize;
            fre_sect = fre_clust * pfs->csize;
            if (free_mb != NULL)  *free_mb  = ((fre_sect / 2) / 1024);
            if (total_mb != NULL) *total_mb = ((tot_sect / 2) / 1024);
        } else {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }
    return status;
}

/**
 * @brief [신규] 파일 또는 빈 디렉터리를 삭제합니다.
 * @details [FIX] f_unlink 호출을 Mutex로 보호합니다.
 */
AGRBFileSystemStatusDef ioif_filesystem_delete(const char* path, FRESULT* fresult)
{
    if (path == NULL) return IOIF_FileSystem_PARAM_ERROR;
    if (!s_is_fs_ready) return IOIF_FileSystem_NOT_READY;
    
    FRESULT res;
    AGRBFileSystemStatusDef status = IOIF_FileSystem_OK;

    if (FATFS_MUTEX_LOCK()) {
        res = f_unlink(path); // f_unlink는 파일과 빈 디렉터리 모두 삭제
        if (fresult) *fresult = res;
        
        if (res != FR_OK) {
            status = IOIF_FileSystem_FS_ERROR;
        }
        FATFS_MUTEX_UNLOCK();
    } else {
        status = IOIF_FileSystem_TIMEOUT;
    }
    return status;
}

/**
 * @brief [신규] usb_host.c의 USER CODE 블록에서 호출될 함수
 * @details f_mount()를 직접 호출하지 않고, 'mount pending' 플래그만 설정합니다.
 */
void ioif_filesystem_HandleHostEvent(bool isActive)
{
    if (isActive) {
        // HOST_USER_CLASS_ACTIVE 이벤트
        if (FATFS_MUTEX_LOCK()) {
            // HOST_USER_CLASS_ACTIVE 이벤트
            s_is_mount_pending = true; // [수정] 마운트 요청
            s_is_fs_ready = false;
        	// mountRes = f_mount(&s_fs_drive, (TCHAR const*)s_fs_path, 1);
            // if (mountRes == FR_OK) {
            //     s_is_fs_ready = true;
            // } else {
            //     s_is_fs_ready = false;
            // }
            FATFS_MUTEX_UNLOCK();
        }
    } else {
        // HOST_USER_DISCONNECTION 이벤트
        s_is_mount_pending = false;
        s_is_fs_ready = false;

        // Unmount는 즉시 실행
        if (FATFS_MUTEX_LOCK()) {
            f_mount(NULL, (TCHAR const*)s_fs_path, 0);
            FATFS_MUTEX_UNLOCK();
        }
    }
}
/**
 * @brief [신규] 저순위 태스크가 주기적으로 호출할 마운트 처리 함수
 */
AGRBFileSystemStatusDef ioif_filesystem_ProcessMount(void)
{
    // 1. 마운트 요청이 없거나, 이미 마운트되었다면 할 일 없음
    if (!s_is_mount_pending || s_is_fs_ready) {
        return IOIF_FileSystem_OK;
    }

#if defined(HAL_HCD_MODULE_ENABLED)
    // 2. ST의 MSC 스택이 SCSI 명령을 받을 준비가 되었는지 확인 (Polling)
    if (USBH_MSC_UnitIsReady(&hUsbHostFS, 0) == 0) {
        return IOIF_FileSystem_BUSY; // 아직 준비 안 됨 (다음 100ms에 재시도)
    }
#endif

    // 3. 디스크가 준비되었으므로, f_mount 시도
    FRESULT mount_res = FR_NOT_READY;
    if (FATFS_MUTEX_LOCK()) {
        mount_res = f_mount(&s_fs_drive, (TCHAR const*)s_fs_path, 1);
        if (mount_res == FR_OK) {
            s_is_fs_ready = true;
            s_is_mount_pending = false;
        }
        FATFS_MUTEX_UNLOCK();
    }
    
    return (mount_res == FR_OK) ? IOIF_FileSystem_OK : IOIF_FileSystem_FS_ERROR;
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief [유지] 파일 핸들 ID로 파일 객체 풀에서 검색
 */
static IOIF_FileObject_t* _get_file_from_pool(IOIF_FILEx_t id)
{
    // [유지] 기존 `_get_file_from_pool` 내용 복사
    if (id == IOIF_FILE_INVALID_ID) return NULL;
    if (id >= IOIF_FILE_MAX_INSTANCES) return NULL;
    // (원본 코드의 _file_instances -> s_file_pool로 변수명 변경됨)
    if (s_file_pool[id].allocated == false) return NULL;
    return &s_file_pool[id];
}

/**
 * @brief [유지] 사용 완료된 파일 객체를 풀에 반환
 */
static AGRBFileSystemStatusDef _release_file_to_pool(IOIF_FILEx_t id)
{
    // [유지] 기존 `_release_file_to_pool` 내용 복사
    if (id == IOIF_FILE_INVALID_ID) return IOIF_FileSystem_PARAM_ERROR;
    if (id >= IOIF_FILE_MAX_INSTANCES) return IOIF_FileSystem_PARAM_ERROR;
    // (원본 코드의 _file_instances -> s_file_pool로 변수명 변경됨)
    if (s_file_pool[id].allocated == false) return IOIF_FileSystem_FILEINSTANCE_INVALID;
    
    memset(&s_file_pool[id], 0, sizeof(IOIF_FileObject_t));
    s_file_pool[id].allocated = false;
    s_file_pool[id].id = IOIF_FILE_INVALID_ID;
    
    return IOIF_FileSystem_OK;
}

/**
 * @brief [유지] 파일 객체 풀에서 사용 가능한 새 ID 할당
 */
static IOIF_FileObject_t* _get_free_file_id(IOIF_FILEx_t* id)
{
    // [유지] 기존 `_get_free_file_id` 내용 복사
    // (원본 코드의 _file_instances -> s_file_pool로 변수명 변경됨)
    for (uint32_t i = 0; i < IOIF_FILE_MAX_INSTANCES; i++) {
        if (s_file_pool[i].allocated == false) {
            s_file_pool[i].allocated = true;
            s_file_pool[i].id = i;
            *id = i;
            return &s_file_pool[i];
        }
    }
    return NULL;
}

/**
 * @brief [유지] IOIF 접근 모드를 FATFS 모드로 변환
 */
static BYTE _IOIF_FileSystem_Mode_To_FatFs_Mode(IOIF_FileSystem_AccessMode_e mode)
{
    // [유지] 기존 `_IOIF_FileSystem_Mode_To_FatFs_Mode` 내용 복사
    switch (mode) {
        case IOIF_FileSystem_AccessMode_READONLY: return FA_READ;
        case IOIF_FileSystem_AccessMode_WRITE: return FA_WRITE;
        case IOIF_FileSystem_AccessMode_APPEND: return FA_OPEN_APPEND;
        default: return FA_READ;
    }
}

/**
 * @brief [유지] IOIF 생성 모드를 FATFS 모드로 변환
 */
static BYTE _IOIF_FileSystem_CreateMode_To_FatFs_Mode(IOIF_FileSystem_CreateMode_e mode)
{
    // [유지] 기존 `_IOIF_FileSystem_CreateMode_To_FatFs_Mode` 내용 복사
    switch (mode) {
        case IOIF_FileSystem_CreateMode_EXCLUSIVE: return (FA_WRITE | FA_CREATE_NEW);
        case IOIF_FileSystem_CreateMode_APPEND: return (FA_WRITE | FA_OPEN_APPEND);
        case IOIF_FileSystem_CreateMode_OVERWRITE: return (FA_WRITE | FA_CREATE_ALWAYS);
        case IOIF_FileSystem_CreateMode_INCREMENT: return (FA_WRITE | FA_CREATE_NEW); // (INCREMENT 로직은 _check_file_increment가 담당)
        default: return (FA_WRITE | FA_CREATE_NEW);
    }
}

/**
 * @brief [유지] 장치 타입에 맞는 스토리지 드라이버 반환
 */
static const Diskio_drvTypeDef* _ioif_filesystem_get_diskio_driver(IOIF_FileSystem_DeviceType_e device_type)
{
    // [유지] 기존 `_ioif_filesystem_get_diskio_driver` 내용 복사
#if defined(HAL_HCD_MODULE_ENABLED)
    if (device_type == IOIF_FileSystem_DeviceType_USBH_MSC) {
        return &USBH_Driver;
    }
#endif
#if defined(HAL_SD_MODULE_ENABLED)
    if (device_type == IOIF_FileSystem_DeviceType_SDCard) {
        return &SD_Driver;
    }
#endif
    return NULL;
}

/**
 * @brief [유지] (사용자 요청) 원본 `_build_full_path` 함수
 * @details 경로, 파일명, 확장자를 조합하여 전체 경로 문자열을 생성합니다.
 */
static bool _build_full_path(const char* path, const char* filename, const char* ext, char* out_fullname, uint32_t max_len)
{
    if (path == NULL || filename == NULL || ext == NULL || out_fullname == NULL) return false;
    
    // (경고: 원본의 strnlen 대신 strlen을 사용하면 위험할 수 있습니다. strnlen 사용 권장)
    size_t path_len = strnlen(path, IOIF_FILESYSTEM_MAX_FULLPATH_LENGTH);
    size_t filename_len = strnlen(filename, IOIF_FILESYSTEM_MAX_FILENAME_LENGTH);
    size_t ext_len = strnlen(ext, IOIF_FILESYSTEM_MAX_EXTENSION_LENGTH);

    if (path_len >= IOIF_FILESYSTEM_MAX_FULLPATH_LENGTH) return false;
    if (filename_len == 0 || filename_len >= IOIF_FILESYSTEM_MAX_FILENAME_LENGTH) return false;
    if (ext_len >= IOIF_FILESYSTEM_MAX_EXTENSION_LENGTH) return false; //확장자는 없어도 됨

    // 전체 경로 길이 검사 ( / + path + / + filename + . + ext + \0 )
    size_t required_len = path_len + 1 + filename_len + (ext_len > 0 ? (1 + ext_len) : 0) + 1;
    if (path[0] != '/') required_len++; // 맨 앞 '/' 추가 공간
    
    if (required_len > max_len) return false; //Fullname too long

    // Construct full path
    memset(out_fullname, 0, max_len); 

    // 1. Path 복사 (예: "/LOGS/S_001") 맨 앞에 '/'가 없으면 붙여줌
    if (path[0] != '/') {
        out_fullname[0] = '/';
        strncpy(&out_fullname[1], path, path_len);
        path_len++; // '/' 추가했으므로 길이 1 증가
    } else {
        strncpy(out_fullname, path, path_len);
    }

    // 2. Path 끝에 '/' 추가 (예: "/LOGS/S_001/")
    if (out_fullname[path_len-1] != '/') {
        out_fullname[path_len] = '/';
        path_len++;
    }
    
    // 3. Filename 추가 (예: "/LOGS/S_001/data")
    strncat(out_fullname, filename, filename_len);
    
    // 4. Extension 추가 (예: "/LOGS/S_001/data.bin")
    if (ext_len > 0) {
    	// 이미 required_len 검사를 통과했으므로 오버플로우 발생 안 함 -> strcat 사용 (안전함)
    	strcat(out_fullname, ".");
        strncat(out_fullname, ext, ext_len);
    }
    
    return true;
}

/**
 * @brief 디렉터리 스캔으로 다음 증가 번호를 찾아 파일을 생성합니다.
 * @details f_opendir+f_readdir 1회 스캔으로 최대 번호를 찾고, 그 다음 번호로 f_open.
 *          기존 방식(f_stat 최대 64회 반복)을 대체하여 Mutex lock 횟수를 대폭 절감합니다.
 *          호출자가 이미 FATFS Mutex를 보유한 상태에서 호출해야 합니다.
 */
static bool _check_file_increment(IOIF_FileObject_t* file_obj, const char* path, const char* filename, const char* ext, FRESULT* out_fresult)
{
    char fullname[IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH];
    DIR dir;
    FILINFO fno;
    int32_t max_num = -1;
    size_t base_len = strlen(filename);
    size_t ext_len = (ext != NULL) ? strlen(ext) : 0;

    FRESULT res = f_opendir(&dir, path);
    if (res != FR_OK) {
        if (out_fresult != NULL) *out_fresult = res;
        return false;
    }

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0') {
        if (fno.fattrib & AM_DIR) continue;

        if (strncmp(fno.fname, filename, base_len) != 0) continue;

        const char* suffix = fno.fname + base_len;

        if (ext_len > 0) {
            char dot_ext[IOIF_FILESYSTEM_MAX_EXTENSION_LENGTH + 2];
            snprintf(dot_ext, sizeof(dot_ext), ".%s", ext);

            if (strcmp(suffix, dot_ext) == 0) {
                if (max_num < 0) max_num = 0;
                continue;
            }

            if (suffix[0] == '(') {
                char* end_paren = NULL;
                long num = strtol(suffix + 1, &end_paren, 10);
                if (end_paren != NULL && *end_paren == ')' && strcmp(end_paren + 1, dot_ext) == 0) {
                    if (num > max_num) max_num = (int32_t)num;
                }
            }
        } else {
            if (*suffix == '\0') {
                if (max_num < 0) max_num = 0;
            } else if (suffix[0] == '(') {
                char* end_paren = NULL;
                long num = strtol(suffix + 1, &end_paren, 10);
                if (end_paren != NULL && *end_paren == ')' && *(end_paren + 1) == '\0') {
                    if (num > max_num) max_num = (int32_t)num;
                }
            }
        }
    }
    f_closedir(&dir);

    uint32_t next_num = (max_num < 0) ? 0 : (uint32_t)(max_num + 1);

    if (next_num == 0) {
        _build_full_path(path, filename, ext, fullname, sizeof(fullname));
    } else {
        char filename_inc[IOIF_FILESYSTEM_MAX_FILENAME_LENGTH];
        snprintf(filename_inc, IOIF_FILESYSTEM_MAX_FILENAME_LENGTH, "%s(%lu)", filename, (unsigned long)next_num);
        _build_full_path(path, filename_inc, ext, fullname, sizeof(fullname));
    }

    file_obj->fresult = f_open(&file_obj->file, fullname, FA_WRITE | FA_CREATE_NEW);
    if (out_fresult != NULL) *out_fresult = file_obj->fresult;

    if (file_obj->fresult == FR_OK) {
        strncpy(file_obj->filename, fullname, IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH - 1);
        file_obj->mode = IOIF_FileSystem_AccessMode_WRITE;
        return true;
    }

    return false;
}

/**
 * @brief [유지] 경로/파일명의 유효성을 검사합니다. (예시 구현)
 * @details 이 함수는 FATFS API를 호출하지 않으므로 Mutex가 필요 없습니다.
 */
static bool _ioif_filesystem_check_pathname_vaildation(const char* path)
{
    if (path == NULL) return false;
    size_t path_len = strnlen(path, IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH);
    if (path_len == 0 || path_len >= IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH) return false;
    // FATFS/Windows에서 금지된 문자 검색: < > : " / \ | ? *
    // (단, '/'는 경로 구분을 위해 허용되어야 함)
    const char* invalid_chars = "<>:\"\\|?*"; 
    
    for (size_t i = 0; i < strlen(path); i++) {
        // 경로 구분자 외에 금지된 문자가 있는지 확인
        if (path[i] != '/' && strchr(invalid_chars, path[i]) != NULL) {
            return false; // Found invalid char
        }
    }
    return true; // Valid
}

/**
 * @brief [유지] 파일 존재 여부를 확인합니다.
 * @details [FIX] f_stat 호출을 Mutex로 보호합니다.
 */
static FRESULT _ioif_filesystem_check_file_exist(const char* path)
{
    FILINFO fno;
    FRESULT res;

    if (!FATFS_MUTEX_LOCK()) return FR_TIMEOUT;
    
    res = f_stat(path, &fno);
    
    FATFS_MUTEX_UNLOCK();

    if (res == FR_OK && (fno.fattrib & AM_DIR)) {
        return FR_NO_FILE; // 경로가 존재하지만 디렉터리임 (파일이 아님)
    }
    
    return res; // FR_OK (파일 있음) 또는 FR_NO_FILE (파일 없음)
}

/**
 * @brief [유지] 디렉터리(경로) 존재 여부를 확인합니다.
 * @details [FIX] f_stat 호출을 Mutex로 보호합니다.
 */
static FRESULT _ioif_filesystem_check_path_exist(const char* path)
{
    FILINFO fno;
    FRESULT res;

    if (!FATFS_MUTEX_LOCK()) return FR_TIMEOUT;
    
    res = f_stat(path, &fno);
    
    FATFS_MUTEX_UNLOCK();

    if (res == FR_OK && !(fno.fattrib & AM_DIR)) {
        return FR_NO_PATH; // 경로가 존재하지만 파일임 (디렉터리가 아님)
    }
    
    return res; // FR_OK (경로 있음) 또는 FR_NO_PATH (경로 없음)
}

/**
 * @brief [유지] 특정 경로의 파일 개수를 셉니다.
 * @details [FIX] f_opendir/f_readdir/f_closedir 호출을 Mutex로 보호합니다.
 */
static FRESULT _ioif_filesystem_check_filecount(const char* path, uint32_t* count)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    uint32_t counter = 0;

    if (!FATFS_MUTEX_LOCK()) return FR_TIMEOUT;

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        while (1) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break; // 오류 또는 디렉터리 끝
            
            // '.' 또는 '..' 디렉터리는 무시
            if (fno.fname[0] == '.') continue; 
            
            if (!(fno.fattrib & AM_DIR)) { // 파일만 카운트
                counter++;
            }
        }
        f_closedir(&dir);
    }
    
    FATFS_MUTEX_UNLOCK();
    
    *count = counter;
    return res;
}

/**
 * @brief [유지] 중첩된 경로(Recursive)를 한 번에 생성합니다. (mkdir -p)
 * @details [FIX] f_mkdir 호출을 Mutex로 보호합니다.
 * (원본의 `_ioif_filesystem_make_directories`와 동일한 기능)
 */
static FRESULT _ioif_filesystem_create_path_recursively(char* path)
{
    FRESULT res = FR_OK;
    char* ptr;

    // 원본 문자열을 수정하므로, 스택에 복사 (strdup/malloc 회피)
    char path_copy[IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH];
    strncpy(path_copy, path, IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH - 1);
    path_copy[IOIF_FILESYSTEM_MAX_FULLNAME_LENGTH - 1] = '\0';

    ptr = path_copy;

    // "0:/" 또는 "1:/" 같은 드라이브 루트 건너뛰기
    if (ptr[1] == ':') {
        ptr += 2; 
    }
    // "/" (루트) 건너뛰기
    if (*ptr == '/') {
        ptr++;
    }

    if (!FATFS_MUTEX_LOCK()) return FR_TIMEOUT;

    while ((ptr = strchr(ptr, '/')) != NULL) {
        *ptr = '\0'; // 임시로 문자열 자르기 (예: "0:/LOGS/S_001" -> "0:/LOGS")

        res = f_mkdir(path_copy);
        
        *ptr = '/'; // 문자열 복원
        ptr++; // 다음 '/' 검색 시작 위치

        if (res != FR_OK && res != FR_EXIST) {
            // "LOGS" 폴더 생성 실패
            FATFS_MUTEX_UNLOCK();
            return res;
        }
    }
    
    // 마지막 최종 경로 생성 (예: "0:/LOGS/S_001")
    res = f_mkdir(path_copy);
    if (res != FR_OK && res != FR_EXIST) {
        FATFS_MUTEX_UNLOCK();
        return res;
    }

    FATFS_MUTEX_UNLOCK();
    return FR_OK;
}

/**
 * @brief [유지] `_file_open_write_exclusive`
 */
static bool _file_open_write_exclusive(IOIF_FileObject_t* file_obj, const char* path, FRESULT* out_fresult)
{
    if (FATFS_MUTEX_LOCK()) {
        file_obj->fresult = f_open(&file_obj->file, path, FA_WRITE | FA_CREATE_NEW);
        if (out_fresult != NULL) *out_fresult = file_obj->fresult;
        FATFS_MUTEX_UNLOCK();
        return (file_obj->fresult == FR_OK);
    }
    if (out_fresult != NULL) *out_fresult = FR_TIMEOUT;
    return false;
}

/**
 * @brief [유지] `_file_open_write_append`
 */
static bool _file_open_write_append(IOIF_FileObject_t* file_obj, const char* path, FRESULT* out_fresult)
{
    if (FATFS_MUTEX_LOCK()) {
        file_obj->fresult = f_open(&file_obj->file, path, FA_WRITE | FA_OPEN_APPEND);
        if (out_fresult != NULL) *out_fresult = file_obj->fresult;
        FATFS_MUTEX_UNLOCK();
        return (file_obj->fresult == FR_OK);
    }
    if (out_fresult != NULL) *out_fresult = FR_TIMEOUT;
    return false;
}

/**
 * @brief [유지] `_file_open_write_overwrite`
 */
static bool _file_open_write_overwrite(IOIF_FileObject_t* file_obj, const char* path, FRESULT* out_fresult)
{
    if (FATFS_MUTEX_LOCK()) {
        file_obj->fresult = f_open(&file_obj->file, path, FA_WRITE | FA_CREATE_ALWAYS);
        if (out_fresult != NULL) *out_fresult = file_obj->fresult;
        FATFS_MUTEX_UNLOCK();
        return (file_obj->fresult == FR_OK);
    }
    if (out_fresult != NULL) *out_fresult = FR_TIMEOUT;
    return false;
}

/**
 * @brief [유지] 원본 `_ioif_filesystem_parse_path`
 * @details (Mutex 불필요 - 문자열 처리)
 */
static bool _ioif_filesystem_parse_path(const char* fullname, char* path, uint32_t max_len)
{
    if (fullname == NULL || path == NULL || max_len == 0) return false;
    
    char* last_slash = strrchr(fullname, '/');
    if (last_slash == NULL) { 
        // '/'가 없음, 경로 없음 (예: "file.txt")
        path[0] = '\0';
        return true;
    }
    
    uint32_t path_len = (last_slash - fullname) + 1; // '/' 포함
    if (path_len >= max_len) return false; // 버퍼가 너무 작음
    
    strncpy(path, fullname, path_len);
    path[path_len] = '\0';
    return true;
}

/**
 * @brief [유지] 원본 `_ioif_filesystem_parse_filename`
 * @details (Mutex 불필요 - 문자열 처리)
 */
static bool _ioif_filesystem_parse_filename(const char* fullname, char* filename, uint32_t max_len)
{
    if (fullname == NULL || filename == NULL || max_len == 0) return false;

    char* last_slash = strrchr(fullname, '/');
    const char* name_start = (last_slash == NULL) ? fullname : (last_slash + 1);
    
    char* dot = strrchr(name_start, '.');
    uint32_t name_len;

    if (dot == NULL) { // 확장자 없음
        name_len = strlen(name_start);
    } else { // 확장자 있음
        name_len = (dot - name_start);
    }
    
    if (name_len == 0 || name_len >= max_len) return false; // 이름이 없거나 버퍼가 작음
    
    strncpy(filename, name_start, name_len);
    filename[name_len] = '\0';
    return true;
}

/**
 * @brief [유지] 원본 `_ioif_filesystem_parse_extension`
 * @details (Mutex 불필요 - 문자열 처리)
 */
static bool _ioif_filesystem_parse_extension(const char* fullname, char* ext, uint32_t max_len)
{
    if (fullname == NULL || ext == NULL || max_len == 0) return false;
    
    char* last_dot = strrchr(fullname, '.');
    if (last_dot == NULL || last_dot == fullname) { 
        // '.'가 없거나, ".hidden_file"처럼 파일명 시작인 경우
        ext[0] = '\0';
        return true;
    }
    
    // '/'가 '.'보다 뒤에 있는지 확인 (예: "/path.with.dot/filename_no_ext")
    char* last_slash = strrchr(fullname, '/');
    if (last_slash != NULL && last_dot < last_slash) {
        ext[0] = '\0'; // '.'가 경로명에 속함
        return true;
    }
    
    uint32_t ext_len = strlen(last_dot); // '.' 포함 (예: ".txt")
    if (ext_len >= max_len) return false; // 버퍼 작음
    
    strncpy(ext, last_dot, ext_len);
    ext[ext_len] = '\0';
    return true;
}

/** EVENT CALLBACK **/
#if defined(HAL_HCD_MODULE_ENABLED)

#elif defined(HAL_SD_MODULE_ENABLED)
void BSP_SD_AbortCallback(void)
{
    //TODO: Implement if needed
    IOIF_FILESYSTEM_SIGNAL_RW_COMPLETE();
}

/**
  * @brief BSP Tx Transfer completed callback
  * @retval None
  * @note empty (up to the user to fill it in or to remove it if useless)
  */
void BSP_SD_WriteCpltCallback(void)
{
    //TODO: Implement if needed
    IOIF_FILESYSTEM_SIGNAL_RW_COMPLETE();
}

/**
  * @brief BSP Rx Transfer completed callback
  * @retval None
  * @note empty (up to the user to fill it in or to remove it if useless)
  */
void BSP_SD_ReadCpltCallback(void)
{
    //TODO: Implement if needed
    IOIF_FILESYSTEM_SIGNAL_RW_COMPLETE();
}
/* USER CODE END CallBacksSection_C */

/**
 * @brief  Detects if SD card is correctly plugged in the memory slot or not.
 * @param  None
 * @retval Returns if SD is detected or not
 */
__weak uint8_t BSP_SD_IsDetected(void)
{
  __IO uint8_t status = SD_PRESENT;

  /* USER CODE BEGIN IsDetectedSection */
  /* user code can be inserted here */
  /* USER CODE END IsDetectedSection */

  return status;
}
#endif // HAL_SD_MODULE_ENABLED

#endif /* AGRB_IOIF_FILESYSTEM_ENABLE */
