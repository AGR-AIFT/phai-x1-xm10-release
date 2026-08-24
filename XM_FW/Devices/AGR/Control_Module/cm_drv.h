/**
 ******************************************************************************
 * @file    cm_drv.h
 * @author  HyundoKim
 * @brief   CM Device Driver - DOP V1 기반 CM 통신 + PnP 상태 머신 통합
 * @version 2.0.0
 * @date    2026-02-10
 *
 * @details
 * [변경 이유] .cursorrules Phase 5 - Link → Device 병합
 * - AS-IS: cm_drv.c (데이터 처리) + cm_xm_link.c (PnP 상태 머신) 분리
 * - TO-BE: cm_drv.c 하나에 PnP 상태 머신 통합
 * 
 * [주요 변경 사항]
 * 1. CM_EventCallbacks_t 제거 → 내부 함수 직접 호출
 * 2. CM_Init() 시그니처 변경 (callbacks 파라미터 제거)
 * 3. CM_Drv_RunPeriodic(), CM_Drv_IsConnected(), CM_Drv_GetNmtState() 추가
 * 4. PnP Heartbeat 타임아웃: 1초 → 3초 (.cursorrules 표준 통일)
 * 5. Pre-Op fall-through 버그 수정
 *
 * @copyright Copyright (c) 2025 Angel Robotics Inc. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef DEVICES_CONTROL_MODULE_INC_CM_DRV_H_
#define DEVICES_CONTROL_MODULE_INC_CM_DRV_H_

#include "data_object_interface.h"

#include <stdint.h>
#include <stdbool.h>

/**
 *-----------------------------------------------------------
 *            PUBLIC DEFINITIONS AND ENUMERATIONS
 *-----------------------------------------------------------
 */

/* ===== PnP NMT 상태 정의 (cm_xm_link.h에서 이동) =====
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - 값은 CM의 ExtLinkNmtState_t와 동일 (SDO 통신 호환성)
 */
typedef enum {
    CM_NMT_INITIALISING = 0,
    CM_NMT_PRE_OPERATIONAL,
    CM_NMT_OPERATIONAL,
    CM_NMT_STOPPED
} CM_NmtState_t;

// --- 1. 논리적 Dictionary ID (통신 방향) ---
typedef enum {
    CM_DICT_ID_CM_TO_XM = 0, // CM -> XM
    CM_DICT_ID_XM_TO_CM = 1, // XM -> CM
    CM_DICT_ID_COUNT
} CM_DictID_t;

