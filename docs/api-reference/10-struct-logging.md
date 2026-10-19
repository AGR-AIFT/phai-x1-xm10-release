# API Reference: 구조체 로깅

> 📌 **이 페이지를 읽고 나면**: 직접 만든 구조체를 필드 이름 · 단위 · 타입과 함께 PC 로 보낼 수 있습니다.
> ⏱️ 예상 학습 시간: 20분
> 🧰 사전 지식: [Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) CDC 스트리밍
> 🎯 핵심 매크로: `XM_LOG_DECLARE` / `XM_LOG_FIELDS` / `XM_LOG_REGISTER` / `XM_LOG_SEND`
>
> 🟢 **Rev 2.0 전용** *(v2.9.0 신규)*: Rev 1.1 SDK 에는 `xm_api_logging.h` 가 없습니다. Rev 1.1 에서는 [Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) 방식(`XM_SetUsbCustomMeta` + `XM_SendUsbDataWithId`)을 쓰세요.
>
> ⚠️ **USB-CDC 단일 점유**: **PC 프로그램은 한 번에 하나만 연결하세요.** PhAI Studio · `xm10` 도구 · 시리얼 터미널 (PuTTY/RealTerm 등) 을 같은 COM 포트로 동시에 열면 포트 충돌로 데이터가 손실됩니다.

`xm_api_logging.h` 에 정의된 **구조체 로깅 API** 안내입니다.
직접 만든 C 구조체를 한 번 등록해 두면, 필드 이름 · 단위 · 타입이 값과 함께 PC 로 갑니다. (보드 확인 전) <!-- board-check -->
`float` 만 보내던 Ex.09 방식과 달리 정수 · `bool` · `double` · 배열 멤버도 쓸 수 있습니다.

매크로 · 함수 하나하나의 시그니처와 반환값은 [`xm_api_logging.h` 레퍼런스](ref/xm-api-logging.md) 에 있습니다. 이 페이지는 **어떻게 쓰는지**를 다룹니다.

> ℹ️ **보드 확인 안내**: 구조체를 PC 로 보내 `xm10` 도구에서 보는 전체 흐름은 아직 보드에서 확인하지 못했습니다. PC 에 보이는 모습을 설명하는 문장에는 '(보드 확인 전)' 을 붙였습니다. <!-- board-check -->

-----

## 1. 동작 원리

구조체 로깅은 **등록은 한 번, 전송은 매 루프**입니다.

1. **등록** — `Control_Setup()` 에서 구조체와 필드 설명(이름 · 단위 · 배율)을 한 번 등록합니다. 모듈 ID 와 보낼 주기도 이때 정합니다.
2. **전송** — `Control_Loop()` 에서 값을 채워 `XM_LOG_SEND()` 를 부릅니다. 매 루프 불러도 등록한 주기로만 나가고, 주기가 되기 전에는 아무것도 보내지 않고 `false` 를 돌려줍니다.
3. **해석** — 보드는 값과 함께 필드 이름 · 단위 · 타입도 PC 로 보냅니다. `xm10` 도구가 그 정보로 값을 풀어 탭 · 그래프 · CSV 로 보여 줍니다. (보드 확인 전) <!-- board-check -->

### 기존 방식과 무엇이 다른가

| 항목 | 기존 방식 ([Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/)) | 구조체 로깅 |
|------|------|------|
| 쓰는 것 | `XM_SetUsbCustomMeta()` + `XM_SendUsbDataWithId()` | `XM_LOG_DECLARE` · `XM_LOG_FIELDS` · `XM_LOG_REGISTER` · `XM_LOG_SEND` |
| 값의 타입 | 타입을 보내지 않아 PC 는 값을 전부 `float` 로 읽음 | 멤버의 C 타입(`float` · `double` · `bool` · 정수 · 배열)을 그대로 씀 |
| 이름 · 단위 | JSON 문자열을 따로 적고, 구조체 필드 순서와 직접 맞춤 | 필드 설명(`XM_FIELD`)에 이름 · 단위 · 배율을 한 번 적음 |
| 보내는 시점 | 부른 때마다 나감 — 주기를 직접 만듦 | 등록한 주기로만 나감 — 매 루프 불러도 됨 |
| 한 번에 보내는 크기 | 1,020바이트까지 | 구조체 1,008바이트까지 (4 의 배수) |
| 이름을 붙일 수 있는 구조체 | 연결당 하나 | 4개까지, 구조체마다 모듈 ID 하나 |
| 규칙을 어기면 | 열이 밀리거나 이상한 숫자가 됨 | 컴파일 오류 또는 등록 `false` |
| 보드 | Rev 1.1 · Rev 2.0 | 🟢 **Rev 2.0 전용** |

