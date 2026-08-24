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

/* --- 2. Device Drivers --- */
#include "cm_drv.h"             // Control Module 드라이버 (Rx/Tx + PnP 통합)
#include "imu_hub_drv.h"        // IMU Hub Driver (DOP V2)
#include "emg_hub_drv.h"        // EMG Hub Driver (DOP V2)
#include "fes_hub_drv.h"        // FES Hub Driver (DOP V2)
#include "mdaf-25-6850.h"       // GRF(FSR) Device (Auto-Sense + DataLake)
#include "mti-630.h"            // XSENS IMU Device (Auto-Sense + DataLake)
#include "xm_total_data.h"      // Total Data Packet 스냅샷 (System-Managed CDC)
#include "system_startup.h"     // System_SendSync_Ch1() — CiA 301 SYNC 전송
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
    const TickType_t xPeriod = pdMS_TO_TICKS(1); // 1ms 주기 고정 (1kHz)

    /* 3. Main Control Loop */
    for (;;)
    {
        /* [Timing Control] */
        // 정확한 1ms 주기를 보장하기 위해 vTaskDelayUntil 사용
        // 알고리즘 연산 시간이 1ms를 넘지 않도록 주의해야 함
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* [Step 1: Input] Data Gathering */
        // 모든 센서 데이터를 User가 보기 편한 구조체(XM.status)로 업데이트
        _FetchAllInputs();

        /* [Step 1.2: SYNC broadcast] _FetchAllInputs() 직후 SYNC 전송.
         * CM이 다음 PDO를 준비하도록 트리거 → 다음 사이클 Read에서 수신.
         * Read↔SYNC가 같은 태스크에서 실행되어 phase drift 제거. */
        if (CM_Drv_IsConnected()) {
            System_SendSync_Ch1();
        }

        /* [Step 1.5: Total Data Snapshot] System-Managed CDC 데이터 수집 */
        // _FetchAllInputs() 직후, 모든 센서 raw 데이터를 패킷 버퍼에 스냅샷
        // USB CDC Total Data(Module ID 0x20)로 자동 전송됨
        XM_TotalData_Snapshot();

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
        uint32_t current_time = IOIF_TIM_GetTick(); //

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
    CM_RxData_t h10_data;

    // CM은 전원을 공급하므로 항상 물리적으로 연결되어 있다고 가정하지만,
    // 논리적 통신 상태(Operational)를 확인하는 것이 안전합니다.
    if (CM_Drv_IsConnected()) {
        // Mutex로 보호된 안전한 데이터 복사 수행
        // 실패 시 XM.status.h10은 이전 값 유지 (garbage 방지)
        if (CM_GetRxData(&h10_data)) {
            XM.status.h10.is_connected = true;

            // --- Info & State ---
            XM.status.h10.h10AssistModeLoopCnt  = h10_data.pdo.h10AssistModeLoopCnt;
            XM.status.h10.h10PostProcessingCnt  = h10_data.pdo.postProcessingCnt;
            XM.status.h10.h10Mode               = (XmH10Mode_t)h10_data.sdo.h10Mode;
            XM.status.h10.h10AssistLevel        = h10_data.sdo.h10AssistLevel;
            XM.status.h10.h10FSMcurrentState    = h10_data.pdo.h10FSMcurrentState;
            XM.status.h10.h10IsNeutralPosSet    = h10_data.sdo.h10NeutralPosSet;
            XM.status.h10.isPVectorRHDone       = h10_data.sdo.isPVectorRHDone;
            XM.status.h10.isPVectorLHDone       = h10_data.sdo.isPVectorLHDone;

            // --- Kinematics Data ---
            XM.status.h10.leftHipAngle     = h10_data.pdo.leftHipAngle;
            XM.status.h10.rightHipAngle    = h10_data.pdo.rightHipAngle;
            XM.status.h10.leftThighAngle   = h10_data.pdo.leftThighAngle;
            XM.status.h10.rightThighAngle  = h10_data.pdo.rightThighAngle;
            XM.status.h10.leftKneeAngle    = h10_data.pdo.leftKneeAngle;
            XM.status.h10.rightKneeAngle   = h10_data.pdo.rightKneeAngle;
            XM.status.h10.pelvicAngle      = h10_data.pdo.pelvicAngle;

            // --- Gait Data ---
            XM.status.h10.isLeftFootContact  = h10_data.pdo.isLeftFootContact;
            XM.status.h10.isRightFootContact = h10_data.pdo.isRightFootContact;
            XM.status.h10.forwardVelocity    = h10_data.pdo.forwardVelocity;

            // --- Motor Data ---
            XM.status.h10.leftHipTorque      = h10_data.pdo.leftHipTorque;
            XM.status.h10.rightHipTorque     = h10_data.pdo.rightHipTorque;
            XM.status.h10.leftHipMotorAngle  = h10_data.pdo.leftHipMotorAngle;
            XM.status.h10.rightHipMotorAngle = h10_data.pdo.rightHipMotorAngle;

            // --- IMU Data ---
            XM.status.h10.leftHipImuFrontalRoll    = h10_data.pdo.leftHipImuFrontalRoll;
            XM.status.h10.leftHipImuSagittalPitch  = h10_data.pdo.leftHipImuSagittalPitch;
            XM.status.h10.rightHipImuFrontalRoll   = h10_data.pdo.rightHipImuFrontalRoll;
            XM.status.h10.rightHipImuSagittalPitch = h10_data.pdo.rightHipImuSagittalPitch;
            XM.status.h10.leftHipImuTransverseYaw  = h10_data.pdo.leftHipImuTransverseYaw;
            XM.status.h10.rightHipImuTransverseYaw = h10_data.pdo.rightHipImuTransverseYaw;

            XM.status.h10.leftHipImuGlobalAccX = h10_data.pdo.leftHipImuGlobalAccX;
            XM.status.h10.leftHipImuGlobalAccY = h10_data.pdo.leftHipImuGlobalAccY;
            XM.status.h10.leftHipImuGlobalAccZ = h10_data.pdo.leftHipImuGlobalAccZ;

            XM.status.h10.leftHipImuGlobalGyrX = h10_data.pdo.leftHipImuGlobalGyrX;
            XM.status.h10.leftHipImuGlobalGyrY = h10_data.pdo.leftHipImuGlobalGyrY;
            XM.status.h10.leftHipImuGlobalGyrZ = h10_data.pdo.leftHipImuGlobalGyrZ;

            XM.status.h10.rightHipImuGlobalAccX = h10_data.pdo.rightHipImuGlobalAccX;
            XM.status.h10.rightHipImuGlobalAccY = h10_data.pdo.rightHipImuGlobalAccY;
            XM.status.h10.rightHipImuGlobalAccZ = h10_data.pdo.rightHipImuGlobalAccZ;

            XM.status.h10.rightHipImuGlobalGyrX = h10_data.pdo.rightHipImuGlobalGyrX;
            XM.status.h10.rightHipImuGlobalGyrY = h10_data.pdo.rightHipImuGlobalGyrY;
            XM.status.h10.rightHipImuGlobalGyrZ = h10_data.pdo.rightHipImuGlobalGyrZ;
        }
        // Mutex 실패 시: XM.status.h10은 이전 값 그대로 유지 (duplicate > garbage)
    } else {
        XM.status.h10.is_connected = false;
        // 연결 끊김 시 데이터 처리 정책 (0으로 초기화 또는 이전 값 유지)
    }

    /* 2. GRF Sensor Data Mapping (Mutex + Snapshot) (Optional) */
    MarvelDex_packet_t grf_L, grf_R;
    
    /* GRF Left (ch=0) */
    if (MarvelDex_IsOnline(MARVELDEX_CH_LEFT)) {
        MarvelDex_GetLatest(MARVELDEX_CH_LEFT, &grf_L);
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
    
    /* GRF Right (ch=1) */
    if (MarvelDex_IsOnline(MARVELDEX_CH_RIGHT)) {
        MarvelDex_GetLatest(MARVELDEX_CH_RIGHT, &grf_R);
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

    /* 3. IMU Sensor Data (ch=0) (Optional) */
    XsensMTi_packet_t imu_packet;
    if (XsensMTi_IsOnline(0)) {
        XsensMTi_GetLatest(0, &imu_packet);

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
    
    /* 4. IMU Hub Module Data (6-axis IMU × 6, DOP V2) ✅ */
    ImuHub_RxData_t imu_hub_data;
    if (ImuHub_Drv_IsConnected()) {  /* ✅ V3.0: Device Driver API 직접 호출 */
        XM.status.imu_hub.is_connected = true;
        // IMU Hub 드라이버에서 최신 데이터 가져오기 (Mutex 보호)
        if (ImuHub_Drv_GetRxData(&imu_hub_data)) {
            XM.status.imu_hub.lastUpdateTick = imu_hub_data.timestamp;
            XM.status.imu_hub.connected_mask = imu_hub_data.connected_mask;
            
            // 6개 IMU 센서 데이터 변환 (int16 → float)
            for (int i = 0; i < XM_IMU_HUB_SENSOR_COUNT; i++) {
                ImuHub_ImuData_t* src = &imu_hub_data.imu[i];
                XmImuHubSensor_t* dst = &XM.status.imu_hub.sensor[i];
                
                // Quaternion (int16 / 10000.0f → float)
                dst->q_w = (float)src->q[0] / IMUHUB_SCALE_INT16_TO_QUAT;
                dst->q_x = (float)src->q[1] / IMUHUB_SCALE_INT16_TO_QUAT;
                dst->q_y = (float)src->q[2] / IMUHUB_SCALE_INT16_TO_QUAT;
                dst->q_z = (float)src->q[3] / IMUHUB_SCALE_INT16_TO_QUAT;
                
                // Euler (int16 / 100.0f → float, Degree)
                // dst->roll  = (float)src->rpy[0] / IMUHUB_SCALE_INT16_TO_EULER;
                // dst->pitch = (float)src->rpy[1] / IMUHUB_SCALE_INT16_TO_EULER;
                // dst->yaw   = (float)src->rpy[2] / IMUHUB_SCALE_INT16_TO_EULER;
                
                // Acceleration (int16 / 100.0f → float, g)
                dst->acc_x = (float)src->a[0] / IMUHUB_SCALE_INT16_TO_ACC;
                dst->acc_y = (float)src->a[1] / IMUHUB_SCALE_INT16_TO_ACC;
                dst->acc_z = (float)src->a[2] / IMUHUB_SCALE_INT16_TO_ACC;
                
                // Gyroscope (int16 / 10.0f → float, deg/s)
                dst->gyr_x = (float)src->g[0] / IMUHUB_SCALE_INT16_TO_GYRO;
                dst->gyr_y = (float)src->g[1] / IMUHUB_SCALE_INT16_TO_GYRO;
                dst->gyr_z = (float)src->g[2] / IMUHUB_SCALE_INT16_TO_GYRO;
                
                // Magnetometer (int16 / 1.0f → float, uT)
                // dst->mag_x = (float)src->m[0] / IMUHUB_SCALE_INT16_TO_MAG;
                // dst->mag_y = (float)src->m[1] / IMUHUB_SCALE_INT16_TO_MAG;
                // dst->mag_z = (float)src->m[2] / IMUHUB_SCALE_INT16_TO_MAG;
            }
        }
    } else {
        XM.status.imu_hub.is_connected = false;
    }

    /* 5. EMG Hub Module Data (sEMG 1ch, DOP V2) */
    EmgHub_RxData_t emg_hub_data;
    if (EmgHub_Drv_IsConnected()) {
        XM.status.emg_hub.is_connected = true;
        if (EmgHub_Drv_GetRxData(&emg_hub_data)) {
            XM.status.emg_hub.lastUpdateTick  = emg_hub_data.timestamp;
            XM.status.emg_hub.raw_adc         = emg_hub_data.raw_adc;
            XM.status.emg_hub.voltage_uv      = (float)emg_hub_data.voltage_uv_x10 / EMGHUB_SCALE_UV_X10;
            XM.status.emg_hub.rms_uv          = (float)emg_hub_data.rms_uv_x10 / EMGHUB_SCALE_UV_X10;
            XM.status.emg_hub.envelope_uv     = (float)emg_hub_data.envelope_uv_x10 / EMGHUB_SCALE_UV_X10;
            XM.status.emg_hub.mvc_percent     = emg_hub_data.mvc_percent;
            XM.status.emg_hub.is_active       = emg_hub_data.is_active;
            XM.status.emg_hub.status_flags    = emg_hub_data.status_flags;
        }
    } else {
        XM.status.emg_hub.is_connected = false;
    }

    /* 6. FES Hub Module Data (2ch FES feedback, DOP V2) */
    FesHub_RxData_t fes_hub_data;
    if (FesHub_Drv_IsConnected()) {
        XM.status.fes_hub.is_connected = true;
        if (FesHub_Drv_GetRxData(&fes_hub_data)) {
            XM.status.fes_hub.lastUpdateTick = fes_hub_data.timestamp;
            for (int i = 0; i < XM_FES_HUB_CH_COUNT; i++) {
                XM.status.fes_hub.ch_state[i]      = (uint8_t)fes_hub_data.ch_state[i];
                XM.status.fes_hub.ch_current_mA[i]  = fes_hub_data.ch_current_mA[i];
                XM.status.fes_hub.ch_fault_code[i]  = fes_hub_data.ch_fault_code[i];
            }
            XM.status.fes_hub.hv_voltage_V   = fes_hub_data.hv_voltage_V;
            XM.status.fes_hub.digipot_pos    = fes_hub_data.digipot_pos;
            XM.status.fes_hub.error_register = fes_hub_data.error_register;
        }
    } else {
        XM.status.fes_hub.is_connected = false;
    }

    /* 7. I/O (LED & Button) Update */
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
