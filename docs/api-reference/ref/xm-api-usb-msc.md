# `xm_api_usb_msc.h` — USB 메모리 저장

> 🟢 **Rev 2.0 전용** — 본 헤더 전체가 XM10 Rev 2.0 SDK에만 포함되어 있습니다. Rev 1.1 SDK (`XM10_SDK/Rev1.1/`) 에는 `xm_api_usb_msc.h` 자체가 존재하지 않으므로, 아래 함수를 Rev 1.1 프로젝트에서 호출하면 **빌드에 실패**합니다. 보드 리비전 확인은 [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교) 참고.
>
> - **대상 헤더**: `XM_FW/XM_API/xm_api_usb_msc.h` (`xm_api.h` 를 include 하면 함께 쓸 수 있습니다)
> - **관련 개념 문서**: [06. USB 메모리 저장](../06-usb-data-logging.md)
> - **따라 하기**: [USB 메모리에 저장하기](../../getting-started/05-usb-memory-log.md)
> - **관련 예제**: [10_USB_Memory_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/) · [34_USB_Memory_Gait_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/)

이 헤더는 **구조체 하나를 USB 메모리에 주기적으로 저장**하는 API 를 정의합니다. 구조체를 등록해 두면 저장하는 동안 직접 정한 주기마다 시스템이 그 구조체를 USB 메모리에 기록합니다. 기록 함수를 따로 부르지 않습니다.

---

## 언제 사용하나

PC 를 연결하지 않고 값을 USB 메모리에 저장해 두었다가, 나중에 PC 의 XM10 Log Decoder 로 풀고 싶을 때 사용합니다. 쓰는 순서 · USB 메모리 준비 · USB-C 포트 규칙 같은 전체 그림은 [06. USB 메모리 저장](../06-usb-data-logging.md) 을 먼저 읽어 보는 것을 권장합니다 — 이 페이지는 헤더에 선언된 **모든 함수 · 타입 · 상수의 시그니처와 파라미터 상세**만 다룹니다.

> ⚠️ **USB-C 포트는 하나입니다.** USB 메모리를 꽂아 두는 동안에는 PhAI Studio · `xm10` 도구 · USB 로 하는 펌웨어 업로드를 쓸 수 없습니다. 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요. (보드 확인 전) <!-- board-check -->

---

## 호출 규칙

| 종류 | 함수 | 부르는 곳 |
|------|------|-----------|
| 설정 · 시작 · 정지 · 마커 · 통계 (8개) | `XM_SetUsbLogSource` · `XM_SetUsbLogSampleRateHz` · `XM_SetUsbLogAutoTimestamp` · `XM_SetUsbLogRollingSize` · `XM_StartUsbDataLog` · `XM_StopUsbDataLog` · `XM_InsertUsbLogMarker` · `XM_GetUsbLogStats` | `Control_Setup()` · `Control_Loop()` 에서만. 백그라운드 태스크(`xm_api_freertos.h`)나 인터럽트에서 부르면 아무것도 하지 않고 `false` 를 돌려줍니다 |
| 상태 (6개) | `XM_IsUsbLogReady` · `XM_IsUsbLogSessionIdle` · `XM_IsUsbLogRecording` · `XM_GetUsbLogLastStartError` · `XM_GetUsbDiskFreeMB` · `XM_GetUsbDiskTotalMB` | 어디서나 |

---

## 함수 목록

