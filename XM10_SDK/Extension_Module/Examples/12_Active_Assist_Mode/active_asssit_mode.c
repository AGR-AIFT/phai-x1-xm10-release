/**
 ******************************************************************************
 * @file    active_assist_mode.c
 * @author  Your Name
 * @brief   Active-Assist Mode 단일 동작 예제
 * @version 0.1
 * @date    Oct 2, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"           // XM API

#include <math.h>
#include <stdlib.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// --- default 설정 값 ---
#define JOINT_ANGLE_MAX_ANGLE_INT16	250  // + 방향 최대 각도(deg, 1/10)
#define JOINT_ANGLE_MIN_ANGLE_INT16	-250   // - 방향 최소 각도(deg, 1/10)

// --- Mode Change 설정 값 ---
#define MODE_TRANSITION_DELAY_MS    500 // 모드 전환 지연 시간 (ms)
#define STOP_CMD_DELAY_MS           100 // P vector Reset 명령 후 정지 명령까지의 지연 시간 (ms)

// --- Homing 설정 값 ---
#define HOMING_TRANSITION_DELAY_MS  50    // 각 단계 사이의 지연 시간 (50ms)
#define HOMING_SPEED_RH             150   // 초당 이동 속도 (deg/s)
#define HOMING_ACCEL_S0_RH          4     // 초기 가속도(deg/s^2)
#define HOMING_ACCEL_SD_RH          4     // 말기 가속도(deg/s^2)
#define HOMING_SPEED_LH             150   // 초당 이동 속도 (deg/s)
#define HOMING_ACCEL_S0_LH          4     // 초기 가속도(deg/s^2)
#define HOMING_ACCEL_SD_LH          4     // 말기 가속도(deg/s^2)

// --- Active-Assist Mode 설정 값 ---
#define ASSIST_TORQUE_NM                3.0f   // 최대 보조 토크 크기 (Nm)
#define INTENT_TRACKING_DELAY_MS        2000   // 보조력 제공 전 사용자 의도 추적 시간 (ms)
#define INTENT_ANGLE_THRESHOLD_DEG10    50     // 사용자 의도 감지 임계 각도 (5.0도)
#define MOVEMENT_START_THRESHOLD_DEG10  5      // 움직임 시작 감지 임계 각도 (0.5도)
#define TORQUE_SMOOTHING_FACTOR         0.005f // 토크 변화의 부드러움 (작을수록 부드러움)

// --- Stop 설정 값 ---
#define STOP_DURATION_MS    200    // 현재 위치에서 정지할 때까지 걸리는 시간 (ms)
#define STOP_ACCEL_S0_RH    4      // 초기 가속도(deg/s^2)
#define STOP_ACCEL_SD_RH    4      // 말기 가속도(deg/s^2)
#define STOP_DURATION_MS    200    // 현재 위치에서 정지할 때까지 걸리는 시간 (ms)
#define STOP_ACCEL_S0_LH    4      // 초기 가속도(deg/s^2)
#define STOP_ACCEL_SD_LH    4      // 말기 가속도(deg/s^2)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 보조 모드 간 전환 과정을 관리하는 상태
 */
typedef enum {
    MODE_TRANSITION_IDLE,          // 평상시 (전환 없음)
    MODE_TRANSITION_STOP_PENDING,  // 이전 궤적(P-Vector)의 정지 완료를 대기
    MODE_TRANSITION_STOP_COMPLETED,// 현재 위치 정지 완료 대기
    MODE_TRANSITION_DELAYING,      // 모드 변경 전/후의 안정화 지연
} ModeTransitionState_t;

 /**
 * @brief Homing(원점 복귀) 절차의 내부 동작 상태
 */
typedef enum {
    HOMING_ENTRY,                 // 1. 진입
    HOMING_SET_IMPEDANCE,         // 2. 임피던스 설정
    HOMING_START_MOTION,          // 3. 초기 각도를 향한 이동 시작
    HOMING_WAIT_FOR_DONE,         // 4. 이동 완료 대기
    HOMING_FINALIZE_DELAY,        // 5. 안정화 지연
    HOMING_FINALIZE_CLEANUP       // 6. 설정 정리 및 종료
} HomingState_t;

/**
 * @brief Active-Assist Mode의 전체적인 "상위" 상태
 * @details Homing과 같이 양쪽 다리가 동기화되어야 하는 동작과,
 * 각자 독립적으로 움직이는 보조 단계를 구분합니다.
 */
