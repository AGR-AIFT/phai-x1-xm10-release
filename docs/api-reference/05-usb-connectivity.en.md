# API Reference: USB Connectivity

> 📌 **After reading this page** you will be able to handle USB-CDC text/binary send-receive end-to-end.
> ⏱️ Estimated reading time: 20 minutes
> 🧰 Prerequisites: [Ex.07–09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) CDC
> 🎯 Key functions: `XM_SendUsbDebugMessage` / `XM_SetUsbCustomMeta` / `XM_SendUsbDataWithId`
>
> ⚠️ **USB-CDC single-owner rule**: **Connect only one PC program at a time.** If PhAI Studio, the `xm10` tool or a serial terminal (PuTTY, RealTerm, etc.) open the same COM port together, the port conflicts and data is lost.

Detailed reference for the **USB-CDC communication API** defined in `xm_api_usb.h`.
XM10 connects to a PC as a virtual serial port (CDC) for real-time data transfer and debugging.

> 🟢 **Rev 2.0 only**: The USB memory logging API exists only on Rev2.0 (v2.9.0 or later) — see [06. USB Memory Logging](06-usb-data-logging.en.md). Rev1.1 has no USB memory logging.
>
> ⚠️ **There is only one USB-C port.** While a USB memory stick is plugged in, you cannot use PhAI Studio, the `xm10` tool, or firmware upload over USB. When logging is done, remove the stick and connect the PC. (not yet confirmed on the board) <!-- board-check -->
>
> This page covers USB-CDC, which streams data to a PC in real time. To receive it on a PC, use PhAI Studio, or the `xm10` tool in the repo (graphs + lossless `.xmlog` recording + CSV export, [guide](../getting-started/04-pc-data-tool.en.md)). PhAI Studio shows the automatically sent Total Data (0x20). PhAI Studio is still under development, so for now use the `xm10` tool to view and save your own (custom) data structs (the 0xF0–0xFE channels you send with `XM_SendUsbDataWithId` are such structs). On-board storage (SD card) is not supported at present.

In this module, the system streams Total Data (0x20) to the PC automatically in the background, and you send any extra channels yourself with `XM_SendUsbDataWithId()`.

> 🟢 **Rev 2.0 only**: The API that sends a struct together with field names, units and types (`XM_LOG_REGISTER` · `XM_LOG_SEND`) is described in [10. Struct Logging](10-struct-logging.en.md) (example [44_CDC_Struct_Logging](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/)).

-----

## 1. Operating Principle

The USB module follows a **configure-once, run-automatically** pattern to minimize user intervention.

### The Automation Cycle

Total Data (0x20) is sent automatically every 1 ms once a PC program opens the COM port. To send more values, use `XM_SetUsbCustomMeta()` (in Setup) + `XM_SendUsbDataWithId()` (in the loop) — see [Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/).

-----

## 2. Function Reference

> ⚠️ **Call the send functions from `Control_Loop()`.** This applies to `XM_SendUsbData()`, `XM_SendUsbDataWithId()` and `XM_SendUsbDebugMessage()`.
>
> * Rev2.0 (v2.9.0 or later): called inside an interrupt handler, they send nothing and return `false`. Do not call them from a background task you created with `XM_Task_*` ([09. Auxiliary Tasks + Data Sharing](09-task-creation.en.md)) either — the data you send can get corrupted.
> * Rev1.1 (v2.8.1): calling them inside an interrupt handler is not blocked. As before, call them from `Control_Loop()` only.

### 2.1. Data Source Registration

(Deprecated) You don't need to call it. If you skip registration, nothing is sent on this path — the whole `XM` state is carried by the separate, automatic Total Data (0x20) stream.

#### `XM_SetUsbStreamSource`

> ⚠️ **Deprecated** — replaced by the automatic Total Data (0x20) stream and `XM_SendUsbDataWithId`.

