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
import warnings
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
# 무손실 저장(.xmlog). 표준 라이브러리만 쓰므로 항상 import 된다.
from xmlog_capture import XmLogCapture, unique_path
import schema_registry as _schema

# 0x20 Total Data 디코더. 생성된 맵(xm_total_data_map.py)이 옆에 있어야 동작한다 —
# 없으면 --total-data 만 못 쓰고 나머지(--log 포함)는 그대로 돈다.
try:
    from total_data_decoder import TotalDataDecoder
    from total_data_decoder import MAP_AVAILABLE as TOTAL_DATA_MAP_AVAILABLE
except ImportError:
    TotalDataDecoder = None
    TOTAL_DATA_MAP_AVAILABLE = False

from frame_router import (
    parse_phai_frame, cobs_decode, FrameRouter,
    PHAI_MODULE_TOTAL_DATA, PHAI_MODULE_USER_META,
)
# CSV 파일명 규약(모듈 라벨)은 xmlog_export.py 가 원본이다 — 여기서 새로 만들지 않는다
# (post-hoc export 와 라이브 CSV 가 같은 이름으로 짝지어져야 나중에 헷갈리지 않는다).
from xmlog_export import _module_label, HexRowFilter

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

# _on_poll() 은 30ms 마다 워커 큐를 비운다. 예전엔 큐를 통째로 비웠고, 프레임이 밀리면 그
# 한 번이 창을 멈춰 세웠다(밀린 4만 프레임 -> 0.3초, module 15개의 탭을 한 번에 만들 때는
# 첫 틱이 거의 1초 — 2026-09-29 감사). 그 한 틱이 GUI 스레드를 붙잡는 시간을 아래 세 값으로
# 묶는다. 넘치는 프레임은 버리지 않고 큐에 남겨 다음 틱이 이어받는다.
# (아래 시간은 이 개발 PC 에서 잰 것이다: 프레임 하나를 탭에 반영하는 데 채널 6개 기준 약 7µs,
#  64개면 약 40µs. 느린 PC 에서는 비례해서 길어진다.)
#
# 한 틱에 탭에 반영하는 프레임 수. 3000개면 한 틱이 약 20ms 다. 밀렸을 때 틱이 이어지면 초당
# 약 8만 프레임을 비운다 — 채널 6개짜리 module 15개가 1kHz 로 보내는 양(1만 5천/초)의 5배다.
MAX_PACKETS_PER_POLL = 3000
# 한 틱에 새로 만드는 탭 수(1 이상). 탭 하나(6-plot)를 만드는 데 약 50ms 가 든다. 새 module 의
# 첫 프레임을 만나 이 한도를 넘으면 거기서 멈추고, 그 프레임부터는 다음 틱이 이어받는다
# (순서는 그대로다).
MAX_NEW_TABS_PER_POLL = 1
# 프레임이 밀려 있는 동안 그래프를 다시 그리는 최소 간격(초). 그리기는 창이 보일 때 한 번에
# 수십 ms 라서 틱마다 그리면 그 시간만큼 큐를 비우지 못한다(창을 띄운 채 밀린 4만 프레임을
# 비우는 데 틱마다 그리면 1.5초, 밀려 있는 동안 안 그리면 0.5초). 밀리기 시작한 뒤 이만큼
# 지나야 그리고, 다 따라잡으면 그 틱에서 바로 그린다. 평소(안 밀릴 때)는 틱마다 그린다.
BACKLOG_RENDER_PERIOD_S = 0.5

# "Show 0x20 tab" — 197채널을 6-plot 으로 그리는 건 무겁다(스펙 §2). 그래프 대신
# 표(이름+최신값) 하나로 대체하고, 갱신을 5Hz 로 스로틀한다 — 1kHz 로 오는 값을
# 표 위젯에 매번 반영하면 그 자체가 병목이다.
TOTAL_DATA_TABLE_HZ = 5.0
TOTAL_DATA_TABLE_PERIOD_S = 1.0 / TOTAL_DATA_TABLE_HZ

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

# FW 가 보내는 채널 설명을 담는다. 예전에는 이걸 받고도 버려서 사용자 채널이
# 늘 ch0.. 로 나왔다 — FW 는 이미 0xEF 로 이름과 단위를 보내고 있었다.
SCHEMA_REG = _schema.SchemaRegistry()


def feed_schema_frame(pkt, now_s: float = 0.0) -> None:
    """스키마를 실어 오는 프레임이면 레지스트리에 먹인다. 아니면 아무 일도 안 한다."""
    if pkt.module_id == _schema.MODULE_ID_USER_META:
        SCHEMA_REG.feed_0xEF(pkt.payload)
    elif pkt.module_id == _schema.MODULE_ID_SCHEMA_DESC:
        SCHEMA_REG.feed_0xEE(pkt.payload, now_s)


# 값의 타입까지 아는 출처 — 0xEE 스키마, 그리고 0x20 의 생성된 맵. 이 둘로만 "이 숫자는
# 맞다" 고 말할 수 있다. 0xEF(이름만)나 fallback 은 float32 **가정**이다.
_TYPED_SOURCES = ("0xEE", "generated-map")


def decode_user_frame(pkt):
    """사용자 module 프레임 하나 -> (채널 이름들, 값들, 해석 지문).

    **타입을 아는 출처(`0xEE` 스키마)가 있으면 그걸로 푼다.** 없을 때만 payload 를
    float32 배열로 가정한다 — 이 가정은 **사용자 모듈에서만 안전**하다. 0x20 은 이 가정으로
    풀면 안 되므로 `decode_system_values()` 가 따로 맡는다.

    이 함수가 없던 동안 화면·CSV 는 언제나 `pkt.as_float32()` 만 썼다. 그래서 `0xEE` 가
    도착해도 **이름만 맞고 값은 틀리는** 상태가 됐다 — 정수 필드가 float 로 재해석되니
    숫자는 엉망인데 열 제목이 그럴듯해서 오히려 더 속기 쉽다 (2026-09-10 적대 감사 #7).

    해석 지문은 "이 값들을 어떤 규칙으로 풀었나" 다. `0xEE` 스키마로 풀었으면 그 스키마를
    식별하는 문자열(struct 이름 · 크기 · CRC32), float32 로 가정해서 풀었으면 `None`. 같은
    module 의 프레임끼리 지문이 다르면 **채널 수가 같아도 같은 열이 다른 뜻**이다(스키마가
    늦게 도착해 정수 필드가 float32 재해석에서 진짜 정수로 바뀌는 경우). 실시간 CSV 가 이걸
    보고 행을 나눈다. `0xEF` 는 이름만 늦게 붙일 뿐 값을 푸는 규칙을 안 바꾸므로 지문에
    들어가지 않는다.
    """
    cs = SCHEMA_REG.get(pkt.module_id, len(pkt.payload))
    if cs is not None and cs.source in _TYPED_SOURCES:
        vals = cs.decode(pkt.payload)
        if vals is not None and len(vals) == len(cs.names):
            return list(cs.names), list(vals), cs.detail
    floats = pkt.as_float32()
    return get_channel_names(pkt.module_id, len(floats)), list(floats), None


def decode_user_values(pkt):
    """사용자 module(0xF0~0xFE) 프레임 하나 -> (채널 이름들, 값들)."""
    names, values, _key = decode_user_frame(pkt)
    return names, values


def decode_system_values(pkt):
    """시스템 module(현재는 0x20 Total Data) 프레임 하나 -> (채널 이름들, 값들) 또는 None.

    사용자 채널과 달리 **float32 로 가정해서 푸는 폴백이 없다.** 타입을 아는 출처
    (0x20 의 생성된 맵)로 못 풀면 `None` 이다 — 페이로드 크기가 지금 PC 의 맵과 안
    맞는다는 뜻이다. `TotalDataDecoder.accepts()` 가 크기를 엄격하게 보는 이유가
    "느슨하게 받으면 맵이 바뀐 보드를 조용히 잘못 풀게 된다" 이므로, 여기서 float32 로
    받아 주면 이름은 `ch0..chN`, 값은 그럴듯한데 전부 틀린 표가 화면에 뜬다. CLI 의
    `--total-data` 도 못 풀면 그 행을 건너뛴다(2026-09-29 감사).

    호출부는 `None` 을 받으면 `TotalDataTab.show_mismatch()` 로 그 사실을 보여 준다.
    """
    cs = SCHEMA_REG.get(pkt.module_id, len(pkt.payload))
    if cs is None or cs.source not in _TYPED_SOURCES:
        return None
    vals = cs.decode(pkt.payload)
    if vals is None or len(vals) != len(cs.names):
        return None
    return list(cs.names), list(vals)


