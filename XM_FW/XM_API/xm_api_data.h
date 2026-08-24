/**
 ******************************************************************************
 * @file    xm_api_data.h
 * @author  HyundoKim
 * @brief   XM10 통합 데이터 및 제어 인터페이스
 * @details 
 * 로봇의 모든 센서 데이터(Input)를 읽고, 제어 명령(Output)을 내리는 핵심 API입니다.
 * 사용자는 전역 객체 'XM'을 통해 모든 데이터에 접근할 수 있습니다.
 * * @note    [데이터 흐름]
 * 1. Input (Read):  XM.status 구조체 (센서값, 2ms마다 자동 갱신됨)
 * 2. Output (Write): XM_SetAssistTorque() 함수 사용 (명령 전달)
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_DATA_H_
#define XM_API_XM_API_DATA_H_

#include "cm_drv.h"             /* CM Device Driver (PnP 통합 완료) */
#include "data_object_dictionaries.h"

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* --- Constants --- */
#define XM_GRF_CHANNEL_SIZE  (14) // FSR 센서 채널 수

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief XM10 제어 권한 모드
 * @warning 알고리즘 시작 시 반드시 모드를 설정해야 합니다.
 */
typedef enum {
    XM_CTRL_MONITOR = 0,  /**< [기본] 모니터링 모드. 센서 데이터만 수신하고 제어 명령은 전송하지 않습니다. */
    XM_CTRL_TORQUE  = 1   /**< [주의] 토크 제어 모드. 설정된 토크 명령을 주기적으로 전송합니다. */
} XmControlMode_t;

/**
 * @brief H10 로봇 동작 모드
 */
typedef enum {
    XM_H10_MODE_STANDBY = 0,  /**< 대기 모드 (무출력) */
    XM_H10_MODE_ASSIST  = 1,  /**< 보조 모드 (토크 출력) */
    XM_H10_MODE_UNKNOWN
} XmH10Mode_t;

// P-Vector 데이터 구조체
typedef struct {
    int16_t  yd; // Desired Position (unit: deg, scaled by 100)
    uint16_t L;  // Trajectory Duration (ms)
    uint8_t  s0; // Acceleration Profile (deg/s^2)
    uint8_t  sd; // Deceleration Profile (deg/s^2)
} PVector_t;

// F-Vector 데이터 구조체
typedef struct {
    uint16_t modeIdx; 	// Torque Profile Index, Tp : 0.1 ~ 10
    int16_t  tauMax;  	// Max Torque (A, scaled by 100)
    uint16_t delay;   	// Initial Delay (ms)
	uint16_t zero;		// Dummy data for reset
} FVector_t;

// I-Vector 데이터 구조체
typedef struct {
    uint8_t  epsilon; 	// Half width of Corridor (deg, scaled by 10)
    uint8_t  kp;      	// Virtual Spring Magnitude (%)
    uint8_t  kd;      	// Virtual Damper Magnitude (%)
    uint8_t  lambda;  	// Impedance Ratio (scaled by 100)
    uint16_t duration;	// Transition Duration (ms)
} IVector_t;

/**
 * @brief [KIT H10] 로봇 본체 데이터
 * @details 엔코더, 관절 각도, 보행 상태 등 핵심 정보를 포함합니다.
 */