**[CDC]** Specifies the data source to be streamed to the PC in real time.

  * **Syntax**
    ```c
    void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: Pointer to the struct to transmit
      * `size`: Size of the struct
  * **Note**: Once streaming is active, data registered with this function is sent to the PC as PhAI packets (binary, default Module ID 0x10), so a serial plotter or terminal cannot read it.

-----

### 2.2. CDC Control (Debug & Streaming)

Functions for serial communication with a PC.

#### `XM_SendUsbData`

> ⚠️ **Deprecated** — replaced by the automatic Total Data (0x20) stream and `XM_SendUsbDataWithId`.

Wraps a data struct in a PhAI packet and sends it to the PC (not readable in a terminal). You call it yourself, whenever you want to send.

  * **Syntax**
    ```c
    bool XM_SendUsbData(const void* data, uint32_t len);
    ```
  * **Parameters**
      * `data`: Pointer to the struct to send
      * `len`: Data length (number of bytes)

#### `XM_SendUsbDebugMessage`

Sends a **text string** to a PC terminal (e.g., TeraTerm). Use this like `printf` for debugging purposes. `Control_Setup()` runs before USB is ready, so a message you send from there usually does not go out — send it from `Control_Loop()`.

> ⚠️ Use it for text you read in a plain terminal. For text-only output, call `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)` once in `Control_Setup()` on Rev2.0 (or `XM_SetUsbAutoStream(false)` on Rev1.1); the output then stays clean.
>
> * Rev2.0 (v2.9.0 or later): one 0x00 byte (null character) is appended to the text. It is appended with the `XM_USB_HOST_TERMINAL` profile too.
>   This keeps the data packets sent along with it from being damaged, so sending text while PhAI Studio or the `xm10` tool is decoding the stream does not lose a data packet. (not yet confirmed on the board) <!-- board-check -->
> * Rev1.1 (v2.8.1): same as before. No 0x00 is appended, and if you send text while the stream is being decoded, one data packet is lost per message.

  * **Syntax**
    ```c
    bool XM_SendUsbDebugMessage(const char* message);
    ```
  * **Parameters**
      * `message`: String to transmit (null-terminated). The length is counted in **bytes**, up to `XM_USB_DEBUG_MESSAGE_MAX_LEN` (1,021 bytes) — Korean and other non-ASCII text takes more bytes than characters (Rev2.0)
  * **Returns**: `true` = queued in the transmit buffer (says nothing about whether the PC received it). `false` =
      * Common: when the transmit buffer is full, or USB is not ready
      * Rev2.0 (v2.9.0 or later): when `message` is `NULL` or longer than 1,021 bytes, or when called inside an interrupt handler (nothing is sent). An empty string (`""`) sends just the one 0x00 byte and returns `true`
      * Rev1.1 (v2.8.1): when the string is empty
  * **Note**: With struct logging ([10. Struct Logging](10-struct-logging.en.md)) the limit is shorter: `false` is also returned when you send too much in a short time (counted together with `XM_SendUsbDataWithId()`), or when one message is too long. A message that is too long never goes out however long you wait — shorten it, or split it and send the pieces with a pause between them (Rev2.0)
  * **Example**
    ```c
    char buf[64];
    sprintf(buf, "Current State: %d\r\n", current_state);
    XM_SendUsbDebugMessage(buf);
    ```

#### `XM_IsUsbStreamConnected`

Returns whether the USB device (CDC) is ready: `true` once the cable is plugged in and USB initialization has finished, regardless of whether a PC program has opened the port. On Rev2.0 it is `false` while a USB memory stick is plugged in, and the send functions return `false` too. To check that the port is open and streaming, use `XM_IsUsbStreamingActive()` (on Rev2.0 with the `XM_USB_HOST_TERMINAL` profile, `XM_IsUsbStreamingActive()` stays `false` even when a terminal has the port open, while `XM_IsUsbStreamConnected()` stays `true`).

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

Sets the mode that streams Total Data (0x20) automatically as soon as a PC program opens the COM port. Default is `true` (on Rev2.0 it is turned off if you choose the `XM_USB_HOST_TERMINAL` profile). Plugging in the cable alone does not start it.

  * **Syntax**
    ```c
    void XM_SetUsbAutoStream(bool enabled);
    ```
  * **Parameters**
      * `enabled`: When `true`, streaming starts when the COM port is opened; when `false`, it waits for the `AGRB MON START` command (legacy)

#### `XM_SetUsbStreamModuleId` *(new in v2.0.0)*

> ⚠️ **Deprecated** — replaced by the automatic Total Data (0x20) stream and `XM_SendUsbDataWithId`.

Sets the Module ID used by the old-style stream (`XM_SetUsbStreamSource`, `XM_SendUsbData`); the default is `0x10`. Not needed for PhAI Studio.

  * **Syntax**
    ```c
    void XM_SetUsbStreamModuleId(uint8_t module_id);
    ```
  * **Parameters**
      * `module_id`: PhAI protocol module identifier
  * **Note**: On Rev2.0 (v2.9.0 or later), a system ID (`0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF`) is ignored and the previous ID stays in use. Rev1.1 (v2.8.1) has no such rule.

#### `XM_GetUsbData`

Receives data from the PC (e.g., keyboard input).

  * **Syntax**
    ```c
    uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);
    ```
  * **Returns**: Number of bytes actually read
  * **Note**: Within one tick of `Control_Loop()`, keep reading until it returns 0. Anything you leave behind is taken by the system later in the same tick and cannot be received again.

-----

### 2.3. System Interface

#### `XM_SetUsbCustomMeta`

Registers channel names and units (as JSON) for the User Custom channel (Module IDs `0xF0`–`0xFE`). The `xm10` tool uses them for graph titles and CSV column names.
On Rev2.0 (v2.9.0 or later), the channel names are sent again every time a PC program opens the COM port, so you do not need to re-plug the USB cable. (not yet confirmed on the board) <!-- board-check -->
The JSON string must be `XM_USB_CUSTOM_META_MAX_LEN` (1,007 bytes) or less. If it is longer, the function returns `false` and the names you registered earlier stay in place (shorten it if you have many channels or long names). Only one registration is kept (the last successful call overwrites the previous one). On Rev2.0, nothing is sent if you choose the `XM_USB_HOST_TERMINAL` profile.

> Rev1.1 (v2.8.1) is the same as before. There is no return value (`void`), the JSON can be up to 512 bytes (if it is longer, it is silently not sent), and the channel names arrive once when you connect USB. If they are missing, re-plug the USB cable and connect again.

  * **Syntax**
    ```c
    bool XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
    ```
  * **Parameters**

    | Name | Description |
    |------|-------------|
    | `module_id` | Custom Module ID (0xF0–0xFE) |
    | `json_str` | JSON array string defining the channels (`false` if `NULL`) |

  * **Returns**: `true` = registered, `false` = `json_str` is `NULL` or longer than 1,007 bytes (the names registered earlier stay in place). If you use struct logging ([10. Struct Logging](10-struct-logging.en.md)), calling it outside `Control_Setup()` also returns `false` (Rev2.0). Rev1.1 returns nothing (`void`)

  * **Example**
    ```c
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"angle\",\"unit\":\"deg\"},"
        "{\"name\":\"torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"velocity\",\"unit\":\"deg/s\"}]");
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
    | `module_id` | Module ID (user channels use `0xF0`–`0xFE`) |

  * **Returns**: `true` = queued in the transmit buffer (says nothing about whether the PC received it). `false` =
      * Common: when the transmit buffer is full, USB is not ready, or the arguments are invalid (NULL pointer, length 0, or more than 1,020 bytes)
      * Rev2.0 (v2.9.0 or later): when `module_id` is a system ID (`0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF`), or when called inside an interrupt handler (nothing is sent). Send your own data with `0xF0`–`0xFE`

