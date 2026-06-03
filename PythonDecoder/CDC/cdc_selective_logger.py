"""
XM10 USB-CDC Selective CSV Logger
=================================

This script receives PhAI V2.2 float data from an XM10 board over USB-CDC and
saves only the channels checked by the user to CSV.

How to run
----------
Run this script from the folder that contains it.

    cd "/Users/gunhee/Downloads/HAR 2026/Extension_Module/PythonDecoder/CDC"
    python3 cdc_selective_logger.py

Required package:

    python3 -m pip install pyserial

`tkinter` is included in most standard Python installations.

GUI workflow
------------
1. Connect the board over USB.
2. Select the COM port or `/dev/tty.*` port from the `Port` dropdown.
   The tool does not auto-connect, so the user must choose the port.
3. Click `Connect`.
4. Select the received module when it appears under `Detected module`.
   Most student FW examples send custom data as module `0xF0`.
5. The default channel names are always `ch0`, `ch1`, ...
6. Select the `Channel name preset` that matches the flashed FW.
7. Check only the channels to save. The `Name` cells remain editable.
8. Click `Start recording` and choose the CSV file path.
9. Click `Stop recording` when the experiment is finished.

FW presets
----------
Presets only rename channels on the PC side. The actual data order must match
the struct passed to `XM_SendUsbDataWithId()` in the FW.

Supported presets:

* `Generic ch0...`
  - Works with any received packet
  - Displays channels as `ch0`, `ch1`, ...

* `Final_FSR_Fuzzy_Logic`
  - FW file:
    `Extension_Module/XM_Apps/User_Algorithm/Final_FSR_Fuzzy_Logic.c`
  - Module ID: `0xF0`
  - Channel count: 16 floats
  - Order:
    `PF3 V`, `PF4 V`, `PF5 V`, `PF6 V`,
    `LT Load`, `LH Load`, `RT Load`, `RH Load`,
    `L Gait`, `R Gait`, `L Torque`, `R Torque`,
    `Cal Ready`, `Control Req`, `Assist On`, `H10 Assist`

* `Final_EMG`
  - FW file:
    `Extension_Module/XM_Apps/User_Algorithm/Final_EMG.c`
  - Module ID: `0xF0`
  - Channel count: 6 floats
  - Order:
    `Raw RH`, `Raw LH`, `Env RH`, `Env LH`, `Tau RH`, `Tau LH`

* `Final_Encoder_ex01`
  - FW file:
    `Extension_Module/XM_Apps/User_Algorithm/Final_Encoder_ex01.c`
  - Module ID: `0xF0`
  - Channel count: 14 floats
  - Order:
    `L Encoder`, `R Encoder`, `L Angle`, `R Angle`,
    `L Pulse`, `R Pulse`, `L Torque`, `R Torque`,
    `Threshold`, `Control Req`, `Assist On`, `H10 Assist`,
    `L Current`, `R Current`

Why the default is ch0, ch1
---------------------------
Normal PhAI V2.2 data packets do not contain variable names. They contain only
`module_id`, `status`, `seq_id`, and a float array. The meaning of each float
must therefore be checked against the FW stream struct or `CDC_STREAM_CHANNELS()`
definition.

Some FW examples can send a `0xEF` metadata packet via `XM_SetUsbCustomMeta()`,
but this tool keeps the default display as `ch0`, `ch1`, ... so the user
explicitly chooses the FW preset.

CSV format
----------
The CSV contains metadata columns followed by the checked channels.

    pc_time_s,seq_id,module_id,status,tx_drops,<selected channels...>

Example:

    pc_time_s,seq_id,module_id,status,tx_drops,PF3 V,LT Load,L Gait
    0.012345,100,0xF0,0x00,0,1.23456789,0.45678901,2.00000000

Important
---------
If a FW CDC stream layout changes, update `CHANNEL_PRESETS` in this file with
the same order. If the preset channel count differs from the received packet
count, the GUI warns the user and returns to `Generic ch0...`.
"""

from __future__ import annotations

import csv
from dataclasses import dataclass
from datetime import datetime
import json
import os
import queue
import struct
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import serial
import serial.tools.list_ports

from cdc_4ch_live_monitor import (
    DEFAULT_BAUD,
    PHAI_CRC_SIZE,
    PHAI_HEADER_SIZE,
    PHAI_SOF,
    cobs_decode,
    crc16_ccitt,
)


