#!/usr/bin/env python3
"""보드 없이 돌리는 **합성 USB-CDC 스트림**. 데모와 회귀 시험이 함께 쓴다.

여기서 만든 바이트는 FW 가 실제로 내보내는 것과 같은 규약이다 —
`[SOF][LEN][SEQ][MODULE][STATUS][payload][CRC16]` -> COBS -> `0x00`.
그래서 이걸 먹인 경로는 보드를 꽂았을 때와 **같은 코드 경로**다. 가짜 프레임 객체를
만들어 넣는 mock 이 아니다.

왜 시험 파일이 아니라 여기 있나
--------------------------------
`test_xmlog_chain.py` 안에 있던 빌더(`_wire`)는 시험에서만 쓸 수 있었다. 배포되는
실행파일(`xm10.py demo`)이 시험 모듈을 import 하면 PyInstaller 번들에 시험이 딸려
들어가고, 시험을 고치면 데모가 깨진다. 생성기를 여기 두고 양쪽이 import 한다.

이 스트림이 일부러 담고 있는 것
--------------------------------
현실에서 틀리기 쉬운 자리를 전부 한 번씩 밟는다.

* `0x20` Total Data — 365 B 구조체가 와이어에서는 **368 B**(LEN 이 4바이트 단위라
  0 패딩이 붙는다). 365 를 기대하는 디코더는 단위시험을 통과하고 보드에서 100% 실패한다.
* `0xF0` — **패딩 구멍이 둘 있는** 사용자 구조체. 필드를 이어붙인 struct 포맷
  문자열로 풀면 틀린다. `0xEE` 가 필드마다 `struct_offset` 을 들고 오는 이유다.
* **데이터가 스키마보다 먼저 온다.** FW 는 `Control_Loop()` 를 먼저 돌리고 주기 작업에서
  스키마를 보내므로 이 순서가 정상이다(PLAN 4.1). 사후 내보내기에서는 전부 풀려야 한다.
* `0xF1` — `0xEF` JSON 메타만 있는 채널. 이름은 알지만 **타입은 모른다** -> float32 가정.
* seq 구멍 하나 (프레임 손실), CRC 깨진 프레임 하나.
"""
from __future__ import annotations

import math
import struct
import zlib

import schema_0xee as EE
import xm_total_data_map as M
from frame_router import (
    PHAI_SOF, PHAI_MODULE_TOTAL_DATA, PHAI_MODULE_USER_META,
    crc16_ccitt, cobs_decode,
)


# 이 데모가 쓰는 모듈 ID — 사용자 대역(0xF0~0xFE) 안이다.
MODULE_TYPED = 0xF0        # 0xEE 로 타입까지 알려주는 채널
MODULE_META_ONLY = 0xF1    # 0xEF 로 이름만 알려주는 채널 (구형 경로)
MODULE_SCHEMA_DESC = 0xEE


# ---------------------------------------------------------------------------
# 사용자 구조체 — 데모의 주인공
# ---------------------------------------------------------------------------
# C 로 이렇게 쓴 것과 같다 (STM32 / ARM EABI 정렬):
#
#     typedef struct {
#         uint8_t  state;           /* off  0 */
#         uint8_t  contact;         /* off  1  (bool) */
#         /*       2 B padding    */
#         float    emg_rms;         /* off  4 */
#         int16_t  angle_x10;       /* off  8 */
#         /*       2 B padding    */
#         uint32_t tick;            /* off 12 */
#         float    torque[2];       /* off 16, 20 */
#     } DemoUser_t;                 /* sizeof = 24 */
#
# 패딩 구멍이 offset 2..3 과 10..11 두 곳이다. 필드를 이어붙인 "<BBfhIff" 로 풀면
# 두 번째 필드부터 전부 어긋난다 — 그래서 필드마다 offset 을 실어 보낸다.
DEMO_STRUCT_NAME = "DemoUser"
DEMO_STRUCT_SIZE = 24
DEMO_FIELDS = (
    EE.FieldDef("state",     "",     3,  1,  0, 1.0),    # u8
    EE.FieldDef("contact",   "",     EE.BOOL_TAG, 1, 1, 1.0),
    EE.FieldDef("emg_rms",   "mV",   0,  1,  4, 1.0),    # f32
    EE.FieldDef("angle_x10", "deg",  4,  1,  8, 0.1),    # i16, scale 0.1
    EE.FieldDef("tick",      "n",    7,  1, 12, 1.0),    # u32
    EE.FieldDef("torque",    "Nm",   0,  2, 16, 1.0),    # f32[2]
)

META_ONLY_JSON = ('[{"name":"Battery","unit":"V"},'
                  '{"name":"Load Cell","unit":"kg"},'
                  '{"name":"Ambient","unit":"degC"}]')


