# `xm_api_freertos.h` — RTOS Helper Task API

> 📄 Target header: [`XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api_freertos.h`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api_freertos.h) (Rev1.1 has **byte-for-byte the same file** at the same relative path — there are no Rev-specific items on this page)
> 🧭 Related concept doc: [09. Helper Tasks & Data Sharing](../09-task-creation.en.md) — read this first for the task priority zones, the data-flow picture, and the 4 shared-variable patterns.
> 🧰 Related examples: [Ex.38 Periodic_Background_Task](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/38_Periodic_Background_Task/) · [Ex.39 Task_Lifecycle](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/39_Task_Lifecycle/) · [Ex.36 OnDevice_Kinesthetic_Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) (an example of migrating from the deprecated API to the new one)

This is the API for safely creating helper RTOS tasks that run alongside `Control_Loop()` (the 1 kHz control loop) without disturbing it, and for exchanging data with those tasks safely.

---

## When to use this

If you run something heavy — an FFT, a neural-network training pass, a large matrix operation — directly inside Control_Loop, you break the 1 ms period and destabilize control. In that case, use `XM_Task_CreateOneShot()` / `XM_Task_CreatePeriodic()` to split the computation into a separate RTOS task, and use `XM_Mutex_*` to exchange data safely with Control_Loop.

- For how to pick a task priority and how data flows between Control_Loop and helper tasks, read [09. Helper Tasks & Data Sharing](../09-task-creation.en.md) first.
- This page only covers each function's signature / parameters / return value / calling context.

---

## Function List

