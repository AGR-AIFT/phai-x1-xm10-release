/**
 ******************************************************************************
 * @file    xm_api_data.c
 * @author  HyundoKim
 * @brief   XM10 데이터 및 제어 API 구현부
 * @details 
 * End User가 'XM' 전역 구조체와 'XM_Set...' 함수를 통해 
 * 시스템과 상호작용하는 로직을 구현합니다.
 * 실제 데이터의 업데이트와 전송은 'System Core'에서 백그라운드로 처리됩니다.
 * @version 0.1
 * @date    Nov 17, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api_data.h"

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

// 전역 인스턴스 (Linker가 core_process의 extern 참조와 연결함)
XmRobot_t XM;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void XM_SetControlMode(XmControlMode_t mode)
{
    // 모드가 변경될 때 안전 조치
    if (XM.command.control_mode != mode) {
        // 어떤 모드로 바뀌든, 일단 명령 버퍼를 0으로 초기화하여 급발진 방지
        XM.command.assist_torque_rh = 0.0f;
        XM.command.assist_torque_lh = 0.0f;
        
        // 드라이버 쪽 Staging 영역도 0으로 덮어쓰기 위해 Flag 설정 (Flush 트리거)
        XM.command._dirty_flags.torque_rh_updated = 1;
        XM.command._dirty_flags.torque_lh_updated = 1;
    }
    // 모드 적용
    XM.command.control_mode = mode;
}

void XM_SetAssistTorque(float rh, float lh)
{
    XM.command.assist_torque_rh = rh;
    XM.command.assist_torque_lh = lh;
    
    // 변경되었음을 표시 (둘 다 업데이트)
    XM.command._dirty_flags.torque_rh_updated = 1;
    XM.command._dirty_flags.torque_lh_updated = 1;
}

void XM_SetAssistTorqueRH(float rh)
{
    XM.command.assist_torque_rh = rh;
    XM.command._dirty_flags.torque_rh_updated = 1;
}

void XM_SetAssistTorqueLH(float lh)
{
    XM.command.assist_torque_lh = lh;
    XM.command._dirty_flags.torque_lh_updated = 1;
}

/**
 * =================================================================================================
 * 시스템 및 연결 상태 API (System & Connection Status API)
 * =================================================================================================
 */

/**
 * @brief 제어 모듈(CM)과의 통신 연결 상태를 확인합니다.
 */
bool XM_IsCmConnected(void)
{
    // 실제 연결 상태 확인 로직은 System 계층에 위임합니다.
    return CM_XM_Link_IsConnected();
}

/**
 * @brief 현재 CM과의 PnP(NMT) 상태를 가져옵니다.
 */
LinkNmtState_t XM_GetXMNmtState(void)
{
    // 실제 NMT 상태 값은 cm_xm_link 모듈이 관리합니다.
    return CM_XM_Link_GetXMNmtState();
}

/**
 * =================================================================================================
 * 데이터 송신 API (Data Transmission API)
 * =================================================================================================
 */

/**
 * @brief 사용자 신체 정보를 CM으로 전송합니다.
 */
void XM_SendUserBodyData(const uint32_t bodyData[8])
{
    CM_SendUserBodyData(bodyData);
}

/**
 * @brief 지정된 관절에 위치 기반 궤적(P-Vector)을 전송합니다.
 */
void XM_SendPVector(SystemNodeID_t nodeId, const PVector_t* pVector)
{
    CM_SendPVector(nodeId, (CM_PVector_t*)pVector);
}

/**
 * @brief 지정된 관절에 P-Vector Reset을 전송합니다.
 */
void XM_SendPVectorReset(SystemNodeID_t nodeId)
{
    CM_SendPVectorReset(nodeId);
}

/**
 * @brief P-Vector 완료 플래그를 수동으로 클리어합니다.
 */
void XM_ClearPVectorDoneFlag(SystemNodeID_t nodeId)
{
    // 실제 로직은 하위 cm_drv 계층에 위임
    CM_ClearPVectorCompletedFlag(nodeId);
}

/**
 * @brief 지정된 관절에 힘 기반 궤적(F-Vector)을 전송합니다.
 */
void XM_SendFVector(SystemNodeID_t nodeId, const FVector_t* fVector)
{
    CM_SendFVector(nodeId, (CM_FVector_t*)fVector);
}

/**
 * @brief 지정된 관절에 임피던스 궤적(I-Vector)을 전송합니다.
 */
void XM_SendIVector(SystemNodeID_t nodeId, const IVector_t* iVector)
{
    CM_SendIVector(nodeId, (CM_IVector_t*)iVector);
}

/**
 * @brief 지정된 관절에 임피던스 궤적(I-Vector)을 전송합니다.
 */
void XM_SendIVectorKpKdMax(SystemNodeID_t nodeId, const float kpMax, const float kdMax)
{
    CM_SendIVectorKpKdmax(nodeId, kpMax, kdMax);
}

/**
 * @brief 지정된 관절(Node)의 각도 제한 루틴을 활성화/비활성화합니다.
 */
void XM_SetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet)
{
    // 실제 기능은 cm_driver 계층에 위임
    CM_SendSetDegreeLimitRoutine(nodeId, isSet);
}

/**
 * @brief 지정된 관절(Node)의 가동범위(ROM) 상/하한을 설정합니다.
 */
void XM_SetDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit)
{
    CM_SendDegreeLimit(nodeId, upperLimit, lowerLimit);
}

/**
 * @brief 지정된 관절(Node)의 각도 제한 루틴을 활성화/비활성화합니다.
 */
void XM_SetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet)
{
    // 실제 기능은 cm_driver 계층에 위임
    CM_SendSetVelocityLimitRoutine(nodeId, isSet);
}

/**
 * @brief 지정된 관절(Node)의 가동속도범위(ROM) 상/하한을 설정합니다.
 */
void XM_SetVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit)
{
    CM_SendVelocityLimit(nodeId, upperLimit, lowerLimit);
}

/**
 * @brief 지정된 관절(Node)의 외란 관측기(DOB) 루틴을 활성화/비활성화합니다.
 */
void XM_SetDOBRoutine(SystemNodeID_t nodeId, bool isSet)
{
    CM_SendSetDOBRoutine(nodeId, isSet);
}

/**
 * @brief 지정된 관절(Node)의 Normal Compensation Gain을 설정합니다.
 */
void XM_SetNormalCompGain(SystemNodeID_t nodeId, uint8_t gain)
{
    CM_SendNormalCompGain(nodeId, gain);
}

/**
 * @brief 지정된 관절(Node)의 Resistive Compensation Gain을 설정합니다.
 */
void XM_SetResistiveCompGain(SystemNodeID_t nodeId, float gain)
{
    CM_SendResistiveCompGain(nodeId, gain);
}

/**
 * @brief H10의 기존 보조 알고리즘 설정 여부를 전송합니다.
 */
void XM_SetH10AssistExistingMode(bool isSet)
{
    CM_SendSetH10AssistExistingMode(isSet);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */
