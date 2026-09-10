"""frame_router.py 자체 검증 — 보드 없이 돌아간다.

    python test_frame_router.py

pytest 없이 그냥 실행된다. 합성 스트림을 만들어 넣고, 마지막에는 data/ 의 실제
캡처 CSV 를 와이어로 되돌려 재생해 기존 세션이 깨지지 않았는지 확인한다.

"예전에는 이랬다" 는 주장을 주석으로만 적지 않으려고, 예전 로직(_old_seq_logic)을
그대로 재현해 두고 신/구를 나란히 돌려 비교한다. 주석이 틀려도 테스트가 잡는다.

여기서 확인하는 것:
  1. 시스템 프레임(0x20 등)이 사용자 채널에 섞이지 않는다 — 그리고 버려지지도 않는다
  2. 시스템 프레임이 섞여 있어도 거짓 손실이 잡히지 않는다
  3. 1초를 넘는 단선이 통째로 사라지지 않는다  ← 예전 코드의 진짜 결함
  4. seq 가 뒤로 가도 손실 통계와 시간축이 오염되지 않는다
  5. 채널 구성이 0x20 이 아니라 첫 '사용자' 프레임으로 정해진다
"""
import csv
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from frame_router import (            # noqa: E402
    FrameRouter, PhAIFrame, parse_phai_frame, cobs_decode, crc16_ccitt,
    PHAI_SOF, DEVICE_PERIOD_MS,
)

HERE = os.path.dirname(os.path.abspath(__file__))
SAMPLE_CSV = os.path.join(HERE, "data", "cdc_phai_20260224_165904.csv")

_passed = 0


def ok(msg):
    global _passed
    _passed += 1
    print("  [OK] " + msg)


# ============================================================================
# 예전 로직 재현 — "구 코드는 이랬다" 를 주석이 아니라 코드로 남긴다
# ============================================================================

def _old_seq_logic(seq_list):
    """변경 전 cdc_phai_receiver.py 의 _on_poll 시퀀스 계산 그대로.

        if self._last_seq >= 0:
            delta = (pkt.seq_id - self._last_seq) & 0xFFFF
            if delta > 1000:
                delta = 1
            if delta != 1:
                self._seq_drops += delta - 1
            self._device_time_s += delta * (DEVICE_PERIOD_MS / 1000.0)
        self._last_seq = pkt.seq_id

    module 필터가 없었다는 점이 중요하다 — 모든 프레임이 이 루프를 지났다.
    반환: (seq_drops, device_time_s)
    """
    last_seq = -1
    seq_drops = 0
    device_time_s = 0.0
    for seq in seq_list:
        if last_seq >= 0:
            delta = (seq - last_seq) & 0xFFFF
            if delta > 1000:
                delta = 1
            if delta != 1:
                seq_drops += delta - 1
            device_time_s += delta * (DEVICE_PERIOD_MS / 1000.0)
        last_seq = seq
    return seq_drops, device_time_s


def _new_seq_logic(seq_list):
    """현재 원장으로 같은 입력을 처리한다. 반환: (lost_count, device_time_s)"""
    r = FrameRouter()
    for seq in seq_list:
        r.route(mkframe(seq))
    return r.ledger.lost_count, r.ledger.device_time_s


# ============================================================================
# 테스트용 인코더 — 펌웨어 phai_packet_builder.c 와 같은 규약
# ============================================================================

def cobs_encode(data: bytes) -> bytes:
    out = bytearray()
    code_idx = len(out)
    out.append(0)
    code = 1
    for b in data:
        if b == 0:
            out[code_idx] = code
            code_idx = len(out)
            out.append(0)
            code = 1
        else:
            out.append(b)
            code += 1
            if code == 0xFF:
                out[code_idx] = code
                code_idx = len(out)
                out.append(0)
                code = 1
    out[code_idx] = code
    return bytes(out)


def build_wire(seq_id, module_id, floats, status=0):
    """반환: (와이어 바이트(딜리미터 포함), COBS 이전 raw 프레임 길이)"""
    payload = struct.pack("<%df" % len(floats), *floats)
    body = bytearray([PHAI_SOF, len(floats), seq_id & 0xFF, (seq_id >> 8) & 0xFF,
                      module_id, status])
    body.extend(payload)
    crc = crc16_ccitt(bytes(body))
    body.append(crc & 0xFF)
    body.append((crc >> 8) & 0xFF)
    return cobs_encode(bytes(body)) + b"\x00", len(body)


