#!/usr/bin/env python3
"""수신한 PhAI 프레임을 `.xmlog` 로 눕히는 다리.

`frame_router.PhAIFrame` 하나를 받아 레코드 하나를 적는다. 그게 전부다.
(예외가 하나 있다: `0xEE` 스키마가 완성되면 SCHEMA_ACTIVATION 을 하나 더 적는다 — 아래.)

**왜 GUI/CLI 파일 안에 두지 않았나** — 시리얼 포트를 열어야만 실행되는 코드 안에
로직을 넣으면 보드 없이는 한 줄도 시험할 수 없다. 실제로 그 방식으로 넣었던
`--total-data` 경로가 `NameError` 를 안은 채 "검증 통과" 로 보고된 적이 있다
(2026-09-10). 정책은 여기 두고, 수신 루프는 이 클래스를 부르기만 한다.
GUI 워커와 CLI 가 **같은 파일을 만드는 이유**도 이것이다 — 둘 다 이 클래스 하나를 부른다.

저장 규약 (PLAN 4.1 "data-before-schema", 4.6)
----------------------------------------------
* CRC 를 통과한 프레임은 **스키마를 몰라도 즉시 적는다** (`activation_id = 0`).
  해석은 나중에 `xmlog_export.py` 가 한다.
* 시스템 채널(0x20 등)도 사용자 채널과 **똑같이 적는다.** 화면에 안 그린다고
  버리면 나중에 되짚을 수 없다.
* 손실은 GAP 레코드로 파일 안에 남긴다 — 파일만 보고도 무슨 일이 있었는지 알아야 한다.

스키마의 수명 (PLAN 4.6 activation identity · 4.11 · §9 D-H)
------------------------------------------------------------
* `0xEE` 프레임은 다른 프레임처럼 DATA 로도 적힌다(원본 보존). 그러면서 이 클래스가 **재조립을 직접
  돌려** 스키마가 완성되는 순간 SCHEMA_ACTIVATION 을 적고 `activation_id` 를 받는다. 같은 스키마가
  다시 오면 새 레코드 없이 같은 id 를 돌려받는다(`XmLogWriter.schema_activation`).
* 그 뒤 그 모듈의 DATA 에는 **그 id 를 달아** 적는다. 스키마가 아직 안 왔거나 무효가 된 동안의
  DATA 는 0(미상)이다. 읽는 쪽이 DATA 마다 그 id 의 스키마로 푼다 — 파일 끝의 최종 스키마 하나로
  전 구간을 푸는 것이 아니다.
* 번호를 못 다는 스키마도 있다: 파일에 남길 바이트를 읽는 쪽이 되읽지 못하거나, 번호(u16)가 바닥난
  경우다. 그 모듈의 DATA 는 0 으로 남고(`schema_errors` 에 센다), 읽는 쪽은 `0xEE` 프레임에서
  스키마를 다시 만든다. 번호를 못 달았다고 저장을 멈추지는 않는다.
* 화면용 레지스트리(`SchemaRegistry`)와 **따로** 재조립한다. 파일에 남기는 것은 "그때 와이어에서
  실제로 있었던 일" 이고, 화면 쪽은 재연결 때 비워지기도 하는 표시용 상태라 둘이 어긋날 수 있다.
"""
from __future__ import annotations

import os

import schema_0xee as EE
import schema_registry as R
import xmlog as X

# 펌웨어가 어떤 빌드인지 알아낼 경로가 아직 없다(`0xED` 에 빌드 ID 가 없고 SDO 로 묻지도 않는다).
# 값을 0 으로 비워 두면 "안 적었다" 와 "모른다" 가 구별되지 않는다 — 모른다고 **적는다**.
FW_BUILD_ID_UNKNOWN = "unknown"


def unique_path(path: str) -> str:
    """이미 있는 파일을 덮어쓰지 않는 경로. 없으면 그대로, 있으면 `이름_2.확장자`, `_3` ....

    `.xmlog` 이름은 초 단위 시각이다. 연결을 끊고 1초 안에 다시 붙이면 같은 이름이 나오는데,
    그대로 열면 방금 받은 파일이 비워진다 — "재연결마다 새 파일로 끊는다" 는 규칙이 오히려 앞 파일을
    지우는 일이 되는 셈이다.
    """
    if not os.path.exists(path):
        return path
    base, ext = os.path.splitext(path)
    n = 2
    while os.path.exists("%s_%d%s" % (base, n, ext)):
        n += 1
    return "%s_%d%s" % (base, n, ext)


