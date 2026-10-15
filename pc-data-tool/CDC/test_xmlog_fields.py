#!/usr/bin/env python3
"""`.xmlog` 필드의 뜻 회귀 시험 — 보드도 시리얼 포트도 없이 돈다.

    python test_xmlog_fields.py

바이트 모양(폭 · 순서 · CRC)은 `test_xmlog.py` 가 지킨다. 이 파일은 그 칸에 **무엇을** 적는지를
지킨다(spec/xmlog.md 의 '필드의 뜻'):

  * DATA 의 `pc_time_us` = 1970-01-01 UTC 부터의 µs. 그래프 창 · CLI · soak · 데모가 모두 `PcClock`
    으로 만든다 — 수신을 시작할 때 PC 시계로 기준을 잡고, 그 뒤는 단조 시계로 흐른 시간을 더한다.
    (그래프 창 · CLI 경로는 `test_gui_tabs.py` 가 같은 성질을 본다.)
  * GAP 의 `from_seq` · `to_seq` = 둘 다 **받은** 프레임의 번호(빠지기 직전 · 빠진 뒤 처음). 빠진 번호는
    그 사이이고 그 개수가 `lost_count` 다. seq 가 뒤로 가면 reason 3 · `lost_count` 0.

오라클은 손으로 정한 값과 지금 PC 시계다.
"""
import contextlib
import io
import os
import shutil
import sys
import tempfile
import time

import demo_stream as DS
import xmlog as X
from frame_router import cobs_decode, parse_phai_frame
from xmlog_capture import PcClock, XmLogCapture

# 2020-09-13 00:00 UTC 근처의 µs. 수신 시작부터 흐른 시간으로 적힌 값은 이보다 훨씬 작다.
UTC_US_FLOOR = 1_600_000_000 * 1_000_000
# 시계 두 개를 잇달아 읽을 때의 허용폭. 기준점을 10 초 옮기는 시험이 이 폭보다 커야 뜻이 있다.
CLOCK_NEAR_US = 2 * 1_000_000
# 파일을 쓰고 읽는 시험의 허용폭 — 데모 한 번이 도는 시간보다 넉넉하게.
FILE_NEAR_US = 120 * 1_000_000


def _now_us():
    return time.time_ns() // 1000


def _frame(seq, module_id, payload):
    """진짜 와이어(COBS · CRC16)를 만들어 파서를 통과시킨 프레임 — mock 이 아니다."""
    wire = DS.wire_frame(seq & 0xFFFF, module_id, payload)
    pkt, err = parse_phai_frame(cobs_decode(wire[:-1]), 0.0)
    assert err is None, err
    return pkt


def _read(path):
    res = X.read_file(path)
    assert res.stopped_reason is None, res.stopped_reason
    return res


def _data_times(res):
    return [r.fields["pc_time_us"] for r in res.records if r.rec_type == X.REC_DATA]


# =============================================================================
# pc_time_us = 1970-01-01 UTC 부터의 µs
# =============================================================================

def test_pc_clock_is_utc_us():
    before = _now_us()
    clock = PcClock()
    after = _now_us()
    t0 = clock.us(0.0)
    assert t0 >= UTC_US_FLOOR, "UTC µs 가 아니다: %d" % t0
    assert before - CLOCK_NEAR_US <= t0 <= after + CLOCK_NEAR_US, \
        "PC 시계와 다르다: %d (PC %d ~ %d)" % (t0, before, after)
    # 흐른 초를 µs 로 더한다 — PC 시계를 다시 읽지 않는다
    assert clock.us(1.5) - t0 == 1_500_000, clock.us(1.5) - t0
    # 반올림 — 1.001 s 를 µs 로 바꾸면 부동소수점으로 1000999.999… 가 된다(버리면 1 µs 모자란다)
    assert clock.us(1.001) - t0 == 1_001_000, clock.us(1.001) - t0


def test_pc_clock_origin_in_the_past():
    """수신 루프가 10 초 전에 잡은 기준점을 넘기면, 10 초 흐른 지금이 PC 시계와 같아야 한다."""
    origin = time.perf_counter() - 10.0
    clock = PcClock(origin)
    now = _now_us()
    got = clock.us(time.perf_counter() - origin)
    assert abs(got - now) <= CLOCK_NEAR_US, "%d vs PC %d" % (got, now)
    assert abs(clock.us(0.0) - (now - 10_000_000)) <= CLOCK_NEAR_US, \
        "기준점이 10 초 전이 아니다: %d vs %d" % (clock.us(0.0), now - 10_000_000)


def test_soak_writes_utc_us():
    """soak 수신 루프와 같은 함수(`feed_stream`)로 적은 .xmlog — DATA 의 pc_time_us 가 UTC µs."""
    import soak as S
    d = tempfile.mkdtemp(prefix="xmlog_fields_")
    try:
        path = os.path.join(d, "soak.xmlog")
        cap = XmLogCapture(path, flush_every=1)
        clock = PcClock()
        stream = b"".join(DS.wire_frame(i, 0xF0, b"\x00" * 8) for i in range(5))
        S.feed_stream(S.SoakLedger(), stream, bytearray(), 0.5, cap, clock)
        cap.close()
        times = _data_times(_read(path))
        assert len(times) == 5, times
        assert set(times) == {clock.us(0.5)}, times
        assert abs(times[0] - _now_us()) <= FILE_NEAR_US, (times[0], _now_us())

        # 파일만 주고 시계를 안 주면 거부한다 — 흐른 초가 UTC 칸에 들어가지 않게
        try:
            S.feed_stream(S.SoakLedger(), stream, bytearray(), 0.5, cap)
        except ValueError:
            pass
        else:
            raise AssertionError("pc_clock 없이 cap 을 받아들였다")
    finally:
        shutil.rmtree(d, ignore_errors=True)


