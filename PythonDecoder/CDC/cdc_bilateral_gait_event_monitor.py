"""
XM10 bilateral 4-FSR gait event monitor.

Receives PhAI V2.2 COBS-framed USB-CDC packets with four float channels:

    PF3, PF4, PF5, PF6

The script normalizes each FSR through a two-step calibration, converts the
normalized values into four contact bits, maps the 4-bit pattern to a bilateral
state, and emits a small public gait-event set:

    L_HEEL_STRIKE, R_HEEL_STRIKE, L_TOE_OFF, R_TOE_OFF,
    L_SUPPORT_START, R_SUPPORT_START, DOUBLE_SUPPORT, STANDING
"""

import argparse
from collections import deque
import csv
from dataclasses import dataclass
from datetime import datetime
import json
import math
import os
import struct
import sys
import time

import serial
import serial.tools.list_ports


PHAI_SOF = 0xAA
PHAI_HEADER_SIZE = 6
PHAI_CRC_SIZE = 2
DEFAULT_BAUD = 921600
DEFAULT_MODULE_ID = 0xF0
DEFAULT_LABELS = ["PF3", "PF4", "PF5", "PF6"]
DEFAULT_SAMPLE_RATE_HZ = 500.0
DEFAULT_LPF_CUTOFF_HZ = 8.0
DEFAULT_ON_THRESHOLD = 0.35
DEFAULT_OFF_THRESHOLD = 0.20
DEFAULT_STANDING_DWELL_S = 0.7
DEFAULT_MINIMUM_SPAN = 0.05
DEFAULT_PLOT_HZ = 20.0
DEFAULT_WINDOW_SEC = 6.0
DEFAULT_MAX_PLOT_POINTS = 300
DEFAULT_SAMPLE_DECIMATE = 2

CONTACT_PRESETS = {
    "Sensitive": (0.25, 0.12),
    "Normal": (DEFAULT_ON_THRESHOLD, DEFAULT_OFF_THRESHOLD),
    "Strict": (0.55, 0.35),
}

BITS = ("LH", "LT", "RH", "RT")
EVENTS = (
    "L_HEEL_STRIKE",
    "R_HEEL_STRIKE",
    "L_TOE_OFF",
    "R_TOE_OFF",
    "L_SUPPORT_START",
    "R_SUPPORT_START",
    "DOUBLE_SUPPORT",
    "STANDING",
)

HIGH_LEVEL_STATES = (
    "NO_CONTACT",
    "LEFT_SUPPORT",
    "RIGHT_SUPPORT",
    "DOUBLE_SUPPORT",
    "STANDING",
)

STATE_TABLE = {
    0b0000: ("NO_CONTACT", "no contact"),
    0b0001: ("R_TOE_ONLY", "right toe partial contact"),
    0b0010: ("R_HEEL_ONLY", "right heel loading"),
    0b0011: ("R_FOOT_FLAT", "right single support"),
    0b0100: ("L_TOE_ONLY", "left toe partial contact"),
    0b0101: ("TOE_TOE", "toe-toe transition"),
    0b0110: ("R_LOAD_L_PUSH", "right loading + left push-off"),
    0b0111: ("R_STANCE_L_PUSH", "double support, left push-off"),
    0b1000: ("L_HEEL_ONLY", "left heel loading"),
    0b1001: ("L_LOAD_R_PUSH", "left loading + right push-off"),
    0b1010: ("HEEL_HEEL", "heel-heel transition"),
    0b1011: ("L_LOAD_R_STANCE", "double support, left loading"),
    0b1100: ("L_FOOT_FLAT", "left single support"),
    0b1101: ("L_STANCE_R_PUSH", "double support, right push-off"),
    0b1110: ("R_LOAD_L_STANCE", "double support, right loading"),
    0b1111: ("DOUBLE_FULL", "full double contact"),
}


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def cobs_decode(encoded: bytes) -> bytes:
    out = bytearray()
    idx = 0
    while idx < len(encoded):
        code = encoded[idx]
        idx += 1
        if code == 0:
            raise ValueError("zero byte inside COBS frame")
        end = idx + code - 1
        if end > len(encoded):
            raise ValueError("truncated COBS frame")
        out.extend(encoded[idx:end])
        idx = end
        if code < 0xFF and idx < len(encoded):
            out.append(0)
    return bytes(out)


def parse_phai_frame(frame: bytes):
    decoded = cobs_decode(frame)
    min_size = PHAI_HEADER_SIZE + PHAI_CRC_SIZE
    if len(decoded) < min_size or decoded[0] != PHAI_SOF:
        return None

    len_units = decoded[1]
    total_size = PHAI_HEADER_SIZE + len_units * 4 + PHAI_CRC_SIZE
    if len_units == 0 or len(decoded) < total_size:
        return None

    payload_end = PHAI_HEADER_SIZE + len_units * 4
    crc_rx = decoded[payload_end] | (decoded[payload_end + 1] << 8)
    crc_calc = crc16_ccitt(decoded[:payload_end])
    if crc_rx != crc_calc:
        return None

    seq_id = decoded[2] | (decoded[3] << 8)
    module_id = decoded[4]
    status = decoded[5]
    payload = decoded[PHAI_HEADER_SIZE:payload_end]
    values = struct.unpack(f"<{len_units}f", payload)
    return seq_id, module_id, status, values


def list_ports() -> list[str]:
    return [p.device for p in serial.tools.list_ports.comports()]


