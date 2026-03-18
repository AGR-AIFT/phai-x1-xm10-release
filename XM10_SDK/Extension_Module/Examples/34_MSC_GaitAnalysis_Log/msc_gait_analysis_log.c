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
 * [저장 데이터 — 25채널]
 *   MATLAB RT 필수 (10ch): count, pelvic, thigh L/R, torque L/R,
 *                           velocity, foot contact L/R, post_processing_cnt
 *   Extended (15ch):        hip/knee/motor angle, IMU 6DOF×2, FSM state
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
 * [조작 방법]
 *   BTN1 클릭  : 로깅 시작 (USB 메모리 연결 필요)
 *   BTN2 클릭  : 로깅 정지
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
#include <string.h>

/* ===================================================================
 * CONSTANTS
 * =================================================================== */

/** PhAI X1 SAM10 Actuator: Kt × Gear Ratio = 0.085 × 18.75 */
#define TORQUE_SCALE    (0.085f * 18.75f)

/** 파일 롤링 크기 (MB) — 10MB마다 분할 */
#define ROLLING_SIZE_MB     10

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
 * 패킷 크기: 108 bytes
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

    /* --- Extended: 상태 (1ch + padding) --- */
    uint8_t  fsm_current_state;         /**< CM FSM 현재 상태 */
    uint8_t  _pad[3];                   /**< 4-byte 정렬 패딩 */
} GaitAnalysisLog_t;    /* 108 bytes */

_Static_assert(sizeof(GaitAnalysisLog_t) == 108,
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
    "fsm_current_state(uint8_t), "          \
    "_pad(3bytes)"

/* ===================================================================
 * STATIC VARIABLES
 * =================================================================== */

static GaitAnalysisLog_t s_log;
static XmTsmHandle_t     s_tsm;
static uint32_t          s_session_counter = 0;
static char              s_session_name[32];

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
void User_Setup(void)
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

    /* 로그 소스 등록 (auto_timestamp OFF — count 필드로 대체) */
    XM_SetUsbLogSource(&s_log, sizeof(GaitAnalysisLog_t));
    XM_SetUsbLogAutoTimestamp(false);
    XM_SetUsbLogRollingSize(ROLLING_SIZE_MB);
}

/**
 * @brief 주기 루프 (2ms)
 */
void User_Loop(void)
{
    XM_TSM_Run(s_tsm);
    XM_IO_Update();
}

/* ===================================================================
 * STATIC FUNCTIONS
 * =================================================================== */

/**
 * @brief STANDBY 상태 — USB 연결 대기, BTN1으로 시작
 */
static void _Standby_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogReady()) {
            XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
        } else {
            /* USB 미연결 경고 (LED3 하트비트) */
            XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 200);
        }
    }
}

/**
 * @brief LOGGING 진입 — 세션 시작
 */
static void _Logging_Entry(void)
{
    snprintf(s_session_name, sizeof(s_session_name),
             "GA_%03lu", (unsigned long)s_session_counter++);

    bool ok = XM_StartUsbDataLog(s_session_name, GAIT_LOG_METADATA);

    if (ok) {
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
        XM_InsertUsbLogMarker(1, (uint16_t)s_session_counter);
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
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    default:
        XM_SetLedEffect(XM_LED_2, XM_LED_OFF, 0);
        break;
    }

    /* 3. 수동 정지 (BTN2 클릭) */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
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
    s_log.count                 = h10->h10AssistModeLoopCnt;
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

    /* --- Extended: 상태 (1ch) --- */
    s_log.fsm_current_state     = h10->h10FSMcurrentState;
}
