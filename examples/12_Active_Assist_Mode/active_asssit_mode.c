/**
 ******************************************************************************
 * @file    active_assist_mode.c
 * @author  HyundoKim
 * @brief   [고급] Active-Assist Mode — 의도 기반 양측 독립 토크 보조
 * @version 1.1
 * @date    Mar 09, 2026
 *
 * @see docs/api-reference/02-h10-control-n-data.md
 * @see docs/api-reference/01-task-state-machine.md
 * @see docs/api-reference/05-usb-connectivity.md
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
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
#define STOP_WAIT_MARGIN_MS         1000U // 정지 P-Vector Done 대기 margin (STOP_DURATION_MS + margin)

// --- Homing 설정 값 ---
#define HOMING_TRANSITION_DELAY_MS  50    // 각 단계 사이의 지연 시간 (50ms)
#define HOMING_WAIT_MARGIN_MS       2000U // 호밍 Done 대기 margin — 계산된 duration + margin 초과 시 fail-closed
#define HOMING_MIN_DURATION_MS      50    // P-Vector 최소 duration (L=0 방어)
#define HOMING_SPEED_RH             150   // 초당 이동 속도 (deg/s)
#define HOMING_ACCEL_S0_RH          2     // 초기 가속도(deg/s^2)
#define HOMING_ACCEL_SD_RH          2     // 말기 가속도(deg/s^2)
#define HOMING_SPEED_LH             150   // 초당 이동 속도 (deg/s)
#define HOMING_ACCEL_S0_LH          2     // 초기 가속도(deg/s^2)
#define HOMING_ACCEL_SD_LH          2     // 말기 가속도(deg/s^2)

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
	float leftKneeAngle;
	float rightKneeAngle;
	bool isLeftFootContact;
	bool isRightFootContact;
	float forwardVelocity;
	float leftHipTorque;
	float rightHipTorque;
	float leftHipMotorAngle;
	float rightHipMotorAngle;
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
static uint32_t             s_homingDeadline = 0; // 호밍 Done 대기 데드라인 (duration + margin)
static ActiveAssistState_t  s_aaGlobalState = AA_STATE_HOMING; // AA모드의 전체 상위 상태
static ActiveAssistFsm_t    s_aaFsm_RH;  // 오른쪽 다리(RH)를 위한 상태 머신 객체
static ActiveAssistFsm_t    s_aaFsm_LH;  // 왼쪽 다리(LH)를 위한 상태 머신 객체

// --- For Dat Save ---
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
/**
 * @brief Active-Assist Mode 예제 애플리케이션을 초기화합니다.
 * @details 시스템 부팅 시 Main 태스크에서 단 한 번만 호출되어야 합니다.
 * 내부적으로 Task State Machine을 생성하고 각 상태에 맞는 함수들을 등록합니다.
 */
void Control_Setup(void)
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

    // [등록 3] ACTIVE 상태 설정
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_userHandle, &act_conf);

    // USB-CDC로 'myData' 구조체를 실시간 스트리밍하겠다!
    XM_SetUsbStreamSource(&myData, sizeof(MyData_t));
    XM_SetUsbAutoStream(true);

    /* Total Data Packet (Module ID 0x20)이 모든 H10 센서 데이터를
     * 자동 스트리밍합니다. 위 등록은 'myData' 를 추가로 스트리밍하는
     * 예시입니다. */
}

/*
 * @brief Active-Assist Mode 예제 애플리케이션을 주기적으로 실행합니다.
 * @details Main 태스크의 제어 루프(예: 1ms)에서 계속 호출되어야 합니다.
 */
