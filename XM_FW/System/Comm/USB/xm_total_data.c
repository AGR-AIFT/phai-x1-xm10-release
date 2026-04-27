/**
 ******************************************************************************
 * @file    xm_total_data.c
 * @author  HyundoKim
 * @brief   [System Layer] Total Data Packet 스냅샷 모듈 구현부
 * @details
 * 모든 센서의 raw 데이터를 1ms마다 스냅샷으로 수집합니다.
 * _FetchAllInputs() 직후 호출되어, 각 드라이버의 DataLake에서
 * 최신 데이터를 raw 타입 그대로 복사합니다.
 *
 * [데이터 흐름]
 *   _FetchAllInputs() → XM_TotalData_Snapshot() → XM_USB_ProcessPeriodic()
 *                                                    └→ PhAI_PacketBuild(0x20)
 *
 * [Thread-Safety]
 *   - CM: CM_GetRawPdoData() / CM_GetRxData() 내부 Mutex
 *   - GRF: MarvelDex_GetLatest() 내부 Mutex
 *   - XSENS: XsensMTi_GetLatest() 내부 Mutex
 *   - IMU Hub: ImuHub_Drv_GetRxData() 내부 Mutex
 *   - EMG Hub: EmgHub_Drv_GetRxData() 내부 Mutex
 *   - External IO: 단일 Task 접근 (UserTask)
 *
 * @version 2.3
 * @date    Apr 01, 2026
 * @changes
 *   v2.3: FDCAN_Diag Ch1/Ch2 분리 (6B+6B=12B),
 *         Rx FIFO0 fill level + Tx FIFO free level Ch1/Ch2 각각 구현,
 *         HAL 직접 호출 → IOIF API로 래핑 (아키텍처 준수),
 *         Reserved_HMMG 34B → 28B (365B 유지)
 *   v2.2: FDCAN_Diag 그룹 추가 (4B: TEC/REC/LEC/BusStatus),
 *         HAL_FDCAN_GetErrorCounters + HAL_FDCAN_GetProtocolStatus 직접 호출,
 *         Reserved_HMMG 40B → 36B (365B 유지)
 *   v2.1: 그룹 재구조화 (CM_PDO_Raw → H10_Joint/Segment/Gait/IMU/Count/State),
 *         네이밍 통일 (suitMode → h10Mode, xm_status → phai_x1_status 등),
 *         External_IMU → Ext_IMU, External_IO → Ext_IO (365B, 변경 없음)
 *   v2.0: sensor_mask → device_online_mask, IMU orient ×6 추가,
 *         EMG Hub 실제 필드 매핑, FES Hub 제거,
 *         adc_active_mask 추가 (365B)
 *   v1.3: postProcessingCnt 추가 (CM PDO line-by-line copy)
 *   v1.2: CM PDO memcpy → line-by-line copy (미요청 7개 필드 제거)
 *   v1.1: phai_x1_status 추가, grf/ext_imu *_connected 제거 → sensor_mask 통합
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_total_data.h"

/* --- Device Drivers --- */
#include "cm_drv.h"             /* CM_GetRawPdoData(), CM_GetRxData(), CM_Drv_IsConnected() */
#include "imu_hub_drv.h"        /* ImuHub_Drv_GetRxData(), ImuHub_Drv_IsConnected() */
#include "emg_hub_drv.h"        /* EmgHub_Drv_GetRxData(), EmgHub_Drv_IsConnected() */
#include "mdaf-25-6850.h"       /* MarvelDex_GetLatest(), MarvelDex_IsOnline() */
#include "mti-630.h"            /* XsensMTi_GetLatest(), XsensMTi_IsOnline() */

/* --- XM API --- */
#include "xm_api_external_io.h" /* XM_DigitalRead(), XM_AnalogRead(), XM_IsDioSwitchedToAdc() */
#include "xm_api_data.h"        /* XM (XmRobot_t), XM_CTRL_TORQUE */

/* --- Utilities --- */
#include "ioif_agrb_tim.h"      /* IOIF_TIM_GetTick() */
#include "ioif_agrb_fdcan.h"    /* IOIF_FDCAN_GetErrorCounters(), GetBusStatus(), GetRxFifo0FillLevel() */
#include "system_startup.h"     /* System_GetFDCAN1_Id(), System_GetFDCAN2_Id() */
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* device_online_mask 비트 정의 */
#define DEVICE_MASK_CM          (1U << 0)
#define DEVICE_MASK_GRF_LEFT    (1U << 1)
#define DEVICE_MASK_GRF_RIGHT   (1U << 2)
#define DEVICE_MASK_EXT_IMU     (1U << 3)
#define DEVICE_MASK_IMU_HUB     (1U << 4)
#define DEVICE_MASK_EMG_HUB     (1U << 5)

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

