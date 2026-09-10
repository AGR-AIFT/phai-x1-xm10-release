#!/usr/bin/env python3
"""
XM10 Total Data Packet — Code Generator

YAML SSOT → C header + TypeScript types + Python decode map.

Usage:
    python generate_data_map.py xm_total_data.yaml \
        --c-out ../../XM_FW/System/Comm/USB/xm_total_data_packet.h \
        --ts-out generated/xm_total_data_map.ts \
        --py-out generated/xm_total_data_map.py

    python generate_data_map.py xm_total_data.yaml --check ...   # 재생성 없이 정합만 검사

산출물은 **결정적**이다 — 같은 YAML 이면 같은 바이트가 나온다. 예전에는 세 산출물 모두
`datetime.now()` 를 헤더 주석에 박아서, 내용이 같아도 재생성할 때마다 diff 가 났다.
그래서 "빌드가 헤더를 건드렸는지" 와 "레이아웃이 바뀌었는지" 를 구별할 수 없었다.
타임스탬프를 빼고 그 자리에 **레이아웃 지문**(descriptor 정규화 SHA-256 앞 16자리)을 넣었다.

지문은 세 산출물이 모두 같은 값을 갖는다. 버전 문자열(YAML `version`)과 달리 지문은
**바이트 레이아웃 그 자체**에서 나오므로, 사람이 version 을 올리는 걸 잊어도 갈라진다.
반대로 주석만 고친 YAML 두 개는 같은 지문을 준다 (Rev1.1/Rev2.0 SSOT 가 실제로 그렇다 —
2026-09-10 실측: 두 브랜치 YAML 은 텍스트가 다르지만 지문은 ecb61fd3ef12d9af 로 같다).

Dependencies:
    pip install pyyaml
"""
import sys
import os
import argparse
import hashlib

try:
    import yaml
except ImportError:
    print("ERROR: pyyaml not installed. Run: pip install pyyaml", file=sys.stderr)
    sys.exit(1)


# =============================================================================
# Type system
# =============================================================================
TYPE_INFO = {
    "uint8":   {"c": "uint8_t",  "ts": "'uint8'",   "size": 1, "struct": "B"},
    "int8":    {"c": "int8_t",   "ts": "'int8'",    "size": 1, "struct": "b"},
    "uint16":  {"c": "uint16_t", "ts": "'uint16'",  "size": 2, "struct": "H"},
    "int16":   {"c": "int16_t",  "ts": "'int16'",   "size": 2, "struct": "h"},
    "uint32":  {"c": "uint32_t", "ts": "'uint32'",  "size": 4, "struct": "I"},
    "int32":   {"c": "int32_t",  "ts": "'int32'",   "size": 4, "struct": "i"},
    "float32": {"c": "float",    "ts": "'float32'", "size": 4, "struct": "f"},
}

# YAML `endian` -> struct 접두사. 패킷은 `#pragma pack(1)` 이라 정렬 패딩이 없고,
# 접두사가 '<'/'>' 이면 struct 도 native alignment 를 쓰지 않는다 — 둘이 맞아떨어진다.
ENDIAN_PREFIX = {"little": "<", "big": ">"}


def load_yaml(path):
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def get_type_size(type_name, custom_types):
    if type_name in TYPE_INFO:
        return TYPE_INFO[type_name]["size"]
    if type_name in custom_types:
        return custom_types[type_name]["size"]
    raise ValueError(f"Unknown type: {type_name}")


def get_c_type(type_name, custom_types):
    if type_name in TYPE_INFO:
        return TYPE_INFO[type_name]["c"]
    return None  # Custom type handled separately


def calc_field_size(field, custom_types):
    """Calculate total byte size of a field (including count)."""
    count = field.get("count", 1)
    type_name = field.get("type", "")
    return get_type_size(type_name, custom_types) * count


def calc_group_size(group, custom_types):
    """Calculate total byte size of a group."""
    if group.get("reserved"):
        return group["size"]
    total = 0
    for field in group.get("fields", []):
        total += calc_field_size(field, custom_types)
    return total


