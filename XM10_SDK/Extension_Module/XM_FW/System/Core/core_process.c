/**
 ******************************************************************************
 * @file    core_process.c
 * @author  HyundoKim
 * @brief   User Task 구동 및 XM API 데이터 동기화 엔진 구현부
 * @details
 * 이 모듈은 System Layer에 위치하지만, XM API(Interface)의
 * 'Backend Implementation' 역할을 수행합니다.
 * 따라서 상위 인터페이스인 xm_api_data.h에 의존하여
 * 데이터를 채워 넣습니다 (Dependency Inversion 적용).
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "core_process.h"

/* --- 1. Interfaces & APIs (계약서) --- */
#include "xm_api.h"     // 통합 파사드

/* --- 2. System Links & Drivers (하위 모듈) --- */
#include "cm_drv.h"             // Control Module 드라이버 (Rx/Tx)
#include "cm_xm_link.h"         // CM 데이터 링크
#include "grf_xm_link.h"        // GRF Module 데이터 링크
#include "xsens_imu_xm_link.h"  // XSENS IMU 데이터 링크
#include "pnp_manager.h"        // 모듈 연결 상태 확인
#include "mdaf-25-6850.h"       // GRF Driver Struct
#include "mti-630.h"            // IMU Driver Struct
#include "ioif_agrb_tim.h"

/* --- 3. RTOS & Utilities --- */
#include "FreeRTOS.h"
#include "task.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

/**
 * @brief 전역 로봇 데이터 인스턴스 (End User가 'XM'으로 접근하는 실체)
 * @note  xm_api_data.h에 extern 선언되어 있음
 */
extern XmRobot_t XM; // 전역 인스턴스 정의

