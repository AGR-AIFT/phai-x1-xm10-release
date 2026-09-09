# XM10 USB 데이터 다루기 — CDC 실시간 수신 샘플

XM10 보드가 USB-CDC 로 내보내는 데이터를 PC 에서 직접 받아 처리하는 **참고용 파이썬 샘플**입니다.
직접 파싱 코드가 필요한 경우(자체 GUI 제작, 커스텀 후처리, 다른 언어로 포팅 등)를 위한 것으로,
XM10 의 PhAI 와이어 프로토콜(SOF/LEN/SEQ_ID/MODULE_ID/CRC16 + COBS)을 직접 파싱하는 실제 동작 코드입니다.

> **USB 메모리(MSC) 파일 로깅 기능은 XM10 v2.5.0 에서 제거되었습니다.** 데이터 수집은 이제
> USB-CDC 실시간 스트리밍으로 합니다. 실시간 그래프·CSV 저장만 필요하다면 **PhAI Studio** 로
> 충분하니 이 폴더는 필요 없습니다. 온보드 저장(SD카드)은 향후 HW 리비전에서 지원 예정입니다.

---

## ⚠️ 다루는 범위 (중요)

이 샘플은 아래 채널만 의미 있게 디코딩합니다.

- 예전 방식의 소형 센서 모듈 채널 (Module ID `0x01~0x07`, `0x10`, `0xFF`)
- 사용자 정의 커스텀 채널 (Module ID `0xF0~0xFE`) — [Ex.09 CDC Stream](../examples/09_CDC_Stream/) 방식

**System이 항상 자동으로 내보내는 Total Data Packet(Module ID `0x20`)은 이 샘플이 디코딩하지 않습니다.**
이 패킷은 uint32/uint16/uint8/float가 섞인 구조체라 "payload를 float 배열로 해석"하는 이 샘플 방식으로는
값이 깨집니다. 0x20 패킷은 PhAI Studio를 쓰거나, `XM_TotalDataPacket_t`
(FW: `XM_FW/System/Comm/USB/xm_total_data_packet.h`) 구조체 레이아웃에 맞춰 직접 `struct.unpack`을 확장하세요.

디코딩하지 않는다는 것이 무시한다는 뜻은 아닙니다. `frame_router.py` 가 시스템 채널
(`0x20`/`0xEF`/`0xED`/`0xEE`)을 사용자 채널과 **분리해서 따로 셉니다** — 그래프와 CSV 에는
들어가지 않지만 몇 프레임 몇 바이트가 왔는지는 화면 하단 `Sys[...]` 에 그대로 표시됩니다.
예전 버전은 이 패킷을 사용자 채널에 섞어 넣어 CSV 컬럼 수가 헤더와 어긋나곤 했습니다.

## `CDC/cdc_phai_receiver.py` — 실시간 모니터링 GUI

PhAI V2.2 프로토콜을 실시간 수신하여 그래프 표시 + CSV 저장합니다.

```bash
python CDC/cdc_phai_receiver.py                 # GUI 모드
python CDC/cdc_phai_receiver.py --cli --port COM6   # CLI 모드 (CSV만 저장)
```

주요 기능: OpenGL 6-plot 실시간 그래프, 센서 도메인별 자동 그룹핑, 채널 visibility 토글,
처리량(pkt/s, KB/s) 표시, Freeze/Screenshot/테마 토글, Auto-reconnect, CSV 자동 저장.

의존성:
```bash
pip install pyserial pyqt5 pyqtgraph numpy
```

## `CDC/frame_router.py` — 프레임 파싱·라우팅 (GUI/CLI 공용)

와이어 프로토콜을 다루는 부분만 따로 뺀 모듈입니다. Qt 도 pyserial 도 쓰지 않아서
단독으로 import 해 쓸 수 있습니다 — 자체 도구를 만든다면 이 파일만 가져가면 됩니다.
필요한 외부 패키지는 **numpy 하나**입니다(`PhAIFrame.as_float32()` 가 씁니다).

- `parse_phai_frame()` — COBS 를 푼 프레임 하나를 검증해 `PhAIFrame` 으로
- `FrameRouter` — 프레임이 사용자 채널인지 시스템 채널인지 판정
- `GlobalSequenceLedger` — 패킷 손실 집계

화면 하단 통계줄에 나오는 값들:

| 표시 | 뜻 |
|---|---|
| `Good` | CRC 까지 통과한 **전체** 프레임 수 (시스템 채널 포함) |
| `SEQ↓` | 와이어에서 실제로 잃은 프레임 수 |
| `QOvf` | PC 가 못 따라가 큐에서 버린 수 — 케이블 문제가 아니라 **로컬 지연**입니다 |
| `Sys[...]` | 시스템 채널별 수신 프레임 수 |
| `Other` | 첫 사용자 module 과 다른 module_id 프레임 수 (있을 때만 표시) |
| `Resync` | seq 가 뒤로 간 횟수 — 손실이 아니라 기준을 다시 잡은 것 (있을 때만) |

`seq_id` 는 module 과 무관하게 펌웨어 카운터 하나가 발급하므로, 손실 계산은 **module 을
나누기 전에** 전체 프레임을 대상으로 해야 합니다. 특정 module 의 프레임끼리만 비교하면
그 사이에 낀 다른 module 프레임이 전부 손실로 잡힙니다. `FrameRouter.route()` 가 이 순서를 강제합니다.

### 검증

보드 없이 돌아갑니다. `data/` 의 실제 캡처를 와이어로 되돌려 재생하는 회귀 시험까지 포함합니다.

```bash
python CDC/test_frame_router.py
```

## `CDC/cdc_csv_reviewer.py` — 후처리 분석 뷰어

`cdc_phai_receiver.py`가 저장한 CSV를 로드하여 전체 세션을 분석합니다.

```bash
python CDC/cdc_csv_reviewer.py                                  # 파일 선택 다이얼로그
python CDC/cdc_csv_reviewer.py data/cdc_phai_20260224_120000.csv
```

주요 기능: 센서 도메인별 그래프 그룹, **Sequence Gap(ΔSeq) 분석**(패킷 누락 시점),
**Tx Drop 누적 그래프**, 이상치 자동 필터, X축 연동, 드래그 앤 드롭.

---

## CSV 포맷 참고 (CDC, PhAI V2.2)

```
time_s,pc_time_s,seq_id,module_id,tx_drops,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,MotorAngle_L,MotorAngle_R,MotorTorque_L,MotorTorque_R
0.001234,0.001501,0,16,0,0.012,-9.781,0.234,0.001,-0.002,0.003,15.2,14.8,1.23,1.15
```

- `time_s` — 장치 시각. `seq_id` 로 재구성해 매끄럽습니다(손실 구간도 시간이 흐릅니다)
- `pc_time_s` — PC 가 받은 시각. 다른 장비와 맞출 때 씁니다
