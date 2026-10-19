/**
 ******************************************************************************
 * @file    xm_api_usb_msc.h
 * @brief   XM10 USB 메모리 저장 API — 구조체 하나를 USB 메모리에 주기적으로 저장합니다
 * @details
 * 구조체 하나를 등록해 두면, 저장하는 동안 직접 정한 주기마다 시스템이 그 구조체를 USB 메모리에 기록합니다.
 * 기록 함수를 따로 부르지 않습니다. 저장한 폴더를 PC 로 옮겨 XM10 Log Decoder(2.2.0 이상)로 풀면
 * CSV 가 나오고, 빠진 표본이 있으면 그 위치와 길이를 알려 줍니다.
 *
 * [준비]
 *  - Rev2.0 보드 전용입니다.
 *  - USB 메모리는 FAT32 로 포맷합니다. exFAT · NTFS 는 인식하지 않습니다.
 *  - USB-C 포트는 하나입니다. USB 메모리를 꽂아 둔 동안에는 PhAI Studio · xm10 도구 · USB 로 하는
 *    펌웨어 업로드를 쓸 수 없습니다. 저장이 끝나면 메모리를 뽑고 PC 를 연결하세요.
 *
 * [부르는 곳]
 *  - 설정 · 시작 · 정지 · 마커 · 통계 함수는 Control_Setup() · Control_Loop() 에서만 부릅니다.
 *    백그라운드 태스크(xm_api_freertos.h)나 인터럽트에서 부르면 아무것도 하지 않고 false 를 돌려줍니다.
 *  - 상태 함수(XM_IsUsbLog… · XM_GetUsbLogLastStartError · XM_GetUsbDisk…)는 어디서 불러도 됩니다.
 *
 * [쓰는 순서]
 *  1. Control_Setup() 에서 구조체를 등록하고 주기를 정합니다.
 *       XM_SetUsbLogSource(&s_log, sizeof(s_log));   // s_log 는 전역 또는 static
 *       XM_SetUsbLogSampleRateHz(100);               // 생략하면 1000 Hz
 *  2. Control_Loop() 에서 s_log 의 값을 채웁니다. 저장은 Control_Loop() 가 끝난 뒤 시스템이 합니다.
 *  3. XM_IsUsbLogReady() 와 XM_IsUsbLogSessionIdle() 이 모두 true 일 때 XM_StartUsbDataLog() 로 시작합니다.
 *     false 가 돌아오면 XM_GetUsbLogLastStartError() 로 이유를 봅니다.
 *  4. XM_StopUsbDataLog() 로 멈추고 XM_IsUsbLogSessionIdle() 이 true 가 될 때까지 기다립니다.
 *     true 가 되면 파일이 모두 닫힌 것이므로 USB 메모리를 뽑아도 됩니다.
 *
 * [메타 줄]  구조체의 필드를 선언 순서대로 "이름(타입)" 으로 적고 쉼표로 잇습니다. 디코더는 이 줄로 데이터를 풉니다.
 *     예) "loop_count(uint32_t), hip_L(float), hip_R(float), contact(uint8_t), _pad(3bytes)"
 *  - 타입: uint8_t · int8_t · uint16_t · int16_t · uint32_t · int32_t · float · double · bool (소문자 그대로 — 대소문자를 구분합니다)
 *  - 비워 둔 바이트는 _이름(Nbytes) 로 적습니다(예: _pad(3bytes)). 이름이 _ 로 시작해야 채움이고, pad(3bytes) 는 모르는 타입이라 시작하지 않습니다.
 *  - 이름은 영문 · 숫자 · _ 만 쓰고, "이름(타입)" 안에는 공백을 넣지 않으며, 줄은 한 줄로 적습니다.
 *  - 디코더가 따로 쓰는 이름(tick_ms · t_grid_ms · row_origin · count · h10_loop_count)은 그 뜻이 아니면 필드 이름으로 쓰지 마세요.
 *  - 구조체는 __attribute__((packed)) 로 선언하고 _Static_assert 로 크기를 고정해 두면 실수를 막을 수 있습니다.
 *  - 위 규칙을 어기거나 필드 크기의 합이 등록한 구조체 크기와 다르면 시작하지 않습니다(METADATA_MISMATCH).
 *
 * [주기와 크기]
 *  - 주기는 1000 의 약수입니다: 1000 · 500 · 250 · 200 · 125 · 100 · 50 · 40 · 25 · 20 · 10 · 8 · 5 · 4 · 2 · 1 Hz.
 *  - 1000 Hz 미만은 주기마다 한 번만 저장합니다(그 사이 값은 저장하지 않습니다). 이때는 타임스탬프를 켜 두어야 합니다.
 *  - 주기에 비해 구조체가 크면 시작하지 않습니다(TOO_BIG_FOR_RATE).
 *    예) 1000 Hz 는 구조체 134 B 까지, 200 Hz 이하는 512 B 까지 시작됩니다(타임스탬프를 켠 경우).
 *  - 1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다. 빠진 수와 가장 긴 공백은 XM_GetUsbLogStats() 와
 *    디코더 결과에 나옵니다.
 *
 * [파일]  /LOGS/<세션 이름>/ 폴더에 저장합니다. 이름을 비우면 시계(RTC)로 YYMMDD_HHMMSS 를 만듭니다.
 *         파일은 정한 크기(기본 10 MB)마다 새 파일로 나뉩니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_USB_MSC_H_
#define XM_API_XM_API_USB_MSC_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief 등록할 수 있는 구조체의 최대 크기 [B] */
#define XM_USB_LOG_MAX_STRUCT_SIZE      (512U)

