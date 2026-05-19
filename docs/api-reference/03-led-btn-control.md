# API Reference: LED & Button Control

> 📌 **이 페이지를 읽고 나면**: 내장 LED 3개 + 버튼 3개를 폴링/이벤트 양방향 모두 제어할 수 있습니다.
> ⏱️ 예상 학습 시간: 15분
> 🧰 사전 지식: [Ex.01](../../examples/01_Button_LED_Basic/) (폴링) + [Ex.02](../../examples/02_Button_LED_Event/) (이벤트) + [Ex.03](../../examples/03_Button_LED_FSM/) (롱프레스)
> 🎯 핵심 함수: `XM_SetLedState` / `XM_SetLedEffect` / `XM_GetButtonState` / `XM_GetButtonEvent` / `XM_SetChannelLedRGB` (Rev2.0)

`xm_api_led_btn.h`에 정의된 **내장 UI(LED, Button) 제어 API**에 대한 상세 레퍼런스입니다.
XM10 펌웨어는 사용자가 복잡한 타이머 인터럽트나 디바운싱(Debouncing) 로직을 직접 구현할 필요 없이, 직관적인 함수 호출만으로 **깜빡임(Blink)**, **클릭(Click)**, **롱 프레스(Long Press)** 등의 고급 입출력 기능을 구현할 수 있도록 돕습니다.

-----

## 1\. 동작 원리 (Operating Principle)

이 모듈은 시간 기반(Time-driven)으로 동작하며, 이를 위해 시스템 내부의 **Manager**들이 백그라운드에서 열심히 일하고 있습니다.

### The Core Engine: `XM_IO_Update()`

  * **역할:** LED의 깜빡임 타이밍을 계산하고, 버튼의 신호를 분석하여 노이즈를 제거(Debouncing)하고 이벤트를 생성하는 **엔진**입니다.
  * **구동:** `core_process`에 의해 **2ms(500Hz) 주기**로 자동 호출되므로, 사용자가 직접 호출할 필요는 없으나 그 존재와 역할은 이해하고 있어야 합니다.
  * **흐름:**
    1.  사용자가 `XM_SetLedEffect` 호출 -\> 설정값 저장.
    2.  `XM_IO_Update` 주기적 실행 -\> 시간이 흐름에 따라 LED ON/OFF 토글.
    3.  사용자가 버튼 누름 -\> `XM_IO_Update`가 시간 측정 -\> '클릭' 또는 '롱 프레스' 판단 후 이벤트 큐에 저장.
    4.  사용자가 `XM_GetButtonEvent` 호출 -\> 큐에서 꺼내서 반환.

-----

## 2\. 데이터 구조 (Enumerations)

사용 편의성을 위해 모든 식별자와 상태 값은 **Enum**으로 정의되어 있습니다. 숫자(0, 1, 2...) 대신 이 상수를 사용하세요.

### 2.1. Identifiers (식별자)

#### `XmLedId_t`

제어할 Function LED의 번호입니다.

```c
typedef enum {
    XM_LED_1 = 1, /**< 왼쪽 LED */
    XM_LED_2 = 2, /**< 가운데 LED */
    XM_LED_3 = 3  /**< 오른쪽 LED */
} XmLedId_t;
```

#### `XmBtnId_t`

입력을 확인할 Function Button의 번호입니다.

```c
typedef enum {
    XM_BTN_1 = 1, /**< 왼쪽 버튼 */
    XM_BTN_2 = 2, /**< 가운데 버튼 */
    XM_BTN_3 = 3  /**< 오른쪽 버튼 */
} XmBtnId_t;
```

-----

### 2.2. State Types (상태 정의)

#### `XmState_t`

LED 등의 논리적인 On/Off 상태를 표현합니다.

```c
typedef enum {
    XM_OFF = 0, /**< 끄기 (Logic Low) */
    XM_ON  = 1  /**< 켜기 (Logic High) */
} XmState_t;
```

#### `XmBtnState_t`

버튼의 현재 물리적 눌림 상태를 표현합니다. (회로의 Active-High/Low 여부와 상관없이 추상화됨)

```c
typedef enum {
    XM_RELEASED = 0, /**< 버튼이 떨어져 있음 (안 눌림) */
    XM_PRESSED  = 1  /**< 버튼이 눌려 있음 */
} XmBtnState_t;
```

-----

### 2.3. Modes & Events (모드 및 이벤트)

#### `XmLedMode_t`

LED의 동작 패턴을 결정합니다.
| Mode | Description |
| :--- | :--- |
| **`XM_LED_OFF`** | LED를 끕니다. |
| **`XM_LED_SOLID`** | LED를 계속 켭니다. |
| **`XM_LED_BLINK`** | 일정 주기(Period)로 깜빡입니다. (50% Duty) |
| **`XM_LED_HEARTBEAT`** | 심장 박동처럼 두 번 빠르게 깜빡입니다. (두근-두근) |
| **`XM_LED_ONESHOT`** | 설정한 시간만큼 한 번 켜졌다가 자동으로 꺼집니다. (알림용) |

