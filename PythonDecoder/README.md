# PythonDecoder — XM10 USB 데이터 직접 파싱하기

XM10 보드가 USB로 내보내는 데이터를 **PhAI Studio 없이 직접 파싱**하고 싶을 때 참고할 수 있는
Python 샘플 모음입니다. "나만의 디코더/GUI를 만들고 싶다" — 이 폴더가 그 출발점입니다.
와이어 프로토콜(COBS/CRC16 프레이밍)과 USB-MSC 로그 파일 포맷을 직접 파싱하는 실제 동작 코드이므로,
여기서 구조를 파악한 뒤 필요한 부분만 잘라 여러분의 프로젝트에 붙여 쓰면 됩니다.

> PhAI Studio로 실시간 그래프를 보거나 로그를 관리하는 것으로 충분하다면 이 폴더는 필요 없습니다.
> 여기 있는 도구들은 **직접 파싱 코드가 필요한 경우**(자체 GUI 제작, 커스텀 후처리 파이프라인,
> 다른 언어로 포팅 등)를 위한 참고용 샘플입니다.

---

## 폴더 구조

```
PythonDecoder/
├── CDC/                              ← USB-CDC 실시간 수신 샘플
│   ├── cdc_phai_receiver.py          ← 실시간 모니터링 + 로깅 GUI
│   └── cdc_csv_reviewer.py           ← 후처리 CSV 분석 뷰어
├── MSC/                              ← USB-MSC 로그 디코더 (표준)
│   └── data_decoder_xm10.py          ← 바이너리 로그 → CSV 디코더 (신규/구버전 포맷 모두 자동 인식)
├── Legacy/                           ← 구버전 참고용 (신규 프로젝트에는 사용하지 마세요)
│   ├── data_decoder_xm10_v1.py       ← 헤더 없는 V1 로그 포맷 전용
│   ├── cdc_serial_data_logging.py
│   └── xm10_data_logger.zip
└── README.md
```

---

## 1. CDC — USB-CDC 실시간 수신 샘플

### ⚠️ 다루는 범위 (중요)

`cdc_phai_receiver.py`는 XM10의 PhAI 와이어 프로토콜(SOF/LEN/SEQ_ID/MODULE_ID/CRC16 + COBS 프레이밍)을
직접 파싱하는 **레퍼런스 구현**입니다. 다만 아래 두 종류의 채널만 의미 있게 디코딩합니다.

- 예전 방식의 소형 센서 모듈 채널 (Module ID `0x01~0x07`, `0x10`, `0xFF`) — 현재 예제들은 이 채널을 쓰지 않습니다.
- 사용자 정의 커스텀 채널 (Module ID `0xF0~0xFE`) — [Ex.09 CDC Stream](../examples/09_CDC_Stream/) 등에서 사용하는 방식입니다.

**System이 항상 자동으로 내보내는 Total Data Packet(Module ID `0x20`, 365 B, 1 kHz)은 이 샘플이 디코딩하지
않습니다.** 이 패킷은 uint32/uint16/uint8/float가 섞인 구조체라서, 이 샘플의 "payload를 float 배열로
해석" 방식으로는 값이 깨집니다. 0x20 패킷을 다루려면 PhAI Studio를 쓰거나, 여러분이 직접
`XM_TotalDataPacket_t`(FW: `XM_FW/System/Comm/USB/xm_total_data_packet.h`) 구조체 레이아웃에 맞춰
`struct.unpack`을 확장해야 합니다.

### 1.1 `cdc_phai_receiver.py` — 실시간 모니터링 GUI

PhAI V2.2 프로토콜(`SOF 0xAA + CRC16-CCITT + STATUS`, COBS 프레이밍)을 실시간 수신하여 그래프 표시 + CSV 저장합니다.

```bash
# GUI 모드
python CDC/cdc_phai_receiver.py

# CLI 모드 (GUI 없이 CSV만 저장)
python CDC/cdc_phai_receiver.py --cli --port COM6
```

**주요 기능:**
- OpenGL 가속 6-plot 실시간 그래프
- 센서 도메인별 자동 그룹핑 (Accel, Gyro, Motor Angle, Motor Torque)
- 채널별 체크박스 visibility 토글
- 최신값 패널 + 처리량(pkt/s, KB/s) 표시
- Freeze / Screenshot / Dark-Light 테마 토글
- 크로스헤어 마우스 호버
- 윈도우 사이즈 런타임 슬라이더
- Auto-reconnect (포트 분리 시 자동 재연결)
- CSV 자동 저장 + 배치 flush