typedef enum {
    AA_STATE_HOMING,      // 초기 위치 정렬 단계
    AA_STATE_ASSISTING,   // 독립적인 토크 보조 단계
} ActiveAssistState_t;

/**
 * @brief Active-Assist Mode의 "하위" 동작 상태 (한쪽 다리 기준)
 */
typedef enum {
    // --- 토크 보조 단계 ---
    AA_SUBSTATE_WAIT_AT_PEAK,         // 1. 피크에서 사용자 움직임 대기
    AA_SUBSTATE_TRACKING_INTENT,      // 2. 사용자 움직임 의도 추적
    AA_SUBSTATE_PROVIDE_ASSIST_PF,    // 3. Plantar Flexion 방향으로 토크 보조
    AA_SUBSTATE_PROVIDE_ASSIST_DF,    // 4. Dorsiflexion 방향으로 토크 보조
} AA_SubState_t;

/**
 * @brief 한쪽 다리의 Active-Assist 상태 머신을 관리하는 모든 변수를 담는 구조체
 */
typedef struct {
    AA_SubState_t state;                  // 현재 하위 상태
    float         targetTorque_Nm;        // 목표 보조 토크
    float         currentTorque_Nm;       // 현재 보조 토크 (스무딩 적용)
    int16_t       anchorAngle_deg10;      // 움직임의 기준점이 된 피크 각도
    int16_t       maxAngleSincePeak_deg10;
    int16_t       minAngleSincePeak_deg10;
    uint32_t      intentTrackingTimer;
} ActiveAssistFsm_t;

/**
 * @brief 발목 관절의 최대 가동범위(ROM) 피크 도달 상태
 * @note  이 예제에서는 Peak 신호 GPIO는 사용하지 않지만, 내부 로직을 위해 사용됩니다.
 */
typedef enum {
    PEAK_REACHED_NONE,      // 피크에 도달하지 않은 중간 영역
    PEAK_REACHED_DORSI,     // Dorsiflexion (발등굽힘) 피크 도달
    PEAK_REACHED_PLANTAR    // Plantar Flexion (발바닥굽힘) 피크 도달
} PeakReachedState_t;

typedef struct __attribute__((packed)) {
	uint32_t loopCnt;
    uint8_t h10Mode;
	uint8_t h10AssistLevel;
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
} MyData_t;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

// --- Task State Machine Handle ---
static XmTsmHandle_t s_userHandle;
static uint32_t s_dataSaveLoopCnt;

// --- Mode State Management ---
static uint32_t s_modeTransitionTimer = 0;
static ModeTransitionState_t s_modeTransitionState = MODE_TRANSITION_IDLE;
static XmH10Mode_t s_previoush10Mode = XM_H10_MODE_STANDBY;

// --- Active Assist Mode State Management ---
static HomingState_t        s_homingState = HOMING_ENTRY;
static ActiveAssistState_t  s_aaGlobalState = AA_STATE_HOMING; // AA모드의 전체 상위 상태
static ActiveAssistFsm_t    s_aaFsm_RH;  // 오른쪽 다리(RH)를 위한 상태 머신 객체
static ActiveAssistFsm_t    s_aaFsm_LH;  // 왼쪽 다리(LH)를 위한 상태 머신 객체

// --- For Dat Save ---
static bool s_debug_USB_metData = false;
static MyData_t myData;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

// --- Task State Machine Functions ---
static void Off_Loop(void);

static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

// --- Mode Management ---
static void ManageModeTransition(void);
static void ExecuteActiveAssistMode(void);
static bool IsDuringModeTransition(void);
static void StopMotorAndHold(void);

// --- Mode Implementations ---
static void EnterStandbyMode(void);
static void EnterActiveAssistMode(void);
static void InitializeFsm(ActiveAssistFsm_t* fsm);
static void UpdateActiveAssistMode(void);
static void UpdateSingleLegAssistLogic(ActiveAssistFsm_t* fsm, float currentThighAngle_deg, SystemNodeID_t nodeId);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */
bool xsensIMUenableRes = false;
/**
 * @brief Active-Assist Mode 예제 애플리케이션을 초기화합니다.
 * @details 시스템 부팅 시 Main 태스크에서 단 한 번만 호출되어야 합니다.
 * 내부적으로 Task State Machine을 생성하고 각 상태에 맞는 함수들을 등록합니다.
 */
