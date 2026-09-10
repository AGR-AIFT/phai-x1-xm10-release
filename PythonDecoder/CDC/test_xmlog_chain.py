#!/usr/bin/env python3
"""와이어 -> 저장 -> 내보내기 전 구간 회귀 시험. 보드도 시리얼 포트도 없이 돈다.

    python test_xmlog_chain.py

앞의 시험들이 조각을 본다면(`test_xmlog.py` = 바이트 ABI, `test_schema.py` = 스키마,
`test_total_data_decoder.py` = 0x20 디코드), 이 시험은 **사용자가 실제로 겪는 경로 전체**다:

    바이트 스트림 -> COBS -> PhAI 프레임 -> FrameRouter -> .xmlog -> CSV

여기서만 잡히는 것
------------------
조각들이 각자 맞아도 이어 붙이면 틀리는 자리가 있다.

* 0x20 페이로드는 와이어에서 368 B(4바이트 패딩)다. 저장할 때 365 로 자르거나
  CSV 단계에서 365 를 기대하면 조각 시험은 전부 통과하고 이 시험만 실패한다.
* `0xEF` 메타는 데이터보다 **늦게** 올 수 있다. 그래도 사후 내보내기에서는
  전부 이름이 붙어야 한다(PLAN 4.1 data-before-schema).

그리고 이 파일은 **시리얼 포트를 열지 않는다.** 수신 루프에 로직을 넣으면 보드 없이
검증할 수 없어서, 정책을 `xmlog_capture.XmLogCapture` 로 빼 둔 덕분이다.
"""
import os
import shutil
import struct
import sys
import tempfile

from frame_router import (
    PHAI_SOF, PHAI_MODULE_TOTAL_DATA, PHAI_MODULE_USER_META,
    crc16_ccitt, cobs_decode, parse_phai_frame, FrameRouter,
)
from test_frame_router import cobs_encode
import xmlog as X
from xmlog_capture import XmLogCapture
import xmlog_export as EXP
import xm_total_data_map as M


USER_MODULE = 0xF0

USER_META_JSON = ('[{"name":"H10 Connected","unit":"bool"},'
                  '{"name":"Left Hip Angle","unit":"deg"},'
                  '{"name":"Right Hip Angle","unit":"deg"},'
                  '{"name":"Forward Velocity","unit":"m/s"}]')


def _wire(seq_id, module_id, payload, status=0):
    """FW 규약 그대로: payload 를 4바이트 배수로 올려 LEN(4바이트 단위)에 담는다."""
    padded = bytes(payload) + bytes((-len(payload)) % 4)
    body = bytearray([PHAI_SOF, len(padded) // 4, seq_id & 0xFF, (seq_id >> 8) & 0xFF,
                      module_id, status])
    body.extend(padded)
    crc = crc16_ccitt(bytes(body))
    body.append(crc & 0xFF)
    body.append((crc >> 8) & 0xFF)
    return cobs_encode(bytes(body)) + bytes([0])


