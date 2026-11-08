#!/usr/bin/env python3
"""XM10 예제 Ex.45(CAN-FD My Sensor)의 상대 장치를 PC 에서 흉내 냅니다 — CAN-FD USB 어댑터 + python-can.

하는 일 (STM32G474 참조 장치와 같은 규칙)
  - 주기 데이터 ID 0x180 + n, 16 바이트 — 카운터(uint32) · 가짜 ADC 2채널(uint16, 파형) · 나머지 0
  - 살아있음 ID 0x700 + n, 0x05, 1 초마다 — 켜질 때 0x00 을 한 번(재시작 알림)
  - 명령 ID 0x200 + n, [순번, LED] — 순번이 1 ~ 127 앞서면 새 명령. LED 값을 화면에 보여 줍니다
  - (선택) --sync — XM10 이 보내는 동기 신호(ID 0x080) 직후에 주기 데이터를 보냅니다

예
  pip install python-can
  python canfd_sensor_sim.py --interface pcan --channel PCAN_USBBUS1
  python canfd_sensor_sim.py --interface virtual --channel demo     # 어댑터 없이 이 PC 안에서만(시험용)

버스 설정은 XM10 과 같게 합니다 — 1 Mbit/s / 5 Mbit/s, 샘플 포인트 75 %. 어댑터 클럭이 80 MHz 면 XM10 과 같은 값을 그대로 쓰고,
다른 클럭이면 --f-clock-mhz 로 알려 주세요(같은 샘플 포인트로 계산합니다). 어댑터를 밖에서 설정하는 경우(예: Linux socketcan)는
--timing none 을 쓰세요.
"""

import argparse
import math
import sys
import time

DATA_LEN = 16
ALIVE_OK = 0x05
ALIVE_REBOOT = 0x00
ALIVE_PERIOD_S = 1.0
CMD_REBASE_S = 1.0          # 명령이 이만큼 없었으면 다음 순번을 새 기준으로(재시작 대비)
SYNC_ID = 0x080
SYNC_LOST_S = 0.005         # 동기 신호가 이만큼 없으면 자기 주기로
STATUS_PERIOD_S = 1.0
ADC_MAX = 4095
TIME_EPS = 1e-6             # 초 단위 소수 비교의 여유(1.001 - 1.000 이 0.001 보다 조금 작게 나오는 것)


class SensorSim:
    """규칙만 담습니다 — 보내기는 send(id, data) 로, 시각(초)은 인자로 받습니다(시험하기 쉽게)."""

    def __init__(self, send, node=0x20, period_s=0.01, use_sync=False):
        self.send = send
        self.node = node
        self.data_id = 0x180 + node
        self.cmd_id = 0x200 + node
        self.alive_id = 0x700 + node
        self.period_s = period_s
        self.use_sync = use_sync
        self.counter = 0
        self.send_fail = 0
        self.cmd_new = 0
        self.cmd_ignored = 0
        self.sync_count = 0
        self.free_rx = 0
        self.led = None
        self._have_cmd = False
        self._last_seq = 0
        self._last_cmd_rx = 0.0
        self._last_alive = 0.0
        self._last_data = 0.0
        self._sync_seen = False
        self._sync_pending = False
        self._last_sync = 0.0
        self._t0 = 0.0

    def start(self, now):
        self._t0 = now
        self._send(self.alive_id, bytes([ALIVE_REBOOT]))      # 켜졌음 — XM10 은 받아 둔 옛 값을 버립니다
        self._last_alive = now - ALIVE_PERIOD_S              # 다음 poll() 에서 바로 0x05
        self._last_data = now

    def _send(self, can_id, data):
        if self.send(can_id, data):
            return True
        self.send_fail += 1
        return False

    def _adc(self, now):
        t = now - self._t0
        a0 = int((math.sin(2.0 * math.pi * 0.5 * t) + 1.0) * 0.5 * ADC_MAX)      # 0.5 Hz 사인
        a1 = int(abs(((t * 1.0) % 2.0) - 1.0) * ADC_MAX)                          # 0.5 Hz 삼각
        return max(0, min(ADC_MAX, a0)), max(0, min(ADC_MAX, a1))

    def _send_data(self, now):
        a0, a1 = self._adc(now)
        payload = (self.counter.to_bytes(4, "little") + a0.to_bytes(2, "little") + a1.to_bytes(2, "little")
                   + bytes(DATA_LEN - 8))
        if self._send(self.data_id, payload):
            self.counter = (self.counter + 1) & 0xFFFFFFFF   # 보낼 때마다 +1 — XM10 은 빈 번호로 잃은 값을 셉니다

    def poll(self, now):
        if now - self._last_alive >= ALIVE_PERIOD_S - TIME_EPS:
            if self._send(self.alive_id, bytes([ALIVE_OK])):
                self._last_alive = now                       # 못 보냈으면 다음 poll() 에 다시
        use_sync = self.use_sync and self._sync_seen and (now - self._last_sync) < SYNC_LOST_S
        if use_sync:
            if self._sync_pending:
                self._sync_pending = False
                self._last_data = now
                self._send_data(now)
        elif now - self._last_data >= self.period_s - TIME_EPS:
            self._last_data = now
            self._send_data(now)

    def on_frame(self, can_id, data, now):
        """받은 메시지 하나. 새 LED 명령이면 LED 값(0/1)을, 아니면 None 을 돌려줍니다."""
        if can_id == SYNC_ID:
            self.sync_count += 1
            self._sync_seen = True
            self._sync_pending = True
            self._last_sync = now
            return None
        if can_id != self.cmd_id:
            self.free_rx += 1                                # XM10 의 자유 송신 등
            return None
        if len(data) < 2:
            return None
        seq = data[0]
        rebase = (not self._have_cmd) or (now - self._last_cmd_rx > CMD_REBASE_S)
        diff = (seq - self._last_seq) & 0xFF
        self._last_cmd_rx = now
        if rebase or 1 <= diff <= 127:
            self._have_cmd = True
            self._last_seq = seq
            self.cmd_new += 1
            self.led = 1 if data[1] else 0
            return self.led
        self.cmd_ignored += 1                                # 중복 · 순서가 뒤바뀐 옛 명령
        return None


