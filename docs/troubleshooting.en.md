# Troubleshooting — FAQ & Problem Resolution

This document covers common issues encountered during XM10 development and how to resolve them.

---

## Build Errors

### Path Length Issues (Windows MAX_PATH)

**Symptom:** Build fails with `No such file or directory` or `file name too long`

**Cause:** Windows enforces a default path length limit of 260 characters. If the project is nested deeply or lives inside a OneDrive sync folder, the build output path can exceed this limit.

**Resolution:**

**Option 1: Extract the SDK to a short path (recommended)**

The SDK is distributed as a ZIP from GitHub Releases. Extracting it close to the drive root avoids path length issues.

```powershell
# After downloading your Rev ZIP from the Releases page:
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\XM_SDK\ -Force
```

| Case | Example path | Approximate build path length |
| :--- | :--- | :---: |
| Recommended | `C:\XM_SDK\` | ~130 chars |
| Typical | `C:\Users\Name\Documents\GitHub\Extension_Module\` | ~190 chars |
| Risky | `C:\Users\...\OneDrive - Company\...\Extension_Module\` | May exceed 260 chars |

**Option 2: Enable Windows Long Path support (permanent fix)**

On Windows 10 (1607+) / Windows 11 you can lift the 260-character limit entirely.

PowerShell (run as Administrator):
```powershell
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

Or via Group Policy:
`Computer Configuration > Administrative Templates > System > Filesystem > Enable Win32 long paths` → **Enabled**

> A PC restart is required after applying this setting. It only needs to be done once and applies to all tools — Git, CMake, GCC, and others.

---

### Non-ASCII / Space Characters in Path

**Symptom:** Build fails with `No such file or directory` or encoding-related errors

**Cause:** The project path or the STM32CubeIDE installation path contains non-ASCII characters (e.g., Korean), spaces, or special characters.

**Resolution:**
* Move the project to an ASCII-only path (e.g., `C:\XM_SDK\`)
* Install STM32CubeIDE to an ASCII-only path (e.g., `C:\dev\STM32CubeIDE`)
* If your Windows username is non-ASCII: place the project under a drive root directory rather than inside your user folder

---

### Duplicate `nano.specs` Error

**Symptom:** `fatal error: nano.specs: attempt to rename spec 'link' to already defined spec 'nano_link'`

**Cause:** The `--specs=nano.specs` flag is applied more than once in the CMake build configuration.

**Resolution:** In `CMakeLists.txt`, verify that `CMAKE_C_FLAGS` and `CMAKE_C_FLAGS_DEBUG/RELEASE` do not include the same flags. Place shared flags in `CMAKE_C_FLAGS` and use `DEBUG/RELEASE` variants only for build-type-specific flags.

---

### IOIF Macro Redefinition Warnings

**Symptom:** Warnings such as `warning: "AGRB_IOIF_FDCAN_ENABLE" redefined`

**Cause:** `AGRB_IOIF_*_ENABLE` macros are defined in both the `-D` flags in `CMakeLists.txt` and in `ioif_conf.h`.

**Resolution:** Remove the `AGRB_IOIF_*` definitions from `PROJECT_DEFINES` in `CMakeLists.txt` and manage them exclusively in `ioif_conf.h`. These warnings do not affect behavior, but eliminating them keeps the build output clean.

---

## Board Revision / SDK ZIP Mismatch

### Center or Right Button Not Responding, or an Unpressed Button Always Reads as Pressed

**Symptom:**
- `XM_GetButtonEvent(XM_BTN_2)` does not respond when the center button is pressed
- Only one button works; the others are detected under a different ID, or are stuck (`XM_PRESSED` is fixed)
- Comparing `XM_GetButtonState` against `HAL_GPIO_ReadPin` reveals the mapping is shifted by one position

**Cause:** Rev 1.1 and Rev 2.0 boards assign the three built-in buttons to different MCU pins. When Rev 2.0 was introduced, the button pins shifted by one position to make room for the external SRAM (PSRAM).

| | BTN 1 (left) | BTN 2 (center) | BTN 3 (right) |
|---|---|---|---|
| Rev 1.1 | PC10 | PC11 | PC12 |
| Rev 2.0 | PC11 | PC12 | PC13 |

The API names (`XM_BTN_1/2/3`) retain their left/center/right semantics on both revisions, but which MCU pin is actually read depends on the `main.h` pin definitions in your Rev's SDK. **Building with the wrong ZIP** produces a successful build, but the library reads the wrong pins, causing the mapping to be off by one.

**Resolution:**
1. Check the board label for `Rev 1.1` or `Rev 2.0`. If the label is unclear, look for an RJ45 Ethernet port — only Rev 2.0 has one. For a detailed comparison, see [docs/hardware/README.md — Board Revision Comparison](hardware/README.md#보드-리비전-비교).
2. Download the ZIP that matches your board revision from [Releases](https://github.com/AGR-EXO/Extension_Module/releases) and re-import it into STM32CubeIDE.
3. Rebuild, flash, and confirm that all three buttons in Ex.01 (Button & LED Basic) work correctly.

**Diagnostic code (optional):** To directly identify the active pins on your board:

```c
// Read PC10–PC13 simultaneously inside Run_Loop and compare
uint8_t pc10 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10);
uint8_t pc11 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11);
uint8_t pc12 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_12);
uint8_t pc13 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
// Press each button (left, center, right) one at a time and observe which pin goes low
// → Rev 1.1 board: PC10/11/12 go low; Rev 2.0 board: PC11/12/13 go low
```

> ⚠️ The direct HAL reads above are for diagnostics only. Once you have confirmed normal behavior, switch back to `XM_GetButtonEvent` / `XM_GetButtonState` — the library handles debouncing and click/long-press detection.

---

## Ex.36 (OnDevice Kinesthetic Learning) — Rev 1.1 Build Failure

**Symptom:** Building Ex.36 with the Rev 1.1 SDK produces the following linker errors:

```
control_task.c:(.text.Active_Entry+0x2a): undefined reference to `XM_UserNV_Read'
control_task.c:(.text.Active_Loop+0x284): undefined reference to `XM_UserNV_Erase'
control_task.c:(.text.Active_Loop+0x292): undefined reference to `XM_UserNV_Write'
collect2.exe: error: ld returned 1 exit status
```

