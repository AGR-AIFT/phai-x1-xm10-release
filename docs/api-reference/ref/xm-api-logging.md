# `xm_api_logging.h` — USB-CDC 구조체 로깅

> 🟢 **Rev 2.0 전용** *(v2.9.0 신규)* — 본 헤더와 `xm_api_logging_raw.h` 는 XM10 Rev 2.0 SDK 에만 포함되어 있습니다. Rev 1.1 SDK (`XM10_SDK/Rev1.1/`) 에는 두 헤더가 모두 없으므로, Rev 1.1 프로젝트에서 include 하면 빌드가 실패합니다. 보드 리비전 확인은 [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교) 참고.
>
> - **대상 헤더**: `XM_FW/XM_API/xm_api_logging.h` · `XM_FW/XM_API/xm_api_logging_raw.h`
> - **관련 개념 문서**: [10. 구조체 로깅](../10-struct-logging.md)
> - **관련 예제**: [Ex.44 CDC Struct Logging](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) — 구조체 둘을 이름 · 단위 · 타입과 함께 PC 로 보냅니다

---

## 언제 사용하나

직접 만든 구조체를 필드 이름 · 단위 · 타입과 함께 PC 로 보낼 때 사용합니다. 기존 방식(`XM_SetUsbCustomMeta` + `XM_SendUsbDataWithId`)과의 차이, 한도, PC 에서 보이는 모습, 흔한 실수 같은 전체 그림은 [10. 구조체 로깅](../10-struct-logging.md) 을 먼저 읽어 보세요 — 이 페이지는 헤더에 선언된 **모든 매크로 · 함수 · 타입의 시그니처 · 파라미터 · 반환값**만 다룹니다.

`xm_api.h` 가 이 헤더를 함께 불러오므로 `XM_LOG_*` · `XM_FIELD*` · `XM_Log_*` 이름은 SDK 가 씁니다. 같은 이름을 직접 만들지 마세요.

### include

```c
#include "xm_api.h"                // xm_api_logging.h 를 함께 불러옵니다
#include "xm_api_logging_raw.h"    // XM_LOG_* 매크로를 쓰는 .c 파일은 꼭 include 하세요
```

`xm_api.h` 는 `xm_api_logging.h` 를 포함하지만 `xm_api_logging_raw.h` 는 포함하지 않습니다. `XM_LOG_REGISTER` · `XM_LOG_SEND` 매크로가 안에서 부르는 함수의 원형이 그 헤더에 있어서, 빠지면 매크로 줄에서 `implicit declaration of function` 빌드 오류가 납니다.

### 기본 사용 흐름

```c
#include "xm_api.h"
#include "xm_api_logging_raw.h"

typedef struct {
    float    hip_angle;
    int16_t  emg[8];
    uint32_t mode;
} MyLog_t;                                          // 24 B — 크기는 4 의 배수

XM_LOG_DECLARE(MyLog, MyLog_t);                     // 토큰 MyLog ↔ 구조체 MyLog_t

XM_LOG_FIELDS(MyLog) = {
    XM_FIELD(MyLog, hip_angle, "hip_angle", "deg", 1.0f),
    XM_FIELD_ARRAY(MyLog, emg, "emg", "mV", 1.0f),
    XM_FIELD(MyLog, mode, "mode", "", 1.0f),
};

void Control_Setup(void)
{
    if (!XM_LOG_REGISTER(MyLog, 0xF0, 1000)) {
        // 등록 실패는 LED 로 알립니다 — Control_Setup() 에서는 보통 USB 가 아직 준비되지 않아 메시지가 나가지 않습니다
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    }
}

void Control_Loop(void)
{
    MyLog_t s = { 0 };
    (void)XM_LOG_SEND(MyLog, &s);                   // 등록한 주기보다 일찍 부르면 보내지 않고 false
}
```

네 매크로(`XM_LOG_DECLARE` · `XM_LOG_FIELDS` · `XM_LOG_REGISTER` · `XM_LOG_SEND`)는 같은 `.c` 파일 안에서 씁니다. 매크로는 C 파일 전용입니다. 지원하지 않는 멤버 타입, 4 의 배수가 아닌 크기, 다른 구조체 포인터처럼 틀린 사용은 컴파일러가 그 줄을 가리킵니다.

---

## 함수 · 매크로 목록

**구조체 정의 · 등록 · 전송 (매크로)**

