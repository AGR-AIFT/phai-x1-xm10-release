#!/usr/bin/env python3
"""`.xmlog` 스키마(activation)의 수명 회귀 시험 — 보드도 시리얼 포트도 없이 돈다.

    python test_xmlog_activation.py

무엇을 지키나
-------------
같은 module_id 가 한 파일 안에서 **다른 스키마**를 들고 올 수 있다(한 캡처를 재연결 뒤에도 이어
쓴 경우 등 — GUI · CLI 는 연결마다 새 파일을 열어서 보통은 만들지 않는다). 예전에는 캡처가 모든
DATA 를 `activation_id=0` 으로 적었고, 내보내기는 파일 끝에 남은 **최종 스키마 하나**로 전 구간을
풀었다. 그러면 스키마 A(float32)일 때 받은 `00 00 80 3f`(=1.0)가 스키마 B(uint32)로 풀려
`1065353216` 이 된다 — 이름은 맞고 값이 조용히 틀린다.

이 파일의 시험은 그 수명을 캡처(발급 · 참조) → 파일(SESSION 경계) → 내보내기(시점별 스키마)
전 구간에서 지킨다.

  (i)   필드가 같은 두 모듈        -> activation 2개, DATA 마다 자기 id
  (ii)  같은 모듈이 다시 연결해 같은 스키마를 보냄 -> activation 1개
  (iii) struct_name 만 다르고 CRC 는 같음 -> activation 2개
  (iv)  손상 파일: DATA.module_id 와 activation.module_id 가 다름 -> 0 으로 강등, 행 수 보존
  (v)   같은 module · 같은 크기 · 다른 type_tag 로 세션 도중 교체 -> 앞은 A 로, 뒤는 B 로
  장치를 식별할 수 없는 재연결 -> 새 SESSION, 유효하던 스키마 무효화, 그 뒤 스키마 전의
        DATA 는 activation_id=0 원시 바이트로 보존

그 밖에 이 파일이 지키는 것: 캡처가 적은 스키마는 읽는 쪽이 항상 되읽는다(프래그먼트마다 이름이 달라도) ·
번호(u16)가 바닥나도 저장이 이어지지만 디스크 오류는 삼키지 않는다 · CSV 파일 이름은 순서가 아니라 내용으로
정해진다 · 재연결(캡처)과 SESSION 경계(읽는 쪽)가 반쯤 받은 스키마를 버린다 · 재연결이 seq 기준을 비운다 ·
깨진 스키마 기록(못 나누는 activation payload · 깨진 0xEE 프레임)이 든 파일도 export 가 죽지 않는다.

오라클은 손으로 정한 값이다. 캡처가 쓴 파일을 읽어서 같으면 통과 — 는 시험이 아니다. 여기서는
"이 프레임을 흘리면 파일에 이 id 가 붙고, CSV 에 이 숫자가 나와야 한다" 를 미리 정해 두고 대조한다.
"""
import contextlib
import csv
import errno
import io
import os
import random
import shutil
import struct
import sys
import tempfile

import demo_stream as DS
import schema_0xee as EE
import xmlog as X
import xmlog_export as EXP
from frame_router import cobs_decode, parse_phai_frame
from xmlog_capture import FW_BUILD_ID_UNKNOWN, XmLogCapture, unique_path

F0, F1 = 0xF0, 0xF1
EE_MODULE = 0xEE


# =============================================================================
# 도구
# =============================================================================

def _fld(name, tag, off, alen=1, unit="", scale=1.0):
    return EE.FieldDef(name, unit, tag, alen, off, scale)


def _ee(module_id, fields, size, name="S"):
    """단일 프래그먼트 0xEE payload."""
    return EE.encode_fragment(module_id, size, name, list(fields), 0, 1, len(fields))


def _frame(seq, module_id, payload):
    """진짜 와이어(COBS · CRC16)를 만들어 파서를 통과시킨 프레임 — mock 이 아니다."""
    wire = DS.wire_frame(seq & 0xFFFF, module_id, payload)
    pkt, err = parse_phai_frame(cobs_decode(wire[:-1]), 0.0)
    assert err is None, err
    return pkt


class Feeder:
    """캡처에 프레임을 하나씩 흘린다. seq 는 0.. 로 이어지고 pc_time_us 는 1 ms 씩 간다."""

    def __init__(self, cap):
        self.cap, self.seq, self.us = cap, 0, 0

    def send(self, module_id, payload):
        self.us += 1000
        self.cap.on_frame(_frame(self.seq, module_id, payload), self.us)
        self.seq += 1


def _acts_of(res, mid):
    """파일 안 순서대로, 그 모듈 DATA 가 달고 있는 activation_id."""
    return [r.fields["activation_id"] for r in res.records
            if r.rec_type == X.REC_DATA and r.fields["module_id"] == mid]


def _of_type(res, rtype):
    return [r for r in res.records if r.rec_type == rtype]


def _read_csv(path):
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.reader(f))
    return rows[0], rows[1:]


def _export(res, out_dir, raw=False, stem="t"):
    """export_csv 를 돌리고 ({파일명: (머리글, 행들)}, 찍힌 글)을 돌려준다."""
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        written = EXP.export_csv(res, out_dir, stem, want_raw_hex=raw)
    return ({os.path.basename(p): _read_csv(p) for p in written}, buf.getvalue())


def _export_main(path, csv_dir=None):
    """`xm10 export` 가 타는 진입점(`xmlog_export.main`)을 돌리고 (종료코드, 찍힌 글)을 돌려준다.

    `_export` 는 `export_csv` 만 부른다. 요약(`build_registry`)과 시점별 해석(`resolve_rows`)을 지나는 길은
    이 진입점이라, 깨진 스키마 기록에서 export 가 죽는지는 여기로 봐야 한다.
    """
    buf, saved = io.StringIO(), sys.argv
    sys.argv = ["xmlog_export", path] + (["--csv", csv_dir] if csv_dir else [])
    try:
        with contextlib.redirect_stdout(buf):
            rc = EXP.main()
    finally:
        sys.argv = saved
    return rc, buf.getvalue()


def _values(rows, col):
    return [float(r[col]) for r in rows]


# 필드 둘: f32 @0, u16 @4 -> 뒤에 2 B 패딩, sizeof = 8
FIELDS_XY = (_fld("x", 0, 0), _fld("y", 5, 4))


def _xy(x, y):
    return struct.pack("<fHxx", x, y)


# =============================================================================
# 쓰기 쪽 — 재사용 판정의 키와 바이트 동등성
# =============================================================================

def test_writer_reuse_key_and_bytes(tmp):
    """재사용 = 키 (module, proto_ver, struct_size, crc) 가 같고 **바이트도 같을 때**만."""
    p = os.path.join(tmp, "w.xmlog")
    w = X.XmLogWriter(p, flush_every=1)
    w.session(fw_build_id="fw")
    a_bytes, b_bytes = b"A" * 56, b"B" * 56          # 키는 같고 바이트만 다르다 (struct_name 차이 흉내)
    key = (0xF0, 1, 8, 0x1234)

    assert w.schema_activation(*key, a_bytes) == 1
    assert w.schema_activation(*key, a_bytes) == 1, "같은 키 · 같은 바이트인데 새 id 를 발급했다"
    assert w.schema_activation(*key, b_bytes) == 2, "키가 같아도 바이트가 다르면 새 activation 이다"
    # A -> B -> A: 돌아온 A 는 처음 받은 id 를 다시 받는다 (B 가 A 를 덮어쓰지 않는다)
    assert w.schema_activation(*key, a_bytes) == 1
    assert w.schema_activation(*key, b_bytes) == 2

    # module_id, struct_size, proto_ver 가 각각 키에 들어 있다 — 바이트가 같아도 다른 activation
    assert w.schema_activation(0xF1, 1, 8, 0x1234, a_bytes) == 3, "module_id 가 키에 없다"
    assert w.schema_activation(0xF0, 1, 16, 0x1234, a_bytes) == 4, "struct_size 가 키에 없다"
    assert w.schema_activation(0xF0, 2, 8, 0x1234, a_bytes) == 5, "proto_ver 가 키에 없다"
    assert w.activation_count == 5
    w.close()

    res = X.read_file(p)
    assert len(_of_type(res, X.REC_SCHEMA_ACTIVATION)) == 5, "재사용은 레코드를 늘리지 않는다"


# =============================================================================
# fixture (i)~(iii) — 캡처가 activation 을 발급하고 DATA 가 참조한다
# =============================================================================

