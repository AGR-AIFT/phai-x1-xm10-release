# API 07: Memory Management

> 📌 **After reading this page**: You will know how to select the right memory region on XM10 — RAM_D1 / DTCM / Flash NV — based on your use case.
> ⏱️ Estimated reading time: 15 minutes
> 🧰 Prerequisites: Static data-structure patterns from [Ex.19 Memory Aware Design](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/19_Memory_Aware_Design/)
> 🎯 Key APIs: `XM_RAMFUNC` / `XM_DTCM_VAR` macros + `XM_UserNV_Read/Write/Erase` Flash API

> **Header file**: `xm_api_memory.h`

This API provides access to the various memory regions available on XM10.

---

## Memory Region Overview

| Region | Characteristics | Speed | Typical Use |
|--------|-----------------|-------|-------------|
| **RAM_D1 Workspace** | Cacheable, volatile | Fast | Algorithm variables, sensor buffers |
| **DTCM** | Zero-Wait-State, no DMA | Fastest | Real-time control variables |
| **Flash NV** | Non-volatile, slow write | Slow | Configuration, calibration data |

---

## Section Placement Macros

### `XM_RAMFUNC`

Places a function in ITCMRAM for Zero-Wait-State execution.

```c
XM_RAMFUNC void MyControlLoop(void) {
    // This function runs from ITCMRAM (1-cycle fetch @ 480MHz)
}
```

> Ideal for ISRs, control loops, and any function that must execute without branch-prediction penalties.

### `XM_DTCM_VAR`

Places a variable in DTCMRAM for Zero-Wait-State access.

```c
XM_DTCM_VAR static float s_pid_state[6];
XM_DTCM_VAR static float s_lookup_table[256];
```

> Bypasses the D-Cache, guaranteeing deterministic access latency. DMA cannot access DTCM.

---

## RAM_D1 Workspace API

### `XM_GetUserWorkspace()`

```c
void* XM_GetUserWorkspace(void);
```

Returns the base address of the RAM_D1 user workspace.

- Cacheable (goes through the D-Cache)
- Lost on power-off
- General-purpose: algorithm variables, sensor data arrays, computation buffers

### `XM_GetUserWorkspaceSize()`

```c
uint32_t XM_GetUserWorkspaceSize(void);
```

Returns the size of the user workspace in bytes.

---

## DTCM API

### `XM_GetUserDTCM()`

```c
void* XM_GetUserDTCM(void);
```

Returns the base address of the DTCM user variable region.

- Zero-Wait-State @ 480 MHz, bypasses the D-Cache
- DMA cannot access this region
- Use when you need a large contiguous block (for individual variables, the `XM_DTCM_VAR` macro is more convenient)

### `XM_GetUserDTCMSize()`

```c
uint32_t XM_GetUserDTCMSize(void);
```

Returns the size of the DTCM user region in bytes.

---

## Flash User NV (Non-Volatile Storage) API

Non-volatile storage that retains data across power cycles.

> Uses **Flash Bank 2, Sector 7 (128 KB)**. Minimum guaranteed endurance: 10,000 erase cycles.

### `XM_UserNV_GetSize()`

```c
uint32_t XM_UserNV_GetSize(void);
```

Returns the size of the User NV region in bytes.

### `XM_UserNV_Read()`

```c
int32_t XM_UserNV_Read(uint32_t offset, void *data, uint32_t size);
```

Reads data from Flash User NV.

| Parameter | Description |
|-----------|-------------|
| `offset` | Byte offset from the start of User NV (0 ~ Size-1) |
| `data` | Buffer to store the read data |
| `size` | Number of bytes to read |

| Return value | Meaning |
|--------------|---------|
| `0` | Success |
| `-1` | Parameter error (offset + size exceeds NV size) |

### `XM_UserNV_Write()`

```c
int32_t XM_UserNV_Write(uint32_t offset, const void *data, uint32_t size);
```

Writes data to Flash User NV.

| Parameter | Description |
|-----------|-------------|
| `offset` | Byte offset from the start of User NV (32-byte alignment recommended) |
| `data` | Pointer to the data to write |
| `size` | Number of bytes to write |

| Return value | Meaning |
|--------------|---------|
| `0` | Success |
| `-1` | Parameter error |
| `-2` | Flash programming failure |

> **Caution**: Flash can only be written after an erase — bits can only go from 1 to 0. Call `XM_UserNV_Erase()` before writing new data.
> STM32H7 Flash is programmed in 32-byte (256-bit) units.
> Frequent writes wear out Flash. The recommended pattern is: read once at boot, write once on shutdown or configuration change.

### `XM_UserNV_Erase()`

```c
int32_t XM_UserNV_Erase(void);
```

Erases the entire Flash User NV region (fills with 0xFF).

| Return value | Meaning |
|--------------|---------|
| `0` | Success |
| `-2` | Flash erase failure |

> Erases the full 128 KB sector. Takes approximately 1–2 seconds. **Do not call this from a real-time control loop.**

### `XM_UserNV_IsErased()`

```c
bool XM_UserNV_IsErased(void);
```

Checks whether the Flash User NV region is blank (entirely 0xFF).

---

## Usage Example

```c
// Settings structure definition
typedef struct {
    float kp_gain;
    float kd_gain;
    uint32_t magic;  // Validity sentinel
} UserSettings_t;

#define SETTINGS_MAGIC 0xCAFEBEEF

void LoadSettings(UserSettings_t* s) {
    XM_UserNV_Read(0, s, sizeof(UserSettings_t));
    if (s->magic != SETTINGS_MAGIC) {
        // Use default values
        s->kp_gain = 4.8f;
        s->kd_gain = 0.6f;
        s->magic = SETTINGS_MAGIC;
    }
}

void SaveSettings(const UserSettings_t* s) {
    XM_UserNV_Erase();  // Erase the full sector first (required)
    XM_UserNV_Write(0, s, sizeof(UserSettings_t));
}
```

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `XM_UserNV_Write` returns `-2` | Write attempted without erasing first (Flash bits can only go 1 → 0) | Call `XM_UserNV_Erase()` first |
| Flash wears out quickly | Write called every cycle or every second | Read once at boot; write only on shutdown or configuration change |
| DMA does not work with DTCM variables | DTCM is CPU-only; DMA cannot access it | Move the buffer to RAM_D1 or SRAM |
| `XM_DTCM_VAR` variable contains garbage instead of zero | DTCM `.bss` is not zero-initialized in some cases | Add explicit `= 0` initializers or call `memset` in `Control_Setup` |
| Large array declared inside an `XM_RAMFUNC` function | ITCM has a limited size (64 KB) | Keep large data in RAM_D1; place only the function itself in ITCM |
| `XM_UserNV_Read` returns all 0xFF | Region was erased but never written (erased state = 0xFF) | Check with `XM_UserNV_IsErased()` first and load defaults if blank |
| Settings structure stored without a magic field | Garbage values mistaken for valid data on first boot | Add a sentinel field such as `magic = 0xCAFEBEEF` to validate stored data |

---

## Related Examples

| Example | Difficulty | Memory Usage |
|---------|------------|--------------|
| [16_TinyAI_Sensor_Fusion](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) | Advanced | NN weights (`.rodata` constant arrays) |
| [19_Memory_Aware_Design](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/19_Memory_Aware_Design/) | Advanced | Ring buffer + pool allocator (static `.bss` allocation) |