**설정** (`Control_Setup()`)

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SetUsbLogSource`](#xm_setusblogsource) | 저장할 구조체 등록 |
| [`XM_SetUsbLogSampleRateHz`](#xm_setusblogsampleratehz) | 저장 주기 설정 (기본 1000 Hz) |
| [`XM_SetUsbLogAutoTimestamp`](#xm_setusblogautotimestamp) | 레코드마다 시각을 함께 저장할지 설정 (기본 켜짐) |
| [`XM_SetUsbLogRollingSize`](#xm_setusblogrollingsize) | 파일을 나누는 크기 설정 (기본 10 MB) |

**상태** (어디서나)

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_IsUsbLogReady`](#xm_isusblogready) | USB 메모리가 꽂혀 있고 쓸 준비가 되었는지 |
| [`XM_IsUsbLogSessionIdle`](#xm_isusblogsessionidle) | 저장 세션이 하나도 없는지 |
| [`XM_IsUsbLogRecording`](#xm_isusblogrecording) | 지금 파일에 기록하고 있는지 |
| [`XM_GetUsbLogLastStartError`](#xm_getusbloglaststarterror) | 마지막 시작의 결과 |
| [`XM_GetUsbDiskFreeMB`](#xm_getusbdiskfreemb) | USB 메모리의 빈 공간 [MB] |
| [`XM_GetUsbDiskTotalMB`](#xm_getusbdisktotalmb) | USB 메모리의 전체 크기 [MB] |

**제어** (`Control_Loop()`)

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_StartUsbDataLog`](#xm_startusbdatalog) | 저장 시작 |
| [`XM_StopUsbDataLog`](#xm_stopusbdatalog) | 저장 정지 |
| [`XM_InsertUsbLogMarker`](#xm_insertusblogmarker) | 저장 중에 표시(마커) 넣기 |
| [`XM_GetUsbLogStats`](#xm_getusblogstats) | 저장 통계 읽기 |

**타입 · 상수**: [`XmUsbLogStartError_e`](#xmusblogstarterror_e) · [`XmUsbLogStats_t`](#xmusblogstats_t) · [상수 3개](#상수)

---

## 저장 세션의 상태

저장 세션의 상태는 `XM_IsUsbLogRecording()` 과 `XM_IsUsbLogSessionIdle()` 의 조합으로 읽습니다.

| 상태 | `XM_IsUsbLogRecording()` | `XM_IsUsbLogSessionIdle()` | 설명 |
|------|:---:|:---:|------|
| 세션 없음 | `false` | `true` | 새로 시작하거나 설정을 바꿀 수 있습니다 |
| 시작 처리 중 | `false` | `false` | `XM_StartUsbDataLog()` 가 `true` 를 돌려준 뒤 파일이 열리기 전입니다. 이때 `XM_GetUsbLogStats()` 는 전부 0 을 돌려줍니다 |
| 기록 중 | `true` | `false` | |
| 마무리 중 | `false` | `false` | 정지했거나 저장이 스스로 끝난 뒤, 남은 데이터를 쓰고 파일을 닫는 중입니다 |

---

## 함수 상세

### 설정 (`Control_Setup()`)

#### `XM_SetUsbLogSource`

```c
bool XM_SetUsbLogSource(const void* data, uint32_t size);
```

저장할 구조체를 등록합니다. 구조체를 복사하지 않고 주소를 그대로 쓰므로, 프로그램이 도는 동안 계속 유효해야 합니다. 저장 세션이 없을 때는 다시 불러서 등록을 바꿀 수 있습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `data` | `const void*` | 구조체 주소 — 전역 또는 `static` 변수 |
| `size` | `uint32_t` | 구조체 크기 [B], 1 ~ `XM_USB_LOG_MAX_STRUCT_SIZE` (512) |

**반환값**: `bool` — `true`: 등록했습니다. `false`: 저장 세션이 있거나 인자가 잘못됨 — 이전 등록은 그대로입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서 한 번 부릅니다.

**예제**

```c
static MyLog_t s_log;     /* packed 구조체 */

void Control_Setup(void) {
    XM_SetUsbLogSource(&s_log, sizeof(s_log));
}
```

**참고**: [`XM_StartUsbDataLog`](#xm_startusbdatalog), [메타 줄](#메타-줄)

---

#### `XM_SetUsbLogSampleRateHz`

```c
bool XM_SetUsbLogSampleRateHz(uint32_t hz);
```

저장 주기를 정합니다. 정하지 않으면 1000 Hz 입니다. 1000 Hz 미만은 주기마다 한 번만 저장하고, 그 사이 값은 저장하지 않습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `hz` | `uint32_t` | 1000 의 약수: 1000 · 500 · 250 · 200 · 125 · 100 · 50 · 40 · 25 · 20 · 10 · 8 · 5 · 4 · 2 · 1 |

**반환값**: `bool` — `true`: 다음 시작부터 적용합니다. `false`: 저장 세션이 있거나 1000 의 약수가 아님(0 포함) — 이전 값은 그대로입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서 한 번 부릅니다.

**참고**: 주기에 비해 구조체가 크면 시작하지 않습니다(`XM_USB_LOG_START_TOO_BIG_FOR_RATE`). 타임스탬프를 켠 경우 1000 Hz 는 구조체 134 B 까지, 200 Hz 이하는 512 B 까지 시작됩니다. 1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다.

---

#### `XM_SetUsbLogAutoTimestamp`

```c
bool XM_SetUsbLogAutoTimestamp(bool enable);
```

레코드마다 시각(ms)을 함께 저장할지 정합니다. 기본은 켜짐입니다. 시계(RTC)의 날짜와 시각이 아니라 시스템이 세는 시간입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `enable` | `bool` | `true`: 시각을 함께 저장 (기본). `false`: 저장하지 않음 |

**반환값**: `bool` — `true`: 다음 시작부터 적용합니다. `false`: 저장 세션이 있음 — 이전 값은 그대로입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서 부릅니다.

**참고**: 1000 Hz 미만으로 저장할 때는 켜 두어야 합니다. 끄면 시작할 때 `XM_USB_LOG_START_NEEDS_TIMESTAMP` 로 거부됩니다.

---

#### `XM_SetUsbLogRollingSize`

```c
bool XM_SetUsbLogRollingSize(uint32_t size_mb);
```

파일을 나누는 크기를 정합니다. 기본 10 MB 입니다. 파일이 이 크기를 넘으면 새 파트(`data_NNN_part_PPP.bin`)로 나뉩니다. (보드 확인 전) <!-- board-check -->

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `size_mb` | `uint32_t` | 1 ~ 100 [MB]. USB 메모리의 빈 공간이 이 값보다 커야 시작됩니다 |

**반환값**: `bool` — `true`: 다음 시작부터 적용합니다. `false`: 저장 세션이 있거나 범위 밖 — 이전 값은 그대로입니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서 부릅니다.

---

### 상태 (어디서나)

#### `XM_IsUsbLogReady`

```c
bool XM_IsUsbLogReady(void);
```

USB 메모리가 꽂혀 있고 파일을 쓸 준비가 되었는지 돌려줍니다(FAT32). exFAT · NTFS 는 인식하지 않아 계속 `false` 입니다. 꽂은 직후에는 잠깐 `false` 일 수 있습니다.

**파라미터**: 없음

**반환값**: `bool` — `true`: 꽂혀 있고 파일을 쓸 수 있습니다.

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

**참고**: [`XM_StartUsbDataLog`](#xm_startusbdatalog)

---

#### `XM_IsUsbLogSessionIdle`

```c
bool XM_IsUsbLogSessionIdle(void);
```

저장 세션이 하나도 없는지 돌려줍니다 — 저장 중도, 시작하는 중도, 파일을 닫는 중도 아닙니다. `true` 일 때 새로 시작하거나 설정을 바꿀 수 있고, USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check -->

저장 중에 이 값이 스스로 `true` 가 되면 세션이 끝난 것입니다(USB 메모리가 빠졌거나 내부 버퍼가 넘친 경우 등).

**파라미터**: 없음

**반환값**: `bool` — `true`: 저장 세션이 없습니다. `false`: 저장 중이거나 시작하는 중이거나 파일을 닫는 중입니다.

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다. `Control_Setup()` · `Control_Loop()` 밖에서 읽은 값은 참고용입니다 — 시작이나 설정 변경은 이 값으로 판단하지 않습니다.

**참고**: [저장 세션의 상태](#저장-세션의-상태), [`XM_StopUsbDataLog`](#xm_stopusbdatalog)

---

#### `XM_IsUsbLogRecording`

```c
bool XM_IsUsbLogRecording(void);
```

지금 파일에 기록하고 있는지 돌려줍니다. 시작을 요청한 뒤 파일이 열리면 `true` 가 되고, `XM_StopUsbDataLog()` 를 부르면 바로 `false` 가 됩니다(파일은 아직 닫는 중일 수 있습니다).

**파라미터**: 없음

**반환값**: `bool` — `true`: 파일에 기록 중입니다.

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

**참고**: 파일이 모두 닫혔는지는 이 값이 아니라 [`XM_IsUsbLogSessionIdle`](#xm_isusblogsessionidle) 로 확인하세요.

---

#### `XM_GetUsbLogLastStartError`

```c
XmUsbLogStartError_e XM_GetUsbLogLastStartError(void);
```

마지막 `XM_StartUsbDataLog()` 의 결과입니다. 시작 요청이 바로 거부된 이유, 또는 받아들여진 뒤 파일을 여는 중에 실패한 이유가 남습니다. 시작 요청을 받아들이면 `XM_USB_LOG_START_OK` 로 지워집니다.

**파라미터**: 없음

**반환값**: [`XmUsbLogStartError_e`](#xmusblogstarterror_e)

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다. 받아들여진 시작의 결과는 `XM_IsUsbLogRecording()` 이 `true` 가 되거나 `XM_IsUsbLogSessionIdle()` 이 다시 `true` 가 된 뒤, `XM_StartUsbDataLog()` 를 다시 부르기 전에 읽습니다.

**예제**

```c
if (!XM_StartUsbDataLog("walk01", LOG_METADATA)) {
    XmUsbLogStartError_e why = XM_GetUsbLogLastStartError();   /* 디버거에서 값을 보세요 */
    (void)why;
}
```

---

#### `XM_GetUsbDiskFreeMB`

```c
uint32_t XM_GetUsbDiskFreeMB(void);
```

USB 메모리의 빈 공간 [MB] 을 돌려줍니다. 약 10 초마다 갱신하며, 꽂은 직후에는 잠깐 0 입니다. USB 메모리가 없으면 0 입니다.

**파라미터**: 없음

**반환값**: `uint32_t` — 빈 공간 [MB]

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

---

#### `XM_GetUsbDiskTotalMB`

```c
uint32_t XM_GetUsbDiskTotalMB(void);
```

USB 메모리의 전체 크기 [MB] 를 돌려줍니다. 꽂은 직후에는 잠깐 0 이고, USB 메모리가 없으면 0 입니다.

**파라미터**: 없음

**반환값**: `uint32_t` — 전체 크기 [MB]

**⚠️ 호출 컨텍스트**: 어디서나 부를 수 있습니다.

---

### 제어 (`Control_Loop()`)

#### `XM_StartUsbDataLog`

```c
bool XM_StartUsbDataLog(const char* session_name, const char* metadata);
```

저장을 시작합니다. 기다리지 않고 바로 돌아옵니다. 두 문자열은 시작할 때 복사합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `session_name` | `const char*` | 세션 폴더 이름(`XM_USB_LOG_MAX_SESSION_NAME` 자 이하). `NULL` 이나 `""` 이면 시계로 `YYMMDD_HHMMSS` 를 만듭니다. 같은 이름의 폴더가 있으면 거기에 이어서 저장합니다 — 주기 · 구조체 크기 · 타임스탬프 설정이 같아야 합니다. 폴더의 메타 줄은 처음 저장할 때의 것을 그대로 쓰고 새 메타 줄과 비교하지 않으니, 필드 이름이나 타입을 바꿨다면 다른 이름을 쓰세요 |
| `metadata` | `const char*` | [메타 줄](#메타-줄) (`NULL` 이면 `XM_USB_LOG_START_BAD_ARG`) |

세션 이름에 쓸 수 없는 문자: `<` `>` `:` `"` `/` `\` `?` `*` 와 세로줄 문자.

**반환값**: `bool`

- `true`: 시작 요청을 받아들였습니다. 파일이 열리면 `XM_IsUsbLogRecording()` 이 `true` 가 됩니다. 파일을 열다가 실패하면 `XM_IsUsbLogSessionIdle()` 이 다시 `true` 가 되고 이유가 `XM_GetUsbLogLastStartError()` 에 남습니다.
- `false`: 시작하지 않았습니다 — 이유는 `XM_GetUsbLogLastStartError()` 로 읽습니다. (`Control_Setup()` · `Control_Loop()` 밖에서 부르면 이유를 남기지 않고 `false` 입니다.)

**⚠️ 호출 컨텍스트**: `Control_Loop()` 에서 부릅니다. `XM_IsUsbLogReady()` 와 `XM_IsUsbLogSessionIdle()` 이 모두 `true` 일 때 부르세요.

**예제**

```c
if (XM_IsUsbLogReady() && XM_IsUsbLogSessionIdle()) {
    XM_StartUsbDataLog("", LOG_METADATA);   /* "" 이면 시계로 폴더 이름을 만듭니다 */
}
```

**참고**: [`XmUsbLogStartError_e`](#xmusblogstarterror_e), [`XM_StopUsbDataLog`](#xm_stopusbdatalog)

---

#### `XM_StopUsbDataLog`

```c
bool XM_StopUsbDataLog(void);
```

저장을 멈춥니다. 기다리지 않고 바로 돌아오며, 파일은 뒤에서 닫습니다.

**파라미터**: 없음

**반환값**: `bool` — `true`: 멈춤을 요청했거나 멈출 세션이 없습니다. `false`: 잠깐 바쁨 — 조금 뒤 다시 부릅니다.

**⚠️ 호출 컨텍스트**: `Control_Loop()` 에서 부릅니다. `XM_IsUsbLogSessionIdle()` 이 `true` 가 될 때까지 기다리는 동안 다시 불러도 됩니다.

- 시작을 요청한 직후, 시스템이 그 시작을 처리하기 전에 부르면 시작을 취소합니다(`XM_USB_LOG_START_CANCELED`, 파일을 만들지 않음).
- 이미 파일을 만드는 중이었다면 만든 뒤 곧바로 닫으므로 기록이 없는 세션 폴더가 남습니다(이때 이유는 `XM_USB_LOG_START_OK`).

**참고**: [`XM_IsUsbLogSessionIdle`](#xm_isusblogsessionidle)

---

#### `XM_InsertUsbLogMarker`

```c
bool XM_InsertUsbLogMarker(uint8_t type, uint16_t value);
```

저장 중에 표시(마커)를 하나 넣습니다. 디코더가 표시를 `events.csv` 로 따로 모아 보여 줍니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `type` | `uint8_t` | 표시 종류 1 ~ 254 — 뜻은 사용자가 정합니다(예: 1 = 실험 시작, 2 = 실험 끝) |
| `value` | `uint16_t` | 함께 남길 값 |

**반환값**: `bool` — `true`: 넣었습니다. `false`: 저장 중이 아니거나 `type` 이 1 ~ 254 밖이거나 버퍼에 자리가 없습니다(저장은 멈추지 않습니다).

**⚠️ 호출 컨텍스트**: `Control_Loop()` 에서 부릅니다.

---

#### `XM_GetUsbLogStats`

```c
bool XM_GetUsbLogStats(XmUsbLogStats_t* out);
```

저장 통계를 복사합니다. 저장 중에는 지금까지의 값, 끝나면 그 저장의 마지막 값을 돌려줍니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | [`XmUsbLogStats_t*`](#xmusblogstats_t) | 통계를 받을 구조체 |

**반환값**: `bool` — `true`: 복사했습니다. `false`: `out` 이 `NULL` 이거나 `Control_Setup()` · `Control_Loop()` 밖에서 불렀습니다.

**⚠️ 호출 컨텍스트**: `Control_Loop()` 에서 부릅니다. 시작을 요청한 뒤 파일이 열릴 때까지는 전부 0 을 돌려줍니다.

**예제**

```c
XmUsbLogStats_t st;
if (XM_GetUsbLogStats(&st)) {
    /* st.empty_slots 가 0 이면 빠진 표본이 없습니다 */
}
```

---

## 타입 / 상수

### `XmUsbLogStartError_e`

`XM_StartUsbDataLog()` 가 시작하지 못한 이유입니다. [`XM_GetUsbLogLastStartError()`](#xm_getusbloglaststarterror) 로 읽습니다.

| 값 | 이름 | 뜻 |
|----|------|------|
| 0 | `XM_USB_LOG_START_OK` | 실패 기록 없음 |
| 1 | `XM_USB_LOG_START_NOT_READY` | USB 메모리가 없거나(시작을 기다리는 중에 빠진 경우 포함) 아직 준비 중이거나 FAT32 가 아님 — `XM_IsUsbLogReady()` 확인 |
| 2 | `XM_USB_LOG_START_BUSY` | 이미 저장 중이거나 시작하는 중 — `XM_IsUsbLogSessionIdle()` 이 `true` 일 때 시작 |
| 3 | `XM_USB_LOG_START_TRY_AGAIN` | 잠깐 바쁨 — 조금 뒤 다시 시작 |
| 4 | `XM_USB_LOG_START_NO_SOURCE` | 저장할 구조체가 없음 — `XM_SetUsbLogSource()` 를 먼저 |
| 5 | `XM_USB_LOG_START_BAD_SIZE` | 구조체 크기가 1 ~ `XM_USB_LOG_MAX_STRUCT_SIZE` 밖이거나 등록한 크기와 다름 |
| 6 | `XM_USB_LOG_START_NEEDS_TIMESTAMP` | 1000 Hz 미만은 타임스탬프가 필요 — `XM_SetUsbLogAutoTimestamp(true)` |
| 7 | `XM_USB_LOG_START_TOO_BIG_FOR_RATE` | 이 주기에 구조체가 너무 큼 — 주기를 낮추거나 구조체를 줄임 |
| 8 | `XM_USB_LOG_START_METADATA_TOO_LONG` | 메타 줄이 `XM_USB_LOG_MAX_METADATA_LEN` 보다 김 |
| 9 | `XM_USB_LOG_START_BAD_NAME` | 세션 이름이 너무 길거나 쓸 수 없는 문자가 있음, 또는 이름이 비었는데 시계를 읽지 못함 |
| 10 | `XM_USB_LOG_START_CANCELED` | 시작을 처리하기 전에 `XM_StopUsbDataLog()` 를 불러 시작을 취소함(그동안 USB 메모리가 빠졌으면 `NOT_READY`) |
| 11 | `XM_USB_LOG_START_FOLDER_MISMATCH` | 같은 이름의 폴더가 다른 주기 · 구조체 크기로 이미 있거나 그 폴더의 `metadata.txt` 를 읽어 확인할 수 없음 — 다른 이름을 쓰거나 PC 에서 그 폴더를 지움 |
| 12 | `XM_USB_LOG_START_STORAGE` | 빈 공간이 부족하거나 폴더 · 파일을 만들거나 열지 못함 |
| 13 | `XM_USB_LOG_START_BAD_ARG` | 인자가 잘못됨(메타 줄이 `NULL` 등) |
| 14 | `XM_USB_LOG_START_METADATA_MISMATCH` | 메타 줄을 읽을 수 없음 — 형식 오류 · 모르는 타입 · 필드 크기의 합이 구조체 크기와 다름 |

대부분의 이유는 `XM_StartUsbDataLog()` 가 `false` 를 돌려줄 때 바로 남습니다. `XM_USB_LOG_START_STORAGE` · `XM_USB_LOG_START_FOLDER_MISMATCH` · `XM_USB_LOG_START_CANCELED` 는 시작 요청을 받아들인 뒤 파일을 여는 중에 남습니다. `XM_USB_LOG_START_NOT_READY`(시작을 기다리는 중에 USB 메모리가 빠진 경우)와 `XM_USB_LOG_START_BAD_NAME`(이름이 비었는데 시계를 읽지 못한 경우)도 이때 남을 수 있습니다.

---

### `XmUsbLogStats_t`

저장 통계입니다. [`XM_GetUsbLogStats()`](#xm_getusblogstats) 로 읽습니다.

```c
typedef struct {
    uint32_t records;
    uint32_t bytes;
    uint32_t duration_ms;
    uint32_t empty_slots;
    uint32_t longest_gap_ms;
    uint32_t overflow_records;
    uint32_t write_errors;
    uint8_t  buffer_peak_percent;
} XmUsbLogStats_t;
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `records` | `uint32_t` | 저장하려고 받은 레코드 수(마커 포함) |
| `bytes` | `uint32_t` | USB 메모리 파일에 쓴 레코드 바이트 수 |
| `duration_ms` | `uint32_t` | 세션 길이 [ms] |
| `empty_slots` | `uint32_t` | 빠진 표본 수 — 저장 주기 칸 가운데 비어 있는 칸의 수 |
| `longest_gap_ms` | `uint32_t` | 가장 긴 공백 [ms] — 연달아 빠진 표본 수 × 주기 |
| `overflow_records` | `uint32_t` | 내부 버퍼가 넘쳐 저장하지 못한 레코드 수 — 1 이상이면 그 세션은 거기서 멈춥니다(버퍼가 가득 차기 직전에 멈출 때는 0 일 수 있으니 `buffer_peak_percent` 가 100 에 가까운지도 보세요) |
| `write_errors` | `uint32_t` | USB 메모리 쓰기 실패 횟수 |
| `buffer_peak_percent` | `uint8_t` | 내부 버퍼를 가장 많이 쓴 비율 [%] |

- 저장 중에는 지금까지의 값, 세션이 끝나면 그 세션의 마지막 값입니다.
- 시작을 요청한 뒤 파일이 열릴 때까지는 모두 0 이고, 새 세션이 시작되면 0 부터 다시 셉니다.
- 시작하지 못하면(파일을 열다 실패 · 취소 · USB 메모리가 빠짐) 앞 세션의 값이 그대로 남습니다.
- `records` 는 저장하려고 받은 레코드 수이고 `bytes` 는 그중 USB 메모리 파일에 쓴 레코드 바이트 수입니다(파일 머리 · 꼬리 · 오류 검사 값 · `metadata.txt` · `summary.txt` 는 세지 않습니다). 저장 중에는 `records` 가 `bytes` 보다 앞서고, USB 메모리가 빠지거나 버퍼가 넘치면 마지막 일부 레코드는 USB 메모리에 없으므로 `records` 가 실제로 저장된 수보다 클 수 있습니다.

---

### 상수

| 이름 | 값 | 설명 |
|------|----|------|
| `XM_USB_LOG_MAX_STRUCT_SIZE` | `512U` | 등록할 수 있는 구조체의 최대 크기 [B] |
| `XM_USB_LOG_MAX_METADATA_LEN` | `1700U` | 메타 줄의 최대 길이 [글자, 끝의 `'\0'` 제외] |
| `XM_USB_LOG_MAX_SESSION_NAME` | `100U` | 세션 이름의 최대 길이 [글자] |

> `xm_api_usb.h` 의 `XM_USB_CUSTOM_META_MAX_LEN` 은 USB-CDC 채널 이름(JSON)의 한도입니다. 이 메타 줄의 한도와는 별개이니 섞어 쓰지 마세요.

---

## 메타 줄

`XM_StartUsbDataLog()` 의 `metadata` 는 구조체의 필드를 선언 순서대로 `이름(타입)` 으로 적고 쉼표로 이은 한 줄입니다. 디코더는 이 줄로 데이터를 풉니다.

```c
"loop_count(uint32_t), hip_L(float), hip_R(float), contact(uint8_t), _pad(3bytes)"
```

| 규칙 | 내용 |
|------|------|
| 타입 | `uint8_t` · `int8_t` · `uint16_t` · `int16_t` · `uint32_t` · `int32_t` · `float` · `double` · `bool` — 소문자 그대로 씁니다(대소문자를 구분합니다) |
| 채움 | 비워 둔 바이트는 `_이름(Nbytes)` 로 적습니다. 예: `_pad(3bytes)`. 이름이 `_` 로 시작해야 채움이고, `pad(3bytes)` 는 모르는 타입이라 시작하지 않습니다 |
| 이름 | 영문 · 숫자 · `_` 만 씁니다 |
| 모양 | `이름(타입)` 안에는 공백을 넣지 않고, 줄은 한 줄로 적습니다 |
| 크기 | 필드 크기의 합이 등록한 구조체 크기와 같아야 합니다. 다르면 시작하지 않습니다(`XM_USB_LOG_START_METADATA_MISMATCH`) |

- 시스템은 크기의 합만 확인합니다. 이름 · 타입 · 순서가 구조체와 같은지는 확인하지 않으니 직접 맞추세요.
- 구조체는 `__attribute__((packed))` 로 선언하고 `_Static_assert` 로 크기를 고정해 두면 실수를 막을 수 있습니다.
- 디코더가 따로 쓰는 이름(`tick_ms` · `t_grid_ms` · `row_origin` · `count` · `h10_loop_count`)은 그 뜻이 아니면 필드 이름으로 쓰지 마세요.

---

## v2.4.1 이하에서 코드를 옮길 때

v2.5.0 이전에 쓰던 USB 메모리 저장 코드는 함수 이름이 같아도 반환 형식과 인자가 달라서 그대로 빌드되지 않을 수 있습니다.

| v2.4.1 이하 | 지금 |
|------|------|
| `XM_SetUsbLogSource(void*, uint32_t)` — 반환값 없음 | `bool` 을 돌려주고 첫 인자는 `const void*` |
| `XM_StopUsbDataLog()` · `XM_SetUsbLogAutoTimestamp()` · `XM_SetUsbLogRollingSize()` — 반환값 없음 | 모두 `bool` 을 돌려줍니다 |
| `XM_InsertUsbLogMarker(XM_LOG_MARKER_USER, value)` 처럼 열거 상수를 씀 | 첫 인자는 1 ~ 254 의 숫자이고 뜻은 사용자가 정합니다. `XM_LOG_MARKER_*` 상수는 없습니다 |
| `XM_GetUsbLogStats()` 가 `XmLogStats_t` 를 채움 | `XmUsbLogStats_t` 를 채웁니다. 필드가 다릅니다 |
| — | 새 함수: `XM_SetUsbLogSampleRateHz()` (기본 1000 Hz) · `XM_IsUsbLogSessionIdle()` · `XM_IsUsbLogRecording()` · `XM_GetUsbLogLastStartError()` |
| — | 새로 생긴 검사: 시작할 때 메타 줄을 검사하고, 맞지 않으면 시작하지 않습니다(`XM_USB_LOG_START_METADATA_MISMATCH`) |

- 빌드 오류가 나면 이 페이지의 함수 목록과 비교해서 고치세요.
- 예제 10 · 34 는 이전 버전에서도 USB 메모리 예제 번호였지만, 지금 예제는 새로 만든 것입니다. 이전 예제는 이번 릴리스에 들어 있지 않습니다.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| USB 메모리 저장 API (`xm_api_usb_msc.h`) | ❌ 없음 | 🟢 전용 |

---

## 관련 문서

- [06. USB 메모리 저장 (개념)](../06-usb-data-logging.md) — 쓰는 순서, USB 메모리 준비, USB-C 포트 규칙, 흔한 실수
- [USB 메모리에 저장하기](../../getting-started/05-usb-memory-log.md) — 포맷 · 예제 · 디코더까지 단계별로
- [`xm_api_usb.h` — USB-CDC 실시간 스트리밍](xm-api-usb.md) — PC 에 실시간으로 보내는 API
- [`xm_api_rtc.h` — RTC](xm-api-rtc.md) — 세션 폴더 이름에 쓰는 시계
