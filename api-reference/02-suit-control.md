# API 레퍼런스: KIT H10 제어 & Data Interface

`XM10`의 핵심 가치중 하나는 `KIT H10` 로봇을 직접 설계한 알고리즘으로 제어하는 것입니다. 본 API는 SUIT H10과의 연결 상태를 확인하고, 로봇의 현재 상태 데이터를 실시간으로 수신하며, `PIF-Vectors`, `Aux inputs`와 같은 제어 명령을 전송하여 로봇의 움직임을 제어하는 데 필요한 기능을 제공합니다.

---

## 📌 개요 (Overview)

XM10은 **IPO(Input-Process-Output)** 모델을 따릅니다. 사용자는 `XM.status`에서 센서 값을 읽고(Read-Only), `XM_Set...` 함수를 통해 제어 명령을 내립니다.

## 🛠 데이터 구조 (Data Structures)

### `XmRobot_t` (Global Instance `XM`)

```c
typedef struct {
    XmInput_t  status;  // [Read] 센서 데이터 (H10, GRF, IMU)
    XmOutput_t command; // [Write] 제어 명령 (Helper 함수 사용)
} XmRobot_t;
```

  * **주요 필드 접근:**
      * `XM.status.h10.leftHipAngle`: 왼쪽 고관절 각도 (Degree)
      * `XM.status.h10.rightHipTorque`: 오른쪽 현재 토크 (Nm)
      * `XM.status.grf.leftSensorData`: 왼쪽 FSR 센서 배열
      * `XM.status.imu.acc_z`: IMU 수직 가속도

### `XmControlMode_t`

  * `XM_CTRL_MONITOR` (0): 모니터링 모드. 제어 명령을 전송하지 않습니다. (안전)
  * `XM_CTRL_TORQUE` (1): 토크 제어 모드. 설정된 토크를 모터로 전송합니다.

-----

## 📚 함수 (Functions)

### `XM_SetControlMode`

로봇의 제어 권한을 설정합니다. 모드 변경 시 안전을 위해 모든 토크 명령이 0으로 초기화됩니다.

  * **Parameters**
      * `XmControlMode_t mode`: 설정할 모드

### `XM_SetAssistTorque`

양쪽 다리의 보조 토크를 설정합니다. 설정된 값은 다음 제어 주기에 전송됩니다.

  * **Parameters**
      * `float r`: 오른쪽 토크 (Nm)
      * `float l`: 왼쪽 토크 (Nm)
  * **Example**
    ```c
    // 5.0Nm 토크 출력
    XM_SetAssistTorque(5.0f, 5.0f);
    ```

## 1. 시스템 및 연결 상태 (System & Connection)

알고리즘을 시작하기 전, `XM10`이 `KIT H10`의 `제어 모듈(Control Module)`과 안정적으로 통신하고 있는지 반드시 확인해야 합니다.

### `IsCmConnected()`

`제어 모듈(CM)`과의 통신 연결이 활성화(`Operational`) 상태인지 확인합니다.
`제어 모듈(CM)`과 `XM10`간 연결은 내부 `Plug and Play` 백그라운드 태스크에 의해 수행되며, **PDO 데이터를 수신 받은 첫 시점부터 통신 연결을 활성화 상태**로 판단합니다.

**Syntax**
```c
bool IsCmConnected(void);
```

**Returns**
- `true`: 연결이 정상적으로 활성화된 상태입니다.
- `false`: 연결이 끊겼거나 아직 준비되지 않은 상태입니다.

**Example**
```c
#include "xm_api.h"

void UpdateOffState(void) {
    // CM과 연결이 확인되면 Standby 상태로 전환합니다.
    if (IsCmConnected()) {
        TransitionTaskTo(s_mainTaskHandle, TASK_STATE_STANDBY);
    }
}
```

---

## 2. 데이터 수신 (Data Inputs)

