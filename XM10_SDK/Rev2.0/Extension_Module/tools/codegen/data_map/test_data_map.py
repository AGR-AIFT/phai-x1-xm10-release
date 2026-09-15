#!/usr/bin/env python3
"""Data Map 코드젠 회귀 시험 — 보드도 컴파일러도 없이 돈다.

    python test_data_map.py

무엇을 지키려는 시험인가
------------------------
생성기가 "111 descriptor / 197 scalar / 365 B" 라고 **스스로 말하는 것**은 증거가 아니다.
같은 함수가 세 산출물을 다 만들기 때문에, 그 함수가 틀리면 셋이 사이좋게 같이 틀린다.

그래서 이 시험은 가능한 한 **서로 다른 경로끼리** 맞춰 본다:

  * struct 포맷 문자열 ↔ descriptor 의 offset 필드
      → 포맷은 `build_struct_chunks()`, offset 은 `flatten_fields()` 가 따로 만든다.
        `struct` 모듈이 계산한 크기와 손으로 누적한 offset 이 197개 전부 일치해야 한다.
  * C 헤더 ↔ Python 맵
      → 채널 수는 C 생성기가 `channel_count` 로 따로 세고, Python 은 `expand_scalars()`
        로 따로 센다. 두 값이 같아야 한다. 크기는 C 쪽 `_Static_assert` 리터럴이 근거다.
  * TypeScript ↔ Python
      → descriptor 개수와 지문·크기 상수.
  * 디스크의 산출물 ↔ YAML 을 지금 다시 읽어 만든 산출물
      → 생성물을 손으로 고쳤거나 YAML 만 고치고 재생성을 잊은 상태를 잡는다.

시험이 통과한다고 **FW 가 그 레이아웃으로 보낸다**는 뜻은 아니다. 그것은 보드에서
확인할 일이고(A25), 여기서 보는 것은 세 산출물이 한 YAML 에서 모순 없이 나왔다는 것뿐이다.
"""
import importlib.util
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
YAML = os.path.join(HERE, "xm_total_data.yaml")
GEN = os.path.join(HERE, "generate_data_map.py")
PY_MAP = os.path.join(HERE, "generated", "xm_total_data_map.py")
TS_MAP = os.path.join(HERE, "generated", "xm_total_data_map.ts")
C_HDR = os.path.normpath(os.path.join(HERE, "..", "..", "..",
                                      "XM_FW", "System", "Comm", "USB",
                                      "xm_total_data_packet.h"))

# 계약 수치. 바뀌면 이 파일도 같이 고쳐야 한다 — 그게 이 상수들의 존재 이유다.
EXPECT_DESCRIPTORS = 111
EXPECT_SCALARS = 197
EXPECT_SIZE = 365


