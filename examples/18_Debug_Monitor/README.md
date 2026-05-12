# Ex.18 — Debug Monitor (실시간 시스템 진단 + Health Dashboard)

> 🎯 **학습 목표**:
> - **루프 실행 시간 측정** (Min/Max/Avg + 오버런 감지) 으로 실시간성 검증.
> - **USB CDC Health Dashboard** (1초 주기) + 진단 LED 패턴 + 데이터 신선도(Staleness) Watchdog.
> - 양산 환경에서 쓸 수 있는 **비침습적 모니터링 패턴** — `printf` 디버깅의 한계 극복.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.07 CDC Basic](../07_CDC_Basic_Print/) + [Ex.03 FSM](../03_Button_LED_FSM/) | 📚 관련 docs: [LED & Button](../../docs/api-reference/03-led-btn-control.md) · [USB](../../docs/api-reference/05-usb-connectivity.md)

---

> ⚠️ **USB-CDC 단일 점유** — 본 예제 실행 중 PhAI Studio 를 동시에 열어두지 마세요 (같은 COM 포트 충돌 → 접속 실패). 실시간 그래프 필요 시 PhAI Studio 만 단독 실행하세요.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 가 연결되지 않아도 **시스템 자체 진단** 이 항상 동작합니다 (단일 상태 TSM).

| 출력 | 의미 |
|------|------|
| **LED 1** Heartbeat (1초) | 정상 동작 |
| **LED 1** 빠른 Blink (200ms) | 오버런 OR STALE 데이터 감지 |
| **LED 2** ON / OFF | CM 연결 / 미연결 |
| **LED 3** Oneshot 깜빡 | 아무 버튼이나 눌림 (입력 피드백) |
| **USB CDC** `[HEALTH]` 4줄 | 매 1초 — Uptime + Loops + Loop Time + CM + ADC |
| **USB CDC** `[WATCH] WARNING` | 좌측 고관절 5초간 변화 없음 (STALE) |

버튼 조작: **BTN 1** 통계 리셋 / **BTN 2** Verbose 모드 토글 / **BTN 3 롱프레스** 종합 덤프.

> 📸 `![Health Dashboard 콘솔 출력](../assets/img/18_health_dashboard.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 왜 printf 디버깅이 위험한가

- 1 kHz 루프에서 매 cycle 호출하면 USB 송신이 1ms 안에 못 끝나 **오버런** 유발.
- 디버그 출력 자체가 측정 대상의 타이밍을 바꾸는 **Heisenbug**.
- 해결: **데이터를 모아서 1초 주기로 배치 출력**.

### XM_GetTick() 의 해상도

- ms 단위. 1 ms 루프 내 측정값은 대부분 0, 가끔 1.
- 더 정밀한 측정 (us 단위) 이 필요하면 `DWT->CYCCNT` (CPU 사이클 카운터) — 본 예제 범위 밖.
- 그래도 **오버런 (>1 ms) 감지** 는 ms 해상도로도 충분히 유용.

### Staleness Watchdog 패턴

```
prev = leftHipAngle
매 cycle:
  if |current − prev| > ε  →  타이머 리셋, prev 갱신
  else if (now − 마지막 변화 시각) > 5000 ms  →  STALE 경고
```
부동소수 비교는 epsilon (`0.001f`) 사용 — 노이즈로 인한 false-positive 방지.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define LOOP_PERIOD_MS         1U
#define HEALTH_REPORT_INTERVAL 1000U
#define STALE_DATA_TIMEOUT_MS  5000U
#define ANGLE_CHANGE_EPSILON   0.001f

typedef struct {                                                   // ① 통계 구조체
    uint32_t min_ms, max_ms, sum_ms, sample_count;
    float    avg_ms;
} LoopStats_t;

static LoopStats_t s_stats;
static uint32_t    s_loop_count = 0, s_overrun_count = 0;

void User_Loop(void)
{
    uint32_t start_tick = XM_GetTick();                            // ② 시작 시각
    XM_TSM_Run(s_tsm);
    uint32_t elapsed = XM_GetTick() - start_tick;                  // ③ 경과 측정
    _MeasureLoopTime(elapsed);
}

static void _MeasureLoopTime(uint32_t elapsed_ms)
{
    s_loop_count++;
    if (elapsed_ms < s_stats.min_ms) s_stats.min_ms = elapsed_ms;
    if (elapsed_ms > s_stats.max_ms) s_stats.max_ms = elapsed_ms;
    s_stats.sum_ms += elapsed_ms;
    s_stats.sample_count++;
    if (elapsed_ms > LOOP_PERIOD_MS) s_overrun_count++;             // ④ 오버런 감지
}

static void _CheckDataStaleness(uint32_t now)                      // ⑤ Staleness Watchdog
{
    float delta = XM.status.h10.leftHipAngle - s_prev_left_hip_angle;
    if (delta < 0.0f) delta = -delta;

    if (delta > ANGLE_CHANGE_EPSILON) {
        s_last_data_change_time = now;                             //    → 데이터 변화 → 타이머 리셋
        s_prev_left_hip_angle = XM.status.h10.leftHipAngle;
        if (s_is_data_stale) { s_is_data_stale = false; /* 복귀 메시지 */ }
    } else if (!s_is_data_stale &&
               (now - s_last_data_change_time) >= STALE_DATA_TIMEOUT_MS) {
        s_is_data_stale = true;                                    //    → 5초 무변화 → STALE
        XM_SendUsbDebugMessage("[WATCH] WARNING: STALE DATA — ...\r\n");
    }
}

static void _UpdateDiagnosticLeds(void)                            // ⑥ LED = 가장 신뢰성 높은 디버그
{
    if (s_overrun_count > 0 || s_is_data_stale)
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);              //    이상 → 빠른 Blink
    else
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);         //    정상 → Heartbeat

    XM_SetLedState(XM_LED_2, XM_IsCmConnected() ? XM_ON : XM_OFF);
}
```

