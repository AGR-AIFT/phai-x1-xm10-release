# API Reference: USB Data Logging

> **헤더 파일:** `xm_api_usb.h`
>
> USB 메모리(MSC)에 센서 데이터를 바이너리로 고속 저장하고,
> Python 디코더로 CSV 변환하여 분석하는 완전한 데이터 로깅 시스템입니다.
>
> 📌 **이 페이지를 읽고 나면**: 등록 기반 USB 자동 로깅 + 에러 모니터링 + 파일 롤링 + 세션 마커를 활용할 수 있습니다.
> ⏱️ 예상 학습 시간: 30분
> 🧰 사전 지식: [Ex.10a~10c](../../examples/10a_MSC_Basic_Log/) MSC 시리즈 + FAT32 / 32KB cluster USB 메모리
> 🎯 핵심 함수: `XM_SetUsbLogSource` / `XM_StartUsbDataLog` / `XM_StopUsbDataLog` / `XM_GetUsbLogStatus` / `XM_InsertUsbLogMarker`

---

## 📌 개요 (Overview)

XM10 USB Data Logging API는 1ms 실시간 제어루프의 성능을 보장하면서
대용량 센서 데이터를 USB 플래시 드라이브에 안전하게 저장합니다.

**주요 특징:**
- **3단계 비동기 파이프라인**: UserTask(1ms) → Hot Buffer(128KB) → Cold Buffer(PSRAM 4MB) → USB
- **자동 파일 포맷**: 32-byte 파일 헤더 + 4KB 블록 CRC + 12-byte 풋터 (Self-describing)
- **RTC 타임스탬프**: 파일 생성 시각이 실제 시계 기반으로 자동 기록
- **이벤트 마커**: 로깅 중 모드 전환/에러 등 특정 시점을 표시
- **디스크 모니터링**: 잔여 용량 실시간 감시 + 부족 시 자동 경고
- **자동 세션 넘버링**: 세션명 NULL 전달 시 S_001, S_002, ... 자동 생성

---

## 🛠 데이터 구조 (Data Structures)

### XmLogStatus_e — 로거 상태

```c
typedef enum {
    XM_LOG_STATUS_IDLE,               // 중지됨 (초기 상태)
    XM_LOG_STATUS_LOGGING,            // 정상 로깅 중
    XM_LOG_STATUS_WARNING_QUEUE_FULL, // 버퍼 사용률 높음
    XM_LOG_STATUS_WARNING_DISK_LOW,   // 디스크 잔여 50MB 미만
    XM_LOG_STATUS_ERROR_STOPPED,      // 에러로 강제 중지됨
} XmLogStatus_e;
```

| 값 | 의미 | 대응 |
|----|------|------|
| `IDLE` | 로깅 비활성 | 정상 대기 |
| `LOGGING` | 정상 로깅 중 | 데이터 기록 진행 |
| `WARNING_QUEUE_FULL` | 내부 버퍼 90%+ | USB 쓰기 지연 발생 중 (데이터 유실 위험) |
| `WARNING_DISK_LOW` | 디스크 잔여 50MB 미만 | 곧 용량 부족 — 세션 종료 권장 |
| `ERROR_STOPPED` | 에러로 강제 중지 | USB 분리, 쓰기 실패 등 — 재시작 필요 |

### XmLogStats_t — 세션 실시간 통계

```c
typedef struct {
    uint32_t total_bytes;          // 총 기록 바이트 수
    uint32_t total_records;        // 총 레코드 수
    uint32_t dropped_records;      // 누락된 레코드 수 (버퍼 오버플로)
    uint32_t write_errors;         // 쓰기 실패 횟수
    uint32_t duration_ms;          // 세션 경과 시간 (ms)
    uint8_t  hot_buffer_percent;   // Hot Buffer 피크 사용률 (0~100)
    uint8_t  cold_buffer_percent;  // Cold Buffer 피크 사용률 (0~100)
    uint32_t disk_free_mb;         // USB 잔여 용량 (MB)
    uint32_t disk_total_mb;        // USB 전체 용량 (MB)
} XmLogStats_t;
```

