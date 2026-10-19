# API Reference: USB Memory Logging

> 📌 **After reading this page** you will be able to save a struct to a USB memory stick at a fixed rate and decode the saved folder on a PC.
> ⏱️ Estimated reading time: 20 minutes
> 🧰 Prerequisites: [Ex.01–03](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/01_Button_LED_Basic/) buttons · LEDs · state machine
> 🎯 Key functions: `XM_SetUsbLogSource` / `XM_StartUsbDataLog` / `XM_StopUsbDataLog` / `XM_IsUsbLogSessionIdle`
>
> 🟢 **Rev 2.0 only**: This works only with the Rev 2.0 board and the Rev 2.0 SDK. Rev 1.1 (v2.8.1) has no USB memory logging. On Rev 1.1, receive your data with the real-time streaming in [05. USB Connectivity](05-usb-connectivity.en.md).
>
> ⚠️ **There is only one USB-C port**: While a USB memory stick is plugged in, you cannot connect to a PC over USB. Please read [the section below](#usb-c-port) first.
>
> ⚠️ The whole USB memory logging flow (save · stop · remove) and Ex.10 · Ex.34 have not been tested on the board yet. (not yet confirmed on the board) <!-- board-check -->

This page explains the **USB memory logging API** defined in `xm_api_usb_msc.h`.
Register one struct, and while logging is running the system writes that struct to the USB memory stick at the rate you choose.
You do not call a write function. You only fill in the struct's values in `Control_Loop()`.

Move the saved folder to a PC and decode it with XM10 Log Decoder to get a CSV. If any samples are missing, it tells you where and how long.

> v2.5.0 removed USB memory logging, and it is available again. This time, missing samples show up in the statistics, in `summary.txt`, and in the decoder's verdict.

For the exact signature of each function, see the [`xm_api_usb_msc.h` reference](ref/xm-api-usb-msc.en.md). For a step-by-step walkthrough from the beginning, see [Save Data to a USB Memory Stick](../getting-started/05-usb-memory-log.en.md).

-----

## When to Use It

| What you want | How |
|------|------|
| Watch and save in real time with the PC connected | [05. USB Connectivity](05-usb-connectivity.en.md) — view your own struct with the `xm10` tool |
| Save without a PC and decode later on a PC | This page — USB memory logging |

You do not need a PC while saving. The board gets its power from the KIT H10 body, so keep the H10 body powered on.

-----

## Preparing the USB Memory Stick

| Item | Details |
|------|------|
| File system | **FAT32**. exFAT and NTFS are not recognized |
| Allocation unit | 32 KB is recommended |
| Free space | Must be larger than the file-split size (default 10 MB) for logging to start |
| Session name | Becomes the folder name. Use only letters, digits, `_` and `-` |

- Formatting erases every file on the stick. Move any files you need first.
- Windows' built-in format window cannot choose FAT32 for sticks larger than 32 GB. Use a stick of 32 GB or less.
- You can read the remaining space with `XM_GetUsbDiskFreeMB()`. It reads 0 for a moment right after you plug the stick in.
- Sticks differ in write speed, so the number of missing samples can differ from stick to stick. (not yet confirmed on the board) <!-- board-check -->

-----

<a id="usb-c-port"></a>
## There Is Only One USB-C Port

The XM10 has a single USB-C port. You cannot use a PC and a USB memory stick at the same time.

- When you plug in a USB memory stick, the board powers it and reads and writes it.
- While a stick is plugged in, you cannot use PhAI Studio, the `xm10` tool, or firmware upload over USB.
- During that time `XM_IsUsbStreamConnected()` is `false`, and USB-CDC send functions such as `XM_SendUsbDebugMessage()` return `false`. You cannot send messages to the PC while logging.
- You check the logging state with the board's LEDs. The example status LEDs are LED 1 · 2 · 3.
- When logging is done, remove the USB memory stick and connect the PC. (not yet confirmed on the board) <!-- board-check -->

-----

## Order of Use

1. In `Control_Setup()`, register the struct and set the rate. The struct must be global or `static`. If you do not set a rate, it is 1000 Hz.
2. In `Control_Loop()`, fill in the struct's values. The system does the saving after `Control_Loop()` finishes.
3. When both `XM_IsUsbLogReady()` and `XM_IsUsbLogSessionIdle()` are `true`, start with `XM_StartUsbDataLog()`. If it returns `false`, read the reason with `XM_GetUsbLogLastStartError()`.
4. Stop with `XM_StopUsbDataLog()` and wait until `XM_IsUsbLogSessionIdle()` becomes `true`.
5. Once it is `true`, all files are closed, so you may remove the USB memory stick. (not yet confirmed on the board) <!-- board-check -->

This is a trimmed-down version of [Ex.10](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/). The full code (state machine · LEDs · markers · statistics) is in the example. The example uses button 1 alone to alternate between start and stop, while the code below uses button 2 for stop.

```c
/* The struct to save — declared packed so no padding bytes appear between fields */
typedef struct __attribute__((packed)) {
    uint32_t loop_count;
    float    sine;
    float    ramp;
    uint8_t  btn1;
    uint8_t  btn2;
    uint8_t  btn3;
    uint8_t  h10_connected;
} LogRecord_t;
_Static_assert(sizeof(LogRecord_t) == 16U, "LogRecord_t size changed - update the meta line too");

/* Meta line — list the fields in declaration order as "name(type)", separated by commas */
#define LOG_METADATA \
    "loop_count(uint32_t), sine(float), ramp(float), " \
    "btn1(uint8_t), btn2(uint8_t), btn3(uint8_t), h10_connected(uint8_t)"

static LogRecord_t s_log;                                    /* global or static */
volatile XmUsbLogStartError_e g_last_start_error = XM_USB_LOG_START_OK;

void Control_Setup(void)
{
    XM_SetUsbLogSource(&s_log, sizeof(s_log));               /* register the struct to save */
    XM_SetUsbLogSampleRateHz(100);                           /* 100 Hz — 1000 Hz if omitted */
}

void Control_Loop(void)
{
    s_log.loop_count++;                                      /* just fill in values; the system does the saving */

    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {       /* button 1: start */
        if (XM_IsUsbLogReady() && XM_IsUsbLogSessionIdle()) {
            if (!XM_StartUsbDataLog("", LOG_METADATA)) {     /* "" makes the folder name from the clock */
                g_last_start_error = XM_GetUsbLogLastStartError();   /* why it did not start */
            }
        }
    }

    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {       /* button 2: request stop */
        XM_StopUsbDataLog();                                 /* the file is closed in the background */
    }
}
```

- Call the setup · start · stop · marker · statistics functions only from `Control_Setup()` or `Control_Loop()`. Called from a background task or an interrupt, they do nothing and return `false`.
- The status-reading functions (`XM_IsUsbLog…` · `XM_GetUsbLogLastStartError` · `XM_GetUsbDisk…`) can be called from anywhere.
- `XM_StartUsbDataLog()` returns immediately without waiting. `true` means "the start request was accepted". When the file opens, `XM_IsUsbLogRecording()` becomes `true`.
- If opening the file fails, `XM_IsUsbLogSessionIdle()` becomes `true` again and the reason is left in `XM_GetUsbLogLastStartError()`.
- If you stop right after starting, an empty folder with no records can be left behind.
- To leave a mark while logging, call `XM_InsertUsbLogMarker(type, value)`. Choose the meaning of `type` (1 – 254) yourself; `value` is an extra value to store with it. The decoder collects the marks separately in `events.csv`.

-----

## Logging Rate and Struct Size

- The logging rate is a divisor of 1000: 1000 · 500 · 250 · 200 · 125 · 100 · 50 · 40 · 25 · 20 · 10 · 8 · 5 · 4 · 2 · 1 Hz. If you do not set one, it is 1000 Hz.
- Below 1000 Hz, one sample is saved per period and the values in between are not saved. The timestamp must be on (it is by default).
- If the struct is too big for the rate, logging does not start and `XM_USB_LOG_START_TOO_BIG_FOR_RATE` is returned.
- A smaller struct gives the internal buffer more margin. Keep only the values you need.
- When you log at 1000 Hz, some samples may be missing. The number missing and the longest gap show up in `XM_GetUsbLogStats()` and in the decoder result.
- If the decoder report says "내부 버퍼 여유: 부족" (internal buffer margin: insufficient), make the struct smaller or lower the logging rate. <!-- release-check -->

The struct sizes that can start with the default settings (timestamp on):

| Logging rate | Struct size that can start | Example |
|------|------|------|
| 1000 Hz (default) | Up to 134 B | Ex.34 (96 B) |
| 200 Hz or lower | Up to 512 B | Ex.10 (16 B, 100 Hz) |
| 500 Hz · 250 Hz | Depends on the rate. Too big a struct does not start | — |

-----

<a id="meta-line"></a>
## Meta Line

The meta line is a single line that lists the struct's fields in declaration order as `name(type)`, separated by commas. The decoder uses this line to decode the data.

```c
"loop_count(uint32_t), hip_L(float), hip_R(float), contact(uint8_t), _pad(3bytes)"
```

- The types are `uint8_t` `int8_t` `uint16_t` `int16_t` `uint32_t` `int32_t` `float` `double` `bool`, written in lowercase exactly as shown.
- Write unused bytes as `_name(Nbytes)`, for example `_pad(3bytes)`. The name must start with `_` to count as padding.
- Use only letters, digits and `_` in names, put no spaces inside `name(type)`, and keep everything on one line.
- If the sum of the field sizes differs from the registered struct size, logging does not start and `XM_USB_LOG_START_METADATA_MISMATCH` is returned.
- The system checks only the sum of sizes. It does not check that names, types and order match the struct, so match them yourself.
- Declare the struct with `__attribute__((packed))` and fix its size with `_Static_assert`.
- Do not use the names the decoder uses (`tick_ms` · `t_grid_ms` · `row_origin` · `count` · `h10_loop_count`) as field names unless you mean that.
- The meta line can be up to 1700 characters (`XM_USB_LOG_MAX_METADATA_LEN`).

-----

## Files

Saved files go in the `/LOGS/<session name>/` folder on the USB memory stick. If you leave the session name empty, `YYMMDD_HHMMSS` is made from the clock (RTC).

```text
/LOGS/<session name>/
    metadata.txt             the meta line and logging settings
    data_000_part_000.bin    the saved data
    data_000_part_001.bin    the next part of the saved data
    summary.txt              a summary written when logging ends
```

- When a file passes the set size (default 10 MB), a new part starts. (not yet confirmed on the board) <!-- board-check -->
- If you give the whole folder to the decoder, it joins the parts and decodes them.
- If a folder with the same name already exists, logging continues in it. The rate, struct size and timestamp setting must be the same. If they differ, `XM_USB_LOG_START_FOLDER_MISMATCH` is returned.
- When continuing, `metadata.txt` keeps what was written the first time. If you changed a field name or type, use a different name.
- If you do not set the clock with [`XM_RTC_SetDateTime()`](08-rtc-clock.en.md), the folder name will not match the real date and can collide with an earlier recording. You can also choose the name yourself.
- If the name is empty and the clock cannot be read, logging does not start and `XM_USB_LOG_START_BAD_NAME` is returned.
- `summary.txt` is written when logging ends. If you remove the USB memory stick while logging, it is not written. (not yet confirmed on the board) <!-- board-check -->
- In `summary.txt`, look at `status` and `error_reason` first. If `error_reason` is `NORMAL`, logging ended normally. Judge the quality from `slots_empty`, `write_errors` and the decoder report. Do not use any other keys to judge the quality.

-----

## Removing the Stick Safely

1. Call `XM_StopUsbDataLog()`. If it returns `false`, call it again a moment later.
2. Wait until `XM_IsUsbLogSessionIdle()` becomes `true`. Meanwhile the system writes the remaining data, closes the files and writes `summary.txt`.
3. Once it is `true`, all files are closed, so you may remove the USB memory stick. (not yet confirmed on the board) <!-- board-check -->

The examples show this with LED 2. While it blinks fast, the files are being closed — do not remove the stick. When it stays on, logging is done. (not yet confirmed on the board) <!-- board-check -->

If `XM_IsUsbLogSessionIdle()` becomes `true` by itself while logging, logging has ended. Examples are the stick being pulled out or the internal buffer overflowing.

- If `overflow_records` is 1 or more, that logging run stops at that point.
- If the USB memory stick becomes full while logging, logging stops by itself. (not yet confirmed on the board) <!-- board-check -->
- If you remove the USB memory stick while logging, the files cannot be closed properly. `summary.txt` is not written, and only the part written so far can be decoded. (not yet confirmed on the board) <!-- board-check -->
- Do not remove the USB memory stick while logging.

-----

## Checking the Result

Read the statistics with `XM_GetUsbLogStats()`. While logging they are the values so far; after logging ends, the last values of that run are kept.

| Field | What it tells you |
|------|------|
| `empty_slots` | Number of missing samples. 0 means none are missing |
| `longest_gap_ms` | The longest gap [ms] |
| `overflow_records` | Number of records that could not be saved because the internal buffer overflowed. 1 or more means that logging run stopped at that point |
| `write_errors` | Number of failed writes to the USB memory stick |

You cannot send statistics to the PC while logging. After logging ends, once you remove the stick and connect the PC, you can send them with `XM_SendUsbDebugMessage()` (button 3 in Ex.10). (not yet confirmed on the board) <!-- board-check -->

-----

## Decoding on the PC

Move the saved `LOGS` folder to a PC and decode it with **XM10 Log Decoder**. It is an installer-based Windows program, and Python is not required. <!-- release-check -->

- Get the decoder from the files attached to the release page. <!-- release-check -->
- The decoder produces the data table (`decoded_output.csv` for 1000 Hz; for other rates `decoded_output_<rate>hz.csv`, for example `decoded_output_100hz.csv` for 100 Hz), `events.csv` if you inserted markers, and `data_quality_report.txt`. <!-- release-check -->
- Missing samples are filled in by interpolation and marked `1` in the `row_origin` column. Interpolated values are not measured values.
- If the verdict in `data_quality_report.txt` is `GOOD`, `USABLE` or `WARNING`, the recording is acceptable. If it is `RE-COLLECT`, `INVALID` or `UNVERIFIED`, record again.
- If `RE-COLLECT` keeps coming up, please contact the Angel Robotics(엔젤로보틱스) CS team.

For how to get it, install it and read the verdict, see the step-by-step [Save Data to a USB Memory Stick](../getting-started/05-usb-memory-log.en.md).

-----

## Examples

| Example | Difficulty | What it does |
|------|--------|------|
| [10_USB_Memory_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/) 🟢 **Rev 2.0 only** | Intermediate | Button 1 starts and stops. Saves one struct (16 B) at 100 Hz |
| [34_USB_Memory_Gait_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/) 🟢 **Rev 2.0 only** | Intermediate | Saves H10 gait data at 1 kHz (96 B struct). If this is your first time, do Ex.10 first |

- Examples 10 and 34 had the same numbers as USB memory examples in earlier versions, but the current examples are new.
- Ex.10 does not receive sensor values from the H10. The `h10_connected` field of the struct holds only whether the H10 is connected (0 or 1).
- In Ex.10, button 2 inserts a marker while logging. In Ex.34, button 2 is marker 1 (walking starts) and button 3 is marker 2 (event).
- After logging, remove the USB memory stick, connect the PC, open a serial terminal and press button 3 in Ex.10 to print the last run's statistics. (not yet confirmed on the board) <!-- board-check -->
- While logging, neither example sends anything to the PC. Check the state with LEDs 1 · 2 · 3.
- Ex.10 uses the plain-terminal setting, so the PhAI Studio real-time view does not appear. For a real-time graph, see Ex.09.
- Start Ex.34 after you connect the H10 and the wearer puts it on. Before building, change the body information in `_SetupBodyData()` (weight · height · leg lengths) to the wearer's values. With the sample values, knee and thigh angles are inaccurate.
- Ex.34's default setting (`USE_H10_ASSIST 0`) only records. It sends no torque or control commands.
- In Ex.34, if the H10 values are not updating, the decoder builds the time base from the saved time (the time stored with every record). (not yet confirmed on the board) <!-- board-check -->

> ⚠️ **Safety**: If you change `USE_H10_ASSIST` to 1 in Ex.34, the H10's existing assist algorithm turns on and assist torque is applied to the wearer. Turn it on only after checking the safety of the wearer and the surroundings.

-----

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|------|------|------|
| `XM_IsUsbLogReady()` stays `false` (LED 1 in the examples does not turn on) | The stick is not FAT32 (exFAT and NTFS are not recognized), or it is still getting ready just after plugging in | Format it as FAT32 (32 KB allocation unit recommended) and plug it in again. Formatting erases all files |
| `XM_StartUsbDataLog()` returns `false` | The start conditions are not met | Look up the value of `XM_GetUsbLogLastStartError()` in the [start-failure reasons](ref/xm-api-usb-msc.en.md#xmusblogstarterror_e) table |
| Start returned `true`, but soon `XM_IsUsbLogSessionIdle()` is `true` again | Opening the file failed (for example, not enough free space `XM_USB_LOG_START_STORAGE`, or a folder with the same name and different settings `XM_USB_LOG_START_FOLDER_MISMATCH`) | Read `XM_GetUsbLogLastStartError()` the same way. Read it before you call `XM_StartUsbDataLog()` again |
| `XM_USB_LOG_START_TOO_BIG_FOR_RATE` | The struct is too big for the rate | Lower the logging rate or make the struct smaller |
| `XM_USB_LOG_START_METADATA_MISMATCH` | The meta line has a wrong format, an unknown type, or its field sizes do not add up to the struct size | Write types in lowercase, write unused bytes like `_pad(3bytes)`, and fix the struct size with `_Static_assert` |
| A setup function such as `XM_SetUsbLogSampleRateHz()` returns `false` | A logging session exists, you called it outside `Control_Setup()` / `Control_Loop()`, or the value is out of range | Call it from `Control_Setup()` / `Control_Loop()` when `XM_IsUsbLogSessionIdle()` is `true` |
| The folder name does not match the real date, or matches an earlier recording | The clock may not have been set | Set the clock with `XM_RTC_SetDateTime()` or choose the session name yourself |
| PhAI Studio / the `xm10` tool cannot find the board | A USB memory stick is plugged in | See [There Is Only One USB-C Port](#usb-c-port) |
| The decoder verdict is `RE-COLLECT` | Many samples are missing or the gaps are long | Fully format the stick as FAT32 (32 KB allocation unit) and record again. If it is still the same, use a different stick, lower the logging rate, or make the struct smaller |

-----

## Related Documents

- [`xm_api_usb_msc.h` reference](ref/xm-api-usb-msc.en.md) — exact descriptions of the functions, types and constants
- [Save Data to a USB Memory Stick](../getting-started/05-usb-memory-log.en.md) — step by step, from formatting to the decoder
- [05. USB Connectivity](05-usb-connectivity.en.md) — how to send data to a PC in real time
- [10. Struct Logging](10-struct-logging.en.md) — send a struct to the PC in real time
- [08. Real-Time Clock (RTC)](08-rtc-clock.en.md) — the clock used for folder names
- [Troubleshooting](../troubleshooting.en.md)
