# 03 — First Build & Run

This is the final step. You should have your first firmware running on the board — with an LED blinking — in about 20 minutes. This guide assumes you have already completed [01 Hardware Setup](01-hardware-setup.md) and [02 Software Setup](02-software-setup.md).

The C code you write is just text. The processor inside the board cannot read text — it only executes machine code. That means every change requires two steps:

- **Build** — compile your C code into machine code (`.elf`)
- **Flash** — write that machine code into the board's memory

You will repeat these two steps dozens or hundreds of times as you develop.

---

## Build Outputs

STM32CubeIDE generates these automatically:

- `Debug/Extension_Module.elf` — debug build with symbol information (used during development)
- `Debug/Extension_Module.bin` / `.hex` — binary files for production deployment
- Console log — compile errors, warnings, and memory usage summary

---

## Steps

### 1. Import the Project

`File` → `Import...` → `General` → **`Existing Projects into Workspace`** → `Next`

Click `Browse...` and select **the extracted folder itself**:
- Recommended path: `C:\dev\Extension_Module\` (the `.project` file is at the folder root)

The SDK ZIP ships as a flat archive per revision (Rev1.1.zip / Rev2.0.zip), so extracting it places your revision's SDK directly at the ZIP root. There is no inner revision subfolder to navigate into.

When `Extension_Module` appears in the `Projects:` list, click `Finish`.

✅ `Extension_Module` now appears in the Project Explorer panel on the left.

### 2. Apply Example Code (Optional)

The default project builds and boots as-is — LED 1 will blink in a heartbeat pattern. To try a specific example:

- **Option A (recommended)** — Copy the entire contents of an example file such as `Examples/00_Quick_Start/quick_start.c` and paste it into `XM_Apps/Control_Task/control_task.c`.
- **Option B** — Move the example `.c` file into the `Control_Task` folder and delete the existing `control_task.c`.

### 3. Build

Go to `Project` → `Build All` (shortcut: `Ctrl + B`), or click the **hammer icon** in the toolbar.

The Console panel streams the compile log in real time. The first build takes roughly 5 minutes because it compiles several hundred files.

✅ A successful build ends with:
```
   text    data     bss     dec     hex filename
 XXXXXX   YYYYY   ZZZZZ  AAAAAA  BBBBBB Extension_Module.elf

13:23:45 Build Finished. 0 errors, N warnings. (took XmYs)
```
**"0 errors"** is what matters. Warnings can be ignored.

### 4. Flash & Run

**Debug mode (recommended):**
1. Click the bug icon (Debug) in the toolbar.
2. If ST-Link is detected, STM32CubeIDE automatically flashes the firmware and pauses execution.
3. STM32CubeIDE switches to the Debug perspective.
4. Press `Resume` (F8) to start running code on the board.

**Normal run mode:**
1. Click the play icon (Run) in the toolbar.
2. After flashing completes, the board starts running automatically.

### 5. Verify Operation

If LED 1 on the XM10 board blinks in a heartbeat pattern at a 1-second interval, the firmware is running correctly.

If you applied example code, follow the "Experiment" steps in that example's README (e.g., [Ex.00](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/README.md): press BTN 1 and check the USB message).

---

## Common Issues

### Build

- **`fatal error: 'xxx.h' file not found`** — An include path is missing. Check Project Properties → C/C++ Build → Settings.
- **`undefined reference to 'xxx'`** — A library (`.a`) file is missing. The SDK folder may be corrupted — go back to step 02 and re-download and re-extract the ZIP.
- **`region 'RAM' overflowed by N bytes`** — Your code exceeds the memory limit. Start by reducing the size of large arrays or structs.
- **Build is very slow** — Your antivirus software may be continuously scanning temporary build files. Add the STM32CubeIDE workspace folder to the antivirus exclusion list.
- **`1 errors`** — Copy the full red error message from the top of the Console and ask an AI assistant about it.

### Flash

- **"No ST-Link detected"** — Check your USB cable and port. Avoid USB hubs; plug directly into a rear port on your PC.
- **"Target no device found"** — The board may not be powered, or the SWD 4-pin connector is misaligned. Check the pin 1 marker.
- **"Old firmware on ST-Link, please update"** — STM32CubeIDE is asking you to update the ST-Link firmware. Click Yes.
- **"Connection error" during flash** — In Debug Configurations → Debugger tab, change Reset Behaviour to `Connect under reset`.

### Runtime

- **LED 1 does not turn on** — Flash may have failed, or the board halted after booting. Press the Reset button on the board once.
- **LED blinks very rapidly (error signal)** — The firmware entered a fault handler. In the Debug perspective, click Suspend and check which function the PC register points to.
- **No USB serial messages appear** — The USB-CDC serial port can only be opened by one application at a time. Close any other clients such as PhAI Studio and try again.

---

## What's Next

Your development environment is ready. Time to start writing code.

- First example → [Ex.00 Quick Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/README.md) (verifies board operation)
- Learning path → [tutorials/README.md](../tutorials/README.md) (46 examples + recommended order)
- Suggested starting sequence: Ex.00 → Ex.01 → Ex.02 → Ex.03 (buttons + LEDs, ⭐~⭐⭐)
