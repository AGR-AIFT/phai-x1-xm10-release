"""
PhAI V2.1 — CDC CSV Post-Processing Reviewer
==============================================
cdc_phai_receiver.py 가 저장한 CSV 파일을 로드하여
전체 세션 데이터를 정밀 분석하는 후처리 뷰어입니다.

CSV 포맷 (PhAI V2.1):
    time_s, seq_id, module_id, tx_drops, ch0, ch1, ...

주요 기능:
    - 센서 도메인별 그래프 그룹 (최대 10개)
    - Sequence Gap (ΔSeq) 분석 그래프 + 통계
    - Tx Drop 누적 그래프
    - 이상치 자동 필터
    - 채널별 체크박스 visibility 토글
    - 동적 그리드 레이아웃
    - CSV 파일 Open / Drag-and-Drop

사용법:
    python cdc_csv_reviewer.py                        # 파일 선택 다이얼로그
    python cdc_csv_reviewer.py path/to/file.csv       # 직접 지정

Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
"""

import sys
import os
import csv
import argparse

from PyQt5 import QtWidgets, QtGui, QtCore
from PyQt5.QtCore import Qt

import pyqtgraph as pg
import numpy as np

# ============================================================================
# Plot Group Definitions
# ============================================================================

COMBINED_PLOT_GROUPS = [
    ("Accelerometer",  ["AccX", "AccY", "AccZ"]),
    ("Gyroscope",      ["GyrX", "GyrY", "GyrZ"]),
    ("Motor Angle",    ["MotorAngle_L", "MotorAngle_R"]),
    ("Motor Torque",   ["MotorTorque_L", "MotorTorque_R"]),
]

META_COLS = {"time_s", "pc_time_s", "seq_id", "module_id", "tx_drops"}


