/**
 ******************************************************************************
 * @file    xm_api_imu_calib.h
 * @author  HyundoKim
 * @brief   CAN-FD 로 연결된 IMU Hub 의 센서를 제자리에서 보정하기 (Rev 2.0)
 * @details
 * 케이스나 옷에 넣은 센서를 꺼내지 않고 보정합니다. 지자기 보정은 장착한 그대로 돌려야 맞습니다.
 *
 * 쓰는 법
 * 1. XM_ImuCalibStart(채널, 항목) 을 **한 번만** 부릅니다. 버튼을 누른 순간처럼 한 번만 일어나는 곳에서 부르세요.
 *    매 주기 부르면 끝날 때마다 보정이 다시 시작됩니다.
 * 2. XM_ImuCalibGetStatus 로 상태를 봅니다. 함수는 기다리지 않으므로 Control_Loop 에서 불러도 됩니다.
 * 3. 지자기 · 가속도 정밀은 상태가 XM_IMU_CALIB_WAIT_USER 가 되면 센서를 움직인 뒤 XM_ImuCalibFinish 를 부릅니다.
 * 4. XM_IMU_CALIB_DONE 이면 끝, XM_IMU_CALIB_FAILED 이면 result 를 봅니다.
 *
 * 시작 요청은 IMU Hub 에 한 번만 보냅니다. 같은 보정이 두 번 도는 일을 막기 위해서입니다. 답이 오지 않으면 XM_IMU_CALIB_PENDING 에
 * 머물고 그동안 다음 요청은 XM_IMU_CALIB_REQ_BUSY 입니다. IMU Hub 가 상태를 다시 알려 주거나 연결이 끊기면 풀립니다.
 *
 * 알아 둘 것
 * - 제어 모드(XM_CTRL_CONTROL)에서는 시작할 수 없습니다. XM_CTRL_MONITOR 로 바꾼 뒤 시작하세요.
 * - 보정 중인 채널은 그 동안 XM.status.imu_hub.connected_mask 비트가 꺼집니다. 이 채널의 값으로 제어하지 마세요.
 *   끝난 뒤에는 새 데이터가 들어오면 다시 켜집니다. 보정 중에 연결이 끊겼다 돌아오면 IMU Hub 상태를 확인할 때까지 꺼져 있습니다.
 *   외부 드라이버를 XM_CanSend 로 직접 움직인다면 이 비트를 꼭 확인하세요.
 * - 중단은 지자기 · 가속도 정밀을 XM_ImuCalibFinish 하기 전까지만 됩니다. 자이로 영점 · 가속도 간이는 금방 끝나므로
 *   중단되지 않습니다.
 * - EBIMU 의 지자기 · 가속도 정밀을 중단하거나 움직이는 시간이 끝나면 모든 채널의 전원이 약 2 초 꺼졌다 켜집니다. 그 동안 다른 채널도
 *   끊기고, 채널마다 새 데이터가 들어와야 다시 연결된 것으로 보입니다. 다른 센서가 값을 저장하는 중이면 먼저 기다리므로 중단 결과가
 *   나올 때까지 약 17 초가 걸릴 수 있습니다.
 * - 움직이며 기다리는 시간은 지자기 120 초, 가속도 정밀 600 초까지입니다. 넘으면 XM_IMU_CALIB_RESULT_SESSION_TIMEOUT 으로
 *   끝납니다.
 * - MTi-630 은 자이로 영점 · 지자기만 됩니다.
 * - PC 프로그램에서 보정하는 동안 이 함수를 함께 쓰지 마세요. 나중 요청이 앞 요청을 지웁니다.
 *
 * @note  IMU Hub 는 내부에서 개발 중인 모듈입니다. 사용하려면 https://huphailab.com/contact 로 문의해 주세요.
 * @warning XM10 Rev 2.0 전용입니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_IMU_CALIB_H_
#define XM_API_XM_API_IMU_CALIB_H_

#include <stdbool.h>
#include <stdint.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief XM_ImuCalibGetSupported 의 비트 — 예: mask & XM_IMU_CALIB_OP_BIT(XM_IMU_CALIB_OP_MAG) */
#define XM_IMU_CALIB_OP_BIT(op)  (1U << ((unsigned)(op) - 1U))

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 보정 항목 */
typedef enum {
    XM_IMU_CALIB_OP_GYRO         = 1,  /**< 자이로 영점 — 센서를 가만히 둡니다 */
    XM_IMU_CALIB_OP_MAG          = 2,  /**< 지자기 — 장착한 그대로 돌린 뒤 XM_ImuCalibFinish */
    XM_IMU_CALIB_OP_ACCEL_SIMPLE = 3,  /**< 가속도 간이 — 수평으로 가만히 둡니다 */
    XM_IMU_CALIB_OP_ACCEL_FULL   = 4,  /**< 가속도 정밀 — 여러 자세를 취한 뒤 XM_ImuCalibFinish */
} XmImuCalibOp_t;