def feed(stream: bytes, max_chunk=None, seed=4242):
    """수신기와 같은 방식으로 와이어를 분해해 router 에 넣는다.

    max_chunk 를 주면 1..max_chunk 바이트의 임의 조각으로 쪼개 넣는다
    (USB 수신이 어떤 크기로 끊길지 보장이 없으므로).
    """
    router = FrameRouter()
    user_frames = []
    errs = 0
    wire_buf = bytearray()

    if max_chunk is None:
        chunks = [stream]
    else:
        chunks = []
        rng = random.Random(seed)
        i = 0
        while i < len(stream):
            n = rng.randrange(1, max_chunk + 1)
            chunks.append(stream[i:i + n])
            i += n

    for chunk in chunks:
        wire_buf.extend(chunk)
        while True:
            delim = wire_buf.find(b"\x00")
            if delim < 0:
                break
            if delim > 0:
                pkt, err = parse_phai_frame(cobs_decode(bytes(wire_buf[:delim])), 0.0)
                if err is not None:
                    errs += 1
                elif router.route(pkt)[0] == "user_primary":
                    user_frames.append(pkt)
            del wire_buf[:delim + 1]

    return router, user_frames, errs


def mkframe(seq, module_id=0xF0, payload=b"\x00" * 4):
    return PhAIFrame(seq, module_id, 0, payload, 12, 0.0)


def seq_run(start, count):
    """start 부터 count 개의 연속 seq"""
    return [(start + i) & 0xFFFF for i in range(count)]


# ============================================================================
# 1. 프레임 파싱
# ============================================================================

def test_parse():
    print("\n[1] 프레임 파싱")
    wire, raw_len = build_wire(1234, 0xF0, [1.5, -2.5, 3.5])
    pkt, err = parse_phai_frame(cobs_decode(wire[:-1]), 0.0)
    assert err is None, err
    assert pkt.seq_id == 1234 and pkt.module_id == 0xF0
    assert pkt.wire_len == raw_len
    assert list(pkt.as_float32()) == [1.5, -2.5, 3.5]
    ok("정상 프레임 왕복 (raw %d 바이트)" % raw_len)

    raw = bytearray(cobs_decode(wire[:-1]))
    raw[-1] ^= 0xFF
    assert parse_phai_frame(bytes(raw), 0.0)[1] == "crc"
    ok("CRC 손상 감지")

    raw = bytearray(cobs_decode(wire[:-1]))
    raw[0] = 0x55
    assert parse_phai_frame(bytes(raw), 0.0)[1] == "sync"
    ok("SOF 손상 감지")

    # payload 에 0x00 이 섞인 경우 — 실제 float 데이터에 흔하다. COBS 가 이걸 다룬다.
    wire, _ = build_wire(7, 0xF0, [0.0, 1.0, 0.0, -1.0])
    pkt, err = parse_phai_frame(cobs_decode(wire[:-1]), 0.0)
    assert err is None and list(pkt.as_float32()) == [0.0, 1.0, 0.0, -1.0]
    ok("payload 안 0x00 바이트 왕복")

    # 최대 프레임 (len_units=255 -> payload 1020B)
    wire, raw_len = build_wire(9, 0xF0, [1.0] * 255)
    pkt, err = parse_phai_frame(cobs_decode(wire[:-1]), 0.0)
    assert err is None and len(pkt.as_float32()) == 255
    ok("최대 프레임 왕복 (payload 1020B, raw %d 바이트)" % raw_len)


# ============================================================================
# 2. 전역 시퀀스 원장 — 신/구를 나란히 돌려 비교한다
# ============================================================================

