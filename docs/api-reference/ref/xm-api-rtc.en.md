# `xm_api_rtc.h` — Real-Time Clock (RTC) API

> **Header**: `XM_FW/XM_API/xm_api_rtc.h`
> **Related concept docs**: [08. Real-Time Clock](../08-rtc-clock.en.md) · [06. USB Mass-Storage Logging](../06-usb-data-logging.en.md) (RTC timestamp integration)
> **Related examples**: [34_MSC_GaitAnalysis_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) — builds a session folder name from the RTC, with an automatic fallback to a tick-based name when the RTC is absent or unset

> 🟢 **Rev 2.0 only (hardware-dependent)** — The RTC chip (MCP79510) is only populated on the XM10 **Rev2.0** board. The `xm_api_rtc.h` header itself is included identically in both the Rev1.1 and Rev2.0 SDKs, but **on a Rev1.1 board all three functions below run as stubs** — they always return `false` and do nothing. On Rev1.1, use a `XM_GetTick()`-based timer instead.

---

## When to use it

Use this API when you want a real date/time stamped into a log file or session folder name, or when you want accurate file timestamps for USB mass-storage logging. Check the RTC concept and common pitfalls (battery drain, weekday not auto-computed, etc.) first in [08. Real-Time Clock](../08-rtc-clock.en.md). This page only covers the detailed specification of the functions and types declared in `xm_api_rtc.h`.

If you start USB data logging without setting the RTC, file timestamps get stamped with a default value (2025-01-01). The standard pattern is to check `XM_RTC_IsRunning()` once in `Control_Setup()` and call `XM_RTC_SetDateTime()` if needed (see [06. USB Mass-Storage Logging — Caveats](../06-usb-data-logging.en.md) for details).

## Function list

