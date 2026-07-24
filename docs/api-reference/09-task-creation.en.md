# XM10 Task Topology — RTOS Task API User Guide

> A guide to writing auxiliary tasks in the XM10 SDK. Covers the system task inventory,
> priority levels, data flow, and four shared-variable patterns.

**Audience**: SDK users (researchers / students / general robotics developers)
**Date**: 2026-05-15
**Related**:
- User API: [`XM_FW/XM_API/xm_api_freertos.h`](https://github.com/AGR-EXO/Extension_Module/tree/Develop/XM_FW/XM_API/xm_api_freertos.h)
- Task Manager: [`XM_FW/System/Task/xm_task_manager.{c,h}`](https://github.com/AGR-EXO/Extension_Module/tree/Develop/XM_FW/System/Task/)
- Examples: [`Examples/38_Periodic_Background_Task/`](../../Examples/38_Periodic_Background_Task/) · [`Examples/39_Task_Lifecycle/`](../../Examples/39_Task_Lifecycle/)

---

## 1. System Task Inventory

The tasks listed below are created automatically by the XM10 SDK at boot. **Do not modify
them directly** — doing so will compromise system stability.

| Priority | Number | Task Name | Stack | Period | Responsibility |
|---|---|---|---|---|---|
| Realtime7 | 55 | `StartupTask`     | 2 KB  | Once (self-deletes) | HW init / module enumeration |
| Realtime7 | 55 | `IOIF_UartRx`     | 512 B | Event-driven  | UART receive (shared) |
| **Realtime6** | **54** | **`UserTask`** | **32 KB** | **1 ms (1 kHz)** | **Calls Control_Setup + Control_Loop + PDO snapshot** |
| Realtime3 | 51 | `NRT_Proc`        | 2 KB  | Semaphore     | SDO/NMT processing |
| High      | 40 | `USBH_Queue`      | 2 KB  | Event         | USB Host events |
| Normal1   | 25 | `PnP_Task`        | 2 KB  | 100 ms        | Plug & Play (automatic module registration) |
| Normal    | 24 | `usbContolTask`   | 2 KB  | 10 ms         | USB mode switching / CDC |
| Normal    | 24 | `DataLoggerTask`  | 8 KB  | Event         | internal, reserved (currently unused) |
| **Normal** | **24** | **`XM_Task_*` (user)** | **Per prio_hint** | OneShot/Periodic | **Auxiliary tasks created by the user** |
| Low       | 8  | `DefaultTask`     | 2 KB  | Suspended     | (Unused) |

---

## 2. User Task Priority Zones

When creating an auxiliary task with `XM_Task_CreateOneShot()` or
`XM_Task_CreatePeriodic()`, choose a `prio_hint` from the zones below.

```
┌──────────────────────────────────────────────────────────────┐
│ System task zone — do not enter                              │
│   55  Realtime7  StartupTask / IOIF_UartRx                   │
│   54  Realtime6  UserTask (Control_Loop, 1 kHz control loop) │
│   51  Realtime3  NRT_Proc (SDO/NMT)                          │
├──────────────────────────────────────────────────────────────┤
│ User-selectable zone                                         │
│   48  Realtime  ← XM_PRIO_NEAR_REALTIME (caution: may contend with PnP) │
│   40  High      ← XM_PRIO_ABOVE_CONTROL                      │
│   32  AbvNormal ← XM_PRIO_BELOW_CONTROL                      │
│   24  Normal    ← XM_PRIO_BACKGROUND ⭐ (recommended default) │
│    8  Low       ← XM_PRIO_IDLE                               │
└──────────────────────────────────────────────────────────────┘
```

| Priority hint | Number | Default stack | Use case |
|---|---|---|---|
| `XM_PRIO_IDLE`          | 8  | 1 KB | Statistics, logging, and other very lightweight work |
| `XM_PRIO_BACKGROUND`    | 24 | **8 KB** | **Recommended default** — FFT, learning algorithms, large matrix operations, etc. |
| `XM_PRIO_BELOW_CONTROL` | 32 | 4 KB | PD gain updates at ~100 ms intervals |
| `XM_PRIO_ABOVE_CONTROL` | 40 | 2 KB | Safety monitor tasks |
| `XM_PRIO_NEAR_REALTIME` | 48 | 2 KB | (Use with caution) Fast control auxiliaries above 100 Hz |

> **Recommendation**: Start with `XM_PRIO_BACKGROUND`. The 8 KB stack is sufficient for
> most algorithms, and it has minimal impact on Control_Loop (1 kHz) jitter.

---

## 3. Data Flow — Control_Loop ↔ Auxiliary Tasks

```
┌─ FDCAN ISR ──┐
│  (PDO recv)  │
└──────┬───────┘
       │ seqlock
       ▼
┌─────────────────────────────────────────────────────────────┐
│ UserTask (1 kHz)                                            │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  _FetchAllInputs() ─▶ XM.status.*                   │    │
│  │  SYNC broadcast      (CM/SM trigger)                │    │
│  │  XM_TotalData_Snapshot()                            │    │
│  │  Control_Loop() ◀── user algorithm                  │    │
│  │  _FlushAllOutputs() ─▶ XM.command.* ─▶ CAN TX       │    │
│  │  XM_USB_ProcessPeriodic()                           │    │
│  └─────────────────────────────────────────────────────┘    │
│         ↕ (single-word volatile / multi-word XM_Mutex)      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  User auxiliary tasks (Normal=24, etc.) — XM_Task_* │    │
│  │  e.g. AdcSummary (100 Hz), HeavyCalc (OneShot)      │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### Four Shared-Variable Patterns

| Pattern | Variable form | Protection method | Example |
|---|---|---|---|
| **A** Single-word flag | `volatile bool ready;` | `volatile` only | Ex.38 `s_adc_avg` |
| **B** Multi-word data  | `float buf[10];` | `XM_Mutex_*` | Ex.38 `s_adc_buf` |
| **C** ISR → Task       | ISR writes / Task reads | `volatile` + memory barrier | (System domain) |
| **D** Snapshot         | `memcpy` inside Mutex → external read | Mutex + Snapshot | `CM_GetRxData` |

---

## 4. Hardware Constraints (Task Manager API Contract Guards)

| Constant | Value | Meaning |
|---|---|---|
| `XM_TASK_MAX_INSTANCES`      | 4     | Maximum number of concurrent user tasks |
| `XM_TASK_HEAP_BUDGET_BYTES`  | 32768 | Total heap budget available to user tasks (32 KB) |
| `XM_TASK_STACK_MAX_WORDS`    | 2048  | Maximum stack per task (8 KB) |
| `XM_TASK_STACK_MIN_WORDS`    | 128   | Minimum stack per task (512 B) |

Behavior on violation:
- Exceeding 4 tasks or the heap budget → `XM_Task_Create*` returns `NULL` (silently rejected)
- Using an invalid handle → API returns `false`/`NULL` (Use-After-Delete defense)
- Attempting self-Delete → no-op (rejected)

---

## 5. Recommended Usage Patterns

### 5.1 Periodic Reader + Control_Loop Writer (Ex.38)

```c
static XmMutexHandle_t s_mutex;
static volatile float  s_result;        /* single word — volatile */
static float           s_buffer[10];    /* multi-word — Mutex */

static void _Reader(void) {              /* 100 Hz */
    if (XM_Mutex_Lock(s_mutex, 0)) {
        /* read s_buffer + compute */
        XM_Mutex_Unlock(s_mutex);
        s_result = computed_value;       /* OK to write outside the lock */
    }
}

void Control_Setup(void) {
    s_mutex = XM_Mutex_Create();
    XM_Task_CreatePeriodic("Reader", _Reader, 10, XM_PRIO_BACKGROUND);
}

void Control_Loop(void) {                /* 1 kHz */
    if (XM_Mutex_Lock(s_mutex, 0)) {     /* always timeout=0 */
        /* write s_buffer */
        XM_Mutex_Unlock(s_mutex);
    }
}
```

### 5.2 OneShot Trigger + Lifecycle Management (Ex.39)

```c
static XmTaskHandle_t s_heavy;

void Control_Loop(void) {
    if (trigger && s_heavy == NULL) {
        s_heavy = XM_Task_CreateOneShot("Heavy", _Calc, NULL, XM_PRIO_BACKGROUND);
    }
    if (s_heavy && XM_Task_IsComplete(s_heavy)) {
        XM_Task_Delete(s_heavy);
        s_heavy = NULL;                  /* ← required: prevents dangling handle */
    }
}
```

---

## 6. Common Pitfalls

| Mistake | Consequence | Fix |
|---|---|---|
| Calling `Mutex_Lock(m, > 0)` inside Control_Loop | Breaks the 1 ms period | Always use `timeout = 0`; skip on failure |
| Forgetting to Unlock after locking | Mutex permanently held | Call Unlock before every early return |
| Reusing a handle after Task Delete | API returns false | Set `handle = NULL` immediately after Delete |
| Not deleting completed tasks | New task creation returns NULL once 4 tasks are reached | Use the IsComplete → Delete cycle |
| Infinite loop at `XM_PRIO_NEAR_REALTIME` | Contention with PnP / system delays | Use `XM_PRIO_BACKGROUND` instead |
| Misidentifying single-word vs. multi-word access | Data corruption | Use `volatile` for 32-bit variables; use Mutex for anything larger |
| Outputting NaN/Inf values | Motor runaway on CAN-FD transmission | Check divisor != 0 and clamp before computation |

---

## 7. Recommended Learning Path

1. **[Ex.00 ~ Ex.35](../../Examples/)** — Single-task algorithm fundamentals
2. **[Ex.38 Periodic_Background_Task](../../Examples/38_Periodic_Background_Task/)** — Mutex + Snapshot pattern (see §3 of this document)
3. **[Ex.39 Task_Lifecycle](../../Examples/39_Task_Lifecycle/)** — OneShot create/delete cycle
4. **This document §4–6** — Hardware constraints and pitfalls

---

## 8. Risk Management Integration (Phase 2 Follow-up)

Runtime guards (Control_Loop overrun, mutex deadlock, periodic overrun, fault analysis,
etc.) and debugging channels (PhAI Studio diagnostic integration) are planned for a
separate introduction under the XM10-wide Risk Management roadmap. The Task API itself
is stable within the Phase 1 scope (Task API contract guards only).

---

## 9. Revision History

| Date       | Version | Change |
|---|---|---|
| 2026-05-15 | 1.0  | Initial draft — Task API essentials extracted after separating RM guards |
