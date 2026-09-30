# 04 — Receiving Data on Your PC — the `xm10` Tool

A tool for **plotting as a live graph, saving raw, and exporting to CSV** the data your board streams out over USB. It lives in the `Extension_Module/PythonDecoder/` folder — run it directly with Python, or bundle it into a single executable for a PC that doesn't have Python installed.

> **Where is it?** When you unzip the SDK, `Extension_Module\PythonDecoder\` is included. If your ZIP doesn't have that folder (an older release), grab just the `PythonDecoder/` folder from the GitHub repo and put it anywhere — it's the same files regardless of board revision.

> **How is this different from PhAI Studio?** PhAI Studio is the official tool you use straight from the browser. This is an open-source, local tool. It **saves the raw bytes before interpreting them**, so you can re-export later even if the channel layout changes, and if you know Python you can modify it however you like. The two tools **can't have the same COM port open at the same time** — only run one at a time.

---

## 5-Minute Demo — No Board Required

You can watch the whole flow once even without a board yet.

```bash
cd Extension_Module/PythonDecoder   # inside the unzipped SDK folder
pip install pyserial pyqt5 pyqtgraph numpy      # first time only
python xm10.py demo
```

This feeds data made to look like it came from a real board into the actual receive code, saves it, reads it back, exports it to CSV, and self-checks 17 items. You should see this at the end:

```
데모 통과 — 17/17 항목. 산출물:
   demo_out\demo.xmlog
   demo_out\csv\demo_total_0x20.csv
   demo_out\csv\demo_user_0xF0.csv
   ...
```
*(Real console output, in Korean: "Demo passed — 17/17 items. Artifacts:" followed by the file list.)*

Open `demo_out\csv\demo_user_0xF0.csv`. The first three columns (`pc_time_us` receive time, `seq_id` receive sequence number, `activation_id` the number of the struct description that was valid when the row was received — 0 means "the description hadn't arrived yet", as in the first few rows, which are still decoded with the description that arrived right after) are receive metadata added by the PC; after those come `state, contact, emg_rms, angle_x10, tick, torque[0], torque[1]` — the field names defined in the board-side C struct became the CSV columns, unchanged. That's exactly what this tool is for.

---

## Connecting a Board

### 1. Find the Port

```bash
python xm10.py ports
```

```
포트 3개:
  COM6     0483:5740  USB 직렬 장치(COM6)          <- XM10 후보
  COM11    -          표준 Bluetooth에서 직렬 링크(COM11)
  COM12    -          표준 Bluetooth에서 직렬 링크(COM12)
```
*(3 ports found. COM6 is flagged as the XM10 candidate; COM11/COM12 are Bluetooth virtual serial links.)*

If no XM10 candidate shows up, check the cable and the board's power. Seeing only Bluetooth virtual ports is common — those aren't the XM10.

### 2. View as a Graph

```bash
python xm10.py recv
```

A window opens. Pick your **Port** in the top row and click **Connect** — the board's user channels start plotting live. If you registered names with `XM_SetUsbCustomMeta`, as in Example 09, those names show up in the graph titles.

If you send user channels under several Module IDs (`0xF0`, `0xF1`, …), each Module ID gets its own tab. Every tab has its own graphs and its own saved file, and a tab you aren't looking at keeps saving.

| Button | What it does |
| :--- | :--- |
| **Refresh** | Re-read the port list — if you opened the window before plugging in the board, click this or it won't show up |
| **Connect / Disconnect** | Connect / disconnect |
| **Freeze** | Pause the graph only (saving keeps going) |
| **Output** | Folder where files are saved (default `data/`) |
| **Save .xmlog** | Save the received bytes as-is (on by default — see below) |
| **Screenshot** | Save the current view as a PNG |

When you disconnect, the status bar at the bottom shows how many frames were received and which file they were saved to.

### 3. Saved Files

Each time you connect, two kinds of files show up in the `Output` folder.

| File | What it is |
| :--- | :--- |
| `cdc_phai_<timestamp>_user_0xF0.csv` | The user channels plotted on screen, as a table (opens directly in Excel). One file per Module ID |
| `cdc_<timestamp>.xmlog` | **Every** frame received, raw bytes (explained below) |

Why save a second file when the CSV should be enough? The CSV is **already interpreted** at the moment it's received. If the channel names arrived after the data, or you changed the channel layout afterward, or there was a mistake in interpretation, that CSV is final — you're stuck with it. The `.xmlog` is laid down without any interpretation, so you can always re-export it later. It also includes the system channels that never get plotted on screen (the 0x20 Total Data, 197 channels).

### 4. Export CSV from `.xmlog`

```bash
python xm10.py export data/cdc_20260910_143000.xmlog              # summary of what happened
python xm10.py export data/cdc_20260910_143000.xmlog --csv out/   # one CSV per channel
```

```
CSV:
  cdc_..._total_0x20.csv      12000 rows   [generated-map] v2.8 ...
  cdc_..._user_0xF0.csv       12000 rows   [0xEF] JSON 메타 4채널  ⚠ 타입 미상 — float32 가정
