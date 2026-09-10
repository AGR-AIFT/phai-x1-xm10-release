#!/usr/bin/env python3
"""`.xmlog` v1 — append-only 타입드 저널. 읽기/쓰기/복구.

무엇을 위한 파일인가
--------------------
USB-CDC 로 들어온 것을 **해석하기 전에 먼저 안전하게 눕히기** 위한 포맷이다.
CSV 는 해석의 결과물이라, 스키마를 모르면 아무것도 못 적는다. 그런데 스키마는
데이터보다 늦게 올 수 있고(FW 가 Control_Loop 을 먼저 돌린다), 아예 안 올 수도 있다.

그래서 규약이 이렇다:

  * CRC 를 통과한 데이터는 **스키마가 없어도 즉시 기록한다** (activation_id=0).
  * 해석은 나중에 소급해서 한다. CSV 는 이 파일에서 뽑는다.
  * 손실은 숨기지 않고 GAP 레코드로 **파일 안에** 남긴다.

바이트 ABI 는 PLAN-20260908-usb-cdc-schema-logging.md 4.6 이 SSOT 다.
이 파일은 그 표를 코드로 옮긴 것이고, `test_xmlog.py` 의 **손으로 적은 골든 바이트**가
양쪽이 일치하는지 감시한다 (구현이 만든 것끼리 비교하면 같이 틀린다).

왜 append-only 인가
-------------------
rev1 은 "사전할당 + footerless 선형 복구" 였는데 둘이 양립하지 않았다. 사전할당이
EOF 를 늘리면 아직 안 쓴 zero tail 이 `rec_type=0, len=0` 인 정상 레코드처럼 보여서,
"마지막 유효 레코드가 어디까지인가" 를 증명할 방법이 없어진다.
모든 섹션을 타입드 레코드로 통일하고 앞에서부터 순차 검증한다 — 첫 실패에서 멈추면
그 앞은 전부 유효다. 그래서 CHECKPOINT 레코드도 필요 없다(파서가 스스로 안다).

CRC 규약 — skip 방식
--------------------
`rec_crc32` 는 자기 자신(offset 12..15)만 건너뛰고 나머지를 이어붙여 계산한다.
zero-fill 이 아니다 — zero-fill 은 "0 을 채운 임시 버퍼" 라는 중간 상태를 요구해
스트리밍 writer 에 불리하다. CRC-32/ISO-HDLC = `zlib.crc32`.
"""
from __future__ import annotations

import os
import struct
import time
import zlib
from typing import BinaryIO, Iterator, NamedTuple, Optional

# =============================================================================
# 상수 — PLAN 4.6 표 그대로
# =============================================================================

FILE_MAGIC = b"XMLOG\x00\x00\x00"        # 8 B, 디스크 바이트 그대로
REC_MAGIC = b"XMR1"                       # 4 B. 정수 0x584D5231 이 아니다 (LE 로 쓰면 "1RMX")
FORMAT_VERSION = 1
HEADER_SIZE = 32
REC_HEADER_SIZE = 16

REC_SESSION = 1
REC_SCHEMA_ACTIVATION = 2
REC_DATA = 3
REC_GAP = 4

REC_TYPE_NAMES = {
    REC_SESSION: "SESSION",
    REC_SCHEMA_ACTIVATION: "SCHEMA_ACTIVATION",
    REC_DATA: "DATA",
    REC_GAP: "GAP",
}

# 타입별 header_rest_len — 고정값이다. 다르면 그 레코드는 무효.
HEADER_REST_LEN = {
    REC_SESSION: 72,
    REC_SCHEMA_ACTIVATION: 12,
    REC_DATA: 16,
    REC_GAP: 16,
}

GAP_SEQ = 1          # 와이어에서 seq 가 건너뛰었다
GAP_QUEUE_OVERFLOW = 2   # PC 가 못 따라가 버렸다 — 케이블이 아니라 로컬 지연
GAP_LINK_RESET = 3       # 연결이 끊겼다 붙었다