typedef struct {
    bool  is_connected;      // 연결 상태

    // --- Info & State ---
    uint32_t h10AssistModeLoopCnt;  // H10 보조 모드 루프 카운트 (Assist Mode시작시 count)
    uint32_t h10PostProcessingCnt;  // GaitAnalysis Post-Processing 샘플 카운트 (0~30000, 5m 분석용)
    XmH10Mode_t h10Mode;    // H10 동작 모드 (Assist(1)<->Standby(0))
    uint8_t h10AssistLevel; // H10 보조 레벨 (0~10)
    uint8_t h10FSMcurrentState; // H10 현재 FSM 상태
    bool isPVectorRHDone;   // RH Pvector Complete Flag
    bool isPVectorLHDone;   // LH Pvector Complete Flag
    bool h10IsNeutralPosSet;   // H10 중립각도 설정 완료 상태 (Core Process 끝단에서 초기화)

    // --- Kinematics Data (운동학 정보) ---
    float leftHipAngle;     // 왼쪽 고관절 각도 (Degree)
    float rightHipAngle;    // 오른쪽 고관절 각도
    float leftThighAngle;   // 왼쪽 허벅지 절대각 (Degree)
    float rightThighAngle;  // 오른쪽 허벅지 절대각
    float leftKneeAngle;    // 왼쪽 무릎 각도 (추정치)
    float rightKneeAngle;   // 오른쪽 무릎 각도 (추정치)
    float pelvicAngle;      // 골반 각도 (Tilt)

    // --- Gait Data (보행 정보) ---
    bool isLeftFootContact;  // 왼쪽 발 착지 여부
    bool isRightFootContact; // 오른쪽 발 착지 여부
    float forwardVelocity;  // 전방 보행 속도 (m/s)

    // --- Motor Data (모터 상태) ---
    // [주의] leftHipTorque / rightHipTorque는 필드명과 달리 모터 전류(A)입니다.
    // 내부: int16_raw / CURRENT_SCALING_FACTOR(60) → 범위 -30 ~ +30 A
    // 관절 토크 추정: τ_joint [Nm] = Kt_motor [Nm/A] × gear_ratio × hipTorque [A]
    //                             = 0.085 × 18.75 × hipTorque ≈ 1.594 × hipTorque
    // 실제 토크 센서가 없으므로 전류 기반 추정값입니다.
    float leftHipTorque;      // 왼쪽 모터 전류 (A) — 필드명 주의: 단위는 Nm이 아닌 A
    float rightHipTorque;     // 오른쪽 모터 전류 (A) — 필드명 주의: 단위는 Nm이 아닌 A
    float leftHipMotorAngle;  // 왼쪽 모터 엔코더 각도 (Degree) — 고관절 각도와 다를 수 있음(감속기 비율)
    float rightHipMotorAngle; // 오른쪽 모터 엔코더 각도 (Degree) — 고관절 각도와 다를 수 있음

    // --- IMU Data (관성 센서 상세 정보) ---
    // Orientation
    float leftHipImuFrontalRoll;    // 왼쪽 고관절 IMU Frontal Roll 각도 (Degree)
    float rightHipImuFrontalRoll;
    float leftHipImuSagittalPitch;  // 왼쪽 고관절 IMU Sagittal Pitch 각도 (Degree)
    float rightHipImuSagittalPitch;
    float leftHipImuTransverseYaw;  // 왼쪽 고관절 IMU Transverse Yaw 각도 (Degree)
    float rightHipImuTransverseYaw;

    // Global Acceleration (m/s^2)
    float leftHipImuGlobalAccX;
    float leftHipImuGlobalAccY;
    float leftHipImuGlobalAccZ;
    float rightHipImuGlobalAccX;
    float rightHipImuGlobalAccY;
    float rightHipImuGlobalAccZ;

    // Global Gyroscope (deg/s)
    float leftHipImuGlobalGyrX;
    float leftHipImuGlobalGyrY;
    float leftHipImuGlobalGyrZ;
    float rightHipImuGlobalGyrX;
    float rightHipImuGlobalGyrY;
    float rightHipImuGlobalGyrZ;
} XmH10Data_t;

/**
 * @brief [GRF Sensor] 지면 반발력 센서 데이터 (족압 센서)
 * @details MarvelDex FSR 신발 센서 데이터입니다.
 */
typedef enum {
    XM_SPACE_LEFT = 1,
    XM_SPACE_RIGHT,
    XM_SPACE_UNKNOWN,
} XM_GRF_SPACE_e;