POLL_MS = 50
SERIAL_TIMEOUT_S = 0.02
QUEUE_LIMIT = 50000
STREAM_START_COMMAND = b"AGRB MON START\r\n"
USER_META_MODULE_ID = 0xEF

# The CDC data packet only contains float values. These presets are therefore
# PC-side labels that must stay in the exact same order as each FW stream struct.
# Keep "Generic ch0..." as the default so students can see the raw channel order
# before applying a FW-specific name preset.
CHANNEL_PRESETS = {
    "Generic ch0...": None,
    # Matches:
    # Extension_Module/XM_Apps/User_Algorithm/Final_FSR_Fuzzy_Logic.c
    # CDC_STREAM_CHANNELS(...)
    "Final_FSR_Fuzzy_Logic": [
        "PF3 V", "PF4 V", "PF5 V", "PF6 V",
        "LT Load", "LH Load", "RT Load", "RH Load",
        "L Gait", "R Gait", "L Torque", "R Torque",
        "Cal Ready", "Control Req", "Assist On", "H10 Assist",
    ],
    # Matches:
    # Extension_Module/XM_Apps/User_Algorithm/Final_EMG.c
    # XM_SetUsbCustomMeta(0xF0, ...)
    "Final_EMG": [
        "Raw RH", "Raw LH", "Env RH", "Env LH", "Tau RH", "Tau LH",
    ],
    # Matches:
    # Extension_Module/XM_Apps/User_Algorithm/Final_Encoder_ex01.c
    # CDC_STREAM_CHANNELS(...)
    "Final_Encoder_ex01": [
        "L Encoder", "R Encoder", "L Angle", "R Angle",
        "L Pulse", "R Pulse", "L Torque", "R Torque",
        "Threshold", "Control Req", "Assist On", "H10 Assist",
        "L Current", "R Current",
    ],
}


@dataclass(frozen=True)
class Packet:
    recv_t: float
    seq_id: int
    module_id: int
    status: int
    values: tuple[float, ...]


@dataclass(frozen=True)
class Metadata:
    target_module_id: int
    labels: tuple[str, ...]


def parse_phai_payload_frame(frame: bytes) -> tuple[int, int, int, bytes] | None:
    """Decode the PhAI/COBS envelope and return the raw payload bytes.

    Normal user streams reinterpret this payload as float32 values. The 0xEF
    metadata stream uses the same envelope but stores JSON bytes instead.
    """
    decoded = cobs_decode(frame)
    if len(decoded) < PHAI_HEADER_SIZE + PHAI_CRC_SIZE or decoded[0] != PHAI_SOF:
        return None
    len_units = decoded[1]
    total_size = PHAI_HEADER_SIZE + len_units * 4 + PHAI_CRC_SIZE
    if len_units == 0 or len(decoded) < total_size:
        return None
    payload_end = total_size - PHAI_CRC_SIZE
    crc_rx = decoded[payload_end] | (decoded[payload_end + 1] << 8)
    if crc_rx != crc16_ccitt(decoded[:payload_end]):
        return None
    seq_id = decoded[2] | (decoded[3] << 8)
    return seq_id, decoded[4], decoded[5], decoded[PHAI_HEADER_SIZE:payload_end]


def parse_metadata(payload: bytes) -> Metadata:
    """Parse optional 0xEF JSON metadata from FW.

    The GUI intentionally does not auto-apply these names. Students choose the
    preset explicitly so the displayed labels are tied to the FW file they use.
    """
    if not payload:
        raise ValueError("empty metadata payload")
    target_module_id = payload[0]
    items = json.loads(payload[1:].rstrip(b"\x00").decode("utf-8"))
    labels = tuple(str(item["name"]) for item in items)
    if not labels:
        raise ValueError("metadata has no channels")
    return Metadata(target_module_id, labels)


def default_labels(module_id: int, count: int) -> list[str]:
    """Return raw channel names when no preset is selected."""
    return [f"ch{i}" for i in range(count)]


def module_text(module_id: int, count: int) -> str:
    return f"0x{module_id:02X} ({count} float channels)"


