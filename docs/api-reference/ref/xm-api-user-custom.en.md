# `xm_api_user_custom.h` — 28-byte user slot API inside the Total Data Packet

> **Target header**: `XM_FW/XM_API/xm_api_user_custom.h` (Rev1.1 and Rev2.0 headers are identical — no functional difference, only the top-of-file `@date` comment differs by one day)
> 📚 **Related concept docs**: [05. USB Serial Communication](../05-usb-connectivity.en.md) (distinguishes Total Data Packet 0x20 from the User Custom Channel 0xF0~0xFE), [02. KIT H10 Control + Data](../02-h10-control-n-data.en.md) (the `Control_Setup`/`Control_Loop` IPO cycle)
> 🧪 **Related examples**: No example currently calls this API directly (it is a new API in v2.3.0 — a full-SDK search turned up zero usages). Read [Ex.09 CDC Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) first, which covers the same Total Data Packet (0x20) so you can see which stream this slot rides on.

---

## When to use this

XM10 automatically sends a 365-byte **Total Data Packet** (Module ID `0x20`) over USB at 1 kHz, once per `Control_Loop()` iteration. The last 28 bytes of this packet are the `User_Custom` region — slots deliberately left open for whatever values you want to carry, such as a filtered EMG signal or an FSM state. Once you fill these 28 bytes with the functions in this header, they are automatically included in the PhAI Studio live graph and the OPFS (browser-local file system) recording — no extra transmission code required.

Don't confuse this with the similarly-named User Custom **Channel** (`0xF0~0xFE`, `XM_SetUsbCustomMeta`/`XM_SendUsbDataWithId`) described in [doc 05](../05-usb-connectivity.en.md) — that is a different mechanism. That channel lets you freely grow the packet size and channel count; this API is a much lighter-weight way to fill in a **fixed 28 bytes** inside the `0x20` packet that is already always being sent. If 4 float slots + 4 int16 slots + 16 flag bits + 2 uint8 slots are enough for you, use this API; if you need larger or variable-sized data, use the custom channel from doc 05 instead.

### Basic usage flow

```c
void Control_Setup(void) {
    /* (optional) register PhAI Studio channel labels — default labels (f0, i16_0, ...) are used if omitted */
    XM_SetUsbCustomMeta(0xE0,
        "[{\"slot\":\"f0\",\"name\":\"EMG_envelope\",\"unit\":\"uV\"},"
         " {\"slot\":\"i16_0\",\"name\":\"PF3_raw\",\"unit\":\"LSB\"}]");
}

void Control_Loop(void) {
    XM_UserCustom_SetFloat(0, envelope_uV);   // user_f[0]
    XM_UserCustom_SetI16  (0, pf3_raw);       // user_i16[0]
    XM_UserCustom_SetFlag (0, is_fsm_active); // bit0 of user_flags
}
```

`XM_SetUsbCustomMeta` belongs to `xm_api_usb.h`, not this header — see [doc 05](../05-usb-connectivity.en.md) for its full signature.

---

## Function list

