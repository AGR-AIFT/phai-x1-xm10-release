# `xm_api_usb.h` — USB Data Logging · Real-Time Streaming

> **Target header**: `XM_FW/XM_API/xm_api_usb.h`
> **Related concept docs**: [05. USB Serial](../05-usb-connectivity.en.md) · [06. USB Mass-Storage Logging](../06-usb-data-logging.en.md)
> **Related examples**: [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) · [07_CDC_Basic_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/07_CDC_Basic_Print/) · [08_CDC_Sensor_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/) · [09_CDC_Stream](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) · [10a_MSC_Basic_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10a_MSC_Basic_Log/) · [10b_MSC_Custom_Struct](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10b_MSC_Custom_Struct/) · [10c_MSC_Advanced_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10c_MSC_Advanced_Log/) · [18_Debug_Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/) · [34_MSC_GaitAnalysis_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/)

This single header defines two domains together: **USB mass-storage logging (MSC)** and **real-time PC communication (CDC)**. On the MSC side, once you register a user struct, a background task automatically stores it to USB storage; on the CDC side, you exchange text/binary data with a PC client such as PhAI Studio or PuTTY. Starting with Rev2.0, a host-profile API (`XM_USB_SetHostProfile`) has also been added that lets you **choose whether a single USB-CDC cable is used for PhAI Studio real-time streaming or a plain terminal**. (Production-inspection GUI support is handled automatically inside the board, so it is not a user option.)

---

## When to use it

Use this API when you want to log sensor data to USB storage over a long period, or exchange real-time data with a PC over a serial connection. For the registration-based automation principle (register a source in Setup → the System processes it automatically on a periodic basis) and the big picture — file format, Python decoder usage, and so on — we recommend reading [05. USB Serial](../05-usb-connectivity.en.md) and [06. USB Mass-Storage Logging](../06-usb-data-logging.en.md) first. This page only covers the detailed **signatures/parameters of every function and type** declared in the header, and it also includes the new Rev2.0 **USB-CDC host-profile API**, which the two concept docs do not yet cover.

---

## Function list

**Data Source Registration**