### 이럴 때 쓰세요

- 정수 · `bool` · 배열이 섞인 구조체를 이름 · 단위와 함께 PC 에서 보고 싶을 때
- 구조체를 나눠 서로 다른 주기로 보내고 싶을 때 (예: 빠른 센서 값은 1 kHz, 느린 상태 값은 10 Hz)
- 구조체 필드에 이름 · 단위를 붙여서 기록을 남기고 싶을 때

### 이럴 때는 다른 방법

- `float` 몇 개만 빠르게 확인하려면 `XM_SendUsbDataWithId()` 를 쓰세요. → [05. USB 시리얼 통신](05-usb-connectivity.md)
- 문자열을 터미널로 보려면 `XM_SendUsbDebugMessage()` 를 쓰세요. → [05. USB 시리얼 통신](05-usb-connectivity.md)
- Total Data(`0x20`) 안의 28바이트 슬롯에 값 몇 개를 얹으려면 `XM_UserCustom_*` 를 쓰세요. → [`xm_api_user_custom.h`](ref/xm-api-user-custom.md)
- PC 가 아니라 USB 메모리에 저장하려면 → [06. USB 메모리 저장](06-usb-data-logging.md)

-----

## 2. 시작하기 — 구조체 하나 보내기

[Ex.44](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) 를 줄여 만든 예입니다. 구조체 하나를 100 Hz 로 보냅니다.

```c
#include "xm_api.h"
#include "xm_api_logging_raw.h"   // XM_LOG_* 매크로를 쓰는 .c 파일은 꼭 include

typedef struct {
    uint32_t tick_ms;             // 보드 시간 [ms]
    int16_t  ramp_x10;            // 0.1 % 단위 정수 (0 ~ 999)
    uint16_t adc_mv[2];           // 확장 ADC 1 ~ 2 [mV]
    bool     button[3];           // 버튼 1 ~ 3 이 눌려 있는가
    uint8_t  pad[3];              // 크기를 4 의 배수로 맞추는 빈 자리 (필드로 적지 않음)
} SensorLog_t;                                                        // ① 16 B

XM_LOG_DECLARE(SensorLog, SensorLog_t);                               // ② 구조체에 이름(토큰) 붙이기

XM_LOG_FIELDS(SensorLog) = {                                          // ③ 보낼 필드 — 빈 자리(pad)는 적지 않음
    XM_FIELD(SensorLog, tick_ms, "tick", "ms", 1.0f),
    XM_FIELD(SensorLog, ramp_x10, "ramp", "%", 0.1f),                 //    물리값 = 보낸 값 × 0.1 (배율)
    XM_FIELD_ARRAY(SensorLog, adc_mv, "adc", "mV", 1.0f),
    XM_FIELD_ARRAY(SensorLog, button, "button", "", 1.0f),
};

void Control_Setup(void)
{
    if (!XM_LOG_REGISTER(SensorLog, 0xF0, 100)) {                     // ④ 모듈 ID · 보낼 주기 [Hz]
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);                 //    등록 실패는 LED 로 알립니다
    }
}

void Control_Loop(void)
{
    SensorLog_t s = { 0 };                                            // 빈 자리(pad)까지 0 으로
    s.tick_ms   = XM_GetTick();
    s.ramp_x10  = (int16_t)(s.tick_ms % 1000U);                       // 1 초마다 0 → 999 를 반복
    s.adc_mv[0] = XM_AnalogReadMillivolts(XM_EXT_ADC_1);
    s.adc_mv[1] = XM_AnalogReadMillivolts(XM_EXT_ADC_2);
    s.button[0] = (XM_GetButtonState(XM_BTN_1) == XM_PRESSED);
    s.button[1] = (XM_GetButtonState(XM_BTN_2) == XM_PRESSED);
    s.button[2] = (XM_GetButtonState(XM_BTN_3) == XM_PRESSED);

    (void)XM_LOG_SEND(SensorLog, &s);                                 // ⑤ 매 루프 불러도 100 Hz 로만 나갑니다
}
```