def guess_port() -> str | None:
    ports = list_ports()
    if len(ports) == 1:
        return ports[0]
    preferred = [
        p for p in ports
        if "usbmodem" in p.lower()
        or "usbserial" in p.lower()
        or p.upper().startswith("COM")
    ]
    if len(preferred) == 1:
        return preferred[0]
    return None


def format_module(module_id: int) -> str:
    return f"0x{module_id:02X}"


def bitmask_from_contacts(contacts) -> int:
    mask = 0
    for idx, contact in enumerate(contacts):
        if contact:
            mask |= 1 << (3 - idx)
    return mask


def bits_text(mask: int) -> str:
    return format(mask, "04b")


def high_level_state(mask: int, standing: bool) -> str:
    if standing:
        return "STANDING"
    left = bool(mask & 0b1100)
    right = bool(mask & 0b0011)
    if left and right:
        return "DOUBLE_SUPPORT"
    if left:
        return "LEFT_SUPPORT"
    if right:
        return "RIGHT_SUPPORT"
    return "NO_CONTACT"


def lpf_alpha(cutoff_hz: float, sample_rate_hz: float) -> float:
    if cutoff_hz <= 0.0 or sample_rate_hz <= 0.0:
        return 1.0
    return float(1.0 - math.exp(-2.0 * math.pi * cutoff_hz / sample_rate_hz))


def contact_membership(value: float, off_threshold: float, on_threshold: float) -> float:
    """Linear contact membership used for visualization.

    0 below OFF threshold, 1 above ON threshold, linear between them.
    Hysteresis still decides the actual binary contact bit.
    """
    if on_threshold <= off_threshold:
        return 1.0 if value >= on_threshold else 0.0
    return max(0.0, min((float(value) - off_threshold) / (on_threshold - off_threshold), 1.0))


@dataclass
class Calibration:
    off: list[float]
    on: list[float]
    minimum_span: float = DEFAULT_MINIMUM_SPAN

    @classmethod
    def default(cls):
        return cls([0.0] * 4, [1.0] * 4)

    def spans(self):
        return [max(self.on[i] - self.off[i], self.minimum_span) for i in range(4)]

    def normalize(self, values):
        spans = self.spans()
        out = []
        for i, value in enumerate(values):
            norm = (float(value) - self.off[i]) / spans[i]
            out.append(max(0.0, min(norm, 1.5)))
        return out

    def quality(self):
        return [
            "OK" if self.on[i] - self.off[i] >= self.minimum_span else "WEAK"
            for i in range(4)
        ]

    def to_dict(self):
        return {
            "off": self.off,
            "on": self.on,
            "minimum_span": self.minimum_span,
            "labels": DEFAULT_LABELS,
            "version": 1,
        }

    @classmethod
    def from_dict(cls, data):
        off = [float(v) for v in data.get("off", [0.0] * 4)]
        on = [float(v) for v in data.get("on", [1.0] * 4)]
        if len(off) != 4 or len(on) != 4:
            raise ValueError("calibration must contain four off/on values")
        minimum_span = float(data.get("minimum_span", DEFAULT_MINIMUM_SPAN))
        return cls(off, on, minimum_span)


class ContactDetector:
    def __init__(self, on_threshold=DEFAULT_ON_THRESHOLD, off_threshold=DEFAULT_OFF_THRESHOLD):
        self.on_threshold = float(on_threshold)
        self.off_threshold = float(off_threshold)
        self.contacts = [False] * 4

    def reset(self):
        self.contacts = [False] * 4

    def update(self, normalized):
        for i, value in enumerate(normalized):
            if self.contacts[i]:
                if value <= self.off_threshold:
                    self.contacts[i] = False
            else:
                if value >= self.on_threshold:
                    self.contacts[i] = True
        return list(self.contacts)


class BilateralGaitDetector:
    def __init__(self, standing_dwell_s=DEFAULT_STANDING_DWELL_S):
        self.standing_dwell_s = float(standing_dwell_s)
        self.prev_mask = None
        self.prev_standing = False
        self.mask_since_t = 0.0

    def reset(self):
        self.prev_mask = None
        self.prev_standing = False
        self.mask_since_t = 0.0

    def update(self, t, contacts):
        mask = bitmask_from_contacts(contacts)
        if self.prev_mask is None or mask != self.prev_mask:
            self.mask_since_t = t

        standing = mask == 0b1111 and (t - self.mask_since_t) >= self.standing_dwell_s
        events = []

        if self.prev_mask is not None and mask != self.prev_mask:
            prev_left = bool(self.prev_mask & 0b1100)
            prev_right = bool(self.prev_mask & 0b0011)
            left = bool(mask & 0b1100)
            right = bool(mask & 0b0011)

            left_contact_on = not prev_left and left
            right_contact_on = not prev_right and right
            left_heel_on = not (self.prev_mask & 0b1000) and bool(mask & 0b1000)
            right_heel_on = not (self.prev_mask & 0b0010) and bool(mask & 0b0010)
            left_toe_off = bool(self.prev_mask & 0b0100) and not (mask & 0b0100)
            right_toe_off = bool(self.prev_mask & 0b0001) and not (mask & 0b0001)

            if (left_heel_on or left_contact_on) and prev_right:
                events.append("L_HEEL_STRIKE")
            if (right_heel_on or right_contact_on) and prev_left:
                events.append("R_HEEL_STRIKE")
            if left_toe_off:
                events.append("L_TOE_OFF")
            if right_toe_off:
                events.append("R_TOE_OFF")
            if left and not right and prev_right:
                events.append("L_SUPPORT_START")
            if right and not left and prev_left:
                events.append("R_SUPPORT_START")
            if left and right and (prev_left != prev_right):
                events.append("DOUBLE_SUPPORT")

        if standing and not self.prev_standing:
            events.append("STANDING")

        state_name, state_desc = STATE_TABLE[mask]
        if standing:
            state_name = "QUIET_STANDING"
            state_desc = "stable full-contact standing"

        self.prev_mask = mask
        self.prev_standing = standing
        return {
            "mask": mask,
            "bits": bits_text(mask),
            "state": state_name,
            "description": state_desc,
            "high_level": high_level_state(mask, standing),
            "standing": standing,
            "events": events,
        }