/* 사용자가 작성할 함수들 (링커가 찾을 수 있도록 extern 선언) */
extern void User_Setup(void);
extern void User_Loop(void);

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static UBaseType_t stack_remaining_words;
static uint32_t stack_remaining_bytes;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _FetchAllInputs(void);
static void _FlushAllOutputs(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void StartUserTask(void *argument)
{
    /* 1. User Initialization (1회 실행) */
    // End User가 작성한 초기화 코드 실행 (TSM 생성, 초기값 설정 등)
    User_Setup();

    /* 2. Timing Initialization */
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(2); // 2ms 주기 고정 (500Hz)

    /* 3. Main Control Loop */
    for (;;)
    {
        /* [Timing Control] */
        // 정확한 2ms 주기를 보장하기 위해 vTaskDelayUntil 사용
        // 알고리즘 연산 시간이 2ms를 넘지 않도록 주의해야 함
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* [Step 1: Input] Data Gathering */
        // 모든 센서 데이터를 User가 보기 편한 구조체(XM.status)로 업데이트
        _FetchAllInputs();

        /* [Step 2: Process] User Algorithm */
        // End User가 작성한 제어 로직 실행 (TSM_Run 등)
        User_Loop();

        /* [Step 3: Output] Command Flushing */
        // User가 구조체(XM.command)에 쓴 값을 실제 하드웨어로 전송
        _FlushAllOutputs();

        /* [Step 4: USB Data Handling] Logging or Streaming */
        // Tx가 끝난 시점(데이터 확정)에서 로깅 및 모니터링 수행
        XM_USB_ProcessPeriodic();

        // --- 스택 모니터링 (디버깅용) ---
        static uint32_t last_check_time = 0;
        uint32_t current_time = IOIF_GetTick(); //

        // 5초(5000ms)에 한 번씩 스택 상태를 업데이트
        // 오버플로우 발생시 LED indicate?
        if (current_time - last_check_time > 5000) {
            last_check_time = current_time;
            
            // 1. 현재 태스크(NULL 전달)의 스택 High Water Mark를 확인합니다.
            //    (단위: Words, 4바이트)
            stack_remaining_words = uxTaskGetStackHighWaterMark(NULL);
            
            // 2. 바이트 단위로 변환
            stack_remaining_bytes = stack_remaining_words * 4;

            // // 3. USB(CDC)로 메시지 전송
            // char debug_msg[100];
            // sprintf(debug_msg, "[UserTask Stack] Min. Remaining: %lu Bytes\r\n", 
            //         stack_remaining_bytes);
            
            // // 4. API를 통해 PC로 전송
            // SendUsbDebugMessage(debug_msg); //
        }
    }
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* ---- [IPO Step 1] Input Gathering (Rx Integration) ---- */
/**
 * @brief [IPO Step 1] Input Gathering
 * @details 하드웨어 계층(Link/Driver)의 최신 데이터를 API 계층(Facade)으로 복사합니다.
 * 데이터의 원자성(Atomicity)과 동기화(Synchronization)를 보장합니다.
 */
static void _FetchAllInputs(void)
{
    /* 1. Control Module (KIT H10) Data */
    // CM_RxData_t는 cm_drv.h에 정의된 구조체 (float 변환 완료된 상태)
    CM_RxData_t cm_data;

    // CM은 전원을 공급하므로 항상 물리적으로 연결되어 있다고 가정하지만,
    // 논리적 통신 상태(Operational)를 확인하는 것이 안전합니다.
    if (CM_XM_Link_IsConnected()) {
        // Mutex로 보호된 안전한 데이터 복사 수행
        CM_GetRxData(&cm_data);

        XM.status.h10.is_connected = true;
        
        // --- Info & State ---
        XM.status.h10.h10AssistModeLoopCnt = cm_data.suitAssistModeLoopCnt;
        XM.status.h10.h10Mode        = (XmH10Mode_t)cm_data.suitMode; 
        XM.status.h10.h10AssistLevel = cm_data.suitAssistLevel;
        XM.status.h10.isPVectorRHDone = cm_data.isPVectorRHDone;
        XM.status.h10.isPVectorLHDone = cm_data.isPVectorLHDone;

        // --- Kinematics Data ---
        XM.status.h10.leftHipAngle     = cm_data.leftHipAngle;
        XM.status.h10.rightHipAngle    = cm_data.rightHipAngle;
        XM.status.h10.leftThighAngle   = cm_data.leftThighAngle;
        XM.status.h10.rightThighAngle  = cm_data.rightThighAngle;
        XM.status.h10.leftKneeAngle    = cm_data.leftKneeAngle;
        XM.status.h10.rightKneeAngle   = cm_data.rightKneeAngle;
        XM.status.h10.pelvicAngle      = cm_data.pelvicAngle;
        XM.status.h10.pelvicVelY       = cm_data.pelvicVelY;

        // --- Gait Data ---
        XM.status.h10.isLeftFootContact  = cm_data.isLeftFootContact;
        XM.status.h10.isRightFootContact = cm_data.isRightFootContact;
        XM.status.h10.gaitState          = cm_data.gaitState;
        XM.status.h10.gaitCycle          = cm_data.gaitCycle;
        XM.status.h10.forwardVelocity    = cm_data.forwardVelocity;

        // --- Motor Data ---
        XM.status.h10.leftHipTorque      = cm_data.leftHipTorque;
        XM.status.h10.rightHipTorque     = cm_data.rightHipTorque;
        XM.status.h10.leftHipMotorAngle  = cm_data.leftHipMotorAngle;
        XM.status.h10.rightHipMotorAngle = cm_data.rightHipMotorAngle;

        // --- IMU Data ---
        XM.status.h10.leftHipImuFrontalRoll    = cm_data.leftHipImuFrontalRoll;
        XM.status.h10.leftHipImuSagittalPitch  = cm_data.leftHipImuSagittalPitch;
        XM.status.h10.rightHipImuFrontalRoll   = cm_data.rightHipImuFrontalRoll;
        XM.status.h10.rightHipImuSagittalPitch = cm_data.rightHipImuSagittalPitch;
        
        XM.status.h10.leftHipImuGlobalAccX = cm_data.leftHipImuGlobalAccX;
        XM.status.h10.leftHipImuGlobalAccY = cm_data.leftHipImuGlobalAccY;
        XM.status.h10.leftHipImuGlobalAccZ = cm_data.leftHipImuGlobalAccZ;
        
        XM.status.h10.leftHipImuGlobalGyrX = cm_data.leftHipImuGlobalGyrX;
        XM.status.h10.leftHipImuGlobalGyrY = cm_data.leftHipImuGlobalGyrY;
        XM.status.h10.leftHipImuGlobalGyrZ = cm_data.leftHipImuGlobalGyrZ;
        
        XM.status.h10.rightHipImuGlobalAccX = cm_data.rightHipImuGlobalAccX;
        XM.status.h10.rightHipImuGlobalAccY = cm_data.rightHipImuGlobalAccY;
        XM.status.h10.rightHipImuGlobalAccZ = cm_data.rightHipImuGlobalAccZ;
        
        XM.status.h10.rightHipImuGlobalGyrX = cm_data.rightHipImuGlobalGyrX;
        XM.status.h10.rightHipImuGlobalGyrY = cm_data.rightHipImuGlobalGyrY;
        XM.status.h10.rightHipImuGlobalGyrZ = cm_data.rightHipImuGlobalGyrZ;
    } else {
        XM.status.h10.is_connected = false;
        // 연결 끊김 시 데이터 처리 정책 (0으로 초기화 또는 이전 값 유지)
    }

    /* 2. GRF Sensor Data Mapping (Lock-Free Read) (Optional) */
    // GRF 드라이버(mdaf-25-6850)는 Left/Right 패킷을 각각 관리하거나 
    // 각각의 최근 수신된 패킷을 System Layer가 가져옴
    MarvelDex_packet_t grf_L, grf_R;
    // GRF 데이터가 수신되었는지 확인 (Link Layer/Driver 함수 호출)
    // *실제로는 mdaf 드라이버가 Left/Right 최신본을 각각 저장하고 있어야 합니다.
    if (GRF_XM_Link_IsConnected(SENSOR_SPACE_LEFT)) {
        GRF_XM_Link_GetLatest(&grf_L, NULL);
        // GRF 드라이버 큐 또는 버퍼에서 데이터 가져오기
        XM.status.grf.is_left_grf_connected = true;
                
        XM.status.grf.leftLastUpdateTick = grf_L.timestamp;
        XM.status.grf.leftSensorSpace = (XM_GRF_SPACE_e)grf_L.sensorSpace;
        XM.status.grf.leftRollingIndex = grf_L.rollingIndex;
        memcpy(XM.status.grf.leftSensorData, grf_L.sensorData, XM_GRF_CHANNEL_SIZE);
        XM.status.grf.leftBatteryLevel = grf_L.batteryLevel;
        XM.status.grf.leftStatusFlags  = grf_L.statusFlags;
    } else {
        XM.status.grf.is_left_grf_connected = false;
    }
    
    if (GRF_XM_Link_IsConnected(SENSOR_SPACE_RIGHT)) {
        GRF_XM_Link_GetLatest(NULL, &grf_R);

        XM.status.grf.is_right_grf_connected = true;
                
        XM.status.grf.rightLastUpdateTick = grf_R.timestamp;
        XM.status.grf.rightSensorSpace = (XM_GRF_SPACE_e)grf_R.sensorSpace;
        XM.status.grf.rightRollingIndex = grf_R.rollingIndex;
        memcpy(XM.status.grf.rightSensorData, grf_R.sensorData, XM_GRF_CHANNEL_SIZE);
        XM.status.grf.rightBatteryLevel = grf_R.batteryLevel;
        XM.status.grf.rightStatusFlags  = grf_R.statusFlags;
    } else {
        XM.status.grf.is_right_grf_connected = false;
    }

    /* 3. IMU Sensor Data (Optional) */
    XsensMTi_packet_t imu_packet;
    if (XsensIMU_XM_Link_IsConnected()) {
        // IMU 드라이버 큐 또는 버퍼에서 데이터 가져오기
        XsensIMU_XM_Link_GetLatest(&imu_packet);

        XM.status.imu.is_connected = true;
        XM.status.imu.lastUpdateTick = imu_packet.timestamp;
        
        // Quaternion
        XM.status.imu.q_w = imu_packet.q_w;
        XM.status.imu.q_x = imu_packet.q_x;
        XM.status.imu.q_y = imu_packet.q_y;
        XM.status.imu.q_z = imu_packet.q_z;
        
        // Acceleration
        XM.status.imu.acc_x = imu_packet.acc_x;
        XM.status.imu.acc_y = imu_packet.acc_y;
        XM.status.imu.acc_z = imu_packet.acc_z;
        
        // Gyroscope
        XM.status.imu.gyr_x = imu_packet.gyr_x;
        XM.status.imu.gyr_y = imu_packet.gyr_y;
        XM.status.imu.gyr_z = imu_packet.gyr_z;
    } else {
        XM.status.imu.is_connected = false;
    }
   
    /* 4. I/O (LED & Button) Update */
    // 버튼 디바운싱 및 LED 깜빡임 타이밍 계산 (Time-driven update)
    XM_IO_Update();
}

/* ---- [IPO Step 2] Algorithm Process (Process) ---- */
// IPO의 Process는 End User가 작성

/* ---- [IPO Step 3] Output Flushing (Tx Integration) ---- */
/**
 * @brief [IPO Step 3] Output Flushing
 * @details API 계층(Facade)에 기록된 제어 명령을 하드웨어 드라이버로 전달합니다.
 * 변경 사항이 있는 데이터만 선별적으로 Staging하여 버스 부하를 줄입니다.
 */
static void _FlushAllOutputs(void)
{
    /* ------------------------------------------------------
     * 1. Data Staging (값 업데이트)
     * ------------------------------------------------------ */
    // 사용자가 Process단계에서 SetAssistTorque를 호출해서 값이 바뀐 경우에만 드라이버 메모리 갱신
    if (XM.command._dirty_flags.torque_rh_updated) {
        CM_StageAuxTorque(SYS_NODE_ID_RH, XM.command.assist_torque_rh);
        XM.command._dirty_flags.torque_rh_updated = 0;
    }

    if (XM.command._dirty_flags.torque_lh_updated) {
        CM_StageAuxTorque(SYS_NODE_ID_LH, XM.command.assist_torque_lh);
        XM.command._dirty_flags.torque_lh_updated = 0;
    }

    /* ------------------------------------------------------
     * 2. Physical Transmission (Tx 정책)
     * ------------------------------------------------------ */
    // [정책] TORQUE 모드일 때만 주기적 전송 (Cyclic Transmission)
    if (XM.command.control_mode == XM_CTRL_TORQUE) {
        // 변경 사항이 없어도 이전에 Staging된 값(Zero-Order Hold)을 계속 전송함.
        // 이는 Watchdog에 걸리지 않고 "나 살아있어"라고 알리는 역할도 함.
        CM_FlushControlPDOs(); 
    }
    // MONITOR 모드일 때는 아무것도 전송하지 않음 (Silence)
}
