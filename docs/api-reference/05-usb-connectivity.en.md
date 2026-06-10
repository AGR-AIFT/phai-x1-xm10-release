# API Reference: USB Connectivity

> 📌 **After reading this page** you will be able to handle both USB-CDC text/binary send-receive and USB-MSC logging registration end-to-end.
> ⏱️ Estimated reading time: 25 minutes
> 🧰 Prerequisites: [Ex.07–09](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/07_CDC_Basic_Print/) CDC + [Ex.10–10c](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10a_MSC_Basic_Log/) MSC
> 🎯 Key functions: `XM_SendUsbDebugMessage` / `XM_SetUsbCustomMeta` / `XM_SendUsbDataWithId` / `XM_SetUsbLogSource` / `XM_StartUsbDataLog`
>
> ⚠️ **USB-CDC single-owner rule**: Never open PhAI Studio and a serial terminal (PuTTY, RealTerm, etc.) on the **same COM port simultaneously** — doing so causes a port conflict and data loss.

Detailed reference for the **USB communication and data management API** defined in `xm_api_usb.h`.
XM10 exposes two powerful features simultaneously over the USB port:

1.  **MSC (Mass Storage Class):** Saves data as binary files (`.bin`) to a USB flash drive.
2.  **CDC (Communication Device Class):** Connects to a PC as a virtual serial port for real-time data transfer and debugging.

This module uses a **registration-based automation** model: once you register the data struct you want to capture, the system handles saving and transmitting it in the background automatically.

-----

## 1. Operating Principle

The USB module follows a **configure-once, run-automatically** pattern to minimize user intervention.

### The Automation Cycle

1.  **Registration:** In `Control_Setup()`, tell the system the address of the data struct you want to save (e.g., `MyData`).
2.  **Control:** Enable the feature by calling `XM_StartUsbDataLog()` or sending `AGRB MON START` over the serial link.
3.  **Automatic processing:** The `core_process` engine calls `XM_USB_ProcessPeriodic()` every 2 ms.
      * The system **automatically copies** data from the registered struct and writes it to the USB flash drive or streams it to the PC.
      * You do not need to call `Log()` or `Send()` in every loop iteration.

-----

## 2. Data Structures (Enumerations)

### 2.1. Status Types

#### `XmLogStatus_e` (recommended)

Return type used to clearly communicate the result of a function call.

```c
typedef enum {
    XM_LOG_STATUS_IDLE,      // Stopped (initial state)
    XM_LOG_STATUS_LOGGING,   // Logging normally
    XM_LOG_STATUS_WARNING_QUEUE_FULL, // Buffer utilization high (f_write delays occurring)
    XM_LOG_STATUS_WARNING_DISK_LOW,   // USB disk has less than 50 MB remaining
    XM_LOG_STATUS_ERROR_STOPPED,    // Logging forcibly stopped due to an error
} XmLogStatus_e;
```

-----

## 3. Function Reference

### 3.1. Data Source Registration

Call these functions first. If you skip registration, the system falls back to the default (the entire `XM` struct) or may save nothing at all.

#### `XM_SetUsbLogSource`

**[MSC]** Specifies the data source to be written to the USB file.

  * **Syntax**
    ```c
    void XM_SetUsbLogSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: Pointer to the user struct to save (e.g., `&myData`)
      * `size`: Size of the struct (`sizeof(myData)`)
  * **Example**
    ```c
    typedef struct { uint32_t time; float angle; } MyLog_t;
    MyLog_t myLog;

    void Control_Setup() {
        // Register this struct to be written to the file every period
        XM_SetUsbLogSource(&myLog, sizeof(MyLog_t));
    }
    ```

#### `XM_SetUsbStreamSource`

**[CDC]** Specifies the data source to be streamed to the PC in real time. This source can differ from the MSC log source.

  * **Syntax**
    ```c
    void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: Pointer to the struct to transmit
      * `size`: Size of the struct
  * **Note**: Once streaming is active (via `XM_SetUsbAutoStream(true)` or a stream-start command from the PC side), data registered with this function is transmitted to the PC in **binary** form. Suitable for serial plotters and similar tools.

-----

### 3.2. MSC Control (Data Logging)

Functions for creating files on USB flash and recording data.

#### `XM_StartUsbDataLog`