- `dropped_records > 0` → 데이터 유실 발생. 구조체 크기 줄이거나 로깅 주기 조정 필요.
- `hot_buffer_percent` → 피크 기준. 80% 이상이면 GC stall 마진 부족 경고.
- `disk_free_mb` → 10초 주기 캐시값. 실시간 정밀도는 아니지만 모니터링 충분.

### XmLogMarkerType_e — 이벤트 마커 타입

```c
typedef enum {
    XM_LOG_MARKER_USER   = 0x01,  // 수동 마킹 (버튼/명령)
    XM_LOG_MARKER_MODE   = 0x02,  // 모드 전환
    XM_LOG_MARKER_ERROR  = 0x03,  // 에러 발생
    XM_LOG_MARKER_SYNC   = 0x04,  // 시간 동기점
} XmLogMarkerType_e;
```

---

## 📚 함수 (Functions)

### 기본 설정 (Setup)

#### `XM_SetUsbLogSource()`

로깅할 데이터 소스를 등록합니다. `User_Setup()`에서 1회 호출합니다.

```c
void XM_SetUsbLogSource(void* data_ptr, uint32_t size);
```

- **Parameters**
  - `data_ptr` — 저장할 구조체의 주소 (`&myData`)
  - `size` — 구조체의 크기 (`sizeof(myData)`)

- **Example**
  ```c
  typedef struct {
      float hip_angle_L;
      float hip_angle_R;
  } MyLogData_t;

  MyLogData_t myData;

  void User_Setup(void) {
      XM_SetUsbLogSource(&myData, sizeof(myData));
  }
  ```

#### `XM_SetUsbLogAutoTimestamp()`

자동 타임스탬프(4-byte tick_ms)를 활성화/비활성화합니다.

```c
void XM_SetUsbLogAutoTimestamp(bool enabled);
```

- **Parameters**
  - `enabled` — `true`: 매 레코드 앞에 tick 자동 삽입 (기본값). `false`: 비활성화.
- **Note** `User_Setup()`에서 `XM_StartUsbDataLog()` 호출 전에 설정하세요.

#### `XM_SetUsbLogRollingSize()`

파일 롤링(분할) 크기를 설정합니다.

```c
void XM_SetUsbLogRollingSize(uint32_t size_mb);
```

- **Parameters**
  - `size_mb` — 파일 분할 크기 (MB). 1~100, 기본값: 10.

---

### 세션 제어 (Session Control)

#### `XM_IsUsbLogReady()`

USB 저장 장치가 연결되고 로깅이 준비되었는지 확인합니다.

```c
bool XM_IsUsbLogReady(void);
```

- **Return** USB 준비 시 `true`, 아니면 `false`.
- **Note** 2ms 실시간 루프에서 안전하게 호출 가능합니다.

#### `XM_StartUsbDataLog()`

로깅 세션을 시작합니다.

```c
bool XM_StartUsbDataLog(const char* sessionName, const char* metadata);
```

- **Parameters**
  - `sessionName` — 세션 폴더명 (예: `"S_001_TestRun"`). **NULL 또는 빈 문자열** 전달 시 `S_001`, `S_002`, ... 자동 생성.
  - `metadata` — 데이터 구조를 설명하는 문자열 (예: `"hip_L(float), hip_R(float)"`)
- **Return** 성공 시 `true`. USB 미준비, 큐 만료, 잘못된 문자 시 `false`.
- **Warning** 최대 100ms 블로킹 가능 — **2ms 루프에서 호출 금지.**

- **Example**
  ```c
  // 수동 세션명
  XM_StartUsbDataLog("Gait_001", "hip_L(float), hip_R(float)");

  // 자동 세션 넘버링
  XM_StartUsbDataLog(NULL, "hip_L(float), hip_R(float)");
  // → /LOGS/S_001/, /LOGS/S_002/, ... 순차 생성
  ```

#### `XM_StopUsbDataLog()`

로깅 세션을 중지합니다.

```c
void XM_StopUsbDataLog(void);
```

- **Warning** **2ms 루프에서 호출 금지.** 상태 전이 함수(on_exit)에서 1회 호출 권장.

---

### 상태 모니터링 (Status & Monitoring)

#### `XM_GetUsbLogStatus()`

