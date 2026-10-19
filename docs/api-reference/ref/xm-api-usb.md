# `xm_api_usb.h` — USB-CDC 실시간 스트리밍

> **대상 헤더**: `XM_FW/XM_API/xm_api_usb.h`
> **관련 개념 문서**: [05. USB 시리얼](../05-usb-connectivity.md)
> **관련 예제**: [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) · [07_CDC_Basic_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) · [08_CDC_Sensor_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/08_CDC_Sensor_Print/) · [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) · [18_Debug_Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/)

이 헤더는 **PC 실시간 통신(CDC)** 도메인을 정의합니다. PhAI Studio·PuTTY 같은 PC 클라이언트와 텍스트/바이너리를 주고받으며, System 이 Total Data(0x20)를 자동 전송하고 사용자는 `XM_SendUsbDataWithId()` 로 추가 채널을 보내는 방식으로 동작합니다. Rev2.0 부터는 여기에 더해 **하나의 USB-CDC 케이블을 PhAI Studio 실시간 스트리밍 / 일반 터미널 두 가지 용도 중 무엇으로 쓸지 지정**하는 호스트 프로파일 API(`XM_USB_SetHostProfile`)가 추가되었습니다.

> 🟢 **Rev 2.0 전용**: USB 메모리 저장 API 는 Rev2.0 (v2.9.0 이상)에만 있습니다 — 개념 안내 [06. USB 메모리 저장](../06-usb-data-logging.md), 함수 참고 [`xm_api_usb_msc.h`](xm-api-usb-msc.md). Rev1.1 에는 USB 메모리 저장이 없습니다.
>
> ⚠️ **USB-C 포트는 하나입니다.** USB 메모리를 꽂아 두는 동안에는 PhAI Studio · `xm10` 도구 · USB 로 하는 펌웨어 업로드를 쓸 수 없습니다. 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요. (보드 확인 전) <!-- board-check -->
>
> 🟢 **Rev 2.0 전용**: 구조체를 필드 이름 · 단위 · 타입과 함께 보내는 함수는 [`xm_api_logging.h`](xm-api-logging.md) 에 있습니다 — 개념 안내 [10. 구조체 로깅](../10-struct-logging.md).
>
> 이 헤더는 PC 로 실시간 전송하는 USB-CDC 를 다룹니다. PC 로 받을 때는 PhAI Studio, 또는 레포 내 `xm10` 도구(그래프 + 무손실 `.xmlog` 저장 + CSV 내보내기, [안내](../../getting-started/04-pc-data-tool.md))를 쓰세요. PhAI Studio 는 Total Data(0x20)를 보여 줍니다. PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체, 0xF0~0xFE)는 우선 `xm10` 도구로 보고 저장하세요. 온보드 저장(SD카드)은 현재 지원하지 않습니다.

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

