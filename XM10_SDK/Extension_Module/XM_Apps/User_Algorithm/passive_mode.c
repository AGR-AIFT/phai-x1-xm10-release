/**
 ******************************************************************************
 * @file    passive_mode.c
 * @author  HyundoKim
 * @brief   
 * @version 0.1
 * @date    Nov 18, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"           // XM API

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

// --- default 설정 값 ---
#define JOINT_ANGLE_MAX_ANGLE_INT16	250  // + 방향 최대 각도(deg, 1/10)
#define JOINT_ANGLE_MIN_ANGLE_INT16	-250   // - 방향 최소 각도(deg, 1/10)

// --- Homing 설정 값 ---
#define HOMING_TRANSITION_DELAY_MS  50    // 각 단계 사이의 지연 시간 (50ms)
#define HOMING_SPEED_RH             150   // 초당 이동 속도 (deg/s)
#define HOMING_ACCEL_S0_RH          4     // 초기 가속도(deg/s^2)
#define HOMING_ACCEL_SD_RH          4     // 말기 가속도(deg/s^2)
#define HOMING_SPEED_LH             150   // 초당 이동 속도 (deg/s)
#define HOMING_ACCEL_S0_LH          4     // 초기 가속도(deg/s^2)
#define HOMING_ACCEL_SD_LH          4     // 말기 가속도(deg/s^2)

// --- Mode Change 설정 값 ---
#define MODE_TRANSITION_DELAY_MS    500 // 모드 전환 지연 시간 (ms) 설정
#define STOP_CMD_DELAY_MS           100 // P vector Reset 명령 후 정지 명령까지의 지연 시간 (ms)
#define STOP_DURATION_MS            100 // 현재 위치에서 정지할 때까지 걸리는 시간 (ms)

// --- PMode 설정 값 ---
#define PM_SPEED_RH         150    // 초당 이동 속도 (deg/s)
#define PM_ACCEL_S0_RH      4      // 초기 가속도(deg/s^2)
#define PM_ACCEL_SD_RH      4      // 말기 가속도(deg/s^2)
#define PM_SPEED_LH         150    // 초당 이동 속도 (deg/s)
#define PM_ACCEL_S0_LH      4      // 초기 가속도(deg/s^2)
#define PM_ACCEL_SD_LH      4      // 말기 가속도(deg/s^2)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

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
 * @brief 보조 모드 간 전환 과정을 관리하는 상태
 */
typedef enum {
    MODE_TRANSITION_IDLE,          // 평상시 (전환 없음)
    MODE_TRANSITION_STOP_PENDING,  // 이전 궤적(P-Vector)의 정지 완료를 대기
    MODE_TRANSITION_STOP_COMPLETED,// 현재 위치 정지 완료 대기
    MODE_TRANSITION_DELAYING,      // 모드 변경 전/후의 안정화 지연
} ModeTransitionState_t;

/**
 * @brief Passive Mode 내부 동작 상태
 */
typedef enum {
    PASSIVE_STATE_SET_IMPEDANCE,    // 진입 초기 상태 (DOB, Impedance 설정)
    PASSIVE_STATE_START_MOTION,     // 첫 왕복 운동 시작
    PASSIVE_STATE_MOVING_TO_MAX,    // + 방향 최대각도로 이동
    PASSIVE_STATE_MOVING_TO_MIN,    // - 방향 최대각도로 이동
} PassiveState_t;

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
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

// --- Task State Machine Handle ---
static XmTsmHandle_t s_userHandle;
static uint32_t s_dataSaveLoopCnt;

// --- Init Homing State Management ---
static HomingState_t s_homingState = HOMING_ENTRY;

// --- Mode Transition Management ---
static uint32_t s_modeTransitionTimer = 0;
static ModeTransitionState_t s_modeTransitionState = MODE_TRANSITION_IDLE;
static XmH10Mode_t s_previousSuitMode = XM_H10_MODE_STANDBY;

// --- Passive Mode ---
static PassiveState_t s_passiveState = PASSIVE_STATE_SET_IMPEDANCE;

