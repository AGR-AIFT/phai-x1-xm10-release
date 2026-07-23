# XM10 USB 데이터 다루기 — 디코더 & 샘플

XM10 보드가 USB로 내보내는 데이터를 다루는 두 가지 경로를 안내합니다.

| 하고 싶은 것 | 도구 |
|---|---|
| USB 메모리(MSC)에 저장된 **로그 파일(`.bin`)을 CSV로 변환** | **XM10 Log Decoder** 실행 파일 (아래 1번) |
| USB-CDC로 나오는 데이터를 **실시간 수신**하는 코드를 직접 짜고 싶다 | `CDC/` 파이썬 샘플 (아래 2번) |

> PhAI Studio로 실시간 그래프를 보거나 로그를 관리하는 것으로 충분하다면 이 폴더는 필요 없습니다.

---

## 1. USB-MSC 로그 디코딩 — XM10 Log Decoder (실행 파일)

USB 메모리에 저장된 바이너리 로그(`data_XXX_part_YYY.bin`)를 CSV로 변환하는 **표준 도구**입니다.
설치형 실행 파일이라 파이썬 설치 없이 바로 쓸 수 있고, 로그 포맷 버전(신규/구버전)을 자동으로 인식합니다.

### 다운로드

[**Releases** 페이지](https://github.com/AGR-EXO/Extension_Module/releases)에서 최신 `XM10LogDecoder_Setup_*.exe`를 받아 설치하세요.
`.exe` 다운로드가 브라우저·백신에 막히면 같은 이름의 `.zip`을 받아 풀고 설치하면 됩니다.
`SHA256SUMS.txt`로 무결성을 확인할 수 있습니다.

> 설치 시 SmartScreen "알 수 없는 게시자" 경고가 나오면 **추가 정보 → 실행**을 한 번 누르면 됩니다.

### 사용

1. XM10을 USB 대용량저장장치로 연결해 **세션 폴더**(`metadata.txt` + `*.bin`)를 PC로 복사합니다.
2. XM10 Log Decoder를 실행하고 세션(또는 LOGS) 폴더를 선택합니다.
3. 같은 폴더에 결과가 생성됩니다.
   - `decoded_output.csv` — 디코딩된 데이터 (열 제목 = 여러분이 정의한 필드 이름)
   - `events.csv` — 이벤트 마커 (사용 시)
   - `data_quality_report.txt` — 누락/중복/동기화 품질 리포트

### ⭐ 메타데이터를 정확히 입력해야 합니다

디코더는 로그 안에 필드 정보를 저장하지 않습니다. 세션 폴더의 `metadata.txt`가
"이 로그가 어떤 구조체인지"를 알려주는데, **이 정보가 실제 구조체와 어긋나면
CSV 전체가 깨집니다**. 그래서 디코더는 크기가 맞지 않으면 디코딩을 중단하고
무엇이 틀렸는지 알려줍니다.

자기 구조체를 자유롭게 정의해 로깅하고 올바르게 디코딩하는 방법은
**[메타데이터 작성 가이드](METADATA_GUIDE.md)** 를 꼭 읽어보세요
(가장 중요한 팁: C 구조체를 `__attribute__((packed))`로 선언하면 정렬 패딩
실수를 원천 차단합니다).

> 예전 로그(파일 헤더가 없던 v1 포맷)도 이 실행 파일이 자동으로 인식해 처리합니다.
> 별도의 구버전 디코더 스크립트는 더 이상 제공하지 않습니다.

---

## 2. CDC — USB-CDC 실시간 수신 샘플

직접 파싱 코드가 필요한 경우(자체 GUI 제작, 커스텀 후처리, 다른 언어로 포팅 등)를 위한
**참고용 파이썬 샘플**입니다. XM10의 PhAI 와이어 프로토콜(SOF/LEN/SEQ_ID/MODULE_ID/CRC16 + COBS)을
직접 파싱하는 실제 동작 코드입니다.

### ⚠️ 다루는 범위 (중요)

이 샘플은 아래 채널만 의미 있게 디코딩합니다.

- 예전 방식의 소형 센서 모듈 채널 (Module ID `0x01~0x07`, `0x10`, `0xFF`)
- 사용자 정의 커스텀 채널 (Module ID `0xF0~0xFE`) — [Ex.09 CDC Stream](../examples/09_CDC_Stream/) 방식

**System이 항상 자동으로 내보내는 Total Data Packet(Module ID `0x20`)은 이 샘플이 디코딩하지 않습니다.**
이 패킷은 uint32/uint16/uint8/float가 섞인 구조체라 "payload를 float 배열로 해석"하는 이 샘플 방식으로는
값이 깨집니다. 0x20 패킷은 PhAI Studio를 쓰거나, `XM_TotalDataPacket_t`
(FW: `XM_FW/System/Comm/USB/xm_total_data_packet.h`) 구조체 레이아웃에 맞춰 직접 `struct.unpack`을 확장하세요.

### 2.1 `CDC/cdc_phai_receiver.py` — 실시간 모니터링 GUI

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

### 2.2 `CDC/cdc_csv_reviewer.py` — 후처리 분석 뷰어

`cdc_phai_receiver.py`가 저장한 CSV를 로드하여 전체 세션을 분석합니다.

```bash
python CDC/cdc_csv_reviewer.py                                  # 파일 선택 다이얼로그
python CDC/cdc_csv_reviewer.py data/cdc_phai_20260224_120000.csv
```

주요 기능: 센서 도메인별 그래프 그룹, **Sequence Gap(ΔSeq) 분석**(패킷 누락 시점),
**Tx Drop 누적 그래프**, 이상치 자동 필터, X축 연동, 드래그 앤 드롭.

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