# =============================================================================
# Flatten fields for TypeScript (expand complex types and arrays)
# =============================================================================
def flatten_fields(groups, custom_types):
    """Flatten all fields into a list of (offset, name, type, scale, scaleFormula, unit, group, count)."""
    result = []
    offset = 0

    for group in groups:
        group_name = group["name"]

        if group.get("reserved"):
            offset += group["size"]
            continue

        group_formula = group.get("scale_formula", "none")

        for field in group.get("fields", []):
            fname = field.get("name", "")
            ftype = field.get("type", "")
            fcount = field.get("count", 1)
            fscale = field.get("scale", 1)
            funit = field.get("unit", "")
            fdesc = field.get("desc", "")

            # Check if it's a custom type (struct of struct)
            if ftype in custom_types:
                ct = custom_types[ftype]
                ct_size = ct["size"]
                for i in range(fcount):
                    for sf in ct["fields"]:
                        sf_name = sf["name"]
                        sf_type = sf["type"]
                        sf_count = sf.get("count", 1)
                        sf_scale = sf.get("scale", 1)
                        sf_unit = sf.get("unit", "")
                        sf_formula = group_formula if sf_scale != 1 else "none"
                        # Use group's scale_formula for sub-fields with scale
                        if sf_scale != 1:
                            sf_formula = group_formula

                        full_name = f"{fname}[{i}].{sf_name}"
                        result.append({
                            "offset": offset,
                            "name": full_name,
                            "type": sf_type,
                            "scale": sf_scale,
                            "scaleFormula": sf_formula,
                            "unit": sf_unit,
                            "group": group_name,
                            "count": sf_count,
                        })
                        offset += TYPE_INFO[sf_type]["size"] * sf_count
            else:
                formula = "none"
                if fscale != 1:
                    formula = group_formula

                result.append({
                    "offset": offset,
                    "name": fname,
                    "type": ftype,
                    "scale": fscale,
                    "scaleFormula": formula,
                    "unit": funit,
                    "group": group_name,
                    "count": fcount,
                })
                offset += TYPE_INFO[ftype]["size"] * fcount

    return result, offset


# =============================================================================
# Layout identity — 산출물 3종이 공유하는 지문
# =============================================================================
def canonical_layout(flat_fields, total_size):
    """레이아웃을 사람이 읽을 수 있는 정규형 텍스트로. 지문의 입력이자 diff 대상.

    포함: 필드 이름·타입·offset·개수·스케일·공식·단위·그룹 + 총 크기.
    제외: YAML 주석, 필드 desc, version 문자열, 파일 내 공백.
    """
    lines = ["total_size=%d" % total_size]
    for f in flat_fields:
        lines.append("%s|%s|%d|%d|%s|%s|%s|%s" % (
            f["name"], f["type"], f["offset"], f["count"],
            f["scale"], f["scaleFormula"], f["unit"], f["group"]))
    return "\n".join(lines)


def compute_fingerprint(flat_fields, total_size):
    """정규형의 SHA-256 앞 16자리(64비트). 충돌 저항이 목적이 아니라 대조가 목적이다."""
    canon = canonical_layout(flat_fields, total_size)
    return hashlib.sha256(canon.encode("utf-8")).hexdigest()[:16]


def build_struct_chunks(flat_fields, endian):
    """(prefix, [(struct_chunk, group_name), ...]) — 그룹 단위로 묶은 struct 포맷.

    그룹 경계를 유지하는 이유는 생성물이 읽히기 위해서다. 한 줄로 붙여 놓으면
    '이 h 가 어느 필드였나' 를 사람이 셀 수 없다.
    """
    prefix = ENDIAN_PREFIX.get(endian, "<")
    chunks = []
    cur_group = None
    cur = []
    for f in flat_fields:
        if f["group"] != cur_group:
            if cur:
                chunks.append(("".join(cur), cur_group))
            cur_group = f["group"]
            cur = []
        code = TYPE_INFO[f["type"]]["struct"]
        cur.append(code if f["count"] == 1 else "%d%s" % (f["count"], code))
    if cur:
        chunks.append(("".join(cur), cur_group))
    return prefix, chunks


def expand_scalars(flat_fields):
    """descriptor(111) -> scalar(197). struct.unpack 이 돌려주는 순서와 1:1 로 맞춘다.

    count>1 인 descriptor 는 `name[i]` 로 펼친다. 커스텀 타입 배열은 flatten 단계에서
    이미 `imu[0].q` 형태가 됐으므로 여기서 `imu[0].q[2]` 가 된다.
    """
    out = []
    for f in flat_fields:
        for i in range(f["count"]):
            out.append({
                "name": f["name"] if f["count"] == 1 else "%s[%d]" % (f["name"], i),
                "group": f["group"],
                "unit": f["unit"],
                "type": f["type"],
                "scale": f["scale"],
                "formula": f["scaleFormula"],
            })
    return out