typedef struct {
    bool     is_left_grf_connected;  // 왼쪽 GRF 모듈 연결 상태
    bool     is_right_grf_connected; // 오른쪽 GRF 모듈 연결 상태

    // --- Left Foot Data ---
    // sensorSpace가 LEFT(1)인 패킷의 데이터
    uint32_t leftLastUpdateTick;    // 데이터 수신 시각 (ms)
    XM_GRF_SPACE_e leftSensorSpace; // 1=왼발, 2=오른발
    uint8_t leftRollingIndex;   // 0-199 패킷 시퀀스
    uint8_t leftSensorData[XM_GRF_CHANNEL_SIZE]; // 14개 채널 값 (0~255 Raw Value)
    uint8_t leftBatteryLevel;   // 배터리 잔량 (0~100)
    uint8_t leftStatusFlags;    // 상태 플래그
    
    // --- Right Foot Data ---
    // sensorSpace가 RIGHT(2)인 패킷의 데이터
    uint32_t rightLastUpdateTick;
    XM_GRF_SPACE_e  rightSensorSpace;   // 1=왼발, 2=오른발
    uint8_t  rightRollingIndex; // 0-199 패킷 시퀀스
    uint8_t  rightSensorData[XM_GRF_CHANNEL_SIZE]; // (0~255 Raw Value)
    uint8_t  rightBatteryLevel;
    uint8_t  rightStatusFlags;
} XmGrfData_t;

/**
 * @brief [IMU Sensor] 외부 장착 정밀 IMU 데이터 (Xsens MTi)
 */
typedef struct {
    bool  is_connected; // XSENS IMU 모듈 연결 상태
    uint32_t lastUpdateTick; // 데이터 수신 시각 (ms)

    // --- 1. Orientation (Quaternion) ---
    float q_w, q_x, q_y, q_z;

    // --- 2. Calibrated Acceleration (m/s^2) ---
    float acc_x, acc_y, acc_z;

    // --- 3. Calibrated Gyroscope (deg/s or rad/s) ---
    float gyr_x, gyr_y, gyr_z;
} XmImuData_t;

/**
 * @brief [IMU Hub Module] 6축 IMU 센서 허브 (EBIMU-9DOFV6 × 6)
 * @details DOP V2 프로토콜로 연결된 IMU Hub Module 데이터
 */
#define XM_IMU_HUB_SENSOR_COUNT  6  /**< IMU Hub의 센서 개수 */

typedef struct {
    // --- Orientation (Quaternion) ---
    float q_w, q_x, q_y, q_z;

    // --- Orientation (Euler Angles, Degree) ---
    float roll, pitch, yaw;

    // --- Calibrated Acceleration (g) ---
    float acc_x, acc_y, acc_z;

    // --- Calibrated Gyroscope (deg/s) ---
    float gyr_x, gyr_y, gyr_z;

    // --- Calibrated Magnetometer (uT) ---
    float mag_x, mag_y, mag_z;
} XmImuHubSensor_t;

typedef struct {
    bool  is_connected;      /**< IMU Hub Module 연결 상태 */
    uint32_t lastUpdateTick; /**< 데이터 수신 시각 (ms) */
    
    /** @brief 6개 IMU 센서 데이터 (포트 0~5) */
    XmImuHubSensor_t sensor[XM_IMU_HUB_SENSOR_COUNT];
    
    /** @brief 각 센서의 연결 상태 비트마스크 (bit0~5) */
    uint8_t connected_mask;
} XmImuHubData_t;

/**
 * @brief [EMG Hub Module] sEMG 센서 허브 (DOP V2)
 * @details DOP V2 프로토콜로 연결된 EMG Hub Module 데이터
 *
 * [데이터 원본] EMG Hub TPDO1 (CAN ID 0x18A)
 * - 1kHz 샘플링, 신호처리 파이프라인 결과 포함
 * - HPF 20Hz → Rectification → RMS 200ms → Envelope 8Hz → MVC → Activation
 */
