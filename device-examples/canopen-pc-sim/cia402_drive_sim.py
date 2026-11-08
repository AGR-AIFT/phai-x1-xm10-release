#!/usr/bin/env python3
"""XM10 예제 Ex.46 · Ex.47 의 상대 — CANopen(CiA 301 · 402) 속도 모드 드라이브를 PC 에서 흉내 냅니다.
클래식 CAN USB 어댑터 + python-can 으로 돌고, 실제 모터는 없습니다.

흉내 내는 것
  - NMT — 부팅 알림 · 시작 · 정지 · PRE_OPERATIONAL · 노드 재설정 · 통신 재설정(통신 설정을 기본값으로)
  - 하트비트 송신(0x1017) · 하트비트 소비(0x1016:01) — 첫 하트비트를 받은 뒤부터 감시하고, 시간이 넘으면 고장으로 들어가 멈춥니다
  - SDO 서버 — 4 바이트 이하 빠른 전송만(분할 전송은 중단 코드로 거절)
  - CiA 402 상태 기계 — 제어 단어(0x6040) · 상태 단어(0x6041) · 고장 해제(bit 7 이 0 에서 1 로)
  - 속도 — 프로파일 속도(0x6060 = 3, 가속 한도) · 주기 동기 속도(0x6060 = 9, SYNC 마다)
  - PDO — RPDO1 · TPDO1 의 통신 파라미터 · 매핑을 CiA 301 절차로 바꾸고, SYNC 마다 RPDO 를 적용하고 TPDO 를 냅니다
  - 시뮬레이터 표시 0x2F00 = "XSIM" · 하트비트 소비 상태 0x2F01(0 미설정 · 1 첫 하트비트 전 · 2 감시 중)

예
  pip install python-can
  python cia402_drive_sim.py --interface pcan --channel PCAN_USBBUS1
  python cia402_drive_sim.py --interface virtual --channel demo     # 어댑터 없이 이 PC 안에서만(시험용)

버스는 XM10 과 같은 클래식 CAN 속도여야 합니다(기본 1 Mbit/s, 샘플 포인트 87.5 %).
"""

import argparse
import sys
import time

# ---- NMT ----
NMT_START, NMT_STOP, NMT_PREOP, NMT_RESET_NODE, NMT_RESET_COMM = 0x01, 0x02, 0x80, 0x81, 0x82
ST_BOOTUP, ST_STOPPED, ST_OPERATIONAL, ST_PREOP = 0x00, 0x04, 0x05, 0x7F
NMT_NAMES = {ST_BOOTUP: "부팅", ST_STOPPED: "STOPPED", ST_OPERATIONAL: "OPERATIONAL", ST_PREOP: "PRE_OPERATIONAL"}
SYNC_ID = 0x080
TIME_EPS = 1e-6                 # 초 단위 소수 비교의 여유(1.001 - 1.000 이 0.001 보다 조금 작게 나오는 것)

# ---- SDO 중단 코드 (CiA 301) ----
ABORT_CMD = 0x05040001          # 명령 바이트를 모름(분할 전송 등)
ABORT_WRITE_ONLY = 0x06010001
ABORT_READ_ONLY = 0x06010002
ABORT_NO_OBJECT = 0x06020000
ABORT_NOT_MAPPABLE = 0x06040041
ABORT_PDO_LEN = 0x06040042
ABORT_LEN = 0x06070010
ABORT_NO_SUB = 0x06090011
ABORT_RANGE = 0x06090030
ABORT_STATE = 0x08000022

# ---- CiA 402 ----
SOD, RTSO, SO, OE, QSA, FRA, FAULT = "SWITCH_ON_DISABLED", "READY_TO_SWITCH_ON", "SWITCHED_ON", \
    "OPERATION_ENABLED", "QUICK_STOP_ACTIVE", "FAULT_REACTION_ACTIVE", "FAULT"
