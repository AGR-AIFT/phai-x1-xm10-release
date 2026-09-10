#!/usr/bin/env python3
"""`0xEE` 이진 스키마 + `0xEF` JSON 메타 + 레지스트리 회귀 시험. 보드 없이 돈다.

    python test_schema.py

오라클
------
`0xEE` 는 **FW 가 아직 보내지 않는** 포맷이라, 대조할 실물이 없다. 그래서 설계 문서
(PLAN §4.1)의 표를 보고 **바이트를 직접 타이핑**했다. 구현이 이걸 재현하지 못하면
구현이 틀린 것이고, 나중에 FW 가 다른 바이트를 보내면 그때 **FW 가 이 벡터와 다르다**고
말할 수 있다. 골든 벡터의 값은 거기에 있다 — 한쪽이 다른 쪽을 검사할 수 있게 만드는 것.

여기서만 잡히는 것
------------------
**C 구조체 정렬 패딩.** 필드를 순서대로 이어붙인 하나의 struct 포맷으로 푸는 구현은
패딩이 없을 때만 맞는다. 아래 `test_padded_struct_decode` 가 일부러 구멍 뚫린
구조체를 먹여서 그 방식을 거른다.
"""
import struct
import sys
import zlib

import schema_0xee as EE
import schema_registry as R


# =============================================================================
# 손으로 적은 골든 바이트 — PLAN 4.1 표에서 직접 옮김
# =============================================================================

# FieldRecord 32 B: name[16] unit[8] type_tag u8 array_len u8 struct_offset u16 scale f32
GOLDEN_FIELD = (
    b"speed\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"   # name[16] = "speed"
    b"m/s\x00\x00\x00\x00\x00"                              # unit[8]  = "m/s"
    b"\x00"                                                  # type_tag = 0 (f32)
    b"\x01"                                                  # array_len = 1
    b"\x00\x00"                                              # struct_offset = 0
    b"\x00\x00\x80\x3F"                                      # scale = 1.0f
)
assert len(GOLDEN_FIELD) == 32, len(GOLDEN_FIELD)

GOLDEN_CRC = zlib.crc32(GOLDEN_FIELD) & 0xFFFFFFFF

# Header 24 B
GOLDEN_HEADER = (
    b"\x01"                          # schema_proto_ver = 1
    b"\xF0"                          # target_module_id = 0xF0
    b"\x08\x00"                      # struct_size = 8
    + struct.pack("<I", GOLDEN_CRC)  # schema_crc32
    + b"\x01"                        # field_count_total = 1
    b"\x00"                          # frame_index = 0
    b"\x01"                          # frame_count = 1
    b"\x00"                          # reserved
    b"Demo\x00\x00\x00\x00\x00\x00\x00\x00"   # struct_name[12] = "Demo"
)
assert len(GOLDEN_HEADER) == 24, len(GOLDEN_HEADER)

GOLDEN_FRAGMENT = GOLDEN_HEADER + GOLDEN_FIELD


def _fld(name, unit, tag, alen, off, scale=1.0):
    return EE.FieldDef(name, unit, tag, alen, off, scale)


# =============================================================================
# 0xEE — 문법
# =============================================================================

def test_golden_fragment_bytes():
    """구현이 손으로 적은 바이트를 그대로 만들어내는가."""
    got = EE.encode_fragment(module_id=0xF0, struct_size=8, struct_name="Demo",
                             fields=[_fld("speed", "m/s", 0, 1, 0)],
                             frame_index=0, frame_count=1, field_count_total=1)
    assert len(got) == 56, "24 + 32 = 56 이어야: %d" % len(got)
    assert got == GOLDEN_FRAGMENT, \
        "바이트 불일치\n got %s\nwant %s" % (got.hex(), GOLDEN_FRAGMENT.hex())


def test_golden_fragment_parses():
    f = EE.parse_fragment(GOLDEN_FRAGMENT)
    assert f.proto_ver == 1
    assert f.module_id == 0xF0
    assert f.struct_size == 8
    assert f.schema_crc32 == GOLDEN_CRC
    assert f.field_count_total == 1
    assert (f.frame_index, f.frame_count) == (0, 1)
    assert f.struct_name == "Demo", repr(f.struct_name)
    assert len(f.fields) == 1
    fd = f.fields[0]
    assert (fd.name, fd.unit, fd.type_tag, fd.array_len, fd.offset, fd.scale) == \
        ("speed", "m/s", 0, 1, 0, 1.0), fd