void Control_Loop(void)
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

    // USB-CDC 스트리밍은 연결 시 연속 — phai-studio 로 수신

    /* [v2.6] 토크 + P/I 벡터 모두 CONTROL 모드 필요 (구 XM_CTRL_TORQUE 와 동일 값) */
    XM_SetControlMode(XM_CTRL_CONTROL);
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
    myData.leftKneeAngle = XM.status.h10.leftKneeAngle;
    myData.rightKneeAngle = XM.status.h10.rightKneeAngle;
    myData.isLeftFootContact = XM.status.h10.isLeftFootContact;
    myData.isRightFootContact = XM.status.h10.isRightFootContact;
    myData.forwardVelocity = XM.status.h10.forwardVelocity;
    myData.leftHipTorque = XM.status.h10.leftHipTorque;
    myData.rightHipTorque = XM.status.h10.rightHipTorque;
    myData.leftHipMotorAngle = XM.status.h10.leftHipMotorAngle;
    myData.rightHipMotorAngle = XM.status.h10.rightHipMotorAngle;
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
    // USB-CDC 스트리밍은 연결 시 연속 — phai-studio 로 수신 (세션 종료 처리 불필요)
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
                        XM_SendPVectorReset(SYS_NODE_ID_RH);   // FIFO 비우기
                        XM_SendPVectorReset(SYS_NODE_ID_LH);
                        StopMotorAndHold();                     // 즉시 부드러운 정지
                        s_modeTransitionTimer = XM_GetTick();   // 정지 Done 대기 타임아웃 기준
                        s_modeTransitionState = MODE_TRANSITION_STOP_COMPLETED;
                    }
                    // [CASE 2] Homing 중이 아닐 때, Active-Assist Mode -> Standby Mode로의 전환
                    else if (s_previoush10Mode == XM_H10_MODE_ASSIST && currenth10Mode == XM_H10_MODE_STANDBY) {
                        // [v2.6] 토크를 1틱에 0 으로 끊지 않고 FW 내장 램프다운을 사용:
                        // MONITOR 요청 시 FW 가 지수 감쇠(τ=100ms) → 0 확정 → 벡터 해제를
                        // 자동 수행합니다. DELAYING 단계가 램프다운 완료까지 함께 대기합니다.
                        // (재진입 시 Active_Entry 가 CONTROL 을 다시 설정)
                        XM_SetControlMode(XM_CTRL_MONITOR);
                        s_modeTransitionTimer = XM_GetTick();
                        s_modeTransitionState = MODE_TRANSITION_DELAYING;
                    }
                }
            }
            break;
        
        case MODE_TRANSITION_STOP_COMPLETED: 
            {
                // P-Vector 정지 명령이 양쪽 모두 완료되었는지 확인합니다.
                if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);

                    // 2. 정지가 완료되면, FW 램프다운을 요청하고 안정화 지연 단계로 넘어갑니다.
                    XM_SetControlMode(XM_CTRL_MONITOR);
                    s_modeTransitionTimer = XM_GetTick();
                    s_modeTransitionState = MODE_TRANSITION_DELAYING;
                }
                // [타임아웃] Done 미수신 (MD 무응답/프레임 유실) — 정지 명령은 이미
                // 전송됐으므로 플래그를 강제 정리하고 진행합니다 (영구 대기 방지).
                else if (XM_GetTick() - s_modeTransitionTimer >= (uint32_t)STOP_DURATION_MS + STOP_WAIT_MARGIN_MS) {
                    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                    XM_SendUsbDebugMessage("[AA] 정지 Done 타임아웃 — 강제 진행\r\n");
                    XM_SetControlMode(XM_CTRL_MONITOR);
                    s_modeTransitionTimer = XM_GetTick();
                    s_modeTransitionState = MODE_TRANSITION_DELAYING;
                }
            }
            break;
            
        case MODE_TRANSITION_DELAYING: 
            {
                // 모드 변경 전/후의 안정화를 위해 일정 시간 대기하고,
                // FW 램프다운(CONTROL→MONITOR 전환 시퀀스) 완료까지 함께 기다립니다.
                if ((XM_GetTick() - s_modeTransitionTimer >= MODE_TRANSITION_DELAY_MS)
                    && (XM_GetAppliedControlMode() == XM_MODE_APPLIED_MONITOR)) {

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
        /* [v2.6] zero-impedance I-Vector 는 여기서 보내지 않습니다 — 이 함수는
         * FW 램프다운 완료(applied==MONITOR) 후에만 호출되어 벡터가 게이트에
         * 차단되며, FW 의 VECTOR_RELEASE 단계가 동일한 임피던스 해제(kp=0,kd=0)를
         * 이미 전송한 뒤입니다. 로컬 Done 플래그 정리만 수행합니다. */
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
    static uint32_t homingTimer = 0; // ✅ HOMING_FINALIZE_DELAY 전용 타이머
    switch (s_aaGlobalState) {
        case AA_STATE_HOMING:
            switch (s_homingState) {
                // --- Homing ---
                case HOMING_ENTRY: {
                    /* [가드] 직전 비상정지/타임아웃이 요청한 MONITOR 전환 정리
                     * (TRANSITION: ZERO_HOLD→VECTOR_RELEASE, ~10 tick)가 끝나
                     * FW 게이트가 CONTROL 로 다시 열린 뒤에만 호밍 벡터를 전송.
                     * 가드 없이 전송하면 벡터가 무음 차단되고 상태만 전진해
                     * '전송된 적 없는 P-Vector 의 Done' 을 기다리는 재시도
                     * livelock 이 됨 (CONTROL 요청은 Active_Entry 가 이미 latch). */
                    if (XM_GetAppliedControlMode() != XM_MODE_APPLIED_CONTROL) {
                        break;
                    }
                    XM_SendPVectorReset(SYS_NODE_ID_RH);
                    XM_SendPVectorReset(SYS_NODE_ID_LH);
                    XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 6);
                    XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6, 6);
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
                    // 이동 시간 계산 후 R/L 중 긴 쪽으로 통일 (동시 도착)
                    uint16_t durationRH = (uint16_t)(((float)angleToMoveRH / (float)HOMING_SPEED_RH) * 1000.0f);
                    uint16_t durationLH = (uint16_t)(((float)angleToMoveLH / (float)HOMING_SPEED_RH) * 1000.0f);
                    uint16_t homingDuration = (durationRH > durationLH) ? durationRH : durationLH;
                    if (homingDuration < HOMING_MIN_DURATION_MS) homingDuration = HOMING_MIN_DURATION_MS;

                    PVector_t pVecRH = { .yd = targetAngle, .L = homingDuration, .s0 = HOMING_ACCEL_S0_RH, .sd = HOMING_ACCEL_SD_RH };
                    PVector_t pVecLH = { .yd = targetAngle, .L = homingDuration, .s0 = HOMING_ACCEL_S0_RH, .sd = HOMING_ACCEL_SD_RH };
                    XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
                    XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);
                    // Done 대기 데드라인: 계산된 이동 시간 + margin (duration+margin 패턴)
                    s_homingDeadline = XM_GetTick() + (uint32_t)homingDuration + HOMING_WAIT_MARGIN_MS;
                    s_homingState = HOMING_WAIT_FOR_DONE;
                    break;
                }
                    
                case HOMING_WAIT_FOR_DONE: {
                    if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                        XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
                        XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                        homingTimer = XM_GetTick(); // ✅ FINALIZE_DELAY 진입 시 타이머 시작
                        s_homingState = HOMING_FINALIZE_DELAY;
                    }
                    // [fail-closed] Done 미수신 (MD 무응답 등) — 보조 단계로 진입하지 않고
                    // 즉시 출력을 끊은 뒤 STANDBY 로 복귀합니다. h10Mode 가 ASSIST 로
                    // 유지되면 STANDBY→ACTIVE 재진입으로 호밍을 처음부터 재시도합니다.
                    else if ((int32_t)(XM_GetTick() - s_homingDeadline) >= 0) {
                        XM_EmergencyDisengage();
                        s_homingState = HOMING_ENTRY;
                        XM_SendUsbDebugMessage("[AA] 호밍 Done 타임아웃 — 정지 후 STANDBY 복귀\r\n");
                        XM_TSM_TransitionTo(s_userHandle, XM_STATE_STANDBY);
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
                s_aaFsm_RH.anchorAngle_deg10 = (int16_t)round(XM.status.h10.rightThighAngle * 10.0f); // 허벅지 각도의 Homing 완료 위치를 기준점으로 (deg10 스케일 — UpdateSingleLegAssistLogic 과 단위 일치)
                s_aaFsm_LH.anchorAngle_deg10 = (int16_t)round(XM.status.h10.leftThighAngle * 10.0f);

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
    // AssistLevel 0~10 클램프 후 배율 적용 — 범위 밖 수신값 방어
    float level_scale = (float)XM_SafeAssistLevel(XM.status.h10.h10AssistLevel) / 10.0f;
    if (nodeId == SYS_NODE_ID_RH) {
    	XM_SetAssistTorqueRH(level_scale * fsm->currentTorque_Nm);
    } else if (nodeId == SYS_NODE_ID_LH) {
    	XM_SetAssistTorqueLH(level_scale * fsm->currentTorque_Nm);
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
