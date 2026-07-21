# `xm_api_usb.h` — USB 데이터 로깅 · 실시간 스트리밍

> **대상 헤더**: `XM_FW/XM_API/xm_api_usb.h`
> **관련 개념 문서**: [05. USB 시리얼](../05-usb-connectivity.md) · [06. USB 메모리 로깅](../06-usb-data-logging.md)
> **관련 예제**: [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) · [07_CDC_Basic_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/07_CDC_Basic_Print/) · [08_CDC_Sensor_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/) · [09_CDC_Stream](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) · [10a_MSC_Basic_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10a_MSC_Basic_Log/) · [10b_MSC_Custom_Struct](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10b_MSC_Custom_Struct/) · [10c_MSC_Advanced_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10c_MSC_Advanced_Log/) · [18_Debug_Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/) · [34_MSC_GaitAnalysis_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/)

이 헤더 하나에 **USB 메모리 로깅(MSC)** 과 **PC 실시간 통신(CDC)** 두 도메인이 함께 정의되어 있습니다. MSC 쪽은 사용자 구조체를 등록해두면 백그라운드 태스크가 알아서 USB 메모리에 저장하고, CDC 쪽은 PhAI Studio·PuTTY 같은 PC 클라이언트와 텍스트/바이너리를 주고받습니다. Rev2.0 부터는 여기에 더해 **하나의 USB-CDC 케이블을 PhAI Studio 실시간 스트리밍 / 일반 터미널 두 가지 용도 중 무엇으로 쓸지 지정**하는 호스트 프로파일 API(`XM_USB_SetHostProfile`)가 추가되었습니다. (생산 검사 GUI 대응은 보드 내부에서 자동 처리되므로 사용자 선택지에는 없습니다.)

---

## 언제 사용하나

센서 데이터를 USB 메모리에 장시간 기록하고 싶거나, PC와 시리얼로 실시간 데이터를 주고받고 싶을 때 사용합니다. 등록 기반 자동화(Setup에서 소스 등록 → System이 주기적으로 자동 처리) 원리와 파일 포맷·Python 디코더 사용법 같은 전체 그림은 [05. USB 시리얼](../05-usb-connectivity.md), [06. USB 메모리 로깅](../06-usb-data-logging.md) 두 문서를 먼저 읽어보는 것을 권장합니다 — 이 페이지는 헤더에 선언된 **모든 함수·타입의 시그니처/파라미터 상세**만 다루며, 두 개념 문서가 아직 다루지 않는 Rev2.0 신규 **USB-CDC 호스트 프로파일 API** 도 포함합니다.

---

## 함수 목록

