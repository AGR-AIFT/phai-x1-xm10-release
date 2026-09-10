#!/usr/bin/env python3
"""`demo_stream.py` 가 만드는 바이트가 규약과 맞는지 — **다른 구현**을 오라클로 쓴다.

    python test_demo_stream.py

왜 이 시험이 따로 있나
----------------------
데모가 스스로 만든 바이트를 스스로 풀어 "통과" 라고 말하면 아무것도 증명하지 못한다.
같은 오해를 양쪽이 공유하면 그대로 통과하기 때문이다. 그래서 여기서는

  * COBS 인코더를 **두 개** 맞댄다 (`demo_stream` vs `test_frame_router`),
  * 프레임 헤더·CRC·LEN 단위를 이 파일이 **직접** 뜯어 본다 (파서를 안 믿는다),
  * `0xEE` 스키마의 오프셋이 C 정렬 규칙과 맞는지 손으로 계산해 대조한다.

PLAN P2-2 가 말하는 독립 오라클이 이것이다.
"""
import struct
import sys
import zlib

import demo_stream as DS
import schema_0xee as EE
from frame_router import (PHAI_SOF, crc16_ccitt, cobs_decode, parse_phai_frame)
from test_frame_router import cobs_encode as ref_cobs_encode


_passed = 0


def ok(msg):
    global _passed
    _passed += 1
    print("  [OK] " + msg)


def test_cobs_two_implementations_agree():
    """서로 다른 손이 쓴 인코더 둘이 **바이트까지** 같아야 한다."""
    cases = [
        b"",
        b"\x00",
        b"\x00\x00\x00",
        bytes(range(1, 256)),
        bytes([1]) * 253,
        bytes([1]) * 254,                     # code == 0xFF 경계
        bytes([1]) * 255,
        bytes([1]) * 254 + b"\x00" + bytes([2]) * 254,
        bytes(1024),
        bytes((i * 37) % 256 for i in range(1500)),
    ]
    for i, data in enumerate(cases):
        a = DS.cobs_encode(data)
        b = ref_cobs_encode(data)
        assert a == b, "case %d (%d B): demo=%s ref=%s" % (i, len(data), a.hex(), b.hex())
        assert cobs_decode(a) == data, "case %d 왕복 실패" % i
        assert b"\x00" not in a, "case %d 인코딩 결과에 0 이 들어 있다" % i
    ok("COBS 두 구현이 %d개 경우에서 바이트 동일 + 왕복 무손실" % len(cases))


def test_wire_frame_layout_by_hand():
    """`wire_frame` 이 만든 것을 파서 대신 **손으로** 뜯는다."""
    payload = bytes([0xAA, 0xBB, 0xCC])       # 3 B -> 4 B 로 패딩돼야 한다
    w = DS.wire_frame(0x1234, 0xF0, payload, status=0x05)

    assert w[-1] == 0x00, "딜리미터가 없다"
    raw = cobs_decode(w[:-1])

    assert raw[0] == PHAI_SOF, "SOF %02X" % raw[0]
    assert raw[1] == 1, "LEN 은 4바이트 단위라 3 B payload 는 1 이어야: %d" % raw[1]
    assert raw[2] | (raw[3] << 8) == 0x1234, "SEQ"
    assert raw[4] == 0xF0, "MODULE"
    assert raw[5] == 0x05, "STATUS"
    assert raw[6:9] == payload, "payload 본문"
    assert raw[9] == 0x00, "패딩 자리는 0 이어야"
    want = crc16_ccitt(raw[:10])
    assert raw[10] | (raw[11] << 8) == want, "CRC"
    assert len(raw) == 6 + 4 + 2, "raw 길이 %d (6+4+2 여야)" % len(raw)
    ok("헤더/LEN(4B 단위)/패딩/CRC 를 손으로 뜯어 확인")


def test_corrupt_crc_is_rejected():
    good = DS.wire_frame(1, 0xF0, bytes(8))
    bad = DS.wire_frame(1, 0xF0, bytes(8), corrupt_crc=True)
    assert good != bad, "corrupt_crc 가 아무것도 안 바꿨다"
    pkt, err = parse_phai_frame(cobs_decode(good[:-1]), 0.0)
    assert pkt is not None and err is None, "정상 프레임이 거부됐다: %r" % err
    pkt, err = parse_phai_frame(cobs_decode(bad[:-1]), 0.0)
    assert pkt is None and err == "crc", "깨진 CRC 가 통과했다: %r / %r" % (pkt, err)
    ok("일부러 깨뜨린 CRC 는 'crc' 로 거부된다")


