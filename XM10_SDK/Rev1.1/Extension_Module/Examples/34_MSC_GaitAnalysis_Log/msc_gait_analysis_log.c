/**
 ******************************************************************************
 * @file    msc_gait_analysis_log.c
 * @author  HyundoKim
 * @brief   [Application] H10 SUIT 동작분석 데이터 USB-MSC 로깅
 * @details
 * H10 CM에서 PDO로 수신한 보행 분석 데이터를 USB 메모리에 저장합니다.
 * 저장된 바이너리를 Python 디코더로 변환하면, MATLAB GaitAnalysis_RT/PP에서
 * 바로 분석할 수 있는 CSV가 생성됩니다.
 *
 * [데이터 파이프라인]
 *   H10 CM ──PDO──> XM10 ──USB-MSC──> .bin
 *     > Python data_decoder_xm10_v2.py ──> decoded_output.csv
 *       > MATLAB load_decoded_data() ──> GaitAnalysis_RT ──> GaitAnalysis_PP
 *
 * [저장 데이터 — 36채널]
 *   MATLAB RT 필수 (10ch): count, pelvic, thigh L/R, torque L/R,
 *                           velocity, foot contact L/R, post_processing_cnt
 *   Extended (15ch):        hip/knee/motor angle, IMU 6DOF×2, FSM state
 *   Ext IMU (11ch):         Xsens MTi-630 quat 4 + acc 3 + gyr 3 + connected 1
 *
 * [메타데이터 필드명]
 *   CSV 컬럼명이 MATLAB 변수명과 1:1 매칭되도록 설계하였습니다.
 *   load_decoded_data.m이 CSV 컬럼을 workspace 변수로 직접 생성하므로,
 *   GaitAnalysis_RT.m에서 추가 변환 없이 바로 사용 가능합니다.
 *
 * [토크 단위 — 중요]
 *   XM API의 leftHipTorque/rightHipTorque는 모터 전류(A)입니다.
 *   MATLAB에서 관절 토크로 변환: torque[Nm] = current[A] × 0.085 × 18.75
 *   이 예제에서는 Nm 변환 후 저장하여 MATLAB에서 추가 변환이 불필요합니다.
 *
 * [H10 동작 모드]
 *   XM10은 제어를 수행하지 않으며, H10의 기존 보조 알고리즘을 그대로 사용합니다.
 *   XM_SetH10AssistExistingMode(true) 호출로 H10 Existing Mode를 활성화합니다.
 *   (default: XM10 연결 시 H10 보조 알고리즘 비활성화 → true로 재활성화 필요)
 *
 * [자동 트리거]
 *   H10 Assist 모드 진입 → 로깅 자동 시작 (USB 메모리 연결 필요)
 *   H10 Standby 모드 복귀 → 로깅 자동 정지
 *
 * [LED 피드백]
 *   LED1 OFF   : 대기 (STANDBY)
 *   LED1 BLINK : 로깅 중 (LOGGING)
 *   LED2 BLINK : 경고 — 버퍼 90%+ (쓰기 지연)
 *   LED2 SOLID : 경고 — 디스크 잔여 50MB 미만
 *   LED3 SOLID : 에러 — 로깅 강제 중단
 *
 * @version 1.0
 * @date    Mar 18, 2026
 * @see     GaitAnalysis_Rulebase/MATLAB/GaitAnalysis_RT.m
 * @see     GaitAnalysis_Rulebase/Python_Decoder/data_decoder_xm10_v2.py
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include "stm32h7xx_hal.h"      /* HAL_FDCAN_GetErrorCounters/ProtocolStatus/RxFifoFillLevel */
#include "ioif_agrb_dwt.h"      /* IOIF_DWT_GetCycles/CyclesToUs */
#include "data_logger.h"        /* DataLogger_GetLastFsyncUs */
#include "diag_perf.h"          /* [Step 0 2026-04-18] UserTask jitter histogram */
#include <string.h>

extern FDCAN_HandleTypeDef hfdcan1;

/* ===================================================================
 * CONSTANTS
 * =================================================================== */

/** PhAI X1 SAM10 Actuator: Kt × Gear Ratio = 0.085 × 18.75 */
#define TORQUE_SCALE    (0.085f * 18.75f)