# =============================================================================
# C Header Generator
# =============================================================================
def generate_c_header(data, custom_types, total_size, fingerprint):
    packet_name = data["packet_name"]
    module_id = data["module_id"]
    version = data["version"]
    groups = data["groups"]

    lines = []
    lines.append(f"/* AUTO-GENERATED from xm_total_data.yaml v{version} — DO NOT EDIT MANUALLY */")
    lines.append(f"/* Layout fingerprint: {fingerprint} (sha256 of the canonical descriptor list) */")
    lines.append("")
    lines.append(f"#ifndef XM_TOTAL_DATA_PACKET_H")
    lines.append(f"#define XM_TOTAL_DATA_PACKET_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdbool.h>")
    lines.append("")

    # Custom types first
    for tname, tdef in custom_types.items():
        c_tname = f"{tname}"
        lines.append(f"/* Complex type: {tdef.get('desc', '')} */")
        lines.append(f"typedef struct __attribute__((packed)) {{")
        for sf in tdef["fields"]:
            c_type = TYPE_INFO[sf["type"]]["c"]
            count = sf.get("count", 1)
            arr = f"[{count}]" if count > 1 else ""
            scale_comment = f", scale: {sf['scale']}" if sf.get("scale", 1) != 1 else ""
            unit_comment = f", unit: {sf.get('unit', '')}" if sf.get("unit") else ""
            lines.append(f"    {c_type:10s} {sf['name']}{arr};{' ' * max(1, 20 - len(sf['name']) - len(arr))}/* {sf.get('desc', '')}{scale_comment}{unit_comment} */")
        lines.append(f"}} {c_tname};")
        lines.append("")

    # Main packet struct
    lines.append("#pragma pack(push, 1)")
    lines.append(f"typedef struct {{")

    offset = 0
    channel_count = 0

    for group in groups:
        gname = group["name"]
        gsize = calc_group_size(group, custom_types)
        lines.append(f"    /* === {gname} ({gsize}B) === */")

        if group.get("reserved"):
            lines.append(f"    uint8_t  rsv_{gname.lower().replace('reserved_', '')}[{gsize}];{' ' * max(1, 10)}/* offset: {offset}, {group.get('desc', '')} */")
            offset += gsize
            lines.append("")
            continue

        for field in group.get("fields", []):
            fname = field.get("name", "")
            ftype = field.get("type", "")
            fcount = field.get("count", 1)
            fscale = field.get("scale", 1)
            funit = field.get("unit", "")

            if ftype in custom_types:
                # Custom type field
                ct = custom_types[ftype]
                arr = f"[{fcount}]" if fcount > 1 else ""
                lines.append(f"    {ftype:22s} {fname}{arr};{' ' * max(1, 15 - len(fname) - len(arr))}/* offset: {offset} */")
                field_size = ct["size"] * fcount
                offset += field_size
                # Count sub-channels
                for _ in range(fcount):
                    for sf in ct["fields"]:
                        channel_count += sf.get("count", 1)
            else:
                c_type = TYPE_INFO[ftype]["c"]
                arr = f"[{fcount}]" if fcount > 1 else ""
                scale_str = f", scale: {fscale}" if fscale != 1 else ""
                unit_str = f", unit: {funit}" if funit else ""

                c_name = fname
                lines.append(f"    {c_type:10s} {c_name}{arr};{' ' * max(1, 25 - len(c_name) - len(arr))}/* offset: {offset}{scale_str}{unit_str} */")
                field_size = TYPE_INFO[ftype]["size"] * fcount
                offset += field_size
                channel_count += fcount

        lines.append("")

    lines.append(f"}} {packet_name}_t;")
    lines.append("#pragma pack(pop)")
    lines.append("")

    # Static assert
    lines.append(f"_Static_assert(sizeof({packet_name}_t) == {total_size},")
    lines.append(f'    "{packet_name}_t size mismatch — update YAML and regenerate");')
    lines.append("")

    # Defines
    lines.append(f"#define XM_TOTAL_DATA_PAYLOAD_SIZE  sizeof({packet_name}_t)")
    mid_hex = f"0x{module_id:02X}" if isinstance(module_id, int) else str(module_id)
    lines.append(f"#define XM_TOTAL_DATA_MODULE_ID     {mid_hex}")
    lines.append(f"#define XM_TOTAL_DATA_NUM_CHANNELS  {channel_count}   /* excluding reserved */")
    lines.append("")
    lines.append("/* 맵 정체성 — PC 디코더 산출물(.ts/.py)과 같은 값이다.")
    lines.append(" * 지금은 어떤 .c 도 참조하지 않으므로 코드/플래시 영향이 0 이다.")
    lines.append(" * 와이어에 실어야 할 때(호스트가 FW 맵을 스스로 식별해야 할 때)")
    lines.append(" * 그 자리에 넣을 값이 이미 여기 있다. */")
    lines.append(f'#define XM_TOTAL_DATA_MAP_VERSION      "{version}"')
    lines.append(f'#define XM_TOTAL_DATA_MAP_FINGERPRINT  "{fingerprint}"')
    lines.append("")
    lines.append(f"#endif /* XM_TOTAL_DATA_PACKET_H */")
    lines.append("")

    return "\n".join(lines)