**의존성:**
```bash
pip install pyserial pyqt5 pyqtgraph numpy
```

### 1.2 `cdc_csv_reviewer.py` — 후처리 분석 뷰어

`cdc_phai_receiver.py`가 저장한 CSV를 로드하여 전체 세션을 정밀 분석합니다.

```bash
# 파일 선택 다이얼로그
python CDC/cdc_csv_reviewer.py

# 직접 지정
python CDC/cdc_csv_reviewer.py data/cdc_phai_20260224_120000.csv
```

**주요 기능:**
- CSV 헤더 자동 감지 → 센서 도메인별 그래프 그룹
- **Sequence Gap (ΔSeq) 분석** — 패킷 누락 시점 시각화
- **Tx Drop 누적 그래프** — 펌웨어 측 전송 드롭 추적
- 이상치 자동 필터 (NaN / Inf / |value| > 1e6)
- 채널별 + 그룹별 체크박스 토글
- X축 연동 (줌/팬 동기화)
- 드래그 앤 드롭 CSV 열기
- 동적 그리드 레이아웃 (1~3열 자동)

---

## 2. MSC — USB 메모리 로그 디코더 (표준)

### `data_decoder_xm10.py`

USB 메모리(MSC)로 저장된 바이너리 로그(`data_XXX_part_YYY.bin`)를 CSV로 변환하는 표준 디코더입니다.
파일 헤더(magic/버전/플래그) + 4KB 블록 CRC32 + 풋터가 있는 신규 로그 포맷을 자동 인식하고,
헤더가 없는 구버전(V1) 로그 파일도 같은 스크립트로 자동 대체 처리합니다 — 어떤 버전의 로그인지
직접 확인할 필요 없이 이 스크립트 하나만 실행하면 됩니다.

```bash
cd MSC
python data_decoder_xm10.py path/to/session_folder
```

세션 폴더 경로 하나만 인자로 받으며, 별도의 옵션 플래그는 없습니다. `metadata.txt`를 읽어
struct format과 CSV 헤더를 자동으로 구성합니다.

**주요 기능:**
- 파일 헤더 자동 감지 (신규 포맷 + 구버전 raw 포맷 모두 지원)
- 4KB 블록 경계 CRC32 자동 스킵 처리 (순수 데이터만 추출)
- 이벤트 마커 자동 분리 (`events.csv`)
- 풋터 검증 (파일 정상 종료 여부 확인)
- `metadata.txt` 자동 파싱 (auto_timestamp, payload_size 등)
- 멀티파트 bin 파일 자동 탐색 + 순서 병합

**출력:**
- `decoded_output.csv` — 메인 데이터 CSV
- `events.csv` — 이벤트 마커 (마커 사용 시)

---

## 3. Legacy — 구버전 참고용

새 프로젝트에서는 사용하지 마세요. 예전 로그 파일이나 예전 프로토콜을 다시 열어봐야 할 때만 참고합니다.

| 파일 | 설명 |
|------|------|
| `data_decoder_xm10_v1.py` | 헤더/CRC/풋터가 없던 **V1 로그 포맷 전용** 디코더. 현재 FW가 만드는 로그(파일 헤더 포함)는 읽지 못합니다 — 위 `MSC/data_decoder_xm10.py`를 사용하세요. |
| `cdc_serial_data_logging.py` | PhAI V1 (SOF 0xAA55 + CRC16) 전용 CLI 로거 |
| `xm10_data_logger.zip` | 이전 배포 패키지 |

---

## CSV 포맷 참고

### CDC CSV (PhAI V2.2)
```
time_s,seq_id,module_id,tx_drops,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,MotorAngle_L,MotorAngle_R,MotorTorque_L,MotorTorque_R
0.001234,0,16,0,0.012,-9.781,0.234,0.001,-0.002,0.003,15.2,14.8,1.23,1.15
```

### MSC Binary (auto-timestamp 활성 시)
```
[header: 4B payload_size] [4B tick_ms] [user_struct...]
```