/* --- 2. SDO Object ID 정의 --- */
// Master(XM) -> CM 으로 보내는 SDO ID 목록
typedef enum {
    CM_SDO_ID_X2C_SET_PDO_MAPPING = 0,  // 기존: CMEXT_LINK_SDO_ID_EXT_to_CM_PDO_LIST
    CM_SDO_ID_X2C_SET_SDO_MAPPING,      // 기존: ..._SDO_LIST

    CM_SDO_ID_X2C_NOTIFY_XM_BOOTUP,     // 기존: ..._EXT_NMT_BOOTUP // 확장팩의 bootup SDO
    CM_SDO_ID_X2C_CMD_SET_CM_NMT_STATE, // 기존: ..._CM_NMT_STATE // CM의 NMT State 변경 SDO
    CM_SDO_ID_X2C_NOTIFY_XM_HEARTBEAT,  // 기존: ..._EXT_NMT_HEARTBEAT // 확장팩의 Heartbeat SDO
    CM_SDO_ID_X2C_SYNC_STATES,          // XM과 CM간 상태 변수 동기화를 위한 SDO

    CM_SDO_ID_X2C_SET_USER_BODY_DATA,

    CM_SDO_ID_X2C_SET_P_VECTOR_YD_RH,
    CM_SDO_ID_X2C_SET_P_VECTOR_L_RH,
    CM_SDO_ID_X2C_SET_P_VECTOR_S0_RH,
    CM_SDO_ID_X2C_SET_P_VECTOR_SD_RH,
    CM_SDO_ID_X2C_SET_P_VECTOR_RESET_RH,
    CM_SDO_ID_X2C_SET_P_VECTOR_YD_LH,
    CM_SDO_ID_X2C_SET_P_VECTOR_L_LH,
    CM_SDO_ID_X2C_SET_P_VECTOR_S0_LH,
    CM_SDO_ID_X2C_SET_P_VECTOR_SD_LH,
    CM_SDO_ID_X2C_SET_P_VECTOR_RESET_LH,

    CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_RH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KP_RH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KD_RH,
    CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_RH,
    CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_RH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_RH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_RH,
    CM_SDO_ID_X2C_SET_I_VECTOR_EPSILON_LH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KP_LH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KD_LH,
    CM_SDO_ID_X2C_SET_I_VECTOR_LAMBDA_LH,
    CM_SDO_ID_X2C_SET_I_VECTOR_DURATION_LH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KP_MAX_LH,
    CM_SDO_ID_X2C_SET_I_VECTOR_KD_MAX_LH,

    CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_RH,
    CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_RH,
    CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_RH,
    CM_SDO_ID_X2C_SET_F_VECTOR_MODE_IDX_LH,
    CM_SDO_ID_X2C_SET_F_VECTOR_TMAX_LH,
    CM_SDO_ID_X2C_SET_F_VECTOR_DELAY_LH,

    CM_SDO_ID_X2C_SET_DEGREE_LIMIT_ROUTINE_RH,
    CM_SDO_ID_X2C_CLEAR_DEGREE_LIMIT_ROUTINE_RH,
    CM_SDO_ID_X2C_SET_DEGREE_LIMIT_ROUTINE_LH,
    CM_SDO_ID_X2C_CLEAR_DEGREE_LIMIT_ROUTINE_LH,
    CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_RH,
    CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_RH,
    CM_SDO_ID_X2C_SET_DEGREE_LIMIT_UPPER_LH,
    CM_SDO_ID_X2C_SET_DEGREE_LIMIT_LOWER_LH,

    CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_ROUTINE_RH,
    CM_SDO_ID_X2C_CLEAR_VELOCITY_LIMIT_ROUTINE_RH,
    CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_ROUTINE_LH,
    CM_SDO_ID_X2C_CLEAR_VELOCITY_LIMIT_ROUTINE_LH,
    CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_RH,
    CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_RH,
    CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_UPPER_LH,
    CM_SDO_ID_X2C_SET_VELOCITY_LIMIT_LOWER_LH,

    CM_SDO_ID_X2C_SET_DOB_ROUTINE_RH,
    CM_SDO_ID_X2C_CLEAR_DOB_ROUTINE_RH,
    CM_SDO_ID_X2C_SET_DOB_ROUTINE_LH,
    CM_SDO_ID_X2C_CLEAR_DOB_ROUTINE_LH,

    CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_RH,
    CM_SDO_ID_X2C_SEND_NORMAL_COMP_GAIN_LH,
    CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_RH,
    CM_SDO_ID_X2C_SEND_RESISITIVE_COMP_GAIN_LH,

    CM_SDO_ID_X2C_SET_H10_ORIGINAL_ASSIST_MODE,
} CM_SdoToCmID_t;

// CM -> Master(XM) 으로 보내는 SDO ID 목록
typedef enum {
	CM_SDO_ID_C2X_SET_PDO_MAPPING = 0,
	CM_SDO_ID_C2X_SET_SDO_MAPPING,

    CM_SDO_ID_C2X_NOTIFY_CM_BOOTUP, 	// CM의 Boot-up SDO
    CM_SDO_ID_C2X_GET_CM_NMT_STATE,		// CM의 NMT State SDO
    CM_SDO_ID_C2X_NOTIFY_CM_HEARTBEAT,	// CM의 Heartbeat SDO
    CM_SDO_ID_C2X_SYNC_STATES,          // XM과 CM간 상태 변수 동기화를 위한 SDO
    CM_SDO_ID_C2X_GET_H10_MODE,
	CM_SDO_ID_C2X_GET_H10_ASSIST_LEVEL,
    CM_SDO_ID_C2X_NOTIFY_PVECTOR_DONE_RH,
	CM_SDO_ID_C2X_NOTIFY_PVECTOR_DONE_LH,
    CM_SDO_ID_C2X_GET_H10_NEUTRAL_POS_SETFLAG,
} CM_SdoToMasterID_t;

/* --- 3. PDO Object ID 정의 --- */
// Master(XM) -> CM 으로 보내는 PDO ID 목록
typedef enum {
    CM_PDO_ID_X2C_SET_AUX_TORQUE_RH = 0,
    CM_PDO_ID_X2C_SET_AUX_TORQUE_LH,
} CM_PdoToCmID_t;

