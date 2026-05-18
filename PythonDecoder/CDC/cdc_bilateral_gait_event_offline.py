"""
Offline bilateral 4-FSR gait event viewer.

Loads a CSV recording, runs the same contact/state/event detector used by
cdc_bilateral_gait_event_monitor.py, and visualizes normalized load,
contact membership, and detected gait events.
"""

import argparse
import csv
import json
import os
import sys

from cdc_bilateral_gait_event_monitor import (
    BITS,
    CONTACT_PRESETS,
    DEFAULT_LABELS,
    DEFAULT_MINIMUM_SPAN,
    DEFAULT_OFF_THRESHOLD,
    DEFAULT_ON_THRESHOLD,
    EVENTS,
    HIGH_LEVEL_STATES,
    BilateralGaitDetector,
    Calibration,
    ContactDetector,
    contact_membership,
)


def _float_or_zero(row, key):
    try:
        return float(row.get(key, 0.0) or 0.0)
    except ValueError:
        return 0.0


def _find_raw_columns(columns, labels):
    candidates = [
        [f"{label}_raw" for label in labels],
        labels,
        ["PF3", "PF4", "PF5", "PF6"],
        ["LHeel", "LToe", "RHeel", "RToe"],
    ]
    for group in candidates:
        if all(col in columns for col in group):
            return group
    raise ValueError(
        "CSV must contain either *_raw columns, label columns, PF3..PF6, or LHeel..RToe"
    )


def _find_norm_columns(columns, labels):
    group = [f"{label}_norm" for label in labels]
    if all(col in columns for col in group):
        return group
    return None