- ① 구조체 크기는 **4 의 배수**이고 1,008바이트 이하여야 합니다. 남는 자리는 `uint8_t pad[]` 로 채우고 `XM_FIELD` 로 적지 않습니다. 필드로 적지 않은 바이트도 데이터에는 실려 갑니다.
- ② `XM_LOG_DECLARE(토큰, 구조체)` 로 구조체에 짧은 이름(영문 · 숫자 · `_`, 11자 이하)을 붙입니다. 이 토큰이 구조체 이름이 됩니다.
- ③ 필드 한 줄이 `XM_FIELD(토큰, 멤버, "필드 이름", "단위", 배율)` 입니다. 배열 멤버는 `XM_FIELD_ARRAY` 를 씁니다. 타입 · 위치 · 배열 길이는 멤버에서 자동으로 정해지므로 적지 않습니다.
- ④ `XM_LOG_REGISTER(토큰, 모듈 ID, 주기)` 는 `Control_Setup()` 안에서만 부릅니다. 모듈 ID(`0xF0` ~ `0xFE`)는 구조체마다 하나씩 정하는 번호이고, 주기는 Hz 입니다. 결과(`true` / `false`)를 확인하세요. `Control_Setup()` 에서 PC 로 보낸 메시지는 USB 가 아직 준비되지 않아 보통 나가지 않으니, 실패는 LED 로 알립니다.
- ⑤ `XM_LOG_SEND(토큰, 포인터)` 는 `Control_Loop()` 안에서 부릅니다. 매 루프 불러도 등록한 주기로만 나가므로, 주기가 되기 전의 `false` 는 정상입니다. 구조체는 호출 안에서 복사되므로 지역 변수여도 됩니다.
- 구조체가 더 있으면 `XM_LOG_DECLARE` · `XM_LOG_FIELDS` · `XM_LOG_REGISTER` · `XM_LOG_SEND` 를 토큰마다 한 세트씩 더 씁니다. 4개까지 등록할 수 있고, 모듈 ID 는 서로 달라야 합니다.
- 예제 파일에서는 숫자 대신 `#define` 상수를 씁니다 (`SENSOR_LOG_MODULE_ID` · `SENSOR_LOG_RATE_HZ` 등).

### PC 에서 확인하기

1. 빌드 → 업로드 → USB-C 케이블을 PC 에 연결합니다.
2. [`xm10` 도구](../getting-started/04-pc-data-tool.md) 로 보드의 COM 포트를 엽니다 (`python xm10.py recv` → Port 선택 → Connect).
3. 모듈 ID `0xF0` 탭이 생기고 `tick` · `ramp` · `adc[0]` · `adc[1]` · `button[0]` ~ `button[2]` 열이 보이는지 확인합니다. (보드 확인 전) <!-- board-check -->
4. 버튼을 누르고, 확장 포트의 ADC 핀에 가변저항 등을 연결해 값이 따라 움직이는지 봅니다. (보드 확인 전) <!-- board-check -->

-----

## 3. 필드 타입

필드 타입은 적지 않습니다. `XM_FIELD` · `XM_FIELD_ARRAY` 가 **멤버의 C 타입**에서 자동으로 정합니다.

| 멤버의 C 타입 | 필드 타입 | 크기 | 종류 |
|------|------|------|------|
| `float` | `XM_FIELD_F32` | 4 B | 실수 |
| `double` | `XM_FIELD_F64` | 8 B | 실수 |
| `int8_t` | `XM_FIELD_I8` | 1 B | 정수 |
| `uint8_t` | `XM_FIELD_U8` | 1 B | 정수 |
| `int16_t` | `XM_FIELD_I16` | 2 B | 정수 |
| `uint16_t` | `XM_FIELD_U16` | 2 B | 정수 |
| `int32_t` · `int` | `XM_FIELD_I32` | 4 B | 정수 |
| `uint32_t` · `unsigned` | `XM_FIELD_U32` | 4 B | 정수 |
| `int64_t` | `XM_FIELD_I64` | 8 B | 정수 |
| `uint64_t` | `XM_FIELD_U64` | 8 B | 정수 |
| `bool` | `XM_FIELD_BOOL` | 1 B | 참 / 거짓 |