USB 장치(CDC)가 준비되었는지 확인합니다. 케이블이 꽂히고 USB 초기화가 끝나면 `true` 이며, PC 프로그램이 COM 포트를 열었는지와는 무관합니다. Rev2.0 에서는 USB 메모리를 꽂아 두는 동안 `false` 이고, 이때 송신 함수도 `false` 를 돌려줍니다. 포트가 열려 스트리밍 중인지는 [`XM_IsUsbStreamingActive`](#xm_isusbstreamingactive) 로 확인하세요 (Rev2.0 에서 `XM_USB_HOST_TERMINAL` 프로파일을 쓰면 `XM_IsUsbStreamingActive()` 는 포트를 열어도 `false` 이고, `XM_IsUsbStreamConnected()` 는 그대로 `true` 입니다).

**파라미터**: 없음

**반환값**: `bool` — USB 장치 준비가 끝났으면 `true` (포트가 닫혀 있어도 `true`). Rev2.0 에서는 USB 메모리를 꽂아 두는 동안 `false`

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

**⚠️ 구조체 로깅과 함께 쓸 때**: 첫 `XM_LOG_REGISTER` 보다 먼저 부르세요. 구조체를 하나라도 등록한 뒤에는 이 호출이 무시됩니다(프로파일이 바뀌지 않습니다). 또 `XM_USB_HOST_TERMINAL` 이어도 `XM_SendUsbDataWithId()` 로 직접 보낸 데이터 패킷과 구조체 로깅의 데이터 · 필드 정보는 그대로 전송되어 터미널에 깨진 글자로 섞여 보입니다. 텍스트만 볼 때는 둘 다 쓰지 마세요. ([`xm_api_logging.h`](xm-api-logging.md), Rev2.0)

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

**반환값**: `bool` — `true`: 전송 버퍼에 쌓음(PC 가 받았는지와는 무관), `false`: 버퍼가 가득 찼거나, USB 미준비, 또는 인자 오류(NULL 포인터 · 길이 0 · 1,020 바이트 초과). Rev2.0 (v2.9.0 이상)에서는 인터럽트 안에서 불렀을 때(아무것도 보내지 않습니다)도 `false` 입니다. 구조체 로깅을 쓰면 Module ID 가 등록한 구조체의 ID 일 때, 짧은 시간에 너무 많이 보낼 때, 한 번에 너무 클 때도 `false` 입니다 (Rev2.0)

**⚠️ 호출 컨텍스트**: `Control_Loop()`의 1 ms 주기 내에서 안전하게 호출 가능합니다 (Non-blocking). `XM_Task_*` 로 직접 만든 백그라운드 태스크에서는 부르지 마세요 — 보내는 데이터가 깨질 수 있습니다 (Rev2.0). Rev1.1 (v2.8.1) 은 인터럽트 안에서 불러도 막지 않지만 `Control_Loop()` 안에서만 부르세요.

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

**⚠️ 시스템이 쓰는 ID**: Rev2.0 (v2.9.0 이상)에서는 `0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF` 를 주면 무시하고 이전 ID 를 그대로 씁니다. Rev1.1 (v2.8.1) 에는 이 규칙이 없습니다.

**참고**: [`XM_SendUsbDataWithId`](#xm_sendusbdatawithid)

---

### `XM_SetUsbCustomMeta`

```c
bool XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
```

User Custom 채널의 메타데이터(채널 이름/단위 등)를 등록합니다. `xm10` 도구가 그 이름을 그래프 제목·CSV 열 이름으로 씁니다 (Rev2.0 은 기본 프로파일 `XM_USB_HOST_PHAI_STUDIO` 일 때만).
Rev2.0 (v2.9.0 이상)은 PC 프로그램이 COM 포트를 열 때마다 채널 이름을 다시 보내므로 USB 케이블을 다시 꽂을 필요가 없습니다. (보드 확인 전) <!-- board-check -->

> Rev1.1 (v2.8.1) 은 이전과 같습니다. 반환값이 없고(`void`), JSON 은 512바이트까지이며(더 길면 아무 알림 없이 전송되지 않습니다), 채널 이름은 USB 를 연결할 때 한 번 전달됩니다. 이름이 안 보이면 USB 케이블을 다시 꽂고 다시 연결하세요.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `module_id` | `uint8_t` | 대상 Module ID (`0xF0`~`0xFE`) |
| `json_str` | `const char*` | 채널 정의 JSON 배열 문자열 (NULL-terminated, 문자열 리터럴 권장). Rev2.0 은 **[`XM_USB_CUSTOM_META_MAX_LEN`](#xm_usb_custom_meta_max_len)(1,007바이트) 이하** — 넘거나 `NULL` 이면 `false` 를 돌려주고 이전에 등록한 이름이 그대로 남습니다 (`NULL` 로 이름을 지울 수는 없습니다). Rev1.1 은 **512바이트 이하** |

**반환값**: `bool` — `true`: 등록됨, `false`: `json_str` 이 `NULL` 이거나 1,007바이트보다 김. 구조체 로깅을 쓰면서 `Control_Setup()` 밖에서 불러도 `false` 입니다 (Rev2.0). Rev1.1 은 반환값이 없습니다 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()`에서 1회 호출. 구조체 로깅을 쓰면 `Control_Setup()` 안에서만 쓸 수 있습니다 (Rev2.0).

**⚠️ 포인터 수명 주의**: `json_str` 포인터는 프로그램 수명 동안 유효해야 합니다 (내부에서 복사하지 않음) — 스택 지역 변수가 아닌 문자열 리터럴을 사용하세요.

**⚠️ 단일 슬롯**: 하나의 Module ID 메타만 유지됩니다 (등록에 성공한 마지막 호출이 이전 것을 덮어씁니다). 여러 채널 그룹을 라벨링하려면 채널들을 하나의 Module ID로 모아 등록하세요.

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

**반환값**: `bool` — `true`: 전송 버퍼에 쌓음(PC 가 받았는지와는 무관). `false`:

- 공통: 버퍼가 가득 찼을 때, USB 가 준비되지 않았을 때, 인자가 잘못됐을 때(NULL 포인터 · 길이 0 · 1,020 바이트 초과)
- Rev2.0 (v2.9.0 이상): `module_id` 가 시스템이 쓰는 ID(`0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF`)일 때, 인터럽트 안에서 불렀을 때(아무것도 보내지 않습니다). 사용자 데이터는 `0xF0`~`0xFE` 로 보내세요
- 구조체 로깅을 쓸 때 (Rev2.0): 등록한 구조체의 Module ID 로 보낼 때, 이 함수 · `XM_SendUsbDebugMessage()` 로 짧은 시간에 너무 많이 보낼 때(둘을 합쳐 셉니다), 한 번에 너무 클 때. 너무 큰 데이터는 기다려도 나가지 않으니 구조체 로깅([10. 구조체 로깅](../10-struct-logging.md))으로 보내세요

> Rev1.1 (v2.8.1): 시스템이 쓰는 ID 로 보내도 막지 않습니다. 그래도 사용자 데이터는 `0xF0`~`0xFE` 로 보내세요.

**⚠️ 호출 컨텍스트**: `Control_Loop()` 내에서 호출 (Non-blocking). 매 tick 호출이 필수는 아니며, 필요 시에만 호출해도 됩니다. `XM_Task_*` 로 직접 만든 백그라운드 태스크에서는 부르지 마세요 — 보내는 데이터가 깨질 수 있습니다 (Rev2.0). Rev1.1 (v2.8.1) 은 인터럽트 안에서 불러도 막지 않지만 `Control_Loop()` 안에서만 부르세요.

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

> ⚠️ 텍스트 로그는 일반 터미널로 볼 때 쓰세요. 텍스트만 쓸 때는 `Control_Setup()` 에서 Rev2.0 은 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)`, Rev1.1 은 `XM_SetUsbAutoStream(false)` 를 부르면 깨끗하게 보입니다.
>
> * Rev2.0 (v2.9.0 이상): 문자열 뒤에 0x00(널 문자) 한 바이트를 붙여 보냅니다. `XM_USB_HOST_TERMINAL` 프로파일에서도 똑같이 붙습니다.
>   함께 보내는 데이터 패킷이 깨지지 않게 하려는 것이라, PhAI Studio 나 `xm10` 도구가 스트림을 받는 중에 텍스트를 보내도 데이터 패킷이 손실되지 않습니다. (보드 확인 전) <!-- board-check -->
> * Rev1.1 (v2.8.1): 이전과 같습니다. 0x00 을 붙이지 않고, 스트림을 받는 중에 텍스트를 보내면 데이터 패킷 1개가 손실됩니다(메시지 1회당 1개).

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `message` | `const char*` | 전송할 문자열 (Null-terminated). 길이는 **바이트** 기준이고 최대 [`XM_USB_DEBUG_MESSAGE_MAX_LEN`](#xm_usb_debug_message_max_len)(1,021바이트)입니다 — 한글은 글자 수보다 바이트가 많습니다 (Rev2.0) |

**반환값**: `bool` — `true`: 전송 버퍼에 쌓음(PC 가 받았는지와는 무관). `false`:

- 공통: 전송 버퍼가 가득 찼을 때, USB 가 준비되지 않았을 때
- Rev2.0 (v2.9.0 이상): `message` 가 `NULL` 이거나 1,021바이트보다 길 때, 인터럽트 안에서 불렀을 때(아무것도 보내지 않습니다). 빈 문자열(`""`)은 0x00 한 바이트만 보내고 `true` 입니다
- Rev1.1 (v2.8.1): 빈 문자열일 때

구조체 로깅을 쓰면 한도가 더 짧아집니다. 짧은 시간에 너무 많이 보내거나(`XM_SendUsbDataWithId()` 와 합쳐 셉니다) 한 번에 너무 길면 `false` 입니다 — 너무 긴 메시지는 기다려도 나가지 않으니 줄이거나, 시간 간격을 두고 나눠 보내세요 (Rev2.0, [10. 구조체 로깅](../10-struct-logging.md)).

**⚠️ 호출 컨텍스트**: `Control_Loop()` 안에서 부르세요. 비차단(Non-blocking)입니다. `Control_Setup()` 에서는 USB 가 아직 준비되지 않아 보낸 메시지가 보통 나가지 않습니다. `XM_Task_*` 로 직접 만든 백그라운드 태스크에서는 부르지 마세요 — 보내는 데이터가 깨질 수 있습니다 (Rev2.0). Rev1.1 (v2.8.1) 은 인터럽트 안에서 불러도 막지 않지만 `Control_Loop()` 안에서만 부르세요.

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

> 🟢 **Rev 2.0 참고** — USB-CDC 호스트 프로파일을 지정하는 사용자 공개 API 는 [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile--rev-20-전용) 하나입니다.

<a id="xm_usb_custom_meta_max_len"></a>
### `XM_USB_CUSTOM_META_MAX_LEN` 🟢 Rev 2.0 전용

> Rev1.1 헤더에는 정의되어 있지 않습니다. Rev1.1 의 JSON 한도는 512바이트입니다.

```c
#define XM_USB_CUSTOM_META_MAX_LEN  (1007U)
```

[`XM_SetUsbCustomMeta()`](#xm_setusbcustommeta) 에 넣을 수 있는 JSON 문자열의 최대 길이입니다 (바이트, 끝의 `'\0'` 제외). 넘으면 `XM_SetUsbCustomMeta()` 가 `false` 를 돌려줍니다.

<a id="xm_usb_debug_message_max_len"></a>
### `XM_USB_DEBUG_MESSAGE_MAX_LEN` 🟢 Rev 2.0 전용

> Rev1.1 헤더에는 정의되어 있지 않습니다.

```c
#define XM_USB_DEBUG_MESSAGE_MAX_LEN  (1021U)
```

[`XM_SendUsbDebugMessage()`](#xm_sendusbdebugmessage) 로 한 번에 보낼 수 있는 문자열의 최대 길이입니다 (바이트, 끝의 `'\0'` 제외). 넘으면 `false` 를 돌려줍니다. 구조체 로깅을 쓰면 이보다 짧은 한도가 적용됩니다.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CDC 스트리밍 기본 API (`SendUsbDataWithId`/`SetUsbCustomMeta`/`SendUsbDebugMessage`) | ✅ | ✅ |
| `XM_USB_HostProfile_e` / `XM_USB_SetHostProfile` (PhAI Studio / 터미널 호스트 프로파일 지정) | ❌ 없음 | 🟢 전용 |
| `XM_SetUsbCustomMeta` — 반환값 · JSON 한도 | `void` · 512바이트 (넘으면 아무 알림 없이 전송되지 않음) | `bool` · 1,007바이트 (`XM_USB_CUSTOM_META_MAX_LEN`, 넘거나 `NULL` 이면 `false`) |
| 채널 이름을 보내는 때 | USB 를 연결할 때 한 번 | PC 프로그램이 COM 포트를 열 때마다. (보드 확인 전) | <!-- board-check -->
| `XM_SendUsbDebugMessage` | 문자열만 보냄 · 빈 문자열은 `false` · 스트림을 받는 중에 보내면 메시지마다 데이터 패킷 1개 손실 | 문자열 뒤에 0x00 한 바이트 · 1,021바이트 한도(`XM_USB_DEBUG_MESSAGE_MAX_LEN`) · 빈 문자열은 `true` |
| 시스템이 쓰는 ID (`0x20` · `0xB2` · `0xED` · `0xEE` · `0xEF` · `0xFF`) | 막지 않음 | `XM_SendUsbDataWithId()` 는 `false`, `XM_SetUsbStreamModuleId()` 는 무시 |
| 송신 함수를 인터럽트 안에서 호출 | 막지 않음 | 아무것도 보내지 않고 `false` |
| 헤더의 시스템 내부용 선언 3개 | 남아 있음 (직접 호출하지 않습니다) | 빠짐 — 이 선언을 쓰던 코드가 있다면 지우세요. 남아 있으면 컴파일 오류가 납니다 ([v2.9.0 릴리즈 노트](../../release-notes/v2.9.0.md)) |
| 구조체 로깅 ([`xm_api_logging.h`](xm-api-logging.md)) | ❌ 없음 | 🟢 전용 |
| USB 메모리 저장 ([`xm_api_usb_msc.h`](xm-api-usb-msc.md)) | ❌ 없음 | 🟢 전용 |

---

## 관련 문서

- [05. USB 시리얼 (개념)](../05-usb-connectivity.md) — CDC 동작 원리, 흔한 실수, 예제 매핑
- [10. 구조체 로깅 (개념)](../10-struct-logging.md) · [`xm_api_logging.h`](xm-api-logging.md) — 구조체를 필드 이름 · 단위 · 타입과 함께 보내기 🟢 **Rev 2.0 전용**
- [06. USB 메모리 저장 (개념)](../06-usb-data-logging.md) · [`xm_api_usb_msc.h`](xm-api-usb-msc.md) — USB 메모리에 저장하기 🟢 **Rev 2.0 전용**