`KIT H10`의 데이터는 `GetSuitData()` 함수 하나로 간편하게 받아올 수 있습니다.
**`KIT H10`과 `XM10`간 데이터 송수신 목록은 고정이나, 추후 변경될 여지가 있습니다.**

### `GetSuitData()`

`KIT H10`으로부터 수신된 모든 최신 데이터를 `RxData_t` 구조체에 채워줍니다. 이 함수는 제어 루프가 시작될 때마다 주기적으로 호출하여 로봇의 현재 상태를 파악하는 데 사용됩니다.
사용자는 `RunUserAlgorithm()`함수 내에서 **(1) 데이터 수신 → (2) 알고리즘 계산 -> (3) 데이터 송신**에 따라 `RunTask()`함수 앞에서 `GetSuitData()`함수를 수행하면 됩니다.

**Syntax**
```c
bool GetSuitData(RxData_t* data);
```

**Parameters**
- `data`: 수신된 데이터를 저장할 `RxData_t` 구조체의 포인터.

**Returns**
- `true`: 새로운 데이터가 성공적으로 수신되었습니다.
- `false`: 새로운 데이터가 없습니다.

**`RxData_t` 구조체 주요 멤버:**
| PDO데이터 | 설명 | 단위 | 타입 |
| :--- | :--- | :--- | :-- |
| `suitAssistModeLoopCnt` | 보조 모드 루프 카운트 | count | uint32_t |
| `leftHipAngle` | 왼쪽 고관절 각도 | degree | float |
| `rightHipAngle` | 오른쪽 고관절 각도 | degree | float |
| `leftThighAngle`| 왼쪽 허벅지 절대각 | degree | float |
| `rightThighAngle` | 오른쪽 허벅지 절대각 | degree| float |
| `pelvicAngle`| 골반 절대각 | degree | float |
| `pelvicVelY` | 골반 Y축 각속도 | deg/s | float |
| `leftKneeAngle` | 추정된 왼쪽 무릎 각도 | degree | float |
| `rightKneeAngle` | 추정된 오른쪽 무릎 각도 | degree | float |
| `isLeftFootContact`| 왼쪽 발 접지 여부 | - | bool |
| `isRightFootContact` | 오른쪽 발 접지 여부 | - | bool |
| `gaitState`| 보행 상태 | state | uint8_t |
| `gaitCycle` | 현재 보행 주기 | % | uint8_t |
| `forwardVelocity`| 추정된 전진 속도 | m/s | float |
| `leftHipTorque` | 왼쪽 고관절 토크 | Nm | float |
| `rightHipTorque`| 오른쪽 고관절 토크 | Nm | float |
| `leftHipMotorAngle` | 왼쪽 고관절 모터 각도 | degree | float |
| `rightHipMotorAngle` | 오른쪽 고관절 모터 각도 | degree | float |

| SDO데이터 | 설명 | 단위 | 타입 |
| :--- | :--- | :--- | :-- |
| `suitMode` | 현재 KIT H10 모드(보조/대기) | - | SuitMode_t |
| `suitAssistLevel` | 보조 레벨 | level(0-10) | uint8_t |
| `isPVectorRHDone` | 오른쪽 P-Vector 완료 여부 | - | float |
| `isPVectorLHDone`| 왼쪽 P-Vector 완료 여부 | - | float |

**Example**
```c
// 데이터를 저장할 전역 구조체를 선언합니다.
static RxData_t s_suitData;

void RunUserAlgorithm(void)
{
    // CM 연결 상태 체크
    if (!IsCmConnected()) {
        TransitionTaskTo(s_mainTaskHandle, TASK_STATE_OFF);
    }

    // 매 루프 시작 시, CM으로부터 최신 데이터를 가져와 내부 캐시에 저장
    if (IsCmConnected()) UpdateSuitData();

    // Task State Machine 프레임워크를 통해 현재 상태의 Run 함수 실행
    RunTask(s_mainTaskHandle);
}

static void UpdateSuitData(void)
{
  // GetSuitData()를 통해 모든 수신 데이터를 한번에 가져옴
	GetSuitData(&s_suitData);
}
```