| Function | One-line description |
|---|---|
| [`XM_Task_CreateOneShot`](#xm_task_createoneshot) | Create a task that runs once and terminates automatically (e.g. NN training, FFT) |
| [`XM_Task_IsComplete`](#xm_task_iscomplete) | Check whether a OneShot task has finished (returned) |
| [`XM_Task_CreatePeriodic`](#xm_task_createperiodic) | Create a task that runs periodically (e.g. 100 Hz logging) |
| [`XM_Task_Suspend`](#xm_task_suspend) | Suspend a task |
| [`XM_Task_Resume`](#xm_task_resume) | Resume a suspended task |
| [`XM_Task_Delete`](#xm_task_delete) | Terminate a task and free its memory |
| [`XM_Mutex_Create`](#xm_mutex_create) | Create a mutex |
| [`XM_Mutex_Lock`](#xm_mutex_lock) | Attempt to lock a mutex |
| [`XM_Mutex_Unlock`](#xm_mutex_unlock) | Unlock a mutex |
| [`XM_Mutex_Delete`](#xm_mutex_delete) | Delete a mutex and free its memory |
| [`XM_Task_GetHeapFreeBytes`](#xm_task_getheapfreebytes) | Query the current system heap's free bytes |
| [`XM_Task_GetHeapMinEverBytes`](#xm_task_getheapmineverbytes) | Query the all-time-lowest heap free level (leak detection) |
| [`XM_Task_GetInstanceCount`](#xm_task_getinstancecount) | Query how many user tasks currently exist |
| [`XM_Task_GetBudgetRemainingBytes`](#xm_task_getbudgetremainingbytes) | Query the remaining user-task heap budget |
| [`XM_RTOS_PrintTaskList`](#xm_rtos_printtasklist) | Print the list of currently running tasks (diagnostics) |

> ⚠️ `XM_BgTask_Create` / `XM_BgTask_IsDone` are **Deprecated** and excluded from the table above. See the [Deprecated API](#deprecated-api-avoid-in-new-code) section below.

---

## Function Details

### OneShot Task API

#### `XM_Task_CreateOneShot`

```c
XmTaskHandle_t XM_Task_CreateOneShot(const char*         name,
                                      XmTaskOneShotFunc_t func,
                                      void*               arg,
                                      XmTaskPrio_t        prio_hint);
```

Creates a task that runs once and terminates automatically when the function `return`s. Use this to run time-consuming computation such as neural-network training or FFT outside Control_Loop.

| Name | Type | Description |
|---|---|---|
| `name` | `const char*` | Task name (10 characters or fewer recommended). `NULL`, an empty string, or a name already in use is rejected. |
| `func` | `XmTaskOneShotFunc_t` | The function to run as the task. Returning from it marks the task complete. |
| `arg` | `void*` | The argument passed to `func`. Pass **only static variables or heap pointers** — the address of a caller's local (stack) variable may already be gone by the time the task actually runs, so this is forbidden. |
| `prio_hint` | `XmTaskPrio_t` | Priority hint. Use `XM_PRIO_BACKGROUND` unless you have a specific reason not to. |

**Return value**: a valid `XmTaskHandle_t` on success, `NULL` if rejected (plus a `[XM-WARN]` message over USB-CDC).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately — do not call from an ISR). At most [`XM_TASK_MAX_INSTANCES`](#macros) (4) tasks can exist at the same time, and a completed task still counts toward this limit until you call [`XM_Task_Delete`](#xm_task_delete).

```c
static XmTaskHandle_t s_heavy;

if (trigger && s_heavy == NULL) {
    s_heavy = XM_Task_CreateOneShot("HeavyCalc", _HeavyCalc, NULL, XM_PRIO_BACKGROUND);
}
```

**See also**: [`XM_Task_IsComplete`](#xm_task_iscomplete), [`XM_Task_Delete`](#xm_task_delete), Ex.39 `task_lifecycle.c`

---

#### `XM_Task_IsComplete`

```c
bool XM_Task_IsComplete(XmTaskHandle_t handle);
```

Checks whether a OneShot task's function body has already `return`ed.

| Name | Type | Description |
|---|---|---|
| `handle` | `XmTaskHandle_t` | The handle returned by `XM_Task_CreateOneShot()` |

**Return value**: `true` — the function returned and the task is complete / `false` — still running, or the handle is invalid (or already deleted).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately).

```c
if (s_heavy != NULL && XM_Task_IsComplete(s_heavy)) {
    XM_Task_Delete(s_heavy);
    s_heavy = NULL;   /* prevents a dangling pointer — always set it to NULL */
}
```

**See also**: [`XM_Task_CreateOneShot`](#xm_task_createoneshot), [`XM_Task_Delete`](#xm_task_delete)

---

### Periodic Task API

#### `XM_Task_CreatePeriodic`

```c
XmTaskHandle_t XM_Task_CreatePeriodic(const char*          name,
                                       XmTaskPeriodicFunc_t func,
                                       uint32_t             period_ms,
                                       XmTaskPrio_t         prio_hint);
```

Creates a task that is called repeatedly at the given period. Use this for work that must keep running on its own cadence separate from Control_Loop, such as 100 Hz logging or a 10 Hz UI refresh.

| Name | Type | Description |
|---|---|---|
| `name` | `const char*` | Task name |
| `func` | `XmTaskPeriodicFunc_t` | The function called every period. **It takes no argument** — exchange data with Control_Loop through a shared variable (volatile or a mutex). |
| `period_ms` | `uint32_t` | Call period in ms. **1 ms is reserved for Control_Loop**, so 2 ms or more is recommended. |
| `prio_hint` | `XmTaskPrio_t` | Priority hint |

**Return value**: a valid handle on success, `NULL` if rejected.

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately).

```c
static XmMutexHandle_t s_buf_mutex;
static XmTaskHandle_t  s_summary_task;

void Control_Setup(void) {
    s_buf_mutex    = XM_Mutex_Create();
    s_summary_task = XM_Task_CreatePeriodic("AdcSummary", _SummaryTask,
                                             10U /* 10 ms = 100 Hz */,
                                             XM_PRIO_BACKGROUND);
}
```

**See also**: [09. Helper Tasks & Data Sharing §3 Shared-Variable Patterns](../09-task-creation.en.md), Ex.38 `periodic_bg_task.c`

---

### Task Lifecycle API

#### `XM_Task_Suspend`

```c
void XM_Task_Suspend(XmTaskHandle_t handle);
```

Suspends a task. It will not run again until you call [`XM_Task_Resume`](#xm_task_resume).

| Name | Type | Description |
|---|---|---|
| `handle` | `XmTaskHandle_t` | Handle of the task to suspend |

**Return value**: none (`void`).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). Passing an invalid handle is ignored without crashing (the same defensive pattern used by the other APIs).

**See also**: [`XM_Task_Resume`](#xm_task_resume)

---

#### `XM_Task_Resume`

```c
void XM_Task_Resume(XmTaskHandle_t handle);
```

Resumes a task previously stopped with [`XM_Task_Suspend`](#xm_task_suspend).

| Name | Type | Description |
|---|---|---|
| `handle` | `XmTaskHandle_t` | Handle of the task to resume |

**Return value**: none (`void`).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately).

**See also**: [`XM_Task_Suspend`](#xm_task_suspend)

---

#### `XM_Task_Delete`

```c
void XM_Task_Delete(XmTaskHandle_t handle);
```

Terminates a task and reclaims its memory (heap). Every task you create must eventually be matched with a `Delete` call, or you will leak heap.

| Name | Type | Description |
|---|---|---|
| `handle` | `XmTaskHandle_t` | Handle of the task to delete |

**Return value**: none (`void`).

- **OneShot**: it is recommended to confirm completion with [`XM_Task_IsComplete`](#xm_task_iscomplete) before calling this.
- **Periodic**: there is no "complete" state, so you can call this at any time to force termination.
- **No self-deletion**: an attempt by a task to `Delete` itself (from inside its own currently-running context) is rejected (stated in the header — do not call `XM_Task_Delete(own_handle)` from inside the task function itself).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately).

```c
if (s_heavy != NULL && XM_Task_IsComplete(s_heavy)) {
    XM_Task_Delete(s_heavy);
    s_heavy = NULL;   /* required — prevents a dangling pointer */
}
```

**See also**: [`XM_Task_CreateOneShot`](#xm_task_createoneshot), [`XM_Task_CreatePeriodic`](#xm_task_createperiodic), Ex.39 `task_lifecycle.c`

---

### Mutex API

#### `XM_Mutex_Create`

```c
XmMutexHandle_t XM_Mutex_Create(void);
```

Creates a mutex. Use one to protect **multi-word** data — arrays or structs — that Control_Loop and a helper task both read and write.

**Parameters**: none.

**Return value**: a valid `XmMutexHandle_t` on success, `NULL` on failure.

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). Typically created once in `Control_Setup()` and reused from then on.

```c
static XmMutexHandle_t s_buf_mutex;

void Control_Setup(void) {
    s_buf_mutex = XM_Mutex_Create();
}
```

**See also**: [`XM_Mutex_Lock`](#xm_mutex_lock), [09. Helper Tasks & Data Sharing §3 Shared-Variable Patterns](../09-task-creation.en.md)

---

#### `XM_Mutex_Lock`

```c
bool XM_Mutex_Lock(XmMutexHandle_t mutex, uint32_t timeout_ms);
```

Attempts to lock a mutex.

| Name | Type | Description |
|---|---|---|
| `mutex` | `XmMutexHandle_t` | Handle of the mutex to lock |
| `timeout_ms` | `uint32_t` | Lock wait time in ms. **Always use 0 inside Control_Loop** (stated in the header — never block; a 1 ms period would be broken). 0 is recommended inside helper tasks too. |

**Return value**: `true` — lock acquired / `false` — timed out, or the handle is invalid.

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). Using `timeout_ms > 0` inside Control_Loop can, in the worst case, delay the 1 kHz period by that much — always use `0` together with a skip-on-failure pattern.

```c
/* Control_Loop — 1 kHz writer, always timeout=0 */
if (XM_Mutex_Lock(s_buf_mutex, 0U)) {
    s_adc_buf[s_adc_idx] = XM_AnalogRead(XM_EXT_ADC_1);
    s_adc_idx = (s_adc_idx + 1) % ADC_BUF_SIZE;
    XM_Mutex_Unlock(s_buf_mutex);
}
/* On failure, this cycle's sample is simply skipped (preserving jitter) */
```

**See also**: [`XM_Mutex_Unlock`](#xm_mutex_unlock), Ex.38 `periodic_bg_task.c`

---

#### `XM_Mutex_Unlock`

```c
void XM_Mutex_Unlock(XmMutexHandle_t mutex);
```

Unlocks a mutex. **Only the task that locked it** may unlock it.

| Name | Type | Description |
|---|---|---|
| `mutex` | `XmMutexHandle_t` | Handle of the mutex to unlock |

**Return value**: none (`void`).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). After a successful `XM_Mutex_Lock()`, be sure to call `Unlock` before any early return — omitting it leaves that mutex permanently locked.

**See also**: [`XM_Mutex_Lock`](#xm_mutex_lock)

---

#### `XM_Mutex_Delete`

```c
void XM_Mutex_Delete(XmMutexHandle_t mutex);
```

Deletes a mutex and reclaims its memory.

| Name | Type | Description |
|---|---|---|
| `mutex` | `XmMutexHandle_t` | Handle of the mutex to delete |

**Return value**: none (`void`).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). Do not delete a mutex while a task that still uses it is alive — delete the related task(s) with [`XM_Task_Delete`](#xm_task_delete) first, then delete the mutex.

```c
XM_Task_Delete(s_summary_task);
XM_Mutex_Delete(s_buf_mutex);
```

**See also**: [`XM_Mutex_Create`](#xm_mutex_create), [`XM_Task_Delete`](#xm_task_delete)

---

### HW-constraint Diagnostic API

These four functions all take no parameters and immediately return the current state as a single number — simple getters. Use them when you want to check in code the HW constraints described in [09. Helper Tasks & Data Sharing §4](../09-task-creation.en.md) (max 4 tasks, 32 KB heap budget).

#### `XM_Task_GetHeapFreeBytes`

```c
uint32_t XM_Task_GetHeapFreeBytes(void);
```

Returns the current system heap's free byte count.

**Return value**: free heap (bytes).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately).

---

#### `XM_Task_GetHeapMinEverBytes`

```c
uint32_t XM_Task_GetHeapMinEverBytes(void);
```

Returns the lowest the system heap has ever dropped to. If this value keeps shrinking across repeated task create/delete cycles, suspect a heap leak (e.g. a missing `XM_Task_Delete` call).

**Return value**: all-time-lowest heap free level (bytes).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). Ex.39 verifies there is no leak by pressing a button 3 times to repeat OneShot task create/delete cycles and confirming this value does not keep dropping.

**See also**: Ex.39 `README.md` (heap-leak verification scenario)

---

#### `XM_Task_GetInstanceCount`

```c
uint32_t XM_Task_GetInstanceCount(void);
```

Returns the number of user tasks created so far that have not yet been `Delete`d (including OneShot tasks that completed but were not yet deleted).

**Return value**: current user-task count (0 to [`XM_TASK_MAX_INSTANCES`](#macros)).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). Once this reaches 4, every subsequent `XM_Task_CreateOneShot`/`CreatePeriodic` call returns `NULL` — checking this value before creating a new task helps diagnose why.

---

#### `XM_Task_GetBudgetRemainingBytes`

```c
uint32_t XM_Task_GetBudgetRemainingBytes(void);
```

Returns how much of the user-task-only heap budget (total [`XM_TASK_HEAP_BUDGET_BYTES`](#macros), 32 KB) remains.

**Return value**: remaining budget (bytes).

> ⚠️ **Calling context**: Based on the Control_Setup()/Control_Loop() context (the header does not state ISR-safety separately). If this value is too low, creating a task with a large stack (e.g. the 8 KB default for `XM_PRIO_BACKGROUND`) may be rejected.

---

### Diagnostic API

#### `XM_RTOS_PrintTaskList`

```c
void XM_RTOS_PrintTaskList(void);
```

Prints the list of currently running tasks (on-demand diagnostics).

**Parameters**: none. **Return value**: none (`void`).

> ⚠️ **Current status (Phase 1)**: in this SDK version, the diagnostic output channel is not yet wired up, so calling this is a **no-op stub that does nothing** (stated in the header). It is planned to connect to the PhAI Studio diagnostic channel in Phase 2.
>
> ⚠️ **Calling context (get ahead of it)**: right now it is a stub, so calling it anywhere is safe, but the internal implementation this function wraps already documents the constraint "**do not call inside the 1 kHz Control_Loop** (accumulated string formatting can take 100 µs or more)." To stay safe once Phase 2 lands, it's worth building the habit now of only calling this from somewhere that does **not** run at 1 kHz, such as `Control_Setup()` or a button-event handler.

---

## Deprecated API (avoid in new code)

These APIs remain for v1.0 compatibility. **They are scheduled for removal in the next major release**, so do not use them in new code. Only the table is kept below — full function-detail blocks are omitted.

| Function | Replacement API | Notes |
|---|---|---|
| `XmBgTaskHandle_t XM_BgTask_Create(const char* name, XmBgTaskFunc_t func, void* arg, uint32_t stack_words)` | [`XM_Task_CreateOneShot`](#xm_task_createoneshot)`(name, func, arg, XM_PRIO_BACKGROUND)` | The compiler emits a `deprecated` warning. Calling [`XM_Task_Delete`](#xm_task_delete) after completion is still required, same as the new API. |
| `bool XM_BgTask_IsDone(XmBgTaskHandle_t handle)` | [`XM_Task_IsComplete`](#xm_task_iscomplete) | Also subject to a `deprecated` compiler warning. |

> Ex.36 (`OnDevice_Kinesthetic_Learning`) is a real example of migrating from `XM_BgTask_Create` to `XM_Task_CreateOneShot` (its source comment records `[2026-05-13] XM_BgTask_Create → XM_Task_CreateOneShot migration`).

---

## Type Definitions

### `XmTaskPrio_t`

The task-priority-hint enum. Instead of exposing CMSIS-OS2's raw numeric priority values, it wraps them into named, meaningful levels. The numeric range already occupied by system tasks (51–55) does not exist in this enum at all — so a user task can never accidentally collide with a system task's priority.

```c
typedef enum {
    XM_PRIO_IDLE          = 8,   /* osPriorityLow      — same tier as DefaultTask */
    XM_PRIO_BACKGROUND    = 24,  /* osPriorityNormal   — recommended default */
    XM_PRIO_BELOW_CONTROL = 32,  /* osPriorityAboveNormal — below Control_Loop */
    XM_PRIO_ABOVE_CONTROL = 40,  /* osPriorityHigh     — same tier as USBH, still below Control_Loop */
    XM_PRIO_NEAR_REALTIME = 48,  /* osPriorityRealtime — caution: may contend with PnP traffic */
} XmTaskPrio_t;
```

| Value | Number | Default stack | Description |
|---|---|---|---|
| `XM_PRIO_IDLE` | 8 | 1 KB | The lowest priority, same tier as `DefaultTask` |
| `XM_PRIO_BACKGROUND` | 24 | 8 KB | **Recommended default** |
| `XM_PRIO_BELOW_CONTROL` | 32 | 4 KB | Below Control_Loop |
| `XM_PRIO_ABOVE_CONTROL` | 40 | 2 KB | Same tier as USBH — still below Control_Loop |
| `XM_PRIO_NEAR_REALTIME` | 48 | 2 KB | ⚠️ Does not contend with PnP traffic (`PnP_Task`, 25) directly, but sits close to the system tasks above it — don't overuse it. |

For the full picture of how priority zones are laid out system-wide (and why 51–55 are excluded), see [09. Helper Tasks & Data Sharing §1–2](../09-task-creation.en.md).

### Handle Types

| Type | Definition | Description |
|---|---|---|
| `XmTaskHandle_t` | `typedef void*` | Task handle. `NULL` means an invalid handle. |
| `XmMutexHandle_t` | `typedef void*` | Mutex handle. `NULL` means an invalid handle. |

### Function Pointer Types

| Type | Signature | Description |
|---|---|---|
| `XmTaskOneShotFunc_t` | `void (*)(void* arg)` | The type of the `func` parameter of [`XM_Task_CreateOneShot`](#xm_task_createoneshot) |
| `XmTaskPeriodicFunc_t` | `void (*)(void)` | The type of the `func` parameter of [`XM_Task_CreatePeriodic`](#xm_task_createperiodic) (no argument — pass data through a shared variable) |

---

## Macros

### System Guard Constants

Students cannot change these values, but they are exposed as-is in the header so you can use them in conditional logic (e.g. `if (XM_Task_GetInstanceCount() < XM_TASK_MAX_INSTANCES)`).

| Macro | Value | Meaning |
|---|---|---|
| `XM_TASK_MAX_INSTANCES` | `4U` | Max number of user tasks that can exist at the same time |
| `XM_TASK_HEAP_BUDGET_BYTES` | `32768U` (32 KB) | Total heap all user tasks together may use |
| `XM_TASK_STACK_MAX_WORDS` | `2048U` (8 KB) | Max stack for a single task (1 word = 4 bytes) |
| `XM_TASK_STACK_MIN_WORDS` | `128U` (512 B) | Min stack for a single task |

### Default Stack per `prio_hint`

When you call `XM_Task_CreateOneShot`/`CreatePeriodic`, you don't specify a stack size directly — the value below is applied automatically based on `prio_hint`.

| Macro | Value (words) | In bytes | Corresponding `prio_hint` |
|---|---|---|---|
| `XM_TASK_STACK_IDLE_WORDS` | `256U` | 1 KB | `XM_PRIO_IDLE` |
| `XM_TASK_STACK_BACKGROUND_WORDS` | `2048U` | 8 KB | `XM_PRIO_BACKGROUND` |
| `XM_TASK_STACK_BELOW_WORDS` | `1024U` | 4 KB | `XM_PRIO_BELOW_CONTROL` |
| `XM_TASK_STACK_ABOVE_WORDS` | `512U` | 2 KB | `XM_PRIO_ABOVE_CONTROL` |
| `XM_TASK_STACK_NRT_WORDS` | `512U` | 2 KB | `XM_PRIO_NEAR_REALTIME` |

> The header comment records `XM_TASK_STACK_BACKGROUND_WORDS` (8 KB) as "measured from Ex.36 NN_Train" — it's large enough for typical algorithmic computation.

---

## Common Mistakes

The full pitfalls table lives in [09. Helper Tasks & Data Sharing §6](../09-task-creation.en.md). Summarized here are only the ones directly tied to this header:

| Mistake | Result | Fix |
|---|---|---|
| `XM_Mutex_Lock(m, > 0)` inside Control_Loop | Breaks the 1 ms period | Always use `timeout_ms = 0`, skip on failure |
| Forgetting `XM_Mutex_Unlock` after a successful `XM_Mutex_Lock` | Mutex stays locked forever | Always unlock before any early return |
| Reusing a handle after `XM_Task_Delete` | All subsequent calls return `false`/`NULL` | Set `handle = NULL` right after Delete |
| Forgetting to call `XM_Task_Delete` at all | Hits the 4-task limit → later Creates return `NULL` | Follow the `IsComplete` → `Delete` cycle for OneShot tasks |
| A task function calling `XM_Task_Delete` on itself | Rejected (no-op) | Check `IsComplete` and call Delete from the outside (e.g. Control_Loop) |

---

## Related Documents

- [09. Helper Tasks & Data Sharing](../09-task-creation.en.md) — system task inventory, priority zones, data flow, shared-variable patterns
- [Ex.38 Periodic_Background_Task](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/38_Periodic_Background_Task/)
- [Ex.39 Task_Lifecycle](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/39_Task_Lifecycle/)
- [Ex.36 OnDevice_Kinesthetic_Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/)
