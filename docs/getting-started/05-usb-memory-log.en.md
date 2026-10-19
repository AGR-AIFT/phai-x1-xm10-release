# 05 — Save Data to a USB Memory Stick

Follow along from the very beginning to **save the values your board makes onto a USB memory stick and decode them to CSV on a PC**. You do not need a PC while saving. Doing [Ex.10](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/) once shows you the whole flow.

> 🟢 **Rev 2.0 only** — This works only with the Rev 2.0 board and the Rev 2.0 SDK. Rev 1.1 (v2.8.1) has no USB memory logging. On Rev 1.1, receive your data with the `xm10` tool in [04. Receiving Data on Your PC](04-pc-data-tool.en.md).

> ⚠️ **There is only one USB-C port.** While a USB memory stick is plugged in, you cannot use PhAI Studio, the `xm10` tool, or firmware upload over USB. When logging is done, remove the stick and connect the PC. (not yet confirmed on the board) <!-- board-check -->

> ⚠️ The whole USB memory logging flow (save · stop · remove) and Ex.10 · Ex.34 have not been tested on the board yet. (not yet confirmed on the board) <!-- board-check -->

| What you want | How |
| :--- | :--- |
| Watch and save in real time with the PC connected | the `xm10` tool ([04. Receiving Data on Your PC](04-pc-data-tool.en.md)) |
| Save to a USB memory stick without a PC and decode later on a PC | This document |

---

## What You Need

- **A Rev 2.0 board and the KIT H10 body.** The board gets its power from the H10 body, so keep the H10 body powered on ([01 Hardware Setup](01-hardware-setup.en.md)).
- **1 USB memory stick.** Its files will be erased, so use one you can empty.
- **A development environment and the Rev 2.0 SDK.** You have finished [02](02-software-setup.en.md) and [03](03-first-build.en.md).
- **A Windows PC to install the decoder on.**

---

## 1. Format the USB Memory Stick

The stick must be **FAT32**. exFAT and NTFS are not recognized.

1. Plug the stick into your PC. Move any files you need first. Formatting erases everything.
2. In File Explorer, right-click the stick and choose **Format**.
3. Change **File system** to `FAT32`. The default is sometimes exFAT.
4. Change **Allocation unit size** to `32 kilobytes`. 32 KB is recommended.
5. Click **Start**.

> Windows' built-in format window cannot choose FAT32 for sticks larger than 32 GB. Use a stick of 32 GB or less.

---

## 2. Put the Example on the Board

1. Copy the entire contents of `Examples/10_USB_Memory_Log/usb_memory_log.c` in the SDK and paste it into `XM_Apps/Control_Task/control_task.c`. This is the same as option A under "Apply an example" in [03. First Build](03-first-build.en.md).
2. Decide the folder name. If you have set the clock (RTC), leave `LOG_SESSION_NAME` as `""`. The name is made from the clock as `YYMMDD_HHMMSS`.
3. If you have not set the clock, choose a name yourself, such as `LOG_SESSION_NAME` = `"walk01"`. Saving again with the same name continues in the same folder, so use a different name every time you save.
4. Build and upload to the board. This is the same as steps 3 and 4 of [03. First Build](03-first-build.en.md).

Set the clock with `XM_RTC_SetDateTime()` ([08. Real-Time Clock](../api-reference/08-rtc-clock.en.md)).

---

## 3. Save

1. Plug the USB memory stick into the board's USB-C port. When **LED 1 turns on**, you can save.
2. Press **button 1** briefly. Logging starts.
3. While the file is opening, LED 2 blinks fast. Then, when **LED 2 blinks slowly**, it is logging.
4. While logging, press **button 2** to insert a marker. The pressed state of buttons 1 · 2 · 3 is also saved as values.
5. After a few seconds, press **button 1** again. Logging stops. If you stop right after starting, an empty folder with no records can be left behind.
6. While LED 2 blinks fast, the files are being closed. **Do not remove the stick — wait.**
7. When LED 2 stays on, logging is done. You may now remove the USB memory stick. (not yet confirmed on the board) <!-- board-check -->