# =============================================================================
# TypeScript Generator
# =============================================================================
def generate_ts(data, flat_fields, total_size, fingerprint):
    version = data["version"]
    module_id = data["module_id"]

    lines = []
    lines.append(f"/* AUTO-GENERATED from xm_total_data.yaml v{version} — DO NOT EDIT MANUALLY */")
    lines.append(f"/* Layout fingerprint: {fingerprint} (sha256 of the canonical descriptor list) */")
    lines.append("")
    lines.append("export interface ChannelDef {")
    lines.append("  offset: number;")
    lines.append("  type: 'uint8' | 'int8' | 'uint16' | 'int16' | 'uint32' | 'int32' | 'float32';")
    lines.append("  scale: number;")
    lines.append("  scaleFormula: 'none' | 'divide' | 'multiply_divide';")
    lines.append("  unit: string;")
    lines.append("  name: string;")
    lines.append("  group: string;")
    lines.append("  count?: number;  // array size (default 1)")
    lines.append("}")
    lines.append("")

    # Type size map
    lines.append("export const TYPE_SIZE: Record<string, number> = {")
    for tname, tinfo in TYPE_INFO.items():
        lines.append(f"  '{tname}': {tinfo['size']},")
    lines.append("};")
    lines.append("")

    lines.append("export const TOTAL_DATA_MAP: ChannelDef[] = [")

    for f in flat_fields:
        count_str = ""
        if f["count"] > 1:
            count_str = f", count: {f['count']}"

        scale_val = f["scale"]
        if isinstance(scale_val, float):
            scale_str = f"{scale_val}"
        else:
            scale_str = str(scale_val)

        lines.append(
            f"  {{ offset: {f['offset']:3d}, type: {TYPE_INFO[f['type']]['ts']:10s}, "
            f"scale: {scale_str:>8s}, scaleFormula: '{f['scaleFormula']}', "
            f"unit: '{f['unit']}', name: '{f['name']}', group: '{f['group']}'"
            f"{count_str} }},"
        )

    lines.append("];")
    lines.append("")
    lines.append(f"export const TOTAL_PACKET_SIZE = {total_size};")
    lines.append(f"export const DATA_MAP_VERSION = '{version}';")
    lines.append(f"export const DATA_MAP_FINGERPRINT = '{fingerprint}';")
    mid_hex = f"0x{module_id:02X}" if isinstance(module_id, int) else str(module_id)
    lines.append(f"export const MODULE_ID_TOTAL = {mid_hex};")
    lines.append(f"export const MODULE_ID_USER_META = 0xEF;")
    lines.append(f"export const MODULE_ID_USER_CUSTOM_START = 0xF0;")
    lines.append(f"export const MODULE_ID_USER_CUSTOM_END = 0xFE;")
    lines.append("")

    return "\n".join(lines)


