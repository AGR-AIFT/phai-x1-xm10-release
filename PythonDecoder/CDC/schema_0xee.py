#!/usr/bin/env python3
"""`0xEE` SCHEMA_DESC — FW 가 보내는 이진 스키마의 파서/재조립기.

**FW 는 아직 이걸 보내지 않는다** (`0xEE` 송신 코드 0건, 2026-09-10 실측).
그런데도 지금 쓰는 이유는, 바이트 문법이 설계 문서에 이미 동결돼 있고
(PLAN-20260908-usb-cdc-schema-logging.md §4.0~4.1), **호스트 쪽이 먼저 준비돼 있어야**
FW 가 보내기 시작할 때 아무것도 안 고치고 붙기 때문이다.
골든 벡터로 검증해 두면 FW 구현자가 맞춰야 할 대상이 생긴다는 이점도 있다.

무엇을 담고 있나
----------------
사용자가 C 로 정의한 struct 를 PC 가 **무설정으로** 풀 수 있게 하는 정보다 —
필드 이름·단위·타입·배열길이·구조체 내 offset·스케일. 지금의 `0xEF` JSON 은
이름/단위만 있어서 혼합 타입 struct 를 기술하지 못한다. 그 차이가 이 포맷의 존재 이유다.

바이트 문법 (PLAN §4.1, 리틀엔디안 · 필드 단위 인코딩)
------------------------------------------------------
    Header 24 B
      u8   schema_proto_ver     (=1)
      u8   target_module_id     (0xF0~0xFE)
      u16  struct_size
      u32  schema_crc32         FieldRecord 전체의 CRC-32/ISO-HDLC (헤더 제외)
      u8   field_count_total
      u8   frame_index          0-based
      u8   frame_count
      u8   reserved             (=0)
      char struct_name[12]      ASCII, NUL 종료

    FieldRecord 32 B × N   (N <= 31, frame_count <= 3, 총 <= 93 필드)
      char name[16]
      char unit[8]
      u8   type_tag
      u8   array_len            1~255
      u16  struct_offset        offsetof
      f32  scale                물리값 = raw * scale

offset 을 명시적으로 들고 다니는 이유
-------------------------------------
C 구조체에는 **정렬 패딩**이 있다. 필드를 순서대로 이어붙인 하나의 struct 포맷 문자열로
푸는 방식은 패딩이 없을 때만 맞다 — 그래서 여기서는 필드마다 자기 offset 에서
따로 읽는다. 느려 보이지만 이게 유일하게 옳은 방식이다.
"""
from __future__ import annotations

import struct
import zlib
from typing import Dict, List, NamedTuple, Optional, Tuple

SCHEMA_PROTO_VER = 1

HEADER_SIZE = 24
FIELD_RECORD_SIZE = 32
MAX_FIELDS_PER_FRAME = 31          # (1020 - 24) // 32
MAX_FRAME_COUNT = 3                # PLAN rev3: 슬롯 4개 × 프래그 3개 = 모듈당 93 필드
MAX_FIELDS_TOTAL = 93
REASSEMBLY_TIMEOUT_S = 3.0

_HDR = struct.Struct("<BBHIBBBB12s")
_FLD = struct.Struct("<16s8sBBHf")

if _HDR.size != HEADER_SIZE:
    raise RuntimeError("Header struct %d != %d" % (_HDR.size, HEADER_SIZE))
if _FLD.size != FIELD_RECORD_SIZE:
    raise RuntimeError("FieldRecord struct %d != %d" % (_FLD.size, FIELD_RECORD_SIZE))

# type_tag -> (이름, struct 코드, 바이트 수)
TYPE_TAGS = {
    0:  ("f32", "f", 4),
    1:  ("f64", "d", 8),
    2:  ("i8", "b", 1),
    3:  ("u8", "B", 1),
    4:  ("i16", "h", 2),
    5:  ("u16", "H", 2),
    6:  ("i32", "i", 4),
    7:  ("u32", "I", 4),
    8:  ("i64", "q", 8),
    9:  ("u64", "Q", 8),
    10: ("bool", "B", 1),          # C 에서 1바이트. 값은 != 0 으로 해석한다.
}
BOOL_TAG = 10


class SchemaError(Exception):
    """스키마가 문법·불변식을 어겼다. 어긴 것을 그대로 말한다."""


