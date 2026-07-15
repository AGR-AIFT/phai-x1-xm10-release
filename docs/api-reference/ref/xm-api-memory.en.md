# `xm_api_memory.h` — XM10 Memory Access API

> 🟢 **Rev 2.0 only** — This entire header ships only in the XM10 Rev 2.0 SDK. The Rev 1.1 SDK (`XM10_SDK/Rev1.1/`) does not contain `xm_api_memory.h` at all, so calling any of the functions below from a Rev 1.1 project will **fail at the link stage**. To check your board revision, see [Board Revision Comparison](../../hardware/README.md#보드-리비전-비교).
>
> - **Header**: `XM_FW/XM_API/xm_api_memory.h`
> - **Related concept doc**: [07. Memory Management](../07-memory-management.en.md)
> - **Related example**: [Ex.36 OnDevice Kinesthetic Learning](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) — saves/restores trained neural-network weights to Flash with `XM_UserNV_Read/Write/Erase`

---

## When to use this

Reach for this API when plain global variables (`.bss`/`.data`) aren't enough — you need a large buffer, deterministic cache-free access inside a control loop, or values that must survive a power cycle. [07. Memory Management](../07-memory-management.en.md) covers the characteristics of each region (speed / volatility / DMA capability) and how to choose between them; this page only covers the signature and parameters of each function.

---

## Function List

| Function | One-line description |
|----------|----------------------|
| [`XM_GetUserWorkspace()`](#xm_getuserworkspace) | Returns the base address of the RAM_D1 user workspace |
| [`XM_GetUserWorkspaceSize()`](#xm_getuserworkspacesize) | Returns the size (bytes) of the RAM_D1 workspace |
| [`XM_GetUserPSRAM()`](#xm_getuserpsram) 🟢 | Returns the base address of the PSRAM user area |
| [`XM_GetUserPSRAMSize()`](#xm_getuserpsramsize) 🟢 | Returns the size (bytes) of the PSRAM user area |
| [`XM_GetUserDTCM()`](#xm_getuserdtcm) | Returns the base address of the DTCM user area |
| [`XM_GetUserDTCMSize()`](#xm_getuserdtcmsize) | Returns the size (bytes) of the DTCM user area |
| [`XM_UserNV_GetSize()`](#xm_usernv_getsize) | Returns the size (bytes) of the Flash User NV area |
| [`XM_UserNV_Read()`](#xm_usernv_read) | Reads data from Flash User NV |
| [`XM_UserNV_Write()`](#xm_usernv_write) | Writes data to Flash User NV |
| [`XM_UserNV_Erase()`](#xm_usernv_erase) | Erases the entire Flash User NV area |
| [`XM_UserNV_IsErased()`](#xm_usernv_iserased) | Checks whether Flash User NV is empty (all `0xFF`) |

The two macros (`XM_RAMFUNC`, `XM_DTCM_VAR`) are covered in [Types/Macros](#typesmacros).

---

## Function Details

### RAM_D1 User Workspace

#### `XM_GetUserWorkspace()`

```c
void* XM_GetUserWorkspace(void);
```

Returns the base address of the user workspace reserved in RAM_D1. This is a cacheable region (goes through D-Cache), so access is fast, but the contents are lost on power-down. It's the first region to reach for general-purpose data: algorithm variables, sensor data arrays, computation buffers.

**Parameters**: none

**Return value**

| Return | Meaning |
|--------|---------|
| `void*` | Base address of the contiguous memory block |

**⚠️ Calling context**: The header does not state any specific restriction. This page is written conservatively against a `Control_Setup()`/`Control_Loop()` task context. The usual pattern is to fetch the pointer once in `Control_Setup()`, store it in a static/global pointer, and reuse it in `Control_Loop()`.

```c
static float* s_work_buf;

void Control_Setup(void) {
    s_work_buf = (float*)XM_GetUserWorkspace();
}
```

**See also**: [07. Memory Management — RAM_D1 Workspace API](../07-memory-management.en.md#ram_d1-workspace-api)

#### `XM_GetUserWorkspaceSize()`

```c
uint32_t XM_GetUserWorkspaceSize(void);
```

Returns the total size, in bytes, of the block returned by `XM_GetUserWorkspace()`. Use it to check bounds when you manually partition the workspace by offset.

**Parameters**: none · **Return value**: workspace size (bytes)

---

### PSRAM User Area 🟢 Rev 2.0 only

#### `XM_GetUserPSRAM()`

```c
void* XM_GetUserPSRAM(void);
```

Returns the base address (`0x90700000`) of the user area inside the external PSRAM (APS6404L, QSPI memory-mapped). It is write-through cacheable, has far more capacity than RAM_D1, but only medium-speed access (~30MB/s). Suited for data that is "large but doesn't need to be that fast" — AI/ML model weights, large lookup tables.

**Parameters**: none

**Return value**

| Return | Meaning |
|--------|---------|
| `void*` | Base address of the PSRAM contiguous memory block (`0x90700000`) |

**⚠️ Calling context**: Safe from `Control_Setup()` onward. QSPI memory-mapped initialization completes automatically during system startup, so dereferencing this pointer before that (e.g. in global-variable initializers) can cause a **HardFault**.

```c
static float* s_nn_weights;

void Control_Setup(void) {
    s_nn_weights = (float*)XM_GetUserPSRAM();
    // safe to read/write from this point on
}
```

**See also**: PSRAM is Rev 2.0-only hardware — not populated on Rev 1.1 boards. [07. Memory Management — PSRAM API](../07-memory-management.en.md#psram-api)

#### `XM_GetUserPSRAMSize()`

```c
uint32_t XM_GetUserPSRAMSize(void);
```

Returns the total size, in bytes, of the block returned by `XM_GetUserPSRAM()`.

**Parameters**: none · **Return value**: PSRAM user area size (bytes)

---

### DTCM User Area

#### `XM_GetUserDTCM()`

```c
void* XM_GetUserDTCM(void);
```

Returns the base address of the user variable area inside DTCMRAM. It is accessed with zero wait states at 480MHz and bypasses D-Cache, so access latency is deterministic — but **it cannot be reached by DMA.** If you only need a handful of individual variables, the `XM_DTCM_VAR` macro is more convenient; use this function when you need a single large contiguous block, such as a LUT.

**Parameters**: none

**Return value**

| Return | Meaning |
|--------|---------|
| `void*` | Base address of the DTCM contiguous memory block |

**⚠️ Calling context**: The header does not state any specific restriction. This page is written conservatively against a `Control_Setup()`/`Control_Loop()` task context. Do not place DMA buffers in this block — the DMA controller cannot access the DTCM address space.

**See also**: [07. Memory Management — DTCM API](../07-memory-management.en.md#dtcm-api)

#### `XM_GetUserDTCMSize()`

```c
uint32_t XM_GetUserDTCMSize(void);
```

Returns the total size, in bytes, of the block returned by `XM_GetUserDTCM()`.

**Parameters**: none · **Return value**: DTCM user area size (bytes)

---

### Flash User NV (Non-Volatile Storage)

A non-volatile storage area using Flash Bank 2, Sector 7 (128KB). Values survive a power cycle, but the usual Flash constraints still apply (write only possible after erase, finite erase-cycle count).

#### `XM_UserNV_GetSize()`

```c
uint32_t XM_UserNV_GetSize(void);
```

Returns the total size (bytes) of the User NV area.

**Parameters**: none · **Return value**: User NV area size (bytes, fixed at 128KB)

#### `XM_UserNV_Read()`

```c
int32_t XM_UserNV_Read(uint32_t offset, void *data, uint32_t size);
```

Reads data from the Flash User NV area into the `data` buffer.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `offset` | `uint32_t` | Byte offset from the start of User NV (0 ~ `Size`-1) |
| `data` | `void*` | Buffer to receive the data read |
| `size` | `uint32_t` | Number of bytes to read |

**Return value**

| Return | Meaning |
|--------|---------|
| `0` | Success |
| `-1` | Parameter error (`offset + size` > NV size) |

**⚠️ Calling context**: The header does not state any specific restriction. This page is written conservatively against a `Control_Setup()`/`Control_Loop()` task context; the usual pattern is to call this once at boot, from `Control_Setup()`.

**See also**: It's safer to check [`XM_UserNV_IsErased()`](#xm_usernv_iserased) first to see whether valid data exists — right after an erase, the whole area is `0xFF`.

#### `XM_UserNV_Write()`

```c
int32_t XM_UserNV_Write(uint32_t offset, const void *data, uint32_t size);
```

Writes data to the Flash User NV area.

**Parameters**

| Name | Type | Description |
|------|------|--------------|
| `offset` | `uint32_t` | Byte offset from the start of User NV (32-byte alignment recommended) |
| `data` | `const void*` | Pointer to the data to write |
| `size` | `uint32_t` | Number of bytes to write |

**Return value**

| Return | Meaning |
|--------|---------|
| `0` | Success |
| `-1` | Parameter error |
| `-2` | Flash programming failed |

**⚠️ Calling context**: The header does not state any specific restriction. This page is written conservatively against a `Control_Setup()`/`Control_Loop()` task context. However, the following two constraints are explicitly stated in the header and must be respected:

- Flash can only be **written after an erase** (1→0 only, never 0→1). Call [`XM_UserNV_Erase()`](#xm_usernv_erase) before writing new data.
- STM32H7 Flash is programmed in 32-byte (256-bit) units; if `offset` isn't aligned, it is automatically aligned before writing.
- Frequent writes affect Flash lifetime (minimum 10,000 erase cycles guaranteed). A "read once at boot, write once on setting-change/shutdown" pattern is recommended.

```c
// Ex.36 pattern — define offsets as constants and write per struct field
#define NV_MAGIC          0xCA5E0A01
#define NV_OFFSET_MAGIC   0
#define NV_OFFSET_WEIGHTS 4

XM_UserNV_Erase();
XM_UserNV_Write(NV_OFFSET_MAGIC, &magic, sizeof(magic));
XM_UserNV_Write(NV_OFFSET_WEIGHTS, &s_nn, sizeof(s_nn));
```

**See also**: [Ex.36 OnDevice Kinesthetic Learning](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) saves trained neural-network weights this way.

#### `XM_UserNV_Erase()`

```c
int32_t XM_UserNV_Erase(void);
```

Erases the entire Flash User NV area (128KB), resetting it to all `0xFF`.

**Parameters**: none

**Return value**

| Return | Meaning |
|--------|---------|
| `0` | Success |
| `-2` | Flash erase failed |

**⚠️ Calling context**: Erasing the full 128KB sector takes roughly 1–2 seconds — the header explicitly warns **"do not call from a real-time control loop."** Never call this from inside a 1kHz `Control_Loop()`; only use it from a one-shot path such as button-event handling or boot-time initialization.

#### `XM_UserNV_IsErased()`

```c
bool XM_UserNV_IsErased(void);
```

Checks whether the entire Flash User NV area is empty (all `0xFF`, i.e. right after an erase).

**Parameters**: none

**Return value**

| Return | Meaning |
|--------|---------|
| `true` | The entire area is `0xFF` (state right after erase) |
| `false` | Data exists, or the area was partially written |

**⚠️ Calling context**: The header does not state any specific restriction. This page is written conservatively against a `Control_Setup()`/`Control_Loop()` task context. It's typically used as a guard before `XM_UserNV_Read()`, to check whether valid data has been stored (a magic-number field checked after reading is another valid approach).

---

## Types/Macros

### `XM_RAMFUNC`

```c
#define XM_RAMFUNC  __attribute__((section(".itcm_text")))
```

A function attribute macro that places a function in ITCMRAM so it executes with zero wait states. Because it avoids the branch-prediction-miss latency of executing directly from Flash, it's suited to functions whose execution time must be deterministic, such as ISRs and control loops. Startup code automatically handles the Flash → ITCM copy, so no manual copying is required.

| Field | Description |
|-------|--------------|
| Applies to | An attribute placed before a function declaration |
| Placement | ITCMRAM (`.itcm_text` section) |
| Constraint | ITCM capacity is limited (64KB) — place only function code here; keep large arrays/buffers in RAM_D1 or another region |

```c
XM_RAMFUNC void MyControlLoop(void) {
    // This function runs from ITCMRAM (1-cycle fetch @ 480MHz)
}
```

### `XM_DTCM_VAR`

```c
#define XM_DTCM_VAR  __attribute__((section(".dtcm_data")))
```

A variable attribute macro that places a variable in DTCMRAM for zero-wait-state access. Because it bypasses D-Cache, access latency is deterministic, making it suitable for individual variables that need fast, per-variable access — control-algorithm state variables, lookup tables.

| Field | Description |
|-------|--------------|
| Applies to | An attribute placed before a variable declaration |
| Placement | DTCMRAM (`.dtcm_data` section) |
| Constraint | Not reachable by DMA — do not use for buffers that need to be filled by DMA |

```c
XM_DTCM_VAR static float s_pid_state[6];
XM_DTCM_VAR static float s_lookup_table[256];
```

**See also**: If you need one large contiguous block rather than a few variables, use [`XM_GetUserDTCM()`](#xm_getuserdtcm).

---

## Internal Only (Do Not Call)

`xm_api_memory.h` has no functions marked [Internal]/system-only — all 11 functions and 2 macros listed above are public API intended to be called from user code (`XM_Apps`).

---

## Related Docs

- [07. Memory Management](../07-memory-management.en.md) — comparison of the four memory regions, common mistakes, related examples
- [Ex.36 OnDevice Kinesthetic Learning](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) — a real-world use of the Flash User NV API