---

## 3. 데이터 송신 (Data Outputs, Control inputs)

`P-Vector`, `I-Vector`, `F-Vector` (PIF-Vectors)와 다양한 제어 명령을 통해 `KIT H10`의 움직임을 정밀하게 설계할 수 있습니다.
**PIF-Vector와 같은 사전정의된 제어 기법의 자세한 내용에 대해서는 `[angel Robotics-Control Algorithm]`(작성 예정)에서 확인할 수 있습니다.**

### `SendPVector()`

**위치 기반 궤적**(`P-Vector`)을 전송하여, 지정된 시간 동안 목표 위치까지 부드러운 궤적을 그리며 움직이도록 명령합니다.
**반드시, `I-Vector`에 의해 사전에 임피던스 제어 파라미터가 설정되어 있어야 합니다.**
**`P-Vector` 전송시, 모터드라이버에서 5차 polynomial 형태의 위치 궤적을 생성합니다.**

**P-Vector 기반 위치 궤적 생성 예시**
<img width="1912" height="841" alt="image" src="https://github.com/user-attachments/assets/ebd67c3d-2b5d-4453-b081-c20d8750204d" />

**Syntax**
```c
void SendPVector(SystemNodeID_t nodeId, const PVector_t* pVector);
```

**Parameters**
- `nodeId`: 명령을 전달할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).
- `pVector`: 목표 위치(`yd`), 이동 시간(`L`), 시작 가속도(`s0`), 끝 감속도(`sd`) 정보가 담긴 `PVector_t` 구조체의 포인터.

**Returns**
없음.

**`pVector` 구조체 주요 멤버:**
| 멤버 | 설명 | 단위 | 타입 |
| :--- | :--- | :--- | :--- |
| `yd` | 목표 위치 | degree, scaled by 100 | int16_t |
| `L` | 이동 시간 | ms | uint16_t |
| `s0`| 시작 가속도 | deg/s^2 | uint8_t |
| `sd` | 끝 감속도 | deg/s^2 | uint8_t |

**Example**
```c
static void UpdatePassiveMode(void)
{
    // s_suitData 캐시에서 현재 각도를 읽어옵니다.
    int16_t currentAngleRH = (int16_t)round(s_suitData.rightHipMotorAngle * 10.0f);
    int16_t currentAngleLH = (int16_t)round(s_suitData.leftHipMotorAngle * 10.0f);

    switch (s_passiveState) {
        case PASSIVE_STATE_SET_IMPEDANCE: {
            // 위치 제어를 위한 임피던스(강성) 설정
            IVector_t stiffImpedance = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
            SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
            SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
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
            SendPVector(SYS_NODE_ID_RH, &pVecRH);
            SendPVector(SYS_NODE_ID_LH, &pVecLH);

            s_passiveState = PASSIVE_STATE_MOVING_TO_MIN;
            break;
        }
        // 다음 상태 동작 수행
        ...
```

### `SendIVector()`

**임피던스 제어 파라미터**(`I-Vector`)를 전송하여, 로봇 관절이 마치 용수철이나 댐퍼처럼 동작하도록 설정합니다.
사전에 `kp`와 `kd`의 최대값을 `KIT H10`의 **구동기 최대 토크인 10Nm**와 전체 시스템의 동작을 보면서 **신중히 튜닝**해야 합니다. (`SendIVectorKpKdMax()`)
**구동기 최대 전류는 14A이고, 모터드라이버 내부 임피던스 제어 입력 생성시 최대 10A에서 Saturation을 수행하도록 되어 있습니다.**

**`I-Vector`(빨강)와 `P-Vector`(파랑)를 통한 위치 기반 제어 시뮬레이션 예시**
<img width="1875" height="1024" alt="image" src="https://github.com/user-attachments/assets/abd3a1e3-55cd-4f33-b103-52c22d88a4a2" />