// --- For Dat Save ---
static bool s_debug_USB_metData = false;
static MyData_t myData;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

// --- State Machine Functions ---
static void Off_Loop(void);

static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

// --- Init Homing ---
static void InitHoming(void);

// --- Mode Management ---
static void ManageModeTransition(void);
static void ExecutePassiveMode(void);
static bool IsDuringModeTransition(void);
static void StopMotorAndHold(void);

// --- Mode Implementations ---
static void EnterStandbyMode(void);
static void ExecutePassiveMode(void);
static void EnterPassiveMode(void);
static void UpdatePassiveMode(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
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

    // 로깅할 때 'myData' 구조체를 저장하겠다!
    XM_SetUsbLogSource(&myData, sizeof(MyData_t));
    
    // 스트리밍할 때도 'myData'를 보내겠다! (서로 달라도 됨)
    XM_SetUsbStreamSource(&myData, sizeof(MyData_t));

    // 기본적으로 XM_CTRL_MONITOR 모드이므로 굳이 Set하지 않아도 됨
    XM_SetControlMode(XM_CTRL_MONITOR);
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
 * @brief OFF 상태에서 매 주기마다 실행됩니다.
 * @details CM과 연결이 수립되기를 기다립니다.
 */
static void Off_Loop(void)
{
    // XM_IsCmConnected()로 CM 연결 상태 확인
    if (XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_userHandle, XM_STATE_STANDBY);
    }
}

/**
 * @brief STANDBY 상태에서 매 주기마다 실행됩니다.
 */
static void Standby_Loop(void)
{
    // CM이 보조 모드를 요청하면 ACTIVE 상태로 전환
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        InitHoming();
    }
}

/**
 * @brief ACTIVE 상태에 처음 진입할 때 단 한 번 호출됩니다.
 */
static void Active_Entry(void)
{
    // 모드 전환 로직을 초기화하여, 첫 보조 모드부터 시작하도록 준비
    s_modeTransitionState = MODE_TRANSITION_IDLE;
    s_previousSuitMode = XM.status.h10.h10Mode;
    EnterPassiveMode();

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
}

/**
 * @brief ACTIVE 상태에서 매 주기마다 실행됩니다.
 */