static XM_TotalDataPacket_t s_snapshot;
static uint32_t s_xm_loop_count = 0;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void XM_TotalData_Snapshot(void)
{
    uint16_t mask = 0;

    /* === Header === */
    s_snapshot.xm_loop_count = ++s_xm_loop_count;

    /* phai_x1_status: bit0=CM_CONNECTED, bit1=CTRL_TORQUE */
    uint8_t xm_sts = 0;
    if (CM_Drv_IsConnected())                              { xm_sts |= (1U << 0); }
    if (XM.command.control_mode == XM_CTRL_TORQUE)         { xm_sts |= (1U << 1); }
    s_snapshot.phai_x1_status = xm_sts;

    /* === H10 PDO (Joint + Segment + Gait + IMU + Count + FSM) === */
    if (CM_Drv_IsConnected()) {
        CM_PdoRx_CmToXm_t raw_pdo;
        if (CM_GetRawPdoData(&raw_pdo)) {
            /* H10_Joint */
            s_snapshot.leftHipAngle           = raw_pdo.leftHipAngle;
            s_snapshot.rightHipAngle          = raw_pdo.rightHipAngle;
            s_snapshot.leftKneeAngle          = raw_pdo.leftKneeAngle;
            s_snapshot.rightKneeAngle         = raw_pdo.rightKneeAngle;
            s_snapshot.leftHipTorque          = raw_pdo.leftHipTorque;
            s_snapshot.rightHipTorque         = raw_pdo.rightHipTorque;
            s_snapshot.leftHipMotorAngle      = raw_pdo.leftHipMotorAngle;
            s_snapshot.rightHipMotorAngle     = raw_pdo.rightHipMotorAngle;
            /* H10_Segment */
            s_snapshot.leftThighAngle         = raw_pdo.leftThighAngle;
            s_snapshot.rightThighAngle        = raw_pdo.rightThighAngle;
            s_snapshot.pelvicAngle            = raw_pdo.pelvicAngle;
            /* H10_Gait */
            s_snapshot.isLeftFootContact      = raw_pdo.isLeftFootContact;
            s_snapshot.isRightFootContact     = raw_pdo.isRightFootContact;
            s_snapshot.forwardVelocity        = raw_pdo.forwardVelocity;
            /* H10_IMU — Orientation ×6 (H10 PDO mapping 미활성 → placeholder 0) */
            s_snapshot.leftHipImuFrontalRoll    = raw_pdo.leftHipImuFrontalRoll;
            s_snapshot.leftHipImuSagittalPitch  = raw_pdo.leftHipImuSagittalPitch;
            s_snapshot.leftHipImuTransverseYaw  = raw_pdo.leftHipImuTransverseYaw;
            s_snapshot.rightHipImuFrontalRoll   = raw_pdo.rightHipImuFrontalRoll;
            s_snapshot.rightHipImuSagittalPitch = raw_pdo.rightHipImuSagittalPitch;
            s_snapshot.rightHipImuTransverseYaw = raw_pdo.rightHipImuTransverseYaw;
            /* H10_IMU — Global Acc/Gyr */
            s_snapshot.leftHipImuGlobalAccX   = raw_pdo.leftHipImuGlobalAccX;
            s_snapshot.leftHipImuGlobalAccY   = raw_pdo.leftHipImuGlobalAccY;
            s_snapshot.leftHipImuGlobalAccZ   = raw_pdo.leftHipImuGlobalAccZ;
            s_snapshot.rightHipImuGlobalAccX  = raw_pdo.rightHipImuGlobalAccX;
            s_snapshot.rightHipImuGlobalAccY  = raw_pdo.rightHipImuGlobalAccY;
            s_snapshot.rightHipImuGlobalAccZ  = raw_pdo.rightHipImuGlobalAccZ;
            s_snapshot.leftHipImuGlobalGyrX   = raw_pdo.leftHipImuGlobalGyrX;
            s_snapshot.leftHipImuGlobalGyrY   = raw_pdo.leftHipImuGlobalGyrY;
            s_snapshot.leftHipImuGlobalGyrZ   = raw_pdo.leftHipImuGlobalGyrZ;
            s_snapshot.rightHipImuGlobalGyrX  = raw_pdo.rightHipImuGlobalGyrX;
            s_snapshot.rightHipImuGlobalGyrY  = raw_pdo.rightHipImuGlobalGyrY;
            s_snapshot.rightHipImuGlobalGyrZ  = raw_pdo.rightHipImuGlobalGyrZ;
            /* H10_Count */
            s_snapshot.h10AssistModeLoopCnt   = raw_pdo.h10AssistModeLoopCnt;
            s_snapshot.postProcessingCnt      = raw_pdo.postProcessingCnt;
            /* H10_State (PDO part) */
            s_snapshot.h10FSMcurrentState     = raw_pdo.h10FSMcurrentState;
            mask |= DEVICE_MASK_CM;
        }
    }

    /* === H10_State (SDO part) === */
    if (CM_Drv_IsConnected()) {
        CM_RxData_t cm_data;
        if (CM_GetRxData(&cm_data)) {
            s_snapshot.h10Mode         = (uint8_t)cm_data.sdo.h10Mode;
            s_snapshot.h10AssistLevel  = cm_data.sdo.h10AssistLevel;
            s_snapshot.isPVectorRHDone  = (uint8_t)cm_data.sdo.isPVectorRHDone;
            s_snapshot.isPVectorLHDone  = (uint8_t)cm_data.sdo.isPVectorLHDone;
            s_snapshot.h10NeutralPosSet = (uint8_t)cm_data.sdo.h10NeutralPosSet;
        }
    }

    /* === GRF Left (17B) === */
    MarvelDex_packet_t grf_pkt;
    if (MarvelDex_IsOnline(MARVELDEX_CH_LEFT)) {
        MarvelDex_GetLatest(MARVELDEX_CH_LEFT, &grf_pkt);
        memcpy(s_snapshot.grf_left_sensor_data, grf_pkt.sensorData, 14);
        s_snapshot.grf_left_battery     = grf_pkt.batteryLevel;
        s_snapshot.grf_left_status      = grf_pkt.statusFlags;
        s_snapshot.grf_left_rolling_idx = grf_pkt.rollingIndex;
        mask |= DEVICE_MASK_GRF_LEFT;
    }

    /* === GRF Right (17B) === */
    if (MarvelDex_IsOnline(MARVELDEX_CH_RIGHT)) {
        MarvelDex_GetLatest(MARVELDEX_CH_RIGHT, &grf_pkt);
        memcpy(s_snapshot.grf_right_sensor_data, grf_pkt.sensorData, 14);
        s_snapshot.grf_right_battery     = grf_pkt.batteryLevel;
        s_snapshot.grf_right_status      = grf_pkt.statusFlags;
        s_snapshot.grf_right_rolling_idx = grf_pkt.rollingIndex;
        mask |= DEVICE_MASK_GRF_RIGHT;
    }

    /* === External IMU (40B, float 원본) === */
    XsensMTi_packet_t imu_pkt;
    if (XsensMTi_IsOnline(0)) {
        XsensMTi_GetLatest(0, &imu_pkt);
        s_snapshot.ext_imu_q_w   = imu_pkt.q_w;
        s_snapshot.ext_imu_q_x   = imu_pkt.q_x;
        s_snapshot.ext_imu_q_y   = imu_pkt.q_y;
        s_snapshot.ext_imu_q_z   = imu_pkt.q_z;
        s_snapshot.ext_imu_acc_x = imu_pkt.acc_x;
        s_snapshot.ext_imu_acc_y = imu_pkt.acc_y;
        s_snapshot.ext_imu_acc_z = imu_pkt.acc_z;
        s_snapshot.ext_imu_gyr_x = imu_pkt.gyr_x;
        s_snapshot.ext_imu_gyr_y = imu_pkt.gyr_y;
        s_snapshot.ext_imu_gyr_z = imu_pkt.gyr_z;
        mask |= DEVICE_MASK_EXT_IMU;
    }

    /* === IMU Hub (125B, int16 raw) === */
    ImuHub_RxData_t imu_hub_data;
    if (ImuHub_Drv_IsConnected()) {
        if (ImuHub_Drv_GetRxData(&imu_hub_data)) {
            s_snapshot.imu_hub_timestamp      = imu_hub_data.timestamp;
            s_snapshot.imu_hub_connected_mask = imu_hub_data.connected_mask;
            memcpy(s_snapshot.imu_hub_sensor, imu_hub_data.imu,
                   sizeof(imu_hub_sensor_t) * 6);
            mask |= DEVICE_MASK_IMU_HUB;
        }
    }

    /* === External IO (27B) === */
    {
        /* DIO 8핀 → 1바이트 bitpack */
        uint8_t dio = 0;
        for (int i = 0; i < 8; i++) {
            if (XM_DigitalRead((XmDioPin_t)i) == XM_HIGH) {
                dio |= (1U << i);
            }
        }
        s_snapshot.dio_state = dio;

        /* ADC active mask: bit0~3 = ADC1~4 (항상 활성), bit4~11 = ADC5~12 (DIO 전환 시 활성) */
        uint16_t adc_mask = 0x000F;  /* ADC1~4 항상 활성 */
        for (int i = 0; i < 8; i++) {
            if (XM_IsDioSwitchedToAdc((XmDioPin_t)i)) {
                adc_mask |= (1U << (i + 4));
            }
        }
        s_snapshot.adc_active_mask = adc_mask;

        /* ADC 12채널 */
        for (int i = 0; i < 12; i++) {
            s_snapshot.adc_channel[i] = XM_AnalogRead((XmAdcPin_t)i);
        }
    }

    /* === EMG Hub (16B) === */
    EmgHub_RxData_t emg_hub_data;
    if (EmgHub_Drv_IsConnected()) {
        if (EmgHub_Drv_GetRxData(&emg_hub_data)) {
            s_snapshot.emg_status_flags    = emg_hub_data.status_flags;
            s_snapshot.emg_raw_adc         = emg_hub_data.raw_adc;
            s_snapshot.emg_voltage_uv_x10  = emg_hub_data.voltage_uv_x10;
            s_snapshot.emg_rms_uv_x10      = emg_hub_data.rms_uv_x10;
            s_snapshot.emg_envelope_uv_x10 = emg_hub_data.envelope_uv_x10;
            s_snapshot.emg_mvc_percent     = emg_hub_data.mvc_percent;
            s_snapshot.emg_is_active       = emg_hub_data.is_active;
            mask |= DEVICE_MASK_EMG_HUB;
        }
    }

    /* === FDCAN1 Diagnostics (6B) — Ch1: XM↔CM === */
    {
        IOIF_FDCANx_t ch1 = System_GetFDCAN1_Id();
        IOIF_FDCAN_GetErrorCounters(ch1, &s_snapshot.fdcan1_tec, &s_snapshot.fdcan1_rec);
        IOIF_FDCAN_GetBusStatus(ch1, &s_snapshot.fdcan1_lec, &s_snapshot.fdcan1_bus_status);
        s_snapshot.fdcan1_rx_fifo0_fill = (uint8_t)IOIF_FDCAN_GetRxFifo0FillLevel(ch1);
        s_snapshot.fdcan1_tx_fifo_free  = (uint8_t)IOIF_FDCAN_GetTxFifoFreeLevel(ch1);
    }

    /* === FDCAN2 Diagnostics (6B) — Ch2: XM↔Sensor Module === */
    {
        IOIF_FDCANx_t ch2 = System_GetFDCAN2_Id();
        IOIF_FDCAN_GetErrorCounters(ch2, &s_snapshot.fdcan2_tec, &s_snapshot.fdcan2_rec);
        IOIF_FDCAN_GetBusStatus(ch2, &s_snapshot.fdcan2_lec, &s_snapshot.fdcan2_bus_status);
        s_snapshot.fdcan2_rx_fifo0_fill = (uint8_t)IOIF_FDCAN_GetRxFifo0FillLevel(ch2);
        s_snapshot.fdcan2_tx_fifo_free  = (uint8_t)IOIF_FDCAN_GetTxFifoFreeLevel(ch2);
    }

    /* === device_online_mask 최종 설정 === */
    s_snapshot.device_online_mask = mask;
}

const XM_TotalDataPacket_t* XM_TotalData_GetLatest(uint32_t* size)
{
    if (size != NULL) {
        *size = sizeof(XM_TotalDataPacket_t);
    }
    return &s_snapshot;
}
