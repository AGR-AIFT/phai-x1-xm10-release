# `xm_api_usb_msc.h` — USB Memory Logging

> 🟢 **Rev 2.0 only** — This entire header ships only in the XM10 Rev 2.0 SDK. The Rev 1.1 SDK (`XM10_SDK/Rev1.1/`) does not contain `xm_api_usb_msc.h` at all, so calling any of the functions below from a Rev 1.1 project will **fail to build**. To check your board revision, see [Board Revision Comparison](../../hardware/README.en.md#board-revision-comparison).
>
> - **Header**: `XM_FW/XM_API/xm_api_usb_msc.h` (you get it together with `xm_api.h`)
> - **Related concept doc**: [06. USB Memory Logging](../06-usb-data-logging.en.md)
> - **Step by step**: [Save Data to a USB Memory Stick](../../getting-started/05-usb-memory-log.en.md)
> - **Related examples**: [10_USB_Memory_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/) · [34_USB_Memory_Gait_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/)

This header defines the API for **saving one struct to a USB memory stick at a fixed rate**. Register a struct, and while logging is running the system writes that struct to the stick at the rate you choose. You do not call a write function.

---

## When to use it

Use it when you want to save values to a USB memory stick without a PC connected, and decode them later on a PC with XM10 Log Decoder. For the big picture — order of use, preparing the stick, the USB-C port rule — we recommend reading [06. USB Memory Logging](../06-usb-data-logging.en.md) first. This page only covers the signatures and parameters of **every function, type and constant** declared in the header.

> ⚠️ **There is only one USB-C port.** While a USB memory stick is plugged in, you cannot use PhAI Studio, the `xm10` tool, or firmware upload over USB. When logging is done, remove the stick and connect the PC. (not yet confirmed on the board) <!-- board-check -->

---

## Calling rules

| Kind | Functions | Where to call |
|------|------|-----------|
| Setup · start · stop · marker · statistics (8) | `XM_SetUsbLogSource` · `XM_SetUsbLogSampleRateHz` · `XM_SetUsbLogAutoTimestamp` · `XM_SetUsbLogRollingSize` · `XM_StartUsbDataLog` · `XM_StopUsbDataLog` · `XM_InsertUsbLogMarker` · `XM_GetUsbLogStats` | Only from `Control_Setup()` and `Control_Loop()`. Called from a background task (`xm_api_freertos.h`) or an interrupt, they do nothing and return `false` |
| Status (6) | `XM_IsUsbLogReady` · `XM_IsUsbLogSessionIdle` · `XM_IsUsbLogRecording` · `XM_GetUsbLogLastStartError` · `XM_GetUsbDiskFreeMB` · `XM_GetUsbDiskTotalMB` | Anywhere |

---

## Function list

**Setup** (`Control_Setup()`)

| Function | One-line description |
|------|-----------|
| [`XM_SetUsbLogSource`](#xm_setusblogsource) | Registers the struct to save |
| [`XM_SetUsbLogSampleRateHz`](#xm_setusblogsampleratehz) | Sets the logging rate (default 1000 Hz) |
| [`XM_SetUsbLogAutoTimestamp`](#xm_setusblogautotimestamp) | Sets whether a time is saved with every record (default on) |
| [`XM_SetUsbLogRollingSize`](#xm_setusblogrollingsize) | Sets the file-split size (default 10 MB) |

**Status** (anywhere)

| Function | One-line description |
|------|-----------|
| [`XM_IsUsbLogReady`](#xm_isusblogready) | Whether a stick is plugged in and ready to write |
| [`XM_IsUsbLogSessionIdle`](#xm_isusblogsessionidle) | Whether there is no logging session at all |
| [`XM_IsUsbLogRecording`](#xm_isusblogrecording) | Whether a file is being written right now |
| [`XM_GetUsbLogLastStartError`](#xm_getusbloglaststarterror) | The result of the last start |
| [`XM_GetUsbDiskFreeMB`](#xm_getusbdiskfreemb) | Free space on the stick [MB] |
| [`XM_GetUsbDiskTotalMB`](#xm_getusbdisktotalmb) | Total size of the stick [MB] |

**Control** (`Control_Loop()`)

| Function | One-line description |
|------|-----------|
| [`XM_StartUsbDataLog`](#xm_startusbdatalog) | Starts logging |
| [`XM_StopUsbDataLog`](#xm_stopusbdatalog) | Stops logging |
| [`XM_InsertUsbLogMarker`](#xm_insertusblogmarker) | Inserts a marker while logging |
| [`XM_GetUsbLogStats`](#xm_getusblogstats) | Reads the logging statistics |

**Types · constants**: [`XmUsbLogStartError_e`](#xmusblogstarterror_e) · [`XmUsbLogStats_t`](#xmusblogstats_t) · [3 constants](#constants)

---

## State of a logging session

Read the state of a logging session from the combination of `XM_IsUsbLogRecording()` and `XM_IsUsbLogSessionIdle()`.

| State | `XM_IsUsbLogRecording()` | `XM_IsUsbLogSessionIdle()` | Description |
|------|:---:|:---:|------|
| No session | `false` | `true` | You can start a new one or change settings |
| Starting | `false` | `false` | After `XM_StartUsbDataLog()` returned `true` and before the file opens. `XM_GetUsbLogStats()` returns 0 for every field now |
| Recording | `true` | `false` | |
| Finishing | `false` | `false` | After a stop, or after logging ended by itself — the remaining data is being written and the file closed |

---

## Function details

### Setup (`Control_Setup()`)

#### `XM_SetUsbLogSource`

```c
bool XM_SetUsbLogSource(const void* data, uint32_t size);
```

Registers the struct to save. The struct is not copied — its address is used as is, so it must stay valid as long as the program runs. When there is no logging session, you can call it again to change the registration.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `data` | `const void*` | Address of the struct — a global or `static` variable |
| `size` | `uint32_t` | Struct size [B], 1 – `XM_USB_LOG_MAX_STRUCT_SIZE` (512) |

**Return value**: `bool` — `true`: registered. `false`: a logging session exists or an argument is invalid — the previous registration is kept.

**⚠️ Call context**: Call it once in `Control_Setup()`.

**Example**

```c
static MyLog_t s_log;     /* a packed struct */

void Control_Setup(void) {
    XM_SetUsbLogSource(&s_log, sizeof(s_log));
}
```

**See also**: [`XM_StartUsbDataLog`](#xm_startusbdatalog), [Meta line](#meta-line)

---

#### `XM_SetUsbLogSampleRateHz`

```c
bool XM_SetUsbLogSampleRateHz(uint32_t hz);
```

Sets the logging rate. If you do not set one, it is 1000 Hz. Below 1000 Hz, one sample is saved per period and the values in between are not saved.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `hz` | `uint32_t` | A divisor of 1000: 1000 · 500 · 250 · 200 · 125 · 100 · 50 · 40 · 25 · 20 · 10 · 8 · 5 · 4 · 2 · 1 |

**Return value**: `bool` — `true`: takes effect from the next start. `false`: a logging session exists, or the value is not a divisor of 1000 (0 included) — the previous value is kept.

**⚠️ Call context**: Call it once in `Control_Setup()`.

**See also**: If the struct is too big for the rate, logging does not start (`XM_USB_LOG_START_TOO_BIG_FOR_RATE`). With the timestamp on, a struct of up to 134 B can start at 1000 Hz, and up to 512 B at 200 Hz or lower. When you log at 1000 Hz, some samples may be missing.

---

#### `XM_SetUsbLogAutoTimestamp`

```c
bool XM_SetUsbLogAutoTimestamp(bool enable);
```

Sets whether a time (ms) is saved with every record. The default is on. It is a time counted by the system, not the date and time of the clock (RTC).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `enable` | `bool` | `true`: save the time with every record (default). `false`: do not save it |

**Return value**: `bool` — `true`: takes effect from the next start. `false`: a logging session exists — the previous value is kept.

**⚠️ Call context**: Call it in `Control_Setup()`.

**See also**: Keep it on when you log below 1000 Hz. If it is off, the start is rejected with `XM_USB_LOG_START_NEEDS_TIMESTAMP`.

---

#### `XM_SetUsbLogRollingSize`

```c
bool XM_SetUsbLogRollingSize(uint32_t size_mb);
```

Sets the file-split size. The default is 10 MB. When a file passes this size, a new part (`data_NNN_part_PPP.bin`) starts. (not yet confirmed on the board) <!-- board-check -->

**Parameters**

| Name | Type | Description |
|------|------|------|
| `size_mb` | `uint32_t` | 1 – 100 [MB]. The free space on the stick must be larger than this value for logging to start |

**Return value**: `bool` — `true`: takes effect from the next start. `false`: a logging session exists or the value is out of range — the previous value is kept.

**⚠️ Call context**: Call it in `Control_Setup()`.

---

### Status (anywhere)

#### `XM_IsUsbLogReady`

```c
bool XM_IsUsbLogReady(void);
```

Returns whether a USB memory stick is plugged in and ready for writing files (FAT32). exFAT and NTFS are not recognized, so it stays `false` for them. It can be `false` for a moment just after you plug the stick in.

**Parameters**: none

**Return value**: `bool` — `true`: a stick is plugged in and can be written.

**⚠️ Call context**: Can be called from anywhere.

**See also**: [`XM_StartUsbDataLog`](#xm_startusbdatalog)

---

#### `XM_IsUsbLogSessionIdle`

```c
bool XM_IsUsbLogSessionIdle(void);
```

Returns whether there is no logging session at all — not recording, not starting, and not closing a file. When it is `true`, you can start a new session or change settings, and you may remove the USB memory stick. (not yet confirmed on the board) <!-- board-check -->

If this value becomes `true` by itself while logging, the session has ended (the stick was pulled out, the internal buffer overflowed, and so on).

**Parameters**: none

**Return value**: `bool` — `true`: there is no logging session. `false`: recording, starting, or closing a file.

**⚠️ Call context**: Can be called from anywhere. A value read outside `Control_Setup()` / `Control_Loop()` is for reference only — do not use it to decide on a start or a settings change.

**See also**: [State of a logging session](#state-of-a-logging-session), [`XM_StopUsbDataLog`](#xm_stopusbdatalog)

---

#### `XM_IsUsbLogRecording`

```c
bool XM_IsUsbLogRecording(void);
```

Returns whether a file is being written right now. It becomes `true` when the file opens after a start request, and becomes `false` right after you call `XM_StopUsbDataLog()` (the file may still be closing).

**Parameters**: none

**Return value**: `bool` — `true`: a file is being written.

**⚠️ Call context**: Can be called from anywhere.

**See also**: To know whether all files are closed, check [`XM_IsUsbLogSessionIdle`](#xm_isusblogsessionidle), not this value.

---

#### `XM_GetUsbLogLastStartError`

```c
XmUsbLogStartError_e XM_GetUsbLogLastStartError(void);
```

The result of the last `XM_StartUsbDataLog()`. It keeps the reason a start request was rejected right away, or the reason opening the file failed after the request was accepted. When a start request is accepted, it is cleared to `XM_USB_LOG_START_OK`.

**Parameters**: none

**Return value**: [`XmUsbLogStartError_e`](#xmusblogstarterror_e)

**⚠️ Call context**: Can be called from anywhere. Read the result of an accepted start after `XM_IsUsbLogRecording()` becomes `true` or `XM_IsUsbLogSessionIdle()` becomes `true` again, and before you call `XM_StartUsbDataLog()` again.

**Example**

```c
if (!XM_StartUsbDataLog("walk01", LOG_METADATA)) {
    XmUsbLogStartError_e why = XM_GetUsbLogLastStartError();   /* look at the value in the debugger */
    (void)why;
}
```

---

#### `XM_GetUsbDiskFreeMB`

```c
uint32_t XM_GetUsbDiskFreeMB(void);
```

Returns the free space on the USB memory stick [MB]. It is refreshed about every 10 seconds, and reads 0 for a moment just after you plug the stick in. It is 0 when no stick is present.

**Parameters**: none

**Return value**: `uint32_t` — free space [MB]

**⚠️ Call context**: Can be called from anywhere.

---

#### `XM_GetUsbDiskTotalMB`

```c
uint32_t XM_GetUsbDiskTotalMB(void);
```

Returns the total size of the USB memory stick [MB]. It reads 0 for a moment just after you plug the stick in, and 0 when no stick is present.

**Parameters**: none

**Return value**: `uint32_t` — total size [MB]

**⚠️ Call context**: Can be called from anywhere.

---

### Control (`Control_Loop()`)

#### `XM_StartUsbDataLog`

```c
bool XM_StartUsbDataLog(const char* session_name, const char* metadata);
```

Starts logging. It returns immediately without waiting. The two strings are copied at the start.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `session_name` | `const char*` | Session folder name (at most `XM_USB_LOG_MAX_SESSION_NAME` characters). `NULL` or `""` makes `YYMMDD_HHMMSS` from the clock. If a folder with the same name exists, logging continues in it — the rate, struct size and timestamp setting must be the same. The folder keeps the meta line from the first run and does not compare it with the new one, so if you changed a field name or type, use a different name |
| `metadata` | `const char*` | The [meta line](#meta-line) (`NULL` gives `XM_USB_LOG_START_BAD_ARG`) |

Characters not allowed in a session name: `<` `>` `:` `"` `/` `\` `?` `*` and the vertical-bar character.

**Return value**: `bool`

- `true`: the start request was accepted. When the file opens, `XM_IsUsbLogRecording()` becomes `true`. If opening the file fails, `XM_IsUsbLogSessionIdle()` becomes `true` again and the reason is left in `XM_GetUsbLogLastStartError()`.
- `false`: logging did not start — read the reason with `XM_GetUsbLogLastStartError()`. (Called outside `Control_Setup()` / `Control_Loop()`, it returns `false` without leaving a reason.)

**⚠️ Call context**: Call it in `Control_Loop()`. Call it when both `XM_IsUsbLogReady()` and `XM_IsUsbLogSessionIdle()` are `true`.

**Example**

```c
if (XM_IsUsbLogReady() && XM_IsUsbLogSessionIdle()) {
    XM_StartUsbDataLog("", LOG_METADATA);   /* "" makes the folder name from the clock */
}
```

**See also**: [`XmUsbLogStartError_e`](#xmusblogstarterror_e), [`XM_StopUsbDataLog`](#xm_stopusbdatalog)

---

#### `XM_StopUsbDataLog`

```c
bool XM_StopUsbDataLog(void);
```

Stops logging. It returns immediately without waiting, and the files are closed in the background.

**Parameters**: none

**Return value**: `bool` — `true`: a stop was requested, or there is no session to stop. `false`: busy for a moment — call it again shortly.

**⚠️ Call context**: Call it in `Control_Loop()`. While you wait for `XM_IsUsbLogSessionIdle()` to become `true`, you may call it again.

- If you call it right after requesting a start, before the system has handled that start, the start is canceled (`XM_USB_LOG_START_CANCELED`, no file is created).
- If the file was already being created, it is closed right after it is created, so a session folder with no records is left behind (the reason is `XM_USB_LOG_START_OK` in that case).

**See also**: [`XM_IsUsbLogSessionIdle`](#xm_isusblogsessionidle)

---

#### `XM_InsertUsbLogMarker`

```c
bool XM_InsertUsbLogMarker(uint8_t type, uint16_t value);
```

Inserts one marker while logging. The decoder collects the markers separately and shows them in `events.csv`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `type` | `uint8_t` | Marker kind, 1 – 254 — you decide the meaning (for example, 1 = experiment start, 2 = experiment end) |
| `value` | `uint16_t` | An extra value to store with it |

**Return value**: `bool` — `true`: inserted. `false`: not logging, `type` is outside 1 – 254, or there is no room in the buffer (logging does not stop).

**⚠️ Call context**: Call it in `Control_Loop()`.

---

#### `XM_GetUsbLogStats`

```c
bool XM_GetUsbLogStats(XmUsbLogStats_t* out);
```

Copies the logging statistics. While logging it returns the values so far; after logging ends, it returns the last values of that run.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out` | [`XmUsbLogStats_t*`](#xmusblogstats_t) | Struct that receives the statistics |

**Return value**: `bool` — `true`: copied. `false`: `out` is `NULL`, or you called it outside `Control_Setup()` / `Control_Loop()`.

**⚠️ Call context**: Call it in `Control_Loop()`. It returns 0 for every field after a start request until the file opens.

**Example**

```c
XmUsbLogStats_t st;
if (XM_GetUsbLogStats(&st)) {
    /* if st.empty_slots is 0, no samples are missing */
}
```

---

## Types / constants

### `XmUsbLogStartError_e`

The reason `XM_StartUsbDataLog()` could not start. Read it with [`XM_GetUsbLogLastStartError()`](#xm_getusbloglaststarterror).

| Value | Name | Meaning |
|----|------|------|
| 0 | `XM_USB_LOG_START_OK` | No failure recorded |
| 1 | `XM_USB_LOG_START_NOT_READY` | No USB memory stick (including one pulled out while waiting to start), still getting ready, or not FAT32 — check `XM_IsUsbLogReady()` |
| 2 | `XM_USB_LOG_START_BUSY` | Already logging or starting — start when `XM_IsUsbLogSessionIdle()` is `true` |
| 3 | `XM_USB_LOG_START_TRY_AGAIN` | Busy for a moment — start again shortly |
| 4 | `XM_USB_LOG_START_NO_SOURCE` | No struct to save — call `XM_SetUsbLogSource()` first |
| 5 | `XM_USB_LOG_START_BAD_SIZE` | The struct size is outside 1 – `XM_USB_LOG_MAX_STRUCT_SIZE` or differs from the registered size |
| 6 | `XM_USB_LOG_START_NEEDS_TIMESTAMP` | Below 1000 Hz the timestamp is required — `XM_SetUsbLogAutoTimestamp(true)` |
| 7 | `XM_USB_LOG_START_TOO_BIG_FOR_RATE` | The struct is too big for this rate — lower the rate or shrink the struct |
| 8 | `XM_USB_LOG_START_METADATA_TOO_LONG` | The meta line is longer than `XM_USB_LOG_MAX_METADATA_LEN` |
| 9 | `XM_USB_LOG_START_BAD_NAME` | The session name is too long or has a character that is not allowed, or the name is empty and the clock could not be read |
| 10 | `XM_USB_LOG_START_CANCELED` | `XM_StopUsbDataLog()` was called before the start was handled, which canceled the start (if the stick was pulled out in the meantime, `NOT_READY`) |
| 11 | `XM_USB_LOG_START_FOLDER_MISMATCH` | A folder with the same name already exists with a different rate or struct size, or its `metadata.txt` could not be read and checked — use another name or delete that folder on the PC |
| 12 | `XM_USB_LOG_START_STORAGE` | Not enough free space, or a folder or file could not be created or opened |
| 13 | `XM_USB_LOG_START_BAD_ARG` | An argument is invalid (for example, the meta line is `NULL`) |
| 14 | `XM_USB_LOG_START_METADATA_MISMATCH` | The meta line cannot be read — wrong format, unknown type, or field sizes that do not add up to the struct size |

Most reasons are left right away when `XM_StartUsbDataLog()` returns `false`. `XM_USB_LOG_START_STORAGE`, `XM_USB_LOG_START_FOLDER_MISMATCH` and `XM_USB_LOG_START_CANCELED` are left while the file is being opened, after the start request was accepted. `XM_USB_LOG_START_NOT_READY` (the stick was pulled out while waiting to start) and `XM_USB_LOG_START_BAD_NAME` (the name was empty and the clock could not be read) can also be left at that point.

---

### `XmUsbLogStats_t`

The logging statistics. Read them with [`XM_GetUsbLogStats()`](#xm_getusblogstats).

```c
typedef struct {
    uint32_t records;
    uint32_t bytes;
    uint32_t duration_ms;
    uint32_t empty_slots;
    uint32_t longest_gap_ms;
    uint32_t overflow_records;
    uint32_t write_errors;
    uint8_t  buffer_peak_percent;
} XmUsbLogStats_t;
```

| Field | Type | Description |
|------|------|------|
| `records` | `uint32_t` | Number of records received for saving (markers included) |
| `bytes` | `uint32_t` | Number of record bytes written to the USB memory file |
| `duration_ms` | `uint32_t` | Session length [ms] |
| `empty_slots` | `uint32_t` | Number of missing samples — the number of empty slots among the logging-rate slots |
| `longest_gap_ms` | `uint32_t` | Longest gap [ms] — consecutive missing samples × period |
| `overflow_records` | `uint32_t` | Number of records that could not be saved because the internal buffer overflowed — 1 or more means the session stops at that point (it can be 0 if it stops just before the buffer fills, so also check whether `buffer_peak_percent` is close to 100) |
| `write_errors` | `uint32_t` | Number of failed writes to the USB memory stick |
| `buffer_peak_percent` | `uint8_t` | The highest share of the internal buffer that was used [%] |

- While logging they are the values so far; after the session ends they are that session's last values.
- After a start request they are all 0 until the file opens, and a new session counts from 0 again.
- If a start fails (opening the file failed · canceled · the stick was pulled out), the previous session's values are kept.
- `records` is the number of records received for saving, and `bytes` is how many record bytes of them were written to the USB memory file (the file header · tail · error-check values · `metadata.txt` · `summary.txt` are not counted). While logging, `records` runs ahead of `bytes`, and if the stick is pulled out or the buffer overflows, the last few records are not on the stick, so `records` can be larger than the number actually saved.

---

### Constants

| Name | Value | Description |
|------|----|------|
| `XM_USB_LOG_MAX_STRUCT_SIZE` | `512U` | Maximum size of a struct you can register [B] |
| `XM_USB_LOG_MAX_METADATA_LEN` | `1700U` | Maximum length of the meta line [characters, not counting the trailing `'\0'`] |
| `XM_USB_LOG_MAX_SESSION_NAME` | `100U` | Maximum length of a session name [characters] |

> `XM_USB_CUSTOM_META_MAX_LEN` in `xm_api_usb.h` is the limit for USB-CDC channel names (JSON). It is separate from the meta-line limit here, so do not mix them up.

---

## Meta line

The `metadata` argument of `XM_StartUsbDataLog()` is a single line that lists the struct's fields in declaration order as `name(type)`, separated by commas. The decoder uses this line to decode the data.

```c
"loop_count(uint32_t), hip_L(float), hip_R(float), contact(uint8_t), _pad(3bytes)"
```

| Rule | Details |
|------|------|
| Types | `uint8_t` · `int8_t` · `uint16_t` · `int16_t` · `uint32_t` · `int32_t` · `float` · `double` · `bool` — written in lowercase exactly as shown (case matters) |
| Padding | Write unused bytes as `_name(Nbytes)`, for example `_pad(3bytes)`. The name must start with `_` to count as padding, and `pad(3bytes)` is an unknown type, so logging does not start |
| Names | Use only letters, digits and `_` |
| Shape | Put no spaces inside `name(type)`, and keep everything on one line |
| Size | The sum of the field sizes must equal the registered struct size. If it does not, logging does not start (`XM_USB_LOG_START_METADATA_MISMATCH`) |

- The system checks only the sum of sizes. It does not check that names, types and order match the struct, so match them yourself.
- Declaring the struct with `__attribute__((packed))` and fixing its size with `_Static_assert` helps you avoid mistakes.
- Do not use the names the decoder uses (`tick_ms` · `t_grid_ms` · `row_origin` · `count` · `h10_loop_count`) as field names unless you mean that.

---

## Moving code from v2.4.1 or earlier

USB memory logging code written before v2.5.0 may not build as is — even where the function names are the same, the return types and arguments differ.

| v2.4.1 or earlier | Now |
|------|------|
| `XM_SetUsbLogSource(void*, uint32_t)` — no return value | Returns `bool`, and the first argument is `const void*` |
| `XM_StopUsbDataLog()` · `XM_SetUsbLogAutoTimestamp()` · `XM_SetUsbLogRollingSize()` — no return value | All of them return `bool` |
| `XM_InsertUsbLogMarker(XM_LOG_MARKER_USER, value)` — an enum constant is used | The first argument is a number from 1 to 254, and you decide what it means. There are no `XM_LOG_MARKER_*` constants |
| `XM_GetUsbLogStats()` fills an `XmLogStats_t` | It fills an `XmUsbLogStats_t`. The fields are different |
| — | New functions: `XM_SetUsbLogSampleRateHz()` (default 1000 Hz) · `XM_IsUsbLogSessionIdle()` · `XM_IsUsbLogRecording()` · `XM_GetUsbLogLastStartError()` |
| — | New check: at the start the meta line is checked, and logging does not start if it is wrong (`XM_USB_LOG_START_METADATA_MISMATCH`) |

- If you get a build error, compare against the function list on this page and fix it.
- Examples 10 and 34 had the same numbers as USB memory examples in earlier versions, but the current examples are new. The earlier examples are not part of this release.

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| USB memory logging API (`xm_api_usb_msc.h`) | ❌ Absent | 🟢 Exclusive |

---

## Related documents

- [06. USB Memory Logging (concept)](../06-usb-data-logging.en.md) — order of use, preparing the stick, the USB-C port rule, common mistakes
- [Save Data to a USB Memory Stick](../../getting-started/05-usb-memory-log.en.md) — step by step, from formatting to the decoder
- [`xm_api_usb.h` — USB-CDC Real-Time Streaming](xm-api-usb.en.md) — the API for sending data to a PC in real time
- [`xm_api_rtc.h` — RTC](xm-api-rtc.en.md) — the clock used for session folder names