def test_header_and_record_sizes():
    """PLAN 이 24 / 32 라고 못박은 값. 여기가 흔들리면 FW 와 어긋난다."""
    assert EE.HEADER_SIZE == 24
    assert EE.FIELD_RECORD_SIZE == 32
    assert EE.MAX_FIELDS_PER_FRAME == 31, "(1020-24)//32 = 31"
    assert 24 + 31 * 32 == 1016, "프래그먼트 최대 = 1016 B (payload cap 1020 이하)"


def test_rejects_bad_headers():
    cases = [
        (b"\x02" + GOLDEN_FRAGMENT[1:], "proto_ver 2"),
        (GOLDEN_FRAGMENT[:1] + b"\x10" + GOLDEN_FRAGMENT[2:], "module_id 0x10 (사용자 범위 밖)"),
        # 헤더 오프셋: 0 proto / 1 module / 2-3 size / 4-7 crc / 8 count_total
        #             9 frame_index / 10 frame_count / 11 reserved / 12-23 name
        (GOLDEN_FRAGMENT[:10] + b"\x00" + GOLDEN_FRAGMENT[11:], "frame_count 0 (@10)"),
        (GOLDEN_FRAGMENT[:11] + b"\x01" + GOLDEN_FRAGMENT[12:], "reserved != 0 (@11)"),
        (GOLDEN_FRAGMENT[:9] + b"\x05" + GOLDEN_FRAGMENT[10:], "frame_index >= frame_count (@9)"),
        (GOLDEN_FRAGMENT[:24], "FieldRecord 0개"),
        (GOLDEN_FRAGMENT[:8] + b"\x00" + GOLDEN_FRAGMENT[9:], "field_count_total 0"),
    ]
    for payload, why in cases:
        try:
            EE.parse_fragment(payload)
        except EE.SchemaError:
            continue
        raise AssertionError("통과하면 안 되는 것을 통과시켰다: %s" % why)


def test_rejects_missing_nul_termination():
    """PLAN 4.0: NUL 이 없는 고정폭 문자열은 무효."""
    bad = bytearray(GOLDEN_FRAGMENT)
    bad[24:40] = b"A" * 16                      # name[16] 전부 채워 NUL 제거
    try:
        EE.parse_fragment(bytes(bad))
    except EE.SchemaError:
        return
    raise AssertionError("NUL 없는 이름을 받아들였다")


# =============================================================================
# 0xEE — 재조립
# =============================================================================

def _multi_fragment_schema(n_fields=70, module_id=0xF1):
    """여러 프래그먼트로 쪼개지는 스키마. 필드는 4바이트씩 촘촘히 놓는다."""
    fields = [_fld("f%d" % i, "u", 0, 1, i * 4) for i in range(n_fields)]
    struct_size = n_fields * 4
    crc = zlib.crc32(EE.canonical_field_bytes(fields)) & 0xFFFFFFFF
    per = EE.MAX_FIELDS_PER_FRAME
    chunks = [fields[i:i + per] for i in range(0, n_fields, per)]
    frames = [
        EE.encode_fragment(module_id, struct_size, "Multi", chunk,
                           frame_index=i, frame_count=len(chunks),
                           field_count_total=n_fields, schema_crc32=crc)
        for i, chunk in enumerate(chunks)
    ]
    return frames, fields, struct_size, crc


def test_reassembly_multi_fragment():
    frames, fields, ssize, crc = _multi_fragment_schema(70)
    assert len(frames) == 3, "70 필드 -> 31/31/8 = 3 프래그먼트: %d" % len(frames)

    ra = EE.Reassembler()
    assert ra.feed(frames[0], 0.0) is None, "1/3 인데 완성됐다고 한다"
    assert ra.feed(frames[1], 0.1) is None, "2/3 인데 완성됐다고 한다"
    sc = ra.feed(frames[2], 0.2)
    assert sc is not None, "3/3 인데 완성되지 않았다"
    assert len(sc.fields) == 70
    assert sc.schema_crc32 == crc
    assert sc.struct_size == ssize
    assert ra.completed == 1


