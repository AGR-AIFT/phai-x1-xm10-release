#!/usr/bin/env python3
"""수신한 PhAI 프레임을 `.xmlog` 로 눕히는 다리.

`frame_router.PhAIFrame` 하나를 받아 레코드 하나를 적는다. 그게 전부다.

**왜 GUI/CLI 파일 안에 두지 않았나** — 시리얼 포트를 열어야만 실행되는 코드 안에
로직을 넣으면 보드 없이는 한 줄도 시험할 수 없다. 실제로 그 방식으로 넣었던
`--total-data` 경로가 `NameError` 를 안은 채 "검증 통과" 로 보고된 적이 있다
(2026-09-10). 정책은 여기 두고, 수신 루프는 이 클래스를 부르기만 한다.

저장 규약 (PLAN 4.1 "data-before-schema", 4.6)
----------------------------------------------
* CRC 를 통과한 프레임은 **스키마를 몰라도 즉시 적는다** (`activation_id = 0`).
  해석은 나중에 `xmlog_export.py` 가 한다.
* 시스템 채널(0x20 등)도 사용자 채널과 **똑같이 적는다.** 화면에 안 그린다고
  버리면 나중에 되짚을 수 없다.
* 손실은 GAP 레코드로 파일 안에 남긴다 — 파일만 보고도 무슨 일이 있었는지 알아야 한다.
"""
from __future__ import annotations

import xmlog as X


class XmLogCapture:
    """PhAIFrame -> .xmlog. 상태는 카운터와 직전 seq 뿐이다.

        cap = XmLogCapture(path, fw_build_id="fw-2.8", total_data_map_version="2.8")
        cap.on_frame(frame, pc_time_us)     # 매 프레임
        cap.on_gap(X.GAP_QUEUE_OVERFLOW, ...)   # 큐를 버렸을 때
        cap.close()
    """

    __slots__ = ("w", "path", "frames", "gaps", "lost_total", "_last_seq", "_started")

    def __init__(self, path, device_usb_serial="", fw_build_id="",
                 total_data_map_version="", boot_epoch=0, link_epoch=0,
                 flush_every=256):
        self.w = X.XmLogWriter(path, flush_every=flush_every)
        self.path = self.w.path
        self.w.session(device_usb_serial=device_usb_serial,
                       fw_build_id=fw_build_id,
                       total_data_map_version=total_data_map_version,
                       boot_epoch=boot_epoch, link_epoch=link_epoch)
        self.frames = 0
        self.gaps = 0
        self.lost_total = 0
        self._last_seq = -1
        self._started = False

    # -----------------------------------------------------------------
    def on_frame(self, frame, pc_time_us: int) -> None:
        """CRC 를 통과한 프레임 하나.

        seq 갭은 여기서 **직접** 센다. FrameRouter 의 원장을 다시 읽지 않는 이유는,
        원장이 화면 표시용으로 클램프·리싱크 규칙을 갖고 있어서 "파일에 남길 사실"
        과 "사람에게 보여줄 통계" 가 다를 수 있기 때문이다. 파일은 사실만 적는다.
        """
        seq = frame.seq_id
        if self._started:
            delta = (seq - self._last_seq) & 0xFFFF
            if delta == 0:
                pass                      # 중복 또는 65536 배수 wrap — 손실 아님
            elif delta >= 0x8000:
                # 경계는 frame_router.GlobalSequenceLedger 와 **같아야 한다**.
                # 예전엔 여기만 `> 32768` 이라 delta == 32768 에서 두 곳이 갈렸다 —
                # 원장은 resync 로 보고 파일에는 "32767개 손실" 이 영구 기록됐다 (감사 #5).
                # seq 가 뒤로 갔다. 재시작/중복이지 손실이 아니다 — 기준만 다시 잡는다.
                self.w.gap(X.GAP_LINK_RESET, self._last_seq, seq, 0)
                self.gaps += 1
            elif delta > 1:
                lost = delta - 1
                self.w.gap(X.GAP_SEQ, self._last_seq, seq, lost)
                self.gaps += 1
                self.lost_total += lost
        self._last_seq = seq
        self._started = True

        self.w.data(frame.module_id, seq, pc_time_us, frame.payload)
        self.frames += 1

    def on_gap(self, reason, from_seq=0, to_seq=0, lost_count=0) -> None:
        """와이어가 아니라 **PC 쪽** 사정으로 잃은 것 (큐 오버플로 등)."""
        self.w.gap(reason, from_seq, to_seq, lost_count)
        self.gaps += 1
        self.lost_total += lost_count

    # -----------------------------------------------------------------
    def flush(self) -> None:
        self.w.flush()

    def close(self) -> None:
        self.w.close()

    def summary(self) -> str:
        return ("%s  frames=%d  gaps=%d  lost=%d  %d B"
                % (self.path, self.frames, self.gaps, self.lost_total,
                   self.w.bytes_written))

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False