def make_timing(can, f_clock_mhz):
    """XM10 과 같은 1 Mbit/s · 5 Mbit/s, 샘플 포인트 75 %."""
    if f_clock_mhz == 80:
        return can.BitTimingFd(f_clock=80_000_000, nom_brp=10, nom_tseg1=5, nom_tseg2=2, nom_sjw=2,
                               data_brp=1, data_tseg1=11, data_tseg2=4, data_sjw=4)
    return can.BitTimingFd.from_sample_point(f_clock=int(f_clock_mhz * 1_000_000), nom_bitrate=1_000_000,
                                             nom_sample_point=75.0, data_bitrate=5_000_000, data_sample_point=75.0)


def open_bus(can, args):
    kwargs = {"interface": args.interface, "channel": args.channel, "fd": True}
    if args.interface != "virtual" and args.timing == "xm10":
        kwargs["timing"] = make_timing(can, args.f_clock_mhz)
    return can.Bus(**kwargs)


def parse_args(argv):
    ap = argparse.ArgumentParser(description="XM10 Ex.45 의 상대 장치(CAN-FD 센서)를 PC 에서 흉내 냅니다")
    ap.add_argument("--interface", required=True, help="python-can 인터페이스 이름 — pcan · kvaser · vector · virtual …")
    ap.add_argument("--channel", required=True, help="채널 — 예: PCAN_USBBUS1 · 0 · demo(virtual)")
    ap.add_argument("--node", type=lambda s: int(s, 0), default=0x20, help="장치 번호(기본 0x20 — XM10 예제의 MY_DEVICE_ID)")
    ap.add_argument("--period-ms", type=float, default=10.0, help="주기 데이터 간격 ms(기본 10 — PC 는 1 ms 를 지키기 어렵습니다)")
    ap.add_argument("--sync", action="store_true", help="동기 신호(ID 0x080) 직후에 보냅니다 — XM10 예제의 MY_USE_SYNC 를 true 로")
    ap.add_argument("--timing", choices=("xm10", "none"), default="xm10",
                    help="xm10 = XM10 과 같은 비트 타이밍을 어댑터에 넣음(기본) · none = 어댑터 설정을 그대로 씀")
    ap.add_argument("--f-clock-mhz", type=float, default=80.0, help="어댑터의 CAN-FD 클럭 MHz(기본 80)")
    ap.add_argument("--seconds", type=float, default=0.0, help="이만큼 돌고 끝냅니다(0 = Ctrl+C 까지)")
    a = ap.parse_args(argv)
    if not 1 <= a.node <= 127:
        ap.error("--node 는 1 ~ 127")
    if a.period_ms < 1.0:
        ap.error("--period-ms 는 1 이상")
    return a


def main(argv=None):
    args = parse_args(argv)
    try:
        import can
    except ImportError:
        print("python-can 이 필요합니다: pip install python-can", file=sys.stderr)
        return 2
    bus = open_bus(can, args)

    def send(can_id, data):
        msg = can.Message(arbitration_id=can_id, is_extended_id=False, is_fd=True, bitrate_switch=True, data=data)
        try:
            bus.send(msg, timeout=0.01)
            return True
        except can.CanError:
            return False

    sim = SensorSim(send, node=args.node, period_s=args.period_ms / 1000.0, use_sync=args.sync)
    t_start = time.monotonic()
    sim.start(t_start)
    print("장치 번호 0x%02X — 주기 데이터 0x%03X · 살아있음 0x%03X · 명령 0x%03X (Ctrl+C 로 끝)"
          % (args.node, sim.data_id, sim.alive_id, sim.cmd_id))
    next_status = t_start + STATUS_PERIOD_S
    try:
        while True:
            now = time.monotonic()
            if args.seconds and now - t_start >= args.seconds:
                break
            sim.poll(now)
            msg = bus.recv(timeout=0.001)
            if msg is not None and not msg.is_extended_id and not msg.is_remote_frame and not msg.is_error_frame:
                led = sim.on_frame(msg.arbitration_id, bytes(msg.data), time.monotonic())
                if led is not None:
                    print("LED 명령: %s" % ("켬" if led else "끔"))
            if now >= next_status:
                next_status += STATUS_PERIOD_S
                print("보냄 %d · 못 보냄 %d · 명령 새 %d / 버림 %d · 동기 신호 %d · 그 밖에 받음 %d"
                      % (sim.counter, sim.send_fail, sim.cmd_new, sim.cmd_ignored, sim.sync_count, sim.free_rx))
    except KeyboardInterrupt:
        pass
    finally:
        bus.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
