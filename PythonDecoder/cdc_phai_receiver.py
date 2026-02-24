"""
PhAI V2 Protocol — USB-CDC Real-time Receiver & Logger
=======================================================
XM10 보드의 PhAI V2 바이너리 프로토콜을 실시간으로 수신하여
그래프로 시각화하고 CSV로 저장하는 GUI 도구입니다.

패킷 구조 (Little-Endian):
    [SOF:0xAA] [LEN:1] [SEQ_ID:2 LE] [MODULE_ID:1] [PAYLOAD: LEN×4] [CRC8:1]
    - LEN: payload의 4-byte 단위 개수 (0~255)
    - CRC8: polynomial 0x07, init 0x00, bytes[1]~bytes[total-2]

Module IDs:
    0x01  IMU Accel       0x02  IMU Gyro        0x03  IMU Quat
    0x04  GRF Left        0x05  GRF Right       0x06  Motor Left
    0x07  Motor Right     0x10  Combined (12ch)  0xF0~0xFE  User Custom
    0xFF  Debug

사용법:
    pip install pyserial pyqt5 pyqtgraph numpy
    python cdc_phai_receiver.py

    또는 CLI 모드:
    python cdc_phai_receiver.py --cli --port COM6

Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
"""

import sys
import os
import glob
import struct
import time
import argparse
import serial
import serial.tools.list_ports
from datetime import datetime

from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import pyqtSignal, pyqtSlot, Qt

import pyqtgraph as pg
import numpy as np

# ===================== 0) Protocol & Configuration =====================

PHAI_SOF = 0xAA
PHAI_HEADER_SIZE = 5   # SOF(1) + LEN(1) + SEQ_ID(2) + MODULE_ID(1)
PHAI_CRC_SIZE = 1
PHAI_MAX_LEN_UNITS = 255

CRC8_TABLE = [
    0x00,0x07,0x0E,0x09,0x1C,0x1B,0x12,0x15,0x38,0x3F,0x36,0x31,0x24,0x23,0x2A,0x2D,
    0x70,0x77,0x7E,0x79,0x6C,0x6B,0x62,0x65,0x48,0x4F,0x46,0x41,0x54,0x53,0x5A,0x5D,
    0xE0,0xE7,0xEE,0xE9,0xFC,0xFB,0xF2,0xF5,0xD8,0xDF,0xD6,0xD1,0xC4,0xC3,0xCA,0xCD,
    0x90,0x97,0x9E,0x99,0x8C,0x8B,0x82,0x85,0xA8,0xAF,0xA6,0xA1,0xB4,0xB3,0xBA,0xBD,
    0xC7,0xC0,0xC9,0xCE,0xDB,0xDC,0xD5,0xD2,0xFF,0xF8,0xF1,0xF6,0xE3,0xE4,0xED,0xEA,
    0xB7,0xB0,0xB9,0xBE,0xAB,0xAC,0xA5,0xA2,0x8F,0x88,0x81,0x86,0x93,0x94,0x9D,0x9A,
    0x27,0x20,0x29,0x2E,0x3B,0x3C,0x35,0x32,0x1F,0x18,0x11,0x16,0x03,0x04,0x0D,0x0A,
    0x57,0x50,0x59,0x5E,0x4B,0x4C,0x45,0x42,0x6F,0x68,0x61,0x66,0x73,0x74,0x7D,0x7A,
    0x89,0x8E,0x87,0x80,0x95,0x92,0x9B,0x9C,0xB1,0xB6,0xBF,0xB8,0xAD,0xAA,0xA3,0xA4,
    0xF9,0xFE,0xF7,0xF0,0xE5,0xE2,0xEB,0xEC,0xC1,0xC6,0xCF,0xC8,0xDD,0xDA,0xD3,0xD4,
    0x69,0x6E,0x67,0x60,0x75,0x72,0x7B,0x7C,0x51,0x56,0x5F,0x58,0x4D,0x4A,0x43,0x44,
    0x19,0x1E,0x17,0x10,0x05,0x02,0x0B,0x0C,0x21,0x26,0x2F,0x28,0x3D,0x3A,0x33,0x34,
    0x4E,0x49,0x40,0x47,0x52,0x55,0x5C,0x5B,0x76,0x71,0x78,0x7F,0x6A,0x6D,0x64,0x63,
    0x3E,0x39,0x30,0x37,0x22,0x25,0x2C,0x2B,0x06,0x01,0x08,0x0F,0x1A,0x1D,0x14,0x13,
    0xAE,0xA9,0xA0,0xA7,0xB2,0xB5,0xBC,0xBB,0x96,0x91,0x98,0x9F,0x8A,0x8D,0x84,0x83,
    0xDE,0xD9,0xD0,0xD7,0xC2,0xC5,0xCC,0xCB,0xE6,0xE1,0xE8,0xEF,0xFA,0xFD,0xF4,0xF3,
]

