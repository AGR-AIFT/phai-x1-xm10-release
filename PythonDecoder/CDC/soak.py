#!/usr/bin/env python3
"""무손실 soak 측정 — 보드를 꽂고 한 명령으로 판정한다.

    python soak.py --minutes 30                 # 포트 자동 탐색
    python soak.py --port COM6 --minutes 10
    python soak.py --selftest                   # 보드 없이 회계 로직만 검증

무엇을 재는가
-------------
설계 문서(PLAN §5)가 요구하는 **선행 실측**이다. 두 가지를 동시에 본다.

1. **드롭이 0인가** — 펌웨어의 Tx 드롭은 **이미 와이어에 실려 있다.** 모든 PhAI 프레임의
   STATUS 바이트 하위 7비트가 그 tick 의 드롭 delta 다. 그래서 코드 변경도 디버거도
   필요 없다. 이 값이 30분 내내 0이면 현재 배포본의 링 여유(허용 stall 86 ms)가
   충분하다는 실증이 된다.

2. **살아 있는가** — ⚠ 1번만 보면 **거짓 통과한다.** 스트림이 조용히 죽으면
   "관측된 STATUS 가 전부 0" 이 자동으로 성립하기 때문이다. 실제로 그런 경로가 있다:
   PhAI Studio 가 Firmware 탭에서 QUERY_INFO 를 보내면 `_Boot_FTP_CdcTxWrapper` 가
   스트리밍을 내리고, 되살아나는 건 DTR 0→1 엣지뿐이다. 그래서 **초당 프레임 수**를
   함께 보고 950 미만이면 실패로 본다 (1 kHz 기준).

그리고 **받은 것이 디스크에 남았는지**까지 확인한다 — 파싱한 프레임 수와, soak 이 끝난 뒤
`.xmlog` 를 다시 읽어 센 DATA 레코드 수가 같아야 한다. "받았다" 와 "저장됐다" 는 다른 주장이다.

판정은 사람이 아니라 이 스크립트가 한다 — 30분을 보고 눈으로 세지 않게.
"""
from __future__ import annotations

import argparse
import os
import struct
import sys
import time
from datetime import datetime

from frame_router import (
    PHAI_SOF, crc16_ccitt, cobs_decode, parse_phai_frame, FrameRouter,
)
import xmlog as X
from xmlog_capture import XmLogCapture

DEFAULT_BAUD = 921600
LIVENESS_MIN_FPS = 950.0          # 1 kHz 스트림 기준. rev4.1 자체 감사에서 추가된 조건.


class SoakLedger:
    """soak 회계. 시리얼과 분리해 둬서 보드 없이 검증할 수 있다."""

    __slots__ = ("frames", "crc_errors", "sync_errors", "status_drop_total",
                 "status_drop_frames", "seq_lost", "resyncs", "bytes_in",
                 "oversized_drops", "t_start", "t_end", "_router")

    def __init__(self):
        self.frames = 0
        self.crc_errors = 0
        self.sync_errors = 0
        self.status_drop_total = 0     # STATUS 하위 7비트의 합 = FW 가 보고한 드롭
        self.status_drop_frames = 0    # 0 이 아닌 STATUS 를 실어온 프레임 수
        self.seq_lost = 0
        self.resyncs = 0
        self.bytes_in = 0
        self.oversized_drops = 0
        self.t_start = None
        self.t_end = None
        self._router = FrameRouter()

    def on_error(self, kind: str) -> None:
        if kind == "crc":
            self.crc_errors += 1
        else:
            self.sync_errors += 1

    def on_frame(self, pkt) -> str:
        self.frames += 1
        if pkt.tx_drops:
            self.status_drop_total += pkt.tx_drops
            self.status_drop_frames += 1
        before = self._router.ledger.lost_count
        tag, _delta = self._router.route(pkt)
        # 원장의 lost_count 는 단조 증가라 차분이 음수가 될 수 없다. 그래도 방어한다 —
        # 음수가 나오면 원장 규약이 바뀐 것이고, 조용히 상쇄되면 손실을 놓친다.
        self.seq_lost += max(0, self._router.ledger.lost_count - before)
        self.resyncs = self._router.ledger.resync_count
        return tag

    # -- 판정 ---------------------------------------------------------
    @property
    def duration_s(self) -> float:
        if self.t_start is None:
            return 0.0
        end = self.t_end if self.t_end is not None else time.perf_counter()
        return max(1e-9, end - self.t_start)

    @property
    def fps(self) -> float:
        return self.frames / self.duration_s

    def verdict(self, durable_records=None):
        """반환: (pass: bool, 줄 목록). 각 줄은 하나의 조건과 그 결과."""
        checks = []

        checks.append(("FW Tx 드롭 (STATUS)", self.status_drop_total == 0,
                       "%d (프레임 %d개가 보고)" % (self.status_drop_total,
                                                    self.status_drop_frames)))
        checks.append(("CRC 오류", self.crc_errors == 0, str(self.crc_errors)))
        checks.append(("프레이밍/동기 오류", self.sync_errors == 0, str(self.sync_errors)))
        checks.append(("와이어 시퀀스 손실", self.seq_lost == 0, str(self.seq_lost)))
        checks.append(("liveness (>= %.0f fps)" % LIVENESS_MIN_FPS,
                       self.fps >= LIVENESS_MIN_FPS, "%.1f fps" % self.fps))
        # seq 가 뒤로 가는 것은 원장이 "손실" 로 세지 않는다(재부팅/재연결이므로).
        # 그래서 seq_lost 만 보면 **soak 도중 보드가 재부팅해도 통과**한다 — 그건
        # 무손실 실증이 아니다. 별도 조건으로 세운다 (감사 #6).
        checks.append(("장치 재시작/재연결 (resync)", self.resyncs == 0, str(self.resyncs)))

        if durable_records is not None:
            ok = durable_records == self.frames
            checks.append(("받은 것 == 디스크에 남은 것", ok,
                           "%d 수신 / %d 저장" % (self.frames, durable_records)))

        lines = []
        allok = True
        for name, ok, detail in checks:
            allok = allok and ok
            lines.append("  %-28s %-5s %s" % (name, "OK" if ok else "FAIL", detail))
        return allok, lines

    def summary(self) -> str:
        return ("frames=%d  %.1f fps  %.1f s  bytes=%d  "
                "crc_err=%d sync_err=%d seq_lost=%d resync=%d  status_drop=%d  "
                "oversized_drop=%d"
                % (self.frames, self.fps, self.duration_s, self.bytes_in,
                   self.crc_errors, self.sync_errors, self.seq_lost,
                   self.resyncs, self.status_drop_total, self.oversized_drops))