// CM -> Master(XM) 으로 보내는 PDO ID 목록
typedef enum {
    CM_PDO_ID_C2X_GET_H10_ASSIST_LOOP_CNT = 0,
    CM_PDO_ID_C2X_GET_LEFT_HIP_ANGLE,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_ANGLE,
    CM_PDO_ID_C2X_GET_LEFT_THIGH_ANGLE,
    CM_PDO_ID_C2X_GET_RIGHT_THIGH_ANGLE,
    CM_PDO_ID_C2X_GET_PELVIC_ANGLE,
    CM_PDO_ID_C2X_GET_PELVIC_VEL_Y,
    CM_PDO_ID_C2X_GET_LEFT_KNEE_ANGLE,
    CM_PDO_ID_C2X_GET_RIGHT_KNEE_ANGLE,
    CM_PDO_ID_C2X_GET_LEFT_FOOT_CONTACT,
    CM_PDO_ID_C2X_GET_RIGHT_FOOT_CONTACT,
    CM_PDO_ID_C2X_GET_GAIT_STATE,
    CM_PDO_ID_C2X_GET_GAIT_CYCLE,
    CM_PDO_ID_C2X_GET_FORWARD_VELOCITY,
    CM_PDO_ID_C2X_GET_LEFT_HIP_TORQUE,
	CM_PDO_ID_C2X_GET_RIGHT_HIP_TORQUE,
    CM_PDO_ID_C2X_GET_LEFT_HIP_MOTOR_ANGLE,
	CM_PDO_ID_C2X_GET_RIGHT_HIP_MOTOR_ANGLE,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_FRONTAL_ROLL,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_FORNTAL_ROLL,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_SAGITTAL_PITCH,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_SAGITTAL_PITCH,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_X,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_Y,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_ACC_Z,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_X,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_Y,
    CM_PDO_ID_C2X_GET_LEFT_HIP_IMU_GLOBAL_GYR_Z,
	CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_X,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_Y,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_ACC_Z,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_X,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_Y,
    CM_PDO_ID_C2X_GET_RIGHT_HIP_IMU_GLOBAL_GYR_Z,
    CM_PDO_ID_C2X_GET_FSM_CURRENT_STATE,
    CM_PDO_ID_C2X_GET_POST_PROCESSING_CNT,
} CM_PdoToMasterID_t;

/**
 *-----------------------------------------------------------
 *                       PUBLIC TYPES
 *-----------------------------------------------------------
 */

/* ===== CM_EventCallbacks_t 제거 =====
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: cm_xm_link.c가 콜백을 등록하여 SDO 이벤트 수신
 * - TO-BE: PnP 상태 머신이 cm_drv.c 내부에 통합되어 직접 호출
 * - 외부 콜백이 더 이상 불필요하므로 제거
 */

// 통신 채널(FDCAN 등)을 통해 데이터를 전송하기 위한 함수 포인터 타입
// 이 함수 포인터를 통해 cm_api는 하위 IOIF 계층과 분리됩니다.
// [AGR-DOP V2 호환] AGR_TxFunc_t와 동일한 시그니처 사용
typedef int (*CM_TxFunc_t)(uint32_t can_id, const uint8_t* data, uint8_t len);

typedef enum {
	H10_STANDBY_MODE,
	H10_ASSIST_MODE,
	H10_MODE_NUM
} CM_H10Mode_t;

// P-Vector 데이터 구조체
typedef struct {
    int16_t  yd; // Desired Position (unit: deg, scaled by 100)
    uint16_t L;  // Trajectory Duration (ms)
    uint8_t  s0; // Acceleration Profile (deg/s^2)
    uint8_t  sd; // Deceleration Profile (deg/s^2)
} CM_PVector_t;

// F-Vector 데이터 구조체
typedef struct {
    uint16_t modeIdx; 	// Torque Profile Index, Tp : 0.1 ~ 10
    int16_t  tauMax;  	// Max Torque (A, scaled by 100)
    uint16_t delay;   	// Initial Delay (ms)
	uint16_t zero;		// Dummy data for reset
} CM_FVector_t;