/** @brief 요청을 받았는지. 받았다고 보정이 끝난 것은 아닙니다 — 결과는 XM_ImuCalibGetStatus 로 봅니다 */
typedef enum {
    XM_IMU_CALIB_REQ_OK = 0,            /**< 받았습니다 */
    XM_IMU_CALIB_REQ_BUSY,              /**< 앞 요청을 처리하는 중입니다. 잠시 뒤 다시 부르세요 */
    XM_IMU_CALIB_REQ_NOT_CONNECTED,     /**< IMU Hub 가 연결되어 있지 않습니다 */
    XM_IMU_CALIB_REQ_CONTROL_ACTIVE,    /**< 제어 모드입니다. XM_CTRL_MONITOR 로 바꾼 뒤 시작하세요 */
    XM_IMU_CALIB_REQ_INVALID,           /**< 채널이나 항목 값이 틀렸습니다 */
} XmImuCalibRequest_t;

/** @brief 채널 상태 */
typedef enum {
    XM_IMU_CALIB_IDLE      = 0,         /**< 보정한 적 없음 */
    XM_IMU_CALIB_RUNNING   = 1,         /**< 진행 중 */
    XM_IMU_CALIB_WAIT_USER = 2,         /**< 센서를 움직이는 단계 — 끝나면 XM_ImuCalibFinish */
    XM_IMU_CALIB_DONE      = 3,         /**< 끝. 센서 데이터가 다시 들어오는 것까지 확인했습니다 */
    XM_IMU_CALIB_FAILED    = 4,         /**< 실패 · 중단 — result 를 보세요 */
    XM_IMU_CALIB_PENDING   = 5,         /**< 요청을 보내는 중 — 아직 IMU Hub 가 확인하기 전입니다 */
} XmImuCalibState_t;

/** @brief 결과 */
typedef enum {
    XM_IMU_CALIB_RESULT_NONE               = 0,   /**< 아직 없음 */
    XM_IMU_CALIB_RESULT_OK                 = 1,   /**< 성공 */
    XM_IMU_CALIB_RESULT_NO_SENSOR          = 2,   /**< 이 채널에 센서가 없습니다 */
    XM_IMU_CALIB_RESULT_NOT_SUPPORTED      = 3,   /**< 이 센서는 이 항목을 지원하지 않습니다 */
    XM_IMU_CALIB_RESULT_SENSOR_TIMEOUT     = 4,   /**< 센서가 답하지 않았습니다. info = 실패한 단계 */
    XM_IMU_CALIB_RESULT_STREAM_NOT_RESUMED = 5,   /**< 보정 뒤 센서 데이터가 다시 들어오지 않았습니다 */
    /* 6 은 쓰지 않습니다 */
    XM_IMU_CALIB_RESULT_HUB_BUSY           = 7,   /**< IMU Hub 가 지금 요청을 처리할 수 없습니다. 잠시 기다리고 연결 상태를 확인한 뒤 다시 시도하세요 */
    XM_IMU_CALIB_RESULT_ABORTED            = 8,   /**< 중단했습니다. 값은 저장되지 않았습니다 */
    XM_IMU_CALIB_RESULT_SENSOR_REJECTED    = 9,   /**< 센서가 거절했습니다. info = 센서가 보낸 상태 값 */
    XM_IMU_CALIB_RESULT_SESSION_TIMEOUT    = 10,  /**< 움직이는 단계의 시간 상한을 넘겼습니다 */
    XM_IMU_CALIB_RESULT_NO_RESPONSE        = 11,  /**< IMU Hub 가 답하지 않습니다. 펌웨어가 오래되었거나 통신 문제입니다.
                                                    *   XM_IMU_CALIB_FAILED 면 요청이 닿지 않았습니다. 다시 시작하세요.
                                                    *   XM_IMU_CALIB_PENDING 이면 보정이 이미 돌고 있을 수 있습니다. 이 채널을 쓰지 말고 답을 기다리세요.
                                                    *   그 밖의 진행 상태면 답이 다시 오는지 보고, XM_ImuCalibFinish · XM_ImuCalibAbort 를 다시 부르세요 */
    XM_IMU_CALIB_RESULT_HUB_NOT_SUPPORTED  = 12,  /**< IMU Hub 가 요청을 거절했습니다. 펌웨어가 오래되었을 수 있습니다. 업데이트해 보세요 */
    XM_IMU_CALIB_RESULT_LINK_LOST          = 13,  /**< 보정 중 IMU Hub 와 연결이 끊겼거나 IMU Hub 가 다시 시작했습니다 */
} XmImuCalibResult_t;