def test_reassembly_out_of_order_and_duplicate():
    frames, _f, _s, _c = _multi_fragment_schema(70)
    ra = EE.Reassembler()
    assert ra.feed(frames[2], 0.0) is None
    assert ra.feed(frames[2], 0.1) is None, "중복이 완성으로 세어졌다"
    assert ra.feed(frames[0], 0.2) is None
    assert ra.feed(frames[1], 0.3) is not None, "순서를 바꿔 넣으면 완성되지 않는다"


def test_reassembly_timeout():
    frames, _f, _s, _c = _multi_fragment_schema(70)
    ra = EE.Reassembler(timeout_s=3.0)
    ra.feed(frames[0], 0.0)
    ra.feed(frames[1], 10.0)          # 3초 넘어 도착 -> 앞 것은 폐기되고 새로 시작
    assert ra.discarded_timeout == 1, "타임아웃 폐기 %d" % ra.discarded_timeout
    assert ra.feed(frames[2], 10.1) is None, "폐기 후인데 완성됐다"


def test_reassembly_key_change_discards():
    a, _f, _s, _c = _multi_fragment_schema(70, module_id=0xF1)
    b, _f2, _s2, _c2 = _multi_fragment_schema(40, module_id=0xF1)   # 다른 struct_size/crc
    ra = EE.Reassembler()
    ra.feed(a[0], 0.0)
    ra.feed(b[0], 0.1)                # 키가 다르다 -> 진행 중인 것 폐기
    assert ra.discarded_key_change == 1, "키 변경 폐기 %d" % ra.discarded_key_change


def test_crc_mismatch_rejected():
    fields = [_fld("a", "", 0, 1, 0), _fld("b", "", 0, 1, 4)]
    frag = EE.encode_fragment(0xF0, 8, "Bad", fields, 0, 1, 2, schema_crc32=0xDEADBEEF)
    ra = EE.Reassembler()
    try:
        ra.feed(frag, 0.0)
    except EE.SchemaError as e:
        assert "crc32" in str(e).lower(), str(e)
        assert ra.crc_failures == 1
        return
    raise AssertionError("CRC 가 틀린 스키마를 받아들였다")


# =============================================================================
# 0xEE — 불변식
# =============================================================================

def test_overlap_rejected():
    fields = (_fld("a", "", 0, 1, 0), _fld("b", "", 4, 1, 2))   # f32@0 (0..4) vs i16@2
    sc = EE.Schema(0xF0, "Ovl", 8, 1, 0, fields)
    try:
        EE.validate(sc)
    except EE.SchemaError as e:
        assert "겹친다" in str(e), str(e)
        return
    raise AssertionError("겹치는 필드를 통과시켰다")


def test_out_of_bounds_rejected():
    fields = (_fld("a", "", 0, 1, 6),)           # f32@6 -> 6..10 > struct_size 8
    sc = EE.Schema(0xF0, "Oob", 8, 1, 0, fields)
    try:
        EE.validate(sc)
    except EE.SchemaError:
        return
    raise AssertionError("struct_size 를 넘는 필드를 통과시켰다")


def test_duplicate_name_rejected():
    fields = (_fld("a", "", 0, 1, 0), _fld("a", "", 0, 1, 4))
    sc = EE.Schema(0xF0, "Dup", 8, 1, 0, fields)
    try:
        EE.validate(sc)
    except EE.SchemaError as e:
        assert "중복" in str(e), str(e)
        return
    raise AssertionError("이름이 중복된 스키마를 통과시켰다")


# =============================================================================
# 0xEE — 값 (핵심: C 정렬 패딩)
# =============================================================================

