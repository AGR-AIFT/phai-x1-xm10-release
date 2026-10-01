# XM10 Task Topology — RTOS Task API 사용자 가이드

> XM10 SDK 의 사용자 보조 task 작성 가이드. 우선순위 + 데이터 흐름 +
> 공유변수 패턴 4가지.

**Audience**: SDK 사용자 (연구자 / 학습자 / 일반 로보틱스 개발자)
**Date**: 2026-05-15
**Related**:
- 사용자 API: [`XM_FW/XM_API/xm_api_freertos.h`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api_freertos.h)
- 예제: [`Examples/38_Periodic_Background_Task/`](../../Examples/38_Periodic_Background_Task/) · [`Examples/39_Task_Lifecycle/`](../../Examples/39_Task_Lifecycle/)

---

## 1. 시스템 Task

XM10 SDK 는 부팅 시 필요한 시스템 task 를 자동으로 만듭니다. **사용자가 직접 변경하면
안 됩니다** (시스템 안정성 손상). `Control_Setup()` 과 `Control_Loop()` 는 그중
제어 루프 task (Control_Loop) 에서 1 ms (1 kHz) 주기로 호출됩니다 (우선순위 53, 스택 32 KB).
사용자 보조 task 는 `XM_Task_Create*()` 와 아래 `XM_PRIO_*` 힌트로만 만드세요.

---

## 2. 사용자 Task 우선순위 영역

사용자가 `XM_Task_CreateOneShot()` / `XM_Task_CreatePeriodic()` 로 보조 task 를
만들 때 `prio_hint` 로 선택할 수 있는 영역입니다.

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ 시스템 task — 사용자가 만들거나 바꿀 수 없음                                  │
│   55  Realtime7  시스템 시작 / CAN-FD 수신  ← 로봇·센서 데이터 수신 즉시 처리 │
│   54  Realtime6  UART 수신                  ← 센서 패킷 파싱                  │
│   53  Realtime5  Control_Loop               ← 1 kHz 제어 루프                 │
│   51  Realtime3  모듈 설정 메시지 처리                                        │
│   25  Normal1    모듈 연결 관리                                               │
├───────────────────────────────────────────────────────────────────────────────┤
│ 사용자 선택 가능 영역 (XM_PRIO_*)                                             │
│   48  Realtime     ← XM_PRIO_NEAR_REALTIME (주의: 모듈 연결 처리와 경합 가능) │
│   40  High         ← XM_PRIO_ABOVE_CONTROL                                    │
│   32  AboveNormal  ← XM_PRIO_BELOW_CONTROL                                    │
│   24  Normal       ← XM_PRIO_BACKGROUND ⭐ (권장 기본값)                      │
│    8  Low          ← XM_PRIO_IDLE                                             │
└───────────────────────────────────────────────────────────────────────────────┘
```

| 우선순위 hint | 숫자 | 기본 stack | 용도 |
|---|---|---|---|
| `XM_PRIO_IDLE`          | 8  | 1 KB | 통계 / 로깅 등 매우 가벼운 작업 |
| `XM_PRIO_BACKGROUND`    | 24 | **8 KB** | **권장 기본** — FFT / 학습 / 큰 행렬 등 |
| `XM_PRIO_BELOW_CONTROL` | 32 | 4 KB | 100 ms 단위 PD 게인 갱신 등 |
| `XM_PRIO_ABOVE_CONTROL` | 40 | 2 KB | 안전 감시 task |
| `XM_PRIO_NEAR_REALTIME` | 48 | 2 KB | (주의) 100 Hz 이상 빠른 제어 보조 |

> **권장**: 처음에는 `XM_PRIO_BACKGROUND` 로 만드세요. stack 8 KB 가 일반
> 알고리즘에 충분하고 Control_Loop (1 kHz) jitter 에도 영향 최소.

---

## 3. 데이터 흐름 — Control_Loop ↔ 사용자 보조 Task

```
┌─────────────────────────────────────────────────────────────┐
│ 제어 루프 task (Control_Loop, 1 kHz)                        │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  입력 수집 ─▶ XM.status.*                           │    │
│  │  Control_Loop() ◀── 사용자 알고리즘                 │    │
│  │  XM.command.* ─▶ 출력 전송                          │    │
│  └─────────────────────────────────────────────────────┘    │
│         ↕ (단일 워드 volatile / 멀티 워드 XM_Mutex)         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  사용자 보조 task (Normal=24 등) — XM_Task_*        │    │
│  │  예: AdcSummary (100 Hz), HeavyCalc (OneShot)       │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### 공유변수 패턴 4가지

| 패턴 | 변수 형태 | 보호 방법 | 예제 |
|---|---|---|---|
| **A** Single-word flag | `volatile bool ready;` | volatile 만 | Ex.38 `s_adc_avg` |
| **B** Multi-word data  | `float buf[10];` | `XM_Mutex_*` | Ex.38 `s_adc_buf` |
| **C** ISR → Task       | ISR write / Task read | volatile + memory barrier | (시스템 영역) |
| **D** Snapshot         | Mutex 안 memcpy → 외부 read | Mutex+Snapshot | `XM.status` 스냅샷 |

---

## 4. 사용자 task 한도