static void Active_Loop(void)
{
    // 모드 변경 감지 및 전환 절차 진행
    ManageModeTransition();
    
    // 실제 보조 모드 알고리즘 실행
    ExecutePassiveMode();

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

// -------------------- Init Homing --------------------
static void InitHoming(void)
{
    static uint32_t homingTimer = 0;
    // --- Homing 상태 머신 ---
    switch (s_homingState) {
        case HOMING_ENTRY:
            XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 1);
            XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6, 1);
            s_homingState = HOMING_SET_IMPEDANCE;
            break;

        case HOMING_SET_IMPEDANCE: {
            // 위치 제어를 위한 임피던스(강성) 설정(epsilon 0도, kP 5%, kd 1%)
            IVector_t stiffImpedance = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
            XM_SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
            XM_SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
            s_homingState = HOMING_START_MOTION;
            break;
        }
        case HOMING_START_MOTION: {
            int16_t targetAngle = 0;    // Homing 목표 각도는 0도

            // 현재 각도를 기준으로 0도까지 이동하는 P-Vector 전송
            int16_t currentAngleRH_degx10 = (int16_t)round(XM.status.h10.rightHipMotorAngle * 10.0f);
            int16_t currentAngleLH_degx10 = (int16_t)round(XM.status.h10.leftHipMotorAngle * 10.0f);
            
            // 이동할 각도 계산 (절대값)
            int16_t angleToMoveRH = abs(targetAngle - currentAngleRH_degx10);
            int16_t angleToMoveLH = abs(targetAngle - currentAngleLH_degx10);
            // 이동 속도(HOMING_SPEED_RH/LH)를 기반으로 이동 시간(duration_RH/LH) 계산
            uint16_t durationRH = (uint16_t)(((float)angleToMoveRH / (float)HOMING_SPEED_RH) * 1000.0f);
            uint16_t durationLH = (uint16_t)(((float)angleToMoveLH / (float)HOMING_SPEED_LH) * 1000.0f);

            PVector_t homingVecRH = { .yd = targetAngle, .L = durationRH, .s0 = HOMING_ACCEL_S0_RH, .sd = HOMING_ACCEL_SD_RH };
            PVector_t homingVecLH = { .yd = targetAngle, .L = durationLH, .s0 = HOMING_ACCEL_S0_LH, .sd = HOMING_ACCEL_SD_LH };
            XM_SendPVector(SYS_NODE_ID_RH, &homingVecRH);
            XM_SendPVector(SYS_NODE_ID_LH, &homingVecLH);

            s_homingState = HOMING_WAIT_FOR_DONE;
            break;
        }
        case HOMING_WAIT_FOR_DONE:
            // Homing P-Vector가 완료되었는지 XM.status.h10 캐시를 통해 확인
            if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                
                homingTimer = XM_GetTick(); // 짧은 지연을 위한 타이머 시작
                s_homingState = HOMING_FINALIZE_DELAY;
            }
            break;

        case HOMING_FINALIZE_DELAY:
            // 설정 해제 후 짧은 안정화 시간(HOMING_TRANSITION_DELAY_MS) 대기
            if (XM_GetTick() - homingTimer >= HOMING_TRANSITION_DELAY_MS) {
                s_homingState = HOMING_FINALIZE_CLEANUP;
            }
            break;
        
        case HOMING_FINALIZE_CLEANUP:
            // Homing 완료, Task State를 STANDBY로 전환
            XM_TSM_TransitionTo(s_userHandle, XM_STATE_ACTIVE);
            
            // 다음 Homing을 위해 상태 초기화
            s_homingState = HOMING_ENTRY;
            break;
    }
}

// -------------------- Mode Management --------------------
/**
 * @brief SUIT 모드 변경을 감지하고, Passive Mode를 안전하게 종료하는 절차를 관리합니다.
 * @details 이 예제에서는 오직 Assist Mode(Passive Mode) -> Standby Mode 전환만 처리합니다.
 */
static void ManageModeTransition(void)
{
    // 현재 모드 값 가져오기
	XmH10Mode_t currentSuitMode  = XM.status.h10.h10Mode;

    switch (s_modeTransitionState) {
        case MODE_TRANSITION_IDLE:
            // 평상시에 모드 변경이 감지되었는지 확인합니다.
            if (currentSuitMode != s_previousSuitMode) {
                
                // Passive Mode -> Standby Mode 로의 전환
                // P-Vector를 사용하던 Passive Mode를 안전하게 정지시키는 절차를 시작합니다.
                if (s_previousSuitMode == XM_H10_MODE_ASSIST && currentSuitMode == XM_H10_MODE_STANDBY) {
                    XM_SendPVectorReset(SYS_NODE_ID_RH);   // P-Vector 궤적 생성 취소 명령 전송
                    XM_SendPVectorReset(SYS_NODE_ID_LH);
                    s_modeTransitionTimer = XM_GetTick();  // reset 지연 타이머 시작
                    s_modeTransitionState = MODE_TRANSITION_STOP_PENDING; // 다음 상태로 전환
                }
            }
            break;
        
        case MODE_TRANSITION_STOP_PENDING:
            // 취소 명령 후 지연 시간 지났는지 확인
            if (XM_GetTick() - s_modeTransitionTimer >= STOP_CMD_DELAY_MS) {
                // 현재 위치에 정지하도록 P-Vector 전송
                StopMotorAndHold();

                // 메인 지연 타이머 시작 및 다음 상태로 전환
                s_modeTransitionTimer = XM_GetTick();
                s_modeTransitionState = MODE_TRANSITION_STOP_COMPLETED;
            }

        case MODE_TRANSITION_STOP_COMPLETED:
            // P-Vector 정지 명령이 양쪽 모두 완료되었는지 확인합니다.
            if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                
                // 2. 정지가 완료되면, 안정화 지연 단계로 넘어갑니다.
                s_modeTransitionTimer = XM_GetTick();
                s_modeTransitionState = MODE_TRANSITION_DELAYING;
            }
            break;

        case MODE_TRANSITION_DELAYING:
            // 모드 변경 전/후의 안정화를 위해 일정 시간 대기합니다.
            if (XM_GetTick() - s_modeTransitionTimer >= MODE_TRANSITION_DELAY_MS) {
                
                // 초기화(Enter) 함수를 호출합니다.
                EnterStandbyMode();
                
                // 4. 모든 전환 절차가 완료되었으므로, 현재 모드를 기록하고 IDLE 상태로 복귀합니다.
                s_previousSuitMode = currentSuitMode;
                s_modeTransitionState = MODE_TRANSITION_IDLE;
                XM_TSM_TransitionTo(s_userHandle, XM_STATE_STANDBY);
            }
            break;
    }
}

