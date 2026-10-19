# `xm_api_usb.h` — USB-CDC Real-Time Streaming

> **Target header**: `XM_FW/XM_API/xm_api_usb.h`
> **Related concept docs**: [05. USB Serial](../05-usb-connectivity.en.md)
> **Related examples**: [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) · [07_CDC_Basic_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) · [08_CDC_Sensor_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/08_CDC_Sensor_Print/) · [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) · [18_Debug_Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/)

This header defines the **real-time PC communication (CDC)** domain. You exchange text/binary data with a PC client such as PhAI Studio or PuTTY: the System sends Total Data (0x20) automatically, and you send additional channels with `XM_SendUsbDataWithId()`. Starting with Rev2.0, a host-profile API (`XM_USB_SetHostProfile`) has also been added that lets you **choose whether a single USB-CDC cable is used for PhAI Studio real-time streaming or a plain terminal**.

> 🟢 **Rev 2.0 only**: The USB memory logging API exists only on Rev2.0 (v2.9.0 or later) — concept guide [06. USB Memory Logging](../06-usb-data-logging.en.md), function reference [`xm_api_usb_msc.h`](xm-api-usb-msc.en.md). Rev1.1 has no USB memory logging.
>
> ⚠️ **There is only one USB-C port.** While a USB memory stick is plugged in, you cannot use PhAI Studio, the `xm10` tool, or firmware upload over USB. When logging is done, remove the stick and connect the PC. (not yet confirmed on the board) <!-- board-check -->
>
> 🟢 **Rev 2.0 only**: The functions that send a struct together with field names, units and types are in [`xm_api_logging.h`](xm-api-logging.en.md) — concept guide [10. Struct Logging](../10-struct-logging.en.md).
>
> This header covers USB-CDC, which streams data to a PC in real time. To receive it on a PC, use PhAI Studio, or the `xm10` tool in the repo (graphs + lossless `.xmlog` recording + CSV export, [guide](../../getting-started/04-pc-data-tool.en.md)). PhAI Studio shows Total Data (0x20). PhAI Studio is still under development, so for now use the `xm10` tool to view and save your own (custom) data structs (0xF0–0xFE). On-board storage (SD card) is not supported at present.

---

## When to use it

Use this API when you want to exchange real-time data with a PC over a serial connection. For how the automatic Total Data (0x20) stream and the extra-channel path (`XM_SendUsbDataWithId`) work, and the big picture — common mistakes, example mapping — we recommend reading [05. USB Serial](../05-usb-connectivity.en.md) first. This page only covers the detailed **signatures/parameters of every function and type** declared in the header, and it also includes the new Rev2.0 **USB-CDC host-profile API**.

---

## Function list

**Data Source Registration**