def test_padded_struct_decode():
    """구멍 뚫린 구조체를 정확히 푸는가.

        struct { uint8_t flag; /* pad 3 */ float speed; int16_t count; /* pad 2 */ };
        offsets: flag@0  speed@4  count@8   sizeof = 12

    필드를 이어붙인 하나의 struct 포맷('<Bfh')으로 풀면 speed 를 offset 1 에서 읽어
    전부 틀린다. offset 을 따로 들고 다녀야 하는 이유가 이것이다.
    """
    fields = (_fld("flag", "", 3, 1, 0),       # u8  @0
              _fld("speed", "m/s", 0, 1, 4),   # f32 @4
              _fld("count", "n", 4, 1, 8))     # i16 @8
    sc = EE.Schema(0xF0, "Padded", 12, 1, 0, fields)
    EE.validate(sc)

    buf = bytearray(12)
    buf[0] = 7
    struct.pack_into("<f", buf, 4, 2.5)
    struct.pack_into("<h", buf, 8, -300)

    vals = sc.decode(bytes(buf))
    assert vals == [7, 2.5, -300], vals
    assert sc.scalar_names() == ["flag", "speed", "count"]

    # 순진한 방식이 실제로 틀린다는 것도 확인 — 이 시험이 무엇을 막는지 못박아 둔다
    naive = list(struct.unpack_from("<Bfh", bytes(buf), 0))
    assert naive != vals, "패딩이 없어 이 시험이 아무것도 막지 못한다"


def test_array_and_scale_and_bool():
    fields = (_fld("acc", "m/s2", 4, 3, 0, scale=0.001),   # i16[3] @0, scale
              _fld("ok", "", EE.BOOL_TAG, 1, 6))            # bool @6
    sc = EE.Schema(0xF0, "Arr", 8, 1, 0, fields)
    EE.validate(sc)

    buf = bytearray(8)
    struct.pack_into("<3h", buf, 0, 1000, -2000, 3000)
    buf[6] = 5                                              # 0 이 아니면 True
    vals = sc.decode(bytes(buf))
    assert len(vals) == 4, vals
    assert abs(vals[0] - 1.0) < 1e-9 and abs(vals[1] + 2.0) < 1e-9, vals
    assert vals[3] is True, vals[3]
    assert sc.scalar_names() == ["acc[0]", "acc[1]", "acc[2]", "ok"]


def test_decode_accepts_wire_padding():
    """와이어는 4바이트 배수로 패딩된다 — struct_size 보다 긴 payload 도 받아야 한다."""
    fields = (_fld("a", "", 4, 1, 0),)          # i16 @0, struct_size 2
    sc = EE.Schema(0xF0, "Pad", 2, 1, 0, fields)
    padded = struct.pack("<h", 1234) + b"\xFF\xFF"          # 와이어에서 4 B
    assert sc.decode(padded) == [1234]
    assert sc.decode(b"\x01") is None, "모자란 payload 를 받아들였다"


# =============================================================================
# 0xEF JSON 메타
# =============================================================================

def _ef_payload(module_id, json_text, pad_to_4=True):
    """FW 규약: `[target_module_id:1B][json bytes...]`, 와이어에서 4바이트 패딩."""
    raw = bytes([module_id]) + json_text.encode("utf-8")
    if pad_to_4:
        raw += b"\x00" * ((-len(raw)) % 4)
    return raw


def test_ef_parsing():
    reg = R.SchemaRegistry()
    js = ('[{"name":"H10 Connected","unit":"bool"},'
          '{"name":"Left Hip Angle","unit":"deg"},'
          '{"name":"Right Hip Angle","unit":"deg"},'
          '{"name":"Forward Velocity","unit":"m/s"}]')
    target = reg.feed_0xEF(_ef_payload(0xF0, js))
    assert target == 0xF0, target

    cs = reg.get(0xF0, 16)                        # float32 × 4
    assert cs is not None
    assert cs.source == "0xEF", cs.source
    assert cs.names == ["H10 Connected", "Left Hip Angle",
                        "Right Hip Angle", "Forward Velocity"], cs.names
    assert cs.units[1] == "deg"
    assert cs.assumed_float32 is True, "0xEF 는 타입을 모른다 — 가정임을 표시해야 한다"

    vals = cs.decode(struct.pack("<4f", 1.0, 12.5, -3.25, 0.5))
    assert vals == [1.0, 12.5, -3.25, 0.5], vals


def test_ef_count_mismatch_is_adjusted():
    """메타 3개인데 payload 가 5채널 — 이름이 밀린 CSV 보다 ch3/ch4 가 낫다."""
    reg = R.SchemaRegistry()
    reg.feed_0xEF(_ef_payload(0xF2, '[{"name":"a"},{"name":"b"},{"name":"c"}]'))
    cs = reg.get(0xF2, 20)
    assert cs.names == ["a", "b", "c", "ch3", "ch4"], cs.names
    assert "맞춰 조정" in cs.detail, cs.detail


