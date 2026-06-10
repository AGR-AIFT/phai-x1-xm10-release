# API 08: RTC (Real-Time Clock)

> 📌 **What you'll learn**: How to read and write date/time using the external RTC (MCP79510), and how to detect a dead backup battery.
> ⏱️ Estimated reading time: 10 minutes
> 🧰 Prerequisites: None (standalone API)
> 🎯 Key functions: `XM_RTC_SetDateTime` / `XM_RTC_GetDateTime` / `XM_RTC_IsRunning`

> **Header file**: `xm_api_rtc.h`

Date/time management API for the MCP79510 RTC. Internally converts the 2-digit year (0–99) stored on the chip to a 4-digit year (2000–2099).

---

## Types

### `XmDateTime_t`

```c
typedef struct {
    uint16_t year;      // Year (2000–2099)
    uint8_t  month;     // Month (1–12)
    uint8_t  day;       // Day (1–31)
    uint8_t  weekday;   // Day of week (1=Mon … 7=Sun)
    uint8_t  hour;      // Hour (0–23)
    uint8_t  minute;    // Minute (0–59)
    uint8_t  second;    // Second (0–59)
} XmDateTime_t;
```

---

## API Functions

### `XM_RTC_SetDateTime()`

```c
bool XM_RTC_SetDateTime(const XmDateTime_t* dt);
```

Sets the date and time on the RTC.

| Parameter | Description |
|-----------|-------------|
| `dt` | Date/time to set (year: 2000–2099) |

| Return value | Meaning |
|--------------|---------|
| `true` | Success |
| `false` | Failure (value out of range or SPI error) |

### `XM_RTC_GetDateTime()`

```c
bool XM_RTC_GetDateTime(XmDateTime_t* dt);
```

Reads the current date and time from the RTC.

| Parameter | Description |
|-----------|-------------|
| `dt` | Pointer to the structure that receives the date/time |

| Return value | Meaning |
|--------------|---------|
| `true` | Success |
| `false` | Failure (SPI error) |

### `XM_RTC_IsRunning()`

```c
bool XM_RTC_IsRunning(void);
```

Checks whether the RTC oscillator is running.

| Return value | Meaning |
|--------------|---------|
| `true` | Running |
| `false` | Stopped (e.g., dead backup battery) |

---

## Usage Example

```c
void Control_Setup(void) {
    // Check whether the RTC is running and set the time if it isn't
    if (!XM_RTC_IsRunning()) {
        XmDateTime_t dt = {
            .year = 2026, .month = 4, .day = 3,
            .weekday = 5,  // Friday
            .hour = 14, .minute = 30, .second = 0
        };
        XM_RTC_SetDateTime(&dt);
    }

    // Read the current time
    XmDateTime_t now;
    if (XM_RTC_GetDateTime(&now)) {
        // Use now.year, now.month, now.day, now.hour, now.minute, now.second
    }
}
```

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `XM_RTC_GetDateTime` always returns false | SPI communication failure (bad wiring or missing CS) | Check hardware connections and verify operation with `XM_RTC_IsRunning()` |
| `XM_RTC_IsRunning` returns false | RTC backup battery is dead (coin cell CR1220) | Replace the battery and reset the time with `XM_RTC_SetDateTime` |
| `year = 1970` or 2000 | Time was never set after the first power-on | Check `IsRunning` in `Control_Setup`, then call `SetDateTime` |
| `year = 2100` or other out-of-range value | Invalid value passed to `SetDateTime` | `year` must be in the range 2000–2099 (MCP79510 2-digit limit) |
| Calling `GetDateTime` every cycle makes the loop slow | SPI communication has millisecond-level overhead | Call it only once per second and cache the timestamp |
| `weekday` value is incorrect | Unfamiliarity with the 1=Monday … 7=Sunday convention | Follow ISO 8601: `1=Mon … 7=Sun` |
| Time resets after power cycle | Backup battery not installed or dead | Check the battery — typical lifetime is about 5 years |
| `weekday` is not calculated automatically | The MCP79510 requires the weekday to be supplied by the user | Pre-calculate it using Zeller's formula or `<time.h>` |

---

## Related Examples

| Example | How it uses the RTC |
|---------|---------------------|
| [10_MSC_Manual_log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10_MSC_Manual_log/) | Uses a timestamp in the log filename (e.g., `/LOGS/20260512_143000.bin`) |
| [10c_MSC_Advanced_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10c_MSC_Advanced_Log/) | Records start and end times in `summary.txt` |