# ---------------------------------------------------------------------------
# 와이어 만들기
# ---------------------------------------------------------------------------
def cobs_encode(data: bytes) -> bytes:
    """COBS 인코더. `frame_router.cobs_decode` 의 역함수여야 한다.

    `test_frame_router.py` 에도 같은 기능의 인코더가 있다. 일부러 남겨 둔 **두 번째
    구현**이고, `test_demo_stream.py` 가 둘을 대조한다 — 하나가 틀리면 그 대조가 깨진다.
    생성물끼리 비교하는 시험은 둘 다 같은 방식으로 틀리면 통과하므로, 서로 다른 손이
    쓴 구현을 맞대는 편이 낫다(PLAN P2-2 독립 오라클).
    """
    out = bytearray(b"\x00")     # 첫 code 자리를 잡아 둔다
    code_at = 0
    run = 1
    for b in data:
        if b:
            out.append(b)
            run += 1
            if run < 0xFF:
                continue
        out[code_at] = run
        code_at = len(out)
        out.append(0)
        run = 1
    out[code_at] = run
    return bytes(out)


def wire_frame(seq_id: int, module_id: int, payload: bytes,
               status: int = 0, corrupt_crc: bool = False) -> bytes:
    """FW 규약 그대로 한 프레임. 반환값은 딜리미터(`0x00`)까지 포함한다.

    `LEN` 은 **4바이트 단위**라 payload 를 4의 배수로 올린다. 이 한 줄이 0x20 의
    365 -> 368 을 만든다.
    """
    padded = bytes(payload) + bytes((-len(payload)) % 4)
    if len(padded) // 4 > 255:
        raise ValueError("payload %d B 는 LEN(4바이트 단위, 최대 255)을 넘는다" % len(payload))
    body = bytearray([PHAI_SOF, len(padded) // 4,
                      seq_id & 0xFF, (seq_id >> 8) & 0xFF,
                      module_id, status])
    body.extend(padded)
    crc = crc16_ccitt(bytes(body))
    if corrupt_crc:
        crc ^= 0xFFFF          # 수신기가 버려야 하는 프레임
    body.append(crc & 0xFF)
    body.append((crc >> 8) & 0xFF)
    return cobs_encode(bytes(body)) + b"\x00"


def total_payload(values: dict) -> bytes:
    """0x20 Total Data 페이로드(365 B). 준 채널만 채우고 나머지는 0."""
    by_name = {d.name: d for d in M.TOTAL_DATA_MAP}
    code = {"uint8": "B", "int8": "b", "uint16": "H", "int16": "h",
            "uint32": "I", "int32": "i", "float32": "f"}
    buf = bytearray(M.TOTAL_PACKET_SIZE)
    for name, raw in values.items():
        d = by_name[name]
        struct.pack_into("<" + code[d.type], buf, d.offset, raw)
    return bytes(buf)


def typed_payload(state: int, contact: bool, emg_rms: float,
                  angle_x10: int, tick: int, torque) -> bytes:
    """`DemoUser_t` 한 개. **패딩 자리는 0 으로 남긴다** — C 가 그렇게 한다."""
    buf = bytearray(DEMO_STRUCT_SIZE)
    struct.pack_into("<B", buf, 0, state & 0xFF)
    struct.pack_into("<B", buf, 1, 1 if contact else 0)
    struct.pack_into("<f", buf, 4, emg_rms)
    struct.pack_into("<h", buf, 8, angle_x10)
    struct.pack_into("<I", buf, 12, tick & 0xFFFFFFFF)
    struct.pack_into("<ff", buf, 16, torque[0], torque[1])
    return bytes(buf)


def schema_frame_payload() -> bytes:
    """`DemoUser_t` 의 `0xEE` 스키마 — 6필드라 한 프래그먼트에 들어간다."""
    return EE.encode_fragment(
        MODULE_TYPED, DEMO_STRUCT_SIZE, DEMO_STRUCT_NAME, list(DEMO_FIELDS),
        frame_index=0, frame_count=1, field_count_total=len(DEMO_FIELDS))


def schema_crc32() -> int:
    return zlib.crc32(EE.canonical_field_bytes(list(DEMO_FIELDS))) & 0xFFFFFFFF


# ---------------------------------------------------------------------------
# 한 세션 만들기
# ---------------------------------------------------------------------------
class DemoExpectation:
    """이 스트림이 무엇을 담고 있는지 — 데모의 판정과 시험의 기대가 같은 출처를 본다."""

    __slots__ = ("frames_ok", "frames_crc_bad", "lost_frames", "gap_from", "gap_to",
                 "total_frames", "typed_frames", "meta_only_frames",
                 "typed_rows", "seq_max")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw[k])