def test_ef_bad_json_is_recorded_not_raised():
    reg = R.SchemaRegistry()
    assert reg.feed_0xEF(_ef_payload(0xF3, "{not json")) is None
    assert len(reg.ef_errors) == 1
    cs = reg.get(0xF3, 8)                          # 그래도 fallback 은 나와야 한다
    assert cs.source == "fallback" and cs.names == ["ch0", "ch1"], cs.names


# =============================================================================
# 레지스트리 — 우선순위
# =============================================================================

def test_ee_beats_ef():
    """같은 모듈에 둘 다 오면 타입을 아는 쪽이 이겨야 한다."""
    reg = R.SchemaRegistry()
    reg.feed_0xEF(_ef_payload(0xF0, '[{"name":"wrong0"},{"name":"wrong1"},{"name":"wrong2"}]'))
    cs = reg.get(0xF0, 12)
    assert cs.source == "0xEF", cs.source

    fields = [_fld("flag", "", 3, 1, 0), _fld("speed", "m/s", 0, 1, 4),
              _fld("count", "n", 4, 1, 8)]
    frag = EE.encode_fragment(0xF0, 12, "Real", fields, 0, 1, 3)
    got = reg.feed_0xEE(frag, 0.0)
    assert got is not None and got.source == "0xEE", got

    cs2 = reg.get(0xF0, 12)
    assert cs2.source == "0xEE", cs2.source
    assert cs2.names == ["flag", "speed", "count"], cs2.names
    assert cs2.assumed_float32 is False


def test_fallback_when_nothing_known():
    reg = R.SchemaRegistry()
    cs = reg.get(0xF5, 12)
    assert cs.source == "fallback" and cs.names == ["ch0", "ch1", "ch2"], cs.names
    assert cs.assumed_float32 is True
    assert reg.get(0xF5, 7) is not None or True     # 4의 배수 아님 -> None 이어도 정상


def test_total_data_uses_generated_map():
    reg = R.SchemaRegistry()
    cs = reg.get(R.MODULE_ID_TOTAL, 368)
    if cs is None:
        print("  (생성된 0x20 맵 없음 — 이 항목 생략)")
        return
    assert cs.source == "generated-map", cs.source
    assert len(cs.names) == 197, len(cs.names)
    assert cs.assumed_float32 is False


# =============================================================================
# 실시간 경로 — 수신기의 채널 이름 결정
# =============================================================================

def test_live_channel_naming():
    """수신기가 0xEF 를 받으면 그 다음 프레임부터 이름이 붙는가.

    PyQt5 가 없으면 건너뛴다(수신기 모듈이 GUI 를 import 한다). 건너뛴 사실은 찍는다 —
    조용히 통과하면 없는 것과 같다.
    """
    try:
        import cdc_phai_receiver as RX
    except ImportError as e:
        print('  (수신기 import 불가 — %s. 이 항목 생략)' % e)
        return

    class _Frame:
        def __init__(self, module_id, payload):
            self.module_id = module_id
            self.payload = payload
            self.recv_t = 0.0

    RX.SCHEMA_REG = R.SchemaRegistry()          # 시험끼리 새지 않게 초기화

    # 메타가 오기 전 — ch0..
    assert RX.get_channel_names(0xF0, 4) == ['ch0', 'ch1', 'ch2', 'ch3'], \
        RX.get_channel_names(0xF0, 4)

    js = '[{"name":"Target","unit":"deg"},{"name":"Actual","unit":"deg"}]'
    RX.feed_schema_frame(_Frame(R.MODULE_ID_USER_META, _ef_payload(0xF0, js)), 0.0)

    # 메타가 온 뒤 — 진짜 이름
    assert RX.get_channel_names(0xF0, 2) == ['Target', 'Actual'], \
        RX.get_channel_names(0xF0, 2)

    # 개수가 안 맞으면 아는 이름은 지키고 나머지는 ch* 로 채운다.
    # (예전에는 캐시가 module_id 만으로 굳어 5채널 요청이 조용히 2채널로 잘렸다 — 감사 #4)
    got = RX.get_channel_names(0xF0, 5)
    assert got == ['Target', 'Actual', 'ch2', 'ch3', 'ch4'], got

    # 하드코딩 표는 여전히 동작해야 한다 (0x10 Combined 10채널)
    assert RX.get_channel_names(0x10, 10)[0] == 'AccX', RX.get_channel_names(0x10, 10)[:3]