def _fixed(text: str, width: int) -> bytes:
    """ASCII 고정폭, NUL 종료 보장. 비-ASCII 는 SchemaError.

    `.encode("ascii")` 를 errors 없이 부르면 UnicodeEncodeError 가 나는데, 그건 이 모듈의
    오류 타입이 아니라 호출자가 잡지 못한다. 여기서 SchemaError 로 바꿔 던진다.
    """
    try:
        raw = text.encode("ascii")
    except UnicodeEncodeError:
        raise SchemaError("문자열이 ASCII 가 아니다: %r" % (text,))
    if len(raw) >= width:
        raw = raw[:width - 1]
    return raw + b"\x00" * (width - len(raw))


def _unfixed(raw: bytes) -> str:
    """NUL 종료 고정폭 **ASCII**. 어기면 SchemaError.

    PLAN §4.0 이 ASCII 라고 못박았다. 예전에는 비-ASCII 바이트를 U+FFFD 로 **조용히
    치환**했는데, 그러면 나중에 CRC 재계산을 위해 다시 ASCII 로 인코딩할 때
    UnicodeEncodeError 가 나고 그건 SchemaError 가 아니라서 아무도 잡지 않는다 —
    수신 스레드가 죽고, 그런 프레임이 하나 섞인 `.xmlog` 는 영영 못 읽게 된다
    (2026-09-10 적대 감사 P0). 파싱 단계에서 거부하면 그 경로 자체가 사라진다.
    """
    i = raw.find(b"\x00")
    if i < 0:
        raise SchemaError("고정폭 문자열에 NUL 종료가 없다: %r" % (raw,))
    body = raw[:i]
    if any(b >= 0x80 for b in body):
        raise SchemaError("고정폭 문자열이 ASCII 가 아니다: %r" % (body,))
    return body.decode("ascii")


class FieldDef(NamedTuple):
    name: str
    unit: str
    type_tag: int
    array_len: int
    offset: int
    scale: float

    @property
    def type_name(self) -> str:
        return TYPE_TAGS[self.type_tag][0]

    @property
    def elem_size(self) -> int:
        return TYPE_TAGS[self.type_tag][2]

    @property
    def byte_len(self) -> int:
        return self.elem_size * self.array_len

    def scalar_names(self) -> List[str]:
        if self.array_len == 1:
            return [self.name]
        return ["%s[%d]" % (self.name, i) for i in range(self.array_len)]


class Fragment(NamedTuple):
    proto_ver: int
    module_id: int
    struct_size: int
    schema_crc32: int
    field_count_total: int
    frame_index: int
    frame_count: int
    struct_name: str
    fields: Tuple[FieldDef, ...]

    @property
    def key(self):
        """재조립 키 (PLAN §4.1). 네 값이 모두 같아야 같은 스키마다."""
        return (self.module_id, self.proto_ver, self.struct_size, self.schema_crc32)


def parse_fragment(payload: bytes) -> Fragment:
    """`0xEE` 프레임 payload 하나 -> Fragment. 문법 위반이면 SchemaError."""
    if len(payload) < HEADER_SIZE:
        raise SchemaError("payload %d B < 헤더 %d B" % (len(payload), HEADER_SIZE))

    (proto, mid, ssize, crc, fcount_total, fidx, fcount,
     reserved, sname_raw) = _HDR.unpack_from(payload, 0)

    if proto != SCHEMA_PROTO_VER:
        raise SchemaError("schema_proto_ver %d — 이 파서는 %d 만 안다" % (proto, SCHEMA_PROTO_VER))
    if reserved != 0:
        raise SchemaError("reserved 가 0 이 아니다 (%d)" % reserved)
    if not (0xF0 <= mid <= 0xFE):
        raise SchemaError("target_module_id 0x%02X 가 사용자 범위(0xF0~0xFE) 밖" % mid)
    if fcount == 0 or fcount > MAX_FRAME_COUNT:
        raise SchemaError("frame_count %d (1~%d)" % (fcount, MAX_FRAME_COUNT))
    if fidx >= fcount:
        raise SchemaError("frame_index %d >= frame_count %d" % (fidx, fcount))
    if fcount_total == 0 or fcount_total > MAX_FIELDS_TOTAL:
        raise SchemaError("field_count_total %d (1~%d)" % (fcount_total, MAX_FIELDS_TOTAL))
    struct_name = _unfixed(sname_raw)

    body = len(payload) - HEADER_SIZE
    # 와이어는 4바이트 단위로 패딩될 수 있다. 헤더 24 + 32N 은 항상 4의 배수라
    # 패딩이 붙지 않는 것이 정상이지만, 붙어 오더라도 남는 꼬리는 무시한다.
    n = body // FIELD_RECORD_SIZE
    if n == 0:
        raise SchemaError("FieldRecord 가 하나도 없다")
    if n > MAX_FIELDS_PER_FRAME:
        raise SchemaError("프래그먼트당 FieldRecord %d 개 > %d" % (n, MAX_FIELDS_PER_FRAME))

    fields = []
    for i in range(n):
        off = HEADER_SIZE + i * FIELD_RECORD_SIZE
        nm, un, tag, alen, soff, scale = _FLD.unpack_from(payload, off)
        if tag not in TYPE_TAGS:
            raise SchemaError("알 수 없는 type_tag %d (필드 %d)" % (tag, i))
        if alen == 0:
            raise SchemaError("array_len 0 (필드 %d) — 스칼라는 1 이다" % i)
        fields.append(FieldDef(_unfixed(nm), _unfixed(un), tag, alen, soff, scale))

    return Fragment(proto, mid, ssize, crc, fcount_total, fidx, fcount,
                    struct_name, tuple(fields))