SW_BASE = {SOD: 0x0040, RTSO: 0x0021, SO: 0x0023, OE: 0x0027, QSA: 0x0007, FRA: 0x000F, FAULT: 0x0008}
SW_VOLTAGE = 0x0010             # bit 4 — 전압 있음
SW_REMOTE = 0x0200              # bit 9 — 원격 제어 중
SW_TARGET_REACHED = 0x0400      # bit 10
MODE_PROFILE_VELOCITY, MODE_CSV = 3, 9
ERR_HEARTBEAT = 0x8130          # 하트비트(생명 감시) 오류
SIM_ID_VALUE = 0x4D495358       # 작은 쪽 바이트부터 "XSIM"
DEVICE_TYPE = 0x00020192        # CiA 402 드라이브

RPDO_MAPPABLE = {0x6040: 16, 0x60FF: 32, 0x6060: 8}
TPDO_MAPPABLE = {0x6041: 16, 0x606C: 32, 0x603F: 16, 0x6061: 8}


class Entry:
    __slots__ = ("size", "signed", "access", "default", "value", "comm")

    def __init__(self, size, access, default=0, signed=False, comm=False):
        self.size, self.access, self.default, self.signed, self.comm = size, access, default, signed, comm
        self.value = default


class Cia402DriveSim:
    """규칙만 담습니다 — 보내기는 send(id, data) 로, 시각(초)은 인자로 받습니다(시험하기 쉽게)."""

    def __init__(self, send, node=1, hb_ms=0, accel=2000.0, csv_tau_s=0.005, log=print, verbose=False):
        self.send = send
        self.node = node
        self.hb_default = hb_ms
        self.accel = accel            # 프로파일 속도의 가속 한도(단위/s)
        self.csv_tau_s = csv_tau_s    # 주기 동기 속도의 따라가는 빠르기
        self.log = log
        self.verbose = verbose
        self.nmt = ST_PREOP
        self.state = SOD
        self.velocity = 0.0
        self.cons_state = 0
        self.prev_cw = 0
        self.rpdo_pending = None
        self.sdo_count = 0
        self.abort_count = 0
        self.tpdo_count = 0
        self.rpdo_count = 0
        self.bootup_count = 0
        self._last_hb_tx = 0.0
        self._last_cons_rx = 0.0
        self._last_phys = 0.0
        self._tpdo_sync_cnt = 0
        self._tpdo_last_async = None
        self.od = {}
        self._build_od()                 # 상태 값을 만든 뒤에(읽기 전용 값을 상태에서 채웁니다)

    # ------------------------------------------------------------------ 객체 사전
    def _build_od(self):
        n = self.node
        od = self.od
        od[(0x1000, 0)] = Entry(4, "const", DEVICE_TYPE, comm=True)
        od[(0x1001, 0)] = Entry(1, "ro", 0)
        od[(0x1006, 0)] = Entry(4, "rw", 0, comm=True)                       # 통신 주기 µs
        od[(0x1016, 0)] = Entry(1, "const", 1, comm=True)
        od[(0x1016, 1)] = Entry(4, "rw", 0, comm=True)                       # (노드 << 16) | 시간 ms
        od[(0x1017, 0)] = Entry(2, "rw", self.hb_default, comm=True)         # 하트비트 송신 주기 ms(0 = 끔)
        od[(0x1018, 0)] = Entry(1, "const", 4, comm=True)
        for s in range(1, 5):
            od[(0x1018, s)] = Entry(4, "const", 0, comm=True)
        for comm_idx, map_idx, base in ((0x1400, 0x1600, 0x200), (0x1800, 0x1A00, 0x180)):
            od[(comm_idx, 0)] = Entry(1, "const", 2, comm=True)
            od[(comm_idx, 1)] = Entry(4, "rw", 0x80000000 | (base + n), comm=True)   # 처음에는 꺼짐(bit 31)
            od[(comm_idx, 2)] = Entry(1, "rw", 0xFF, comm=True)
            od[(map_idx, 0)] = Entry(1, "rw", 0, comm=True)
            for s in range(1, 9):
                od[(map_idx, s)] = Entry(4, "rw", 0, comm=True)
        od[(0x2F00, 0)] = Entry(4, "const", SIM_ID_VALUE)
        od[(0x2F01, 0)] = Entry(1, "ro", 0)
        od[(0x603F, 0)] = Entry(2, "ro", 0)
        od[(0x6040, 0)] = Entry(2, "rw", 0)
        od[(0x6041, 0)] = Entry(2, "ro", 0)
        od[(0x6060, 0)] = Entry(1, "rw", 0, signed=True)
        od[(0x6061, 0)] = Entry(1, "ro", 0, signed=True)
        od[(0x606C, 0)] = Entry(4, "ro", 0, signed=True)
        od[(0x60FF, 0)] = Entry(4, "rw", 0, signed=True)
        self._refresh()

    def _get(self, idx, sub=0):
        return self.od[(idx, sub)].value

    def _set(self, idx, sub, value):
        self.od[(idx, sub)].value = value

    def _refresh(self):
        """읽기 전용 값을 지금 상태로."""
        sw = SW_BASE[self.state] | SW_REMOTE
        if self.state in (RTSO, SO, OE, QSA):
            sw |= SW_VOLTAGE
        if self.state == OE and abs(self._target() - self.velocity) < 1.0:
            sw |= SW_TARGET_REACHED
        self._set(0x6041, 0, sw)
        self._set(0x606C, 0, int(round(self.velocity)))
        self._set(0x6061, 0, self._get(0x6060))
        self._set(0x2F01, 0, self.cons_state)

    def _target(self):
        return float(self._get(0x60FF)) if self.state == OE and self._get(0x6060) in (MODE_PROFILE_VELOCITY, MODE_CSV) else 0.0

    # ------------------------------------------------------------------ 보내기
    def _tx(self, can_id, data):
        return self.send(can_id, bytes(data))

    def _set_nmt(self, st):
        if st != self.nmt:
            self.log("NMT %s → %s" % (NMT_NAMES[self.nmt], NMT_NAMES[st]))
        self.nmt = st

    def _set_state(self, st):
        if st != self.state:
            self.log("드라이브 %s → %s" % (self.state, st))
        self.state = st

    # ------------------------------------------------------------------ 시작 · 재설정
    def _reset(self, app, comm, now):
        for e in self.od.values():
            if (comm and e.comm) or (app and not e.comm):
                e.value = e.default
        if comm:
            self.cons_state = 0
            self.rpdo_pending = None
            self._tpdo_sync_cnt = 0
            self._tpdo_last_async = None
        if app:
            self.state = SOD
            self.velocity = 0.0
            self.prev_cw = 0
        self._last_hb_tx = now
        self._last_phys = now
        self._refresh()

    def boot(self, now):
        """전원을 켠 것처럼 — 모두 기본값 · 부팅 알림 · PRE_OPERATIONAL."""
        self._reset(app=True, comm=True, now=now)
        self._bootup(now)

    def _bootup(self, now):
        self.bootup_count += 1
        self.nmt = ST_PREOP
        self._tx(0x700 + self.node, [ST_BOOTUP])
        self._last_hb_tx = now
        self.log("부팅 알림 — PRE_OPERATIONAL")

    # ------------------------------------------------------------------ 주기 일
    def poll(self, now):
        hb = self._get(0x1017)
        if hb > 0 and now - self._last_hb_tx >= hb / 1000.0 - TIME_EPS:
            self._last_hb_tx = now
            self._tx(0x700 + self.node, [self.nmt])
        cons_node, cons_ms = self._consumer()
        if self.cons_state == 2 and now - self._last_cons_rx > cons_ms / 1000.0:
            self.cons_state = 1                                  # 다음 하트비트를 받으면 다시 감시
            self.log("하트비트 소비 시간 초과 — 노드 0x%02X 의 하트비트가 %d ms 없음" % (cons_node, cons_ms))
            self._comm_error(now)
        self._physics(now)
        self._tpdo_async()
        self._refresh()

    def _physics(self, now):
        dt = max(0.0, now - self._last_phys)
        self._last_phys = now
        target = self._target()
        if self.state == OE and self._get(0x6060) == MODE_CSV:
            self.velocity += (target - self.velocity) * min(1.0, dt / self.csv_tau_s)
        else:
            step = self.accel * dt
            diff = target - self.velocity
            self.velocity = target if abs(diff) <= step else self.velocity + (step if diff > 0 else -step)
        if self.state == QSA and abs(self.velocity) < 1.0:
            self.velocity = 0.0
            self._set_state(SOD)                                 # 빠른 정지 끝
        if self.state == FRA and abs(self.velocity) < 1.0:
            self.velocity = 0.0
            self._set_state(FAULT)

    def _comm_error(self, now):
        """통신 상실 반응 — 움직일 수 있는 상태면 고장으로 들어가 멈추고, OPERATIONAL 이면 PRE_OPERATIONAL 로."""
        if self.state in (RTSO, SO, OE, QSA):
            self._set_state(FRA)
            self._set(0x603F, 0, ERR_HEARTBEAT)
            self._set(0x1001, 0, 0x11)                            # 일반 · 통신 오류
            self._tx(0x080 + self.node, [ERR_HEARTBEAT & 0xFF, ERR_HEARTBEAT >> 8, 0x11, 0, 0, 0, 0, 0])
            self.log("고장 0x%04X — 멈춥니다(제어 단어 0x00 → 0x80 으로 해제)" % ERR_HEARTBEAT)
        if self.nmt == ST_OPERATIONAL:
            self._set_nmt(ST_PREOP)

    def _consumer(self):
        v = self._get(0x1016, 1)
        return (v >> 16) & 0x7F, v & 0xFFFF

    # ------------------------------------------------------------------ 받기
    def on_frame(self, can_id, data, now):
        if can_id == 0x000:
            self._on_nmt(data, now)
        elif can_id == SYNC_ID:
            self._on_sync(now)
        elif can_id == 0x600 + self.node:
            self._on_sdo(data, now)
        elif 0x701 <= can_id <= 0x77F:
            self._on_heartbeat(can_id - 0x700, now)
        elif self.nmt == ST_OPERATIONAL and self._pdo_valid(0x1400) and can_id == (self._get(0x1400, 1) & 0x7FF):
            self._on_rpdo(data, now)
        self._refresh()

    def _on_nmt(self, data, now):
        if len(data) < 2 or data[1] not in (0, self.node):
            return
        cmd = data[0]
        if cmd == NMT_START:
            self._set_nmt(ST_OPERATIONAL)
        elif cmd == NMT_STOP:
            self._set_nmt(ST_STOPPED)
        elif cmd == NMT_PREOP:
            self._set_nmt(ST_PREOP)
        elif cmd == NMT_RESET_NODE:
            self.log("NMT 노드 재설정")
            self._reset(app=True, comm=True, now=now)
            self._bootup(now)
        elif cmd == NMT_RESET_COMM:
            self.log("NMT 통신 재설정 — 통신 설정을 기본값으로")
            self._reset(app=False, comm=True, now=now)
            self._bootup(now)

    def _on_heartbeat(self, node, now):
        cons_node, cons_ms = self._consumer()
        if self.cons_state >= 1 and node == cons_node and cons_ms > 0:
            if self.cons_state == 1:
                self.log("하트비트 소비 — 노드 0x%02X 감시 시작(%d ms)" % (node, cons_ms))
            self.cons_state = 2
            self._last_cons_rx = now

    def _on_sync(self, now):
        if self.nmt != ST_OPERATIONAL:
            return
        if self.rpdo_pending is not None and self._pdo_valid(0x1400) and self._get(0x1400, 2) <= 240:
            data, self.rpdo_pending = self.rpdo_pending, None
            self._apply_rpdo(data, now)                          # 동기 RPDO — SYNC 에서 적용
        self._physics(now)
        self._refresh()
        ttype = self._get(0x1800, 2)
        if self._pdo_valid(0x1800) and 1 <= ttype <= 240:
            self._tpdo_sync_cnt += 1
            if self._tpdo_sync_cnt >= ttype:
                self._tpdo_sync_cnt = 0
                self._send_tpdo()

    def _on_rpdo(self, data, now):
        need = sum(bits for _, _, bits in self._mapping(0x1600)) // 8
        if len(data) < need:
            return                                               # 짧은 RPDO 는 버립니다
        self.rpdo_count += 1
        if self._get(0x1400, 2) <= 240:
            self.rpdo_pending = bytes(data)                      # 다음 SYNC 에 적용
        else:
            self._apply_rpdo(bytes(data), now)                   # 비동기 — 바로

    def _apply_rpdo(self, data, now):
        ofs = 0
        for idx, sub, bits in self._mapping(0x1600):
            size = bits // 8
            value = int.from_bytes(data[ofs:ofs + size], "little", signed=self.od[(idx, sub)].signed)
            ofs += size
            self._write_value(idx, sub, value, now)

    # ------------------------------------------------------------------ PDO
    def _pdo_valid(self, comm_idx):
        return (self._get(comm_idx, 1) & 0x80000000) == 0

    def _mapping(self, map_idx):
        out = []
        for s in range(1, self._get(map_idx, 0) + 1):
            e = self._get(map_idx, s)
            out.append(((e >> 16) & 0xFFFF, (e >> 8) & 0xFF, e & 0xFF))
        return out

    def _tpdo_bytes(self):
        out = b""
        for idx, sub, bits in self._mapping(0x1A00):
            e = self.od[(idx, sub)]
            out += (e.value & ((1 << bits) - 1)).to_bytes(bits // 8, "little")
        return out

    def _send_tpdo(self):
        if self._tx(self._get(0x1800, 1) & 0x7FF, self._tpdo_bytes()):
            self.tpdo_count += 1

    def _tpdo_async(self):
        """비동기 TPDO(전송 형식 254 · 255) — 값이 바뀔 때 OPERATIONAL 에서만."""
        if self.nmt != ST_OPERATIONAL or not self._pdo_valid(0x1800) or self._get(0x1800, 2) < 254:
            return
        data = self._tpdo_bytes()
        if data != self._tpdo_last_async:
            self._tpdo_last_async = data
            self._send_tpdo()

    # ------------------------------------------------------------------ SDO
    def _on_sdo(self, data, now):
        if self.nmt == ST_STOPPED or len(data) < 8:
            return                                               # STOPPED 는 SDO 에 답하지 않습니다
        cs, idx, sub = data[0], data[1] | (data[2] << 8), data[3]
        self.sdo_count += 1
        if cs == 0x80:
            return                                               # 상대가 중단
        if cs == 0x40:
            code, value, size = self._read(idx, sub)
            if code:
                return self._abort(idx, sub, code)
            resp = [0x43 | ((4 - size) << 2), idx & 0xFF, idx >> 8, sub] + list(value.to_bytes(4, "little"))
            return self._tx(0x580 + self.node, resp)
        if (cs & 0xE0) == 0x20:
            if (cs & 0x02) == 0:
                return self._abort(idx, sub, ABORT_CMD)          # 분할 전송은 지원하지 않습니다
            size = 4 - ((cs >> 2) & 0x03) if (cs & 0x01) else None
            code = self._write(idx, sub, data[4:8], size, now)
            if code:
                return self._abort(idx, sub, code)
            if self.verbose:
                self.log("SDO 쓰기 0x%04X:%02X = 0x%X" % (idx, sub, self._get(idx, sub)))
            return self._tx(0x580 + self.node, [0x60, idx & 0xFF, idx >> 8, sub, 0, 0, 0, 0])
        return self._abort(idx, sub, ABORT_CMD)

    def _abort(self, idx, sub, code):
        self.abort_count += 1
        if self.verbose:
            self.log("SDO 중단 0x%04X:%02X — 0x%08X" % (idx, sub, code))
        self._tx(0x580 + self.node, [0x80, idx & 0xFF, idx >> 8, sub] + list(code.to_bytes(4, "little")))

    def _lookup(self, idx, sub):
        if (idx, sub) in self.od:
            return 0, self.od[(idx, sub)]
        return (ABORT_NO_SUB if any(k[0] == idx for k in self.od) else ABORT_NO_OBJECT), None

    def _read(self, idx, sub):
        code, e = self._lookup(idx, sub)
        if code:
            return code, 0, 0
        return 0, e.value & ((1 << (8 * e.size)) - 1), e.size

    def _write(self, idx, sub, raw, size, now):
        code, e = self._lookup(idx, sub)
        if code:
            return code
        if e.access in ("ro", "const"):
            return ABORT_READ_ONLY
        if size is not None and size != e.size:
            return ABORT_LEN
        value = int.from_bytes(bytes(raw[:e.size]), "little", signed=e.signed)
        return self._write_value(idx, sub, value, now)

    def _write_value(self, idx, sub, value, now):
        """값을 씁니다 — 규칙에 어긋나면 중단 코드, 아니면 0."""
        if idx in (0x1400, 0x1800):
            return self._write_pdo_comm(idx, sub, value)
        if idx in (0x1600, 0x1A00):
            return self._write_pdo_map(idx, sub, value)
        if idx == 0x6060:
            if value not in (0, MODE_PROFILE_VELOCITY, MODE_CSV):
                return ABORT_RANGE
        if idx == 0x1016 and sub == 1:
            self._set(idx, sub, value & 0xFFFFFFFF)
            node, ms = self._consumer()
            self.cons_state = 1 if (node and ms) else 0
            self.log("하트비트 소비 설정 — 노드 0x%02X · %d ms (%s)" % (node, ms, "첫 하트비트 기다림" if self.cons_state else "끔"))
            return 0
        if idx == 0x1017:
            self._last_hb_tx = now                               # 새 주기로 다시 셉니다
        self._set(idx, sub, value)
        if idx == 0x6040:
            self._controlword(value & 0xFFFF)
        return 0

    def _write_pdo_comm(self, idx, sub, value):
        if sub == 1:
            value &= 0xFFFFFFFF
            if value & 0x20000000 or (value & 0x7FF) == 0:
                return ABORT_RANGE                               # 29비트 ID · ID 0 은 지원하지 않습니다
            enabling = (value & 0x80000000) == 0
            if enabling and self._pdo_valid(idx) and (value & 0x7FF) != (self._get(idx, 1) & 0x7FF):
                return ABORT_RANGE                               # 켜진 채로 ID 를 바꿀 수 없습니다
            self._set(idx, 1, value)
            self.log("%s %s (ID 0x%03X)" % ("RPDO1" if idx == 0x1400 else "TPDO1", "켬" if enabling else "끔", value & 0x7FF))
            return 0
        if sub == 2:
            if 241 <= value <= 251:
                return ABORT_RANGE
            self._set(idx, 2, value & 0xFF)
            return 0
        return ABORT_NO_SUB

    def _write_pdo_map(self, idx, sub, value):
        comm = 0x1400 if idx == 0x1600 else 0x1800
        if self._pdo_valid(comm):
            return ABORT_STATE                                   # 매핑은 PDO 를 끈 뒤에만 바꿀 수 있습니다
        if sub == 0:
            if value > 8:
                return ABORT_RANGE
            mappable = RPDO_MAPPABLE if idx == 0x1600 else TPDO_MAPPABLE
            total = 0
            for s in range(1, value + 1):
                e = self._get(idx, s)
                m_idx, m_sub, bits = (e >> 16) & 0xFFFF, (e >> 8) & 0xFF, e & 0xFF
                if m_sub != 0 or mappable.get(m_idx) != bits:
                    return ABORT_NOT_MAPPABLE
                total += bits
            if total > 64:
                return ABORT_PDO_LEN
            self._set(idx, 0, value)
            return 0
        if self._get(idx, 0) != 0:
            return ABORT_STATE                                   # 항목은 개수를 0 으로 만든 뒤에만
        self._set(idx, sub, value & 0xFFFFFFFF)
        return 0

    # ------------------------------------------------------------------ CiA 402 상태 기계
    def _controlword(self, cw):
        fault_reset = (cw & 0x80) and not (self.prev_cw & 0x80)
        self.prev_cw = cw
        st = self.state
        if st == FAULT:
            if fault_reset:
                self._set(0x603F, 0, 0)
                self._set(0x1001, 0, 0)
                self._set_state(SOD)
            return
        if st == FRA:
            return
        if (cw & 0x02) == 0:                                     # 전압 끄기
            if st in (RTSO, SO, OE, QSA):
                self._set_state(SOD)
        elif (cw & 0x06) == 0x02:                                # 빠른 정지
            if st == OE:
                self._set_state(QSA)
            elif st in (RTSO, SO):
                self._set_state(SOD)
        elif (cw & 0x07) == 0x06:                                # 끄기(shutdown)
            if st in (SOD, SO, OE):
                self._set_state(RTSO)
        elif (cw & 0x0F) == 0x07:                                # 켜기 · 동작 멈춤
            if st in (RTSO, OE):
                self._set_state(SO)
        elif (cw & 0x0F) == 0x0F:                                # 켜기 + 동작
            if st in (RTSO, SO):
                self._set_state(OE)


def make_timing(can, f_clock_mhz, bitrate):
    """XM10 과 같은 속도 · 샘플 포인트 87.5 %. 80 MHz 면 XM10 프리셋과 같은 값."""
    f_clock = int(f_clock_mhz * 1_000_000)
    if f_clock == 80_000_000 and bitrate in (1_000_000, 500_000, 250_000, 125_000):
        return can.BitTiming(f_clock=f_clock, brp=80_000_000 // (16 * bitrate), tseg1=13, tseg2=2, sjw=2)
    return can.BitTiming.from_sample_point(f_clock=f_clock, bitrate=bitrate, sample_point=87.5)


def parse_args(argv):
    ap = argparse.ArgumentParser(description="XM10 Ex.46 · Ex.47 의 상대 — CANopen 속도 모드 드라이브를 PC 에서 흉내 냅니다")
    ap.add_argument("--interface", required=True, help="python-can 인터페이스 이름 — pcan · kvaser · vector · virtual …")
    ap.add_argument("--channel", required=True, help="채널 — 예: PCAN_USBBUS1 · 0 · demo(virtual)")
    ap.add_argument("--node", type=lambda s: int(s, 0), default=1, help="노드 ID(기본 1 — XM10 예제의 EX46_NODE_ID · EX47_NODE_ID)")
    ap.add_argument("--bitrate", type=int, default=1_000_000, help="클래식 CAN 속도(기본 1000000 — XM10 의 XM_CanClassicBitTiming 과 같게)")
    ap.add_argument("--timing", choices=("xm10", "none"), default="xm10",
                    help="xm10 = 샘플 포인트 87.5 %% 로 어댑터에 넣음(기본) · none = 어댑터 설정을 그대로 씀")
    ap.add_argument("--f-clock-mhz", type=float, default=8.0, help="어댑터의 클래식 CAN 클럭 MHz(기본 8 — PCAN)")
    ap.add_argument("--hb-ms", type=int, default=0, help="켜질 때의 하트비트 송신 주기 ms(0x1017 기본값, 기본 0 = 끔)")
    ap.add_argument("--accel", type=float, default=2000.0, help="프로파일 속도의 가속 한도(단위/s, 기본 2000)")
    ap.add_argument("--verbose", action="store_true", help="SDO 쓰기 · 중단을 모두 보여 줍니다")
    ap.add_argument("--seconds", type=float, default=0.0, help="이만큼 돌고 끝냅니다(0 = Ctrl+C 까지)")
    a = ap.parse_args(argv)
    if not 1 <= a.node <= 127:
        ap.error("--node 는 1 ~ 127")
    return a


def main(argv=None):
    args = parse_args(argv)
    try:
        import can
    except ImportError:
        print("python-can 이 필요합니다: pip install python-can", file=sys.stderr)
        return 2
    kwargs = {"interface": args.interface, "channel": args.channel}
    if args.interface != "virtual" and args.timing == "xm10":
        kwargs["timing"] = make_timing(can, args.f_clock_mhz, args.bitrate)
    bus = can.Bus(**kwargs)

    def send(can_id, data):
        try:
            bus.send(can.Message(arbitration_id=can_id, is_extended_id=False, data=data), timeout=0.01)
            return True
        except can.CanError:
            return False

    def log(text):
        print("[%8.3f] %s" % (time.monotonic() - t_start, text))

    t_start = time.monotonic()
    sim = Cia402DriveSim(send, node=args.node, hb_ms=args.hb_ms, accel=args.accel, log=log, verbose=args.verbose)
    sim.boot(t_start)
    log("노드 0x%02X — SDO 0x%03X · 하트비트 0x%03X · RPDO1 0x%03X · TPDO1 0x%03X (Ctrl+C 로 끝)"
        % (args.node, 0x600 + args.node, 0x700 + args.node, 0x200 + args.node, 0x180 + args.node))
    try:
        while True:
            now = time.monotonic()
            if args.seconds and now - t_start >= args.seconds:
                break
            sim.poll(now)
            msg = bus.recv(timeout=0.0005)
            if msg is not None and not msg.is_extended_id and not msg.is_remote_frame and not msg.is_error_frame:
                sim.on_frame(msg.arbitration_id, bytes(msg.data), time.monotonic())
    except KeyboardInterrupt:
        pass
    finally:
        bus.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