**Cause:** Ex.36 uses the Internal Flash UserNV API (`XM_UserNV_Read/Write/Erase`) to persist trained NN weights across power cycles. This API is **only included in the Rev 2.0 SDK's `libXM_Lib.a`** and has not yet been backported to the Rev 1.1 library.

**Resolution:**
- **If your board is Rev 2.0** → Re-import the Rev 2.0 SDK ZIP and rebuild.
- **If your board is Rev 1.1** → Ex.36 is not available. Complete up through Ex.35 (`MultiLayer_Transparent_Control`), then proceed to Ex.36 once you have a Rev 2.0 board. If you only want to study the NN + LQR logic in Ex.36, you can temporarily remove the BTN1 long-press (Flash save) behavior and run the learn/replay loop in RAM only as a study exercise.

> If you are unsure of your board revision, use the label or the RJ45 / PSRAM / button pin mapping table in [docs/hardware/README.md — Board Revision Comparison](hardware/README.md#보드-리비전-비교).

---

## USB Connection Issues

### USB-CDC Not Recognized by PC

**Checklist:**
1. Confirm the USB-C cable supports **data transfer** (charge-only cables will not work)
2. In Windows Device Manager, check for `STMicroelectronics Virtual COM Port` under `Ports (COM & LPT)`
3. If the driver is missing, install the [STM32 Virtual COM Port Driver](https://www.st.com/en/development-tools/stsw-stm32102.html)
4. Verify that the USB CDC initialization in the XM10 firmware completed successfully

### USB-MSC Not Recognized

**Checklist:**
1. Confirm the USB drive is fully inserted into the XM10 board's USB Host port
2. Verify the USB drive is formatted as **FAT32** (NTFS and exFAT are not supported)
3. Check that the drive capacity is **32 GB or less** (recommended: SanDisk Ultra Dual Drive Type-C 32 GB)
4. Confirm that the MSC initialization in the XM10 firmware succeeded and the filesystem is mounted

---

## CAN-FD Communication Issues

### Cannot Communicate with KIT H10

**Checklist:**
1. Confirm the XM10 ↔ KIT H10 cable is firmly connected
2. Verify KIT H10 is powered on (24 V supply)
3. Check the connector pinmap: CAN HIGH (pin 5), CAN LOW (pin 6) must be correctly wired
4. Confirm the CAN-FD baud rate settings match on both sides

### Sensor Hub Module Integration Error

**Checklist:**
1. Verify there are no Node ID conflicts among sensor hub modules
2. Confirm device discovery via AGR PnP V2 completes successfully
3. Check that the sensor hub module firmware version is compatible with the XM10 SDK

---

## Debugging

### ST-Link Connection Failure

**Checklist:**
1. Verify the ST-Link debugger's USB connection
2. Confirm the SWD 4-pin cable is wired correctly (SWDIO, SWCLK, GND, 3.3V)
3. In STM32CubeIDE's Debug Configuration, check that ST-Link is detected
4. Update ST-Link firmware to the latest version (`Help > ST-Link Upgrade`)

### Variable Shows as "Optimized Out" During Debugging

**Cause:** In a Release build (`-O2`), the compiler optimizes away variables that it considers unnecessary.

**Resolution:** Build using the Debug configuration (`-Og -g3`). You can verify the optimization setting under `Project > Properties > C/C++ Build > Settings > Optimization`.

---

## Common Pitfalls

### Code Patterns

#### `sprintf("%.2f", val)` Prints an Integer

**Cause:** newlib-nano (the default ARM runtime library) does not support floating-point format specifiers by default.

**Resolution:** In STM32CubeIDE, go to `Project > Properties > C/C++ Build > Settings > MCU Settings` and check **"Use float with printf"**.

#### Missing `static` Causes Variable to Reset to 0 on Every Call

**Symptom:** Toggle logic never flips, or a timer resets every iteration.

**Cause:** A local variable is allocated on the stack on every call and starts fresh each time. Use the `static` keyword to preserve its value across calls.

```c
// ❌ Wrong
static void Run_Loop(void) {
    bool toggle = false;          // Resets to false on every call
    if (clicked) toggle = !toggle;  // Can never become true
}

// ✅ Correct
static void Run_Loop(void) {
    static bool toggle = false;    // Retained across calls
    if (clicked) toggle = !toggle;
}
```

#### State Callbacks Not Firing — Missing `XM_TSM_Run()`

**Symptom:** A state registered with `Add_State` never triggers its `on_loop` callback.

**Resolution:** Call `XM_TSM_Run(handle);` inside `Control_Loop()`.

#### Active Low vs. Active High Confusion

**Symptom:** Pressing an external switch causes the LED to behave in reverse.

**Rules:**
- Pull-up (`INPUT_PULLUP`) + switch to GND → pressed = `XM_LOW` (Active Low)
- Pull-down (`INPUT_PULLDOWN`) + switch to 3.3 V → pressed = `XM_HIGH` (Active High)

The SDK's built-in buttons (`XM_GetButtonState`) are abstracted and always return `XM_PRESSED` when pressed.

#### Missing Edge Detection

**Symptom:** A single button press triggers multiple toggles.

**Resolution:** Compare the current state against the previous state (`s_btn_prev`) to detect only the exact moment the button is pressed.

```c
bool btn_now = (XM_DigitalRead(BTN) == XM_LOW);
bool pressed = (btn_now && !s_btn_prev);  // Leading edge
s_btn_prev = btn_now;
if (pressed) { /* Execute once */ }
```

Alternatively, use the read-clear behavior of `XM_GetButtonEvent()` (see Ex.02).

### USB / Communication Pitfalls

#### USB-CDC Messages Not Visible in PhAI Studio or a Terminal

**Cause 1:** Another serial client is holding the same COM port open (e.g., PhAI Studio and PuTTY running simultaneously)

**Cause 2:** The USB-C cable is charge-only and cannot transfer data

**Cause 3:** `STMicroelectronics Virtual COM Port` is not recognized in Windows Device Manager

**Resolution steps:**
1. Close all serial clients, then open only one at a time
2. Use a data-capable USB-C cable (preferably connected directly to a rear USB-A port on the PC — avoid hubs)
3. Install the [STM32 VCP driver](https://www.st.com/en/development-tools/stsw-stm32102.html)

#### Python Decoder Cannot Read a `.bin` File ("size mismatch")

**Cause:** The struct size does not match the sum declared in `metadata.txt` due to padding or alignment differences.

**Resolution:**
1. On the board, use `XM_SendUsbDebugMessage` to print `printf("size=%u", sizeof(MyStruct))`
2. Verify that the field sizes in the metadata sum to the reported value
3. If they do not match, either add explicit `_pad(Nbytes)` entries or apply `__attribute__((packed))` to the struct

### KIT H10 / Robot Control Pitfalls

#### `XM.status.h10.*` Fields Are All Zero

- KIT H10 is powered off → Check the 24 V supply
- CAN-FD HIGH/LOW pins are swapped → Verify the pinmap ([01-hardware-setup.md Figure 1](getting-started/01-hardware-setup.md))
- KIT H10 firmware is below v2.3.0 → Follow the [kit-h10-firmware/](kit-h10-firmware/) guide

#### `forwardVelocity`, `footContact`, and Other Gait Analysis Fields Are Always Zero

**Cause:** The prerequisite body parameters have not been provided — `XM_SendUserBodyData()` has not been called.

**Resolution:** See [examples/README.md — Body Data guide](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md#part-5).

#### Called `SetAssistTorque` but KIT H10 Does Not Move

- `XM_SetControlMode(XM_CTRL_TORQUE)` is missing → Call it once when entering the Active state
- Safety switch is triggered → Check the ERROR state
- KIT H10 motor activation did not complete → Check the chassis LED and torque queue

### Environment / System Pitfalls

#### Windows Username Contains Non-ASCII Characters

**Symptom:** Building from a path like `C:\Users\홍길동\...` causes encoding or MAX_PATH errors.

**Resolution:** Extract the SDK to a **drive root directory** instead of your user folder:
```powershell
Expand-Archive -Path "$HOME\Downloads\Rev2.0.zip" -DestinationPath C:\dev\ -Force
```

#### SDK Extracted Inside a Cloud Sync Folder (OneDrive, iCloud)

**Symptom:** Build fails due to file locking conflicts; conflict copies are created by the sync client.

**Resolution:** Move the SDK to a non-synced folder (e.g., `C:\dev\`).

---

If your issue is not resolved, open a ticket on [GitHub Issues](https://github.com/AGR-EXO/Extension_Module/issues).

> 🤖 Claude Code users: type a single line such as `"Ex.XX isn't working"` or `"build error"` and the `example-helper` skill will reference the relevant entry on this page along with the ⚠️ section in the example's README.