# COBS 구분자(0x00)가 안 나올 때 버퍼가 무한히 자라는 것을 막는 상한.
# 정상 최대 프레임은 1030 B 남짓이라 이 값은 넉넉하다. 넘으면 스트림이 PhAI 가
# 아니거나 심하게 깨진 것이므로, 버리고 다시 동기를 잡는 편이 낫다 (감사 #12).
MAX_RESYNC_BUFFER = 64 * 1024


def feed_stream(ledger: SoakLedger, chunk: bytes, buf: bytearray,
                now: float, cap=None) -> None:
    """수신 바이트 한 덩어리를 프레임으로 쪼개 회계에 넣는다. 수신 루프와 공유."""
    ledger.bytes_in += len(chunk)
    buf.extend(chunk)
    if len(buf) > MAX_RESYNC_BUFFER and buf.find(0) < 0:
        # 구분자가 하나도 없다 = 프레임 경계를 못 찾고 있다. 통째로 버린다.
        ledger.on_error("sync")
        ledger.oversized_drops += 1
        del buf[:]
        return
    while True:
        d = buf.find(0)
        if d < 0:
            break
        if d > 0:
            raw = cobs_decode(bytes(buf[:d]))
            pkt, err = parse_phai_frame(raw, now)
            if err is not None:
                ledger.on_error(err)
            else:
                ledger.on_frame(pkt)
                if cap is not None:
                    cap.on_frame(pkt, int(now * 1e6))
        del buf[:d + 1]


# =============================================================================
# 포트 탐색
# =============================================================================

def find_ports():
    try:
        import serial.tools.list_ports as lp
    except ImportError:
        return []
    return list(lp.comports())


def guess_xm_port():
    """XM10 으로 보이는 포트. 확신이 없으면 None 을 돌려주고 사람에게 고르게 한다."""
    cands = []
    for p in find_ports():
        desc = (p.description or "").lower()
        if "bluetooth" in desc:
            continue
        # STM32 USB CDC. VID 0x0483 = STMicroelectronics.
        if p.vid == 0x0483 or "stm" in desc or "usb serial" in desc or "cdc" in desc:
            cands.append(p)
    if len(cands) == 1:
        return cands[0].device
    return None


# =============================================================================
# 실측
# =============================================================================