def canonical_field_bytes(fields) -> bytes:
    """CRC 대상 — FieldRecord 들의 canonical 바이트를 순서대로 이어붙인 것(헤더 제외)."""
    out = bytearray()
    for f in fields:
        out += _FLD.pack(_fixed(f.name, 16), _fixed(f.unit, 8),
                         f.type_tag, f.array_len, f.offset, f.scale)
    return bytes(out)


def encode_fragment(module_id, struct_size, struct_name, fields,
                    frame_index, frame_count, field_count_total,
                    schema_crc32=None, proto_ver=SCHEMA_PROTO_VER) -> bytes:
    """Fragment 를 바이트로. 시험·골든 벡터·FW 구현 대조용.

    호스트가 스키마를 만들 일은 없지만, **파서만 있으면 시험을 쓸 수 없다** —
    무엇을 먹일지 만들 수 없기 때문이다. 인코더는 그래서 있다.
    """
    if schema_crc32 is None:
        schema_crc32 = zlib.crc32(canonical_field_bytes(fields)) & 0xFFFFFFFF
    hdr = _HDR.pack(proto_ver, module_id, struct_size, schema_crc32,
                    field_count_total, frame_index, frame_count, 0,
                    _fixed(struct_name, 12))
    return hdr + canonical_field_bytes(fields)


class Schema(NamedTuple):
    """완성된 스키마 — 재조립·CRC 검증까지 끝난 것."""
    module_id: int
    struct_name: str
    struct_size: int
    proto_ver: int
    schema_crc32: int
    fields: Tuple[FieldDef, ...]

    def scalar_names(self) -> List[str]:
        out = []
        for f in self.fields:
            out.extend(f.scalar_names())
        return out

    def decode(self, payload: bytes) -> Optional[list]:
        """필드마다 **자기 offset 에서** 읽는다 — C 정렬 패딩 때문에 이어붙이면 안 된다.

        payload 는 와이어에서 4바이트 배수로 패딩돼 struct_size 보다 클 수 있다.
        모자라면 None.
        """
        if len(payload) < self.struct_size:
            return None
        out = []
        for f in self.fields:
            code = TYPE_TAGS[f.type_tag][1]
            vals = struct.unpack_from("<%d%s" % (f.array_len, code), payload, f.offset)
            if f.type_tag == BOOL_TAG:
                out.extend(bool(v) for v in vals)
            elif f.scale == 1.0:
                out.extend(vals)
            else:
                out.extend(v * f.scale for v in vals)
        return out


def validate(schema: Schema) -> None:
    """구조체 불변식. 어기면 SchemaError — 조용히 잘못 푸느니 거부한다."""
    if not schema.fields:
        raise SchemaError("필드가 없다")
    if len(schema.fields) > MAX_FIELDS_TOTAL:
        raise SchemaError("필드 %d > %d" % (len(schema.fields), MAX_FIELDS_TOTAL))

    seen = []
    for f in schema.fields:
        end = f.offset + f.byte_len
        if end > schema.struct_size:
            raise SchemaError("필드 %r 가 struct_size 를 넘는다 (%d..%d > %d)"
                              % (f.name, f.offset, end, schema.struct_size))
        for (o2, e2, n2) in seen:
            if f.offset < e2 and o2 < end:
                raise SchemaError("필드 %r(%d..%d) 와 %r(%d..%d) 가 겹친다"
                                  % (f.name, f.offset, end, n2, o2, e2))
        seen.append((f.offset, end, f.name))

    names = [f.name for f in schema.fields]
    if len(set(names)) != len(names):
        dup = sorted({n for n in names if names.count(n) > 1})
        raise SchemaError("필드 이름 중복: %s" % ", ".join(dup))