def test_ledger():
    print("\n[2] 전역 시퀀스 원장")

    # --- 500 손실: 예전 클램프(임계값 1000)가 발동하지 않는 구간. 신/구가 같아야 한다 ---
    seqs = seq_run(0, 200) + seq_run(700, 200)      # 199 -> 700, delta=501
    old_drops, _ = _old_seq_logic(seqs)
    new_lost, _ = _new_seq_logic(seqs)
    assert new_lost == 500, new_lost
    assert old_drops == 500, old_drops
    ok("500 손실 -> 신 %d / 구 %d (클램프 임계값 1000 미만이라 예전에도 정확했다)"
       % (new_lost, old_drops))

    # --- 1500 손실: 여기서 예전 코드가 무너진다 ---
    # delta=1501 > 1000 이면 delta 를 1 로 덮어쓰는데, 그러면 뒤따르는 `if delta != 1` 이
    # False 가 되어 손실이 '축소'가 아니라 아예 '0건' 으로 사라진다.
    seqs = seq_run(0, 200) + seq_run(1700, 200)     # 199 -> 1700, delta=1501
    old_drops, _ = _old_seq_logic(seqs)
    new_lost, _ = _new_seq_logic(seqs)
    assert new_lost == 1500, new_lost
    assert old_drops == 0, old_drops
    ok("1500 손실 -> 신 %d / 구 %d — 예전 코드는 1초 넘는 단선을 통째로 삼켰다"
       % (new_lost, old_drops))

    # --- 시간축도 확인한다. 손실 구간에도 시간은 흘러야 한다 ---
    seqs = seq_run(0, 200) + seq_run(1700, 200)
    _, new_time = _new_seq_logic(seqs)
    # 첫 프레임은 기준점이라 시간을 더하지 않는다. 이후 delta 합 = 199 + 1501 + 199 = 1899
    expected = 1899 * (DEVICE_PERIOD_MS / 1000.0)
    assert abs(new_time - expected) < 1e-9, (new_time, expected)
    ok("device_time_s = %.3f 초 (기대 %.3f) — 손실 구간만큼 시간이 흐른다"
       % (new_time, expected))

    # 무손실 스트림의 시간축
    _, t = _new_seq_logic(seq_run(0, 1001))
    assert abs(t - 1.0) < 1e-9, t
    ok("무손실 1001 프레임 -> device_time_s = 1.000 초")

    # --- wraparound 는 손실이 아니다 ---
    r = FrameRouter()
    r.route(mkframe(65535))
    r.route(mkframe(0))
    assert r.ledger.lost_count == 0 and r.ledger.resync_count == 0
    ok("seq wraparound 65535 -> 0 은 손실도 역행도 아님")

    # --- 중복 seq ---
    r = FrameRouter()
    r.route(mkframe(10)); r.route(mkframe(10))
    assert r.ledger.lost_count == 0 and r.ledger.zero_delta_count == 1
    ok("같은 seq 중복 도착은 손실 아님 (zero_delta 로 따로 센다)")

    # --- 역행: 한 프레임이 세션 전체를 오염시키면 안 된다 ---
    r = FrameRouter()
    r.route(mkframe(100))
    r.route(mkframe(99))          # 뒤로 1 -> mod 연산으로는 65535 로 보인다
    assert r.ledger.lost_count == 0, r.ledger.lost_count
    assert r.ledger.resync_count == 1, r.ledger.resync_count
    assert r.ledger.device_time_s == 0.0, r.ledger.device_time_s
    ok("seq 역행 -> 손실 0 / resync 1 (그냥 세면 손실 65534, 시간 +65.5초가 된다)")

    # 역행 후에도 정상 계측이 이어진다
    for s in seq_run(100, 10):
        r.route(mkframe(s))
    assert r.ledger.lost_count == 0, r.ledger.lost_count
    ok("역행 직후 기준을 다시 잡고 정상 계측 재개")


# ============================================================================
# 3. 시스템 프레임 라우팅 — 섞이지 않고, 버려지지도 않는다
# ============================================================================