/** 파일 롤링 크기 (MB) — 10MB마다 분할 */
#define ROLLING_SIZE_MB     10

/** [Option A Redesign 2026-04-17] Emergency Stop 후 재시도 cooldown (USB GC 회복) */
#define RESTART_COOLDOWN_MS         3000
/** 세션당 최대 재시도 횟수 — 초과 시 h10 Standby까지 재진입 차단 (폭주 방지) */
#define MAX_RESTART_ATTEMPTS        3
/** 세션 이름 버퍼 크기 (RTC 형식 "YYMMDD_HHMMSS" 또는 tick 기반 "XM_NNNNNNNN") */
#define SESSION_NAME_BUF_SIZE       20

/* ===================================================================
 * LOG STRUCT — MATLAB 변수명과 1:1 매칭
 * =================================================================== */

/**
 * @brief 동작분석 로깅 패킷
 * @details
 * 필드명이 Python 디코더의 metadata → CSV 컬럼명 → MATLAB 변수명으로
 * 그대로 전달됩니다. 변수명 변경 시 metadata 문자열도 반드시 동기화하세요.
 *
 * auto_timestamp = OFF (수동 count 필드로 대체)
 * 패킷 크기: 159 bytes
 */
typedef struct __attribute__((packed)) {
    /* --- MATLAB RT 필수 채널 (10ch) --- */
    uint32_t count;                     /**< CM assist loop counter (시간 기준) */
    float    theta_trunk_improved;      /**< 골반/체간 각도 [deg] */
    float    thigh_angle_lh;            /**< 좌측 대퇴 절대각 [deg] */
    float    thigh_angle_rh;            /**< 우측 대퇴 절대각 [deg] */
    float    LeftHipTorque;             /**< 좌측 관절 토크 [Nm] (Kt×GR 변환 후) */
    float    RightHipTorque;            /**< 우측 관절 토크 [Nm] (Kt×GR 변환 후) */
    float    velX;                      /**< 전방 보행 속도 [m/s] */
    uint8_t  LeftFootContact;           /**< 좌측 지면 접촉 [0/1] */
    uint8_t  RightFootContact;          /**< 우측 지면 접촉 [0/1] */
    uint16_t post_processing_cnt;       /**< 동작분석 구간 카운터 (0~30000) */

    /* --- Extended: 관절 각도 (6ch) --- */
    float    left_hip_angle;            /**< 좌측 고관절 엔코더 [deg] */
    float    right_hip_angle;           /**< 우측 고관절 엔코더 [deg] */
    float    left_knee_angle;           /**< 좌측 무릎 추정각 [deg] */
    float    right_knee_angle;          /**< 우측 무릎 추정각 [deg] */
    float    left_motor_angle;          /**< 좌측 모터 엔코더 [deg] */
    float    right_motor_angle;         /**< 우측 모터 엔코더 [deg] */

    /* --- Extended: IMU 좌측 고관절 6DOF (6ch) --- */
    float    imu_acc_x_global_lh;       /**< 좌측 IMU 가속도 X [m/s^2] */
    float    imu_acc_y_global_lh;       /**< 좌측 IMU 가속도 Y [m/s^2] */
    float    imu_acc_z_global_lh;       /**< 좌측 IMU 가속도 Z [m/s^2] */
    float    imu_gyr_x_global_lh;      /**< 좌측 IMU 자이로 X [deg/s] */
    float    imu_gyr_y_global_lh;      /**< 좌측 IMU 자이로 Y [deg/s] */
    float    imu_gyr_z_global_lh;      /**< 좌측 IMU 자이로 Z [deg/s] */

    /* --- Extended: IMU 우측 고관절 6DOF (6ch) --- */
    float    imu_acc_x_global_rh;       /**< 우측 IMU 가속도 X [m/s^2] */
    float    imu_acc_y_global_rh;       /**< 우측 IMU 가속도 Y [m/s^2] */
    float    imu_acc_z_global_rh;       /**< 우측 IMU 가속도 Z [m/s^2] */
    float    imu_gyr_x_global_rh;      /**< 우측 IMU 자이로 X [deg/s] */
    float    imu_gyr_y_global_rh;      /**< 우측 IMU 자이로 Y [deg/s] */
    float    imu_gyr_z_global_rh;      /**< 우측 IMU 자이로 Z [deg/s] */

    /* --- External IMU: Xsens MTi-630 (USART2, 11ch) --- */
    uint8_t  ext_imu_connected;         /**< Xsens 연결 상태 [0/1] */
    float    ext_imu_q_w;               /**< Quaternion W (orientation) */
    float    ext_imu_q_x;               /**< Quaternion X */
    float    ext_imu_q_y;               /**< Quaternion Y */
    float    ext_imu_q_z;               /**< Quaternion Z */
    float    ext_imu_acc_x;             /**< Calibrated Acc X [m/s^2] */
    float    ext_imu_acc_y;             /**< Calibrated Acc Y [m/s^2] */
    float    ext_imu_acc_z;             /**< Calibrated Acc Z [m/s^2] */
    float    ext_imu_gyr_x;             /**< Calibrated Gyr X [deg/s] */
    float    ext_imu_gyr_y;             /**< Calibrated Gyr Y [deg/s] */
    float    ext_imu_gyr_z;             /**< Calibrated Gyr Z [deg/s] */

    /* --- Extended: 상태 (1ch) --- */
    uint8_t  fsm_current_state;         /**< CM FSM 현재 상태 */

    /* --- FDCAN Diagnostics (5ch) --- */
    uint8_t  fdcan_rec;                 /**< Receive Error Counter (0~255) */
    uint8_t  fdcan_tec;                 /**< Transmit Error Counter (0~255) */
    uint8_t  fdcan_lec;                 /**< Last Error Code (0=None~7) */
    uint8_t  fdcan_rx_fifo0_fill;       /**< Rx FIFO0 Fill Level (0~37) */
    uint8_t  fdcan_bus_status;          /**< bit0:BusOff bit1:Warn bit2:ErrPassive */

    /* --- Timing Diagnostics (4B) --- */
    uint16_t usertask_jitter_us;        /**< UserTask 주기 편차 [µs] (nominal 1000) */
    uint16_t fsync_last_us;             /**< 직전 f_sync 소요 시간 [µs] */

    /* --- H10 Counter (4B) --- */
    uint32_t h10_loop_count;            /**< H10 assist loop counter (PDO gap 검출용) */
} GaitAnalysisLog_t;    /* 159 bytes (118 + Ext IMU 41) */