**데이터 소스 등록**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SetUsbLogSource`](#xm_setusblogsource) | [MSC] USB 메모리에 저장할 데이터 소스 등록 |
| [`XM_SetUsbStreamSource`](#xm_setusbstreamsource) ⚠️ Deprecated | [CDC] PC로 스트리밍할 데이터 소스 등록 (레거시) |

**MSC 로깅 — 세션 제어**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_IsUsbLogReady`](#xm_isusblogready) | USB 저장장치 연결 + 로깅 준비 여부 확인 |
| [`XM_StartUsbDataLog`](#xm_startusbdatalog) | 로깅 세션 시작 |
| [`XM_StopUsbDataLog`](#xm_stopusbdatalog) | 로깅 세션 중지 |
| [`XM_GetActiveUsbSessionName`](#xm_getactiveusbsessionname) | 현재/마지막 활성 세션 이름 조회 (Option A 재진입용) |
| [`XM_GetUsbLogStatus`](#xm_getusblogstatus) | 로거 상태 조회 |

**MSC 로깅 — 설정**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SetUsbLogAutoTimestamp`](#xm_setusblogautotimestamp) | 자동 타임스탬프 삽입 on/off |
| [`XM_SetUsbLogRollingSize`](#xm_setusblogrollingsize) | 파일 롤링(분할) 크기 설정 |

**MSC 로깅 — 통계 · 디스크 용량**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_GetUsbLogStats`](#xm_getusblogstats) | 세션 실시간 통계 조회 |
| [`XM_GetUsbDiskFreeMB` / `XM_GetUsbDiskTotalMB`](#xm_getusbdiskfreemb--xm_getusbdisktotalmb) | USB 잔여/전체 용량 조회 (10초 캐시) |

**MSC 로깅 — 이벤트 마커**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_InsertUsbLogMarker`](#xm_insertusblogmarker) | 로깅 스트림에 이벤트 마커 삽입 |

**CDC — 연결 · 스트리밍 상태**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_IsUsbStreamConnected`](#xm_isusbstreamconnected) | PC와 CDC 가상 시리얼 포트 연결 여부 |
| [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive) | 스트리밍 활성 여부 |
| [`XM_SetUsbAutoStream`](#xm_setusbautostream) | Auto-Stream 모드 on/off |

**CDC — 호스트 프로파일** 🟢 Rev 2.0 전용

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_USB_SetHostProfile`](#xm_usb_sethostprofile) 🟢 | USB-CDC 호스트 프로파일 지정 (PhAI Studio / 터미널) |

**CDC — 데이터 전송 (레거시 + Custom Data)**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SendUsbData`](#xm_sendusbdata) ⚠️ Deprecated | PhAI 패킷으로 래핑해 전송 (Module ID 고정) |
| [`XM_SetUsbStreamModuleId`](#xm_setusbstreammoduleid) ⚠️ Deprecated | 스트리밍 데이터의 Module ID 설정 |
| [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta) | User Custom 채널 메타데이터(JSON) 등록 |
| [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid) | 지정 Module ID로 float[] 데이터 전송 |

**CDC — 디버그 메시지 · 수신**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SendUsbDebugMessage`](#xm_sendusbdebugmessage) | PC로 raw 텍스트 디버그 메시지 전송 |
| [`XM_GetUsbData`](#xm_getusbdata) | PC로부터 데이터 수신 |

---

## 함수 상세

### `XM_SetUsbLogSource`

```c
void XM_SetUsbLogSource(void* data_ptr, uint32_t size);
```

**[MSC]** USB 메모리에 저장할 데이터 소스를 등록합니다. 등록된 주소의 데이터를 System이 주기적으로 읽어 파일에 기록합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `data_ptr` | `void*` | 저장할 구조체의 주소 (`&myData`) |
| `size` | `uint32_t` | 구조체의 크기 (`sizeof(myData)`) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 컨텍스트 기준. 로깅 시작(`XM_StartUsbDataLog`) 전에 1회 등록하면 됩니다.

**예제**

```c
typedef struct {
    float hip_angle_L;
    float hip_angle_R;
} MyLogData_t;

MyLogData_t myData;

void Control_Setup(void) {
    XM_SetUsbLogSource(&myData, sizeof(myData));
}
```

**참고**: [`XM_StartUsbDataLog`](#xm_startusbdatalog), [06. USB 메모리 로깅](../06-usb-data-logging.md)

---

### `XM_SetUsbStreamSource`

```c
void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
```

> ⚠️ **Deprecated** — Total Data(0x20) 자동 전송으로 대체되었습니다. 추가 채널이 필요하면 [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta) + [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid) 조합을 사용하세요.

**[CDC]** PC로 실시간 스트리밍할 데이터 소스를 등록합니다 (레거시 방식).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `data_ptr` | `void*` | 전송할 구조체의 주소 |
| `size` | `uint32_t` | 구조체의 크기 |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 컨텍스트 기준.

**참고**: [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta), [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_IsUsbLogReady`

```c
bool XM_IsUsbLogReady(void);
```

USB 저장 장치(MSC)가 연결되고 로깅이 준비되었는지 확인합니다. System Layer의 `usb_mode_handler`가 관리하는 상태 플래그를 읽기만 하므로 가볍습니다.

**파라미터**: 없음

**반환값**: `bool` — 준비되었으면 `true`, 아니면 `false`

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 2ms 실시간 루프에서 안전하게 호출할 수 있습니다 (Non-blocking).

**예제**

```c
if (XM_IsUsbLogReady() && !logging_started) {
    XM_StartUsbDataLog(NULL, "hip_L(float), hip_R(float)");
    logging_started = true;
}
```

**참고**: [`XM_StartUsbDataLog`](#xm_startusbdatalog)

---

### `XM_StartUsbDataLog`

```c
bool XM_StartUsbDataLog(const char* sessionName, const char* metadata);
```

USB 데이터 로깅 세션을 시작합니다. 저순위 로깅 태스크가 `/LOGS/[sessionName]` 폴더와 `metadata.txt`를 생성하고 `data_000...bin` 파일 쓰기를 준비합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `sessionName` | `const char*` | 세션(폴더) 이름 (예: `"S_001_TestRun"`). `NULL` 또는 빈 문자열 전달 시 `S_001`, `S_002`, ... 자동 생성 |
| `metadata` | `const char*` | 저장될 바이너리 데이터를 설명하는 문자열. `metadata.txt`에 그대로 기록됨 |

**반환값**: `bool` — 명령 큐 전송 성공 시 `true`, 큐가 꽉 찼거나 USB 미준비 시 `false`

**⚠️ 호출 컨텍스트**: 이 함수는 저순위 로깅 태스크에 명령을 보내며, 큐가 꽉 찬 경우 **최대 100ms까지 블로킹될 수 있습니다.** **2ms 실시간 루프(`Control_Loop()`) 안에서 절대 호출하지 마십시오** — 상태 전이 진입 함수(`EnterActive` 등)에서 1회만 호출하는 방식을 권장합니다.

**예제**

```c
// 수동 세션명
XM_StartUsbDataLog("Gait_001", "hip_L(float), hip_R(float)");

// 자동 세션 넘버링
XM_StartUsbDataLog(NULL, "hip_L(float), hip_R(float)");
// → /LOGS/S_001/, /LOGS/S_002/, ... 순차 생성
```

**참고**: [`XM_StopUsbDataLog`](#xm_stopusbdatalog), [`XM_GetActiveUsbSessionName`](#xm_getactiveusbsessionname), [06. USB 메모리 로깅 §세션 출력 파일 구조](../06-usb-data-logging.md#세션-출력-파일-구조-session-output)

---

### `XM_StopUsbDataLog`

```c
void XM_StopUsbDataLog(void);
```

USB 데이터 로깅 세션을 중지합니다. 저순위 로깅 태스크에게 현재 열린 파일을 닫고 로깅을 종료하라는 명령을 비동기적으로 전송합니다.

**파라미터**: 없음

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: **2ms 실시간 루프(`Control_Loop()`) 안에서 호출하지 마십시오.** USB를 뽑기 전 반드시 호출해야 파일이 정상 종료됩니다.

**참고**: [`XM_StartUsbDataLog`](#xm_startusbdatalog)

---

### `XM_GetActiveUsbSessionName`

```c
void XM_GetActiveUsbSessionName(char* out_buf, uint32_t buf_size);
```

**[Option A]** 현재/마지막 활성 USB 세션 이름을 조회합니다. Emergency Stop 후 재진입 시 **같은 폴더에 이어쓰기** 위한 용도로 설계되었습니다. 의도된 사용 흐름은: 최초 `XM_StartUsbDataLog("", ...)`로 시작 → boot_count 기반 자동 이름 생성 → 이 API로 이름 조회 → 예제가 보관 → 재진입 시 `XM_StartUsbDataLog(보관_이름)` 호출 → FW가 같은 폴더에 `data_001_*`, `data_002_*` 등을 증분 생성합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out_buf` | `char*` | 세션 이름이 복사될 버퍼 |
| `buf_size` | `uint32_t` | 버퍼 크기 (32바이트 이상 권장) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. 헤더는 ISR-안전성을 별도로 명시하지 않습니다.

> ⚠️ **알려진 레이스 컨디션 — 직접 호출 시 주의**: 세션 이름은 저순위 `DataLoggerTask`가 비동기로 갱신합니다. `XM_StartUsbDataLog()` 호출 **직후 바로 이어서** 이 함수를 호출하면, 아직 갱신 전이라 빈 문자열이나 이전 세션 이름을 받을 수 있습니다. 실제로 [Ex.34 (`msc_gait_analysis_log.c`)](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) 는 이 문제를 겪은 뒤, 이 API에 의존하는 대신 **예제 코드가 직접 RTC/tick 기반으로 세션 이름을 생성**해 `XM_StartUsbDataLog()`에 동기적으로 전달하고, 그 이름을 자체 변수에 보관해 재진입 시 재사용하는 방식(레이스 없음)으로 전환했습니다. 재진입 로직을 구현할 때는 이 함수의 반환 타이밍에 의존하기보다 Ex.34 패턴을 참고하는 것을 권장합니다.

**참고**: [`XM_StartUsbDataLog`](#xm_startusbdatalog), [Ex.34 MSC_GaitAnalysis_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/)

---

### `XM_GetUsbLogStatus`

```c
XmLogStatus_e XM_GetUsbLogStatus(void);
```

현재 로거의 상태를 확인합니다. 로깅이 강제 중단되었는지(`ERROR_STOPPED`), 버퍼가 꽉 차고 있는지(`WARNING_QUEUE_FULL`) 등을 이 함수로 확인할 수 있습니다.

**파라미터**: 없음

**반환값**: [`XmLogStatus_e`](#xmlogstatus_e) 열거형 값

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 2ms 실시간 루프에서 안전하게 호출 가능합니다.

**참고**: [`XmLogStatus_e`](#xmlogstatus_e) 전체 값 목록

---

### `XM_SetUsbLogAutoTimestamp`

```c
void XM_SetUsbLogAutoTimestamp(bool enabled);
```

자동 타임스탬프(4-byte tick_ms)를 매 레코드 앞에 삽입할지 설정합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `enabled` | `bool` | `true`: 매 패킷 앞에 tick 자동 삽입 (기본값). `false`: 사용자 구조체에 이미 tick이 포함된 경우 비활성화 |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()`에서 [`XM_StartUsbDataLog`](#xm_startusbdatalog) 호출 **전에** 설정하세요.

**참고**: [`XM_SetUsbLogRollingSize`](#xm_setusblogrollingsize)

---

### `XM_SetUsbLogRollingSize`

```c
void XM_SetUsbLogRollingSize(uint32_t size_mb);
```

파일 롤링(자동 분할) 크기를 설정합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `size_mb` | `uint32_t` | 파일 분할 크기 (MB). 범위 1~100, 기본값 10 |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()`에서 [`XM_StartUsbDataLog`](#xm_startusbdatalog) 호출 **전에** 설정하세요.

**참고**: [`XM_SetUsbLogAutoTimestamp`](#xm_setusblogautotimestamp)

---

### `XM_GetUsbLogStats`

```c
bool XM_GetUsbLogStats(XmLogStats_t* out_stats);
```

로깅 중 또는 로깅 종료 후 세션의 상세 통계를 조회합니다. Hot/Cold 버퍼 피크 사용률, 디스크 잔여 용량 등 진단 정보를 포함합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out_stats` | `XmLogStats_t*` | 통계가 복사될 구조체 포인터 |

**반환값**: `bool` — 성공 시 `true`, 파라미터 오류 또는 USB 미연결 시 `false`

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 2ms 실시간 루프에서 안전하게 호출 가능합니다.

**예제**

```c
XmLogStats_t stats;
if (XM_GetUsbLogStats(&stats)) {
    printf("Records: %lu, Dropped: %lu, Disk: %lu MB\n",
           stats.total_records, stats.dropped_records, stats.disk_free_mb);
}
```

**참고**: [`XmLogStats_t`](#xmlogstats_t)

---

### `XM_GetUsbDiskFreeMB` / `XM_GetUsbDiskTotalMB`

```c
uint32_t XM_GetUsbDiskFreeMB(void);
uint32_t XM_GetUsbDiskTotalMB(void);
```

USB 디스크의 잔여/전체 용량(MB)을 반환합니다. 10초 주기로 캐시된 값을 즉시 반환하므로 매 tick 호출해도 부담이 없습니다.

**파라미터**: 없음

**반환값**: `uint32_t` — 용량 (MB). USB 미연결 시 0

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 2ms 실시간 루프에서 안전하게 호출 가능합니다 (Non-blocking, 캐시값 반환).

**참고**: [`XM_GetUsbLogStats`](#xm_getusblogstats) — `disk_free_mb`/`disk_total_mb` 필드로도 동일 값 확인 가능

---

### `XM_InsertUsbLogMarker`

```c
bool XM_InsertUsbLogMarker(XmLogMarkerType_e type, uint16_t data);
```

로깅 중 특정 시점(모드 전환, 이상 감지, 수동 마킹)을 마커로 표시합니다. 마커는 일반 데이터와 동일한 파이프라인에 저장되며, Python 디코더가 자동으로 분리해 이벤트 로그(`events.csv`)를 생성합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `type` | `XmLogMarkerType_e` | 마커 타입 |
| `data` | `uint16_t` | 컨텍스트 데이터 (에러 코드, 모드 ID 등. 불필요 시 0) |

**반환값**: `bool` — 성공 시 `true`, 로깅 비활성 또는 버퍼 부족 시 `false`

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 2ms 실시간 루프에서 안전하게 호출 가능합니다 (Non-blocking).

**예제**

```c
// 모드 전환 시
XM_InsertUsbLogMarker(XM_LOG_MARKER_MODE, newModeId);

// 에러 감지 시
XM_InsertUsbLogMarker(XM_LOG_MARKER_ERROR, errorCode);

// 수동 마킹
XM_InsertUsbLogMarker(XM_LOG_MARKER_USER, 0);
```

**참고**: [`XmLogMarkerType_e`](#xmlogmarkertype_e)

---

### `XM_IsUsbStreamConnected`

```c
bool XM_IsUsbStreamConnected(void);
```

PC가 USB 가상 시리얼 포트(CDC)에 연결되었는지 확인합니다.

**파라미터**: 없음

**반환값**: `bool` — 연결되었으면 `true`

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**참고**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

### `XM_IsUsbStreamingActive`

```c
bool XM_IsUsbStreamingActive(void);
```

스트리밍이 활성화되었는지 확인합니다. Auto-Stream 모드에서는 USB 연결 시 자동으로 `true`가 되고, Legacy 모드에서는 `"AGRB MON START"` 수신 시 `true`가 됩니다.

**파라미터**: 없음

**반환값**: `bool` — `true`: 스트리밍 활성(데이터 전송 중), `false`: 대기

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**참고**: [`XM_SetUsbAutoStream`](#xm_setusbautostream)

---

### `XM_SetUsbAutoStream`

```c
void XM_SetUsbAutoStream(bool enabled);
```

Auto-Stream 모드를 설정합니다. 기본값은 ON — USB 연결 시 자동으로 스트리밍을 시작합니다 (PhAI Studio 기본 동작).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `enabled` | `bool` | `true`: USB 연결 시 자동 스트리밍 (기본값). `false`: `"AGRB MON START"` 명령 대기 (Legacy Python 호환) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 컨텍스트 기준.

**참고**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

### `XM_USB_SetHostProfile` 🟢 Rev 2.0 전용

```c
void XM_USB_SetHostProfile(XM_USB_HostProfile_e profile);
```

> 🟢 **Rev 2.0 전용** — Rev1.1 헤더에는 이 함수와 [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e) 타입 자체가 존재하지 않습니다.

이 보드의 USB-CDC 케이블에 붙는 PC 앱의 종류를 지정합니다. **미호출 시 기본값은 `XM_USB_HOST_PHAI_STUDIO`** 이므로, 프로파일을 따로 만지지 않으면 기존 PhAI Studio 실시간 스트리밍 동작이 그대로 유지됩니다.

일반 시리얼 터미널 / 커스텀 프로그램(Tera Term · VS Code Serial Monitor · 자작 Python GUI 등)으로 **깨끗한 텍스트/커스텀 IO 만** 쓰려면 `Control_Setup()`에서 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)`을 1회 호출하세요. 1kHz Total Data auto-pump 가 꺼지고, 이 설정은 **DTR 재토글/케이블 재접속에도 유지**됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `profile` | [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e) | `XM_USB_HOST_PHAI_STUDIO`(기본) 또는 `XM_USB_HOST_TERMINAL` |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 컨텍스트 기준. 1회 호출하면 됩니다.

**예제**

```c
void Control_Setup(void) {
    // 일반 시리얼 터미널로 텍스트만 확인할 때 (Ex.07 / Ex.08 방식)
    XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL);
}
```

> 생산 검사(PRODUCTION) 동작은 보드 내부에서 첫 유효 DOP 프레임 수신 시 자동으로 진입/이탈하며, 사용자 선택지에 없습니다. `XM_USB_HOST_TERMINAL` 로 지정해두면 이 자동 진입이 비활성화되어 터미널 출력이 방해받지 않습니다.

**참고**: [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e), [05. USB 시리얼](../05-usb-connectivity.md)

---

### `XM_SendUsbData`

```c
bool XM_SendUsbData(const void* data, uint32_t len);
```

> ⚠️ **Deprecated** — [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)로 대체되었습니다. Module ID를 명시적으로 지정하세요.

USB CDC로 데이터를 PhAI 패킷(SOF + LEN + SEQ_ID + MODULE_ID + CRC16)으로 자동 래핑하여 전송합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `data` | `const void*` | 전송할 사용자 구조체 포인터 (float 배열 또는 4-byte 정렬 struct) |
| `len` | `uint32_t` | 데이터 바이트 수 |

**반환값**: `bool` — 전송 성공 시 `true`, 버퍼 풀 또는 연결 안 됨 시 `false`

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 2ms 주기 내에서 안전하게 호출 가능합니다 (Non-blocking).

**참고**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SetUsbStreamModuleId`

```c
void XM_SetUsbStreamModuleId(uint8_t module_id);
```

> ⚠️ **Deprecated** — [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)로 대체되었습니다. Module ID는 전송 시점에 직접 지정하세요.

스트리밍 데이터의 Module ID를 설정합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `module_id` | `uint8_t` | Module ID (기본값 `0x10` = COMBINED). `phai_packet_builder.h`의 `PHAI_MODULE_*` 매크로 참조 |

**반환값**: 없음 (`void`)

**참고**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SetUsbCustomMeta`

```c
void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
```

User Custom 채널의 메타데이터(채널 이름/단위 등)를 등록합니다. USB 연결 시 Module ID `0xEF`로 자동 전송되어 PhAI Studio에 채널 이름이 표시됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `module_id` | `uint8_t` | 대상 Module ID (`0xF0`~`0xFE`) |
| `json_str` | `const char*` | 채널 정의 JSON 문자열 (NULL-terminated, 문자열 리터럴 권장) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()`에서 1회 호출.

**⚠️ 포인터 수명 주의**: `json_str` 포인터는 프로그램 수명 동안 유효해야 합니다 (내부에서 복사하지 않음) — 스택 지역 변수가 아닌 문자열 리터럴을 사용하세요.

**⚠️ 단일 슬롯**: USB 연결당 하나의 Module ID 메타만 유지됩니다 (마지막 호출이 이전 것을 덮어씀). 여러 채널 그룹을 라벨링하려면 채널들을 하나의 Module ID로 모아 등록하세요.

**예제**

```c
void Control_Setup(void) {
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Target\",\"unit\":\"deg\"},"
        "{\"name\":\"Current\",\"unit\":\"deg\"}]");
}
```

**참고**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SendUsbDataWithId`

```c
bool XM_SendUsbDataWithId(const void* data, uint32_t len, uint8_t module_id);
```

User Custom `float[]` 데이터를 지정된 Module ID로 전송합니다. Total Data(0x20)는 System이 이미 자동 전송하므로, 이 함수는 사용자 알고리즘의 추가 디버그 채널을 전송할 때 사용합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `data` | `const void*` | float 배열 포인터 (4-byte aligned) |
| `len` | `uint32_t` | 바이트 수 (`sizeof(float) × 채널수`) |
| `module_id` | `uint8_t` | Module ID (`0xF0`~`0xFE`) |

**반환값**: `bool` — 전송 성공 시 `true`, 버퍼 풀 또는 연결 없음 시 `false`

**⚠️ 호출 컨텍스트**: `Control_Loop()` 내에서 호출 (Non-blocking). 매 tick 호출이 필수는 아니며, 필요 시에만 호출해도 됩니다.

**예제**

```c
float data[4] = { target, current, error, torque };
XM_SendUsbDataWithId(data, sizeof(data), 0xF0);
```

**참고**: [`XM_SetUsbCustomMeta`](#xm_setusbcustommeta)

---

### `XM_SendUsbDebugMessage`

```c
bool XM_SendUsbDebugMessage(const char* message);
```

PC로 디버그 메시지를 raw 텍스트로 전송합니다 (PhAI 패킷 래핑 없음).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `message` | `const char*` | 전송할 문자열 (Null-terminated) |

**반환값**: `bool` — 전송 성공 시 `true`

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. 비차단(Non-blocking)입니다.

**예제**

```c
char buf[64];
sprintf(buf, "Current State: %d\r\n", current_state);
XM_SendUsbDebugMessage(buf);
```

**참고**: [05. USB 시리얼](../05-usb-connectivity.md)

---

### `XM_GetUsbData`

```c
uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);
```

PC로부터 데이터를 수신합니다 (Non-Blocking).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `buffer` | `void*` | 수신 데이터를 저장할 버퍼 |
| `max_len` | `uint32_t` | 버퍼 최대 크기 |

**반환값**: `uint32_t` — 실제로 읽어온 바이트 수 (0이면 수신 데이터 없음)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. 헤더에 상세 설명(`@details`)이 없어 반환값 0 이외의 오류 상태 구분은 문서화되어 있지 않습니다 — 사용 전 실측 확인을 권장합니다.

---

## 타입/매크로

### `XmLogStatus_e`

로거의 현재 상태를 나타내는 열거형입니다.

| 값 | 설명 |
|----|------|
| `XM_LOG_STATUS_IDLE` | 중지됨 (초기 상태) |
| `XM_LOG_STATUS_LOGGING` | 정상 로깅 중 |
| `XM_LOG_STATUS_WARNING_QUEUE_FULL` | 버퍼 사용률 높음 (`f_write` 지연 발생 중) |
| `XM_LOG_STATUS_WARNING_DISK_LOW` | USB 디스크 잔여 용량 50MB 미만 |
| `XM_LOG_STATUS_ERROR_STOPPED` | 에러로 로깅이 강제 중지됨 |

### `XmLogStats_t`

로깅 세션의 실시간 통계 구조체입니다. [`XM_GetUsbLogStats()`](#xm_getusblogstats)로 조회합니다.

| 필드 | 타입 | 설명 |
|------|------|------|
| `total_bytes` | `uint32_t` | 총 기록 바이트 수 |
| `total_records` | `uint32_t` | 총 레코드 수 |
| `dropped_records` | `uint32_t` | 누락된 레코드 수 (버퍼 오버플로) |
| `write_errors` | `uint32_t` | 쓰기 실패 횟수 |
| `duration_ms` | `uint32_t` | 세션 경과 시간 (ms) |
| `hot_buffer_percent` | `uint8_t` | Hot Buffer 피크 사용률 (0~100) |
| `cold_buffer_percent` 🟢 | `uint8_t` | Cold Buffer 피크 사용률 (0~100) — 레거시 필드, [현재 Hot-buffer 전용 구조에서는 항상 0](../06-usb-data-logging.md) |
| `disk_free_mb` | `uint32_t` | USB 잔여 용량 (MB) |
| `disk_total_mb` | `uint32_t` | USB 전체 용량 (MB) |

> 🟢 **Rev 2.0 전용 필드**: `cold_buffer_percent`는 Rev1.1 헤더의 `XmLogStats_t`에는 존재하지 않습니다 (Rev1.1은 8개 필드, Rev2.0은 9개 필드). Rev1.1/Rev2.0 양쪽을 함께 다루는 코드에서 이 구조체를 그대로 캐스팅하거나 raw-copy 하지 마세요.

### `XmLogMarkerType_e`

로깅 중 이벤트 마커의 종류를 나타내는 열거형입니다.

| 값 | 설명 |
|----|------|
| `XM_LOG_MARKER_USER` = 0x01 | 수동 마킹 (버튼/명령) |
| `XM_LOG_MARKER_MODE` = 0x02 | 모드 전환 |
| `XM_LOG_MARKER_ERROR` = 0x03 | 에러 발생 |
| `XM_LOG_MARKER_SYNC` = 0x04 | 시간 동기점 |

### `XM_USB_HostProfile_e` 🟢 Rev 2.0 전용

> Rev1.1 헤더에는 정의되어 있지 않습니다.

한 보드/한 케이블을 어떤 PC 앱 용도로 쓸지 사용자가 지정하는 USB-CDC 호스트 프로파일입니다. 선택지는 2가지이며, [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile)로 지정합니다.

| 값 | 설명 |
|----|------|
| `XM_USB_HOST_PHAI_STUDIO` = 0 | **기본값** — PhAI Studio 실시간 스트리밍 (Total Data auto-pump ON) |
| `XM_USB_HOST_TERMINAL` = 1 | 일반 터미널 / 커스텀 프로그램 — auto-pump OFF, 사용자 명시 TX 만 |

> 생산 검사(PRODUCTION) 동작은 사용자 선택지가 아니라 **보드 내부에서 자동 처리**됩니다: 첫 유효 DOP 프레임 수신 시 진입, DTR=0(케이블 분리) 시 이탈. 단, 프로파일을 `XM_USB_HOST_TERMINAL` 로 지정해두면 이 자동 진입이 비활성화됩니다(터미널 출력 보호). 프로파일 지정은 DTR 재토글/재접속에도 유지됩니다.

---

## 내부 전용 (호출 금지)

아래 함수는 시스템 내부(core_process)가 호출하는 전용 함수입니다. 사용자 코드에서 직접 호출할 필요가 없으며, 호출해도 의도한 동작을 보장하지 않습니다.

| 함수 | 실제 호출 주체 | 설명 |
|------|--------------|------|
| `void XM_USB_ProcessPeriodic(void)` | `core_process` | USB 로깅·스트리밍 로직의 주기 처리 엔진. `core_process`가 자동 호출하므로 사용자가 직접 호출할 필요 없음 |

> 🟢 **Rev 2.0 참고** — 이전(v2.3.1)에 이 자리에 있던 PRODUCTION 자동 latch / DTR 처리(구 `XM_USB_RequestProductionLatch` / `XM_USB_OnDtrLost`)는 v2.4.0 에서 System 레이어(`usb_host_mode`) 내부로 이관되어 공개 헤더에서 빠졌습니다. 사용자 공개 API 는 [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile) 하나입니다.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| MSC 로깅 기본 API (Start/Stop/Status/Stats/Marker/디스크 조회) | ✅ | ✅ |
| CDC 스트리밍 기본 API (`SendUsbDataWithId`/`SetUsbCustomMeta`/`SendUsbDebugMessage`) | ✅ | ✅ |
| `XmLogStats_t.cold_buffer_percent` 필드 | ❌ 없음 (8개 필드) | 🟢 있음 (9개 필드 — 레거시, 현재 항상 0) |
| `XM_USB_HostProfile_e` / `XM_USB_SetHostProfile` (PhAI Studio / 터미널 호스트 프로파일 지정) | ❌ 없음 | 🟢 전용 |
| PRODUCTION 자동 진입 / DTR 처리 (System `usb_host_mode` 내부, 공개 API 아님) | ❌ 없음 | 🟢 전용 (Internal) |
| MSC 로깅 내부 파이프라인 단계 | 2단계 — UserTask가 Lock-Free SPSC 링 버퍼에 직접 기록 → `DataLoggerTask`가 `f_write()` | 3단계 — UserTask가 1차 큐 적재 → `DataLoggerTask`가 Binary 변환 후 2차 큐 적재 → 저순위 태스크가 `f_write()` |

---

## 관련 문서

- [05. USB 시리얼 (개념)](../05-usb-connectivity.md) — CDC 동작 원리, 흔한 실수, 예제 매핑
- [06. USB 메모리 로깅 (개념)](../06-usb-data-logging.md) — MSC 파일 포맷, Python 디코더, 세션 출력 구조
