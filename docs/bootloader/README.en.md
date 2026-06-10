# AGR_BOOT V2 — XM10 Bootloader Guide

> 📌 **What you'll learn**: Two firmware upload methods — STM32CubeIDE SWD debugging vs. PhAI Studio USB FTP — plus how to avoid corrupting the bootloader and how the automatic rollback mechanism works.
> ⏱️ Estimated reading time: 30 minutes (60 minutes including hands-on)
> 🧰 Prerequisites: STM32CubeIDE installed ([docs/getting-started/02-software-setup.md](../getting-started/02-software-setup.md))
> 🎯 Key point: **Debug Configuration Start address = `0x08040000`** (leaving it at the default `0x08000000` destroys the bootloader)

> ⚠️ **Never do this**: STM32CubeIDE Debug → Start address `0x08000000` → bootloader corrupted. Always use `0x08040000`.

> **Version**: v1.1.0 | **Target module**: XM10 Extension Module (STM32H743XI) | **Created**: 2026-04-02

---

## Table of Contents

1. [Overview](#1-overview)
2. [Flash Memory Map](#2-flash-memory-map)
3. [Initial Installation (starting from a blank board)](#3-initial-installation-starting-from-a-blank-board)
4. [App Firmware Upload — STM32CubeIDE (SWD Debugging)](#4-app-firmware-upload--stm32cubeide-swd-debugging)
5. [App Firmware Upload — PhAI Studio (USB FTP)](#5-app-firmware-upload--phai-studio-usb-ftp)
6. [Post-Build Binary Generation](#6-post-build-binary-generation)
7. [How the Bootloader Works](#7-how-the-bootloader-works)
8. [Troubleshooting](#8-troubleshooting)
9. [Important Notes](#9-important-notes)

---

## 1. Overview

The XM10 Extension Module ships with the **AGR_BOOT V2** bootloader, which enables firmware updates over USB.

### What the bootloader does
- **Validates** the app firmware at power-on (signature + CRC-32 + HW revision check)
- Receives app firmware uploads via the **USB CDC FTP** protocol
- **Automatically rolls back** to the backup firmware if an update fails (after 3 consecutive failed boots)
- Supports both Rev1.1 and Rev2.0 hardware from a **single bootloader binary** (GPIO auto-detection)

### Required tools

| Tool | Purpose | Download |
|------|---------|----------|
| **STM32CubeProgrammer** | Initial bootloader installation (SWD/JTAG) | [st.com](https://www.st.com/en/development-tools/stm32cubeprog.html) |
| **STM32CubeIDE** | App firmware build + SWD debugging | [st.com](https://www.st.com/en/development-tools/stm32cubeide.html) |
| **PhAI Studio** | USB FTP firmware upload | [studio.onephai.com](https://studio.onephai.com) |
| **ST-Link V3** | SWD debugging probe | ST-Link V2/V3 compatible |

---

## 2. Flash Memory Map

The STM32H743XI has 2 MB of internal Flash (Bank1 + Bank2).

```
┌──────────────────────────────────────────────────────────────┐
│ Address         │ Size    │ Region         │ Description       │
├──────────────────────────────────────────────────────────────┤
│ 0x08000000      │ 256 KB  │ Bootloader     │ AGR_BOOT V2       │
│                 │         │                │ (Bank1 S0-S1)     │
├──────────────────────────────────────────────────────────────┤
│ 0x08040000      │ 1 KB    │ FW Header      │ AGR_FwInfo_t      │
│                 │         │                │ (signature+CRC+version) │
├──────────────────────────────────────────────────────────────┤
│ 0x08040400      │ 767 KB  │ App (Active)   │ User app firmware │
│                 │         │                │ (Bank1 S2-S7)     │
├──────────────────────────────────────────────────────────────┤
│ 0x08100000      │ 768 KB  │ Backup Slot    │ Previous firmware backup │
│                 │         │                │ (Bank2 S0-S5)     │
├──────────────────────────────────────────────────────────────┤
│ 0x081C0000      │ 128 KB  │ Boot Config    │ AGR_BootConfig_t  │
│                 │         │                │ (Bank2 S6)        │
├──────────────────────────────────────────────────────────────┤
│ 0x081E0000      │ 128 KB  │ Reserved       │ Reserved for future use │
│                 │         │                │ (Bank2 S7)        │
└──────────────────────────────────────────────────────────────┘
```

> **Important**: The bootloader (0x08000000) and the app (0x08040000) occupy completely separate regions.  
> Flashing the app over SWD does **not** damage the bootloader.

---

## 3. Initial Installation (starting from a blank board)

Follow these steps when the board has never had a bootloader installed, or when you are installing the bootloader for the first time on a board that was previously used without one.

### Step 1: Full Chip Erase

> **Why is this necessary?** If the existing firmware was a single binary starting at 0x08000000, its address range will conflict with the bootloader. Always perform a full erase before proceeding.

1. Launch **STM32CubeProgrammer**
2. Connect ST-Link to the board
3. Click **Connect** in the left panel (SWD or JTAG)
4. Select **Erasing & Programming** (download icon) from the left menu
5. Click the **Full chip erase** button at the top
6. Confirm the "Erase complete" message

### Step 2: Upload the Bootloader Binary

1. In the **Erasing & Programming** tab:
   - **File path**: select `AGR_Bootloader.bin` (downloaded from the Release page)
   - **Start address**: `0x08000000` (leave as default)
   - **Skip flash erase before programming**: unchecked (default)
   - **Verify programming**: checked
2. Click **Start Programming**
3. Confirm the "Download complete" + "Verification OK" messages

> **Using a Hex file**: Select `AGR_Bootloader.hex` and the address is set automatically.

### Step 3: Verify

1. Reset the board (or power-cycle it)
2. The bootloader starts and:
   - Because no valid app firmware exists, it enters **FTP mode**
   - A USB CDC port appears on the PC (COM port)
3. Proceed to **Step 4 or Step 5** to upload the app firmware

---

## 4. App Firmware Upload — STM32CubeIDE (SWD Debugging)

This is the most common method during development. It writes directly to Flash over SWD.

### One-time Setup

**Configure the Debug Configuration:**

1. Open **Run → Debug Configurations → STM32 C/C++ Application**
2. Create a new configuration or select an existing one
3. Under the **Debugger** tab:
   - **Download**: ✅ checked
   - **Start address**: `0x08040000` (⚠️ not the default 0x08000000!)
   - **Size**: `0x000C0000` (768 KB = Active Slot)
   - **Reset behaviour**: Software Reset

> **⚠️ Critical**: Set Start address to `0x08040000`.  
> Leaving it at the default (0x08000000) **overwrites the bootloader**.

### Build & Debug

1. **Build** the project in STM32CubeIDE (Ctrl+B)
2. Post-build scripts run automatically:
   - `size_report.py` → memory usage report
   - `version_generator.py` → generates `version.h` from the git tag
   - `patch_fw_info.py` → patches `fw_size`/CRC into the `.fw_header` section of the ELF
   - `fw_packager.py` → creates the packaged binary for FTP upload
3. Click **Debug** (F11) → the app firmware is written to 0x08040000 over SWD
4. Debug normally (breakpoints, variable watch, etc.)

### Why SWD Debugging is Compatible with the Bootloader

- The linker script (`STM32H743XIHX_FLASH.ld`) places app code at `0x08040400`
- The `.fw_header` section is placed at `0x08040000` so the bootloader can read the signature
- `boot_fw_info.c` embeds the `AGRBOOT` signature in the ELF, ensuring the bootloader recognises the app even after an SWD flash

---

## 5. App Firmware Upload — PhAI Studio (USB FTP)

This method uploads firmware over a USB cable alone, with no SWD debugger required.  
It is the recommended approach for **production environments**, **field updates**, and **end users**.

### Required File

After a successful build, the `Debug/` folder contains:

```
Debug/
├── Extension_Module.elf        ← For SWD debugging
├── Extension_Module.bin        ← For SWD flash (includes FW Header)
├── Extension_Module.hex        ← For SWD flash
├── Extension_Module_app.bin    ← For FTP upload (no FW Header)
└── XM10_X_X_X_X.bin           ← Packaged binary for FTP upload ★
```

> **File to use in PhAI Studio**: `XM10_X_X_X_X.bin` (e.g. `XM10_2_0_1_0.bin`)  
> This file is produced by `fw_packager.py` and consists of a 1 KB `AGR_FwInfo_t` header followed by the app binary.

### Upload Procedure

1. Open **PhAI Studio**: [studio.onephai.com](https://studio.onephai.com)
2. Connect the XM10 board to your PC via USB cable
3. Confirm that PhAI Studio automatically detects the device
4. Select the **FW Upload** menu
5. Choose the `XM10_X_X_X_X.bin` file
6. Start the **Upload**
7. Monitor the progress bar → the board reboots automatically when complete
8. The bootloader validates the new firmware → app launches

### Forcing FTP Mode

To switch to FTP mode while the app is already running:
- Send the **Enter Bootloader** command from PhAI Studio
- Or call `AGR_Boot_RequestUpdate()` from the app code

---

## 6. Post-Build Binary Generation

When you run **Build** in STM32CubeIDE, the following post-build scripts execute in order:

```
[Build]   GCC compile + link → Extension_Module.elf
           │
[Step 1]  size_report.py → memory usage report (console output)
           │
[Step 2]  version_generator.py → git tag → auto-generate version.h
           │
[Step 3]  patch_fw_info.py → patch ELF .fw_header (fw_size, fw_crc32)
           │                  → generate .bin, .hex, _app.bin
           │
[Step 4]  fw_packager.py → _app.bin + FwInfo header → XM10_X_X_X_X.bin
```

### Details of Each Step

| Step | Script | Input | Output | Description |
|------|--------|-------|--------|-------------|
| 1 | `size_report.py` | `.elf` | Console | Visualises FLASH/RAM usage |
| 2 | `version_generator.py` | git tag | `version.h` | Defines `FW_VER_MAJOR/MINOR/PATCH/DEBUG` |
| 3 | `patch_fw_info.py` | `.elf` | `.bin`, `.hex`, `_app.bin` | Patches `fw_size`/CRC into `.fw_header` |
| 4 | `fw_packager.py` | `_app.bin` | `XM10_X_X_X_X.bin` | 1 KB header + app binary = FTP-ready package |

> **Python 3.x required**: All post-build scripts are written in Python.  
> Run `pip install pyyaml` (required for Data Map code generation).

---

## 7. How the Bootloader Works

### Boot Sequence

```
Power ON → Bootloader starts (0x08000000)
    │
    ├─ Read Boot Config (Bank2 S6)
    │   └─ First boot: auto-create config with default values
    │
    ├─ Rollback check (boot_count >= 3?)
    │   └─ Yes: restore from Backup Slot
    │
    ├─ Validate app firmware
    │   ├─ Check "AGRBOOT\x01" signature
    │   ├─ Verify CRC-32 (FTP uploads only)
    │   └─ Check HW revision compatibility
    │
    ├─ Validation passed → jump to app (0x08040400)
    │   └─ App calls AGR_Boot_ConfirmBoot() → resets boot_count
    │
    └─ Validation failed → enter FTP wait mode
        └─ Wait to receive new firmware over USB CDC
```

### FTP Update Sequence

```
FW Upload initiated from PhAI Studio
    │
    ├─ [1] App switches to bootloader (writes magic value to RTC BKP0R + NVIC Reset)
    ├─ [2] Copy current Active FW → Backup Slot
    ├─ [3] Erase Active Slot
    ├─ [4] Receive new FW + write to Active Slot
    ├─ [5] Verify CRC-32
    ├─ [6] Update Boot Config (PENDING_CONFIRM)
    └─ [7] Reset → new app starts → ConfirmBoot() → done
```

---

## 8. Troubleshooting

### "I installed the bootloader but the app won't run"

**Cause**: No app firmware is present, or the firmware is invalid.
1. Build the project in STM32CubeIDE and upload the app via Debug (F11)
2. Or upload `XM10_X_X_X_X.bin` from PhAI Studio

### "After SWD debugging, the bootloader is gone"

**Cause**: The Debug Configuration Start address was set to `0x08000000`.
1. Change it to `0x08040000` (see Section 4)
2. Reinstall the bootloader (see Section 3)

### "PhAI Studio cannot find the device"

1. Check the USB cable connection
2. Verify the COM port in Windows Device Manager
3. Confirm that the bootloader is in FTP mode (if the app is running, send "Enter Bootloader" from PhAI Studio first)

### "After uploading firmware, the board keeps rebooting and reverts to the previous version"

**Cause**: The new firmware never called `AGR_Boot_ConfirmBoot()`, triggering an automatic rollback after 3 boot attempts.
- Check that `system_startup.c` calls `AGR_Boot_ConfirmBoot()` immediately after startup
- The default SDK code already includes this call — verify that `system_startup.c` has not been modified by the user

### "The build succeeds but XM10_X_X_X_X.bin is not generated"

1. Check Python 3.x installation: `python --version`
2. Review post-build script output in the Build Console for error messages
3. Confirm that the script files exist in the `tools/build/` folder

### "STM32CubeProgrammer shows no data at 0x08040000 when I read Flash"

**Cause**: The Start address was set incorrectly when uploading a `.bin` file directly with STM32CubeProgrammer.
- When uploading a `.bin` file, set Start address to `0x08040000`
- When uploading a `.hex` file, the address is embedded in the file and set automatically

---

## 9. Important Notes

### Things you must never do

| Prohibited action | Consequence |
|-------------------|-------------|
| Set Start address to 0x08000000 in STM32CubeIDE Debug | **Bootloader corrupted** |
| Full chip erase in STM32CubeProgrammer, then upload app only | **No bootloader → board cannot boot** |
| Write directly to the bootloader region (0x08000000–0x0803FFFF) | **Bootloader corrupted** |
| Arbitrarily erase the Boot Config region (0x081C0000) | **Boot settings reset (auto-recovery will run)** |

### Common notes for Rev1.1 / Rev2.0

- Both revisions use the **same bootloader binary** (HW revision is detected automatically via GPIO)
- Both revisions use the **same Flash memory map**
- App firmware is built with a **separate `libXM_Lib.a`** for each revision, but the bootloader is shared

### Python Environment Setup

To run the post-build scripts:
```bash
# After installing Python 3.x
pip install pyyaml
```

If STM32CubeIDE's Build Console cannot find the Python interpreter:
- Windows: add the Python installation path to the system `PATH` environment variable
- Restart STM32CubeIDE
