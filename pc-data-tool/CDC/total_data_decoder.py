#!/usr/bin/env python3
"""Total Data Packet(module_id 0x20) 디코더 — 생성된 맵을 그대로 쓴다.

지금까지 이 폴더의 샘플은 0x20 을 **디코딩하지 않았다**. payload 를 float32 배열로
읽는 방식이라, uint32/uint16/uint8/float 가 섞인 이 패킷에서는 값이 깨졌기 때문이다.
README 는 직접 struct.unpack 을 확장하라고 안내했고, 그 확장을 손으로 하면
필드가 하나 바뀔 때마다 사람이 따라가야 했다.

이 파일은 그 손작업을 없앤다. 레이아웃은 FW 와 **같은 YAML SSOT** 에서 생성된
xm_total_data_map.py 가 갖고 있고, 여기에는 그 표를 쓰는 방법만 있다.

    from total_data_decoder import TotalDataDecoder
    dec = TotalDataDecoder()
    values = dec.scaled(frame.payload)          # 197개, 물리단위
    named  = dec.named(frame.payload)           # 이름 -> 값

보드 없이 자기검사가 된다:  python total_data_decoder.py

와이어 페이로드는 365 B 가 아니라 368 B 다
------------------------------------------
PhAI V2.2 의 LEN 은 **4바이트 단위**라, 365 B 구조체는 92 units = 368 B 로 실린다.
뒤 3 B 는 패딩이다. parse_phai_frame() 이 돌려주는 payload 도 368 B 다.
그래서 unpack() 이 아니라 unpack_from() 을 쓴다 — 길이를 365 로 단정하면
정상 프레임이 전부 실패한다.

맵 정체성 — 알아야 할 한계
--------------------------
디코더는 자기가 **어떤 맵을 쓰는지**(DATA_MAP_FINGERPRINT) 는 말할 수 있지만,
보드가 **어떤 맵으로 보내는지**는 알 수 없다. 0x20 패킷 어디에도 버전·지문 필드가
없기 때문이다(Header = xm_loop_count / device_online_mask / phai_x1_status).
따라서 FW 와 PC 의 맵이 어긋나면 **조용히 잘못된 값**이 나온다.

지금 할 수 있는 것은 쓰는 쪽 지문을 로그·CSV·세션 기록에 남겨, 나중에 값이 이상할 때
어느 맵으로 풀었는지를 되짚을 수 있게 하는 것뿐이다. identity() 가 그 한 줄이다.
와이어에 지문을 싣는 것은 FW 변경이 필요하고, 그 자리에 넣을 값은 이미 생성돼 있다
(xm_total_data_packet.h 의 XM_TOTAL_DATA_MAP_FINGERPRINT).
"""
from __future__ import annotations

import struct

try:
    import xm_total_data_map as _MAP
except ImportError:                                   # pragma: no cover
    _MAP = None

MAP_AVAILABLE = _MAP is not None

MAP_MISSING_HINT = (
    "xm_total_data_map.py 가 없다. 이 파일은 FW 레포의 YAML SSOT 에서 생성돼 "
    "릴리스와 함께 배달된다(tools/release_sync.py). 개발 트리에서 직접 만들려면 "
    "generate_data_map.py 를 --py-out 으로 돌려 이 폴더에 두면 된다."
)

