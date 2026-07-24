# `xm_api.h` — XM10 SDK Entry Point (Umbrella API Header)

> **Target header**: [`XM_FW/XM_API/xm_api.h`](https://github.com/AGR-EXO/Extension_Module/tree/Develop/XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api.h) (Rev2.0 baseline — the same file also exists in Rev1.1; differences are covered in [§2](#2-what-this-header-aggregates))
> **Related concept docs**: [Full API reference index](../README.en.md) — all of docs 01–09 cover the sub-APIs this header aggregates
> **Related examples**: virtually every example — almost none of the repository examples skip including `xm_api.h`. The flagship example that calls `XM_GetTick()` directly is [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/)

---

## When to use this

Add this single line at the top of any XM10 example (`.c`) file, and you get access to every user-facing SDK API at once: robot control, sensor reads, LED/button, extension IO, USB communication/logging, memory, RTC, and auxiliary tasks.

```c
#include "xm_api.h"
```

`xm_api.h` itself implements no functionality — it is a **facade header**. It only aggregates the sub-API headers via `#include`, and directly declares one system-wide timer utility function, `XM_GetTick()`. For the actual usage of each domain (function lists, examples, caveats), follow the concept docs linked in the table below — this page does not re-explain any of those domains.

## What this header aggregates

`xm_api.h` includes the following sub-API headers. There are 9 on Rev2.0 and 8 on Rev1.1 (see the Notes column for the difference).

| # | Included header | One-line description | Concept doc |
|---|-----------|-----------|-----------|
| 1 | `xm_api_data.h` | Reads robot (KIT H10) sensor data + sends torque/position control commands | [02 KIT H10 Control & Data](../02-h10-control-n-data.en.md) |
| 2 | `xm_api_tsm.h` | Task State Machine (FSM) | [01 Task State Machine (TSM)](../01-task-state-machine.en.md) |
| 3 | `xm_api_led_btn.h` | Onboard LED and button control | [03 LED & Button](../03-led-btn-control.en.md) |
| 4 | `xm_api_external_io.h` | Extension port GPIO/ADC | [04 External IO](../04-external-io.en.md) |
| 5 | `xm_api_usb.h` | USB serial communication (CDC) | [05 USB Serial Communication](../05-usb-connectivity.en.md) |
| 6 | `xm_api_user_custom.h` | User custom slot (28B) inside the Total Data (0x20) packet — automatically included in PhAI Studio streaming/recording | No dedicated concept doc yet — see the Doxygen example (`@code` block) inside the header |
| 7 | `xm_api_memory.h` | Memory region access (Workspace/PSRAM/DTCM/non-volatile storage) | [07 Memory Regions](../07-memory-management.en.md) — 🟢 **Rev 2.0 only** |
| 8 | `xm_api_rtc.h` | RTC date/time management | [08 Real-Time Clock](../08-rtc-clock.en.md) — ⚠️ stub on Rev1.1 |
| 9 | `xm_api_freertos.h` | Auxiliary task creation + Mutex (student-friendly FreeRTOS wrapper) | [09 Auxiliary Tasks & Data Sharing](../09-task-creation.en.md) |

**Revision differences at a glance**:

- 🟢 **Rev 2.0 only**: `xm_api_memory.h` — this header file does not exist at all in the Rev1.1 SDK (i.e. `xm_api.h` does not include it there). It reflects Rev2.0-only hardware such as the 8MB Workspace/PSRAM.
- ⚠️ **Rev1.1 limitation**: `xm_api_rtc.h` exists as a file in both revisions, but Rev1.1 boards do not carry the RTC chip (MCP79510), so every function compiles down to a stub (always returns `false` / no-op). If you need timekeeping on Rev1.1, fall back to elapsed time based on `XM_GetTick()`.
- The other 7 headers (`xm_api_data/tsm/led_btn/external_io/usb/user_custom/freertos`) are identical in content across both revisions.

## Function list

`xm_api.h` itself directly declares exactly one public function (everything else is declared by the headers in §2 above).

| Function | One-line description |
|------|-----------|
| [`XM_GetTick`](#xm_gettick) | Gets the elapsed time (ms) since system boot |

## Function details

### `XM_GetTick`

```c
uint32_t XM_GetTick(void);
```

Returns the elapsed time, in milliseconds, since the system booted. Because it is a 32-bit counter, it rolls over to 0 approximately every 49.7 days (2³² ms).

**Parameters**

None.

**Return value**

| Type | Description |
|------|------|
| `uint32_t` | Elapsed time since boot (ms) |

⚠️ **Calling context**: the header comment does not state any specific task/ISR restriction. It is recommended to treat this conservatively as usable from the `Control_Setup()` / `Control_Loop()` context — every SDK example calls it only from within those two functions.

**Example** — rollover-safe elapsed-time calculation (the [Ex.00 Quick Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) pattern):

```c
static uint32_t s_boot_timer;

void Control_Setup(void)
{
    s_boot_timer = XM_GetTick();
}

void Control_Loop(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_boot_timer >= 500) {   /* unsigned subtraction -> rollover-safe */
        s_boot_timer = now;
        /* ... logic to run every 500ms ... */
    }
}
```

⚠️ Always write time comparisons as `(now - past) >= interval`. The form `now >= past + interval` can overflow in `past + interval` itself and misbehave near the rollover point.

**See also**:

- [09 Auxiliary Tasks & Data Sharing](../09-task-creation.en.md) — using tick values in periodic tasks
- [00_Quick_Start example](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/)

## Internal-only (do not call)

`xm_api.h` itself contains no symbols marked `[Internal]` or system-only. Internal-only functions of each aggregated sub-header are covered in that header's own concept doc (§2 table).

## See also

- [API reference index](../README.en.md)
- The 01–09 concept docs linked in the §2 table above
