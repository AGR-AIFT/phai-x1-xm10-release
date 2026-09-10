#!/usr/bin/env python3
"""채널 스키마 레지스트리 — 출처가 무엇이든 **하나의 모양**으로.

module_id 하나에 대해 "이 payload 를 어떤 이름의 값들로 푸는가" 를 답한다.
그 답이 어디서 왔는지는 소비자가 몰라도 되게 감싼다. 그래서 FW 가 `0xEE` 를
보내기 시작하면 **여기 한 곳만** 채워지고 화면·CSV·로그는 아무것도 안 바뀐다.

출처와 우선순위
---------------
| 우선 | 출처 | 아는 것 | 지금 |
|---|---|---|---|
| 1 | `0xEE` 이진 스키마 | 이름·단위·**타입**·배열·offset·스케일 | FW 미구현 (송신 0건) |
| 2 | `0xEF` JSON 메타 | 이름·단위 **뿐** | FW 가 이미 보냄. PC 가 버리고 있었다 |
| 3 | 생성된 0x20 맵 | 197채널 전부 (FW 와 같은 SSOT) | 있음 |
| 4 | 없음 | 아무것도 | float32 로 **가정**하고 ch0..chN |

`0xEF` 가 `0xEE` 보다 아래인 이유는 **타입을 모르기 때문**이다. 이름만 있고 타입이 없으면
정수 필드를 float 로 읽어 조용히 틀린 값을 낸다 — 이름이 붙어 있어서 오히려 더 그럴듯해 보인다.
그래서 `0xEF` 로 만든 채널셋은 `assumed_float32 = True` 를 달고 다니고, 소비자가 그 사실을
사용자에게 보여줄 수 있게 한다.
"""
from __future__ import annotations

import json
import struct
from typing import Dict, List, Optional

import schema_0xee as EE

try:
    from total_data_decoder import TotalDataDecoder, MAP_AVAILABLE as _MAP_OK
except ImportError:                                     # pragma: no cover
    TotalDataDecoder = None
    _MAP_OK = False

MODULE_ID_TOTAL = 0x20
MODULE_ID_LINK_HEALTH = 0xED
MODULE_ID_SCHEMA_DESC = 0xEE
MODULE_ID_USER_META = 0xEF


class ChannelSet:
    """한 module_id 를 푸는 방법. 출처가 달라도 이 인터페이스는 같다."""

    __slots__ = ("module_id", "names", "units", "source", "detail",
                 "assumed_float32", "_decode")

    def __init__(self, module_id, names, units, source, detail, decode,
                 assumed_float32=False):
        self.module_id = module_id
        self.names = list(names)
        self.units = list(units) if units else [""] * len(self.names)
        self.source = source
        self.detail = detail
        self.assumed_float32 = assumed_float32
        self._decode = decode

    def decode(self, payload: bytes) -> Optional[list]:
        return self._decode(payload)

    def __len__(self):
        return len(self.names)

    def describe(self) -> str:
        warn = "  ⚠ 타입 미상 — float32 로 가정" if self.assumed_float32 else ""
        return "0x%02X  %d채널  [%s] %s%s" % (self.module_id, len(self.names),
                                              self.source, self.detail, warn)


# ---------------------------------------------------------------- 출처별 생성자

def from_ee_schema(schema: EE.Schema) -> ChannelSet:
    units = []
    for f in schema.fields:
        units.extend([f.unit] * f.array_len)
    detail = "%s (%d필드, %dB, crc=%08x)" % (schema.struct_name, len(schema.fields),
                                             schema.struct_size, schema.schema_crc32)
    return ChannelSet(schema.module_id, schema.scalar_names(), units,
                      "0xEE", detail, schema.decode)


