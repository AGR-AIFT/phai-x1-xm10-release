# API Reference: Task State Machine (TSM)

> 📌 **After reading this page**: You will be able to write state-based control algorithms using the TSM.
> ⏱️ Estimated reading time: 15 minutes
> 🧰 Prerequisites: Single-state TSM from [Ex.00 Quick Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/)
> 🎯 Key functions: `XM_TSM_Create` / `XM_TSM_AddState` / `XM_TSM_Run` / `XM_TSM_TransitionTo`

The task state machine API is defined in `xm_api_tsm.h`. It lets you break complex control logic into discrete **states**, making your algorithm straightforward to implement and reason about.

---

## 📌 Overview

The XM10 TSM automatically manages a three-phase lifecycle for each state: **Entry → Loop → Exit**. You only need to register the functions to run at each phase.

## 🛠 Data Structures

### `XmStateConfig_t`

A configuration struct that defines a single state.

```c
typedef struct {
    XmStateId_e id;          // State ID (e.g., XM_STATE_STANDBY)
    XmStateFunc_t on_entry;  // [Entry] Called once when entering the state
    XmStateFunc_t on_loop;   // [Loop]  Called every cycle while in the state
    XmStateFunc_t on_exit;   // [Exit]  Called once when leaving the state
} XmStateConfig_t;
```

### `XmStateId_e` (Standard States)

Recommended standard state IDs. (You may define your own starting from ID 10.)

  * `XM_STATE_OFF` (0): Initial stopped state
  * `XM_STATE_STANDBY` (1): Standby state
  * `XM_STATE_ACTIVE` (2): Running / active
  * `XM_STATE_ERROR` (3): Error condition
  * ...
  * `XM_STATE_USER_START` (10): Starting ID for user-defined states

### `XmTsmHandle_t`

A unique handle pointing to a created state machine task. You control a specific state machine through this handle. You must declare `XmTsmHandle_t yourTask` before use.

The handle lets you inspect the current state, the current execution phase within that state, and the previous state and its execution phase through `yourTask` in the STM32CubeIDE Live Expression window.

```c
// The handle is a pointer to the state machine task (XmTask_t).
typedef XmTask_t* XmTsmHandle_t;
```

> The `XmTask_t` struct that the handle points to contains monitoring fields (`currentStateId`, `currentStep`, `prevStateId`, `prevStep`). You can inspect current and previous states in STM32CubeIDE's Live Expression view using `*yourTask`.

---

## 📚 Functions

### `XM_TSM_Create`

Creates a new state machine instance.

  * **Parameters**
      * `uint8_t initial_state_id`: ID of the initial state
  * **Return**: `XmTsmHandle_t` (the created handle)

### `XM_TSM_AddState`

Registers a new state and its behavior with the TSM.

  * **Parameters**
      * `XmTsmHandle_t handle`: TSM handle
      * `const XmStateConfig_t* config`: Pointer to the state configuration struct
  * **Example**
    ```c
    XmStateConfig_t conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop
        // .on_exit can be omitted (handled as NULL automatically)
    };
    XM_TSM_AddState(handle, &conf);
    ```

### `XM_TSM_Run`

Runs the TSM. **Must be called inside the infinite loop of your User Task every cycle.**

  * **Parameters**
      * `XmTsmHandle_t handle`: TSM handle to run

### `XM_TSM_TransitionTo`

Requests a transition to another state. The transition is not immediate — the current state's `Exit` function runs first, and the transition takes effect on the next cycle.

  * **Parameters**
      * `XmTsmHandle_t handle`: TSM handle
      * `uint8_t next_state_id`: ID of the target state

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| State registered but callbacks never called | `XM_TSM_Run(handle)` missing from `Control_Loop` | `XM_TSM_Run` must be called every cycle to dispatch callbacks |
| `TransitionTo` called but state doesn't change immediately | Intended behavior — transition happens on the next cycle via `Exit → Entry` | This is normal. If you need an immediate effect, use a separate flag |
| `Entry` is called every cycle | Calling `Entry()` directly in user code (the TSM already calls it automatically) | `Entry` is called only once per `TransitionTo`. Do not call it directly |
| State ID conflict (two states share the same ID) | `AddState` called twice with the same `id` | Use standard IDs (`XM_STATE_*`) and user-defined IDs starting from `XM_STATE_USER_START` (10) to avoid conflicts |
| Nothing happens when starting from `XM_STATE_OFF` | `on_loop` not registered for `XM_STATE_OFF`, or intentional idle state | Define `on_loop` for the first state, or add a `TransitionTo` call to move out of it |

---

## Related Examples

| Example | Difficulty | TSM Usage |
|---------|------------|-----------|
| [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) | Beginner | Single-state TSM |
| [01_Button_LED_Basic](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/01_Button_LED_Basic/) | Beginner | Single-state basics |
| [03_Button_LED_FSM](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/03_Button_LED_FSM/) | Intermediate | STANDBY ↔ ACTIVE transition |
| [06_Ext_IO_Safety_Switch](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/06_Ext_IO_Safety_Switch/) | Intermediate | 3-state + ERROR handling |
| [11_Passive_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/) | Advanced | Homing + mode transition |
| [12_Active_Assist_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) | Advanced | Hierarchical FSM |
| [17_FSM_Gait_Intent](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/17_FSM_Gait_Intent/) | Advanced | 7-phase gait FSM |