def detect_plot_groups(data_col_names: list) -> list:
    """Auto-detect plot groups from column names."""
    combined_names = set()
    for _, names in COMBINED_PLOT_GROUPS:
        combined_names.update(names)

    if combined_names.issubset(set(data_col_names)):
        return list(COMBINED_PLOT_GROUPS)

    n = len(data_col_names)
    if n <= 6:
        return [("All Channels", list(data_col_names))]

    per = max(1, (n + 5) // 6)
    groups = []
    for i in range(0, n, per):
        chunk = data_col_names[i:i + per]
        groups.append((f"Ch {i}–{i + len(chunk) - 1}", list(chunk)))
    return groups[:6]


# ============================================================================
# Style
# ============================================================================

def apply_style(app):
    app.setStyle("Fusion")
    pal = QtGui.QPalette()
    pal.setColor(QtGui.QPalette.Window, QtGui.QColor("#f5f7fb"))
    pal.setColor(QtGui.QPalette.Base, QtGui.QColor("#ffffff"))
    pal.setColor(QtGui.QPalette.Text, QtGui.QColor("#111827"))
    pal.setColor(QtGui.QPalette.WindowText, QtGui.QColor("#111827"))
    pal.setColor(QtGui.QPalette.Button, QtGui.QColor("#2563eb"))
    pal.setColor(QtGui.QPalette.ButtonText, QtGui.QColor("#ffffff"))
    pal.setColor(QtGui.QPalette.Highlight, QtGui.QColor("#2563eb"))
    pal.setColor(QtGui.QPalette.HighlightedText, QtGui.QColor("#ffffff"))
    app.setPalette(pal)
    app.setStyleSheet("""
        QWidget { background-color: #f5f7fb; font-family: 'Segoe UI', sans-serif;
                  font-size: 10pt; color: #111827; }
        QLineEdit, QComboBox { background-color: #ffffff; border: 1px solid #cbd5e1;
            border-radius: 6px; padding: 3px 6px; }
        QPushButton { background-color: #2563eb; color: #ffffff; border-radius: 6px;
            padding: 5px 12px; border: none; font-weight: 500; }
        QPushButton:hover { background-color: #1d4ed8; }
        QPushButton:disabled { background-color: #9ca3af; }
        QCheckBox { spacing: 4px; }
        QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px;
            border: 1px solid #cbd5e1; background-color: #ffffff; }
        QCheckBox::indicator:checked { background-color: #2563eb; border: 1px solid #2563eb; }
        QGroupBox { border: 1px solid #e5e7eb; border-radius: 8px; margin-top: 6px;
            background-color: #ffffff; }
        QGroupBox::title { subcontrol-origin: margin; padding: 2px 8px; }
    """)


# ============================================================================
# Main Window
# ============================================================================

class CsvReviewWindow(QtWidgets.QMainWindow):
    def __init__(self, csv_path=None):
        super().__init__()
        self.setWindowTitle("PhAI V2.1 — CSV Post-Processing Reviewer")
        self.resize(1600, 950)
        self.setAcceptDrops(True)

        self.csv_path = None
        self.csv_cols = None
        self.data = None
        self.data_col_names = []
        self.col_index = {}

        self.time_col = None
        self.seq_col = None
        self.txdrop_col = None

        self.plot_groups = []
        self.plots = []
        self.ch_checkboxes = []
        self.group_checkboxes = []
        self.extra_plots = {}

        self._build_ui()

        if csv_path:
            self.load_csv(csv_path)

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        root = QtWidgets.QVBoxLayout(central)
        root.setContentsMargins(6, 6, 6, 6)

        # Top bar
        top = QtWidgets.QHBoxLayout()
        root.addLayout(top)
        top.addWidget(QtWidgets.QLabel("CSV File:"))
        self.lbl_path = QtWidgets.QLabel("(Drop CSV here or click Open)")
        self.lbl_path.setTextInteractionFlags(Qt.TextSelectableByMouse)
        top.addWidget(self.lbl_path, stretch=1)
        btn_open = QtWidgets.QPushButton("Open CSV...")
        btn_open.clicked.connect(self._on_open)
        top.addWidget(btn_open)

        # Info bar
        info_bar = QtWidgets.QHBoxLayout()
        root.addLayout(info_bar)
        bold = QtGui.QFont()
        bold.setBold(True)
        self.lbl_info = QtWidgets.QLabel("")
        self.lbl_info.setFont(bold)
        info_bar.addWidget(self.lbl_info)
        info_bar.addStretch()
        self.lbl_seq_stats = QtWidgets.QLabel("")
        self.lbl_seq_stats.setFont(bold)
        info_bar.addWidget(self.lbl_seq_stats)

        # Body: sidebar + grid
        body = QtWidgets.QHBoxLayout()
        root.addLayout(body, stretch=1)

        # Sidebar
        sidebar_w = QtWidgets.QWidget()
        sidebar_w.setFixedWidth(200)
        self._sidebar = QtWidgets.QVBoxLayout(sidebar_w)
        self._sidebar.setContentsMargins(2, 2, 2, 2)
        self._sidebar.setSpacing(1)

        lbl = QtWidgets.QLabel("Graph Groups")
        lbl.setFont(bold)
        self._sidebar.addWidget(lbl)
        self._group_container = QtWidgets.QVBoxLayout()
        self._sidebar.addLayout(self._group_container)

        self._sidebar.addSpacing(8)
        lbl2 = QtWidgets.QLabel("Channels")
        lbl2.setFont(bold)
        self._sidebar.addWidget(lbl2)
        self._ch_container = QtWidgets.QVBoxLayout()
        self._sidebar.addLayout(self._ch_container)

        hb = QtWidgets.QHBoxLayout()
        self._sidebar.addLayout(hb)
        bsa = QtWidgets.QPushButton("All")
        bsa.clicked.connect(lambda: self._set_all_ch(True))
        hb.addWidget(bsa)
        bsn = QtWidgets.QPushButton("None")
        bsn.clicked.connect(lambda: self._set_all_ch(False))
        hb.addWidget(bsn)
        self._sidebar.addStretch()

        # Analysis checkboxes
        self._sidebar.addSpacing(8)
        lbl3 = QtWidgets.QLabel("Analysis")
        lbl3.setFont(bold)
        self._sidebar.addWidget(lbl3)

        self._cb_seq_gap = QtWidgets.QCheckBox("Sequence Gap (ΔSeq)")
        self._cb_seq_gap.setChecked(True)
        self._cb_seq_gap.stateChanged.connect(self._on_visibility_changed)
        self._sidebar.addWidget(self._cb_seq_gap)

        self._cb_tx_drop = QtWidgets.QCheckBox("Tx Drop Cumulative")
        self._cb_tx_drop.setChecked(True)
        self._cb_tx_drop.stateChanged.connect(self._on_visibility_changed)
        self._sidebar.addWidget(self._cb_tx_drop)

        scroll = QtWidgets.QScrollArea()
        scroll.setWidget(sidebar_w)
        scroll.setWidgetResizable(True)
        scroll.setFixedWidth(220)
        body.addWidget(scroll)

        # Plot grid
        self.grid = QtWidgets.QGridLayout()
        self.grid.setSpacing(4)
        body.addLayout(self.grid, stretch=1)

        self.statusBar().showMessage("Ready — Drop or Open a PhAI V2.1 CSV file")

    # ------------------------------------------------------------------ Drag & Drop
    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dropEvent(self, event):
        for url in event.mimeData().urls():
            path = url.toLocalFile()
            if path.lower().endswith('.csv'):
                self.load_csv(path)
                break

    # ------------------------------------------------------------------ Open
    def _on_open(self):
        f, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Open CSV", "", "CSV Files (*.csv);;All Files (*)")
        if f:
            self.load_csv(f)

    # ------------------------------------------------------------------ Load
    def load_csv(self, path):
        if not os.path.isfile(path):
            QtWidgets.QMessageBox.warning(self, "Warning", f"File not found:\n{path}")
            return

        try:
            with open(path, 'r', encoding='utf-8') as f:
                header = f.readline().strip().split(',')
            arr = np.genfromtxt(path, delimiter=",", skip_header=1, dtype=np.float64)
            if arr.ndim == 1:
                arr = arr.reshape(1, -1)
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to load CSV:\n{e}")
            return

        self.csv_path = path
        self.csv_cols = header
        self.col_index = {n: i for i, n in enumerate(header)}

        self.time_col = self.col_index.get("time_s")
        self.seq_col = self.col_index.get("seq_id")
        self.txdrop_col = self.col_index.get("tx_drops")
        self.data_col_names = [n for n in header if n not in META_COLS]

        # Outlier filter
        data_indices = [self.col_index[n] for n in self.data_col_names]
        if data_indices:
            data_slice = arr[:, data_indices]
            bad_mask = (
                np.isnan(data_slice).any(axis=1) |
                np.isinf(data_slice).any(axis=1) |
                (np.abs(data_slice) > 1e6).any(axis=1)
            )
            if bad_mask.any():
                first_bad = int(np.argmax(bad_mask))
                if first_bad > 0:
                    arr = arr[:first_bad]
                    self.statusBar().showMessage(
                        f"Outlier detected at row {first_bad} — truncated to {first_bad} rows")

        self.data = arr
        self.lbl_path.setText(path)

        # Duration
        duration = 0.0
        if self.time_col is not None and arr.shape[0] > 1:
            duration = arr[-1, self.time_col] - arr[0, self.time_col]

        self.lbl_info.setText(
            f"{arr.shape[0]:,} rows  |  {len(self.data_col_names)} channels  |  "
            f"Duration: {duration:.2f}s")

        # Sequence analysis
        seq_gap_count = 0
        total_tx_drops = 0
        if self.seq_col is not None and arr.shape[0] > 1:
            seq = arr[:, self.seq_col].astype(np.int64)
            dseq = np.diff(seq)
            dseq_wrapped = np.where(dseq < 0, dseq + 65536, dseq)
            seq_gap_count = int(np.sum(dseq_wrapped > 1))
        if self.txdrop_col is not None:
            total_tx_drops = int(np.sum(arr[:, self.txdrop_col]))

        self.lbl_seq_stats.setText(
            f"Seq gaps (Δ>1): {seq_gap_count}  |  Total Tx drops: {total_tx_drops}")

        self._rebuild_all()

    # ------------------------------------------------------------------ Rebuild
    def _rebuild_all(self):
        # Clear old plots
        for pw in self.plots:
            self.grid.removeWidget(pw)
            pw.deleteLater()
        self.plots.clear()
        self.extra_plots.clear()

        # Clear sidebar
        while self._group_container.count():
            item = self._group_container.takeAt(0)
            w = item.widget()
            if w:
                w.deleteLater()
        while self._ch_container.count():
            item = self._ch_container.takeAt(0)
            w = item.widget()
            if w:
                w.deleteLater()

        self.group_checkboxes.clear()
        self.ch_checkboxes.clear()

        if self.data is None or len(self.data_col_names) == 0:
            return

        # Detect groups
        self.plot_groups = detect_plot_groups(self.data_col_names)

        # Group checkboxes
        for i, (gname, _) in enumerate(self.plot_groups):
            cb = QtWidgets.QCheckBox(gname)
            cb.setChecked(True)
            cb.stateChanged.connect(self._on_visibility_changed)
            self._group_container.addWidget(cb)
            self.group_checkboxes.append(cb)

        # Channel checkboxes
        self.ch_visible = {n: True for n in self.data_col_names}
        for name in self.data_col_names:
            cb = QtWidgets.QCheckBox(name)
            cb.setChecked(True)
            cb.stateChanged.connect(self._on_ch_toggled)
            self._ch_container.addWidget(cb)
            self.ch_checkboxes.append((name, cb))

        self._plot_all()

    # ------------------------------------------------------------------ Checkboxes
    def _set_all_ch(self, val):
        for _, cb in self.ch_checkboxes:
            cb.setChecked(val)

    def _on_ch_toggled(self, _state):
        self.ch_visible = {n: cb.isChecked() for n, cb in self.ch_checkboxes}
        self._plot_all()

    def _on_visibility_changed(self, _state):
        self._plot_all()

    # ------------------------------------------------------------------ Plotting
    def _plot_all(self):
        # Remove existing
        for pw in self.plots:
            self.grid.removeWidget(pw)
            pw.deleteLater()
        self.plots.clear()

        if self.data is None:
            return

        arr = self.data
        x = arr[:, self.time_col] - arr[0, self.time_col] if self.time_col is not None else np.arange(arr.shape[0])

        # Data group plots
        active_groups = []
        for i, (gname, ch_names) in enumerate(self.plot_groups):
            if i < len(self.group_checkboxes) and not self.group_checkboxes[i].isChecked():
                continue
            visible_chs = [n for n in ch_names if self.ch_visible.get(n, True)]
            if not visible_chs:
                continue
            active_groups.append((gname, visible_chs))

        # Analysis plots
        show_seq = self._cb_seq_gap.isChecked() and self.seq_col is not None and arr.shape[0] > 1
        show_txd = self._cb_tx_drop.isChecked() and self.txdrop_col is not None

        total_plots = len(active_groups) + (1 if show_seq else 0) + (1 if show_txd else 0)

        all_pw_list = []

        # Create data plots
        for gname, visible_chs in active_groups:
            pw = pg.PlotWidget(useOpenGL=True)
            pw.setBackground('w')
            pw.showGrid(x=True, y=True, alpha=0.15)
            self._style_axes(pw)
            pw.setLabel("bottom", "time (s)")
            pw.setLabel("left", "Value")

            preview = ", ".join(visible_chs[:4])
            if len(visible_chs) > 4:
                preview += " ..."
            pw.setTitle(f"{gname}  [{preview}]")

            legend = pw.addLegend()
            hues = max(8, len(visible_chs))
            for k, name in enumerate(visible_chs):
                ci = self.col_index.get(name)
                if ci is None or ci >= arr.shape[1]:
                    continue
                color = pg.intColor(k, hues=hues)
                pw.plot(x, arr[:, ci], pen=pg.mkPen(color, width=1), name=name)

            if len(x) > 0:
                pw.setXRange(float(x[0]), float(x[-1]), padding=0.01)
            all_pw_list.append(pw)

        # Sequence Gap plot
        if show_seq:
            pw = pg.PlotWidget(useOpenGL=True)
            pw.setBackground('w')
            pw.showGrid(x=True, y=True, alpha=0.15)
            self._style_axes(pw)
            pw.setLabel("bottom", "time (s)")
            pw.setLabel("left", "ΔSeq")

            seq = arr[:, self.seq_col].astype(np.int64)
            dseq = np.diff(seq, prepend=seq[0])
            dseq = np.where(dseq < 0, dseq + 65536, dseq)

            pw.setTitle(f"Sequence Gap (ΔSeq)  [jumps>1: {int(np.sum(dseq > 1))}]")
            pw.plot(x, dseq, pen=pg.mkPen('#ef4444', width=1), name="ΔSeq")

            threshold_line = pg.InfiniteLine(
                pos=1.5, angle=0, pen=pg.mkPen('#f59e0b', width=1, style=Qt.DashLine),
                label="threshold=2", labelOpts={'color': '#f59e0b', 'position': 0.95})
            pw.addItem(threshold_line)

            if len(x) > 0:
                pw.setXRange(float(x[0]), float(x[-1]), padding=0.01)
            all_pw_list.append(pw)

        # Tx Drop cumulative plot
        if show_txd:
            pw = pg.PlotWidget(useOpenGL=True)
            pw.setBackground('w')
            pw.showGrid(x=True, y=True, alpha=0.15)
            self._style_axes(pw)
            pw.setLabel("bottom", "time (s)")
            pw.setLabel("left", "Cumulative Tx Drops")

            cumulative = np.cumsum(arr[:, self.txdrop_col])
            total = int(cumulative[-1]) if len(cumulative) > 0 else 0
            pw.setTitle(f"Tx Drop Cumulative  [total: {total}]")
            pw.plot(x, cumulative, pen=pg.mkPen('#8b5cf6', width=1), name="CumTxDrop")

            if len(x) > 0:
                pw.setXRange(float(x[0]), float(x[-1]), padding=0.01)
            all_pw_list.append(pw)

        # Layout — dynamic grid
        self.plots = all_pw_list
        n = len(all_pw_list)
        if n == 0:
            return

        if n <= 3:
            cols = 1
        elif n <= 6:
            cols = 2
        else:
            cols = 3

        for i, pw in enumerate(all_pw_list):
            r = i // cols
            c = i % cols
            self.grid.addWidget(pw, r, c)

        # Link x-axes
        if len(all_pw_list) > 1:
            for pw in all_pw_list[1:]:
                pw.setXLink(all_pw_list[0])

    def _style_axes(self, pw):
        axis_pen = pg.mkPen("#9ca3af")
        for name in ("bottom", "left"):
            pw.getAxis(name).setPen(axis_pen)
            pw.getAxis(name).setTextPen("#4b5563")


# ============================================================================
# Entry Point
# ============================================================================

def main():
    ap = argparse.ArgumentParser(description="PhAI V2.1 CSV Post-Processing Reviewer")
    ap.add_argument("csv", nargs="?", default=None, help="Path to CSV file")
    args = ap.parse_args()

    app = QtWidgets.QApplication(sys.argv)
    apply_style(app)
    pg.setConfigOptions(antialias=True, useOpenGL=True)
    pg.setConfigOption("background", "w")
    pg.setConfigOption("foreground", "#111827")

    win = CsvReviewWindow(csv_path=args.csv)
    win.showMaximized()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