GAP_REASON_NAMES = {
    GAP_SEQ: "seq gap",
    GAP_QUEUE_OVERFLOW: "queue overflow",
    GAP_LINK_RESET: "link reset",
}

# struct 포맷 (전부 리틀엔디안, 필드 단위 인코딩 — C 구조체 raw memcpy 금지)
_FILE_HDR = struct.Struct("<8sHHIQII")            # magic, ver, hdr_size, crc, create_ns, rsv0, rsv1
_REC_HDR = struct.Struct("<4sBBHII")              # magic, type, flags, hdr_rest_len, payload_len, crc
_SESSION_HDR = struct.Struct("<24s24s8sIIQ")      # serial, fw_build, map_ver, boot_epoch, link_epoch, host_ns
_ACTIVATION_HDR = struct.Struct("<HBBHHI")        # act_id, module_id, proto_ver, struct_size, rsv, schema_crc32
_DATA_HDR = struct.Struct("<HBBHHQ")              # act_id, module_id, rsv, seq_id, rsv2, pc_time_us
_GAP_HDR = struct.Struct("<BBHHHII")              # reason, rsv, from_seq, to_seq, rsv2, lost_count, rsv3

for _s, _n, _want in ((_FILE_HDR, "FileHeader", 32),
                      (_REC_HDR, "RecordHeader", 16),
                      (_SESSION_HDR, "SESSION", 72),
                      (_ACTIVATION_HDR, "SCHEMA_ACTIVATION", 12),
                      (_DATA_HDR, "DATA", 16),
                      (_GAP_HDR, "GAP", 16)):
    if _s.size != _want:
        raise RuntimeError("%s struct size %d != %d (PLAN 4.6 표와 어긋남)"
                           % (_n, _s.size, _want))


def _fixed(text, width: int) -> bytes:
    """ASCII 고정폭, NUL 패딩. 넘치면 자르되 **NUL 종료를 보장**한다.

    PLAN 4.0: NUL 이 없으면 무효로 간주한다 — 그래서 잘라낼 때 마지막 한 칸을 비운다.
    """
    if text is None:
        return b"\x00" * width
    if isinstance(text, str):
        raw = text.encode("ascii", "replace")
    else:
        raw = bytes(text)
    if len(raw) >= width:
        raw = raw[:width - 1]
    return raw + b"\x00" * (width - len(raw))


def _unfixed(raw: bytes) -> str:
    """NUL 종료 고정폭 문자열을 파이썬 문자열로. NUL 이 없으면 무효라 빈 문자열."""
    i = raw.find(b"\x00")
    if i < 0:
        return ""
    return raw[:i].decode("ascii", "replace")


def _crc_skip_own(buf: bytes, crc_off: int = 12, crc_len: int = 4) -> int:
    """자기 자신 4바이트만 건너뛴 CRC-32/ISO-HDLC."""
    return zlib.crc32(buf[:crc_off] + buf[crc_off + crc_len:]) & 0xFFFFFFFF


# =============================================================================
# 인코딩 — 순수 함수. writer 없이도 바이트를 만들 수 있어야 시험이 쉽다.
# =============================================================================

def encode_file_header(create_unix_ns: Optional[int] = None) -> bytes:
    if create_unix_ns is None:
        create_unix_ns = time.time_ns()
    buf = bytearray(_FILE_HDR.pack(
        FILE_MAGIC, FORMAT_VERSION, HEADER_SIZE, 0, create_unix_ns, 0, 0))
    struct.pack_into("<I", buf, 12, _crc_skip_own(bytes(buf)))
    return bytes(buf)


