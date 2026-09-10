#!/usr/bin/env python3
"""0x20 Total Data 디코드 회귀 시험 — 보드 없이 돈다.

    python test_total_data_decoder.py

total_data_decoder.py 안의 자기검사가 **디코더 단독**을 보는 반면, 이 파일은
**와이어부터** 본다: 프레임을 만들어 COBS 로 싸고, 수신기와 같은 경로
(cobs_decode -> parse_phai_frame -> FrameRouter -> TotalDataDecoder)로 되푼다.

여기서만 잡히는 것이 하나 있다 — **페이로드가 368 B 로 온다는 사실**이다.
PhAI 의 LEN 은 4바이트 단위라 365 B 구조체가 92 units = 368 B 로 실린다.
디코더를 365 로 단정해 짜면 단독 시험은 통과하고 실기에서 전멸한다.
"""
import struct
import sys

from frame_router import (
    PHAI_SOF, PHAI_MODULE_TOTAL_DATA,
    crc16_ccitt, cobs_decode, parse_phai_frame, FrameRouter,
)
from test_frame_router import cobs_encode
from total_data_decoder import TotalDataDecoder, MAP_AVAILABLE
import xm_total_data_map as M


def build_total_data_wire(seq_id, payload365, status=0):
    """365 B 구조체를 FW 와 같은 규약으로 와이어 바이트로 만든다.

    FW 는 payload 를 4바이트 배수로 올려 LEN(4바이트 단위)에 담는다
    (phai_packet_builder.c). 그 패딩을 여기서도 그대로 한다 — 안 하면
    시험이 실제와 다른 모양을 검사하게 된다.
    """
    padded = bytes(payload365) + bytes((-len(payload365)) % 4)
    len_units = len(padded) // 4
    body = bytearray([PHAI_SOF, len_units, seq_id & 0xFF, (seq_id >> 8) & 0xFF,
                      module_id_of(), status])
    body.extend(padded)
    crc = crc16_ccitt(bytes(body))
    body.append(crc & 0xFF)
    body.append((crc >> 8) & 0xFF)
    return cobs_encode(bytes(body)) + bytes([0]), len(body)


def module_id_of():
    return PHAI_MODULE_TOTAL_DATA


def make_payload(values):
    """{필드이름: raw값} -> 365 B."""
    by_name = {d.name: d for d in M.TOTAL_DATA_MAP}
    code = {"uint8": "B", "int8": "b", "uint16": "H", "int16": "h",
            "uint32": "I", "int32": "i", "float32": "f"}
    buf = bytearray(M.TOTAL_PACKET_SIZE)
    for name, raw in values.items():
        d = by_name[name]
        struct.pack_into("<" + code[d.type], buf, d.offset, raw)
    return bytes(buf)


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    if not MAP_AVAILABLE:
        print("FAIL: xm_total_data_map.py 없음 — release_sync 가 배달하지 않았거나 "
              "코드젠을 아직 안 돌렸다.")
        return 1

    fails = []

    def check(cond, msg):
        if not cond:
            fails.append(msg)

    dec = TotalDataDecoder()
    print(dec.identity())

    # 알려진 raw 를 심는다. 세 스케일 공식이 모두 걸리도록 고른다.
    known = {
        "xm_loop_count": 1234567,
        "leftHipAngle": 16384,      # multiply_divide, scale 720 -> 360.0 deg
        "rightHipAngle": -16384,    # -> -360.0
        "phai_x1_status": 0xA5,     # none
    }
    payload = make_payload(known)
    check(len(payload) == 365, "payload 가 365 B 가 아님: %d" % len(payload))

    wire, raw_len = build_total_data_wire(7, payload)

    # 와이어 산수 — 설계 문서(PLAN 4.2)의 0x20 원시값과 대조한다.
    #   raw  = 6(헤더) + roundup4(365) + 2(CRC) = 376  ... 항등식
    #   wire = raw + floor(raw/254) + 2 = 379        ... **상한**이지 항등식이 아니다
    #
    # COBS 오버헤드는 0 바이트가 없을 때 최대다. 0x20 페이로드는 보통 0 이 많아
    # 실제로는 378 B 로 나온다(2026-09-10 실측). 예산은 상한을 써야 하므로 379 가
    # 맞고, 아래는 그 상한이 정말 상한인지와 최악치가 정확히 379 인지를 함께 본다.
    check(raw_len == 376, "raw 프레임이 376 B 가 아님: %d" % raw_len)
    budget = raw_len + raw_len // 254 + 2
    check(budget == 379, "예산식이 379 를 주지 않음: %d" % budget)
    check(len(wire) <= budget,
          "실제 와이어 %d 가 예산 상한 %d 를 넘었다" % (len(wire), budget))

    # 0 이 하나도 없는 페이로드 = COBS 최악. 이때 상한과 딱 맞아야 한다.
    worst_payload = bytes((i % 255) + 1 for i in range(M.TOTAL_PACKET_SIZE))
    worst_wire, worst_raw = build_total_data_wire(8, worst_payload)
    check(worst_raw == 376, "최악 raw %d" % worst_raw)
    check(len(worst_wire) == 379,
          "COBS 최악 와이어가 379 가 아님: %d — 예산 상한식이 틀렸다" % len(worst_wire))

    # 수신기와 같은 경로로 되푼다
    delim = wire.index(0)
    frame = cobs_decode(wire[:delim])
    pkt, err = parse_phai_frame(frame, 0.0)
    check(err is None, "parse 실패: %r" % err)
    if pkt is None:
        for m in fails:
            print("  FAIL  " + m)
        return 1

    check(pkt.module_id == PHAI_MODULE_TOTAL_DATA,
          "module_id 0x%02X" % pkt.module_id)
    check(len(pkt.payload) == 368,
          "와이어 페이로드가 368 B 가 아님: %d (4바이트 패딩 규약)" % len(pkt.payload))

    router = FrameRouter()
    tag, _delta = router.route(pkt)
    check(tag == "system", "0x20 이 system 으로 라우팅되지 않음: %r" % tag)
    check(router.system_taps[PHAI_MODULE_TOTAL_DATA].frame_count == 1,
          "system tap 이 세지 않음")

    named = dec.named(pkt.payload)
    check(named is not None, "368 B 페이로드 디코드 실패")
    if named is not None:
        check(named["xm_loop_count"] == 1234567,
              "xm_loop_count %r" % named["xm_loop_count"])
        check(named["phai_x1_status"] == 0xA5,
              "phai_x1_status %r" % named["phai_x1_status"])
        check(named["leftHipAngle"] == 360.0,
              "leftHipAngle %r" % named["leftHipAngle"])
        check(named["rightHipAngle"] == -360.0,
              "rightHipAngle %r" % named["rightHipAngle"])
        check(len(named) == 197, "채널 수 %d" % len(named))

    # CSV 헤더가 값 개수와 맞는가 — 어긋나면 컬럼이 밀린 CSV 가 나온다
    hdr = dec.csv_header().split(",")
    vals = dec.scaled(pkt.payload)
    check(len(hdr) == len(vals) + 4,
          "CSV 헤더 %d 개 vs 값 %d + 접두 4" % (len(hdr), len(vals)))

    print(dec.summary())
    if fails:
        for m in fails:
            print("  FAIL  " + m)
        print("%d FAILED" % len(fails))
        return 1
    print("  wire -> decode chain passed (365 B struct, 368 B payload, 379 B wire)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
