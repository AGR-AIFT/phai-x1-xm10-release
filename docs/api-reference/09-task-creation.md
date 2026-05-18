# XM10 Task Topology — RTOS Task API 사용자 가이드

> XM10 SDK 의 사용자 보조 task 작성 가이드. 시스템 task 인벤토리 + 우선순위 +
> 데이터 흐름 + 공유변수 패턴 4가지.

**Audience**: SDK 사용자 (연구자 / 학습자 / 일반 로보틱스 개발자)
**Date**: 2026-05-15
**Related**:
- 사용자 API: [`XM_FW/XM_API/xm_api_freertos.h`](../../XM_FW/XM_API/xm_api_freertos.h)
- Task Manager: [`XM_FW/System/Task/xm_task_manager.{c,h}`](../../XM_FW/System/Task/)
- 예제: [`Examples/38_Periodic_Background_Task/`](../../Examples/38_Periodic_Background_Task/) · [`Examples/39_Task_Lifecycle/`](../../Examples/39_Task_Lifecycle/)

---

## 1. 시스템 Task 인벤토리

XM10 SDK 가 부팅 시 자동으로 생성하는 task 목록. **사용자가 직접 변경하면
안 됩니다** (시스템 안정성 손상).

| 우선순위 | 숫자 | Task 이름        | 스택  | 주기      | 책임 |
|---|---|---|---|---|---|
| Realtime7 | 55 | `StartupTask`     | 2 KB  | 1회 (자기 삭제) | HW init / module enumerate |
| Realtime7 | 55 | `IOIF_UartRx`     | 512 B | event-driven  | UART 수신 (공유) |
| **Realtime6** | **54** | **`UserTask`** | **32 KB** | **1 ms (1 kHz)** | **Control_Setup + Control_Loop 호출 + PDO snapshot** |
| Realtime3 | 51 | `NRT_Proc`        | 2 KB  | semaphore     | SDO/NMT 처리 |
| High      | 40 | `USBH_Queue`      | 2 KB  | event         | USB Host 이벤트 |
| Normal1   | 25 | `PnP_Task`        | 2 KB  | 100 ms        | Plug & Play (모듈 자동 등록) |
| Normal    | 24 | `usbContolTask`   | 2 KB  | 10 ms         | USB 모드 전환 / CDC/MSC |
| Normal    | 24 | `DataLoggerTask`  | 8 KB  | event         | USB MSC 로깅 drain |
| **Normal** | **24** | **`XM_Task_*` (사용자)** | **prio_hint 별** | OneShot/Periodic | **사용자가 만든 보조 task** |
| Low       | 8  | `DefaultTask`     | 2 KB  | suspended     | (사용 안 함) |

---

## 2. 사용자 Task 우선순위 영역

사용자가 `XM_Task_CreateOneShot()` / `XM_Task_CreatePeriodic()` 로 보조 task 를
만들 때 `prio_hint` 로 선택할 수 있는 영역입니다.

```
┌──────────────────────────────────────────────────────────────┐
│ 시스템 task 점유 영역 — 사용자 진입 금지                       │
│   55  Realtime7  StartupTask / IOIF_UartRx                   │
│   54  Realtime6  UserTask (Control_Loop, 1 kHz 제어 루프)    │
│   51  Realtime3  NRT_Proc (SDO/NMT)                          │
├──────────────────────────────────────────────────────────────┤
│ 사용자 선택 가능 영역                                          │
│   48  Realtime  ← XM_PRIO_NEAR_REALTIME (주의 — PnP 경합 가능)│
│   40  High      ← XM_PRIO_ABOVE_CONTROL                       │
│   32  AbvNormal ← XM_PRIO_BELOW_CONTROL                       │
│   24  Normal    ← XM_PRIO_BACKGROUND ⭐ (권장 기본값)          │
│    8  Low       ← XM_PRIO_IDLE                                │
└──────────────────────────────────────────────────────────────┘
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
┌─ FDCAN ISR ──┐
│  (PDO 수신)  │
└──────┬───────┘
       │ seqlock
       ▼
┌─────────────────────────────────────────────────────────────┐
│ UserTask (1 kHz)                                            │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  _FetchAllInputs() ─▶ XM.status.*                   │    │
│  │  SYNC broadcast      (CM/SM 트리거)                  │    │
│  │  XM_TotalData_Snapshot()                            │    │
│  │  Control_Loop() ◀── 사용자 알고리즘                 │    │
│  │  _FlushAllOutputs() ─▶ XM.command.* ─▶ CAN TX       │    │
│  │  XM_USB_ProcessPeriodic()                           │    │
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
| **D** Snapshot         | Mutex 안 memcpy → 외부 read | Mutex+Snapshot | `CM_GetRxData` |

---

## 4. HW 제약 (Task Manager API contract 가드)

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
| `XM_PRIO_NEAR_REALTIME` 무한 루프 | PnP 경합 / 시스템 지연 | `XM_PRIO_BACKGROUND` 권장 |
| 단일 워드 vs 멀티 워드 race | 데이터 깨짐 | 32-bit 변수는 volatile, 그 이상은 Mutex |
| Float NaN/Inf 출력 | CAN-FD 송신 시 모터 폭주 | 계산 전 분모 != 0 / clamp |

---

## 7. 추천 학습 순서

1. **[Ex.00 ~ Ex.35](../../Examples/)** — 단일 task 알고리즘 학습
2. **[Ex.38 Periodic_Background_Task](../../Examples/38_Periodic_Background_Task/)** — Mutex+Snapshot 패턴 (이 문서 §3 참조)
3. **[Ex.39 Task_Lifecycle](../../Examples/39_Task_Lifecycle/)** — OneShot 생성/삭제 사이클
4. **본 문서 §4~6** — HW 제약 + Pitfalls

---

## 8. Risk Management 통합 (Phase 2 후속)

런타임 가드 (Control_Loop overrun / mutex deadlock / Periodic overrun / fault 분석
등) 와 디버깅 채널 (PhAI Studio 진단 통합) 은 XM10 전체 차원 Risk Management 계획
하에 별도 도입 예정. 본 SDK 의 Task API 자체는 Phase 1 (Task API contract 가드만)
범위에서 안정 동작합니다.

---

## 9. 변경 이력

| 날짜       | 버전 | 변경 |
|---|---|---|
| 2026-05-15 | 1.0  | 초안 — RM 가드 분리 후 Task API 본질만 정리 |