전체 코드: [`debug_monitor.c`](debug_monitor.c)

> 🧒 ④ 의 **오버런 카운터** = 핵심 KPI. 1 시간 운전 후 0 이면 실시간성 확보, 0 초과면 어딘가 무거운 코드.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시** → ✅ LED 1 Heartbeat (1초) 시작
2. **USB CDC 터미널** → ✅ 1초마다 `[HEALTH] Uptime: ... | Loops: ... | Overrun: 0` 4줄 출력
3. **CM 미연결 상태** → ✅ LED 2 OFF + `[HEALTH] CM: Disconnected`
4. **CM 연결** → ✅ LED 2 ON + `CM: Connected` + `Data: Fresh`
5. **5초 동안 다리 움직임 없이 대기** → ✅ `[WATCH] WARNING: STALE DATA` + LED 1 빠른 Blink
6. **다리 움직임 재개** → ✅ `[WATCH] Data refresh detected — STALE cleared.` + LED 1 Heartbeat 복귀
7. **BTN 1 클릭** → ✅ `[BTN1] Stats reset complete.` + Loops/Overrun 카운터 0 으로 리셋
8. **BTN 2 클릭** → ✅ Verbose 모드 ON. 추가로 `[DETAIL]` 줄 (각도, 토크, ADC 4ch) 출력
9. **BTN 3 1초 이상 꾸욱** → ✅ `========== SYSTEM DUMP ==========` 종합 출력
10. **변형 1 — 오버런 임계치 강제 trigger**: `User_Loop` 안에서 `for (volatile int i=0; i<100000; i++);` 추가 → ✅ Overrun 카운터 증가, LED 1 빠른 Blink
11. **변형 2 — Stale 타임아웃 단축**: `STALE_DATA_TIMEOUT_MS` 5000 → 1000 (1초). 빠른 trigger 가능
12. **변형 3 — Health 출력 주기**: `HEALTH_REPORT_INTERVAL` 1000 → 100 (10 Hz). USB 부하 관찰
13. **변형 4 — 추가 모니터링 항목**: `_PrintHealthDashboard` 에 IMU 각속도 / 토크 누적 등 추가

---

## 5️⃣ 다음 단계

- 정적 메모리 패턴 (malloc 없는 설계): [Ex.19 Memory Aware Design](../19_Memory_Aware_Design/)
- 양산 안전 (Safety Switch): [Ex.06 Safety Switch](../06_Ext_IO_Safety_Switch/)
- 외란 관측기 (DOB): [Ex.31 Friction Comp DOB](../31_Friction_Comp_DOB/) (Phase 2D)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `[HEALTH]` 가 안 보임 | USB CDC 포트 오픈 안 됨 OR PhAI Studio 동시 점유 | 터미널 (Tera Term 등) 만 단독 실행 |
| Loop Time 이 항상 0 ms | 의도된 동작 (ms 해상도 한계) | us 정밀도 필요 시 DWT 사이클 카운터 활용 |
| Overrun 카운터가 계속 증가 | User 코드가 무거움 OR USB 송신이 너무 자주 | 출력 주기 ↑, 무거운 연산 분할 |
| STALE 경고 안 뜸 | CM 연결되어 있고 데이터 변화 있음 | 정상. CM 끄고 5초 대기로 강제 trigger |
| BTN 3 클릭하니 LONG_PRESS 안 됨 | 1초 미만 클릭 = `XM_BTN_CLICK` 이벤트 | 1초 이상 꾸욱 눌러야 `XM_BTN_LONG_PRESS` |
| BTN 1 누른 직후 Min/Max 가 0 | `_ResetStats` 가 min 을 `UINT32_MAX` 로 초기화 | 정상. 다음 측정 사이클부터 갱신 |
| Verbose 모드 OFF 해도 줄어들지 않음 | `s_verbose_mode` 토글 호출 안 됨 (오타) | `XM_BTN_CLICK` 이벤트로 명확히 체크 |
| LED 2 항상 OFF | CM 케이블 미연결 or `XM_IsCmConnected` 폴링 누락 | `_UpdateDiagnosticLeds` 가 매 cycle 호출되는지 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