#### `XmBtnEvent_t`

버튼 조작을 분석하여 생성된 고수준 이벤트입니다.
| Event | Description |
| :--- | :--- |
| `XM_BTN_NONE` | 발생한 이벤트 없음 |
| `XM_BTN_PRESSED` | 버튼을 막 누른 순간 (Rising Edge) |
| `XM_BTN_RELEASED` | 버튼을 막 뗀 순간 (Falling Edge) |
| **`XM_BTN_CLICK`** | 짧게 눌렀다 뗌 (클릭) |
| **`XM_BTN_LONG_PRESS`** | 1초 이상 길게 누름 (모드 변경 등에 사용) |

-----

## 3\. 함수 상세 (Function Reference)

### 3.1. LED Control Functions

#### `XM_SetLedState`

LED를 단순히 켜거나 끕니다. 가장 직관적인 제어 함수입니다.

  * **Syntax**
    ```c
    void XM_SetLedState(XmLedId_t led_id, XmState_t state);
    ```
  * **Parameters**
      * `led_id`: 제어할 LED (`XM_LED_1` \~ `3`)
      * `state`: 목표 상태 (`XM_ON`, `XM_OFF`)
  * **Example**
    ```c
    // 1번 LED 켜기
    XM_SetLedState(XM_LED_1, XM_ON);
    ```

#### `XM_SetLedEffect`

LED에 특수 효과(깜빡임 등)를 설정합니다. 설정 후에는 `XM_IO_Update()`에 의해 자동으로 동작하므로, 별도의 타이머 코드를 작성할 필요가 없습니다.

  * **Syntax**
    ```c
    void XM_SetLedEffect(XmLedId_t led_id, XmLedMode_t mode, uint32_t period_ms);
    ```
  * **Parameters**
      * `led_id`: 제어할 LED
      * `mode`: 동작 모드 (`XM_LED_BLINK`, `XM_LED_HEARTBEAT` 등)
      * `period_ms`: 효과의 주기 또는 지속 시간 (밀리초 단위)
          * `BLINK`: 깜빡임 주기 (예: 500 = 0.5초 간격)
          * `ONESHOT`: 켜져 있는 시간 (예: 200 = 0.2초간 점등 후 소등)
          * `SOLID`/`OFF`: 무시됨 (0 입력 권장)
  * **Example**
    ```c
    // 2번 LED를 0.5초 간격으로 깜빡이게 설정
    XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 500);

    // 3번 LED를 심장 박동 모드로 설정 (1초 주기)
    XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 1000);
    ```

-----

### 3.2. Button Control Functions

#### `XM_GetButtonState`

버튼의 **현재 물리적 상태**를 실시간으로 확인합니다. (Polling 방식)

  * **Syntax**
    ```c
    XmBtnState_t XM_GetButtonState(XmBtnId_t btn_id);
    ```
  * **Parameters**
      * `btn_id`: 확인할 버튼 (`XM_BTN_1` \~ `3`)
  * **Returns**: `XM_PRESSED` (눌림) 또는 `XM_RELEASED` (안 눌림)
  * **Example**
    ```c
    // 버튼 1을 누르고 있는 동안만 실행
    if (XM_GetButtonState(XM_BTN_1) == XM_PRESSED) {
        // ... 동작 수행 ...
    }
    ```

#### `XM_GetButtonEvent`

버튼에서 발생한 **최신 이벤트**를 가져옵니다. (Event-Driven 방식)
이벤트를 한 번 읽으면 내부 큐에서 사라집니다 (**Read-Clear**). 따라서 변수에 저장해서 사용하거나 `switch` 문에서 사용하는 것이 좋습니다.

  * **Syntax**
    ```c
    XmBtnEvent_t XM_GetButtonEvent(XmBtnId_t btn_id);
    ```
  * **Parameters**
      * `btn_id`: 확인할 버튼
  * **Returns**: 감지된 이벤트 (`XM_BTN_CLICK`, `XM_BTN_LONG_PRESS` 등)
  * **Example**
    ```c
    XmBtnEvent_t evt = XM_GetButtonEvent(XM_BTN_1);

    if (evt == XM_BTN_CLICK) {
        // 버튼 1이 '딸깍' 클릭되었을 때 1회 실행
        XM_SendUsbDebugMessage("Button 1 Clicked!\r\n");
    } 
    else if (evt == XM_BTN_LONG_PRESS) {
        // 버튼 1이 '꾸욱' 눌렸을 때 1회 실행
        XM_SendUsbDebugMessage("Button 1 Long Pressed!\r\n");
    }
    ```

-----

### 3.3. Channel LED (Rev2.0 전용)

