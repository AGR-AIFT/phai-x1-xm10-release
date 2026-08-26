# `xm_api_tsm.h` — Task State Machine (TSM)

> **Header**: `XM_FW/XM_API/xm_api_tsm.h` (Rev1.1 / Rev2.0 fully identical — no Rev-specific items)
> **Related concept doc**: [01. Task State Machine](../01-task-state-machine.en.md)
> **Related examples**: [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) · [01_Button_LED_Basic](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/01_Button_LED_Basic/) · [03_Button_LED_FSM](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/03_Button_LED_FSM/) · [06_Ext_IO_Safety_Switch](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/06_Ext_IO_Safety_Switch/) · [11_Passive_Mode](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/11_Passive_Mode/) · [12_Active_Assist_Mode](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/12_Active_Assist_Mode/) · [17_FSM_Gait_Intent](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/17_FSM_Gait_Intent/)

A framework that lets you break complex control logic into discrete **states**, each with a three-phase lifecycle: Entry → Loop → Exit. You only need to register the functions to run at each phase — the TSM manages the lifecycle transitions for you.

> User code does not include this header directly. Include only the top-level facade header `xm_api.h` (it includes `xm_api_tsm.h` internally).

---

## When to Use

Use this when your `Control_Setup()` / `Control_Loop()` needs to handle multiple operating modes (e.g., STANDBY ↔ ACTIVE, or per-phase gait states) and you want to cleanly separate code that runs "once on entry," "every cycle while active," and "once on exit." Instead of managing state directly with `if`/`switch`, you just register `on_entry`/`on_loop`/`on_exit` functions per state. For the design rationale, operating principle, and common mistakes, read [01. Task State Machine](../01-task-state-machine.en.md) first — this page only covers exact function signatures and parameters.

---

## Function List

