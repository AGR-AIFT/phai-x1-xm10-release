"""
PhAI V2.2 Protocol — USB-CDC Real-time Receiver & Logger
=========================================================
XM10 보드의 PhAI V2.2 바이너리 프로토콜을 실시간으로 수신하여
그래프로 시각화하고 CSV로 저장하는 GUI 도구입니다.

패킷 구조 (Little-Endian) — V2.2:
    내부 패킷:
    [SOF:0xAA] [LEN:1] [SEQ_ID:2 LE] [MODULE_ID:1] [STATUS:1] [PAYLOAD: LEN×4] [CRC16:2 LE]

    와이어 포맷:
    [COBS_ENCODE(내부 패킷)] [0x00 delimiter]

    - LEN: payload의 4-byte 단위 개수 (0~255)
    - STATUS: bits 0-6 = Tx drop delta (0~127), bit 7 = reserved
    - CRC16-CCITT: polynomial 0x1021, init 0xFFFF, bytes[0]~bytes[total-3]
    - COBS: 0x00을 프레임 구분자로 사용, payload에서 0x00 제거

Module IDs:
    0x01  IMU Accel       0x02  IMU Gyro        0x03  IMU Quat
    0x04  GRF Left        0x05  GRF Right       0x06  Motor Left
    0x07  Motor Right     0x10  Combined (10ch)  0xF0~0xFE  User Custom
    0xFF  Debug

사용법:
    pip install pyserial pyqt5 pyqtgraph numpy
    python cdc_phai_receiver.py

    CLI 모드:
    python cdc_phai_receiver.py --cli --port COM6

Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
"""

import sys
import os
import glob
import struct
import time
import argparse
import threading
import serial
import serial.tools.list_ports
from datetime import datetime
from collections import deque

from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import pyqtSignal, pyqtSlot, Qt

import pyqtgraph as pg
import numpy as np

# ============================================================================
# Protocol Constants — PhAI V2.2
# ============================================================================

PHAI_SOF = 0xAA
PHAI_HEADER_SIZE = 6   # SOF(1) + LEN(1) + SEQ_ID(2) + MODULE_ID(1) + STATUS(1)
PHAI_CRC_SIZE = 2      # CRC16-CCITT (2 bytes LE)
PHAI_MAX_LEN_UNITS = 255

CRC16_TABLE = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
]

# MODULE_ID → (name, [channel_names] or None)
# Combined V2.1: 10ch (Accel3 + Gyro3 + MotorAngle2 + MotorTorque2)
MODULE_DEFS = {
    0x01: ("IMU_Accel",    ["AccX", "AccY", "AccZ"]),
    0x02: ("IMU_Gyro",     ["GyrX", "GyrY", "GyrZ"]),
    0x03: ("IMU_Quat",     ["QuatW", "QuatX", "QuatY", "QuatZ"]),
    0x04: ("GRF_Left",     ["GRF_LX", "GRF_LY", "GRF_LZ"]),
    0x05: ("GRF_Right",    ["GRF_RX", "GRF_RY", "GRF_RZ"]),
    0x06: ("Motor_Left",   None),
    0x07: ("Motor_Right",  None),
    0x10: ("Combined",     [
        "AccX", "AccY", "AccZ",
        "GyrX", "GyrY", "GyrZ",
        "MotorAngle_L", "MotorAngle_R",
        "MotorTorque_L", "MotorTorque_R",
    ]),
    0xFF: ("Debug", None),
}

# Combined 모드의 의미론적 플롯 그룹 (센서 도메인별)
COMBINED_PLOT_GROUPS = [
    ("Accelerometer",  [0, 1, 2]),
    ("Gyroscope",      [3, 4, 5]),
    ("Motor Angle",    [6, 7]),
    ("Motor Torque",   [8, 9]),
]

DEVICE_PERIOD_MS = 1          # XM 제어 주기 1ms
DEFAULT_BAUD = 921600
DEFAULT_TIMEOUT = 0.02        # 20ms serial read timeout
FLUSH_EVERY = 500
DEFAULT_WINDOW_SAMPLES = 3000
MAX_CHANNELS = 64

# ============================================================================
# Helpers
# ============================================================================

def crc16_ccitt(data: bytes) -> int:
    """CRC16-CCITT (poly 0x1021, init 0xFFFF)."""
    crc = 0xFFFF
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ CRC16_TABLE[((crc >> 8) ^ b) & 0xFF]
    return crc


def cobs_decode(encoded: bytes) -> bytes:
    """COBS-decode a frame (without trailing 0x00 delimiter)."""
    out = bytearray()
    i = 0
    while i < len(encoded):
        code = encoded[i]
        i += 1
        if code == 0:
            break
        for _ in range(1, code):
            if i >= len(encoded):
                break
            out.append(encoded[i])
            i += 1
        if code < 0xFF and i < len(encoded):
            out.append(0x00)
    return bytes(out)

def get_module_name(mid: int) -> str:
    if mid in MODULE_DEFS:
        return MODULE_DEFS[mid][0]
    if 0xF0 <= mid <= 0xFE:
        return f"User_0x{mid:02X}"
    return f"Unknown_0x{mid:02X}"

def get_channel_names(mid: int, n: int) -> list:
    if mid in MODULE_DEFS and MODULE_DEFS[mid][1] is not None:
        names = MODULE_DEFS[mid][1]
        if len(names) == n:
            return list(names)
    return [f"ch{i}" for i in range(n)]