/** @brief 메타 줄의 최대 길이 [글자, 끝의 '\0' 제외] */
#define XM_USB_LOG_MAX_METADATA_LEN     (1700U)

/** @brief 세션 이름의 최대 길이 [글자] */
#define XM_USB_LOG_MAX_SESSION_NAME     (100U)

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief XM_StartUsbDataLog() 가 시작하지 못한 이유 — XM_GetUsbLogLastStartError() 로 읽습니다.
 */
typedef enum {
    XM_USB_LOG_START_OK                = 0,  /**< 실패 기록 없음 */
    XM_USB_LOG_START_NOT_READY         = 1,  /**< USB 메모리가 없거나(시작을 기다리는 중에 빠진 경우 포함) 아직 준비 중이거나 FAT32 가 아님 — XM_IsUsbLogReady() 확인 */
    XM_USB_LOG_START_BUSY              = 2,  /**< 이미 저장 중이거나 시작하는 중 — XM_IsUsbLogSessionIdle() 이 true 일 때 시작 */
    XM_USB_LOG_START_TRY_AGAIN         = 3,  /**< 잠깐 바쁨 — 조금 뒤 다시 시작 */
    XM_USB_LOG_START_NO_SOURCE         = 4,  /**< 저장할 구조체가 없음 — XM_SetUsbLogSource() 를 먼저 */
    XM_USB_LOG_START_BAD_SIZE          = 5,  /**< 구조체 크기가 1 ~ XM_USB_LOG_MAX_STRUCT_SIZE 밖이거나 등록한 크기와 다름 */
    XM_USB_LOG_START_NEEDS_TIMESTAMP   = 6,  /**< 1000 Hz 미만은 타임스탬프가 필요 — XM_SetUsbLogAutoTimestamp(true) */
    XM_USB_LOG_START_TOO_BIG_FOR_RATE  = 7,  /**< 이 주기에 구조체가 너무 큼 — 주기를 낮추거나 구조체를 줄임 */
    XM_USB_LOG_START_METADATA_TOO_LONG = 8,  /**< 메타 줄이 XM_USB_LOG_MAX_METADATA_LEN 보다 김 */
    XM_USB_LOG_START_BAD_NAME          = 9,  /**< 세션 이름이 너무 길거나 쓸 수 없는 문자(< > : " / \ | ? * 등)가 있음, 또는 이름이 비었는데 시계를 읽지 못함 */
    XM_USB_LOG_START_CANCELED          = 10, /**< 시작을 처리하기 전에 XM_StopUsbDataLog() 를 불러 시작을 취소함(그동안 USB 메모리가 빠졌으면 NOT_READY) */
    XM_USB_LOG_START_FOLDER_MISMATCH   = 11, /**< 같은 이름의 폴더가 다른 주기 · 구조체 크기로 이미 있거나 그 폴더의 metadata.txt 를 읽어 확인할 수 없음 — 다른 이름을 쓰거나 PC 에서 그 폴더를 지움 */
    XM_USB_LOG_START_STORAGE           = 12, /**< 빈 공간이 부족하거나 폴더 · 파일을 만들거나 열지 못함 */
    XM_USB_LOG_START_BAD_ARG           = 13, /**< 인자가 잘못됨(메타 줄이 NULL 등) */
    XM_USB_LOG_START_METADATA_MISMATCH = 14  /**< 메타 줄을 읽을 수 없음 — 형식 오류 · 모르는 타입 · 필드 크기의 합이 구조체 크기와 다름 */
} XmUsbLogStartError_e;