def encode_record(rec_type: int, type_header: bytes, payload: bytes = b"",
                  flags: int = 0) -> bytes:
    want = HEADER_REST_LEN.get(rec_type)
    if want is None:
        raise ValueError("unknown rec_type %r" % (rec_type,))
    if len(type_header) != want:
        raise ValueError("rec_type %d: type_header %d B != %d B"
                         % (rec_type, len(type_header), want))
    buf = bytearray(_REC_HDR.pack(REC_MAGIC, rec_type, flags,
                                  len(type_header), len(payload), 0))
    buf += type_header
    buf += payload
    struct.pack_into("<I", buf, 12, _crc_skip_own(bytes(buf)))
    return bytes(buf)


def encode_session(device_usb_serial="", fw_build_id="", total_data_map_version="",
                   boot_epoch=0, link_epoch=0, host_unix_ns=None) -> bytes:
    """SESSION — 이 파일이 무엇을 받아 적은 것인지.

    `total_data_map_version` 이 8 B 뿐이라 버전 문자열("2.8")은 들어가지만 지문
    (16자리)은 안 들어간다. 지문은 SESSION 이 아니라 **사이드카 메타**에 남긴다 —
    포맷을 늘리면 FW 와의 계약이 깨지므로 여기서 늘리지 않는다.
    """
    if host_unix_ns is None:
        host_unix_ns = time.time_ns()
    return encode_record(REC_SESSION, _SESSION_HDR.pack(
        _fixed(device_usb_serial, 24), _fixed(fw_build_id, 24),
        _fixed(total_data_map_version, 8),
        boot_epoch & 0xFFFFFFFF, link_epoch & 0xFFFFFFFF, host_unix_ns))


def encode_schema_activation(activation_id, module_id, schema_proto_ver,
                             struct_size, schema_crc32, schema_payload) -> bytes:
    """SCHEMA_ACTIVATION — payload 는 `0xEE` canonical 바이트 **그대로**.

    재해석해서 저장하지 않는다. 파서가 나중에 바뀌어도 원본이 남아 있어야
    "그때 FW 가 뭐라고 했는지" 를 되짚을 수 있다.
    """
    return encode_record(REC_SCHEMA_ACTIVATION, _ACTIVATION_HDR.pack(
        activation_id & 0xFFFF, module_id & 0xFF, schema_proto_ver & 0xFF,
        struct_size & 0xFFFF, 0, schema_crc32 & 0xFFFFFFFF), bytes(schema_payload))


def encode_data(activation_id, module_id, seq_id, pc_time_us, payload) -> bytes:
    """DATA — activation_id 0 은 "스키마 미상". 버리지 않고 그대로 적는다."""
    return encode_record(REC_DATA, _DATA_HDR.pack(
        activation_id & 0xFFFF, module_id & 0xFF, 0,
        seq_id & 0xFFFF, 0, pc_time_us & 0xFFFFFFFFFFFFFFFF), bytes(payload))


def encode_gap(reason, from_seq, to_seq, lost_count) -> bytes:
    return encode_record(REC_GAP, _GAP_HDR.pack(
        reason & 0xFF, 0, from_seq & 0xFFFF, to_seq & 0xFFFF, 0,
        lost_count & 0xFFFFFFFF, 0))


# =============================================================================
# 디코딩
# =============================================================================

class FileHeader(NamedTuple):
    format_version: int
    header_size: int
    create_unix_ns: int


class Record(NamedTuple):
    rec_type: int
    flags: int
    offset: int          # 파일 내 시작 오프셋 — 진단·복구 보고용
    total_len: int
    fields: dict         # 타입별 헤더를 푼 것
    payload: bytes


class LogError(Exception):
    """복구 파서가 멈춘 이유. 어디서 왜 멈췄는지 담는다."""

    def __init__(self, message, offset: int, recoverable: bool = True):
        super().__init__(message)
        self.offset = offset
        self.recoverable = recoverable