# Module ID → (name, channel_names or None for dynamic)
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
        "GRF_LX", "GRF_LY", "GRF_LZ",
        "GRF_RX", "GRF_RY", "GRF_RZ",
    ]),
    0xFF: ("Debug", None),
}

UPDATE_INTERVAL_MS = 30
WINDOW_SIZE_SAMPLES = 2000
FLUSH_EVERY = 500

DEFAULT_BAUD = 921600
DEFAULT_TIMEOUT = 0.05


def crc8_phai(data: bytes) -> int:
    crc = 0x00
    for b in data:
        crc = CRC8_TABLE[crc ^ b]
    return crc


def get_module_name(module_id: int) -> str:
    if module_id in MODULE_DEFS:
        return MODULE_DEFS[module_id][0]
    if 0xF0 <= module_id <= 0xFE:
        return f"User_0x{module_id:02X}"
    return f"Unknown_0x{module_id:02X}"


def get_channel_names(module_id: int, num_floats: int) -> list:
    if module_id in MODULE_DEFS:
        names = MODULE_DEFS[module_id][1]
        if names is not None:
            if len(names) == num_floats:
                return names
    return [f"ch{i}" for i in range(num_floats)]


# ===================== 1) PhAI V2 Packet Parser =====================

class PhAIPacket:
    """Decoded PhAI V2 packet."""
    __slots__ = ('seq_id', 'module_id', 'floats', 'raw_len', 'recv_time')

    def __init__(self, seq_id, module_id, floats, raw_len):
        self.seq_id = seq_id
        self.module_id = module_id
        self.floats = floats
        self.raw_len = raw_len
        self.recv_time = time.time()


# ===================== 2) Serial Worker =====================

class PhAISerialWorker(QtCore.QObject):
    packet_received = pyqtSignal(object)    # PhAIPacket
    status_msg = pyqtSignal(str)
    finished = pyqtSignal()
    connection_failed = pyqtSignal(str)
    stats_update = pyqtSignal(int, int, int)  # (good, crc_err, sync_err)

    def __init__(self, port_name, baudrate=DEFAULT_BAUD, timeout=DEFAULT_TIMEOUT, parent=None):
        super().__init__(parent)
        self.port_name = port_name
        self.baudrate = baudrate
        self.timeout = timeout
        self._running = False
        self._good = 0
        self._crc_err = 0
        self._sync_err = 0

    @pyqtSlot()
    def run(self):
        self._running = True
        ser = None
        try:
            try:
                ser = serial.Serial(self.port_name, self.baudrate, timeout=self.timeout)
                self.status_msg.emit(f"Connected to {self.port_name}")
            except Exception as e:
                self.connection_failed.emit(f"Serial open error: {e}")
                return

            buf = bytearray()
            last_stats_time = time.time()

            while self._running:
                try:
                    chunk = ser.read(1024)
                except serial.SerialException as e:
                    self.status_msg.emit(f"Serial read error: {e}")
                    break

                if not chunk:
                    continue

                buf.extend(chunk)

                while True:
                    if len(buf) < PHAI_HEADER_SIZE + PHAI_CRC_SIZE:
                        break

                    # SOF 검색
                    if buf[0] != PHAI_SOF:
                        self._sync_err += 1
                        del buf[0]
                        continue

                    len_units = buf[1]
                    if len_units == 0 or len_units > PHAI_MAX_LEN_UNITS:
                        self._sync_err += 1
                        del buf[0]
                        continue

                    total_size = PHAI_HEADER_SIZE + (len_units * 4) + PHAI_CRC_SIZE
                    if len(buf) < total_size:
                        break

                    packet_bytes = bytes(buf[:total_size])
                    del buf[:total_size]

                    # CRC8 검증: bytes[1] ~ bytes[total-2]
                    calc_crc = crc8_phai(packet_bytes[1:total_size - 1])
                    recv_crc = packet_bytes[total_size - 1]

                    if calc_crc != recv_crc:
                        self._crc_err += 1
                        continue

                    # 헤더 파싱
                    seq_id = packet_bytes[2] | (packet_bytes[3] << 8)
                    module_id = packet_bytes[4]

                    # Payload → float 배열
                    payload = packet_bytes[PHAI_HEADER_SIZE:PHAI_HEADER_SIZE + (len_units * 4)]
                    floats = list(struct.unpack(f'<{len_units}f', payload))

                    pkt = PhAIPacket(seq_id, module_id, floats, total_size)
                    self.packet_received.emit(pkt)
                    self._good += 1

                # 주기적 통계
                now = time.time()
                if now - last_stats_time >= 1.0:
                    self.stats_update.emit(self._good, self._crc_err, self._sync_err)
                    last_stats_time = now

        finally:
            if ser is not None and ser.is_open:
                ser.close()
                self.status_msg.emit("Serial port closed")
            self.finished.emit()

    def stop(self):
        self._running = False


