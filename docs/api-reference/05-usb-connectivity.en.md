# API Reference: USB Connectivity

> 📌 **After reading this page** you will be able to handle USB-CDC text/binary send-receive end-to-end.
> ⏱️ Estimated reading time: 20 minutes
> 🧰 Prerequisites: [Ex.07–09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) CDC
> 🎯 Key functions: `XM_SendUsbDebugMessage` / `XM_SetUsbCustomMeta` / `XM_SendUsbDataWithId` / `XM_SetUsbStreamSource` / `XM_SetUsbAutoStream`
>
> ⚠️ **USB-CDC single-owner rule**: Never open PhAI Studio and a serial terminal (PuTTY, RealTerm, etc.) on the **same COM port simultaneously** — doing so causes a port conflict and data loss.

Detailed reference for the **USB-CDC communication API** defined in `xm_api_usb.h`.
XM10 connects to a PC as a virtual serial port (CDC) for real-time data transfer and debugging.

> **USB memory (MSC) file logging was removed in v2.5.0.** Data capture now uses USB-CDC real-time streaming — PhAI Studio, or the `xm10` tool in the repo (graphs + lossless `.xmlog` recording + CSV export, [guide](../getting-started/04-pc-data-tool.en.md)). On-board storage (SD card) is planned for a future HW revision.

This module uses a **registration-based automation** model: once you register the data struct you want to transmit, the system streams it to the PC in the background automatically.

-----

## 1. Operating Principle

The USB module follows a **configure-once, run-automatically** pattern to minimize user intervention.

### The Automation Cycle

1.  **Registration:** In `Control_Setup()`, tell the system the address of the data struct you want to transmit (e.g., `MyData`) via `XM_SetUsbStreamSource()`.
2.  **Control:** Enable transmission by calling `XM_SetUsbAutoStream(true)` or starting the stream from the PC side.
3.  **Automatic processing:** The `core_process` engine calls `XM_USB_ProcessPeriodic()` every 2 ms.
      * The system **automatically copies** data from the registered struct and streams it to the PC.
      * You do not need to call `Send()` in every loop iteration.

-----

## 2. Function Reference

### 2.1. Data Source Registration

Call this function first. If you skip registration, the system falls back to the default (the entire `XM` struct).

#### `XM_SetUsbStreamSource`

**[CDC]** Specifies the data source to be streamed to the PC in real time.

  * **Syntax**
    ```c
    void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: Pointer to the struct to transmit
      * `size`: Size of the struct
  * **Note**: Once streaming is active (via `XM_SetUsbAutoStream(true)` or a stream-start command from the PC side), data registered with this function is transmitted to the PC in **binary** form. Suitable for serial plotters and similar tools.

-----

### 2.2. CDC Control (Debug & Streaming)

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

### 2.3. System Interface

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

**[Internal — system use only]** Handles the USB streaming logic.
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
| [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) | Beginner | Sending debug messages |
| [07_CDC_Basic_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) | Beginner | Text messages |
| [08_CDC_Sensor_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/08_CDC_Sensor_Print/) | Beginner | sprintf formatting |
| [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) | Intermediate | PhAI V2 binary streaming |
| [18_Debug_Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/) | Intermediate | Health dashboard |

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| No messages appear in the serial terminal | PhAI Studio and a serial terminal are occupying the same COM port simultaneously | Close all other clients, then reconnect |
| No COM port appears in the OS | USB-C cable is charge-only (no data lines) | Use a data-capable cable and check Windows Device Manager |
| `XM_SendUsbDataWithId` frequently returns `false` | Transmit buffer full (drops occurring) | Reduce send frequency (e.g., 1 kHz → 100 Hz) or reduce struct size |
| User custom channel names do not appear in PhAI Studio | `XM_SetUsbCustomMeta` not called, or JSON syntax error | Add a single-line JSON call in Setup and validate with jsonlint |
| `sprintf` with `%f` prints integers | newlib-nano (default) does not support `%f` | Enable `Project Properties > MCU Settings > Use float with printf` |
| Korean characters are garbled in the terminal | Terminal encoding is not UTF-8 | PuTTY: Translation → UTF-8 |
