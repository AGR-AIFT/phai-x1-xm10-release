# Ex.01 — Button & LED Basic (폴링 입출력)

> 🎯 **학습 목표**:
> - **폴링 (Polling)** 방식으로 버튼의 현재 상태를 읽습니다.
> - `XM_GetButtonState()` + `XM_SetLedState()` API 로 디지털 입출력의 직접 매핑을 구현합니다.
>
> ⏱️ 권장 시간: 20분 | 🔧 난이도: ⭐
> 🧰 사전 예제: [Ex.00 Quick Start](../00_Quick_Start/) | 📚 관련 docs: [LED/BTN](../../docs/api-reference/03-led-btn-control.md) · [TSM](../../docs/api-reference/01-task-state-machine.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

**버튼 1을 누르고 있는 동안만 LED 1 이 켜집니다** (모멘터리 스위치).

- 누른 순간: LED 1 ON
- 떼는 순간: LED 1 OFF
- 즉각 반응 (지연 거의 0)

> 📸 `![BTN1 누르면 LED1 점등](../assets/img/01_btn_led_basic.gif)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **Polling vs Event** — 폴링: "지금 상태 어떄?" 매 주기 직접 묻기 / 이벤트: "변화 생기면 알려줘". 본 예제는 폴링.
- **`XmBtnState_t`** — `XM_PRESSED` 또는 `XM_RELEASED`. ([api-ref](../../docs/api-reference/03-led-btn-control.md))
- **TSM 단일 상태** — `XM_STATE_USER_START` 하나만 등록. 상태 전환 없음.

> 💡 폴링은 가장 단순하지만, **클릭** 이나 **롱 프레스** 같은 패턴 감지에는 부적합. 그건 [Ex.02 이벤트](../02_Button_LED_Event/) 에서 다룹니다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void User_Setup(void)                                     // ① 부팅 시 1회
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = {
        .id      = XM_STATE_USER_START,
        .on_loop = Run_Loop                                // ② 매 2 ms 호출
    };
    XM_TSM_AddState(s_tsm, &conf);
}

static void Run_Loop(void)
{
    XmBtnState_t state = XM_GetButtonState(XM_BTN_1);     // ③ 현재 물리 상태 폴링

    if (state == XM_PRESSED) {                             // ④ 누름?
        XM_SetLedState(XM_LED_1, XM_ON);                    // ⑤ → LED ON
    } else {
        XM_SetLedState(XM_LED_1, XM_OFF);                   // ⑥ 그 외 → LED OFF
    }
}
```

전체 코드: [`button_led_basic.c`](button_led_basic.c)

> 🧒 한 줄 요약: 매 2 ms마다 ③ "버튼 눌렀나?" 물어보고, 결과를 ⑤/⑥ LED 에 반영. 이것이 폴링.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드** → ✅ `0 errors`
2. **플래시** → ✅ 보드 부팅 후 LED 1 꺼진 상태
3. **BTN 1 누르기** → ✅ 누르고 있는 동안 LED 1 켜짐
4. **BTN 1 떼기** → ✅ 즉시 LED 1 꺼짐
5. **변형 1 — 반대 동작**: `if (state == XM_PRESSED)` 의 LED `XM_ON` ↔ `XM_OFF` 를 서로 바꿔보세요. 떼면 켜지고 누르면 꺼지는 NC (Normally-Closed) 스위치가 됩니다.
6. **변형 2 — LED 변경**: `XM_LED_1` → `XM_LED_2` 또는 `XM_LED_3` 으로 바꿔 다른 LED 가 반응하도록.
7. **변형 3 — 다중 버튼**: 새 if 블록을 추가해 `XM_BTN_2` 도 폴링 → `XM_LED_2` 제어.

---

## 5️⃣ 다음 단계

- 이벤트 기반 (클릭/롱프레스 감지): [Ex.02 Button & LED Event](../02_Button_LED_Event/)
- 상태 머신으로 모드 전환: [Ex.03 Button & LED FSM](../03_Button_LED_FSM/)
- 외부 GPIO 로 확장: [Ex.04 Ext IO Basic](../04_Ext_IO_Basic/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| LED 가 항상 꺼져있음 | 펌웨어 빌드 후 플래시 안 함 | Console 에 `Download verified successfully` 확인 |
| LED 가 항상 켜져있음 | `XM_RELEASED` 대신 `XM_PRESSED` 양쪽 모두 ON 처리 | `if/else` 분기 정확히 확인 |
| 버튼 짧게 눌렀다 떼면 LED 가 깜빡 안 보임 | 너무 짧은 누름 + 폴링 주기 (2 ms) | 정상 동작. 길게 눌러서 확인하거나 [Ex.02 이벤트](../02_Button_LED_Event/) 로 진행 |
| LED 가 미세하게 흔들림 (반딧불처럼) | LED 효과 (`XM_LED_BLINK` 등) 가 남아있음 | `XM_SetLedState` 호출 전에 `XM_SetLedEffect(LED, XM_LED_NONE, 0)` 호출 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