# 구조체 크기(365)와 와이어 페이로드 크기(368, 4바이트 단위 패딩).
PAYLOAD_SIZE = _MAP.TOTAL_PACKET_SIZE if MAP_AVAILABLE else 0
WIRE_PAYLOAD_SIZE = ((PAYLOAD_SIZE + 3) // 4) * 4 if MAP_AVAILABLE else 0
MODULE_ID = _MAP.MODULE_ID_TOTAL if MAP_AVAILABLE else 0x20


class TotalDataDecoder:
    """0x20 페이로드 -> 이름 붙은 값.

    상태는 카운터뿐이다(디코드 수·거절 수). 프레임 순서나 시퀀스는 FrameRouter 몫이라
    여기서 또 세지 않는다 — 두 곳에서 세면 두 곳을 고쳐야 한다.
    """

    __slots__ = ("names", "groups", "units", "size", "wire_size",
                 "version", "fingerprint",
                 "_st", "_mul_idx", "_div_idx",
                 "decoded", "rejected_size")

    def __init__(self):
        if not MAP_AVAILABLE:
            raise RuntimeError(MAP_MISSING_HINT)

        self.names = _MAP.SCALAR_NAMES
        self.groups = _MAP.SCALAR_GROUPS
        self.units = _MAP.SCALAR_UNITS
        self.size = _MAP.TOTAL_PACKET_SIZE
        self.wire_size = WIRE_PAYLOAD_SIZE
        self.version = _MAP.DATA_MAP_VERSION
        self.fingerprint = _MAP.DATA_MAP_FINGERPRINT
        self._st = _MAP.PACKET_STRUCT

        # 스케일이 있는 자리만 미리 뽑아 둔다. 1 kHz 에서 197개를 매번 분기시키는 것과
        # 스케일 있는 것만 손보는 것은 체감이 다르다.
        self._mul_idx = tuple((i, m) for i, m in enumerate(_MAP.SCALAR_MUL) if m is not None)
        self._div_idx = tuple((i, d) for i, d in enumerate(_MAP.SCALAR_DIV) if d is not None)

        self.decoded = 0
        self.rejected_size = 0

    # ---------------------------------------------------------------- 기본
    def accepts(self, payload) -> bool:
        """365(정확) 또는 368(4바이트 패딩)만 받는다.

        느슨하게 받으면 맵이 바뀐 보드를 조용히 잘못 풀게 된다. 크기는 우리가 가진
        유일한 무료 오라클이라 버리지 않는다.
        """
        return len(payload) in (self.size, self.wire_size)

    def unpack(self, payload):
        """스케일 없이 raw 197개. 형식이 아니면 None 을 돌려주고 카운터를 올린다."""
        if not self.accepts(payload):
            self.rejected_size += 1
            return None
        self.decoded += 1
        return self._st.unpack_from(payload, 0)

    def scaled(self, payload):
        """물리단위 197개(list). 형식이 아니면 None."""
        raw = self.unpack(payload)
        if raw is None:
            return None
        out = list(raw)
        for i, m in self._mul_idx:
            out[i] = raw[i] * m
        for i, d in self._div_idx:
            out[i] = raw[i] / d
        return out

    def named(self, payload):
        """이름 -> 물리값 dict. 형식이 아니면 None."""
        vals = self.scaled(payload)
        if vals is None:
            return None
        return dict(zip(self.names, vals))

    # ---------------------------------------------------------------- 부분 선택
    def selector(self, wanted):
        """이름 몇 개만 뽑는 콜러블. 197개를 다 스케일링하지 않는다.

            take = dec.selector(["leftHipAngle", "rightHipAngle"])
            a, b = take(frame.payload)

        GUI 가 6채널만 그린다면 이쪽이 맞다.
        """
        index = {n: i for i, n in enumerate(self.names)}
        missing = [n for n in wanted if n not in index]
        if missing:
            raise KeyError("맵에 없는 채널: %s" % ", ".join(missing))

        mul = dict(self._mul_idx)
        div = dict(self._div_idx)
        picks = tuple((index[n], mul.get(index[n]), div.get(index[n])) for n in wanted)
        st = self._st

        def take(payload):
            if not self.accepts(payload):
                self.rejected_size += 1
                return None
            self.decoded += 1
            raw = st.unpack_from(payload, 0)
            return tuple(
                raw[i] * m if m is not None else (raw[i] / d if d is not None else raw[i])
                for i, m, d in picks
            )

        return take

    # ---------------------------------------------------------------- 보고
    def identity(self) -> str:
        """어느 맵으로 풀었는지 한 줄. CSV 헤더·세션 기록에 남기는 용도."""
        return ("total_data_map v%s fingerprint=%s size=%dB wire=%dB channels=%d"
                % (self.version, self.fingerprint, self.size, self.wire_size,
                   len(self.names)))

    def summary(self) -> str:
        return ("0x%02X: %d decoded / %d rejected(size)"
                % (MODULE_ID, self.decoded, self.rejected_size))

    def csv_header(self, prefix=("time_s", "pc_time_s", "seq_id", "tx_drops")) -> str:
        return ",".join(list(prefix) + list(self.names))


# =============================================================================
# 자기검사 — 보드 없이 돈다:  python total_data_decoder.py
# =============================================================================
def _selftest() -> int:
    import sys
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    if not MAP_AVAILABLE:
        print("FAIL: " + MAP_MISSING_HINT)
        return 1

    dec = TotalDataDecoder()
    print(dec.identity())

    fails = []

    def check(cond, msg):
        if not cond:
            fails.append(msg)

    check(dec.size == 365, "구조체 크기가 365 가 아님: %d" % dec.size)
    check(dec.wire_size == 368, "와이어 페이로드가 368 이 아님: %d" % dec.wire_size)
    check(len(dec.names) == 197, "채널 수가 197 이 아님: %d" % len(dec.names))

    # 1) 패딩된 368 B 를 받아들이는가 (이게 실제 와이어 모양이다)
    check(dec.unpack(bytes(dec.wire_size)) is not None, "368 B 패딩 페이로드를 거절했다")

    # 2) 패딩 안 된 365 B 도 받는가
    check(dec.unpack(bytes(dec.size)) is not None, "365 B 페이로드를 거절했다")

    # 3) 엉뚱한 길이는 거절하는가
    check(dec.unpack(bytes(dec.size - 1)) is None, "짧은 페이로드를 통과시켰다")
    check(dec.unpack(bytes(dec.wire_size + 4)) is None, "긴 페이로드를 통과시켰다")

    # 4) 알려진 값이 물리단위로 나오는가
    by_name = {d.name: d for d in _MAP.TOTAL_DATA_MAP}
    buf = bytearray(dec.wire_size)
    struct.pack_into("<I", buf, by_name["xm_loop_count"].offset, 987654)
    struct.pack_into("<h", buf, by_name["leftHipAngle"].offset, 16384)   # 720/32768 -> 360.0
    named = dec.named(bytes(buf))
    check(named is not None, "패딩 페이로드 named() 실패")
    if named is not None:
        check(named["xm_loop_count"] == 987654,
              "xm_loop_count %r" % named["xm_loop_count"])
        check(named["leftHipAngle"] == 360.0, "leftHipAngle %r" % named["leftHipAngle"])

    # 5) selector 가 전체 디코드와 같은 값을 주는가
    take = dec.selector(["xm_loop_count", "leftHipAngle"])
    picked = take(bytes(buf))
    check(picked == (987654, 360.0), "selector 결과 %r" % (picked,))

    # 6) 뒤 패딩 3 B 가 값에 새지 않는가 — 패딩을 흔들어도 결과가 같아야 한다
    noisy = bytearray(buf)
    for i in range(dec.size, dec.wire_size):
        noisy[i] = 0xFF
    check(dec.scaled(bytes(noisy)) == dec.scaled(bytes(buf)),
          "패딩 바이트가 디코드 결과에 영향을 준다")

    # 7) frame_router 의 module_id 상수와 어긋나지 않는가
    try:
        import frame_router as FR
        check(FR.PHAI_MODULE_TOTAL_DATA == MODULE_ID,
              "module_id 불일치: frame_router 0x%02X vs map 0x%02X"
              % (FR.PHAI_MODULE_TOTAL_DATA, MODULE_ID))
    except ImportError:
        print("  (frame_router 없음 — module_id 대조 생략)")

    print(dec.summary())
    if fails:
        for m in fails:
            print("  FAIL  " + m)
        print("%d FAILED" % len(fails))
        return 1
    print("  self-test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
