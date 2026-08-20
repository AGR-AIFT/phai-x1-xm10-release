# `xm_api_usb.h` — USB-CDC 실시간 스트리밍

> **대상 헤더**: `XM_FW/XM_API/xm_api_usb.h`
> **관련 개념 문서**: [05. USB 시리얼](../05-usb-connectivity.md)
> **관련 예제**: [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) · [07_CDC_Basic_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/07_CDC_Basic_Print/) · [08_CDC_Sensor_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/) · [09_CDC_Stream](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/09_CDC_Stream/) · [18_Debug_Monitor](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/18_Debug_Monitor/)

이 헤더는 **PC 실시간 통신(CDC)** 도메인을 정의합니다. PhAI Studio·PuTTY 같은 PC 클라이언트와 텍스트/바이너리를 주고받으며, 등록 기반 자동화(Setup에서 소스 등록 → System이 주기적으로 자동 전송)로 동작합니다. Rev2.0 부터는 여기에 더해 **하나의 USB-CDC 케이블을 PhAI Studio 실시간 스트리밍 / 일반 터미널 두 가지 용도 중 무엇으로 쓸지 지정**하는 호스트 프로파일 API(`XM_USB_SetHostProfile`)가 추가되었습니다. (생산 검사 GUI 대응은 보드 내부에서 자동 처리되므로 사용자 선택지에는 없습니다.)

> **USB 메모리(MSC) 파일 로깅 기능은 v2.5.0 에서 제거되었습니다.** 데이터 수집은 USB-CDC 실시간 스트리밍(PhAI Studio 또는 레포 내 `PythonDecoder/CDC`)으로 하며, 온보드 저장(SD카드)은 향후 HW 리비전에서 지원 예정입니다.

---

## 언제 사용하나

PC와 시리얼로 실시간 데이터를 주고받고 싶을 때 사용합니다. 등록 기반 자동화(Setup에서 소스 등록 → System이 주기적으로 자동 처리) 원리와 흔한 실수·예제 매핑 같은 전체 그림은 [05. USB 시리얼](../05-usb-connectivity.md) 문서를 먼저 읽어보는 것을 권장합니다 — 이 페이지는 헤더에 선언된 **모든 함수·타입의 시그니처/파라미터 상세**만 다루며, Rev2.0 신규 **USB-CDC 호스트 프로파일 API** 도 포함합니다.

---

## 함수 목록

**데이터 소스 등록**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_SetUsbStreamSource`](#xm_setusbstreamsource) ⚠️ Deprecated | [CDC] PC로 스트리밍할 데이터 소스 등록 (레거시) |

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
| `void XM_USB_ProcessPeriodic(void)` | `core_process` | USB 스트리밍 로직의 주기 처리 엔진. `core_process`가 자동 호출하므로 사용자가 직접 호출할 필요 없음 |

> 🟢 **Rev 2.0 참고** — 이전(v2.3.1)에 이 자리에 있던 PRODUCTION 자동 latch / DTR 처리(구 `XM_USB_RequestProductionLatch` / `XM_USB_OnDtrLost`)는 v2.4.0 에서 System 레이어(`usb_host_mode`) 내부로 이관되어 공개 헤더에서 빠졌습니다. 사용자 공개 API 는 [`XM_USB_SetHostProfile()`](#xm_usb_sethostprofile) 하나입니다.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| CDC 스트리밍 기본 API (`SendUsbDataWithId`/`SetUsbCustomMeta`/`SendUsbDebugMessage`) | ✅ | ✅ |
| `XM_USB_HostProfile_e` / `XM_USB_SetHostProfile` (PhAI Studio / 터미널 호스트 프로파일 지정) | ❌ 없음 | 🟢 전용 |
| PRODUCTION 자동 진입 / DTR 처리 (System `usb_host_mode` 내부, 공개 API 아님) | ❌ 없음 | 🟢 전용 (Internal) |

---

## 관련 문서

- [05. USB 시리얼 (개념)](../05-usb-connectivity.md) — CDC 동작 원리, 흔한 실수, 예제 매핑