_Static_assert(sizeof(GaitAnalysisLog_t) == 159,
               "GaitAnalysisLog_t size mismatch — metadata 동기화 필요");

/* ===================================================================
 * METADATA — Python 디코더가 읽는 필드 정의 문자열
 * =================================================================== */

/**
 * @brief metadata.txt에 기록될 필드 정의
 * @details 순서와 타입이 GaitAnalysisLog_t 구조체와 정확히 일치해야 합니다.
 *          Python 디코더의 build_struct_format()이 이 문자열을 파싱하여
 *          struct.unpack 포맷을 생성합니다.
 */
#define GAIT_LOG_METADATA \
    "count(uint32_t), "                     \
    "theta_trunk_improved(float), "         \
    "thigh_angle_lh(float), "               \
    "thigh_angle_rh(float), "               \
    "LeftHipTorque(float), "                \
    "RightHipTorque(float), "               \
    "velX(float), "                         \
    "LeftFootContact(uint8_t), "            \
    "RightFootContact(uint8_t), "           \
    "post_processing_cnt(uint16_t), "       \
    "left_hip_angle(float), "               \
    "right_hip_angle(float), "              \
    "left_knee_angle(float), "              \
    "right_knee_angle(float), "             \
    "left_motor_angle(float), "             \
    "right_motor_angle(float), "            \
    "imu_acc_x_global_lh(float), "          \
    "imu_acc_y_global_lh(float), "          \
    "imu_acc_z_global_lh(float), "          \
    "imu_gyr_x_global_lh(float), "          \
    "imu_gyr_y_global_lh(float), "          \
    "imu_gyr_z_global_lh(float), "          \
    "imu_acc_x_global_rh(float), "          \
    "imu_acc_y_global_rh(float), "          \
    "imu_acc_z_global_rh(float), "          \
    "imu_gyr_x_global_rh(float), "          \
    "imu_gyr_y_global_rh(float), "          \
    "imu_gyr_z_global_rh(float), "          \
    "ext_imu_connected(uint8_t), "          \
    "ext_imu_q_w(float), "                  \
    "ext_imu_q_x(float), "                  \
    "ext_imu_q_y(float), "                  \
    "ext_imu_q_z(float), "                  \
    "ext_imu_acc_x(float), "                \
    "ext_imu_acc_y(float), "                \
    "ext_imu_acc_z(float), "                \
    "ext_imu_gyr_x(float), "                \
    "ext_imu_gyr_y(float), "                \
    "ext_imu_gyr_z(float), "                \
    "fsm_current_state(uint8_t), "          \
    "fdcan_rec(uint8_t), "                  \
    "fdcan_tec(uint8_t), "                  \
    "fdcan_lec(uint8_t), "                  \
    "fdcan_rx_fifo0_fill(uint8_t), "        \
    "fdcan_bus_status(uint8_t), "           \
    "usertask_jitter_us(uint16_t), "        \
    "fsync_last_us(uint16_t), "             \
    "h10_loop_count(uint32_t)"