def test_routing():
    print("\n[3] 시스템 프레임 라우팅")

    stream = bytearray()
    sent20 = bytes20 = 0
    seq = 0
    all_seqs = []
    for i in range(2000):
        w, raw_len = build_wire(seq, 0x20, [0.5] * 91)
        stream.extend(w); sent20 += 1; bytes20 += raw_len
        all_seqs.append(seq); seq = (seq + 1) & 0xFFFF

        w, _ = build_wire(seq, 0xF0, [float(i)] * 4)
        stream.extend(w)
        all_seqs.append(seq); seq = (seq + 1) & 0xFFFF

    router, user_frames, errs = feed(bytes(stream))
    assert errs == 0, errs

    # 혼입 0건
    assert len(user_frames) == 2000, len(user_frames)
    assert all(p.module_id == 0xF0 for p in user_frames)
    ok("사용자 스트림 2000 프레임에 0x20 혼입 0건")

    # 버려지지 않고 정확히 보존
    tap = router.system_taps[0x20]
    assert tap.frame_count == sent20, (tap.frame_count, sent20)
    assert tap.byte_count == bytes20, (tap.byte_count, bytes20)
    ok("0x20 이 %d 프레임 / %d 바이트로 정확히 보존" % (tap.frame_count, tap.byte_count))

    # 개수·합계는 오라클이 아니다 — 내용까지 봐야 한다.
    # payload 를 전부 다른 값으로 바꾸되 **길이는 그대로** 둔 스트림과 대조한다.
    # 구 tap(개수 + 바이트 합)은 이 두 스트림을 구분하지 못했다.
    mutated = bytearray()
    seq_m = 0
    for i in range(2000):
        w, _ = build_wire(seq_m, 0x20, [1.5] * 91)     # 0.5 -> 1.5, 길이 동일
        mutated.extend(w); seq_m = (seq_m + 1) & 0xFFFF
        w, _ = build_wire(seq_m, 0xF0, [float(i)] * 4)
        mutated.extend(w); seq_m = (seq_m + 1) & 0xFFFF

    router_m, _, errs_m = feed(bytes(mutated))
    assert errs_m == 0, errs_m
    tap_m = router_m.system_taps[0x20]

    assert tap_m.frame_count == tap.frame_count, "구 지표(개수)가 달라 대조가 성립 안 함"
    assert tap_m.byte_count == tap.byte_count, "구 지표(바이트)가 달라 대조가 성립 안 함"
    assert tap_m.content_crc != tap.content_crc, "내용이 바뀌었는데 지문이 같다"
    ok("payload 를 전부 바꾸고 길이만 유지하면 개수·바이트는 동일하고 crc32 만 다르다 "
       "(%08x vs %08x) — 구 지표로는 못 잡던 것" % (tap.content_crc, tap_m.content_crc))

    # 순서 뒤바뀜도 잡힌다 — 동일 payload 두 프레임의 seq 만 맞바꾼 스트림
    fa, _ = build_wire(0, 0x20, [0.5] * 91)
    fb, _ = build_wire(1, 0x20, [0.5] * 91)
    r_fwd, _, e1 = feed(bytes(fa + fb))
    r_rev, _, e2 = feed(bytes(fb + fa))
    assert e1 == 0 and e2 == 0, (e1, e2)
    c_fwd = r_fwd.system_taps[0x20].content_crc
    c_rev = r_rev.system_taps[0x20].content_crc
    assert c_fwd != c_rev, "순서가 바뀌었는데 지문이 같다"
    ok("동일 payload 프레임의 순서가 바뀌면 crc32 도 다르다 (체인이 순서 의존)")

    # 거짓 손실 0건
    assert router.ledger.lost_count == 0, router.ledger.lost_count

    # 예전 코드도 이 스트림에서는 거짓 손실이 0 이었다 — module 필터가 아예 없었기 때문이다.
    # 즉 이 항목에서 예전 코드가 틀렸던 게 아니다(아래 test_first_frame_lock 이 진짜 결함).
    old_drops, _ = _old_seq_logic(all_seqs)
    assert old_drops == 0, old_drops

    # 위험한 것은 '필터만 넣고 원장 갱신 순서는 안 고친' 수정안이다.
    # 사용자 프레임끼리만 seq 를 비교하면 사이에 낀 0x20 이 전부 손실로 잡힌다.
    naive_last, naive_lost = -1, 0
    for p in user_frames:
        if naive_last >= 0:
            d = (p.seq_id - naive_last) & 0xFFFF
            if d > 1:
                naive_lost += d - 1
        naive_last = p.seq_id
    assert naive_lost == 1999, naive_lost
    ok("거짓 손실 0건 — 구 코드도 0 이었지만, 필터만 넣은 순진한 수정안이면 %d 건이 된다"
       % naive_lost)


