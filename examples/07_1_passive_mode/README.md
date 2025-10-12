# 예제 07-1: Passive Mode 구현

본 예제는 `XM10`의 핵심 기능인 **Task State Machine**과 **P-Vector**와 **I-Vector**를 사용하여, `SUIT H10`이 사용자의 개입 없이 설정된 범위(ROM) 내에서 부드러운 왕복 운동을 지속하는 **Passive Mode**를 구현하는 방법을 보여줍니다.

## 🎯 학습 목표 (Objective)

* `Task State Machine` API를 사용하여 `OFF`, `STANDBY`, `ACTIVE` 상태를 가진 체계적인 애플리케이션을 구성하는 방법을 학습합니다.
* 안전한 시작을 위한 **Homing(원점 복귀)** 절차를 구현하는 방법을 이해합니다.
* `I-Vector`를 사전에 설정하여 **임피던스 제어**를 수행하는 방법을 학습합니다.
* `P-Vector`를 반복적으로 전송하여 **연속적인 궤적**을 생성하는 방법을 학습합니다.
* `isPVector...Done` 플래그와 `ClearPVectorDoneFlag()` 함수를 사용한 **이벤트 기반 상태 전환** 로직을 이해합니다.
* 모드 변경 시 `SendPVectorReset()`을 사용하여 **안전하게 동작을 중지**하는 방법을 학습합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 크게 **(1) 초기화 및 원점 복귀**, **(2) Passive Mode 실행**, **(3) 안전한 모드 전환**의 세 단계로 구성된 상태 머신을 기반으로 동작합니다.
**Passive Mode**에서 사용되는 제어는 `RxData_t`의 `rightHipMotorAngle`, `leftHipMotorAngle` 각도를 기준으로 수행됩니다. **해당 각도는 `SUIT H10`의 구동기 출력측의 `Encoder`를 통해 측정된 각도입니다.**

### 1. 초기화 및 원점 복귀 (`InitHoming`)

`XM10` 태스크의 상태가 `STANDBY`중에 `SUIT_ASSIST_MODE`가 감지되면(`suitMode`), 로봇은 안전한 시작을 위해 **원점(0도)으로 복귀하는 Homing 절차**를 시작합니다.

1.  **임피던스 설정:** `SendIVectorKpKdMax`와 `SendIVector()`를 호출하여 제어에 적합한 파라미터들을 설정합니다.
2.  **이동 시작:** 현재 각도에서 0도까지 이동하는 `P-Vector`를 전송합니다. 이때 이동 시간(`L`)은 목표 속도(`HOMING_SPEED_RH`)에 따라 동적으로 계산됩니다.
3.  **완료 대기:** `s_suitData.isPVector...Done` 플래그가 `true`가 될 때까지 기다립니다.
4.  **상태 전환:** Homing이 완료되면 메인 Task의 상태를 `ACTIVE`로 전환합니다.

### 2. Passive Mode 실행 (`UpdatePassiveMode`)

`XM10` 태스크의 상태가 `ACTIVE` 상태에 진입하면, `UpdatePassiveMode` 함수가 주기적으로 호출되어 **최대 각도와 최소 각도 사이를 끊임없이 왕복**합니다.

1.  **최대 각도로 이동:** `JOINT_ANGLE_MAX_ANGLE_INT16`으로 설정된 목표 위치로 `P-Vector`를 전송합니다.
2.  **완료 대기 및 방향 전환:** 이동이 완료되면(`isPVector...Done == true`), 완료 플래그를 `ClearPVectorDoneFlag()`로 클리어한 뒤, 이번에는 `JOINT_ANGLE_MIN_ANGLE_INT16`을 목표 위치로 하는 `P-Vector`를 전송합니다.
3.  **최소 각도로 이동 및 반복:** 최소 각도까지 이동이 완료되면, 다시 최대 각도를 목표로 하는 `P-Vector`를 전송하여 왕복 운동을 계속합니다.

### 3. 안전한 모드 전환 (`ManageModeTransition`)

사용자가 `SUIT H10`의 `SUIT_ASSIST_MODE`를 끄면(`suitMode`의 값이 `SUIT_STANDBY_MODE`로 변경), `ManageModeTransition` 함수가 **안전하게 움직임을 중단**시킵니다.

1.  **궤적 리셋:** `SendPVectorReset()`을 호출하여 현재 진행 중인 `P-Vector` 궤적 생성을 즉시 취소합니다.
2.  **현재 위치 정지:** `StopMotorAndHold()` 함수가 현재 모터 각도를 읽어와, **목표 위치가 현재 위치인 P-Vector**를 전송하여 부드럽게 그 자리에 멈추도록 합니다.
3.  **임피던스 설정 초기화:** `EnterStandbyMode()`을 호출하여 임피던스 설정을 초기화합니다.
4.  **상태 전환:** 정지 동작이 완료되면 메인 Task의 상태를 `STANDBY`로 안전하게 되돌립니다. 

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 프로젝트를 빌드하고 펌웨어를 `XM10`에 업로드합니다.
2.  `SUIT H10`의 전원을 켜고 `XM10`과 연결합니다.
3.  `angel'a DEV` 또는 'SUIT H10'의 전원 버튼 더블 클릭을 통해 모드를 **`ASSIST_MODE`로 변경**합니다.
4.  로봇 다리가 `Homing`에 의해 먼저 **원점(0도)으로 이동**한 후, **설정된 최대/최소 각도 사이를 자동으로 왕복**하는 것을 확인합니다.
5.  SUIT의 모드를 다시 **`STANDBY_MODE`로 변경**합니다.
6.  로봇 다리가 **그 자리에서 부드럽게 정지**하는 것을 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* `passive_mode.c` 파일 상단의 `#define` 값을 수정하여 운동 특성을 변경해보세요.
* `JOINT_ANGLE_MAX_ANGLE_INT16` / `JOINT_ANGLE_MIN_ANGLE_INT16` 값을 변경하여 **운동 범위(ROM)**를 조절해보세요.
* `PM_SPEED_RH` / `PM_SPEED_LH` 값을 변경하여 **왕복 운동 속도**를 조절해보세요.
* `SendIVectorKpKdMax`의 `kp`, `kd` 최대값을 조절하여 적절한 제어 게인을 조절해보세요.
* `SendIVector`의 파라미터를 조절하여 제어 성능을 튜닝해보세요.