/* ===================================================================
 * STATIC VARIABLES
 * =================================================================== */

static GaitAnalysisLog_t s_log;
static XmTsmHandle_t     s_tsm;
static uint32_t          s_log_loop_count = 0;       /**< 로깅 세션 자체 카운터 (0부터 시작) */

/* [Option A Redesign 2026-04-17] 세션 지속성 — H10 Assist → Standby 사이 한 폴더 유지.
 * 이전 버그: XM_GetActiveUsbSessionName를 StartUsbDataLog 직후 호출 → DataLoggerTask가
 *           아직 세션 이름 업데이트 전이라 race → 빈 문자열/stale 반환 → 의도대로 재사용 실패.
 * 해결:    예제가 RTC/tick 기반으로 이름 '직접' 생성 → FW에 동기 전달 → race 없음.
 */
static char     s_active_session_name[SESSION_NAME_BUF_SIZE] = {0};
static uint32_t s_restart_cooldown_tick = 0;   /**< 0 = 해제, >0 = 마지막 Emergency Stop tick */
static uint8_t  s_restart_attempts = 0;        /**< 이번 H10 Assist 사이클 내 재시도 횟수 */
static bool     s_error_latched = false;       /**< MAX_RESTART_ATTEMPTS 초과 → Standby까지 재진입 차단 */

/**
 * @brief 세션 이름 생성 — RTC 기반 "YYMMDD_HHMMSS" 또는 fallback tick.
 * @note  예제 내부에서 결정 → race-free. FW의 _GenerateRtcSessionName과 같은 형식.
 */
static void _GenerateSessionName(char* buf, uint32_t buf_size)
{
    XmDateTime_t rtc = {0};
    XM_RTC_GetDateTime(&rtc);
    if (rtc.year >= 2020 && rtc.year <= 2099) {
        snprintf(buf, buf_size, "%02u%02u%02u_%02u%02u%02u",
                 (unsigned)(rtc.year % 100), (unsigned)rtc.month, (unsigned)rtc.day,
                 (unsigned)rtc.hour, (unsigned)rtc.minute, (unsigned)rtc.second);
    } else {
        /* RTC 미설정 fallback — 부팅 tick 기반 (재부팅 충돌 가능하나 개발용 OK) */
        snprintf(buf, buf_size, "XM_%08lu", (unsigned long)osKernelGetTickCount());
    }
}

/* ===================================================================
 * STATIC FUNCTION PROTOTYPES
 * =================================================================== */

static void _Standby_Loop(void);
static void _Logging_Entry(void);
static void _Logging_Loop(void);
static void _Logging_Exit(void);
static void _UpdateLogData(void);

/* ===================================================================
 * PUBLIC FUNCTIONS — User Task Entry Points
 * =================================================================== */

/**
 * @brief 초기 설정 (1회 호출)
 */
