# `xm_api_usb.h` — USB-CDC 실시간 스트리밍

> **대상 헤더**: `XM_FW/XM_API/xm_api_usb.h`
> **관련 개념 문서**: [05. USB 시리얼](../05-usb-connectivity.md)
> **관련 예제**: [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) · [07_CDC_Basic_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) · [08_CDC_Sensor_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/08_CDC_Sensor_Print/) · [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) · [18_Debug_Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/)

이 헤더는 **PC 실시간 통신(CDC)** 도메인을 정의합니다. PhAI Studio·PuTTY 같은 PC 클라이언트와 텍스트/바이너리를 주고받으며, System 이 Total Data(0x20)를 자동 전송하고 사용자는 `XM_SendUsbDataWithId()` 로 추가 채널을 보내는 방식으로 동작합니다. Rev2.0 부터는 여기에 더해 **하나의 USB-CDC 케이블을 PhAI Studio 실시간 스트리밍 / 일반 터미널 두 가지 용도 중 무엇으로 쓸지 지정**하는 호스트 프로파일 API(`XM_USB_SetHostProfile`)가 추가되었습니다.

> **USB 메모리(MSC) 파일 로깅 기능은 v2.5.0 에서 제거되었습니다.** 데이터 수집은 USB-CDC 실시간 스트리밍으로 합니다 — PhAI Studio, 또는 레포 내 `xm10` 도구(그래프 + 무손실 `.xmlog` 저장 + CSV 내보내기, [안내](../../getting-started/04-pc-data-tool.md)). PhAI Studio 는 Total Data(0x20)를 보여 줍니다. PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체, 0xF0~0xFE)는 우선 `xm10` 도구로 보고 저장하세요. 온보드 저장(SD카드)은 현재 지원하지 않습니다.

---

## 언제 사용하나

PC와 시리얼로 실시간 데이터를 주고받고 싶을 때 사용합니다. Total Data(0x20) 자동 전송과 추가 채널(`XM_SendUsbDataWithId`)의 동작 원리, 흔한 실수·예제 매핑 같은 전체 그림은 [05. USB 시리얼](../05-usb-connectivity.md) 문서를 먼저 읽어보는 것을 권장합니다 — 이 페이지는 헤더에 선언된 **모든 함수·타입의 시그니처/파라미터 상세**만 다루며, Rev2.0 신규 **USB-CDC 호스트 프로파일 API** 도 포함합니다.

---

## 함수 목록