def from_ef_json(module_id: int, entries: list, payload_len: int) -> ChannelSet:
    """`0xEF` 는 이름/단위만 준다. 값 해석은 float32 **가정**이다.

    채널 수가 payload 와 안 맞을 수 있다 — FW 가 메타를 등록한 뒤 다른 개수를 보내거나,
    사용자가 JSON 을 안 고쳤을 때. 그때는 이름을 payload 길이에 맞춰 자르거나 채운다.
    이름이 하나 밀린 CSV 보다 `ch7` 이 낫다.
    """
    n_wire = payload_len // 4
    names, units = [], []
    for e in entries:
        if isinstance(e, dict):
            names.append(str(e.get("name", "")) or "ch%d" % len(names))
            units.append(str(e.get("unit", "")))
        else:
            names.append(str(e))
            units.append("")

    note = ""
    if len(names) != n_wire:
        note = " (메타 %d개 vs payload %d채널 — 맞춰 조정)" % (len(names), n_wire)
        if len(names) > n_wire:
            names, units = names[:n_wire], units[:n_wire]
        else:
            while len(names) < n_wire:
                names.append("ch%d" % len(names))
                units.append("")

    st = struct.Struct("<%df" % len(names)) if names else None

    def _dec(payload):
        if st is None or len(payload) < st.size:
            return None
        return list(st.unpack_from(payload, 0))

    return ChannelSet(module_id, names, units, "0xEF",
                      "JSON 메타 %d채널%s" % (len(names), note), _dec,
                      assumed_float32=True)


def from_generated_map(dec) -> ChannelSet:
    return ChannelSet(MODULE_ID_TOTAL, dec.names, dec.units, "generated-map",
                      "v%s fingerprint=%s" % (dec.version, dec.fingerprint),
                      dec.scaled)


def float32_fallback(module_id: int, payload_len: int) -> Optional[ChannelSet]:
    n = payload_len // 4
    if n == 0 or payload_len % 4:
        return None
    st = struct.Struct("<%df" % n)

    def _dec(payload):
        if len(payload) < st.size:
            return None
        return list(st.unpack_from(payload, 0))

    return ChannelSet(module_id, ["ch%d" % i for i in range(n)], None,
                      "fallback", "스키마 없음 — float32 %d채널로 가정" % n, _dec,
                      assumed_float32=True)


# ---------------------------------------------------------------- 레지스트리

