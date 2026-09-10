#!/usr/bin/env python3
"""골든 벡터 대조 — **다른 구현이 만든 바이트**를 우리 파서가 푸는가.

    python test_golden_vectors.py

왜 이 시험이 다른가
-------------------
같은 폴더의 다른 시험들은 우리 코드가 만든 것을 우리 코드가 검사한다. 손으로 적은
골든 바이트를 섞어 그 위험을 줄였지만, 바이트를 적은 사람도 결국 같은 사람이다.

이 시험이 먹는 바이트는 **개발 레포의 독립 생성기**(`Extension_Module/tools/spec/
gen_golden_vectors.py`)가 만든 것이다. 그 생성기는 이 폴더의 코드를 import 하지 않고
설계 문서(PLAN §4.1 / §4.6)의 표를 보고 `struct` 로 직접 조립한다. 두 구현이 서로 모르는
채 같은 바이트에 도달하면, 둘 다 표를 제대로 읽었다는 뜻이다.

FW 가 세 번째 구현이 된다 — `0xEE` 송신을 만들 때 같은 벡터를 재현해야 한다.

벡터를 못 찾으면
----------------
건너뛰되 **크게 적는다**. 조용히 통과하면 없는 것과 같다.
"""
import json
import os
import sys

import schema_0xee as EE
import xmlog as X

_HERE = os.path.dirname(os.path.abspath(__file__))

# 배달된 사본이 먼저, 없으면 옆에 있는 개발 레포.
# 세 번째는 PyInstaller 번들 안 — 그때는 `_HERE` 가 추출 디렉토리라서 `..` 로 올라가면
# 번들 밖으로 나가 버린다. 벡터를 번들 루트에 같이 넣고 여기서 찾는다.
_CANDIDATES = [
    os.path.normpath(os.path.join(_HERE, "..", "spec", "golden")),
    os.path.normpath(os.path.join(_HERE, "..", "..", "..",
                                  "phai-x1-xm10-develop", "docs", "spec", "golden")),
    os.path.join(_HERE, "spec", "golden"),
]

TYPE_NAMES = {v: k for k, v in
              ((n, t) for n, t in
               (("f32", 0), ("f64", 1), ("i8", 2), ("u8", 3), ("i16", 4), ("u16", 5),
                ("i32", 6), ("u32", 7), ("i64", 8), ("u64", 9), ("bool", 10)))}


def find_dir():
    for d in _CANDIDATES:
        if os.path.isdir(d) and os.path.exists(os.path.join(d, "0xEE_minimal.hex")):
            return d
    return None


def load(d, name):
    with open(os.path.join(d, name + ".hex"), encoding="utf-8") as f:
        blob = bytes.fromhex(f.read().strip())
    with open(os.path.join(d, name + ".json"), encoding="utf-8") as f:
        meta = json.load(f)
    if len(blob) != meta["byte_length"]:
        raise AssertionError("%s: hex %d B != json byte_length %d"
                             % (name, len(blob), meta["byte_length"]))
    return blob, meta


def _cmp_fields(got_fields, want_fields, label):
    assert len(got_fields) == len(want_fields), \
        "%s: 필드 %d개 != %d개" % (label, len(got_fields), len(want_fields))
    for i, (g, w) in enumerate(zip(got_fields, want_fields)):
        assert g.name == w["name"], "%s[%d] name %r != %r" % (label, i, g.name, w["name"])
        assert g.unit == w["unit"], "%s[%d] unit %r != %r" % (label, i, g.unit, w["unit"])
        assert TYPE_NAMES[g.type_tag] == w["type"], \
            "%s[%d] type %s != %s" % (label, i, TYPE_NAMES[g.type_tag], w["type"])
        assert g.array_len == w["array_len"], \
            "%s[%d] array_len %d != %d" % (label, i, g.array_len, w["array_len"])
        assert g.offset == w["offset"], \
            "%s[%d] offset %d != %d" % (label, i, g.offset, w["offset"])
        assert abs(g.scale - w["scale"]) < 1e-9, \
            "%s[%d] scale %r != %r" % (label, i, g.scale, w["scale"])