def test_first_frame_lock():
    print("\n[4] 첫 프레임 채널 잠금 — 예전 코드의 진짜 결함")

    # 예전 _on_poll 은 배치의 '첫 프레임'으로 채널 구성을 잠갔다.
    # 0x20 auto-stream 이 기본 ON 이라 그 첫 프레임은 대개 0x20 이었고,
    # 그래서 사용자 채널이 0x20 기준으로 잘못 잠겼다.
    r = FrameRouter()
    seq = 0
    for _ in range(50):
        r.route(mkframe(seq, 0x20)); seq = (seq + 1) & 0xFFFF
    tag, _ = r.route(mkframe(seq, 0xF0))
    assert tag == "user_primary" and r.primary_user_module == 0xF0
    ok("0x20 이 50개 먼저 와도 primary 는 0xF0 (예전에는 0x20 으로 잠겼다)")

    # primary 가 아닌 두 번째 사용자 module 은 그래프·CSV 에 안 들어가지만 세어는 둔다
    tag, _ = r.route(mkframe(seq + 1, 0xF1))
    assert tag == "user_other"
    assert r.other_user_frames == 1
    ok("primary 아닌 사용자 module 은 user_other 로 분리되고 개수가 남는다")


# ============================================================================
# 5. 청크 경계 강건성
# ============================================================================

def test_chunking():
    print("\n[5] 청크 경계 강건성")
    stream = bytearray()
    seq = 0
    for i in range(500):
        w, _ = build_wire(seq, 0x20, [0.5] * 8); stream.extend(w)
        seq = (seq + 1) & 0xFFFF
        w, _ = build_wire(seq, 0xF0, [float(i)] * 4); stream.extend(w)
        seq = (seq + 1) & 0xFFFF

    whole = feed(bytes(stream))
    split = feed(bytes(stream), max_chunk=50)

    assert split[2] == whole[2] == 0
    assert len(split[1]) == len(whole[1])
    assert split[0].ledger.lost_count == whole[0].ledger.lost_count
    assert split[0].ledger.frame_count == whole[0].ledger.frame_count
    assert split[0].primary_user_module == whole[0].primary_user_module
    assert (split[0].system_taps[0x20].byte_count
            == whole[0].system_taps[0x20].byte_count)
    ok("1~50 바이트 임의 분할 결과가 일괄 투입과 완전히 동일")


# ============================================================================
# 6. 섞인 스트림 + 의도적 손실 — 시간축 재구성까지 확인
# ============================================================================

def test_mixed_with_gap():
    print("\n[6] 섞인 스트림 + 의도적 손실")

    stream = bytearray()
    seq = 0
    n_user = 0
    for i in range(300):
        w, _ = build_wire(seq, 0x20, [0.5] * 8); stream.extend(w)
        seq = (seq + 1) & 0xFFFF
        w, _ = build_wire(seq, 0xF0, [float(i)] * 4); stream.extend(w)
        seq = (seq + 1) & 0xFFFF
        n_user += 1

    # 1200 프레임을 통째로 잃는다 (1.2초 단선) — 예전 코드였다면 0건으로 사라졌을 구간
    seq = (seq + 1200) & 0xFFFF

    for i in range(300):
        w, _ = build_wire(seq, 0x20, [0.5] * 8); stream.extend(w)
        seq = (seq + 1) & 0xFFFF
        w, _ = build_wire(seq, 0xF0, [float(i)] * 4); stream.extend(w)
        seq = (seq + 1) & 0xFFFF
        n_user += 1

    router, user_frames, errs = feed(bytes(stream))
    assert errs == 0
    assert len(user_frames) == n_user, (len(user_frames), n_user)
    assert router.ledger.lost_count == 1200, router.ledger.lost_count
    ok("0x20 혼재 + 1200 프레임 단선 -> 손실 정확히 1200, 사용자 %d 프레임 보존" % n_user)

    # 프레임이 자기 시각을 들고 다니는지 — 단선 구간에서 시간이 건너뛰어야 한다
    times = [p.device_time_s for p in user_frames]
    assert times == sorted(times), "device_time_s 가 단조증가하지 않는다"
    jump = times[300] - times[299]
    assert abs(jump - 1.202) < 1e-6, jump
    ok("단선 경계에서 device_time_s 가 %.3f 초 건너뛴다 (손실 1200 + 정상 2틱)" % jump)


# ============================================================================
# 7. 워커 큐 — 큐 오버플로가 와이어 손실로 둔갑하지 않아야 한다
# ============================================================================