# ===================== 3) CSV Review Dialog =====================

class CsvReviewDialog(QtWidgets.QDialog):
    def __init__(self, csv_path=None, parent=None):
        super().__init__(parent)

        flags = self.windowFlags()
        flags &= ~Qt.WindowContextHelpButtonHint
        flags |= Qt.WindowMaximizeButtonHint
        self.setWindowFlags(flags)

        self.setWindowTitle(
            f"CSV Review — {os.path.basename(csv_path)}" if csv_path else "CSV Review Viewer"
        )
        self.resize(1400, 800)

        self.csv_path = csv_path
        self.csv_cols = None
        self.data = None
        self.time_col = None

        self._build_ui()
        if csv_path:
            self.load_csv(csv_path)

    def _build_ui(self):
        layout = QtWidgets.QVBoxLayout(self)

        top = QtWidgets.QHBoxLayout()
        layout.addLayout(top)
        top.addWidget(QtWidgets.QLabel("CSV File:"))
        self.label_path = QtWidgets.QLabel("(no file loaded)")
        self.label_path.setTextInteractionFlags(Qt.TextSelectableByMouse)
        top.addWidget(self.label_path, stretch=1)

        btn_open = QtWidgets.QPushButton("Open CSV...")
        btn_open.clicked.connect(self._on_open)
        top.addWidget(btn_open)

        self.info_label = QtWidgets.QLabel("")
        layout.addWidget(self.info_label)

        self.grid = QtWidgets.QGridLayout()
        layout.addLayout(self.grid, stretch=1)
        self.plots = []

    def _on_open(self):
        fname, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Select CSV file", "", "CSV Files (*.csv);;All Files (*)")
        if fname:
            self.load_csv(fname)

    def load_csv(self, path):
        if not os.path.isfile(path):
            return

        try:
            arr = np.genfromtxt(path, delimiter=",", skip_header=1)
            with open(path, 'r') as f:
                self.csv_cols = f.readline().strip().split(',')

            if arr.ndim == 1:
                arr = arr.reshape(1, -1)
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to load CSV:\n{e}")
            return

        self.csv_path = path
        self.data = arr
        self.label_path.setText(path)
        self.info_label.setText(f"{arr.shape[0]} rows × {arr.shape[1]} cols")

        time_idx = 0
        self.time_col = arr[:, time_idx] - arr[0, time_idx]

        for pw in self.plots:
            self.grid.removeWidget(pw)
            pw.deleteLater()
        self.plots.clear()

        num_data_cols = arr.shape[1] - 1
        cols_per_plot = max(3, num_data_cols)
        num_plots = max(1, (num_data_cols + cols_per_plot - 1) // cols_per_plot)
        num_plots = min(num_plots, 6)

        per_plot = max(1, (num_data_cols + num_plots - 1) // num_plots)

        for p_idx in range(num_plots):
            pw = pg.PlotWidget()
            pw.setBackground("w")
            pw.showGrid(x=True, y=True, alpha=0.15)

            start_col = 1 + p_idx * per_plot
            end_col = min(1 + (p_idx + 1) * per_plot, arr.shape[1])

            hues = max(8, end_col - start_col)
            for k, ci in enumerate(range(start_col, end_col)):
                name = self.csv_cols[ci] if ci < len(self.csv_cols) else f"col{ci}"
                color = pg.intColor(k, hues=hues)
                pw.plot(self.time_col, arr[:, ci], pen=color, name=name)

            pw.addLegend()
            pw.setLabel("bottom", self.csv_cols[0] if self.csv_cols else "sample")
            self.plots.append(pw)

            r, c = p_idx // 2, p_idx % 2
            self.grid.addWidget(pw, r, c)


# ===================== 4) Modern Style =====================

def apply_modern_style(app: QtWidgets.QApplication):
    app.setStyle("Fusion")

    palette = QtGui.QPalette()
    palette.setColor(QtGui.QPalette.Window, QtGui.QColor("#f5f7fb"))
    palette.setColor(QtGui.QPalette.Base, QtGui.QColor("#ffffff"))
    palette.setColor(QtGui.QPalette.Text, QtGui.QColor("#111827"))
    palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor("#111827"))
    palette.setColor(QtGui.QPalette.Button, QtGui.QColor("#2563eb"))
    palette.setColor(QtGui.QPalette.ButtonText, QtGui.QColor("#ffffff"))
    palette.setColor(QtGui.QPalette.Highlight, QtGui.QColor("#2563eb"))
    palette.setColor(QtGui.QPalette.HighlightedText, QtGui.QColor("#ffffff"))
    app.setPalette(palette)

    app.setStyleSheet("""
        QWidget { background-color: #f5f7fb; font-family: 'Segoe UI', sans-serif;
                  font-size: 11pt; color: #111827; }
        QMainWindow { background-color: #f5f7fb; }
        QLineEdit, QComboBox { background-color: #ffffff; border: 1px solid #cbd5e1;
            border-radius: 8px; padding: 4px 8px; }
        QPushButton { background-color: #2563eb; color: #ffffff; border-radius: 8px;
            padding: 6px 14px; border: none; font-weight: 500; }
        QPushButton:hover { background-color: #1d4ed8; }
        QPushButton:disabled { background-color: #9ca3af; color: #e5e7eb; }
        QStatusBar { background-color: #ffffff; border-top: 1px solid #e5e7eb; }
        QGroupBox { border: 1px solid #e5e7eb; border-radius: 10px;
            margin-top: 8px; background-color: #ffffff; }
        QGroupBox::title { subcontrol-origin: margin; padding: 4px 10px; }
    """)


# ===================== 5) Main Window =====================

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PhAI V2 — USB-CDC Real-time Receiver")
        self.resize(1400, 900)

        # Rolling buffer: max 64 float channels
        self.max_channels = 64
        self.data_buffer = np.full((WINDOW_SIZE_SAMPLES, 1 + self.max_channels),
                                   np.nan, dtype=np.float32)
        self.buffer_idx = 0
        self.buffer_filled = False
        self._recv_count = 0
        self._active_channels = 0
        self._channel_names = []
        self._module_name = ""

        self.serial_thread = None
        self.serial_worker = None

        self.log_file = None
        self._written_lines = 0
        self._pending_lines = []
        self._last_log_path = None

        self._last_seq_id = -1
        self._seq_drops = 0

        # 6 plots
        self.plot_groups = []
        self.plot_widgets = []
        self.plot_curves = []

        self._build_ui()
        self.refresh_ports()

        self.plot_timer = QtCore.QTimer(self)
        self.plot_timer.timeout.connect(self.update_all_plots)
        self.plot_timer.start(UPDATE_INTERVAL_MS)

    def _build_ui(self):
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        vbox = QtWidgets.QVBoxLayout(central)

        # Top control bar
        top = QtWidgets.QHBoxLayout()
        vbox.addLayout(top)

        top.addWidget(QtWidgets.QLabel("Serial Port:"))
        self.combo_port = QtWidgets.QComboBox()
        top.addWidget(self.combo_port)

        self.btn_refresh = QtWidgets.QPushButton("Refresh")
        self.btn_refresh.clicked.connect(self.refresh_ports)
        top.addWidget(self.btn_refresh)

        self.btn_connect = QtWidgets.QPushButton("Connect")
        self.btn_connect.clicked.connect(self.on_connect_clicked)
        top.addWidget(self.btn_connect)

        self.btn_disconnect = QtWidgets.QPushButton("Disconnect")
        self.btn_disconnect.clicked.connect(self.on_disconnect_clicked)
        self.btn_disconnect.setEnabled(False)
        top.addWidget(self.btn_disconnect)

        top.addStretch()

        top.addWidget(QtWidgets.QLabel("Output folder:"))
        default_folder = os.path.abspath(os.path.join(os.getcwd(), "data"))
        self.edit_folder = QtWidgets.QLineEdit(default_folder)
        self.edit_folder.setMinimumWidth(200)
        top.addWidget(self.edit_folder)

        self.btn_browse = QtWidgets.QPushButton("Browse...")
        self.btn_browse.clicked.connect(self._on_browse_folder)
        top.addWidget(self.btn_browse)

        # Info line
        info_line = QtWidgets.QHBoxLayout()
        vbox.addLayout(info_line)

        self.module_label = QtWidgets.QLabel("Module: -")
        font = self.module_label.font()
        font.setPointSize(font.pointSize() + 1)
        font.setBold(True)
        self.module_label.setFont(font)
        info_line.addWidget(self.module_label)

        info_line.addSpacing(20)

        self.stats_label = QtWidgets.QLabel("Good: 0 | CRC Err: 0 | Sync Err: 0 | SEQ Drop: 0")
        self.stats_label.setFont(font)
        info_line.addWidget(self.stats_label)

        info_line.addStretch()

        self.btn_review = QtWidgets.QPushButton("Review CSV")
        self.btn_review.clicked.connect(self._open_review)
        info_line.addWidget(self.btn_review)

        # Plot grid (2×3)
        grid = QtWidgets.QGridLayout()
        vbox.addLayout(grid, stretch=1)

        for i in range(6):
            pw = pg.PlotWidget()
            pw.setBackground("w")
            pw.showGrid(x=True, y=True, alpha=0.15)
            pw.setClipToView(True)
            pw.setDownsampling(auto=True)

            axis_pen = pg.mkPen("#9ca3af")
            pw.getAxis("bottom").setPen(axis_pen)
            pw.getAxis("left").setPen(axis_pen)
            pw.getAxis("bottom").setTextPen("#4b5563")
            pw.getAxis("left").setTextPen("#4b5563")

            pw.setLabel("bottom", "sample")
            pw.setLabel("left", "Value")
            pw.setTitle(f"Plot {i + 1}")

            self.plot_widgets.append(pw)
            self.plot_curves.append([])
            r, c = i // 3, i % 3
            grid.addWidget(pw, r, c)

        self.status_bar = self.statusBar()
        self.status_bar.showMessage("Ready — PhAI V2 Protocol")

    def refresh_ports(self):
        self.combo_port.clear()
        ports = serial.tools.list_ports.comports()
        for p in ports:
            self.combo_port.addItem(f"{p.device} — {p.description}")
        if not ports:
            self.combo_port.addItem("(no ports)")
        self.status_bar.showMessage("Port list refreshed")

    def _on_browse_folder(self):
        folder = QtWidgets.QFileDialog.getExistingDirectory(
            self, "Select output folder",
            self.edit_folder.text().strip() or os.getcwd())
        if folder:
            self.edit_folder.setText(folder)

    def _open_log_file(self):
        folder = (self.edit_folder.text() or "").strip()
        if not folder:
            folder = os.path.abspath("data")
        os.makedirs(folder, exist_ok=True)

        filename = f"cdc_phai_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        full_path = os.path.join(folder, filename)

        self.log_file = open(full_path, "w", encoding="utf-8", newline="")
        self._written_lines = 0
        self._pending_lines = []
        self._last_log_path = full_path
        self.status_bar.showMessage(f"Logging to: {full_path}")

    def _flush_pending(self):
        if self.log_file and self._pending_lines:
            self.log_file.writelines(self._pending_lines)
            self._pending_lines.clear()

    def _close_log_file(self):
        if self.log_file:
            self._flush_pending()
            self.log_file.flush()
            self.log_file.close()
            self.log_file = None

    def _write_csv_header(self, channel_names):
        if self.log_file:
            header = "recv_time,seq_id,module_id," + ",".join(channel_names) + "\n"
            self.log_file.write(header)

    def _setup_plots(self, channel_names):
        """채널 수에 따라 플롯 곡선들을 동적으로 재구성합니다."""
        n_ch = len(channel_names)
        self._active_channels = n_ch
        self._channel_names = channel_names

        per_plot = max(1, (n_ch + 5) // 6)

        for p_idx in range(6):
            pw = self.plot_widgets[p_idx]

            for _, curve in self.plot_curves[p_idx]:
                pw.removeItem(curve)
            self.plot_curves[p_idx] = []

            start = p_idx * per_plot
            end = min(start + per_plot, n_ch)

            if start >= n_ch:
                pw.setTitle(f"Plot {p_idx + 1} (unused)")
                continue

            preview = ", ".join(channel_names[start:min(start + 4, end)])
            if end - start > 4:
                preview += ", ..."
            pw.setTitle(f"Plot {p_idx + 1}  [{preview}]")

            hues = max(8, end - start)
            for k, ch_idx in enumerate(range(start, end)):
                color = pg.intColor(k, hues=hues)
                pen = pg.mkPen(color, width=0, cosmetic=True)
                curve = pw.plot([], [], pen=pen, name=channel_names[ch_idx],
                                skipFiniteCheck=True)
                self.plot_curves[p_idx].append((ch_idx, curve))

    # ---------- Connect/Disconnect ----------

    def on_connect_clicked(self):
        port_text = self.combo_port.currentText()
        if not port_text or port_text.startswith("(no"):
            QtWidgets.QMessageBox.warning(self, "Warning", "No serial port selected.")
            return

        port_name = port_text.split(" — ")[0].strip()

        try:
            self._open_log_file()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to open log file:\n{e}")
            return

        self.buffer_idx = 0
        self.buffer_filled = False
        self.data_buffer[:] = np.nan
        self._recv_count = 0
        self._last_seq_id = -1
        self._seq_drops = 0

        self.serial_thread = QtCore.QThread()
        self.serial_worker = PhAISerialWorker(port_name)
        self.serial_worker.moveToThread(self.serial_thread)

        self.serial_thread.started.connect(self.serial_worker.run)
        self.serial_worker.packet_received.connect(self.on_packet_received)
        self.serial_worker.status_msg.connect(self.on_status_msg)
        self.serial_worker.finished.connect(self.on_serial_finished)
        self.serial_worker.connection_failed.connect(self.on_connection_failed)
        self.serial_worker.stats_update.connect(self.on_stats_update)

        self.serial_thread.start()

        self.btn_connect.setEnabled(False)
        self.btn_disconnect.setEnabled(True)
        self.status_bar.showMessage(f"Connecting to {port_name} ...")

    def on_disconnect_clicked(self):
        if self.serial_worker:
            self.serial_worker.stop()

    # ---------- Data handling ----------

    @pyqtSlot(object)
    def on_packet_received(self, pkt: PhAIPacket):
        n_floats = len(pkt.floats)

        if self._recv_count == 0:
            self._module_name = get_module_name(pkt.module_id)
            ch_names = get_channel_names(pkt.module_id, n_floats)
            self._setup_plots(ch_names)
            self._write_csv_header(ch_names)
            self.module_label.setText(
                f"Module: {self._module_name} (0x{pkt.module_id:02X}) — {n_floats} channels")

        # SEQ_ID gap detection
        if self._last_seq_id >= 0:
            expected = (self._last_seq_id + 1) & 0xFFFF
            if pkt.seq_id != expected:
                gap = (pkt.seq_id - self._last_seq_id) & 0xFFFF
                self._seq_drops += gap - 1
        self._last_seq_id = pkt.seq_id

        # Rolling buffer write
        idx = self.buffer_idx % WINDOW_SIZE_SAMPLES
        self.data_buffer[idx, 0] = self._recv_count
        for i, v in enumerate(pkt.floats):
            if i < self.max_channels:
                self.data_buffer[idx, 1 + i] = v
        self.buffer_idx += 1
        if self.buffer_idx >= WINDOW_SIZE_SAMPLES:
            self.buffer_filled = True
        self._recv_count += 1

        # CSV logging
        if self.log_file:
            ts = f"{pkt.recv_time:.6f}"
            vals = ",".join(f"{v:.6f}" for v in pkt.floats)
            line = f"{ts},{pkt.seq_id},{pkt.module_id},{vals}\n"
            self._pending_lines.append(line)
            self._written_lines += 1
            if len(self._pending_lines) >= FLUSH_EVERY:
                self._flush_pending()

    def update_all_plots(self):
        if self._recv_count == 0 or self._active_channels == 0:
            return

        if self.buffer_filled:
            start = self.buffer_idx % WINDOW_SIZE_SAMPLES
            data = np.concatenate([self.data_buffer[start:], self.data_buffer[:start]])
        else:
            data = self.data_buffer[:self.buffer_idx]

        if len(data) == 0:
            return

        x_vals = data[:, 0]

        per_plot = max(1, (self._active_channels + 5) // 6)

        for p_idx, curves in enumerate(self.plot_curves):
            if not curves:
                continue

            pw = self.plot_widgets[p_idx]
            pw.setUpdatesEnabled(False)

            for ch_idx, curve in curves:
                y_vals = data[:, 1 + ch_idx]
                curve.setData(x_vals, y_vals, connect='finite', skipFiniteCheck=True)

            if len(x_vals) > 0:
                pw.setXRange(float(x_vals[0]), float(x_vals[-1]), padding=0)

            pw.setUpdatesEnabled(True)

    # ---------- Slots ----------

    @pyqtSlot(str)
    def on_status_msg(self, msg):
        self.status_bar.showMessage(msg)

    @pyqtSlot(str)
    def on_connection_failed(self, msg):
        self._close_log_file()
        QtWidgets.QMessageBox.critical(self, "Connection Failed", msg)
        self.btn_connect.setEnabled(True)
        self.btn_disconnect.setEnabled(False)

    @pyqtSlot()
    def on_serial_finished(self):
        if self.serial_thread:
            self.serial_thread.quit()
            self.serial_thread.wait()
            self.serial_thread = None
        self.serial_worker = None

        self._close_log_file()
        self.btn_connect.setEnabled(True)
        self.btn_disconnect.setEnabled(False)
        self.status_bar.showMessage(
            f"Disconnected — {self._recv_count} packets, {self._written_lines} lines saved")

    @pyqtSlot(int, int, int)
    def on_stats_update(self, good, crc_err, sync_err):
        self.stats_label.setText(
            f"Good: {good} | CRC Err: {crc_err} | Sync Err: {sync_err} | SEQ Drop: {self._seq_drops}")

    def _open_review(self):
        if self._last_log_path and os.path.isfile(self._last_log_path):
            path = self._last_log_path
        else:
            folder = self.edit_folder.text().strip() or os.getcwd()
            pattern = os.path.join(folder, "cdc_phai_*.csv")
            files = sorted(glob.glob(pattern), key=os.path.getmtime, reverse=True)
            path = files[0] if files else None

        if not path:
            QtWidgets.QMessageBox.information(self, "Info", "No CSV file found.")
            return
        dlg = CsvReviewDialog(path, self)
        dlg.setModal(False)
        dlg.show()

    def closeEvent(self, event):
        if self.serial_worker:
            self.serial_worker.stop()
        if self.serial_thread:
            self.serial_thread.quit()
            self.serial_thread.wait()
        self._close_log_file()
        event.accept()


# ===================== 6) CLI Mode =====================

def run_cli(port_name, baudrate, output_folder):
    """GUI 없이 콘솔에서 실행하는 CLI 모드."""
    os.makedirs(output_folder, exist_ok=True)

    filename = f"cdc_phai_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    output_path = os.path.join(output_folder, filename)

    print(f"[PhAI V2 CLI Receiver]")
    print(f"  Port: {port_name}, Baud: {baudrate}")
    print(f"  Output: {output_path}")
    print(f"  Press Ctrl+C to stop.\n")

    try:
        ser = serial.Serial(port_name, baudrate, timeout=DEFAULT_TIMEOUT)
    except Exception as e:
        print(f"[ERROR] {e}")
        return

    buf = bytearray()
    good_count = 0
    err_count = 0
    header_written = False
    last_print = time.time()

    try:
        with open(output_path, 'w', encoding='utf-8') as fout:
            while True:
                chunk = ser.read(1024)
                if not chunk:
                    continue

                buf.extend(chunk)

                while True:
                    if len(buf) < PHAI_HEADER_SIZE + PHAI_CRC_SIZE:
                        break

                    if buf[0] != PHAI_SOF:
                        del buf[0]
                        err_count += 1
                        continue

                    len_units = buf[1]
                    if len_units == 0 or len_units > PHAI_MAX_LEN_UNITS:
                        del buf[0]
                        err_count += 1
                        continue

                    total = PHAI_HEADER_SIZE + (len_units * 4) + PHAI_CRC_SIZE
                    if len(buf) < total:
                        break

                    pkt_bytes = bytes(buf[:total])
                    del buf[:total]

                    calc = crc8_phai(pkt_bytes[1:total - 1])
                    if calc != pkt_bytes[total - 1]:
                        err_count += 1
                        continue

                    seq_id = pkt_bytes[2] | (pkt_bytes[3] << 8)
                    module_id = pkt_bytes[4]
                    payload = pkt_bytes[PHAI_HEADER_SIZE:PHAI_HEADER_SIZE + (len_units * 4)]
                    floats = struct.unpack(f'<{len_units}f', payload)

                    if not header_written:
                        ch_names = get_channel_names(module_id, len_units)
                        fout.write("recv_time,seq_id,module_id," +
                                   ",".join(ch_names) + "\n")
                        header_written = True
                        print(f"  Module: {get_module_name(module_id)} — {len_units} channels")

                    ts = f"{time.time():.6f}"
                    vals = ",".join(f"{v:.6f}" for v in floats)
                    fout.write(f"{ts},{seq_id},{module_id},{vals}\n")
                    good_count += 1

                    if good_count % FLUSH_EVERY == 0:
                        fout.flush()

                now = time.time()
                if now - last_print >= 2.0:
                    print(f"  Packets: {good_count}, Errors: {err_count}")
                    last_print = now

    except KeyboardInterrupt:
        print(f"\n[DONE] {good_count} packets → {output_path}")
    finally:
        ser.close()


# ===================== 7) Entry Point =====================

def main():
    parser = argparse.ArgumentParser(description="PhAI V2 CDC Receiver")
    parser.add_argument("--cli", action="store_true", help="Run in CLI mode (no GUI)")
    parser.add_argument("--port", type=str, default=None, help="Serial port (e.g., COM6)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baud rate")
    parser.add_argument("--output", type=str, default="data", help="Output folder")
    args = parser.parse_args()

    if args.cli:
        if not args.port:
            print("Error: --port required for CLI mode")
            sys.exit(1)
        run_cli(args.port, args.baud, args.output)
    else:
        app = QtWidgets.QApplication(sys.argv)
        apply_modern_style(app)
        pg.setConfigOptions(antialias=False)
        pg.setConfigOption("background", "w")
        pg.setConfigOption("foreground", "#111827")

        win = MainWindow()
        win.showMaximized()
        sys.exit(app.exec_())


if __name__ == "__main__":
    main()