**Syntax**
```c
void SendIVector(SystemNodeID_t nodeId, const IVector_t* iVector);
```

**Parameters**
- `nodeId`: 명령을 전달할 관절.
- `iVector`: 임피던스 파라미터(Stiffness `kP`, Damping `kD` 등)가 담긴 `IVector_t` 구조체의 포인터.

**Returns**
없음.

**`iVector` 구조체 주요 멤버:**
| 멤버 | 설명 | 단위 | 타입 |
| :--- | :--- | :--- | :--- |
| `epsilon` | 코리더(Corridor)의 절반 폭 | degree, scaled by 10 | uint8_t |
| `kp` | 가상 스프링 강도 | % | uint8_t |
| `kd`| 가상 댐퍼 강도 | % | uint8_t |
| `lambda` | 임피던스 비율 | ratio, scaled by 100 | uint8_t |
| `duration`| 전환 시간 | ms | uint16_t |

**Example**
```c
static void EnterStandbyMode(void)
{
    // 임피던스 설정 및 파라미터 해제
    IVector_t stiffImpedance = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
    SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
    SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
    ClearPVectorDoneFlag(SYS_NODE_ID_RH);
    ClearPVectorDoneFlag(SYS_NODE_ID_LH);
}
```

### `SendFVector()`

**힘 기반 궤적**(`F-Vector`)을 전송하여, **지정된 시간 동안 사전 정의된 토크 궤적을 생성**하도록 명령합니다.

**F-Vector 기반 힘 궤적 생성 예시**
<img width="1295" height="799" alt="image" src="https://github.com/user-attachments/assets/a39ffb45-f10f-4e61-a1c7-b0f235dbc0c7" />
<img width="1538" height="846" alt="image" src="https://github.com/user-attachments/assets/3560f9a4-d9fa-407e-b9cd-eb24faae42c9" />

**Syntax**
```c
void SendFVector(SystemNodeID_t nodeId, const FVector_t* fVector);
```

**Parameters**
- `nodeId`: 명령을 전달할 관절.
- `fVector`: 목표 토크(`tauMax`), 모드(`modeIdx`) 등의 정보가 담긴 `FVector_t` 구조체의 포인터.

**Returns**
없음.

**`fVector` 구조체 주요 멤버:**
| 멤버 | 설명 | 단위 | 타입 |
| :--- | :--- | :--- | :--- |
| `modeIdx` | 토크 프로파일 인덱스 | Index (0.1 ~ 10) | uint16_t |
| `tauMax` | 최대 토크 | A, scaled by 100	 | int16_t |
| `delay`| 초기 지연 시간 | ms | uint16_t |
| `zero` | 리셋을 위한 더미 데이터 | - | uint16_t |

**Example**
```c
(작성 예정)
```

### `SendPVectorReset()`

현재 실행 중이거나 대기 중인 `P-Vector` 명령을 즉시 초기화(리셋)합니다. 비상 상황이나 사용자의 의도가 바뀌었을 때 현재 동작을 중단시키는 용도로 사용됩니다.

**Syntax**
```c
void SendPVectorReset(SystemNodeID_t nodeId);
```

**Parameters**
- `nodeId`: P-Vector를 리셋할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).

**Returns**
없음.

**Example**
```c
static void ManageModeTransition(void)
{
    // 현재 모드 값 가져오기
    SuitMode_t currentSuitMode  = s_suitData.suitMode;

    switch (s_modeTransitionState) {
        case MODE_TRANSITION_IDLE:
            // 평상시에 모드 변경이 감지되었는지 확인합니다.
            if (currentSuitMode != s_previousSuitMode) {
                
                // Passive Mode -> Standby Mode 로의 전환
                // P-Vector를 사용하던 Passive Mode를 안전하게 정지시키는 절차를 시작합니다.
                if (s_previousSuitMode == SUIT_ASSIST_MODE && currentSuitMode == SUIT_STANDBY_MODE) {
                    SendPVectorReset(SYS_NODE_ID_RH);   // P-Vector 궤적 생성 취소 명령 전송
                    SendPVectorReset(SYS_NODE_ID_LH);
                    s_modeTransitionTimer = GetTick();  // reset 지연 타이머 시작
                    s_modeTransitionState = MODE_TRANSITION_STOP_PENDING; // 다음 상태로 전환
                }
            }
            break;
        // 다음 상태 동작 수행
        ...
```