/**
 * @brief 저장 통계 — XM_GetUsbLogStats() 로 읽습니다.
 * @note  저장 중에는 지금까지의 값, 세션이 끝나면 그 세션의 마지막 값입니다. 시작을 요청한 뒤 파일이 열릴 때까지는 모두 0 이고,
 *        새 세션이 시작되면 0 부터 다시 셉니다. 시작하지 못하면(파일을 열다 실패 · 취소 · USB 메모리가 빠짐) 앞 세션의 값이 그대로 남습니다.
 * @note  records 는 저장하려고 받은 레코드 수이고 bytes 는 그중 USB 메모리 파일에 쓴 레코드 바이트 수입니다(파일 머리 · 꼬리 · 오류 검사 값 ·
 *        metadata.txt · summary.txt 는 세지 않습니다). 저장 중에는 records 가 bytes 보다 앞서고, USB 메모리가 빠지거나 버퍼가 넘치면
 *        마지막 일부 레코드는 USB 메모리에 없으므로 records 가 실제로 저장된 수보다 클 수 있습니다.
 */
typedef struct {
    uint32_t records;              /**< 저장하려고 받은 레코드 수(마커 포함) */
    uint32_t bytes;                /**< USB 메모리 파일에 쓴 레코드 바이트 수 */
    uint32_t duration_ms;          /**< 세션 길이 [ms] */
    uint32_t empty_slots;          /**< 빠진 표본 수 — 저장 주기 칸 가운데 비어 있는 칸의 수 */
    uint32_t longest_gap_ms;       /**< 가장 긴 공백 [ms] — 연달아 빠진 표본 수 × 주기 */
    uint32_t overflow_records;     /**< 내부 버퍼가 넘쳐 저장하지 못한 레코드 수 — 1 이상이면 그 세션은 거기서 멈춥니다(버퍼가 가득 차기 직전에 멈출 때는 0 일 수 있으니 buffer_peak_percent 가 100 에 가까운지도 보세요) */
    uint32_t write_errors;         /**< USB 메모리 쓰기 실패 횟수 */
    uint8_t  buffer_peak_percent;  /**< 내부 버퍼를 가장 많이 쓴 비율 [%] */
} XmUsbLogStats_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* ======================== 설정 (Control_Setup) ======================== */

/**
 * @brief  저장할 구조체를 등록합니다.
 * @param  data : 구조체 주소 — 전역 또는 static 변수여야 합니다(프로그램이 도는 동안 계속 유효해야 합니다).
 * @param  size : 구조체 크기 [B], 1 ~ XM_USB_LOG_MAX_STRUCT_SIZE
 * @return true: 등록했습니다. false: 저장 세션이 있거나 인자가 잘못됨 — 이전 등록은 그대로입니다.
 */
bool XM_SetUsbLogSource(const void* data, uint32_t size);

/**
 * @brief  저장 주기를 정합니다. 정하지 않으면 1000 Hz 입니다.
 * @param  hz : 1000 의 약수(1000 · 500 · 250 · 200 · 125 · 100 · 50 · 40 · 25 · 20 · 10 · 8 · 5 · 4 · 2 · 1)
 * @return true: 다음 시작부터 적용합니다. false: 저장 세션이 있거나 1000 의 약수가 아님 — 이전 값은 그대로입니다.
 */
bool XM_SetUsbLogSampleRateHz(uint32_t hz);

/**
 * @brief  레코드마다 시각(ms)을 함께 저장할지 정합니다. 기본은 켜짐입니다.
 * @note   1000 Hz 미만으로 저장할 때는 켜 두어야 합니다. 끄면 시작할 때 NEEDS_TIMESTAMP 로 거부됩니다.
 * @return true: 다음 시작부터 적용합니다. false: 저장 세션이 있음 — 이전 값은 그대로입니다.
 */
bool XM_SetUsbLogAutoTimestamp(bool enable);

/**
 * @brief  파일을 나누는 크기를 정합니다. 기본 10 MB 입니다.
 * @param  size_mb : 1 ~ 100 [MB]. USB 메모리의 빈 공간이 이 값보다 커야 시작됩니다.
 * @return true: 다음 시작부터 적용합니다. false: 저장 세션이 있거나 범위 밖 — 이전 값은 그대로입니다.
 */
bool XM_SetUsbLogRollingSize(uint32_t size_mb);

/* ========================== 상태 (어디서나) ========================== */

/** @brief USB 메모리가 꽂혀 있고 파일을 쓸 준비가 되었는지(FAT32). */
bool XM_IsUsbLogReady(void);

/**
 * @brief  저장 세션이 하나도 없는지 — 저장 중도, 시작하는 중도, 파일을 닫는 중도 아닙니다.
 * @details true 일 때 새로 시작하거나 설정을 바꿀 수 있고, USB 메모리를 뽑아도 됩니다.
 *          저장 중에 이 값이 스스로 true 가 되면 세션이 끝난 것입니다(USB 메모리가 빠짐 · 버퍼가 넘침 등).
 *          Control_Setup() · Control_Loop() 밖에서 읽은 값은 참고용입니다 — 시작이나 설정 변경은 이 값으로 판단하지 않습니다.
 */