def run_gui(
    port: str | None,
    baud: int,
    module_id: int,
    labels: list[str],
    window_sec: float,
    plot_hz: float,
    max_plot_points: int,
    sample_decimate: int,
):
    try:
        from PyQt5 import QtCore, QtWidgets
        import pyqtgraph as pg
    except ImportError as exc:
        missing = exc.name or "PyQt5/pyqtgraph"
        print(f"Missing GUI dependency: {missing}", file=sys.stderr)
        print("Install with: python3 -m pip install pyserial pyqt5 pyqtgraph", file=sys.stderr)
        sys.exit(2)

    class SerialWorker(QtCore.QThread):
        sample = QtCore.pyqtSignal(float, int, tuple, float, int)
        status = QtCore.pyqtSignal(str)

        def __init__(self, port_name):
            super().__init__()
            self.port_name = port_name
            self._running = True

        def stop(self):
            self._running = False

        def run(self):
            try:
                ser = serial.Serial(self.port_name, baud, timeout=0.02)
                ser.dtr = True
                ser.rts = True
                ser.reset_input_buffer()
                ser.write(b"AGRB MON START\r\n")
            except Exception as exc:
                self.status.emit(f"open failed: {exc}")
                return

            self.status.emit(f"connected: {self.port_name} @ {baud}")
            wire_buf = bytearray()
            rx_bytes = 0
            decoded_count = 0
            matched_count = 0
            error_count = 0
            t0 = time.perf_counter()
            last_status = 0.0
            last_keepalive = 0.0

            try:
                while self._running:
                    try:
                        chunk = ser.read(4096)
                    except serial.SerialException as exc:
                        self.status.emit(f"serial error: {exc}")
                        break
                    now_abs = time.perf_counter()
                    if not chunk:
                        if now_abs - last_keepalive >= 1.0:
                            try:
                                ser.write(b"AGRB MON START\r\n")
                            except serial.SerialException as exc:
                                self.status.emit(f"serial error: {exc}")
                                break
                            self.status.emit(
                                f"waiting for module {format_module(module_id)} | "
                                f"decoded:{decoded_count} matched:{matched_count} errors:{error_count}"
                            )
                            last_keepalive = now_abs
                        continue

                    rx_bytes += len(chunk)
                    wire_buf.extend(chunk)
                    if now_abs - last_status >= 0.5:
                        self.status.emit(
                            f"connected: {self.port_name} @ {baud} | "
                            f"rx:{rx_bytes}B decoded:{decoded_count} "
                            f"matched:{matched_count} errors:{error_count}"
                        )
                        last_status = now_abs

                    while self._running:
                        delim = wire_buf.find(b"\x00")
                        if delim < 0:
                            break
                        raw_frame = bytes(wire_buf[:delim])
                        del wire_buf[:delim + 1]
                        if not raw_frame:
                            continue
                        try:
                            packet = parse_phai_frame(raw_frame)
                        except ValueError:
                            packet = None
                        if packet is None:
                            error_count += 1
                            continue

                        seq_id, mid, _status, values = packet
                        decoded_count += 1
                        if mid != module_id or len(values) < 4:
                            continue

                        matched_count += 1
                        if sample_decimate > 1 and (matched_count % sample_decimate) != 0:
                            continue
                        t = now_abs - t0
                        rate_hz = matched_count / max(t, 1e-6)
                        self.sample.emit(t, seq_id, tuple(values[:4]), rate_hz, error_count)
            finally:
                ser.close()
                self.status.emit("disconnected")

    class MainWindow(QtWidgets.QMainWindow):
        def __init__(self):
            super().__init__()
            self.setWindowTitle(f"XM10 Bilateral Gait Event Monitor - module {format_module(module_id)}")
            self.resize(1280, 760)

            self.calibration = Calibration.default()
            self.contact_detector = ContactDetector()
            self.gait_detector = BilateralGaitDetector()
            self.lpf_enabled = True
            self.lpf_state = None
            self.alpha = lpf_alpha(DEFAULT_LPF_CUTOFF_HZ, DEFAULT_SAMPLE_RATE_HZ)

            self.times = deque()
            self.raw_values = [deque() for _ in range(4)]
            self.norm_values = [deque() for _ in range(4)]
            self.membership_values = [deque() for _ in range(4)]
            self.contact_history = [deque() for _ in range(4)]
            self.event_markers = deque()
            self.latest_raw = [0.0] * 4
            self.latest_norm = [0.0] * 4
            self.latest_contacts = [False] * 4
            self.latest_info = {
                "bits": "0000",
                "state": "-",
                "high_level": "-",
                "events": [],
            }
            self.last_plot_update = 0.0
            self.recv_count = 0

            self.capture_mode = None
            self.capture_target = 0
            self.capture_samples = []

            self.record_file = None
            self.record_writer = None
            self.record_path = None
            self.record_count = 0
            self.marker_items = deque()
            self.last_event = "-"
            self.worker = None

            self._build_ui()
            self.refresh_ports()
            if port:
                self.select_port(port)
                self.connect_serial()

        def _build_ui(self):
            from PyQt5 import QtCore, QtWidgets
            import pyqtgraph as pg

            central = QtWidgets.QWidget()
            root = QtWidgets.QVBoxLayout(central)
            root.setContentsMargins(10, 10, 10, 10)
            root.setSpacing(8)

            top = QtWidgets.QHBoxLayout()
            top.addWidget(QtWidgets.QLabel("Port"))
            self.port_combo = QtWidgets.QComboBox()
            self.port_combo.setEditable(True)
            self.port_combo.setMinimumWidth(260)
            self.port_combo.setInsertPolicy(QtWidgets.QComboBox.NoInsert)
            top.addWidget(self.port_combo)

            self.refresh_button = QtWidgets.QPushButton("Refresh")
            self.refresh_button.clicked.connect(self.refresh_ports)
            top.addWidget(self.refresh_button)

            self.connect_button = QtWidgets.QPushButton("Connect")
            self.connect_button.clicked.connect(self.connect_serial)
            top.addWidget(self.connect_button)

            self.disconnect_button = QtWidgets.QPushButton("Disconnect")
            self.disconnect_button.clicked.connect(self.disconnect_serial)
            self.disconnect_button.setEnabled(False)
            top.addWidget(self.disconnect_button)

            self.status_label = QtWidgets.QLabel("select a serial port")
            self.status_label.setMinimumHeight(24)
            top.addWidget(self.status_label, 1)

            self.record_button = QtWidgets.QPushButton("Record")
            self.record_button.setCheckable(True)
            self.record_button.clicked.connect(self.on_record_clicked)
            top.addWidget(self.record_button)
            root.addLayout(top)

            controls = QtWidgets.QHBoxLayout()
            self.btn_zero = QtWidgets.QPushButton("Capture Zero/Off")
            self.btn_zero.clicked.connect(lambda: self.start_capture("off", 500))
            controls.addWidget(self.btn_zero)

            self.btn_on = QtWidgets.QPushButton("Capture On/Loaded")
            self.btn_on.clicked.connect(lambda: self.start_capture("on", 1000))
            controls.addWidget(self.btn_on)

            self.btn_save_cal = QtWidgets.QPushButton("Save Cal")
            self.btn_save_cal.clicked.connect(self.save_calibration)
            controls.addWidget(self.btn_save_cal)

            self.btn_load_cal = QtWidgets.QPushButton("Load Cal")
            self.btn_load_cal.clicked.connect(self.load_calibration)
            controls.addWidget(self.btn_load_cal)

            self.btn_clear_cal = QtWidgets.QPushButton("Clear Cal")
            self.btn_clear_cal.clicked.connect(self.clear_calibration)
            controls.addWidget(self.btn_clear_cal)

            controls.addWidget(QtWidgets.QLabel("ON"))
            self.spin_on = QtWidgets.QDoubleSpinBox()
            self.spin_on.setRange(0.01, 1.50)
            self.spin_on.setSingleStep(0.05)
            self.spin_on.setValue(DEFAULT_ON_THRESHOLD)
            self.spin_on.valueChanged.connect(self.on_threshold_changed)
            controls.addWidget(self.spin_on)

            controls.addWidget(QtWidgets.QLabel("OFF"))
            self.spin_off = QtWidgets.QDoubleSpinBox()
            self.spin_off.setRange(0.00, 1.49)
            self.spin_off.setSingleStep(0.05)
            self.spin_off.setValue(DEFAULT_OFF_THRESHOLD)
            self.spin_off.valueChanged.connect(self.on_threshold_changed)
            controls.addWidget(self.spin_off)

            preset_box = QtWidgets.QGroupBox("Presets")
            preset_layout = QtWidgets.QHBoxLayout(preset_box)
            preset_layout.setContentsMargins(6, 2, 6, 2)
            for name, (on_thr, off_thr) in CONTACT_PRESETS.items():
                btn = QtWidgets.QPushButton(name)
                btn.setToolTip(
                    f"contact threshold preset: ON={on_thr:.2f}, OFF={off_thr:.2f}"
                )
                btn.clicked.connect(
                    lambda _checked=False, n=name, on=on_thr, off=off_thr:
                    self.apply_contact_preset(n, on, off)
                )
                preset_layout.addWidget(btn)
            controls.addWidget(preset_box)

            self.cb_lpf = QtWidgets.QCheckBox("LPF")
            self.cb_lpf.setChecked(True)
            self.cb_lpf.toggled.connect(lambda checked: setattr(self, "lpf_enabled", bool(checked)))
            controls.addWidget(self.cb_lpf)

            controls.addStretch()
            root.addLayout(controls)

            summary = QtWidgets.QHBoxLayout()
            self.bit_labels = []
            for bit in BITS:
                lbl = QtWidgets.QLabel(f"{bit}:0")
                lbl.setAlignment(QtCore.Qt.AlignCenter)
                lbl.setMinimumWidth(72)
                lbl.setStyleSheet(self._bit_css(False))
                self.bit_labels.append(lbl)
                summary.addWidget(lbl)

            self.state_label = QtWidgets.QLabel("bits: 0000 | state: - | event: -")
            self.state_label.setMinimumHeight(32)
            summary.addWidget(self.state_label, 1)
            root.addLayout(summary)

            state_row = QtWidgets.QHBoxLayout()
            self.state_blocks = {}
            for name in HIGH_LEVEL_STATES:
                lbl = QtWidgets.QLabel(name.replace("_", " "))
                lbl.setAlignment(QtCore.Qt.AlignCenter)
                lbl.setMinimumHeight(34)
                lbl.setStyleSheet(self._block_css(False, "#1f77b4"))
                self.state_blocks[name] = lbl
                state_row.addWidget(lbl, 1)
            root.addLayout(state_row)

            event_row = QtWidgets.QHBoxLayout()
            self.event_blocks = {}
            for name in EVENTS:
                lbl = QtWidgets.QLabel(name.replace("_", "\n"))
                lbl.setAlignment(QtCore.Qt.AlignCenter)
                lbl.setMinimumHeight(46)
                lbl.setStyleSheet(self._block_css(False, "#d62728"))
                self.event_blocks[name] = lbl
                event_row.addWidget(lbl, 1)
            root.addLayout(event_row)

            self.plot = pg.PlotWidget(title="Normalized FSR Signals")
            self.plot.setBackground("w")
            self.plot.showGrid(x=True, y=True, alpha=0.25)
            self.plot.setLabel("bottom", "time", units="s")
            self.plot.setLabel("left", "normalized load")
            self.plot.setYRange(0.0, 1.55)
            self.plot.addLegend(offset=(10, 10))
            root.addWidget(self.plot, 2)

            colors = ["#d62728", "#ff7f0e", "#1f77b4", "#2ca02c"]
            self.curves = []
            for label, color in zip(labels, colors):
                self.curves.append(self.plot.plot([], [], pen=pg.mkPen(color=color, width=2), name=label))

            self.on_line = pg.InfiniteLine(
                pos=self.spin_on.value(),
                angle=0,
                pen=pg.mkPen("#444444", width=1, style=QtCore.Qt.DashLine),
            )
            self.off_line = pg.InfiniteLine(
                pos=self.spin_off.value(),
                angle=0,
                pen=pg.mkPen("#888888", width=1, style=QtCore.Qt.DotLine),
            )
            self.plot.addItem(self.on_line)
            self.plot.addItem(self.off_line)

            self.membership_plot = pg.PlotWidget(title="Contact Membership Functions")
            self.membership_plot.setBackground("w")
            self.membership_plot.showGrid(x=True, y=True, alpha=0.25)
            self.membership_plot.setLabel("bottom", "time", units="s")
            self.membership_plot.setLabel("left", "membership")
            self.membership_plot.setYRange(0.0, 1.05)
            self.membership_plot.addLegend(offset=(10, 10))
            root.addWidget(self.membership_plot, 2)

            self.membership_curves = []
            for label, color in zip(labels, colors):
                self.membership_curves.append(
                    self.membership_plot.plot(
                        [], [], pen=pg.mkPen(color=color, width=2), name=f"{label} μ"
                    )
                )

            lower = QtWidgets.QHBoxLayout()
            self.event_log = QtWidgets.QTableWidget(0, 4)
            self.event_log.setHorizontalHeaderLabels(["time", "event", "bits", "state"])
            self.event_log.horizontalHeader().setStretchLastSection(True)
            self.event_log.setMinimumHeight(88)
            self.event_log.setMaximumHeight(115)
            lower.addWidget(self.event_log, 2)

            self.info_label = QtWidgets.QLabel("")
            self.info_label.setMinimumWidth(360)
            self.info_label.setMaximumHeight(115)
            self.info_label.setAlignment(QtCore.Qt.AlignTop)
            lower.addWidget(self.info_label, 1)
            root.addLayout(lower)

            self.record_label = QtWidgets.QLabel("recording: off")
            root.addWidget(self.record_label)
            self.setCentralWidget(central)
            self.update_info_label()

        @staticmethod
        def _bit_css(active):
            if active:
                return "background:#1a9850; color:white; border:1px solid #0b6b32; padding:6px;"
            return "background:#e8e8e8; color:#444; border:1px solid #c0c0c0; padding:6px;"

        @staticmethod
        def _block_css(active, color):
            if active:
                return f"background:{color}; color:white; border:2px solid {color}; padding:6px; font-weight:bold;"
            return "background:#eeeeee; color:#777; border:1px solid #c8c8c8; padding:6px;"

        def refresh_ports(self):
            selected = self.current_port_text()
            self.port_combo.blockSignals(True)
            self.port_combo.clear()
            for item in serial.tools.list_ports.comports():
                self.port_combo.addItem(f"{item.device} - {item.description}", item.device)
            self.port_combo.blockSignals(False)
            if selected:
                self.select_port(selected)
            elif self.port_combo.count() == 0:
                self.port_combo.setEditText("")
            self.status_label.setText(
                f"{self.port_combo.count()} serial port(s) found"
                if self.port_combo.count()
                else "no serial ports found; type COM port manually"
            )

        def select_port(self, port_name):
            for idx in range(self.port_combo.count()):
                if self.port_combo.itemData(idx) == port_name:
                    self.port_combo.setCurrentIndex(idx)
                    return
            self.port_combo.setEditText(port_name)

        def current_port_text(self):
            text = self.port_combo.currentText().strip()
            if " - " in text:
                text = text.split(" - ", 1)[0].strip()
            data = self.port_combo.currentData()
            if not text and data:
                return str(data).strip()
            return text

        def connect_serial(self):
            port_name = self.current_port_text()
            if not port_name:
                self.status_label.setText("select or type a serial port first")
                return
            if self.worker and self.worker.isRunning():
                self.disconnect_serial()

            self.reset_runtime_state()
            self.worker = SerialWorker(port_name)
            self.worker.sample.connect(self.on_sample)
            self.worker.status.connect(self.status_label.setText)
            self.worker.finished.connect(self.on_worker_finished)
            self.worker.start()

            self.port_combo.setEnabled(False)
            self.refresh_button.setEnabled(False)
            self.connect_button.setEnabled(False)
            self.disconnect_button.setEnabled(True)
            self.status_label.setText(f"opening {port_name} @ {baud}")

        def disconnect_serial(self):
            if self.worker:
                self.worker.stop()
                self.worker.wait(1000)
                self.worker = None
            self.on_worker_finished()

        def on_worker_finished(self):
            self.port_combo.setEnabled(True)
            self.refresh_button.setEnabled(True)
            self.connect_button.setEnabled(True)
            self.disconnect_button.setEnabled(False)

        def reset_runtime_state(self):
            for _, item in list(self.marker_items):
                self.plot.removeItem(item)
            self.times.clear()
            for series in (
                self.raw_values
                + self.norm_values
                + self.membership_values
                + self.contact_history
            ):
                series.clear()
            self.event_markers.clear()
            self.marker_items.clear()
            self.latest_raw = [0.0] * 4
            self.latest_norm = [0.0] * 4
            self.latest_contacts = [False] * 4
            self.latest_info = {
                "bits": "0000",
                "state": "-",
                "high_level": "-",
                "events": [],
            }
            self.last_plot_update = 0.0
            self.recv_count = 0
            self.last_event = "-"
            self.lpf_state = None
            self.capture_mode = None
            self.capture_target = 0
            self.capture_samples = []
            self.contact_detector.reset()
            self.gait_detector.reset()
            self.event_log.setRowCount(0)
            for curve in self.curves + self.membership_curves:
                curve.setData([], [])
            self.update_state_labels(0, 0.0, 0)

        def on_threshold_changed(self):
            if self.spin_off.value() >= self.spin_on.value():
                self.spin_off.setValue(max(0.0, self.spin_on.value() - 0.01))
            self.contact_detector.on_threshold = float(self.spin_on.value())
            self.contact_detector.off_threshold = float(self.spin_off.value())
            self.on_line.setValue(self.spin_on.value())
            self.off_line.setValue(self.spin_off.value())

        def apply_contact_preset(self, name, on_threshold, off_threshold):
            self.spin_on.setValue(float(on_threshold))
            self.spin_off.setValue(float(off_threshold))
            self.contact_detector.reset()
            self.gait_detector.reset()
            self.status_label.setText(
                f"{name} preset applied: ON={on_threshold:.2f}, OFF={off_threshold:.2f}"
            )

        def filtered_values(self, raw):
            if self.lpf_state is None:
                self.lpf_state = [float(v) for v in raw]
            else:
                for i, value in enumerate(raw):
                    self.lpf_state[i] = self.alpha * float(value) + (1.0 - self.alpha) * self.lpf_state[i]
            return self.lpf_state if self.lpf_enabled else [float(v) for v in raw]

        def start_capture(self, mode, samples):
            self.capture_mode = mode
            self.capture_target = int(samples)
            self.capture_samples = []
            self.status_label.setText(f"capturing {mode} calibration: 0/{samples}")

        def finish_capture(self):
            if not self.capture_samples:
                return
            from statistics import median

            med = [
                float(median(sample[i] for sample in self.capture_samples))
                for i in range(4)
            ]
            if self.capture_mode == "off":
                self.calibration.off = med
            elif self.capture_mode == "on":
                self.calibration.on = med
            self.status_label.setText(f"{self.capture_mode} calibration captured")
            self.capture_mode = None
            self.capture_target = 0
            self.capture_samples = []
            self.contact_detector.reset()
            self.gait_detector.reset()
            self.update_info_label()

        def save_calibration(self):
            from PyQt5 import QtWidgets

            default_dir = os.path.join(os.path.dirname(__file__), "data")
            os.makedirs(default_dir, exist_ok=True)
            default_path = os.path.join(default_dir, "bilateral_gait_calibration.json")
            path, _ = QtWidgets.QFileDialog.getSaveFileName(
                self, "Save Calibration", default_path, "JSON Files (*.json);;All Files (*)"
            )
            if not path:
                return
            try:
                with open(path, "w", encoding="utf-8") as f:
                    json.dump(self.calibration.to_dict(), f, indent=2)
                self.status_label.setText(f"saved calibration: {path}")
            except OSError as exc:
                QtWidgets.QMessageBox.critical(self, "Calibration Save Error", str(exc))

        def load_calibration(self):
            from PyQt5 import QtWidgets

            default_dir = os.path.join(os.path.dirname(__file__), "data")
            path, _ = QtWidgets.QFileDialog.getOpenFileName(
                self, "Load Calibration", default_dir, "JSON Files (*.json);;All Files (*)"
            )
            if not path:
                return
            try:
                with open(path, "r", encoding="utf-8") as f:
                    self.calibration = Calibration.from_dict(json.load(f))
                self.contact_detector.reset()
                self.gait_detector.reset()
                self.update_info_label()
                self.status_label.setText(f"loaded calibration: {path}")
            except (OSError, ValueError, json.JSONDecodeError) as exc:
                QtWidgets.QMessageBox.critical(self, "Calibration Load Error", str(exc))

        def clear_calibration(self):
            self.calibration = Calibration.default()
            self.contact_detector.reset()
            self.gait_detector.reset()
            self.update_info_label()
            self.status_label.setText("calibration cleared")

        def update_info_label(self):
            spans = self.calibration.spans()
            quality = self.calibration.quality()
            lines = [
                f"module: {format_module(module_id)}",
                f"labels: {', '.join(labels)}",
                f"events: {', '.join(EVENTS)}",
                "",
                "calibration:",
            ]
            for i, label in enumerate(labels):
                lines.append(
                    f"  {label}: off={self.calibration.off[i]:.4f} "
                    f"on={self.calibration.on[i]:.4f} span={spans[i]:.4f} {quality[i]}"
                )
            self.info_label.setText("\n".join(lines))

        def on_sample(self, t, seq_id, raw, rate_hz, error_count):
            self.recv_count += 1
            self.latest_raw = [float(v) for v in raw]
            filt = self.filtered_values(self.latest_raw)

            if self.capture_mode:
                self.capture_samples.append(list(filt))
                n = len(self.capture_samples)
                self.status_label.setText(
                    f"capturing {self.capture_mode} calibration: {n}/{self.capture_target}"
                )
                if n >= self.capture_target:
                    self.finish_capture()

            norm = self.calibration.normalize(filt)
            memberships = [
                contact_membership(v, self.spin_off.value(), self.spin_on.value())
                for v in norm
            ]
            contacts = self.contact_detector.update(norm)
            info = self.gait_detector.update(t, contacts)
            self.latest_norm = norm
            self.latest_contacts = contacts
            self.latest_info = info

            self.write_record_row(t, seq_id, raw, norm, contacts, info)
            for event in info["events"]:
                self.last_event = event
                self.add_event_log(t, event, info)
                self.event_markers.append((t, event))

            now_abs = time.perf_counter()
            if now_abs - self.last_plot_update < 1.0 / plot_hz:
                return
            self.last_plot_update = now_abs

            self.times.append(t)
            for idx in range(4):
                self.raw_values[idx].append(self.latest_raw[idx])
                self.norm_values[idx].append(norm[idx])
                self.membership_values[idx].append(memberships[idx])
                self.contact_history[idx].append(1.0 if contacts[idx] else 0.0)

            cutoff = t - window_sec
            while self.times and self.times[0] < cutoff:
                self.times.popleft()
                for series in (
                    self.raw_values
                    + self.norm_values
                    + self.membership_values
                    + self.contact_history
                ):
                    series.popleft()
            while self.event_markers and self.event_markers[0][0] < cutoff:
                self.event_markers.popleft()

            self.update_plot(t)
            self.update_state_labels(seq_id, rate_hz, error_count)

        def update_plot(self, t):
            xs_full = list(self.times)
            if not xs_full:
                return
            stride = max(1, len(xs_full) // max(1, int(max_plot_points)))
            xs = xs_full[::stride]
            for curve, series in zip(self.curves, self.norm_values):
                curve.setData(xs, list(series)[::stride])
            for curve, series in zip(self.membership_curves, self.membership_values):
                curve.setData(xs, list(series)[::stride])
            if t > window_sec:
                self.plot.setXRange(t - window_sec, t, padding=0)
                self.membership_plot.setXRange(t - window_sec, t, padding=0)
            else:
                self.plot.setXRange(0, window_sec, padding=0)
                self.membership_plot.setXRange(0, window_sec, padding=0)
            cutoff = t - window_sec
            while self.marker_items and self.marker_items[0][0] < cutoff:
                _old_t, item = self.marker_items.popleft()
                self.plot.removeItem(item)

        def update_state_labels(self, seq_id, rate_hz, error_count):
            for idx, lbl in enumerate(self.bit_labels):
                lbl.setText(f"{BITS[idx]}:{1 if self.latest_contacts[idx] else 0}")
                lbl.setStyleSheet(self._bit_css(self.latest_contacts[idx]))

            events = self.latest_info["events"]
            event_text = "|".join(events) if events else "-"
            raw_text = " ".join(f"{label}:{value:.3f}" for label, value in zip(labels, self.latest_raw))
            norm_text = " ".join(f"{label}:{value:.2f}" for label, value in zip(labels, self.latest_norm))
            self.state_label.setText(
                f"seq:{seq_id} rate:{rate_hz:.1f}Hz errors:{error_count} | "
                f"bits:{self.latest_info['bits']} state:{self.latest_info['state']} "
                f"level:{self.latest_info['high_level']} event:{event_text} | "
                f"{norm_text}"
            )
            self.status_label.setText(f"recv:{self.recv_count} | {raw_text}")

            active_state = self.latest_info["high_level"]
            for name, lbl in self.state_blocks.items():
                lbl.setStyleSheet(self._block_css(name == active_state, "#1f77b4"))

            active_events = set(events)
            for name, lbl in self.event_blocks.items():
                lbl.setStyleSheet(self._block_css(name in active_events, "#d62728"))

        def add_event_log(self, t, event, info):
            from PyQt5 import QtCore, QtWidgets
            import pyqtgraph as pg

            row = 0
            self.event_log.insertRow(row)
            values = [f"{t:.3f}", event, info["bits"], info["state"]]
            for col, value in enumerate(values):
                self.event_log.setItem(row, col, QtWidgets.QTableWidgetItem(value))
            while self.event_log.rowCount() > 100:
                self.event_log.removeRow(self.event_log.rowCount() - 1)
            marker = pg.InfiniteLine(
                pos=t,
                angle=90,
                pen=pg.mkPen("#222222", width=1, style=QtCore.Qt.DashLine),
            )
            self.plot.addItem(marker)
            self.marker_items.append((t, marker))

        def on_record_clicked(self, checked):
            if checked:
                if not self.start_recording():
                    self.record_button.setChecked(False)
            else:
                self.stop_recording()

        def start_recording(self):
            from PyQt5 import QtWidgets

            default_dir = os.path.join(os.path.dirname(__file__), "data")
            os.makedirs(default_dir, exist_ok=True)
            default_name = f"xm10_bilateral_gait_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
            default_path = os.path.join(default_dir, default_name)
            path, _ = QtWidgets.QFileDialog.getSaveFileName(
                self, "Save CSV Recording", default_path, "CSV Files (*.csv);;All Files (*)"
            )
            if not path:
                return False
            try:
                self.record_file = open(path, "w", newline="", encoding="utf-8")
            except OSError as exc:
                QtWidgets.QMessageBox.critical(self, "Recording Error", str(exc))
                return False

            self.record_writer = csv.writer(self.record_file)
            self.record_writer.writerow([
                "time_s", "seq_id",
                f"{labels[0]}_raw", f"{labels[1]}_raw", f"{labels[2]}_raw", f"{labels[3]}_raw",
                f"{labels[0]}_norm", f"{labels[1]}_norm", f"{labels[2]}_norm", f"{labels[3]}_norm",
                "LH", "LT", "RH", "RT",
                "bits", "state", "high_level_state", "event",
            ])
            self.record_path = path
            self.record_count = 0
            self.record_button.setText("Stop")
            self.record_label.setText(f"recording: 0 rows -> {path}")
            return True

        def stop_recording(self):
            if self.record_file:
                self.record_file.flush()
                self.record_file.close()
            saved_path = self.record_path
            saved_count = self.record_count
            self.record_file = None
            self.record_writer = None
            self.record_path = None
            self.record_button.setText("Record")
            if saved_path:
                self.record_label.setText(f"saved: {saved_count} rows -> {saved_path}")
            else:
                self.record_label.setText("recording: off")

        def write_record_row(self, t, seq_id, raw, norm, contacts, info):
            if not self.record_writer:
                return
            event_text = "|".join(info["events"]) if info["events"] else ""
            self.record_writer.writerow([
                f"{t:.6f}",
                seq_id,
                *(f"{v:.8f}" for v in raw),
                *(f"{v:.8f}" for v in norm),
                *(1 if c else 0 for c in contacts),
                info["bits"],
                info["state"],
                info["high_level"],
                event_text,
            ])
            self.record_count += 1
            if self.record_count % 200 == 0:
                self.record_file.flush()
                self.record_label.setText(f"recording: {self.record_count} rows -> {self.record_path}")

        def closeEvent(self, event):
            if self.record_file:
                self.stop_recording()
            if self.worker:
                self.worker.stop()
                self.worker.wait(1000)
            event.accept()

    app = QtWidgets.QApplication(sys.argv)
    pg.setConfigOptions(antialias=True)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())