/**
 * @brief 현재 설정된 Assist Mode에 맞는 Update...() 함수를 실행합니다.
 */
static void ExecutePassiveMode(void)
{
    // [설계 조건 5] 모드 전환 중이 아닐 때만 Passive Mode 로직을 실행합니다.
    if (IsDuringModeTransition()) {
        return;
    }

    // 이 예제에서는 Passive Mode만 존재합니다.
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        UpdatePassiveMode();
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
    // 1. 현재 모터의 정확한 각도를 읽어옵니다.
    int16_t currentAngleRH = (int16_t)round(XM.status.h10.rightHipMotorAngle * 10.0f);
    int16_t currentAngleLH = (int16_t)round(XM.status.h10.leftHipMotorAngle * 10.0f);

    // 2. 목표 위치(yd)를 현재 위치로 설정하여 P_Vector 전송
    PVector_t pVecRH = { .yd = currentAngleRH, .L = STOP_DURATION_MS, .s0 = PM_ACCEL_S0_RH, .sd = PM_ACCEL_SD_RH };
    PVector_t pVecLH = { .yd = currentAngleLH, .L = STOP_DURATION_MS, .s0 = PM_ACCEL_S0_LH, .sd = PM_ACCEL_SD_LH };
    XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
    XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);
    
    // 3. 진행 중인 P_Vector가 있었으므로, 완료 플래그를 false로 설정하여
    //    새로운 정지 명령이 진행 중임을 알립니다.
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
}

// -------------------- Mode Implementations --------------------
/**
 * @brief Standby(No Assist) 모드 진입 시 호출됩니다.
 */
static void EnterStandbyMode(void)
{
    // 임피던스 설정 및 파라미터 해제
    IVector_t stiffImpedance = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
    XM_SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
    XM_SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
}

/**
 * @brief Passive Mode 진입 시 호출됩니다.
 */
static void EnterPassiveMode(void)
{
    s_passiveState = PASSIVE_STATE_SET_IMPEDANCE;
}

/**
 * @brief Passive Mode의 메인 상태 머신입니다.
 */