void User_Setup(void)
{
    // 태스크를 생성하고 핸들을 받아옵니다. 
    // (Task 최대 생성 수 : 10)
    // (States 최대 생성 수 : 64)

    // [생성] 초기 상태는 OFF
    s_userHandle = XM_TSM_Create(XM_STATE_OFF);

    // [등록 1] OFF 상태 설정
    XmStateConfig_t off_conf = {
        .id = XM_STATE_OFF,
        .on_loop  = Off_Loop
        // .on_enrty, .on_exit는 없으므로 생략 (자동 NULL)
    };
    XM_TSM_AddState(s_userHandle, &off_conf);

    // [등록 2] STANDBY 상태 설정
    XmStateConfig_t sb_conf = {
        .id = XM_STATE_STANDBY,
        .on_loop  = Standby_Loop
    };
    XM_TSM_AddState(s_userHandle, &sb_conf);

    // [등록 3] STANDBY 상태 설정
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_userHandle, &act_conf);

    // 외부 XSENS IMU 사용 설정
    if (XM_EnableExternalImu()) {
        // IMU 활성화 성공! (이제 UART4로 데이터가 들어옴)
        xsensIMUenableRes = true;
    } else {
        // 실패 처리 (이미 켜져있거나 하드웨어 오류)
    }

    // 로깅할 때 'myData' 구조체를 저장하겠다!
    XM_SetUsbLogSource(&myData, sizeof(MyData_t));
    
    // 스트리밍할 때도 'myData'를 보내겠다! (서로 달라도 됨)
    XM_SetUsbStreamSource(&myData, sizeof(MyData_t));
}

/*
 * @brief Active-Assist Mode 예제 애플리케이션을 주기적으로 실행합니다.
 * @details Main 태스크의 제어 루프(예: 2ms)에서 계속 호출되어야 합니다.
 */
void User_Loop(void)
{
    // CM 연결 상태를 최우선으로 확인하여, 연결이 끊겼을 경우 OFF 상태로 강제 전환합니다.
    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_userHandle, XM_STATE_OFF);
    }

    // Task State Machine 프레임워크를 통해 현재 상태에 맞는 Run 함수를 실행합니다.
    XM_TSM_Run(s_userHandle);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

// -------------------- Task State Machine Functions --------------------
/**
 * @brief OFF 상태: CM 연결을 기다립니다.
 */
static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_userHandle, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태: 보조 시작 명령을 기다립니다.
 */
static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_userHandle, XM_STATE_ACTIVE);
    }
}

/**
 * @brief ACTIVE 상태 진입: AA 모드 관련 변수를 초기화합니다.
 */
static void Active_Entry(void)
{
    s_dataSaveLoopCnt = 0;
    s_modeTransitionState = MODE_TRANSITION_IDLE;
    s_previoush10Mode = XM.status.h10.h10Mode;
    // 초기화(Enter) 함수를 호출합니다.
    EnterActiveAssistMode();

    if (XM_IsUsbLogReady()) {
        // "/LOGS/TestRun_001" 폴더를 만들고 "metadata.txt"를 생성함
        // C언어 문자열 연결 기능을 사용하여 깔끔하게 작성
        // 각 줄 끝에 공백이나 쉼표가 빠지지 않도록 주의하세요.
        // meta data를 저장하면서 log status를 LOG_STATUS_LOGGING으로 변경하여 데이터 저장을 수행할 수 있음.
        s_debug_USB_metData = XM_StartUsbDataLog("TestRun_001", 
            "dataSaveLoopCnt, h10Mode, h10AssistLevel,"
            "leftHipAngle, rightHipAngle, leftThighAngle, rightThighAngle,"
            "pelvicAngle, pelvicVelY, leftKneeAngle, rightKneeAngle, isLeftFootContact,"
            "isRightFootContact, gaitState, gaitCycle, forwardVelocity, leftHipTorque,"
            "rightHipTorque, leftHipMotorAngle, rightHipMotorAngle, leftHipImuFrontalRoll,"
            "leftHipImuSagittalPitch, rightHipImuFrontalRoll, rightHipImuSagittalPitch,"
            "leftHipImuGlobalAccX, leftHipImuGlobalAccY, leftHipImuGlobalAccZ,"
            "leftHipImuGlobalGyrX, leftHipImuGlobalGyrY, leftHipImuGlobalGyrZ,"
            "rightHipImuGlobalAccX, rightHipImuGlobalAccY, rightHipImuGlobalAccZ,"
            "rightHipImuGlobalGyrX, rightHipImuGlobalGyrY, rightHipImuGlobalGyrZ\n"); // 마지막에만 \n 추가
    }

    XM_SetControlMode(XM_CTRL_TORQUE);
}