Starts data logging.

  * **Syntax**
    ```c
    bool XM_StartUsbDataLog(const char* sessionName, const char* metadata);
    ```
  * **Parameters**
      * `sessionName`: Prefix for the folder that will be created. (e.g., `"Walk"` → `Walk/data_000_part_000.bin`, `data_001_part_000.bin` …)
      * `metadata`: String written to the file header (first line). Useful for labeling binary columns. (may be `NULL`)
  * **Returns**: `true` (started successfully), `false` (no USB drive or error)
  * **Example**
    ```c
    // When button is pressed, create a "Test" session and write "Time_ms, Angle_deg" as the header
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_StartUsbDataLog("Test", "Time_ms, Angle_deg");
    }
    ```

#### `XM_StopUsbDataLog`

Stops logging and flushes the file (Close & Sync).
**[Important]** Always call this function before unplugging the USB drive; otherwise data may be corrupted.

  * **Syntax**
    ```c
    void XM_StopUsbDataLog(void);
    ```

#### `XM_IsUsbLogReady`

Checks whether the USB flash drive is recognized and the file system is ready.

  * **Syntax**
    ```c
    bool XM_IsUsbLogReady(void);
    ```
  * **Returns**: `true` (ready), `false` (not connected)

#### `XM_GetUsbLogStatus` *(new in v2.0.0)*

Returns the current USB logging state as a detailed enum value.

  * **Syntax**
    ```c
    XmLogStatus_e XM_GetUsbLogStatus(void);
    ```
  * **Returns**: `XM_LOG_STATUS_IDLE`, `XM_LOG_STATUS_LOGGING`, `XM_LOG_STATUS_WARNING_QUEUE_FULL`, `XM_LOG_STATUS_ERROR_STOPPED`

#### `XM_SetUsbLogAutoTimestamp` *(new in v2.0.0)*

Automatically inserts a timestamp into log files.

  * **Syntax**
    ```c
    void XM_SetUsbLogAutoTimestamp(bool enabled);
    ```
  * **Parameters**
      * `enabled`: When `true`, the system time (ms) is automatically prepended to each sample

#### `XM_SetUsbLogRollingSize` *(new in v2.0.0)*

Sets the rolling (auto-split) size for log files. A new file is created automatically once the current file exceeds this size.

  * **Syntax**
    ```c
    void XM_SetUsbLogRollingSize(uint32_t size_mb);
    ```
  * **Parameters**
      * `size_mb`: File split threshold in MB. Range: 1–100, default: 10

-----

### 3.3. CDC Control (Debug & Streaming)

Functions for serial communication with a PC.

#### `XM_SendUsbData`

Sends a **data struct** to a PC terminal (e.g., TeraTerm).
Use this function when you want to transmit data on your own schedule without relying on `AGRB MON START`.

  * **Syntax**
    ```c
    bool XM_SendUsbData(const void* data, uint32_t len);
    ```
  * **Parameters**
      * `message`: String to transmit (null-terminated)
  * **Example**
    ```c
    typedef struct { uint32_t time; float angle; } MyLog_t;
    MyLog_t myLog;
    XM_SendUsbData(&myLog, sizeof(MyLog_t));
    ```

#### `XM_SendUsbDebugMessage`

Sends a **text string** to a PC terminal (e.g., TeraTerm). Use this like `printf` for debugging purposes.

  * **Syntax**
    ```c
    bool XM_SendUsbDebugMessage(const char* message);
    ```
  * **Parameters**
      * `message`: String to transmit (null-terminated)
  * **Example**
    ```c
    char buf[64];
    sprintf(buf, "Current State: %d\r\n", current_state);
    XM_SendUsbDebugMessage(buf);
    ```

#### `XM_IsUsbStreamConnected`

Checks whether a USB cable is connected to a PC and the virtual serial port is open.

  * **Syntax**
    ```c
    bool XM_IsUsbStreamConnected(void);
    ```

#### `XM_IsUsbStreamingActive` *(new in v2.0.0)*

Checks whether CDC streaming is currently active.

  * **Syntax**
    ```c
    bool XM_IsUsbStreamingActive(void);
    ```
  * **Returns**: `true` (streaming), `false` (inactive)

#### `XM_SetUsbAutoStream` *(new in v2.0.0)*

Enables automatic streaming of registered data when a PC connection is detected.

  * **Syntax**
    ```c
    void XM_SetUsbAutoStream(bool enable);
    ```
  * **Parameters**
      * `enable`: When `true`, streaming starts automatically upon connection detection

#### `XM_SetUsbStreamModuleId` *(new in v2.0.0)*