**데이터 소스 등록**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SetUsbStreamSource`](#xm_setusbstreamsource) ⚠️ Deprecated | [CDC] PC로 스트리밍할 데이터 소스 등록 (레거시) |

**CDC — 연결 · 스트리밍 상태**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_IsUsbStreamConnected`](#xm_isusbstreamconnected) | USB 장치(CDC) 준비 여부 |
| [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive) | 스트리밍 활성 여부 |
| [`XM_SetUsbAutoStream`](#xm_setusbautostream) | Auto-Stream 모드 on/off |

**CDC — 호스트 프로파일** 🟢 Rev 2.0 전용

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_USB_SetHostProfile`](#xm_usb_sethostprofile--rev-20-전용) 🟢 | USB-CDC 호스트 프로파일 지정 (PhAI Studio / 터미널) |

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

### `XM_IsUsbStreamConnected`

```c
bool XM_IsUsbStreamConnected(void);
```

USB 장치(CDC)가 준비되었는지 확인합니다. 케이블이 꽂히고 USB 초기화가 끝나면 `true` 이며, PC 프로그램이 COM 포트를 열었는지와는 무관합니다. 포트가 열려 스트리밍 중인지는 [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive) 로 확인하세요 (Rev2.0 에서 `XM_USB_HOST_TERMINAL` 프로파일을 쓰면 `XM_IsUsbStreamingActive()` 는 포트를 열어도 `false` 이고, `XM_IsUsbStreamConnected()` 는 그대로 `true` 입니다).

**파라미터**: 없음

**반환값**: `bool` — USB 장치 준비가 끝났으면 `true` (포트가 닫혀 있어도 `true`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**참고**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

### `XM_IsUsbStreamingActive`

```c
bool XM_IsUsbStreamingActive(void);
```

스트리밍이 활성화되었는지 확인합니다. Auto-Stream 모드에서는 PC 프로그램이 COM 포트를 열 때 자동으로 `true`가 되고(케이블만 꽂아서는 `true` 가 되지 않습니다), Legacy 모드에서는 `"AGRB MON START"` 수신 시 `true`가 됩니다.

**파라미터**: 없음

**반환값**: `bool` — `true`: 스트리밍 활성(데이터 전송 중), `false`: 대기

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**참고**: [`XM_SetUsbAutoStream`](#xm_setusbautostream)

---

### `XM_SetUsbAutoStream`

```c
void XM_SetUsbAutoStream(bool enabled);
```

Auto-Stream 모드를 설정합니다. 기본값은 ON — PC 프로그램이 COM 포트를 열면 자동으로 스트리밍을 시작합니다 (Rev2.0 에서는 기본 프로파일 `XM_USB_HOST_PHAI_STUDIO` 일 때만).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `enabled` | `bool` | `true`: COM 포트가 열릴 때 자동 스트리밍 (기본값). `false`: `"AGRB MON START"` 명령 대기 (Legacy Python 호환) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 컨텍스트 기준.

**참고**: [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive)

---

<a id="xm_usb_sethostprofile"></a>
### `XM_USB_SetHostProfile` 🟢 Rev 2.0 전용

```c
void XM_USB_SetHostProfile(XM_USB_HostProfile_e profile);
```

> 🟢 **Rev 2.0 전용** — Rev1.1 헤더에는 이 함수와 [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e--rev-20-전용) 타입 자체가 존재하지 않습니다.

이 보드의 USB-CDC 케이블에 붙는 PC 앱의 종류를 지정합니다. **미호출 시 기본값은 `XM_USB_HOST_PHAI_STUDIO`** 이므로, 프로파일을 따로 만지지 않으면 기존 PhAI Studio 실시간 스트리밍 동작이 그대로 유지됩니다.

일반 시리얼 터미널 / 커스텀 프로그램(Tera Term · VS Code Serial Monitor · 자작 Python GUI 등)으로 **깨끗한 텍스트/커스텀 IO 만** 쓰려면 `Control_Setup()`에서 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)`을 1회 호출하세요. 자동 전송(Total Data)과 채널 이름 전송이 꺼지고, 이 설정은 **케이블을 다시 꽂아도 유지**됩니다. `xm10` 도구나 PhAI Studio 로 받을 때는 지정하지 마세요(기본값을 그대로 두세요).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `profile` | [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e--rev-20-전용) | `XM_USB_HOST_PHAI_STUDIO`(기본) 또는 `XM_USB_HOST_TERMINAL` |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 컨텍스트 기준. 1회 호출하면 됩니다.

**예제**

```c
void Control_Setup(void) {
    // 일반 시리얼 터미널로 텍스트만 확인할 때 (Ex.07 / Ex.08 방식)
    XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL);
}
```

**참고**: [`XM_USB_HostProfile_e`](#xm_usb_hostprofile_e--rev-20-전용), [05. USB 시리얼](../05-usb-connectivity.md)

---

### `XM_SendUsbData`

```c
bool XM_SendUsbData(const void* data, uint32_t len);
```

> ⚠️ **Deprecated** — [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)로 대체되었습니다. Module ID를 명시적으로 지정하세요.

USB CDC로 데이터를 PhAI 패킷(바이너리)으로 자동 래핑하여 전송합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `data` | `const void*` | 전송할 사용자 구조체 포인터 (float 배열 또는 4-byte 정렬 struct) |
| `len` | `uint32_t` | 데이터 바이트 수 |

**반환값**: `bool` — `true`: 전송 버퍼에 쌓음(PC 가 받았는지와는 무관), `false`: 버퍼가 가득 찼거나, USB 미준비, 또는 인자 오류(NULL 포인터 · 길이 0 · 1020 바이트 초과)

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 1 ms 주기 내에서 안전하게 호출 가능합니다 (Non-blocking).

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
| `module_id` | `uint8_t` | Module ID (기본값 `0x10`) |

**반환값**: 없음 (`void`)

**참고**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SetUsbCustomMeta`

```c
void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
```

User Custom 채널의 메타데이터(채널 이름/단위 등)를 등록합니다. 채널 이름은 연결할 때 한 번 전달됩니다. 이름이 안 보이면 USB 케이블을 다시 꽂고 다시 연결하세요. `xm10` 도구가 그 이름을 그래프 제목·CSV 열 이름으로 씁니다 (Rev2.0 은 기본 프로파일 `XM_USB_HOST_PHAI_STUDIO` 일 때만).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `module_id` | `uint8_t` | 대상 Module ID (`0xF0`~`0xFE`) |
| `json_str` | `const char*` | 채널 정의 JSON 배열 문자열 (NULL-terminated, 문자열 리터럴 권장). **512바이트 이하** — 넘으면 채널 이름이 전송되지 않습니다 |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()`에서 1회 호출.

**⚠️ 포인터 수명 주의**: `json_str` 포인터는 프로그램 수명 동안 유효해야 합니다 (내부에서 복사하지 않음) — 스택 지역 변수가 아닌 문자열 리터럴을 사용하세요.

**⚠️ 단일 슬롯**: USB 연결당 하나의 Module ID 메타만 유지됩니다 (마지막 호출이 이전 것을 덮어씁니다). 여러 채널 그룹을 라벨링하려면 채널들을 하나의 Module ID로 모아 등록하세요.

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

**반환값**: `bool` — `true`: 전송 버퍼에 쌓음(PC 가 받았는지와는 무관), `false`: 버퍼가 가득 찼거나, USB 미준비, 또는 인자 오류(NULL 포인터 · 길이 0 · 1020 바이트 초과)

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

> ⚠️ PhAI Studio 나 `xm10` 도구가 스트림을 받는 중에 텍스트를 보내면 데이터 패킷 1개가 손실됩니다(메시지 1회당 1개). 텍스트 로그는 일반 터미널로 볼 때 쓰세요. 텍스트만 쓸 때는 `Control_Setup()` 에서 Rev2.0 은 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)`, Rev1.1 은 `XM_SetUsbAutoStream(false)` 를 부르면 깨끗하게 보입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `message` | `const char*` | 전송할 문자열 (Null-terminated) |

**반환값**: `bool` — `true`: 전송 버퍼에 쌓음, `false`: 버퍼가 가득 찼거나, USB 미준비, 또는 빈 문자열

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

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. `Control_Loop()` 의 한 틱 안에서 반환값이 0 이 될 때까지 반복해서 읽으세요 — 남겨 두면 같은 틱 안에 시스템이 가져가 다시 받을 수 없습니다. 반환값 0 이외의 오류 상태 구분은 문서화되어 있지 않습니다 — 사용 전 실측 확인을 권장합니다.

---

## 타입/매크로

<a id="xm_usb_hostprofile_e"></a>
### `XM_USB_HostProfile_e` 🟢 Rev 2.0 전용

> Rev1.1 헤더에는 정의되어 있지 않습니다.

한 보드/한 케이블을 어떤 PC 앱 용도로 쓸지 사용자가 지정하는 USB-CDC 호스트 프로파일입니다. 선택지는 2가지이며, [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile--rev-20-전용)로 지정합니다.

| 값 | 설명 |
|----|------|
| `XM_USB_HOST_PHAI_STUDIO` = 0 | **기본값** — PhAI Studio · `xm10` 도구로 받을 때 (Total Data 자동 전송 + 채널 이름 전송 ON) |
| `XM_USB_HOST_TERMINAL` = 1 | 일반 터미널 / 커스텀 프로그램 — 자동 전송 OFF, 사용자 명시 TX 만 |

---

## 내부 전용 (호출 금지)

아래 함수는 시스템이 자동으로 호출하는 전용 함수입니다. 사용자 코드에서 직접 호출할 필요가 없으며, 호출해도 의도한 동작을 보장하지 않습니다.

| 함수 | 설명 |
|------|------|
| `void XM_USB_ProcessPeriodic(void)` | USB 스트리밍의 주기 처리 함수. 시스템이 자동으로 호출하므로 사용자가 직접 호출할 필요 없음 |

> 🟢 **Rev 2.0 참고** — USB-CDC 호스트 프로파일을 지정하는 사용자 공개 API 는 [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile--rev-20-전용) 하나입니다.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CDC 스트리밍 기본 API (`SendUsbDataWithId`/`SetUsbCustomMeta`/`SendUsbDebugMessage`) | ✅ | ✅ |
| `XM_USB_HostProfile_e` / `XM_USB_SetHostProfile` (PhAI Studio / 터미널 호스트 프로파일 지정) | ❌ 없음 | 🟢 전용 |

---

## 관련 문서

- [05. USB 시리얼 (개념)](../05-usb-connectivity.md) — CDC 동작 원리, 흔한 실수, 예제 매핑