def test_ee_single(d, name):
    blob, meta = load(d, name)
    want = meta["expect"]
    frag = EE.parse_fragment(blob)
    assert frag.proto_ver == want["schema_proto_ver"]
    assert frag.module_id == want["target_module_id"], \
        "module 0x%02X != 0x%02X" % (frag.module_id, want["target_module_id"])
    assert frag.struct_size == want["struct_size"], \
        "struct_size %d != %d" % (frag.struct_size, want["struct_size"])
    assert frag.schema_crc32 == want["schema_crc32"], \
        "crc %08x != %08x" % (frag.schema_crc32, want["schema_crc32"])
    assert frag.field_count_total == want["field_count_total"]
    assert frag.frame_count == want["frame_count"]
    assert frag.struct_name == want["struct_name"], \
        "name %r != %r" % (frag.struct_name, want["struct_name"])
    _cmp_fields(frag.fields, want["fields"], name)

    # 우리 인코더가 같은 바이트를 만들어내는가 (양방향)
    remade = EE.encode_fragment(frag.module_id, frag.struct_size, frag.struct_name,
                                list(frag.fields), frag.frame_index, frag.frame_count,
                                frag.field_count_total, frag.schema_crc32)
    assert remade == blob, "%s: 재인코딩이 원본과 다르다" % name

    # 완성된 스키마로서 불변식도 통과해야 한다 (겹침·범위 초과 없음)
    sc = EE.Schema(frag.module_id, frag.struct_name, frag.struct_size,
                   frag.proto_ver, frag.schema_crc32, frag.fields)
    if frag.frame_count == 1:
        EE.validate(sc)


def test_ee_multi(d):
    blob, meta = load(d, "0xEE_multi_fragment")
    want = meta["expect"]
    lens = meta["frame_lengths"]
    assert sum(lens) == len(blob), "frame_lengths 합 %d != %d" % (sum(lens), len(blob))

    ra = EE.Reassembler()
    off = 0
    schema = None
    for i, L in enumerate(lens):
        part = blob[off:off + L]
        off += L
        got = ra.feed(part, i * 0.1)
        if i < len(lens) - 1:
            assert got is None, "프래그먼트 %d/%d 인데 완성됐다" % (i + 1, len(lens))
        else:
            schema = got
    assert schema is not None, "전부 먹였는데 완성되지 않았다"
    assert schema.module_id == want["target_module_id"]
    assert schema.struct_size == want["struct_size"]
    assert schema.schema_crc32 == want["schema_crc32"], \
        "crc %08x != %08x — CRC 범위 규약이 생성기와 다르다" % (schema.schema_crc32,
                                                              want["schema_crc32"])
    assert len(schema.fields) == want["field_count_total"] == 93, len(schema.fields)
    _cmp_fields(schema.fields, want["fields"], "multi")
    EE.validate(schema)


def test_xmlog_file(d):
    blob, meta = load(d, "xmlog_v1_minimal_file")
    want = meta["expect"]
    res = X.scan(blob)
    assert res.stopped_reason is None, "골든 파일을 끝까지 못 읽었다: %s" % res.stopped_reason
    assert res.trailing_bytes == 0
    assert res.header.format_version == want["format_version"]
    assert res.header.create_unix_ns == want["create_unix_ns"], \
        "create_unix_ns %x != %x" % (res.header.create_unix_ns, want["create_unix_ns"])

    got = res.records
    exp = want["records"]
    assert len(got) == len(exp), "레코드 %d != %d" % (len(got), len(exp))

    names = {X.REC_SESSION: "SESSION", X.REC_SCHEMA_ACTIVATION: "SCHEMA_ACTIVATION",
             X.REC_DATA: "DATA", X.REC_GAP: "GAP"}
    for i, (g, w) in enumerate(zip(got, exp)):
        assert names[g.rec_type] == w["type"], \
            "레코드 %d 타입 %s != %s" % (i, names[g.rec_type], w["type"])
        for k, v in w.items():
            if k in ("type", "payload_hex"):
                continue
            assert g.fields.get(k) == v, \
                "레코드 %d(%s) %s = %r, 기대 %r" % (i, w["type"], k, g.fields.get(k), v)
        if "payload_hex" in w:
            assert g.payload.hex() == w["payload_hex"], \
                "레코드 %d payload %s != %s" % (i, g.payload.hex(), w["payload_hex"])


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    d = find_dir()
    if d is None:
        print("  SKIP  골든 벡터를 찾지 못했다. 찾아본 곳:")
        for c in _CANDIDATES:
            print("        " + c)
        print("        생성: python Extension_Module/tools/spec/gen_golden_vectors.py")
        print("        ⚠ 이 시험 없이는 '우리 구현끼리만 맞는' 상태를 구별할 수 없다")
        return 0

    print("골든 벡터: %s" % d)
    tests = [
        ("0xEE minimal (1 field)", lambda: test_ee_single(d, "0xEE_minimal")),
        ("0xEE max single fragment (31 fields)",
         lambda: test_ee_single(d, "0xEE_max_single_fragment")),
        ("0xEE multi-fragment (93 fields, 3 frames)", lambda: test_ee_multi(d)),
        ("xmlog v1 minimal file", lambda: test_xmlog_file(d)),
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
        print("%d/%d FAILED — 독립 구현 두 개가 서로 다른 바이트를 말하고 있다"
              % (failed, len(tests)))
        return 1
    print("%d/%d passed — 독립 생성기의 바이트를 그대로 푼다" % (len(tests), len(tests)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