| 상수 | 값 | 의미 |
|---|---|---|
| `XM_TASK_MAX_INSTANCES`      | 4     | 동시 사용자 task 한도 |
| `XM_TASK_HEAP_BUDGET_BYTES`  | 32768 | 사용자 task 가 쓸 수 있는 heap 한도 (32 KB) |
| `XM_TASK_STACK_MAX_WORDS`    | 2048  | 단일 task 최대 stack (8 KB) |
| `XM_TASK_STACK_MIN_WORDS`    | 128   | 단일 task 최소 stack (512 B) |

위반 시 동작:
- 4개 초과 / budget 초과 → `XM_Task_Create*` 가 `NULL` 반환 (단순 거부)
- 잘못된 핸들 사용 → API 가 `false`/`NULL` 반환 (Use-After-Delete 방어)
- self-Delete 시도 → no-op (거부)

> ⚠️ **워치독 (v2.6.0~)**: 시스템 워치독(IWDG, 약 8 초)은 `Control_Loop` 가 도는 1 kHz 주기에서 갱신됩니다. 따라서 **`Control_Setup` 이나 `Control_Loop` 안에서 8 초 넘게 머물면 보드가 리셋**됩니다. 오래 걸리는 작업은 나눠서 수행하세요.
>
> 여기서 만드는 사용자 task 는 `XM_PRIO_*` 가 모두 `Control_Loop` 보다 낮으므로 (최고값 `XM_PRIO_NEAR_REALTIME` = 48 < 제어 루프 task 53) **워치독 갱신을 막을 수 없습니다.** 무거운 계산은 오히려 이쪽으로 옮기는 것이 안전합니다 (Ex.36 의 NN 학습이 `XM_PRIO_BACKGROUND` 로 도는 이유).

---

## 5. 권장 사용 패턴

### 5.1 Periodic Reader + Control_Loop Writer (Ex.38)

```c
static XmMutexHandle_t s_mutex;
static volatile float  s_result;        /* 단일 워드 — volatile */
static float           s_buffer[10];    /* 멀티 워드 — Mutex */

static void _Reader(void) {              /* 100 Hz */
    if (XM_Mutex_Lock(s_mutex, 0)) {
        /* read s_buffer + compute */
        XM_Mutex_Unlock(s_mutex);
        s_result = computed_value;       /* lock 밖 OK */
    }
}

void Control_Setup(void) {
    s_mutex = XM_Mutex_Create();
    XM_Task_CreatePeriodic("Reader", _Reader, 10, XM_PRIO_BACKGROUND);
}

void Control_Loop(void) {                /* 1 kHz */
    if (XM_Mutex_Lock(s_mutex, 0)) {     /* 항상 timeout=0 */
        /* write s_buffer */
        XM_Mutex_Unlock(s_mutex);
    }
}
```

### 5.2 OneShot Trigger + Lifecycle 관리 (Ex.39)

```c
static XmTaskHandle_t s_heavy;

void Control_Loop(void) {
    if (trigger && s_heavy == NULL) {
        s_heavy = XM_Task_CreateOneShot("Heavy", _Calc, NULL, XM_PRIO_BACKGROUND);
    }
    if (s_heavy && XM_Task_IsComplete(s_heavy)) {
        XM_Task_Delete(s_heavy);
        s_heavy = NULL;                  /* ← 필수: dangling 방지 */
    }
}
```

---

## 6. 흔한 실수 (Pitfalls)

| 실수 | 결과 | 대응 |
|---|---|---|
| Control_Loop 안에서 `Mutex_Lock(m, > 0)` | 1 ms 주기 깨짐 | 항상 `timeout = 0`, 실패 시 skip |
| Lock 후 Unlock 누락 | mutex 영구 점유 | early return 전에 Unlock |
| Task Delete 후 handle 재사용 | API false 반환 | Delete 직후 `handle = NULL` |
| Task Delete 누락 | 4개 도달 후 새 task 생성 NULL | IsComplete → Delete 사이클 |
| `XM_PRIO_NEAR_REALTIME` 무한 루프 | 모듈 연결 처리와 경합 / 시스템 지연 | `XM_PRIO_BACKGROUND` 권장 |
| 단일 워드 vs 멀티 워드 race | 데이터 깨짐 | 32-bit 변수는 volatile, 그 이상은 Mutex |
| Float NaN/Inf 출력 | CAN-FD 송신 시 모터 폭주 | 계산 전 분모 != 0 / clamp |

---

## 7. 추천 학습 순서

1. **[Ex.00 ~ Ex.35](../../Examples/)** — 단일 task 알고리즘 학습
2. **[Ex.38 Periodic_Background_Task](../../Examples/38_Periodic_Background_Task/)** — Mutex+Snapshot 패턴 (이 문서 §3 참조)
3. **[Ex.39 Task_Lifecycle](../../Examples/39_Task_Lifecycle/)** — OneShot 생성/삭제 사이클
4. **본 문서 §4~6** — 사용자 task 한도 + Pitfalls

---

## 8. 직접 점검할 것

Task API 는 §4 의 제약(개수·스택·우선순위)을 지키면 동작합니다. 실행 시간 초과나
교착(deadlock)은 자동으로 감시하지 않으니 직접 점검하세요.

---

## 9. 변경 이력

| 날짜       | 버전 | 변경 |
|---|---|---|
| 2026-05-15 | 1.0  | 초안 |