def run_soak(port, baud, minutes, out_dir, quiet=False) -> int:
    try:
        import serial
    except ImportError:
        print("pyserial 이 없다: pip install pyserial")
        return 2

    os.makedirs(out_dir, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = os.path.join(out_dir, "soak_%s.xmlog" % stamp)

    try:
        ser = serial.Serial(port, baud, timeout=0.2)
    except Exception as e:                                   # noqa: BLE001
        print("포트를 열 수 없다 (%s): %s" % (port, e))
        return 2

    print("soak 시작 — port=%s  %d분  ->  %s" % (port, minutes, log_path))
    print("  Ctrl+C 로 중단해도 그 시점까지의 판정이 나온다.")

    ledger = SoakLedger()
    cap = XmLogCapture(log_path, fw_build_id="", total_data_map_version="")
    buf = bytearray()
    ledger.t_start = time.perf_counter()
    deadline = ledger.t_start + minutes * 60.0
    last_print = ledger.t_start

    try:
        while time.perf_counter() < deadline:
            chunk = ser.read(4096)
            now = time.perf_counter()
            if chunk:
                feed_stream(ledger, chunk, buf, now - ledger.t_start, cap)
            if not quiet and now - last_print >= 10.0:
                print("  " + ledger.summary())
                last_print = now
    except KeyboardInterrupt:
        print("\n  중단됨 — 그 시점까지로 판정한다.")
    finally:
        ledger.t_end = time.perf_counter()
        ser.close()
        cap.close()

    # 디스크에 정말 남았는지 다시 읽어 센다 — "받았다" 와 "저장됐다" 는 다른 주장이다.
    durable = None
    try:
        res = X.read_file(log_path)
        durable = sum(1 for r in res.records if r.rec_type == X.REC_DATA)
        if res.stopped_reason:
            print("  ⚠ 로그 파일이 온전하지 않다: %s" % res.stopped_reason)
    except X.LogError as e:
        print("  ⚠ 로그를 다시 읽을 수 없다: %s" % e)

    return report(ledger, durable, log_path)


def report(ledger: SoakLedger, durable, log_path) -> int:
    print("")
    print("결과 — " + ledger.summary())
    ok, lines = ledger.verdict(durable)
    for ln in lines:
        print(ln)
    print("")
    if ok:
        print("PASS — 무손실. 이 구성에서 링 여유가 충분하다는 실증이다.")
    else:
        print("FAIL — 위 FAIL 줄이 원인이다.")
        if ledger.fps < LIVENESS_MIN_FPS and ledger.status_drop_total == 0:
            print("       (드롭 0 + fps 미달 = 스트림이 죽어 있었을 가능성.")
            print("        PhAI Studio 등 다른 클라이언트가 같은 포트를 건드렸는지 확인)")
    if log_path:
        print("")
        print("로그: %s" % log_path)
        print("CSV: python CDC/xmlog_export.py %s --csv data/" % log_path)
    return 0 if ok else 1


# =============================================================================
# 자기검사 — 보드 없이 회계·판정 로직을 검증한다
# =============================================================================

def _wire(seq_id, module_id, payload, status=0):
    padded = bytes(payload) + bytes((-len(payload)) % 4)
    body = bytearray([PHAI_SOF, len(padded) // 4, seq_id & 0xFF, (seq_id >> 8) & 0xFF,
                      module_id, status])
    body.extend(padded)
    crc = crc16_ccitt(bytes(body))
    body.append(crc & 0xFF)
    body.append((crc >> 8) & 0xFF)

    def _cobs(data):
        out = bytearray()
        ci = len(out)
        out.append(0)
        code = 1
        for b in data:
            if b == 0:
                out[ci] = code
                ci = len(out)
                out.append(0)
                code = 1
            else:
                out.append(b)
                code += 1
                if code == 0xFF:
                    out[ci] = code
                    ci = len(out)
                    out.append(0)
                    code = 1
        out[ci] = code
        return bytes(out)

    return _cobs(bytes(body)) + bytes([0])


def selftest() -> int:
    fails = []

    def check(cond, msg):
        if not cond:
            fails.append(msg)

    # --- 1. 깨끗한 스트림은 PASS 여야 한다 ---------------------------------
    led = SoakLedger()
    led.t_start = 0.0
    buf = bytearray()
    n = 1000
    stream = bytearray()
    for i in range(n):
        stream += _wire(i, 0xF0, struct.pack("<4f", 1.0, 2.0, 3.0, 4.0))
    feed_stream(led, bytes(stream), buf, 0.0)
    led.t_end = 1.0                                   # 1초에 1000 프레임 = 1000 fps
    check(led.frames == n, "프레임 %d != %d" % (led.frames, n))
    ok, lines = led.verdict(durable_records=n)
    check(ok, "깨끗한 스트림이 FAIL 로 판정됐다:\n" + "\n".join(lines))

    # --- 2. STATUS 드롭이 있으면 FAIL ------------------------------------
    led2 = SoakLedger()
    led2.t_start = 0.0
    b2 = bytearray()
    s2 = bytearray()
    for i in range(n):
        st = 3 if i == 500 else 0                     # 한 프레임이 드롭 3 보고
        s2 += _wire(i, 0xF0, struct.pack("<4f", 0, 0, 0, 0), status=st)
    feed_stream(led2, bytes(s2), b2, 0.0)
    led2.t_end = 1.0
    check(led2.status_drop_total == 3, "STATUS 드롭 합 %d" % led2.status_drop_total)
    ok2, _l = led2.verdict()
    check(not ok2, "드롭이 있는데 PASS 로 판정됐다")

    # --- 3. 죽은 스트림(느림)은 드롭 0이어도 FAIL — 거짓 통과 방지 ----------
    led3 = SoakLedger()
    led3.t_start = 0.0
    b3 = bytearray()
    s3 = bytearray()
    for i in range(100):                              # 1초에 100 프레임뿐
        s3 += _wire(i, 0xF0, struct.pack("<4f", 0, 0, 0, 0))
    feed_stream(led3, bytes(s3), b3, 0.0)
    led3.t_end = 1.0
    check(led3.status_drop_total == 0, "이 경우 드롭은 0 이어야 한다")
    ok3, lines3 = led3.verdict()
    check(not ok3, "드롭 0 + 100 fps 인데 PASS — liveness 조건이 동작하지 않는다")
    check(any("liveness" in ln and "FAIL" in ln for ln in lines3),
          "liveness 줄이 FAIL 로 표시되지 않았다:\n" + "\n".join(lines3))

    # --- 4. 시퀀스 손실 감지 ---------------------------------------------
    led4 = SoakLedger()
    led4.t_start = 0.0
    b4 = bytearray()
    s4 = bytearray()
    for i in list(range(0, 500)) + list(range(505, 1000)):   # 5개 건너뜀
        s4 += _wire(i, 0xF0, struct.pack("<4f", 0, 0, 0, 0))
    feed_stream(led4, bytes(s4), b4, 0.0)
    led4.t_end = 1.0
    check(led4.seq_lost == 5, "시퀀스 손실 %d (5 여야)" % led4.seq_lost)
    ok4, _l4 = led4.verdict()
    check(not ok4, "손실 5인데 PASS")

    # --- 5. CRC 오류 감지 -------------------------------------------------
    led5 = SoakLedger()
    led5.t_start = 0.0
    b5 = bytearray()
    good = bytearray(_wire(0, 0xF0, struct.pack("<4f", 1, 2, 3, 4)))
    good[3] ^= 0xFF                                   # COBS 안쪽 바이트 훼손
    feed_stream(led5, bytes(good), b5, 0.0)
    led5.t_end = 1.0
    check(led5.crc_errors + led5.sync_errors == 1,
          "훼손 프레임을 못 잡았다 (crc=%d sync=%d)" % (led5.crc_errors, led5.sync_errors))

    # --- 6. 저장 누락 감지 ------------------------------------------------
    ok6, lines6 = led.verdict(durable_records=n - 1)
    check(not ok6, "받은 수와 저장된 수가 다른데 PASS")
    check(any("디스크" in ln and "FAIL" in ln for ln in lines6),
          "저장 대조 줄이 FAIL 로 표시되지 않았다")

    if fails:
        for m in fails:
            print("  FAIL  " + m)
        print("")
        print("%d FAILED" % len(fails))
        return 1
    print("  soak 회계 자기검사 통과")
    print("  (깨끗=PASS / STATUS 드롭=FAIL / 죽은 스트림=FAIL / seq 손실=FAIL /")
    print("   CRC 훼손 감지 / 저장 누락 감지)")
    return 0


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    ap = argparse.ArgumentParser(description="XM10 USB-CDC 무손실 soak 측정")
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--minutes", type=float, default=30.0)
    ap.add_argument("--output", default="data")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--selftest", action="store_true",
                    help="보드 없이 회계·판정 로직만 검증")
    ap.add_argument("--list-ports", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    if args.list_ports:
        ports = find_ports()
        if not ports:
            print("COM 포트 없음")
            return 1
        for p in ports:
            print("  %-8s VID:PID=%s:%s  %s"
                  % (p.device, ("%04X" % p.vid) if p.vid else "----",
                     ("%04X" % p.pid) if p.pid else "----", p.description))
        return 0

    port = args.port or guess_xm_port()
    if not port:
        print("XM10 포트를 찾지 못했다. --port 로 지정하거나 --list-ports 로 확인할 것.")
        ports = find_ports()
        if ports:
            print("보이는 포트:")
            for p in ports:
                print("  %-8s %s" % (p.device, p.description))
        else:
            print("  (COM 포트가 하나도 안 보인다 — 보드가 꽂혀 있는지 확인)")
        return 2

    return run_soak(port, args.baud, args.minutes, args.output, args.quiet)


if __name__ == "__main__":
    raise SystemExit(main())
