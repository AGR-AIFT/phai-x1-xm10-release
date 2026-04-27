# 예제 10a: USB 메모리에 데이터 저장하기 — 기초 (MSC Basic Log)

본 예제는 USB 메모리에 센서 데이터를 **바이너리(Binary)**로 저장하는 가장 기본적인 방법을 다룹니다. 버튼 하나로 로깅을 시작하고, 다른 버튼으로 정지합니다.

## 🎯 학습 목표 (Objective)

* USB 메모리가 준비되었는지 `XM_IsUsbLogReady()`로 확인하는 방법을 학습합니다.
* 저장할 데이터 구조체를 `XM_SetUsbLogSource()`로 등록하는 방법을 이해합니다.
* `XM_StartUsbDataLog()` / `XM_StopUsbDataLog()` 로 로깅 세션을 관리합니다.
* **자동 타임스탬프(Auto Timestamp)** 기능이 기본으로 활성화되어 있음을 확인합니다.

## ⚙️ 동작 원리 (How it Works)

* **데이터 등록:** `User_Setup()`에서 `BasicLog_t` 구조체(float 1개)를 등록합니다.
* **자동 타임스탬프:** 시스템이 매 레코드마다 4바이트 `tick_ms`를 자동으로 앞에 붙여 저장합니다. 사용자가 별도로 시간 관리를 할 필요가 없습니다.
* **2ms 주기 저장:** 로깅이 시작되면, 제어 루프(2ms)마다 자동으로 구조체 내용을 USB 메모리에 기록합니다.
* **세션 관리:** 저장이 끝나면 `/LOGS/BasicTest/` 폴더에 `metadata.txt`, `summary.txt`, `data_000_part_000.bin` 파일이 자동 생성됩니다.
* **자동 파일 포맷:** 각 `.bin` 파일에 32-byte 헤더와 12-byte 풋터가 자동 기록됩니다. RTC 시계 기반 타임스탬프도 파일/메타데이터에 자동 반영됩니다.
* **블록 CRC:** 4KB 데이터 블록마다 CRC32 무결성 체크섬이 자동 삽입됩니다. 데이터 손상 시 디코더가 자동으로 감지합니다.

## 📂 저장 파일 구조

```
/LOGS/BasicTest/
  ├── metadata.txt            ← 데이터 포맷 + System 정보 + RTC 시각 (자동 생성)
  ├── summary.txt             ← 세션 통계 + 종료 상태 (Stop 시 자동 생성)
  └── data_000_part_000.bin   ← 바이너리 데이터 (헤더 + CRC 블록 + 풋터)
```

## 🚀 실행 방법 (How to Use)

1.  **USB 메모리 연결:** XM10 보드의 USB Host 포트에 USB 플래시 드라이브를 꽂습니다.
2.  코드를 업로드하고 전원을 켭니다.
3.  **BTN1 클릭:** 로깅 시작 → LED1이 깜빡이면 저장 중입니다.
4.  **BTN2 클릭:** 로깅 정지 → LED1이 꺼집니다.
5.  USB 메모리를 PC에 꽂고 `/LOGS/BasicTest/` 폴더를 확인합니다.
6.  **Python 디코더로 CSV 변환:**
    ```bash
    python data_decoder_xm10.py /LOGS/BasicTest
    ```

## 💡 직접 해보기 (Things to Try)

* `BasicLog_t` 구조체에 `rightHipAngle` 필드를 추가하여 2개 채널을 동시에 저장해보세요.
* `metadata.txt`와 `summary.txt` 파일 내용을 열어 어떤 정보가 자동으로 기록되는지 확인해보세요. `summary.txt`에서 `status=OK`와 `rtc_start=`/`rtc_end=` 필드를 확인해보세요.
* PC에서 `.bin` 파일의 속성을 확인하여 **RTC 기반 생성 시각**이 반영되었는지 확인해보세요 (1980-01-01이 아닌 실제 시각).
* 다음 예제 **10b**에서 더 복잡한 구조체와 수동 타임스탬프를 다루어봅니다.