- 열거형(`enum`) 멤버는 크기가 같은 정수 타입으로 받습니다.
- 배열은 `XM_FIELD_ARRAY` 로 적습니다. 원소 타입은 위 표와 같습니다.
- 멤버 사이에 컴파일러가 넣는 빈 자리가 있어도 됩니다. 필드 위치는 `XM_FIELD` 가 계산해 적습니다.
- `char` · 포인터 · 구조체 멤버는 쓸 수 없습니다. `XM_FIELD` 에 적으면 컴파일 오류(`unsupported member type`)가 납니다. `char` 대신 `uint8_t` 나 `int8_t` 를 쓰세요.

-----

## 4. 한도

| 항목 | 한도 | 어기면 |
|------|------|--------|
| 구조체 크기 | 4 의 배수, 1,008 B 이하 (`XM_LOG_MAX_STRUCT_SIZE`) | `XM_LOG_DECLARE` 줄에서 컴파일 오류 |
| 구조체 이름(토큰) | 영문 · 숫자 · `_` 11자 이하 (`XM_LOG_MAX_TOKEN_LEN`) — 한글처럼 ASCII 가 아닌 글자는 불가 | 11자를 넘으면 `XM_LOG_DECLARE` 줄에서 컴파일 오류, ASCII 가 아니면 컴파일 오류 또는 `XM_LOG_REGISTER` 가 `false` |
| 필드 수 | 구조체 하나에 1 ~ 90개 (`XM_LOG_MAX_FIELDS`) — 배열은 한 줄이 한 필드 | `XM_LOG_REGISTER` 줄에서 컴파일 오류 |
| 배열 원소 수 | 필드 하나에 1 ~ 255개 (`XM_LOG_MAX_ARRAY_LEN`) | `XM_FIELD_ARRAY` 줄에서 컴파일 오류 |
| 필드 이름 | 영문 · 숫자 · 기호(ASCII) 15자까지 — 한글 · `°` 처럼 ASCII 가 아닌 글자는 불가 | `XM_LOG_REGISTER` 가 `false` |
| 단위 | 영문 · 숫자 · 기호(ASCII) 7자까지, 없으면 `""` — `°` 같은 글자는 불가 | `XM_LOG_REGISTER` 가 `false` |
| 배율(scale) | 0 이 아닌 숫자 (0 · 무한대 · NaN 불가) | `XM_LOG_REGISTER` 가 `false` |
| 등록할 수 있는 구조체 | 4개까지 | 5번째 등록이 `false` |
| 모듈 ID | `0xF0` ~ `0xFE`, 구조체마다 하나 (겹치면 불가) | `XM_LOG_REGISTER` 가 `false` |
| 보낼 주기 | 1 ~ 1,000 Hz — 1,000 Hz 보다 자주는 나가지 않음 | 0 이면 `XM_LOG_REGISTER` 가 `false` |
| 보내는 양 | 구조체가 크고, 주기가 높고, 구조체가 많을수록 늘어남 | 너무 많으면 `XM_LOG_REGISTER` 가 `false` |

- 보낼 주기는 1 ms 단위로 **올려** 잡습니다. 1,000 의 약수가 아니면 선언한 값보다 덜 자주 나갑니다 (예: 300 Hz → 250 Hz, 600 Hz → 500 Hz).
- 보낼 양이 USB 로 보낼 수 있는 양을 넘으면 등록이 거절됩니다. 이때는 주기를 낮추거나 구조체를 줄이세요. 먼저 등록한 구조체와 합쳐서 셉니다.
- 어느 크기 · 주기까지 등록되는지는 보드에서 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

-----

## 5. 어디서 부르나