/**
 * @brief ACTIVE 상태 실행: 모드 전환 관리 및 AA 모드 로직을 수행합니다.
 */
static void Active_Loop(void)
{
    // 모드 변경 감지 및 전환 절차 진행
    ManageModeTransition();
    // 실제 보조 모드 알고리즘 실행
    ExecuteActiveAssistMode();

    // 저장할 데이터 채우기
    myData.loopCnt = s_dataSaveLoopCnt;
    myData.h10Mode = (uint8_t)XM.status.h10.h10Mode;
    myData.h10AssistLevel = XM.status.h10.h10AssistLevel;
    myData.leftHipAngle = XM.status.h10.leftHipAngle;
    myData.rightHipAngle = XM.status.h10.rightHipAngle;
    myData.leftThighAngle = XM.status.h10.leftThighAngle;
    myData.rightThighAngle = XM.status.h10.rightThighAngle;
    myData.pelvicAngle = XM.status.h10.pelvicAngle;
    myData.pelvicVelY = XM.status.h10.pelvicVelY;
    myData.leftKneeAngle = XM.status.h10.leftKneeAngle;
    myData.rightKneeAngle = XM.status.h10.rightKneeAngle;
    myData.isLeftFootContact = XM.status.h10.isLeftFootContact;
    myData.isRightFootContact = XM.status.h10.isRightFootContact;
    myData.gaitState = XM.status.h10.gaitState;
    myData.gaitCycle = XM.status.h10.gaitCycle;
    myData.forwardVelocity = XM.status.h10.forwardVelocity;
    myData.leftHipTorque = XM.status.h10.leftHipTorque;
    myData.rightHipTorque = XM.status.h10.rightHipTorque;
    myData.leftHipMotorAngle = XM.status.h10.leftHipMotorAngle;
    myData.rightHipMotorAngle = XM.status.h10.rightHipMotorAngle;
    myData.leftHipImuFrontalRoll = XM.status.h10.leftHipImuFrontalRoll;
    myData.leftHipImuSagittalPitch = XM.status.h10.leftHipImuSagittalPitch;
    myData.rightHipImuFrontalRoll = XM.status.h10.rightHipImuFrontalRoll;
    myData.rightHipImuSagittalPitch = XM.status.h10.rightHipImuSagittalPitch;
    myData.leftHipImuGlobalAccX = XM.status.h10.leftHipImuGlobalAccX;
    myData.leftHipImuGlobalAccY = XM.status.h10.leftHipImuGlobalAccY;
    myData.leftHipImuGlobalAccZ = XM.status.h10.leftHipImuGlobalAccZ;
    myData.leftHipImuGlobalGyrX = XM.status.h10.leftHipImuGlobalGyrX;
    myData.leftHipImuGlobalGyrY = XM.status.h10.leftHipImuGlobalGyrY;
    myData.leftHipImuGlobalGyrZ = XM.status.h10.leftHipImuGlobalGyrZ;
    myData.rightHipImuGlobalAccX = XM.status.h10.rightHipImuGlobalAccX;
    myData.rightHipImuGlobalAccY = XM.status.h10.rightHipImuGlobalAccY;
    myData.rightHipImuGlobalAccZ = XM.status.h10.rightHipImuGlobalAccZ;
    myData.rightHipImuGlobalGyrX = XM.status.h10.rightHipImuGlobalGyrX;
    myData.rightHipImuGlobalGyrY = XM.status.h10.rightHipImuGlobalGyrY;
    myData.rightHipImuGlobalGyrZ = XM.status.h10.rightHipImuGlobalGyrZ;
    s_dataSaveLoopCnt++;
}

static void Active_Exit(void)
{
	if (XM_GetUsbLogStatus() == XM_LOG_STATUS_LOGGING) {
	    XM_StopUsbDataLog();
	}
}