# =============================================================================
# Python Generator — PC 디코더가 import 하는 맵
# =============================================================================
def generate_py(data, flat_fields, total_size, fingerprint):
    """0x20 페이로드를 이름 붙은 스칼라로 푸는 데 필요한 **데이터만** 낸다.

    로직(디코더)은 이 파일에 두지 않는다 — 생성물은 손대면 안 되는 파일이고,
    디코딩 정책(스케일 적용 여부, numpy 사용 여부)은 소비자마다 다르기 때문이다.
    그래서 여기에는 표·상수·미리 컴파일한 struct 만 들어간다.

    표준 라이브러리만 쓴다(numpy 도 안 쓴다). 어디서든 import 되어야 한다.
    """
    Q = chr(34) * 3   # 생성물 안의 docstring 구분자. 이 파일 안에서 중첩되지 않게 조립한다.
    version = data["version"]
    module_id = data["module_id"]
    endian = data.get("endian", "little")
    prefix, chunks = build_struct_chunks(flat_fields, endian)
    scalars = expand_scalars(flat_fields)

    mid_hex = f"0x{module_id:02X}" if isinstance(module_id, int) else str(module_id)
    gw = max(len(c[1]) for c in chunks)

    L = []
    L.append(f"# AUTO-GENERATED from xm_total_data.yaml v{version} — DO NOT EDIT MANUALLY")
    L.append(f"# Layout fingerprint: {fingerprint} (sha256 of the canonical descriptor list)")
    L.append("# 생성기: Extension_Module/tools/codegen/data_map/generate_data_map.py --py-out")
    L.append(Q)
    L.append("XM10 Total Data Packet (PhAI module_id " + mid_hex + ") — 디코드 맵.")
    L.append("")
    L.append("PhAI V2.2 프레임의 payload 를 이름 붙은 스칼라로 푸는 데 필요한 표다.")
    L.append("표준 라이브러리만 쓴다 — 어디서든 import 된다.")
    L.append("")
    L.append("    import xm_total_data_map as M")
    L.append("    raw = M.PACKET_STRUCT.unpack(payload)      # " + str(len(scalars)) + " 개 스칼라")
    L.append("    for name, v, mul, div in zip(M.SCALAR_NAMES, raw, M.SCALAR_MUL, M.SCALAR_DIV):")
    L.append("        phys = v * mul if mul is not None else (v / div if div is not None else v)")
    L.append("")
    L.append("스케일 공식(YAML `scale_formula`)은 아래 두 배열로 미리 접어 두었다:")
    L.append("  none            -> mul=None, div=None      (raw 그대로)")
    L.append("  divide          -> div=scale               (phys = raw / scale)")
    L.append("  multiply_divide -> mul=scale/32768         (phys = raw * scale / 32768)")
    L.append("")
    L.append("`multiply_divide` 를 곱셈 하나로 접어도 값이 달라지지 않는다 — 분모 32768 이")
    L.append("2의 거듭제곱이라 scale/32768 이 이진 부동소수점에서 **정확**하고, 반올림이")
    L.append("한 번뿐인 것도 그대로다. `divide` 는 그렇지 않아서(1/100 은 부정확) 나눗셈을")
    L.append("그대로 남겼다 — 참조 구현(TypeScript / PhAI Studio)과 같은 값을 내기 위해서다.")
    L.append(Q)
    L.append("from __future__ import annotations")
    L.append("")
    L.append("import struct")
    L.append("from typing import NamedTuple, Optional, Tuple")
    L.append("")
    L.append(f"DATA_MAP_VERSION = {version!r}")
    L.append(f"DATA_MAP_FINGERPRINT = {fingerprint!r}")
    L.append(f"TOTAL_PACKET_SIZE = {total_size}")
    L.append(f"PACKET_ENDIAN = {endian!r}")
    L.append(f"NUM_DESCRIPTORS = {len(flat_fields)}")
    L.append(f"NUM_SCALARS = {len(scalars)}")
    L.append("")
    L.append("# `multiply_divide` 의 분모. YAML 주석(scale_formula 설명)이 정의하는 상수다 —")
    L.append("# YAML 데이터에는 없으므로 생성기가 알고 있어야 한다.")
    L.append("MULTIPLY_DIVIDE_DENOM = 32768")
    L.append("")
    L.append(f"MODULE_ID_TOTAL = {mid_hex}")
    L.append("MODULE_ID_LINK_HEALTH = 0xED")
    L.append("MODULE_ID_SCHEMA_DESC = 0xEE")
    L.append("MODULE_ID_USER_META = 0xEF")
    L.append("MODULE_ID_USER_CUSTOM_START = 0xF0")
    L.append("MODULE_ID_USER_CUSTOM_END = 0xFE")
    L.append("")
    L.append("TYPE_SIZE = {")
    for tname, tinfo in TYPE_INFO.items():
        L.append(f"    {tname!r}: {tinfo['size']},")
    L.append("}")
    L.append("")
    L.append("")
    L.append("class ChannelDef(NamedTuple):")
    L.append(f"    {Q}YAML 필드 하나. 배열은 펼치지 않고 count 로 남는다 — C 구조체와 1:1.{Q}")
    L.append("    offset: int")
    L.append("    name: str")
    L.append("    type: str")
    L.append("    count: int")
    L.append("    scale: float")
    L.append("    scale_formula: str")
    L.append("    unit: str")
    L.append("    group: str")
    L.append("")
    L.append("")
    L.append("class ScalarDef(NamedTuple):")
    L.append(f"    {Q}펼친 스칼라 하나. PACKET_STRUCT.unpack() 결과와 순서가 1:1 이다.{Q}")
    L.append("    name: str")
    L.append("    group: str")
    L.append("    unit: str")
    L.append("    type: str")
    L.append("    mul: Optional[float]")
    L.append("    div: Optional[float]")
    L.append("")
    L.append("")
    L.append(f"# --- descriptors ({len(flat_fields)}) — C 구조체 필드와 1:1 ---")
    L.append("TOTAL_DATA_MAP: Tuple[ChannelDef, ...] = (")
    for f in flat_fields:
        L.append("    ChannelDef(%3d, %-28s %-10s %2d, %-10s %-18s %-12s %s)," % (
            f["offset"], repr(f["name"]) + ",", repr(f["type"]) + ",", f["count"],
            repr(f["scale"]) + ",", repr(f["scaleFormula"]) + ",",
            repr(f["unit"]) + ",", repr(f["group"])))
    L.append(")")
    L.append("")
    L.append("# --- struct 포맷 — 그룹 경계를 살려 둔다(사람이 대조할 수 있게) ---")
    L.append("PACKET_STRUCT_FORMAT = (")
    L.append(f"    {prefix!r}")
    for chunk, gname in chunks:
        L.append(f"    {chunk!r:<{gw + 4}}  # {gname}")
    L.append(")")
    L.append("PACKET_STRUCT = struct.Struct(PACKET_STRUCT_FORMAT)")
    L.append("")
    L.append(f"# --- scalars ({len(scalars)}) — unpack() 순서 그대로 ---")
    L.append("SCALARS: Tuple[ScalarDef, ...] = (")
    for s in scalars:
        if s["formula"] == "multiply_divide":
            mul, div = repr(float(s["scale"]) / 32768.0), "None"
        elif s["formula"] == "divide":
            mul, div = "None", repr(float(s["scale"]))
        else:
            mul, div = "None", "None"
        L.append("    ScalarDef(%-34s %-16s %-12s %-10s %-24s %s)," % (
            repr(s["name"]) + ",", repr(s["group"]) + ",", repr(s["unit"]) + ",",
            repr(s["type"]) + ",", mul + ",", div))
    L.append(")")
    L.append("")
    L.append("SCALAR_NAMES: Tuple[str, ...] = tuple(s.name for s in SCALARS)")
    L.append("SCALAR_GROUPS: Tuple[str, ...] = tuple(s.group for s in SCALARS)")
    L.append("SCALAR_UNITS: Tuple[str, ...] = tuple(s.unit for s in SCALARS)")
    L.append("SCALAR_MUL: Tuple[Optional[float], ...] = tuple(s.mul for s in SCALARS)")
    L.append("SCALAR_DIV: Tuple[Optional[float], ...] = tuple(s.div for s in SCALARS)")
    L.append("")
    L.append("")
    L.append("# 생성물이 스스로 어긋났는지 import 시점에 잡는다. assert 가 아닌 이유는")
    L.append("# `python -O` 가 assert 를 지우기 때문 — 이 검사는 지워지면 안 된다.")
    L.append("if PACKET_STRUCT.size != TOTAL_PACKET_SIZE:")
    L.append("    raise RuntimeError(")
    L.append(f"        {'struct size %d != TOTAL_PACKET_SIZE %d'!r} % (PACKET_STRUCT.size, TOTAL_PACKET_SIZE))")
    L.append("if len(SCALARS) != NUM_SCALARS or len(TOTAL_DATA_MAP) != NUM_DESCRIPTORS:")
    L.append("    raise RuntimeError(")
    L.append(f"        {'table length mismatch: %d scalars / %d descriptors'!r}")
    L.append("        % (len(SCALARS), len(TOTAL_DATA_MAP)))")
    L.append("")
    return "\n".join(L)


