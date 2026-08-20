# `xm_api_led_btn.h` — 내장 LED · 버튼 제어

> **대상 헤더**: `XM_FW/XM_API/xm_api_led_btn.h`
> **관련 개념 문서**: [03. LED + 버튼](../03-led-btn-control.md)
> **관련 예제**: [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) · [01_Button_LED_Basic](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/01_Button_LED_Basic/) · [02_Button_LED_Event](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/02_Button_LED_Event/) · [03_Button_LED_FSM](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/03_Button_LED_FSM/) · [18_Debug_Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/)

보드에 내장된 Function LED 3개(`XM_LED_1~3`)와 Function Button 3개(`XM_BTN_1~3`)를 제어하는 API 입니다. Rev2.0 에서는 여기에 더해 센서 모듈 연결 상태를 표시하는 채널 RGB LED(`XM_CH_LED_*`) 제어도 포함합니다.

---

## 언제 사용하나

버튼을 눌러 동작을 시작/정지하거나, LED로 현재 상태(대기/실행/에러 등)를 사용자에게 보여주고 싶을 때 사용합니다. 폴링(현재 상태 확인)과 이벤트(클릭/롱프레스 감지) 두 가지 방식을 모두 지원하므로, 원하는 방식을 골라 쓰면 됩니다. 동작 원리와 전체 예제는 [03. LED + 버튼](../03-led-btn-control.md) 문서를 먼저 읽어보는 것을 권장합니다 — 이 페이지는 각 함수의 시그니처/파라미터 상세만 다룹니다.

---

## 함수 목록

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SetLedState`](#xm_setledstate) | LED를 단순 On/Off |
| [`XM_SetLedEffect`](#xm_setledeffect) | LED에 Blink/Heartbeat/Oneshot 효과 설정 |
| [`XM_GetButtonState`](#xm_getbuttonstate) | 버튼의 현재 물리적 눌림 상태 조회 (폴링) |
| [`XM_GetButtonEvent`](#xm_getbuttonevent) | 버튼의 최신 이벤트 조회 (Read-Clear) |
| [`XM_SetChannelLedRGB`](#xm_setchannelledrgb) 🟢 Rev 2.0 전용 | 채널 RGB LED 색상 직접 설정 |
| [`XM_IO_Update`](#xm_io_update) | LED/버튼 내부 상태 업데이트 (엔진) |

---

## 함수 상세

### `XM_SetLedState`

```c
void XM_SetLedState(XmLedId_t led_idx, XmState_t state);
```

LED를 켜거나 끕니다. 효과(Blink 등) 없이 즉시 상태를 반영하는 가장 단순한 제어 함수입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `led_idx` | `XmLedId_t` | 제어할 LED (`XM_LED_1` ~ `XM_LED_3`) |
| `state` | `XmState_t` | 목표 상태 (`XM_ON`, `XM_OFF`) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. ISR에서 호출하는 용도로 설계되지 않았습니다.

**예제**

```c
XM_SetLedState(XM_LED_1, XM_ON);   // 1번 LED 켜기
```

**참고**: [`XM_SetLedEffect`](#xm_setledeffect) — 이 함수 이후 `XM_SetLedEffect`로 효과 모드가 설정되어 있으면 `XM_SetLedState` 호출이 무시되거나 덮어써질 수 있습니다 (`03-led-btn-control.md` 흔한 실수 참고).

---

### `XM_SetLedEffect`

```c
void XM_SetLedEffect(XmLedId_t led_idx, XmLedMode_t mode, uint32_t period_ms);
```

LED에 깜빡임(Blink), 심장박동(Heartbeat), 일회성 점등(Oneshot) 등의 효과를 설정합니다. 이 함수는 **설정값만 저장**하며, 실제 타이밍 계산과 On/Off 전환은 [`XM_IO_Update()`](#xm_io_update)가 주기적으로 수행합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `led_idx` | `XmLedId_t` | 제어할 LED (`XM_LED_1` ~ `XM_LED_3`) |
| `mode` | `XmLedMode_t` | 동작 모드 (`XM_LED_OFF`/`SOLID`/`BLINK`/`HEARTBEAT`/`ONESHOT`) |
| `period_ms` | `uint32_t` | 효과 주기 또는 지속 시간(ms). `BLINK`=깜빡임 주기, `ONESHOT`=점등 유지 시간, `SOLID`/`OFF`는 무시(0 권장) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. 설정 직후 효과가 바로 보이지 않을 수 있으며, `XM_IO_Update()`가 주기적으로 호출되고 있어야 반영됩니다 (User Task 안에서 자동 호출되므로 일반적으로 별도 조치 불필요).

**예제**

```c
XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 500);       // 0.5초 간격 깜빡임
XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 1000);  // 1초 주기 심장박동
```

**참고**: [`XmLedMode_t`](#xmledmode_t) 전체 모드 목록

---

### `XM_GetButtonState`

```c
XmBtnState_t XM_GetButtonState(XmBtnId_t btn_idx);
```

버튼의 현재 물리적 상태를 즉시 조회합니다 (폴링 방식). 회로가 Active-High든 Active-Low든 상관없이 항상 "눌렸는지 여부"로 추상화되어 반환됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `btn_idx` | `XmBtnId_t` | 확인할 버튼 (`XM_BTN_1` ~ `XM_BTN_3`) |

**반환값**: `XmBtnState_t` — `XM_PRESSED`(눌림) 또는 `XM_RELEASED`(안 눌림)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. 매 호출마다 실시간 상태를 반환하므로 여러 곳에서 동시에 호출해도 무방합니다 (Read-Clear가 아님).

**예제**

```c
if (XM_GetButtonState(XM_BTN_1) == XM_PRESSED) {
    XM_SetLedState(XM_LED_1, XM_ON);
}
```

**참고**: [`XM_GetButtonEvent`](#xm_getbuttonevent) — 클릭/롱프레스 같은 "동작"을 감지하려면 이벤트 방식을 사용하세요.

---

### `XM_GetButtonEvent`

```c
XmBtnEvent_t XM_GetButtonEvent(XmBtnId_t btn_idx);
```

버튼에서 발생한 최신 고수준 이벤트(클릭/롱프레스 등)를 가져옵니다 (이벤트 기반 방식).

> ⚠️ **Read-Clear 의미론**: 이벤트를 한 번 읽으면 내부 큐에서 즉시 사라집니다. 같은 버튼의 이벤트를 두 곳 이상에서 각각 `XM_GetButtonEvent`로 읽으면, 먼저 호출된 쪽이 이벤트를 "소비"해버려서 나머지 호출은 항상 `XM_BTN_NONE`을 받습니다. **한 버튼 이벤트 = 코드 상 한 곳에서만 읽기**를 지키세요. `if/else`를 여러 번 나눠 호출하지 말고, 한 번 변수에 저장한 뒤 그 변수로 분기하십시오.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `btn_idx` | `XmBtnId_t` | 확인할 버튼 (`XM_BTN_1` ~ `XM_BTN_3`) |

**반환값**: `XmBtnEvent_t` — 감지된 이벤트 (`XM_BTN_NONE`/`PRESSED`/`RELEASED`/`CLICK`/`LONG_PRESS`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**예제**

```c
XmBtnEvent_t evt = XM_GetButtonEvent(XM_BTN_1);  // 한 번만 저장해서 사용