// I-Vector 데이터 구조체
typedef struct {
    uint8_t  epsilon; 	// Half width of Corridor (deg, scaled by 10)
    uint8_t  kp;      	// Virtual Spring Magnitude (%)
    uint8_t  kd;      	// Virtual Damper Magnitude (%)
    uint8_t  lambda;  	// Impedance Ratio (scaled by 100)
    uint16_t duration;	// Transition Duration (ms)
} CM_IVector_t;

// C언어의 메모리 패딩을 제거하여 CAN 페이로드와 1:1로 일치시킵니다.
#pragma pack(push, 1)
/**
 * @brief [신규] XM -> CM으로 전송되는 실시간 PDO의 고정 페이로드 구조체
 * @note  이 순서가 PnP(약속) 단계에서 CM에게 전달될 Tx 맵입니다.
 */
typedef struct {
    int16_t   auxTorqueInputRH;  // 2 bytes (0-1)
    int16_t   auxTorqueInputLH;  // 2 bytes (2-3)
    // (향후 Tx PDO가 추가되면 여기에 순서대로 추가)
} CM_PdoTx_XmToCm_t;

/**
 * @brief [신규] CM -> XM으로 수신되는 실시간 PDO의 고정 페이로드 구조체
 * @note
 */
typedef struct {
    uint32_t h10AssistModeLoopCnt;     // 4 bytes
    int16_t  leftHipAngle;              // 2 bytes
    int16_t  rightHipAngle;             // 2 bytes
    int16_t  leftThighAngle;            // 2 bytes
    int16_t  rightThighAngle;           // 2 bytes
    int16_t  pelvicAngle;               // 2 bytes
    int16_t  pelvicVelY;                // 2 bytes
    int16_t  leftKneeAngle;             // 2 bytes
    int16_t  rightKneeAngle;            // 2 bytes
    uint8_t  isLeftFootContact;         // 1 byte
    uint8_t  isRightFootContact;        // 1 byte
    uint8_t  gaitState;                 // 1 byte
    uint8_t  gaitCycle;                 // 1 byte
    int16_t  forwardVelocity;           // 2 bytes
    int16_t  leftHipTorque;             // 2 bytes
    int16_t  rightHipTorque;            // 2 bytes
    int16_t  leftHipMotorAngle;         // 2 bytes
    int16_t  rightHipMotorAngle;        // 2 bytes
    int16_t  leftHipImuFrontalRoll;     // 2 bytes
    int16_t  leftHipImuSagittalPitch;   // 2 bytes
    int16_t  rightHipImuFrontalRoll;    // 2 bytes
    int16_t  rightHipImuSagittalPitch;  // 2 bytes
    int16_t  leftHipImuTransverseYaw;   // 2 bytes
    int16_t  rightHipImuTransverseYaw;  // 2 bytes
    int16_t  leftHipImuGlobalAccX;      // 2 bytes
    int16_t  leftHipImuGlobalAccY;      // 2 bytes
    int16_t  leftHipImuGlobalAccZ;      // 2 bytes
    int16_t  leftHipImuGlobalGyrX;      // 2 bytes
    int16_t  leftHipImuGlobalGyrY;      // 2 bytes
    int16_t  leftHipImuGlobalGyrZ;      // 2 bytes
    int16_t  rightHipImuGlobalAccX;     // 2 bytes
    int16_t  rightHipImuGlobalAccY;     // 2 bytes
    int16_t  rightHipImuGlobalAccZ;     // 2 bytes
    int16_t  rightHipImuGlobalGyrX;     // 2 bytes
    int16_t  rightHipImuGlobalGyrY;     // 2 bytes
    int16_t  rightHipImuGlobalGyrZ;     // 2 bytes
    uint8_t  h10FSMcurrentState;
    uint32_t postProcessingCnt;         // 4 bytes
} CM_PdoRx_CmToXm_t;
#pragma pack(pop)