// -------------------- Mode Management --------------------
/**
 * @brief SUIT 모드 변경을 감지하고, AA Mode -> Standby 전환을 처리합니다.
 * @details 이 예제에서는 오직 Assist Mode(Active-Assist Mode) -> Standby Mode 전환만 처리합니다.
 */
static void ManageModeTransition(void)
{
	XmH10Mode_t currenth10Mode = XM.status.h10.h10Mode;

    switch (s_modeTransitionState) {
        case MODE_TRANSITION_IDLE: 
            {
                // 평상시에 모드 변경이 감지되었는지 확인합니다.
                if (currenth10Mode != s_previoush10Mode) {
                    
                    // Active-Assist Mode -> Standby Mode 로의 전환
                    // [CASE 1] Homing 중 P-Vector를 사용하던 AA Mode를 안전하게 정지시키는 절차를 시작합니다.
                    if (s_previoush10Mode == XM_H10_MODE_ASSIST && currenth10Mode == XM_H10_MODE_STANDBY
                        && s_aaGlobalState == AA_STATE_HOMING) {
                        XM_SendPVectorReset(SYS_NODE_ID_RH);   // P-Vector 궤적 생성 취소 명령 전송
                        XM_SendPVectorReset(SYS_NODE_ID_LH);
                        s_modeTransitionTimer = XM_GetTick();  // reset 지연 타이머 시작
                        s_modeTransitionState = MODE_TRANSITION_STOP_PENDING; // 다음 상태로 전환
                    }
                    // [CASE 2] Homing 중이 아닐 때, Active-Assist Mode -> Standby Mode로의 전환
                    else if (s_previoush10Mode == XM_H10_MODE_ASSIST && currenth10Mode == XM_H10_MODE_STANDBY) {
                        // 별도의 정지 절차 없이, 바로 안정화 지연 단계로 넘어갑니다.
                        s_modeTransitionTimer = XM_GetTick();
                        s_modeTransitionState = MODE_TRANSITION_DELAYING;
                    }
                }
            }
            break;
        
        case MODE_TRANSITION_STOP_PENDING: 
            {
                // 취소 명령 후 지연 시간 지났는지 확인
                if (XM_GetTick() - s_modeTransitionTimer >= STOP_CMD_DELAY_MS) {
                    // 현재 위치에 정지하도록 P-Vector 전송
                    StopMotorAndHold();

                    // 메인 지연 타이머 시작 및 다음 상태로 전환
                    s_modeTransitionTimer = XM_GetTick();
                    s_modeTransitionState = MODE_TRANSITION_STOP_COMPLETED;
                }
            }
            break;
            
        case MODE_TRANSITION_STOP_COMPLETED: 
            {
                // P-Vector 정지 명령이 양쪽 모두 완료되었는지 확인합니다.
                if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                    
                    // 2. 정지가 완료되면, 안정화 지연 단계로 넘어갑니다.
                    s_modeTransitionTimer = XM_GetTick();
                    s_modeTransitionState = MODE_TRANSITION_DELAYING;
                }
            }
            break;
            
        case MODE_TRANSITION_DELAYING: 
            {
                // 모드 변경 전/후의 안정화를 위해 일정 시간 대기합니다.
                if (XM_GetTick() - s_modeTransitionTimer >= MODE_TRANSITION_DELAY_MS) {

                    // 초기화(Enter) 함수를 호출합니다.
                    EnterStandbyMode();
                    
                    // 모든 전환 절차가 완료되었으므로, 현재 모드를 기록하고 IDLE 상태로 복귀합니다.
                    s_previoush10Mode = currenth10Mode;
                    s_modeTransitionState = MODE_TRANSITION_IDLE;
                    XM_TSM_TransitionTo(s_userHandle, XM_STATE_STANDBY);
                }
            }
            break;
    }
}

/**
 * @brief 현재 Assist Mode에 맞는 함수를 실행합니다. (AA 모드 전용)
 */
static void ExecuteActiveAssistMode(void)
{
    if (IsDuringModeTransition()) {
        return;
    }
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        UpdateActiveAssistMode();
    }
}

/**
 * @brief 현재 모드 전환 절차가 진행 중인지 여부를 반환합니다.
 */
static bool IsDuringModeTransition(void)
{
    return (s_modeTransitionState != MODE_TRANSITION_IDLE);
}

/**
 * @brief 현재 진행 중인 P_Vector 움직임을 중단하고 현재 위치에 부드럽게 정지시킵니다.
 */