typedef struct {
    bool     is_connected;        /**< EMG Hub Module 연결 상태 */
    uint32_t lastUpdateTick;      /**< 데이터 수신 시각 (ms) */

    /* Raw & Processed EMG Data (float, 스케일링 복원 완료) */
    uint16_t raw_adc;             /**< ADC 원시값 (12-bit, HW OVS 16×) */
    float    voltage_uv;          /**< EMG 전압 (µV, 아날로그 프론트엔드 환산) */
    float    rms_uv;              /**< RMS 값 (µV, 200ms 슬라이딩 윈도우) */
    float    envelope_uv;         /**< Envelope 값 (µV, EMA fc≈8Hz) */
    uint8_t  mvc_percent;         /**< MVC 정규화 (0~100%) */
    bool     is_active;           /**< 근수축 감지 (Schmitt trigger) */

    /* Status Flags (비트 필드) */
    uint8_t  status_flags;        /**< bit0: ADC_OK, bit1: IS_ACTIVE, bit2: SATURATED */
} XmEmgHubData_t;

/**
 * @brief [FES Hub Module] 기능적 전기 자극 모듈 (DOP V2)
 * @details DOP V2 프로토콜로 연결된 FES Hub Module 데이터
 *
 * [데이터 원본] FES Hub TPDO1 (CAN ID 0x18B)
 * - 2채널 독립 자극, HV 전압 피드백, 채널별 상태/전류/Fault
 */
#define XM_FES_HUB_CH_COUNT     2  /**< FES Hub 채널 수 */

typedef struct {
    bool     is_connected;        /**< FES Hub Module 연결 상태 */
    uint32_t lastUpdateTick;      /**< 데이터 수신 시각 (ms) */

    /* 채널 상태 (0=IDLE, 1=READY, 2=STIMULATING, 3=FAULT) */
    uint8_t  ch_state[XM_FES_HUB_CH_COUNT];

    /* 전류 피드백 (mA) */
    float    ch_current_mA[XM_FES_HUB_CH_COUNT];

    /* Fault 코드 (채널별) */
    uint8_t  ch_fault_code[XM_FES_HUB_CH_COUNT];

    /* HV 부스트 컨버터 출력 전압 (V) */
    float    hv_voltage_V;

    /* Digipot 위치 (0~127, 진폭 제어) */
    uint8_t  digipot_pos;

    /* Error Register */
    uint8_t  error_register;
} XmFesHubData_t;

// [FSR Sensor]

/* --- 2. Main Facade Structure --- */

/**
 * @brief [Input] 로봇 상태 통합 구조체
 * @note  End User는 XM.status를 통해 이 구조체에 접근합니다.
 */
typedef struct {
    XmH10Data_t     h10;      /**< H10 로봇 본체 데이터 (DOP V1) */
    XmGrfData_t     grf;      /**< GRF 족압 센서 데이터 */
    XmImuData_t     imu;      /**< XSENS IMU 데이터 (외부 장착) */
    XmImuHubData_t  imu_hub;  /**< [신규] IMU Hub 센서 데이터 (DOP V2) ✅ */
    XmEmgHubData_t  emg_hub;  /**< [신규] EMG Hub 센서 데이터 (DOP V2) */
    XmFesHubData_t  fes_hub;  /**< [신규] FES Hub 자극 피드백 (DOP V2) */
} XmInput_t;

/**
 * @brief [Output] 로봇 제어 명령 구조체
 * @note  End User는 직접 구조체를 수정하지 말고, XM_SetAssistTorque() 함수를 사용하세요.
 */
typedef struct {
    XmControlMode_t control_mode; // 현재 제어 모드

    float assist_torque_rh;
    float assist_torque_lh;
    
    /* Dirty Flags (User는 몰라도 됨 - Helper 함수가 관리) */
    struct {
        uint8_t torque_rh_updated : 1;
        uint8_t torque_lh_updated : 1;
    } _dirty_flags;
} XmOutput_t;

/**
 * @brief XM10 로봇 통합 객체
 * @details 사용자는 전역 변수 'XM'을 통해 로봇의 상태를 읽고 명령을 내립니다.
 */
typedef struct {
    XmInput_t  status;  // [Read] 센서값 (Input)
    XmOutput_t command; // [Write] 명령값 (Output)
} XmRobot_t;

