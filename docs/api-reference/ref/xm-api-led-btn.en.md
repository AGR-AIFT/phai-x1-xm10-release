# `xm_api_led_btn.h` — Onboard LED · Button Control

> **Header**: `XM_FW/XM_API/xm_api_led_btn.h`
> **Related concept doc**: [03. LED + Button](../03-led-btn-control.en.md)
> **Related examples**: [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) · [01_Button_LED_Basic](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/01_Button_LED_Basic/) · [02_Button_LED_Event](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/02_Button_LED_Event/) · [03_Button_LED_FSM](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/03_Button_LED_FSM/) · [18_Debug_Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/)

This is the API for controlling the board's three onboard Function LEDs (`XM_LED_1~3`) and three Function Buttons (`XM_BTN_1~3`). On Rev2.0, it also includes control of the per-channel RGB LEDs (`XM_CH_LED_*`) that indicate sensor module connection status.

---

## When to Use

Use this API when you want to start/stop actions with a button press, or show the current status (idle/running/error, etc.) to the user through an LED. Both polling (check current state) and event-driven (detect click/long-press) approaches are supported — pick whichever fits your use case. It's recommended to read [03. LED + Button](../03-led-btn-control.en.md) first for how this all works together — this page only covers the exact signature/parameters of each function.

---

## Function List