def _load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _read(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


# =============================================================================
# 1. 생성기 자체 — 개수와 지문
# =============================================================================
def test_generator_counts(gen, M):
    data = gen.load_yaml(YAML)
    flat, size = gen.flatten_fields(data.get("groups", []), data.get("types", {}))
    scalars = gen.expand_scalars(flat)

    assert size == EXPECT_SIZE, f"total size {size} != {EXPECT_SIZE}"
    assert len(flat) == EXPECT_DESCRIPTORS, f"descriptors {len(flat)} != {EXPECT_DESCRIPTORS}"
    assert len(scalars) == EXPECT_SCALARS, f"scalars {len(scalars)} != {EXPECT_SCALARS}"

    fp = gen.compute_fingerprint(flat, size)
    assert re.fullmatch(r"[0-9a-f]{16}", fp), f"fingerprint shape: {fp!r}"
    # 두 번 계산해도 같아야 한다 (dict 순회 순서 등에 새지 않는지)
    assert fp == gen.compute_fingerprint(flat, size), "fingerprint is not stable"
    assert fp == M.DATA_MAP_FINGERPRINT, \
        f"생성물 지문 {M.DATA_MAP_FINGERPRINT} != 지금 계산한 {fp} — 재생성 필요"


def test_no_gaps_or_overlaps(gen):
    """365 B 를 111 descriptor 가 빈틈없이·겹침없이 덮는가."""
    data = gen.load_yaml(YAML)
    flat, size = gen.flatten_fields(data.get("groups", []), data.get("types", {}))
    cursor = 0
    for f in flat:
        assert f["offset"] == cursor, \
            f"{f['name']}: offset {f['offset']} != 누적 {cursor} (gap 또는 overlap)"
        cursor += gen.TYPE_INFO[f["type"]]["size"] * f["count"]
    assert cursor == size, f"덮은 바이트 {cursor} != 총 크기 {size}"


# =============================================================================
# 2. struct 포맷 ↔ descriptor offset (독립 경로 대조)
# =============================================================================
def test_struct_matches_descriptors(M):
    """포맷 문자열을 앞에서부터 잘라 가며 각 descriptor 의 시작 바이트를 다시 구한다.

    `struct.calcsize` 가 세는 값과 `flatten_fields` 가 누적한 offset 은 서로 다른
    코드가 만든 것이라, 둘이 197번 연속으로 우연히 같을 수는 없다.
    """
    assert M.PACKET_STRUCT.size == EXPECT_SIZE, \
        f"struct size {M.PACKET_STRUCT.size} != {EXPECT_SIZE}"

    endian = M.PACKET_STRUCT_FORMAT[0]
    acc = ""
    for d in M.TOTAL_DATA_MAP:
        here = struct.calcsize(endian + acc) if acc else 0
        assert here == d.offset, \
            f"{d.name}: struct 누적 {here} != 선언 offset {d.offset}"
        code = {1: "B", 2: "H", 4: "I"}  # 크기만 보므로 부호/실수 구분 불필요
        acc += "%d%s" % (d.count, code[M.TYPE_SIZE[d.type]])
    assert struct.calcsize(endian + acc) == EXPECT_SIZE


def test_scalar_expansion_order(M):
    """SCALARS 가 unpack() 순서와 1:1 인지 — descriptor 를 count 만큼 편 것과 같아야 한다."""
    expanded = []
    for d in M.TOTAL_DATA_MAP:
        for i in range(d.count):
            expanded.append(d.name if d.count == 1 else "%s[%d]" % (d.name, i))
    assert len(expanded) == EXPECT_SCALARS
    assert tuple(expanded) == M.SCALAR_NAMES, "SCALAR_NAMES 가 descriptor 전개 순서와 다르다"
    assert len(M.SCALAR_MUL) == len(M.SCALAR_DIV) == EXPECT_SCALARS
    for name, mul, div in zip(M.SCALAR_NAMES, M.SCALAR_MUL, M.SCALAR_DIV):
        assert not (mul is not None and div is not None), \
            f"{name}: mul 과 div 가 동시에 설정됨 — 공식이 하나여야 한다"


# =============================================================================
# 3. 값 검증 — 실제 바이트를 넣고 물리값을 확인
# =============================================================================
def test_decode_known_values(M):
    """알려진 raw 를 심어 물리값을 검증한다. 세 공식이 모두 한 번씩 걸리게 고른다."""
    idx = {n: i for i, n in enumerate(M.SCALAR_NAMES)}
    by_name = {d.name: d for d in M.TOTAL_DATA_MAP}

    # multiply_divide: leftHipAngle(int16, scale 720) — raw 16384 -> 16384*720/32768 = 360.0
    assert by_name["leftHipAngle"].scale_formula == "multiply_divide"
    # none: xm_loop_count(uint32) — 그대로
    assert by_name["xm_loop_count"].scale_formula == "none"

    buf = bytearray(EXPECT_SIZE)
    struct.pack_into("<I", buf, by_name["xm_loop_count"].offset, 123456)
    struct.pack_into("<h", buf, by_name["leftHipAngle"].offset, 16384)

    raw = M.PACKET_STRUCT.unpack(bytes(buf))
    assert len(raw) == EXPECT_SCALARS

    def phys(name):
        i = idx[name]
        v, mul, div = raw[i], M.SCALAR_MUL[i], M.SCALAR_DIV[i]
        return v * mul if mul is not None else (v / div if div is not None else v)

    assert raw[idx["xm_loop_count"]] == 123456
    assert phys("xm_loop_count") == 123456
    assert phys("leftHipAngle") == 360.0, phys("leftHipAngle")

    # multiply_divide 를 곱셈 하나로 접은 것이 나눗셈 형태와 **정확히** 같은지.
    # 분모가 2의 거듭제곱이라 성립하는 성질이라, 분모가 바뀌면 여기서 깨져야 한다.
    for d in M.TOTAL_DATA_MAP:
        if d.scale_formula != "multiply_divide":
            continue
        i = idx[d.name if d.count == 1 else "%s[0]" % d.name]
        for probe in (1, -1, 32767, -32768, 12345):
            assert probe * M.SCALAR_MUL[i] == probe * d.scale / M.MULTIPLY_DIVIDE_DENOM, \
                f"{d.name}: 접은 곱셈이 원식과 다르다 (raw={probe})"

    # divide 는 나눗셈을 그대로 남겼는지
    for d in M.TOTAL_DATA_MAP:
        if d.scale_formula != "divide":
            continue
        i = idx[d.name if d.count == 1 else "%s[0]" % d.name]
        assert M.SCALAR_MUL[i] is None and M.SCALAR_DIV[i] == float(d.scale), \
            f"{d.name}: divide 인데 div 가 {M.SCALAR_DIV[i]}"


def test_decode_per_descriptor_independent(M):
    """빅 struct 한 방 unpack 과, descriptor 별 unpack_from 이 같은 값을 주는가.

    앞의 offset 대조가 '크기' 기준이었다면 이건 '값' 기준이다 — 부호·실수 코드까지 걸린다.
    """
    code = {"uint8": "B", "int8": "b", "uint16": "H", "int16": "h",
            "uint32": "I", "int32": "i", "float32": "f"}
    # 재현 가능한 패턴. 값 자체엔 의미가 없고, 두 경로가 같은 바이트를 읽는지만 본다.
    buf = bytes((i * 37 + 11) & 0xFF for i in range(EXPECT_SIZE))
    raw = M.PACKET_STRUCT.unpack(buf)

    k = 0
    endian = M.PACKET_STRUCT_FORMAT[0]
    for d in M.TOTAL_DATA_MAP:
        vals = struct.unpack_from("%s%d%s" % (endian, d.count, code[d.type]), buf, d.offset)
        for j, v in enumerate(vals):
            got = raw[k + j]
            if d.type == "float32":
                # NaN 은 == 로 비교되지 않는다. 비트 패턴으로 본다.
                assert struct.pack("<f", got) == struct.pack("<f", v), \
                    f"{d.name}[{j}]: {got!r} != {v!r}"
            else:
                assert got == v, f"{d.name}[{j}]: {got} != {v}"
        k += d.count
    assert k == EXPECT_SCALARS


# =============================================================================
# 4. 산출물 3종의 상호 정합
# =============================================================================
def test_c_header_agrees(M):
    src = _read(C_HDR)

    m = re.search(r"_Static_assert\(sizeof\(\w+\)\s*==\s*(\d+)", src)
    assert m, "C 헤더에서 _Static_assert 크기를 못 찾음"
    assert int(m.group(1)) == EXPECT_SIZE, f"C 헤더 크기 {m.group(1)} != {EXPECT_SIZE}"

    m = re.search(r"#define\s+XM_TOTAL_DATA_NUM_CHANNELS\s+(\d+)", src)
    assert m, "XM_TOTAL_DATA_NUM_CHANNELS 없음"
    # C 생성기는 채널을 자기 루프에서 따로 센다 — Python 전개와 같아야 한다.
    assert int(m.group(1)) == M.NUM_SCALARS, \
        f"C 채널수 {m.group(1)} != Python scalar {M.NUM_SCALARS}"

    m = re.search(r'#define\s+XM_TOTAL_DATA_MAP_FINGERPRINT\s+"([0-9a-f]{16})"', src)
    assert m, "XM_TOTAL_DATA_MAP_FINGERPRINT 없음"
    assert m.group(1) == M.DATA_MAP_FINGERPRINT, "C 헤더 지문 불일치"

    m = re.search(r'#define\s+XM_TOTAL_DATA_MAP_VERSION\s+"([^"]+)"', src)
    assert m and m.group(1) == M.DATA_MAP_VERSION, "C 헤더 버전 불일치"

    assert "Generated:" not in src, "C 헤더에 wall-clock 타임스탬프가 남아 있다 (비결정적)"


def test_ts_agrees(M):
    src = _read(TS_MAP)

    m = re.search(r"export const TOTAL_PACKET_SIZE = (\d+);", src)
    assert m and int(m.group(1)) == EXPECT_SIZE, "TS 크기 불일치"

    m = re.search(r"export const DATA_MAP_FINGERPRINT = '([0-9a-f]{16})';", src)
    assert m and m.group(1) == M.DATA_MAP_FINGERPRINT, "TS 지문 불일치"

    n = len(re.findall(r"^\s*\{ offset:", src, re.M))
    assert n == EXPECT_DESCRIPTORS, f"TS descriptor {n} != {EXPECT_DESCRIPTORS}"

    assert "Generated:" not in src, "TS 에 wall-clock 타임스탬프가 남아 있다 (비결정적)"


def test_artifacts_are_current(gen):
    """디스크의 셋이 지금 YAML 에서 나오는 것과 같은가 (= --check 와 같은 판정)."""
    data = gen.load_yaml(YAML)
    groups, types = data.get("groups", []), data.get("types", {})
    total = sum(gen.calc_group_size(g, types) for g in groups)
    flat, _ = gen.flatten_fields(groups, types)
    fp = gen.compute_fingerprint(flat, total)

    pairs = [
        (C_HDR, gen.generate_c_header(data, types, total, fp), "C header"),
        (TS_MAP, gen.generate_ts(data, flat, total, fp), "TypeScript"),
        (PY_MAP, gen.generate_py(data, flat, total, fp), "Python map"),
    ]
    for path, fresh, label in pairs:
        # EOL 은 비교 대상이 아니다 — 워킹트리는 autocrlf 로 CRLF, git blob 은 LF.
        assert _read(path).splitlines() == fresh.splitlines(), \
            f"{label} 가 YAML 과 어긋남 ({path}) — 재생성 필요"


def test_generation_is_deterministic(gen):
    """같은 입력으로 두 번 생성하면 같은 텍스트여야 한다 (타임스탬프 회귀 방지)."""
    data = gen.load_yaml(YAML)
    groups, types = data.get("groups", []), data.get("types", {})
    total = sum(gen.calc_group_size(g, types) for g in groups)
    flat, _ = gen.flatten_fields(groups, types)
    fp = gen.compute_fingerprint(flat, total)
    for label, fn in (("C", lambda: gen.generate_c_header(data, types, total, fp)),
                      ("TS", lambda: gen.generate_ts(data, flat, total, fp)),
                      ("PY", lambda: gen.generate_py(data, flat, total, fp))):
        assert fn() == fn(), f"{label} 생성이 비결정적이다"


# =============================================================================
# Runner
# =============================================================================
def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    if not os.path.exists(PY_MAP):
        print(f"SKIP-FAIL: {PY_MAP} 없음 — 먼저 --py-out 으로 생성할 것")
        return 1

    gen = _load(GEN, "generate_data_map")
    try:
        M = _load(PY_MAP, "xm_total_data_map")
    except Exception as e:  # noqa: BLE001 — 생성물이 깨졌다는 것 자체가 시험 결과다
        print("  FAIL  generated map import")
        print("        %s: %s" % (type(e).__name__, e))
        print("")
        print("1/1 FAILED — 생성물을 import 조차 할 수 없다. 재생성할 것.")
        return 1

    tests = [
        ("generator counts / fingerprint", lambda: test_generator_counts(gen, M)),
        ("layout has no gaps or overlaps", lambda: test_no_gaps_or_overlaps(gen)),
        ("struct format matches offsets", lambda: test_struct_matches_descriptors(M)),
        ("scalar expansion order", lambda: test_scalar_expansion_order(M)),
        ("decode known values", lambda: test_decode_known_values(M)),
        ("per-descriptor decode agrees", lambda: test_decode_per_descriptor_independent(M)),
        ("C header agrees", lambda: test_c_header_agrees(M)),
        ("TypeScript agrees", lambda: test_ts_agrees(M)),
        ("artifacts are current", lambda: test_artifacts_are_current(gen)),
        ("generation is deterministic", lambda: test_generation_is_deterministic(gen)),
    ]

    failed = 0
    for name, fn in tests:
        try:
            fn()
            print(f"  PASS  {name}")
        except AssertionError as e:
            failed += 1
            print(f"  FAIL  {name}\n        {e}")
        except Exception as e:  # noqa: BLE001 — 어떤 예외든 실패로 보고하고 계속 돈다
            failed += 1
            print(f"  ERROR {name}\n        {type(e).__name__}: {e}")

    print()
    if failed:
        print(f"{failed}/{len(tests)} FAILED")
        return 1
    print(f"{len(tests)}/{len(tests)} passed — "
          f"{EXPECT_DESCRIPTORS} descriptors / {EXPECT_SCALARS} scalars / "
          f"{EXPECT_SIZE} B, fingerprint {M.DATA_MAP_FINGERPRINT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