static void UpdatePassiveMode(void)
{
    // XM.status.h10 캐시에서 현재 각도를 읽어옵니다.
    int16_t currentAngleRH = (int16_t)round(XM.status.h10.rightHipMotorAngle * 10.0f);
    int16_t currentAngleLH = (int16_t)round(XM.status.h10.leftHipMotorAngle * 10.0f);

    switch (s_passiveState) {
        case PASSIVE_STATE_SET_IMPEDANCE: {
            // 위치 제어를 위한 임피던스(강성) 설정
            IVector_t stiffImpedance = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
            XM_SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
            XM_SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
            s_passiveState = PASSIVE_STATE_START_MOTION;
            break;
        }
        case PASSIVE_STATE_START_MOTION: {
            // 첫 목표 각도로 이동하는 P-Vector 전송
            int16_t targetAngle = JOINT_ANGLE_MAX_ANGLE_INT16;

            // 목표 각도로 이동하는 데 필요한 duration 계산
            int16_t angleToMoveRH = abs(targetAngle - currentAngleRH);
            int16_t angleToMoveLH = abs(targetAngle - currentAngleLH);
            uint16_t durationRH = (uint16_t)(((float)angleToMoveRH / (float)PM_SPEED_RH) * 1000.0f);
            uint16_t durationLH = (uint16_t)(((float)angleToMoveLH / (float)PM_SPEED_LH) * 1000.0f);

            PVector_t pVecRH = { .yd = targetAngle, .L = durationRH, .s0 = PM_ACCEL_S0_RH, .sd = PM_ACCEL_SD_RH };
            PVector_t pVecLH = { .yd = targetAngle, .L = durationLH, .s0 = PM_ACCEL_S0_LH, .sd = PM_ACCEL_SD_LH };
            XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
            XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);

            s_passiveState = PASSIVE_STATE_MOVING_TO_MIN;
            break;
        }

        case PASSIVE_STATE_MOVING_TO_MIN: {
            // P-Vector 이동이 완료되었는지 확인
            if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                
                // 다시 이전 각도로 이동
                int16_t targetAngle = JOINT_ANGLE_MIN_ANGLE_INT16;

                // 전체 ROM을 이동하는 데 필요한 duration 계산
                int16_t angleToMoveRH = abs(JOINT_ANGLE_MAX_ANGLE_INT16 - JOINT_ANGLE_MIN_ANGLE_INT16);
				int16_t angleToMoveLH = abs(JOINT_ANGLE_MAX_ANGLE_INT16 - JOINT_ANGLE_MIN_ANGLE_INT16);
				uint16_t durationRH = (uint16_t)(((float)angleToMoveRH / (float)PM_SPEED_RH) * 1000.0f);
				uint16_t durationLH = (uint16_t)(((float)angleToMoveLH / (float)PM_SPEED_LH) * 1000.0f);

				PVector_t pVecRH = { .yd = targetAngle, .L = durationRH, .s0 = PM_ACCEL_S0_RH, .sd = PM_ACCEL_SD_RH };
				PVector_t pVecLH = { .yd = targetAngle, .L = durationLH, .s0 = PM_ACCEL_S0_LH, .sd = PM_ACCEL_SD_LH };
                XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
                XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);

                s_passiveState = PASSIVE_STATE_MOVING_TO_MAX;
            }
            break;
        }

        case PASSIVE_STATE_MOVING_TO_MAX: {
            // P-Vector 이동이 완료되었는지 확인
            if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);

                // 다음 목표 각도로 이동
                int16_t targetAngle = JOINT_ANGLE_MAX_ANGLE_INT16;

                // 전체 ROM(75도)을 이동하는 데 필요한 duration 계산
                int16_t angleToMoveRH = abs(JOINT_ANGLE_MAX_ANGLE_INT16 - JOINT_ANGLE_MIN_ANGLE_INT16);
				int16_t angleToMoveLH = abs(JOINT_ANGLE_MAX_ANGLE_INT16 - JOINT_ANGLE_MIN_ANGLE_INT16);
				uint16_t durationRH = (uint16_t)(((float)angleToMoveRH / (float)PM_SPEED_RH) * 1000.0f);
				uint16_t durationLH = (uint16_t)(((float)angleToMoveLH / (float)PM_SPEED_LH) * 1000.0f);
                
                PVector_t pVecRH = { .yd = targetAngle, .L = durationRH, .s0 = PM_ACCEL_S0_RH, .sd = PM_ACCEL_SD_RH };
				PVector_t pVecLH = { .yd = targetAngle, .L = durationLH, .s0 = PM_ACCEL_S0_LH, .sd = PM_ACCEL_SD_LH };
                XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
                XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);

                s_passiveState = PASSIVE_STATE_MOVING_TO_MIN;
            }
            break;
        }

        default:
            // 예외 발생 시 안전을 위해 초기 상태로
            s_passiveState = PASSIVE_STATE_SET_IMPEDANCE;
            break;
    }
}