| 함수 · 매크로 | 부르는 곳 | 지키지 않으면 |
|------|------|------|
| `XM_LOG_DECLARE` · `XM_LOG_FIELDS` (안에 `XM_FIELD` · `XM_FIELD_ARRAY`) | `.c` 파일 위쪽, 함수 밖 | 컴파일 오류 |
| `XM_USB_SetHostProfile()` (선택) | `Control_Setup()` — 첫 `XM_LOG_REGISTER` 보다 먼저 | 등록 뒤에는 호출이 무시됨 |
| `XM_SetUsbCustomMeta()` (선택) | `Control_Setup()` — 첫 `XM_LOG_REGISTER` 보다 먼저 | 등록한 뒤 `Control_Setup()` 밖에서는 `false` |
| `XM_Log_SetMinStallMs()` (선택) | `Control_Setup()` — 첫 `XM_LOG_REGISTER` 보다 먼저 | 등록한 뒤나 `Control_Setup()` 밖에서는 `false` |
| `XM_Log_EnableLinkHealth()` (선택) | `Control_Setup()` | `Control_Setup()` 밖에서는 `false` |
| `XM_LOG_REGISTER` | `Control_Setup()` 안에서만 | `false` |
| `XM_LOG_SEND` | `Control_Loop()` 안 | `false` |
| `XM_Log_GetStats()` | `Control_Loop()` | — |

- 네 매크로(`XM_LOG_DECLARE` · `XM_LOG_FIELDS` · `XM_LOG_REGISTER` · `XM_LOG_SEND`)는 같은 `.c` 파일 안에서 씁니다. 매크로는 C 파일 전용입니다.
- 인터럽트 · 직접 만든 백그라운드 태스크(`XM_Task_*`)에서는 부르지 마세요 (`false`).

-----

## 6. PC 에서 보기 — `xm10` 도구

> PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요.

SDK 에 함께 들어 있는 `xm10` 도구(`pc-data-tool` 폴더)로 보세요. 예전 릴리스의 도구로는 일부 값이 보이지 않거나 틀리게 보일 수 있습니다.
사용법과 저장 · CSV 내보내기는 [PC 에서 데이터 받기](../getting-started/04-pc-data-tool.md) 를 보세요.

`xm10` 도구는 구조체를 이렇게 보여 줍니다.

- 모듈 ID 마다 탭이 하나씩 열리고, 탭 제목은 `0xF0 · SensorLog` 처럼 모듈 ID 와 구조체 이름(토큰)입니다. (보드 확인 전) <!-- board-check -->
- 그래프와 CSV 의 열 이름은 `XM_FIELD` 에 적은 필드 이름입니다. 배열은 `adc[0]`, `adc[1]` … 처럼 원소마다 열이 생깁니다. (보드 확인 전) <!-- board-check -->
- 값은 `저장값 × 배율` 로 보입니다. `xm10 export` 로 만든 CSV 에서는 `bool` 이 `1` / `0` 으로 적히고, 배율이 1 인 정수는 정수 그대로 적힙니다. 실시간으로 저장하는 CSV 는 모든 값을 소수 여섯 자리로 적습니다. (보드 확인 전) <!-- board-check -->
- 보드는 필드 정보를 몇 초 간격으로 계속 다시 보냅니다. 그래서 PC 프로그램이 포트를 열면 몇 초 안에 필드 정보를 받고, USB 케이블을 다시 꽂을 필요가 없습니다. 포트를 연 직후 몇 초 동안은 필드 이름이 아직 안 보일 수 있습니다. (설계 목표, 보드 확인 전) <!-- board-check -->

-----

## 7. 함께 쓰는 USB 함수에 생기는 변화

구조체를 하나라도 등록하면 다른 USB 함수에 아래 변화가 생깁니다. 하나도 등록하지 않으면 이 변화는 생기지 않습니다.