switch (evt) {
    case XM_BTN_CLICK:
        XM_SendUsbDebugMessage("Button 1 Clicked!\r\n");
        break;
    case XM_BTN_LONG_PRESS:
        XM_SendUsbDebugMessage("Button 1 Long Pressed!\r\n");
        break;
    default:
        break;
}
```

**참고**: [`XmBtnEvent_t`](#xmbtnevent_t) 전체 이벤트 목록, [`XM_GetButtonState`](#xm_getbuttonstate) — 상태만 필요하면 이쪽이 더 단순합니다.

---

### `XM_SetChannelLedRGB` 🟢 Rev 2.0 전용

```c
void XM_SetChannelLedRGB(XmChannelLed_t ch, uint8_t r, uint8_t g, uint8_t b);
```

> 🟢 **Rev 2.0 전용** — Rev1.1 헤더에는 이 함수가 존재하지 않습니다 (PCA9957 채널 LED 드라이버 미장착).

PCA9957 24채널 LED 드라이버를 통해 센서 모듈별 채널 LED의 RGB 색상을 직접 설정합니다. 호출 시 해당 채널의 **시스템 자동 상태 LED 표시가 즉시 오버라이드**됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `ch` | `XmChannelLed_t` | 채널 식별자 (`XM_CH_LED_EMG`/`FES`/`IMU`/`HMMG`/`GRF_L`/`GRF_R`/`USB`) |
| `r` | `uint8_t` | Red PWM (0~255) |
| `g` | `uint8_t` | Green PWM (0~255) |
| `b` | `uint8_t` | Blue PWM (0~255) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**예제**

```c
XM_SetChannelLedRGB(XM_CH_LED_IMU, 255, 0, 0);  // IMU 채널 LED를 빨간색으로
XM_SetChannelLedRGB(XM_CH_LED_IMU, 0, 0, 0);    // r=g=b=0 → 시스템 자동 제어로 복구
```

**참고**: [`XmChannelLed_t`](#xmchannelled_t) 채널 목록. `r=0,g=0,b=0` 으로 호출하면 시스템 자동 상태 표시로 복구됩니다.

---

### `XM_IO_Update`

```c
void XM_IO_Update(void);
```

LED 깜빡임 타이밍 계산과 버튼 디바운싱/이벤트 판정을 수행하는 내부 엔진 구동 함수입니다.

**파라미터**: 없음

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. `core_process` 가 입력 수집 단계에서 1 ms(1 kHz) 주기로 자동 호출하므로, **일반적으로 사용자가 직접 호출할 필요는 없습니다.** 다만 이 함수가 주기적으로 실행되지 않으면(예: User Task가 무한 루프에 빠짐) Blink/Heartbeat/Oneshot 효과와 버튼 이벤트가 전혀 갱신되지 않습니다.

**예제**

```c
// 일반적으로 직접 호출할 필요 없음 — core_process가 자동 호출
// 참고용: 개념상의 호출 위치
void Control_Loop(void)
{
    // 사용자 알고리즘 ...
    XM_IO_Update();  // 필요 시 루프 마지막에 호출 가능
}
```

**참고**: [03. LED + 버튼 §1. 동작 원리](../03-led-btn-control.md#1-동작-원리-operating-principle)

---

## 타입/매크로

### `XmLedId_t`

| 값 | 설명 |
|----|------|
| `XM_LED_1` = 1 | 왼쪽 LED |
| `XM_LED_2` = 2 | 중간 LED |
| `XM_LED_3` = 3 | 오른쪽 LED |

### `XmBtnId_t`

| 값 | 설명 |
|----|------|
| `XM_BTN_1` = 1 | 왼쪽 버튼 |
| `XM_BTN_2` = 2 | 중간 버튼 |
| `XM_BTN_3` = 3 | 오른쪽 버튼 |

### `XmState_t`

| 값 | 설명 |
|----|------|
| `XM_OFF` = 0 | 끄기 (Logic Low) |
| `XM_ON` = 1 | 켜기 (Logic High) |

### `XmBtnState_t`

| 값 | 설명 |
|----|------|
| `XM_RELEASED` = 0 | 버튼이 떨어져 있음 |
| `XM_PRESSED` = 1 | 버튼이 눌려 있음 |

### `XmLedMode_t`

| 값 | 설명 |
|----|------|
| `XM_LED_OFF` = 0 | LED를 끕니다 |
| `XM_LED_SOLID` = 1 | LED를 계속 켭니다 |
| `XM_LED_BLINK` = 2 | 일정 주기로 깜빡입니다 (50% Duty) |
| `XM_LED_HEARTBEAT` = 3 | 두 번 빠르게 깜빡이는 심장박동 패턴 |
| `XM_LED_ONESHOT` = 4 | 설정 시간만큼 한 번 켜졌다가 자동으로 꺼짐 (알림용) |

### `XmChannelLed_t` 🟢 Rev 2.0 전용

> Rev1.1 헤더에는 정의되어 있지 않습니다.

| 값 | 설명 |
|----|------|
| `XM_CH_LED_EMG` = 0 | EMG 모듈 LED |
| `XM_CH_LED_FES` = 1 | FES 모듈 LED |
| `XM_CH_LED_IMU` = 2 | IMU 모듈 LED |
| `XM_CH_LED_HMMG` = 3 | HMMG 모듈 LED |
| `XM_CH_LED_GRF_L` = 4 | GRF 좌측 LED |
| `XM_CH_LED_GRF_R` = 5 | GRF 우측 LED |
| `XM_CH_LED_USB` = 6 | USB LED |
| `XM_CH_LED_COUNT` = 7 | (내부용) 채널 개수 |

### `XmBtnEvent_t`

| 값 | 설명 |
|----|------|
| `XM_BTN_NONE` = 0 | 발생한 이벤트 없음 |
| `XM_BTN_PRESSED` = 1 | 버튼을 막 누른 순간 (Rising Edge) |
| `XM_BTN_RELEASED` = 2 | 버튼을 막 뗀 순간 (Falling Edge) |
| `XM_BTN_CLICK` = 3 | 짧게 눌렀다 뗌 (클릭) |
| `XM_BTN_LONG_PRESS` = 4 | 1초 이상 길게 누름 (롱 프레스) |

---

## 내부 전용 (호출 금지)

이 헤더에 정의된 함수/타입 중 `[Internal]` 또는 System-only 로 표시된 항목은 없습니다 — 위에서 다룬 6개 함수와 7개 타입 모두 공개(Public) API 입니다.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| LED 3개 + 버튼 3개 기본 제어 | ✅ | ✅ |
| `XmChannelLed_t` / `XM_SetChannelLedRGB` (채널 RGB LED) | ❌ 없음 | 🟢 전용 |

---

## 관련 문서

- [03. LED + 버튼 (개념)](../03-led-btn-control.md) — 동작 원리, 흔한 실수, 예제 매핑
- [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교) — Rev1.1/Rev2.0 버튼 핀 시프트 주의