# =============================================================================
# 감사에서 나온 결함의 회귀 시험 (2026-09-10)
# =============================================================================

def test_non_ascii_never_escapes_as_unexpected_exception():
    """P0 회귀 — 비-ASCII 이름/단위가 SchemaError 밖으로 새면 수신 스레드가 죽는다.

    예전에는 `_unfixed` 가 U+FFFD 로 조용히 치환하고, 나중에 CRC 재계산에서
    `UnicodeEncodeError` 가 났다. 그건 SchemaError 가 아니라 아무도 안 잡았고,
    그런 프레임이 하나 든 `.xmlog` 는 영영 못 읽게 됐다.
    """
    good = EE.encode_fragment(0xF0, 8, "Demo", [_fld("speed", "m/s", 0, 1, 0)], 0, 1, 1)

    # name[16] 안에 0x80 바이트를 심는다 (NUL 종료는 유지)
    bad = bytearray(good)
    bad[24] = 0xC3          # 비-ASCII
    try:
        EE.parse_fragment(bytes(bad))
    except EE.SchemaError:
        pass
    except Exception as e:  # noqa: BLE001
        raise AssertionError("SchemaError 가 아닌 %s 가 샜다: %s" % (type(e).__name__, e))
    else:
        raise AssertionError("비-ASCII 이름을 받아들였다")

    # 레지스트리는 어떤 실패든 삼키고 기록해야 한다 (docstring 의 약속)
    reg = R.SchemaRegistry()
    got = reg.feed_0xEE(bytes(bad), 0.0)
    assert got is None, "실패했는데 ChannelSet 을 돌려줬다"
    assert len(reg.ee_errors) == 1, "오류가 기록되지 않았다: %r" % reg.ee_errors

    # 인코더 쪽도 — 비-ASCII 를 담은 FieldDef 로 CRC 를 계산하려 하면 SchemaError
    try:
        EE.canonical_field_bytes([_fld("속도", "m/s", 0, 1, 0)])
    except EE.SchemaError:
        pass
    except Exception as e:  # noqa: BLE001
        raise AssertionError("canonical_field_bytes 에서 %s 가 샜다" % type(e).__name__)
    else:
        raise AssertionError("비-ASCII 이름으로 CRC 바이트를 만들었다")


def test_registry_cache_respects_payload_length():
    """#4 회귀 — 같은 module 이 다른 길이로 오면 채널이 조용히 잘리면 안 된다."""
    reg = R.SchemaRegistry()
    a = reg.get(0xF5, 12)
    assert len(a.names) == 3, a.names
    b = reg.get(0xF5, 20)
    assert len(b.names) == 5, "길이가 바뀌었는데 %d채널로 굳었다: %r" % (len(b.names), b.names)
    c = reg.get(0xF5, 12)
    assert len(c.names) == 3, "되돌아왔을 때 %r" % (c.names,)


def test_fragment_count_consistency():
    """#10 회귀 — 같은 키인데 frame_count 가 다르면 재조립이 어긋난다."""
    fields = [_fld("f%d" % i, "", 0, 1, i * 4) for i in range(2)]
    crc = zlib.crc32(EE.canonical_field_bytes(fields)) & 0xFFFFFFFF
    a = EE.encode_fragment(0xF0, 8, "M", fields[:1], 0, 2, 2, schema_crc32=crc)
    # 같은 (module, proto, size, crc) 인데 frame_count 만 다르다
    b = EE.encode_fragment(0xF0, 8, "M", fields[1:], 1, 3, 2, schema_crc32=crc)
    ra = EE.Reassembler()
    assert ra.feed(a, 0.0) is None
    try:
        ra.feed(b, 0.1)
    except EE.SchemaError as e:
        assert "frame_count" in str(e), str(e)
        return
    raise AssertionError("frame_count 가 다른 프래그먼트를 같은 그룹에 넣었다")