void Control_Setup(void)
{
    /* TSM: STANDBY ↔ ACTIVE */
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);

    XmStateConfig_t standby = {
        .id      = XM_STATE_STANDBY,
        .on_loop = _Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &standby);

    XmStateConfig_t logging = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = _Logging_Entry,
        .on_loop  = _Logging_Loop,
        .on_exit  = _Logging_Exit
    };
    XM_TSM_AddState(s_tsm, &logging);

    /* H10 Existing Mode 활성화 — 모니터링 전용
     * XM10 연결 시 H10 보조 알고리즘이 기본 비활성화되므로,
     * 데이터 수집 목적일 때 반드시 true로 재활성화해야 합니다.
     * XM10은 제어 출력 없이 PDO 데이터만 수집합니다. */
    XM_SetH10AssistExistingMode(true);

    /* External IMU (Xsens MTi-630) 결합 — USART2(PD5/PD6).
     * 케이블 미연결 상태에서도 호출 안전. 결합 시 XM.status.ext_imu.* 자동 갱신.
     * 센서 EEPROM이 1kHz Quat+Acc+Gyro 설정을 보존하면 추가 호출 불필요.
     * 신품 센서면 XM_ConfigureXsensMTi630() 1회 호출 (블로킹 ~1초). */
    XM_AttachXsensMTi630();

    /* 로그 소스 등록 (auto_timestamp OFF — count 필드로 대체) */
    XM_SetUsbLogSource(&s_log, sizeof(GaitAnalysisLog_t));
    XM_SetUsbLogAutoTimestamp(false);
    XM_SetUsbLogRollingSize(ROLLING_SIZE_MB);
}

/**
 * @brief 주기 루프 (1ms)
 */
void Control_Loop(void)
{
    XM_TSM_Run(s_tsm);
    XM_IO_Update();
}

/* ===================================================================
 * STATIC FUNCTIONS
 * =================================================================== */

/**
 * @brief STANDBY 상태 — H10 Assist 진입 시 자동 시작.
 * @details [Option A Redesign] Emergency Stop 후 같은 폴더 재진입 + cooldown + 재시도 한도.
 *          H10 Standby 복귀 시에만 세션 상태 전체 리셋 → 다음 Assist는 새 폴더.
 */
static void _Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        /* Error latched: 최대 재시도 초과 → H10 Standby 복귀까지 재진입 차단 */
        if (s_error_latched) {
            return;
        }

        /* Cooldown 중: USB GC/초기화 안정 시간 확보 — 매 1ms 호출이지만 함수는 빠름 */
        if (s_restart_cooldown_tick != 0) {
            uint32_t elapsed = osKernelGetTickCount() - s_restart_cooldown_tick;
            if (elapsed < RESTART_COOLDOWN_MS) {
                return;
            }
            s_restart_cooldown_tick = 0;
        }

        if (XM_IsUsbLogReady()) {
            XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
        } else {
            XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 200);
        }
    } else {
        /* H10 Standby = 한 번의 녹화 완전 종료 → 모든 세션 상태 리셋 */
        if (s_active_session_name[0] != '\0') { s_active_session_name[0] = '\0'; }
        s_restart_cooldown_tick = 0;
        s_restart_attempts = 0;
        s_error_latched = false;
    }
}

/**
 * @brief LOGGING 진입 — 세션 시작.
 * @details [Option A Redesign] 최초 진입은 예제가 직접 RTC/tick 기반 이름 생성 (race-free).
 *          재진입(Emergency Stop 후 cooldown 복귀)은 보관한 이름 재전달 →
 *          FW의 session_index 이진 탐색이 같은 폴더에 data_001_*, data_002_* 로 증분.
 *          FW에 metadata.txt "이미 있으면 스킵" 로직 추가되어 FAT wear hotspot 방지.
 */
