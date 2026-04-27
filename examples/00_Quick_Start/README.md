# 예제 00: 보드 동작 확인 (Quick Start)

본 예제는 `XM10` 보드가 정상적으로 동작하는지 확인하는 **가장 첫 번째 예제**입니다. 외부 하드웨어 연결 없이 보드 단독으로 실행하며, LED 점등 시퀀스, 버튼 입력, USB CDC 출력을 통해 시스템 전체를 빠르게 검증합니다.

> 📖 API 레퍼런스: [Task State Machine](../../docs/api-reference/01-task-state-machine.md) · [LED & Button](../../docs/api-reference/03-led-btn-control.md) · [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

## 🎯 학습 목표 (Objective)

* `User_Setup()`과 `User_Loop()`로 구성된 **진입점 구조**를 이해합니다.
* `Task State Machine(TSM)`을 생성하고 초기 상태를 등록하는 기초 방법을 학습합니다.
* LED, 버튼, USB CDC의 **기초 동작**을 확인하여 보드 정상 여부를 판별합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 **(1) 부팅 LED 시퀀스**와 **(2) 버튼 이벤트 처리**의 두 단계로 동작합니다.

### 1. 부팅 LED 시퀀스 (`Run_Entry` → `Run_Loop` 전반부)

프로그램이 시작되면 LED 1 → LED 2 → LED 3이 순차적으로 점등됩니다. 전체 LED가 잠시 유지된 후 소등되고, LED 1이 **Heartbeat** 모드로 전환되어 정상 동작을 표시합니다.

1.  **LED 1 점등** → 200ms 후 **LED 2 점등** → 200ms 후 **LED 3 점등**
2.  전체 LED 유지 (500ms)
3.  전체 소등 → **LED 1 Heartbeat** 시작 (정상 동작 표시)

### 2. 버튼 이벤트 처리 (`Run_Loop` 후반부)

부팅 시퀀스가 완료되면 버튼 입력을 감시합니다.

1.  **BTN 1 클릭:** USB CDC로 `"Hello XM10!"` 메시지를 전송합니다. LED 3이 Oneshot으로 한 번 깜빡여 전송을 알립니다.
2.  **BTN 2 클릭:** LED 2를 토글합니다 (ON ↔ OFF).

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 코드를 `user_app.c`에 복사하거나 프로젝트를 빌드합니다.
2.  펌웨어를 `XM10` 보드에 업로드(Download)하고 재부팅합니다.
3.  **부팅 시퀀스 확인:** LED 1 → 2 → 3이 순서대로 켜진 후 전체 소등, LED 1 Heartbeat가 시작되는지 확인합니다.
4.  **USB 연결:** PC와 USB를 연결하고 시리얼 터미널(Tera Term, PuTTY 등)을 열어 `"[QuickStart] XM10 보드 준비 완료!"` 메시지를 확인합니다.
5.  **BTN 1** 을 눌러 `"Hello XM10!"` 메시지가 터미널에 출력되는지 확인합니다.
6.  **BTN 2** 를 눌러 LED 2가 켜졌다 꺼졌다 하는지 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **부팅 속도 변경:** `BOOT_LED_INTERVAL_MS`와 `BOOT_LED_HOLD_MS` 값을 변경하여 부팅 LED 시퀀스의 속도를 빠르게 또는 느리게 조절해보세요.
* **BTN 3 기능 추가:** 코드에 `XM_BTN_3` 이벤트 처리를 추가하여 LED 3을 토글하거나, 다른 USB 메시지를 전송하도록 확장해보세요.
* **부팅 완료 후 LED 효과:** 부팅 시퀀스 완료 후 LED 1의 Heartbeat 대신 다른 LED 효과(`XM_LED_BLINK` 등)를 적용해보세요.