static void StopMotorAndHold(void)
{
    // 현재 모터의 정확한 각도를 읽어옵니다.
    int16_t currentAngleRH_ForStop = (int16_t)round(XM.status.h10.rightHipMotorAngle * 10.0f);
    int16_t currentAngleLH_ForStop = (int16_t)round(XM.status.h10.leftHipMotorAngle * 10.0f);

    // 목표 위치(yd)를 현재 위치로 설정하여 P_Vector 전송
    PVector_t pVecRH = { .yd = currentAngleRH_ForStop, .L = STOP_DURATION_MS, .s0 = STOP_ACCEL_S0_RH, .sd = STOP_ACCEL_SD_RH };
    PVector_t pVecLH = { .yd = currentAngleLH_ForStop, .L = STOP_DURATION_MS, .s0 = STOP_ACCEL_S0_LH, .sd = STOP_ACCEL_SD_LH };
    XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
    XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);
    
    // 진행 중인 P_Vector가 있다면(Homing 중 Stop), 완료 플래그를 false로 설정하여
    // 새로운 정지 명령이 진행 중임을 알립니다.
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
}

// -------------------- Mode Implementations --------------------
/**
 * @brief Standby(No Assist) 모드 진입 시 호출됩니다.
 */
static void EnterStandbyMode(void)
{
    // 모든 토크를 0으로 설정하고 임피던스를 해제
	XM_SetAssistTorqueRH(0.0f);
	XM_SetAssistTorqueLH(0.0f);
    InitializeFsm(&s_aaFsm_RH);
    InitializeFsm(&s_aaFsm_LH);
    
    if (s_aaGlobalState == AA_STATE_HOMING) {
        IVector_t zeroImpedance = {.epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50};
        XM_SendIVector(SYS_NODE_ID_RH, &zeroImpedance);
        XM_SendIVector(SYS_NODE_ID_LH, &zeroImpedance);
        XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
        XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
    }
}

/**
 * @brief Active-Assist Mode 진입 시 호출됩니다.
 */
static void EnterActiveAssistMode(void)
{
	XM_SetAssistTorqueRH(0.0f);
	XM_SetAssistTorqueLH(0.0f);
    InitializeFsm(&s_aaFsm_RH);
    InitializeFsm(&s_aaFsm_LH);
    // Homing 절차부터 시작
    s_homingState = HOMING_ENTRY;
    s_aaGlobalState = AA_STATE_HOMING;
}

/**
 * @brief Active-Assist Mode FSM 초기화.
 */
static void InitializeFsm(ActiveAssistFsm_t* fsm)
{
    fsm->state                   = AA_SUBSTATE_WAIT_AT_PEAK; // 특정 초기 상태 지정
    fsm->targetTorque_Nm         = 0.0f;
    fsm->currentTorque_Nm        = 0.0f;
    fsm->anchorAngle_deg10       = 0;
    fsm->maxAngleSincePeak_deg10 = 0;
    fsm->minAngleSincePeak_deg10 = 0;
    fsm->intentTrackingTimer     = 0;
}

/**
 * @brief Active-Assist Mode의 메인 상태 머신 (상위 디스패처)
 */