static void _Logging_Entry(void)
{
    const bool is_reentry = (s_active_session_name[0] != '\0');

    if (!is_reentry) {
        /* 최초 진입: 예제가 이름 결정 → XM_StartUsbDataLog에 동기 전달 */
        _GenerateSessionName(s_active_session_name, sizeof(s_active_session_name));
        s_restart_attempts = 0;
    } else {
        /* 재진입: 횟수 카운트 */
        s_restart_attempts++;
    }

    bool ok = XM_StartUsbDataLog(s_active_session_name, GAIT_LOG_METADATA);

    if (ok) {
        memset(&s_log, 0, sizeof(s_log));
        s_log_loop_count = 0;
        /* [Step 0 2026-04-18] Histogram 리셋 — 세션 시작 시점 */
        DiagPerf_Reset();
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
        /* marker_type 1 = START, marker_data = 재진입 횟수 (0=최초, 1+=재진입 인덱스) */
        XM_InsertUsbLogMarker(1, (uint16_t)s_restart_attempts);
    } else {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief LOGGING 루프 — 데이터 갱신 + 상태 모니터링
 */
static void _Logging_Loop(void)
{
    /* 1. 데이터 갱신 */
    _UpdateLogData();

    /* 2. 로깅 상태 모니터링 */
    XmLogStatus_e status = XM_GetUsbLogStatus();

    switch (status) {
    case XM_LOG_STATUS_WARNING_QUEUE_FULL:
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 200);
        break;
    case XM_LOG_STATUS_WARNING_DISK_LOW:
        XM_SetLedEffect(XM_LED_2, XM_LED_SOLID, 0);
        break;
    case XM_LOG_STATUS_ERROR_STOPPED:
        XM_SetLedEffect(XM_LED_3, XM_LED_SOLID, 0);
        /* [Option A Redesign] cooldown 시작 + 재시도 한도 체크 */
        if (s_restart_attempts >= (MAX_RESTART_ATTEMPTS - 1)) {
            /* 한도 도달 → 완전 latched, H10 Standby 복귀 전까지 재진입 차단 */
            s_error_latched = true;
        } else {
            s_restart_cooldown_tick = osKernelGetTickCount();
            if (s_restart_cooldown_tick == 0) { s_restart_cooldown_tick = 1; }
        }
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    default:
        XM_SetLedEffect(XM_LED_2, XM_LED_OFF, 0);
        break;
    }

    /* 3. H10 Standby 복귀 시 자동 정지 */
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_InsertUsbLogMarker(2, 0);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/**
 * @brief LOGGING 종료 — 세션 닫기 + LED 초기화
 */
static void _Logging_Exit(void)
{
    if (XM_GetUsbLogStatus() != XM_LOG_STATUS_IDLE) {
        XM_StopUsbDataLog();
    }
    XM_SetLedEffect(XM_LED_1, XM_LED_OFF, 0);
    XM_SetLedEffect(XM_LED_2, XM_LED_OFF, 0);
    XM_SetLedEffect(XM_LED_3, XM_LED_OFF, 0);
}

/**
 * @brief 로그 패킷 데이터 갱신
 * @details
 * XM API에서 H10 PDO 데이터를 읽어 로그 구조체에 채웁니다.
 *
 * [토크 변환]
 *   XM.status.h10.leftHipTorque  = 모터 전류 [A]
 *   저장값 = 전류 × TORQUE_SCALE = 관절 토크 [Nm]
 *   MATLAB에서 추가 변환 없이 직접 사용 가능
 */
static void _UpdateLogData(void)
{
    const XmH10Data_t *h10 = &XM.status.h10;

    /* --- MATLAB RT 필수 (10ch) --- */
    s_log.count                 = s_log_loop_count++;
    s_log.theta_trunk_improved  = h10->pelvicAngle;
    s_log.thigh_angle_lh        = h10->leftThighAngle;
    s_log.thigh_angle_rh        = h10->rightThighAngle;
    s_log.LeftHipTorque         = h10->leftHipTorque  * TORQUE_SCALE;
    s_log.RightHipTorque        = h10->rightHipTorque * TORQUE_SCALE;
    s_log.velX                  = h10->forwardVelocity;
    s_log.LeftFootContact       = h10->isLeftFootContact  ? 1 : 0;
    s_log.RightFootContact      = h10->isRightFootContact ? 1 : 0;
    s_log.post_processing_cnt   = (uint16_t)h10->h10PostProcessingCnt;

    /* --- Extended: 관절 각도 (6ch) --- */
    s_log.left_hip_angle        = h10->leftHipAngle;
    s_log.right_hip_angle       = h10->rightHipAngle;
    s_log.left_knee_angle       = h10->leftKneeAngle;
    s_log.right_knee_angle      = h10->rightKneeAngle;
    s_log.left_motor_angle      = h10->leftHipMotorAngle;
    s_log.right_motor_angle     = h10->rightHipMotorAngle;

    /* --- Extended: IMU 좌측 고관절 (6ch) --- */
    s_log.imu_acc_x_global_lh   = h10->leftHipImuGlobalAccX;
    s_log.imu_acc_y_global_lh   = h10->leftHipImuGlobalAccY;
    s_log.imu_acc_z_global_lh   = h10->leftHipImuGlobalAccZ;
    s_log.imu_gyr_x_global_lh   = h10->leftHipImuGlobalGyrX;
    s_log.imu_gyr_y_global_lh   = h10->leftHipImuGlobalGyrY;
    s_log.imu_gyr_z_global_lh   = h10->leftHipImuGlobalGyrZ;

    /* --- Extended: IMU 우측 고관절 (6ch) --- */
    s_log.imu_acc_x_global_rh   = h10->rightHipImuGlobalAccX;
    s_log.imu_acc_y_global_rh   = h10->rightHipImuGlobalAccY;
    s_log.imu_acc_z_global_rh   = h10->rightHipImuGlobalAccZ;
    s_log.imu_gyr_x_global_rh   = h10->rightHipImuGlobalGyrX;
    s_log.imu_gyr_y_global_rh   = h10->rightHipImuGlobalGyrY;
    s_log.imu_gyr_z_global_rh   = h10->rightHipImuGlobalGyrZ;

    /* --- External IMU (Xsens MTi-630, 11ch) ---
     * 미연결/미Attach 시 is_connected=false → quat/acc/gyr는 직전 값 유지(0 초기화).
     * Auto-Sense가 케이블 결합 시 자동 OPERATIONAL 전환. */
    const XmExtImuData_t *ext = &XM.status.ext_imu;
    s_log.ext_imu_connected = ext->is_connected ? 1 : 0;
    s_log.ext_imu_q_w   = ext->q_w;
    s_log.ext_imu_q_x   = ext->q_x;
    s_log.ext_imu_q_y   = ext->q_y;
    s_log.ext_imu_q_z   = ext->q_z;
    s_log.ext_imu_acc_x = ext->acc_x;
    s_log.ext_imu_acc_y = ext->acc_y;
    s_log.ext_imu_acc_z = ext->acc_z;
    s_log.ext_imu_gyr_x = ext->gyr_x;
    s_log.ext_imu_gyr_y = ext->gyr_y;
    s_log.ext_imu_gyr_z = ext->gyr_z;

    /* --- Extended: 상태 --- */
    s_log.fsm_current_state     = h10->h10FSMcurrentState;

    /* --- FDCAN Diagnostics (5ch) --- */
    FDCAN_ErrorCountersTypeDef err_cnt;
    FDCAN_ProtocolStatusTypeDef proto_status;
    if (HAL_FDCAN_GetErrorCounters(&hfdcan1, &err_cnt) == HAL_OK) {
        s_log.fdcan_rec = (uint8_t)err_cnt.RxErrorCnt;
        s_log.fdcan_tec = (uint8_t)err_cnt.TxErrorCnt;
    }
    if (HAL_FDCAN_GetProtocolStatus(&hfdcan1, &proto_status) == HAL_OK) {
        s_log.fdcan_lec = (uint8_t)proto_status.LastErrorCode;
        s_log.fdcan_bus_status =
            ((proto_status.BusOff)       ? 0x01 : 0) |
            ((proto_status.Warning)      ? 0x02 : 0) |
            ((proto_status.ErrorPassive) ? 0x04 : 0);
    }
    s_log.fdcan_rx_fifo0_fill = (uint8_t)HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0);

    /* --- Timing Diagnostics (4B) --- */
    static uint32_t s_last_dwt = 0;
    uint32_t dwt_now = IOIF_DWT_GetCycles();
    if (s_last_dwt != 0) {
        uint32_t delta_us = IOIF_DWT_CyclesToUs(dwt_now - s_last_dwt);
        int32_t jitter = (int32_t)delta_us - 1000;   /* nominal 1000µs */
        if (jitter < 0) jitter = -jitter;
        s_log.usertask_jitter_us = (jitter > 65535) ? 65535 : (uint16_t)jitter;
        /* [Step 0 2026-04-18] DTCM histogram 기록 — 첫 500 샘플 자동 제외 */
        DiagPerf_RecordJitter((uint32_t)jitter);
    } else {
        s_log.usertask_jitter_us = 0;
    }
    s_last_dwt = dwt_now;
    uint32_t fsync_us = DataLogger_GetLastFsyncUs();
    s_log.fsync_last_us = (fsync_us > 65535) ? 65535 : (uint16_t)fsync_us;

    /* --- H10 Counter (4B) --- */
    s_log.h10_loop_count        = h10->h10AssistModeLoopCnt;
}
