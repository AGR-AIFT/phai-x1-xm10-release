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
import threading
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
                 "assumed_float32", "_decode", "struct_name")

    def __init__(self, module_id, names, units, source, detail, decode,
                 assumed_float32=False, struct_name=""):
        self.module_id = module_id
        self.names = list(names)
        self.units = list(units) if units else [""] * len(self.names)
        self.source = source
        self.detail = detail
        self.assumed_float32 = assumed_float32
        self._decode = decode
        # 0xEE 스키마일 때만 채워진다 — GUI 탭 제목 등, 이름이 필요한 소비자를 위한
        # 부가 정보다(추가 필드라 기존 소비자에게는 영향이 없다).
        self.struct_name = struct_name

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
                      "0xEE", detail, schema.decode, struct_name=schema.struct_name)


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
    """module_id -> ChannelSet. 스트리밍 중에도, 파일을 사후에 읽을 때도 같은 것을 쓴다.

    스레드: 실시간 GUI 에서는 **수신 스레드**가 `feed_0xEE`/`feed_0xEF` 로 채우고, **화면
    스레드**가 프레임마다 `get()` 으로 조회한다. `get()` 은 조회하면서 캐시에 새 항목을
    넣기 때문에, 둘이 겹치면 한쪽이 딕셔너리를 돌고 있는 사이 다른 쪽이 크기를 바꾼다
    ("dictionary changed size during iteration" — 수신 스레드가 죽으면 앱 전체가 종료된다).
    그래서 딕셔너리를 **바꾸는** 자리(`feed_*`, 그리고 캐시에 없어서 새로 만들어 넣는
    `get()` 의 느린 길)는 `_lock` 안에서 한다. 캐시에 이미 있는 항목을 읽는 `get()` 의
    빠른 길은 락이 없다 — 프레임마다 불리는 함수라 락 비용(프레임당 약 1µs)이 아깝고,
    딕셔너리 조회 한 번은 원자적이다.
    """

    __slots__ = ("_by_module", "_by_module_len", "_ef_raw", "reasm", "_total",
                 "ee_errors", "ef_errors", "_lock")

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
        # 위 세 딕셔너리를 바꾸는 구간(과 캐시에 없어서 새로 만들어 넣는 조회)을 묶는다.
        # 안에서 다시 이 락을 잡는 호출이 없어서 재진입 가능한 RLock 이 아니라 Lock 이다.
        self._lock = threading.Lock()
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
        with self._lock:
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
        with self._lock:
            self._ef_raw[target] = entries
            # 이미 만들어 둔 추측을 무효화 — 다음 get() 에서 새 메타로 다시 만든다.
            # 새 메타를 넣는 것과 옛 캐시를 지우는 것이 한 덩어리여야, 그 사이에 끼어든
            # get() 이 옛 메타로 만든 항목을 다시 캐시에 남기지 못한다.
            for k in [k for k in self._by_module_len if k[0] == target]:
                del self._by_module_len[k]
        return target

    # -- 조회 ----------------------------------------------------------
    def get(self, module_id: int, payload_len: int) -> Optional[ChannelSet]:
        """이 module_id 를 푸는 최선의 방법. 없으면 None (= raw 로 다뤄라)."""
        # 빠른 길: 이미 캐시에 있으면 락 없이 돌려준다. 이 함수는 프레임마다(초당 수만 번)
        # 불리고, 딕셔너리 조회 한 번은 원자적이다 — 캐시 항목은 다 만들어진 뒤에 넣고
        # 락 안에서만 바꾸거나 지우므로, 여기서 읽는 항목은 늘 온전하다.
        cs = self._by_module.get(module_id)
        if cs is not None:
            return cs
        cs = self._by_module_len.get((module_id, payload_len))
        if cs is not None:
            return cs
        # 느린 길: 캐시에 없어서 만들어 넣어야 한다. 다른 스레드의 feed_* 와 겹치지 않게 락 안에서.
        with self._lock:
            return self._get_locked(module_id, payload_len)

    def _get_locked(self, module_id: int, payload_len: int) -> Optional[ChannelSet]:
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

    # -- 표시용 조회 (탭 제목 등, GUI 전용 — 값 디코딩과 무관) -----------
    def struct_name(self, module_id: int) -> Optional[str]:
        """`0xEE` 로 확정된 struct 이름. 길이와 무관하다. 없으면 None."""
        cs = self._by_module.get(module_id)          # 딕셔너리 조회 한 번은 원자적이다
        return cs.struct_name if (cs is not None and cs.struct_name) else None

    def meta_single_name(self, module_id: int) -> Optional[str]:
        """`0xEF` 메타의 채널이 **하나뿐**일 때 그 이름.

        `0xEF` 는 채널별 이름/단위만 나른다 — module 전체를 가리키는 이름 필드는
        와이어에 없다. 채널이 하나뿐인 module 은 그 채널 이름이 사실상 module 이름
        구실을 하지만, 여럿이면 대표할 이름이 없다.
        """
        entries = self._ef_raw.get(module_id)        # 딕셔너리 조회 한 번은 원자적이다
        if entries and len(entries) == 1 and isinstance(entries[0], dict):
            name = str(entries[0].get("name", "")).strip()
            return name or None
        return None

    def known(self) -> List[ChannelSet]:
        with self._lock:
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