현재 로거 상태를 반환합니다.

```c
XmLogStatus_e XM_GetUsbLogStatus(void);
```

- **Return** `XmLogStatus_e` 열거형 값
- **Note** 2ms 루프에서 안전하게 호출 가능. LED 피드백 등에 활용.

- **상태 전이 다이어그램**
  ```
  IDLE ──(Start)──▶ LOGGING ──(Stop)──▶ IDLE
                      │
                      ├──(Buffer 90%+)──▶ WARNING_QUEUE_FULL ──(회복)──▶ LOGGING
                      ├──(Disk <50MB)───▶ WARNING_DISK_LOW
                      └──(Error)────────▶ ERROR_STOPPED
  ```

#### `XM_GetUsbLogStats()`

로깅 세션의 실시간 통계를 조회합니다.

```c
bool XM_GetUsbLogStats(XmLogStats_t* out_stats);
```

- **Parameters**
  - `out_stats` — 통계가 복사될 `XmLogStats_t` 구조체 포인터
- **Return** 성공 시 `true`, NULL 파라미터 시 `false`.

- **Example**
  ```c
  XmLogStats_t stats;
  if (XM_GetUsbLogStats(&stats)) {
      if (stats.dropped_records > 0) {
          // 데이터 유실 경고
      }
      if (stats.disk_free_mb < 100) {
          // 용량 부족 경고
      }
  }
  ```

#### `XM_GetUsbDiskFreeMB()` / `XM_GetUsbDiskTotalMB()`

USB 디스크 용량을 조회합니다 (10초 주기 캐시값).

```c
uint32_t XM_GetUsbDiskFreeMB(void);
uint32_t XM_GetUsbDiskTotalMB(void);
```

- **Return** 용량 (MB). USB 미연결 시 0.

---

### 이벤트 마커 (Event Markers)

#### `XM_InsertUsbLogMarker()`

로깅 중 이벤트 마커를 삽입합니다.

```c
bool XM_InsertUsbLogMarker(XmLogMarkerType_e type, uint16_t data);
```

- **Parameters**
  - `type` — 마커 타입 (`XmLogMarkerType_e`)
  - `data` — 컨텍스트 데이터 (에러 코드, 모드 ID 등. 불필요 시 0)
- **Return** 성공 시 `true`. 로깅 비활성 또는 버퍼 부족 시 `false`.
- **Note** 2ms 루프에서 안전하게 호출 가능 (Non-blocking).

- **사용 시나리오**

  | 마커 타입 | 용도 | data 예시 |
  |----------|------|----------|
  | `USER` | 수동 마킹 (버튼, 디버그) | 0 |
  | `MODE` | 상태 전이 (Active→Standby) | 새 모드 ID |
  | `ERROR` | 에러 발생 시점 | 에러 코드 |
  | `SYNC` | 외부 시스템과 시간 동기 | 동기 ID |

- **Example**
  ```c
  // Active 모드 진입 시
  XM_InsertUsbLogMarker(XM_LOG_MARKER_MODE, STATE_ACTIVE);

  // 에러 감지 시
  XM_InsertUsbLogMarker(XM_LOG_MARKER_ERROR, err_code);
  ```

---

## 🔄 세션 출력 파일 구조 (Session Output)

```
/LOGS/<SessionName>/
  ├── metadata.txt            ← 데이터 포맷 + System 정보
  ├── summary.txt             ← 세션 통계 + 종료 상태
  ├── data_000_part_000.bin   ← 바이너리 데이터 (파일 헤더 + CRC 블록 + 풋터)
  ├── data_000_part_001.bin   ← (파일 롤링 시 추가 파트)
  └── ...
```

### metadata.txt

세션 시작 시 자동 생성. Python 디코더가 이 파일을 파싱하여 바이너리 포맷을 결정합니다.

```
hip_L(float), hip_R(float), gait_phase(uint8_t), _pad(3bytes)

=== System Info ===
auto_timestamp=1
timestamp_bytes=4
user_payload_bytes=12
record_header_bytes=4
record_total_bytes=20
rolling_size_mb=10
buffer_size_kb=128
logger_period_ms=100
rtc_start=2026-03-04 14:30:25
file_format_version=1
```