def _format_user_csv_row(n_channels: int, header_key, floats, decode_key, payload: bytes):
    """채널 값 -> (라이브 CSV 한 행의 값 부분(개행 없이), 값 대신 hex 로 적었는가).

    CSV 헤더는 그 module 의 **첫 프레임**에서 채널 이름으로 정해진다(`ModuleTab` 과
    `run_cli` 둘 다). 그 뒤 풀리는 방식이 달라지면 — 0xEE 스키마가 데이터보다 늦게
    완성되는 게 대표적이다 — 그 열은 헤더가 말하는 것과 다른 값을 담게 된다. 채널 수가
    바뀌면 값 개수가 헤더 열 수와 어긋나 열이 밀리고, 채널 수가 같아도 예전 행은 정수를
    float32 로 잘못 읽은 값, 새 행은 진짜 값인 채로 한 열에 섞인다. 엑셀·pandas 는 그걸
    말없이 한 열로 읽는다.

    그래서 `xmlog_export.export_csv()` 와 같은 규약을 따른다: 채널 수(`n_channels`)나
    해석 지문(`decode_key`, `decode_user_frame` 참조)이 첫 프레임과 다른 행은 값 대신
    원본 payload 를 hex 로 마지막 열에 적고, 열 수는 헤더와 같게 유지한다. 그런 행은
    `.xmlog` 를 `xm10 export` 로 다시 뽑으면 제대로 풀린다(2026-09-29 감사).
    """
    if len(floats) != n_channels or decode_key != header_key:
        pad = [""] * max(0, n_channels - 1)
        return ",".join(pad + [payload.hex()]), True
    return ",".join(f"{v:.6f}" for v in floats), False


def get_channel_names(mid: int, n: int) -> list:
    """이름의 출처 우선순위: FW 가 보낸 스키마 > 하드코딩 표 > ch0.."""
    cs = SCHEMA_REG.get(mid, n * 4)
    if cs is not None and cs.source in ("0xEE", "0xEF") and len(cs.names) == n:
        return list(cs.names)
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

    def __init__(self, port_name, baudrate=DEFAULT_BAUD, capture=None, parent=None):
        super().__init__(parent)
        self.port_name = port_name
        self.baudrate = baudrate
        # 무손실 저장(.xmlog). GUI 스레드가 만들어 넘기고, 쓰기와 닫기는 **이 워커에서만**
        # 한다 — 파일 하나를 두 스레드가 만지지 않게. 화면 큐(deque)보다 **앞**에서
        # 적으므로 화면이 밀려 프레임을 버려도 파일은 온전하다.
        self.capture = capture
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
            if self.capture is not None:
                try:
                    self.capture.close()
                except Exception as e:   # 저장 실패가 종료 신호까지 막으면 창이 굳는다
                    self.status_msg.emit(f".xmlog close failed: {e}")
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

        # 스키마 프레임이면 여기서 먹인다 — 큐에 넣기 전에 해야
        # 뒤따르는 데이터 프레임이 이름을 갖고 화면에 올라간다.
        feed_schema_frame(pkt, pkt.recv_t)

        # 무손실 저장 — 라우팅·큐보다 먼저. 화면에 안 그리는 시스템 채널도 파일에는
        # 남아야 하고, 아래 큐 오버플로로 버려질 프레임도 파일에는 있어야 한다.
        # CLI(run_cli)와 같은 자리다.
        if self.capture is not None:
            try:
                self.capture.on_frame(pkt, int(recv_t * 1e6))
            except Exception as e:
                # 디스크가 찼거나 파일이 잠긴 경우. 수신은 계속하되 저장은 여기서 끝낸다 —
                # 조용히 계속 실패하면 사용자는 파일이 있는 줄 안다.
                self.status_msg.emit(f".xmlog write failed — 저장 중단: {e}")
                self.capture = None

        # 시퀀스 회계와 module 판정은 큐에 넣기 전에 끝낸다.
        # 바로 아래 오버플로로 버려질 프레임도 '와이어로는 도착한' 프레임이다.
        route_tag, _module_id, _delta = self.router.route(pkt)

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
# Module Tab — 사용자 module 하나의 화면 (버퍼 · 6-plot · 사이드바 · CSV)
# ============================================================================
#
# 2026-09-15 "Phase E-live" 다중 module 실시간 뷰 (PLAN rev4.2 §4.11, 사용자 승인).
# 예전엔 MainWindow 가 이 상태(버퍼·플롯·사이드바·CSV)를 전부 직접 들고 있었고,
# 그 전제가 "화면에 사용자 module 은 하나뿐" 이었다. FrameRouter 가 이제 사용자
# module 을 전부 'user' 로 라우팅하므로, module_id 하나가 관측될 때마다 이 위젯을
# 하나 만들어 QTabWidget 에 얹는다. 보이지 않는 탭도 handle_packet() 은 계속
# 호출된다 — 그리기만 건너뛴다(Freeze 와 같은 원칙, 스펙 §2).