def test_worker_queue():
    print("\n[7] 워커 큐 (PyQt5 필요)")
    try:
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
        import cdc_phai_receiver as recv
    except ImportError as e:
        print("  [SKIP] %s" % e)
        return

    w = recv.PhAISerialWorker("COM_TEST")
    maxlen = w.packet_queue.maxlen
    total = maxlen + 10000

    for i in range(total):
        wire, _ = build_wire(i & 0xFFFF, 0xF0, [1.0, 2.0, 3.0, 4.0])
        w._parse_frame(cobs_decode(wire[:-1]), 0.0)

    assert w.good == total, (w.good, total)
    assert w.crc_err == 0 and w.sync_err == 0
    assert w.queue_overflow_count == total - maxlen, w.queue_overflow_count
    ok("%d 개 투입(maxlen=%d) -> 오버플로 %d (정확히 초과분)"
       % (total, maxlen, w.queue_overflow_count))

    # 핵심: 큐에서 밀려난 프레임도 '와이어로는 도착한' 것이다.
    # 시퀀스 회계를 큐 뒤에서 하면 이 값이 오버플로 수만큼 부풀어 로컬 지연이
    # 케이블 문제로 오진된다. 큐 앞에서 세므로 0 이어야 한다.
    assert w.router.ledger.lost_count == 0, w.router.ledger.lost_count
    assert w.router.ledger.frame_count == total, w.router.ledger.frame_count
    ok("큐가 %d 개를 버렸지만 와이어 손실은 0 — 회계가 큐 앞에서 이뤄진다"
       % w.queue_overflow_count)

    # 큐에는 (프레임, 태그) 가 들어간다
    pkt, tag = w.packet_queue[0]
    assert tag == "user_primary" and isinstance(pkt, PhAIFrame)
    ok("큐 원소는 (프레임, 라우팅 태그) 쌍")


# ============================================================================
# 8. 실제 캡처 재생 (회귀)
# ============================================================================

def test_regression():
    print("\n[8] 실제 캡처 재생 (회귀)")
    if not os.path.exists(SAMPLE_CSV):
        print("  [SKIP] 샘플 CSV 가 없다: %s" % SAMPLE_CSV)
        return

    rows = []
    with open(SAMPLE_CSV, newline="") as f:
        rdr = csv.reader(f)
        next(rdr)
        for r in rdr:
            if r:
                rows.append((int(r[2]), int(r[3]), [float(x) for x in r[5:]]))

    stream = bytearray()
    for seq, mid, vals in rows:
        w, _ = build_wire(seq, mid, vals)
        stream.extend(w)

    router, user_frames, errs = feed(bytes(stream))
    assert errs == 0, errs
    assert len(user_frames) == len(rows), (len(user_frames), len(rows))
    assert router.ledger.lost_count == 0, router.ledger.lost_count
    assert router.primary_user_module == rows[0][1]
    ok("%d 행 전부 사용자 채널로, 손실 0 / 파싱오류 0" % len(rows))

    for idx in (0, len(rows) // 2, len(rows) - 1):
        got, want = user_frames[idx].as_float32(), rows[idx][2]
        assert len(got) == len(want)
        assert all(abs(float(a) - b) < 1e-6 for a, b in zip(got, want))
    ok("첫/중간/끝 행의 채널 값이 원본과 일치")

    # 이 캡처는 module_id 1종 · seq 연속(gap 0) 이라 순수한 바이트 왕복 검증이다.
    # 라우팅과 손실 계산은 위 합성 스트림들이 담당한다.
    mods = {m for _, m, _ in rows}
    assert len(mods) == 1
    print("     (참고: 이 캡처는 module 0x%02X 단일 · gap 0 이라 왕복 검증 전용이다)"
          % list(mods)[0])


def main():
    """다른 러너가 in-process 로 부를 수 있게 한 진입점 (형제 시험 파일과 같은 규약).

    실패는 AssertionError 로 튄다 — 부르는 쪽이 잡아 종료코드로 옮긴다.
    """
    # Windows 기본 콘솔(cp949)에서 em dash 를 찍다 UnicodeEncodeError 로 죽던 것을 막는다.
    # README 는 이 파일을 "보드 없이 돌아간다"는 검증으로 안내하는데, 정작 기본 환경에서는
    # 실행 자체가 안 됐다 (2026-09-10 실측).
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    print("frame_router.py 검증")
    test_parse()
    test_ledger()
    test_routing()
    test_first_frame_lock()
    test_chunking()
    test_mixed_with_gap()
    test_worker_queue()
    test_regression()
    print("\n전부 통과 (%d 항목)" % _passed)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
