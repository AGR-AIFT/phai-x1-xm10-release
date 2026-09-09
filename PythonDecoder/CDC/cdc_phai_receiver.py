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

# 프레임 파싱·시퀀스 회계·module 라우팅은 frame_router.py 하나로 모았다.
# GUI 워커와 CLI 가 같은 코드를 쓰게 하려는 것이다 (예전에는 각자 복사본을 갖고 있었다).
from frame_router import (
    parse_phai_frame, cobs_decode, FrameRouter,
    PHAI_MODULE_TOTAL_DATA, PHAI_MODULE_USER_META,
)

# 프로토콜 상수(PHAI_SOF 등)와 CRC16 테이블은 frame_router.py 로 옮겼다.

# MODULE_ID → (name, [channel_names] or None)
# Combined V2.2: 10ch (Accel3 + Gyro3 + MotorAngle2 + MotorTorque2)
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

DEFAULT_BAUD = 921600
DEFAULT_TIMEOUT = 0.02        # 20ms serial read timeout
FLUSH_EVERY = 500
DEFAULT_WINDOW_SAMPLES = 3000
MAX_CHANNELS = 64

# ============================================================================
# Helpers
# ============================================================================

# crc16_ccitt() / cobs_decode() 는 frame_router.py 에 있다 (위에서 import).

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