/** @brief 채널 하나의 보정 상태 */
typedef struct {
    XmImuCalibState_t  state;
    XmImuCalibOp_t     op;      /**< 이 상태의 항목. 보정한 적 없으면 0 */
    XmImuCalibResult_t result;
    uint8_t            info;    /**< 진행 중 = 단계 번호, 실패 = result 설명 참고, 그 밖 = 0 */
} XmImuCalibStatus_t;

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 * 전부 기다리지 않습니다. Control_Setup · Control_Loop · 백그라운드 태스크에서 부를 수 있고, 인터럽트에서는 부르지 마세요.
 */

/**
 * @brief 보정을 시작합니다
 * @param ch_mask bit0~5 = 채널 0~5. 여러 채널을 한 번에 시작할 수 있습니다. 이미 진행 중인 채널은 그대로 둡니다
 * @param op      보정 항목
 */
XmImuCalibRequest_t XM_ImuCalibStart(uint8_t ch_mask, XmImuCalibOp_t op);

/**
 * @brief 센서를 움직이는 단계를 끝냅니다 — 값을 저장합니다
 * @note  제어 모드에서도 부를 수 있습니다
 */
XmImuCalibRequest_t XM_ImuCalibFinish(uint8_t ch_mask);

/**
 * @brief 보정을 중단합니다 — 머리말의 '알아 둘 것' 을 보세요
 * @note  제어 모드에서도 부를 수 있습니다
 */
XmImuCalibRequest_t XM_ImuCalibAbort(uint8_t ch_mask);

/**
 * @brief 채널의 보정 상태를 읽습니다
 * @param ch  채널 0~5
 * @return true = out 을 채웠습니다. false 면 다음 주기에 다시 읽으세요
 */
bool XM_ImuCalibGetStatus(uint8_t ch, XmImuCalibStatus_t* out);

/** @brief 채널별로 어떤 항목을 지원하는지 IMU Hub 에 묻습니다 — 결과는 XM_ImuCalibGetSupported 로 봅니다 */
XmImuCalibRequest_t XM_ImuCalibQuerySupported(void);

/**
 * @brief 지원 항목을 읽습니다
 * @param ch           채널 0~5
 * @param[out] out_op_mask 결과가 XM_IMU_CALIB_RESULT_OK 일 때 지원 항목(XM_IMU_CALIB_OP_BIT). 0 이면 이 채널에 센서가 없습니다
 * @return NONE = 아직 답이 없음(채널 값이 틀렸거나 out_op_mask 가 NULL 일 때도), OK,
 *         NO_RESPONSE · HUB_NOT_SUPPORTED = 보정 기능이 없는 펌웨어일 수 있음, LINK_LOST
 */
XmImuCalibResult_t XM_ImuCalibGetSupported(uint8_t ch, uint8_t* out_op_mask);

#endif /* XM_API_XM_API_IMU_CALIB_H_ */
