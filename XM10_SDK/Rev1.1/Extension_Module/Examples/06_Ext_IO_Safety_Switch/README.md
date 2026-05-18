# Ex.06 — Safety Switch (3-state FSM + 비상 정지 인터록)

> 🎯 **학습 목표**:
> - 외부 리미트 스위치를 활용한 **안전 인터록** 패턴.
> - 3-state FSM (STANDBY / ACTIVE / ERROR) + 토크 제어 시작/정지.
> - 비상 정지 (E-Stop) 후 사용자 확인 없이는 ACTIVE 복귀 금지.
>
> ⏱️ 권장 시간: 35분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.03 FSM](../03_Button_LED_FSM/) + [Ex.04 Ext IO Basic](../04_Ext_IO_Basic/) | 📚 관련 docs: [External IO](../../docs/api-reference/04-external-io.md) · [TSM](../../docs/api-reference/01-task-state-machine.md) · [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

3개 상태 + 외부 안전 스위치 인터록:

| 상태 | 진입 조건 | 동작 |
|------|----------|------|
| **STANDBY** | 시작 (대기) | 외부 시작 버튼 (DIO_3) 누름 감지 |
| **ACTIVE** | STANDBY 에서 시작 버튼 | LED 2 ON + 토크 제어 시작. 외부 리미트 스위치 (DIO_4) 누르면 ERROR 진입 |
| **ERROR** | 리미트 트리거 (비상 정지) | LED 1 빠른 깜빡 + 토크 0. **내부 BTN 1 클릭으로만** STANDBY 복귀 |

> 📸 `![Safety FSM 다이어그램](../assets/img/06_safety_fsm.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **인터록 (Interlock)** — 한 조건이 충족되어야 다음 동작이 가능한 안전 메커니즘. 본 예제는 리미트 스위치 = "여기 이상 움직이면 안 됨" 의 물리 한계.
- **비상 정지 (E-Stop)** — 즉시 토크 0 + 사용자 확인 후에만 복귀. 자동 복귀 절대 금지 (외골격 안전 룰).
- **`XM_SetControlMode()`** — `XM_CTRL_MONITOR` (제어 X, 모니터링만) vs `XM_CTRL_TORQUE` (사용자 토크 제어). ([api-ref](../../docs/api-reference/02-h10-control-n-data.md))
- **Active High 리미트 스위치** — 본 예제는 리미트 = HIGH 일 때 트리거. Pull-up 회로 + N.O. (Normally Open) 스위치 조합 시.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void User_Setup(void)
{
    /* ① 핀 설정 */
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP);    // 시작 버튼
    XM_SetPinMode(XM_EXT_DIO_4, XM_EXT_DIO_MODE_INPUT);            // 리미트 스위치

    /* ② TSM 3개 상태 등록 */
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);
    XmStateConfig_t states[] = {
        { .id = XM_STATE_STANDBY, .on_loop = Standby_Loop },
        { .id = XM_STATE_ACTIVE,  .on_entry = Active_Entry, .on_loop = Active_Loop },
        { .id = XM_STATE_ERROR,   .on_entry = Error_Entry,  .on_loop = Error_Loop }
    };
    for (int i = 0; i < 3; i++) XM_TSM_AddState(s_tsm, &states[i]);
}

static void Standby_Loop(void)
{
    if (XM_DigitalRead(XM_EXT_DIO_3) == XM_LOW) {                  // ③ 시작 버튼 (Active Low)
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetLedState(XM_LED_2, XM_ON);                                // ④ 동작 표시
    XM_SetControlMode(XM_CTRL_TORQUE);                              // ⑤ 토크 제어 시작
}

static void Active_Loop(void)
{
    /* ⑥ 안전 인터록 — 리미트 트리거 시 즉시 ERROR */
    if (XM_DigitalRead(XM_EXT_DIO_4) == XM_HIGH) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ERROR);
        return;
    }

    /* ⑦ 정상 토크 제어 (램프 업) */
    static float torque = 0;
    torque += 0.01f;
    if (torque > 2.0f) torque = 0;
    XM_SetAssistTorque(torque, torque);
}

static void Error_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 100);                  // ⑧ 빠른 깜빡 (경고)
    XM_SetControlMode(XM_CTRL_MONITOR);                             // ⑨ 토크 0 — 비상 정지
}

static void Error_Loop(void)
{
    /* ⑩ 내부 BTN 1 클릭으로만 복귀 — 사용자 확인 강제 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}
```

전체 코드: [`ext_io_safety_control.c`](ext_io_safety_control.c)

> 🧒 핵심 안전 룰: ⑥ 리미트 트리거 → **`return;` 으로 토크 제어 차단** 후 즉시 ERROR. 단 1 cycle 도 토크 출력 X.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 연결**: 외부 시작 버튼 (Active Low, DIO_3), 리미트 스위치 (Active High, DIO_4), KIT H10 연결.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **부팅** → ✅ STANDBY (LED 변화 없음)
4. **외부 시작 버튼 누름** → ✅ ACTIVE 진입, LED 2 ON, 토크 점진 증가
5. **리미트 스위치 누름** → ✅ ERROR 진입, LED 1 빠른 깜빡, 토크 즉시 0
6. **내부 BTN 1 클릭** → ✅ STANDBY 복귀
7. **변형 1 — 자동 복귀 금지 검증**: ERROR Loop 끝에 `XM_TSM_TransitionTo(STANDBY)` 추가 (자동 복귀 시도) → 위험한 패턴이므로 즉시 되돌리기. **자동 복귀는 외골격 안전 룰 위반.**
8. **변형 2 — 다중 인터록**: DIO_5 에도 리미트 스위치 추가 → 둘 중 하나라도 트리거 시 ERROR.
9. **변형 3 — 디버그 로그**: 각 상태 Entry 에 `XM_SendUsbDebugMessage` 추가 → 상태 전환 이력 USB 출력.

---

## 5️⃣ 다음 단계

- 본격 토크 제어 알고리즘: [Ex.14 PD Realtime Control](../14_PD_Realtime_Control/)
- 외골격 모드 진입: [Ex.11 Passive](../11_Passive_Mode/) · [Ex.12 Active Assist](../12_Active_Assist_Mode/)
- 7-phase 보행 FSM (Ex.06 패턴 확장): [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| ACTIVE 진입 시 토크가 들쭉날쭉 | `static float torque` 미선언 → 매 호출 0 초기화 | `static` 필수 |
| 리미트 트리거에도 ERROR 진입 안 함 | Active Low/High 혼동 | DIO_4 의 풀업/풀다운 + 스위치 동작 다시 확인 |
| ERROR 에서 자동 복귀 | 학생이 `Error_Loop` 에 자동 transition 추가 | **금지**. E-Stop 은 항상 사용자 확인 후 |
| H10 무반응 | `XM_SetControlMode(XM_CTRL_TORQUE)` 누락 또는 H10 미연결 | Entry 호출 확인 + CAN-FD 연결 |
| 모든 상태에서 LED 1 만 깜빡 | Active_Entry 가 LED 1 도 건드림 (코드 오타) | LED 2 = ACTIVE, LED 1 = ERROR 표시 |
| H10 토크가 너무 빠르게 증가 | `torque += 0.01f` 가 2 ms 주기 → 1초에 5 Nm | 학생 실험 안전을 위해 H10 분리 또는 ramp 속도 ↓ |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