class Reassembler:
    """프래그먼트를 모아 스키마를 완성한다 (PLAN §4.1 재조립 규약).

    * 키 = (module_id, proto_ver, struct_size, schema_crc32)
    * `frame_count` 비트맵을 유지하고 **전부 모인 뒤에만** CRC 를 검증한다
      (CRC 가 키의 일부라 순환 의존이 생길 것 같지만, 검증은 완성 후 한 번뿐이라 아니다)
    * 3초 안에 못 모으면 폐기
    * 같은 프래그먼트가 다시 오면 덮어쓴다
    * 키가 다른 프래그먼트가 오면 그 모듈의 진행 중 재조립을 폐기한다
    """

    __slots__ = ("_pending", "timeout", "completed", "discarded_timeout",
                 "discarded_key_change", "crc_failures", "invalid")

    def __init__(self, timeout_s: float = REASSEMBLY_TIMEOUT_S):
        self._pending: Dict[int, dict] = {}      # module_id -> 진행 상태
        self.timeout = timeout_s
        self.completed = 0
        self.discarded_timeout = 0
        self.discarded_key_change = 0
        self.crc_failures = 0
        self.invalid = 0

    def feed(self, payload: bytes, now: float) -> Optional[Schema]:
        """`0xEE` payload 하나. 스키마가 **완성된 순간에만** Schema 를 돌려준다."""
        try:
            frag = parse_fragment(payload)
        except SchemaError:
            self.invalid += 1
            raise

        self._expire(now)
        mid = frag.module_id
        st = self._pending.get(mid)

        if st is not None and st["key"] != frag.key:
            self.discarded_key_change += 1
            st = None
        if st is None:
            st = {"key": frag.key, "frag": frag, "parts": {}, "t0": now}
            self._pending[mid] = st

        # 재조립 키에 frame_count / field_count_total 이 없다. 같은 키인데 그 둘이
        # 다르면 완료 판정과 필드 수집이 어긋난다 — 여기서 따로 본다 (감사 #10).
        first = st["frag"]
        if (frag.frame_count != first.frame_count
                or frag.field_count_total != first.field_count_total):
            self.discarded_key_change += 1
            del self._pending[mid]
            raise SchemaError(
                "같은 키인데 frame_count/field_count_total 이 다르다: "
                "(%d,%d) vs (%d,%d)" % (first.frame_count, first.field_count_total,
                                        frag.frame_count, frag.field_count_total))

        st["parts"][frag.frame_index] = frag     # 중복은 덮어쓴다
        st["t0"] = now

        if len(st["parts"]) < frag.frame_count:
            return None

        fields = []
        for i in range(frag.frame_count):
            part = st["parts"].get(i)
            if part is None:
                return None
            fields.extend(part.fields)
        del self._pending[mid]

        if len(fields) != frag.field_count_total:
            self.invalid += 1
            raise SchemaError("필드 %d 개 모였는데 field_count_total 은 %d"
                              % (len(fields), frag.field_count_total))

        calc = zlib.crc32(canonical_field_bytes(fields)) & 0xFFFFFFFF
        if calc != frag.schema_crc32:
            self.crc_failures += 1
            raise SchemaError("schema_crc32 불일치: 계산 %08x != 수신 %08x"
                              % (calc, frag.schema_crc32))

        schema = Schema(mid, frag.struct_name, frag.struct_size, frag.proto_ver,
                        frag.schema_crc32, tuple(fields))
        validate(schema)
        self.completed += 1
        return schema

    def _expire(self, now: float) -> None:
        for mid in [m for m, st in self._pending.items()
                    if now - st["t0"] > self.timeout]:
            del self._pending[mid]
            self.discarded_timeout += 1

    def pending_modules(self) -> List[int]:
        return sorted(self._pending)

    def summary(self) -> str:
        return ("0xEE: completed=%d  timeout=%d  key-change=%d  crc-fail=%d  invalid=%d"
                % (self.completed, self.discarded_timeout, self.discarded_key_change,
                   self.crc_failures, self.invalid))