def test_total_payload_is_365_and_pads_to_368():
    p = DS.total_payload({"xm_loop_count": 7})
    assert len(p) == 365, "0x20 구조체 %d B (365 여야)" % len(p)
    raw = cobs_decode(DS.wire_frame(0, 0x20, p)[:-1])
    assert raw[1] == 92, "LEN %d (368/4 = 92 여야)" % raw[1]
    assert len(raw) == 6 + 368 + 2, "raw %d B" % len(raw)
    assert raw[6 + 365:6 + 368] == b"\x00\x00\x00", "패딩 3 B 가 0 이 아니다"
    ok("0x20 은 365 B 구조체 -> 와이어 368 B (LEN=92)")


def test_demo_struct_offsets_match_c_alignment():
    """`DemoUser_t` 오프셋을 ARM EABI 정렬 규칙으로 **직접 계산**해 대조한다."""
    # (이름, 크기, 정렬) 순서대로 쌓으면서 정렬 패딩을 넣는다
    layout = [("state", 1, 1), ("contact", 1, 1), ("emg_rms", 4, 4),
              ("angle_x10", 2, 2), ("tick", 4, 4), ("torque", 8, 4)]
    off = 0
    want = {}
    align_max = 1
    for name, size, align in layout:
        off = (off + align - 1) // align * align
        want[name] = off
        off += size
        align_max = max(align_max, align)
    size_of = (off + align_max - 1) // align_max * align_max

    assert size_of == DS.DEMO_STRUCT_SIZE, \
        "sizeof 계산 %d != 선언 %d" % (size_of, DS.DEMO_STRUCT_SIZE)
    for f in DS.DEMO_FIELDS:
        assert f.offset == want[f.name], \
            "%s offset %d (C 정렬로는 %d)" % (f.name, f.offset, want[f.name])
    # 패딩 구멍이 정말 둘 있어야 이 데모가 의미가 있다
    holes = size_of - sum(s for _n, s, _a in layout)
    assert holes == 4, "패딩이 %d B — 구멍 없는 구조체면 offset 시험이 무의미하다" % holes
    ok("DemoUser_t 오프셋이 C 정렬과 일치 (패딩 구멍 %d B)" % holes)


def test_typed_payload_holes_are_zero():
    p = DS.typed_payload(3, True, 1.5, -42, 999, (0.25, -0.5))
    assert len(p) == DS.DEMO_STRUCT_SIZE
    assert p[2:4] == b"\x00\x00", "offset 2..3 패딩이 0 이 아니다"
    assert p[10:12] == b"\x00\x00", "offset 10..11 패딩이 0 이 아니다"
    # 필드를 이어붙인 포맷으로 풀면 **틀려야** 한다 — 그게 offset 이 필요한 이유다
    naive = struct.unpack_from("<BBfhIff", p + bytes(8))
    assert naive[2] != 1.5, "이어붙인 포맷이 우연히 맞았다 — 데모가 아무것도 증명 못 한다"
    ok("패딩 자리는 0, 이어붙인 struct 포맷으로는 못 푼다")


def test_schema_fragment_round_trip():
    frag = EE.parse_fragment(DS.schema_frame_payload())
    assert frag.module_id == DS.MODULE_TYPED
    assert frag.struct_size == DS.DEMO_STRUCT_SIZE
    assert frag.frame_count == 1 and frag.frame_index == 0
    assert len(frag.fields) == len(DS.DEMO_FIELDS)
    for got, want in zip(frag.fields, DS.DEMO_FIELDS):
        # scale 만 빼고는 정확히 같아야 한다.
        assert (got.name, got.unit, got.type_tag, got.array_len, got.offset) == \
               (want.name, want.unit, want.type_tag, want.array_len, want.offset), \
               "필드가 왕복에서 변했다: %r != %r" % (got, want)
        # `scale` 은 FieldRecord 에서 **float32** 다(`<16s8sBBHf`). 0.1 처럼 이진으로
        # 딱 떨어지지 않는 값은 왕복하면 float64 원본과 정확히 같지 않다 —
        # 이건 결함이 아니라 와이어 규약이다. 호스트 코드가 스키마 CRC 나 캐시 키에
        # scale 의 **정확한 일치**를 쓰면 그때부터 결함이 된다.
        assert struct.unpack("<f", struct.pack("<f", want.scale))[0] == got.scale, \
            "%s scale %r != float32(%r)" % (got.name, got.scale, want.scale)
    assert DS.DEMO_FIELDS[3].scale != frag.fields[3].scale, \
        "0.1 이 float32 왕복에서 그대로다 — 이 시험이 아무것도 못박지 못한다"
    # CRC 는 이 파일이 직접 계산해 본다
    mine = zlib.crc32(EE.canonical_field_bytes(list(DS.DEMO_FIELDS))) & 0xFFFFFFFF
    assert frag.schema_crc32 == mine == DS.schema_crc32(), "CRC 불일치"
    ok("0xEE 스키마 왕복 + CRC 독립 계산 일치 (scale 은 f32 라 근사)")