def load_rows(path, labels):
    with open(path, "r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        columns = reader.fieldnames or []

    if not rows:
        raise ValueError("CSV has no rows")

    raw_cols = _find_raw_columns(columns, labels)
    norm_cols = _find_norm_columns(columns, labels)

    times = []
    seq_ids = []
    raw = []
    norm = [] if norm_cols else None

    for idx, row in enumerate(rows):
        times.append(_float_or_zero(row, "time_s") if "time_s" in row else float(idx))
        seq_ids.append(int(_float_or_zero(row, "seq_id")) if "seq_id" in row else idx)
        raw.append([_float_or_zero(row, col) for col in raw_cols])
        if norm_cols:
            norm.append([_float_or_zero(row, col) for col in norm_cols])

    return times, seq_ids, raw, norm, raw_cols, norm_cols


def auto_calibration(raw, minimum_span=DEFAULT_MINIMUM_SPAN):
    cols = list(zip(*raw))
    off = []
    on = []
    for values in cols:
        sorted_values = sorted(float(v) for v in values)
        n = len(sorted_values)
        low_idx = max(0, int(n * 0.05))
        high_idx = min(n - 1, int(n * 0.95))
        off.append(sorted_values[low_idx])
        on.append(sorted_values[high_idx])
    return Calibration(off, on, minimum_span)


def load_calibration(path):
    with open(path, "r", encoding="utf-8") as f:
        return Calibration.from_dict(json.load(f))


def process_samples(times, seq_ids, raw, norm, calibration, on_threshold, off_threshold):
    contact_detector = ContactDetector(on_threshold, off_threshold)
    gait_detector = BilateralGaitDetector()

    processed = []
    for t, seq_id, raw_values, norm_values in zip(times, seq_ids, raw, norm):
        contacts = contact_detector.update(norm_values)
        info = gait_detector.update(t, contacts)
        memberships = [
            contact_membership(v, off_threshold, on_threshold)
            for v in norm_values
        ]
        processed.append({
            "time": t,
            "seq_id": seq_id,
            "raw": raw_values,
            "norm": norm_values,
            "memberships": memberships,
            "contacts": contacts,
            "info": info,
        })
    return processed


def run_gui(path, labels, processed, raw_cols, on_threshold, off_threshold):
    try:
        from PyQt5 import QtCore, QtWidgets
        import pyqtgraph as pg
    except ImportError as exc:
        missing = exc.name or "PyQt5/pyqtgraph"
        print(f"Missing GUI dependency: {missing}", file=sys.stderr)
        sys.exit(2)

    class MainWindow(QtWidgets.QMainWindow):
        def __init__(self):
            super().__init__()
            self.setWindowTitle(f"Offline Bilateral Gait Event Viewer - {os.path.basename(path)}")
            self.resize(1280, 760)
            self._build_ui()
            self._populate()

        @staticmethod
        def _block_css(active, color):
            if active:
                return f"background:{color}; color:white; border:2px solid {color}; padding:6px; font-weight:bold;"
            return "background:#eeeeee; color:#777; border:1px solid #c8c8c8; padding:6px;"

        def _build_ui(self):
            central = QtWidgets.QWidget()
            root = QtWidgets.QVBoxLayout(central)
            root.setContentsMargins(10, 10, 10, 10)
            root.setSpacing(8)

            self.summary = QtWidgets.QLabel("")
            self.summary.setMinimumHeight(28)
            root.addWidget(self.summary)

            state_row = QtWidgets.QHBoxLayout()
            self.state_counts = {}
            for name in HIGH_LEVEL_STATES:
                lbl = QtWidgets.QLabel(name.replace("_", " "))
                lbl.setAlignment(QtCore.Qt.AlignCenter)
                lbl.setMinimumHeight(34)
                lbl.setStyleSheet(self._block_css(False, "#1f77b4"))
                self.state_counts[name] = lbl
                state_row.addWidget(lbl, 1)
            root.addLayout(state_row)

            event_row = QtWidgets.QHBoxLayout()
            self.event_counts = {}
            for name in EVENTS:
                lbl = QtWidgets.QLabel(name.replace("_", "\n"))
                lbl.setAlignment(QtCore.Qt.AlignCenter)
                lbl.setMinimumHeight(46)
                lbl.setStyleSheet(self._block_css(False, "#d62728"))
                self.event_counts[name] = lbl
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

            self.membership_plot = pg.PlotWidget(title="Contact Membership Functions")
            self.membership_plot.setBackground("w")
            self.membership_plot.showGrid(x=True, y=True, alpha=0.25)
            self.membership_plot.setLabel("bottom", "time", units="s")
            self.membership_plot.setLabel("left", "membership")
            self.membership_plot.setYRange(0.0, 1.05)
            self.membership_plot.addLegend(offset=(10, 10))
            root.addWidget(self.membership_plot, 2)

            lower = QtWidgets.QHBoxLayout()
            self.event_table = QtWidgets.QTableWidget(0, 5)
            self.event_table.setHorizontalHeaderLabels(["time", "seq", "event", "bits", "state"])
            self.event_table.horizontalHeader().setStretchLastSection(True)
            lower.addWidget(self.event_table, 3)

            self.info = QtWidgets.QLabel("")
            self.info.setAlignment(QtCore.Qt.AlignTop)
            self.info.setMinimumWidth(320)
            lower.addWidget(self.info, 1)
            root.addLayout(lower)

            self.setCentralWidget(central)

        def _populate(self):
            import pyqtgraph as pg

            times = [item["time"] for item in processed]
            colors = ["#d62728", "#ff7f0e", "#1f77b4", "#2ca02c"]
            for idx, (label, color) in enumerate(zip(labels, colors)):
                self.plot.plot(
                    times,
                    [item["norm"][idx] for item in processed],
                    pen=pg.mkPen(color=color, width=2),
                    name=label,
                )
                self.membership_plot.plot(
                    times,
                    [item["memberships"][idx] for item in processed],
                    pen=pg.mkPen(color=color, width=2),
                    name=f"{label} μ",
                )

            self.plot.addItem(pg.InfiniteLine(pos=on_threshold, angle=0, pen=pg.mkPen("#444", width=1, style=QtCore.Qt.DashLine)))
            self.plot.addItem(pg.InfiniteLine(pos=off_threshold, angle=0, pen=pg.mkPen("#888", width=1, style=QtCore.Qt.DotLine)))

            state_counts = {name: 0 for name in HIGH_LEVEL_STATES}
            event_counts = {name: 0 for name in EVENTS}
            event_rows = []
            for item in processed:
                state_counts[item["info"]["high_level"]] = state_counts.get(item["info"]["high_level"], 0) + 1
                for event in item["info"]["events"]:
                    event_counts[event] = event_counts.get(event, 0) + 1
                    event_rows.append((item["time"], item["seq_id"], event, item["info"]["bits"], item["info"]["state"]))
                    marker = pg.InfiniteLine(pos=item["time"], angle=90, pen=pg.mkPen("#222", width=1, style=QtCore.Qt.DashLine))
                    self.plot.addItem(marker)

            for name, lbl in self.state_counts.items():
                count = state_counts.get(name, 0)
                lbl.setText(f"{name.replace('_', ' ')}\n{count}")
                lbl.setStyleSheet(self._block_css(count > 0, "#1f77b4"))

            for name, lbl in self.event_counts.items():
                count = event_counts.get(name, 0)
                lbl.setText(f"{name.replace('_', chr(10))}\n{count}")
                lbl.setStyleSheet(self._block_css(count > 0, "#d62728"))

            self.event_table.setRowCount(len(event_rows))
            for row, values in enumerate(event_rows):
                display = [f"{values[0]:.3f}", str(values[1]), values[2], values[3], values[4]]
                for col, value in enumerate(display):
                    self.event_table.setItem(row, col, QtWidgets.QTableWidgetItem(value))

            duration = times[-1] - times[0] if len(times) > 1 else 0.0
            self.summary.setText(
                f"{path} | samples:{len(processed)} duration:{duration:.2f}s "
                f"thresholds: ON={on_threshold:.2f}, OFF={off_threshold:.2f}"
            )
            self.info.setText(
                "columns:\n"
                + "\n".join(f"  {bit}: {col}" for bit, col in zip(BITS, raw_cols))
                + "\n\n"
                + "events:\n"
                + "\n".join(f"  {name}: {event_counts.get(name, 0)}" for name in EVENTS)
            )

    app = QtWidgets.QApplication(sys.argv)
    pg.setConfigOptions(antialias=True)
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())