| Function | One-line description |
|------|-----------|
| [`XM_SetUsbStreamSource`](#xm_setusbstreamsource) ⚠️ Deprecated | [CDC] Registers a data source to stream to the PC (legacy) |

**CDC — Connection · Streaming Status**

| Function | One-line description |
|------|-----------|
| [`XM_IsUsbStreamConnected`](#xm_isusbstreamconnected) | Whether the USB device (CDC) is ready |
| [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive) | Whether streaming is active |
| [`XM_SetUsbAutoStream`](#xm_setusbautostream) | Turns Auto-Stream mode on/off |

**CDC — Host Profile** 🟢 Rev 2.0 only

| Function | One-line description |
|------|-----------|
| [`XM_USB_SetHostProfile`](#xm_usb_sethostprofile--rev-20-only) 🟢 | Selects the USB-CDC host profile (PhAI Studio / terminal) |

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

Checks whether the USB device (CDC) is ready: `true` once the cable is plugged in and USB initialization has finished — regardless of whether a PC program has opened the COM port. On Rev2.0 it is `false` while a USB memory stick is plugged in, and the send functions return `false` too. To check that the port is open and streaming, use [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive) (on Rev2.0 with the `XM_USB_HOST_TERMINAL` profile, `XM_IsUsbStreamingActive()` stays `false` even when a terminal has the port open, while `XM_IsUsbStreamConnected()` stays `true`).

**Parameters**: none

**Return value**: `bool` — `true` once the USB device is ready (also while the port is closed). On Rev2.0 it is `false` while a USB memory stick is plugged in

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context.

**See also**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

### `XM_IsUsbStreamingActive`

```c
bool XM_IsUsbStreamingActive(void);
```

Checks whether streaming is active. In Auto-Stream mode this automatically becomes `true` when a PC program opens the COM port — plugging in the cable alone is not enough; in Legacy mode it becomes `true` when `"AGRB MON START"` is received.

**Parameters**: none

**Return value**: `bool` — `true`: streaming active (data being sent), `false`: idle

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context.

**See also**: [`XM_SetUsbAutoStream`](#xm_setusbautostream)

---

### `XM_SetUsbAutoStream`

```c
void XM_SetUsbAutoStream(bool enabled);
```

Sets Auto-Stream mode. Default is ON — streaming starts automatically when a PC program opens the COM port (on Rev2.0, only with the default `XM_USB_HOST_PHAI_STUDIO` profile).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `enabled` | `bool` | `true`: automatic streaming when the COM port is opened (default). `false`: wait for the `"AGRB MON START"` command (Legacy Python compatibility) |

**Return value**: none (`void`)

**⚠️ Call context**: Assume `Control_Setup()` context.

**See also**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

<a id="xm_usb_sethostprofile"></a>
### `XM_USB_SetHostProfile` 🟢 Rev 2.0 only

```c
void XM_USB_SetHostProfile(XM_USB_HostProfile_e profile);
```

> 🟢 **Rev 2.0 only** — This function and the [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e--rev-20-only) type itself do not exist at all in the Rev1.1 header.

Specifies the kind of PC app attached to this board's USB-CDC cable. **If never called, the default is `XM_USB_HOST_PHAI_STUDIO`**, so leaving the profile untouched keeps the existing PhAI Studio real-time streaming behavior unchanged.

To use a plain serial terminal / custom program (Tera Term · VS Code Serial Monitor · your own Python GUI, etc.) with **clean text / custom IO only**, call `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)` once in `Control_Setup()`. Automatic Total Data and channel-name sending are turned off, and this setting **is kept even if you re-plug the cable**. Do not set it when you receive with the `xm10` tool or PhAI Studio (keep the default).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `profile` | [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e--rev-20-only) | `XM_USB_HOST_PHAI_STUDIO` (default) or `XM_USB_HOST_TERMINAL` |

**Return value**: none (`void`)

**⚠️ Call context**: Assume `Control_Setup()` context. Call it once.

**⚠️ When you use struct logging**: call it before the first `XM_LOG_REGISTER`. Once any struct is registered, this call is ignored (the profile does not change). Also, even with `XM_USB_HOST_TERMINAL`, data packets you send yourself with `XM_SendUsbDataWithId()` and the data and field information of struct logging are still sent, and show up in the terminal as garbled characters. For text-only output, use neither. ([`xm_api_logging.h`](xm-api-logging.en.md), Rev2.0)

**Example**

```c
void Control_Setup(void) {
    // When you just want to read text in a plain serial terminal (the Ex.07 / Ex.08 way)
    XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL);
}
```

**See also**: [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e--rev-20-only), [05. USB Serial](../05-usb-connectivity.en.md)

---

### `XM_SendUsbData`

```c
bool XM_SendUsbData(const void* data, uint32_t len);
```

> ⚠️ **Deprecated** — Replaced by [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid). Specify the Module ID explicitly.

Sends data over USB CDC, automatically wrapped in a PhAI packet (binary).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `data` | `const void*` | Pointer to the user struct to send (a float array or a 4-byte-aligned struct) |
| `len` | `uint32_t` | Number of data bytes |

**Return value**: `bool` — `true`: queued in the transmit buffer (says nothing about whether the PC received it), `false`: buffer full, USB not ready, or invalid arguments (NULL pointer, length 0, or more than 1,020 bytes). On Rev2.0 (v2.9.0 or later) it also returns `false` when called inside an interrupt handler (nothing is sent). With struct logging it also returns `false` when the Module ID is the ID of a registered struct, when you send too much in a short time, or when one block is too large (Rev2.0)

**⚠️ Call context**: Safe to call within the 1 ms cycle of `Control_Loop()` (non-blocking). Do not call it from a background task you created with `XM_Task_*` — the data you send can get corrupted (Rev2.0). On Rev1.1 (v2.8.1), calling it inside an interrupt handler is not blocked, but call it from `Control_Loop()` only.

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
| `module_id` | `uint8_t` | Module ID (default `0x10`) |

**Return value**: none (`void`)

**⚠️ System IDs**: On Rev2.0 (v2.9.0 or later), `0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF` are ignored and the previous ID stays in use. Rev1.1 (v2.8.1) has no such rule.

**See also**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SetUsbCustomMeta`

```c
bool XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
```

Registers metadata (channel names/units, etc.) for the User Custom channel. The `xm10` tool uses the names for graph titles and CSV columns (on Rev2.0, only with the default `XM_USB_HOST_PHAI_STUDIO` profile).
On Rev2.0 (v2.9.0 or later), the channel names are sent again every time a PC program opens the COM port, so you do not need to re-plug the USB cable. (not yet confirmed on the board) <!-- board-check -->

> Rev1.1 (v2.8.1) is the same as before. There is no return value (`void`), the JSON can be up to 512 bytes (if it is longer, it is silently not sent), and the channel names arrive once when you connect USB. If they are missing, re-plug the USB cable and connect again.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `module_id` | `uint8_t` | Target Module ID (`0xF0`~`0xFE`) |
| `json_str` | `const char*` | JSON array string of channel definitions (NULL-terminated; a string literal is recommended). On Rev2.0, **[`XM_USB_CUSTOM_META_MAX_LEN`](#xm_usb_custom_meta_max_len) (1,007 bytes) or less** — if it is longer, or `NULL`, the function returns `false` and the names registered earlier stay in place (you cannot clear the names with `NULL`). On Rev1.1, **512 bytes or less** |

**Return value**: `bool` — `true`: registered, `false`: `json_str` is `NULL` or longer than 1,007 bytes. If you use struct logging, calling it outside `Control_Setup()` also returns `false` (Rev2.0). Rev1.1 returns nothing (`void`)

**⚠️ Call context**: Call once from `Control_Setup()`. If you use struct logging, you can call it only inside `Control_Setup()` (Rev2.0).

**⚠️ Pointer lifetime caution**: The `json_str` pointer must remain valid for the lifetime of the program (it is not copied internally) — use a string literal, not a stack-local variable.

**⚠️ Single slot**: Only one Module ID's metadata is kept (the last successful call overwrites the previous one). To label multiple channel groups, gather the channels under a single Module ID for registration.

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

**Return value**: `bool` — `true`: queued in the transmit buffer (says nothing about whether the PC received it). `false`:

- Common: when the transmit buffer is full, USB is not ready, or the arguments are invalid (NULL pointer, length 0, or more than 1,020 bytes)
- Rev2.0 (v2.9.0 or later): when `module_id` is a system ID (`0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF`), or when called inside an interrupt handler (nothing is sent). Send your own data with `0xF0`–`0xFE`
- With struct logging (Rev2.0): when you send with the Module ID of a registered struct, when you send too much with this function and `XM_SendUsbDebugMessage()` in a short time (they are counted together), or when one block is too large. Data that is too large never goes out however long you wait, so send it with struct logging ([10. Struct Logging](../10-struct-logging.en.md))

> Rev1.1 (v2.8.1): sending with a system ID is not blocked. Still, send your own data with `0xF0`–`0xFE`.

**⚠️ Call context**: Call from within `Control_Loop()` (non-blocking). Calling it on every tick is not mandatory — call it only when needed. Do not call it from a background task you created with `XM_Task_*` — the data you send can get corrupted (Rev2.0). On Rev1.1 (v2.8.1), calling it inside an interrupt handler is not blocked, but call it from `Control_Loop()` only.

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

> ⚠️ Use it for text you read in a plain terminal. For text-only output, call `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)` once in `Control_Setup()` on Rev2.0 (or `XM_SetUsbAutoStream(false)` on Rev1.1); the output then stays clean.
>
> * Rev2.0 (v2.9.0 or later): one 0x00 byte (null character) is appended to the text. It is appended with the `XM_USB_HOST_TERMINAL` profile too.
>   This keeps the data packets sent along with it from being damaged, so sending text while PhAI Studio or the `xm10` tool is decoding the stream does not lose a data packet. (not yet confirmed on the board) <!-- board-check -->
> * Rev1.1 (v2.8.1): same as before. No 0x00 is appended, and if you send text while the stream is being decoded, one data packet is lost per message.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `message` | `const char*` | String to send (null-terminated). The length is counted in **bytes**, up to [`XM_USB_DEBUG_MESSAGE_MAX_LEN`](#xm_usb_debug_message_max_len) (1,021 bytes) — Korean and other non-ASCII text takes more bytes than characters (Rev2.0) |

**Return value**: `bool` — `true`: queued in the transmit buffer (says nothing about whether the PC received it). `false`:

- Common: when the transmit buffer is full, or USB is not ready
- Rev2.0 (v2.9.0 or later): when `message` is `NULL` or longer than 1,021 bytes, or when called inside an interrupt handler (nothing is sent). An empty string (`""`) sends just the one 0x00 byte and returns `true`
- Rev1.1 (v2.8.1): when the string is empty

With struct logging the limit is shorter: `false` is also returned when you send too much in a short time (counted together with `XM_SendUsbDataWithId()`), or when one message is too long — a message that is too long never goes out however long you wait, so shorten it, or split it and send the pieces with a pause between them (Rev2.0, [10. Struct Logging](../10-struct-logging.en.md)).

**⚠️ Call context**: Call it from within `Control_Loop()`. Non-blocking. `Control_Setup()` runs before USB is ready, so a message you send from there usually does not go out. Do not call it from a background task you created with `XM_Task_*` — the data you send can get corrupted (Rev2.0). On Rev1.1 (v2.8.1), calling it inside an interrupt handler is not blocked, but call it from `Control_Loop()` only.

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

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context. Within one tick of `Control_Loop()`, keep reading until it returns 0 — anything you leave behind is taken by the system later in the same tick and cannot be received again. Error states other than a 0 return value are not documented — we recommend verifying empirically before use.

---

## Types / macros

<a id="xm_usb_hostprofile_e"></a>
### `XM_USB_HostProfile_e` 🟢 Rev 2.0 only

> Not defined in the Rev1.1 header.

The USB-CDC host profile you set to say what a single board/cable is used for. There are two choices, set with [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile--rev-20-only).

| Value | Description |
|----|------|
| `XM_USB_HOST_PHAI_STUDIO` = 0 | **Default** — for receiving with PhAI Studio or the `xm10` tool (automatic Total Data sending + channel-name sending ON) |
| `XM_USB_HOST_TERMINAL` = 1 | Plain terminal / custom program — automatic sending OFF, user-initiated TX only |

> 🟢 **Rev 2.0 note** — The only public user-facing API for choosing the USB-CDC host profile is [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile--rev-20-only).

<a id="xm_usb_custom_meta_max_len"></a>
### `XM_USB_CUSTOM_META_MAX_LEN` 🟢 Rev 2.0 only

> Not defined in the Rev1.1 header. The JSON limit on Rev1.1 is 512 bytes.

```c
#define XM_USB_CUSTOM_META_MAX_LEN  (1007U)
```

The maximum length of the JSON string you can pass to [`XM_SetUsbCustomMeta()`](#xm_setusbcustommeta) (in bytes, not counting the terminating `'\0'`). If it is longer, `XM_SetUsbCustomMeta()` returns `false`.

<a id="xm_usb_debug_message_max_len"></a>
### `XM_USB_DEBUG_MESSAGE_MAX_LEN` 🟢 Rev 2.0 only

> Not defined in the Rev1.1 header.

```c
#define XM_USB_DEBUG_MESSAGE_MAX_LEN  (1021U)
```

The maximum length of the string you can send at once with [`XM_SendUsbDebugMessage()`](#xm_sendusbdebugmessage) (in bytes, not counting the terminating `'\0'`). If it is longer, the function returns `false`. With struct logging, a shorter limit applies.

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CDC streaming basic API (`SendUsbDataWithId`/`SetUsbCustomMeta`/`SendUsbDebugMessage`) | ✅ | ✅ |
| `XM_USB_HostProfile_e` / `XM_USB_SetHostProfile` (PhAI Studio / terminal host-profile selection) | ❌ Absent | 🟢 Exclusive |
| `XM_SetUsbCustomMeta` — return value · JSON limit | `void` · 512 bytes (if longer, silently not sent) | `bool` · 1,007 bytes (`XM_USB_CUSTOM_META_MAX_LEN`; `false` if longer or `NULL`) |
| When the channel names are sent | Once, when USB connects | Every time a PC program opens the COM port. (not yet confirmed on the board) | <!-- board-check -->
| `XM_SendUsbDebugMessage` | Sends the string only · an empty string returns `false` · sending while the stream is being decoded loses one data packet per message | One 0x00 byte appended · 1,021-byte limit (`XM_USB_DEBUG_MESSAGE_MAX_LEN`) · an empty string returns `true` |
| System IDs (`0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF`) | Not blocked | `XM_SendUsbDataWithId()` returns `false`, `XM_SetUsbStreamModuleId()` ignores them |
| Calling a send function inside an interrupt handler | Not blocked | Sends nothing and returns `false` |
| 3 system-internal declarations in the header | Still present (do not call them) | Removed — if your code used them, delete that code. If it stays, it fails to compile ([v2.9.0 release notes](../../release-notes/v2.9.0.en.md)) |
| Struct logging ([`xm_api_logging.h`](xm-api-logging.en.md)) | ❌ Absent | 🟢 Exclusive |
| USB memory logging ([`xm_api_usb_msc.h`](xm-api-usb-msc.en.md)) | ❌ Absent | 🟢 Exclusive |

---

## Related Documents

- [05. USB Serial (concept)](../05-usb-connectivity.en.md) — CDC operating principles, common mistakes, example mapping
- [10. Struct Logging (concept)](../10-struct-logging.en.md) · [`xm_api_logging.h`](xm-api-logging.en.md) — sending structs with field names, units and types 🟢 **Rev 2.0 only**
- [06. USB Memory Logging (concept)](../06-usb-data-logging.en.md) · [`xm_api_usb_msc.h`](xm-api-usb-msc.en.md) — saving to a USB memory stick 🟢 **Rev 2.0 only**
