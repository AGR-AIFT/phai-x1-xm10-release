# XM10 Python 데이터 도구 (Python Data Tools)

USB-CDC 실시간 수신 및 USB-MSC 바이너리 디코딩을 위한 Python 스크립트 모음입니다.

---

## 파일 목록

| 파일명 | 용도 | 프로토콜 |
|--------|------|----------|
| `cdc_phai_receiver.py` | **USB-CDC 실시간 수신 & 그래프** (PhAI V2) | PhAI V2 (SOF 0xAA, CRC8) |
| `cdc_serial_data_logging.py` | USB-CDC 바이너리 수신 & CSV 저장 (레거시) | Raw struct (AGRB MON) |
| `data_decoder_xm10.py` | USB-MSC 바이너리 로그 → CSV 변환 | metadata.txt 자동 파싱 |

---

## 1. `cdc_phai_receiver.py` — PhAI V2 CDC Receiver (신규)

XM10 보드의 **PhAI V2 프로토콜**을 실시간으로 수신하여 그래프로 시각화하고 CSV로 저장합니다.

### PhAI V2 패킷 구조

```
[SOF:0xAA] [LEN:1] [SEQ_ID:2 LE] [MODULE_ID:1] [PAYLOAD: LEN×4] [CRC8:1]
```

- **LEN**: payload의 4-byte 단위 개수 (0~255)
- **CRC8**: polynomial 0x07, init 0x00
- **Auto-Stream**: USB 연결 시 자동 수신 (핸드쉐이크 불필요)

### 요구 패키지

```bash
pip install pyserial pyqt5 pyqtgraph numpy
```

### 사용법

**GUI 모드 (기본):**
```bash
python cdc_phai_receiver.py
```

**CLI 모드 (터미널 전용):**
```bash
python cdc_phai_receiver.py --cli --port COM6
python cdc_phai_receiver.py --cli --port COM6 --output my_data
```

### 주요 기능

- Module ID 자동 인식 (Combined 12ch, IMU, GRF 등)
- SEQ_ID 기반 패킷 드롭 감지
- 실시간 6-panel 그래프 (NumPy rolling buffer, 2000 샘플)
- CSV 자동 저장 + 세션 리뷰 뷰어
- CRC8 검증 + 통계 표시

---

## 2. `cdc_serial_data_logging.py` — Legacy CDC Logger

리팩토링 이전의 **Raw 바이너리 구조체** 프로토콜을 사용합니다. `AGRB MON START` 핸드쉐이크가 필요합니다.

### 요구 패키지

```bash
pip install pyserial
```

### 사용법

```bash
python cdc_serial_data_logging.py
```

> **참고:** 새로운 펌웨어(PhAI V2)에서는 `cdc_phai_receiver.py`를 사용하세요. 이 파일은 구버전 펌웨어와의 호환을 위해 유지됩니다.

---

## 3. `data_decoder_xm10.py` — MSC Binary Decoder

USB 메모리에 저장된 바이너리 로그 파일(`.bin`)을 CSV로 변환합니다.

### 요구 패키지

Python 표준 라이브러리만 사용합니다 (추가 설치 없음).

### 사용법

```bash
python data_decoder_xm10.py <session_folder>
python data_decoder_xm10.py /LOGS/BasicTest
python data_decoder_xm10.py .
```

### 주요 기능

- `metadata.txt` 자동 파싱 → `struct.unpack` 포맷 & CSV 헤더 자동 생성
- 자동 타임스탬프(`tick_ms`) 처리
- 다중 파트 파일(`data_*_part_*.bin`) 순차 처리
- `summary.txt` 통계 출력
- 레거시 모드: `decode_legacy()` 함수로 이전 포맷 호환