def parse_args():
    parser = argparse.ArgumentParser(description="Offline bilateral 4-FSR gait event viewer")
    parser.add_argument("csv_path", help="CSV file recorded by the online monitor or 4ch monitor")
    parser.add_argument("--labels", default=",".join(DEFAULT_LABELS))
    parser.add_argument("--calibration", help="Calibration JSON from the online monitor")
    parser.add_argument("--preset", choices=CONTACT_PRESETS.keys(), default="Normal")
    parser.add_argument("--on-threshold", type=float)
    parser.add_argument("--off-threshold", type=float)
    return parser.parse_args()


def main():
    args = parse_args()
    labels = [item.strip() for item in args.labels.split(",") if item.strip()]
    if len(labels) != 4:
        print("--labels must contain exactly 4 comma-separated names", file=sys.stderr)
        sys.exit(2)

    times, seq_ids, raw, norm, raw_cols, _norm_cols = load_rows(args.csv_path, labels)

    if args.calibration:
        calibration = load_calibration(args.calibration)
    else:
        calibration = auto_calibration(raw)

    if norm is None:
        norm = [calibration.normalize(values) for values in raw]

    preset_on, preset_off = CONTACT_PRESETS[args.preset]
    on_threshold = args.on_threshold if args.on_threshold is not None else preset_on
    off_threshold = args.off_threshold if args.off_threshold is not None else preset_off

    processed = process_samples(
        times,
        seq_ids,
        raw,
        norm,
        calibration,
        on_threshold,
        off_threshold,
    )
    run_gui(args.csv_path, labels, processed, raw_cols, on_threshold, off_threshold)


if __name__ == "__main__":
    main()
