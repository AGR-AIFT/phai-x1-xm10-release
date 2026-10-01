#!/usr/bin/env python3
"""보드 없이 **앱 전체**를 한 번 돌린다 — 데모이자 통합 회귀 시험.

    python demo_run.py --out ./demo_out

무엇을 지나가나
---------------
    합성 와이어  ->  COBS/CRC 파싱  ->  FrameRouter  ->  실시간 타입 디코딩
                 ->  .xmlog 저장   ->  되읽기/복구  ->  CSV 내보내기  ->  판정

가짜로 만든 부분은 **바이트를 만드는 곳 하나뿐**이다(`demo_stream.py`). 그 뒤로는
보드를 꽂았을 때 도는 코드와 같다. 프레임 객체를 직접 만들어 넣는 mock 이 아니라
진짜 바이트를 흘려보내므로, 프레이밍·CRC·시퀀스 회계까지 전부 실제로 검사된다.

시리얼 읽기는 프레임 경계를 지켜 주지 않는다
--------------------------------------------
`serial.read()` 는 임의 크기로 끊어서 준다 — 한 프레임이 세 번에 나눠 오거나, 한 번에
두 프레임 반이 오기도 한다. 그래서 이 데모는 스트림을 **일부러 어중간한 크기로 잘라**
먹인다. 프레임 하나씩 넣으면 절대 안 잡히는 경계 버그가 있다.
"""
from __future__ import annotations

import argparse
import os
import sys

import demo_stream as DS
import schema_registry as R
import xmlog as X
import xmlog_export as EXP
from frame_router import cobs_decode, parse_phai_frame, FrameRouter
from xmlog_capture import XmLogCapture

try:                                    # 생성된 0x20 맵 — 없으면 이름만 못 붙는다
    import xm_total_data_map as M
    MAP_VERSION = M.DATA_MAP_VERSION
except ImportError:                     # pragma: no cover
    MAP_VERSION = ""


# 실시간 경로는 수신기 것을 **그대로** 쓴다. 여기 복사본을 두면 수신기가 바뀌었을 때
# 데모만 옳게 도는 상태가 되고, 그건 데모가 아무것도 증명하지 못한다는 뜻이다.
# (실제로 실시간 경로가 as_float32 하드코딩이던 것을 데모가 못 잡은 전례가 있다 — 감사 #7)
try:
    import cdc_phai_receiver as RX
    LIVE_PATH = "cdc_phai_receiver.decode_user_values"
except Exception as _e:                 # PyQt5/pyqtgraph 부재 등
    RX = None
    LIVE_PATH = "레지스트리 직결 (수신기 import 실패: %s)" % type(_e).__name__


class Check:
    """판정 하나. 실패해도 나머지를 계속 본다 — 한 번에 다 보여주는 쪽이 쓸모 있다."""

    __slots__ = ("label", "ok", "detail")

    def __init__(self, label, ok, detail=""):
        self.label = label
        self.ok = bool(ok)
        self.detail = detail


def _feed(stream: bytes, chunk: int, on_frame):
    """스트림을 `chunk` 바이트씩 흘려보내며 프레임을 뽑는다. 수신 루프와 같은 모양.

    반환: `(ok, crc_err, framing_err)`
    """
    buf = bytearray()
    ok = crc_err = framing_err = 0
    pos = 0
    while pos < len(stream):
        buf += stream[pos:pos + chunk]
        pos += chunk
        while True:
            d = buf.find(0)
            if d < 0:
                break
            if d > 0:
                pkt, err = parse_phai_frame(cobs_decode(bytes(buf[:d])), 0.0)
                if pkt is not None:
                    ok += 1
                    on_frame(pkt)
                elif err == "crc":
                    crc_err += 1
                else:
                    framing_err += 1
            del buf[:d + 1]
    return ok, crc_err, framing_err