def test_session_counts_match_expectation():
    """`DemoExpectation` 이 실제 스트림과 어긋나면 데모 판정이 거짓말을 한다."""
    stream, exp = DS.build_demo_session(cycles=15)

    seen = {}
    crc_bad = 0
    seqs = []
    buf = bytearray(stream)
    while True:
        d = buf.find(0)
        if d < 0:
            break
        if d > 0:
            pkt, err = parse_phai_frame(cobs_decode(bytes(buf[:d])), 0.0)
            if pkt is not None:
                seen[pkt.module_id] = seen.get(pkt.module_id, 0) + 1
                seqs.append(pkt.seq_id)
            elif err == "crc":
                crc_bad += 1
        del buf[:d + 1]

    assert seen.get(0x20) == exp.total_frames, \
        "0x20 %r != %d" % (seen.get(0x20), exp.total_frames)
    assert seen.get(DS.MODULE_TYPED) == exp.typed_frames, \
        "0xF0 %r != %d" % (seen.get(DS.MODULE_TYPED), exp.typed_frames)
    assert seen.get(DS.MODULE_META_ONLY) == exp.meta_only_frames
    assert seen.get(0xEE) == 1 and seen.get(0xEF) == 1, "스키마/메타가 1개씩이어야"
    assert sum(seen.values()) == exp.frames_ok, \
        "통과 프레임 %d != 기대 %d" % (sum(seen.values()), exp.frames_ok)
    assert crc_bad == exp.frames_crc_bad

    # seq 구멍이 정말 뚫려 있는가
    gaps = [(a, b) for a, b in zip(seqs, seqs[1:]) if b - a > 1]
    assert gaps, "seq 구멍이 없다 — 손실 회계가 시험되지 않는다"
    lost = sum(b - a - 1 for a, b in gaps)
    assert lost == exp.lost_frames + exp.frames_crc_bad, \
        "구멍 합 %d != 드롭 %d + CRC %d" % (lost, exp.lost_frames, exp.frames_crc_bad)
    ok("모듈별 프레임 수·CRC 실패·seq 구멍이 기대와 일치")


def test_data_really_precedes_schema():
    """데이터가 스키마보다 먼저 나가야 PLAN 4.1 경로가 시험된다."""
    stream, _exp = DS.build_demo_session(cycles=10)
    order = []
    buf = bytearray(stream)
    while True:
        d = buf.find(0)
        if d < 0:
            break
        if d > 0:
            pkt, err = parse_phai_frame(cobs_decode(bytes(buf[:d])), 0.0)
            if pkt is not None:
                order.append(pkt.module_id)
        del buf[:d + 1]
    first_data = order.index(DS.MODULE_TYPED)
    first_schema = order.index(0xEE)
    assert first_data < first_schema, \
        "스키마가 먼저 왔다 (data=%d, schema=%d) — 사후 해석 경로가 안 밟힌다" \
        % (first_data, first_schema)
    ok("0xF0 데이터 %d개가 0xEE 스키마보다 먼저 나간다" % first_schema)


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    print("demo_stream.py 검증 (독립 오라클)")
    test_cobs_two_implementations_agree()
    test_wire_frame_layout_by_hand()
    test_corrupt_crc_is_rejected()
    test_total_payload_is_365_and_pads_to_368()
    test_demo_struct_offsets_match_c_alignment()
    test_typed_payload_holes_are_zero()
    test_schema_fragment_round_trip()
    test_session_counts_match_expectation()
    test_data_really_precedes_schema()
    print("\n전부 통과 (%d 항목)" % _passed)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