def build_demo_session(cycles: int = 25, drop_at: int = 12, drop_len: int = 3):
    """데모용 한 세션치 와이어를 만든다.

    반환: `(stream_bytes, DemoExpectation)`

    프레임 순서는 FW 가 실제로 보내는 순서를 흉내낸다 —
    제어 루프가 먼저 돌기 때문에 **데이터가 스키마보다 앞선다**.
    """
    if cycles < 6:
        raise ValueError("cycles 는 최소 6 이어야 스키마·메타·구멍이 다 들어간다")

    stream = bytearray()
    seq = 0
    n_total = n_typed = n_meta = 0
    typed_rows = []
    crc_bad = 0
    lost = 0
    gap_from = gap_to = 0

    # 스키마/메타를 언제 끼울지. 데이터가 먼저 나가야 하므로 0 이 아니다.
    SCHEMA_AT = 3
    META_AT = 4
    CRC_BAD_AT = 7

    skip_until = -1
    for i in range(cycles):
        if i == skip_until:
            skip_until = -1

        # --- 손실 구간: seq 를 건너뛴다 (FW 는 보냈는데 PC 가 못 받은 상황) ---
        if i == drop_at:
            gap_from = seq - 1
            seq += drop_len
            lost += drop_len
            gap_to = seq

        # --- 0x20 Total Data (매 주기) ---
        ang = math.sin(i * 0.25)
        stream += wire_frame(seq, PHAI_MODULE_TOTAL_DATA, total_payload({
            "xm_loop_count": 1000 + i,
            "leftHipAngle": int(ang * 16384),
            "rightHipAngle": int(-ang * 16384),
        }))
        seq += 1
        n_total += 1

        # --- 0xEE 스키마 (한 번) ---
        if i == SCHEMA_AT:
            stream += wire_frame(seq, MODULE_SCHEMA_DESC, schema_frame_payload())
            seq += 1

        # --- 0xEF 메타 (한 번) — 와이어는 [target_module_id][json...] ---
        if i == META_AT:
            stream += wire_frame(seq, PHAI_MODULE_USER_META,
                                 bytes([MODULE_META_ONLY]) + META_ONLY_JSON.encode("ascii"))
            seq += 1

        # --- 0xF0 타입드 사용자 채널 (매 주기) ---
        row = dict(state=i % 5, contact=(i % 3 == 0), emg_rms=round(0.5 + i * 0.125, 3),
                   angle_x10=int(ang * 300), tick=10000 + i,
                   torque=(round(ang * 1.25, 4), round(-ang * 0.75, 4)))
        stream += wire_frame(seq, MODULE_TYPED, typed_payload(**row))
        seq += 1
        n_typed += 1
        typed_rows.append(row)

        # --- 0xF1 이름만 아는 채널 (3주기마다) ---
        if i % 3 == 0:
            stream += wire_frame(seq, MODULE_META_ONLY,
                                 struct.pack("<3f", 24.0 - i * 0.05, 12.5 + i, 21.0))
            seq += 1
            n_meta += 1

        # --- 깨진 CRC 하나 — 수신기가 세고 버려야 한다 ---
        if i == CRC_BAD_AT:
            stream += wire_frame(seq, MODULE_TYPED,
                                 typed_payload(0, False, 0.0, 0, 0, (0.0, 0.0)),
                                 corrupt_crc=True)
            seq += 1          # FW 는 seq 를 소모했다 -> 수신기에는 손실 1로 보인다
            crc_bad += 1

    return bytes(stream), DemoExpectation(
        frames_ok=n_total + n_typed + n_meta + 2,   # +2 = 0xEE, 0xEF
        frames_crc_bad=crc_bad,
        lost_frames=lost,
        gap_from=gap_from, gap_to=gap_to,
        total_frames=n_total, typed_frames=n_typed, meta_only_frames=n_meta,
        typed_rows=typed_rows, seq_max=seq - 1)


# ---------------------------------------------------------------------------
def _selfcheck() -> int:
    """이 모듈만 단독으로 — 만든 바이트가 정말 되돌려지는지."""
    import sys
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    fails = []

    # COBS 왕복 — 0 이 많은/없는/254 경계 모두
    for name, data in [
        ("zeros", bytes(300)),
        ("no-zero", bytes(range(1, 256)) * 2),
        ("254-boundary", bytes([1]) * 254 + b"\x00" + bytes([2]) * 254),
        ("mixed", bytes([0, 1, 0, 0, 255, 7])),
    ]:
        back = cobs_decode(cobs_encode(data))
        if back != data:
            fails.append("COBS 왕복 실패 (%s): %d B -> %d B" % (name, len(data), len(back)))

    # 구조체 크기·오프셋이 서로 어긋나지 않는가
    end = max(f.offset + f.byte_len for f in DEMO_FIELDS)
    if end > DEMO_STRUCT_SIZE:
        fails.append("필드가 구조체를 넘는다: %d > %d" % (end, DEMO_STRUCT_SIZE))
    if len(typed_payload(1, True, 1.0, 1, 1, (1.0, 2.0))) != DEMO_STRUCT_SIZE:
        fails.append("typed_payload 길이가 sizeof 와 다르다")

    # 스키마 프래그먼트가 자기 파서를 통과하는가
    try:
        frag = EE.parse_fragment(schema_frame_payload())
        if frag.schema_crc32 != schema_crc32():
            fails.append("스키마 CRC 불일치")
    except EE.SchemaError as e:
        fails.append("스키마 프래그먼트 파싱 실패: %s" % e)

    # 세션 하나가 만들어지는가
    stream, exp = build_demo_session(cycles=8)
    if not stream or exp.typed_frames != 8:
        fails.append("세션 생성이 이상하다: %d B, typed=%d" % (len(stream), exp.typed_frames))

    for f in fails:
        print("  FAIL " + f)
    print("demo_stream 자기검사: %s" % ("FAIL %d건" % len(fails) if fails else "OK"))
    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(_selfcheck())
