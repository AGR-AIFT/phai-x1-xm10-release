# 예제 11: Passive Mode 구현

본 예제는 `XM10`의 핵심 기능인 **Task State Machine**과 **P-Vector**와 **I-Vector**를 사용하여, `KIT H10`이 사용자의 개입 없이 설정된 범위(ROM) 내에서 부드러운 왕복 운동을 지속하는 **Passive Mode**를 구현하는 방법을 보여줍니다.

## 🎯 학습 목표 (Objective)

* `Task State Machine` API를 사용하여 `XM_STATE_OFF`, `XM_STATE_STANDBY`, `XM_STATE_ACTIVE` 상태를 가진 체계적인 애플리케이션을 구성하는 방법을 학습합니다.
* 안전한 시작을 위한 **Homing(원점 복귀)** 절차를 구현하는 방법을 이해합니다.
* `I-Vector`를 사전에 설정하여 **임피던스 제어**를 수행하는 방법을 학습합니다.
* `P-Vector`를 반복적으로 전송하여 **연속적인 궤적**을 생성하는 방법을 학습합니다.
* **FIFO Pre-Queuing** 기법을 사용하여 P-Vector 세그먼트 간 **끊김 없는 연속 궤적**을 구현하는 방법을 학습합니다.
* `XM.status.h10.isPVector...Done` 플래그와 `XM_ClearPVectorDoneFlag()` 함수를 사용한 **이벤트 기반 상태 전환** 로직을 이해합니다.
* 모드 변경 시 `XM_SendPVectorReset()`을 사용하여 **안전하게 동작을 중지**하는 방법을 학습합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 크게 **(1) 초기화 및 원점 복귀**, **(2) Passive Mode 실행**, **(3) 안전한 모드 전환**의 세 단계로 구성된 상태 머신을 기반으로 동작합니다.
**Passive Mode**에서 사용되는 제어는 `XM.status.h10`의 `rightHipMotorAngle`, `leftHipMotorAngle` 각도를 기준으로 수행됩니다. **해당 각도는 `KIT H10`의 구동기 출력측의 `Encoder`를 통해 측정된 각도입니다.**

### 1. 초기화 및 원점 복귀 (`InitHoming`)

`XM10` 태스크의 상태가 `XM_STATE_STANDBY`중에 `XM_H10_MODE_ASSIST`가 감지되면(`XM.status.h10.h10Mode`), 로봇은 안전한 시작을 위해 **원점(0도)으로 복귀하는 Homing 절차**를 시작합니다.

1.  **P-Vector 리셋:** `XM_SendPVectorReset()`을 호출하여 MD의 궤적 기준점을 현재 모터 위치로 동기화합니다.
2.  **임피던스 설정:** `XM_SendIVectorKpKdMax`와 `XM_SendIVector()`를 호출하여 제어에 적합한 파라미터들을 설정합니다.
3.  **이동 시작:** 현재 각도에서 0도까지 이동하는 `P-Vector`를 전송합니다. 이때 이동 시간(`L`)은 목표 속도(`HOMING_SPEED_RH`)에 따라 동적으로 계산됩니다.
4.  **완료 대기:** `XM.status.h10.isPVector...Done` 플래그가 `true`가 될 때까지 기다립니다.
5.  **상태 전환:** Homing이 완료되면 메인 Task의 상태를 `XM_STATE_ACTIVE`로 전환합니다.

### 2. Passive Mode 실행 (`UpdatePassiveMode`)

`XM10` 태스크의 상태가 `XM_STATE_ACTIVE` 상태에 진입하면, `UpdatePassiveMode` 함수가 주기적으로 호출되어 **최대 각도와 최소 각도 사이를 끊임없이 왕복**합니다. **FIFO Pre-Queuing** 기법을 사용하여 세그먼트 간 끊김 없는 연속 궤적을 생성합니다.

1.  **첫 왕복 시작 (Pre-Queue):** 최대 각도로의 `P-Vector`와 최소 각도로의 `P-Vector`를 **연속으로 2개** 전송하여 MD의 FIFO에 미리 큐잉합니다. 첫 번째 세그먼트가 완료되면 두 번째가 즉시 시작되어 **끊김이 없습니다.**
2.  **연속 큐잉:** 첫 번째 세그먼트 완료 플래그(`isPVector...Done`)가 수신되면, 두 번째 세그먼트는 이미 실행 중이므로 **다음 방향의 P-Vector를 미리 큐에 추가**합니다.
3.  **반복:** 매 세그먼트 완료 시마다 다음 궤적을 미리 큐잉하여, MD의 FIFO에 항상 1개 이상의 대기 궤적이 존재하도록 유지합니다.

### 3. 안전한 모드 전환 (`ManageModeTransition`)

사용자가 `KIT H10`의 `KIT_ASSIST_MODE`를 끄면(`XM.status.h10.h10Mode`의 값이 `XM_H10_MODE_STANDBY`로 변경), `ManageModeTransition` 함수가 **안전하게 움직임을 중단**시킵니다.

1.  **궤적 리셋:** `XM_SendPVectorReset()`을 호출하여 현재 진행 중인 `P-Vector` 궤적 생성을 즉시 취소합니다.
2.  **현재 위치 정지:** `XM_StopMotorAndHold()` 함수가 현재 모터 각도를 읽어와, **목표 위치가 현재 위치인 P-Vector**를 전송하여 부드럽게 그 자리에 멈추도록 합니다.
3.  **임피던스 설정 초기화:** `EnterStandbyMode()`을 호출하여 임피던스 설정을 초기화합니다.
4.  **상태 전환:** 정지 동작이 완료되면 메인 Task의 상태를 `XM_STATE_STANDBY`로 안전하게 되돌립니다. 

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 소스파일을 `user_app.c`으로 옮겨와서 빌드하고 펌웨어를 `XM10`에 업로드합니다. (user_app.c를 삭제하고 파일 그대로 옮겨와도 됩니다.)
2.  `KIT H10`의 전원을 켜고 `XM10`과 연결합니다.
3.  `angel'a DEV` 또는 'KIT H10'의 전원 버튼 더블 클릭을 통해 모드를 **`ASSIST_MODE`로 변경**합니다.
4.  로봇 다리가 `Homing`에 의해 먼저 **원점(0도)으로 이동**한 후, **설정된 최대/최소 각도 사이를 자동으로 왕복**하는 것을 확인합니다.
5.  KIT의 모드를 다시 **`STANDBY_MODE`로 변경**합니다.
6.  로봇 다리가 **그 자리에서 부드럽게 정지**하는 것을 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* `passive_mode.c` 파일 상단의 `#define` 값을 수정하여 운동 특성을 변경해보세요.
* `JOINT_ANGLE_MAX_ANGLE_INT16` / `JOINT_ANGLE_MIN_ANGLE_INT16` 값을 변경하여 **운동 범위(ROM)**를 조절해보세요.
* `PM_SPEED_RH` / `PM_SPEED_LH` 값을 변경하여 **왕복 운동 속도**를 조절해보세요.
* `XM_SendIVectorKpKdMax`의 `kp`, `kd` 최대값을 조절하여 적절한 제어 게인을 조절해보세요.
* `XM_SendIVector`의 파라미터를 조절하여 제어 성능을 튜닝해보세요.