| Button | What it does |
| :--- | :--- |
| Button 1 | Start / stop logging (short press) |
| Button 2 | Insert a marker while logging |
| Button 3 | Send the last run's statistics to the PC (step 4 below) |

| LED | Meaning |
| :--- | :--- |
| LED 1 on | The USB memory stick is ready |
| LED 2 blinking slowly | Logging |
| LED 2 blinking fast | A file is being opened or closed. Do not remove the stick |
| LED 2 on | Logging is done |
| LED 3 on | An error. Logging could not start, or it ended by itself. Press button 1 again to turn it off |
| LED 3 short flash | A marker could not be inserted. Logging continues |

While logging, the stick is plugged in, so you cannot connect to a PC. Check the state with the LEDs. Do not remove the stick while logging.

---

## 4. View the Statistics (Optional)

After logging, you can remove the stick, connect the PC and view the last run's statistics. (not yet confirmed on the board) <!-- board-check -->

1. Remove the USB memory stick.
2. Connect the PC and board with a USB-C cable, and open the board's COM port in a serial terminal (PuTTY, Tera Term, etc.). Connect only one PC program at a time.
3. Press **button 3**. One line appears in the terminal. (not yet confirmed on the board) <!-- board-check -->

```
[USB Log] records=<n> bytes=<n> duration_ms=<n> empty_slots=<n> longest_gap_ms=<n> overflow_records=<n> write_errors=<n> buffer_peak_percent=<n> start_error=<n>
```

| Item | What it tells you |
| :--- | :--- |
| `empty_slots` | Number of missing samples. 0 means none are missing |
| `longest_gap_ms` | The longest gap [ms] |
| `overflow_records` | Number of records that could not be saved because the internal buffer overflowed. 1 or more means that logging run stopped at that point |
| `write_errors` | Number of failed writes to the USB memory stick |
| `start_error` | The result of the last start. 0 means it started |

This example uses the plain-terminal setting, so the PhAI Studio real-time view does not appear. For a real-time graph, see [Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/).

---

## 5. Decode on the PC — XM10 Log Decoder

Decode the saved folder with **XM10 Log Decoder**. It is an installer-based Windows program, and Python is not required.

This document is based on XM10 Log Decoder 2.2.0 or later.

### Get the Decoder

1. From the files attached to the release page (Assets), download the installer (the `.exe` whose name starts with `XM10LogDecoder_Setup_`).
2. If your browser blocks the `.exe` download, download the `.zip` with the same name and extract it.
3. To check that the file is correct, compare it with the attached `SHA256SUMS.txt`. Run `Get-FileHash <downloaded file>` in PowerShell to get the value.
4. Run the installer. The first time, Windows may show an "unknown publisher" warning. Click **More info** → **Run anyway**.
5. When the installation finishes, start **XM10 Log Decoder** from the Start menu.

### Decode

1. Copy the stick's `LOGS` folder to the PC. You can also decode with the stick plugged into the PC.
2. In the decoder, click `USB 탐지` (detect USB) to find the `LOGS` folder on the stick plugged into the PC. For a folder you copied to the PC, choose it with `폴더 선택` (select folder).
3. Pick the session to decode in the session list, then click `전체 디코딩` (decode all) or `선택 디코딩` (decode selected).
4. The results are created in the session folder. You can also choose a separate output folder.

### Result Files

These are the files XM10 Log Decoder 2.2.0 or later creates.

| File | Contents |
| :--- | :--- |
| `decoded_output.csv` | A table decoded from data saved at 1000 Hz |
| `decoded_output_<rate>hz.csv` | A table decoded from data saved at a rate other than 1000 Hz. Example: `decoded_output_100hz.csv` for 100 Hz |
| `events.csv` | A table of the markers, if you inserted any |
| `data_quality_report.txt` | The logging quality report — the verdict and the reasons |

- Missing samples are filled in by interpolation and marked `1` in the `row_origin` column. Measured values are `0`. Interpolated values are not measured values.
- Ex.10 logs at 100 Hz, so `loop_count` normally increases by 10 each row. (not yet confirmed on the board) <!-- board-check -->

### Reading the Verdict

