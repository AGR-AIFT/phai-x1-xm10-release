# 예제 12: Active-Assist Mode 구현

본 예제는 사용자의 움직임 **의도**를 파악하여, 필요할 때만 목표 지점까지 부드러운 보조력을 가해 움직임을 도와주는 **Active-Assist Mode**를 구현하는 방법을 보여줍니다.

이 예제는 `Passive Mode`와 달리, 정해진 궤적을 따르는 위치 제어(`P-Vector`)가 아닌, **실시간 토크 제어 입력(Step) 생성**를 핵심으로 사용합니다.

## 🎯 학습 목표 (Objective)

* `XM_SetControlMode(XM_CTRL_TORQUE)`와 `XM_SetAssistTorqueRH(target torque)`, `XM_SetAssistTorqueLH(target torque)`를 사용한 **실시간 토크 제어 입력 생성** 방법을 학습합니다.
* 시간과 각도 임계값을 조합하여 **사용자의 움직임 의도를 감지**하는 알고리즘을 이해합니다.
* **계층적 상태 머신**을 사용하여, `Homing`과 같은 동기화 단계와 각 다리의 독립적인 보조 단계를 분리하여 관리하는 방법을 학습합니다.
* `저역 통과 필터(LPF)`를 이용해 **토크를 부드럽게(Smoothing)** 인가하는 기법을 이해합니다.
* `XM.status.h10.h10AssistLevel` 값을 연동하여 **보조력의 강도를 동적으로 조절**하는 방법을 학습합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 **(1) Homing**으로 시작 위치를 정렬한 뒤, **(2) 사용자의 의도를 추적**하고, **(3) 조건이 충족되면 토크를 보조**하는 정교한 상태 머신을 기반으로 동작합니다.

### 1. 초기 위치 정렬 (`AA_STATE_HOMING`)

모드가 시작되면, 로봇은 먼저 `Passive Mode` 예제와 유사하게 `P-Vector`와 `I-Vector`를 사용하여 **일정한 시작 위치(`JOINT_ANGLE_MIN_ANGLE_INT16`)로 이동**합니다. 이는 사용자가 항상 예측 가능한 지점에서 보조를 시작할 수 있도록 보장하는 안전 절차입니다. `Homing`이 완료되면, 임피던스 설정이 초기화되며 상위 상태는 `AA_STATE_ASSISTING`으로 전환됩니다.

### 2. 의도 감지 및 보조 (`AA_STATE_ASSISTING`)

이 단계부터 양쪽 다리는 각각 독립적인 하위 상태 머신(`AA_SubState_t`)에 따라 동작합니다.

1.  **피크에서 대기 (`AA_SUBSTATE_WAIT_AT_PEAK`):**
    움직임의 끝 지점(`Peak`)에서 사용자가 다시 움직이기 시작할 때까지 토크 없이 대기합니다. 움직임이 감지되면(`MOVEMENT_START_THRESHOLD_DEG10` 초과) 다음 단계로 넘어갑니다.

2.  **의도 추적 (`AA_SUBSTATE_TRACKING_INTENT`):**
    사용자의 움직임을 추적하며, 아래 **두 가지 조건이 모두 충족**되는지 확인합니다.
    * **시간 조건:** 움직임이 시작된 후 일정 시간(`INTENT_TRACKING_DELAY_MS`)이 경과했는가?
    * **각도 조건:** 사용자가 한 방향으로 일정 각도(`INTENT_ANGLE_THRESHOLD_DEG10`) 이상 진행했는가?

3.  **토크 보조 (`AA_SUBSTATE_PROVIDE_ASSIST_DF/AA_SUBSTATE_PROVIDE_ASSIST_PF`):**
    위 두 조건이 모두 충족되면, 시스템은 사용자의 의도를 확신하고 목표 방향으로 **보조 토크를 인가**하기 시작합니다.
    * `XM_SetAssistTorqueRH(target torque)`, `XM_SetAssistTorqueLH(target torque)` 함수를 통해 목표 토크(`ASSIST_TORQUE_NM`)를 예약합니다.
    * 이때, `XM.status.h10.h10AssistLevel` 값과 연동하면 사용자가 설정한 강도로 보조력이 조절됩니다.
    * 토크는 `Low-Pass Filter`를 통해 부드럽게 증가하여 사용자에게 안정적인 보조감을 제공합니다.
    * 사용자가 반대편 피크 지점에 도달하면, 토크를 `0`으로 되돌리고 다시 `WAIT_AT_PEAK` 상태로 복귀합니다.

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 소스파일을 `user_app.c`으로 옮겨와서 빌드하고 펌웨어를 `XM10`에 업로드합니다. (user_app.c를 삭제하고 파일 그대로 옮겨와도 됩니다.)
2.  `KIT H10`의 전원을 켜고 `XM10`과 연결합니다.
3.  `angel'a DEV` 또는 다른 제어 수단을 통해 `KIT H10`의 모드를 **`ASSIST_MODE`로 변경**합니다.
4.  로봇 다리가 먼저 **설정된 시작 위치로 이동**한 후 대기하는 것을 확인합니다.
5.  `KIT H10`의 보조 레벨(`XM.status.h10.h10AssistLevel`)을 보조력 조절 버튼으로 **1단계** 높입니다.
6.  **사용자가 직접 다리를 움직여보세요.** 잠시 후, 움직이는 방향으로 **부드러운 보조력이 느껴지는지** 확인합니다.
7.  `KIT H10`의 보조 레벨(`XM.status.h10.h10AssistLevel`)을 조절하며 **보조력의 강도가 변하는지** 테스트합니다.

---

## 💡 직접 해보기 (Things to Try)

* `active_assist_mode.c` 파일 상단의 `#define` 값을 수정하여 보조 특성을 변경해보세요.
* `ASSIST_TORQUE_NM` 값을 변경하여 **최대 보조 토크의 크기**를 조절해보세요.
* `INTENT_ANGLE_THRESHOLD_DEG10` 값을 변경하여 **의도 감지의 민감도**를 조절해보세요. (값이 작을수록 더 민감해집니다.)
* `TORQUE_SMOOTHING_FACTOR` 값을 변경하여 **토크가 얼마나 부드럽게/빠르게** 적용될지 조절해보세요. (값이 클수록 더 빠르게 반응합니다.)