def decode_file_header(buf: bytes) -> FileHeader:
    if len(buf) < HEADER_SIZE:
        raise LogError("파일이 헤더보다 짧다 (%d B)" % len(buf), 0, recoverable=False)
    magic, ver, hdr_size, crc, create_ns, _r0, _r1 = _FILE_HDR.unpack_from(buf, 0)
    if magic != FILE_MAGIC:
        raise LogError("magic 불일치 %r — .xmlog 파일이 아니다" % (magic,), 0, recoverable=False)
    if crc != _crc_skip_own(buf[:HEADER_SIZE]):
        raise LogError("FileHeader CRC 불일치", 0, recoverable=False)
    if ver != FORMAT_VERSION:
        raise LogError("format_version %d — 이 리더는 %d 만 안다" % (ver, FORMAT_VERSION),
                       0, recoverable=False)
    if hdr_size != HEADER_SIZE:
        raise LogError("header_size %d != %d" % (hdr_size, HEADER_SIZE), 0, recoverable=False)
    return FileHeader(ver, hdr_size, create_ns)


def _decode_type_header(rec_type: int, raw: bytes) -> dict:
    if rec_type == REC_SESSION:
        ser, fw, mv, boot, link, host_ns = _SESSION_HDR.unpack(raw)
        return {"device_usb_serial": _unfixed(ser), "fw_build_id": _unfixed(fw),
                "total_data_map_version": _unfixed(mv), "boot_epoch": boot,
                "link_epoch": link, "host_unix_ns": host_ns}
    if rec_type == REC_SCHEMA_ACTIVATION:
        act, mid, pv, ssz, _rsv, crc = _ACTIVATION_HDR.unpack(raw)
        return {"activation_id": act, "module_id": mid, "schema_proto_ver": pv,
                "struct_size": ssz, "schema_crc32": crc}
    if rec_type == REC_DATA:
        act, mid, _rsv, seq, _rsv2, pc_us = _DATA_HDR.unpack(raw)
        return {"activation_id": act, "module_id": mid, "seq_id": seq,
                "pc_time_us": pc_us}
    if rec_type == REC_GAP:
        reason, _rsv, frm, to, _rsv2, lost, _rsv3 = _GAP_HDR.unpack(raw)
        return {"reason": reason, "reason_name": GAP_REASON_NAMES.get(reason, "?"),
                "from_seq": frm, "to_seq": to, "lost_count": lost}
    raise LogError("unknown rec_type %d" % rec_type, 0)


def decode_record(buf: bytes, offset: int) -> Record:
    """offset 위치의 레코드 하나. 실패하면 LogError — 호출자가 거기서 멈춘다."""
    if offset + REC_HEADER_SIZE > len(buf):
        raise LogError("레코드 헤더가 잘렸다", offset)
    magic, rtype, flags, hdr_rest, plen, crc = _REC_HDR.unpack_from(buf, offset)
    if magic != REC_MAGIC:
        raise LogError("rec_magic %r 불일치" % (magic,), offset)
    want = HEADER_REST_LEN.get(rtype)
    if want is None:
        raise LogError("unknown rec_type %d" % rtype, offset)
    if hdr_rest != want:
        raise LogError("rec_type %d: header_rest_len %d != %d" % (rtype, hdr_rest, want),
                       offset)
    total = REC_HEADER_SIZE + hdr_rest + plen
    if offset + total > len(buf):
        raise LogError("레코드가 잘렸다 (필요 %d B, 남은 %d B)"
                       % (total, len(buf) - offset), offset)
    whole = buf[offset:offset + total]
    if crc != _crc_skip_own(whole):
        raise LogError("rec_crc32 불일치", offset)
    fields = _decode_type_header(rtype, whole[REC_HEADER_SIZE:REC_HEADER_SIZE + hdr_rest])
    return Record(rtype, flags, offset, total, fields, whole[REC_HEADER_SIZE + hdr_rest:])


class ScanResult(NamedTuple):
    header: FileHeader
    records: list
    valid_bytes: int          # 여기까지는 확실히 유효하다
    stopped_reason: Optional[str]
    trailing_bytes: int       # 버려진 꼬리 (부분 기록/크래시 흔적)