def _total_payload(values):
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

    fails = []

    def check(cond, msg):
        if not cond:
            fails.append(msg)

    tmp = tempfile.mkdtemp(prefix="xmlog_chain_")
    try:
        log_path = os.path.join(tmp, "session.xmlog")

        # ---- 1. 와이어 만들기 ------------------------------------------------
        # seq 0,1 = 0x20
        # seq 2   = 0xF0 데이터  <- 메타보다 **먼저** 온다 (실제 FW 순서)
        # seq 3   = 0xEF 메타
        # seq 6   = 0xF0 데이터  (4,5 를 건너뛰어 손실 2)
        stream = bytearray()
        stream += _wire(0, PHAI_MODULE_TOTAL_DATA,
                        _total_payload({"xm_loop_count": 1000, "leftHipAngle": 16384}))
        stream += _wire(1, PHAI_MODULE_TOTAL_DATA,
                        _total_payload({"xm_loop_count": 1001, "leftHipAngle": -16384}))
        stream += _wire(2, USER_MODULE, struct.pack("<4f", 1.5, -2.5, 3.25, 0.0))
        stream += _wire(3, PHAI_MODULE_USER_META,
                        bytes([USER_MODULE]) + USER_META_JSON.encode("utf-8"))
        stream += _wire(6, USER_MODULE, struct.pack("<4f", 9.0, 8.0, 7.0, 6.0))

        # ---- 2. 수신기와 같은 경로로 풀어 저장 --------------------------------
        router = FrameRouter()
        cap = XmLogCapture(log_path, fw_build_id="fw-test",
                           total_data_map_version=M.DATA_MAP_VERSION, flush_every=1)
        buf = bytearray(stream)
        pc_us = 0
        parsed = 0
        while True:
            d = buf.find(0)
            if d < 0:
                break
            if d > 0:
                raw = cobs_decode(bytes(buf[:d]))
                pkt, err = parse_phai_frame(raw, 0.0)
                check(err is None, "parse 실패: %r" % err)
                if pkt is not None:
                    router.route(pkt)
                    pc_us += 1000
                    cap.on_frame(pkt, pc_us)
                    parsed += 1
            del buf[:d + 1]
        cap.close()
        check(parsed == 5, "파싱된 프레임 %d (5 여야)" % parsed)

        # ---- 3. 저장된 것 확인 ----------------------------------------------
        res = X.read_file(log_path)
        check(res.stopped_reason is None, "파일이 온전하지 않다: %s" % res.stopped_reason)
        kinds = [r.rec_type for r in res.records]
        check(kinds.count(X.REC_SESSION) == 1, "SESSION %d" % kinds.count(X.REC_SESSION))
        check(kinds.count(X.REC_DATA) == 5, "DATA %d (5 여야)" % kinds.count(X.REC_DATA))
        check(kinds.count(X.REC_GAP) == 1, "GAP %d (1 이어야)" % kinds.count(X.REC_GAP))

        gaps = [r for r in res.records if r.rec_type == X.REC_GAP]
        if gaps:
            g = gaps[0].fields
            check(g["lost_count"] == 2, "손실 %d (2 여야: seq 4,5)" % g["lost_count"])
            check((g["from_seq"], g["to_seq"]) == (3, 6), "gap %r" % ((g["from_seq"], g["to_seq"]),))
        check(cap.lost_total == 2, "capture lost_total %d" % cap.lost_total)

        # 0x20 페이로드가 **368 B 그대로** 저장돼야 한다 (365 로 자르면 안 된다)
        d20 = [r for r in res.records
               if r.rec_type == X.REC_DATA and r.fields["module_id"] == PHAI_MODULE_TOTAL_DATA]
        check(len(d20) == 2, "0x20 DATA %d" % len(d20))
        if d20:
            check(len(d20[0].payload) == 368,
                  "0x20 payload %d B (368 이어야 — 4바이트 패딩)" % len(d20[0].payload))

        # 0xEF 메타도 버려지지 않고 파일 안에 있어야 한다
        dEF = [r for r in res.records
               if r.rec_type == X.REC_DATA and r.fields["module_id"] == PHAI_MODULE_USER_META]
        check(len(dEF) == 1, "0xEF DATA %d (1 이어야 — 메타를 버리면 이름을 잃는다)" % len(dEF))

        acts = {r.fields["activation_id"] for r in res.records if r.rec_type == X.REC_DATA}
        check(acts == {0}, "activation_id %r (0xEE 스키마 없이 받았으면 전부 0)" % (acts,))

        # ---- 4. 레지스트리가 파일만 보고 채널을 재구성하는가 -------------------
        reg = EXP.build_registry(res.records)
        cs = reg.get(USER_MODULE, 16)
        check(cs is not None and cs.source == "0xEF",
              "0xF0 출처 %r (0xEF 여야)" % (cs.source if cs else None))
        if cs is not None:
            check(cs.names == ["H10 Connected", "Left Hip Angle",
                               "Right Hip Angle", "Forward Velocity"], cs.names)
            check(cs.assumed_float32 is True, "0xEF 는 타입 미상 표시가 있어야 한다")

        # ---- 5. CSV 내보내기 -------------------------------------------------
        out = os.path.join(tmp, "csv")
        written = EXP.export_csv(res, out, "session", want_raw_hex=False)
        names = sorted(os.path.basename(p) for p in written)
        check(names == ["session_meta_0xEF.csv", "session_total_0x20.csv",
                        "session_user_0xF0.csv"], "CSV 파일 %r" % (names,))

        # 0x20 CSV — 197 채널이 이름으로
        p20 = os.path.join(out, "session_total_0x20.csv")
        with open(p20, encoding="utf-8") as f:
            hdr = f.readline().rstrip("\n").split(",")
            rows = [ln.rstrip("\n").split(",") for ln in f if ln.strip()]
        check(len(hdr) == 3 + 197, "0x20 헤더 %d 열 (3 + 197 이어야)" % len(hdr))
        check("leftHipAngle" in hdr, "leftHipAngle 열이 없다 — 맵이 안 붙었다")
        check(len(rows) == 2, "0x20 행 %d" % len(rows))
        if rows and "leftHipAngle" in hdr:
            i = hdr.index("leftHipAngle")
            check(float(rows[0][i]) == 360.0, "leftHipAngle[0] = %s" % rows[0][i])
            check(float(rows[1][i]) == -360.0, "leftHipAngle[1] = %s" % rows[1][i])
        for r in rows:
            check(len(r) == len(hdr), "행 길이 %d != 헤더 %d — 열이 밀렸다" % (len(r), len(hdr)))

        # 0xF0 CSV — **메타가 데이터보다 늦게 왔는데도** 두 행 다 이름이 붙어야 한다
        pf0 = os.path.join(out, "session_user_0xF0.csv")
        with open(pf0, encoding="utf-8") as f:
            hdr2 = f.readline().rstrip("\n").split(",")
            rows2 = [ln.rstrip("\n").split(",") for ln in f if ln.strip()]
        check(hdr2 == ["pc_time_us", "seq_id", "activation_id",
                       "H10 Connected", "Left Hip Angle",
                       "Right Hip Angle", "Forward Velocity"],
              "0xF0 헤더 %r — 메타가 늦게 와도 사후엔 붙어야 한다" % (hdr2,))
        check(len(rows2) == 2, "0xF0 행 %d" % len(rows2))
        if len(rows2) == 2:
            check(float(rows2[0][4]) == -2.5, "Left Hip Angle 첫 행 = %s" % rows2[0][4])
            check(float(rows2[1][6]) == 6.0, "Forward Velocity 둘째 행 = %s" % rows2[1][6])

        # 0xEF 자체는 값으로 펴지 않고 원본을 남긴다
        pef = os.path.join(out, "session_meta_0xEF.csv")
        with open(pef, encoding="utf-8") as f:
            hdr3 = f.readline().rstrip("\n").split(",")
        check(hdr3[-1] == "payload_hex", "0xEF 는 raw 로 남아야 한다: %r" % (hdr3,))

        # ---- 6. raw-hex 경로 — 해석을 믿지 못할 때 데이터는 살아 있어야 한다 ----
        out2 = os.path.join(tmp, "csv_hex")
        EXP.export_csv(res, out2, "session", want_raw_hex=True)
        with open(os.path.join(out2, "session_total_0x20.csv"), encoding="utf-8") as f:
            h3 = f.readline().rstrip("\n").split(",")
            r3 = f.readline().rstrip("\n").split(",")
        check(h3[-1] == "payload_hex", "raw-hex 헤더 %r" % (h3,))
        check(len(r3[-1]) == 368 * 2, "hex 길이 %d (368 B = 736 자)" % len(r3[-1]))

        # ---- 7. GUI 워커도 같은 파일을 쓴다 (시리얼 포트 없이 워커 함수만) --------
        # 워커의 _parse_frame 은 포트를 열지 않는다 — 바이트를 직접 넣을 수 있다.
        # 이 경로가 없던 동안 GUI 는 CSV 만 남겼고 무손실 저장은 CLI 전용이었다.
        _gui_worker_writes_xmlog(tmp, stream, check)

    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if fails:
        for m in fails:
            print("  FAIL  " + m)
        print("")
        print("%d FAILED" % len(fails))
        return 1
    print("  wire -> .xmlog -> CSV 전 구간 통과")
    print("  (0x20 368 B 보존 / 손실 2 를 GAP 으로 / 197채널 이름 / "
          "0xEF 가 늦게 와도 사후 라벨링 / raw-hex 대체 경로 / GUI 워커 .xmlog)")
    return 0