bool XM_IsUsbLogSessionIdle(void);

/** @brief 지금 파일에 기록하고 있는지. 시작을 요청한 뒤 파일이 열리면 true 가 됩니다. */
bool XM_IsUsbLogRecording(void);

/**
 * @brief  마지막 XM_StartUsbDataLog() 의 결과입니다.
 * @details 시작 요청이 바로 거부된 이유, 또는 받아들여진 뒤 파일을 여는 중에 실패한 이유가 남습니다.
 *          받아들여진 시작의 결과는 XM_IsUsbLogRecording() 이 true 가 되거나 XM_IsUsbLogSessionIdle() 이 다시 true 가 된 뒤,
 *          XM_StartUsbDataLog() 를 다시 부르기 전에 읽습니다.
 */
XmUsbLogStartError_e XM_GetUsbLogLastStartError(void);

/** @brief USB 메모리의 빈 공간 [MB]. 약 10 초마다 갱신하며, 꽂은 직후에는 잠깐 0 입니다. */
uint32_t XM_GetUsbDiskFreeMB(void);

/** @brief USB 메모리의 전체 크기 [MB]. 꽂은 직후에는 잠깐 0 입니다. */
uint32_t XM_GetUsbDiskTotalMB(void);

/* ========================= 제어 (Control_Loop) ========================= */

/**
 * @brief  저장을 시작합니다. 기다리지 않고 바로 돌아옵니다.
 * @param  session_name : 세션 폴더 이름(XM_USB_LOG_MAX_SESSION_NAME 자 이하). NULL 이나 "" 이면 시계로 YYMMDD_HHMMSS 를 만듭니다.
 *                        같은 이름의 폴더가 있으면 거기에 이어서 저장합니다(주기 · 구조체 크기 · 타임스탬프 설정이 같아야 합니다.
 *                        폴더의 메타 줄은 처음 저장할 때의 것을 그대로 쓰고 새 메타 줄과 비교하지 않으니, 필드 이름이나 타입을 바꿨다면 다른 이름을 쓰세요).
 * @param  metadata     : 메타 줄(파일 머리의 [메타 줄])
 * @return true: 시작 요청을 받아들였습니다 — 파일이 열리면 XM_IsUsbLogRecording() 이 true 가 됩니다.
 *               파일을 열다가 실패하면 XM_IsUsbLogSessionIdle() 이 다시 true 가 되고 이유가 XM_GetUsbLogLastStartError() 에 남습니다.
 *         false: 시작하지 않았습니다 — 이유는 XM_GetUsbLogLastStartError(). (Control_Setup() · Control_Loop() 밖에서 부르면 이유를 남기지 않고 false 입니다.)
 * @note   XM_IsUsbLogReady() 와 XM_IsUsbLogSessionIdle() 이 모두 true 일 때 부릅니다.
 */
bool XM_StartUsbDataLog(const char* session_name, const char* metadata);

/**
 * @brief  저장을 멈춥니다. 기다리지 않고 바로 돌아오며, 파일은 뒤에서 닫습니다.
 * @return true: 멈춤을 요청했거나 멈출 세션이 없습니다. false: 잠깐 바쁨 — 조금 뒤 다시 부릅니다.
 * @note   XM_IsUsbLogSessionIdle() 이 true 가 될 때까지 기다립니다. 기다리는 동안 다시 불러도 됩니다.
 *         시작을 요청한 직후, 시스템이 그 시작을 처리하기 전에 부르면 시작을 취소합니다(CANCELED, 파일을 만들지 않음).
 *         이미 파일을 만드는 중이었다면 만든 뒤 곧바로 닫으므로 기록이 없는 세션 폴더가 남습니다(이때 이유는 OK).
 */
bool XM_StopUsbDataLog(void);

/**
 * @brief  저장 중에 표시(마커)를 하나 넣습니다. 디코더가 표시를 따로 모아(events.csv) 보여 줍니다.
 * @param  type  : 표시 종류 1 ~ 254 — 뜻은 사용자가 정합니다(예: 1 = 실험 시작, 2 = 실험 끝).
 * @param  value : 함께 남길 값
 * @return true: 넣었습니다. false: 저장 중이 아니거나 type 이 1 ~ 254 밖이거나 버퍼에 자리가 없습니다.
 */
bool XM_InsertUsbLogMarker(uint8_t type, uint16_t value);

/**
 * @brief  저장 통계를 복사합니다.
 * @param  out : 통계를 받을 구조체
 * @return true: 복사했습니다. false: out 이 NULL 이거나 Control_Setup() · Control_Loop() 밖에서 불렀습니다.
 */
bool XM_GetUsbLogStats(XmUsbLogStats_t* out);

#ifdef __cplusplus
}
#endif

#endif /* XM_API_XM_API_USB_MSC_H_ */