# =============================================================================
# Main
# =============================================================================
def _emit(path, content, label, args):
    """산출물 하나를 쓰거나(기본) 대조한다(--check). 반환: 0 정상 / 1 불일치."""
    if args.dry_run and not path:
        print(f"\n=== {label.upper()} ===")
        print(content)
        return 0
    if not path:
        return 0

    if args.check:
        if not os.path.exists(path):
            print(f"[CHECK-FAIL] {label}: 파일 없음 — {path}")
            return 1
        with open(path, "r", encoding="utf-8") as f:
            on_disk = f.read()
        # 줄바꿈 정규화 후 비교. 워킹트리는 autocrlf 로 CRLF, git blob 과 리눅스 CI 는 LF —
        # 바이트로 비교하면 플랫폼 때문에 거짓 실패한다. 검사 대상은 내용이지 EOL 이 아니다.
        if on_disk.splitlines() != content.splitlines():
            print(f"[CHECK-FAIL] {label}: YAML 과 불일치 — {path}")
            print("             재생성 필요: --check 를 빼고 같은 명령을 다시 실행")
            return 1
        print(f"[CHECK-OK] {label}: {path}")
        return 0

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"[OK] {label}: {path}")
    if args.dry_run:
        print(f"\n=== {label.upper()} ===")
        print(content)
    return 0


