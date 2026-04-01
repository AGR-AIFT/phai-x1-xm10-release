# 예제 10c: 상태 머신(TSM) + 이벤트 마커 + 통계 모니터링 (MSC Advanced Log)

본 예제는 실제 제품 수준의 데이터 로깅 시스템을 구현하는 **고급 패턴**을 다룹니다. 상태 머신(TSM)으로 로깅 흐름을 제어하고, **이벤트 마커**, **실시간 통계 모니터링**, **디스크 용량 관리**를 통합합니다.

## 🎯 학습 목표 (Objective)

* **상태 머신(TSM)**과 데이터 로깅을 연동하는 패턴을 학습합니다.
* `XM_StartUsbDataLog(NULL, ...)` — **자동 세션 넘버링** (`S_001`, `S_002`, ...) 활용법을 익힙니다.
* `XM_InsertUsbLogMarker()` — **이벤트 마커**로 모드 전환/에러 시점을 기록합니다.
* `XM_GetUsbLogStats()` — 세션 종료 시 **통계(Stats)**를 조회하여 품질을 검증합니다.
* `XM_GetUsbDiskFreeMB()` — 로깅 시작 전 **디스크 잔여 용량**을 확인합니다.
* `XM_LOG_STATUS_WARNING_DISK_LOW` — 디스크 부족 경고를 LED로 피드백합니다.

## ⚙️ 동작 원리 (How it Works)

### 상태 전이 (State Transitions)

```
STANDBY ──(BTN1)──▶ ACTIVE ──(BTN2)──▶ STANDBY
                      │                    ▲
                      └──(ERROR 감지)──────┘
```

### Entry/Exit 패턴

* **`Active_Entry()`:**
  1. `XM_GetUsbDiskFreeMB()` — 잔여 용량 100MB 미만이면 시작 거부
  2. `XM_StartUsbDataLog(NULL, metadata)` — `NULL` 전달 시 자동으로 `S_001`, `S_002`, ... 생성
  3. `XM_InsertUsbLogMarker(XM_LOG_MARKER_MODE, XM_STATE_ACTIVE)` — 시작 마커 삽입

* **`Active_Loop()`:**
  - `XM_GetUsbLogStatus()` 실시간 모니터링
  - `WARNING_QUEUE_FULL` → LED2 느린 깜빡임
  - `WARNING_DISK_LOW` → LED2 빠른 깜빡임 (잔여 50MB 미만)
  - `ERROR_STOPPED` → STANDBY 자동 복귀

* **`Active_Exit()`:**
  1. `XM_InsertUsbLogMarker(XM_LOG_MARKER_MODE, XM_STATE_STANDBY)` — 종료 마커 삽입
  2. `XM_GetUsbLogStats()` — 통계 조회 (`dropped_records` > 0이면 LED3 경고)
  3. `XM_StopUsbDataLog()` — 세션 정상 종료

### 파일 롤링

`XM_SetUsbLogRollingSize(5)` — 5MB마다 새 파트 파일로 분할.

### 바이너리 파일 포맷 (Phase 3)

시스템이 자동으로 관리하는 포맷 — 사용자 코드 변경 불필요:

```
+-------------------------------------+
| FileHeader (32B)                    |  magic(0xA14C4F47) + version + flags + sizes
+-------------------------------------+
| Block 0 (4KB data)                  |  [Record][Record]...[Record]
| CRC32 (4B)                          |  HW CRC — 블록 단위 무결성 검증
+-------------------------------------+
| Block 1 (4KB data)                  |
| CRC32 (4B)                          |
+-------------------------------------+
| ...                                 |
+-------------------------------------+
| Block M (마지막, ≤4KB)              |
| CRC32 (4B)                          |
+-------------------------------------+
| FileFooter (12B)                    |  record_count + data_bytes + footer_magic
+-------------------------------------+
```

* **CRC 검증**: Python 디코더가 블록별 CRC를 자동 검증. 손상 블록만 스킵, 나머지 복구.
* **RTC 타임스탬프**: 파일 생성 시각이 RTC 기준으로 자동 기록 (PC에서 파일 속성으로 확인 가능).

## 📊 LED 피드백 표

| LED   | 상태     | 의미 |
|-------|----------|------|
| LED1 OFF    | STANDBY  | 대기 중 |
| LED1 BLINK  | ACTIVE   | 로깅 진행 중 |
| LED2 BLINK (느림) | WARNING  | 버퍼 90%+ 또는 쓰기 지연 발생 |
| LED2 BLINK (빠름) | DISK LOW | 디스크 잔여 50MB 미만 |
| LED3 SOLID  | ERROR    | 로깅 중단, STANDBY로 자동 복귀 |
| LED3 HEARTBEAT | -    | USB 메모리 미준비 |

## 🚀 실행 방법 (How to Use)

1.  USB 메모리를 연결하고 코드를 업로드합니다.
2.  **BTN1 클릭:** 로깅 시작 → LED1 깜빡임
    - 디스크 잔여 용량이 100MB 미만이면 LED3 깜빡임 후 시작 거부
3.  로깅 중 LED2가 깜빡이면 쓰기 속도가 느려지고 있다는 경고입니다.
4.  **BTN2 클릭:** 로깅 정지 → 세션 통계 출력 → 모든 LED 꺼짐
5.  에러 발생 시 LED3이 켜지고 자동으로 STANDBY 상태로 복귀합니다.
6.  USB 메모리에서 `/LOGS/S_001/` 폴더를 확인합니다. (자동 넘버링)
7.  **Python 디코더로 CSV 변환:**
    ```bash
    python data_decoder_xm10.py /LOGS/S_001
    ```

### 세션 출력 파일 구조

```
/LOGS/S_001/
├── metadata.txt          # 구조체 정보 + RTC 시작 시각
├── data_001_part_001.bin  # 바이너리 데이터 (헤더 + CRC 블록 + 풋터)
├── data_001_part_002.bin  # 파일 롤링 시 자동 분할
├── summary.txt           # 세션 통계 (status, rtc_start/end, 에러 정보)
└── decoded/              # (디코더 실행 후)
    ├── data_001.csv      # 디코딩된 센서 데이터
    └── events.csv        # 이벤트 마커 로그
```

## 💡 직접 해보기 (Things to Try)

* `XM_SetUsbLogRollingSize(1)`로 설정하여 1MB마다 파일이 분할되는 것을 확인해보세요.
* 로깅 중 USB 메모리를 의도적으로 뽑아 에러 복구 동작을 테스트해보세요. `summary.txt`에서 `status=ERROR, error_reason=USB_DISCONNECT`를 확인합니다.
* `XM_InsertUsbLogMarker(XM_LOG_MARKER_USER, 0x1234)`를 BTN2 롱프레스에 연결하여 수동 마커를 찍어보세요. 디코더의 `events.csv`에서 확인할 수 있습니다.
* `XM_GetUsbLogStats()`로 `dropped_records`와 `write_errors` 값을 모니터링하는 코드를 추가해보세요.
* `summary.txt`에서 `rtc_start=`, `rtc_end=` 필드를 확인하여 세션 시작/종료 시각을 확인해보세요.
* 실제 보행 테스트에서 10분 이상 로깅 후 디코더를 실행하여 CRC 검증 결과와 이벤트 마커를 확인해보세요.
* 의도적으로 .bin 파일의 1바이트를 변경하여 디코더가 해당 블록의 CRC 불일치를 보고하는지 확인해보세요.
