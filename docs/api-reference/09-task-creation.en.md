# XM10 Task Topology — RTOS Task API User Guide

> A guide to writing auxiliary tasks in the XM10 SDK. Covers priority levels, data flow,
> and four shared-variable patterns.

**Audience**: SDK users (researchers / students / general robotics developers)
**Date**: 2026-05-15
**Related**:
- User API: [`XM_FW/XM_API/xm_api_freertos.h`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api_freertos.h)
- Examples: [`Examples/38_Periodic_Background_Task/`](../../Examples/38_Periodic_Background_Task/) · [`Examples/39_Task_Lifecycle/`](../../Examples/39_Task_Lifecycle/)

---

## 1. System Tasks

The XM10 SDK automatically creates the system tasks it needs at boot. **Do not modify
them directly** — doing so will compromise system stability. `Control_Setup()` and
`Control_Loop()` are called from one of them, the control-loop task (Control_Loop), on a
1 ms (1 kHz) cycle (priority 53, 32 KB stack). Create your own auxiliary tasks only with
`XM_Task_Create*()` and the `XM_PRIO_*` hints below.

---

## 2. User Task Priority Zones

When creating an auxiliary task with `XM_Task_CreateOneShot()` or
`XM_Task_CreatePeriodic()`, choose a `prio_hint` from the zones below.

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│ System tasks — you cannot create or change these                                                │
│   55  Realtime7  System startup / CAN-FD receive  ← handles robot/sensor data on arrival        │
│   54  Realtime6  UART receive                     ← sensor packet parsing                       │
│   53  Realtime5  Control_Loop                     ← 1 kHz control loop                          │
│   51  Realtime3  Module configuration messages                                                  │
│   25  Normal1    Module connection management                                                   │
├─────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Priorities you can choose (XM_PRIO_*)                                                           │
│   48  Realtime     ← XM_PRIO_NEAR_REALTIME (caution: can clash with module connection handling) │
│   40  High         ← XM_PRIO_ABOVE_CONTROL                                                      │
│   32  AboveNormal  ← XM_PRIO_BELOW_CONTROL                                                      │
│   24  Normal       ← XM_PRIO_BACKGROUND ⭐ (recommended default)                                │
│    8  Low          ← XM_PRIO_IDLE                                                               │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
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
┌─────────────────────────────────────────────────────────────┐
│ control-loop task (Control_Loop, 1 kHz)                     │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  gather inputs ─▶ XM.status.*                       │    │
│  │  Control_Loop() ◀── your algorithm                  │    │
│  │  XM.command.* ─▶ send outputs                       │    │
│  └─────────────────────────────────────────────────────┘    │
│         ↕ (single word: volatile / multi-word: XM_Mutex)    │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  user helper task (Normal=24 etc.) — XM_Task_*      │    │
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
| **D** Snapshot         | `memcpy` inside Mutex → external read | Mutex + Snapshot | `XM.status` snapshot |

---

## 4. User Task Limits

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

> ⚠️ **Watchdog (v2.6.0+)**: the system watchdog (IWDG, about 8 seconds) is refreshed on the 1 kHz cycle that runs `Control_Loop`. So if **`Control_Setup` or `Control_Loop` takes more than 8 seconds, the board resets.** Split long work into steps.
>
> The user tasks created here cannot block that refresh: every `XM_PRIO_*` level sits below `Control_Loop` (the highest, `XM_PRIO_NEAR_REALTIME` = 48, is still under the control-loop task's 53). Heavy computation is in fact safer here — that is why the NN training in Ex.36 runs at `XM_PRIO_BACKGROUND`.

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
| Infinite loop at `XM_PRIO_NEAR_REALTIME` | Clashes with module connection handling / system delays | Use `XM_PRIO_BACKGROUND` instead |
| Misidentifying single-word vs. multi-word access | Data corruption | Use `volatile` for 32-bit variables; use Mutex for anything larger |
| Outputting NaN/Inf values | Motor runaway on CAN-FD transmission | Check divisor != 0 and clamp before computation |

---

## 7. Recommended Learning Path

1. **[Ex.00 ~ Ex.35](../../Examples/)** — Single-task algorithm fundamentals
2. **[Ex.38 Periodic_Background_Task](../../Examples/38_Periodic_Background_Task/)** — Mutex + Snapshot pattern (see §3 of this document)
3. **[Ex.39 Task_Lifecycle](../../Examples/39_Task_Lifecycle/)** — OneShot create/delete cycle
4. **This document §4–6** — User task limits and pitfalls

---

## 8. What You Need to Check Yourself

The Task API works as long as you stay within the limits in §4 (number of tasks, stack,
priority). It does not monitor for overruns or deadlocks automatically, so check those yourself.

---

## 9. Revision History

| Date       | Version | Change |
|---|---|---|
| 2026-05-15 | 1.0  | Initial draft |