| Function | One-line description |
|----------|----------------------|
| [`XM_UserCustom_SetFloat`](#xm_usercustom_setfloat) | Writes a value into one of the 4 float slots |
| [`XM_UserCustom_SetI16`](#xm_usercustom_seti16) | Writes a value into one of the 4 int16 slots |
| [`XM_UserCustom_SetU8`](#xm_usercustom_setu8) | Writes a value into one of the 2 uint8 slots |
| [`XM_UserCustom_SetFlags`](#xm_usercustom_setflags) | Overwrites the entire 16-bit flags word at once |
| [`XM_UserCustom_SetFlag`](#xm_usercustom_setflag) | Sets/clears a single bit in the flags word |
| [`XM_UserCustom_Reset`](#xm_usercustom_reset) | Zeroes out the entire User_Custom block |
| [`XM_UserCustom_GetBlock`](#xm_usercustom_getblock) | Reads the current User_Custom block as one snapshot |

There are no `[Internal]`-marked functions — all 7 functions are public API. That said, `XM_UserCustom_GetBlock` is called automatically by the System Layer on every tick, so ordinary user code will rarely need to call it directly (see details below).

---

## Function details

### `XM_UserCustom_SetFloat`

```c
void XM_UserCustom_SetFloat(uint8_t idx, float value);
```

Writes a float value into one of the `user_f[]` slots. Use this for processed/filtered signals such as an EMG envelope.

| Name | Type | Description |
|------|------|--------------|
| `idx` | `uint8_t` | Slot index (0 ~ `XM_USER_CUSTOM_FLOAT_COUNT`-1, i.e. 0~3). Out-of-range indices are silently ignored |
| `value` | `float` | Value to write |

**Return value**: none (`void`)

⚠️ **Call context**: The header comment states the setter is non-blocking and callable from places other than `Control_Loop()`. However, since there is no explicit statement guaranteeing ISR-safety, calling it from the `Control_Setup()`/`Control_Loop()` context is recommended.

```c
void Control_Loop(void) {
    float envelope_uV = ComputeEmgEnvelope();
    XM_UserCustom_SetFloat(0, envelope_uV);  // writes to user_f[0]
}
```

**See also**: [`XM_UserCustom_GetBlock`](#xm_usercustom_getblock) to read it back · [slot layout table](#xm_usercustomblock_t)

---

### `XM_UserCustom_SetI16`

```c
void XM_UserCustom_SetI16(uint8_t idx, int16_t value);
```

Writes an int16 value into one of the `user_i16[]` slots. Use this for values where an integer is sufficient, such as a raw ADC reading or a counter.

| Name | Type | Description |
|------|------|--------------|
| `idx` | `uint8_t` | Slot index (0 ~ `XM_USER_CUSTOM_I16_COUNT`-1, i.e. 0~3). Out-of-range indices are silently ignored |
| `value` | `int16_t` | Value to write |

**Return value**: none (`void`)

⚠️ **Call context**: Same as `XM_UserCustom_SetFloat` — non-blocking, `Control_Setup()`/`Control_Loop()` context.

```c
XM_UserCustom_SetI16(0, (int16_t)pf3_raw_adc);  // writes to user_i16[0]
```

**See also**: [`XM_UserCustom_SetU8`](#xm_usercustom_setu8) (consider the uint8 slot if the range is small enough)

---

### `XM_UserCustom_SetU8`

```c
void XM_UserCustom_SetU8(uint8_t idx, uint8_t value);
```

Writes a uint8 value into one of the `user_u8[]` slots. Well-suited to small enum values such as an FSM state.

| Name | Type | Description |
|------|------|--------------|
| `idx` | `uint8_t` | Slot index (0 ~ `XM_USER_CUSTOM_U8_COUNT`-1, i.e. 0~1). Out-of-range indices are silently ignored |
| `value` | `uint8_t` | Value to write |

**Return value**: none (`void`)

⚠️ **Call context**: Same as `XM_UserCustom_SetFloat` — non-blocking, `Control_Setup()`/`Control_Loop()` context.

```c
XM_UserCustom_SetU8(0, (uint8_t)fsm_state);  // writes to user_u8[0]
```

**See also**: [`XM_UserCustom_SetFlag`](#xm_usercustom_setflag) (a flag bit is cheaper if you only need a single bool)

---

### `XM_UserCustom_SetFlags`

```c
void XM_UserCustom_SetFlags(uint16_t flags);
```

Overwrites the entire `user_flags` 16-bit word at once. More efficient than calling `SetFlag` repeatedly when changing several bits at the same time.

| Name | Type | Description |
|------|------|--------------|
| `flags` | `uint16_t` | New 16-bit word to write (overwrites all existing bits) |

**Return value**: none (`void`)

⚠️ **Call context**: Same as `XM_UserCustom_SetFloat` — non-blocking, `Control_Setup()`/`Control_Loop()` context.

```c
XM_UserCustom_SetFlags(0x0005);  // bit0 and bit2 = 1, all other bits = 0
```

**See also**: [`XM_UserCustom_SetFlag`](#xm_usercustom_setflag) (when changing only a single bit)

---

### `XM_UserCustom_SetFlag`

```c
void XM_UserCustom_SetFlag(uint8_t bit, bool value);
```

Sets or clears a single bit of `user_flags`. All other bits are left untouched.

| Name | Type | Description |
|------|------|--------------|
| `bit` | `uint8_t` | Bit index (0 ~ `XM_USER_CUSTOM_FLAG_COUNT`-1, i.e. 0~15). Out-of-range indices are silently ignored |
| `value` | `bool` | `true` sets the bit to 1, `false` clears it to 0 |

**Return value**: none (`void`)

⚠️ **Call context**: Same as `XM_UserCustom_SetFloat` — non-blocking, `Control_Setup()`/`Control_Loop()` context.

```c
XM_UserCustom_SetFlag(3, is_fsm_active);  // sets/clears bit3 only, other bits unchanged
```

**See also**: [`XM_UserCustom_SetFlags`](#xm_usercustom_setflags) (when changing several bits at once)

---

### `XM_UserCustom_Reset`

```c
void XM_UserCustom_Reset(void);
```

Zeroes out the entire User_Custom block (4 floats, 4 int16s, the flags word, and 2 uint8s).

No parameters.

**Return value**: none (`void`)

⚠️ **Call context**: The system calls this automatically once at boot, so ordinary user code normally does not need to call it. Call it, from the `Control_Setup()`/`Control_Loop()` context, only when you explicitly want to reset the values.

```c
XM_UserCustom_Reset();  // resets all slots to 0 (not normally needed)
```

**See also**: [Types/macros](#xm_usercustomblock_t) (the full slot layout)

---

### `XM_UserCustom_GetBlock`

```c
void XM_UserCustom_GetBlock(XM_UserCustomBlock_t* out);
```

Copies the current User_Custom block (28 bytes) into the buffer pointed to by `out`, in one call.

| Name | Type | Description |
|------|------|--------------|
| `out` | `XM_UserCustomBlock_t*` | Output buffer to receive the 28-byte result. **Must not be NULL** |

**Return value**: none (`void`) — the result is returned through the `out` pointer.

⚠️ **Call context**: This is the function the System Layer (`XM_TotalData_Snapshot`) calls automatically on every tick to populate the Total Data Packet; ordinary user code will rarely need to call it directly. Call it, from the `Control_Setup()`/`Control_Loop()` context, only if you want to inspect the current values yourself.
The header only states that `out` must not be NULL — it does not document whether an internal guard exists if NULL is passed anyway. Always pass a valid buffer.

```c
XM_UserCustomBlock_t snapshot;
XM_UserCustom_GetBlock(&snapshot);
XM_SendUsbDebugMessage("f0=%.2f, flags=0x%04X\r\n", snapshot.f[0], snapshot.flags);
```

**See also**: [`XM_UserCustomBlock_t`](#xm_usercustomblock_t) (field layout)

---

## Types / macros

### `XM_UserCustomBlock_t`

A struct that mirrors the 28-byte `User_Custom` region at the end of the Total Data Packet (365 B, Module ID `0x20`). This is the output type of `XM_UserCustom_GetBlock()`.

```c
typedef struct {
    float    f[XM_USER_CUSTOM_FLOAT_COUNT];   /**< 16B */
    int16_t  i16[XM_USER_CUSTOM_I16_COUNT];   /**<  8B */
    uint16_t flags;                           /**<  2B */
    uint8_t  u8[XM_USER_CUSTOM_U8_COUNT];     /**<  2B */
} XM_UserCustomBlock_t;
```

| Field | Type | Size | Total Data Packet offset | Corresponding function |
|-------|------|------|---------------------------|--------------------------|
| `f[4]` | `float` | 16B | 337 ~ 352 | `XM_UserCustom_SetFloat` |
| `i16[4]` | `int16_t` | 8B | 353 ~ 360 | `XM_UserCustom_SetI16` |
| `flags` | `uint16_t` | 2B | 361 ~ 362 | `XM_UserCustom_SetFlags` / `XM_UserCustom_SetFlag` |
| `u8[2]` | `uint8_t` | 2B | 363 ~ 364 | `XM_UserCustom_SetU8` |

> The offsets come from the field comments in `XM_FW/System/Comm/USB/xm_total_data_packet.h` (an auto-generated file), specifically the `user_f`/`user_i16`/`user_flags`/`user_u8` fields. Together they total 28 bytes, corresponding to the last segment (offset 337~364) of the 365-byte Total Data Packet.
> `XM_UserCustomBlock_t` itself carries no `packed` attribute, but given the field order (float → int16 → uint16 → uint8), the compiler's natural alignment already produces exactly 28 bytes with no padding.

### Macros

| Macro | Value | Meaning |
|-------|-------|---------|
| `XM_USER_CUSTOM_FLOAT_COUNT` | `4U` | Number of `user_f[]` slots |
| `XM_USER_CUSTOM_I16_COUNT` | `4U` | Number of `user_i16[]` slots |
| `XM_USER_CUSTOM_U8_COUNT` | `2U` | Number of `user_u8[]` slots |
| `XM_USER_CUSTOM_FLAG_COUNT` | `16U` | Number of `user_flags` bits |

Use these macros as loop bounds instead of hardcoding indices/bit counts, so your code keeps working unchanged if a future SDK version changes the slot counts.