def scan(buf: bytes, stop_on_error: bool = True) -> ScanResult:
    """앞에서부터 순차 검증. **첫 실패에서 멈춘다** — 그 앞은 전부 유효다.

    크래시 복구가 이 함수 하나로 끝난다. 마지막 쓰기가 중간에 끊겼으면 그 레코드
    하나만 거부되고 나머지는 살아난다. 별도 체크포인트가 필요 없는 이유다.
    """
    header = decode_file_header(buf)
    records = []
    off = header.header_size
    reason = None
    first_checked = False
    while off < len(buf):
        try:
            rec = decode_record(buf, off)
        except LogError as e:
            reason = "%s @%d" % (e, e.offset)
            if stop_on_error:
                break
            raise
        if not first_checked:
            first_checked = True
            if rec.rec_type != REC_SESSION:
                # PLAN 4.6 "SESSION-first 문법": 첫 유효 레코드가 SESSION 이 아니면
                # 파일 전체를 거부한다 — 잘린 파일의 중간을 시작점으로 오인하지 않기 위해서다.
                raise LogError(
                    "첫 레코드가 SESSION 이 아니다 (%s) — 파일 중간을 가리키고 있을 수 있다"
                    % REC_TYPE_NAMES.get(rec.rec_type, rec.rec_type),
                    rec.offset, recoverable=False)
        records.append(rec)
        off += rec.total_len
    return ScanResult(header, records, off, reason, len(buf) - off)


def read_file(path) -> ScanResult:
    with open(path, "rb") as f:
        return scan(f.read())


def iter_records(path) -> Iterator[Record]:
    for r in read_file(path).records:
        yield r


# =============================================================================
# Writer — append-only. 예외 없이 단순하게 유지한다.
# =============================================================================

