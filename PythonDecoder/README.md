# 1\. `data_decoder_xm10.py` (Binary to CSV Converter)

이 스크립트는 USB MSC(Mass Storage Class) 기능을 통해 XM10 내부 저장소나 USB 메모리에 저장된 \*\*바이너리 로그 파일(`.bin`)\*\*을 엑셀에서 열어볼 수 있는 \*\*CSV 파일(`.csv`)\*\*로 변환해주는 도구입니다.

-----

````markdown
# Data Decoder (Binary to CSV)

XM10에서 저장된 바이너리 로그 파일(`data_*.bin`)을 분석하여, 사람이 읽을 수 있는 CSV(Comma-Separated Values) 파일로 변환하는 파이썬 스크립트입니다.
하나의 파일이 10MB가 넘어가면 `data_*_part_*.bin`으로 Rolling되어 새로운 파일에서 이어서 저장합니다.

## 📌 개요 (Overview)

* **입력:** XM10이 생성한 바이너리 파일 (예: `data_010_part010.bin`)
* **출력:** 데이터가 표 형태로 정리된 CSV 파일 (예: `result10.csv`)
* **기능:**
    * 리틀 엔디안(Little Endian) 바이너리 파싱
    * 구조체 멤버별 데이터 포맷팅 (Int, Float, Bool 변환)
    * CSV 헤더 및 데이터 라인 생성

## ⚙️ 설정 (Configuration)

스크립트 상단의 설정 변수를 수정하여 자신의 환경에 맞게 변경할 수 있습니다.

```python
# --- 설정 (Configuration) ---
INPUT_FILE  = 'data_010_part010.bin'    # 변환할 바이너리 파일명 (USB 메모리에서 복사해온 파일)
OUTPUT_FILE = 'result10.csv'    # 저장될 CSV 파일명
````

## 🛠️ 데이터 구조 (Data Structure)

이 스크립트는 펌웨어의 사용자 정의 (예:`MyLogData_t`) 구조체와 **100% 일치하는 포맷**을 사용합니다.
사용자 정의 구조체 목록에 따라 수정하여야 합니다.

  * **패킷 크기:** 126 Bytes (Total)
  * **구조:**
      * `LoopCnt` (uint32): 루프 카운트
      * `Mode` (uint8): 동작 모드
      * `Level` (uint8): 보조 레벨
      * `Angles` (float x 8): 관절 각도 및 속도
      * `Contacts` (bool x 2): 발바닥 접촉 여부
      * `Gait` (uint8 x 2): 보행 상태 및 주기
      * `Etc` (float x 21): 토크, IMU 데이터 등

## 🚀 실행 방법 (How to Use)

1.  **준비:**
      * Python 3.x가 설치되어 있어야 합니다.
      * 변환할 `.bin` 파일을 스크립트와 같은 폴더에 둡니다.
2.  **실행:**
    터미널(CMD)에서 다음 명령어를 실행합니다.
    ```bash
    python data_decoder_xm10.py
    ```
3.  **확인:**
    생성된 `result10.csv` 파일을 엑셀이나 텍스트 에디터로 엽니다.

## 💡 참고 사항 (Note)

  * 펌웨어에서 데이터 구조체를 변경했다면, 이 스크립트의 `STRUCT_FMT`와 `CSV_HEADER` 변수도 동일하게 수정해야 올바르게 변환됩니다.

<!-- end list -->

````

---

# 2. `cdc_serial_data_logging.py` (Real-time Serial Logger)

이 스크립트는 XM10과 USB CDC(가상 시리얼)로 연결하여, 실시간으로 들어오는 **바이너리 데이터 스트림**을 수신하고 즉시 **CSV 파일로 저장**하는 PC용 로거입니다.

---

```markdown
# CDC Serial Data Logger

XM10과 PC를 USB 케이블로 연결하여 실시간 센서 데이터를 수신하고 로그 파일로 저장하는 파이썬 스크립트입니다. `TeraTerm` 같은 터미널 프로그램 대신 이 스크립트를 사용하면 **고속 바이너리 데이터**를 손실 없이 받아 파일로 남길 수 있습니다.

## 📌 개요 (Overview)

* **통신 방식:** USB CDC (Virtual COM Port)
* **속도:** 2ms 주기 (500Hz) 스트리밍 지원
* **기능:**
    * 시리얼 포트 자동 연결 및 설정
    * "모니터링 시작" 명령어(`AGRB MON START`) 자동 전송
    * 멀티스레딩을 이용한 데이터 수신 버퍼링 (데이터 유실 방지)
    * 실시간 파싱 및 CSV 파일 저장

## ⚙️ 설정 (Configuration)

사용자의 PC 환경에 맞춰 포트 번호를 설정해야 합니다.

```python
# --- 통신 설정 ---
SERIAL_PORT = 'COM6'  # [중요] 장치관리자에서 확인한 포트 번호로 변경하세요!
BAUD_RATE   = 921600  # 전송 속도 (CDC에서는 크게 중요하지 않음)
````

## 📦 요구 사항 (Prerequisites)

이 스크립트는 시리얼 통신을 위해 `pyserial` 라이브러리가 필요합니다.

```bash
pip install pyserial
```

## 🚀 실행 방법 (How to Use)

1.  **연결:** XM10 보드와 PC를 USB C타입 케이블로 연결합니다.
2.  **포트 확인:** 윈도우 `장치 관리자` -\> `포트 (COM & LPT)`에서 `STMicroelectronics Virtual COM Port`의 번호(예: COM6)를 확인합니다.
3.  **설정 수정:** 스크립트 파일의 `SERIAL_PORT` 변수를 확인된 번호로 수정합니다.
4.  **실행:**
    ```bash
    python cdc_serial_data_logging.py
    ```
5.  **종료:**
    로깅을 멈추려면 터미널에서 `Ctrl + C`를 누릅니다. 스크립트가 종료되면서 파일이 안전하게 저장됩니다.

## 📝 출력 결과 (Output)

  * 실행 시 `cdc_monitoring_data_log.csv` 형식의 파일이 생성됩니다.
  * 화면에는 1000개 패킷마다 점(`.`)이 찍히며 수신 상태를 보여줍니다.
  * **[주의]** 엑셀로 파일을 열어둔 상태에서는 스크립트가 파일에 쓸 수 없어 에러가 발생할 수 있습니다. 실행 전 엑셀을 닫아주세요.

<!-- end list -->

```

---

이 두 개의 `README.md` 파일은 각각의 파이썬 스크립트와 함께 제공되어, End User가 펌웨어뿐만 아니라 **데이터 분석 툴**까지 완벽하게 활용할 수 있도록 가이드할 것입니다.
```