| Function | One-line description |
|----------|----------------------|
| [`XM_SetLedState`](#xm_setledstate) | Simple LED On/Off |
| [`XM_SetLedEffect`](#xm_setledeffect) | Apply Blink/Heartbeat/Oneshot effect to an LED |
| [`XM_GetButtonState`](#xm_getbuttonstate) | Read a button's current physical press state (polling) |
| [`XM_GetButtonEvent`](#xm_getbuttonevent) | Read a button's latest event (Read-Clear) |
| [`XM_SetChannelLedRGB`](#xm_setchannelledrgb) 🟢 Rev 2.0 Only | Set a channel RGB LED's color directly |
| [`XM_IO_Update`](#xm_io_update) | Update internal LED/button state (engine) |

---

## Function Details

### `XM_SetLedState`

```c
void XM_SetLedState(XmLedId_t led_idx, XmState_t state);
```

Turns an LED on or off. This is the simplest control function — it applies the state immediately, with no effect (such as Blink).

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `led_idx` | `XmLedId_t` | LED to control (`XM_LED_1` ~ `XM_LED_3`) |
| `state` | `XmState_t` | Target state (`XM_ON`, `XM_OFF`) |

**Returns**: None (`void`)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context. Not designed to be called from an ISR.

**Example**

```c
XM_SetLedState(XM_LED_1, XM_ON);   // Turn LED 1 on
```

**See also**: [`XM_SetLedEffect`](#xm_setledeffect) — if an effect mode was previously set with `XM_SetLedEffect`, a subsequent `XM_SetLedState` call may be ignored or overwritten (see the Common Mistakes table in `03-led-btn-control.en.md`).

---

### `XM_SetLedEffect`

```c
void XM_SetLedEffect(XmLedId_t led_idx, XmLedMode_t mode, uint32_t period_ms);
```

Configures an effect — Blink, Heartbeat, or Oneshot — on an LED. This function only **stores the setting**; the actual timing calculation and On/Off toggling is performed periodically by [`XM_IO_Update()`](#xm_io_update).

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `led_idx` | `XmLedId_t` | LED to control (`XM_LED_1` ~ `XM_LED_3`) |
| `mode` | `XmLedMode_t` | Behavior mode (`XM_LED_OFF`/`SOLID`/`BLINK`/`HEARTBEAT`/`ONESHOT`) |
| `period_ms` | `uint32_t` | Effect period or duration in ms. `BLINK` = blink period, `ONESHOT` = on-time duration, `SOLID`/`OFF` = ignored (pass 0) |

**Returns**: None (`void`)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context. The effect may not appear immediately after this call; it is only reflected while [`XM_IO_Update()`](#xm_io_update) is being called periodically (this happens automatically inside the User Task, so no extra action is normally needed).

**Example**

```c
XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 500);       // Blink every 0.5 s
XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 1000);  // Heartbeat with a 1 s period
```

**See also**: [`XmLedMode_t`](#xmledmode_t) for the full list of modes

---

### `XM_GetButtonState`

```c
XmBtnState_t XM_GetButtonState(XmBtnId_t btn_idx);
```

Reads a button's current physical state immediately (polling approach). Regardless of whether the circuit is Active-High or Active-Low, the return value is always abstracted to a simple "is it pressed" answer.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `btn_idx` | `XmBtnId_t` | Button to read (`XM_BTN_1` ~ `XM_BTN_3`) |

**Returns**: `XmBtnState_t` — `XM_PRESSED` or `XM_RELEASED`

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context. Each call returns the real-time state, so it is safe to call from multiple places at once (this is not Read-Clear).

**Example**

```c
if (XM_GetButtonState(XM_BTN_1) == XM_PRESSED) {
    XM_SetLedState(XM_LED_1, XM_ON);
}
```

**See also**: [`XM_GetButtonEvent`](#xm_getbuttonevent) — use the event-driven approach if you need to detect an "action" such as a click or long-press.

---

### `XM_GetButtonEvent`

```c
XmBtnEvent_t XM_GetButtonEvent(XmBtnId_t btn_idx);
```

Retrieves the latest high-level event (click, long-press, etc.) generated by a button (event-driven approach).

> ⚠️ **Read-Clear semantics**: Once an event is read, it is immediately removed from the internal queue. If the same button's event is read from more than one place with `XM_GetButtonEvent`, whichever call runs first "consumes" the event, and every other call always receives `XM_BTN_NONE`. **Read each button's event from exactly one place in your code.** Store it in a variable once and branch on that variable, rather than calling it again in each `if/else` branch.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `btn_idx` | `XmBtnId_t` | Button to read (`XM_BTN_1` ~ `XM_BTN_3`) |

**Returns**: `XmBtnEvent_t` — the detected event (`XM_BTN_NONE`/`PRESSED`/`RELEASED`/`CLICK`/`LONG_PRESS`)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context.

**Example**

```c
XmBtnEvent_t evt = XM_GetButtonEvent(XM_BTN_1);  // Store once, then use it

switch (evt) {
    case XM_BTN_CLICK:
        XM_SendUsbDebugMessage("Button 1 Clicked!\r\n");
        break;
    case XM_BTN_LONG_PRESS:
        XM_SendUsbDebugMessage("Button 1 Long Pressed!\r\n");
        break;
    default:
        break;
}
```

**See also**: [`XmBtnEvent_t`](#xmbtnevent_t) for the full list of events; [`XM_GetButtonState`](#xm_getbuttonstate) — simpler if you only need raw state.

---

### `XM_SetChannelLedRGB` 🟢 Rev 2.0 Only

```c
void XM_SetChannelLedRGB(XmChannelLed_t ch, uint8_t r, uint8_t g, uint8_t b);
```

> 🟢 **Rev 2.0 only** — This function does not exist in the Rev1.1 header (no PCA9957 channel LED driver populated).

Sets the RGB color of a per-sensor-module channel LED directly, via the PCA9957 24-channel LED driver. Calling this **immediately overrides the automatic system status display** for that channel.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `ch` | `XmChannelLed_t` | Channel identifier (`XM_CH_LED_EMG`/`FES`/`IMU`/`HMMG`/`GRF_L`/`GRF_R`/`USB`) |
| `r` | `uint8_t` | Red PWM (0–255) |
| `g` | `uint8_t` | Green PWM (0–255) |
| `b` | `uint8_t` | Blue PWM (0–255) |

**Returns**: None (`void`)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context.

**Example**

```c
XM_SetChannelLedRGB(XM_CH_LED_IMU, 255, 0, 0);  // Set the IMU channel LED to red
XM_SetChannelLedRGB(XM_CH_LED_IMU, 0, 0, 0);    // r=g=b=0 -> restore automatic system control
```

**See also**: [`XmChannelLed_t`](#xmchannelled_t) for the channel list. Calling with `r=0,g=0,b=0` restores automatic system status display.

---

### `XM_IO_Update`

```c
void XM_IO_Update(void);
```

The internal engine function that computes LED blink timing and performs button debouncing/event detection.

**Parameters**: None

**Returns**: None (`void`)

**⚠️ Calling context**: Assume `Control_Setup()` / `Control_Loop()` context. `core_process` calls this automatically at a 1 ms (1 kHz) interval during its input-collection stage, so **you normally do not need to call it yourself.** However, if this function stops being called periodically (e.g., the User Task gets stuck in an infinite loop), Blink/Heartbeat/Oneshot effects and button events will stop updating entirely.

**Example**

```c
// Normally you do not need to call this directly — core_process calls it automatically
// For reference, a conceptual call site:
void Control_Loop(void)
{
    // Your algorithm ...
    XM_IO_Update();  // May be called at the end of the loop if needed
}
```

**See also**: [03. LED + Button §1. How It Works](../03-led-btn-control.en.md#1-how-it-works)

---

## Types / Macros

### `XmLedId_t`

| Value | Description |
|-------|--------------|
| `XM_LED_1` = 1 | Left LED |
| `XM_LED_2` = 2 | Center LED |
| `XM_LED_3` = 3 | Right LED |

### `XmBtnId_t`

| Value | Description |
|-------|--------------|
| `XM_BTN_1` = 1 | Left button |
| `XM_BTN_2` = 2 | Center button |
| `XM_BTN_3` = 3 | Right button |

### `XmState_t`

| Value | Description |
|-------|--------------|
| `XM_OFF` = 0 | Off (Logic Low) |
| `XM_ON` = 1 | On (Logic High) |

### `XmBtnState_t`

| Value | Description |
|-------|--------------|
| `XM_RELEASED` = 0 | Button is not pressed |
| `XM_PRESSED` = 1 | Button is pressed |

### `XmLedMode_t`

| Value | Description |
|-------|--------------|
| `XM_LED_OFF` = 0 | Turns the LED off |
| `XM_LED_SOLID` = 1 | Keeps the LED on continuously |
| `XM_LED_BLINK` = 2 | Blinks at a fixed period (50% duty) |
| `XM_LED_HEARTBEAT` = 3 | Flashes twice quickly, heartbeat-style |
| `XM_LED_ONESHOT` = 4 | Turns on for a set duration, then off automatically (for notifications) |

### `XmChannelLed_t` 🟢 Rev 2.0 Only

> Not defined in the Rev1.1 header.

| Value | Description |
|-------|--------------|
| `XM_CH_LED_EMG` = 0 | EMG module LED |
| `XM_CH_LED_FES` = 1 | FES module LED |
| `XM_CH_LED_IMU` = 2 | IMU module LED |
| `XM_CH_LED_HMMG` = 3 | HMMG module LED |
| `XM_CH_LED_GRF_L` = 4 | GRF left LED |
| `XM_CH_LED_GRF_R` = 5 | GRF right LED |
| `XM_CH_LED_USB` = 6 | USB LED |
| `XM_CH_LED_COUNT` = 7 | (internal use) channel count |

### `XmBtnEvent_t`

| Value | Description |
|-------|--------------|
| `XM_BTN_NONE` = 0 | No event detected |
| `XM_BTN_PRESSED` = 1 | Button was just pressed (rising edge) |
| `XM_BTN_RELEASED` = 2 | Button was just released (falling edge) |
| `XM_BTN_CLICK` = 3 | Short press and release (click) |
| `XM_BTN_LONG_PRESS` = 4 | Held for 1 second or more (long press) |

---

## Internal Only (Do Not Call)

None of the functions/types in this header are marked `[Internal]` or System-only — all 6 functions and 7 types covered above are part of the public API.

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| 3 onboard LEDs + 3 buttons, basic control | ✅ | ✅ |
| `XmChannelLed_t` / `XM_SetChannelLedRGB` (channel RGB LEDs) | ❌ Not present | 🟢 Only |

---

## Related Docs

- [03. LED + Button (concept)](../03-led-btn-control.en.md) — how it works, common mistakes, example mapping
- [Board Revision Comparison](../../hardware/README.en.md#board-revision-comparison) — button pin shift caveat between Rev1.1/Rev2.0