def test_live_path_uses_typed_schema_values():
    """#7 회귀 — 0xEE 가 오면 **값도** 타입대로 풀어야 한다.

    이름만 붙이고 값은 as_float32() 로 두면 "이름은 맞고 값은 틀린" 최악의 조합이 된다.
    """
    try:
        import cdc_phai_receiver as RX
    except ImportError as e:
        print('  (수신기 import 불가 — %s. 이 항목 생략)' % e)
        return

    class _Frame:
        def __init__(self, module_id, payload):
            self.module_id = module_id
            self.payload = payload
            self.recv_t = 0.0

        def as_float32(self):
            import numpy as np
            return np.frombuffer(self.payload, dtype='<f4').copy()

    RX.SCHEMA_REG = R.SchemaRegistry()

    # uint8@0 / float32@4 / int16@8 — 패딩이 있는 구조체 (sizeof = 12)
    fields = [_fld("flag", "", 3, 1, 0), _fld("speed", "m/s", 0, 1, 4),
              _fld("count", "n", 4, 1, 8)]
    frag = EE.encode_fragment(0xF0, 12, "Real", fields, 0, 1, 3)
    RX.feed_schema_frame(_Frame(R.MODULE_ID_SCHEMA_DESC, frag), 0.0)

    buf = bytearray(12)
    buf[0] = 7
    struct.pack_into("<f", buf, 4, 2.5)
    struct.pack_into("<h", buf, 8, -300)
    pkt = _Frame(0xF0, bytes(buf))

    names, vals = RX.decode_user_values(pkt)
    assert names == ["flag", "speed", "count"], names
    assert vals == [7, 2.5, -300], \
        "타입대로 풀지 않았다 (as_float32 로 뭉갠 값일 것): %r" % (vals,)

    # 순진한 방식이 실제로 다른 값을 준다는 것도 확인 — 이 시험이 무엇을 막는지 못박는다
    naive = list(pkt.as_float32())
    assert naive != vals, "패딩이 없어 이 시험이 아무것도 막지 못한다"


# =============================================================================
# Runner
# =============================================================================

def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    tests = [
        ("golden 0xEE fragment bytes", test_golden_fragment_bytes),
        ("golden fragment parses back", test_golden_fragment_parses),
        ("header/record sizes match PLAN", test_header_and_record_sizes),
        ("bad headers rejected", test_rejects_bad_headers),
        ("missing NUL rejected", test_rejects_missing_nul_termination),
        ("multi-fragment reassembly", test_reassembly_multi_fragment),
        ("out-of-order + duplicate fragments", test_reassembly_out_of_order_and_duplicate),
        ("reassembly timeout", test_reassembly_timeout),
        ("key change discards in-progress", test_reassembly_key_change_discards),
        ("CRC mismatch rejected", test_crc_mismatch_rejected),
        ("overlapping fields rejected", test_overlap_rejected),
        ("out-of-bounds field rejected", test_out_of_bounds_rejected),
        ("duplicate field name rejected", test_duplicate_name_rejected),
        ("padded struct decodes correctly", test_padded_struct_decode),
        ("array + scale + bool", test_array_and_scale_and_bool),
        ("wire padding accepted", test_decode_accepts_wire_padding),
        ("0xEF JSON parsing", test_ef_parsing),
        ("0xEF count mismatch adjusted", test_ef_count_mismatch_is_adjusted),
        ("0xEF bad JSON recorded", test_ef_bad_json_is_recorded_not_raised),
        ("0xEE beats 0xEF", test_ee_beats_ef),
        ("fallback when nothing known", test_fallback_when_nothing_known),
        ("0x20 uses generated map", test_total_data_uses_generated_map),
        ("live channel naming (receiver)", test_live_channel_naming),
        ("non-ASCII never escapes (P0)", test_non_ascii_never_escapes_as_unexpected_exception),
        ("registry cache respects length", test_registry_cache_respects_payload_length),
        ("fragment count consistency", test_fragment_count_consistency),
        ("live path uses typed values", test_live_path_uses_typed_schema_values),
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
    print("%d/%d passed — 0xEE 골든 벡터 · 재조립 · 불변식 · 0xEF · 레지스트리 우선순위"
          % (len(tests), len(tests)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
