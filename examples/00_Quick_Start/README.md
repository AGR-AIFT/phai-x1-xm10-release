# Ex.00 — Quick Start (보드 동작 확인)

> 🎯 **학습 목표**:
> - `User_Setup()` / `User_Loop()` 진입점 구조를 이해합니다.
> - TSM 1개 상태 생성 → LED · 버튼 · USB CDC 기초 동작을 한 번에 체험합니다.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐ (사전지식 0)
> 🧰 사전 예제: 없음 (첫 예제) | 📚 관련 docs: [TSM](../../docs/api-reference/01-task-state-machine.md) · [LED/BTN](../../docs/api-reference/03-led-btn-control.md) · [USB CDC](../../docs/api-reference/05-usb-connectivity.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

XM10 보드 단독으로 다음을 확인합니다 (외부 HW 불필요):

| 동작 | 결과 |
|------|------|
| 부팅 | LED 1 → 2 → 3 순차 점등 → 0.5초 유지 → 소등 → LED 1 Heartbeat |
| BTN 1 클릭 | USB CDC 로 `"Hello XM10!"` 메시지 + LED 3 1회 깜빡임 |
| BTN 2 클릭 | LED 2 토글 (ON ↔ OFF) |

> 📸 `![부팅 LED 시퀀스](../assets/img/00_boot_sequence.gif)` — 보드 영상 placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **User_Setup / User_Loop** — 부팅 시 1회 호출 + 매 2 ms (500 Hz) 반복 호출. ([architecture](../../docs/architecture/))
- **TSM (Task State Machine)** — 상태 + 콜백 (`on_entry` / `on_loop`) 구조의 마이크로 FSM. 본 예제는 단일 상태만 사용. (참고: [Ex.03 FSM](../03_Button_LED_FSM/) 에서 멀티 상태로 확장)
- **USB CDC** — XM10 ↔ PC 시리얼 가상 포트. 텍스트 또는 바이너리 전송.

> ⚠️ USB-CDC 포트는 단일 점유 자원입니다. PhAI Studio 와 시리얼 터미널 (PuTTY · RealTerm 등) 을 동시에 열지 마세요 — COM 포트 충돌로 메시지가 보이지 않습니다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void User_Setup(void)                                     // ① 부팅 시 1회
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);            // ② TSM 핸들 생성
    XmStateConfig_t conf = {
        .id       = XM_STATE_USER_START,
        .on_entry = Run_Entry,                              // ③ 상태 진입 시 1회
        .on_loop  = Run_Loop                                // ④ 매 2 ms 호출
    };
    XM_TSM_AddState(s_tsm, &conf);
}

void User_Loop(void)                                      // ⑤ 매 2 ms System 호출
{
    XM_TSM_Run(s_tsm);                                     // ⑥ TSM 디스패치
}

static void Run_Loop(void)                                // ④의 본문
{
    /* 부팅 시퀀스 (BOOT_STEP_LED1 → DONE) */
    if (s_boot_step != BOOT_STEP_DONE) { ... return; }

    /* BTN 1: USB 메시지 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {     // ⑦ 클릭 이벤트
        XM_SendUsbDebugMessage("Hello XM10! ...\r\n");      // ⑧ CDC 전송
        XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, 200);     // ⑨ LED 1회 효과
    }
    /* BTN 2: LED 토글 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_is_led2_on = !s_is_led2_on;
        XM_SetLedState(XM_LED_2, s_is_led2_on ? XM_ON : XM_OFF);
    }
}
```

전체 코드: [`quick_start.c`](quick_start.c)

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드** → ✅ Console 에 `Build Finished. 0 errors`
2. **플래시** (Run → Debug) → ✅ 부팅 직후 LED 1 → 2 → 3 순차 점등 확인
3. **부팅 완료** → ✅ LED 1 만 Heartbeat (1초 주기) 로 깜빡임
4. **USB 연결** → 시리얼 터미널 (115200 8N1) 에서 `"[QuickStart] XM10 보드 준비 완료!"` 확인
5. **BTN 1 클릭** → ✅ 터미널에 `"Hello XM10!"` + LED 3 1회 깜빡
6. **BTN 2 클릭** → ✅ LED 2 켜짐, 다시 누르면 꺼짐
7. **변형 1** — `BOOT_LED_INTERVAL_MS` 를 `200` → `50` 으로 바꾸고 빌드/플래시 → 부팅 시퀀스가 얼마나 빨라지나?
8. **변형 2** — `Run_Loop` 끝에 `if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK)` 블록을 추가하고 LED 3 토글을 구현 → 직접 동작 확인

---

## 5️⃣ 다음 단계

- 더 단순한 폴링 방식: [Ex.01 Button & LED Basic](../01_Button_LED_Basic/) — `GetButtonState` 와 `GetButtonEvent` 차이 학습
- TSM 멀티 상태 확장: [Ex.03 Button & LED FSM](../03_Button_LED_FSM/)
- USB 텍스트 디버깅: [Ex.07 CDC Basic Print](../07_CDC_Basic_Print/)
- 전체 학습 로드맵: [docs/tutorials/](../../docs/tutorials/README.md)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 보드 부팅 시 LED 가 전혀 안 켜짐 | 펌웨어 미플래시 또는 전원 문제 | Debug 로그 확인 + 보드 전원 LED 점등 확인 |
| 시리얼 터미널에 메시지 안 보임 | 다른 프로그램이 COM 포트 점유 (PhAI Studio 등) | 다른 시리얼 클라이언트 모두 종료 후 재시도 |
| 부팅 시퀀스가 너무 빨라 못 봄 | `BOOT_LED_INTERVAL_MS` 가 짧음 | 값을 `500` ~ `1000` 으로 늘려 천천히 관찰 |
| BTN 1 한 번 눌렀는데 메시지가 여러 번 출력 | `GetButtonEvent` 대신 `GetButtonState` 사용했을 가능성 | `GetButtonEvent` (read-clear) 사용 확인 |
| LED 2 가 토글 안 되고 켜진 채 유지 | `s_is_led2_on` 변수가 `static` 이 아닐 가능성 | `static bool` 으로 선언되어 다음 호출까지 유지되어야 함 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md) 또는 GitHub Issues `bug_report.yml` 템플릿