### `ClearPVectorDoneFlag()`

`GetSuitData()`를 통해 `isPVectorRHDone` 또는 `isPVectorLHDone` 플래그가 `true`가 된 것을 확인한 후, 이벤트를 처리했음을 `XM10`이 알기 위해서는 이 함수를 수동으로 호출해야 합니다. 이 함수를 호출하지 않으면 플래그가 계속 `true`로 남아 동일한 완료 이벤트가 반복 처리될 수 있습니다.
사용 시 `GetSuitData()`를 통해 받은 `isPVectorRHDone` 또는 `isPVectorLHDone` 플래그가 false로 초기화 됨.

**Syntax**
```c
void ClearPVectorDoneFlag(SystemNodeID_t nodeId);
```

**Parameters**
- `nodeId`: 완료 플래그를 클리어할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).

**Example**
```c
static void EnterStandbyMode(void)
{
    // 임피던스 설정 및 파라미터 해제
    IVector_t stiffImpedance = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
    SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
    SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
    ClearPVectorDoneFlag(SYS_NODE_ID_RH);
    ClearPVectorDoneFlag(SYS_NODE_ID_LH);
}
```

### `SendIVectorKpKdMax()`

지정된 관절의 임피던스 제어에서 사용될 **최대 Kp(Stiffness)와 Kd(Damping) 값을 설정**합니다. 이는 `SendIVector`에서 백분율(%)로 전달되는 가상 스프링 및 댐퍼 강도의 기준이 됩니다.

**Syntax**
```c
void SendIVectorKpKdMax(SystemNodeID_t nodeId, const float kpMax, const float kdMax);
```

**Parameters**
- `nodeId`: 파라미터를 설정할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).
- `kpMax`: `SendIVector`의 `kp` 파라미터가 100%일 때 적용될 최대 Kp(가상 스프링 강성) 값입니다.
- `kdMax`: `SendIVector`의 `kd` 파라미터가 100%일 때 적용될 최대 Kd(가상 댐퍼 강성) 값입니다.

**Example**
```c
static void InitHoming(void)
{
    static uint32_t homingTimer = 0;
    // --- Homing 상태 머신 ---
    switch (s_homingState) {
        case HOMING_ENTRY:
            SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 1);
            SendIVectorKpKdMax(SYS_NODE_ID_LH, 6, 1);
            s_homingState = HOMING_SET_IMPEDANCE;
            break;
        // 다음 상태 동작 수행
        ...
```

---

### `Set...` 루틴 및 파라미터 (Routines & Parameters)

`Set`으로 시작하는 함수들은 `KIT H10`에 내장된 다양한 제어 보조 루틴을 활성화하거나 관련 파라미터를 실시간으로 조정하는 데 사용됩니다. 이를 통해 사용자는 복잡한 하위 제어 로직을 직접 구현할 필요 없이, 고수준에서 로봇의 동작 특성을 변경할 수 있습니다.

---

### 각도 및 각속도 제한 (Angle & Velocity Limit)

로봇의 움직임을 물리적으로 안전한 범위 내로 제한하는 기능입니다.

**Syntax**
```c
// 각도 제한 루틴 활성화/비활성화
void SetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet);

// 각도 제한 범위 설정
void SetDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);

// 각속도 제한 루틴 활성화/비활성화
void SetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet);

// 각속도 제한 범위 설정
void SetVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);
```

