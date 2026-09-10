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
"""
from __future__ import annotations

import argparse
import os
import sys

import xmlog as X
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


def build_registry(records) -> R.SchemaRegistry:
    """파일 안의 스키마 정보를 시간 순서대로 먹인다.

    실제 수신 순서를 그대로 재생하므로, 스키마가 데이터보다 늦게 온 캡처도
    (사후에는) 전부 풀린다 — PLAN 4.1 의 data-before-schema 규약이 노린 것이다.
    """
    reg = R.SchemaRegistry()
    # 재조립 타임아웃(3초)의 기준 시계. DATA 레코드의 pc_time_us 를 쓰되, 그런 레코드가
    # 아직 없거나 SCHEMA_ACTIVATION 이 앞에 몰려 있으면 0 이 반복돼 타임아웃 판정이
    # 왜곡된다 — **단조 증가**를 강제한다 (감사 #11).
    t = 0.0
    for rec in records:
        if rec.rec_type == X.REC_DATA:
            t = max(t, rec.fields["pc_time_us"] / 1e6)
        if rec.rec_type == X.REC_SCHEMA_ACTIVATION:
            reg.feed_0xEE(rec.payload, t)
        elif rec.rec_type == X.REC_DATA:
            mid = rec.fields["module_id"]
            if mid == R.MODULE_ID_SCHEMA_DESC:
                reg.feed_0xEE(rec.payload, t)
            elif mid == R.MODULE_ID_USER_META:
                reg.feed_0xEF(rec.payload)
    return reg


def export_csv(res: X.ScanResult, out_dir: str, stem: str, want_raw_hex: bool) -> list:
    reg = build_registry(res.records)
    os.makedirs(out_dir, exist_ok=True)

    files, cols_of, counts, mismatched, chan = {}, {}, {}, {}, {}
    written = []
    try:
        for r in res.records:
            if r.rec_type != X.REC_DATA:
                continue
            mid = r.fields["module_id"]

            if mid not in files:
                cs = None
                if not want_raw_hex and mid not in _RAW_ONLY:
                    cs = reg.get(mid, len(r.payload))
                chan[mid] = cs
                path = os.path.join(out_dir, "%s_%s.csv" % (stem, _module_label(mid)))
                f = open(path, "w", encoding="utf-8", newline="")
                cols = list(cs.names) if cs is not None else ["payload_hex"]
                cols_of[mid] = cols
                f.write(",".join(["pc_time_us", "seq_id", "activation_id"] + cols) + "\n")
                files[mid] = (path, f)
                counts[mid] = 0
                mismatched[mid] = 0
                written.append(path)

            _path, f = files[mid]
            cs = chan[mid]
            prefix = "%d,%d,%d" % (r.fields["pc_time_us"], r.fields["seq_id"],
                                   r.fields["activation_id"])
            if cs is None:
                f.write(prefix + "," + r.payload.hex() + "\n")
            else:
                vals = cs.decode(r.payload)
                if vals is None or len(vals) != len(cols_of[mid]):
                    # 길이가 흔들리면 그 행만 hex 로 떨군다 — 열이 밀린 CSV 보다 낫다.
                    mismatched[mid] += 1
                    pad = [""] * (len(cols_of[mid]) - 1)
                    f.write(prefix + "," + ",".join(pad + [r.payload.hex()]) + "\n")
                else:
                    f.write(prefix + "," + ",".join(_fmt(v) for v in vals) + "\n")
            counts[mid] += 1
    finally:
        for _mid, (_p, f) in files.items():
            f.close()

    print("CSV:")
    for mid in sorted(files):
        path, _f = files[mid]
        cs = chan[mid]
        if cs is None:
            note = "raw hex" + (" (요청)" if want_raw_hex else "")
        else:
            note = "[%s] %s" % (cs.source, cs.detail)
            if cs.assumed_float32:
                note += "  ⚠ 타입 미상 — float32 가정"
        extra = "  ⚠ %d행 길이 불일치 → hex" % mismatched[mid] if mismatched.get(mid) else ""
        print("  %-26s %7d rows   %s%s" % (os.path.basename(path), counts[mid], note, extra))
    return written


def _fmt(v) -> str:
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, float):
        return "%.6g" % v
    return str(v)


def dump(res: X.ScanResult, limit: int) -> None:
    for i, r in enumerate(res.records):
        if i >= limit:
            print("  ... +%d more" % (len(res.records) - limit))
            break
        name = X.REC_TYPE_NAMES.get(r.rec_type, "?")
        if r.rec_type == X.REC_DATA:
            detail = ("module=0x%02X seq=%d act=%d payload=%dB"
                      % (r.fields["module_id"], r.fields["seq_id"],
                         r.fields["activation_id"], len(r.payload)))
        elif r.rec_type == X.REC_GAP:
            detail = ("%s %d..%d lost=%d" % (r.fields["reason_name"], r.fields["from_seq"],
                                             r.fields["to_seq"], r.fields["lost_count"]))
        elif r.rec_type == X.REC_SESSION:
            detail = ("serial=%r fw=%r map=%r" % (r.fields["device_usb_serial"],
                                                  r.fields["fw_build_id"],
                                                  r.fields["total_data_map_version"]))
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
        print("  session: fw=%r  total_data_map=%r  boot_epoch=%d"
              % (f["fw_build_id"], f["total_data_map_version"], f["boot_epoch"]))

    reg = build_registry(res.records)
    known = reg.known()
    if known or reg.ee_errors or reg.ef_errors:
        print("  스키마:")
        for line in reg.summary().splitlines():
            print("    " + line)

    if args.dump:
        print("records:")
        dump(res, args.dump)

    if args.csv:
        stem = os.path.splitext(os.path.basename(args.path))[0]
        export_csv(res, args.csv, stem, args.raw_hex)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