#### `XM_SetChannelLedRGB`

> **Rev2.0 전용** — Rev1.1에서는 사용할 수 없습니다.

PCA9957 기반의 채널별 RGB LED를 제어합니다. 센서 모듈 연결 상태를 시각적으로 표시하는 데 사용합니다.

  * **Syntax**
    ```c
    void XM_SetChannelLedRGB(XmChannelLed_t ch, uint8_t r, uint8_t g, uint8_t b);
    ```
  * **Parameters**

    | 이름 | 설명 |
    |------|------|
    | `ch` | 채널 식별자 (`XM_CH_LED_EMG`, `XM_CH_LED_FES`, `XM_CH_LED_IMU`, `XM_CH_LED_HMMG`, `XM_CH_LED_GRF_L`, `XM_CH_LED_GRF_R`, `XM_CH_LED_USB`) |
    | `r` | Red 밝기 (0~255) |
    | `g` | Green 밝기 (0~255) |
    | `b` | Blue 밝기 (0~255) |

  * **Note**: `r=0, g=0, b=0`으로 설정하면 시스템 자동 제어로 복구됩니다.

  * **Example**
    ```c
    XM_SetChannelLedRGB(XM_CH_LED_IMU, 255, 0, 0);  // IMU LED를 빨간색으로
    XM_SetChannelLedRGB(XM_CH_LED_IMU, 0, 0, 0);     // 시스템 자동 제어 복구
    ```

---

### 3.4. System Functions

#### `XM_IO_Update`

**[시스템 필수]** LED와 버튼의 상태를 업데이트하는 구동 함수입니다.
User Task의 무한 루프(`User_Loop` 또는 `TSM_Run` 내부)에서 주기적으로 호출되어야 `Blink`나 `Long Press` 기능이 정상 동작합니다.

  * **Syntax**
    ```c
    void XM_IO_Update(void);
    ```
  * **Note**: `core_process.c`가 `_FetchAllInputs` 과정에서 자동으로 호출해주므로, 일반적인 경우 **End User가 직접 호출할 필요는 없습니다.**

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| LED 가 깜빡 모드 설정했는데 동작 안 함 | `XM_IO_Update` 가 호출 안 됨 (User Task 가 멈췄거나 `XM_TSM_Run` 누락) | `core_process` 가 자동 호출하므로 사용자 코드가 무한 루프에 빠지지 않았는지 확인 |
| `GetButtonEvent` 가 같은 이벤트를 여러 번 반환 | Run_Loop 안에서 다른 곳에서도 `GetButtonEvent` 호출 (Read-Clear 동시 소비) | 한 이벤트 채널 = 한 곳에서만 읽기 |
| `XM_BTN_LONG_PRESS` 가 트리거 안 됨 | 누르고 있는 시간 < 1초 | 1초 이상 유지 필요 |
| `SetLedEffect` 후에 `SetLedState` 가 안 먹힘 | 효과 모드가 우선 — `SetLedState` 가 무시되거나 덮어써짐 | `SetLedEffect(LED, XM_LED_OFF, 0)` 으로 효과 해제 후 사용 |
| 가운데/우측 버튼 누름이 안 잡히거나 엉뚱한 `XM_BTN_N` 으로 잡힘 | 보드 Rev 와 다른 ZIP 으로 빌드 — 내장 버튼 MCU 핀이 Rev 1.1 (PC10/11/12) 과 Rev 2.0 (PC11/12/13) 에서 한 칸 시프트되었기 때문 | 보드 라벨에서 본인 Rev 확인 후 같은 Rev 의 ZIP 으로 재 import ([보드 리비전 비교](../hardware/README.md#보드-리비전-비교)) |
| Channel LED RGB 가 동작 안 함 | Rev1.1 사용 — PCA9957 미장착 | Rev2.0 전용 — Rev1.1 은 내장 LED 3개만 |
| `ONESHOT` LED 가 1회 후에 다시 안 켜짐 | 의도된 동작 (One-shot) | 매번 다시 트리거하려면 이벤트마다 `SetLedEffect` 호출 |

---

## 관련 예제

| 예제 | 난이도 | LED/버튼 활용 |
|------|--------|-------------|
| [00_Quick_Start](../../examples/00_Quick_Start/) | 입문 | 부팅 시퀀스 + 토글 |
| [01_Button_LED_Basic](../../examples/01_Button_LED_Basic/) | 초급 | 폴링 상태 미러링 |
| [02_Button_LED_Event](../../examples/02_Button_LED_Event/) | 초급 | 이벤트 + ONESHOT |
| [03_Button_LED_FSM](../../examples/03_Button_LED_FSM/) | 중급 | 롱프레스 + Heartbeat/Blink |
| [18_Debug_Monitor](../../examples/18_Debug_Monitor/) | 중급 | 진단 LED 패턴 |
