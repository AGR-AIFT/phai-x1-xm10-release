# XM10 CDC Selective Logger

`cdc_selective_logger.py` is a lightweight GUI for receiving XM10 USB-CDC
PhAI V2.2 packets and saving only the channels selected by the user.

## Run

```bash
cd Extension_Module/PythonDecoder/CDC
python3 cdc_selective_logger.py
```

Only `pyserial` is required. `tkinter` is included with standard Python on
Windows and most Python distributions.

```bash
python3 -m pip install pyserial
```

## Workflow

1. Open the GUI.
2. Select the USB serial port, such as `COM6`, from the `Port` dropdown.
3. Click `Connect`.
4. Select one of the detected CDC modules.
5. Select a channel-name preset, or keep the default `ch0`, `ch1`, ... names.
6. Check the channels to save and edit their names if needed.
7. Click `Start recording`, choose a CSV path, and record the stream.

The current `pd_realtime_control.c` firmware sends module `0xF0` with four
float channels in this order:

```text
PF3, PF4, PF5, PF6
```

## Why Channel Names Are Editable

The normal PhAI V2.2 data packet includes `module_id`, payload length, status,
and float values. It does not include C variable names. Current XM firmware can
also send a one-time `0xEF` JSON metadata packet registered by
`XM_SetUsbCustomMeta()`, but the GUI keeps the default `ch0`, `ch1`, ... names
until the user explicitly selects a preset.

Available presets:

```text
Generic ch0...          -> raw channel order, no FW-specific names
Final_FSR_Fuzzy_Logic   -> Final_FSR_Fuzzy_Logic.c CDC_STREAM_CHANNELS
Final_EMG               -> Final_EMG.c XM_SetUsbCustomMeta / EmgStreamData_t
Final_Encoder_ex01      -> Final_Encoder_ex01.c CDC_STREAM_CHANNELS
```

Confirm that the selected preset matches the firmware flashed on the board.
The GUI rejects a preset when its channel count differs from the received
packet.

Preset channel orders:

```text
Final_FSR_Fuzzy_Logic:
PF3 V, PF4 V, PF5 V, PF6 V,
LT Load, LH Load, RT Load, RH Load,
L Gait, R Gait, L Torque, R Torque,
Cal Ready, Control Req, Assist On, H10 Assist

Final_EMG:
Raw RH, Raw LH, Env RH, Env LH, Tau RH, Tau LH

Final_Encoder_ex01:
L Encoder, R Encoder, L Angle, R Angle,
L Pulse, R Pulse, L Torque, R Torque,
Threshold, Control Req, Assist On, H10 Assist,
L Current, R Current
```

Metadata is sent once when the firmware observes a USB connection. If the
board was already connected and streaming before the logger opened the serial
port, the logger may miss that one-time packet. Reconnect the USB cable or
reset the board to receive metadata again.

## CSV Format

The CSV contains packet metadata followed by the checked channels:

```csv
pc_time_s,seq_id,module_id,status,tx_drops,PF3,PF5
0.001234,10,0xF0,0x00,0,0.12000000,0.34000000
```