| 함수 | 구조체를 등록하면 |
|------|------|
| `XM_SendUsbDataWithId()` | 등록한 모듈 ID 로는 보낼 수 없습니다 (`false`). 다른 ID 로는 보낼 수 있지만, 짧은 시간에 너무 많이 보내거나 한 번에 보내기에 너무 크면 `false` 입니다. 너무 큰 것은 기다려도 나가지 않으니 구조체 로깅으로 보내세요. |
| `XM_SendUsbDebugMessage()` | 같은 보내는 양 한도를 받습니다 (`XM_SendUsbDataWithId()` 와 합쳐서 셉니다). 긴 메시지는 줄이거나 시간 간격을 두고 나눠 보내세요. |
| `XM_USB_SetHostProfile()` | 첫 `XM_LOG_REGISTER` 보다 먼저 부르세요. 구조체를 하나라도 등록한 뒤에는 호출이 무시됩니다. |
| `XM_SetUsbCustomMeta()` | `Control_Setup()` 안에서만 쓸 수 있습니다. 그 뒤에 부르면 `false` 입니다. |
| `XM_Log_SetMinStallMs()` | 첫 `XM_LOG_REGISTER` 보다 먼저 부르세요. 등록한 뒤에는 `false` 입니다. |

- 일반 시리얼 터미널로 텍스트만 볼 때는 구조체를 등록하지 마세요. `XM_USB_HOST_TERMINAL` 프로파일이어도 구조체 데이터와 필드 정보는 그대로 나가 터미널 화면에 깨진 글자로 섞여 보일 수 있습니다.
- Total Data(`0x20`)는 구조체를 등록하면 1 ms 마다 최대 한 번만 보냅니다. 제어 루프가 1 ms 를 넘겨 늦어지면 그 구간에서는 `xm_loop_count` 값이 이어지지 않고 건너뜁니다. 보드가 보내는 간격과 PC 에 도착하는 시각은 1 ms 간격으로 고르지 않을 수 있습니다. (설계 목표, 보드 확인 전) <!-- board-check -->

-----

## 8. 데이터가 밀릴 때

- 송신 버퍼가 가득 차면 보내려던 데이터는 버려지고 `XM_LOG_SEND()` 가 `false` 를 돌려줍니다. 보드는 기다리지 않습니다.
- 버퍼가 가득 차 `false` 가 나와도 다음 송신은 등록한 주기가 지난 뒤에 나갑니다. 바로 다시 불러도 보내지 않습니다.
- 버려진 데이터는 `xm10` 도구 통계줄의 `SEQ↓`(잃은 프레임 수)로 보입니다. (보드 확인 전) <!-- board-check -->
- PC 프로그램이 잠시 멈춰도 보드가 데이터를 빠뜨리지 않고 담아 두는 것이 설계 목표입니다 (기본 50 ms). 이 목표를 지킬 수 없을 만큼 많이 보내는 구조체는 등록이 `false` 로 거절됩니다. 허용하는 멈춤 시간은 `XM_Log_SetMinStallMs()` 로 바꿀 수 있습니다. 50 ms 는 아직 보드에서 재 보지 않은 초기값이라 보장이 아니라 목표입니다. (설계 목표) <!-- board-check -->
- `XM_Log_EnableLinkHealth(true)` 로 켜면 보드가 링크 상태(송신 실패 수 · 버퍼 사용률 등)를 1초마다 PC 로 보냅니다. 기본은 보내지 않으며, 구조체를 하나 이상 등록했을 때만 보냅니다. `xm10` 도구는 링크 상태 값을 따로 풀어 보여 주지 않습니다. (설계 목표, 보드 확인 전) <!-- board-check -->
- 보내지 못하는 이유를 찾으려면 `XM_Log_GetStats()` 로 카운터를 복사해 디버거로 보세요. [Ex.44](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) 의 `g_log_stats` 가 그 예입니다.

---

## 관련 예제

| 예제 | 난이도 | 구조체 로깅 활용 |
|------|--------|---------|
| [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) | 중급 | 기존 방식 — `float` 배열 + 채널 이름 JSON |
| [44_CDC_Struct_Logging](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/44_CDC_Struct_Logging/) 🟢 **Rev 2.0 전용** | 중급 | 구조체 둘을 서로 다른 주기로 이름 · 단위 · 타입과 함께 보내기 (버튼 · ADC 값, 선택으로 H10 값) |

> ℹ️ Ex.44 는 아직 보드에서 확인되지 않은 예제입니다. (보드 확인 전) <!-- board-check -->