**Parameters**
- `nodeId`: 제어할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).
- `isSet`: 해당 루틴을 활성화하려면 `true`, 비활성화하려면 `false`를 전달합니다.
- `upperLimit`: 설정할 가동 범위의 상한값 (단위: degree 또는 deg/s).
- `lowerLimit`: 설정할 가동 범위의 하한값 (단위: degree 또는 deg/s).

**Example**
```c
// 오른쪽 다리의 각도 제한 기능을 활성화하고,
// 가동 범위를 -30도에서 30도 사이로 설정합니다.
SetDegreeLimitRoutine(SYS_NODE_ID_RH, true);
SetDegreeLimit(SYS_NODE_ID_RH, 30.0f, -30.0f);

// 왼쪽 다리의 최대 속도를 100 deg/s로 제한합니다.
SetVelocityLimitRoutine(SYS_NODE_ID_LH, true);
SetVelocityLimit(SYS_NODE_ID_LH, 100.0f, -100.0f);
```

---

### 외란 관측기 (Disturbance Observer)

사용자가 가하는 힘이나 예상치 못한 외부 힘(외란)을 추정하고 보상하여, 더 부드럽고 안정적인 움직임을 만들어내는 `KIT H10`에 내장된 고급 제어 루틴입니다.
**`DOB` 기능을 사용하기 위해서는 `KIT H10`의 구동기가 `DOB`기능에 대한 식별(`System Identification`)이 진행되어 모터드라이버의 `DOB` 식별 정보 기록 여부를 확인해야 합니다.(현재 `KIT H10`은 `DOB` 식별을 진행하지 않았음, 추후 변경 예정)**
**`KIT H10`의 DOB에 대해서는 [`angel Robotics-Control Algorithm`](작성 예정)에서 확인할 수 있습니다.**

**Syntax**
```c
void SetDOBRoutine(SystemNodeID_t nodeId, bool isSet);
```

**Parameters**
- `nodeId`: 제어할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).
- `isSet`: DOB 루틴을 활성화하려면 `true`, 비활성화하려면 `false`를 전달합니다.

**Example**
```c
SetDOBRoutine(SYS_NODE_ID_RH, true);
```

---

### 보상 게인 설정 (Compensation Gain)

`KIT H10`에 내장된 기본 중력/속도 보상 모드의 강도를 조절합니다.
**`KIT H10`의 보상에 대해서는 [`angel Robotics-Compensation`](작성 예정)에서 확인할 수 있습니다.**

**Syntax**
```c
// 일반 보상 게인 설정 (중력 보상 등)
void SetNormalCompGain(SystemNodeID_t nodeId, uint8_t gain);

// 저항 보상 게인 설정 (저항 훈련 모드)
void SetResistiveCompGain(SystemNodeID_t nodeId, float gain);
```

**Parameters**
- `nodeId`: 제어할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).
- `gain`: 설정할 게인 값. 값의 범위와 효과는 각 보상 모드에 따라 다릅니다.

**Example**
```c
// 저항 훈련 모드에서 오른쪽 다리의 저항을 강하게 설정합니다.
float strongResistance = 0.8f;
SetResistiveCompGain(SYS_NODE_ID_RH, strongResistance);
```

---

### `SendUserBodyData()`

사용자의 신체 정보(몸무게, 키, 분절 길이 등)를 `KIT H10`로 전송합니다. `KIT H10`은 이 정보를 바탕으로 실시간 동작 분석을 수행하며 더 정확하고 개인화된 보행 데이터 및 운동 역학 데이터를 계산하여 XM10으로 보내줍니다. **`KIT H10`의 실시간 동작 분석의 자세한 내용은 [`GaitAnalysis`](작성 예정)에서 확인할 수 있습니다.**
**사용자가 직접 신체 정보를 측정하여 `KIT H10`으로 전송해야 합니다.**