// CM으로부터 수신한 최종 처리된 데이터 구조체 (PDO Part)
typedef struct {
    uint32_t h10AssistModeLoopCnt;
    float leftHipAngle;
    float rightHipAngle;
    float leftThighAngle;
    float rightThighAngle;
    float pelvicAngle;
    float pelvicVelY;
    float leftKneeAngle;
    float rightKneeAngle;
    bool isLeftFootContact;
    bool isRightFootContact;
    uint8_t gaitState;
    uint8_t gaitCycle;
    float forwardVelocity;
    float leftHipTorque;
    float rightHipTorque;
    float leftHipMotorAngle;
    float rightHipMotorAngle;
    float leftHipImuFrontalRoll;
    float leftHipImuSagittalPitch;
    float rightHipImuFrontalRoll;
    float rightHipImuSagittalPitch;
    float leftHipImuTransverseYaw;
    float rightHipImuTransverseYaw;
    float leftHipImuGlobalAccX;
    float leftHipImuGlobalAccY;
    float leftHipImuGlobalAccZ;
    float leftHipImuGlobalGyrX;
    float leftHipImuGlobalGyrY;
    float leftHipImuGlobalGyrZ;
    float rightHipImuGlobalAccX;
    float rightHipImuGlobalAccY;
    float rightHipImuGlobalAccZ;
    float rightHipImuGlobalGyrX;
    float rightHipImuGlobalGyrY;
    float rightHipImuGlobalGyrZ;
    uint8_t  h10FSMcurrentState;
    uint32_t postProcessingCnt;  /* no scale — raw uint32 counter */
} CM_RxPdoData_t;

// CM으로부터 수신한 최종 처리된 데이터 구조체 (SDO Part)
typedef struct {
    CM_H10Mode_t h10Mode;
    uint8_t  h10AssistLevel;
    bool isPVectorRHDone;
    bool isPVectorLHDone;
    bool h10NeutralPosSet;
} CM_RxSdoData_t;

// CM으로부터 수신한 최종 처리된 데이터 구조체 (Total)
typedef struct {
    CM_RxPdoData_t pdo; // Data from PDOs
    CM_RxSdoData_t sdo; // Data from SDOs
} CM_RxData_t;

/**
 *------------------------------------------------------------
 *                   PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

// --- 초기화 및 코어 처리 함수 ---
/**
 * @brief CM Device Driver 초기화 (DOP V1 + PnP 상태 머신 통합)
 * 
 * @details
 * [변경 이유] Link → Device 병합 (.cursorrules Phase 5)
 * - AS-IS: CM_Init(txFunc, callbacks, xmNodeId, cmNodeId) + CM_XM_Link_Init()
 * - TO-BE: CM_Init(txFunc, xmNodeId, cmNodeId) 하나로 통합
 * - 외부 콜백(CM_EventCallbacks_t) 제거 → 내부 PnP SM이 직접 처리
 *
 * @param[in] txFunc      CAN 메시지를 전송할 함수 포인터 (IOIF 계층의 Transmit 함수).
 * @param[in] xmNodeId    이 모듈(XM)의 Node ID.
 * @param[in] cmNodeId    통신 대상인 CM의 Node ID.
 */
void CM_Init(CM_TxFunc_t txFunc, uint8_t xmNodeId, uint8_t cmNodeId);

/**
 * @brief CM PnP 상태 머신 주기적 실행 (100ms 주기)
 * 
 * @details
 * [변경 이유] cm_xm_link.c → cm_drv.c 병합 (.cursorrules Phase 5)
 * - AS-IS: CM_XM_Link_RunPeriodic() (System/Links/)
 * - TO-BE: CM_Drv_RunPeriodic() (Devices/AGR/Control_Module/)
 * 
 * [실행 내용]
 * - INITIALISING: Boot-up SDO 주기적 전송 (1초마다)
 * - PRE_OPERATIONAL: PDO Mapping → State Sync → NMT Operational
 * - OPERATIONAL: Heartbeat SDO 전송 + CM Heartbeat Timeout 감시
 * - STOPPED: 3초 후 INITIALISING으로 재진입 (재연결 시도)
 * 
 * @note PnP_Task에서 100ms 주기로 호출됩니다.
 */
void CM_Drv_RunPeriodic(void);

/**
 * @brief CM과의 PnP 연결 완료 여부 반환
 * 
 * @details
 * [변경 이유] CM_XM_Link_IsConnected() 대체 (.cursorrules Phase 5)
 * 
 * @return true: OPERATIONAL + PDO 스트림 시작됨, false: 미연결
 */
bool CM_Drv_IsConnected(void);

/**
 * @brief CM과의 PnP NMT 상태 반환
 * 
 * @details
 * [변경 이유] CM_XM_Link_GetXMNmtState() 대체 (.cursorrules Phase 5)
 * 
 * @return CM_NmtState_t 현재 XM→CM PnP NMT 상태
 */