| 매크로 | 한 줄 설명 |
|------|-----------|
| [`XM_LOG_DECLARE`](#xm_log_declare) | 토큰과 구조체 타입을 묶습니다 (`.c` 파일 위쪽) |
| [`XM_LOG_FIELDS`](#xm_log_fields) | 필드 설명 배열을 정의합니다 |
| [`XM_FIELD`](#xm_field) | 스칼라 멤버 한 줄 |
| [`XM_FIELD_ARRAY`](#xm_field_array) | 배열 멤버 한 줄 |
| [`XM_LOG_REGISTER`](#xm_log_register) | 구조체를 등록합니다 (`Control_Setup()`) |
| [`XM_LOG_SEND`](#xm_log_send) | 등록한 구조체 하나를 보냅니다 (`Control_Loop()`) |

**설정 · 진단 (함수)**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_Log_SetMinStallMs`](#xm_log_setminstallms) | PC 프로그램이 잠시 멈춰도 데이터를 담아 두는 목표 시간(ms) 설정 |
| [`XM_Log_GetMinStallMs`](#xm_log_getminstallms) | 지금 쓰는 목표 시간(ms) — 진단용 |
| [`XM_Log_EnableLinkHealth`](#xm_log_enablelinkhealth) | 링크 상태 정보 전송 켜기 / 끄기 |
| [`XM_Log_GetAdmittedWireBps`](#xm_log_getadmittedwirebps) | 이미 정해진 송신 몫(바이트/초) — 진단용 |
| [`XM_Log_GetStats`](#xm_log_getstats) | 관측 카운터 복사 |

**지원 헤더 `xm_api_logging_raw.h` — 직접 호출 금지**

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_Log_RegisterSchemaRaw`](#xm_log_registerschemaraw) | `XM_LOG_REGISTER` 가 부르는 함수 |
| [`XM_Log_SendRaw`](#xm_log_sendraw) | `XM_LOG_SEND` 가 부르는 함수 |

---

## 함수 · 매크로 상세

### `XM_LOG_DECLARE`

```c
XM_LOG_DECLARE(tok, Type);
```

토큰과 구조체 타입을 묶습니다. `.c` 파일 위쪽에 한 번 씁니다. 토큰이 구조체 이름이 됩니다.

**파라미터**

| 이름 | 설명 |
|------|------|
| `tok` | 토큰(짧은 이름) — 11자 이하 (`XM_LOG_MAX_TOKEN_LEN`), 영문 · 숫자 · `_` |
| `Type` | 보낼 구조체 타입 — 크기는 4 의 배수, 1,008 B 이하 |

**반환값**: 없음 (선언 매크로)

아래 경우에는 이 줄에서 컴파일 오류가 납니다.

| 컴파일 오류 | 원인 |
|------|------|
| `... must be a multiple of 4 - add padding` | 구조체 크기가 4 의 배수가 아님 |
| `... exceeds XM_LOG_MAX_STRUCT_SIZE` | 구조체 크기가 1,008 B 를 넘음 |
| `token ... longer than 11 chars` | 토큰이 11자를 넘음 |

크기를 맞출 때는 끝에 `uint8_t` 배열을 더하세요. 그 배열은 `XM_FIELD` 로 적지 않아도 됩니다.

**⚠️ 호출 컨텍스트**: 파일 범위(함수 밖)에 씁니다. 같은 `.c` 파일 안의 `XM_LOG_FIELDS` · `XM_LOG_REGISTER` · `XM_LOG_SEND` 와 같은 토큰을 씁니다.

**예제**

```c
typedef struct {
    float angle;      // [deg]
    float torque;     // [Nm]
} JointLog_t;         // 8 B

XM_LOG_DECLARE(JointLog, JointLog_t);
```

**참고**: [`XM_LOG_FIELDS`](#xm_log_fields), [`XM_LOG_REGISTER`](#xm_log_register)

---

### `XM_LOG_FIELDS`

```c
XM_LOG_FIELDS(tok) = { /* XM_FIELD(...), XM_FIELD_ARRAY(...) */ };
```

필드 설명 배열을 정의합니다. 뒤에 `= { XM_FIELD(...), ... };` 를 씁니다. 중괄호 안에는 **같은 토큰**의 `XM_FIELD` · `XM_FIELD_ARRAY` 만 쓰세요. 컴파일러가 이 규칙은 검사하지 못합니다.

**파라미터**

| 이름 | 설명 |
|------|------|
| `tok` | `XM_LOG_DECLARE` 의 토큰 |

**반환값**: 없음 (정의 매크로)

필드는 1 ~ 90개입니다. 0개이거나 90개를 넘으면 `XM_LOG_REGISTER` 줄에서 컴파일 오류(`field count must be 1..90`)가 납니다.

**⚠️ 호출 컨텍스트**: 파일 범위(함수 밖)에 씁니다. `XM_LOG_REGISTER` 보다 앞에 있어야 합니다.

**예제**

```c
XM_LOG_FIELDS(JointLog) = {
    XM_FIELD(JointLog, angle,  "angle",  "deg", 1.0f),
    XM_FIELD(JointLog, torque, "torque", "Nm",  1.0f),
};
```

**참고**: [`XM_FIELD`](#xm_field), [`XM_FIELD_ARRAY`](#xm_field_array)

---

### `XM_FIELD`

```c
XM_FIELD(tok, member, nm, un, sc)
```

스칼라 멤버 한 줄입니다. 타입과 위치는 멤버에서 정해집니다.

**파라미터**

| 이름 | 설명 |
|------|------|
| `tok` | `XM_LOG_DECLARE` 의 토큰 |
| `member` | 구조체 멤버 이름 |
| `nm` | 필드 이름 — 영문 · 숫자 · 기호(ASCII) 15자까지 (한글 · `°` 처럼 ASCII 가 아닌 글자는 등록이 `false`) |
| `un` | 단위 — 영문 · 숫자 · 기호(ASCII) 7자까지, 없으면 `""` (예: `"deg"`) |
| `sc` | 배율 — 물리값 = 저장값 × `sc` (보통 `1.0f`) |

**반환값**: 없음 (`XM_FieldDesc_t` 초기화 식)

지원하는 멤버 타입은 `float` · `double` · `bool` · `int` · `unsigned` · `int8_t` ~ `uint64_t` 입니다. 열거형 멤버는 크기가 같은 정수 타입으로 받습니다. 그 밖의 타입(`char` · 포인터 · 구조체 · 배열 등)은 컴파일 오류(`unsupported member type`)입니다.

**⚠️ 호출 컨텍스트**: `XM_LOG_FIELDS` 의 중괄호 안에서만 씁니다.

**예제**

```c
XM_FIELD(JointLog, angle, "angle", "deg", 1.0f)
```

**참고**: [`XM_FIELD_ARRAY`](#xm_field_array), [`XM_FieldType_e`](#xm_fieldtype_e)

---

### `XM_FIELD_ARRAY`

```c
XM_FIELD_ARRAY(tok, member, nm, un, sc)
```

배열 멤버 한 줄입니다. 원소 타입과 개수(1 ~ 255)는 배열에서 정해집니다. 포인터 멤버나 256개 이상인 배열은 컴파일 오류입니다. 인자는 [`XM_FIELD`](#xm_field) 와 같습니다.

**파라미터**: [`XM_FIELD`](#xm_field) 와 같습니다.

**반환값**: 없음 (`XM_FieldDesc_t` 초기화 식)

**⚠️ 호출 컨텍스트**: `XM_LOG_FIELDS` 의 중괄호 안에서만 씁니다.

**예제**

```c
XM_FIELD_ARRAY(MyLog, emg, "emg", "mV", 1.0f)     // int16_t emg[8] → 원소 8개
```

**참고**: [`XM_FIELD`](#xm_field)

---

### `XM_LOG_REGISTER`

```c
XM_LOG_REGISTER(tok, module_id, nominal_rate_hz)   // → bool
```

구조체를 등록합니다. `Control_Setup()` 안에서 부릅니다. 실제로 부르는 함수는 [`XM_Log_RegisterSchemaRaw`](#xm_log_registerschemaraw) 입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `tok` | 토큰 | `XM_LOG_DECLARE` 의 토큰 (`XM_LOG_FIELDS` 가 앞에 있어야 합니다) |
| `module_id` | `uint8_t` | 이 구조체를 보낼 모듈 ID — `0xF0` ~ `0xFE` |
| `nominal_rate_hz` | `uint16_t` | 보낼 주기(Hz) — `XM_LOG_SEND` 를 더 자주 불러도 이보다 자주 나가지 않습니다 |

간격을 1 ms 단위로 올려 잡아서 1,000 의 약수가 아니면 덜 자주 나갑니다 (예: 300 Hz → 250 Hz, 600 Hz → 500 Hz). 1,000 Hz 보다 자주는 나가지 않습니다.

**반환값**: `bool` — `true`: 등록됨. `false`: 아래 표 참고.

| `false` 가 되는 경우 | 해결 |
|------|------|
| 이 토큰을 이미 등록함 | 토큰마다 한 번만 등록합니다 |
| 모듈 ID 가 `0xF0` ~ `0xFE` 밖이거나 이미 쓰임 | 남은 ID 를 씁니다 |
| 등록이 4개를 넘음 | 구조체는 4개까지 등록합니다 |
| `nominal_rate_hz` 가 0 | 1 이상으로 정합니다 |
| 필드 이름이 겹치거나 두 필드가 구조체의 같은 바이트를 가리킴 | 필드 이름 · 멤버를 확인합니다 |
| 필드 이름 · 단위가 너무 길거나, 토큰 · 필드 이름 · 단위에 ASCII 가 아닌 글자(한글 등)가 있음 | 토큰은 영문 · 숫자 · `_` 로, 이름은 15자 · 단위는 7자 이하의 ASCII 문자로 고칩니다 |
| `scale` 이 0 이거나 숫자가 아님 (무한대 · NaN) | 0 이 아닌 값으로 정합니다 |
| 보낼 양이 USB 로 보낼 수 있는 양을 넘음 (먼저 등록한 구조체와 합쳐서 셉니다) | 주기를 낮추거나 구조체를 줄입니다 |
| 먼저 등록한 구조체와 합친 양이 PC 프로그램이 잠시 멈춰도 담아 둘 수 있는 양을 넘음 ([`XM_Log_SetMinStallMs`](#xm_log_setminstallms) 참고) | 주기를 낮추거나 구조체를 줄입니다 |
| `Control_Setup()` 밖(`Control_Setup()` 이 끝난 뒤 · 다른 태스크 · 인터럽트)에서 부름 | `Control_Setup()` 안에서 부릅니다 |

- `false` 면 등록 상태는 바뀌지 않으니 `Control_Setup()` 안에서 고쳐 다시 부를 수 있습니다. `false` 여도 이미 등록된 같은 토큰은 그대로 보낼 수 있습니다.
- 필드는 1 ~ 90개입니다. 0개이거나 90개를 넘으면 컴파일 오류입니다.
- 등록한 모듈 ID 는 `XM_LOG_SEND` 전용입니다. 같은 ID 로 `XM_SendUsbDataWithId()` 를 부르면 `false` 입니다.
- `Control_Setup()` 이 끝난 뒤 부르면 `seal_reject_count` 가, 다른 태스크 · 인터럽트에서 부르면 `owner_violation_count` 가 오릅니다 ([`XM_LogStats_t`](#xm_logstats_t)).
- 어느 크기 · 주기까지 등록되는지는 보드에서 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

**첫 등록에 성공하면** 아래가 달라집니다 (자세한 내용은 [10. 구조체 로깅](../10-struct-logging.md) 7장).

- `XM_USB_SetHostProfile()` 호출이 무시됩니다.
- `XM_Log_SetMinStallMs()` 가 `false` 를 돌려줍니다.
- `XM_SetUsbCustomMeta()` 는 `Control_Setup()` 안에서만 쓸 수 있습니다. 그 뒤에 부르면 `false` 입니다.
- 등록한 모듈 ID 는 이 구조체 전용이 됩니다.
- `XM_SendUsbDataWithId()` · `XM_SendUsbDebugMessage()` 로 보내는 양에 한도가 생깁니다.
- 보드가 필드 이름 · 단위 · 타입을 PC 로 보냅니다. PC 프로그램이 포트를 열 때 전달되고 그 뒤에도 몇 초 간격으로 다시 보냅니다. (설계 목표, 보드 확인 전) <!-- board-check -->

**⚠️ 호출 컨텍스트**: `Control_Setup()` 안에서만 부릅니다.

**예제**

```c
void Control_Setup(void)
{
    if (!XM_LOG_REGISTER(JointLog, 0xF0, 500)) {      // 500 Hz — 2 ms 마다
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);
    }
}
```

**참고**: [`XM_LOG_SEND`](#xm_log_send), [`XM_Log_SetMinStallMs`](#xm_log_setminstallms)

---

### `XM_LOG_SEND`

```c
XM_LOG_SEND(tok, ptr)   // → bool
```

등록한 구조체 하나를 보냅니다. `Control_Loop()` 안에서 부릅니다. 실제로 부르는 함수는 [`XM_Log_SendRaw`](#xm_log_sendraw) 입니다. 구조체는 호출 안에서 복사되므로 지역 변수여도 됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `tok` | 토큰 | `XM_LOG_DECLARE` 의 토큰 |
| `ptr` | 구조체 포인터 | 그 구조체의 포인터 — 다른 타입의 포인터는 컴파일 오류입니다 |

**반환값**: `bool` — `true`: 보냄 (보낼 버퍼에 담았다는 뜻이며 PC 가 받았는지와는 무관합니다). `false`: 아래 중 하나.

- 등록 안 됨 (`XM_LOG_REGISTER` 가 `false` 였던 토큰 포함)
- `ptr` 가 `NULL`
- 등록 주기보다 일찍 부름
- USB 장치가 준비되지 않음
- 송신 버퍼가 가득 참
- `Control_Setup()` · `Control_Loop()` 밖(다른 태스크 · 인터럽트)에서 부름

매 루프 불러도 등록한 주기로만 나가므로, 주기가 되기 전의 `false` 는 정상입니다. 송신 버퍼가 가득 차 `false` 가 나와도 다음 송신은 등록한 주기가 지난 뒤에 나갑니다 (바로 다시 불러도 보내지 않습니다). `Control_Setup()` · `Control_Loop()` 밖에서 부르면 `owner_violation_count` 가 오릅니다 ([`XM_LogStats_t`](#xm_logstats_t)).

**⚠️ 호출 컨텍스트**: `Control_Loop()` 안에서 부릅니다. 인터럽트 · 직접 만든 백그라운드 태스크(`XM_Task_*`)에서는 부르지 마세요.

**예제**

```c
void Control_Loop(void)
{
    JointLog_t s = { XM.status.h10.leftHipAngle, XM.status.h10.leftHipTorque };
    (void)XM_LOG_SEND(JointLog, &s);      // 매 루프 불러도 500 Hz 로만 나갑니다
}
```

**참고**: [`XM_LOG_REGISTER`](#xm_log_register), [`XM_Log_GetStats`](#xm_log_getstats)

---

### `XM_Log_SetMinStallMs`

```c
bool XM_Log_SetMinStallMs(uint16_t ms);
```

PC 프로그램이 잠시 멈춰도 보드가 데이터를 빠뜨리지 않고 담아 두는 목표 시간(ms)을 정합니다. 기본값은 50 ms 입니다. 이 목표를 지킬 수 없을 만큼 많이 보내는 구조체의 등록은 `false` 가 됩니다. 50 ms 는 아직 보드에서 재 보지 않은 초기값이라 보장이 아니라 목표입니다. (설계 목표) <!-- board-check -->

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `ms` | `uint16_t` | 목표 시간(ms) |

**반환값**: `bool` — `true`: 바뀜. `false`: `Control_Setup()` 밖에서 부름, 또는 등록에 성공한 구조체가 이미 있음

**⚠️ 호출 컨텍스트**: `Control_Setup()` 안에서, 첫 `XM_LOG_REGISTER` 보다 먼저 부릅니다.

**예제**

```c
void Control_Setup(void)
{
    (void)XM_Log_SetMinStallMs(100);                  // 값은 예시입니다 — 기본 50 ms 로 충분하면 부르지 않아도 됩니다
    (void)XM_LOG_REGISTER(JointLog, 0xF0, 500);
}
```

**참고**: [`XM_Log_GetMinStallMs`](#xm_log_getminstallms), [`XM_LOG_REGISTER`](#xm_log_register)

---

### `XM_Log_GetMinStallMs`

```c
uint16_t XM_Log_GetMinStallMs(void);
```

지금 쓰는 목표 시간(ms)을 돌려줍니다. 진단용입니다.

**파라미터**: 없음

**반환값**: `uint16_t` — 목표 시간(ms)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**참고**: [`XM_Log_SetMinStallMs`](#xm_log_setminstallms)

---

### `XM_Log_EnableLinkHealth`

```c
bool XM_Log_EnableLinkHealth(bool enable);
```

링크 상태 정보(송신 실패 수 · 버퍼 사용률 등)를 1초마다 보낼지 정합니다. 기본은 보내지 않습니다. 구조체를 하나 이상 등록했을 때만 보냅니다 — 등록하지 않으면 켜도 보내지 않습니다. (설계 목표, 보드 확인 전) <!-- board-check -->

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `enable` | `bool` | `true`: 보냄, `false`: 보내지 않음 |

**반환값**: `bool` — `true`: 설정됨. `false`: `Control_Setup()` 밖에서 부름

`xm10` 도구는 이 값을 따로 풀어 보여 주지 않습니다. 보드 안에서 로깅 관련 카운터를 보려면 [`XM_Log_GetStats`](#xm_log_getstats) 를 쓰세요.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 안에서 부릅니다.

**예제**

```c
void Control_Setup(void)
{
    (void)XM_LOG_REGISTER(JointLog, 0xF0, 500);
    (void)XM_Log_EnableLinkHealth(true);              // 링크 상태 전송 켜기
}
```

**참고**: [`XM_Log_GetStats`](#xm_log_getstats)

---

### `XM_Log_GetAdmittedWireBps`

```c
uint32_t XM_Log_GetAdmittedWireBps(void);
```

USB 송신 예산 가운데 이미 정해진 몫(바이트/초)을 돌려줍니다. 등록한 구조체들의 몫과 보드가 미리 잡아 둔 몫의 합입니다.

미리 잡아 둔 몫은 등록하지 않은 데이터 · 디버그 텍스트, 구조체 정보 · 링크 상태 전송, 그리고 기본 프로파일(`XM_USB_HOST_PHAI_STUDIO`)이면 Total Data 입니다. 구조체를 하나도 등록하지 않았으면 미리 잡아 둔 몫만 나옵니다. 진단용입니다.

**파라미터**: 없음

**반환값**: `uint32_t` — 이미 정해진 송신 몫 (바이트/초)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**참고**: [`XM_LOG_REGISTER`](#xm_log_register)

---

### `XM_Log_GetStats`

```c
void XM_Log_GetStats(XM_LogStats_t* out);
```

관측 카운터를 복사합니다. 칸마다 따로 읽으므로 여러 칸이 정확히 같은 순간의 값은 아닐 수 있습니다. 칸의 뜻은 [`XM_LogStats_t`](#xm_logstats_t) 를 보세요.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `out` | `XM_LogStats_t*` | 받을 곳 — `NULL` 이면 아무것도 하지 않습니다 |

**반환값**: 없음 (`void`) — 결과는 `out` 을 통해 돌아옵니다.

**⚠️ 호출 컨텍스트**: `Control_Loop()` 에서 부릅니다.

**예제**

```c
static XM_LogStats_t s_stats;

void Control_Loop(void)
{
    XM_Log_GetStats(&s_stats);            // 디버거 Live Expressions 에 s_stats 를 넣어 봅니다
}
```

**참고**: [`XM_LogStats_t`](#xm_logstats_t)

---

## 타입 / 상수

### `XM_FieldType_e`

필드 타입입니다. `XM_FIELD` · `XM_FIELD_ARRAY` 가 멤버의 C 타입에서 정합니다 (직접 적지 않습니다).

| 값 | 이름 | 멤버의 C 타입 | 크기 |
|----|------|------|------|
| 0 | `XM_FIELD_F32` | `float` | 4 B |
| 1 | `XM_FIELD_F64` | `double` | 8 B |
| 2 | `XM_FIELD_I8` | `int8_t` | 1 B |
| 3 | `XM_FIELD_U8` | `uint8_t` | 1 B |
| 4 | `XM_FIELD_I16` | `int16_t` | 2 B |
| 5 | `XM_FIELD_U16` | `uint16_t` | 2 B |
| 6 | `XM_FIELD_I32` | `int32_t` · `int` | 4 B |
| 7 | `XM_FIELD_U32` | `uint32_t` · `unsigned` | 4 B |
| 8 | `XM_FIELD_I64` | `int64_t` | 8 B |
| 9 | `XM_FIELD_U64` | `uint64_t` | 8 B |
| 10 | `XM_FIELD_BOOL` | `bool` | 1 B |

---

### `XM_FieldDesc_t`

필드 설명 한 줄입니다. `XM_FIELD` · `XM_FIELD_ARRAY` 로 만드세요. 직접 채우면 컴파일러가 타입 · 위치를 검사하지 못합니다.

| 멤버 | 타입 | 설명 |
|------|------|------|
| `name` | `const char*` | 필드 이름 (영문 · 숫자 · 기호 15자까지, ASCII 만) |
| `unit` | `const char*` | 단위 (영문 · 숫자 · 기호 7자까지, ASCII 만, 없으면 `""`) |
| `type` | `uint8_t` | 원소 타입 — `XM_FieldType_e` 값. 컴파일 옵션과 상관없이 1 바이트입니다 |
| `offset` | `uint16_t` | 구조체 안 위치(바이트) |
| `array_len` | `uint8_t` | 원소 개수 (스칼라 = 1) |
| `scale` | `float` | 물리값 = 저장값 × `scale` |

---

### `XM_LogStats_t`

로깅 관측 카운터입니다. [`XM_Log_GetStats()`](#xm_log_getstats) 로 받습니다. 모두 부팅 뒤 누적값이고, 따로 적지 않은 칸은 구조체를 등록하지 않아도 셉니다.

| 멤버 | 타입 | 설명 |
|------|------|------|
| `rate_gate_suppressed_calls` | `uint32_t` | `XM_LOG_SEND` 를 등록한 주기보다 자주 불러 보내지 않은 횟수 (등록한 구조체가 있을 때만 생깁니다) |
| `owner_violation_count` | `uint32_t` | `XM_LOG_REGISTER` · `XM_LOG_SEND` · 이 헤더의 설정 함수를 `Control_Setup()` · `Control_Loop()` 밖(다른 태스크 · 인터럽트)에서 부른 횟수 (구조체를 등록한 뒤라면 `XM_SetUsbCustomMeta()` 도 셉니다) |
| `profile_change_blocked_count` | `uint32_t` | 등록 뒤 `XM_USB_SetHostProfile()` 를 불러 무시된 횟수 |
| `admission_reject_count` | `uint32_t` | `XM_SendUsbDataWithId()` · `XM_SendUsbDebugMessage()` 등이 송신량 한도(한 번에 보내기에 너무 큰 것 포함)로 거절된 횟수 (구조체를 등록했을 때만 셉니다. 인자가 함수의 최대 길이를 넘어 거절된 호출은 세지 않습니다) |
| `reserved_id_reject_count` | `uint32_t` | 시스템이 쓰는 모듈 ID 나 등록한 구조체의 모듈 ID 로 보내려다 거절된 횟수 + `XM_SetUsbStreamModuleId()` 에 시스템이 쓰는 ID 를 주어 무시된 횟수 (등록한 구조체의 ID 는 정할 때가 아니라 그 ID 로 보낼 때마다 셉니다) |
| `seal_reject_count` | `uint32_t` | `Control_Setup()` 이 끝난 뒤 `XM_LOG_REGISTER` 를 부른 횟수 (구조체를 등록한 뒤라면 `XM_SetUsbCustomMeta()` 도 셉니다) |
| `admission_attempt_calls` | `uint32_t` | 위 송신이 송신량 한도 검사를 받은 횟수 (허용 + 거절, 구조체를 등록했을 때만 셉니다) |
| `x20_same_tick_suppressed_calls` | `uint32_t` | 제어 루프가 같은 1 ms 안에 다시 돌아 Total Data 송신을 건너뛴 횟수 (구조체를 등록했을 때만 셉니다) |

- `rate_gate_suppressed_calls` 는 매 루프 `XM_LOG_SEND` 를 부르면 계속 늘어나는 것이 정상입니다.
- Total Data 를 1 ms 마다 최대 한 번만 보내고 건너뛰는 동작은 보드에서 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

### 한도 상수

| 이름 | 값 | 뜻 |
|------|----|----|
| `XM_LOG_MAX_STRUCT_SIZE` | `1008U` | 등록할 수 있는 구조체 크기 상한(바이트) |
| `XM_LOG_MAX_FIELDS` | `90U` | 구조체 하나에 적을 수 있는 필드 수 상한 |
| `XM_LOG_MAX_ARRAY_LEN` | `255U` | 배열 필드 하나의 원소 수 상한 |
| `XM_LOG_MAX_TOKEN_LEN` | `11U` | 토큰 길이 상한(글자) — 토큰이 구조체 이름이 됩니다 |
| `XM_LOG_SLOT_NONE` | `0xFFU` | 등록되지 않은 토큰의 슬롯 값 |

---

### `XM_TYPE_TAG`

```c
XM_TYPE_TAG(x)
```

멤버 식의 필드 타입입니다. `XM_FIELD` · `XM_FIELD_ARRAY` 가 안에서 쓰므로 직접 쓸 일은 거의 없습니다. 지원 타입은 `float` · `double` · `bool` · `int` · `unsigned` · `int8_t` ~ `uint64_t` 입니다. 열거형 멤버는 크기가 같은 정수 타입으로 받습니다. 그 밖의 타입(`char` · 포인터 · 구조체 · 배열 등)은 컴파일 오류입니다.

---

## 지원 헤더 — `xm_api_logging_raw.h` (직접 호출 금지)

> ⚠️ **지원 대상이 아닙니다.** `XM_LOG_REGISTER` · `XM_LOG_SEND` 매크로가 안에서 부르는 함수의 원형입니다. 매크로를 쓰는 `.c` 파일은 이 헤더를 include 해야 컴파일됩니다. 아래 함수는 타입 · 크기 검사를 거치지 않아 다른 구조체를 보내도 컴파일러가 막지 못합니다. 직접 부르지 말고 항상 `XM_LOG_*` 매크로를 쓰세요.

### `XM_Log_RegisterSchemaRaw`

```c
bool XM_Log_RegisterSchemaRaw(uint8_t* slot_io, uint8_t module_id, const char* struct_name, uint16_t struct_size,
                              uint16_t nominal_rate_hz, const XM_FieldDesc_t* fields, uint8_t field_count);
```

`XM_LOG_REGISTER` 가 부릅니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `slot_io` | `uint8_t*` | 토큰의 슬롯 변수 — `XM_LOG_SLOT_NONE` 일 때만 등록하고, 성공하면 슬롯 번호를 씁니다 |
| `module_id` | `uint8_t` | 모듈 ID (`0xF0` ~ `0xFE`) |
| `struct_name` | `const char*` | 구조체 이름(토큰) |
| `struct_size` | `uint16_t` | 구조체 크기(바이트) |
| `nominal_rate_hz` | `uint16_t` | 보낼 주기(Hz) |
| `fields` | `const XM_FieldDesc_t*` | 필드 설명 배열 |
| `field_count` | `uint8_t` | 필드 수 |

**반환값**: `bool` — `true`: 등록됨. `false`: 이미 등록된 슬롯이거나 등록 실패 — `*slot_io` 는 바뀌지 않습니다.

---

### `XM_Log_SendRaw`

```c
bool XM_Log_SendRaw(uint8_t slot, const void* data, uint16_t size);
```

`XM_LOG_SEND` 가 부릅니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `slot` | `uint8_t` | 등록된 슬롯 번호 |
| `data` | `const void*` | 보낼 구조체 |
| `size` | `uint16_t` | 구조체 크기(바이트) |

**반환값**: `bool` — `true`: 보냄. `false`: 등록 안 된 슬롯, `size` 가 그 슬롯의 구조체 크기와 다름, `data` 가 `NULL`, 등록 주기보다 일찍 부름, USB 장치가 준비되지 않음, 송신 버퍼가 가득 참, `Control_Setup()` · `Control_Loop()` 밖(다른 태스크 · 인터럽트)에서 부름. 송신 버퍼가 가득 차 `false` 가 나와도 다음 송신은 등록한 주기가 지난 뒤에 나갑니다.

---

## 내부 전용 (직접 쓰지 마세요)

이름 끝이 `_` 인 매크로(`XM_LOG_REQUIRE_` · `XM_FIELD_TYPE_NONE_` · `XM_TYPE_TAG_` · `XM_LOG_MEMBER_` · `XM_LOG_ARRLEN_` · `XM_LOG_IS_ARRAY_` · `XM_LOG_FIELD_COUNT_`)는 다른 매크로가 쓰는 보조 정의입니다. 사용자 코드에서 쓰지 마세요.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| 구조체 로깅 (`xm_api_logging.h` · `xm_api_logging_raw.h`) | ❌ 없음 | 🟢 전용 |
| 기존 방식 (`XM_SetUsbCustomMeta` + `XM_SendUsbDataWithId`) | ✅ | ✅ |

---

## 관련 문서

- [10. 구조체 로깅 (개념)](../10-struct-logging.md) — 동작 원리, 한도, 흔한 실수
- [`xm_api_usb.h` — USB-CDC 실시간 스트리밍](xm-api-usb.md) — `XM_SendUsbDataWithId` · `XM_SetUsbCustomMeta` 등 기존 USB-CDC 함수
- [PC 에서 데이터 받기 — `xm10` 도구](../../getting-started/04-pc-data-tool.md)