### summary.txt

세션 종료(정상/에러) 시 자동 생성. 세션 통계를 기록합니다.

```
=== Session Summary ===
total_bytes_written=15728640
total_packets_logged=786432
dropped_packets=0
write_errors=0
sync_count=150
start_tick=1000
end_tick=1573864
status=OK
error_reason=NORMAL
rtc_start=2026-03-04 14:30:25
rtc_end=2026-03-04 14:56:38
```

- `status=OK` → 정상 종료. `status=ERROR` → 에러 종료 (USB 분리, 쓰기 실패 등).
- `error_reason` → `NORMAL`, `HOT_OVERFLOW`, `USB_DISCONNECT`, `COLD_OVERFLOW`, `WRITE_FAILURE`, `ROLLING_FAILURE`

### .bin 바이너리 포맷

```
+-------------------------------------+
| FileHeader (32 bytes)               |  magic + version + flags + sizes
+-------------------------------------+
| Block 0 (4KB data)                  |  [Record][Record]...[Record]
| CRC32 (4 bytes)                     |
+-------------------------------------+
| Block 1 (4KB data)                  |
| CRC32 (4 bytes)                     |
+-------------------------------------+
| ...                                 |
+-------------------------------------+
| Block M (마지막, <=4KB)             |
| CRC32 (4 bytes)                     |
+-------------------------------------+
| FileFooter (12 bytes)               |  record_count + data_bytes + magic
+-------------------------------------+
```

- **FileHeader**: 매직넘버 `0xA14C4F47`, 포맷 버전, 레코드 크기 정보 포함. 디코더가 metadata.txt 없이도 파싱 가능.
- **Block CRC**: 4KB 데이터마다 STM32 HW CRC32 삽입. 손상 시 해당 블록만 스킵, 나머지 복구.
- **FileFooter**: 풋터 매직 `0x474F4CA1`. 풋터 없음 = 비정상 종료 (전원 차단 등).

---

## 🐍 Python 디코더 사용법

### 설치

Python 3.6 이상 필요. 외부 패키지 의존성 없음 (표준 라이브러리만 사용).

### 실행

```bash
# 기본 사용
python data_decoder_xm10.py /LOGS/BasicTest

# struct format 수동 지정 (metadata에 타입 미포함 시)
python data_decoder_xm10.py /LOGS/BasicTest --fmt "<2fB3x"

# stale 레코드 trim 비활성화
python data_decoder_xm10.py /LOGS/BasicTest --no-trim
```

### 출력

- `decoded_output.csv` — 메인 데이터 CSV
- `events.csv` — 이벤트 마커 (마커 사용 시)

### 디코더 기능

| 기능 | 설명 |
|------|------|
| 파일 헤더 자동 감지 | 레거시(헤더 없음) + 신규(헤더 있음) 모두 지원 |
| 블록 CRC 검증 | 손상 블록 감지 + 정상 블록 복구 |
| 이벤트 마커 분리 | 데이터 레코드와 마커 레코드 자동 분리 |
| 풋터 검증 | 파일 정상 종료 여부 확인 |
| 끝부분 stale trim | 세션 종료 시 frozen 레코드 자동 제거 |

---

## ⚠️ 주의사항 (Caveats)

- **RTC 미설정 시**: 파일 타임스탬프가 2025-01-01로 기록됩니다. `XM_RTC_SetDateTime()`으로 시간 설정 권장.
- **비정상 종료 시**: 풋터가 없을 수 있으며, 마지막 블록의 CRC가 불완전할 수 있습니다. 디코더가 레코드 단위로 끝까지 파싱합니다.
- **FATFS 금지문자**: 세션 이름에 `< > : " / \ | ? *` 사용 불가. 사용 시 `false` 반환.
- **구조체 정렬**: 4-byte 정렬을 지키면 `__attribute__((packed))` 없이 안전합니다. 패딩은 메타데이터에 `_pad(Nbytes)` 형식으로 기재.
- **2ms 루프 호출 금지 함수**: `XM_StartUsbDataLog()`, `XM_StopUsbDataLog()` — 블로킹 가능.