def test_fixture_i_two_modules_identical_fields(tmp):
    fa, fb = _ee(F0, FIELDS_XY, 8, "Twin"), _ee(F1, FIELDS_XY, 8, "Twin")
    assert EE.parse_fragment(fa).schema_crc32 == EE.parse_fragment(fb).schema_crc32, \
        "전제: 필드 구성이 같아 CRC 가 같다 — module_id 만 다르다"

    p = os.path.join(tmp, "i.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    f.send(F0, _xy(1.0, 1))                          # 스키마보다 먼저 온 데이터
    f.send(EE_MODULE, fa)
    f.send(EE_MODULE, fb)
    f.send(F0, _xy(2.0, 2)); f.send(F1, _xy(3.0, 3)); f.send(F0, _xy(4.0, 4)); f.send(F1, _xy(5.0, 5))
    cap.close()

    res = X.read_file(p)
    assert res.stopped_reason is None, res.stopped_reason
    acts = _of_type(res, X.REC_SCHEMA_ACTIVATION)
    assert len(acts) == 2, "activation %d개 (필드가 같아도 module 이 다르면 2개)" % len(acts)
    ids = {r.fields["module_id"]: r.fields["activation_id"] for r in acts}
    assert ids[F0] != ids[F1], ids
    assert acts[0].payload == fa and acts[1].payload == fb, \
        "SCHEMA_ACTIVATION payload 는 받은 0xEE 바이트 그대로여야 한다"

    assert _acts_of(res, F0) == [0, ids[F0], ids[F0]], _acts_of(res, F0)
    assert _acts_of(res, F1) == [ids[F1], ids[F1]], _acts_of(res, F1)
    n_data = len(_of_type(res, X.REC_DATA))
    assert n_data == f.seq, "DATA %d개 (프레임 %d개 — 0xEE 프레임도 DATA 로 남는다)" % (n_data, f.seq)

    resolved = EXP.resolve_rows(res.records)
    assert resolved.downgraded == 0 and not resolved.issues, resolved.issues
    out, _txt = _export(res, os.path.join(tmp, "csv"))
    h0, r0 = out["t_user_0xF0.csv"]
    h1, r1 = out["t_user_0xF1.csv"]
    assert h0[3:] == ["x", "y"] and h1[3:] == ["x", "y"], (h0, h1)
    assert _values(r0, 3) == [1.0, 2.0, 4.0], "0xF0 x = %r" % (_values(r0, 3),)
    assert _values(r1, 3) == [3.0, 5.0], "0xF1 x = %r" % (_values(r1, 3),)
    assert [r[2] for r in r0] == ["0", str(ids[F0]), str(ids[F0])]
    assert [r[2] for r in r1] == [str(ids[F1])] * 2


def test_fixture_ii_reconnect_identical_schema_is_one_activation(tmp):
    fa = _ee(F0, FIELDS_XY, 8, "Same")
    p = os.path.join(tmp, "ii.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    f.send(F0, _xy(0.5, 0))                          # 스키마 전
    f.send(EE_MODULE, fa)
    f.send(F0, _xy(1.0, 1))
    cap.on_reconnect()                               # 다시 연결 — 장치는 알 수 없다
    f.send(F0, _xy(2.0, 2))                          # 새 연결의 스키마 전 데이터
    f.send(EE_MODULE, fa)                            # 똑같은 스키마가 다시 온다
    f.send(F0, _xy(3.0, 3))
    cap.close()

    res = X.read_file(p)
    n_act = len(_of_type(res, X.REC_SCHEMA_ACTIVATION))
    assert n_act == 1, "같은 스키마가 다시 왔는데 activation 이 %d개다 (1개여야)" % n_act
    assert len(_of_type(res, X.REC_SESSION)) == 2
    assert EXP.resolve_rows(res.records).sessions == 2, "읽는 쪽이 SESSION 구간을 둘로 센다"
    # 스키마 전(0) -> 유효(1) -> [재연결] 무효라 0 -> 같은 스키마가 다시 오자 같은 id 1
    assert _acts_of(res, F0) == [0, 1, 0, 1], _acts_of(res, F0)

    # 내보내기: 재연결 뒤의 스키마 전 행도, **같은 구간 안에서** 스키마가 다시 도착하므로 그 스키마로 푼다
    out, _txt = _export(res, os.path.join(tmp, "csv"))
    assert sorted(out) == ["t_schema_0xEE.csv", "t_user_0xF0.csv"], sorted(out)
    header, rows = out["t_user_0xF0.csv"]
    assert header[3:] == ["x", "y"] and _values(rows, 3) == [0.5, 1.0, 2.0, 3.0], rows
    assert [r[2] for r in rows] == ["0", "1", "0", "1"], "activation_id 열은 파일에 적힌 그대로"


def test_fixture_iii_same_crc_different_struct_name(tmp):
    fa, fb = _ee(F0, FIELDS_XY, 8, "NameA"), _ee(F0, FIELDS_XY, 8, "NameB")
    assert EE.parse_fragment(fa).schema_crc32 == EE.parse_fragment(fb).schema_crc32 and fa != fb, \
        "전제: CRC 는 같고(FieldRecord 만 해싱) struct_name 때문에 바이트는 다르다"

    p = os.path.join(tmp, "iii.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    f.send(EE_MODULE, fa); f.send(F0, _xy(1.0, 1))
    f.send(EE_MODULE, fb); f.send(F0, _xy(2.0, 2))
    f.send(EE_MODULE, fa); f.send(F0, _xy(3.0, 3))   # A 로 되돌아옴 -> 처음 id 를 다시 받는다
    cap.close()

    res = X.read_file(p)
    acts = _of_type(res, X.REC_SCHEMA_ACTIVATION)
    assert len(acts) == 2, "activation %d개 (struct_name 이 다르면 CRC 가 같아도 2개)" % len(acts)
    assert acts[0].payload == fa and acts[1].payload == fb
    assert _acts_of(res, F0) == [1, 2, 1], _acts_of(res, F0)

    # 열 이름 · 타입이 같으니 한 파일이다 — 어느 activation 이었는지는 activation_id 열이 말한다
    out, _txt = _export(res, os.path.join(tmp, "csv"))
    assert sorted(out) == ["t_schema_0xEE.csv", "t_user_0xF0.csv"], sorted(out)
    _h, rows = out["t_user_0xF0.csv"]
    assert [r[2] for r in rows] == ["1", "2", "1"] and _values(rows, 3) == [1.0, 2.0, 3.0]


# =============================================================================
# fixture (iv) — 손상 파일: 참조가 어긋난 DATA 는 버리지 않고 0 으로 내린다
# =============================================================================

def test_fixture_iv_module_mismatch_is_downgraded_not_dropped(tmp):
    payload_a = _ee(F0, FIELDS_XY, 8, "Dmg")
    sc = EE.parse_fragment(payload_a)
    good = _xy(2.0, 2)
    odd = b"\x11\x22\x33\x44\x55\x66\x77\x88"        # 0xF1 이 activation 1(0xF0 것)을 잘못 참조한 행

    buf = bytearray(X.encode_file_header(0))
    buf += X.encode_session(fw_build_id="fw")
    buf += X.encode_schema_activation(1, F0, 1, 8, sc.schema_crc32, payload_a)
    buf += X.encode_data(1, F1, 10, 1000, odd)                    # module_id != activation.module_id
    buf += X.encode_data(1, F0, 11, 2000, good)                   # 정상
    buf += X.encode_data(9, F0, 12, 3000, good)                   # 아예 없는 activation 9
    res = X.scan(bytes(buf))
    assert res.stopped_reason is None, res.stopped_reason

    # 읽기 층(scan)은 파일에 적힌 번호를 그대로 돌려준다. 어긋난 참조를 0 으로 내리는 일은 스키마를
    # 고르는 resolve_rows 의 몫이다 — 이 경계가 문서(xmlog.scan · README)에 적힌 그대로인지 못박는다.
    raw_ids = [r.fields["activation_id"] for r in res.records if r.rec_type == X.REC_DATA]
    assert raw_ids == [1, 1, 9], "scan 이 번호를 손댔다: %r" % (raw_ids,)

    resolved = EXP.resolve_rows(res.records)
    rows = resolved.rows
    assert len(rows) == 3, "DATA 3개가 그대로 행 3개여야 한다 (%d)" % len(rows)
    assert resolved.downgraded == 2
    assert (rows[0].activation_id, rows[0].downgraded, rows[0].schema) == (0, True, None), rows[0]
    assert rows[0].record.payload == odd, "강등돼도 원본 바이트는 그대로다"
    assert (rows[1].activation_id, rows[1].downgraded) == (1, False) and rows[1].schema is not None
    assert (rows[2].activation_id, rows[2].downgraded, rows[2].schema) == (0, True, None), rows[2]
    assert rows[2].record.payload == good

    # 틀린 스키마로 풀지 않았다: 0xF1 의 행은 0xF0 의 x/y 열이 아니다
    out, txt = _export(res, os.path.join(tmp, "csv"))
    h1, r1 = out["t_user_0xF1.csv"]
    assert h1[3:] != ["x", "y"] and len(r1) == 1 and r1[0][2] == "0", (h1, r1)
    h0, r0 = out["t_user_0xF0.csv"]
    assert h0[3:] == ["x", "y"] and [r[2] for r in r0] == ["1"], (h0, r0)
    h0u, r0u = out["t_user_0xF0_unresolved.csv"]
    assert len(r0u) == 1 and r0u[0][2] == "0", "없는 activation 을 참조한 행도 버리지 않는다"
    assert "activation_id=0 으로 내린 행 2개" in txt, txt

    # 보존 확인: raw-hex 로 뽑으면 세 행의 원본 바이트가 그대로 나온다
    raw, _t = _export(res, os.path.join(tmp, "csv_raw"), raw=True)
    assert [r[-1] for r in raw["t_user_0xF1.csv"][1]] == [odd.hex()]
    assert [r[-1] for r in raw["t_user_0xF0.csv"][1]] == [good.hex(), good.hex()]
    assert [r[2] for r in raw["t_user_0xF0.csv"][1]] == ["1", "0"], "raw-hex 에서도 강등된 번호로 보인다"


# =============================================================================
# fixture (v) — 같은 module · 같은 크기 · 다른 type_tag 로 세션 도중 교체
# =============================================================================

F32_ONE = struct.pack("<f", 1.0)             # 00 00 80 3f
U32_SAME_BITS = struct.unpack("<I", F32_ONE)[0]      # 1065353216


def _swap_fields():
    """스키마 A: x 가 float32 / 스키마 B: x 가 uint32. module_id · 크기(4) · 이름이 모두 같다."""
    a = _ee(F0, [_fld("x", 0, 0)], 4, "Swap")
    b = _ee(F0, [_fld("x", 7, 0)], 4, "Swap")
    assert EE.parse_fragment(a).schema_crc32 != EE.parse_fragment(b).schema_crc32
    return a, b


PRE = struct.pack("<f", 0.5)                 # 스키마보다 먼저 온 데이터 (원본 바이트: 00 00 00 3f)


def _swap_stream(feed):
    """feed(module_id, payload) 로 흘릴 프레임 열. 캡처 경로와 예전 형식 파일이 같은 순서를 쓴다."""
    a, b = _swap_fields()
    feed(F0, PRE)                                   # 스키마 전 — 미상
    feed(EE_MODULE, a)
    feed(F0, F32_ONE); feed(F0, struct.pack("<f", 2.5))           # 스키마 A 구간
    feed(EE_MODULE, b)
    feed(F0, struct.pack("<I", 7)); feed(F0, struct.pack("<I", U32_SAME_BITS))   # 스키마 B 구간


def _check_swap_export(res, tmp, act_cols):
    out, txt = _export(res, os.path.join(tmp, "csv"))
    first = out["t_user_0xF0.csv"]
    second = out["t_user_0xF0_schema2.csv"]
    assert first[0][3:] == ["x"] and second[0][3:] == ["x"], (first[0], second[0])
    # 앞 구간은 A(float32)로: 1.0 이 1.0 이다 — 파일 끝의 최종 스키마(B)로 풀었다면 1065353216 이 된다
    assert _values(first[1], 3) == [0.5, 1.0, 2.5], \
        "교체 전 행이 A 로 안 풀렸다: %r" % (_values(first[1], 3),)
    # 뒤 구간은 B(uint32)로
    assert _values(second[1], 3) == [7.0, float(U32_SAME_BITS)], _values(second[1], 3)
    assert [r[2] for r in first[1]] + [r[2] for r in second[1]] == act_cols, act_cols
    # 바뀐 횟수가 아니라 쓰인 종류의 수를 말한다 — A -> B 한 번은 "2번 바뀜" 이 아니라 "2종류"
    assert "스키마가 2종류 쓰였다" in txt and "번 바뀌었다" not in txt, txt


def test_fixture_v_schema_swap_decodes_by_time(tmp):
    p = os.path.join(tmp, "v.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    _swap_stream(f.send)
    cap.close()

    res = X.read_file(p)
    assert res.stopped_reason is None
    assert len(_of_type(res, X.REC_SCHEMA_ACTIVATION)) == 2
    # 캡처가 붙인 번호: 스키마 전 0 · A 구간 1 · B 구간 2
    assert _acts_of(res, F0) == [0, 1, 1, 2, 2], _acts_of(res, F0)

    # 스키마 전의 미상 행은 activation_id=0 원시 바이트로 **그대로** 남았다 (파일 안에서 고쳐 쓰지 않는다)
    pre = [r for r in res.records if r.rec_type == X.REC_DATA and r.fields["module_id"] == F0][0]
    assert pre.fields["activation_id"] == 0 and pre.payload == PRE
    assert len([r for r in res.records if r.rec_type == X.REC_DATA
                and r.fields["module_id"] == F0]) == 5, "레코드 수가 보존돼야 한다"

    _check_swap_export(res, tmp, ["0", "1", "1", "2", "2"])

    # 읽는 쪽 결과를 직접: 행마다 고른 스키마가 그 시점의 것이다
    rows = [r for r in EXP.resolve_rows(res.records).rows if r.record.fields["module_id"] == F0]
    types = [r.schema.fields[0].type_name for r in rows]
    assert types == ["f32", "f32", "f32", "u32", "u32"], types

    # raw-hex: 해석 없이 원본 바이트가 activation_id 와 함께 그대로 나온다
    raw, _t = _export(res, os.path.join(tmp, "csv_raw"), raw=True)
    hdr, rrows = raw["t_user_0xF0.csv"]
    assert hdr[3:] == ["payload_hex"]
    assert [r[2] for r in rrows] == ["0", "1", "1", "2", "2"]
    assert [r[3] for r in rrows] == [PRE.hex(), F32_ONE.hex(), struct.pack("<f", 2.5).hex(),
                                     struct.pack("<I", 7).hex(), struct.pack("<I", U32_SAME_BITS).hex()]


def test_fixture_v_legacy_file_without_activation_records(tmp):
    """v2.8.0 캡처가 적은 파일 — 전부 activation_id=0, 스키마는 0xEE 프레임(DATA)으로만 있다."""
    p = os.path.join(tmp, "legacy.xmlog")
    w = X.XmLogWriter(p, flush_every=1)
    w.session(fw_build_id="fw-2.8")
    seq = [0]

    def feed(mid, payload):
        seq[0] += 1
        w.data(mid, seq[0], seq[0] * 1000, payload)          # activation_id 는 늘 0

    _swap_stream(feed)
    w.close()

    res = X.read_file(p)
    assert set(_acts_of(res, F0)) == {0} and not _of_type(res, X.REC_SCHEMA_ACTIVATION)
    _check_swap_export(res, tmp, ["0"] * 5)


def test_final_registry_would_misdecode_the_swap(tmp):
    """이 시험이 잡으려는 결함이 실제로 있었음을 확인 — 최종 registry 하나로 풀면 1.0 이 틀린다.

    `build_registry` 는 요약용이라 module_id 하나에 마지막 스키마만 남는다. 값을 푸는 데 쓰면
    A 구간의 `00 00 80 3f` 가 uint32 로 풀린다. 위 fixture (v) 가 통과한다는 것은 export 가 더는
    그렇게 풀지 않는다는 뜻이고, 이 시험은 그 대조군이 여전히 틀린 값을 낸다는 것을 못박는다
    (대조군이 안 틀리면 fixture (v) 는 아무것도 막지 못하는 시험이다).
    """
    p = os.path.join(tmp, "naive.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    _swap_stream(Feeder(cap).send)
    cap.close()
    res = X.read_file(p)

    cs = EXP.build_registry(res.records).get(F0, 4)
    assert cs.source == "0xEE" and cs.decode(F32_ONE) == [U32_SAME_BITS], \
        "대조군이 안 틀린다 — fixture (v) 의 전제가 사라졌다: %r" % (cs.decode(F32_ONE),)
    # 요약 쪽 registry 는 마지막 스키마 하나만 안다 — 값 디코딩에 쓰이지 않는다는 것을 export 가 보인다
    _check_swap_export(res, tmp, ["0", "1", "1", "2", "2"])


# =============================================================================
# 장치를 식별할 수 없는 재연결의 최소 계약
# =============================================================================

def test_dh_reconnect_ends_the_live_schema(tmp):
    a = _ee(F0, [_fld("x", 7, 0)], 4, "DevA")               # uint32
    p = os.path.join(tmp, "dh.xmlog")
    cap = XmLogCapture(p, device_usb_serial="SN-OLD", flush_every=1)
    f = Feeder(cap)
    f.send(EE_MODULE, a)
    f.send(F0, struct.pack("<I", 7))                        # 이 장치의 스키마가 유효한 동안
    cap.on_reconnect()                                      # 붙은 장치가 같은 장치인지 알 수 없다
    # 같은 COM 에 다른 장치가 붙어 같은 module · 같은 크기의 데이터를 **스키마보다 먼저** 보낸다
    other = struct.pack("<I", 0x3F800000)
    f.send(F0, other); f.send(F0, other)
    cap.close()

    res = X.read_file(p)
    sessions = _of_type(res, X.REC_SESSION)
    assert len(sessions) == 2 and cap.reconnects == 1
    s1, s2 = sessions[0].fields, sessions[1].fields
    assert (s1["link_epoch"], s2["link_epoch"]) == (1, 2), "link_epoch 는 호스트가 세는 순번(1부터)"
    assert s1["device_usb_serial"] == "SN-OLD"
    assert s2["device_usb_serial"] == "", "새로 붙은 장치의 serial 은 모른다 — 옛 값을 물려받으면 안 된다"
    assert s1["fw_build_id"] == s2["fw_build_id"] == FW_BUILD_ID_UNKNOWN == "unknown"
    assert s2["boot_epoch"] == 0

    # 무효화: 재연결 뒤 스키마 전 DATA 는 옛 activation(1)이 아니라 0
    assert _acts_of(res, F0) == [1, 0, 0], _acts_of(res, F0)
    after = [r for r in res.records if r.rec_type == X.REC_DATA and r.fields["module_id"] == F0][1:]
    assert [r.payload for r in after] == [other, other], "원시 바이트는 그대로 보존"
    assert len(_of_type(res, X.REC_SCHEMA_ACTIVATION)) == 1

    # 내보내기: 새 구간에는 스키마가 끝내 없으니 옛 스키마(uint32 x)로 풀리지 않는다
    rows = [r for r in EXP.resolve_rows(res.records).rows if r.record.fields["module_id"] == F0]
    assert rows[0].schema is not None and [r.schema for r in rows[1:]] == [None, None], \
        "재연결 뒤의 행이 앞 구간의 스키마로 풀렸다"
    out, _txt = _export(res, os.path.join(tmp, "csv"))
    assert sorted(out) == ["t_schema_0xEE.csv", "t_user_0xF0.csv", "t_user_0xF0_unresolved.csv"], sorted(out)
    hu, ru = out["t_user_0xF0_unresolved.csv"]
    assert hu[3:] != ["x"] and len(ru) == 2 and all(r[2] == "0" for r in ru), (hu, ru)


def test_dh_early_rows_never_cross_a_session_boundary(tmp):
    """스키마를 못 만난 채 구간이 끝난 행은 **다음 구간**에 온 스키마로 풀지 않는다."""
    a = _ee(F0, [_fld("x", 7, 0)], 4, "Later")
    p = os.path.join(tmp, "cross.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    old = struct.pack("<I", 111)
    f.send(F0, old)                                         # 구간 1: 스키마 없이 끝난다
    cap.on_reconnect()
    f.send(F0, struct.pack("<I", 222))                      # 구간 2: 스키마 전
    f.send(EE_MODULE, a)                                    # 구간 2 에서 스키마가 온다
    f.send(F0, struct.pack("<I", 333))
    cap.close()

    rows = [r for r in EXP.resolve_rows(X.read_file(p).records).rows
            if r.record.fields["module_id"] == F0]
    assert rows[0].schema is None, "구간 1 의 미상 행이 구간 2 의 스키마로 풀렸다 — 다른 장치일 수 있다"
    assert rows[1].schema is not None and rows[2].schema is not None, "같은 구간에서는 소급해서 푼다"
    assert rows[1].activation_id == 0 and rows[2].activation_id == 1


def test_dh_fw_build_id_is_explicit_and_epoch_starts_at_one(tmp):
    p = os.path.join(tmp, "unk.xmlog")
    cap = XmLogCapture(p, flush_every=1)                    # 아무것도 안 준다
    cap.close()
    s = X.read_file(p).records[0].fields
    assert s["fw_build_id"] == "unknown", "취득 경로가 없는 빌드 ID 를 0 으로 비워 두면 '모른다' 가 안 보인다"
    assert (s["link_epoch"], s["boot_epoch"]) == (1, 0)
    assert s["device_usb_serial"] == ""

    p2 = os.path.join(tmp, "given.xmlog")
    cap = XmLogCapture(p2, fw_build_id="fw-9.9", device_usb_serial="SN9", flush_every=1)
    cap.close()
    s2 = X.read_file(p2).records[0].fields
    assert (s2["fw_build_id"], s2["device_usb_serial"]) == ("fw-9.9", "SN9"), "준 값은 그대로"
    p3 = os.path.join(tmp, "blank.xmlog")
    XmLogCapture(p3, fw_build_id="", flush_every=1).close()
    assert X.read_file(p3).records[0].fields["fw_build_id"] == "unknown", "빈 문자열도 모른다는 뜻"


def test_flushed_boundaries_survive_a_process_kill(tmp):
    """내구성의 범위: SESSION · SCHEMA_ACTIVATION 은 쓰는 즉시, DATA 는 flush 뒤에 남는다.

    프로세스가 죽는 것을 흉내 낸다 — 쓰는 쪽(w)을 닫지 않은 채 **다른 핸들**로 읽는다. 닫지 않았으니
    파이썬 버퍼에 있는 것은 안 보이고, 파일에 보이는 것은 항상 앞에서부터 이어진 완전한 레코드다.
    """
    p = os.path.join(tmp, "kill.xmlog")
    w = X.XmLogWriter(p, flush_every=10_000)                # 배치 flush 를 사실상 끈다
    try:
        w.session(fw_build_id="fw")
        seen = X.read_file(p)
        assert [r.rec_type for r in seen.records] == [X.REC_SESSION] and seen.trailing_bytes == 0, \
            "SESSION 이 flush 되지 않았다 — 크래시에 파일이 통째로 사라진다"

        act = w.schema_activation(F0, 1, 8, 0x1234, b"\xAA" * 56)
        seen = X.read_file(p)
        assert [r.rec_type for r in seen.records] == [X.REC_SESSION, X.REC_SCHEMA_ACTIVATION], \
            "activation 이 flush 되지 않았다 — DATA 만 남고 그걸 풀 스키마가 사라질 수 있다"

        for i in range(5):
            w.data(F0, i, i, b"\x01\x00\x00\x00", activation_id=act)
        mid = X.read_file(p)                                # flush 전: 몇 개가 보이든 완전한 레코드의 앞부분
        assert mid.trailing_bytes == 0 and len(mid.records) <= 7

        w.flush()
        done = X.read_file(p)
        assert len(done.records) == 7 and done.trailing_bytes == 0 and done.stopped_reason is None, \
            "flush 를 마친 완전한 레코드가 안 보인다"
        assert [r.offset for r in mid.records] == [r.offset for r in done.records[:len(mid.records)]], \
            "중간에 본 것은 최종 파일의 앞부분(prefix)이어야 한다"
    finally:
        w.close()


def test_unique_path_never_overwrites(tmp):
    p = os.path.join(tmp, "cdc_20260101_000000.xmlog")
    assert unique_path(p) == p, "없는 파일은 이름 그대로"
    open(p, "wb").close()
    p2 = unique_path(p)
    assert p2 == os.path.join(tmp, "cdc_20260101_000000_2.xmlog"), p2
    open(p2, "wb").close()
    assert unique_path(p) == os.path.join(tmp, "cdc_20260101_000000_3.xmlog")


# =============================================================================
# 캡처가 깨진 스키마 · 여러 프래그먼트를 다루는 방식
# =============================================================================

def test_capture_survives_broken_schema_frames(tmp):
    p = os.path.join(tmp, "broken.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    f.send(EE_MODULE, b"\x01\x02\x03\x04")                  # 헤더보다 짧다
    good = _ee(F0, [_fld("x", 7, 0)], 4, "Ok")
    bad_crc = bytearray(good)
    bad_crc[4] ^= 0xFF                                       # 스키마 CRC 훼손
    f.send(EE_MODULE, bytes(bad_crc))
    f.send(F0, struct.pack("<I", 5))                        # 스키마가 없다 -> 0
    f.send(EE_MODULE, good)                                 # 이제 정상
    f.send(F0, struct.pack("<I", 6))
    cap.close()
    assert cap.schema_errors == 2, cap.schema_errors
    res = X.read_file(p)
    assert res.stopped_reason is None
    assert len(_of_type(res, X.REC_DATA)) == f.seq == 5, "깨진 스키마 프레임도 DATA 로 남는다"
    assert _acts_of(res, F0) == [0, 1]
    assert _acts_of(res, EE_MODULE) == [0, 0, 0]


def _uneven_schema(module_id, sizes, name="Uneven"):
    """필드를 `sizes` 개씩 나눈 프래그먼트 열 + 스키마 정보. 가득 채우지 않은 프래그먼트도 섞을 수 있다."""
    n = sum(sizes)
    fields = [_fld("f%d" % i, 0, i * 4) for i in range(n)]
    crc = EE.zlib.crc32(EE.canonical_field_bytes(fields)) & 0xFFFFFFFF
    frames, at = [], 0
    for idx, k in enumerate(sizes):
        frames.append(EE.encode_fragment(module_id, n * 4, name, fields[at:at + k],
                                         frame_index=idx, frame_count=len(sizes),
                                         field_count_total=n, schema_crc32=crc))
        at += k
    return frames, n


def test_multi_fragment_schema_is_one_activation(tmp):
    frames, n = _uneven_schema(F1, [31, 31, 8])
    p = os.path.join(tmp, "multi.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    for fr in frames:
        f.send(EE_MODULE, fr)
    payload = b"".join(struct.pack("<f", float(i)) for i in range(n))
    f.send(F1, payload)
    for fr in frames:                                       # 재방송 — 새 레코드 없이 재사용
        f.send(EE_MODULE, fr)
    f.send(F1, payload)
    cap.close()

    res = X.read_file(p)
    acts = _of_type(res, X.REC_SCHEMA_ACTIVATION)
    assert len(acts) == 1, "프래그먼트 3개가 activation 1개여야 한다 (%d)" % len(acts)
    assert acts[0].payload == b"".join(frames), "payload = 프래그먼트를 frame_index 순으로 이어붙인 것"
    assert _acts_of(res, F1) == [1, 1]

    sc = EE.schema_from_canonical(acts[0].payload)
    assert (sc.module_id, len(sc.fields), sc.struct_size) == (F1, 70, 280)
    out, _txt = _export(res, os.path.join(tmp, "csv"))
    hdr, rows = out["t_user_0xF1.csv"]
    assert hdr[3:] == ["f%d" % i for i in range(70)] and len(rows) == 2, "70열이 이름으로"
    assert _values(rows, 3 + 69) == [69.0, 69.0]


def test_split_canonical_handles_uneven_fragments(tmp):
    """프래그먼트 길이는 헤더에 없다 — 가득 차지 않은 것이 섞여도 헤더 경계로 나눠져야 한다."""
    for sizes in ([10, 25, 7], [31, 1, 1], [1, 31, 31], [5, 5], [31, 2]):
        frames, n = _uneven_schema(F1, sizes)
        parts = EE.split_canonical(b"".join(frames))
        assert parts == frames, "%r 를 나누지 못했다" % (sizes,)
        assert len(EE.schema_from_canonical(b"".join(frames)).fields) == n
    # 단일 프래그먼트는 그대로
    one = _ee(F0, FIELDS_XY, 8)
    assert EE.split_canonical(one) == [one]
    # 잘린 payload 는 나눌 수 없다
    frames, _n = _uneven_schema(F1, [31, 31, 8])
    try:
        EE.split_canonical(b"".join(frames)[:-32])
    except EE.SchemaError:
        pass
    else:
        raise AssertionError("FieldRecord 하나가 모자란 payload 를 나눴다")


def _golden_dir():
    here = os.path.dirname(os.path.abspath(__file__))
    for d in (os.path.normpath(os.path.join(here, "..", "spec", "golden")),
              os.path.join(here, "spec", "golden")):
        if os.path.exists(os.path.join(d, "0xEE_multi_fragment.hex")):
            return d
    return None


def test_frozen_golden_vectors_round_trip_through_activation(tmp):
    """동결된 골든 벡터(93필드 · 3프래그먼트)를 SCHEMA_ACTIVATION 으로 왕복 — 형식은 안 건드렸다."""
    d = _golden_dir()
    # 골든은 저장소가 추적하고 실행파일에도 실린다. 없다는 건 환경이 아니라 배포가 깨졌다는 뜻이라
    # 건너뛰지 않고 실패시킨다 — 건너뛰면 "18/18 통과" 에 검사하지 않은 항목이 섞인다.
    assert d is not None, ("골든 벡터를 못 찾았다 (spec/golden) — 배포에 빠졌거나 이 파일만 복사해 "
                           "왔다. 이 항목은 검사되지 않았다")
    with open(os.path.join(d, "0xEE_multi_fragment.hex"), encoding="utf-8") as f:
        blob = bytes.fromhex(f.read().strip())
    parts = EE.split_canonical(blob)
    assert [len(x) for x in parts] == [1016, 1016, 1016], [len(x) for x in parts]
    sc = EE.schema_from_canonical(blob)
    assert len(sc.fields) == 93

    # 골든 xmlog 파일: 파일 형식은 그대로 읽히고, 내보내기는 이 파일에서도 죽지 않고 행을 보존한다.
    # (이 파일의 SCHEMA_ACTIVATION payload 는 0xEE 가 아닌 더미 바이트라 스키마로는 못 푼다.)
    with open(os.path.join(d, "xmlog_v1_minimal_file.hex"), encoding="utf-8") as f:
        gold_bytes = bytes.fromhex(f.read().strip())
    gold = X.scan(gold_bytes)
    assert gold.stopped_reason is None and len(gold.records) == 5
    resolved = EXP.resolve_rows(gold.records)
    assert len(resolved.rows) == 2 and resolved.downgraded == 0
    assert resolved.rows[0].schema is None and resolved.issues, "더미 payload 는 문제로 기록돼야 한다"
    out, _txt = _export(gold, os.path.join(tmp, "csv"))
    assert sorted(out) == ["t_total_0x20.csv", "t_user_0xF0.csv"], sorted(out)
    assert all(len(rows) == 1 for _h, rows in out.values())
    # `xm10 export` 가 타는 길(요약 -> 시점별 해석 -> CSV)로도 같다 — 더미 payload 가 요약을 죽이지 않는다
    gold_path = os.path.join(tmp, "golden.xmlog")
    with open(gold_path, "wb") as f:
        f.write(gold_bytes)
    rc, text = _export_main(gold_path, os.path.join(tmp, "csv_main"))
    assert rc == 0 and "스키마 기록 문제" in text, (rc, text)
    assert sorted(os.listdir(os.path.join(tmp, "csv_main"))) == [
        "golden_total_0x20.csv", "golden_user_0xF0.csv"]


# =============================================================================
# 깨진 스키마 기록이 든 파일 — export 는 죽지 않고, 문제를 알리고, 행을 지킨다
# =============================================================================

def test_export_survives_unsplittable_activation_payload(tmp):
    """SCHEMA_ACTIVATION 의 payload 를 프래그먼트로 못 나누는 파일에서도 `xm10 export` 가 죽지 않는다.

    이 도구는 되읽히는 스키마에만 번호를 달지만(`test_capture_leaves_an_unreadable_schema_unnumbered`)
    다른 쪽이 쓴 파일은 그렇지 않다 — 동결된 골든 파일이 그 예다(payload 가 더미 4 B). 요약을 만드는
    `_feed_activation` 의 가드가 빠지면 SchemaError 로 죽는다.
    """
    frames, _n = _uneven_schema(F1, [31, 31, 8])
    cut = b"".join(frames)[:-32]                            # FieldRecord 하나가 모자란 3프래그먼트: 경계가 안 맞는다
    buf = bytearray(X.encode_file_header(0))
    buf += X.encode_session(fw_build_id="fw")
    buf += X.encode_schema_activation(1, F0, 1, 8, 0x1234, b"\xAA\xBB\xCC\xDD")       # 헤더보다 짧다
    buf += X.encode_schema_activation(2, F1, 1, 280, 0x5678, cut)
    buf += X.encode_data(1, F0, 1, 1000, _xy(1.0, 1))
    buf += X.encode_data(2, F1, 2, 2000, struct.pack("<4f", 1.0, 2.0, 3.0, 4.0))
    buf += X.encode_data(0, F0, 3, 3000, _xy(2.0, 2))
    p = os.path.join(tmp, "bad_act.xmlog")
    with open(p, "wb") as f:
        f.write(bytes(buf))
    res = X.read_file(p)
    assert res.stopped_reason is None and len(_of_type(res, X.REC_DATA)) == 3

    reg = EXP.build_registry(res.records)                   # 가드가 없으면 여기서 SchemaError
    assert len(reg.ee_errors) == 2, reg.ee_errors
    assert "헤더" in reg.ee_errors[0] and "나눌 수 없다" in reg.ee_errors[1], reg.ee_errors

    rc, text = _export_main(p, os.path.join(tmp, "csv"))
    assert rc == 0, text
    assert "0xEE 오류 2건" in text and "스키마 기록 문제 2건" in text, text
    # 행은 하나도 잃지 않는다 — 스키마로는 못 풀어도 원본 바이트를 float32 가정으로 CSV 에 남긴다
    csvs = sorted(os.listdir(os.path.join(tmp, "csv")))
    assert csvs == ["bad_act_user_0xF0.csv", "bad_act_user_0xF1.csv"], csvs
    assert len(_read_csv(os.path.join(tmp, "csv", csvs[0]))[1]) == 2
    assert len(_read_csv(os.path.join(tmp, "csv", csvs[1]))[1]) == 1


def test_export_survives_malformed_0xee_frames(tmp):
    """파일 안의 깨진 0xEE 프레임(짧은 것 · CRC 가 틀린 것)은 export 를 죽이지 않고 문제로만 남는다.

    이런 파일은 이 도구가 직접 쓴다 — 캡처는 CRC 를 통과한 프레임이면 무엇이든 DATA 로 적는다.
    프레임을 재조립하는 `resolve_rows` 의 가드가 빠지면 SchemaError 로 죽는다.
    """
    good = _ee(F0, [_fld("x", 7, 0)], 4, "Ok")
    bad_crc = bytearray(good)
    bad_crc[4] ^= 0xFF                                       # 스키마 CRC 훼손
    p = os.path.join(tmp, "bad_ee.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    f.send(EE_MODULE, b"\x01\x02\x03\x04")                  # 헤더보다 짧다
    f.send(F0, struct.pack("<I", 5))
    f.send(EE_MODULE, bytes(bad_crc))
    f.send(EE_MODULE, good)                                 # 온전한 스키마가 뒤늦게 온다
    f.send(F0, struct.pack("<I", 6))
    cap.close()
    res = X.read_file(p)

    rz = EXP.resolve_rows(res.records)                      # 가드가 없으면 여기서 SchemaError
    assert len(rz.rows) == f.seq == 5, "DATA 가 하나도 빠지지 않아야 한다"
    bad = [s for s in rz.issues if s.startswith("0xEE 프레임")]
    assert len(bad) == 2, rz.issues
    assert not any("예상 못한 예외" in s for s in bad), \
        "SchemaError 는 SchemaError 로 기록돼야 한다: %r" % (bad,)
    # 깨진 프레임이 뒤의 온전한 스키마까지 막지 않는다 — 스키마보다 먼저 온 행도 소급해 푼다
    assert [r.schema is not None for r in rz.rows if r.record.fields["module_id"] == F0] == [True, True]

    rc, text = _export_main(p, os.path.join(tmp, "csv"))
    assert rc == 0, text
    assert "스키마 기록 문제 2건" in text, text
    header, rows = _read_csv(os.path.join(tmp, "csv", "bad_ee_user_0xF0.csv"))
    assert header[3:] == ["x"] and _values(rows, 3) == [5.0, 6.0], (header, rows)


def test_export_survives_a_parser_bug_in_a_0xee_frame(tmp):
    """SchemaError 가 아닌 예외를 던지는 0xEE 프레임도 삼켜 기록한다 — 파서 버그 하나로 파일 전체를 못 읽으면 안 된다."""
    real = EE.parse_fragment

    def boom(payload):
        if bytes(payload) == b"BOOM":
            raise RuntimeError("파서 버그 흉내")
        return real(payload)

    buf = bytearray(X.encode_file_header(0))
    buf += X.encode_session(fw_build_id="fw")
    buf += X.encode_data(0, EE_MODULE, 1, 1000, b"BOOM")
    buf += X.encode_data(0, F0, 2, 2000, struct.pack("<I", 7))
    p = os.path.join(tmp, "boom.xmlog")
    with open(p, "wb") as f:
        f.write(bytes(buf))
    EE.parse_fragment = boom
    try:
        rz = EXP.resolve_rows(X.read_file(p).records)       # 가드가 없으면 여기서 RuntimeError
        rc, text = _export_main(p, os.path.join(tmp, "csv"))
    finally:
        EE.parse_fragment = real
    assert len(rz.rows) == 2, "DATA 가 하나도 빠지지 않아야 한다"
    assert any("예상 못한 예외 RuntimeError" in s for s in rz.issues), rz.issues
    assert rc == 0, text
    assert len(_read_csv(os.path.join(tmp, "csv", "boom_user_0xF0.csv"))[1]) == 1


# =============================================================================
# 캡처가 적은 스키마는 읽는 쪽이 되읽는다 — 이름이 흔들려도, 번호가 바닥나도
# =============================================================================

def _named_schema(module_id, names, per_frag, junk=()):
    """uint32 필드를 프래그먼트 `len(per_frag)` 개로 나눈 0xEE payload 열 + 필드 수.

    `names[i]` 는 i 번째 프래그먼트 헤더의 struct_name, `junk[i]` 는 그 이름의 NUL **뒤**(헤더 끝
    바이트들)에 심을 찌꺼기다. 재조립기는 이름을 비교하지 않아서 이런 스트림도 스키마 하나로 완성한다.
    """
    n = sum(per_frag)
    fields = [_fld("f%d" % i, 7, i * 4) for i in range(n)]
    crc = EE.zlib.crc32(EE.canonical_field_bytes(fields)) & 0xFFFFFFFF
    frames, at = [], 0
    for idx, k in enumerate(per_frag):
        fr = bytearray(EE.encode_fragment(module_id, n * 4, names[idx], fields[at:at + k],
                                          frame_index=idx, frame_count=len(per_frag),
                                          field_count_total=n, schema_crc32=crc))
        j = junk[idx] if idx < len(junk) else b""
        if j:
            fr[EE.HEADER_SIZE - len(j):EE.HEADER_SIZE] = j
        frames.append(bytes(fr))
        at += k
    return frames, n


def test_fragment_name_mismatch_still_reads_back(tmp):
    """프래그먼트마다 struct_name 이 다르거나 NUL 뒤에 찌꺼기가 있어도 — 화면(재조립기)에서 풀리는
    스키마는 파일에서도 되읽혀야 한다. 안 그러면 번호만 달린 채 float32 로 뭉개진 CSV 가 나온다."""
    payload = struct.pack("<40I", *range(40))
    cases = [
        ("이름이 다르다", ["NameA", "NameB"], ()),
        ("NUL 뒤 찌꺼기", ["Same", "Same"], (b"", b"\x11\x22\x33\x44\x55\x66\x77")),
        ("이름도 찌꺼기도", ["Same", "Other"], (b"\xAA\xBB", b"\xCC")),
    ]
    for k, (label, names, junk) in enumerate(cases):
        frames, n = _named_schema(F0, names, [20, 20], junk)
        assert n == 40 and len(payload) == 160
        p = os.path.join(tmp, "nm%d.xmlog" % k)
        cap = XmLogCapture(p, flush_every=1)
        f = Feeder(cap)
        for fr in frames:
            f.send(EE_MODULE, fr)
        f.send(F0, payload)
        cap.close()

        assert cap.schema_errors == 0, (label, cap.schema_errors)
        res = X.read_file(p)
        assert _acts_of(res, F0) == [1], (label, _acts_of(res, F0))
        rz = EXP.resolve_rows(res.records)
        assert not rz.issues, (label, rz.issues)
        row = [r for r in rz.rows if r.record.fields["module_id"] == F0][0]
        assert row.schema is not None and len(row.schema.fields) == 40, label
        out, _txt = _export(res, os.path.join(tmp, "csv%d" % k))
        hdr, rows = out["t_user_0xF0.csv"]
        assert hdr[3:] == ["f%d" % i for i in range(40)], (label, hdr[:6])
        assert _values(rows, 3) == [0.0] and _values(rows, 3 + 39) == [39.0], label
        assert rows[0][2] == "1", label


def test_reassembled_schema_always_reads_back(tmp):
    """재조립기가 완성한 스키마는 그때 남긴 `last_canonical` 로 **언제나** 되읽힌다 — 이름이 흔들리든,
    와이어 꼬리가 붙든, 조각이 뒤섞이거나 겹쳐 오든. 캡처는 이 바이트를 그대로 SCHEMA_ACTIVATION 에 적는다."""
    rng = random.Random(20260930)
    shaky = 0
    for it in range(300):
        per = [rng.randint(1, 31) for _ in range(rng.randint(1, 3))]
        names = [rng.choice(["Same", "Other", "X"]) for _ in per]
        junk = [rng.choice([b"", b"\xAA\xBB\xCC\xDD"]) for _ in per]
        frames, _n = _named_schema(F0, names, per, junk)
        frames = [fr + b"\x00" * rng.choice([0, 0, 4, 8]) for fr in frames]      # 와이어가 붙인 꼬리
        order = list(range(len(per)))
        rng.shuffle(order)
        if rng.random() < 0.3:
            order.insert(rng.randrange(len(order)), rng.choice(order))          # 같은 조각이 또 온다
        ra, done = EE.Reassembler(), None
        for i in order:
            done = ra.feed(frames[i], 0.0) or done
        assert done is not None, (it, per, order)
        back = EE.schema_from_canonical(ra.last_canonical)
        assert (back.fields, back.struct_size, back.schema_crc32) == (
            done.fields, done.struct_size, done.schema_crc32), (it, per, names, junk)
        if len(per) > 1 and (len(set(names)) > 1 or any(junk)):
            shaky += 1
    assert shaky > 50, "이름이 흔들린 스트림이 %d개뿐이다 — 이 시험이 아무것도 막지 못한다" % shaky


def test_capture_leaves_an_unreadable_schema_unnumbered(tmp):
    """되읽을 수 없는 스키마에는 번호를 달지 않는다 — 번호가 달린 DATA 는 읽는 쪽이 그 번호의 스키마로
    푸는데, 그 스키마가 안 읽히면 float32 로 뭉개진다. 번호 없이(0) 남기면 프레임에서 다시 만든다."""
    real = EE.schema_from_canonical

    def cannot(_payload):
        raise EE.SchemaError("되읽기 실패 흉내")

    p = os.path.join(tmp, "unread.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    f.send(EE_MODULE, _ee(F0, [_fld("x", 7, 0)], 4, "Good"))                     # 번호 1
    f.send(F0, struct.pack("<I", 1))
    EE.schema_from_canonical = cannot
    try:
        f.send(EE_MODULE, _ee(F0, [_fld("x", 5, 0)], 4, "Bad"))                  # 되읽기 실패
    finally:
        EE.schema_from_canonical = real
    f.send(F0, struct.pack("<HH", 2, 0))
    cap.close()

    assert cap.schema_errors == 1, cap.schema_errors
    res = X.read_file(p)
    assert res.stopped_reason is None
    assert len(_of_type(res, X.REC_SCHEMA_ACTIVATION)) == 1, "되읽을 수 없는 스키마의 레코드를 적었다"
    assert _acts_of(res, F0) == [1, 0], "옛 번호(1)를 새 스키마의 DATA 에 그대로 달았다: %r" % (_acts_of(res, F0),)
    # 번호는 없어도 0xEE 프레임이 파일에 있으니 읽는 쪽은 새 스키마(u16)로 푼다
    rows = [r for r in EXP.resolve_rows(res.records).rows if r.record.fields["module_id"] == F0]
    assert [r.schema.fields[0].type_name for r in rows] == ["u32", "u16"], \
        [None if r.schema is None else r.schema.fields[0].type_name for r in rows]


def test_writer_activation_ids_run_out_loudly(tmp):
    p = os.path.join(tmp, "full.xmlog")
    w = X.XmLogWriter(p, flush_every=1)
    w.session(fw_build_id="fw")
    w._next_activation_id = 0xFFFF            # 65534개를 실제로 발급하는 대신 마지막 번호 앞으로 건너뛴다
    a, b = b"A" * 56, b"B" * 56
    assert w.schema_activation(F0, 1, 8, 0x1111, a) == 0xFFFF, "마지막 번호(65535)는 발급돼야 한다"
    try:
        w.schema_activation(F0, 1, 8, 0x2222, b)
    except ValueError:
        pass
    else:
        raise AssertionError("번호가 바닥났는데 새 스키마가 번호를 받았다 (0 으로 되감기면 '미상' 과 겹친다)")
    assert w.schema_activation(F0, 1, 8, 0x1111, a) == 0xFFFF, "이미 가진 스키마는 바닥난 뒤에도 재사용된다"
    assert w.activation_count == 0xFFFF
    w.close()
    res = X.read_file(p)
    assert len(_of_type(res, X.REC_SCHEMA_ACTIVATION)) == 1 and res.stopped_reason is None


def test_capture_keeps_saving_when_activation_ids_run_out(tmp):
    """번호가 바닥나도 저장은 이어진다 — 예외가 새면 GUI 는 저장을 멈추고 CLI 는 죽는다."""
    last = _ee(F0, FIELDS_XY, 8, "Last")
    over = _ee(F0, FIELDS_XY, 8, "Over")      # CRC 는 같고 struct_name 만 달라 바이트가 다르다 -> 새 번호가 필요하다
    p = os.path.join(tmp, "runout.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    cap.w._next_activation_id = 0xFFFF        # 번호를 65534개 쓴 것으로 친다
    f = Feeder(cap)
    f.send(EE_MODULE, last); f.send(F0, _xy(1.0, 1))      # 마지막 번호 65535
    f.send(EE_MODULE, over); f.send(F0, _xy(2.0, 2))      # 번호가 없다 -> 0, 예외는 안 새어 나온다
    f.send(EE_MODULE, last); f.send(F0, _xy(3.0, 3))      # 이미 가진 스키마는 계속 번호를 받는다
    cap.close()

    assert cap.schema_errors == 1, cap.schema_errors
    res = X.read_file(p)
    assert res.stopped_reason is None and len(_of_type(res, X.REC_SCHEMA_ACTIVATION)) == 1
    assert _acts_of(res, F0) == [0xFFFF, 0, 0xFFFF], _acts_of(res, F0)
    # 번호를 못 받은 행도 0xEE 프레임이 있으니 그 스키마로 풀린다 — float32 로 뭉개지지 않는다
    out, _txt = _export(res, os.path.join(tmp, "csv"))
    hdr, rows = out["t_user_0xF0.csv"]
    assert hdr[3:] == ["x", "y"] and _values(rows, 3) == [1.0, 2.0, 3.0], (hdr, rows)


class _FullDisk:
    """진짜 파일을 감싼 껍데기 — `full` 을 켜면 flush 가 ENOSPC(디스크가 찼다)로 실패한다."""

    def __init__(self, real):
        self._real, self.full = real, False

    def write(self, data):
        return self._real.write(data)

    def flush(self):
        if self.full:
            raise OSError(errno.ENOSPC, "No space left on device")
        return self._real.flush()

    def close(self):
        return self._real.close()

    @property
    def closed(self):
        return self._real.closed


def test_disk_error_while_numbering_a_schema_propagates(tmp):
    """번호가 바닥난 ValueError 만 삼키고, 디스크 오류(OSError)는 그대로 올라간다.

    삼키면 저장이 깨졌는데도 수신은 이어지고 사용자는 파일이 온전한 줄 안다. 올라가야 GUI 워커가
    "저장 중단" 을 알리고 CLI 가 멈춘다. 짝이 되는 시험: `test_capture_keeps_saving_when_activation_ids_run_out`.
    """
    p = os.path.join(tmp, "full.xmlog")
    cap = XmLogCapture(p, flush_every=10_000)     # DATA 마다는 flush 하지 않는다 — 실패가 activation 을 적는 자리에서 난다
    disk = _FullDisk(cap.w._f)
    cap.w._f = disk
    f = Feeder(cap)
    f.send(F0, _xy(1.0, 1))                       # 디스크가 멀쩡할 때는 아무 일 없다
    disk.full = True
    try:
        f.send(EE_MODULE, _ee(F0, FIELDS_XY, 8, "Full"))    # 스키마가 완성돼 activation 을 적고 flush 하는 순간
    except OSError as e:
        assert e.errno == errno.ENOSPC, e
    else:
        raise AssertionError("디스크 오류가 삼켜졌다 — 저장이 깨졌는데 호출한 쪽이 모른다")
    assert cap.schema_errors == 0, "디스크 오류를 스키마 문제로 세면 안 된다: %d" % cap.schema_errors
    disk.full = False
    cap.close()


# =============================================================================
# CSV 파일 이름은 순서가 아니라 내용으로 — 못 푼 행이 기본 이름을 차지하지 않는다
# =============================================================================

def test_unresolved_rows_never_take_the_plain_name(tmp):
    a = _ee(F0, [_fld("x", 0, 0)], 4, "A")        # float32
    b = _ee(F0, [_fld("x", 7, 0)], 4, "B")        # uint32
    users = lambda out: sorted(k for k in out if "user" in k)     # noqa: E731

    # (1) 못 푼 행이 먼저 나오고, 스키마는 다음 구간에서야 온다
    p1 = os.path.join(tmp, "n1.xmlog")
    cap = XmLogCapture(p1, flush_every=1)
    f = Feeder(cap)
    f.send(F0, struct.pack("<I", 111))                    # 구간 1: 스키마 없이 끝난다
    cap.on_reconnect()
    f.send(EE_MODULE, b); f.send(F0, struct.pack("<I", 222))
    cap.close()
    out, txt = _export(X.read_file(p1), os.path.join(tmp, "c1"))
    assert users(out) == ["t_user_0xF0.csv", "t_user_0xF0_unresolved.csv"], users(out)
    assert txt.index("t_user_0xF0.csv") < txt.index("t_user_0xF0_unresolved.csv"), \
        "요약은 풀린 파일을 먼저, 못 푼 행을 나중에 보여 준다:\n" + txt
    h, r = out["t_user_0xF0.csv"]
    assert h[3:] == ["x"] and _values(r, 3) == [222.0], "기본 이름은 스키마로 푼 파일이어야 한다: %r %r" % (h, r)
    hu, ru = out["t_user_0xF0_unresolved.csv"]
    assert hu[3:] != ["x"] and len(ru) == 1 and ru[0][2] == "0"

    # (2) A -> 못 푼 구간 -> B: _schema2 가 있다 (예전에는 _schema3 만 있었다)
    p2 = os.path.join(tmp, "n2.xmlog")
    cap = XmLogCapture(p2, flush_every=1)
    f = Feeder(cap)
    f.send(EE_MODULE, a); f.send(F0, struct.pack("<f", 1.0))
    cap.on_reconnect()
    f.send(F0, struct.pack("<I", 9))                      # 구간 2: 스키마 없이 끝난다
    f.send(F1, struct.pack("<I", 5))                      # 스키마가 아예 없는 모듈
    cap.on_reconnect()
    f.send(EE_MODULE, b); f.send(F0, struct.pack("<I", 7))
    cap.close()
    out, txt = _export(X.read_file(p2), os.path.join(tmp, "c2"))
    assert users(out) == ["t_user_0xF0.csv", "t_user_0xF0_schema2.csv", "t_user_0xF0_unresolved.csv",
                          "t_user_0xF1.csv"], users(out)
    assert _values(out["t_user_0xF0.csv"][1], 3) == [1.0]
    assert _values(out["t_user_0xF0_schema2.csv"][1], 3) == [7.0]
    assert len(out["t_user_0xF0_unresolved.csv"][1]) == 1
    assert len(out["t_user_0xF1.csv"][1]) == 1, "스키마가 없는 모듈은 파일 하나, 기본 이름"
    assert "스키마가 2종류 쓰였다" in txt, txt


# =============================================================================
# 재연결과 재조립 — 살아남은 뮤턴트를 못박는 작은 시험들
# =============================================================================

def test_reconnect_drops_a_half_received_schema(tmp):
    frames, n = _uneven_schema(F1, [10, 10])
    payload = struct.pack("<%df" % n, *range(n))
    p = os.path.join(tmp, "half.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    f.send(EE_MODULE, frames[0])                          # 스키마의 앞 절반만 받고 연결이 끊긴다
    cap.on_reconnect()
    f.send(EE_MODULE, frames[1])                          # 다시 붙은 (다른 장치일 수 있는) 쪽이 보낸 뒤 절반
    f.send(F1, payload)
    assert cap.w.activation_count == 0 and cap.schema_errors == 0, \
        "끊기기 전의 조각이 새 연결의 조각과 붙어 스키마가 완성됐다"
    f.send(EE_MODULE, frames[0])                          # 이제 앞 절반이 오면 온전한 스키마가 된다
    f.send(F1, payload)
    cap.close()
    res = X.read_file(p)
    assert _acts_of(res, F1) == [0, 1], _acts_of(res, F1)
    assert len(_of_type(res, X.REC_SCHEMA_ACTIVATION)) == 1


def test_session_boundary_drops_a_half_received_schema_on_read(tmp):
    """읽는 쪽도 SESSION 경계에서 반쯤 받은 스키마를 버린다 — 앞 연결의 조각과 뒤 연결의 조각이 붙어
    스키마가 되면 안 된다(다른 장치일 수 있다). 캡처 쪽 짝: `test_reconnect_drops_a_half_received_schema`."""
    frames, n = _uneven_schema(F1, [10, 10])
    payload = struct.pack("<%df" % n, *range(n))

    def read(boundary):
        buf = bytearray(X.encode_file_header(0))
        buf += X.encode_session(fw_build_id="fw", link_epoch=1)
        buf += X.encode_data(0, EE_MODULE, 1, 1000, frames[0])           # 스키마의 앞 절반
        if boundary:
            buf += X.encode_session(fw_build_id="unknown", link_epoch=2)  # 다시 연결
        buf += X.encode_data(0, EE_MODULE, 2, 2000, frames[1])           # 뒤 절반
        buf += X.encode_data(0, F1, 3, 3000, payload)
        res = X.scan(bytes(buf))
        assert res.stopped_reason is None, res.stopped_reason
        return EXP.resolve_rows(res.records)

    same = read(boundary=False)                   # 대조군: 한 연결 안이면 두 조각이 스키마 하나로 완성된다
    assert same.rows[-1].schema is not None and len(same.rows[-1].schema.fields) == n, \
        "전제가 깨졌다 — 두 조각이 한 연결 안에서도 스키마가 안 된다"
    split = read(boundary=True)
    assert split.sessions == 2 and len(split.rows) == 3, (split.sessions, len(split.rows))
    assert split.rows[-1].schema is None and split.rows[-1].activation_id == 0, \
        "SESSION 경계를 넘어 조각이 붙어 스키마가 됐다"


def test_reconnect_starts_a_new_seq_baseline(tmp):
    def run(name, reconnect):
        p = os.path.join(tmp, name)
        cap = XmLogCapture(p, flush_every=1)
        for seq in (0, 1, 2):
            cap.on_frame(_frame(seq, F0, _xy(1.0, 1)), seq * 1000)
        if reconnect:
            cap.on_reconnect()
        for seq in (5000, 5001):
            cap.on_frame(_frame(seq, F0, _xy(1.0, 1)), seq * 1000)
        cap.close()
        return cap, X.read_file(p)

    cap, res = run("seq_reconnect.xmlog", reconnect=True)
    assert cap.gaps == 0 and not _of_type(res, X.REC_GAP), \
        "끊긴 동안 몇 개를 놓쳤는지는 알 수 없다 — 옛 seq 기준으로 4997개 손실을 지어내면 안 된다"
    cap, res = run("seq_plain.xmlog", reconnect=False)          # 대조군: 재연결이 아니면 그 갭은 손실이다
    gaps = _of_type(res, X.REC_GAP)
    assert cap.gaps == 1 and len(gaps) == 1 and gaps[0].fields["lost_count"] == 4997, gaps


def test_wire_padding_tail_does_not_make_a_second_activation(tmp):
    """와이어가 프래그먼트 뒤에 붙인 꼬리는 스키마 바이트가 아니다 — 꼬리 유무로 activation 이 갈리면 안 된다."""
    frames, n = _uneven_schema(F1, [10, 10])
    payload = struct.pack("<%df" % n, *range(n))
    p = os.path.join(tmp, "tail.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    for fr in frames:
        f.send(EE_MODULE, fr + b"\x00\x00\x00\x00")           # 꼬리가 붙어 온 스키마
    f.send(F1, payload)
    for fr in frames:
        f.send(EE_MODULE, fr)                                 # 같은 스키마, 꼬리 없이
    f.send(F1, payload)
    cap.close()
    res = X.read_file(p)
    acts = _of_type(res, X.REC_SCHEMA_ACTIVATION)
    assert cap.schema_errors == 0 and _acts_of(res, F1) == [1, 1], \
        "꼬리가 붙어 온 스키마에 번호가 안 달렸다: 오류 %d, 번호 %r" % (cap.schema_errors, _acts_of(res, F1))
    assert len(acts) == 1, "activation %d개 — 꼬리가 바이트 동등성 판정에 섞였다" % len(acts)
    assert acts[0].payload == b"".join(frames), "SCHEMA_ACTIVATION 에 와이어 꼬리가 딸려 들어갔다"


def test_reader_reports_broken_activation_records(tmp):
    good = _ee(F0, FIELDS_XY, 8, "GoodA")
    other = _ee(F0, [_fld("x", 7, 0)], 8, "GoodB")
    sa, sb = EE.parse_fragment(good), EE.parse_fragment(other)

    buf = bytearray(X.encode_file_header(0))
    buf += X.encode_session(fw_build_id="fw")
    buf += X.encode_schema_activation(1, F0, 1, 8, sa.schema_crc32, good)
    buf += X.encode_schema_activation(1, F0, 1, 8, sb.schema_crc32, other)          # 같은 번호를 또 쓴다
    buf += X.encode_data(1, F0, 1, 1000, _xy(2.0, 2))
    # 레코드 헤더의 CRC 가 payload 안의 스키마와 다르다
    buf += X.encode_schema_activation(2, F0, 1, 8, sa.schema_crc32 ^ 1, good)
    buf += X.encode_data(2, F0, 2, 2000, _xy(3.0, 3))
    res = X.scan(bytes(buf))
    assert res.stopped_reason is None, res.stopped_reason

    rz = EXP.resolve_rows(res.records)
    assert any("두 번 나온다" in s for s in rz.issues), rz.issues
    assert [f.name for f in rz.rows[0].schema.fields] == ["x", "y"], "번호가 겹치면 앞의 것을 쓴다"
    assert any("레코드 헤더와 payload 의 스키마가 다르다" in s for s in rz.issues), rz.issues
    assert rz.rows[1].schema is None and rz.rows[1].activation_id == 2, \
        "헤더와 payload 가 어긋난 activation 으로 풀었다"


def test_summary_registry_counts_a_schema_once(tmp):
    """activation 레코드와 그 스키마를 담은 0xEE 프레임을 둘 다 먹이면 같은 스키마가 두 번 완성된 것으로 센다."""
    p = os.path.join(tmp, "cnt.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    _swap_stream(Feeder(cap).send)
    cap.close()
    reg = EXP.build_registry(X.read_file(p).records)
    assert reg.reasm.completed == 2, "스키마 2종류인데 %d번 완성됐다: %s" % (reg.reasm.completed, reg.summary())

    # activation 레코드가 없는 예전 파일에서는 프레임이 유일한 출처다
    legacy = os.path.join(tmp, "cnt_legacy.xmlog")
    w = X.XmLogWriter(legacy, flush_every=1)
    w.session(fw_build_id="fw-2.8")
    seq = [0]

    def feed(mid, payload):
        seq[0] += 1
        w.data(mid, seq[0], seq[0] * 1000, payload)

    _swap_stream(feed)
    w.close()
    reg = EXP.build_registry(X.read_file(legacy).records)
    assert reg.reasm.completed == 2, reg.summary()


def test_export_cli_reports_swaps_and_reconnects(tmp):
    """`xm10 export` 의 요약이 스키마 교체 · 재연결을 숨기지 않는다."""
    p = os.path.join(tmp, "cli.xmlog")
    cap = XmLogCapture(p, flush_every=1)
    f = Feeder(cap)
    _swap_stream(f.send)
    cap.on_reconnect()
    f.send(F0, struct.pack("<I", 1))
    cap.close()

    buf, saved = io.StringIO(), sys.argv
    sys.argv = ["xmlog_export", p, "--dump", "40", "--csv", os.path.join(tmp, "csv")]
    try:
        with contextlib.redirect_stdout(buf):
            rc = EXP.main()
    finally:
        sys.argv = saved
    text = buf.getvalue()
    assert rc == 0, text
    assert "session #2 (다시 연결): link_epoch=2" in text, text
    assert "스키마가 2종류 쓰였다" in text and "번 바뀌었다" not in text, text
    assert "SCHEMA_ACTIVATION" in text and "link_epoch=1" in text
    assert "fw='unknown'" in text, "펌웨어 빌드는 모른다고 적는다"


# =============================================================================
# Runner
# =============================================================================

def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    tests = [
        ("writer: reuse = key + byte equality", test_writer_reuse_key_and_bytes),
        ("fixture (i)   two modules, identical fields", test_fixture_i_two_modules_identical_fields),
        ("fixture (ii)  reconnect, identical schema",
         test_fixture_ii_reconnect_identical_schema_is_one_activation),
        ("fixture (iii) same CRC, different struct_name", test_fixture_iii_same_crc_different_struct_name),
        ("fixture (iv)  damaged reference is downgraded",
         test_fixture_iv_module_mismatch_is_downgraded_not_dropped),
        ("fixture (v)   schema swap decodes by time", test_fixture_v_schema_swap_decodes_by_time),
        ("fixture (v)   legacy file (no activation records)",
         test_fixture_v_legacy_file_without_activation_records),
        ("fixture (v)   control: final registry misdecodes", test_final_registry_would_misdecode_the_swap),
        ("unidentified reconnect ends the live schema", test_dh_reconnect_ends_the_live_schema),
        ("unidentified reconnect: early rows stay inside their session",
         test_dh_early_rows_never_cross_a_session_boundary),
        ("unidentified reconnect: fw_build_id explicit, epoch from 1",
         test_dh_fw_build_id_is_explicit_and_epoch_starts_at_one),
        ("unidentified reconnect durability: flushed boundaries",
         test_flushed_boundaries_survive_a_process_kill),
        ("unique_path never overwrites", test_unique_path_never_overwrites),
        ("capture survives broken schema frames", test_capture_survives_broken_schema_frames),
        ("multi-fragment schema = one activation", test_multi_fragment_schema_is_one_activation),
        ("split_canonical: uneven fragments", test_split_canonical_handles_uneven_fragments),
        ("frozen golden vectors through activation",
         test_frozen_golden_vectors_round_trip_through_activation),
        ("export survives an unsplittable activation payload",
         test_export_survives_unsplittable_activation_payload),
        ("export survives malformed 0xEE frames", test_export_survives_malformed_0xee_frames),
        ("export survives a parser bug in a 0xEE frame", test_export_survives_a_parser_bug_in_a_0xee_frame),
        ("fragment names differ / junk after NUL: reads back", test_fragment_name_mismatch_still_reads_back),
        ("any reassembled schema reads back (property)", test_reassembled_schema_always_reads_back),
        ("unreadable schema stays unnumbered", test_capture_leaves_an_unreadable_schema_unnumbered),
        ("writer: activation ids run out loudly", test_writer_activation_ids_run_out_loudly),
        ("capture keeps saving when ids run out", test_capture_keeps_saving_when_activation_ids_run_out),
        ("a disk error is not swallowed", test_disk_error_while_numbering_a_schema_propagates),
        ("CSV names by content (unresolved first)", test_unresolved_rows_never_take_the_plain_name),
        ("reconnect drops a half-received schema", test_reconnect_drops_a_half_received_schema),
        ("SESSION boundary drops a half-received schema (read)",
         test_session_boundary_drops_a_half_received_schema_on_read),
        ("reconnect starts a new seq baseline", test_reconnect_starts_a_new_seq_baseline),
        ("wire padding tail: one activation", test_wire_padding_tail_does_not_make_a_second_activation),
        ("reader reports broken activation records", test_reader_reports_broken_activation_records),
        ("summary counts a schema once", test_summary_registry_counts_a_schema_once),
        ("export CLI reports swaps / reconnects", test_export_cli_reports_swaps_and_reconnects),
    ]

    failed = 0
    tmp_root = tempfile.mkdtemp(prefix="xmlog_activation_")
    try:
        for n, (name, fn) in enumerate(tests):
            sub = os.path.join(tmp_root, "t%02d" % n)
            os.makedirs(sub)
            try:
                fn(sub)
                print("  PASS  " + name)
            except AssertionError as e:
                failed += 1
                print("  FAIL  " + name)
                print("        " + str(e))
            except Exception as e:  # noqa: BLE001
                failed += 1
                print("  ERROR " + name)
                print("        %s: %s" % (type(e).__name__, e))
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)

    print("")
    if failed:
        print("%d/%d FAILED" % (failed, len(tests)))
        return 1
    print("%d/%d passed — activation 발급 · 참조 · 재연결 무효화 · 시점별 디코딩" % (len(tests), len(tests)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