H10 보행 데이터를 USB 메모리에 저장하려면 [Ex.34 USB Memory Gait Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/) 를 보세요.

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 빌드 오류 `implicit declaration of function` (`XM_LOG_REGISTER` · `XM_LOG_SEND` 줄) | `xm_api_logging_raw.h` 를 include 하지 않음 — `xm_api.h` 는 이 헤더를 포함하지 않습니다 | `.c` 맨 위에 `#include "xm_api_logging_raw.h"` 를 더합니다 |
| `XM_LOG_DECLARE` 줄에서 컴파일 오류 `add padding` | 구조체 크기가 4 의 배수가 아님 | 끝에 `uint8_t pad[]` 를 더해 크기를 맞춥니다 (`XM_FIELD` 로 적지 않음) |
| 컴파일 오류 `unsupported member type` | `char` · 포인터 · 구조체 멤버를 `XM_FIELD` 에 적음 | 지원 타입(3장)으로 바꿉니다. 배열은 `XM_FIELD_ARRAY` |
| `XM_LOG_REGISTER` 가 `false` (LED 3 이 빠르게 깜빡임) | 이름 · 단위가 길거나, 토큰 · 이름 · 단위가 ASCII 가 아님(한글 · `°` 등), 주기가 0, 같은 모듈 ID · 같은 토큰, 구조체 5개 이상, 배율이 0, 필드 이름 중복, 보내는 양이 너무 많음, `Control_Setup()` 밖에서 부름 | 하나씩 확인합니다. 보내는 양이 문제면 주기를 낮추거나 구조체를 줄입니다 |
| `XM_LOG_SEND` 가 자주 `false` | 등록한 주기가 아직 안 됨(정상), USB 준비 전, 송신 버퍼가 잠시 가득 참 | 같은 값을 다시 보내려 하지 말고 다음 루프에서 새 값을 보냅니다 |
| 같은 모듈 ID 로 `XM_SendUsbDataWithId()` 가 `false` | 등록한 모듈 ID 는 그 구조체 전용 | 남은 ID(`0xF0` ~ `0xFE`)를 씁니다 |
| `XM_USB_SetHostProfile()` · `XM_Log_SetMinStallMs()` 가 먹히지 않음 | 첫 등록 뒤에는 바뀌지 않음 | 첫 `XM_LOG_REGISTER` 보다 먼저 부릅니다 |
| 필드 이름이나 값이 엉뚱하게 나옴 | `XM_LOG_FIELDS` 안에 다른 토큰의 `XM_FIELD` 를 적음 — 컴파일러가 이 규칙은 검사하지 못합니다 | 같은 토큰의 `XM_FIELD` · `XM_FIELD_ARRAY` 만 씁니다 |
| 시리얼 터미널에 깨진 글자 | `XM_USB_HOST_TERMINAL` 이어도 구조체 데이터는 나갑니다 | 텍스트만 볼 때는 구조체를 등록하지 마세요 |
| 탭이 생기지 않거나 값이 이상함 | 다른 PC 프로그램이 같은 포트를 열었거나, 예전 릴리스의 `xm10` 도구 | 다른 프로그램을 모두 닫고 SDK 에 들어 있는 `xm10` 도구로 다시 엽니다 |
| CSV 열 이름이 `ch0`, `ch1` … 이고, 어느 행부터 값 칸이 비고 마지막 칸에 16진수가 적힘 | 필드 정보가 첫 데이터 행보다 늦게 도착함 — 실시간 CSV 는 열을 첫 행에서 정하므로, 그 뒤 필드 정보가 도착하면 이후 행은 값 대신 받은 바이트를 16진수로 적습니다 | `.xmlog` 에서 다시 뽑습니다: `python xm10.py export <파일>.xmlog --csv out/` |

- 포트를 연 직후 필드 정보가 값보다 늦게 도착하는 구간이 실제 보드에서 얼마나 생기는지는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

## 관련 문서

- [`xm_api_logging.h` 레퍼런스](ref/xm-api-logging.md) — 매크로 · 함수 · 타입 · 한도 상수 전체
- [05. USB 시리얼 통신](05-usb-connectivity.md) — 기존 방식(`XM_SendUsbDataWithId`)과 텍스트 전송
- [PC 에서 데이터 받기 — `xm10` 도구](../getting-started/04-pc-data-tool.md)
- [06. USB 메모리 저장](06-usb-data-logging.md) — PC 대신 USB 메모리에 저장