class XmLogCapture:
    """PhAIFrame -> .xmlog. 상태는 카운터 · 직전 seq · 지금 유효한 스키마(activation)뿐이다.

        cap = XmLogCapture(path, total_data_map_version="2.8")
        cap.on_frame(frame, pc_time_us)     # 매 프레임
        cap.on_gap(X.GAP_QUEUE_OVERFLOW, ...)   # 큐를 버렸을 때
        cap.on_reconnect()                  # 연결이 끊겼다 같은 파일에서 다시 붙었을 때
        cap.close()

    `fw_build_id` 를 안 주면 `"unknown"` 이 적힌다(`FW_BUILD_ID_UNKNOWN`).
    `link_epoch` 는 **호스트가 세는 재연결 순번**이다(파일 소유, 1부터). 펌웨어가 부팅할 때마다
    올리는 카운터(`0xED` 의 epoch)와는 다른 값이다.

    `total_data_map_version` 은 SESSION 에 적는 참고용 문자열이다(내보내기는 이걸로 값을 풀지 않고
    요약에 찍기만 한다). 부르는 곳마다 다르다: CLI 는 `--total-data` 일 때만 맵 버전을 적고, GUI 와
    soak 은 비워 둔다. 파일 이름은 GUI · CLI 가 `unique_path` 로 겹치지 않게 하고, soak 은 초 단위
    이름을 그대로 쓴다(같은 초에 두 번 시작하면 뒤 것이 앞 파일을 비운다).
    """

    __slots__ = ("w", "path", "frames", "gaps", "lost_total", "reconnects", "schema_errors",
                 "_last_seq", "_started", "_reasm", "_live", "_link_epoch", "_map_version")

    def __init__(self, path, device_usb_serial="", fw_build_id=None,
                 total_data_map_version="", boot_epoch=0, link_epoch=1,
                 flush_every=256):
        self.w = X.XmLogWriter(path, flush_every=flush_every)
        self.path = self.w.path
        self._link_epoch = link_epoch
        self._map_version = total_data_map_version
        self.w.session(device_usb_serial=device_usb_serial,
                       fw_build_id=fw_build_id or FW_BUILD_ID_UNKNOWN,
                       total_data_map_version=total_data_map_version,
                       boot_epoch=boot_epoch, link_epoch=link_epoch)
        self.frames = 0
        self.gaps = 0
        self.lost_total = 0
        self.reconnects = 0
        self.schema_errors = 0
        self._last_seq = -1
        self._started = False
        self._reasm = EE.Reassembler()
        # module_id -> 지금 유효한 activation_id. 여기 없는 모듈의 DATA 는 0(스키마 미상)이다.
        self._live = {}

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
                # 스키마(_live)는 일부러 그대로 둔다: seq 가 뒤로 간 것만으로는 다른 장치가 붙었다고
                # 할 수 없다(프레임을 3만 개 넘게 놓친 뒤에도 같은 모양이다). 지우면 아직 맞는 스키마로
                # 풀리던 행이 다음 스키마가 올 때까지 미상(0)이 된다. 장치가 바뀌었을 수 있다는 신호는
                # 포트가 끊긴 일이고, 그건 호출하는 쪽이 on_reconnect() 로 알린다(GUI 는 연결마다 새 파일).
                self.w.gap(X.GAP_LINK_RESET, self._last_seq, seq, 0)
                self.gaps += 1
            elif delta > 1:
                lost = delta - 1
                self.w.gap(X.GAP_SEQ, self._last_seq, seq, lost)
                self.gaps += 1
                self.lost_total += lost
        self._last_seq = seq
        self._started = True

        mid = frame.module_id
        # 이 모듈의 스키마가 지금 유효하면 그 번호를 달아 적는다. 아니면 0 — 스키마보다 먼저 온
        # 데이터도 버리지 않고 적는다. 어떤 스키마로 풀지는 읽는 쪽이 정한다.
        self.w.data(mid, seq, pc_time_us, frame.payload,
                    activation_id=self._live.get(mid, 0))
        self.frames += 1

        if mid == R.MODULE_ID_SCHEMA_DESC:
            self._on_schema_frame(frame.payload, pc_time_us)

    def _on_schema_frame(self, payload: bytes, pc_time_us: int) -> None:
        """`0xEE` 프레임 하나 — 스키마가 완성되는 순간 activation 을 받는다(재사용 포함).

        스키마가 깨졌다고 수신·저장을 멈추지 않는다. 프레임 자체는 바로 위에서 DATA 로 이미
        적혔고, 여기서는 세기만 한다(`SchemaRegistry.feed_0xEE` 와 같은 약속).
        """
        try:
            schema = self._reasm.feed(payload, pc_time_us / 1e6)
        except EE.SchemaError:
            self.schema_errors += 1
            return
        except Exception:  # noqa: BLE001 — 예상 못한 예외가 수신 스레드를 죽이면 안 된다
            self.schema_errors += 1
            return
        if schema is None:
            return                        # 프래그먼트가 더 필요하다

        # 번호를 달 수 없는 경우가 둘 있다. 어느 쪽이든 이 모듈의 DATA 는 0(미상)으로 남고, 스키마를
        # 담은 0xEE 프레임은 위에서 이미 DATA 로 적혔으니 읽는 쪽이 그 프레임에서 스키마를 다시
        # 만든다 — 저장을 멈추지도, 틀린 스키마를 달지도 않는다.
        mid = schema.module_id
        canonical = self._reasm.last_canonical
        # (1) 파일에 남는 바이트를 읽는 쪽이 되읽을 수 있어야 한다. 화면에서 풀린 스키마를 파일에서
        #     못 풀면 번호만 달린 채 float32 로 뭉개진 CSV 가 나온다.
        try:
            EE.schema_from_canonical(canonical)
        except Exception:  # noqa: BLE001 — 되읽지 못하는 스키마 하나가 수신 스레드를 죽이면 안 된다
            self.schema_errors += 1
            self._live.pop(mid, None)
            return
        # (2) 번호가 남아 있어야 한다. 바이트가 조금씩 다른 스키마(예: struct_name 뒤 찌꺼기가 방송마다
        #     바뀐다)가 끝없이 오면 u16 번호가 바닥나고, 쓰는 쪽이 ValueError 로 알린다. 디스크 오류
        #     (OSError)는 잡지 않는다 — 그건 저장이 깨졌다는 뜻이라 호출자가 알아야 한다.
        try:
            act = self.w.schema_activation(mid, schema.proto_ver, schema.struct_size,
                                           schema.schema_crc32, canonical)
        except ValueError:
            self.schema_errors += 1
            self._live.pop(mid, None)
            return
        self._live[mid] = act

    def on_gap(self, reason, from_seq=0, to_seq=0, lost_count=0) -> None:
        """와이어가 아니라 **PC 쪽** 사정으로 잃은 것 (큐 오버플로 등)."""
        self.w.gap(reason, from_seq, to_seq, lost_count)
        self.gaps += 1
        self.lost_total += lost_count

    def on_reconnect(self, device_usb_serial="") -> None:
        """연결이 끊겼다가 같은 파일에서 다시 붙었다 — 붙은 장치가 아까 그 장치인지 알 수 없다.

        PLAN §9 D-H 의 최소 계약이다. 장치를 식별할 방법(`device_usb_serial` 취득)은 아직 없으므로
        **다른 장치일 수 있다고 본다**:

        * 새 SESSION 레코드로 구간을 끊는다. `link_epoch` 가 1 오르고, 펌웨어 빌드 · 부팅 epoch 는
          모른다(`"unknown"` · 0).
        * 지금 유효하던 스키마를 **즉시 무효로 한다.** 새 스키마가 도착하기 전의 DATA 는
          `activation_id=0` 원시 바이트로 적힌다 — 다른 장치가 같은 module_id · 같은 크기의 데이터를
          보내도 이전 장치의 스키마로 풀리지 않게 하려는 것이다.
        * 재조립하던 조각도 버린다. seq 기준도 새로 잡는다(끊긴 동안 몇 개를 놓쳤는지는 알 수 없다).

        새 장치가 **바이트까지 같은** 스키마를 보내면 새 레코드 없이 예전 activation 을 재사용한다.
        GUI 는 연결마다 새 `.xmlog` 를 열어서(=이 계약의 "새 파일" 쪽) 이 메서드를 부르지 않는다.
        """
        self._link_epoch += 1
        self._live.clear()
        self._reasm = EE.Reassembler()
        self._started = False
        self.reconnects += 1
        self.w.session(device_usb_serial=device_usb_serial,
                       fw_build_id=FW_BUILD_ID_UNKNOWN,
                       total_data_map_version=self._map_version,
                       boot_epoch=0, link_epoch=self._link_epoch)

    # -----------------------------------------------------------------
    def flush(self) -> None:
        self.w.flush()

    def close(self) -> None:
        self.w.close()

    def summary(self) -> str:
        s = ("%s  frames=%d  gaps=%d  lost=%d  %d B"
             % (self.path, self.frames, self.gaps, self.lost_total,
                self.w.bytes_written))
        if self.w.activation_count:
            s += "  schemas=%d" % self.w.activation_count
        if self.reconnects:
            s += "  reconnects=%d" % self.reconnects
        return s

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False