| Function | One-line description |
|----------|----------------------|
| [`XM_TSM_Create`](#xm_tsm_create) | Create a new TSM instance (with an initial state) |
| [`XM_TSM_AddState`](#xm_tsm_addstate) | Register one state (entry/loop/exit) with the TSM |
| [`XM_TSM_Run`](#xm_tsm_run) | Dispatch the TSM once — call every cycle |
| [`XM_TSM_TransitionTo`](#xm_tsm_transitionto) | Request a transition to another state |

---

## Function Details

### `XM_TSM_Create`

```c
XmTsmHandle_t XM_TSM_Create(uint8_t initial_state_id);
```

Creates a new Task State Machine instance. Use the returned handle for subsequent calls to `XM_TSM_AddState` / `XM_TSM_Run` / `XM_TSM_TransitionTo`.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `initial_state_id` | `uint8_t` | ID of the starting state. Pass one of the standard state values (`XmStateId_e`) directly, or a user-defined value starting at `XM_STATE_USER_START` (10) |

**Returns**: `XmTsmHandle_t` — the created TSM handle (`NULL` on failure). Pass this handle to every subsequent TSM function call.

**⚠️ Calling context**: Call once per state machine, in `Control_Setup()` (initialization time). Calling it from `Control_Loop()` or any repeatedly-invoked context re-creates the state machine every cycle and breaks the intended behavior. The header does not document any thread/ISR-safety guarantee, so treat it as unsafe to call from an ISR by default.

**Example**

```c
static XmTsmHandle_t s_tsm;

void Control_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);
    // ... followed by XM_TSM_AddState calls below
}
```

**See also**: [`XM_TSM_AddState`](#xm_tsm_addstate), [`XmStateId_e`](#xmstateid_e)

---

### `XM_TSM_AddState`

```c
void XM_TSM_AddState(XmTsmHandle_t handle, const XmStateConfig_t* config);
```

Registers one state's behavior (entry/loop/exit functions) with the TSM. All states you intend to use must be registered before entering `Control_Loop()`.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `handle` | `XmTsmHandle_t` | TSM handle obtained from `XM_TSM_Create` |
| `config` | `const XmStateConfig_t*` | Pointer to a config struct holding the state ID plus `on_entry`/`on_loop`/`on_exit` function pointers. `on_entry`/`on_exit` may be omitted (defaults to `NULL`) |

**Returns**: None (`void`)

**⚠️ Calling context**: Call in `Control_Setup()`, right after `XM_TSM_Create`, once per state you need to register. There is no need to call this every cycle inside `Control_Loop()` — registration only needs to happen once.

**Example**

```c
XmStateConfig_t sb_conf = {
    .id       = XM_STATE_STANDBY,
    .on_entry = Standby_Entry,
    .on_loop  = Standby_Loop,
    // .on_exit can be omitted if not needed (defaults to NULL)
};
XM_TSM_AddState(s_tsm, &sb_conf);
```

**See also**: [`XmStateConfig_t`](#xmstateconfig_t), [`XM_TSM_Create`](#xm_tsm_create)

---

### `XM_TSM_Run`

```c
void XM_TSM_Run(XmTsmHandle_t handle);
```

Dispatches the lifecycle function appropriate for the current state (entry once → loop repeatedly → exit once, on a requested transition). If states are registered but this function is never called, none of the state callbacks ever run.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `handle` | `XmTsmHandle_t` | TSM handle to run |

**Returns**: None (`void`)

**⚠️ Calling context**: **Must be called every cycle** inside `Control_Loop()`. The call rate becomes the effective execution rate of the active state's `on_loop` (typically a 1 kHz `Control_Loop`).

**Example**

```c
void Control_Loop(void)
{
    XM_TSM_Run(s_tsm);
}
```

**See also**: [`XM_TSM_TransitionTo`](#xm_tsm_transitionto)

---

### `XM_TSM_TransitionTo`

```c
void XM_TSM_TransitionTo(XmTsmHandle_t handle, uint8_t next_state_id);
```

Requests a transition to another state. The transition is not immediate — the current state's `on_exit` runs first, and the new state's `on_entry` runs on the **next `XM_TSM_Run` cycle**.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `handle` | `XmTsmHandle_t` | TSM handle |
| `next_state_id` | `uint8_t` | ID of the target state (must already be registered via `XM_TSM_AddState`) |

**Returns**: None (`void`)

**⚠️ Calling context**: Typically called from inside a state's `on_loop` (or `on_entry`) callback — i.e., inside the `Control_Loop()` context where `XM_TSM_Run` is being dispatched. "Deferred, not immediate, transition" is a behavior the header explicitly documents — if you need the effect right away, handle it with a separate flag.

**Example**

```c
static void Standby_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_LONG_PRESS) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}
```

**See also**: [`XM_TSM_Run`](#xm_tsm_run)

---

## Types / Macros

### `XmStateId_e`

The standard state IDs recommended by XM10.

```c
typedef enum {
    XM_STATE_OFF        = 0,
    XM_STATE_STANDBY    = 1,
    XM_STATE_ACTIVE     = 2,
    XM_STATE_ERROR      = 3,
    XM_STATE_USER_START = 10
} XmStateId_e;
```

| Value | Description |
|-------|--------------|
| `XM_STATE_OFF` = 0 | Initial state, stopped |
| `XM_STATE_STANDBY` = 1 | Standby (sensors on, output off) |
| `XM_STATE_ACTIVE` = 2 | Running (control algorithm active) |
| `XM_STATE_ERROR` = 3 | Error condition (safe mode) |
| `XM_STATE_USER_START` = 10 | Starting ID for user-defined states |

These values are a **recommended convention**, not a requirement — use them as-is, or freely define entirely different states starting from `XM_STATE_USER_START` (10). Just make sure user-defined values don't collide with the standard range (0–3).

### `XmLifecycle_t`

Represents the TSM lifecycle phase.

```c
typedef enum {
    XM_LIFECYCLE_ENTRY = 0,
    XM_LIFECYCLE_LOOP  = 1,
    XM_LIFECYCLE_EXIT  = 2
} XmLifecycle_t;
```

| Value | Description |
|-------|--------------|
| `XM_LIFECYCLE_ENTRY` = 0 | Entry (initialization) — runs once on state entry |
| `XM_LIFECYCLE_LOOP` = 1 | Loop (execution) — runs every cycle while in the state |
| `XM_LIFECYCLE_EXIT` = 2 | Exit (clean-up) — runs once on state exit |

This is the value carried in `XmTask_t`'s `currentStep` / `prevStep` fields. You rarely assign it yourself — it's mainly read for monitoring/debugging.

### `XmTask_t`

The TSM task object — the actual struct that `XmTsmHandle_t` points to.

```c
typedef struct {
    XmStateId_e   currentStateId;
    XmLifecycle_t currentStep;
    XmStateId_e   prevStateId;
    XmLifecycle_t prevStep;
} XmTask_t;
```

| Field | Type | Description |
|-------|------|--------------|
| `currentStateId` | `XmStateId_e` | Current state ID |
| `currentStep` | `XmLifecycle_t` | Current execution phase within the state (Entry/Loop/Exit) |
| `prevStateId` | `XmStateId_e` | Previous state ID |
| `prevStep` | `XmLifecycle_t` | Execution phase within the previous state |

Treat all fields as **read-only** — never assign to them directly to change state; always transition through [`XM_TSM_TransitionTo`](#xm_tsm_transitionto). Adding `*yourTask` to STM32CubeIDE's Live Expression window is a convenient way to monitor this in real time.

### `XmTsmHandle_t`

The handle type pointing to a TSM instance.

```c
typedef XmTask_t* XmTsmHandle_t;
```

This is the return value of `XM_TSM_Create`, and is used as the first argument of every subsequent TSM function. Declare one variable of this type (typically `static`) per state machine you manage.

### `XmStateConfig_t`

The configuration struct that defines a single state.

```c
typedef struct {
    XmStateId_e   id;
    XmStateFunc_t on_entry;
    XmStateFunc_t on_loop;
    XmStateFunc_t on_exit;
} XmStateConfig_t;
```

| Field | Type | Description |
|-------|------|--------------|
| `id` | `XmStateId_e` | The state ID this config applies to |
| `on_entry` | `XmStateFunc_t` | Function called once on entry. `NULL` if omitted (no-op) |
| `on_loop` | `XmStateFunc_t` | Function called every cycle while in the state. `NULL` if omitted |
| `on_exit` | `XmStateFunc_t` | Function called once on exit. `NULL` if omitted |

Fill in only the fields you need using designated initializers (`.field = value`), then pass the pointer to `XM_TSM_AddState`. None of the three callbacks are mandatory — thanks to C designated-initializer semantics, unfilled fields default to `NULL`.

### `XmStateFunc_t`

The state-callback function pointer type.

```c
typedef void (*XmStateFunc_t)(void);
```

This is the signature required for any function registered as `on_entry`/`on_loop`/`on_exit` — a `void func(void)` with no parameters and no return value. If you need to pass data between states, use `static` global/file-scope variables.

---

## Internal Only (Do Not Call)

No function or type in this header is marked `[Internal]` or system-only — all 4 functions and 6 types (2 structs + 2 enums + 2 function-pointer/handle typedefs) covered above are public API.

---

## Rev1.1 / Rev2.0 Differences

`xm_api_tsm.h` is byte-for-byte identical between Rev1.1 and Rev2.0. The TSM is a pure software framework that doesn't depend on any specific HW peripheral, so there is nothing that needs a Rev-specific marking.

---

## Related Documents

- [01. Task State Machine — concept doc](../01-task-state-machine.en.md) — lifecycle design rationale, common mistakes, example usage table
- [09. Auxiliary Tasks + Data Sharing](../09-task-creation.en.md) — for background work that needs to run outside the TSM
- [API Reference Overview](../README.en.md)