static void UpdateActiveAssistMode(void)
{
    static uint32_t homingTimer = 0;
    switch (s_aaGlobalState) {
        case AA_STATE_HOMING:
            switch (s_homingState) {
                // --- Homing ---
                case HOMING_ENTRY: {
                    XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 1);
                    XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6, 1);
                    s_homingState = HOMING_SET_IMPEDANCE;
                    break;
                }

                case HOMING_SET_IMPEDANCE: {
                    IVector_t stiffImpedance = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
                    XM_SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
                    XM_SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
                    s_homingState = HOMING_START_MOTION;
                    break;
                }

                case HOMING_START_MOTION: {
                    int16_t targetAngle = JOINT_ANGLE_MIN_ANGLE_INT16;

                    // 현재 각도를 기준으로 target각도까지 이동하는 P-Vector 전송
                    int16_t currentAngleRH_ForHoming = (int16_t)round(XM.status.h10.rightHipMotorAngle * 10.0f);
                    int16_t currentAngleLH_ForHoming = (int16_t)round(XM.status.h10.leftHipMotorAngle * 10.0f);

                    // 이동할 각도 계산 (절대값)
                    int16_t angleToMoveRH = abs(targetAngle - currentAngleRH_ForHoming);
                    int16_t angleToMoveLH = abs(targetAngle - currentAngleLH_ForHoming);
                    // 이동 속도(HOMING_SPEED_RH/LH)를 기반으로 이동 시간(duration_RH/LH) 계산
                    uint16_t durationRH = (uint16_t)(((float)angleToMoveRH / (float)HOMING_SPEED_RH) * 1000.0f);
                    uint16_t durationLH = (uint16_t)(((float)angleToMoveLH / (float)HOMING_SPEED_RH) * 1000.0f);

                    PVector_t pVecRH = { .yd = targetAngle, .L = durationRH, .s0 = HOMING_ACCEL_S0_RH, .sd = HOMING_ACCEL_SD_RH };
                    PVector_t pVecLH = { .yd = targetAngle, .L = durationLH, .s0 = HOMING_ACCEL_S0_RH, .sd = HOMING_ACCEL_SD_RH };
                    XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
                    XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);
                    s_homingState = HOMING_WAIT_FOR_DONE;
                    break;
                }
                    
                case HOMING_WAIT_FOR_DONE: {
                    if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                        XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                        XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                        s_homingState = HOMING_FINALIZE_DELAY;
                    }
                    break;
                }
                
                case HOMING_FINALIZE_DELAY:
                    // 설정 해제 후 짧은 안정화 시간(HOMING_TRANSITION_DELAY_MS) 대기
                    if (XM_GetTick() - homingTimer >= HOMING_TRANSITION_DELAY_MS) {
                        IVector_t zeroImpedance = {.epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50};
                        XM_SendIVector(SYS_NODE_ID_RH, &zeroImpedance);
                        XM_SendIVector(SYS_NODE_ID_LH, &zeroImpedance);
                        s_homingState = HOMING_FINALIZE_CLEANUP;
                    }
                    break;

                case HOMING_FINALIZE_CLEANUP:
                    // 모든 Homing 절차가 끝났으므로, 토크 보조 단계로 전환 대기
                    break;
            }
            // Homing이 완료되면 assist 상태로 전환
            if (s_homingState == HOMING_FINALIZE_CLEANUP) {
                // 양쪽 다리의 상태 머신을 보조 대기 상태로 초기화
                s_aaFsm_RH.state = AA_SUBSTATE_WAIT_AT_PEAK;
                s_aaFsm_LH.state = AA_SUBSTATE_WAIT_AT_PEAK;
                s_aaFsm_RH.anchorAngle_deg10 = XM.status.h10.rightThighAngle; // 허벅지 각도의 Homing 완료 위치를 기준점으로
                s_aaFsm_LH.anchorAngle_deg10 = XM.status.h10.leftThighAngle;

                s_aaGlobalState = AA_STATE_ASSISTING;
            }
            break;

        case AA_STATE_ASSISTING: {
            // Homing이 완료되면, 각 다리의 로직을 독립적으로 실행
            UpdateSingleLegAssistLogic(&s_aaFsm_RH, XM.status.h10.rightThighAngle, SYS_NODE_ID_RH);
            UpdateSingleLegAssistLogic(&s_aaFsm_LH, XM.status.h10.leftThighAngle, SYS_NODE_ID_LH);
            break;
        }
    }
}

/**
 * @brief (신규) 한쪽 다리의 Active-Assist 로직을 독립적으로 처리하는 함수
 * @param[in,out] fsm               해당 다리의 상태 머신 객체 포인터
 * @param[in]     currentThighAngle 해당 다리의 현재 허벅지 각도
 * @param[in]     nodeId            해당 다리의 노드 ID (RH/LH)
 */