class SchemaRegistry:
    """module_id -> ChannelSet. 스트리밍 중에도, 파일을 사후에 읽을 때도 같은 것을 쓴다."""

    __slots__ = ("_by_module", "_by_module_len", "_ef_raw", "reasm", "_total",
                 "ee_errors", "ef_errors")

    def __init__(self, reassembly_timeout_s: float = EE.REASSEMBLY_TIMEOUT_S):
        # 길이와 무관한 출처(0xEE — 자기 struct_size 를 안다)
        self._by_module: Dict[int, ChannelSet] = {}
        # 길이에 **의존하는** 출처(0xEF · fallback · 생성맵). module_id 만으로 캐시하면
        # 같은 모듈이 다른 길이로 올 때 첫 길이로 굳어 채널이 조용히 잘린다 (감사 #4).
        self._by_module_len: Dict[tuple, ChannelSet] = {}
        self._ef_raw: Dict[int, list] = {}          # module_id -> JSON entries
        self.reasm = EE.Reassembler(reassembly_timeout_s)
        self.ee_errors: List[str] = []
        self.ef_errors: List[str] = []
        self._total = None
        if _MAP_OK:
            self._total = TotalDataDecoder()

    # -- 입력 ----------------------------------------------------------
    def feed_0xEE(self, payload: bytes, now: float) -> Optional[ChannelSet]:
        """완성된 순간에만 ChannelSet 을 돌려준다. 실패는 삼키고 기록한다."""
        try:
            schema = self.reasm.feed(payload, now)
        except EE.SchemaError as e:
            self.ee_errors.append(str(e))
            return None
        except Exception as e:  # noqa: BLE001
            # docstring 이 "실패는 삼키고 기록한다" 고 약속한다. SchemaError 만 잡으면
            # 예상 못한 예외가 수신 스레드를 죽이고, 그런 프레임이 든 로그는 영영
            # 못 읽게 된다 (2026-09-10 감사 P0). 약속대로 전부 삼킨다.
            self.ee_errors.append("예상 못한 예외 %s: %s" % (type(e).__name__, e))
            return None
        if schema is None:
            return None
        cs = from_ee_schema(schema)
        self._by_module[schema.module_id] = cs      # 0xEE 는 항상 이긴다
        # 길이 의존 캐시에 남아 있던 추측(0xEF/fallback)을 지운다.
        for k in [k for k in self._by_module_len if k[0] == schema.module_id]:
            del self._by_module_len[k]
        return cs

    def feed_0xEF(self, payload: bytes) -> Optional[int]:
        """`0xEF` payload = `[target_module_id:1B][json bytes...]`.

        JSON 만 저장한다 — 채널 수는 실제 payload 를 봐야 정해지므로 `get()` 에서 만든다.
        반환: 대상 module_id (파싱 실패면 None).
        """
        if len(payload) < 2:
            self.ef_errors.append("0xEF payload %d B — 너무 짧다" % len(payload))
            return None
        target = payload[0]
        raw = bytes(payload[1:]).rstrip(b"\x00")      # 와이어 4바이트 패딩 제거
        try:
            entries = json.loads(raw.decode("utf-8", "replace"))
        except Exception as e:  # noqa: BLE001 — 어떤 실패든 기록하고 넘어간다
            self.ef_errors.append("0xEF(0x%02X) JSON 파싱 실패: %s" % (target, e))
            return None
        if not isinstance(entries, list):
            self.ef_errors.append("0xEF(0x%02X) JSON 이 배열이 아니다" % target)
            return None
        self._ef_raw[target] = entries
        # 이미 만들어 둔 추측을 무효화 — 다음 get() 에서 새 메타로 다시 만든다.
        for k in [k for k in self._by_module_len if k[0] == target]:
            del self._by_module_len[k]
        return target

    # -- 조회 ----------------------------------------------------------
    def get(self, module_id: int, payload_len: int) -> Optional[ChannelSet]:
        """이 module_id 를 푸는 최선의 방법. 없으면 None (= raw 로 다뤄라)."""
        cs = self._by_module.get(module_id)          # 0xEE — 길이와 무관
        if cs is not None:
            return cs

        key = (module_id, payload_len)
        cs = self._by_module_len.get(key)
        if cs is not None:
            return cs

        if module_id == MODULE_ID_TOTAL and self._total is not None:
            if self._total.accepts(bytes(payload_len)):
                cs = from_generated_map(self._total)
                self._by_module_len[key] = cs
                return cs
            return None                                # 크기가 안 맞으면 raw 가 정직하다

        entries = self._ef_raw.get(module_id)
        if entries is not None and payload_len % 4 == 0 and payload_len > 0:
            cs = from_ef_json(module_id, entries, payload_len)
            self._by_module_len[key] = cs
            return cs

        cs = float32_fallback(module_id, payload_len)
        if cs is not None:
            self._by_module_len[key] = cs
        return cs

    def known(self) -> List[ChannelSet]:
        out = [self._by_module[m] for m in sorted(self._by_module)]
        seen = {cs.module_id for cs in out}
        for k in sorted(self._by_module_len):
            if k[0] not in seen:
                out.append(self._by_module_len[k])
                seen.add(k[0])
        return out

    def summary(self) -> str:
        lines = [cs.describe() for cs in self.known()]
        if self.reasm.completed or self.reasm.invalid or self.reasm.crc_failures:
            lines.append(self.reasm.summary())
        if self.ee_errors:
            lines.append("0xEE 오류 %d건 (첫 건: %s)" % (len(self.ee_errors), self.ee_errors[0]))
        if self.ef_errors:
            lines.append("0xEF 오류 %d건 (첫 건: %s)" % (len(self.ef_errors), self.ef_errors[0]))
        return "\n".join(lines) if lines else "(등록된 스키마 없음)"
