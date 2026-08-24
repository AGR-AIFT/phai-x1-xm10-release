# XM10 4-Channel CDC Live Monitor

`cdc_4ch_live_monitor.py` is a small PC-side monitor for XM10 USB-CDC data.
It receives PhAI V2.2 packets directly from the board, decodes the selected
User Custom module, plots four float channels in real time, and can record the
stream to CSV.

## Firmware Assumption

The firmware should send four `float` values through `XM_SendUsbDataWithId()`
using module ID `0xF0`.

For the current `pd_realtime_control.c` setup, the expected payload is:

```c
PF3, PF4, PF5, PF6
```

Each value is a voltage in volts.

The firmware packet path is:

```text
pf3_volt/pf4_volt/pf5_volt/pf6_volt
  -> s_stream_data
  -> XM_SendUsbDataWithId(..., 0xF0)
  -> PhAI packet builder
  -> COBS framed USB-CDC stream
```

## Protocol Decoding

The script decodes the same PhAI V2.2 wire format used by the firmware:

```text
[COBS_ENCODED_INTERNAL_PACKET] [0x00 delimiter]
```

After COBS decoding, the internal packet is:

```text
[SOF:0xAA]
[LEN:1 byte, payload size in 4-byte units]
[SEQ_ID:2 bytes, little-endian]
[MODULE_ID:1 byte]
[STATUS:1 byte]
[PAYLOAD: LEN * 4 bytes]
[CRC16:2 bytes, little-endian]
```

The script:

1. Reads bytes from the serial port with `pyserial`.
2. Splits frames by the `0x00` COBS delimiter.
3. COBS-decodes each frame.
4. Checks `SOF == 0xAA`.
5. Verifies CRC16-CCITT.
6. Filters packets by module ID, default `0xF0`.
7. Unpacks the payload as little-endian floats.
8. Uses the first four floats as the displayed channels.

## GUI Mode

GUI mode is the default.

```bash
cd /Users/gunhee/Documents/GitHub/Extension_Module/PythonDecoder/CDC
python3 cdc_4ch_live_monitor.py --port /dev/tty.usbmodemXXXX
```

Windows example:

```bash
python cdc_4ch_live_monitor.py --port COM6
```

The GUI uses:

```text
PyQt5      - window and controls
pyqtgraph - real-time plotting
pyserial  - USB-CDC serial access
```

Install dependencies:

```bash
python3 -m pip install pyserial pyqt5 pyqtgraph
```

## GUI Behavior

The serial reader runs in a `QThread` so the plot window stays responsive.
Every matched packet emits a sample:

```text
time_s, seq_id, (PF3, PF4, PF5, PF6), rate_hz, error_count
```

The plot uses a rolling time window. The default window is 10 seconds:

```bash
python3 cdc_4ch_live_monitor.py --port /dev/tty.usbmodemXXXX --window 20
```

Plot redraw rate is limited by `--plot-hz` so high-rate CDC data does not
overload the UI:

```bash
python3 cdc_4ch_live_monitor.py --port /dev/tty.usbmodemXXXX --plot-hz 60
```

## CSV Recording

Press the `Record` button in the GUI to start recording. A file-save dialog
opens. The default folder is:

```text
PythonDecoder/CDC/data/
```

Press `Stop` to finish and close the CSV file.

CSV columns:

```csv
time_s,seq_id,PF3,PF4,PF5,PF6
```

Notes:

- `time_s` is PC receive time relative to script start.
- `seq_id` is the firmware PhAI packet sequence ID.
- Channel values are written with 8 decimal places.
- The CSV is flushed every 200 recorded rows and when recording stops.

## CLI Mode

CLI mode is useful for quickly checking communication without opening a GUI:

```bash
python3 cdc_4ch_live_monitor.py --cli --port /dev/tty.usbmodemXXXX
```

Change terminal update rate:

```bash
python3 cdc_4ch_live_monitor.py --cli --port /dev/tty.usbmodemXXXX --print-hz 10
```

## Useful Options

List available ports:

```bash
python3 cdc_4ch_live_monitor.py --list-ports
```

Use a different module ID:

```bash
python3 cdc_4ch_live_monitor.py --port COM6 --module 0xF1
```

Rename channels:

```bash
python3 cdc_4ch_live_monitor.py --port COM6 --labels RH,LH,AUX1,AUX2
```

## Troubleshooting

If no values appear:

1. Check that the correct serial port is selected.
2. Confirm the firmware is sending module `0xF0`.
3. Confirm the payload has at least four `float` values.
4. Make sure the board is in the state where `_UpdateStreamData()` is called.

For the current `pd_realtime_control.c`, ADC sampling and USB custom streaming
run inside the active control loop, so values may not update while the firmware
is idle or not in the expected active state.