def parse_module_id(text: str) -> int:
    return int(text, 0)


def main():
    parser = argparse.ArgumentParser(description="XM10 bilateral 4-FSR gait event monitor")
    parser.add_argument("--port", help="Serial port, e.g. COM6 or /dev/tty.usbmodemXXXX")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--module", type=parse_module_id, default=DEFAULT_MODULE_ID)
    parser.add_argument("--labels", default=",".join(DEFAULT_LABELS))
    parser.add_argument("--plot-hz", type=float, default=DEFAULT_PLOT_HZ)
    parser.add_argument("--window", type=float, default=DEFAULT_WINDOW_SEC, help="GUI time window in seconds")
    parser.add_argument("--max-points", type=int, default=DEFAULT_MAX_PLOT_POINTS,
                        help="Maximum points drawn per curve in the GUI")
    parser.add_argument("--sample-decimate", type=int, default=DEFAULT_SAMPLE_DECIMATE,
                        help="Use every Nth matched CDC sample for GUI processing")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()

    if args.list_ports:
        for port in list_ports():
            print(port)
        return

    labels = [item.strip() for item in args.labels.split(",") if item.strip()]
    if len(labels) != 4:
        print("--labels must contain exactly 4 comma-separated names", file=sys.stderr)
        sys.exit(2)

    port = args.port or guess_port()

    run_gui(
        port,
        args.baud,
        args.module,
        labels,
        args.window,
        args.plot_hz,
        args.max_points,
        max(1, args.sample_decimate),
    )


if __name__ == "__main__":
    main()