```
*("JSON meta, 4 channels"; the warning reads "type unknown — assuming float32".)*

`total_0x20` is the 197 channels the board always sends (joint angles, torque, IMU, GRF, etc.), and `user_0xF0` is the channel you sent from your own example code. The channel names the board sent while you were recording are stored inside the file itself, so there's nothing extra to configure when exporting.

Each row is decoded with the **channel description that was valid when the row was received** (a row received before any description is decoded with the first one that arrives afterwards on the same connection). If the struct description for a Module ID changes partway through a single file (for example, a script that keeps writing into one file across a reconnect — the graph window opens a new file for every connection), the CSV for that Module ID is split per description — `..._user_0xF0.csv`, `..._user_0xF0_schema2.csv` — because the same column name can mean something different (the same four bytes are `1.0` as a `float` but `1065353216` as a `uint32_t`).

**If you connect again**, the graph window opens a new `.xmlog`. It can't tell whether the board that came back is the same one, so it doesn't carry the earlier file's channel description over — the new file only uses what the board sent on that connection. The current firmware sends the channel names your example code registered with `XM_SetUsbCustomMeta` once, the first time you connect after USB is freshly enumerated (unplugging and re-plugging the cable, or restarting the board, makes USB enumerate again). It doesn't send the types yet.

- A file recorded after re-plugging the cable or restarting the board contains the channel names.
- If you only clicked Disconnect and then Connect in the window, USB stays up and the board doesn't re-send the names. The graph and the live CSV (`cdc_phai_..._user_0xF0.csv`) keep the earlier names, but the `.xmlog` recorded that way has no channel names, so exporting it gives `ch0, ch1, …` with values assumed to be `float`. If you need the names, re-plug the cable and record again.

---

## Console-Only Mode

When you don't need the graph window, or you're leaving it running for a long time, console mode is lighter.

```bash
python xm10.py recv --cli --port COM6 --log                # saves CSV + .xmlog, Ctrl+C to stop
python xm10.py recv --cli --port COM6 --log --total-data   # also exports the 0x20 (197-channel) data
```

---

## Checking for Dropped Frames — `soak`

At 1 kHz, 30 minutes is 1.8 million frames. There's no way to eyeball whether even one of them got dropped, so there's a command that judges it for you automatically.

```bash
python xm10.py soak --port COM6 --minutes 30
```

When it finishes, it reports PASS / FAIL with the reason — frames the board dropped, corrupted frames, missing sequence numbers, frames per second, even whether the board restarted partway through. Run this once during initial setup, after swapping a cable, and after adding channels.

---

## Sending Your Own Data (Board Side)

`cdc_stream.c` in [Ex.09 CDC Stream](../../examples/09_CDC_Stream/) is the reference. You need three pieces — one struct, one line to register names, one line to send. The code below is taken from that file as-is.

```c
/* The struct holding the values to send — all float; field order = channel order */
typedef struct {
    float is_connected;
    float left_hip_angle;
    float right_hip_angle;
    float forward_velocity;
} UserDebugData_t;
static UserDebugData_t s_debug;