Sets the module ID used in the PhAI V2 protocol. Use this when integrating with PhAI Studio.

  * **Syntax**
    ```c
    void XM_SetUsbStreamModuleId(uint8_t module_id);
    ```
  * **Parameters**
      * `module_id`: PhAI protocol module identifier

#### `XM_GetUsbData`

Receives data from the PC (e.g., keyboard input).

  * **Syntax**
    ```c
    uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);
    ```
  * **Returns**: Number of bytes actually read

-----

### 3.4. System Interface

#### `XM_SetUsbCustomMeta`

Registers a Module ID and JSON metadata for use in PhAI Studio Custom mode.

  * **Syntax**
    ```c
    void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
    ```
  * **Parameters**

    | Name | Description |
    |------|-------------|
    | `module_id` | Custom Module ID (0xF0–0xFE) |
    | `json_str` | Channel definition JSON string (PhAI Studio V2 compatible) |

  * **Example**
    ```c
    XM_SetUsbCustomMeta(0xF0, "{\"ch\":[\"angle\",\"torque\",\"velocity\"]}");
    ```

#### `XM_SendUsbDataWithId`

Streams binary data over USB-CDC using the specified Module ID.

  * **Syntax**
    ```c
    bool XM_SendUsbDataWithId(const void* data, uint32_t len, uint8_t module_id);
    ```
  * **Parameters**

    | Name | Description |
    |------|-------------|
    | `data` | Pointer to the data to transmit |
    | `len` | Data length (bytes) |
    | `module_id` | Module ID (0x10: COMBINED, 0xF0–0xFE: Custom) |

  * **Returns**: `true` on success, `false` on failure (no connection, etc.)

> **Note**: Replaces the deprecated `XM_SendUsbData()`. Explicitly specifying a Module ID enables multi-stream support.

#### `XM_USB_ProcessPeriodic`

**[Internal — system use only]** Handles the USB logging and streaming logic.
Called automatically by `core_process` — **end users do not need to call this directly.**

  * **Syntax**
    ```c
    void XM_USB_ProcessPeriodic(void);
    ```

---

## Related Examples

### CDC (Serial Communication)

| Example | Difficulty | CDC Usage |
|---------|------------|-----------|
| [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) | Beginner | Sending debug messages |
| [07_CDC_Basic_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/07_CDC_Basic_Print/) | Beginner | Text messages |
| [08_CDC_Sensor_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/) | Beginner | sprintf formatting |
| [09_CDC_Stream](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) | Intermediate | PhAI V2 binary streaming |
| [18_Debug_Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/) | Intermediate | Health dashboard |

### MSC (Data Logging)

| Example | Difficulty | MSC Usage |
|---------|------------|-----------|
| [10_MSC_Manual_log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10_MSC_Manual_log/) | Legacy | Basic logging (see 10a) |
| [10a_MSC_Basic_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10a_MSC_Basic_Log/) | Beginner | Minimal struct + auto timestamp |
| [10b_MSC_Custom_Struct](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10b_MSC_Custom_Struct/) | Intermediate | Multiple types + manual timestamp |
| [10c_MSC_Advanced_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10c_MSC_Advanced_Log/) | Advanced | TSM + error recovery + file rolling |
| [19_Memory_Aware_Design](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/19_Memory_Aware_Design/) | Intermediate | Memory-efficient data management |

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| No messages appear in the serial terminal | PhAI Studio and a serial terminal are occupying the same COM port simultaneously | Close all other clients, then reconnect |
| No COM port appears in the OS | USB-C cable is charge-only (no data lines) | Use a data-capable cable and check Windows Device Manager |
| `XM_SendUsbDataWithId` frequently returns `false` | Transmit buffer full (drops occurring) | Reduce send frequency (e.g., 1 kHz → 100 Hz) or reduce struct size |
| User custom channel names do not appear in PhAI Studio | `XM_SetUsbCustomMeta` not called, or JSON syntax error | Add a single-line JSON call in Setup and validate with jsonlint |
| USB drive not recognized for MSC | Drive formatted as exFAT or NTFS | Use **FAT32** with 32 KB clusters |
| `sprintf` with `%f` prints integers | newlib-nano (default) does not support `%f` | Enable `Project Properties > MCU Settings > Use float with printf` |
| Korean characters are garbled in the terminal | Terminal encoding is not UTF-8 | PuTTY: Translation → UTF-8 |
| MSC logging started but `.bin` file is 0 bytes | `XM_SetUsbLogSource` was not called | Verify the call exists in Setup |