class ModuleTab(QtWidgets.QWidget):
    """사용자 module 하나 전용 화면. MainWindow 는 module_id 별로 이거 하나씩만 만든다."""

    def __init__(self, module_id: int, window_size: int, theme: dict, parent=None):
        super().__init__(parent)
        self.module_id = module_id
        self._window_size = window_size

        # 링 버퍼는 하나다. 예전엔 _buf_a/_buf_b 두 개를 만들었지만 서로 바꿔 끼우는
        # 코드가 없어 하나는 늘 빈 채였다 — 탭이 module 마다 생기니 그 낭비도 module
        # 수만큼 곱해졌다(2026-09-29 감사).
        self._buf = np.full((window_size, 1 + MAX_CHANNELS), np.nan, dtype=np.float32)
        self._write_idx = 0
        self.frame_count = 0        # 상태 패널이 읽는다 (router.user_modules 와는 별개 카운터)

        self._n_channels = 0
        self._ch_names = []
        self._ch_visible = []
        self._plot_groups = []

        self._log_file = None
        self._pending = []
        self._written = 0
        # 헤더를 정한 첫 프레임의 해석 지문 (decode_user_frame 참조). 이후 프레임이 채널 수나
        # 지문이 다르면 CSV 에 값 대신 hex 로 적고 아래 횟수를 센다.
        self._decode_key = None
        self._mismatched = 0
        self.csv_path = None

        self._plot_widgets = []
        self._plot_curves = []      # [[(ch_idx, curve), ...], ...] per plot
        self._crosshairs = []
        self._ch_checks = []
        self._ch_val_labels = []
        self._mouse_proxies = []    # SignalProxy 를 들고 있어야 GC 로 죽지 않는다

        self._build_ui()
        self._apply_plot_theme(theme)

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        root = QtWidgets.QHBoxLayout(self)
        root.setContentsMargins(2, 2, 2, 2)
        root.setSpacing(4)

        sidebar_w = QtWidgets.QWidget()
        sidebar_w.setFixedWidth(200)
        self._sidebar_layout = QtWidgets.QVBoxLayout(sidebar_w)
        self._sidebar_layout.setContentsMargins(2, 2, 2, 2)
        self._sidebar_layout.setSpacing(1)

        bold = QtGui.QFont()
        bold.setBold(True)
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
        root.addWidget(scroll)

        grid = QtWidgets.QGridLayout()
        grid.setSpacing(4)
        root.addLayout(grid, stretch=1)

        for i in range(6):
            pw = pg.PlotWidget(useOpenGL=True)
            pw.showGrid(x=True, y=True, alpha=0.15)
            pw.setClipToView(True)
            pw.setLimits(xMin=0)
            pw.setLabel("bottom", "device time (s)  [1 tick = 1 ms]")
            pw.setLabel("left", "Value")
            pw.setTitle(f"Plot {i + 1}")

            vline = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkPen('#ef4444', width=1, style=Qt.DashLine))
            hline = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('#ef4444', width=1, style=Qt.DashLine))
            vline.setVisible(False)
            hline.setVisible(False)
            pw.addItem(vline, ignoreBounds=True)
            pw.addItem(hline, ignoreBounds=True)
            self._crosshairs.append((vline, hline))

            proxy = pg.SignalProxy(pw.scene().sigMouseMoved, rateLimit=30,
                                   slot=lambda evt, idx=i: self._on_mouse_moved(evt, idx))
            self._mouse_proxies.append(proxy)

            self._plot_widgets.append(pw)
            self._plot_curves.append([])
            r, c = i // 3, i % 3
            grid.addWidget(pw, r, c)

        for i in range(1, 6):
            self._plot_widgets[i].setXLink(self._plot_widgets[0])

    def _apply_plot_theme(self, theme: dict):
        for pw in self._plot_widgets:
            pw.setBackground(theme['plot_bg'])
            for axis_name in ('bottom', 'left'):
                pw.getAxis(axis_name).setPen(pg.mkPen(theme['axis_pen']))
                pw.getAxis(axis_name).setTextPen(theme['axis_text'])

    # ------------------------------------------------------------------ Channel sidebar
    def _rebuild_sidebar(self, ch_names):
        while self._ch_container.count():
            item = self._ch_container.takeAt(0)
            w = item.widget()
            if w:
                w.deleteLater()

        self._ch_checks = []
        self._ch_val_labels = []
        for name in ch_names:
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
    def _setup_plots(self, ch_names):
        self._n_channels = len(ch_names)
        self._ch_names = ch_names
        self._ch_visible = [True] * self._n_channels
        self._plot_groups = build_plot_groups(self.module_id, ch_names)
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

    def stop_mouse_tracking(self):
        """마우스 십자선 추적을 끊는다. 탭을 지우기(`deleteLater`) 전에 부른다.

        `SignalProxy` 는 마우스가 그래프 위에 있을 때 30Hz 로 슬롯을 부르는 타이머를 갖는다.
        탭이 지워진 뒤에 그 타이머가 마지막으로 한 번 더 울리면 `_on_mouse_moved` 가 이미
        지워진 그래프를 건드려 RuntimeError 가 난다(콘솔에 traceback 이 찍힌다). 미리 끊어
        두면 그 호출 자체가 없다.
        """
        for proxy in self._mouse_proxies:
            proxy.disconnect()

    # ------------------------------------------------------------------ CSV
    def open_csv(self, path):
        self._log_file = open(path, 'w', encoding='utf-8', newline='')
        self.csv_path = path
        self._pending = []
        self._written = 0
        # 채널 구성을 이미 알면(재사용 등) 헤더를 바로 쓴다. 보통은 아직 몰라서
        # 첫 handle_packet() 에서 쓴다.
        if self._n_channels:
            self._write_csv_header(self._ch_names)

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

    def close_csv(self):
        if self._log_file:
            self._flush_csv()
            try:
                self._log_file.flush()
                self._log_file.close()
            except Exception:
                pass
            self._log_file = None

    # ------------------------------------------------------------------ 데이터 반영
    def handle_packet(self, pkt, ch_names, floats, decode_key=None):
        """버퍼 · CSV 에 프레임 하나를 반영한다. 보이지 않는 탭도 호출된다.

        `decode_key` 는 `decode_user_frame()` 이 돌려준 해석 지문이다.
        """
        if self._n_channels == 0:
            self._setup_plots(ch_names)
            self._write_csv_header(ch_names)
            self._decode_key = decode_key

        ws = self._window_size
        idx = self._write_idx % ws
        buf = self._buf
        buf[idx, 0] = pkt.device_time_s
        n = min(len(floats), MAX_CHANNELS)
        buf[idx, 1:1 + n] = floats[:n]
        self._write_idx += 1
        self.frame_count += 1

        if self._log_file:
            vals, as_hex = _format_user_csv_row(
                self._n_channels, self._decode_key, floats, decode_key, pkt.payload)
            if as_hex:
                self._mismatched += 1
            self._pending.append(
                f"{pkt.device_time_s:.6f},{pkt.recv_t:.6f},"
                f"{pkt.seq_id},{pkt.module_id},{pkt.tx_drops},{vals}\n")
            self._written += 1
            if len(self._pending) >= FLUSH_EVERY:
                self._flush_csv()

    def update_latest_labels(self, floats):
        for i, lbl in enumerate(self._ch_val_labels):
            if i < len(floats):
                lbl.setText(f"{floats[i]:.3f}")

    # ------------------------------------------------------------------ Render (zero-copy)
    def render(self):
        buf = self._buf
        ws = buf.shape[0]
        total = self._write_idx
        n = min(total, ws)
        if n == 0:
            return

        if total <= ws:
            data = buf[:n]
        else:
            start = total % ws
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