/* Control_Setup — register channel names (once). Entry order = struct field order */
XM_SetUsbCustomMeta(0xF0,
    "[{\"name\":\"H10 Connected\",\"unit\":\"bool\"},"
    "{\"name\":\"Left Hip Angle\",\"unit\":\"deg\"},"
    "{\"name\":\"Right Hip Angle\",\"unit\":\"deg\"},"
    "{\"name\":\"Forward Velocity\",\"unit\":\"m/s\"}]");

/* Control_Loop — fill in values and send (every tick) */
XM_SendUsbDataWithId(&s_debug, sizeof(s_debug), 0xF0);
```

Four rules to follow:

1. **The struct must be `float`-only** — the name-registration mechanism doesn't send types along with it, so the PC reads every field as a float. If a single `uint32_t` sneaks in, that column comes out as garbage numbers.
2. **JSON entry count = struct field count, in the same order** — even a mismatch of one shifts every column after it. The values still look like plausible numbers, so it's easy to miss.
3. **Module ID is `0xF0`–`0xFE`** — the rest are reserved for the system.
4. **Only one name registration per connection** — call `XM_SetUsbCustomMeta` twice and only the last one survives. If you have several channel groups, combine them under a single Module ID (Ex.41 merges six IMUs that way).

All 24 examples in this repo are automatically checked against rules 1–3.

> The "struct field names *and types* become CSV columns" behavior you saw in the demo (`state` as an integer, `contact` as a bool, and so on) is already built on the PC side; board-side firmware support for it is coming in a future release. Right now the board only sends **names**, as shown above.

---

## Building a Standalone Executable

To carry this to a PC without Python, bundle it into one file.

```bash
python build_exe.py            # dist/xm10.exe — includes the graph window (~55.6 MB)
python build_exe.py --no-gui   # console-only (~23.0 MB)
```

After building, it actually **runs** `demo` and the self-checks against that exe to confirm it works. The commands are the same as with Python — `xm10.exe demo`, `xm10.exe recv`, `xm10.exe export ...`.

---

## When Something Goes Wrong

| Symptom | What to check |
| :--- | :--- |
| XM10 doesn't show up in the port list | If the window was already open, click **Refresh**. Re-run `xm10.py ports`. Check the cable and power. See if a COM port appears in Device Manager |
| Registered two name sets but only one shows | Only one registration survives per connection — combine them under one Module ID |
| Connect fails | Make sure PhAI Studio or a serial terminal isn't already holding the same port |
| Graph titles show `ch0, ch1…` | Check that `XM_SetUsbCustomMeta` is called in `Control_Setup`, and that the entry count matches the struct |
| Values come out as garbage numbers | Check for a non-`float` member in the struct |
| Frames seem to be dropping | Judge it with `xm10.py soak`. Try sending fewer channels |
| Not sure the tool is installed correctly | `python xm10.py selftest` — self-check, no board needed |

---

## What's in the Folder

```
PythonDecoder/
├── xm10.py          ← Start here. demo / ports / recv / soak / export / selftest
├── build_exe.py     ← Build the standalone executable
├── run_tests.py     ← Development-time verification (a bit more than selftest)
├── README.md        ← Internal structure, file-by-file notes
└── CDC/             ← The actual implementation, for when you want to modify it directly
```

What each file under `CDC/` does is listed in the "파일 한눈에" (files at a glance) table in [PythonDecoder/README.md](../../PythonDecoder/README.md). Normally you only need `xm10.py`.

---

**Next:** move on to Ex.07–09 (USB communication) in [Tutorials](../tutorials/README.en.md).