def _gui_worker_writes_xmlog(tmp, stream, check):
    try:
        import cdc_phai_receiver as RX
    except ImportError as e:
        print("  [SKIP] GUI 워커 시험 — 수신기 import 불가 (%s). PyQt5 없는 환경." % e)
        return

    path = os.path.join(tmp, "gui.xmlog")
    cap = XmLogCapture(path, fw_build_id="fw-test", flush_every=1)
    w = RX.PhAISerialWorker("COM_NONE", capture=cap)     # run() 은 부르지 않는다

    n = 0
    buf = bytearray(stream)
    while True:
        d = buf.find(0)
        if d < 0:
            break
        if d > 0:
            w._parse_frame(cobs_decode(bytes(buf[:d])), 0.001 * (n + 1))
            n += 1
        del buf[:d + 1]
    # 워커의 finally 가 하는 일을 여기서 직접
    w.capture.close()

    check(w.good == 5, "GUI 워커 통과 프레임 %d (5 여야)" % w.good)
    res = X.read_file(path)
    check(res.stopped_reason is None, "GUI .xmlog 가 온전하지 않다: %s" % res.stopped_reason)
    kinds = [r.rec_type for r in res.records]
    check(kinds.count(X.REC_DATA) == 5, "GUI .xmlog DATA %d (5 여야)" % kinds.count(X.REC_DATA))
    check(kinds.count(X.REC_GAP) == 1, "GUI .xmlog GAP %d (1 이어야)" % kinds.count(X.REC_GAP))
    # 시스템 채널(0x20/0xEF)까지 파일에 있어야 한다 — 화면엔 안 그려도 파일엔 남는다
    mids = {r.fields["module_id"] for r in res.records if r.rec_type == X.REC_DATA}
    check(mids == {PHAI_MODULE_TOTAL_DATA, PHAI_MODULE_USER_META, USER_MODULE},
          "GUI .xmlog 모듈 %r — 시스템 채널이 빠졌다" % (sorted(mids),))


if __name__ == "__main__":
    raise SystemExit(main())
