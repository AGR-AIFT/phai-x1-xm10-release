# PythonDecoder — XM10 USB Tools

XM10 보드의 USB-CDC / USB-MSC 데이터를 처리하기 위한 Python 도구 모음입니다.

---

## 폴더 구조

```
PythonDecoder/
├── CDC/                        ← USB-CDC 도구 (실시간 + 후처리)
│   ├── cdc_phai_receiver.py    ← 실시간 모니터링 + 로깅 GUI
│   ├── cdc_selective_logger.py ← 포트/채널 선택형 CSV 저장 GUI
│   └── cdc_csv_reviewer.py     ← 후처리 CSV 분석 뷰어
├── MSC/                        ← USB-MSC 도구
│   └── data_decoder_xm10.py   ← 바이너리 로그 → CSV 디코더
├── Legacy/                     ← 이전 버전 (호환용)
│   ├── cdc_serial_data_logging.py
│   └── xm10_data_logger.zip
└── README.md
```

---

## 1. CDC — 실시간 수신 + 후처리

### 1.1 `cdc_phai_receiver.py` — 실시간 모니터링 GUI

PhAI V2.1 프로토콜(`SOF 0xAA + CRC8 + STATUS`)을 실시간 수신하여 그래프 표시 + CSV 저장합니다.

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

### 1.3 `cdc_selective_logger.py` — 선택형 CSV 저장 GUI

USB 포트를 자동 연결하지 않고 GUI 드롭다운에서 직접 선택합니다. 연결
후 실제로 수신된 CDC 모듈과 float 채널 수를 표시하며, 체크한 채널만 CSV로
저장합니다. 채널 이름은 기본적으로 `ch0`, `ch1`, ... 로 표시합니다.
`Final_FSR_Fuzzy_Logic`, `Final_EMG`, `Final_Encoder_ex01` 프리셋을 선택하면
해당 FW의 변수명을 즉시 적용하며, GUI에서 직접 수정할 수도 있습니다.

```bash
python CDC/cdc_selective_logger.py
```

---

## 2. MSC — 바이너리 로그 디코더

### `data_decoder_xm10.py`

USB MSC로 저장된 바이너리 로그 파일(`data_XXX_part_YYY.bin`)을 CSV로 변환합니다.

```bash
python MSC/data_decoder_xm10.py path/to/session_folder
```

**주요 기능:**
- `metadata.txt` 자동 파싱 (auto_timestamp, payload_size 등)
- 멀티파트 bin 파일 자동 탐색 + 순서 병합
- Auto-timestamp (4바이트 tick_ms) 자동 처리
- `summary.txt` 읽기 + 세션 통계 출력
- 레거시 파일 호환 (`decode_legacy()`)

---

## 3. Legacy — 이전 버전

| 파일 | 설명 |
|------|------|
| `cdc_serial_data_logging.py` | PhAI V1 (SOF 0xAA55 + CRC16) 전용 CLI 로거 |
| `xm10_data_logger.zip` | 이전 배포 패키지 |

---

## CSV 포맷 참고

### CDC CSV (PhAI V2.1)
```
time_s,seq_id,module_id,tx_drops,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,MotorAngle_L,MotorAngle_R,MotorTorque_L,MotorTorque_R
0.001234,0,16,0,0.012,-9.781,0.234,0.001,-0.002,0.003,15.2,14.8,1.23,1.15
```

### MSC Binary (auto-timestamp 활성 시)
```
[header: 4B payload_size] [4B tick_ms] [user_struct...]
```