CM_NmtState_t CM_Drv_GetNmtState(void);

/**
 * @brief CM으로부터 수신된 SDO 메시지를 처리합니다.
 * @details SDO Processor Task (비실시간)에서 호출.
 *          내부에서 Mutex 보호 후 SDO 언패킹 수행.
 * 
 * [변경 이유] .cursorrules 기반 리팩토링
 * - AS-IS: CM_ProcessCANMessage() - SDO/PDO 모두 수신 가능하나 PDO는 default 드롭
 * - TO-BE: CM_Drv_ProcessSdo() - SDO 전용, canfd_rx_handler에서 SDO만 라우팅
 * 
 * @param[in] canId   수신된 메시지의 CAN ID (0x212).
 * @param[in] data    수신된 데이터 버퍼 포인터.
 * @param[in] len     수신된 데이터의 길이 (바이트).
 * 
 * @note Thread-Safe: 내부 Mutex 보호
 */
void CM_Drv_ProcessSdo(uint16_t canId, uint8_t* data, uint8_t len);

/**
 * @brief CM으로부터 수신된 PDO 데이터를 처리합니다.
 * @details FDCAN_Rx_Task (실시간)에서 호출.
 *          내부에서 Mutex 보호 후 PDO 언패킹 및 스케일링 수행.
 * 
 * [변경 이유] .cursorrules 기반 리팩토링
 * - AS-IS: CM_UpdatePdoData() - 네이밍이 모호
 * - TO-BE: CM_Drv_ProcessPdo() - PDO 전용 처리 함수로 명확화
 * 
 * @param[in] data    수신된 데이터 버퍼 포인터.
 * @param[in] len     수신된 데이터의 길이 (바이트).
 * 
 * @note Thread-Safe: 내부 Mutex 보호 (1ms Timeout)
 */
void CM_Drv_ProcessPdo(uint8_t* data, uint8_t len);

// --- 데이터 수신 API ---
/**
 * @brief 가장 최근에 수신된 CM의 피드백 데이터를 가져옵니다.
 * @details 내부적으로 관리되는 최신 RxData를 사용자가 제공한 구조체에 안전하게 복사합니다.
 * @param[out] rxData 수신된 데이터를 저장할 CM_RxData_t 구조체 포인터.
 * @return            데이터 수신 성공 시 `true`, 아직 유효한 데이터가 없을 시 `false`.
 */
bool CM_GetRxData(CM_RxData_t* rxData);

/**
 * @brief CM으로부터 수신된 Raw PDO 데이터(int16)를 가져옵니다.
 * @details Total Data Packet의 CM PDO 섹션에 raw memcpy로 사용됩니다.
 *          float 변환 전의 원본 int16 값을 제공합니다.
 * @param[out] raw_pdo Raw PDO 데이터를 저장할 구조체 포인터
 * @return 데이터 복사 성공 시 true, Mutex 실패 또는 데이터 미수신 시 false
 * @note Thread-Safe: 내부 Mutex 보호 (1ms Timeout)
 */
bool CM_GetRawPdoData(CM_PdoRx_CmToXm_t* raw_pdo);

/**
 * @brief 유효한 PDO 데이터가 한 번 이상 수신되었는지 확인합니다.
 * @details 이 함수는 PnP 연결 후, CM으로부터 실제 데이터 스트림이 시작되었는지
 * 동기화 목적으로 확인하는 데 사용됩니다.
 * @return 유효한 PDO 데이터가 수신되었다면 `true`, 아니라면 `false`.
 */
bool CM_IsDataReady(void);

/**
 * @brief CM으로부터 수신된 모든 데이터 캐시를 리셋합니다.
 * @details 네트워크 연결이 끊어졌을 때(예: STOPPED 상태 진입 시) 호출되어,
 * 오래된(Stale) 데이터가 재연결 시 사용되는 것을 방지합니다.
 * 내부적으로 Mutex를 사용하여 스레드 안전하게 동작합니다.
 */
void CM_ResetRxData(void);

// --- NMT 및 PnP 관련 API (주로 BSP에서 사용) ---
/**
 * @brief CM에게 전송을 요청할 PDO 목록을 SDO로 전송합니다.
 * @details PnP 과정 중 PRE-OPERATIONAL 상태에서 호출됩니다.
 */