# 이 자리에 있던 패킷 클래스는 frame_router.PhAIFrame 으로 대체됐다.
# 차이: payload 를 float 로 미리 해석하지 않는다 (0xEF 같은 비-float 프레임 보호).


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
        self.queue_overflow_count = 0   # deque 가 가득 차 조용히 버려진 프레임 수
        # 라우터를 워커가 갖는다 — 시퀀스 회계를 큐에 넣기 '전에' 해야 하기 때문이다.
        # 큐 뒤에서 세면 PC 쪽 처리 지연(큐 오버플로)이 와이어 손실로 둔갑한다.
        self.router = FrameRouter()

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
        """COBS 를 푼 프레임 하나를 검증해 큐에 넣는다.

        검증 로직 자체는 frame_router.parse_phai_frame() 에 있다 — CLI 와 공유한다.
        여기서는 통계 집계와 큐 적재만 한다.
        """
        pkt, err = parse_phai_frame(frame, recv_t)
        if err == 'sync':
            self.sync_err += 1
            return
        if err == 'crc':
            self.crc_err += 1
            return

        self.total_tx_drops += pkt.tx_drops

        # 시퀀스 회계와 module 판정은 큐에 넣기 전에 끝낸다.
        # 바로 아래 오버플로로 버려질 프레임도 '와이어로는 도착한' 프레임이다.
        route_tag, _delta = self.router.route(pkt)

        # deque(maxlen) 은 가득 차면 반대쪽을 말없이 버린다. 버려졌다는 사실을 남긴다.
        if len(self.packet_queue) >= self.packet_queue.maxlen:
            self.queue_overflow_count += 1

        self.packet_queue.append((pkt, route_tag))
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
        self.setWindowTitle("PhAI V2.2 — USB-CDC Real-time Receiver")
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
        self._status_bar.showMessage("Ready — PhAI V2.2 Protocol")

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
        # 포트 유실 후 자동 재연결 타이머가 돌고 있는데 사용자가 Connect 를 다시 누르면
        # 두 경로가 거의 동시에 여기로 들어와 worker/thread 를 덮어쓴다.
        # 먼저 타이머를 멈추고, 남아 있는 이전 연결이 있으면 동기적으로 정리한다.
        self._stop_reconnect()

        if self._worker is not None or self._serial_thread is not None:
            if self._worker is not None:
                self._worker.stop()
            if self._serial_thread is not None:
                self._serial_thread.quit()
                self._serial_thread.wait()
            self._worker = None
            self._serial_thread = None

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
        # 세션 내내 보여준 'Good' 은 파싱 성공한 '전체' 프레임 수인데
        # _total_recv 는 사용자 채널만 센다. 둘 다 적어야 수천 개가 사라진 것처럼 안 보인다.
        total_frames = self._worker.good if self._worker is not None else self._total_recv
        if self._serial_thread:
            self._serial_thread.quit()
            self._serial_thread.wait()
            self._serial_thread = None
        self._worker = None
        self._close_log_file()
        self._btn_conn.setEnabled(True)
        self._btn_disc.setEnabled(False)
        self._status_bar.showMessage(
            f"Disconnected — {total_frames} frames ({self._total_recv} user), "
            f"{self._written} lines saved")

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
        self._total_recv = 0   # 사용자 채널 프레임 수 (시스템 프레임 제외)
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
                self._log_file.flush()   # 크래시 시 마지막 구간이 통째로 날아가지 않게
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

        buf = self._active_buf
        ws = buf.shape[0]
        last_user_pkt = None

        for pkt, route_tag in batch:
            # 라우팅은 워커 스레드에서 이미 끝났다(큐 앞에서 회계해야 하므로).
            # 여기서는 사용자 채널 프레임만 골라 그리고 저장한다.
            if route_tag != 'user_primary':
                # 0x20/0xEF/0xED/0xEE 와 primary 가 아닌 사용자 module.
                # 그래프·CSV 에는 넣지 않는다. 버리는 게 아니라 router 가 따로 센다.
                continue

            # 채널 구성은 첫 '사용자' 프레임으로 정한다.
            # 예전에는 배치의 첫 프레임(대개 0x20)으로 정해서 채널이 어긋났다.
            if self._module_id < 0:
                self._module_id = pkt.module_id
                self._module_name = get_module_name(pkt.module_id)
                floats0 = pkt.as_float32()
                ch = get_channel_names(pkt.module_id, len(floats0))
                self._setup_plots(ch, pkt.module_id)
                self._write_csv_header(ch)
                self._lbl_module.setText(
                    f"Module: {self._module_name} (0x{pkt.module_id:02X}) — {len(floats0)} ch")

            floats = pkt.as_float32()

            # Write to rolling buffer — x-axis = device time (smooth, gap-aware)
            idx = self._write_idx % ws
            buf[idx, 0] = pkt.device_time_s
            n = min(len(floats), MAX_CHANNELS)
            buf[idx, 1:1 + n] = floats[:n]
            self._write_idx += 1
            self._total_recv += 1

            # CSV — device_time (smooth) + pc_time (absolute)
            if self._log_file:
                vals = ",".join(f"{v:.6f}" for v in floats)
                self._pending.append(
                    f"{pkt.device_time_s:.6f},{pkt.recv_t:.6f},"
                    f"{pkt.seq_id},{pkt.module_id},{pkt.tx_drops},{vals}\n")
                self._written += 1

            last_user_pkt = pkt

        self._tp_count += len(batch)
        self._tp_bytes += sum(p.wire_len for p, _tag in batch)

        if len(self._pending) >= FLUSH_EVERY:
            self._flush_csv()

        # 최신값 라벨 — 이 배치의 마지막 '사용자' 프레임 기준.
        # batch[-1] 을 그대로 쓰면 그게 0x20 일 때 라벨에 엉뚱한 값이 찍힌다.
        if last_user_pkt is not None:
            last_floats = last_user_pkt.as_float32()
            for i, lbl in enumerate(self._ch_val_labels):
                if i < len(last_floats):
                    lbl.setText(f"{last_floats[i]:.3f}")

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

        ledger = w.router.ledger
        sys20 = w.router.system_taps[PHAI_MODULE_TOTAL_DATA].frame_count
        sysef = w.router.system_taps[PHAI_MODULE_USER_META].frame_count

        # Other = primary 가 아닌 사용자 module_id. 그래프·CSV 에는 안 들어가므로
        # 여기에 안 보이면 '조용히 사라진' 것과 같다.
        # Resync = seq 가 뒤로 간 횟수. 손실이 아니라 기준을 다시 잡은 횟수다.
        extra = ""
        if w.router.other_user_frames:
            extra += f"  Other:{w.router.other_user_frames}"
        if ledger.resync_count:
            extra += f"  Resync:{ledger.resync_count}"

        self._lbl_stats.setText(
            f"Good: {w.good}  CRC: {w.crc_err}  Sync: {w.sync_err}  "
            f"SEQ↓: {ledger.lost_count}  QOvf: {w.queue_overflow_count}  "
            f"TxDrop: {w.total_tx_drops}  Sys[0x20:{sys20} 0xEF:{sysef}]{extra}  |  "
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
    # GUI 와 똑같은 라우터를 쓴다. 예전에는 CLI 가 파싱을 따로 갖고 있어서
    # 같은 버그(시퀀스 클램프, module 미분리)를 두 번 고쳐야 했다.
    router = FrameRouter()

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
                        raw = cobs_decode(bytes(wire_buf[:delim]))
                        pkt, err = parse_phai_frame(raw, now)
                        if err is not None:
                            errs += 1
                        else:
                            good += 1
                            route_tag, _delta = router.route(pkt)
                            if route_tag == 'user_primary':
                                floats = pkt.as_float32()
                                if not hdr_written:
                                    ch = get_channel_names(pkt.module_id, len(floats))
                                    fout.write("time_s,pc_time_s,seq_id,module_id,tx_drops,"
                                               + ",".join(ch) + "\n")
                                    hdr_written = True
                                    print(f"  Module: {get_module_name(pkt.module_id)} — {len(floats)} ch")
                                vals = ",".join(f"{v:.6f}" for v in floats)
                                fout.write(
                                    f"{pkt.device_time_s:.6f},{now:.6f},{pkt.seq_id},"
                                    f"{pkt.module_id},{pkt.tx_drops},{vals}\n")
                                if good % FLUSH_EVERY == 0:
                                    fout.flush()
                    del wire_buf[:delim + 1]
                t = time.perf_counter()
                if t - last_print >= 2.0:
                    s20 = router.system_taps[PHAI_MODULE_TOTAL_DATA].frame_count
                    sef = router.system_taps[PHAI_MODULE_USER_META].frame_count
                    extra = ""
                    if router.other_user_frames:
                        extra += f"  Other:{router.other_user_frames}"
                    if router.ledger.resync_count:
                        extra += f"  Resync:{router.ledger.resync_count}"
                    print(f"  Good: {good}  Err: {errs}  Lost(global): {router.ledger.lost_count}  "
                          f"Sys[0x20:{s20} 0xEF:{sef}]{extra}")
                    last_print = t
    except KeyboardInterrupt:
        s20 = router.system_taps[PHAI_MODULE_TOTAL_DATA].frame_count
        sef = router.system_taps[PHAI_MODULE_USER_META].frame_count
        print(f"\n[DONE] {good} packets → {path}  "
              f"(Lost(global)={router.ledger.lost_count}, 0x20={s20}, 0xEF={sef})")
        # 시스템 프레임 지문 — 개수만으로는 "보존됐다" 를 증명하지 못한다.
        # 두 캡처의 crc32 를 비교하면 같은 바이트였는지 바로 알 수 있다.
        sys_lines = [tap.summary() for tap in router.system_taps.values() if tap.frame_count]
        if sys_lines:
            print("  system frames:")
            for line in sys_lines:
                print(f"    {line}")
    finally:
        ser.close()


# ============================================================================
# Entry Point
# ============================================================================

def main():
    ap = argparse.ArgumentParser(description="PhAI V2.2 CDC Receiver")
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
