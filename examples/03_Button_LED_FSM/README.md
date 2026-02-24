# 예제 03: 상태 머신 제어 (Button & LED FSM)

본 예제는 로봇 제어의 핵심인 유한 상태 머신(FSM: Finite State Machine)을 다룹니다. 시스템을 대기(Standby)와 동작(Active) 두 가지 상태로 나누고, **롱 프레스(Long Press)** 이벤트를 통해 상태를 전환하는 구조를 구현합니다.

## 🎯 학습 목표 (Objective)

* **TSM(Task State Machine)** API를 사용하여 상태를 정의하고 등록하는 전체 과정을 학습합니다.
* `Entry`, `Loop` 함수의 역할과 실행 시점을 이해합니다.
* **롱 프레스(Long Press)** 이벤트를 활용하여 오작동을 방지하는 모드 전환법을 학습합니다.
* 상태에 따라 LED 패턴(`Heartbeat` vs `Blink`)을 다르게 설정하여 사용자에게 피드백을 주는 방법을 배웁니다.

---

## ⚙️ 동작 원리 (How it Works)

이 프로그램은 두 개의 상태(`XM_STATE_STANDBY`, `XM_STATE_ACTIVE`)를 오가며 동작합니다.

1.  **XM_STATE_STANDBY (대기):**
    * **진입(Entry):** LED 1을 **심장박동(Heartbeat)** 모드로 설정하여 대기 중임을 알립니다.
    * **반복(Loop):** 버튼 1이 1초 이상 눌리는지(`XM_BTN_LONG_PRESS`) 감시합니다. 감지되면 `XM_STATE_ACTIVE`로 전환합니다.
2.  **XM_STATE_ACTIVE (동작):**
    * **진입(Entry):** LED 1을 빠르게 **깜빡임(Blink)** 모드로 설정하여 동작 중임을 경고합니다.
    * **반복(Loop):** 버튼 1이 1초 이상 눌리면 다시 `XM_STATE_STANDBY`로 복귀합니다.
3.  **전환(Transition):** `XM_TSM_TransitionTo` 함수가 호출되면 현재 상태를 정리하고 다음 상태의 `Entry` 함수를 실행합니다.

---

## 🚀 실행 방법 (How to Use)

1.  코드를 빌드하고 펌웨어를 업로드합니다.
2.  **초기 상태:** **LED 1**이 천천히 두근거리는지(Heartbeat) 확인합니다.
3.  **모드 변경:** **버튼 1**을 1초 이상 **꾸욱** 누릅니다.
4.  **동작 상태:** **LED 1**이 빠르게 깜빡이는지(Blink) 확인합니다.
5.  다시 **버튼 1**을 1초 이상 꾸욱 누르면 초기 상태로 돌아옵니다.

---

## 💡 직접 해보기 (Things to Try)

* **제 3의 모드 추가:** `ERROR` 상태를 만들고, `XM_STATE_ACTIVE` 상태에서 버튼 2를 누르면 `ERROR` 상태로 진입하여 빨간색 LED(LED 3)를 켜보세요.
* **자동 복귀:** `XM_STATE_ACTIVE` 상태 진입 후 5초가 지나면 자동으로 `XM_STATE_STANDBY`로 돌아오도록 `XM_GetTick()`을 활용해 코드를 수정해보세요.