void CM_SendSetPDOMapping(void);

/**
 * @brief CM의 NMT 상태 변경을 요청하는 SDO를 전송합니다.
 * @param[in] targetState 목표 NMT 상태 (예: NMT_STATE_OPERATIONAL).
 */
void CM_SendSetNMTState(uint8_t targetState);

/**
 * @brief XM(자신)의 부팅 완료를 알리는 Boot-up SDO 메시지를 보냅니다.
 * @details PnP 과정의 가장 첫 단계로, 초기화가 완료되었음을 네트워크에 알립니다.
 */
void CM_SendBootup(void);

/**
 * @brief XM(자신)의 생존 신호를 알리는 Heartbeat SDO 메시지를 보냅니다.
 * @details OPERATIONAL 상태에서 주기적으로 호출되어 통신 상태를 알립니다.
 * @param[in] xmNmtState 현재 XM NMT 상태 (예: NMT_STATE_OPERATIONAL).
 */
void CM_SendHeartbeat(uint8_t xmNmtState);

/**
 * @brief CM에게 현재 상태 변수(h10Mode 등) 동기화를 요청하는 SDO를 전송합니다.
 * @details PnP(Plug and Play) 과정 중 NMT 'PRE_OPERATIONAL' 상태에서
 * PDO 매핑이 성공한 직후 호출됩니다.
 * * 이 SDO(CM_SDO_ID_X2C_SYNC_STATES)를 수신한 CM은,
 * 자신의 SDO 콜백을 통해 현재 h10Mode, AssistLevel 등의
 * 핵심 상태 값들을 SDO로 XM에게 전송(응답)합니다.
 * * 이를 통해 XM은 재연결 시에도 CM의 최신 상태를 동기화할 수 있습니다.
 */
void CM_SendSyncStates(void);

// --- BSP 계층에서 수신된 SDO 이벤트 데이터를 cm_api의 데이터 캐시에 업데이트하기 위한 함수들 (주로 BSP에서 사용) ---
/**
 * @brief 수신된 H10 Mode로 내부 데이터 캐시를 업데이트합니다.
 * @param[in] newMode 새로 수신된 H10 Mode 값.
 */
void CM_UpdateH10Mode(CM_H10Mode_t newMode);

/**
 * @brief 수신된 Assist Level로 내부 데이터 캐시를 업데이트합니다.
 * @param[in] newLevel 새로 수신된 Assist Level 값.
 */
void CM_UpdateAssistLevel(uint8_t newLevel);

/**
 * @brief P-Vector 완료 상태를 내부 데이터 캐시에 업데이트합니다.
 * @param[in] nodeId 완료된 관절의 Node ID.
 */
void CM_UpdatePVectorCompletedRH(uint8_t isCompleted);
void CM_UpdatePVectorCompletedLH(uint8_t isCompleted);

/**
 * @brief 중립각도 설정 완료 상태를 내부 데이터 캐시에 업데이트합니다.
 */
void CM_UpdateNeutralPosSet(void);

// --- 주 기능 API (주로 Apps 계층에서 사용) ---
/**
 * @brief 사용자 신체 정보 SDO를 전송합니다.
 * @param[in] bodyData 8개의 uint32_t 값을 담은 배열.
 */
void CM_SendUserBodyData(const uint32_t bodyData[8]);

/**
 * @brief 지정된 관절(Node)에 P-Vector 궤적 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (예: SYS_NODE_ID_RH).
 * @param[in] pVector   전송할 P-Vector 데이터를 담은 구조체 포인터.
 */
void CM_SendPVector(SystemNodeID_t nodeId, const CM_PVector_t* pVector);

/**
 * @brief 지정된 관절(Node)에 P-Vector Reset SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID (SYS_NODE_ID_RH 또는 SYS_NODE_ID_LH).
 */
void CM_SendPVectorReset(SystemNodeID_t nodeId);

/**
 * @brief (내부용) P-Vector 완료 상태 플래그를 false로 리셋합니다.
 * @details 이 함수는 상위 계층(xm_api)에서 이벤트를 소비했음을 알리기 위해 호출됩니다.
 * 스레드 안전(Thread-safe)을 위해 내부적으로 Critical Section을 사용합니다.
 * @param[in] nodeId 플래그를 리셋할 관절의 Node ID.
 */