/**
 *-----------------------------------------------------------
 * PUBLIC VARIABLES(extern)
 *-----------------------------------------------------------
 */

/**
 * @brief 전역 로봇 인스턴스
 * @code
 * // 예제: 현재 왼쪽 고관절 각도 읽기
 * float angle = XM.status.h10.leftHipAngle;
 * @endcode
 */
extern XmRobot_t XM;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/**
 * @brief  제어 모드를 설정합니다. (안전장치 포함)
 * @details 모드를 변경할 때, 급발진 방지를 위해 모든 토크 명령을 0으로 초기화합니다.
 * @param  mode 설정할 모드
 * - XM_CTRL_MONITOR: 명령 전송 중단 (기본값)
 * - XM_CTRL_TORQUE : 토크 제어 시작 (주의!)
 */
void XM_SetControlMode(XmControlMode_t mode);

/**
 * @brief  양쪽 다리의 보조 토크를 설정합니다.
 * @details 설정된 값은 제어 주기에 맞춰 자동으로 전송됩니다.
 * @param  rh 오른쪽 보조 토크 (Nm)
 * @param  lh 왼쪽 보조 토크 (Nm)
 */
void XM_SetAssistTorque(float rh, float lh);

/**
 * @brief  오른쪽 다리의 보조 토크만 설정합니다.
 * @param  rh 오른쪽 보조 토크 (Nm)
 */
void XM_SetAssistTorqueRH(float rh);

/**
 * @brief  왼쪽 다리의 보조 토크만 설정합니다.
 * @param  lh 왼쪽 보조 토크 (Nm)
 */
void XM_SetAssistTorqueLH(float lh);

/**
 * =================================================================================================
 * 시스템 및 연결 상태 API (System & Connection Status API)
 * =================================================================================================
 */

/**
 * @brief 제어 모듈(CM)과의 통신 연결 상태를 확인합니다.
 * @return 연결이 정상적(OPERATIONAL)이면 true, 아니면 false.
 */
bool XM_IsCmConnected(void);

/**
 * @brief 현재 CM과의 PnP(NMT) 상태를 가져옵니다.
 * @return CM_NmtState_t 열거형 값.
 * @details [변경] LinkNmtState_t → CM_NmtState_t (.cursorrules Phase 5)
 */
CM_NmtState_t XM_GetXMNmtState(void);


/**
 * =================================================================================================
 * 데이터 송신 API (Data Transmission API)
 * =================================================================================================
 */

/**
 * @brief 사용자 신체 정보를 CM으로 전송합니다.
 * @param[in] bodyData 8개의 uint32_t 값을 담은 배열 (몸무게, 키, 분절 길이 등).
 */
void XM_SendUserBodyData(const uint32_t bodyData[8]);

/**
 * @brief 지정된 관절에 위치 기반 궤적(P-Vector)을 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (예: SYS_NODE_ID_RH).
 * @param[in] pVector   전송할 P-Vector 데이터를 담은 구조체 포인터.
 */
void XM_SendPVector(SystemNodeID_t nodeId, const PVector_t* pVector);

/**
 * @brief 지정된 관절에 P-Vector Reset을 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (예: SYS_NODE_ID_RH).
 */
void XM_SendPVectorReset(SystemNodeID_t nodeId);

/**
 * @brief P-Vector 완료 이벤트를 '소비(consume)'하여 플래그를 수동으로 클리어합니다.
 * @details GetSuitData()를 통해 isPVector...Done 플래그가 true인 것을 확인하고,
 * 관련된 로직을 모두 처리한 후에 이 함수를 호출해야 합니다.
 * 호출하지 않으면 isPVector...Done 플래그가 계속 true로 남아있어
 * 동일한 이벤트가 반복 처리될 수 있습니다.
 * @param[in] nodeId 플래그를 클리어할 관절의 Node ID (예: SYS_NODE_ID_RH).
 */
void XM_ClearPVectorDoneFlag(SystemNodeID_t nodeId);

/**
 * @brief 지정된 관절에 임피던스 궤적(I-Vector)을 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] iVector   전송할 I-Vector 데이터를 담은 구조체 포인터.
 */