static void UpdateSingleLegAssistLogic(ActiveAssistFsm_t* fsm, float currentThighAngle_deg, SystemNodeID_t nodeId)
{
    int16_t currentAngle_deg10 = (int16_t)round(currentThighAngle_deg * 10.0f);
    
    // LPF를 이용한 토크 스무딩
    fsm->currentTorque_Nm = (fsm->targetTorque_Nm * TORQUE_SMOOTHING_FACTOR) +
                            (fsm->currentTorque_Nm * (1.0f - TORQUE_SMOOTHING_FACTOR));
    if (nodeId == SYS_NODE_ID_RH) {
    	XM_SetAssistTorqueRH(((float)XM.status.h10.h10AssistLevel / 10.0f) * fsm->currentTorque_Nm);
    } else if (nodeId == SYS_NODE_ID_LH) {
    	XM_SetAssistTorqueLH(((float)XM.status.h10.h10AssistLevel / 10.0f) * fsm->currentTorque_Nm);
    }

    switch (fsm->state) {
        case AA_SUBSTATE_WAIT_AT_PEAK: {
            fsm->targetTorque_Nm = 0.0f;
            if (abs(currentAngle_deg10 - fsm->anchorAngle_deg10) > MOVEMENT_START_THRESHOLD_DEG10) {
                fsm->intentTrackingTimer = XM_GetTick();
                fsm->maxAngleSincePeak_deg10 = currentAngle_deg10;
                fsm->minAngleSincePeak_deg10 = currentAngle_deg10;
                fsm->state = AA_SUBSTATE_TRACKING_INTENT;
            }
            break;
        }
        case AA_SUBSTATE_TRACKING_INTENT: {
            if (currentAngle_deg10 > fsm->maxAngleSincePeak_deg10) fsm->maxAngleSincePeak_deg10 = currentAngle_deg10;
            if (currentAngle_deg10 < fsm->minAngleSincePeak_deg10) fsm->minAngleSincePeak_deg10 = currentAngle_deg10;
            
            bool isTimeout = (XM_GetTick() - fsm->intentTrackingTimer >= INTENT_TRACKING_DELAY_MS);
            bool isThresholdPassed = false;
            int8_t direction = 0;
            
            // 단방향 진행(Threshold) 조건 확인
            // [조건 A] 현재 (-) 피크에서 대기 중인가?
            // anchorAngle이 0보다 작거나 같으면 (-) 피크에서 출발한 것으로 간주
            if (fsm->anchorAngle_deg10 <= 0) { // (-) 피크에서 출발
                if ((fsm->maxAngleSincePeak_deg10 - fsm->anchorAngle_deg10) > INTENT_ANGLE_THRESHOLD_DEG10) {
                    isThresholdPassed = true; direction = 1; // (+) 방향
                }
            } 
            // [조건 B] 현재 (+) 피크에서 대기 중인가?
            // anchorAngle이 0보다 크면 (+) 피크에서 출발한 것으로 간주
            else { // (+) 피크에서 출발
                if ((fsm->anchorAngle_deg10 - fsm->minAngleSincePeak_deg10) > INTENT_ANGLE_THRESHOLD_DEG10) {
                    isThresholdPassed = true; direction = -1; // (-) 방향
                }
            }
            
            // Peak에 도달하면 다시 대기 상태로
            if ((direction == 1 && currentAngle_deg10 >= JOINT_ANGLE_MAX_ANGLE_INT16) ||
                (direction == -1 && currentAngle_deg10 <= JOINT_ANGLE_MIN_ANGLE_INT16)) {
                fsm->anchorAngle_deg10 = currentAngle_deg10;
                fsm->state = AA_SUBSTATE_WAIT_AT_PEAK;
            } else if (isTimeout && isThresholdPassed) { // 조건 만족 시 보조 시작
                fsm->state = (direction == 1) ? AA_SUBSTATE_PROVIDE_ASSIST_DF : AA_SUBSTATE_PROVIDE_ASSIST_PF;
            }
            break;
        }
        case AA_SUBSTATE_PROVIDE_ASSIST_DF:
        case AA_SUBSTATE_PROVIDE_ASSIST_PF: {
            fsm->targetTorque_Nm = (fsm->state == AA_SUBSTATE_PROVIDE_ASSIST_PF) ? -ASSIST_TORQUE_NM : ASSIST_TORQUE_NM;
            
            // 반대편 Peak에 도달하면 다시 대기 상태로
            if ((fsm->state == AA_SUBSTATE_PROVIDE_ASSIST_PF && currentAngle_deg10 <= JOINT_ANGLE_MIN_ANGLE_INT16) ||
                (fsm->state == AA_SUBSTATE_PROVIDE_ASSIST_DF && currentAngle_deg10 >= JOINT_ANGLE_MAX_ANGLE_INT16)) {
                fsm->anchorAngle_deg10 = currentAngle_deg10;
                fsm->state = AA_SUBSTATE_WAIT_AT_PEAK;
            }
            break;
        }
    }
}