| Function | One-line description |
|------|-----------|
| [`XM_SetUsbLogSource`](#xm_setusblogsource) | [MSC] Registers a data source to store to USB storage |
| [`XM_SetUsbStreamSource`](#xm_setusbstreamsource) ⚠️ Deprecated | [CDC] Registers a data source to stream to the PC (legacy) |

**MSC Logging — Session Control**

| Function | One-line description |
|------|-----------|
| [`XM_IsUsbLogReady`](#xm_isusblogready) | Checks whether a USB storage device is connected and logging is ready |
| [`XM_StartUsbDataLog`](#xm_startusbdatalog) | Starts a logging session |
| [`XM_StopUsbDataLog`](#xm_stopusbdatalog) | Stops a logging session |
| [`XM_GetActiveUsbSessionName`](#xm_getactiveusbsessionname) | Retrieves the current/last active session name (for Option A re-entry) |
| [`XM_GetUsbLogStatus`](#xm_getusblogstatus) | Queries the logger state |

**MSC Logging — Configuration**

| Function | One-line description |
|------|-----------|
| [`XM_SetUsbLogAutoTimestamp`](#xm_setusblogautotimestamp) | Turns automatic timestamp insertion on/off |
| [`XM_SetUsbLogRollingSize`](#xm_setusblogrollingsize) | Sets the file rolling (splitting) size |

**MSC Logging — Statistics · Disk Capacity**

| Function | One-line description |
|------|-----------|
| [`XM_GetUsbLogStats`](#xm_getusblogstats) | Queries real-time session statistics |
| [`XM_GetUsbDiskFreeMB` / `XM_GetUsbDiskTotalMB`](#xm_getusbdiskfreemb--xm_getusbdisktotalmb) | Queries USB free/total capacity (10-second cache) |

**MSC Logging — Event Markers**

| Function | One-line description |
|------|-----------|
| [`XM_InsertUsbLogMarker`](#xm_insertusblogmarker) | Inserts an event marker into the logging stream |

**CDC — Connection · Streaming Status**

| Function | One-line description |
|------|-----------|
| [`XM_IsUsbStreamConnected`](#xm_isusbstreamconnected) | Whether a CDC virtual serial port connection to the PC exists |
| [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive) | Whether streaming is active |
| [`XM_SetUsbAutoStream`](#xm_setusbautostream) | Turns Auto-Stream mode on/off |

**CDC — Host Profile** 🟢 Rev 2.0 only

| Function | One-line description |
|------|-----------|
| [`XM_USB_SetHostProfile`](#xm_usb_sethostprofile) 🟢 | Selects the USB-CDC host profile (PhAI Studio / terminal) |

**CDC — Data Transmission (Legacy + Custom Data)**

| Function | One-line description |
|------|-----------|
| [`XM_SendUsbData`](#xm_sendusbdata) ⚠️ Deprecated | Sends data wrapped in a PhAI packet (fixed Module ID) |
| [`XM_SetUsbStreamModuleId`](#xm_setusbstreammoduleid) ⚠️ Deprecated | Sets the Module ID of the streamed data |
| [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta) | Registers User Custom channel metadata (JSON) |
| [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid) | Sends `float[]` data with a specified Module ID |

**CDC — Debug Messages · Receiving**

| Function | One-line description |
|------|-----------|
| [`XM_SendUsbDebugMessage`](#xm_sendusbdebugmessage) | Sends a raw text debug message to the PC |
| [`XM_GetUsbData`](#xm_getusbdata) | Receives data from the PC |

---

## Function details

### `XM_SetUsbLogSource`

```c
void XM_SetUsbLogSource(void* data_ptr, uint32_t size);
```

**[MSC]** Registers the data source to store to USB storage. The System periodically reads the data at the registered address and writes it to a file.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `data_ptr` | `void*` | Address of the struct to store (`&myData`) |
| `size` | `uint32_t` | Size of the struct (`sizeof(myData)`) |

**Return value**: none (`void`)

**⚠️ Call context**: Assume `Control_Setup()` context. Register once before starting logging (`XM_StartUsbDataLog`).

**Example**

```c
typedef struct {
    float hip_angle_L;
    float hip_angle_R;
} MyLogData_t;

MyLogData_t myData;

void Control_Setup(void) {
    XM_SetUsbLogSource(&myData, sizeof(myData));
}
```

**See also**: [`XM_StartUsbDataLog`](#xm_startusbdatalog), [06. USB Mass-Storage Logging](../06-usb-data-logging.en.md)

---

### `XM_SetUsbStreamSource`

```c
void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
```

> ⚠️ **Deprecated** — Replaced by Total Data (0x20) automatic transmission. If you need additional channels, use the combination of [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta) + [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid).

**[CDC]** Registers a data source to stream to the PC in real time (legacy method).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `data_ptr` | `void*` | Address of the struct to send |
| `size` | `uint32_t` | Size of the struct |

**Return value**: none (`void`)

**⚠️ Call context**: Assume `Control_Setup()` context.

**See also**: [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta), [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_IsUsbLogReady`

```c
bool XM_IsUsbLogReady(void);
```

Checks whether a USB storage device (MSC) is connected and logging is ready. This is lightweight, since it only reads a status flag managed by the System Layer's `usb_mode_handler`.

**Parameters**: none

**Return value**: `bool` — `true` if ready, `false` otherwise

**⚠️ Call context**: Safe to call from the 2 ms real-time loop in `Control_Loop()` (non-blocking).

**Example**

```c
if (XM_IsUsbLogReady() && !logging_started) {
    XM_StartUsbDataLog(NULL, "hip_L(float), hip_R(float)");
    logging_started = true;
}
```

**See also**: [`XM_StartUsbDataLog`](#xm_startusbdatalog)

---

### `XM_StartUsbDataLog`

```c
bool XM_StartUsbDataLog(const char* sessionName, const char* metadata);
```

Starts a USB data logging session. The low-priority logging task creates the `/LOGS/[sessionName]` folder and `metadata.txt`, and prepares to write `data_000...bin`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `sessionName` | `const char*` | Session (folder) name (e.g., `"S_001_TestRun"`). Passing `NULL` or an empty string auto-generates `S_001`, `S_002`, ... |
| `metadata` | `const char*` | A string describing the binary data to be stored. Written as-is into `metadata.txt` |

**Return value**: `bool` — `true` if the command was successfully queued, `false` if the queue is full or USB is not ready

**⚠️ Call context**: This function sends a command to the low-priority logging task, and **can block for up to 100 ms** if the queue is full. **Never call it from inside the 2 ms real-time loop (`Control_Loop()`)** — it is recommended to call it only once, from a state-transition entry function (such as `EnterActive`).

**Example**

```c
// Manual session name
XM_StartUsbDataLog("Gait_001", "hip_L(float), hip_R(float)");

// Automatic session numbering
XM_StartUsbDataLog(NULL, "hip_L(float), hip_R(float)");
// → creates /LOGS/S_001/, /LOGS/S_002/, ... in sequence
```

**See also**: [`XM_StopUsbDataLog`](#xm_stopusbdatalog), [`XM_GetActiveUsbSessionName`](#xm_getactiveusbsessionname), [06. USB Mass-Storage Logging §Session Output File Structure](../06-usb-data-logging.en.md#session-output-file-structure)

---

### `XM_StopUsbDataLog`

```c
void XM_StopUsbDataLog(void);
```

Stops the USB data logging session. Asynchronously sends the low-priority logging task a command to close the currently open file and end logging.

**Parameters**: none

**Return value**: none (`void`)

**⚠️ Call context**: **Do not call from inside the 2 ms real-time loop (`Control_Loop()`).** You must call this before unplugging USB, so that the file is closed properly.

**See also**: [`XM_StartUsbDataLog`](#xm_startusbdatalog)

---

### `XM_GetActiveUsbSessionName`

```c
void XM_GetActiveUsbSessionName(char* out_buf, uint32_t buf_size);
```

**[Option A]** Queries the current/last active USB session name. Designed so that, after re-entering following an Emergency Stop, you can **keep appending to the same folder**. The intended usage flow is: call `XM_StartUsbDataLog("", ...)` the first time → an automatic name is generated based on the boot count → query the name with this API → the example code stores it → on re-entry, call `XM_StartUsbDataLog(<stored name>)` → the firmware incrementally creates `data_001_*`, `data_002_*`, and so on in the same folder.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out_buf` | `char*` | Buffer into which the session name is copied |
| `buf_size` | `uint32_t` | Buffer size (32 bytes or more recommended) |

**Return value**: none (`void`)

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context. The header does not separately document ISR-safety.

> ⚠️ **Known race condition — caution when calling directly**: The session name is updated asynchronously by the low-priority `DataLoggerTask`. If you call this function **immediately after** `XM_StartUsbDataLog()`, you may get an empty string or the previous session's name, since the update has not happened yet. In practice, [Ex.34 (`msc_gait_analysis_log.c`)](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) ran into exactly this problem, and switched away from relying on this API: instead, **the example code itself generates the session name based on RTC/tick**, passes it synchronously to `XM_StartUsbDataLog()`, and stores that name in its own variable for reuse on re-entry (no race). When implementing re-entry logic, we recommend following the Ex.34 pattern rather than depending on the return timing of this function.

**See also**: [`XM_StartUsbDataLog`](#xm_startusbdatalog), [Ex.34 MSC_GaitAnalysis_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/)

---

### `XM_GetUsbLogStatus`

```c
XmLogStatus_e XM_GetUsbLogStatus(void);
```

Checks the current logger state. Use this to check whether logging has been forcibly stopped (`ERROR_STOPPED`), whether the buffer is filling up (`WARNING_QUEUE_FULL`), and so on.

**Parameters**: none

**Return value**: an [`XmLogStatus_e`](#xmlogstatus_e) enum value

**⚠️ Call context**: Safe to call from the 2 ms real-time loop in `Control_Loop()`.

**See also**: full list of [`XmLogStatus_e`](#xmlogstatus_e) values

---

### `XM_SetUsbLogAutoTimestamp`

```c
void XM_SetUsbLogAutoTimestamp(bool enabled);
```

Sets whether an automatic timestamp (4-byte `tick_ms`) is prepended to each record.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `enabled` | `bool` | `true`: automatically insert the tick before each packet (default). `false`: disable this when the user struct already contains its own tick |

**Return value**: none (`void`)

**⚠️ Call context**: Set this in `Control_Setup()` **before** calling [`XM_StartUsbDataLog`](#xm_startusbdatalog).

**See also**: [`XM_SetUsbLogRollingSize`](#xm_setusblogrollingsize)

---

### `XM_SetUsbLogRollingSize`

```c
void XM_SetUsbLogRollingSize(uint32_t size_mb);
```

Sets the file rolling (automatic splitting) size.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `size_mb` | `uint32_t` | File split size (MB). Range 1–100, default 10 |

**Return value**: none (`void`)

**⚠️ Call context**: Set this in `Control_Setup()` **before** calling [`XM_StartUsbDataLog`](#xm_startusbdatalog).

**See also**: [`XM_SetUsbLogAutoTimestamp`](#xm_setusblogautotimestamp)

---

### `XM_GetUsbLogStats`

```c
bool XM_GetUsbLogStats(XmLogStats_t* out_stats);
```

Queries detailed session statistics, either during logging or after a session has ended. Includes diagnostic information such as Hot/Cold buffer peak utilization and remaining disk space.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out_stats` | `XmLogStats_t*` | Pointer to the struct that receives the statistics |

**Return value**: `bool` — `true` on success, `false` on a parameter error or when USB is not connected

**⚠️ Call context**: Safe to call from the 2 ms real-time loop in `Control_Loop()`.

**Example**

```c
XmLogStats_t stats;
if (XM_GetUsbLogStats(&stats)) {
    printf("Records: %lu, Dropped: %lu, Disk: %lu MB\n",
           stats.total_records, stats.dropped_records, stats.disk_free_mb);
}
```

**See also**: [`XmLogStats_t`](#xmlogstats_t)

---

### `XM_GetUsbDiskFreeMB` / `XM_GetUsbDiskTotalMB`

```c
uint32_t XM_GetUsbDiskFreeMB(void);
uint32_t XM_GetUsbDiskTotalMB(void);
```

Returns the USB disk's free/total capacity (MB). Since the value is cached and updated on a 10-second cycle and returned immediately, there is no cost to calling it on every tick.

**Parameters**: none

**Return value**: `uint32_t` — capacity (MB). 0 if USB is not connected

**⚠️ Call context**: Safe to call from the 2 ms real-time loop in `Control_Loop()` (non-blocking, returns a cached value).

**See also**: [`XM_GetUsbLogStats`](#xm_getusblogstats) — the same value can also be checked via the `disk_free_mb`/`disk_total_mb` fields

---

### `XM_InsertUsbLogMarker`

```c
bool XM_InsertUsbLogMarker(XmLogMarkerType_e type, uint16_t data);
```

Marks a specific point in time during logging (mode transition, anomaly detection, manual mark) as a marker. Markers are stored in the same pipeline as normal data, and the Python decoder automatically separates them out to generate an event log (`events.csv`).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `type` | `XmLogMarkerType_e` | Marker type |
| `data` | `uint16_t` | Context data (error code, mode ID, etc.; 0 if not needed) |

**Return value**: `bool` — `true` on success, `false` if logging is inactive or the buffer is insufficient

**⚠️ Call context**: Safe to call from the 2 ms real-time loop in `Control_Loop()` (non-blocking).

**Example**

```c
// On a mode transition
XM_InsertUsbLogMarker(XM_LOG_MARKER_MODE, newModeId);

// On error detection
XM_InsertUsbLogMarker(XM_LOG_MARKER_ERROR, errorCode);

// Manual marking
XM_InsertUsbLogMarker(XM_LOG_MARKER_USER, 0);
```

**See also**: [`XmLogMarkerType_e`](#xmlogmarkertype_e)

---

### `XM_IsUsbStreamConnected`

```c
bool XM_IsUsbStreamConnected(void);
```

Checks whether a PC is connected to the USB virtual serial port (CDC).

**Parameters**: none

**Return value**: `bool` — `true` if connected

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context.

**See also**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

### `XM_IsUsbStreamingActive`

```c
bool XM_IsUsbStreamingActive(void);
```

Checks whether streaming is active. In Auto-Stream mode this automatically becomes `true` when USB connects; in Legacy mode it becomes `true` when `"AGRB MON START"` is received.

**Parameters**: none

**Return value**: `bool` — `true`: streaming active (data being sent), `false`: idle

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context.

**See also**: [`XM_SetUsbAutoStream`](#xm_setusbautostream)

---

### `XM_SetUsbAutoStream`

```c
void XM_SetUsbAutoStream(bool enabled);
```

Sets Auto-Stream mode. Default is ON — streaming starts automatically when USB connects (PhAI Studio's default behavior).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `enabled` | `bool` | `true`: automatic streaming on USB connect (default). `false`: wait for the `"AGRB MON START"` command (Legacy Python compatibility) |

**Return value**: none (`void`)

**⚠️ Call context**: Assume `Control_Setup()` context.

**See also**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

### `XM_USB_SetHostProfile` 🟢 Rev 2.0 only

```c
void XM_USB_SetHostProfile(XM_USB_HostProfile_e profile);
```

> 🟢 **Rev 2.0 only** — This function and the [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e) type itself do not exist at all in the Rev1.1 header.

Specifies the kind of PC app attached to this board's USB-CDC cable. **If never called, the default is `XM_USB_HOST_PHAI_STUDIO`**, so leaving the profile untouched keeps the existing PhAI Studio real-time streaming behavior unchanged.

To use a plain serial terminal / custom program (Tera Term · VS Code Serial Monitor · your own Python GUI, etc.) with **clean text / custom IO only**, call `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)` once in `Control_Setup()`. The 1 kHz Total Data auto-pump is turned off, and this setting **persists across DTR re-toggle / cable reconnect**.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `profile` | [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e) | `XM_USB_HOST_PHAI_STUDIO` (default) or `XM_USB_HOST_TERMINAL` |

**Return value**: none (`void`)

**⚠️ Call context**: Assume `Control_Setup()` context. Call it once.

**Example**

```c
void Control_Setup(void) {
    // When you just want to read text in a plain serial terminal (the Ex.07 / Ex.08 way)
    XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL);
}
```

> The production-inspection (PRODUCTION) behavior is entered/exited automatically inside the board on receiving the first valid DOP frame, and is not a user option. Setting `XM_USB_HOST_TERMINAL` disables that automatic entry so it does not interfere with your terminal output.

**See also**: [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e), [05. USB Serial](../05-usb-connectivity.en.md)

---

### `XM_SendUsbData`

```c
bool XM_SendUsbData(const void* data, uint32_t len);
```

> ⚠️ **Deprecated** — Replaced by [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid). Specify the Module ID explicitly.

Sends data over USB CDC, automatically wrapped in a PhAI packet (SOF + LEN + SEQ_ID + MODULE_ID + CRC16).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `data` | `const void*` | Pointer to the user struct to send (a float array or a 4-byte-aligned struct) |
| `len` | `uint32_t` | Number of data bytes |

**Return value**: `bool` — `true` on successful send, `false` if the buffer is full or not connected

**⚠️ Call context**: Safe to call within the 2 ms cycle of `Control_Loop()` (non-blocking).

**See also**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SetUsbStreamModuleId`

```c
void XM_SetUsbStreamModuleId(uint8_t module_id);
```

> ⚠️ **Deprecated** — Replaced by [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid). Specify the Module ID directly at send time.

Sets the Module ID of the streamed data.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `module_id` | `uint8_t` | Module ID (default `0x10` = COMBINED). See the `PHAI_MODULE_*` macros in `phai_packet_builder.h` |

**Return value**: none (`void`)

**See also**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SetUsbCustomMeta`

```c
void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
```

Registers metadata (channel names/units, etc.) for the User Custom channel. Automatically sent with Module ID `0xEF` when USB connects, so channel names are displayed in PhAI Studio.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `module_id` | `uint8_t` | Target Module ID (`0xF0`~`0xFE`) |
| `json_str` | `const char*` | JSON string of channel definitions (NULL-terminated; a string literal is recommended) |

**Return value**: none (`void`)

**⚠️ Call context**: Call once from `Control_Setup()`.

**⚠️ Pointer lifetime caution**: The `json_str` pointer must remain valid for the lifetime of the program (it is not copied internally) — use a string literal, not a stack-local variable.

**⚠️ Single slot**: Only one Module ID's metadata is kept per USB connection (the last call overwrites the previous one). To label multiple channel groups, gather the channels under a single Module ID for registration.

**Example**

```c
void Control_Setup(void) {
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Target\",\"unit\":\"deg\"},"
        "{\"name\":\"Current\",\"unit\":\"deg\"}]");
}
```

**See also**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SendUsbDataWithId`

```c
bool XM_SendUsbDataWithId(const void* data, uint32_t len, uint8_t module_id);
```

Sends User Custom `float[]` data with the specified Module ID. Since Total Data (0x20) is already sent automatically by the System, use this function to send additional debug channels for your own algorithm.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `data` | `const void*` | Pointer to a float array (4-byte aligned) |
| `len` | `uint32_t` | Number of bytes (`sizeof(float) × channel count`) |
| `module_id` | `uint8_t` | Module ID (`0xF0`~`0xFE`) |

**Return value**: `bool` — `true` on successful send, `false` if the buffer is full or not connected

**⚠️ Call context**: Call from within `Control_Loop()` (non-blocking). Calling it on every tick is not mandatory — call it only when needed.

**Example**

```c
float data[4] = { target, current, error, torque };
XM_SendUsbDataWithId(data, sizeof(data), 0xF0);
```

**See also**: [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta)

---

### `XM_SendUsbDebugMessage`

```c
bool XM_SendUsbDebugMessage(const char* message);
```

Sends a debug message to the PC as raw text (no PhAI packet wrapping).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `message` | `const char*` | String to send (null-terminated) |

**Return value**: `bool` — `true` on successful send

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context. Non-blocking.

**Example**

```c
char buf[64];
sprintf(buf, "Current State: %d\r\n", current_state);
XM_SendUsbDebugMessage(buf);
```

**See also**: [05. USB Serial](../05-usb-connectivity.en.md)

---

### `XM_GetUsbData`

```c
uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);
```

Receives data from the PC (non-blocking).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `buffer` | `void*` | Buffer to store the received data |
| `max_len` | `uint32_t` | Maximum buffer size |

**Return value**: `uint32_t` — number of bytes actually read (0 means no data received)

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context. The header has no `@details` description, so error states other than a 0 return value are not documented — we recommend verifying empirically before use.

---

## Types / macros

### `XmLogStatus_e`

An enum representing the logger's current state.

| Value | Description |
|----|------|
| `XM_LOG_STATUS_IDLE` | Stopped (initial state) |
| `XM_LOG_STATUS_LOGGING` | Logging normally |
| `XM_LOG_STATUS_WARNING_QUEUE_FULL` | Buffer utilization is high (`f_write` is being delayed) |
| `XM_LOG_STATUS_WARNING_DISK_LOW` | Less than 50 MB of USB disk space remains |
| `XM_LOG_STATUS_ERROR_STOPPED` | Logging was forcibly stopped due to an error |

### `XmLogStats_t`

A struct of real-time statistics for a logging session. Queried with [`XM_GetUsbLogStats()`](#xm_getusblogstats).

| Field | Type | Description |
|------|------|------|
| `total_bytes` | `uint32_t` | Total bytes written |
| `total_records` | `uint32_t` | Total record count |
| `dropped_records` | `uint32_t` | Number of dropped records (buffer overflow) |
| `write_errors` | `uint32_t` | Number of write failures |
| `duration_ms` | `uint32_t` | Session elapsed time (ms) |
| `hot_buffer_percent` | `uint8_t` | Hot buffer peak utilization (0–100) |
| `cold_buffer_percent` 🟢 | `uint8_t` | Cold buffer peak utilization (0–100) — legacy field, [always 0 in the current Hot-buffer-only architecture](../06-usb-data-logging.en.md) |
| `disk_free_mb` | `uint32_t` | USB free capacity (MB) |
| `disk_total_mb` | `uint32_t` | USB total capacity (MB) |

> 🟢 **Rev 2.0-only field**: `cold_buffer_percent` does not exist in the Rev1.1 header's `XmLogStats_t` (Rev1.1 has 8 fields, Rev2.0 has 9). In code that handles both Rev1.1 and Rev2.0, do not cast or raw-copy this struct directly.

### `XmLogMarkerType_e`

An enum representing the type of an event marker during logging.

| Value | Description |
|----|------|
| `XM_LOG_MARKER_USER` = 0x01 | Manual mark (button/command) |
| `XM_LOG_MARKER_MODE` = 0x02 | Mode transition |
| `XM_LOG_MARKER_ERROR` = 0x03 | Error occurred |
| `XM_LOG_MARKER_SYNC` = 0x04 | Time synchronization point |

### `XM_USB_HostProfile_e` 🟢 Rev 2.0 only

> Not defined in the Rev1.1 header.

The USB-CDC host profile you set to say what a single board/cable is used for. There are two choices, set with [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile).

| Value | Description |
|----|------|
| `XM_USB_HOST_PHAI_STUDIO` = 0 | **Default** — PhAI Studio real-time streaming (Total Data auto-pump ON) |
| `XM_USB_HOST_TERMINAL` = 1 | Plain terminal / custom program — auto-pump OFF, user-initiated TX only |

> The production-inspection (PRODUCTION) behavior is not a user choice — it is handled **automatically inside the board**: entered on receiving the first valid DOP frame, exited on DTR=0 (cable unplug). Setting the profile to `XM_USB_HOST_TERMINAL` disables that automatic entry (protecting your terminal output). The profile setting persists across DTR re-toggle / reconnect.

---

## Internal-only (do not call)

The function below is for exclusive use by an internal system component (core_process). User code does not need to call it directly, and calling it does not guarantee the intended behavior.

| Function | Actual caller | Description |
|------|--------------|------|
| `void XM_USB_ProcessPeriodic(void)` | `core_process` | The periodic-processing engine for USB logging/streaming logic. `core_process` calls this automatically, so user code does not need to call it directly |

> 🟢 **Rev 2.0 note** — The PRODUCTION auto-latch / DTR handling that used to live here in v2.3.1 (the former `XM_USB_RequestProductionLatch` / `XM_USB_OnDtrLost`) was moved inside the System layer (`usb_host_mode`) in v2.4.0 and is no longer in the public header. The only public user-facing API is [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile).

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| MSC logging basic API (Start/Stop/Status/Stats/Marker/disk query) | ✅ | ✅ |
| CDC streaming basic API (`SendUsbDataWithId`/`SetUsbCustomMeta`/`SendUsbDebugMessage`) | ✅ | ✅ |
| `XmLogStats_t.cold_buffer_percent` field | ❌ Absent (8 fields) | 🟢 Present (9 fields — legacy, currently always 0) |
| `XM_USB_HostProfile_e` / `XM_USB_SetHostProfile` (PhAI Studio / terminal host-profile selection) | ❌ Absent | 🟢 Exclusive |
| PRODUCTION auto-entry / DTR handling (inside System `usb_host_mode`, not a public API) | ❌ Absent | 🟢 Exclusive (Internal) |
| MSC logging internal pipeline stages | 2-stage — UserTask writes directly into a lock-free SPSC ring buffer → `DataLoggerTask` calls `f_write()` | 3-stage — UserTask enqueues into a primary queue → `DataLoggerTask` converts to binary and enqueues into a secondary queue → the low-priority task calls `f_write()` |

---

## Related Documents

- [05. USB Serial (concept)](../05-usb-connectivity.en.md) — CDC operating principles, common mistakes, example mapping
- [06. USB Mass-Storage Logging (concept)](../06-usb-data-logging.en.md) — MSC file format, Python decoder, session output structure