class SerialReader(threading.Thread):
    def __init__(self, port: str, baud: int, packets: queue.Queue, messages: queue.Queue):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.packets = packets
        self.messages = messages
        self.stop_event = threading.Event()
        self.decode_errors = 0
        self.queue_drops = 0
        self.good_packets = 0

    def stop(self):
        self.stop_event.set()

    def _message(self, text: str):
        self.messages.put(text)

    def run(self):
        ser = None
        try:
            ser = serial.Serial(self.port, self.baud, timeout=SERIAL_TIMEOUT_S)
            ser.dtr = True
            ser.rts = True
            ser.reset_input_buffer()
            ser.write(STREAM_START_COMMAND)
            self._message(f"connected: {self.port} @ {self.baud}")
            wire_buf = bytearray()
            started = time.perf_counter()
            last_keepalive = started

            while not self.stop_event.is_set():
                chunk = ser.read(4096)
                now = time.perf_counter()
                if not chunk:
                    if now - last_keepalive >= 1.0:
                        ser.write(STREAM_START_COMMAND)
                        last_keepalive = now
                    continue

                wire_buf.extend(chunk)
                while True:
                    delimiter = wire_buf.find(b"\x00")
                    if delimiter < 0:
                        break
                    raw_frame = bytes(wire_buf[:delimiter])
                    del wire_buf[:delimiter + 1]
                    if not raw_frame:
                        continue
                    try:
                        parsed = parse_phai_payload_frame(raw_frame)
                    except (ValueError, IndexError):
                        parsed = None
                    if parsed is None:
                        self.decode_errors += 1
                        continue

                    seq_id, module_id, status, payload = parsed
                    if module_id == USER_META_MODULE_ID:
                        # FW may send label metadata once after USB connection.
                        # Store it for status/debug only; do not rename channels
                        # until the user selects one of the visible presets.
                        try:
                            self.packets.put_nowait(parse_metadata(payload))
                        except (UnicodeDecodeError, ValueError, KeyError, TypeError, queue.Full):
                            self.decode_errors += 1
                        continue
                    values = struct.unpack(f"<{len(payload) // 4}f", payload)
                    packet = Packet(
                        recv_t=now - started,
                        seq_id=seq_id,
                        module_id=module_id,
                        status=status,
                        values=tuple(values),
                    )
                    self.good_packets += 1
                    try:
                        self.packets.put_nowait(packet)
                    except queue.Full:
                        self.queue_drops += 1
        except (OSError, serial.SerialException) as exc:
            self._message(f"serial error: {exc}")
        finally:
            if ser is not None and ser.is_open:
                ser.close()
            self._message("disconnected")


class SelectiveLoggerApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("XM10 CDC Selective Logger")
        self.root.geometry("850x650")
        self.root.minsize(720, 500)

        self.reader: SerialReader | None = None
        self.packet_queue: queue.Queue = queue.Queue(maxsize=QUEUE_LIMIT)
        self.message_queue: queue.Queue = queue.Queue()
        self.port_by_label: dict[str, str] = {}
        self.modules: dict[int, int] = {}
        self.metadata_labels: dict[int, list[str]] = {}
        self.latest_values: dict[int, tuple[float, ...]] = {}
        self.channel_rows: list[tuple[tk.BooleanVar, tk.StringVar, ttk.Label]] = []
        self.active_module_id: int | None = None
        self.preset_var = tk.StringVar(value="Generic ch0...")

        self.record_file = None
        self.record_writer = None
        self.record_path: str | None = None
        self.record_module_id: int | None = None
        self.record_indices: list[int] = []
        self.record_count = 0
        self.received_count = 0

        self._build_ui()
        self.refresh_ports()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(POLL_MS, self.poll)

    def _build_ui(self):
        connection = ttk.LabelFrame(self.root, text="1. Serial connection", padding=8)
        connection.pack(fill="x", padx=10, pady=(10, 5))

        ttk.Label(connection, text="Port").grid(row=0, column=0, padx=4, pady=4)
        self.port_combo = ttk.Combobox(connection, state="readonly", width=42)
        self.port_combo.grid(row=0, column=1, padx=4, pady=4, sticky="ew")
        ttk.Button(connection, text="Refresh", command=self.refresh_ports).grid(
            row=0, column=2, padx=4, pady=4
        )

        ttk.Label(connection, text="Baud").grid(row=0, column=3, padx=4, pady=4)
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        ttk.Entry(connection, textvariable=self.baud_var, width=10).grid(
            row=0, column=4, padx=4, pady=4
        )
        self.connect_button = ttk.Button(connection, text="Connect", command=self.connect)
        self.connect_button.grid(row=0, column=5, padx=4, pady=4)
        self.disconnect_button = ttk.Button(
            connection, text="Disconnect", command=self.disconnect, state="disabled"
        )
        self.disconnect_button.grid(row=0, column=6, padx=4, pady=4)
        connection.columnconfigure(1, weight=1)

        channels = ttk.LabelFrame(self.root, text="2. Detected module and channels", padding=8)
        channels.pack(fill="both", expand=True, padx=10, pady=5)

        top = ttk.Frame(channels)
        top.pack(fill="x")
        ttk.Label(top, text="Module").pack(side="left", padx=(0, 5))
        self.module_combo = ttk.Combobox(top, state="readonly", width=30)
        self.module_combo.pack(side="left")
        self.module_combo.bind("<<ComboboxSelected>>", self.on_module_selected)
        ttk.Button(top, text="All", command=lambda: self.select_all(True)).pack(
            side="left", padx=(12, 3)
        )
        ttk.Button(top, text="None", command=lambda: self.select_all(False)).pack(side="left")

        presets = ttk.LabelFrame(channels, text="Channel name preset", padding=5)
        presets.pack(fill="x", pady=(7, 3))
        for preset_name in CHANNEL_PRESETS:
            ttk.Radiobutton(
                presets,
                text=preset_name,
                variable=self.preset_var,
                value=preset_name,
                command=self.apply_selected_preset,
            ).pack(side="left", padx=5)

        ttk.Label(
            channels,
            text=(
                "Default names are ch0, ch1, ... Select the preset that matches "
                "the flashed FW, or edit the Name cells manually."
            ),
        ).pack(fill="x", pady=(7, 5))

        headings = ttk.Frame(channels)
        headings.pack(fill="x")
        ttk.Label(headings, text="Save", width=7).grid(row=0, column=0)
        ttk.Label(headings, text="Index", width=8).grid(row=0, column=1)
        ttk.Label(headings, text="Name").grid(row=0, column=2, sticky="w")
        ttk.Label(headings, text="Latest value", width=18).grid(row=0, column=3)
        headings.columnconfigure(2, weight=1)

        self.channel_canvas = tk.Canvas(channels, highlightthickness=0)
        scrollbar = ttk.Scrollbar(channels, orient="vertical", command=self.channel_canvas.yview)
        self.channel_frame = ttk.Frame(self.channel_canvas)
        self.channel_frame.bind(
            "<Configure>",
            lambda _event: self.channel_canvas.configure(
                scrollregion=self.channel_canvas.bbox("all")
            ),
        )
        self.channel_canvas.create_window((0, 0), window=self.channel_frame, anchor="nw")
        self.channel_canvas.configure(yscrollcommand=scrollbar.set)
        self.channel_canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        recording = ttk.LabelFrame(self.root, text="3. CSV recording", padding=8)
        recording.pack(fill="x", padx=10, pady=5)
        self.record_button = ttk.Button(recording, text="Start recording", command=self.toggle_record)
        self.record_button.pack(side="left")
        self.record_label = ttk.Label(recording, text="not recording")
        self.record_label.pack(side="left", padx=10)

        self.status_var = tk.StringVar(value="Select a port, then click Connect.")
        ttk.Label(self.root, textvariable=self.status_var, relief="sunken", anchor="w").pack(
            fill="x", padx=10, pady=(5, 10)
        )

    def refresh_ports(self):
        previous = self.port_combo.get()
        self.port_by_label.clear()
        labels = []
        for port in serial.tools.list_ports.comports():
            label = f"{port.device} | {port.description}"
            labels.append(label)
            self.port_by_label[label] = port.device
        self.port_combo["values"] = labels
        if previous in labels:
            self.port_combo.set(previous)
        elif labels:
            self.port_combo.current(0)
        else:
            self.port_combo.set("")
            self.status_var.set("No serial ports found. Connect the USB device and click Refresh.")

    def connect(self):
        selected = self.port_combo.get()
        port = self.port_by_label.get(selected)
        if not port:
            messagebox.showwarning("Port required", "Select a serial port first.")
            return
        try:
            baud = int(self.baud_var.get())
        except ValueError:
            messagebox.showerror("Invalid baud", "Baud must be an integer.")
            return

        self.disconnect()
        self.packet_queue = queue.Queue(maxsize=QUEUE_LIMIT)
        self.message_queue = queue.Queue()
        self.modules.clear()
        self.metadata_labels.clear()
        self.latest_values.clear()
        self.module_combo["values"] = []
        self.module_combo.set("")
        self.active_module_id = None
        self.preset_var.set("Generic ch0...")
        self._rebuild_channel_rows(None)
        self.received_count = 0

        self.reader = SerialReader(port, baud, self.packet_queue, self.message_queue)
        self.reader.start()
        self.connect_button.configure(state="disabled")
        self.disconnect_button.configure(state="normal")
        self.status_var.set(f"connecting: {port} @ {baud}")

    def disconnect(self):
        self.stop_recording()
        if self.reader is not None:
            self.reader.stop()
            self.reader = None
        self.connect_button.configure(state="normal")
        self.disconnect_button.configure(state="disabled")

    def _module_labels(self) -> list[str]:
        return [module_text(module_id, self.modules[module_id]) for module_id in sorted(self.modules)]

    def _module_id_from_combo(self) -> int | None:
        text = self.module_combo.get()
        if not text:
            return None
        try:
            return int(text.split()[0], 16)
        except ValueError:
            return None

    def on_module_selected(self, _event=None):
        module_id = self._module_id_from_combo()
        if module_id is not None and module_id != self.active_module_id:
            self.active_module_id = module_id
            self._reset_incompatible_preset(self.modules[module_id])
            self._rebuild_channel_rows(module_id)

    def _rebuild_channel_rows(self, module_id: int | None):
        for widget in self.channel_frame.winfo_children():
            widget.destroy()
        self.channel_rows.clear()
        if module_id is None:
            return

        count = self.modules[module_id]
        values = self.latest_values.get(module_id, ())
        names = self._selected_preset_labels(count)
        for index, name in enumerate(names):
            enabled = tk.BooleanVar(value=True)
            label = tk.StringVar(value=name)
            latest = ttk.Label(self.channel_frame, text="-", width=18, anchor="e")
            ttk.Checkbutton(self.channel_frame, variable=enabled).grid(
                row=index, column=0, padx=5, pady=2
            )
            ttk.Label(self.channel_frame, text=str(index), width=8).grid(
                row=index, column=1, padx=5, pady=2
            )
            ttk.Entry(self.channel_frame, textvariable=label).grid(
                row=index, column=2, sticky="ew", padx=5, pady=2
            )
            latest.grid(row=index, column=3, padx=5, pady=2)
            if index < len(values):
                latest.configure(text=f"{values[index]:.6f}")
            self.channel_rows.append((enabled, label, latest))
        self.channel_frame.columnconfigure(2, weight=1)

    def select_all(self, selected: bool):
        for enabled, _label, _latest in self.channel_rows:
            enabled.set(selected)

    def _selected_preset_labels(self, count: int) -> list[str]:
        """Return labels for the current preset, falling back to ch0... on mismatch."""
        names = CHANNEL_PRESETS[self.preset_var.get()]
        if names is None or len(names) != count:
            return default_labels(self.active_module_id or 0, count)
        return list(names)

    def _reset_incompatible_preset(self, count: int):
        names = CHANNEL_PRESETS[self.preset_var.get()]
        if names is not None and len(names) != count:
            self.preset_var.set("Generic ch0...")

    def apply_selected_preset(self):
        """Apply the selected FW preset to the editable Name column."""
        if self.active_module_id is None:
            return
        names = CHANNEL_PRESETS[self.preset_var.get()]
        count = self.modules[self.active_module_id]
        if names is not None and len(names) != count:
            messagebox.showwarning(
                "Preset mismatch",
                f"{self.preset_var.get()} is a {len(names)}-channel preset. "
                f"The current packet has {count} channels.",
            )
            self.preset_var.set("Generic ch0...")
        self._rebuild_channel_rows(self.active_module_id)

    def toggle_record(self):
        if self.record_file is None:
            self.start_recording()
        else:
            self.stop_recording()

    def start_recording(self):
        module_id = self.active_module_id
        if module_id is None:
            messagebox.showwarning("No module", "Connect and select a detected module first.")
            return
        indices = [index for index, row in enumerate(self.channel_rows) if row[0].get()]
        if not indices:
            messagebox.showwarning("No channels", "Check at least one channel to save.")
            return

        data_dir = os.path.join(os.path.dirname(__file__), "data")
        os.makedirs(data_dir, exist_ok=True)
        default_path = os.path.join(
            data_dir,
            f"cdc_selected_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
        )
        path = filedialog.asksaveasfilename(
            title="Save selected CDC channels",
            initialfile=os.path.basename(default_path),
            initialdir=data_dir,
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
        )
        if not path:
            return
        try:
            self.record_file = open(path, "w", newline="", encoding="utf-8")
        except OSError as exc:
            messagebox.showerror("Recording error", str(exc))
            return

        self.record_writer = csv.writer(self.record_file)
        # Freeze the checked channel indices and current editable names at the
        # moment recording starts. Later GUI edits do not change an open CSV.
        channel_names = [self.channel_rows[index][1].get().strip() or f"ch{index}" for index in indices]
        self.record_writer.writerow(
            ["pc_time_s", "seq_id", "module_id", "status", "tx_drops", *channel_names]
        )
        self.record_path = path
        self.record_module_id = module_id
        self.record_indices = indices
        self.record_count = 0
        self.record_button.configure(text="Stop recording")
        self.record_label.configure(text=f"recording 0 rows -> {path}")

    def stop_recording(self):
        if self.record_file is not None:
            self.record_file.flush()
            self.record_file.close()
        if self.record_path:
            self.record_label.configure(text=f"saved {self.record_count} rows -> {self.record_path}")
        else:
            self.record_label.configure(text="not recording")
        self.record_file = None
        self.record_writer = None
        self.record_path = None
        self.record_module_id = None
        self.record_indices = []
        self.record_button.configure(text="Start recording")

    def _handle_metadata(self, metadata: Metadata):
        self.metadata_labels[metadata.target_module_id] = list(metadata.labels)
        self.status_var.set(
            f"metadata received: module 0x{metadata.target_module_id:02X}, "
            f"{len(metadata.labels)} channels (select a preset to apply names)"
        )

    def _handle_packet(self, packet: Packet):
        self.received_count += 1
        count = len(packet.values)
        if self.modules.get(packet.module_id) != count:
            if self.record_module_id == packet.module_id:
                self.stop_recording()
                self.status_var.set("recording stopped: channel count changed")
            self.modules[packet.module_id] = count
            labels = self._module_labels()
            self.module_combo["values"] = labels
            if self.active_module_id is None:
                self.module_combo.set(module_text(packet.module_id, count))
                self.active_module_id = packet.module_id
                self._rebuild_channel_rows(packet.module_id)
            elif self.active_module_id == packet.module_id:
                self._reset_incompatible_preset(count)
                self._rebuild_channel_rows(packet.module_id)
        self.latest_values[packet.module_id] = packet.values

        if packet.module_id == self.active_module_id:
            for index, (_enabled, _label, latest) in enumerate(self.channel_rows):
                if index < count:
                    latest.configure(text=f"{packet.values[index]:.6f}")

        if self.record_writer is not None and packet.module_id == self.record_module_id:
            # Record only the channels checked when Start recording was pressed.
            self.record_writer.writerow(
                [
                    f"{packet.recv_t:.6f}",
                    packet.seq_id,
                    f"0x{packet.module_id:02X}",
                    f"0x{packet.status:02X}",
                    packet.status & 0x7F,
                    *(f"{packet.values[index]:.8f}" for index in self.record_indices),
                ]
            )
            self.record_count += 1
            if self.record_count % 200 == 0:
                self.record_file.flush()
                self.record_label.configure(
                    text=f"recording {self.record_count} rows -> {self.record_path}"
                )

    def poll(self):
        while True:
            try:
                self.status_var.set(self.message_queue.get_nowait())
            except queue.Empty:
                break

        processed = 0
        while processed < QUEUE_LIMIT:
            try:
                item = self.packet_queue.get_nowait()
            except queue.Empty:
                break
            if isinstance(item, Metadata):
                self._handle_metadata(item)
            else:
                self._handle_packet(item)
            processed += 1

        if self.reader is not None:
            self.status_var.set(
                f"received:{self.reader.good_packets}  decode errors:{self.reader.decode_errors}  "
                f"GUI queue drops:{self.reader.queue_drops}  detected modules:{len(self.modules)}"
            )
        self.root.after(POLL_MS, self.poll)

    def on_close(self):
        self.disconnect()
        self.root.destroy()


def main():
    root = tk.Tk()
    SelectiveLoggerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
