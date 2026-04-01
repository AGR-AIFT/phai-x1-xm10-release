/* AUTO-GENERATED from xm_total_data.yaml v2.5 — DO NOT EDIT MANUALLY */
/* Generated: 2026-04-01 16:15:29 */

#ifndef XM_TOTAL_DATA_PACKET_H
#define XM_TOTAL_DATA_PACKET_H

#include <stdint.h>
#include <stdbool.h>

/* Complex type: Single IMU sensor data (int16 packed, 20 bytes) */
typedef struct __attribute__((packed)) {
    int16_t    q[4];                /* Quaternion (w,x,y,z), scale: 10000, unit: normalized */
    int16_t    a[3];                /* Acceleration (x,y,z), scale: 100, unit: g */
    int16_t    g[3];                /* Gyroscope (x,y,z), scale: 10, unit: deg/s */
} imu_hub_sensor_t;

#pragma pack(push, 1)
typedef struct {
    /* === Header (7B) === */
    uint32_t   xm_loop_count;            /* offset: 0, unit: count */
    uint16_t   device_online_mask;       /* offset: 4, unit: flags */
    uint8_t    phai_x1_status;           /* offset: 6, unit: flags */

    /* === H10_Joint (16B) === */
    int16_t    leftHipAngle;             /* offset: 7, scale: 720, unit: deg */
    int16_t    rightHipAngle;            /* offset: 9, scale: 720, unit: deg */
    int16_t    leftKneeAngle;            /* offset: 11, scale: 720, unit: deg */
    int16_t    rightKneeAngle;           /* offset: 13, scale: 720, unit: deg */
    int16_t    leftHipTorque;            /* offset: 15, scale: 60, unit: A */
    int16_t    rightHipTorque;           /* offset: 17, scale: 60, unit: A */
    int16_t    leftHipMotorAngle;        /* offset: 19, scale: 720, unit: deg */
    int16_t    rightHipMotorAngle;       /* offset: 21, scale: 720, unit: deg */

    /* === H10_Segment (6B) === */
    int16_t    leftThighAngle;           /* offset: 23, scale: 720, unit: deg */
    int16_t    rightThighAngle;          /* offset: 25, scale: 720, unit: deg */
    int16_t    pelvicAngle;              /* offset: 27, scale: 720, unit: deg */

    /* === H10_Gait (4B) === */
    uint8_t    isLeftFootContact;        /* offset: 29, unit: bool */
    uint8_t    isRightFootContact;       /* offset: 30, unit: bool */
    int16_t    forwardVelocity;          /* offset: 31, scale: 6000, unit: m/s */

    /* === H10_IMU (36B) === */
    int16_t    leftHipImuFrontalRoll;    /* offset: 33, scale: 720, unit: deg */
    int16_t    leftHipImuSagittalPitch;  /* offset: 35, scale: 720, unit: deg */
    int16_t    leftHipImuTransverseYaw;  /* offset: 37, scale: 720, unit: deg */
    int16_t    rightHipImuFrontalRoll;   /* offset: 39, scale: 720, unit: deg */
    int16_t    rightHipImuSagittalPitch; /* offset: 41, scale: 720, unit: deg */
    int16_t    rightHipImuTransverseYaw; /* offset: 43, scale: 720, unit: deg */
    int16_t    leftHipImuGlobalAccX;     /* offset: 45, scale: 78.4532, unit: m/s2 */
    int16_t    leftHipImuGlobalAccY;     /* offset: 47, scale: 78.4532, unit: m/s2 */
    int16_t    leftHipImuGlobalAccZ;     /* offset: 49, scale: 78.4532, unit: m/s2 */
    int16_t    rightHipImuGlobalAccX;    /* offset: 51, scale: 78.4532, unit: m/s2 */
    int16_t    rightHipImuGlobalAccY;    /* offset: 53, scale: 78.4532, unit: m/s2 */
    int16_t    rightHipImuGlobalAccZ;    /* offset: 55, scale: 78.4532, unit: m/s2 */
    int16_t    leftHipImuGlobalGyrX;     /* offset: 57, scale: 1000, unit: deg/s */
    int16_t    leftHipImuGlobalGyrY;     /* offset: 59, scale: 1000, unit: deg/s */
    int16_t    leftHipImuGlobalGyrZ;     /* offset: 61, scale: 1000, unit: deg/s */
    int16_t    rightHipImuGlobalGyrX;    /* offset: 63, scale: 1000, unit: deg/s */
    int16_t    rightHipImuGlobalGyrY;    /* offset: 65, scale: 1000, unit: deg/s */
    int16_t    rightHipImuGlobalGyrZ;    /* offset: 67, scale: 1000, unit: deg/s */

    /* === H10_Count (8B) === */
    uint32_t   h10AssistModeLoopCnt;     /* offset: 69, unit: count */
    uint32_t   postProcessingCnt;        /* offset: 73, unit: count */

    /* === H10_State (6B) === */
    uint8_t    h10FSMcurrentState;       /* offset: 77, unit: enum */
    uint8_t    h10Mode;                  /* offset: 78, unit: enum */
    uint8_t    h10AssistLevel;           /* offset: 79, unit: level */
    uint8_t    isPVectorRHDone;          /* offset: 80, unit: bool */
    uint8_t    isPVectorLHDone;          /* offset: 81, unit: bool */
    uint8_t    h10NeutralPosSet;         /* offset: 82, unit: bool */

    /* === GRF_Left (17B) === */
    uint8_t    grf_left_sensor_data[14]; /* offset: 83, unit: raw */
    uint8_t    grf_left_battery;         /* offset: 97, unit: % */
    uint8_t    grf_left_status;          /* offset: 98, unit: flags */
    uint8_t    grf_left_rolling_idx;     /* offset: 99, unit: count */

    /* === GRF_Right (17B) === */
    uint8_t    grf_right_sensor_data[14]; /* offset: 100, unit: raw */
    uint8_t    grf_right_battery;        /* offset: 114, unit: % */
    uint8_t    grf_right_status;         /* offset: 115, unit: flags */
    uint8_t    grf_right_rolling_idx;    /* offset: 116, unit: count */

    /* === Ext_IMU (40B) === */
    float      ext_imu_q_w;              /* offset: 117, unit: normalized */
    float      ext_imu_q_x;              /* offset: 121, unit: normalized */
    float      ext_imu_q_y;              /* offset: 125, unit: normalized */
    float      ext_imu_q_z;              /* offset: 129, unit: normalized */
    float      ext_imu_acc_x;            /* offset: 133, unit: m/s2 */
    float      ext_imu_acc_y;            /* offset: 137, unit: m/s2 */
    float      ext_imu_acc_z;            /* offset: 141, unit: m/s2 */
    float      ext_imu_gyr_x;            /* offset: 145, unit: deg/s */
    float      ext_imu_gyr_y;            /* offset: 149, unit: deg/s */
    float      ext_imu_gyr_z;            /* offset: 153, unit: deg/s */

    /* === IMU_Hub (125B) === */
    uint32_t   imu_hub_timestamp;        /* offset: 157, unit: ms */
    uint8_t    imu_hub_connected_mask;   /* offset: 161, unit: flags */
    imu_hub_sensor_t       imu_hub_sensor[6]; /* offset: 162 */

    /* === Ext_IO (27B) === */
    uint8_t    dio_state;                /* offset: 282, unit: flags */
    uint16_t   adc_active_mask;          /* offset: 283, unit: flags */
    uint16_t   adc_channel[12];          /* offset: 285, unit: raw */

    /* === EMG_Hub (16B) === */
    uint8_t    emg_status_flags;         /* offset: 309, unit: flags */
    uint16_t   emg_raw_adc;              /* offset: 310, unit: raw */
    int16_t    emg_voltage_uv_x10;       /* offset: 312, scale: 10, unit: uV */
    int16_t    emg_rms_uv_x10;           /* offset: 314, scale: 10, unit: uV */
    int16_t    emg_envelope_uv_x10;      /* offset: 316, scale: 10, unit: uV */
    uint8_t    emg_mvc_percent;          /* offset: 318, unit: % */
    uint8_t    emg_is_active;            /* offset: 319, unit: bool */
    uint8_t    rsv_emg[5];               /* offset: 320, unit: reserved */

    /* === FDCAN1_Diag (6B) === */
    uint8_t    fdcan1_tec;               /* offset: 325, unit: count */
    uint8_t    fdcan1_rec;               /* offset: 326, unit: count */
    uint8_t    fdcan1_lec;               /* offset: 327, unit: enum */
    uint8_t    fdcan1_bus_status;        /* offset: 328, unit: flags */
    uint8_t    fdcan1_rx_fifo0_fill;     /* offset: 329, unit: count */
    uint8_t    fdcan1_tx_fifo_free;      /* offset: 330, unit: count */

    /* === FDCAN2_Diag (6B) === */
    uint8_t    fdcan2_tec;               /* offset: 331, unit: count */
    uint8_t    fdcan2_rec;               /* offset: 332, unit: count */
    uint8_t    fdcan2_lec;               /* offset: 333, unit: enum */
    uint8_t    fdcan2_bus_status;        /* offset: 334, unit: flags */
    uint8_t    fdcan2_rx_fifo0_fill;     /* offset: 335, unit: count */
    uint8_t    fdcan2_tx_fifo_free;      /* offset: 336, unit: count */

    /* === Reserved_HMMG (28B) === */
    uint8_t  rsv_hmmg[28];          /* offset: 337, Future HMMG module */

} XM_TotalDataPacket_t;
#pragma pack(pop)

_Static_assert(sizeof(XM_TotalDataPacket_t) == 365,
    "XM_TotalDataPacket_t size mismatch — update YAML and regenerate");

#define XM_TOTAL_DATA_PAYLOAD_SIZE  sizeof(XM_TotalDataPacket_t)
#define XM_TOTAL_DATA_MODULE_ID     0x20
#define XM_TOTAL_DATA_NUM_CHANNELS  187   /* excluding reserved */

#endif /* XM_TOTAL_DATA_PACKET_H */