class TotalDataTab(QtWidgets.QWidget):
    """"Show 0x20 tab" 체크박스가 만드는 요약 탭.

    197채널을 6-plot 으로 그리는 건 무겁다 — 그래서 그래프 대신 표(채널명 + 최신값)
    하나로 대체하고, 갱신을 5Hz 로 스로틀한다(TOTAL_DATA_TABLE_HZ). CSV 는 쓰지 않는다
    — CLI `--total-data` 가 이미 그 역할을 하고, `.xmlog` 에는 어차피 전부 남는다.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._last_update_t = -1.0
        self._names = []

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        info = QtWidgets.QLabel(
            f"0x20 Total Data — 요약 표 ({TOTAL_DATA_TABLE_HZ:.0f}Hz 갱신, 그래프 아님)")
        layout.addWidget(info)

        # 표를 못 채울 때(맵 불일치) 이유를 적는 자리. 평소에는 숨어 있다.
        self._warn = QtWidgets.QLabel("")
        self._warn.setWordWrap(True)
        self._warn.setStyleSheet("color: #ef4444; font-weight: bold;")
        self._warn.setVisible(False)
        layout.addWidget(self._warn)

        self._table = QtWidgets.QTableWidget(0, 2)
        self._table.setHorizontalHeaderLabels(["Channel", "Value"])
        self._table.horizontalHeader().setStretchLastSection(True)
        self._table.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self._table.verticalHeader().setVisible(False)
        layout.addWidget(self._table)

    def allow_update(self, now_t: float) -> bool:
        """5Hz 스로틀 — True 를 받은 호출부만 디코드하고 표를 갱신한다.

        True 를 돌려주는 순간 다음 주기가 시작된다(호출할 때마다 시계가 움직이는 게이트).
        스로틀이 표를 채우는 쪽에만 있으면 표는 5Hz 로 바뀌어도 그 앞의 197채널 unpack·
        스케일링은 poll 틱마다(약 33Hz) 계속 돌기 때문에, 디코드 **앞**에서 물어야 한다
        (2026-09-29 감사). `now_t` 는 단조 증가하는 아무 시계(perf_counter 등)다.
        """
        if now_t - self._last_update_t < TOTAL_DATA_TABLE_PERIOD_S:
            return False
        self._last_update_t = now_t
        return True

    def feed(self, names, values):
        """표를 갱신한다. 스로틀은 `allow_update()` 가 이미 했다 — 여기서 다시 안 한다."""
        self._warn.setVisible(False)

        if self._names != names:
            self._names = list(names)
            self._table.setRowCount(len(names))
            for i, n in enumerate(names):
                item = QtWidgets.QTableWidgetItem(n)
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                self._table.setItem(i, 0, item)

        for i, v in enumerate(values):
            text = "%.6g" % v if isinstance(v, float) else str(v)
            item = self._table.item(i, 1)
            if item is None:
                item = QtWidgets.QTableWidgetItem(text)
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                self._table.setItem(i, 1, item)
            else:
                item.setText(text)

    def show_mismatch(self, n_bytes: int):
        """`decode_system_values()` 가 `None` 을 돌려줬을 때(맵 불일치) 쓴다.

        payload 를 float32 로 뭉개면 그럴듯한 숫자가 나와서, 실제로는 다른 맵인데도
        맞게 풀린 것처럼 보인다. 그래서 값 대신 **못 풀었다는 사실**을 보여 준다.
        표는 비운다 — 옛 값이 남으면 그것도 맞는 값처럼 읽힌다.
        """
        self._names = []
        self._table.setRowCount(0)
        self._warn.setText(
            "⚠ map mismatch — 받은 0x20 payload 는 %d B 입니다. 이 PC 의 데이터 맵과 "
            "크기가 달라 값을 표시하지 않습니다 (FW 와 PC 도구의 버전을 맞춰 보세요)." % n_bytes)
        self._warn.setVisible(True)


# ============================================================================
# Main Window
# ============================================================================

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PhAI V2.2 — USB-CDC Real-time Receiver")
        self.resize(1500, 950)

        self._theme = _MODERN_LIGHT

        # Data state — module_id 별 화면은 전부 ModuleTab 에 있다(_tabs). MainWindow 는
        # 탭 생성 순서·전역 카운터·연결 상태만 갖는다 (2026-09-15 다중 탭 리팩토링).
        self._window_size = DEFAULT_WINDOW_SAMPLES
        self._tabs = {}          # module_id -> ModuleTab, 처음 관측 순서
        self._total_tab = None   # "Show 0x20 tab" 체크 시에만 존재
        self._total_recv = 0     # 사용자 채널 프레임 수 (시스템 프레임 제외, 전체 module 합)

        # 폴링 상태 (_pop_batch / _apply_batch / _render_current 참조)
        self._carry = []         # 큐에서 꺼냈지만 아직 탭에 못 넣은 프레임 — 새 탭 만들기를 다음 틱으로 미룬 것
        self._unrendered = set()  # 새 프레임을 받았는데 아직 그래프에 반영 안 된 module_id
        self._last_render_t = float("-inf")
        self._backlog_since = None  # 큐가 밀리기 시작한 시각 (밀려 있지 않으면 None)

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
        self._port_was_lost = False   # 포트가 끊긴 뒤 아직 다시 연결하지 않았다 (_start_connection 참조)

        # CSV — 폴더/시각도장만 여기서 정한다. 실제 파일은 module 별로
        # ModuleTab.open_csv() 가 그 module 의 첫 프레임에서 연다(모듈을 미리 알 수 없다).
        self._csv_folder = None
        self._csv_stem = None
        self._last_log_path = None     # Review CSV 폴백용 — 가장 최근에 연 CSV
        self._xmlog_path = None

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

        # 무손실 저장. CSV 는 화면에 그리는 채널을 **해석한 결과**라 스키마가 늦게 오거나
        # 디코더에 버그가 있으면 그걸로 끝이다. .xmlog 는 받은 바이트를 그대로 눕히고
        # CSV 는 나중에 다시 뽑는다(xm10 export). 기본 ON — 이 도구를 만든 이유다.
        self._chk_xmlog = QtWidgets.QCheckBox("Save .xmlog")
        self._chk_xmlog.setChecked(True)
        self._chk_xmlog.setToolTip("받은 프레임을 그대로 무손실 저장 (CSV 와 별개, 같은 폴더)")
        r1.addWidget(self._chk_xmlog)

        # 197채널 0x20 은 기본적으로 화면에 안 그린다(무겁다) — 원하면 요약 표 탭을 띄운다.
        self._chk_total_tab = QtWidgets.QCheckBox("Show 0x20 tab")
        self._chk_total_tab.setChecked(False)
        self._chk_total_tab.setToolTip(
            "0x20 Total Data 요약 표 탭을 켠다 (그래프 아님 — 이름+최신값, 5Hz 갱신)")
        self._chk_total_tab.toggled.connect(self._on_total_tab_toggled)
        r1.addWidget(self._chk_total_tab)

        r1.addSpacing(10)
        self._btn_screenshot = QtWidgets.QPushButton("Screenshot")
        self._btn_screenshot.clicked.connect(self._on_screenshot)
        r1.addWidget(self._btn_screenshot)
        self._btn_theme = QtWidgets.QPushButton("Classic")
        self._btn_theme.clicked.connect(self._toggle_theme)
        r1.addWidget(self._btn_theme)

        # ---- Row 1.5: module 상태 패널 — module_id 별 frames/lost/schema 출처
        self._module_status = QtWidgets.QPlainTextEdit()
        self._module_status.setReadOnly(True)
        self._module_status.setMaximumHeight(70)
        self._module_status.setPlaceholderText("(연결 후 사용자 module 이 관측되면 여기 나온다)")
        mono = QtGui.QFont("Consolas")
        mono.setStyleHint(QtGui.QFont.Monospace)
        mono.setPointSize(9)
        self._module_status.setFont(mono)
        root.addWidget(self._module_status)

        # ---- Row 2: info bar
        r2 = QtWidgets.QHBoxLayout()
        root.addLayout(r2)
        bold = QtGui.QFont()
        bold.setBold(True)

        self._lbl_module = QtWidgets.QLabel("Modules: —")
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

        # ---- Body: module_id 별 탭. 탭 하나 = ModuleTab(사이드바 + 6-plot) 하나.
        self._tab_widget = QtWidgets.QTabWidget()
        root.addWidget(self._tab_widget, stretch=1)

        self._status_bar = self.statusBar()
        self._status_bar.showMessage("Ready — PhAI V2.2 Protocol")

    def _apply_plot_theme(self):
        for tab in self._tabs.values():
            tab._apply_plot_theme(self._theme)

    # ------------------------------------------------------------------ 탭 관리
    def _tab_title(self, mid: int) -> str:
        """탭 제목. 우선순위: 0xEE struct_name > 0xEF 단일 채널 이름 > 그냥 0xF0.

        `0xEF` 는 채널별 이름/단위만 나른다 — module 전체를 가리키는 이름 필드가
        와이어에 없다. 그래서 2순위는 '채널이 하나뿐인 0xEF module' 에서만 의미가
        있다(그 하나가 사실상 module 이름 구실을 한다). 채널이 여럿이면 대표할
        이름이 없으므로 3순위(그냥 module_id)로 내려간다.
        """
        base = "0x%02X" % mid
        name = SCHEMA_REG.struct_name(mid) or SCHEMA_REG.meta_single_name(mid)
        return f"{base} · {name}" if name else base

    def _refresh_tab_titles(self):
        """스키마가 데이터보다 늦게 올 수 있다 — 탭을 만든 뒤에도 제목이 바뀔 수 있다."""
        for mid, tab in self._tabs.items():
            idx = self._tab_widget.indexOf(tab)
            if idx < 0:
                continue
            title = self._tab_title(mid)
            if self._tab_widget.tabText(idx) != title:
                self._tab_widget.setTabText(idx, title)

    def _create_tab(self, mid: int) -> ModuleTab:
        tab = ModuleTab(mid, self._window_size, self._theme)
        self._tabs[mid] = tab
        self._tab_widget.addTab(tab, self._tab_title(mid))

        if self._csv_folder is not None:
            path = os.path.join(self._csv_folder, "%s_%s.csv" % (self._csv_stem, _module_label(mid)))
            try:
                tab.open_csv(path)
                self._last_log_path = path
            except Exception as e:
                self._status_bar.showMessage(f"CSV open failed for 0x{mid:02X}: {e}")

        self._update_module_count_label()
        return tab

    def _update_module_count_label(self):
        n = len(self._tabs)
        if n == 0:
            self._lbl_module.setText("Modules: —")
        else:
            ids = ", ".join("0x%02X" % m for m in self._tabs)
            self._lbl_module.setText(f"Modules: {n} ({ids})")

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
            old_worker = self._worker
            if old_worker is not None:
                old_worker.stop()
            if self._serial_thread is not None:
                self._serial_thread.quit()
                self._serial_thread.wait()
            # 이전 연결의 탭은 곧 버려진다 — 큐에 남은 프레임까지 CSV 에 반영하고 닫아 둔다.
            self._drain_remaining(old_worker)
            self._worker = None
            self._serial_thread = None

        try:
            self._open_log_session()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))
            return

        # 채널 이름(0xEF)은 보드가 **USB 가 새로 잡힐 때** 한 번 보낸다 (펌웨어: USB
        # device-ready 상승 에지, 또는 XM_SetUsbCustomMeta() 호출). 타입까지 알려 주는
        # 0xEE 스키마는 지금 펌웨어가 아직 안 보낸다. COM 포트를 닫았다 다시 여는
        # Disconnect -> Connect 는 그 에지가 아니라서 보드는 다시 보내지 않는다 — 그때
        # 비우면 이름·탭 제목·CSV 열 이름이 돌아오지 않는다. 그래서 USB 가 다시
        # 잡혔을 법한 경우에만 비운다: 포트가 끊겼다 되돌아온 재연결이거나, 이전과 다른 포트
        # (=다른 보드일 수 있다)일 때. 안 비우면 다른 보드가 스키마를 안 보낼 때 예전 스키마가
        # 계속 적용돼 값과 탭 제목이 조용히 틀린다. 같은 포트로 수동 재연결하면 그대로 둔다.
        # 한계 세 가지 (포트 이름만으로는 가를 수 없다): 그 사이 보드를 뽑았다 꽂았는데 COM
        # 번호가 같으면, 새 보드가 이름을 보내지 않는 module 은 옛 이름이 남는다. 반대로 포트가
        # 끊겼다고 했지만 USB 는 그대로였다면(읽기 오류만 났다면) 이름이 돌아오지 않는다.
        # 또 다른 포트로 한 번 Connect 했다가(열기에 실패했거나 다른 장치였어도) 원래 보드의
        # 포트로 돌아오면 두 번 다 "다른 포트" 라서 비워지고, 그 보드는 다시 보내지 않으므로
        # 보드를 다시 꽂기 전까지 이름이 돌아오지 않는다(값은 맞다).
        # 이전 워커는 이미 멈춘 뒤에 여기로 온다.
        if port != self._last_port or self._port_was_lost:
            self._forget_schema()
        self._port_was_lost = False

        self._reset_state()
        self._last_port = port

        # .xmlog 는 CSV 와 같은 폴더·같은 시각 도장. 여기서 열어 SESSION 을 적고 워커에
        # 넘긴다 — 이후 쓰기·닫기는 워커 스레드 몫이다 (PhAISerialWorker 주석 참조).
        #
        # 연결(재연결 포함)마다 **새 파일**이다. 다시 붙은 장치가 아까 그 장치인지 알 방법이 아직
        # 없어서(PLAN §9 D-H), 한 파일에 이어 쓰지 않고 파일로 끊는다 — 새 파일은 스키마도 처음부터
        # 다시 배운다. 이름이 초 단위 시각이라 1초 안에 다시 붙으면 같은 이름이 나오는데, 그때
        # 앞 파일을 덮어쓰지 않게 `unique_path` 로 피한다.
        # SESSION 의 total_data_map_version 은 비워 둔다 — CLI 는 --total-data 일 때만 적는다
        # (참고용 문자열이라 값 해석에는 안 쓰인다. `XmLogCapture` docstring 참조).
        capture = None
        self._xmlog_path = None
        if self._chk_xmlog.isChecked():
            folder = self._csv_folder or (self._edit_folder.text().strip() or os.path.abspath("data"))
            stamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            try:
                capture = XmLogCapture(unique_path(os.path.join(folder, f"cdc_{stamp}.xmlog")))
                self._xmlog_path = capture.path
            except Exception as e:
                self._close_all_csv()
                QtWidgets.QMessageBox.critical(self, "Error", f".xmlog 를 열 수 없다: {e}")
                return

        self._serial_thread = QtCore.QThread()
        self._worker = PhAISerialWorker(port, capture=capture)
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
        self._close_all_csv()
        self._btn_conn.setEnabled(True)
        self._btn_disc.setEnabled(False)
        QtWidgets.QMessageBox.critical(self, "Connection Failed", msg)

    def _on_worker_finished(self):
        # 세션 내내 보여준 'Good' 은 파싱 성공한 '전체' 프레임 수인데
        # _total_recv 는 사용자 채널만 센다. 둘 다 적어야 수천 개가 사라진 것처럼 안 보인다.
        total_frames = self._worker.good if self._worker is not None else self._total_recv
        # 워커가 닫기 전에 요약을 읽어 둔다 — finally 에서 close 된 뒤에도 카운터는 남는다.
        cap = self._worker.capture if self._worker is not None else None
        if self._serial_thread:
            self._serial_thread.quit()
            self._serial_thread.wait()
            self._serial_thread = None
        # 워커가 멈췄으니 큐는 더 늘지 않는다. poll 이 아직 못 가져간 프레임까지 반영하고 닫는다.
        self._drain_remaining(self._worker)
        self._worker = None
        # 밀려 있던 동안 건너뛴 마지막 그리기를 여기서 한다 — 안 그러면 화면이 끝에서 조금 뒤에 멈춘다.
        self._render_current(behind=False)
        written = self._close_all_csv()
        self._btn_conn.setEnabled(True)
        self._btn_disc.setEnabled(False)
        msg = (f"Disconnected — {total_frames} frames ({self._total_recv} user, "
               f"{len(self._tabs)} module), {written} lines saved")
        if cap is not None:
            msg += (f"  |  .xmlog: {cap.frames} frames, {cap.w.bytes_written // 1024} KB"
                    f" → {os.path.basename(cap.path)}   (CSV 로 뽑기: xm10 export)")
        elif self._xmlog_path is not None:
            msg += "  |  .xmlog 저장이 도중에 중단됐다 (상태 표시줄 이전 메시지 참조)"
        self._status_bar.showMessage(msg)

    # Auto-reconnect
    def _on_port_lost(self):
        self._status_bar.showMessage("Port lost — attempting reconnect...")
        self._port_was_lost = True      # USB 가 다시 잡히면 보드가 채널 설명을 다시 보낸다
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

    def _forget_schema(self):
        """지금까지 받은 채널 설명(0xEE 스키마 · 0xEF 이름)을 잊고 빈 레지스트리로 시작한다.

        언제 부르는지는 `_start_connection` 이 정한다. 수신 스레드가 이미 멈춘 뒤에만 부른다 —
        레지스트리를 바꿔 끼우는 순간에 옛 워커가 옛 것을 쓰고 있으면 안 된다.
        """
        global SCHEMA_REG
        SCHEMA_REG = _schema.SchemaRegistry()

    def _reset_state(self):
        self._window_size = self._slider_win.value()

        # 이전 연결의 탭을 전부 버린다. 버리기 전에 CSV 부터 닫는다: 탭마다 디스크에 아직 안
        # 나간 행이 최대 FLUSH_EVERY-1 개 있고, 탭 객체와 함께 사라지면 말없이 유실된다
        # (연결된 채로 다시 Connect 하는 경로가 실제로 여기로 온다). 이미 닫힌 탭에는 아무 일도
        # 안 하므로 몇 번 불러도 안전하다.
        self._close_all_csv()
        # removeTab 은 페이지를 탭 목록에서 뺄 뿐 지우지 않는다 — 그대로 두면 연결할 때마다
        # 탭당 OpenGL 그래프 6개가 창이 살아 있는 동안 쌓인다. 지우기 전에 마우스 추적부터
        # 끊는다: 보류 중이던 시그널이 지워진 그래프를 건드리면 RuntimeError 가 난다.
        for tab in self._tabs.values():
            tab.stop_mouse_tracking()
        while self._tab_widget.count():
            page = self._tab_widget.widget(0)
            self._tab_widget.removeTab(0)
            page.deleteLater()
        self._tabs.clear()
        self._total_tab = None
        self._chk_total_tab.setChecked(False)
        self._update_module_count_label()
        self._module_status.setPlainText("")
        self._carry = []
        self._unrendered.clear()
        self._last_render_t = float("-inf")
        self._backlog_since = None

        self._total_recv = 0   # 사용자 채널 프레임 수 (시스템 프레임 제외, 전체 module 합)
        self._tp_count = 0
        self._tp_bytes = 0
        self._tp_time = time.perf_counter()

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

    # ------------------------------------------------------------------ "Show 0x20 tab"
    def _on_total_tab_toggled(self, checked):
        if checked:
            if self._total_tab is None:
                self._total_tab = TotalDataTab()
                self._tab_widget.addTab(self._total_tab, "0x20 · Total Data")
        else:
            if self._total_tab is not None:
                idx = self._tab_widget.indexOf(self._total_tab)
                if idx >= 0:
                    self._tab_widget.removeTab(idx)
                self._total_tab.deleteLater()     # removeTab 은 위젯을 지우지 않는다
                self._total_tab = None

    # ------------------------------------------------------------------ Screenshot
    def _on_screenshot(self):
        cur = self._tab_widget.currentWidget()
        if not isinstance(cur, ModuleTab):
            self._status_bar.showMessage("스크린샷: 활성 탭에 그래프가 없다 (사용자 module 탭을 선택할 것)")
            return
        folder = self._edit_folder.text().strip() or "."
        os.makedirs(folder, exist_ok=True)
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        for i, pw in enumerate(cur._plot_widgets):
            if cur._plot_curves[i]:
                path = os.path.join(folder, f"plot_0x{cur.module_id:02X}_{i}_{ts}.png")
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
    def _open_log_session(self):
        """CSV 폴더/시각도장만 정한다. 실제 파일은 module 별로 첫 프레임에서 연다
        (_create_tab) — 어떤 사용자 module 이 올지 연결 시점엔 모른다."""
        folder = self._edit_folder.text().strip() or os.path.abspath("data")
        os.makedirs(folder, exist_ok=True)
        self._csv_folder = folder
        self._csv_stem = f"cdc_phai_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        self._last_log_path = None

    def _close_all_csv(self) -> int:
        """모든 탭의 CSV 를 닫는다. 반환: 전체 탭에 걸쳐 쓰여진 행 수 합."""
        written = 0
        for tab in self._tabs.values():
            written += tab._written
            tab.close_csv()
        return written

    # ------------------------------------------------------------------ Poll (batch drain + render)
    def _pop_batch(self, w) -> list:
        """이번 틱에 탭에 넣을 프레임을 꺼낸다 (최대 MAX_PACKETS_PER_POLL 개).

        지난 틱에 미뤄 둔 프레임(`self._carry`)이 먼저, 그다음이 워커 큐다. 큐를 통째로
        비우면 밀린 프레임이 많을 때 그 한 번이 GUI 스레드를 오래 붙잡는다
        (MAX_PACKETS_PER_POLL 주석 참조). 남은 프레임은 큐에 그대로 두고 다음 틱이
        이어받는다. 버리는 프레임은 없다. `w` 가 None 이면 미뤄 둔 것만 돌려준다.
        """
        batch, self._carry = self._carry, []
        if w is not None:
            q = w.packet_queue
            try:
                for _ in range(MAX_PACKETS_PER_POLL - len(batch)):
                    batch.append(q.popleft())
            except IndexError:
                pass
        return batch

    def _apply_batch(self, batch, max_new_tabs):
        """꺼낸 프레임을 탭(버퍼·CSV)에 반영한다. 화면은 그리지 않는다.

        `max_new_tabs` 개를 넘게 새 탭을 만들어야 하는 프레임을 만나면 거기서 멈추고, 그
        프레임부터 끝까지를 `self._carry` 에 두어 다음 틱이 이어받게 한다 (module 마다 순서는
        그대로다 — 멈춘 뒤의 프레임은 어느 것도 먼저 반영되지 않는다). `None` 이면 한도가 없다.

        반환: (반영한 프레임 수, {module_id: 반영한 마지막 값들}, 반영한 마지막 0x20 프레임 또는 None).
        module_id 별로 마지막 것만 남기는 건 최신값 라벨을 배치당 한 번만 갱신하려는 것이다
        (1kHz 로 QLabel.setText 를 부르면 그 자체가 병목이 된다).
        """
        last_by_mid = {}
        last_total_pkt = None
        new_tabs = 0
        applied = 0

        for pkt, route_tag in batch:
            # 라우팅은 워커 스레드에서 이미 끝났다(큐 앞에서 회계해야 하므로).
            if route_tag == 'user':
                mid = pkt.module_id
                tab = self._tabs.get(mid)
                if tab is None:
                    if max_new_tabs is not None and new_tabs >= max_new_tabs:
                        break               # 탭 만들기는 비싸다 — 이 프레임부터는 다음 틱 몫
                    tab = self._create_tab(mid)
                    new_tabs += 1
                ch_names, floats, key = decode_user_frame(pkt)
                tab.handle_packet(pkt, ch_names, floats, key)
                last_by_mid[mid] = floats
                self._total_recv += 1
            elif (route_tag == 'system' and pkt.module_id == PHAI_MODULE_TOTAL_DATA
                    and self._total_tab is not None):
                # "Show 0x20 tab" 이 켜져 있을 때만 붙든다 — 꺼져 있으면 굳이 안 푼다.
                last_total_pkt = pkt
            applied += 1

        if applied < len(batch):
            self._carry = batch[applied:]
        self._unrendered.update(last_by_mid)
        return applied, last_by_mid, last_total_pkt

    def _drain_remaining(self, w):
        """세션을 닫기 직전에 부른다: 큐와 미뤄 둔 프레임에 남은 것을 전부 탭(버퍼·CSV)에 반영한다.

        poll 이 틱마다 일부만 가져가므로 연결을 끊는 순간 큐에 밀린 프레임이 남아 있을
        수 있다. 그냥 닫으면 CSV 에서 그만큼 빠진다. 이 시점엔 워커가 멈춰 있어 큐가 더
        늘지 않으므로 반드시 끝난다. 화면이 멈춘 게 아니라 세션을 닫는 중이라 새 탭 수에
        한도를 두지 않는다 — 여기서 미루면 그 프레임이 CSV 에서 빠진다.
        """
        while True:
            batch = self._pop_batch(w)
            if not batch:
                return
            self._apply_batch(batch, None)

    def _render_current(self, behind: bool):
        """지금 보이는 탭이 새 프레임을 받았고 아직 안 그렸으면 그린다 (Freeze 면 안 그린다).

        `behind` 는 아직 못 반영한 프레임이 남았다는 뜻이다(상한 때문에 큐에 남았거나, 새 탭을
        만들려고 미뤄 뒀거나). 그동안은 그리는 시간이 큐를 비우는 시간을 잡아먹으므로, 밀리기 시작한 때(또는 마지막으로 그린 때)부터
        `BACKLOG_RENDER_PERIOD_S` 가 지난 뒤에야 그린다. 다 따라잡은 틱에서는 바로 그린다.
        그리지 않은 탭은 `_unrendered` 에 남아 다음 기회에 그려진다.
        """
        now = time.perf_counter()
        if not behind:
            self._backlog_since = None
        elif self._backlog_since is None:
            self._backlog_since = now

        if self._frozen:
            return
        cur = self._tab_widget.currentWidget()
        if not isinstance(cur, ModuleTab) or cur.module_id not in self._unrendered:
            return
        if behind and now - max(self._last_render_t, self._backlog_since) < BACKLOG_RENDER_PERIOD_S:
            return
        cur.render()
        self._unrendered.discard(cur.module_id)
        self._last_render_t = now

    def _on_poll(self):
        w = self._worker
        if w is None:
            return

        batch = self._pop_batch(w)
        if not batch:
            self._update_stats_label(w)
            return
        # 밀렸는지는 꺼낸 **직후**에 본다: 상한까지 꺼냈는데도 큐에 프레임이 남았는가. 틱이
        # 끝난 뒤에 보면 처리하는 동안 워커 스레드가 새로 넣은 프레임 때문에 평소(안 밀린
        # 스트리밍)에도 늘 '밀림' 으로 보여 그래프가 0.5초에 한 번만 갱신된다.
        queue_has_more = len(batch) >= MAX_PACKETS_PER_POLL and bool(w.packet_queue)

        applied, last_by_mid, last_total_pkt = self._apply_batch(batch, MAX_NEW_TABS_PER_POLL)

        done = batch if applied == len(batch) else batch[:applied]
        self._tp_count += applied
        self._tp_bytes += sum(p.wire_len for p, _tag in done)

        for mid, floats in last_by_mid.items():
            self._tabs[mid].update_latest_labels(floats)

        if last_total_pkt is not None and self._total_tab.allow_update(time.perf_counter()):
            # 디코드는 5Hz 게이트를 통과한 뒤에만 한다 — 197채널 unpack+스케일을 poll
            # 틱마다(약 33Hz) 돌리지 않으려는 것이다. 못 풀면 float32 로 덮지 않고
            # '못 풀었다' 를 그대로 보여준다.
            result = decode_system_values(last_total_pkt)
            if result is None:
                self._total_tab.show_mismatch(len(last_total_pkt.payload))
            else:
                names, vals = result
                self._total_tab.feed(names, vals)

        # Update stats + module 상태 패널 + 탭 제목(스키마가 데이터보다 늦게 올 수 있다)
        self._update_stats_label(w)
        self._update_module_status_panel(w)
        self._refresh_tab_titles()

        # Update plots — Freeze 이거나 안 보이는 탭이면 건너뛴다(버퍼·CSV 는 이미 반영됨).
        self._render_current(behind=queue_has_more or bool(self._carry))

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

        # Resync = seq 가 뒤로 간 횟수. 손실이 아니라 기준을 다시 잡은 횟수다.
        # (예전엔 여기 'Other' — primary 아닌 사용자 module 수 — 도 있었다. 사용자
        # module 을 전부 탭으로 받는 지금은 '버려진 나머지' 라는 개념 자체가 없다.)
        extra = ""
        if ledger.resync_count:
            extra += f"  Resync:{ledger.resync_count}"

        self._lbl_stats.setText(
            f"Good: {w.good}  CRC: {w.crc_err}  Sync: {w.sync_err}  "
            f"SEQ↓: {ledger.lost_count}  QOvf: {w.queue_overflow_count}  "
            f"TxDrop: {w.total_tx_drops}  Sys[0x20:{sys20} 0xEF:{sysef}]{extra}  |  "
            f"{self._pkt_rate:.0f} pkt/s  {self._byte_rate / 1024:.1f} KB/s")

    def _schema_tag(self, mid: int, n_channels: int) -> str:
        """module 상태 패널에 붙일 출처 태그. 아직 채널 수를 모르면(프레임 전) '?'."""
        if n_channels <= 0:
            return "[?]"
        cs = SCHEMA_REG.get(mid, n_channels * 4)
        if cs is None:
            return "[?]"
        if cs.source == "0xEE":
            return "[0xEE]"
        if cs.source == "0xEF":
            return "[0xEF]"
        return "[float32 가정]"

    def _update_module_status_panel(self, w):
        """사이드바/상태: module 별 frames / lost(전역 원장) / schema 출처 태그."""
        router = w.router
        # 워커 스레드가 새 module 을 처음 볼 때마다 router.user_modules 에 키를 넣는다.
        # 여기서(GUI 스레드) 그 딕셔너리를 직접 순회하면 그 삽입과 겹칠 때
        # "dictionary changed size during iteration" 으로 죽는다 — 스냅샷으로만 읽는다.
        modules = router.user_modules_snapshot()
        if not modules:
            return
        lost = router.ledger.lost_count
        lines = []
        for mid, frames in modules:
            tab = self._tabs.get(mid)
            n = tab._n_channels if tab is not None else 0
            tag = self._schema_tag(mid, n)
            title = self._tab_title(mid)
            line = ("%-20s  frames=%-8d  lost(global)=%-6d  %s"
                    % (title, frames, lost, tag))
            if tab is not None and tab._mismatched:
                # 채널 수나 해석이 바뀐 뒤의 프레임은 CSV 에 값 대신 hex 로 적혔다는 표시.
                line += "  ⚠ CSV hex행 %d" % tab._mismatched
            lines.append(line)
        self._module_status.setPlainText("\n".join(lines))

    # ------------------------------------------------------------------ Review
    def _open_review(self):
        cur = self._tab_widget.currentWidget()
        path = cur.csv_path if isinstance(cur, ModuleTab) else None
        if not path or not os.path.isfile(path):
            path = self._last_log_path if (self._last_log_path and os.path.isfile(self._last_log_path)) else None
        if not path:
            folder = self._edit_folder.text().strip() or "."
            files = sorted(glob.glob(os.path.join(folder, "cdc_phai_*_user_0x*.csv")),
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
        self._drain_remaining(self._worker)
        self._close_all_csv()
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
            with open(path, 'r', encoding='utf-8') as f:
                self.csv_cols = f.readline().strip().split(',')
                # 값 대신 받은 바이트(hex)를 적은 행은 값 칸이 숫자가 아니다 — 그 칸만 nan 으로
                # 읽는다. 행을 통째로 빼지 않는다: 시간·seq_id·tx_drops 는 멀쩡한 값이다
                # (cdc_csv_reviewer.load_csv 와 같은 이유·같은 판별).
                meta = {'time_s', 'pc_time_s', 'seq_id', 'module_id', 'tx_drops'}
                first_data = next((i for i, n in enumerate(self.csv_cols) if n not in meta),
                                  len(self.csv_cols))
                hex_rows = HexRowFilter(first_data)
                with warnings.catch_warnings():
                    # 값이 하나도 없으면 genfromtxt 가 경고를 내고 빈 배열을 준다 — 아래에서 직접 알린다
                    warnings.simplefilter("ignore", UserWarning)
                    arr = np.genfromtxt(hex_rows(f), delimiter=",")
            if arr.ndim == 1 and arr.size:
                arr = arr.reshape(1, -1)        # 행이 하나뿐이면 1차원으로 온다
            if arr.size == 0 or len(hex_rows.rows) == arr.shape[0]:
                raise ValueError("숫자로 읽을 수 있는 행이 없다")
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", str(e))
            return
        self.data = arr
        self.lbl_path.setText(path)
        info = f"{arr.shape[0]} rows × {arr.shape[1]} cols"
        if hex_rows.rows:
            info += f"  (hex 행 {len(hex_rows.rows)}개는 값이 아니라 받은 바이트라 값 칸을 비웠다)"
        self.info.setText(info)
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

def run_cli(port, baud, output, total_data=False, log=False):
    os.makedirs(output, exist_ok=True)
    stamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    # 사용자 module 별로 CSV 하나씩 — 파일명은 xmlog_export.py 의 _module_label() 규약을
    # 그대로 따른다(예: cdc_phai_<stamp>_user_0xF0.csv). 어떤 module 이 올지 연결
    # 시점엔 모르므로 stem 만 여기서 정하고, 실제 파일은 그 module 의 첫 프레임에서 연다.
    stem = f"cdc_phai_{stamp}"
    print(f"[PhAI V2.2 CLI]  Port: {port}  Output: {output}  (stem: {stem})")

    # 0x20 은 사용자 채널이 아니라 시스템 채널이라 위 CSV 에 섞지 않는다 —
    # 컬럼 의미가 다르고, 197채널을 사용자 CSV 에 밀어 넣으면 헤더가 어긋난다.
    # 별도 파일로 뺀다.
    td_dec = None
    td_path = None
    if total_data:
        if not TOTAL_DATA_MAP_AVAILABLE:
            print("  [total-data] xm_total_data_map.py 가 없어 0x20 디코딩을 건너뛴다.")
        else:
            td_dec = TotalDataDecoder()
            td_path = os.path.join(output, f"cdc_total_{stamp}.csv")
            meta_path = td_path + ".meta.txt"
            with open(meta_path, "w", encoding="utf-8") as mf:
                print("# XM10 Total Data (module_id 0x20) capture", file=mf)
                print(f"port={port} baud={baud}", file=mf)
                print(f"started={datetime.now().isoformat(timespec='seconds')}", file=mf)
                print(td_dec.identity(), file=mf)
                print("", file=mf)
                print("# 주의: 보드가 어느 맵으로 보냈는지는 와이어에 없다.", file=mf)
                print("#       위 fingerprint 는 PC 가 푼 맵이다.", file=mf)
            print(f"  [total-data] {td_path}")
            print(f"  [total-data] {td_dec.identity()}")

    print("  Ctrl+C to stop.")
    try:
        ser = serial.Serial(port, baud, timeout=DEFAULT_TIMEOUT)
    except Exception as e:
        print(f"[ERROR] {e}")
        return

    wire_buf = bytearray()
    good = 0
    errs = 0
    t0 = time.perf_counter()
    last_print = t0
    # GUI 와 똑같은 라우터를 쓴다. 예전에는 CLI 가 파싱을 따로 갖고 있어서
    # 같은 버그(시퀀스 클램프, module 미분리)를 두 번 고쳐야 했다.
    router = FrameRouter()

    # module_id -> [file, header_written, row_count]. 어떤 사용자 module 이 올지
    # 미리 모르므로 첫 프레임에서 연다 — GUI ModuleTab.open_csv() 와 같은 발상이다.
    user_csv = {}

    # 두 파일을 한 with 로 묶지 않는다 — td_out 은 없을 수도 있어서다.
    # 무손실 저장 — 해석하기 전에 먼저 눕힌다. 스키마를 몰라도 적는다.
    cap = None
    if log:
        # GUI 와 같이 unique_path — 같은 초에 두 번 시작해도(보드마다 recv 를 하나씩 띄우는 스크립트
        # 등) 앞 파일을 열자마자 비워 버리지 않는다.
        cap = XmLogCapture(unique_path(os.path.join(output, f"cdc_{stamp}.xmlog")),
                           total_data_map_version=(
                               td_dec.version if td_dec is not None else ""))
        print(f"  [log] {cap.path}")

    td_out = open(td_path, "w", encoding="utf-8") if td_path else None
    td_hdr_written = False
    try:
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
                        feed_schema_frame(pkt, now)
                        if cap is not None:
                            # 라우팅 판정 전에 적는다 — 화면에 안 그리는
                            # 시스템 채널도 파일에는 남아야 한다.
                            cap.on_frame(pkt, int(now * 1e6))
                        route_tag, _mid, _delta = router.route(pkt)
                        if (td_out is not None and route_tag == 'system'
                                and pkt.module_id == PHAI_MODULE_TOTAL_DATA):
                            vals20 = td_dec.scaled(pkt.payload)
                            if vals20 is not None:
                                if not td_hdr_written:
                                    print(td_dec.csv_header(), file=td_out)
                                    td_hdr_written = True
                                row = ','.join('%.6g' % v for v in vals20)
                                print('%.6f,%.6f,%d,%d,%s' % (
                                    pkt.device_time_s, now, pkt.seq_id,
                                    pkt.tx_drops, row), file=td_out)
                                if good % FLUSH_EVERY == 0:
                                    td_out.flush()
                        if route_tag == 'user':
                            mid = pkt.module_id
                            ch, floats, key = decode_user_frame(pkt)
                            entry = user_csv.get(mid)
                            if entry is None:
                                upath = os.path.join(output, "%s_%s.csv" % (stem, _module_label(mid)))
                                uf = open(upath, 'w', encoding='utf-8', newline='')
                                uf.write("time_s,pc_time_s,seq_id,module_id,tx_drops,"
                                         + ",".join(ch) + "\n")
                                # n_cols · key = 헤더를 정한 첫 프레임의 채널 수와 해석 지문.
                                # 이후 프레임이 이와 달라지면(0xEE 스키마가 늦게 완성되는
                                # 등) 열이 밀리거나 뜻이 섞인 행 대신 hex 행으로 적는다 —
                                # GUI 의 ModuleTab 과 같은 규약 (_format_user_csv_row).
                                entry = {"file": uf, "rows": 0, "n_cols": len(ch), "key": key,
                                         "hex_rows": 0}
                                user_csv[mid] = entry
                                print(f"  Module: {get_module_name(mid)} (0x{mid:02X}) "
                                      f"— {len(floats)} ch  -> {upath}")
                            vals, as_hex = _format_user_csv_row(
                                entry["n_cols"], entry["key"], floats, key, pkt.payload)
                            if as_hex:
                                entry["hex_rows"] += 1
                            entry["file"].write(
                                f"{pkt.device_time_s:.6f},{now:.6f},{pkt.seq_id},"
                                f"{mid},{pkt.tx_drops},{vals}\n")
                            entry["rows"] += 1
                            if good % FLUSH_EVERY == 0:
                                entry["file"].flush()
                del wire_buf[:delim + 1]
            t = time.perf_counter()
            if t - last_print >= 2.0:
                s20 = router.system_taps[PHAI_MODULE_TOTAL_DATA].frame_count
                sef = router.system_taps[PHAI_MODULE_USER_META].frame_count
                extra = ""
                if router.ledger.resync_count:
                    extra += f"  Resync:{router.ledger.resync_count}"
                print(f"  Good: {good}  Err: {errs}  Lost(global): {router.ledger.lost_count}  "
                      f"Modules:{len(router.user_modules)}  Sys[0x20:{s20} 0xEF:{sef}]{extra}")
                last_print = t
    except KeyboardInterrupt:
        s20 = router.system_taps[PHAI_MODULE_TOTAL_DATA].frame_count
        sef = router.system_taps[PHAI_MODULE_USER_META].frame_count
        print(f"\n[DONE] {good} packets  "
              f"(Lost(global)={router.ledger.lost_count}, 0x20={s20}, 0xEF={sef})")
        if user_csv:
            print("  user CSV:")
            for mid, e in user_csv.items():
                note = ""
                if e["hex_rows"]:
                    note = f"  ⚠ 채널 수나 해석이 달라 값 대신 hex 로 적은 행 {e['hex_rows']}개"
                print(f"    0x{mid:02X}  {e['rows']} rows  -> {e['file'].name}{note}")
            if any(e["hex_rows"] for e in user_csv.values()):
                print("    (hex 행은 --log 로 .xmlog 를 같이 저장했다면 xm10 export 로 제대로 풀립니다)")
        else:
            print("  user CSV: 없음 (사용자 module 프레임을 못 받았다)")
        # 시스템 프레임 지문 — 개수만으로는 "보존됐다" 를 증명하지 못한다.
        # 두 캡처의 crc32 를 비교하면 같은 바이트였는지 바로 알 수 있다.
        sys_lines = [tap.summary() for tap in router.system_taps.values() if tap.frame_count]
        if sys_lines:
            print("  system frames:")
            for line in sys_lines:
                print(f"    {line}")
        if cap is not None:
            print(f"  log: {cap.summary()}")
            print(f"       CSV 로 뽑기: python CDC/xmlog_export.py {cap.path} --csv data/")
        if td_dec is not None:
            print(f"  total data: {td_dec.summary()}")
            print(f"              {td_path}")
    finally:
        ser.close()
        for e in user_csv.values():
            try:
                e["file"].close()
            except Exception:
                pass
        if td_out is not None:
            td_out.close()
        if cap is not None:
            cap.close()


# ============================================================================
# Entry Point
# ============================================================================

def main():
    ap = argparse.ArgumentParser(description="PhAI V2.2 CDC Receiver")
    ap.add_argument("--cli", action="store_true")
    ap.add_argument("--port", type=str)
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--output", type=str, default="data")
    ap.add_argument("--log", action="store_true",
                    help="받은 프레임을 그대로 .xmlog 에 무손실 저장 "
                         "(스키마를 몰라도 적는다. CSV 는 xmlog_export.py 로 뽑는다)")
    ap.add_argument("--total-data", action="store_true",
                    help="0x20 Total Data Packet 을 생성된 맵으로 디코딩해 "
                         "별도 CSV(cdc_total_*.csv)로 저장 (CLI 전용)")
    args = ap.parse_args()

    if args.cli:
        if not args.port:
            print("--port required"); sys.exit(1)
        run_cli(args.port, args.baud, args.output,
                total_data=args.total_data, log=args.log)
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
