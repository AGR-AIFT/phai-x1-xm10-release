# Ex.02 — Button & LED Event (이벤트와 원샷 효과)

> 🎯 **학습 목표**:
> - **이벤트 기반 (Event-Driven)** 처리로 클릭을 한 번만 감지합니다.
> - `XM_GetButtonEvent()` 의 read-clear 동작 + `XM_SetLedEffect()` ONESHOT 모드를 학습합니다.
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.01 Button & LED Basic](../01_Button_LED_Basic/) | 📚 관련 docs: [LED/BTN](../../docs/api-reference/03-led-btn-control.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

| 동작 | 결과 |
|------|------|
| BTN 1 클릭 | LED 1 **토글** (ON ↔ OFF 유지) |
| BTN 2 클릭 | LED 2 **2초간만** 켜진 후 자동으로 꺼짐 (원샷) |

> 📸 `![BTN1 토글 + BTN2 원샷](../assets/img/02_button_event.gif)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **이벤트 vs 폴링** — Ex.01 의 폴링은 "현재 누름?" 만 알 수 있지만, 이벤트는 "방금 누르고 뗐다" 같은 **변화** 자체를 잡습니다.
- **`XM_GetButtonEvent()` 는 read-clear** — 한 번 호출하면 이벤트가 소비됩니다. 다시 호출하면 새 이벤트만 보입니다.
- **`XmBtnEvent_t`** 종류: `XM_BTN_NONE`, `XM_BTN_CLICK`, `XM_BTN_DOUBLE_CLICK`, `XM_BTN_LONG_PRESS`. ([api-ref](../../docs/api-reference/03-led-btn-control.md))
- **`XM_LED_ONESHOT`** — 지정한 ms 동안 ON 후 자동 OFF. 학생이 별도 타이머 코드 안 짜도 됨.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
static bool s_led1_state = false;                         // ① 토글 상태 보존 (static)

static void Run_Loop(void)
{
    /* Case 1: 토글 스위치 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {     // ② 클릭 이벤트 1회
        s_led1_state = !s_led1_state;                       // ③ 상태 반전
        XM_SetLedState(XM_LED_1, s_led1_state);             // ④ LED 반영
    }

    /* Case 2: 원샷 알림등 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_SetLedEffect(XM_LED_2, XM_LED_ONESHOT, 2000);    // ⑤ 2000 ms 후 자동 OFF
    }
}
```

전체 코드: [`button_led_event.c`](button_led_event.c)

> 🧒 핵심은 ② — `GetButtonEvent` 는 "한 번 짚으면 사라진다". 그래서 매 호출이 중복 처리 안 됨.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시** → ✅ `0 errors` + LED 1/2 꺼진 상태로 부팅
2. **BTN 1 짧게 클릭** → ✅ LED 1 켜짐 (켜진 채 유지)
3. **BTN 1 다시 클릭** → ✅ LED 1 꺼짐
4. **BTN 2 짧게 클릭** → ✅ LED 2 가 2초 켜졌다가 자동으로 꺼짐
5. **변형 1 — 원샷 시간**: `XM_SetLedEffect(..., XM_LED_ONESHOT, 2000)` 의 `2000` 을 `500` 또는 `5000` 으로 변경.
6. **변형 2 — 이벤트 종류**: `XM_BTN_CLICK` → `XM_BTN_DOUBLE_CLICK` 으로 변경 → 더블 클릭해야 토글되도록.
7. **변형 3 — 폴링과 결합**: Run_Loop 끝에 Ex.01 코드 (`XM_GetButtonState(XM_BTN_3) == XM_PRESSED` → `XM_LED_3` ON) 도 추가 → 이벤트와 폴링 동시 운용.

---

## 5️⃣ 다음 단계

- 롱 프레스 + 상태 머신: [Ex.03 Button & LED FSM](../03_Button_LED_FSM/)
- 외부 안전 스위치 (이벤트 + FSM 결합 응용): [Ex.06 Ext IO Safety Switch](../06_Ext_IO_Safety_Switch/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 클릭했는데 토글이 안 됨 | `s_led1_state` 가 `static` 이 아니라 매 호출 0 으로 초기화 | `static bool` 로 선언 (스택 변수 X) |
| 클릭 한 번에 두 번 토글 | 다른 곳에서 `GetButtonEvent` 도 호출해서 이벤트 두 번 소비 | 한 이벤트 채널은 한 곳에서만 read |
| 원샷 LED 가 꺼지지 않고 계속 켜져있음 | 다른 코드가 `SetLedState(..., XM_ON)` 으로 덮어쓰는 중 | LED 효과와 상태 호출이 충돌. 한쪽만 사용 |
| BTN 1 길게 누르면 토글 안 됨 | `XM_BTN_CLICK` 은 짧은 누름만 트리거. 긴 누름은 `XM_BTN_LONG_PRESS` | 의도에 맞게 이벤트 타입 선택 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
