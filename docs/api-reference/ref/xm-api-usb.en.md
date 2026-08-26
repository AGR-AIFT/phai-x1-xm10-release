# `xm_api_usb.h` — USB-CDC Real-Time Streaming

> **Target header**: `XM_FW/XM_API/xm_api_usb.h`
> **Related concept docs**: [05. USB Serial](../05-usb-connectivity.en.md)
> **Related examples**: [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) · [07_CDC_Basic_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) · [08_CDC_Sensor_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/08_CDC_Sensor_Print/) · [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) · [18_Debug_Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/)

This header defines the **real-time PC communication (CDC)** domain. You exchange text/binary data with a PC client such as PhAI Studio or PuTTY, and it works via registration-based automation (register a source in Setup → the System transmits it automatically on a periodic basis). Starting with Rev2.0, a host-profile API (`XM_USB_SetHostProfile`) has also been added that lets you **choose whether a single USB-CDC cable is used for PhAI Studio real-time streaming or a plain terminal**. (Production-inspection GUI support is handled automatically inside the board, so it is not a user option.)

> **USB memory (MSC) file logging was removed in v2.5.0.** Data capture now uses USB-CDC real-time streaming (PhAI Studio or the `PythonDecoder/CDC` samples in the repo); on-board storage (SD card) is planned for a future HW revision.

---

## When to use it

Use this API when you want to exchange real-time data with a PC over a serial connection. For the registration-based automation principle (register a source in Setup → the System processes it automatically on a periodic basis) and the big picture — common mistakes, example mapping — we recommend reading [05. USB Serial](../05-usb-connectivity.en.md) first. This page only covers the detailed **signatures/parameters of every function and type** declared in the header, and it also includes the new Rev2.0 **USB-CDC host-profile API**.

---

## Function list

**Data Source Registration**

| Function | One-line description |
|------|-----------|
| [`XM_SetUsbStreamSource`](#xm_setusbstreamsource) ⚠️ Deprecated | [CDC] Registers a data source to stream to the PC (legacy) |

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
| `void XM_USB_ProcessPeriodic(void)` | `core_process` | The periodic-processing engine for USB streaming logic. `core_process` calls this automatically, so user code does not need to call it directly |

> 🟢 **Rev 2.0 note** — The PRODUCTION auto-latch / DTR handling that used to live here in v2.3.1 (the former `XM_USB_RequestProductionLatch` / `XM_USB_OnDtrLost`) was moved inside the System layer (`usb_host_mode`) in v2.4.0 and is no longer in the public header. The only public user-facing API is [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile).

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CDC streaming basic API (`SendUsbDataWithId`/`SetUsbCustomMeta`/`SendUsbDebugMessage`) | ✅ | ✅ |
| `XM_USB_HostProfile_e` / `XM_USB_SetHostProfile` (PhAI Studio / terminal host-profile selection) | ❌ Absent | 🟢 Exclusive |
| PRODUCTION auto-entry / DTR handling (inside System `usb_host_mode`, not a public API) | ❌ Absent | 🟢 Exclusive (Internal) |

---

## Related Documents

- [05. USB Serial (concept)](../05-usb-connectivity.en.md) — CDC operating principles, common mistakes, example mapping
