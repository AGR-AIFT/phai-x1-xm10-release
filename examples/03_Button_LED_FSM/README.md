# Ex.03 — Button & LED FSM (롱 프레스 + 상태 머신)

> 🎯 **학습 목표**:
> - **TSM 다중 상태** 생성 + `XM_TSM_TransitionTo()` 로 모드 전환.
> - `on_entry` / `on_loop` 콜백의 역할과 호출 시점을 구분합니다.
> - 롱 프레스 (`XM_BTN_LONG_PRESS`) 로 오작동 방지 패턴을 익힙니다.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.02 Event](../02_Button_LED_Event/) | 📚 관련 docs: [TSM](../../docs/api-reference/01-task-state-machine.md) · [LED/BTN](../../docs/api-reference/03-led-btn-control.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

시스템이 **STANDBY ↔ ACTIVE** 두 모드를 가지고, **BTN 1 을 1초 이상 꾹 누르면** 모드가 전환됩니다.

| 상태 | LED 1 | 의미 |
|------|------|------|
| STANDBY | Heartbeat (1초 주기, 두근-두근) | 대기 중 |
| ACTIVE | Blink (200 ms, 빠른 깜빡) | 동작 중 |

전환 시 USB CDC 로 `"[Mode] STANDBY"` / `"[Mode] ACTIVE !!"` 로그 송출.

> 📸 `![FSM 모드 전환](../assets/img/03_fsm_transition.gif)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **FSM (Finite State Machine)** — 시스템을 유한한 "상태" 의 집합으로 모델. 각 상태에서 동작은 다르고, 이벤트로만 전환.
- **TSM 콜백 3종**:
   - `on_entry` — 상태 진입 시 **1회**만 (예: LED 패턴 설정)
   - `on_loop` — 매 2 ms 반복 (예: 이벤트 감시 + 메인 로직)
   - `on_exit` — 상태 나갈 때 1회 (본 예제에선 미사용)
- **롱 프레스** — 의도하지 않은 짧은 클릭으로 모드가 바뀌지 않게 하는 안전 장치. `XM_BTN_LONG_PRESS` 이벤트로 트리거.

> 💡 외골격 같은 안전 critical 시스템에서 모드 전환은 항상 롱 프레스 또는 다중 확인. 짧은 클릭 = 위험.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void User_Setup(void)                                     // ① 부팅 시 1회
{
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);               // ② 초기 상태 = STANDBY

    XmStateConfig_t sb = {                                 // ③ STANDBY 등록
        .id       = XM_STATE_STANDBY,
        .on_entry = Standby_Entry,                          //    진입 시 1회
        .on_loop  = Standby_Loop                            //    매 2 ms
    };
    XM_TSM_AddState(s_tsm, &sb);

    XmStateConfig_t act = {                                // ④ ACTIVE 등록
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop
    };
    XM_TSM_AddState(s_tsm, &act);
}

static void Standby_Entry(void)                           // ⑤ STANDBY 진입
{
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);     //    Heartbeat 1초 주기
}

static void Standby_Loop(void)                            // ⑥ STANDBY 반복
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_LONG_PRESS) { // ⑦ 1초 이상 꾹 누름
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);        // ⑧ → ACTIVE 로 전환
    }
}

static void Active_Entry(void)                            // ⑨ ACTIVE 진입
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);          //    빠른 깜빡
    XM_SendUsbDebugMessage("[Mode] ACTIVE !!\r\n");
}

static void Active_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_LONG_PRESS) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);       // ACTIVE → STANDBY
    }
    /* 여기에 실제 제어 로직 (모터 명령 등) */
}
```

전체 코드: [`button_led_fsm.c`](button_led_fsm.c)

> 🧒 핵심은 ⑤/⑨ vs ⑥/⑩ 의 차이 — Entry 는 "패턴 바꾸기" 같은 1회 설정, Loop 는 "이벤트 감시" 같은 반복.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시** → ✅ 부팅 직후 LED 1 Heartbeat (천천히 두근거림)
2. **BTN 1 짧게 클릭** → ✅ 아무 일도 안 일어남 (롱프레스 아님)
3. **BTN 1 을 1초 이상 꾸욱 누름** → ✅ LED 1 이 빠른 깜빡 (Blink) 로 전환 + USB 로 `[Mode] ACTIVE !!`
4. **BTN 1 다시 1초 이상 누름** → ✅ Heartbeat 로 복귀 + `[Mode] STANDBY`
5. **변형 1 — ERROR 상태 추가**: 새 상태 `XM_STATE_ERROR` 추가 + `ACTIVE` 에서 BTN 2 클릭 시 ERROR 진입 (LED 3 켜고 멈춤).
6. **변형 2 — 자동 복귀**: `Active_Entry` 에서 `XM_GetTick()` 저장 → `Active_Loop` 에서 5초 경과 시 자동으로 STANDBY 복귀.
7. **변형 3 — Entry 호출 횟수 검증**: `Standby_Entry` 안에 `XM_SendUsbDebugMessage("ENTRY!\r\n");` 추가 → 모드 전환할 때만 1회 출력되는지 확인.

---

## 5️⃣ 다음 단계

- 안전 스위치 + FSM 결합: [Ex.06 Ext IO Safety Switch](../06_Ext_IO_Safety_Switch/)
- 실제 로봇 모드 전환: [Ex.11 Passive Mode](../11_Passive_Mode/) / [Ex.12 Active Assist](../12_Active_Assist_Mode/)
- 7-phase 보행 FSM (응용): [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 롱프레스해도 모드 전환 안 됨 | `XM_TSM_TransitionTo` 누락 또는 잘못된 상태 ID 사용 | `XM_STATE_ACTIVE` / `XM_STATE_STANDBY` 정확히 명시 |
| 모드 전환은 됐는데 LED 패턴 안 바뀜 | `on_entry` 미등록 또는 `SetLedEffect` 인자 오타 | `XmStateConfig_t` 의 `.on_entry` 필드 확인 |
| Entry 가 매번 호출되어 LED 가 깜빡깜빡 | Loop 안에서 직접 `Entry()` 함수 호출함 | Entry 는 `TransitionTo` 가 자동 호출. 직접 호출 금지 |
| STANDBY 에서 ACTIVE 로 못 전환 | `Standby_Loop` 의 `if` 조건이 `XM_BTN_CLICK` 으로 되어있음 | `XM_BTN_LONG_PRESS` 로 수정 |
| 짧은 클릭에도 모드가 바뀜 | 외골격 안전 룰 위반 | 모드 전환은 반드시 LONG_PRESS 또는 2단계 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