Check the verdict in `data_quality_report.txt`. The decoder's log window also shows one line per session with the row count, the missing samples and the verdict.

The table below is based on the report of XM10 Log Decoder 2.2.0 or later.

| Verdict | Meaning | Accepted? |
| :--- | :--- | :---: |
| `GOOD` | No samples are missing | Yes |
| `USABLE` | Only small gaps (the longest gap is 4 ms or less). Accepted | Yes |
| `WARNING` | Gaps were filled by interpolation. Accepted. Up to a longest gap of 20 ms at 1000 Hz and 10 ms (one slot) at 100 Hz | Yes |
| `RE-COLLECT` | We recommend recording again. Many samples are missing or the gaps are long | No |
| `INVALID` | There is a problem with the logging settings or the file | No |
| `UNVERIFIED` | The missing samples cannot be counted, so no verdict is possible | No |

- The allowed gaps for logging rates other than 1000 Hz and 100 Hz are calculated values that have not been checked against real board logs. (not yet confirmed on the board) <!-- board-check -->
- Do not compare the verdicts of logs saved at different rates as they are.
- If the verdict is `RE-COLLECT`, fully format the stick as FAT32 (32 KB allocation unit) and record again, or try another stick. You can also lower the logging rate or make the struct smaller.
- If `RE-COLLECT` keeps coming up, please contact the Angel Robotics(엔젤로보틱스) CS team.

---

## 6. Next Steps

- **Save your own values** — change `LogRecord_t` and `LOG_METADATA` together in Ex.10. If you change the fields you must change the meta line too, and logging does not start if the sizes do not add up. The rules are in [06. USB Memory Logging](../api-reference/06-usb-data-logging.en.md#meta-line).
- **Save H10 gait data at 1 kHz** — that is [Ex.34](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/). It saves one struct (96 B) every 1 ms. If this is your first time, do Ex.10 first.
- Start Ex.34 with button 1 after you connect the H10 and the wearer puts it on. Button 2 is marker 1 (walking starts) and button 3 is marker 2 (event).
- Before building Ex.34, change the body information in `_SetupBodyData()` (weight · height · leg lengths) to the wearer's values. With the sample values, knee and thigh angles are inaccurate.
- Ex.34's default setting (`USE_H10_ASSIST 0`) only records. It sends no torque or control commands.
- When you log at 1000 Hz, some samples may be missing. The decoder tells you where and how long.
- In Ex.34, if the H10 values are not updating, the decoder builds the time base from the saved time (the time stored with every record). (not yet confirmed on the board) <!-- board-check -->

> ⚠️ **Safety**: If you change `USE_H10_ASSIST` to 1 in Ex.34, the H10's existing assist algorithm turns on and assist torque is applied to the wearer. Turn it on only after checking the safety of the wearer and the surroundings.

---

## When Something Seems Wrong

| Symptom | What to check |
| :--- | :--- |
| LED 1 does not turn on | Check that the stick is FAT32. exFAT and NTFS are not recognized. It can take a moment right after plugging in, so try plugging it in again |
| LED 3 turned on after pressing button 1 | Logging did not start. In the debugger (Live Expressions), look at the value of `g_last_start_error` and find it in the [start-failure reasons](../api-reference/ref/xm-api-usb-msc.en.md#xmusblogstarterror_e) table |
| LED 3 turned on while logging | Logging ended by itself. Examples are the stick being pulled out or the internal buffer overflowing |
| The folder name is not a date, or matches an earlier recording | The clock may not have been set. Set it with `XM_RTC_SetDateTime()` or choose `LOG_SESSION_NAME` yourself |
| PhAI Studio / the `xm10` tool cannot find the board | With a USB memory stick plugged in, the board does not connect to a PC. See "There is only one USB-C port" above |
| The decoder verdict is `RE-COLLECT` | See "Reading the Verdict" above |

For more fixes, see [Troubleshooting](../troubleshooting.en.md).

---

**Next:** see the functions and rules in detail in [06. USB Memory Logging](../api-reference/06-usb-data-logging.en.md), or move on to [Tutorials](../tutorials/README.en.md).