**`RxData_t` 구조체 중 신체 정보 기반 데이터:**
| PDO데이터 | 설명 | 단위 | 타입 |
| :--- | :--- | :--- | :-- |
...
| `leftKneeAngle` | **추정된** 왼쪽 무릎 각도 | degree | float |
| `rightKneeAngle` | **추정된** 오른쪽 무릎 각도 | degree | float |
| `isLeftFootContact`| 왼쪽 발 접지 여부 | - | bool |
| `isRightFootContact` | 오른쪽 발 접지 여부 | - | bool |
| `gaitState`| 보행 상태 | state | uint8_t |
| `gaitCycle` | 현재 보행 주기 | % | uint8_t |
| `forwardVelocity`| **추정된** 전진 속도 | m/s | float |
...

**Syntax**
```c
void SendUserBodyData(const uint32_t bodyData[8]);
```

**Parameters**

`bodyData` 8개의 `uint32_t` 신체 정보를 담은 배열:
| 인덱스 | 설명 | 단위 | 타입 |
| :--- | :--- | :--- | :-- |
| `0` | 착용자 몸무게 | g | uint32_t |
| `1` | 착용자 키 | mm | uint32_t |
| `2`| 착용자 오른쪽 허벅지 분절 길이 | mm | uint32_t |
| `3` | 착용자 왼쪽 허벅지 분절 길이 | mm | uint32_t |
| `4`| 착용자 오른쪽 종아리 분절 길이 | mm | uint32_t |
| `5` | 착용자 왼쪽 종아리 분절 길이 | mm | uint32_t |
| `6`| 착용자 오른쪽 발목 분절 길이 | mm | uint32_t |
| `7` | 착용자 왼쪽 종아리 분절 길이 | mm | uint32_t |

**Example**
```c
// 신체 정보를 입력하고 CM에 전송합니다.
bodyData[0] = 73000; // 73kg
bodyData[1] = 1800;  // 180cm
bodyData[2] = 500;   // 0.5m
bodyData[3] = 495;   // 0.495m
bodyData[4] = 440;   // 0.440m
bodyData[5] = 435;   // 0.435m
bodyData[6] = 60;    // 0.06m
bodyData[7] = 59;    // 0.059m
SendUserBodyData(&bodyData[0]);
```

---

## 실시간 제어(Real-time Control)

**2ms** 제어 루프 내에서 실시간으로 토크를 인가하는 데 사용되는 핵심 함수들입니다.

### `StageAuxTorque()`

지정된 관절에 **보조 토크**(`Auxiliary Torque`)를 인가하도록 **예약**합니다. 이 함수는 데이터를 즉시 전송하지 않고, 내부 전송 대기열에 토크 값을 올려놓기만 합니다. 이는 **2ms** 루프 내의 다른 계산들과 전송 로직을 분리(`decoupling`)하여 시스템의 실시간성을 보장하기 위함입니다.

**Syntax**
```c
void StageAuxTorque(SystemNodeID_t nodeId, float torque);
```

**Parameters**
- `nodeId`: 토크를 인가할 관절 (`SYS_NODE_ID_RH` 또는 `SYS_NODE_ID_LH`).
- `torque`: 인가할 보조 토크 값 (단위: Nm).

