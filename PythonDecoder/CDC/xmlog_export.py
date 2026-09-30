#!/usr/bin/env python3
"""`.xmlog` 를 읽어 요약하거나 CSV 로 내보낸다.

    python xmlog_export.py session.xmlog              # 무슨 일이 있었는지 요약
    python xmlog_export.py session.xmlog --csv out/   # 모듈별 CSV 로 내보내기
    python xmlog_export.py session.xmlog --dump 20    # 앞 20개 레코드를 사람이 읽게
    python xmlog_export.py session.xmlog --csv out/ --raw-hex   # 해석 없이 원본 바이트

왜 저장과 내보내기를 나눴나
---------------------------
받는 순간에 CSV 로 적으면 **그 순간의 해석이 곧 원본**이 된다. 스키마가 늦게 오거나
디코더에 버그가 있으면 그걸로 끝이다. `.xmlog` 는 바이트를 그대로 눕히고, CSV 는
언제든 다시 뽑는다. 디코더를 고치면 예전 로그도 같이 고쳐진다.

스키마는 **파일 안에서** 찾는다
-------------------------------
capture 는 모든 프레임을 그대로 적으므로, `0xEF`(JSON 메타)와 `0xEE`(이진 스키마)도
DATA 레코드로 파일 안에 들어 있다. 그래서 이 도구는 밖에서 설정을 받지 않는다 —
파일 하나만 있으면 그때 무슨 채널이 있었는지 재구성한다. 우선순위는
`schema_registry.py` 가 정한다 (0xEE > 0xEF > 생성맵 > float32 가정).

행마다 **그 시점의** 스키마로 푼다
---------------------------------
같은 module_id 가 한 파일 안에서 다른 스키마를 들고 올 수 있다(한 캡처를 재연결 뒤에도 이어 쓴 경우 등 —
GUI · CLI 는 연결마다 새 파일을 열어서 보통은 만들지 않는다). 파일 끝에 남은 **최종 스키마 하나**로
전 구간을 풀면 앞 구간이 조용히 틀린다 — 같은 4바이트 `00 00 80 3f` 가 float32 스키마로는 `1.0`,
uint32 스키마로는 `1065353216` 이다. 그래서 DATA 마다 이 순서로 스키마를 고른다 (`resolve_rows`).

1. `activation_id` 가 있으면 **그 activation 의 스키마**. 단 그 activation 이 같은 module_id 의
   것일 때만이다. 아니면 그 DATA 는 버리지 않고 `activation_id=0`(미상)으로 내려 원본 바이트로
   남긴다 — 틀린 스키마로 풀지도, 멀쩡한 행을 거부하지도 않는다.
2. 0(미상)이면 **그 뒤에 처음 도착하는** 그 모듈의 스키마, 단 같은 SESSION 구간 안에서만.
   스키마보다 먼저 온 데이터를 사후에 푸는 것이다(data-before-schema). 구간 안에 스키마가
   끝내 없으면 못 푼다 — 0xEF 이름 + float32 가정으로, 그것도 없으면 ch0.. 로 내려간다.
3. `0xEE` 를 프레임 그대로만 적은 예전 파일(activation 레코드가 없다)은 그 프레임들이 스키마를
   완성한 순간부터 그 스키마가 유효한 것으로 본다.

SESSION 레코드는 구간의 경계다. 재연결로 다른 장치가 붙었을 수 있어서 앞 구간의 스키마를 그 뒤로
넘기지 않는다. 한 모듈이 스키마를 둘 이상 썼다면 CSV 는 스키마마다 파일을 나눈다 — 처음 쓰인
스키마가 `..._user_0xF0.csv`, 그다음이 `..._user_0xF0_schema2.csv`, `_schema3` … (열 이름이 같아도
값의 뜻이 다르기 때문이다). 스키마로 못 푼 행이 그 모듈에 섞여 있으면 어느 쪽이 먼저 나왔든
`..._user_0xF0_unresolved.csv` 로 따로 간다. 풀린 스키마가 하나도 없는 모듈은 파일이 하나고 기본 이름이다.

이 층이 activation 참조를 검증한다(`resolve_rows`). `xmlog.py` 의 읽기(`scan`)는 레코드를 파일에
적힌 그대로 돌려주므로, 어긋난 참조를 0 으로 내려 읽는 일은 여기서만 일어난다.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from typing import NamedTuple, Optional

import xmlog as X
import schema_0xee as EE
import schema_registry as R


def _module_label(mid: int) -> str:
    if mid == R.MODULE_ID_TOTAL:
        return "total_0x20"
    if mid == R.MODULE_ID_USER_META:
        return "meta_0xEF"
    if mid == R.MODULE_ID_SCHEMA_DESC:
        return "schema_0xEE"
    if mid == R.MODULE_ID_LINK_HEALTH:
        return "health_0xED"
    if 0xF0 <= mid <= 0xFE:
        return "user_0x%02X" % mid
    return "module_0x%02X" % mid


# 값으로 풀지 않고 원본만 남길 채널 — 제어/메타라 열로 펴는 게 의미가 없다.
_RAW_ONLY = frozenset({R.MODULE_ID_USER_META, R.MODULE_ID_SCHEMA_DESC})

_USER_MODULES = range(0xF0, 0xFF)       # 0xF0~0xFE — 0xEE 스키마가 붙을 수 있는 대역


def _feed_activation(reg: R.SchemaRegistry, payload: bytes, t: float) -> None:
    """SCHEMA_ACTIVATION payload(프래그먼트를 이어붙인 것일 수 있다)를 레지스트리에 먹인다."""
    try:
        parts = EE.split_canonical(payload)
    except EE.SchemaError as e:
        reg.ee_errors.append(str(e))
        return
    for part in parts:
        reg.feed_0xEE(part, t)


def build_registry(records) -> R.SchemaRegistry:
    """파일 안의 스키마 정보를 시간 순서대로 먹인다 — **파일 끝 기준의 요약**이다.

    module_id 하나에 스키마 하나만 남으므로(나중 것이 이긴다) 세션 도중 스키마가 바뀐 파일은
    마지막 것만 보인다. 요약을 찍는 데는 이걸로 충분하지만 **값을 푸는 데 쓰면 안 된다** —
    행마다 그 시점의 스키마로 푸는 것은 `resolve_rows` 다.

    실제 수신 순서를 그대로 재생하므로, 스키마가 데이터보다 늦게 온 캡처도
    (사후에는) 전부 풀린다 — PLAN 4.1 의 data-before-schema 규약이 노린 것이다.
    """
    reg = R.SchemaRegistry()
    records = list(records)
    # activation 레코드가 있는 모듈은 그쪽이 같은 스키마를 이미 알려 준다. 스키마를 담은 0xEE 프레임을
    # 또 먹이면 같은 스키마가 두 번 완성된 것으로 세어 요약이 어리둥절해진다(completed=2).
    has_activation = {rec.fields["module_id"] for rec in records
                      if rec.rec_type == X.REC_SCHEMA_ACTIVATION}
    # 재조립 타임아웃(3초)의 기준 시계. DATA 레코드의 pc_time_us 를 쓰되, 그런 레코드가
    # 아직 없거나 SCHEMA_ACTIVATION 이 앞에 몰려 있으면 0 이 반복돼 타임아웃 판정이
    # 왜곡된다 — **단조 증가**를 강제한다 (감사 #11).
    t = 0.0
    for rec in records:
        if rec.rec_type == X.REC_DATA:
            t = max(t, rec.fields["pc_time_us"] / 1e6)
        if rec.rec_type == X.REC_SCHEMA_ACTIVATION:
            _feed_activation(reg, rec.payload, t)
        elif rec.rec_type == X.REC_DATA:
            mid = rec.fields["module_id"]
            if mid == R.MODULE_ID_SCHEMA_DESC:
                # 0xEE 헤더의 둘째 바이트가 target_module_id 다(재조립 전에도 읽힌다).
                # activation 이 없는 모듈 — 예전 파일 — 에서만 프레임이 유일한 출처다.
                if len(rec.payload) < 2 or rec.payload[1] not in has_activation:
                    reg.feed_0xEE(rec.payload, t)
            elif mid == R.MODULE_ID_USER_META:
                reg.feed_0xEF(rec.payload)
    return reg


def _fallback_registry(records) -> R.SchemaRegistry:
    """스키마(0xEE)로 못 푸는 행이 내려가는 곳 — 0xEF 이름 · 0x20 생성맵 · float32 가정.

    0xEE 를 일부러 **안 먹인다.** 먹이면 module_id 하나에 최종 스키마가 붙어서, 스키마가 없던
    구간의 행까지 그 스키마로 풀려 버린다(위 `build_registry` 를 값 디코딩에 쓰면 생기는 오류).
    0xEF 는 이름만 주고 float32 로 **가정**하는 출처라 시점을 따지지 않는다.
    """
    reg = R.SchemaRegistry()
    for rec in records:
        if rec.rec_type == X.REC_DATA and rec.fields["module_id"] == R.MODULE_ID_USER_META:
            reg.feed_0xEF(rec.payload)
    return reg


# ---------------------------------------------------------------------------
# 행마다 그 시점의 스키마 고르기
# ---------------------------------------------------------------------------

class ResolvedRow(NamedTuple):
    record: X.Record                    # DATA 레코드 원본 (바이트는 손대지 않는다)
    activation_id: int                  # 유효한 번호. 0 = 스키마 미상 (파일에서 0 이었거나 강등됐다)
    schema: Optional[EE.Schema]         # 이 행을 풀 스키마. None 이면 스키마로는 못 푼다
    downgraded: bool                    # 파일에는 0 이 아니었는데 교차검증에 실패해 0 으로 내렸다


class Resolution(NamedTuple):
    rows: list                          # ResolvedRow — DATA 레코드 순서 그대로, 하나도 빠지지 않는다
    issues: list                        # 사람이 읽을 문제 (스키마 파싱 실패 · 강등 · 재조립 실패)
    sessions: int                       # SESSION 레코드 수 (재연결 횟수 + 1)
    downgraded: int


def resolve_rows(records) -> Resolution:
    """DATA 레코드마다 **그 행을 풀 스키마**를 고른다 (이 파일의 docstring 의 1~3번 순서).

    파일을 앞에서 한 번 훑는다. 레코드를 버리지 않는다 — 결과 행 수는 DATA 레코드 수와 같다.
    """
    acts = {}          # activation_id -> (module_id, Schema 또는 None)
    in_force = {}      # module_id -> 지금 유효한 Schema (activation 레코드 또는 완성된 0xEE 프레임)
    pending = {}       # module_id -> 아직 스키마를 못 만난 미상(0) 행의 rows 인덱스들
    rows, issues = [], []
    reasm = EE.Reassembler()
    sessions = downgraded = 0
    t = 0.0

    def resolve_pending(mid, schema):
        for idx in pending.pop(mid, ()):
            rows[idx] = rows[idx]._replace(schema=schema)

    for rec in records:
        rtype = rec.rec_type

        if rtype == X.REC_SESSION:
            # 구간 경계 — 재연결로 다른 장치가 붙었을 수 있다. 앞 구간의 스키마는 넘기지 않고,
            # 스키마를 못 만난 채 구간이 끝난 미상 행은 그대로 미상으로 남긴다.
            sessions += 1
            in_force.clear()
            pending.clear()
            reasm = EE.Reassembler()

        elif rtype == X.REC_SCHEMA_ACTIVATION:
            aid, mid = rec.fields["activation_id"], rec.fields["module_id"]
            if aid in acts:
                issues.append("activation_id %d 가 두 번 나온다 (@%d) — 앞의 것을 쓴다" % (aid, rec.offset))
                continue
            schema = None
            try:
                schema = EE.schema_from_canonical(rec.payload)
                if (schema.module_id, schema.proto_ver, schema.struct_size, schema.schema_crc32) != (
                        mid, rec.fields["schema_proto_ver"], rec.fields["struct_size"],
                        rec.fields["schema_crc32"]):
                    raise EE.SchemaError("레코드 헤더와 payload 의 스키마가 다르다")
            except Exception as e:  # noqa: BLE001 — 깨진 스키마 하나가 파일 전체를 못 읽게 하면 안 된다
                issues.append("activation %d (0x%02X, @%d): %s" % (aid, mid, rec.offset, e))
                schema = None
            acts[aid] = (mid, schema)
            if schema is not None:
                in_force[mid] = schema
                resolve_pending(mid, schema)

        elif rtype == X.REC_DATA:
            f = rec.fields
            mid, aid = f["module_id"], f["activation_id"]
            t = max(t, f["pc_time_us"] / 1e6)

            if mid == R.MODULE_ID_SCHEMA_DESC:
                # 스키마를 프레임 그대로 적은 DATA. 새 파일에서는 뒤따르는 SCHEMA_ACTIVATION 이
                # 같은 것을 알려 주지만, 재연결 뒤 **똑같은 스키마가 다시 오면** 그 레코드는
                # 생략되므로(재사용) 이 프레임들만이 "다시 유효해졌다" 는 신호다. 예전 파일은
                # activation 레코드가 아예 없어서 이쪽이 유일한 출처다.
                try:
                    done = reasm.feed(rec.payload, t)
                except EE.SchemaError as e:
                    issues.append("0xEE 프레임 (@%d): %s" % (rec.offset, e))
                    done = None
                except Exception as e:  # noqa: BLE001
                    issues.append("0xEE 프레임 (@%d): 예상 못한 예외 %s: %s"
                                  % (rec.offset, type(e).__name__, e))
                    done = None
                if done is not None:
                    in_force[done.module_id] = done
                    resolve_pending(done.module_id, done)

            eff, schema, down = aid, None, False
            if aid != 0:
                known = acts.get(aid)
                if known is None or known[0] != mid:
                    # 모르는 activation 이거나 다른 모듈의 것 — 틀린 스키마로 풀지 않고, 행도 버리지
                    # 않는다. 미상으로 내려 원본 바이트로 남긴다.
                    # 이 대조 · 강등은 내보내기 층(이 함수)이 한다. xmlog.py 는 레코드를 파일에 적힌
                    # 그대로 돌려주는 파서로 두었다 — wire 계약 §3 의 'reader' 규칙이 사는 곳이 여기다.
                    eff, down = 0, True
                    downgraded += 1
                    if downgraded <= 5:
                        issues.append(
                            "DATA (@%d) 0x%02X 가 %s 을 참조 — activation_id=0 으로 내려 원본을 남긴다"
                            % (rec.offset, mid,
                               "activation %d (0x%02X)" % (aid, known[0]) if known
                               else "없는 activation %d" % aid))
                else:
                    schema = known[1]
            elif mid in _USER_MODULES:
                schema = in_force.get(mid)
                if schema is None:
                    pending.setdefault(mid, []).append(len(rows))     # 이 뒤에 스키마가 오면 푼다
            rows.append(ResolvedRow(rec, eff, schema, down))

    return Resolution(rows, issues, sessions, downgraded)


def schema_layouts(resolution: Resolution) -> dict:
    """module_id -> 그 모듈이 쓴 서로 다른 스키마(배치)의 수. 둘 이상이면 파일 안에서 스키마가 바뀐 것이다."""
    seen = {}
    for row in resolution.rows:
        if row.schema is not None:
            seen.setdefault(row.record.fields["module_id"], set()).add(
                (row.schema.struct_size, row.schema.fields))
    return {mid: len(s) for mid, s in seen.items()}


def _plan_files(resolution: Resolution, want_raw_hex: bool):
    """행마다 어느 파일로 갈지 미리 정한다 -> (행별 (module_id, 배치 번호), {module_id: 풀린 배치 수}).

    배치 번호 0 = 스키마로 못 푼 행, 1.. = 그 모듈이 쓴 스키마 배치를 처음 나온 순서대로. 파일 이름이
    "그 모듈에 풀린 스키마가 있나 · 몇 번째인가" 에 달려 있어서(못 푼 행이 먼저 나와도 기본 이름을
    차지하지 않는다) 첫 행을 쓰기 전에 모듈 전체를 봐 둬야 한다.

    배치 = (struct_size, 필드 목록). 같은 배치는 activation 이 달라도(struct_name 만 다른 경우 등)
    열 이름과 값의 뜻이 같으므로 한 파일에 둔다 — 구분은 activation_id 열이 한다.
    """
    by_schema = {}                          # id(Schema) -> 배치 번호. 행마다 큰 튜플을 해시하지 않는다
    numbers = {}                            # module_id -> {배치: 번호}
    interned = {}                           # 같은 (module_id, 번호) 는 튜플 하나를 나눠 쓴다 — 행 수만큼 새로 만들지 않는다
    keys = []
    for row in resolution.rows:
        mid = row.record.fields["module_id"]
        schema = None if (want_raw_hex or mid in _RAW_ONLY) else row.schema
        if schema is None:
            n = 0
        else:
            n = by_schema.get(id(schema))
            if n is None:
                nums = numbers.setdefault(mid, {})
                n = by_schema[id(schema)] = nums.setdefault(
                    (schema.struct_size, schema.fields), len(nums) + 1)
        key = (mid, n)
        keys.append(interned.setdefault(key, key))
    return keys, {mid: len(nums) for mid, nums in numbers.items()}


def export_csv(res: X.ScanResult, out_dir: str, stem: str, want_raw_hex: bool,
               resolution: Optional[Resolution] = None) -> list:
    """모듈별 CSV 를 쓴다. 반환: 쓴 파일 경로들 (만든 순서).

    파일 이름은 `<stem>_<라벨>.csv` 다(`_module_label`). 한 모듈의 행이 서로 다른 스키마로 풀리면
    처음 쓰인 스키마가 기본 이름이고 그다음은 `_schema2`, `_schema3` 로 나뉜다. 스키마로 못 푼 행은
    그 모듈에 풀린 스키마가 있으면 `_unresolved` 로 간다(어느 쪽이 먼저 나왔는지와 무관하다).
    `want_raw_hex` 면 해석 없이 payload 를 hex 로 — 이때는 모듈 하나가 파일 하나다.
    """
    if resolution is None:                  # main() 이 요약에 쓰려고 이미 계산했다면 그걸 받는다
        resolution = resolve_rows(res.records)
    reg = _fallback_registry(res.records)
    os.makedirs(out_dir, exist_ok=True)

    # 파일 하나 = (module_id, 배치 번호)
    keys, n_layouts = _plan_files(resolution, want_raw_hex)
    groups = {}
    written = []
    try:
        for row, key in zip(resolution.rows, keys):
            r = row.record
            mid, num = key

            g = groups.get(key)
            if g is None:
                if num:
                    cs = R.from_ee_schema(row.schema)
                elif want_raw_hex or mid in _RAW_ONLY:
                    cs = None
                else:
                    cs = reg.get(mid, len(r.payload))
                if num == 0:
                    suffix = "_unresolved" if mid in n_layouts else ""
                else:
                    suffix = "" if num == 1 else "_schema%d" % num
                path = os.path.join(out_dir, "%s_%s%s.csv" % (stem, _module_label(mid), suffix))
                f = open(path, "w", encoding="utf-8", newline="")
                cols = list(cs.names) if cs is not None else ["payload_hex"]
                f.write(",".join(["pc_time_us", "seq_id", "activation_id"] + cols) + "\n")
                g = groups[key] = {"mid": mid, "num": num, "path": path, "f": f,
                                   "cs": cs, "cols": cols, "count": 0, "mismatched": 0}
                written.append(path)

            f, cs, cols = g["f"], g["cs"], g["cols"]
            prefix = "%d,%d,%d" % (r.fields["pc_time_us"], r.fields["seq_id"], row.activation_id)
            if cs is None:
                f.write(prefix + "," + r.payload.hex() + "\n")
            else:
                vals = cs.decode(r.payload)
                if vals is None or len(vals) != len(cols):
                    # 길이가 흔들리면 그 행만 hex 로 떨군다 — 열이 밀린 CSV 보다 낫다.
                    g["mismatched"] += 1
                    pad = [""] * (len(cols) - 1)
                    f.write(prefix + "," + ",".join(pad + [r.payload.hex()]) + "\n")
                else:
                    f.write(prefix + "," + ",".join(_fmt(v) for v in vals) + "\n")
            g["count"] += 1
    finally:
        for g in groups.values():
            g["f"].close()

    print("CSV:")
    # 모듈 순, 그 안에서는 풀린 스키마 순(기본 이름 -> _schema2 …) 다음에 못 푼 행
    for g in sorted(groups.values(), key=lambda g: (g["mid"], g["num"] == 0, g["num"])):
        cs = g["cs"]
        if cs is None:
            note = "raw hex" + (" (요청)" if want_raw_hex else "")
        else:
            note = "[%s] %s" % (cs.source, cs.detail)
            if cs.assumed_float32:
                note += "  ⚠ 타입 미상 — float32 가정"
        extra = "  ⚠ %d행 길이 불일치 → hex" % g["mismatched"] if g["mismatched"] else ""
        print("  %-26s %7d rows   %s%s" % (os.path.basename(g["path"]), g["count"], note, extra))
    if not want_raw_hex:
        for mid, n in sorted(schema_layouts(resolution).items()):
            if n > 1:
                print("  ⚠ 0x%02X 는 이 파일에서 스키마가 %d종류 쓰였다 — 행마다 그 시점의 스키마로 풀고 "
                      "파일을 나눴다 (activation_id 열로 구분)" % (mid, n))
    if resolution.downgraded:
        print("  ⚠ activation 참조가 맞지 않아 activation_id=0 으로 내린 행 %d개 "
              "(원본 바이트는 그대로 — 스키마로 풀지 않았다)" % resolution.downgraded)
    return written


def _fmt(v) -> str:
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, float):
        return "%.6g" % v
    return str(v)


# ---------------------------------------------------------------------------
# hex 행 — 값 대신 받은 바이트를 적은 행 (읽는 쪽)
# ---------------------------------------------------------------------------
# 위 export_csv 와 실시간 CSV(cdc_phai_receiver) 는 해석이 헤더와 안 맞는 프레임을 **열이
# 밀린 행으로 두지 않고** 이렇게 적는다: 값 칸은 비우고, 마지막 칸에 payload 를 hex 로.
# 이 CSV 를 그래프로 읽는 쪽(cdc_csv_reviewer 등)은 그런 행을 값으로 읽으면 안 된다 —
# 빈 칸은 NaN 이 되어 이상치로 처리되고, 그 뒤 데이터가 통째로 잘려 나간다.
# 그렇다고 그 행을 **통째로 버려도** 안 된다. 앞쪽 칸(time_s · seq_id · tx_drops)은 멀쩡한 값이다 —
# 행을 빼면 그 자리가 없던 패킷 손실(seq 구멍)로 보이고, 그 행이 실었던 Tx drop 은 합계에서 빠진다.
_HEX_CELL = re.compile(r"[0-9a-fA-F]+")


def is_hex_row(cells, first_data_col: int) -> bool:
    """CSV 한 행(칸 목록)이 '값 대신 payload 를 hex 로 적은 행' 인가.

    `first_data_col` 은 채널 값이 시작하는 열 번호다(실시간 CSV 는 5: time_s, pc_time_s,
    seq_id, module_id, tx_drops 다음). 판정은 셋이 모두 맞을 때다.

    * 마지막 칸이 16진 문자열이다. payload 는 와이어에서 4바이트 단위라 8자의 배수다.
    * 그 앞의 값 칸은 전부 비어 있다.
    * 숫자 행의 마지막 칸은 `%.6f` 라 항상 소수점이 있거나 `nan`/`inf` 다 — 그래서 값이 하나뿐인
      module 에서도 16진 문자열과 숫자를 혼동하지 않는다.
    """
    if len(cells) <= first_data_col:
        return False
    last = cells[-1].strip()
    if not last or len(last) % 8 or not _HEX_CELL.fullmatch(last):
        return False
    return all(not c.strip() for c in cells[first_data_col:-1])


class HexRowFilter:
    """CSV 줄 스트림에서 hex 행의 **값 칸만** `nan` 으로 바꿔 돌려주고, 몇 번째 행이었는지 기억한다.

        flt = HexRowFilter(first_data_col=5)
        arr = np.genfromtxt(flt(f), delimiter=",")     # f = 머리글 줄 다음부터의 파일 객체
        arr[flt.rows, 5:]                               # hex 행이었던 행들의 값 칸 — 전부 nan

    행은 버리지 않는다. 앞쪽 칸(time_s · seq_id · tx_drops …)이 그대로 남으므로 seq 구멍이나
    Tx drop 합계 같은 통계는 hex 행까지 넣어 셀 수 있고, 값은 `flt.rows` 로 골라 이상치
    검사와 그래프에서만 뺀다. `rows` 는 `genfromtxt` 가 돌려주는 배열의 행 번호(0부터)다 —
    빈 줄은 거기서도 행으로 안 세므로 여기서도 안 센다. `#` 로 시작하는 주석 줄은 따로 처리하지
    않는다(genfromtxt 는 건너뛰지만 이 도구들이 쓰는 CSV 에는 주석 줄이 없다).
    """

    def __init__(self, first_data_col: int):
        self.first_data_col = first_data_col
        self.rows = []

    def __call__(self, lines):
        self.rows = []
        n = 0                                   # 지금까지 낸 행 수 = 다음 행이 배열에서 갖는 번호
        for line in lines:
            if not line.strip():
                continue
            cells = line.rstrip("\r\n").split(",")
            if is_hex_row(cells, self.first_data_col):
                self.rows.append(n)
                line = ",".join(cells[:self.first_data_col]
                                + ["nan"] * (len(cells) - self.first_data_col)) + "\n"
            n += 1
            yield line


def dump(res: X.ScanResult, limit: int, downgraded=frozenset()) -> None:
    """`downgraded` = 참조가 맞지 않아 activation_id=0 으로 내려 읽은 DATA 의 파일 오프셋들."""
    for i, r in enumerate(res.records):
        if i >= limit:
            print("  ... +%d more" % (len(res.records) - limit))
            break
        name = X.REC_TYPE_NAMES.get(r.rec_type, "?")
        if r.rec_type == X.REC_DATA:
            detail = ("module=0x%02X seq=%d act=%d payload=%dB"
                      % (r.fields["module_id"], r.fields["seq_id"],
                         r.fields["activation_id"], len(r.payload)))
            if r.offset in downgraded:
                detail += "  ⚠ 참조가 맞지 않아 act=0 으로 내려 읽는다"
        elif r.rec_type == X.REC_GAP:
            detail = ("%s %d..%d lost=%d" % (r.fields["reason_name"], r.fields["from_seq"],
                                             r.fields["to_seq"], r.fields["lost_count"]))
        elif r.rec_type == X.REC_SESSION:
            detail = ("serial=%r fw=%r map=%r link_epoch=%d"
                      % (r.fields["device_usb_serial"], r.fields["fw_build_id"],
                         r.fields["total_data_map_version"], r.fields["link_epoch"]))
        else:
            detail = ("module=0x%02X act=%d struct=%dB crc=%08x payload=%dB"
                      % (r.fields["module_id"], r.fields["activation_id"],
                         r.fields["struct_size"], r.fields["schema_crc32"], len(r.payload)))
        print("  @%-9d %-18s %s" % (r.offset, name, detail))


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    ap = argparse.ArgumentParser(description="Read/export an .xmlog capture")
    ap.add_argument("path")
    ap.add_argument("--csv", metavar="DIR", help="모듈별 CSV 를 이 디렉토리에 쓴다")
    ap.add_argument("--dump", type=int, metavar="N", help="앞 N개 레코드를 사람이 읽게 출력")
    ap.add_argument("--raw-hex", action="store_true",
                    help="디코딩하지 않고 payload 를 hex 그대로 (해석을 믿지 못할 때)")
    args = ap.parse_args()

    try:
        res = X.read_file(args.path)
    except X.LogError as e:
        print("열 수 없다: %s" % e)
        return 2

    print("%s" % args.path)
    print("  " + X.summarize(res))
    if res.stopped_reason:
        print("  ⚠ 파일이 온전하지 않다 — 위 valid 지점까지만 복구했다.")
        print("    (크래시/중단으로 마지막 레코드가 잘린 경우다. 앞부분은 그대로 쓸 수 있다)")

    sess = [r for r in res.records if r.rec_type == X.REC_SESSION]
    if sess:
        f = sess[0].fields
        # 펌웨어 빌드는 아직 알아낼 경로가 없어 캡처가 "unknown" 을 적는다. 예전 파일은 0 으로 비어
        # 있는데 그것도 같은 뜻이다.
        print("  session: fw=%r  total_data_map=%r  boot_epoch=%d"
              % (f["fw_build_id"] or "unknown", f["total_data_map_version"], f["boot_epoch"]))
        for n, s in enumerate(sess[1:], 2):
            print("  session #%d (다시 연결): link_epoch=%d — 이 앞의 스키마는 이 뒤로 넘기지 않는다"
                  % (n, s.fields["link_epoch"]))

    reg = build_registry(res.records)
    known = reg.known()
    if known or reg.ee_errors or reg.ef_errors:
        print("  스키마:")
        for line in reg.summary().splitlines():
            print("    " + line)

    resolution = resolve_rows(res.records)
    for mid, n in sorted(schema_layouts(resolution).items()):
        if n > 1:
            print("  ⚠ 0x%02X 는 이 파일에서 스키마가 %d종류 쓰였다 (위 요약은 마지막 것만 보인다)" % (mid, n))
    if resolution.downgraded:
        print("  ⚠ activation 참조가 맞지 않는 DATA %d개 — activation_id=0 으로 내려 원본을 남긴다"
              % resolution.downgraded)
    if resolution.issues:
        print("  스키마 기록 문제 %d건 (첫 건: %s)" % (len(resolution.issues), resolution.issues[0]))

    if args.dump:
        print("records:")
        dump(res, args.dump,
             {row.record.offset for row in resolution.rows if row.downgraded})

    if args.csv:
        stem = os.path.splitext(os.path.basename(args.path))[0]
        export_csv(res, args.csv, stem, args.raw_hex, resolution)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
