# `xm_api_logging.h` — USB-CDC Struct Logging

> 🟢 **Rev 2.0 only** *(new in v2.9.0)* — This header and `xm_api_logging_raw.h` ship only in the XM10 Rev 2.0 SDK. The Rev 1.1 SDK (`XM10_SDK/Rev1.1/`) has neither header, so including them in a Rev 1.1 project fails to build. To check your board revision, see the [board revision comparison](../../hardware/README.en.md#board-revision-comparison).
>
> - **Target header**: `XM_FW/XM_API/xm_api_logging.h` · `XM_FW/XM_API/xm_api_logging_raw.h`
> - **Related concept doc**: [10. Struct Logging](../10-struct-logging.en.md)
> - **Related example**: [Ex.44 CDC Struct Logging](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) — sends two structs to the PC with names, units and types

---

## When to use it

Use this API when you send your own struct to the PC together with its field names, units and types. For the big picture — how it differs from the existing way (`XM_SetUsbCustomMeta` + `XM_SendUsbDataWithId`), the limits, what you see on the PC, and common mistakes — we recommend reading [10. Struct Logging](../10-struct-logging.en.md) first. This page only covers the **signatures, parameters and return values of every macro, function and type** declared in the header.

`xm_api.h` pulls in this header, so the names `XM_LOG_*`, `XM_FIELD*` and `XM_Log_*` belong to the SDK. Do not create names of your own that match them.

### include

```c
#include "xm_api.h"                // Also pulls in xm_api_logging.h
#include "xm_api_logging_raw.h"    // Always include this in a .c file that uses the XM_LOG_* macros
```

`xm_api.h` includes `xm_api_logging.h` but not `xm_api_logging_raw.h`. The prototypes of the functions that the `XM_LOG_REGISTER` and `XM_LOG_SEND` macros call are in that header, so if it is missing you get an `implicit declaration of function` build error at the macro line.

### Basic usage flow

```c
#include "xm_api.h"
#include "xm_api_logging_raw.h"

typedef struct {
    float    hip_angle;
    int16_t  emg[8];
    uint32_t mode;
} MyLog_t;                                          // 24 B — the size is a multiple of 4

XM_LOG_DECLARE(MyLog, MyLog_t);                     // Token MyLog ↔ struct MyLog_t

XM_LOG_FIELDS(MyLog) = {
    XM_FIELD(MyLog, hip_angle, "hip_angle", "deg", 1.0f),
    XM_FIELD_ARRAY(MyLog, emg, "emg", "mV", 1.0f),
    XM_FIELD(MyLog, mode, "mode", "", 1.0f),
};

void Control_Setup(void)
{
    if (!XM_LOG_REGISTER(MyLog, 0xF0, 1000)) {
        // Report a registration failure with an LED — in Control_Setup() the USB is usually not ready yet, so no message goes out
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    }
}

void Control_Loop(void)
{
    MyLog_t s = { 0 };
    (void)XM_LOG_SEND(MyLog, &s);                   // Calling earlier than the registered period sends nothing and returns false
}
```

Use the four macros (`XM_LOG_DECLARE`, `XM_LOG_FIELDS`, `XM_LOG_REGISTER`, `XM_LOG_SEND`) in the same `.c` file. The macros are for C files only. For wrong usage — an unsupported member type, a size that is not a multiple of 4, a pointer to a different struct — the compiler points at that line.

---

## Function and macro list

**Struct definition · registration · sending (macros)**

| Macro | One-line description |
|------|-----------|
| [`XM_LOG_DECLARE`](#xm_log_declare) | Ties a token to a struct type (near the top of the `.c` file) |
| [`XM_LOG_FIELDS`](#xm_log_fields) | Defines the field description array |
| [`XM_FIELD`](#xm_field) | One line for a scalar member |
| [`XM_FIELD_ARRAY`](#xm_field_array) | One line for an array member |
| [`XM_LOG_REGISTER`](#xm_log_register) | Registers a struct (`Control_Setup()`) |
| [`XM_LOG_SEND`](#xm_log_send) | Sends one registered struct (`Control_Loop()`) |

**Settings · diagnostics (functions)**

| Function | One-line description |
|------|-----------|
| [`XM_Log_SetMinStallMs`](#xm_log_setminstallms) | Sets the target time (ms) for which data is kept when the PC program pauses briefly |
| [`XM_Log_GetMinStallMs`](#xm_log_getminstallms) | The target time (ms) in use — for diagnostics |
| [`XM_Log_EnableLinkHealth`](#xm_log_enablelinkhealth) | Turns link status information on / off |
| [`XM_Log_GetAdmittedWireBps`](#xm_log_getadmittedwirebps) | The share of the send budget already committed (bytes/s) — for diagnostics |
| [`XM_Log_GetStats`](#xm_log_getstats) | Copies the observation counters |

**Support header `xm_api_logging_raw.h` — do not call directly**

| Function | One-line description |
|------|-----------|
| [`XM_Log_RegisterSchemaRaw`](#xm_log_registerschemaraw) | The function `XM_LOG_REGISTER` calls |
| [`XM_Log_SendRaw`](#xm_log_sendraw) | The function `XM_LOG_SEND` calls |

---

## Function and macro details

### `XM_LOG_DECLARE`

```c
XM_LOG_DECLARE(tok, Type);
```

Ties a token to a struct type. Write it once near the top of the `.c` file. The token becomes the struct name.

**Parameters**

| Name | Description |
|------|------|
| `tok` | Token (short name) — at most 11 characters (`XM_LOG_MAX_TOKEN_LEN`), letters, digits and `_` |
| `Type` | The struct type to send — size is a multiple of 4 and at most 1,008 B |

**Return value**: none (declaration macro)

In the cases below, you get a compile error at this line.

| Compile error | Cause |
|------|------|
| `... must be a multiple of 4 - add padding` | The struct size is not a multiple of 4 |
| `... exceeds XM_LOG_MAX_STRUCT_SIZE` | The struct size exceeds 1,008 B |
| `token ... longer than 11 chars` | The token is longer than 11 characters |

To fix the size, add a `uint8_t` array at the end. You do not have to list that array with `XM_FIELD`.

**⚠️ Call context**: Write it at file scope (outside any function). Use the same token as the `XM_LOG_FIELDS`, `XM_LOG_REGISTER` and `XM_LOG_SEND` in the same `.c` file.

**Example**

```c
typedef struct {
    float angle;      // [deg]
    float torque;     // [Nm]
} JointLog_t;         // 8 B

XM_LOG_DECLARE(JointLog, JointLog_t);
```

**See also**: [`XM_LOG_FIELDS`](#xm_log_fields), [`XM_LOG_REGISTER`](#xm_log_register)

---

### `XM_LOG_FIELDS`

```c
XM_LOG_FIELDS(tok) = { /* XM_FIELD(...), XM_FIELD_ARRAY(...) */ };
```

Defines the field description array. Follow it with `= { XM_FIELD(...), ... };`. Inside the braces, use only `XM_FIELD` and `XM_FIELD_ARRAY` of the **same token**. The compiler cannot check this rule.

**Parameters**

| Name | Description |
|------|------|
| `tok` | The token from `XM_LOG_DECLARE` |

**Return value**: none (definition macro)

There are 1 to 90 fields. With 0 fields, or more than 90, you get a compile error (`field count must be 1..90`) at the `XM_LOG_REGISTER` line.

**⚠️ Call context**: Write it at file scope (outside any function). It must come before `XM_LOG_REGISTER`.

**Example**

```c
XM_LOG_FIELDS(JointLog) = {
    XM_FIELD(JointLog, angle,  "angle",  "deg", 1.0f),
    XM_FIELD(JointLog, torque, "torque", "Nm",  1.0f),
};
```

**See also**: [`XM_FIELD`](#xm_field), [`XM_FIELD_ARRAY`](#xm_field_array)

---

### `XM_FIELD`

```c
XM_FIELD(tok, member, nm, un, sc)
```

One line for a scalar member. The type and position are decided from the member.

**Parameters**

| Name | Description |
|------|------|
| `tok` | The token from `XM_LOG_DECLARE` |
| `member` | The struct member name |
| `nm` | Field name — up to 15 letters, digits and symbols (ASCII) (Korean letters and characters such as `°` make the registration `false`) |
| `un` | Unit — up to 7 letters, digits and symbols (ASCII); `""` if none (e.g., `"deg"`) |
| `sc` | Scale — physical value = stored value × `sc` (usually `1.0f`) |

**Return value**: none (an `XM_FieldDesc_t` initializer expression)

The supported member types are `float`, `double`, `bool`, `int`, `unsigned` and `int8_t` ~ `uint64_t`. An enumeration member is accepted as the integer type of the same size. Any other type (`char`, pointer, struct, array, etc.) is a compile error (`unsupported member type`).

**⚠️ Call context**: Use it only inside the braces of `XM_LOG_FIELDS`.

**Example**

```c
XM_FIELD(JointLog, angle, "angle", "deg", 1.0f)
```

**See also**: [`XM_FIELD_ARRAY`](#xm_field_array), [`XM_FieldType_e`](#xm_fieldtype_e)

---

### `XM_FIELD_ARRAY`

```c
XM_FIELD_ARRAY(tok, member, nm, un, sc)
```

One line for an array member. The element type and the count (1 ~ 255) are decided from the array. A pointer member or an array of 256 or more elements is a compile error. The arguments are the same as for [`XM_FIELD`](#xm_field).

**Parameters**: Same as [`XM_FIELD`](#xm_field).

**Return value**: none (an `XM_FieldDesc_t` initializer expression)

**⚠️ Call context**: Use it only inside the braces of `XM_LOG_FIELDS`.

**Example**

```c
XM_FIELD_ARRAY(MyLog, emg, "emg", "mV", 1.0f)     // int16_t emg[8] → 8 elements
```

**See also**: [`XM_FIELD`](#xm_field)

---

### `XM_LOG_REGISTER`

```c
XM_LOG_REGISTER(tok, module_id, nominal_rate_hz)   // → bool
```

Registers a struct. Call it inside `Control_Setup()`. The function it actually calls is [`XM_Log_RegisterSchemaRaw`](#xm_log_registerschemaraw).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `tok` | token | The token from `XM_LOG_DECLARE` (`XM_LOG_FIELDS` must come before it) |
| `module_id` | `uint8_t` | Module ID to send this struct with — `0xF0` ~ `0xFE` |
| `nominal_rate_hz` | `uint16_t` | Send rate (Hz) — even if you call `XM_LOG_SEND` more often, data does not go out faster than this |

The interval is rounded up to a 1 ms step, so if the rate is not a divisor of 1,000, data goes out less often (e.g., 300 Hz → 250 Hz, 600 Hz → 500 Hz). Data never goes out faster than 1,000 Hz.

**Return value**: `bool` — `true`: registered. `false`: see the table below.

| `false` when | Fix |
|------|------|
| This token is already registered | Register each token only once |
| The module ID is outside `0xF0` ~ `0xFE` or already in use | Use a free ID |
| More than 4 registrations | Register up to 4 structs |
| `nominal_rate_hz` is 0 | Set it to 1 or more |
| Field names collide, or two fields point at the same bytes of the struct | Check the field names and members |
| A field name or unit is too long, or contains a non-ASCII character (Korean, etc.) | Shorten to a name of 15 and a unit of 7 ASCII characters or fewer |
| `scale` is 0 or not a number (infinity, NaN) | Use a non-zero finite value |
| The volume to send exceeds what USB can carry (counted together with structs registered earlier) | Lower the rate or shrink the struct |
| Together with structs registered earlier, the volume exceeds what can be kept when the PC program pauses briefly (see [`XM_Log_SetMinStallMs`](#xm_log_setminstallms)) | Lower the rate or shrink the struct |
| Called outside `Control_Setup()` (after `Control_Setup()` has finished, from another task, or from an interrupt) | Call it inside `Control_Setup()` |

- On `false` the registration state does not change, so you can fix it and call again inside `Control_Setup()`. Even on `false`, a token that is already registered can still be sent as before.
- There are 1 to 90 fields. With 0 fields, or more than 90, you get a compile error.
- A registered module ID belongs to `XM_LOG_SEND` only. Calling `XM_SendUsbDataWithId()` with the same ID returns `false`.
- Calling it after `Control_Setup()` has finished raises `seal_reject_count`; calling it from another task or an interrupt raises `owner_violation_count` ([`XM_LogStats_t`](#xm_logstats_t)).
- How large a struct and how high a rate still register has not yet been confirmed on the board. (not yet confirmed on the board) <!-- board-check -->

**Once the first registration succeeds**, the following things change (details in section 7 of [10. Struct Logging](../10-struct-logging.en.md)).

- Calls to `XM_USB_SetHostProfile()` are ignored.
- `XM_Log_SetMinStallMs()` returns `false`.
- `XM_SetUsbCustomMeta()` works only inside `Control_Setup()`. Calling it later returns `false`.
- The registered module ID belongs to that struct only.
- The send volume of `XM_SendUsbDataWithId()` and `XM_SendUsbDebugMessage()` becomes limited.
- The board sends the field names, units and types to the PC. They are delivered when a PC program opens the port, and sent again every few seconds after that. (design goal, not yet confirmed on the board) <!-- board-check -->

**⚠️ Call context**: Call it only inside `Control_Setup()`.

**Example**

```c
void Control_Setup(void)
{
    if (!XM_LOG_REGISTER(JointLog, 0xF0, 500)) {      // 500 Hz — every 2 ms
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);
    }
}
```

**See also**: [`XM_LOG_SEND`](#xm_log_send), [`XM_Log_SetMinStallMs`](#xm_log_setminstallms)

---

### `XM_LOG_SEND`

```c
XM_LOG_SEND(tok, ptr)   // → bool
```

Sends one registered struct. Call it inside `Control_Loop()`. The function it actually calls is [`XM_Log_SendRaw`](#xm_log_sendraw). The struct is copied inside the call, so a local variable is fine.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `tok` | token | The token from `XM_LOG_DECLARE` |
| `ptr` | struct pointer | A pointer to that struct — a pointer of another type is a compile error |

**Return value**: `bool` — `true`: sent (it means the struct was queued in the transmit buffer, and says nothing about whether the PC received it). `false`: one of the following.

- Not registered (including a token whose `XM_LOG_REGISTER` returned `false`)
- `ptr` is `NULL`
- Called earlier than the registered period
- The USB device is not ready
- The transmit buffer is full
- Called outside `Control_Setup()` and `Control_Loop()` (from another task or an interrupt)

You can call it every loop and data goes out only at the registered rate, so a `false` before the period is due is normal. Even if `false` was returned because the transmit buffer was full, the next send goes out after the registered period has passed (calling it again right away does not send). Calling it outside `Control_Setup()` and `Control_Loop()` raises `owner_violation_count` ([`XM_LogStats_t`](#xm_logstats_t)).

**⚠️ Call context**: Call it inside `Control_Loop()`. Do not call it from an interrupt or from a background task you created yourself (`XM_Task_*`).

**Example**

```c
void Control_Loop(void)
{
    JointLog_t s = { XM.status.h10.leftHipAngle, XM.status.h10.leftHipTorque };
    (void)XM_LOG_SEND(JointLog, &s);      // You can call it every loop — data goes out at 500 Hz only
}
```

**See also**: [`XM_LOG_REGISTER`](#xm_log_register), [`XM_Log_GetStats`](#xm_log_getstats)

---

### `XM_Log_SetMinStallMs`

```c
bool XM_Log_SetMinStallMs(uint16_t ms);
```

Sets the target time (ms) for which the board keeps data without dropping it when the PC program pauses briefly. The default is 50 ms. Registering a struct that sends too much to meet this goal returns `false`. 50 ms is an initial value that has not yet been measured on the board, so it is a goal, not a guarantee. (design goal) <!-- board-check -->

**Parameters**

| Name | Type | Description |
|------|------|------|
| `ms` | `uint16_t` | Target time (ms) |

**Return value**: `bool` — `true`: changed. `false`: called outside `Control_Setup()`, or a struct has already been registered successfully

**⚠️ Call context**: Call it inside `Control_Setup()`, before the first `XM_LOG_REGISTER`.

**Example**

```c
void Control_Setup(void)
{
    (void)XM_Log_SetMinStallMs(100);                  // The value is only an example — if the default 50 ms is enough, you do not have to call it
    (void)XM_LOG_REGISTER(JointLog, 0xF0, 500);
}
```

**See also**: [`XM_Log_GetMinStallMs`](#xm_log_getminstallms), [`XM_LOG_REGISTER`](#xm_log_register)

---

### `XM_Log_GetMinStallMs`

```c
uint16_t XM_Log_GetMinStallMs(void);
```

Returns the target time (ms) currently in use. For diagnostics.

**Parameters**: none

**Return value**: `uint16_t` — the target time (ms)

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context.

**See also**: [`XM_Log_SetMinStallMs`](#xm_log_setminstallms)

---

### `XM_Log_EnableLinkHealth`

```c
bool XM_Log_EnableLinkHealth(bool enable);
```

Sets whether link status information (number of failed sends, buffer usage, etc.) is sent every second. The default is not to send it. It is sent only when at least one struct is registered — if you register none, turning it on sends nothing. (design goal, not yet confirmed on the board) <!-- board-check -->

**Parameters**

| Name | Type | Description |
|------|------|------|
| `enable` | `bool` | `true`: send, `false`: do not send |

**Return value**: `bool` — `true`: set. `false`: called outside `Control_Setup()`

The `xm10` tool does not decode and show this value separately. To look at the logging counters on the board, use [`XM_Log_GetStats`](#xm_log_getstats).

**⚠️ Call context**: Call it inside `Control_Setup()`.

**Example**

```c
void Control_Setup(void)
{
    (void)XM_LOG_REGISTER(JointLog, 0xF0, 500);
    (void)XM_Log_EnableLinkHealth(true);              // Turn link status sending on
}
```

**See also**: [`XM_Log_GetStats`](#xm_log_getstats)

---

### `XM_Log_GetAdmittedWireBps`

```c
uint32_t XM_Log_GetAdmittedWireBps(void);
```

Returns the share of the USB send budget that is already committed (bytes/s). It is the sum of the shares of the registered structs and the share the board reserves in advance.

The reserved share covers unregistered data and debug text, sending of struct information and link status, and — with the default profile (`XM_USB_HOST_PHAI_STUDIO`) — Total Data. If you registered no struct, only the reserved share is returned. For diagnostics.

**Parameters**: none

**Return value**: `uint32_t` — the committed send share (bytes/s)

**⚠️ Call context**: Assume `Control_Setup()` / `Control_Loop()` context.

**See also**: [`XM_LOG_REGISTER`](#xm_log_register)

---

### `XM_Log_GetStats`

```c
void XM_Log_GetStats(XM_LogStats_t* out);
```

Copies the observation counters. Each field is read separately, so several fields may not be values from exactly the same instant. For what each field means, see [`XM_LogStats_t`](#xm_logstats_t).

**Parameters**

| Name | Type | Description |
|------|------|------|
| `out` | `XM_LogStats_t*` | Where to put the copy — if `NULL`, nothing happens |

**Return value**: none (`void`) — the result comes back through `out`.

**⚠️ Call context**: Call it from `Control_Loop()`.

**Example**

```c
static XM_LogStats_t s_stats;

void Control_Loop(void)
{
    XM_Log_GetStats(&s_stats);            // Add s_stats to the debugger's Live Expressions to watch it
}
```

**See also**: [`XM_LogStats_t`](#xm_logstats_t)

---

## Types / constants

### `XM_FieldType_e`

The field type. `XM_FIELD` and `XM_FIELD_ARRAY` decide it from the C type of the member (you do not write it yourself).

| Value | Name | Member C type | Size |
|----|------|------|------|
| 0 | `XM_FIELD_F32` | `float` | 4 B |
| 1 | `XM_FIELD_F64` | `double` | 8 B |
| 2 | `XM_FIELD_I8` | `int8_t` | 1 B |
| 3 | `XM_FIELD_U8` | `uint8_t` | 1 B |
| 4 | `XM_FIELD_I16` | `int16_t` | 2 B |
| 5 | `XM_FIELD_U16` | `uint16_t` | 2 B |
| 6 | `XM_FIELD_I32` | `int32_t` · `int` | 4 B |
| 7 | `XM_FIELD_U32` | `uint32_t` · `unsigned` | 4 B |
| 8 | `XM_FIELD_I64` | `int64_t` | 8 B |
| 9 | `XM_FIELD_U64` | `uint64_t` | 8 B |
| 10 | `XM_FIELD_BOOL` | `bool` | 1 B |

---

### `XM_FieldDesc_t`

One line of a field description. Build it with `XM_FIELD` or `XM_FIELD_ARRAY`. If you fill it in by hand, the compiler cannot check the type and position.

| Member | Type | Description |
|------|------|------|
| `name` | `const char*` | Field name (up to 15 letters, digits and symbols, ASCII only) |
| `unit` | `const char*` | Unit (up to 7 letters, digits and symbols, ASCII only; `""` if none) |
| `type` | `uint8_t` | Element type — an `XM_FieldType_e` value. It is 1 byte regardless of compiler options |
| `offset` | `uint16_t` | Position inside the struct (bytes) |
| `array_len` | `uint8_t` | Number of elements (scalar = 1) |
| `scale` | `float` | Physical value = stored value × `scale` |

---

### `XM_LogStats_t`

The logging observation counters. You receive them with [`XM_Log_GetStats()`](#xm_log_getstats). All are totals since boot, and a field with no special note is counted even if you registered no struct.

| Member | Type | Description |
|------|------|------|
| `rate_gate_suppressed_calls` | `uint32_t` | Number of times `XM_LOG_SEND` was called more often than the registered period and nothing was sent (only occurs when a struct is registered) |
| `owner_violation_count` | `uint32_t` | Number of times `XM_LOG_REGISTER`, `XM_LOG_SEND` or a setting function of this header was called outside `Control_Setup()` and `Control_Loop()` (from another task or an interrupt) (once a struct is registered, `XM_SetUsbCustomMeta()` is counted too) |
| `profile_change_blocked_count` | `uint32_t` | Number of times `XM_USB_SetHostProfile()` was called after registration and ignored |
| `admission_reject_count` | `uint32_t` | Number of times `XM_SendUsbDataWithId()`, `XM_SendUsbDebugMessage()` and the like were rejected by the send-volume limit (including something too large to send at once) (counted only when a struct is registered. A call rejected because an argument exceeded the function's maximum length is not counted) |
| `reserved_id_reject_count` | `uint32_t` | Number of times a send was rejected for using a module ID the system uses or the module ID of a registered struct, plus the number of times a system ID passed to `XM_SetUsbStreamModuleId()` was ignored (the ID of a registered struct is counted every time you send with that ID, not when you set it) |
| `seal_reject_count` | `uint32_t` | Number of times `XM_LOG_REGISTER` was called after `Control_Setup()` had finished (once a struct is registered, `XM_SetUsbCustomMeta()` is counted too) |
| `admission_attempt_calls` | `uint32_t` | Number of times the sends above were checked against the send-volume limit (allowed + rejected, counted only when a struct is registered) |
| `x20_same_tick_suppressed_calls` | `uint32_t` | Number of times the control loop ran again within the same 1 ms and the Total Data send was skipped (counted only when a struct is registered) |

- It is normal for `rate_gate_suppressed_calls` to keep growing when you call `XM_LOG_SEND` every loop.
- Sending Total Data at most once per 1 ms and skipping the rest has not yet been confirmed on the board. (not yet confirmed on the board) <!-- board-check -->

---

### Limit constants

| Name | Value | Meaning |
|------|----|----|
| `XM_LOG_MAX_STRUCT_SIZE` | `1008U` | Upper limit of the size of a struct you can register (bytes) |
| `XM_LOG_MAX_FIELDS` | `90U` | Upper limit of the number of fields you can list for one struct |
| `XM_LOG_MAX_ARRAY_LEN` | `255U` | Upper limit of the number of elements of one array field |
| `XM_LOG_MAX_TOKEN_LEN` | `11U` | Upper limit of the token length (characters) — the token becomes the struct name |
| `XM_LOG_SLOT_NONE` | `0xFFU` | The slot value of a token that is not registered |

---

### `XM_TYPE_TAG`

```c
XM_TYPE_TAG(x)
```

The field type of a member expression. `XM_FIELD` and `XM_FIELD_ARRAY` use it internally, so you rarely need it yourself. The supported types are `float`, `double`, `bool`, `int`, `unsigned` and `int8_t` ~ `uint64_t`. An enumeration member is accepted as the integer type of the same size. Any other type (`char`, pointer, struct, array, etc.) is a compile error.

---

## Support header — `xm_api_logging_raw.h` (do not call directly)

> ⚠️ **Not a supported interface.** It holds the prototypes of the functions that the `XM_LOG_REGISTER` and `XM_LOG_SEND` macros call. A `.c` file that uses the macros must include this header to compile. The functions below skip type and size checks, so the compiler cannot stop you from sending a different struct. Do not call them directly; always use the `XM_LOG_*` macros.

### `XM_Log_RegisterSchemaRaw`

```c
bool XM_Log_RegisterSchemaRaw(uint8_t* slot_io, uint8_t module_id, const char* struct_name, uint16_t struct_size,
                              uint16_t nominal_rate_hz, const XM_FieldDesc_t* fields, uint8_t field_count);
```

Called by `XM_LOG_REGISTER`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `slot_io` | `uint8_t*` | The token's slot variable — it registers only when it is `XM_LOG_SLOT_NONE`, and writes the slot number on success |
| `module_id` | `uint8_t` | Module ID (`0xF0` ~ `0xFE`) |
| `struct_name` | `const char*` | Struct name (token) |
| `struct_size` | `uint16_t` | Struct size (bytes) |
| `nominal_rate_hz` | `uint16_t` | Send rate (Hz) |
| `fields` | `const XM_FieldDesc_t*` | Field description array |
| `field_count` | `uint8_t` | Number of fields |

**Return value**: `bool` — `true`: registered. `false`: the slot was already registered or registration failed — `*slot_io` does not change.

---

### `XM_Log_SendRaw`

```c
bool XM_Log_SendRaw(uint8_t slot, const void* data, uint16_t size);
```

Called by `XM_LOG_SEND`.

**Parameters**

| Name | Type | Description |
|------|------|------|
| `slot` | `uint8_t` | The registered slot number |
| `data` | `const void*` | The struct to send |
| `size` | `uint16_t` | Struct size (bytes) |

**Return value**: `bool` — `true`: sent. `false`: the slot is not registered, `size` differs from the struct size of that slot, `data` is `NULL`, called earlier than the registered period, the USB device is not ready, the transmit buffer is full, or called outside `Control_Setup()` and `Control_Loop()` (from another task or an interrupt). Even if `false` was returned because the transmit buffer was full, the next send goes out after the registered period has passed.

---

## Internal-only (do not use)

The macros whose names end in `_` (`XM_LOG_REQUIRE_`, `XM_FIELD_TYPE_NONE_`, `XM_TYPE_TAG_`, `XM_LOG_MEMBER_`, `XM_LOG_ARRLEN_`, `XM_LOG_IS_ARRAY_`, `XM_LOG_FIELD_COUNT_`) are helper definitions that other macros use. Do not use them in your code.

---

## Rev1.1 / Rev2.0 Difference Summary

| Item | Rev1.1 | Rev2.0 |
|------|--------|--------|
| Struct logging (`xm_api_logging.h` · `xm_api_logging_raw.h`) | ❌ Absent | 🟢 Exclusive |
| Existing way (`XM_SetUsbCustomMeta` + `XM_SendUsbDataWithId`) | ✅ | ✅ |

---

## Related Documents

- [10. Struct Logging (concept)](../10-struct-logging.en.md) — operating principle, limits, common mistakes
- [`xm_api_usb.h` — USB-CDC Real-Time Streaming](xm-api-usb.en.md) — the existing USB-CDC functions such as `XM_SendUsbDataWithId` and `XM_SetUsbCustomMeta`
- [Get Data on PC — the `xm10` tool](../../getting-started/04-pc-data-tool.en.md)