def run_demo(out_dir: str, cycles: int = 25, chunk: int = 61,
             quiet: bool = False) -> int:
    """전 구간을 한 번 돌리고 판정을 낸다. 반환 = 프로세스 종료 코드."""
    def say(*a):
        if not quiet:
            print(*a)

    os.makedirs(out_dir, exist_ok=True)
    log_path = os.path.join(out_dir, "demo.xmlog")
    csv_dir = os.path.join(out_dir, "csv")
    checks = []

    # ---------------------------------------------------------------- 1. 생성
    stream, exp = DS.build_demo_session(cycles=cycles)
    say("1) 합성 와이어  %d B  —  %d 주기, 프레임 %d개 (CRC 손상 %d, 손실 %d)"
        % (len(stream), cycles, exp.frames_ok, exp.frames_crc_bad, exp.lost_frames))
    say("   0x20 Total Data %d · 0xF0 타입드 %d · 0xF1 이름만 %d · 0xEE 1 · 0xEF 1"
        % (exp.total_frames, exp.typed_frames, exp.meta_only_frames))

    # ---------------------------------------------------------------- 2. 수신
    router = FrameRouter()
    if RX is not None:
        RX.SCHEMA_REG = R.SchemaRegistry()
        reg = RX.SCHEMA_REG
    else:
        reg = R.SchemaRegistry()

    cap = XmLogCapture(log_path, device_usb_serial="DEMO-NO-BOARD",
                       fw_build_id="demo", total_data_map_version=MAP_VERSION,
                       flush_every=64)
    live_rows = []          # (names, values) — 0xF0 을 실시간 경로로 푼 것
    state = {"us": 0}

    def on_frame(pkt):
        state["us"] += 1000
        router.route(pkt)
        if RX is not None:
            RX.feed_schema_frame(pkt, state["us"] / 1e6)
        elif pkt.module_id == R.MODULE_ID_SCHEMA_DESC:
            reg.feed_0xEE(pkt.payload, state["us"] / 1e6)
        elif pkt.module_id == R.MODULE_ID_USER_META:
            reg.feed_0xEF(pkt.payload)

        if pkt.module_id == DS.MODULE_TYPED:
            if RX is not None:
                live_rows.append(RX.decode_user_values(pkt))
            else:
                cs = reg.get(pkt.module_id, len(pkt.payload))
                live_rows.append((list(cs.names), cs.decode(pkt.payload))
                                 if cs is not None else ([], []))
        cap.on_frame(pkt, state["us"])

    ok, crc_err, framing_err = _feed(stream, chunk, on_frame)
    cap.close()

    say("")
    say("2) 수신  (%d B 씩 끊어 먹임 — 프레임 경계를 일부러 어긋나게)" % chunk)
    say("   통과 %d · CRC 오류 %d · 프레이밍 오류 %d" % (ok, crc_err, framing_err))
    checks.append(Check("CRC 통과 프레임 수", ok == exp.frames_ok,
                        "%d (기대 %d)" % (ok, exp.frames_ok)))
    checks.append(Check("깨진 CRC 를 세고 버렸다", crc_err == exp.frames_crc_bad,
                        "%d (기대 %d)" % (crc_err, exp.frames_crc_bad)))
    checks.append(Check("프레이밍 오류 없음", framing_err == 0, str(framing_err)))

    # ---------------------------------------------------- 3. 실시간 타입 디코딩
    say("")
    say("3) 실시간 디코딩  경로 = %s" % LIVE_PATH)
    typed_live = [r for r in live_rows if r[0] and r[0][0] == "state"]
    checks.append(Check("0xEE 도착 후 실시간 경로가 타입으로 푼다", len(typed_live) > 0,
                        "타입드로 푼 프레임 %d / %d" % (len(typed_live), len(live_rows))))
    if typed_live:
        names, vals = typed_live[-1]
        say("   %s" % "  ".join("%s=%s" % (n, _fmt(v)) for n, v in zip(names, vals)))
        # 값이 진짜 타입대로인가 — float32 로 뭉갰다면 state 가 정수로 나올 수 없다.
        want = exp.typed_rows[-1]
        got = dict(zip(names, vals))
        checks.append(Check("정수 필드가 정수로", got.get("state") == want["state"],
                            "state=%r (기대 %r)" % (got.get("state"), want["state"])))
        checks.append(Check("scale 이 적용된다 (i16 x0.1)",
                            abs(got.get("angle_x10", 0) - want["angle_x10"] * 0.1) < 1e-6,
                            "angle_x10=%r (기대 %r)"
                            % (got.get("angle_x10"), want["angle_x10"] * 0.1)))
        checks.append(Check("배열이 펴진다 (torque[2])",
                            "torque[0]" in names and "torque[1]" in names,
                            ",".join(names)))
    say("   * 스키마(0xEE)보다 **먼저** 온 프레임은 실시간엔 못 푼다 — 아래 CSV 에선 풀린다")

    # ---------------------------------------------------------------- 4. 저장
    say("")
    say("4) 저장  %s" % cap.summary())
    res = X.read_file(log_path)
    say("   되읽기: %s" % X.summarize(res))
    checks.append(Check("파일이 온전하다", res.stopped_reason is None,
                        str(res.stopped_reason)))
    kinds = [r.rec_type for r in res.records]
    checks.append(Check("SESSION 이 맨 앞 1개", kinds and kinds[0] == X.REC_SESSION
                        and kinds.count(X.REC_SESSION) == 1, str(kinds.count(X.REC_SESSION))))
    checks.append(Check("DATA 수 = 통과 프레임 수", kinds.count(X.REC_DATA) == ok,
                        "%d vs %d" % (kinds.count(X.REC_DATA), ok)))

    gaps = [r for r in res.records if r.rec_type == X.REC_GAP]
    lost = sum(g.fields["lost_count"] for g in gaps)
    # CRC 로 버린 프레임도 seq 를 소모했으므로 수신기에는 손실로 보인다.
    want_lost = exp.lost_frames + exp.frames_crc_bad
    checks.append(Check("손실이 GAP 으로 남았다", lost == want_lost,
                        "%d (기대 %d = 드롭 %d + CRC %d)"
                        % (lost, want_lost, exp.lost_frames, exp.frames_crc_bad)))
    say("   GAP %d개 · 손실 %d 프레임" % (len(gaps), lost))

    d20 = [r for r in res.records
           if r.rec_type == X.REC_DATA and r.fields["module_id"] == 0x20]
    if d20:
        checks.append(Check("0x20 이 368 B 로 저장됐다 (365 아님)",
                            len(d20[0].payload) == 368, "%d B" % len(d20[0].payload)))

    # ------------------------------------------------------------ 5. 내보내기
    say("")
    say("5) CSV 내보내기  ->  %s" % csv_dir)
    written = EXP.export_csv(res, csv_dir, "demo", want_raw_hex=False)

    # 스키마보다 먼저 온 행까지 **전부** 타입으로 풀렸는가 (PLAN 4.1 사후 해석)
    typed_csv = os.path.join(csv_dir, "demo_user_0x%02X.csv" % DS.MODULE_TYPED)
    if os.path.exists(typed_csv):
        with open(typed_csv, encoding="utf-8") as f:
            hdr = f.readline().rstrip("\n").split(",")
            rows = [ln.rstrip("\n").split(",") for ln in f if ln.strip()]
        checks.append(Check("0xF0 CSV 열 이름이 구조체 필드",
                            hdr[3:] == ["state", "contact", "emg_rms", "angle_x10",
                                        "tick", "torque[0]", "torque[1]"],
                            ",".join(hdr[3:])))
        checks.append(Check("0xF0 행 수 = 보낸 수", len(rows) == exp.typed_frames,
                            "%d (기대 %d)" % (len(rows), exp.typed_frames)))
        if rows and len(hdr) > 3:
            i_tick = hdr.index("tick") if "tick" in hdr else -1
            first_ok = (i_tick > 0 and rows[0][i_tick] == str(exp.typed_rows[0]["tick"]))
            checks.append(Check("스키마보다 **먼저** 온 첫 행도 풀렸다", first_ok,
                                "tick=%s (기대 %d)"
                                % (rows[0][i_tick] if i_tick > 0 else "?",
                                   exp.typed_rows[0]["tick"])))
        checks.append(Check("모든 행의 열 수가 헤더와 같다",
                            all(len(r) == len(hdr) for r in rows), ""))

    meta_csv = os.path.join(csv_dir, "demo_user_0x%02X.csv" % DS.MODULE_META_ONLY)
    if os.path.exists(meta_csv):
        with open(meta_csv, encoding="utf-8") as f:
            hdr = f.readline().rstrip("\n").split(",")
        checks.append(Check("0xF1 은 이름만 붙고 타입은 가정",
                            hdr[3:] == ["Battery", "Load Cell", "Ambient"],
                            ",".join(hdr[3:])))

    # ---------------------------------------------------------------- 6. 판정
    bad = [c for c in checks if not c.ok]
    say("")
    say("6) 판정")
    for c in checks:
        say("   %s %-42s %s" % ("[OK]  " if c.ok else "[FAIL]", c.label, c.detail))
    say("")
    if bad:
        say("데모 실패 — %d/%d 항목" % (len(bad), len(checks)))
        return 1
    say("데모 통과 — %d/%d 항목. 산출물:" % (len(checks), len(checks)))
    say("   %s" % log_path)
    for p in written:
        say("   %s" % p)
    say("")
    say("이어서 볼 것:  python xm10.py export %s --csv %s" % (log_path, csv_dir))
    return 0


def _fmt(v):
    if isinstance(v, float):
        return "%.4g" % v
    return str(v)


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description="보드 없이 전 구간을 한 번 돌린다")
    ap.add_argument("--out", default="demo_out", help="산출물 디렉토리")
    ap.add_argument("--cycles", type=int, default=25, help="합성할 제어 주기 수")
    ap.add_argument("--chunk", type=int, default=61,
                    help="한 번에 흘려보낼 바이트 수 (프레임 경계와 어긋나게 두는 게 요점)")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    return run_demo(a.out, cycles=a.cycles, chunk=a.chunk, quiet=a.quiet)


if __name__ == "__main__":
    raise SystemExit(main())