def main():
    # Windows 기본 콘솔(cp949)에서 생성물의 em dash 를 stdout 에 쓰다 UnicodeEncodeError 로
    # 죽던 경로를 막는다 — --dry-run 이 이 환경에서 아예 못 쓰였다(2026-09-08 리뷰 재현).
    # tools/release_sync.py 가 쓰는 것과 같은 방식.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(
        description="Generate C header, TypeScript types and a Python decode map from the YAML Data Map")
    parser.add_argument("yaml_file", help="Input YAML file (SSOT)")
    parser.add_argument("--c-out", help="Output C header path")
    parser.add_argument("--ts-out", help="Output TypeScript path")
    parser.add_argument("--py-out", help="Output Python decode map path")
    parser.add_argument("--dry-run", action="store_true", help="Print to stdout, don't write files")
    parser.add_argument("--check", action="store_true",
                        help="Don't write — compare existing outputs against the YAML "
                             "(exit 1 on mismatch). 손댄 생성물·stale 생성물 게이트.")
    args = parser.parse_args()

    data = load_yaml(args.yaml_file)
    groups = data.get("groups", [])
    custom_types = data.get("types", {})

    # Calculate total size
    total_size = 0
    for g in groups:
        total_size += calc_group_size(g, custom_types)

    # Flatten
    flat_fields, flat_size = flatten_fields(groups, custom_types)
    assert flat_size == total_size, f"Flatten size mismatch: {flat_size} != {total_size}"

    fingerprint = compute_fingerprint(flat_fields, total_size)
    scalar_count = sum(f["count"] for f in flat_fields)

    print(f"[INFO] Packet: {data['packet_name']}, Total: {total_size}B, "
          f"Module ID: {data['module_id']}, Map v{data['version']}")
    print(f"[INFO] Layout fingerprint: {fingerprint}")
    print(f"[INFO] {len(flat_fields)} descriptors -> {scalar_count} scalars, {total_size}B total")

    rc = 0
    rc |= _emit(args.c_out, generate_c_header(data, custom_types, total_size, fingerprint),
                "C header", args)
    rc |= _emit(args.ts_out, generate_ts(data, flat_fields, total_size, fingerprint),
                "TypeScript", args)
    rc |= _emit(args.py_out, generate_py(data, flat_fields, total_size, fingerprint),
                "Python map", args)

    if args.check and rc:
        print("[CHECK] 실패 — 생성물이 YAML SSOT 와 다르다.")
    return rc


if __name__ == "__main__":
    sys.exit(main())