void CM_ClearPVectorCompletedFlag(SystemNodeID_t nodeId);

/**
 * @brief 지정된 관절(Node)에 I-Vector 궤적 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] iVector   전송할 I-Vector 데이터를 담은 구조체 포인터.
 */
void CM_SendIVector(SystemNodeID_t nodeId, const CM_IVector_t* iVector);

/**
 * @brief 지정된 관절(Node)에 I-Vector 궤적 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] kpMax     전송할 I-Vector kp Max 값.
 * @param[in] kdMax     전송할 I-Vector kd Max 값.
 */
void CM_SendIVectorKpKdmax(SystemNodeID_t nodeId, const float kpMax, const float kdMax);

/**
 * @brief 지정된 관절(Node)에 F-Vector 궤적 SDO를 전송합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] fVector   전송할 F-Vector 데이터를 담은 구조체 포인터.
 */
void CM_SendFVector(SystemNodeID_t nodeId, const CM_FVector_t* fVector);

/**
 * @brief 지정된 관절(Node)의 각도 제한 루틴을 SDO로 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void CM_SendSetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet);

/**
 * @brief 지정된 관절(Node)의 각속도 제한 루틴을 SDO로 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void CM_SendSetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet);

/**
 * @brief 지정된 관절(Node)의 외란 관측기(DOB) 루틴을 SDO로 활성화/비활성화합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] isSet     루틴을 활성화하려면 `true`, 비활성화하려면 `false`.
 */
void CM_SendSetDOBRoutine(SystemNodeID_t nodeId, bool isSet);

/**
 * @brief 지정된 관절(Node)의 가동범위(ROM) 상/하한을 SDO로 설정합니다.
 * @param[in] nodeId      명령을 전달할 관절의 Node ID.
 * @param[in] upperLimit  설정할 가동범위 상한값 (단위: degree).
 * @param[in] lowerLimit  설정할 가동범위 하한값 (단위: degree).
 */
void CM_SendDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);

/**
 * @brief 지정된 관절(Node)의 가동속도범위 상/하한을 설정합니다.
 * @param[in] nodeId      명령을 전달할 관절의 Node ID.
 * @param[in] upperLimit  설정할 가동속도범위 상한값 (단위: deg/s).
 * @param[in] lowerLimit  설정할 가동속도범위 하한값 (단위: deg/s).
 */
void CM_SendVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);

/**
 * @brief 지정된 관절(Node)의 Normal Compensation Gain을 SDO로 설정합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] gain      설정할 게인 값.
 */
void CM_SendNormalCompGain(SystemNodeID_t nodeId, uint8_t gain);

/**
 * @brief 지정된 관절(Node)의 Resistive Compensation Gain을 SDO로 설정합니다.
 * @param[in] nodeId    명령을 전달할 관절의 Node ID.
 * @param[in] gain      설정할 저항 게인 값.
 */
void CM_SendResistiveCompGain(SystemNodeID_t nodeId, float gain);

/**
 * @brief H10의 기존 보조 모드를 설정하는 SDO를 전송합니다.
 * @param[in] H10AssistModeEnable H10 보조 모드 활성화 (0 비활성화 또는 1 활성화).
 */
void CM_SendSetH10AssistExistingMode(bool H10AssistModeEnable);

// --- PDO 관련 API ---
/**
 * @brief 지정된 관절(Node)의 보조 토크를 PDO 전송 대기열에 추가합니다.
 * @details 이 함수는 즉시 전송하지 않고, 전송할 데이터를 '스테이징'만 합니다.
 * 실제 전송은 CM_FlushControlPDOs() 호출 시 이루어집니다.
 * @param[in] nodeId    토크를 인가할 관절의 Node ID.
 * @param[in] torque    보조 토크 값 (단위: Nm).
 */
void CM_StageAuxTorque(SystemNodeID_t nodeId, float torque);

/**
 * @brief 대기열에 있는 모든 제어 관련 PDO를 하나의 CAN 메시지로 묶어 보냅니다.
 * @details 제어 루프의 마지막에 주기적으로 호출되어야 합니다.
 */
void CM_FlushControlPDOs(void);

#endif /* DEVICES_CONTROL_MODULE_INC_CM_DRV_H_ */