**Example**
```c
void RunUserAlgorithm(void)
{
    // CM 연결 상태를 최우선으로 확인하여, 연결이 끊겼을 경우 OFF 상태로 강제 전환합니다.
    if (!IsCmConnected()) {
        TransitionTaskTo(s_aaTaskHandle, TASK_STATE_OFF);
    }

    // 매 루프 시작 시, CM으로부터 최신 데이터를 가져와 내부 캐시에 저장합니다.
    // Do not Remove
    if (IsCmConnected()) UpdateSuitData();

    // Task State Machine 프레임워크를 통해 현재 상태에 맞는 Run 함수를 실행합니다.
    RunTask(s_aaTaskHandle);

    // 스테이징된 모든 PDO 데이터(보조 토크 등)를 CM으로 전송
    // Do not Remove
    if (IsCmConnected()) FlushControlData();
}

static void UpdateSingleLegAssistLogic(ActiveAssistFsm_t* fsm, float currentThighAngle_deg, SystemNodeID_t nodeId)
{
    int16_t currentAngle_deg10 = (int16_t)round(currentThighAngle_deg * 10.0f);
    
    // LPF를 이용한 토크 스무딩
    fsm->currentTorque_Nm = (fsm->targetTorque_Nm * TORQUE_SMOOTHING_FACTOR) +
                            (fsm->currentTorque_Nm * (1.0f - TORQUE_SMOOTHING_FACTOR));
    StageAuxTorque(nodeId, ((float)s_suitData.suitAssistLevel / 10.0f) * fsm->currentTorque_Nm);
    // 다음 동작 수행
    ...
```

### `FlushControlData()`

`StageAuxTorque()`를 통해 예약된 모든 실시간 제어 데이터(보조 토크 등)를 **하나의 CAN 메시지로 묶어** `KIT H10`으로 **즉시 전송**합니다. 이 함수는 **2ms 제어 루프의 가장 마지막에 항상 호출**되어야 합니다.

**Syntax**
```c
void FlushControlData(void);
```

**Example**
```c
void RunUserAlgorithm(void)
{
    // CM 연결 상태를 최우선으로 확인하여, 연결이 끊겼을 경우 OFF 상태로 강제 전환합니다.
    if (!IsCmConnected()) {
        TransitionTaskTo(s_aaTaskHandle, TASK_STATE_OFF);
    }

    // 매 루프 시작 시, CM으로부터 최신 데이터를 가져와 내부 캐시에 저장합니다.
    // Do not Remove
    if (IsCmConnected()) UpdateSuitData();

    // Task State Machine 프레임워크를 통해 현재 상태에 맞는 Run 함수를 실행합니다.
    RunTask(s_aaTaskHandle);

    // 스테이징된 모든 PDO 데이터(보조 토크 등)를 CM으로 전송
    // Do not Remove
    if (IsCmConnected()) FlushControlData();
}
```

---

## 유틸리티 (Utilities)

### `GetTick()`

시스템 부팅 후 경과된 시간을 밀리초(ms) 단위로 반환합니다. 특정 동작의 시간을 측정하거나, 일정 시간 동안만 로직을 수행하는 등의 시간 기반 제어에 필수적입니다.

**Syntax**
```c
uint32_t GetTick(void);
```

**Returns**
- `uint32_t`: 부팅 후 경과된 시간 (ms). 이 값은 약 49.7일마다 0으로 되돌아갑니다(rollover).

**Example**
```c
static void ManageModeTransition(void)
{
    SuitMode_t currentSuitMode = s_suitData.suitMode;

    switch (s_modeTransitionState) {
        case MODE_TRANSITION_IDLE:
            // 평상시에 모드 변경이 감지되었는지 확인합니다.
            if (currentSuitMode != s_previousSuitMode) {
                
                // Active-Assist Mode -> Standby Mode 로의 전환
                // [CASE 1] Homing 중 P-Vector를 사용하던 AA Mode를 안전하게 정지시키는 절차를 시작합니다.
                if (s_previousSuitMode == SUIT_ASSIST_MODE && currentSuitMode == SUIT_STANDBY_MODE 
                    && s_aaGlobalState == AA_STATE_HOMING) {
                    SendPVectorReset(SYS_NODE_ID_RH);   // P-Vector 궤적 생성 취소 명령 전송
                    SendPVectorReset(SYS_NODE_ID_LH);
                    s_modeTransitionTimer = GetTick();  // reset 지연 타이머 시작
                    s_modeTransitionState = MODE_TRANSITION_STOP_PENDING; // 다음 상태로 전환
                }
        // 다음 상태 동작 수행
        ...
```