void XM_SendIVector(SystemNodeID_t nodeId, const IVector_t* iVector);

/**
 * @brief 지정된 관절에 힘 기반 궤적(F-Vector)을 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] fVector   전송할 F-Vector 데이터를 담은 구조체 포인터.
 */
void XM_SendFVector(SystemNodeID_t nodeId, const FVector_t* fVector);

/**
 * @brief 지정된 관절의 I-Vector 최대 Kp, Kd 값을 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] kpMax     최대 Kp 값 (float).
 * @param[in] kdMax     최대 Kd 값 (float).
 */
void XM_SendIVectorKpKdMax(SystemNodeID_t nodeId, const float kpMax, const float kdMax);

/**
 * @brief 지정된 관절(Node)의 각도 제한 루틴을 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (예: SYS_NODE_ID_RH).
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void XM_SetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet);

/**
 * @brief 지정된 관절(Node)의 가동범위(ROM) 상/하한을 설정합니다.
 * @param[in] nodeId      명령을 전달할 관절의 Node ID.
 * @param[in] upperLimit  설정할 가동범위 상한값 (단위: degree).
 * @param[in] lowerLimit  설정할 가동범위 하한값 (단위: degree).
 */
void XM_SetDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);

/**
 * @brief 지정된 관절(Node)의 각속도 제한 루틴을 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (예: SYS_NODE_ID_RH).
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void XM_SetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet);

/**
 * @brief 지정된 관절(Node)의 가동속도범위 상/하한을 설정합니다.
 * @param[in] nodeId      명령을 전달할 관절의 Node ID.
 * @param[in] upperLimit  설정할 가동속도범위 상한값 (단위: deg/s).
 * @param[in] lowerLimit  설정할 가동속도범위 하한값 (단위: deg/s).
 */
void XM_SetVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);

/**
 * @brief 지정된 관절(Node)의 외란 관측기(DOB) 루틴을 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void XM_SetDOBRoutine(SystemNodeID_t nodeId, bool isSet);

/**
 * @brief 지정된 관절(Node)의 Normal Compensation Gain을 설정합니다.
 * @details 주로 중력 보상과 같은 일반적인 보상 로직의 강도를 조절합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] gain      설정할 게인 값.
 */
void XM_SetNormalCompGain(SystemNodeID_t nodeId, uint8_t gain);

/**
 * @brief 지정된 관절(Node)의 Resistive Compensation Gain을 설정합니다.
 * @details 저항 모드(Resistive Mode)에서 사용자가 느끼는 저항의 강도를 조절합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] gain      설정할 저항 게인 값.
 */
void XM_SetResistiveCompGain(SystemNodeID_t nodeId, float gain);

/**
 * @brief H10의 기존 보조 알고리즘 활성화 여부를 설정합니다.
 * @details true(1)은 H10 기존 보조 알고리즘 활성화 false(0)은 비활성화.
 * default는 XM10과 H10연결시 기존 보조 알고리즘 비활성화
 * @param[in] isSet 기존 보조 모드 활성화 'true', 비활성화 'false'
 */
void XM_SetH10AssistExistingMode(bool isSet);

/**
 * @brief H10 Assist Loop Counter의 기준점을 캡처합니다.
 * @details 호출 시점의 h10AssistModeLoopCnt를 기준점(0)으로 저장합니다.
 *          이후 XM_GetRelativeLoopCount() 호출 시 이 시점부터의 상대 카운트를 반환합니다.
 *          데이터 로깅 세션 시작 시 호출하면, 저장 데이터의 count가 항상 0부터 시작합니다.
 */
void XM_CaptureLoopCountBase(void);

/**
 * @brief 캡처된 기준점 대비 상대 Loop Count를 반환합니다.
 * @return h10AssistModeLoopCnt - 기준점 (XM_CaptureLoopCountBase 미호출 시 절대값 반환)
 */
uint32_t XM_GetRelativeLoopCount(void);

#endif /* XM_API_XM_API_DATA_H_ */