def test_demo_writes_utc_us():
    """데모(`xm10.py demo`)가 남기는 .xmlog 도 UTC µs 다 — 합성 시각(프레임마다 1 ms)을 지금 시각에 더한다."""
    import demo_run
    d = tempfile.mkdtemp(prefix="xmlog_fields_demo_")
    try:
        start = _now_us()
        with contextlib.redirect_stdout(io.StringIO()):     # quiet 이어도 CSV 목록은 찍는다
            rc = demo_run.run_demo(d, quiet=True)
        assert rc == 0, "데모가 실패했다 (rc=%r)" % rc
        times = _data_times(_read(os.path.join(d, "demo.xmlog")))
        assert times, "DATA 가 없다"
        assert start - FILE_NEAR_US <= times[0] <= _now_us() + FILE_NEAR_US, (times[0], start)
        steps = {b - a for a, b in zip(times, times[1:])}
        assert steps == {1000}, "프레임 간격이 1000 µs 가 아니다: %r" % sorted(steps)[:5]
    finally:
        shutil.rmtree(d, ignore_errors=True)


# =============================================================================
# GAP 의 from_seq · to_seq = 받은 프레임의 번호
# =============================================================================

def _capture_seqs(seqs):
    """seq 를 차례로 흘린 캡처의 (GAP 목록, DATA seq 목록)."""
    d = tempfile.mkdtemp(prefix="xmlog_fields_gap_")
    try:
        path = os.path.join(d, "gap.xmlog")
        cap = XmLogCapture(path, flush_every=1)
        clock = PcClock()
        for i, s in enumerate(seqs):
            cap.on_frame(_frame(s, 0xF0, b"\x00" * 4), clock.us(i * 0.001))
        cap.close()
        res = _read(path)
        gaps = [(r.fields["reason"], r.fields["from_seq"], r.fields["to_seq"], r.fields["lost_count"])
                for r in res.records if r.rec_type == X.REC_GAP]
        data = [r.fields["seq_id"] for r in res.records if r.rec_type == X.REC_DATA]
        return gaps, data
    finally:
        shutil.rmtree(d, ignore_errors=True)


def _missing(from_seq, lost_count):
    return [(from_seq + k) & 0xFFFF for k in range(1, lost_count + 1)]


def test_gap_endpoints_are_received_seqs():
    """7 다음에 10 이 오면 GAP(1, 7, 10, 2) — 빠진 것은 8 · 9. 7 과 10 은 둘 다 DATA 로 남는다."""
    gaps, data = _capture_seqs([5, 6, 7, 10, 11])
    assert gaps == [(X.GAP_SEQ, 7, 10, 2)], gaps
    assert data == [5, 6, 7, 10, 11], data
    assert _missing(7, 2) == [8, 9]
    assert all(s not in data for s in _missing(7, 2))


def test_gap_wraps_at_65536():
    """65534 다음에 1 이 오면 빠진 것은 65535 · 0 — lost_count = (to - from - 1) mod 65536."""
    gaps, data = _capture_seqs([65533, 65534, 1, 2])
    assert gaps == [(X.GAP_SEQ, 65534, 1, 2)], gaps
    reason, frm, to, lost = gaps[0]
    assert lost == (to - frm - 1) & 0xFFFF
    assert _missing(frm, lost) == [65535, 0]
    assert (frm + lost + 1) & 0xFFFF == to
    assert data == [65533, 65534, 1, 2], data


def test_gap_backward_is_link_reset():
    """seq 가 뒤로 가면(11 다음 9) 손실이 아니라 reason 3 — 두 번호를 그대로 적고 lost_count 0."""
    gaps, data = _capture_seqs([10, 11, 9, 10])
    assert gaps == [(X.GAP_LINK_RESET, 11, 9, 0)], gaps
    assert data == [10, 11, 9, 10], data


# =============================================================================
# Runner
# =============================================================================

def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    tests = [
        ("pc_time_us = UTC µs (PcClock)", test_pc_clock_is_utc_us),
        ("PcClock origin in the past", test_pc_clock_origin_in_the_past),
        ("soak writes UTC µs", test_soak_writes_utc_us),
        ("demo writes UTC µs", test_demo_writes_utc_us),
        ("GAP endpoints are received seqs", test_gap_endpoints_are_received_seqs),
        ("GAP wraps at 65536", test_gap_wraps_at_65536),
        ("GAP backward = link reset", test_gap_backward_is_link_reset),
    ]

    failed = 0
    for name, fn in tests:
        try:
            fn()
            print("  PASS  " + name)
        except AssertionError as e:
            failed += 1
            print("  FAIL  " + name)
            print("        " + str(e))
        except Exception as e:  # noqa: BLE001
            failed += 1
            print("  ERROR " + name)
            print("        %s: %s" % (type(e).__name__, e))

    print("")
    if failed:
        print("%d/%d FAILED" % (failed, len(tests)))
        return 1
    print("%d/%d passed — .xmlog 필드의 뜻(PC 시각 · GAP 경계)이 형식 문서와 일치" % (len(tests), len(tests)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