def build_plot_groups(mid: int, ch_names: list) -> list:
    """Returns [(group_name, [ch_indices]), ...] for logical grouping."""
    if mid == 0x10 and len(ch_names) == 10:
        return COMBINED_PLOT_GROUPS
    n = len(ch_names)
    if n <= 6:
        return [("All Channels", list(range(n)))]
    per = max(1, (n + 5) // 6)
    groups = []
    for i in range(0, n, per):
        end = min(i + per, n)
        label = f"Ch {i}–{end - 1}"
        groups.append((label, list(range(i, end))))
    return groups[:6]


# ============================================================================
# Parsed Packet
# ============================================================================

class PhAIPacket:
    __slots__ = ('seq_id', 'module_id', 'status', 'tx_drops', 'floats', 'recv_t')

    def __init__(self, seq_id, module_id, status, floats, recv_t):
        self.seq_id = seq_id
        self.module_id = module_id
        self.status = status
        self.tx_drops = status & 0x7F
        self.floats = floats
        self.recv_t = recv_t


# ============================================================================
# Serial Worker — batch delivery via shared deque (lock-free-ish)
# ============================================================================

class PhAISerialWorker(QtCore.QObject):
    status_msg = pyqtSignal(str)
    finished = pyqtSignal()
    connection_failed = pyqtSignal(str)
    port_lost = pyqtSignal()

    def __init__(self, port_name, baudrate=DEFAULT_BAUD, parent=None):
        super().__init__(parent)
        self.port_name = port_name
        self.baudrate = baudrate
        self._running = False
        self.good = 0
        self.crc_err = 0
        self.sync_err = 0
        self.total_tx_drops = 0
        self.packet_queue = deque(maxlen=50000)

    @pyqtSlot()
    def run(self):
        self._running = True
        ser = None
        try:
            try:
                ser = serial.Serial(self.port_name, self.baudrate, timeout=DEFAULT_TIMEOUT)
                self.status_msg.emit(f"Connected to {self.port_name}")
            except Exception as e:
                self.connection_failed.emit(str(e))
                return

            wire_buf = bytearray()
            perf_start = time.perf_counter()

            while self._running:
                try:
                    chunk = ser.read(2048)
                except serial.SerialException:
                    self.port_lost.emit()
                    break

                if not chunk:
                    continue

                wire_buf.extend(chunk)

                while True:
                    delim = wire_buf.find(b'\x00')
                    if delim < 0:
                        break

                    recv_t = time.perf_counter() - perf_start

                    if delim > 0:
                        encoded = bytes(wire_buf[:delim])
                        frame = cobs_decode(encoded)
                        self._parse_frame(frame, recv_t)

                    del wire_buf[:delim + 1]

        finally:
            if ser is not None and ser.is_open:
                ser.close()
            self.finished.emit()

    def _parse_frame(self, frame: bytes, recv_t: float):
        """Parse a single COBS-decoded frame into a PhAIPacket."""
        min_size = PHAI_HEADER_SIZE + PHAI_CRC_SIZE
        if len(frame) < min_size:
            self.sync_err += 1
            return

        if frame[0] != PHAI_SOF:
            self.sync_err += 1
            return

        len_units = frame[1]
        if len_units == 0 or len_units > PHAI_MAX_LEN_UNITS:
            self.sync_err += 1
            return

        expected_size = PHAI_HEADER_SIZE + (len_units * 4) + PHAI_CRC_SIZE
        if len(frame) < expected_size:
            self.sync_err += 1
            return

        crc_recv = frame[expected_size - 2] | (frame[expected_size - 1] << 8)
        crc_calc = crc16_ccitt(frame[:expected_size - PHAI_CRC_SIZE])
        if crc_calc != crc_recv:
            self.crc_err += 1
            return

        seq_id = frame[2] | (frame[3] << 8)
        module_id = frame[4]
        status_byte = frame[5]

        payload = frame[PHAI_HEADER_SIZE:PHAI_HEADER_SIZE + len_units * 4]
        floats = np.frombuffer(payload, dtype='<f4').copy()

        tx_drop_delta = status_byte & 0x7F
        self.total_tx_drops += tx_drop_delta

        pkt = PhAIPacket(seq_id, module_id, status_byte, floats, recv_t)
        self.packet_queue.append(pkt)
        self.good += 1

    def stop(self):
        self._running = False


# ============================================================================
# Style — Dark / Light themes
# ============================================================================

_MODERN_LIGHT = {
    'name': 'Modern', 'next': 'Classic',
    'bg': '#f5f7fb', 'base': '#ffffff', 'text': '#111827',
    'accent': '#2563eb', 'accent_hover': '#1d4ed8',
    'border': '#cbd5e1', 'disabled': '#9ca3af',
    'plot_bg': 'w', 'grid_alpha': 0.15,
    'axis_pen': '#9ca3af', 'axis_text': '#4b5563',
}
_CLASSIC_LIGHT = {
    'name': 'Classic', 'next': 'Dark',
    'bg': '#f0f0f0', 'base': '#ffffff', 'text': '#000000',
    'accent': '#0078d4', 'accent_hover': '#005a9e',
    'border': '#cccccc', 'disabled': '#aaaaaa',
    'plot_bg': 'w', 'grid_alpha': 0.2,
    'axis_pen': '#888888', 'axis_text': '#333333',
}
_DARK = {
    'name': 'Dark', 'next': 'Modern',
    'bg': '#1e1e2e', 'base': '#2a2a3c', 'text': '#cdd6f4',
    'accent': '#89b4fa', 'accent_hover': '#74c7ec',
    'border': '#45475a', 'disabled': '#585b70',
    'plot_bg': '#1e1e2e', 'grid_alpha': 0.15,
    'axis_pen': '#585b70', 'axis_text': '#a6adc8',
}
_THEMES = {'Modern': _MODERN_LIGHT, 'Classic': _CLASSIC_LIGHT, 'Dark': _DARK}

def apply_theme(app, theme: dict):
    app.setStyle("Fusion")
    p = QtGui.QPalette()
    p.setColor(QtGui.QPalette.Window, QtGui.QColor(theme['bg']))
    p.setColor(QtGui.QPalette.Base, QtGui.QColor(theme['base']))
    p.setColor(QtGui.QPalette.Text, QtGui.QColor(theme['text']))
    p.setColor(QtGui.QPalette.WindowText, QtGui.QColor(theme['text']))
    p.setColor(QtGui.QPalette.Button, QtGui.QColor(theme['accent']))
    p.setColor(QtGui.QPalette.ButtonText, QtGui.QColor('#ffffff'))
    p.setColor(QtGui.QPalette.Highlight, QtGui.QColor(theme['accent']))
    p.setColor(QtGui.QPalette.HighlightedText, QtGui.QColor('#ffffff'))
    app.setPalette(p)
    app.setStyleSheet(f"""
        QWidget {{ background-color: {theme['bg']}; font-family: 'Segoe UI', sans-serif;
                   font-size: 10pt; color: {theme['text']}; }}
        QLineEdit, QComboBox {{ background-color: {theme['base']}; border: 1px solid {theme['border']};
            border-radius: 6px; padding: 3px 6px; color: {theme['text']}; }}
        QPushButton {{ background-color: {theme['accent']}; color: #ffffff; border-radius: 6px;
            padding: 5px 12px; border: none; font-weight: 500; }}
        QPushButton:hover {{ background-color: {theme['accent_hover']}; }}
        QPushButton:disabled {{ background-color: {theme['disabled']}; }}
        QCheckBox {{ spacing: 4px; }}
        QStatusBar {{ background-color: {theme['base']}; border-top: 1px solid {theme['border']}; }}
        QGroupBox {{ border: 1px solid {theme['border']}; border-radius: 8px;
            margin-top: 6px; background-color: {theme['base']}; }}
        QGroupBox::title {{ subcontrol-origin: margin; padding: 2px 8px; }}
        QSlider::groove:horizontal {{ height: 4px; background: {theme['border']}; border-radius: 2px; }}
        QSlider::handle:horizontal {{ width: 14px; margin: -5px 0; background: {theme['accent']};
            border-radius: 7px; }}
        QLabel#latestVal {{ font-family: 'Consolas', 'Courier New', monospace; font-size: 9pt; }}
    """)


# ============================================================================
# Main Window
# ============================================================================

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PhAI V2.1 — USB-CDC Real-time Receiver")
        self.resize(1500, 950)

        self._theme = _MODERN_LIGHT

        # Data state
        self._window_size = DEFAULT_WINDOW_SAMPLES
        self._buf_a = np.full((self._window_size, 1 + MAX_CHANNELS), np.nan, dtype=np.float32)
        self._buf_b = np.full((self._window_size, 1 + MAX_CHANNELS), np.nan, dtype=np.float32)
        self._active_buf = self._buf_a
        self._write_idx = 0
        self._total_recv = 0
        self._n_channels = 0
        self._ch_names = []
        self._ch_visible = []
        self._plot_groups = []
        self._module_name = ""
        self._module_id = -1

        self._last_seq = -1
        self._seq_drops = 0
        self._frozen = False

        # Throughput tracking
        self._tp_count = 0
        self._tp_time = time.perf_counter()
        self._pkt_rate = 0.0
        self._byte_rate = 0.0
        self._tp_bytes = 0

        # Serial
        self._serial_thread = None
        self._worker = None
        self._reconnect_timer = None
        self._last_port = None

        # CSV
        self._log_file = None
        self._pending = []
        self._written = 0
        self._last_log_path = None

        self._plot_widgets = []
        self._plot_curves = []   # [(ch_idx, curve), ...]
        self._crosshairs = []

        self._build_ui()
        self._refresh_ports()

        # Poll timer — drains worker queue + updates plots
        self._poll_timer = QtCore.QTimer(self)
        self._poll_timer.timeout.connect(self._on_poll)
        self._poll_timer.start(30)

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        root = QtWidgets.QVBoxLayout(central)
        root.setContentsMargins(6, 6, 6, 6)
        root.setSpacing(4)

        # ---- Row 1: connection controls
        r1 = QtWidgets.QHBoxLayout()
        root.addLayout(r1)

        r1.addWidget(QtWidgets.QLabel("Port:"))
        self._combo_port = QtWidgets.QComboBox()
        self._combo_port.setMinimumWidth(180)
        r1.addWidget(self._combo_port)
        btn = QtWidgets.QPushButton("Refresh")
        btn.clicked.connect(self._refresh_ports)
        r1.addWidget(btn)

        self._btn_conn = QtWidgets.QPushButton("Connect")
        self._btn_conn.clicked.connect(self._on_connect)
        r1.addWidget(self._btn_conn)
        self._btn_disc = QtWidgets.QPushButton("Disconnect")
        self._btn_disc.clicked.connect(self._on_disconnect)
        self._btn_disc.setEnabled(False)
        r1.addWidget(self._btn_disc)

        r1.addSpacing(10)
        self._btn_freeze = QtWidgets.QPushButton("Freeze")
        self._btn_freeze.setCheckable(True)
        self._btn_freeze.toggled.connect(self._on_freeze_toggled)
        r1.addWidget(self._btn_freeze)

        r1.addStretch()

        r1.addWidget(QtWidgets.QLabel("Output:"))
        self._edit_folder = QtWidgets.QLineEdit(os.path.abspath("data"))
        self._edit_folder.setMinimumWidth(180)
        r1.addWidget(self._edit_folder)
        bbr = QtWidgets.QPushButton("...")
        bbr.setFixedWidth(30)
        bbr.clicked.connect(self._on_browse)
        r1.addWidget(bbr)

        r1.addSpacing(10)
        self._btn_screenshot = QtWidgets.QPushButton("Screenshot")
        self._btn_screenshot.clicked.connect(self._on_screenshot)
        r1.addWidget(self._btn_screenshot)
        self._btn_theme = QtWidgets.QPushButton("Classic")
        self._btn_theme.clicked.connect(self._toggle_theme)
        r1.addWidget(self._btn_theme)

        # ---- Row 2: info bar
        r2 = QtWidgets.QHBoxLayout()
        root.addLayout(r2)
        bold = QtGui.QFont()
        bold.setBold(True)

        self._lbl_module = QtWidgets.QLabel("Module: —")
        self._lbl_module.setFont(bold)
        r2.addWidget(self._lbl_module)
        r2.addSpacing(12)
        self._lbl_stats = QtWidgets.QLabel("Idle")
        self._lbl_stats.setFont(bold)
        r2.addWidget(self._lbl_stats)
        r2.addStretch()

        r2.addWidget(QtWidgets.QLabel("Window:"))
        self._slider_win = QtWidgets.QSlider(Qt.Horizontal)
        self._slider_win.setRange(500, 10000)
        self._slider_win.setValue(self._window_size)
        self._slider_win.setFixedWidth(140)
        self._slider_win.valueChanged.connect(self._on_window_changed)
        r2.addWidget(self._slider_win)
        self._lbl_win = QtWidgets.QLabel(f"{self._window_size}")
        self._lbl_win.setFixedWidth(45)
        r2.addWidget(self._lbl_win)

        self._btn_review = QtWidgets.QPushButton("Review CSV")
        self._btn_review.clicked.connect(self._open_review)
        r2.addWidget(self._btn_review)

        # ---- Body: left sidebar + plot grid
        body = QtWidgets.QHBoxLayout()
        root.addLayout(body, stretch=1)

        # Left sidebar: channel checkboxes + latest values
        sidebar_w = QtWidgets.QWidget()
        sidebar_w.setFixedWidth(200)
        self._sidebar_layout = QtWidgets.QVBoxLayout(sidebar_w)
        self._sidebar_layout.setContentsMargins(2, 2, 2, 2)
        self._sidebar_layout.setSpacing(1)

        lbl = QtWidgets.QLabel("Channels")
        lbl.setFont(bold)
        self._sidebar_layout.addWidget(lbl)

        self._ch_container = QtWidgets.QVBoxLayout()
        self._sidebar_layout.addLayout(self._ch_container)

        hb = QtWidgets.QHBoxLayout()
        self._sidebar_layout.addLayout(hb)
        bsa = QtWidgets.QPushButton("All")
        bsa.clicked.connect(lambda: self._set_all_ch(True))
        hb.addWidget(bsa)
        bsn = QtWidgets.QPushButton("None")
        bsn.clicked.connect(lambda: self._set_all_ch(False))
        hb.addWidget(bsn)

        self._sidebar_layout.addStretch()

        scroll = QtWidgets.QScrollArea()
        scroll.setWidget(sidebar_w)
        scroll.setWidgetResizable(True)
        scroll.setFixedWidth(220)
        body.addWidget(scroll)

        # Plot grid
        self._grid = QtWidgets.QGridLayout()
        self._grid.setSpacing(4)
        body.addLayout(self._grid, stretch=1)

        for i in range(6):
            pw = pg.PlotWidget(useOpenGL=True)
            pw.showGrid(x=True, y=True, alpha=0.15)
            pw.setClipToView(True)
            pw.setLimits(xMin=0)
            pw.setLabel("bottom", "device time (s)  [1 tick = 1 ms]")
            pw.setLabel("left", "Value")
            pw.setTitle(f"Plot {i + 1}")

            # Crosshair
            vline = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkPen('#ef4444', width=1, style=Qt.DashLine))
            hline = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('#ef4444', width=1, style=Qt.DashLine))
            vline.setVisible(False)
            hline.setVisible(False)
            pw.addItem(vline, ignoreBounds=True)
            pw.addItem(hline, ignoreBounds=True)
            self._crosshairs.append((vline, hline))

            proxy = pg.SignalProxy(pw.scene().sigMouseMoved, rateLimit=30, slot=lambda evt, idx=i: self._on_mouse_moved(evt, idx))
            pw.__mouse_proxy = proxy

            self._plot_widgets.append(pw)
            self._plot_curves.append([])
            r, c = i // 3, i % 3
            self._grid.addWidget(pw, r, c)

        # Link x-axes to plot 0
        for i in range(1, 6):
            self._plot_widgets[i].setXLink(self._plot_widgets[0])

        self._status_bar = self.statusBar()
        self._status_bar.showMessage("Ready — PhAI V2.1 Protocol")

        self._apply_plot_theme()

    def _apply_plot_theme(self):
        t = self._theme
        for pw in self._plot_widgets:
            pw.setBackground(t['plot_bg'])
            for axis_name in ('bottom', 'left'):
                pw.getAxis(axis_name).setPen(pg.mkPen(t['axis_pen']))
                pw.getAxis(axis_name).setTextPen(t['axis_text'])

    # ------------------------------------------------------------------ Channel sidebar
    def _rebuild_sidebar(self, ch_names):
        while self._ch_container.count():
            item = self._ch_container.takeAt(0)
            w = item.widget()
            if w:
                w.deleteLater()

        self._ch_checks = []
        self._ch_val_labels = []
        for i, name in enumerate(ch_names):
            row = QtWidgets.QHBoxLayout()
            cb = QtWidgets.QCheckBox(name)
            cb.setChecked(True)
            cb.stateChanged.connect(self._on_ch_toggled)
            row.addWidget(cb)
            lbl = QtWidgets.QLabel("—")
            lbl.setObjectName("latestVal")
            lbl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            lbl.setFixedWidth(70)
            row.addWidget(lbl)
            container = QtWidgets.QWidget()
            container.setLayout(row)
            self._ch_container.addWidget(container)
            self._ch_checks.append(cb)
            self._ch_val_labels.append(lbl)
        self._ch_visible = [True] * len(ch_names)

    def _set_all_ch(self, val):
        for cb in self._ch_checks:
            cb.setChecked(val)

    def _on_ch_toggled(self, _state):
        self._ch_visible = [cb.isChecked() for cb in self._ch_checks]
        self._rebuild_curves()

    # ------------------------------------------------------------------ Plot curves
    def _setup_plots(self, ch_names, module_id):
        self._n_channels = len(ch_names)
        self._ch_names = ch_names
        self._ch_visible = [True] * self._n_channels
        self._plot_groups = build_plot_groups(module_id, ch_names)
        self._rebuild_sidebar(ch_names)
        self._rebuild_curves()

    def _rebuild_curves(self):
        for p_idx in range(6):
            pw = self._plot_widgets[p_idx]
            for _, curve in self._plot_curves[p_idx]:
                pw.removeItem(curve)
            self._plot_curves[p_idx] = []

        for p_idx, (gname, ch_indices) in enumerate(self._plot_groups):
            if p_idx >= 6:
                break
            pw = self._plot_widgets[p_idx]
            visible_chs = [ci for ci in ch_indices if ci < self._n_channels and self._ch_visible[ci]]
            title = gname
            if visible_chs:
                preview = ", ".join(self._ch_names[ci] for ci in visible_chs[:4])
                if len(visible_chs) > 4:
                    preview += " ..."
                title += f"  [{preview}]"
            pw.setTitle(title)

            hues = max(8, len(ch_indices))
            color_map = {ci: k for k, ci in enumerate(ch_indices)}
            for ci in visible_chs:
                color = pg.intColor(color_map[ci], hues=hues)
                pen = pg.mkPen(color, width=1)
                curve = pw.plot([], [], pen=pen, name=self._ch_names[ci], skipFiniteCheck=True)
                self._plot_curves[p_idx].append((ci, curve))

        for p_idx in range(len(self._plot_groups), 6):
            self._plot_widgets[p_idx].setTitle(f"Plot {p_idx + 1} (unused)")

    # ------------------------------------------------------------------ Connection
    def _refresh_ports(self):
        self._combo_port.clear()
        for p in serial.tools.list_ports.comports():
            self._combo_port.addItem(f"{p.device} — {p.description}")
        if self._combo_port.count() == 0:
            self._combo_port.addItem("(no ports)")

    def _on_connect(self):
        text = self._combo_port.currentText()
        if not text or text.startswith("(no"):
            return
        port = text.split(" — ")[0].strip()
        self._start_connection(port)

    def _start_connection(self, port):
        try:
            self._open_log_file()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))
            return

        self._reset_state()
        self._last_port = port

        self._serial_thread = QtCore.QThread()
        self._worker = PhAISerialWorker(port)
        self._worker.moveToThread(self._serial_thread)
        self._serial_thread.started.connect(self._worker.run)
        self._worker.status_msg.connect(lambda m: self._status_bar.showMessage(m))
        self._worker.finished.connect(self._on_worker_finished)
        self._worker.connection_failed.connect(self._on_conn_failed)
        self._worker.port_lost.connect(self._on_port_lost)
        self._serial_thread.start()

        self._btn_conn.setEnabled(False)
        self._btn_disc.setEnabled(True)
        self._status_bar.showMessage(f"Connecting to {port} ...")

    def _on_disconnect(self):
        self._stop_reconnect()
        if self._worker:
            self._worker.stop()

    def _on_conn_failed(self, msg):
        self._close_log_file()
        self._btn_conn.setEnabled(True)
        self._btn_disc.setEnabled(False)
        QtWidgets.QMessageBox.critical(self, "Connection Failed", msg)

    def _on_worker_finished(self):
        if self._serial_thread:
            self._serial_thread.quit()
            self._serial_thread.wait()
            self._serial_thread = None
        self._worker = None
        self._close_log_file()
        self._btn_conn.setEnabled(True)
        self._btn_disc.setEnabled(False)
        self._status_bar.showMessage(
            f"Disconnected — {self._total_recv} packets, {self._written} lines saved")

    # Auto-reconnect
    def _on_port_lost(self):
        self._status_bar.showMessage("Port lost — attempting reconnect...")
        if not self._reconnect_timer:
            self._reconnect_timer = QtCore.QTimer(self)
            self._reconnect_timer.timeout.connect(self._try_reconnect)
        self._reconnect_timer.start(2000)

    def _try_reconnect(self):
        if self._last_port is None:
            return
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if self._last_port in ports:
            self._stop_reconnect()
            self._status_bar.showMessage(f"Reconnecting to {self._last_port}...")
            self._start_connection(self._last_port)

    def _stop_reconnect(self):
        if self._reconnect_timer:
            self._reconnect_timer.stop()

    def _reset_state(self):
        self._window_size = self._slider_win.value()
        self._buf_a = np.full((self._window_size, 1 + MAX_CHANNELS), np.nan, dtype=np.float32)
        self._buf_b = np.full((self._window_size, 1 + MAX_CHANNELS), np.nan, dtype=np.float32)
        self._active_buf = self._buf_a
        self._write_idx = 0
        self._total_recv = 0
        self._last_seq = -1
        self._seq_drops = 0
        self._device_time_s = 0.0
        self._tp_count = 0
        self._tp_bytes = 0
        self._tp_time = time.perf_counter()
        self._module_id = -1

    # ------------------------------------------------------------------ Freeze / Theme
    def _on_freeze_toggled(self, checked):
        self._frozen = checked
        self._btn_freeze.setText("Unfreeze" if checked else "Freeze")

    def _toggle_theme(self):
        next_name = self._theme.get('next', 'Modern')
        self._theme = _THEMES[next_name]
        self._btn_theme.setText(self._theme['next'])
        apply_theme(QtWidgets.QApplication.instance(), self._theme)
        self._apply_plot_theme()

    # ------------------------------------------------------------------ Window size
    def _on_window_changed(self, val):
        self._window_size = val
        self._lbl_win.setText(str(val))

    # ------------------------------------------------------------------ Screenshot
    def _on_screenshot(self):
        folder = self._edit_folder.text().strip() or "."
        os.makedirs(folder, exist_ok=True)
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        for i, pw in enumerate(self._plot_widgets):
            if self._plot_curves[i]:
                path = os.path.join(folder, f"plot_{i}_{ts}.png")
                try:
                    exporter = pg.exporters.ImageExporter(pw.plotItem)
                    exporter.parameters()['width'] = 1920
                    exporter.export(path)
                except Exception:
                    pass
        self._status_bar.showMessage(f"Screenshots saved to {folder}")

    # ------------------------------------------------------------------ Browse
    def _on_browse(self):
        f = QtWidgets.QFileDialog.getExistingDirectory(self, "Output", self._edit_folder.text())
        if f:
            self._edit_folder.setText(f)

    # ------------------------------------------------------------------ CSV
    def _open_log_file(self):
        folder = self._edit_folder.text().strip() or os.path.abspath("data")
        os.makedirs(folder, exist_ok=True)
        path = os.path.join(folder, f"cdc_phai_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
        self._log_file = open(path, 'w', encoding='utf-8', newline='')
        self._pending = []
        self._written = 0
        self._last_log_path = path

    def _write_csv_header(self, names):
        if self._log_file:
            self._log_file.write("time_s,pc_time_s,seq_id,module_id,tx_drops," + ",".join(names) + "\n")

    def _flush_csv(self):
        if self._log_file and self._pending:
            try:
                self._log_file.writelines(self._pending)
            except Exception:
                pass
            self._pending.clear()

    def _close_log_file(self):
        if self._log_file:
            self._flush_csv()
            try:
                self._log_file.flush()
                self._log_file.close()
            except Exception:
                pass
            self._log_file = None

    # ------------------------------------------------------------------ Mouse crosshair
    def _on_mouse_moved(self, evt, plot_idx):
        pos = evt[0]
        pw = self._plot_widgets[plot_idx]
        vb = pw.plotItem.vb
        if pw.sceneBoundingRect().contains(pos):
            mp = vb.mapSceneToView(pos)
            vline, hline = self._crosshairs[plot_idx]
            vline.setPos(mp.x())
            hline.setPos(mp.y())
            vline.setVisible(True)
            hline.setVisible(True)
        else:
            vline, hline = self._crosshairs[plot_idx]
            vline.setVisible(False)
            hline.setVisible(False)

    # ------------------------------------------------------------------ Poll (batch drain + render)
    def _on_poll(self):
        w = self._worker
        if w is None:
            return

        # Drain queue
        batch = []
        try:
            while True:
                batch.append(w.packet_queue.popleft())
        except IndexError:
            pass

        if not batch:
            self._update_stats_label(w)
            return

        # First packet → setup
        if self._total_recv == 0:
            first = batch[0]
            self._module_id = first.module_id
            self._module_name = get_module_name(first.module_id)
            ch = get_channel_names(first.module_id, len(first.floats))
            self._setup_plots(ch, first.module_id)
            self._write_csv_header(ch)
            self._lbl_module.setText(
                f"Module: {self._module_name} (0x{first.module_id:02X}) — {len(first.floats)} ch")

        buf = self._active_buf
        ws = buf.shape[0]
        n_ch = self._n_channels

        for pkt in batch:
            # SEQ gap → device time reconstruction (1ms per seq tick)
            if self._last_seq >= 0:
                delta = (pkt.seq_id - self._last_seq) & 0xFFFF
                if delta > 1000:
                    delta = 1
                if delta != 1:
                    self._seq_drops += delta - 1
                self._device_time_s += delta * (DEVICE_PERIOD_MS / 1000.0)
            self._last_seq = pkt.seq_id

            # Write to rolling buffer — x-axis = device time (smooth, gap-aware)
            idx = self._write_idx % ws
            buf[idx, 0] = self._device_time_s
            n = min(len(pkt.floats), MAX_CHANNELS)
            buf[idx, 1:1 + n] = pkt.floats[:n]
            self._write_idx += 1
            self._total_recv += 1

            # CSV — device_time (smooth) + pc_time (absolute)
            if self._log_file:
                vals = ",".join(f"{v:.6f}" for v in pkt.floats)
                self._pending.append(
                    f"{self._device_time_s:.6f},{pkt.recv_t:.6f},{pkt.seq_id},{pkt.module_id},{pkt.tx_drops},{vals}\n")
                self._written += 1

        self._tp_count += len(batch)
        self._tp_bytes += sum(PHAI_HEADER_SIZE + len(p.floats) * 4 + PHAI_CRC_SIZE for p in batch)

        if len(self._pending) >= FLUSH_EVERY:
            self._flush_csv()

        # Update latest value labels
        last_pkt = batch[-1]
        for i, lbl in enumerate(self._ch_val_labels):
            if i < len(last_pkt.floats):
                lbl.setText(f"{last_pkt.floats[i]:.3f}")

        # Update stats
        self._update_stats_label(w)

        # Update plots (skip if frozen)
        if not self._frozen:
            self._render_plots()

    def _update_stats_label(self, w):
        now = time.perf_counter()
        dt = now - self._tp_time
        if dt >= 1.0:
            self._pkt_rate = self._tp_count / dt
            self._byte_rate = self._tp_bytes / dt
            self._tp_count = 0
            self._tp_bytes = 0
            self._tp_time = now

        self._lbl_stats.setText(
            f"Good: {w.good}  CRC: {w.crc_err}  Sync: {w.sync_err}  "
            f"SEQ↓: {self._seq_drops}  TxDrop: {w.total_tx_drops}  |  "
            f"{self._pkt_rate:.0f} pkt/s  {self._byte_rate / 1024:.1f} KB/s")

    # ------------------------------------------------------------------ Render (zero-copy)
    def _render_plots(self):
        buf = self._active_buf
        ws = buf.shape[0]
        total = self._write_idx
        n = min(total, ws)

        if n == 0:
            return

        if total <= ws:
            data = buf[:n]
        else:
            start = total % ws
            # Zero-copy: use np.roll on axis=0 indices instead of concatenate
            indices = np.arange(start, start + n) % ws
            data = buf[indices]

        x = data[:, 0]

        for p_idx, curves in enumerate(self._plot_curves):
            if not curves:
                continue
            pw = self._plot_widgets[p_idx]
            pw.setUpdatesEnabled(False)
            for ch_idx, curve in curves:
                curve.setData(x, data[:, 1 + ch_idx], connect='finite', skipFiniteCheck=True)
            if len(x) > 0:
                pw.setXRange(float(x[0]), float(x[-1]), padding=0)
            pw.setUpdatesEnabled(True)

    # ------------------------------------------------------------------ Review
    def _open_review(self):
        path = self._last_log_path
        if not path or not os.path.isfile(path):
            folder = self._edit_folder.text().strip() or "."
            files = sorted(glob.glob(os.path.join(folder, "cdc_phai_*.csv")),
                           key=os.path.getmtime, reverse=True)
            path = files[0] if files else None
        if not path:
            QtWidgets.QMessageBox.information(self, "Info", "No CSV found.")
            return

        # Try launching the full-featured standalone reviewer first
        reviewer_script = os.path.join(os.path.dirname(__file__), "cdc_csv_reviewer.py")
        if os.path.isfile(reviewer_script):
            import subprocess
            subprocess.Popen([sys.executable, reviewer_script, path])
        else:
            dlg = CsvReviewDialog(path, self)
            dlg.setModal(False)
            dlg.show()

    # ------------------------------------------------------------------ Close
    def closeEvent(self, event):
        self._stop_reconnect()
        if self._worker:
            self._worker.stop()
        if self._serial_thread:
            self._serial_thread.quit()
            self._serial_thread.wait()
        self._close_log_file()
        event.accept()


# ============================================================================
# CSV Review Dialog
# ============================================================================

class CsvReviewDialog(QtWidgets.QDialog):
    def __init__(self, csv_path=None, parent=None):
        super().__init__(parent)
        flags = self.windowFlags()
        flags &= ~Qt.WindowContextHelpButtonHint
        flags |= Qt.WindowMaximizeButtonHint
        self.setWindowFlags(flags)
        self.setWindowTitle(f"CSV Review — {os.path.basename(csv_path)}" if csv_path else "CSV Review")
        self.resize(1400, 800)
        self.csv_cols = None
        self.data = None
        layout = QtWidgets.QVBoxLayout(self)
        top = QtWidgets.QHBoxLayout()
        layout.addLayout(top)
        top.addWidget(QtWidgets.QLabel("CSV:"))
        self.lbl_path = QtWidgets.QLabel("—")
        self.lbl_path.setTextInteractionFlags(Qt.TextSelectableByMouse)
        top.addWidget(self.lbl_path, stretch=1)
        btn = QtWidgets.QPushButton("Open...")
        btn.clicked.connect(self._on_open)
        top.addWidget(btn)
        self.info = QtWidgets.QLabel("")
        layout.addWidget(self.info)
        self.grid = QtWidgets.QGridLayout()
        layout.addLayout(self.grid, stretch=1)
        self.plots = []
        if csv_path:
            self.load(csv_path)

    def _on_open(self):
        f, _ = QtWidgets.QFileDialog.getOpenFileName(self, "CSV", "", "CSV (*.csv);;All (*)")
        if f:
            self.load(f)

    def load(self, path):
        try:
            with open(path, 'r') as f:
                self.csv_cols = f.readline().strip().split(',')
            arr = np.genfromtxt(path, delimiter=",", skip_header=1)
            if arr.ndim == 1:
                arr = arr.reshape(1, -1)
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))
            return
        self.data = arr
        self.lbl_path.setText(path)
        self.info.setText(f"{arr.shape[0]} rows × {arr.shape[1]} cols")
        for pw in self.plots:
            self.grid.removeWidget(pw)
            pw.deleteLater()
        self.plots.clear()
        skip_cols = {'time_s', 'seq_id', 'module_id', 'tx_drops'}
        data_cols = [(i, n) for i, n in enumerate(self.csv_cols) if n not in skip_cols]
        if not data_cols:
            return
        time_idx = 0
        x = arr[:, time_idx] - arr[0, time_idx]
        n = len(data_cols)
        per = max(1, (n + 5) // 6)
        for p in range(min(6, (n + per - 1) // per)):
            pw = pg.PlotWidget(useOpenGL=True)
            pw.setBackground('w')
            pw.showGrid(x=True, y=True, alpha=0.15)
            s, e = p * per, min((p + 1) * per, n)
            hues = max(8, e - s)
            for k, (ci, cn) in enumerate(data_cols[s:e]):
                pw.plot(x, arr[:, ci], pen=pg.intColor(k, hues=hues), name=cn)
            pw.addLegend()
            pw.setLabel("bottom", self.csv_cols[time_idx])
            self.plots.append(pw)
            self.grid.addWidget(pw, p // 2, p % 2)


# ============================================================================
# CLI Mode
# ============================================================================

def run_cli(port, baud, output):
    os.makedirs(output, exist_ok=True)
    path = os.path.join(output, f"cdc_phai_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
    print(f"[PhAI V2.2 CLI]  Port: {port}  Output: {path}")
    print("  Ctrl+C to stop.\n")
    try:
        ser = serial.Serial(port, baud, timeout=DEFAULT_TIMEOUT)
    except Exception as e:
        print(f"[ERROR] {e}")
        return
    wire_buf = bytearray()
    good = 0
    errs = 0
    hdr_written = False
    t0 = time.perf_counter()
    last_print = t0
    last_seq = -1
    dev_time = 0.0
    try:
        with open(path, 'w') as fout:
            while True:
                chunk = ser.read(2048)
                if not chunk:
                    continue
                wire_buf.extend(chunk)
                now = time.perf_counter() - t0
                while True:
                    delim = wire_buf.find(b'\x00')
                    if delim < 0:
                        break
                    if delim > 0:
                        frame = cobs_decode(bytes(wire_buf[:delim]))
                        min_sz = PHAI_HEADER_SIZE + PHAI_CRC_SIZE
                        if len(frame) >= min_sz and frame[0] == PHAI_SOF:
                            lu = frame[1]
                            tot = PHAI_HEADER_SIZE + lu * 4 + PHAI_CRC_SIZE
                            if 0 < lu <= PHAI_MAX_LEN_UNITS and len(frame) >= tot:
                                crc_r = frame[tot - 2] | (frame[tot - 1] << 8)
                                crc_c = crc16_ccitt(frame[:tot - PHAI_CRC_SIZE])
                                if crc_c != crc_r:
                                    errs += 1
                                else:
                                    seq = frame[2] | (frame[3] << 8)
                                    mid = frame[4]
                                    st = frame[5]
                                    if last_seq >= 0:
                                        delta = (seq - last_seq) & 0xFFFF
                                        if delta > 1000:
                                            delta = 1
                                        dev_time += delta * (DEVICE_PERIOD_MS / 1000.0)
                                    last_seq = seq
                                    floats = struct.unpack(f'<{lu}f', frame[PHAI_HEADER_SIZE:PHAI_HEADER_SIZE+lu*4])
                                    if not hdr_written:
                                        ch = get_channel_names(mid, lu)
                                        fout.write("time_s,pc_time_s,seq_id,module_id,tx_drops," + ",".join(ch) + "\n")
                                        hdr_written = True
                                        print(f"  Module: {get_module_name(mid)} — {lu} ch")
                                    vals = ",".join(f"{v:.6f}" for v in floats)
                                    fout.write(f"{dev_time:.6f},{now:.6f},{seq},{mid},{st & 0x7F},{vals}\n")
                                    good += 1
                                    if good % FLUSH_EVERY == 0:
                                        fout.flush()
                            else:
                                errs += 1
                        else:
                            errs += 1
                    del wire_buf[:delim + 1]
                t = time.perf_counter()
                if t - last_print >= 2.0:
                    print(f"  Good: {good}  Err: {errs}")
                    last_print = t
    except KeyboardInterrupt:
        print(f"\n[DONE] {good} packets → {path}")
    finally:
        ser.close()


# ============================================================================
# Entry Point
# ============================================================================

def main():
    ap = argparse.ArgumentParser(description="PhAI V2.1 CDC Receiver")
    ap.add_argument("--cli", action="store_true")
    ap.add_argument("--port", type=str)
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--output", type=str, default="data")
    args = ap.parse_args()

    if args.cli:
        if not args.port:
            print("--port required"); sys.exit(1)
        run_cli(args.port, args.baud, args.output)
    else:
        app = QtWidgets.QApplication(sys.argv)
        apply_theme(app, _MODERN_LIGHT)
        pg.setConfigOptions(antialias=False, useOpenGL=True)
        pg.setConfigOption("background", "w")
        pg.setConfigOption("foreground", "#111827")
        win = MainWindow()
        win.showMaximized()
        sys.exit(app.exec_())

if __name__ == "__main__":
    main()
