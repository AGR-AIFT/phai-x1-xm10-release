# 예제 01: 버튼과 LED 기초 (Button & LED Basic)

본 예제는 `XM10` 개발의 가장 기초가 되는 **디지털 입출력(Digital I/O)** 제어를 다룹니다. 복잡한 하드웨어 설정 없이 `XM API`를 사용하여 사용자의 버튼 입력(Input)을 감지하고, 이를 LED의 켜짐/꺼짐(Output)으로 즉각 반영하는 **직관적 반응(Momentary Switch)** 시스템을 구현합니다.

## 🎯 학습 목표 (Objective)

* `User_Setup`과 `User_Loop`로 구성된 **기본적인 코드 구조**를 이해합니다.
* `Task State Machine`을 생성하고 초기 상태를 등록하는 기초 방법을 학습합니다.
* `XM_GetButtonState()` API를 사용하여 **폴링(Polling)** 방식으로 입력 상태를 확인하는 방법을 학습합니다.
* `XM_SetLedState()` API를 사용하여 **디지털 출력**을 제어하는 방법을 학습합니다.
* 입력 조건(`if`)에 따라 출력을 실시간으로 변경하는 로직을 이해합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 **(1) 초기화**와 **(2) 무한 루프 모니터링**의 단순한 구조로 동작합니다. 시스템은 2ms(500Hz)마다 `Run_Loop` 함수를 호출하여 버튼의 상태를 확인하고 LED를 업데이트합니다.

### 1. 초기화 (`User_Setup`)

프로그램이 시작되면 가장 먼저 `User_Setup`이 호출됩니다. 여기서 TSM(Task State Machine)을 생성하고, 우리가 작성할 로직 함수(`Run_Loop`)를 등록합니다.

1.  **TSM 생성:** `XM_TSM_Create`를 사용하여 상태 머신 핸들(`s_tsm`)을 만듭니다.
2.  **상태 등록:** `XmStateConfig_t` 구조체를 사용하여 `XM_STATE_USER_START` 상태일 때 `Run_Loop` 함수가 반복 실행되도록 설정합니다.
3.  **등록 완료:** `XM_TSM_AddState`를 통해 설정을 적용합니다.

### 2. 상태 모니터링 및 제어 (`Run_Loop`)

`XM_TSM_Run`에 의해 주기적으로 호출되는 함수입니다.

1.  **입력 확인:** `XM_GetButtonState(XM_BTN_1)` 함수를 호출하여 현재 1번 버튼이 눌려있는지(`XM_PRESSED`) 확인합니다.
2.  **로직 판단:**
    * 만약 버튼이 **눌려 있다면**: `XM_SetLedState`를 호출하여 1번 LED를 **켭니다(XM_ON)**.
    * 만약 버튼이 **떨어져 있다면**: `XM_SetLedState`를 호출하여 1번 LED를 **끕니다(XM_OFF)**.
3.  **즉시 반영:** 이 과정이 매우 빠르게 반복되므로, 사용자는 버튼을 누르는 즉시 LED가 켜지는 것처럼 느끼게 됩니다.

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 코드를 `user_app.c`에 복사하거나 프로젝트를 빌드합니다.
2.  펌웨어를 `XM10` 보드에 업로드(Download)하고 재부팅합니다.
3.  보드에 있는 Function Button 1 (왼쪽 버튼)을 손가락으로 누릅니다.
4.  버튼을 누르고 있는 동안 Function LED 1 (왼쪽 LED)이 켜지는지 확인합니다.
5.  손을 떼면 LED가 즉시 꺼지는지 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **반대로 동작시키기:** 버튼을 누르면 꺼지고, 손을 떼면 켜지도록 `if` 문의 조건을 반대로 수정해보세요.
* **다른 LED 제어하기:** 버튼 1을 눌렀을 때 LED 1 대신 LED 2(가운데)나 LED 3(오른쪽)이 켜지도록 `XM_SetLedState`의 인자를 변경해보세요(`XM_LED_2`, `XM_LED_3`).
* **다중 제어:** `if` 문을 추가하여 버튼 2를 누르면 LED 2가 켜지도록 코드를 확장해보세요.