> **Note**: Replaces the deprecated `XM_SendUsbData()`. Explicitly specifying a Module ID enables multi-stream support.
>
> With struct logging ([10. Struct Logging](10-struct-logging.en.md)) (Rev2.0): you cannot send with the Module ID of a registered struct using this function (`false`), and sending too much with this function and `XM_SendUsbDebugMessage()` in a short time (they are counted together), or too large a block at once, also returns `false`. Data that is too large never goes out however long you wait — send it with struct logging instead.
>
> Rev1.1 (v2.8.1): sending with a system ID is not blocked. Still, send your own data with `0xF0`–`0xFE`.

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
| [44_CDC_Struct_Logging](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) 🟢 **Rev 2.0 only** | Intermediate | Struct logging |

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| No messages appear in the serial terminal | Another program (PhAI Studio, the `xm10` tool, etc.) and the serial terminal are occupying the same COM port simultaneously | Close all other clients, then reconnect |
| No COM port appears in the OS | USB-C cable is charge-only (no data lines) | Use a data-capable cable and check Windows Device Manager |
| `XM_SendUsbDataWithId` frequently returns `false` | Transmit buffer full (drops occurring) — it always returns `false` if the struct is larger than 1,020 bytes | Reduce send frequency (e.g., 1 kHz → 100 Hz) or reduce struct size (1,020 bytes or less) |
| The send functions (`XM_SendUsbDataWithId` · `XM_SendUsbDebugMessage` · `XM_SendUsbData`) always return `false` (Rev2.0) | Called inside an interrupt handler, `module_id` is a system ID (`0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF`), or a debug message is longer than 1,021 bytes | Call them from `Control_Loop()`, send your own data with `0xF0`–`0xFE`, and split long messages |
| My struct (0xF0–0xFE) does not show up in PhAI Studio | PhAI Studio shows the automatically sent Total Data (0x20) | PhAI Studio is still under development, so for now use the `xm10` tool to view and save your own (custom) data structs |
| Channel names show as `ch0, ch1…` in `xm10` | `XM_SetUsbCustomMeta` not called, the JSON is not a valid array (including syntax errors), or the JSON is longer than the limit (Rev2.0: 1,007 bytes, Rev1.1: 512 bytes) | Register a one-line JSON array in Setup (validate it with jsonlint, keep it within the limit). On Rev2.0, if `XM_SetUsbCustomMeta()` returns `false`, the string was `NULL` or longer than 1,007 bytes |
| Names are registered but `xm10` still shows `ch0, ch1…` | The channel names were not received — Rev2.0 sends them every time a PC program opens the COM port, Rev1.1 sends them once when USB connects | Common: connect only one PC program at a time. Rev2.0: disconnect `xm10` and connect again. (not yet confirmed on the board) Rev1.1: re-plug the USB cable and connect `xm10` again. | <!-- board-check -->
| `sprintf` with `%f` prints integers | newlib-nano (default) does not support `%f` | Enable `Project Properties > MCU Settings > Use float with printf` |
| Korean characters are garbled in the terminal | Terminal encoding is not UTF-8 | PuTTY: Translation → UTF-8 |