class XmLogWriter:
    """append-only writer.

    flush 정책만 조심하면 된다. 매 레코드 fsync 는 1 kHz 에서 불가능하고,
    아예 안 하면 크래시 때 잃는 양이 OS 버퍼 크기만큼이다. 기본은 배치 flush 이고,
    **어차피 복구 파서가 부분 레코드를 하나만 버리므로** 잃는 것은 항상
    "마지막 몇 개" 지 파일 전체가 아니다.
    """

    __slots__ = ("_f", "path", "records_written", "bytes_written",
                 "_flush_every", "_since_flush", "_next_activation_id",
                 "_act_by_crc", "_session_written")

    def __init__(self, path, create_unix_ns: Optional[int] = None,
                 flush_every: int = 256):
        self.path = str(path)
        d = os.path.dirname(os.path.abspath(self.path))
        if d:
            os.makedirs(d, exist_ok=True)
        self._f: BinaryIO = open(self.path, "wb")
        self._f.write(encode_file_header(create_unix_ns))
        self.records_written = 0
        self.bytes_written = HEADER_SIZE
        self._flush_every = max(1, int(flush_every))
        self._since_flush = 0
        self._next_activation_id = 1
        # (module_id, schema_crc32) -> activation_id. PLAN 4.6 "중복 억제" —
        # DTR 재연결마다 같은 스키마가 다시 와도 레코드를 새로 쓰지 않는다.
        self._act_by_crc = {}
        self._session_written = False

    # -- 저수준 ----------------------------------------------------------
    def write_raw(self, record_bytes: bytes) -> int:
        # PLAN 4.6 SESSION-first: 파일의 첫 레코드는 반드시 SESSION 이다.
        # reader 가 그걸 전제로 거부 판정을 하므로 writer 쪽에서도 막는다 —
        # 안 그러면 아무도 못 읽는 파일이 조용히 만들어진다.
        if not self._session_written:
            if record_bytes[4:5] != bytes([REC_SESSION]):
                raise ValueError(
                    "첫 레코드는 SESSION 이어야 한다 (PLAN 4.6 SESSION-first). "
                    "XmLogWriter 를 연 직후 .session(...) 을 먼저 부를 것.")
            self._session_written = True
        self._f.write(record_bytes)
        self.records_written += 1
        self.bytes_written += len(record_bytes)
        self._since_flush += 1
        if self._since_flush >= self._flush_every:
            self.flush()
        return len(record_bytes)

    def flush(self) -> None:
        self._f.flush()
        self._since_flush = 0

    def sync(self) -> None:
        """OS 버퍼까지 밀어낸다. 비싸다 — 세션 경계나 명시 요청에만."""
        self._f.flush()
        os.fsync(self._f.fileno())
        self._since_flush = 0

    def close(self) -> None:
        if self._f and not self._f.closed:
            try:
                self._f.flush()
            finally:
                self._f.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    # -- 레코드 ----------------------------------------------------------
    def session(self, **kw) -> int:
        return self.write_raw(encode_session(**kw))

    def schema_activation(self, module_id, schema_proto_ver, struct_size,
                          schema_crc32, schema_payload) -> int:
        """activation_id 는 **파일이 소유**한다 — writer 가 1부터 발급.

        schema_crc32 는 와이어(FW)가 소유한다. 폭이 달라(u16 vs u32) 서로를
        대신할 수 없어서 역할을 나눈 것이다.
        반환값은 발급된 activation_id (뒤따르는 DATA 가 이걸 참조한다).
        """
        # 중복 억제 (PLAN 4.6). 같은 (module, crc) 는 레코드를 다시 쓰지 않고
        # 기존 id 를 재사용한다 — 재연결 10회에 4 KB 씩 불어나는 것을 막는다.
        cached = self._act_by_crc.get((module_id, schema_crc32))
        if cached is not None:
            return cached

        if len(self._act_by_crc) >= 0xFFFE:
            # u16 을 다 썼다. 재사용하면 같은 파일 안에서 서로 다른 스키마가 같은 id 를
            # 갖게 되어 DATA 의 참조가 모호해진다 — 조용히 망가지느니 거부한다.
            raise ValueError("activation_id 를 다 썼다 (65534개) — 새 파일을 열 것")
        act = self._next_activation_id
        self._next_activation_id += 1
        self.write_raw(encode_schema_activation(
            act, module_id, schema_proto_ver, struct_size, schema_crc32, schema_payload))
        self._act_by_crc[(module_id, schema_crc32)] = act
        return act

    def data(self, module_id, seq_id, pc_time_us, payload, activation_id=0) -> int:
        return self.write_raw(encode_data(activation_id, module_id, seq_id,
                                          pc_time_us, payload))

    def gap(self, reason, from_seq, to_seq, lost_count) -> int:
        return self.write_raw(encode_gap(reason, from_seq, to_seq, lost_count))


# =============================================================================
# 요약 — 사람이 파일을 열어보지 않고 무슨 일이 있었는지 알게
# =============================================================================

def summarize(res: ScanResult) -> str:
    counts = {}
    for r in res.records:
        counts[r.rec_type] = counts.get(r.rec_type, 0) + 1
    lost = sum(r.fields.get("lost_count", 0) for r in res.records if r.rec_type == REC_GAP)
    parts = ["records=%d" % len(res.records)]
    for t in (REC_SESSION, REC_SCHEMA_ACTIVATION, REC_DATA, REC_GAP):
        if counts.get(t):
            parts.append("%s=%d" % (REC_TYPE_NAMES[t], counts[t]))
    if lost:
        parts.append("lost=%d" % lost)
    parts.append("valid=%dB" % res.valid_bytes)
    if res.trailing_bytes:
        parts.append("trailing=%dB(거부)" % res.trailing_bytes)
    if res.stopped_reason:
        parts.append("stopped: %s" % res.stopped_reason)
    return "  ".join(parts)