| Function | Description |
|------|------|
| [`XM_RTC_SetDateTime`](#xm_rtc_setdatetime) | Sets the date/time on the RTC |
| [`XM_RTC_GetDateTime`](#xm_rtc_getdatetime) | Reads the current date/time from the RTC |
| [`XM_RTC_IsRunning`](#xm_rtc_isrunning) | Checks whether the RTC oscillator is running |

## Function details

### `XM_RTC_SetDateTime`

```c
bool XM_RTC_SetDateTime(const XmDateTime_t* dt);
```

Sets the date/time on the RTC. Internally converts the 4-digit year (2000–2099) to the 2-digit year that the MCP79510 actually stores.

| Parameter | Type | Description |
|----------|------|------|
| `dt` | `const XmDateTime_t*` | The date/time to set. `year` must be in the 2000–2099 range |

**Return value**

| Value | Meaning |
|----|------|
| `true` | Set succeeded |
| `false` | Failed — `year` out of range, or an SPI communication error. **Always `false` on Rev1.1** |

> ⚠️ **Call context**: assume `Control_Setup()`/`Control_Loop()` context. This talks to the RTC chip over SPI, so do not call it from an ISR. The typical pattern checks [`XM_RTC_IsRunning()`](#xm_rtc_isrunning) once right after boot in `Control_Setup()`, and only calls this once if the battery has drained.

**Example**

```c
XmDateTime_t dt = {
    .year = 2026, .month = 3, .day = 2,
    .weekday = 1,  // Monday (1=Mon ... 7=Sun)
    .hour = 14, .minute = 30, .second = 0
};
XM_RTC_SetDateTime(&dt);
```

**See also**: [`XM_RTC_IsRunning`](#xm_rtc_isrunning) · [08. Real-Time Clock](../08-rtc-clock.en.md)

---

### `XM_RTC_GetDateTime`

```c
bool XM_RTC_GetDateTime(XmDateTime_t* dt);
```

Reads the current date/time from the RTC into `dt`.

| Parameter | Type | Description |
|----------|------|------|
| `dt` | `XmDateTime_t*` | Output buffer that receives the date/time read |

**Return value**

| Value | Meaning |
|----|------|
| `true` | Read succeeded |
| `false` | Failed — SPI communication error. **Always `false` on Rev1.1, and `dt` is left unmodified** |

> ⚠️ **Call context**: assume `Control_Setup()`/`Control_Loop()` context. SPI communication costs on the order of milliseconds, so don't call this on every 1 kHz `Control_Loop()` cycle — only call it when you actually need it (e.g. generating a log filename, at sub-second frequency at most).

**Example — a real pattern that always tolerates failure (Rev1.1 / unset RTC)**

This is the actual pattern used by the [`34_MSC_GaitAnalysis_Log`](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) example. When `GetDateTime` returns `false` or `year` is out of a sane range (i.e. Rev1.1 stub or RTC never set), it automatically falls back to a tick-based name.

```c
static void _GenerateSessionName(char* buf, uint32_t buf_size)
{
    XmDateTime_t rtc = {0};
    XM_RTC_GetDateTime(&rtc);
    if (rtc.year >= 2020 && rtc.year <= 2099) {
        snprintf(buf, buf_size, "%02u%02u%02u_%02u%02u%02u",
                 (unsigned)(rtc.year % 100), (unsigned)rtc.month, (unsigned)rtc.day,
                 (unsigned)rtc.hour, (unsigned)rtc.minute, (unsigned)rtc.second);
    } else {
        /* RTC unset / Rev1.1 stub — fall back to boot tick count */
        snprintf(buf, buf_size, "XM_%08lu", (unsigned long)osKernelGetTickCount());
    }
}
```

**See also**: [`XM_RTC_SetDateTime`](#xm_rtc_setdatetime) · [06. USB Mass-Storage Logging](../06-usb-data-logging.en.md)

---

### `XM_RTC_IsRunning`

```c
bool XM_RTC_IsRunning(void);
```

Checks whether the RTC oscillator is running. If the backup battery (CR1220 coin cell) has drained, the oscillator stops and this returns `false`.

This function takes no parameters.

**Return value**

| Value | Meaning |
|----|------|
| `true` | Running |
| `false` | Stopped — e.g. battery drained. **Always `false` on Rev1.1** |

> ⚠️ **Call context**: assume `Control_Setup()`/`Control_Loop()` context. Typically called once at the start of `Control_Setup()` as a guard: if it returns `false`, re-set the time with `XM_RTC_SetDateTime()`.

**Example**

```c
void Control_Setup(void)
{
    if (!XM_RTC_IsRunning()) {
        XmDateTime_t dt = { .year = 2026, .month = 4, .day = 3, .weekday = 5,
                             .hour = 14, .minute = 30, .second = 0 };
        XM_RTC_SetDateTime(&dt);
    }
}
```

**See also**: [`XM_RTC_SetDateTime`](#xm_rtc_setdatetime)

---

## Types / macros

### `XmDateTime_t`

```c
typedef struct {
    uint16_t year;      // Year (2000-2099)
    uint8_t  month;     // Month (1-12)
    uint8_t  day;       // Day (1-31)
    uint8_t  weekday;   // Weekday (1=Mon ... 7=Sun)
    uint8_t  hour;      // Hour (0-23)
    uint8_t  minute;    // Minute (0-59)
    uint8_t  second;    // Second (0-59)
} XmDateTime_t;
```

| Field | Type | Range | Description |
|------|------|------|------|
| `year` | `uint16_t` | 2000–2099 | 4-digit year. Converted to/from the MCP79510's 2-digit year internally by `XM_RTC_SetDateTime`/`XM_RTC_GetDateTime` |
| `month` | `uint8_t` | 1–12 | Month |
| `day` | `uint8_t` | 1–31 | Day |
| `weekday` | `uint8_t` | 1–7 | Weekday, ISO 8601 style (1=Monday … 7=Sunday). The MCP79510 does not auto-compute this from the date, so you must always fill it in yourself |
| `hour` | `uint8_t` | 0–23 | Hour (24-hour clock) |
| `minute` | `uint8_t` | 0–59 | Minute |
| `second` | `uint8_t` | 0–59 | Second |

> The header defines no other macros or enums besides this struct.

## Internal-only (do not call)

`xm_api_rtc.h` has no symbols marked `[Internal]` / system-only. All 3 declared functions (`XM_RTC_SetDateTime`, `XM_RTC_GetDateTime`, `XM_RTC_IsRunning`) and the 1 declared type (`XmDateTime_t`) are public API.

---

## Rev1.1 / Rev2.0 summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| Header file (`xm_api_rtc.h`) | Included (byte-identical to Rev2.0) | Included |
| RTC hardware (MCP79510) | Not populated | Populated |
| Actual behavior of the 3 functions | Stub — always `false`, no-op | Works normally |
| Alternative when time is needed | `XM_GetTick()`-based elapsed time | Use the RTC directly |

> See also: [Board revision comparison](../../hardware/README.md#보드-리비전-비교)
