# Ex.44 — CDC Struct Logging (구조체를 이름 · 단위 · 타입과 함께 PC 로 보내기)

> 🎯 **학습 목표**:
> - 직접 만든 구조체를 한 번 등록해서, 필드 이름 · 단위 · 타입과 함께 PC 로 보냅니다 (`XM_LOG_DECLARE` · `XM_LOG_FIELDS` · `XM_LOG_REGISTER` · `XM_LOG_SEND`).
> - `float` 말고도 정수 · `bool` · 배열과 배율(scale)을 그대로 보냅니다.
> - 구조체마다 보내는 주기와 모듈 ID 를 따로 정합니다.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐⭐ | 🟢 **Rev 2.0 전용** (구조체 로깅 API)
> 🧰 사전 예제: [Ex.09 CDC Stream](../09_CDC_Stream/) | 📚 관련 docs: [구조체 로깅](../../docs/api-reference/10-struct-logging.md) · [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md) · [xm10 도구 안내](../../docs/getting-started/04-pc-data-tool.md)

> 🟢 **Rev 2.0 전용** — 구조체 로깅 API (`xm_api_logging.h`) 는 Rev 2.0 SDK 에만 있습니다. Rev 1.1 SDK 에는 이 예제가 들어 있지 않습니다. 본인 보드의 리비전은 [보드 리비전 비교](../../docs/hardware/README.md#보드-리비전-비교)에서 확인하세요.

---

## ⚠️ USB-CDC 단독 점유

- PC 프로그램은 **한 번에 하나만** 연결하세요 (PhAI Studio · `xm10` 도구 · 시리얼 터미널 중 하나).
- **SDK 에 함께 들어 있는 `xm10` 도구**(`pc-data-tool` 폴더)로 보세요. 예전 릴리스의 도구로는 일부 값이 보이지 않거나 틀리게 보일 수 있습니다.
- PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

`xm10` 도구로 포트를 열면 구조체 두 개가 각자의 탭으로 들어옵니다. 탭 제목과 CSV 열 이름은 코드에 적은 이름입니다. (보드 확인 전) <!-- board-check -->
기본 설정은 H10 에서 값을 받지 않으니 보드와 PC 만 연결하면 됩니다. 보드 전원은 KIT H10 본체에서 들어오므로 H10 본체의 전원은 켜 둡니다.

| 모듈 ID | 구조체 | 주기 | 필드 |
|:---:|------|:---:|------|
| **0xF0** | `SensorLog` (기본 — 보드만) | 100 Hz | `tick` (ms) · `wave[2]` (사인 · 코사인) · `ramp` (%, 배율 0.1) · `adc[4]` (mV) · `button[3]` |
| **0xF0** | `SensorLog` (`USE_H10` = 1) | 1 kHz | `tick` (ms) · `hip_angle[2]` (deg) · `hip_gyro[2]` (deg/s) · `hip_torque[2]` (Nm) · `h10_connected` |
| **0xF1** | `StatusLog` (두 경우 모두) | 10 Hz | `uptime` (s) · `clicks[3]` |

- 배열은 `adc[0]`, `adc[1]` … 처럼 원소마다 열이 생깁니다. (보드 확인 전) <!-- board-check -->
- H10 값의 `[0]` 은 왼쪽, `[1]` 은 오른쪽입니다.
- `ramp` 는 0.1 단위 정수(0 ~ 1000)로 보내고, 배율 0.1 을 함께 등록해서 PC 에는 0.0 ~ 100.0 으로 보입니다. (보드 확인 전) <!-- board-check -->
- 버튼을 누르고 있으면 `button[]` 이 1, 짧게 누를 때마다 `clicks[]` 가 1 씩 늘어납니다.

| LED | 뜻 |
|:---:|------|
| LED 1 켜짐 | USB 가 준비되어 보낼 수 있습니다 |
| LED 2 켜짐 | H10 이 연결되었습니다 (`USE_H10` = 1) |
| LED 3 빠른 깜빡임 | 구조체를 등록하지 못했습니다. 아래 '흔한 실수' 를 보세요 |

> ℹ️ 위 표와 목록의 LED · 버튼 · 값 동작은 예제 코드가 정한 것입니다. 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **구조체 로깅** — 구조체를 한 번 등록하면 필드 이름 · 단위 · 타입이 PC 로 함께 갑니다. Ex.09 처럼 `float` 만 쓰거나 JSON 으로 이름을 적을 필요가 없습니다.
- **두 헤더** — `#include "xm_api.h"` 와 `#include "xm_api_logging_raw.h"`. 뒤의 헤더는 `XM_LOG_*` 매크로를 쓰는 `.c` 파일에 꼭 필요합니다. 그 안의 함수는 직접 부르지 마세요(지원 대상 아님) — 늘 `XM_LOG_*` 매크로를 씁니다.
- **지켜야 할 것**
  - 구조체 크기는 4 의 배수이고 1,008 B 이하입니다. 남는 자리는 필드로 등록하지 않는 `uint8_t pad[]` 로 채웁니다.
  - 구조체 이름(토큰)은 11 자 이하, 필드 이름은 15 자 · 단위는 7 자 이하의 영문입니다. 필드는 90 개, 배열은 원소 255 개까지입니다.
  - 멤버 타입은 `float` · `double` · `bool` · 정수(`int8_t` ~ `uint64_t`)와 그 배열입니다.
  - 모듈 ID 는 `0xF0` ~ `0xFE` 이고 구조체마다 하나씩, 구조체는 4 개까지 등록합니다. 등록한 ID 는 그 구조체만 씁니다.
- **어디서 부르나** — `XM_LOG_REGISTER` 는 `Control_Setup()` 에서만, `XM_LOG_SEND` 는 `Control_Loop()` 에서 부릅니다.
- **등록이 거절될 수 있습니다** — 주기를 올리거나 구조체를 키우면 PC 로 보내는 양이 늘어 등록이 거절될 수 있습니다. 거절되면 `XM_LOG_REGISTER` 가 `false` 를 돌려주고 등록 상태는 바뀌지 않습니다. `Control_Setup()` 에서는 보통 USB 가 아직 준비되지 않아 PC 로 메시지를 보낼 수 없으니, 등록 실패는 LED 로 알립니다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#include "xm_api.h"
#include "xm_api_logging_raw.h"   // XM_LOG_* 매크로 지원 헤더 — 꼭 include

typedef struct {
    uint32_t tick_ms;
    float    wave[2];
    int16_t  ramp_x10;            // 0.1 단위 정수
    uint16_t adc_mv[4];
    bool     button[3];
    uint8_t  pad[3];              // 크기를 4 의 배수로 (등록하지 않음)
} SensorLog_t;                                                        // ① 28 B

XM_LOG_DECLARE(SensorLog, SensorLog_t);                               // ② 이름 붙이기 — 탭 제목이 됩니다

XM_LOG_FIELDS(SensorLog) = {                                          // ③ 보낼 필드
    XM_FIELD(SensorLog, tick_ms, "tick", "ms", 1.0f),
    XM_FIELD_ARRAY(SensorLog, wave, "wave", "", 1.0f),
    XM_FIELD(SensorLog, ramp_x10, "ramp", "%", 0.1f),                 //    PC 에 보이는 값 = 보낸 값 × 0.1
    XM_FIELD_ARRAY(SensorLog, adc_mv, "adc", "mV", 1.0f),
    XM_FIELD_ARRAY(SensorLog, button, "button", "", 1.0f),
};

void Control_Setup(void)
{
    if (!XM_LOG_REGISTER(SensorLog, 0xF0, 100)) {                     // ④ 모듈 ID · 주기 [Hz]
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);                 //    등록 실패는 LED 로 (USB 준비 전)
    }
}

void Control_Loop(void)
{
    SensorLog_t s = { 0 };
    /* … s 를 채웁니다 … */
    (void)XM_LOG_SEND(SensorLog, &s);                                 // ⑤ 매 루프 불러도 100 Hz 로만 나갑니다
}
```

전체 코드: [`cdc_struct_logging.c`](cdc_struct_logging.c)

> 🧒 ② 타입 · 위치 · 배열 길이는 멤버에서 자동으로 정해집니다. 다른 구조체의 멤버를 적거나, 크기가 4 의 배수가 아니면 컴파일 오류가 납니다.
>
> 🧒 ⑤ `false` 는 '아직 주기가 안 됨 · USB 준비 전 · 보낼 자리가 잠시 없음' 입니다. 다시 보내려 하지 말고 다음 루프에 새 값을 보내세요.
>
> 🧒 예제 파일에서는 숫자 대신 `#define` 상수를 씁니다 (`SENSOR_LOG_MODULE_ID` · `SENSOR_LOG_RATE_HZ` 등).

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

아래 ✅ 는 코드가 의도한 동작이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

1. **PC 프로그램 준비** — `xm10` 도구([안내](../../docs/getting-started/04-pc-data-tool.md)) 하나만 열고, 시리얼 터미널과 PhAI Studio 는 모두 종료하세요.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **USB 연결 + 포트 열기** — XM10 을 USB-C 로 PC 에 연결하고, `xm10` 도구에서 포트를 골라 **Connect** → ✅ `0xF0` · `0xF1` 탭이 생깁니다 (탭 제목 `0xF0 · SensorLog` · `0xF1 · StatusLog`)
4. **버튼 1 ~ 3 을 누르고 있기** → ✅ `button[0~2]` 가 1 이 되고, 짧게 누르면 `StatusLog` 의 `clicks[]` 가 늘어납니다
5. **확장 포트 ADC 1 ~ 4 에 가변저항이나 FSR 연결** → ✅ `adc[]` 가 움직입니다 (아무것도 연결하지 않으면 잡음 값)
6. **변형 1 — H10 데이터**: 파일 위쪽의 `USE_H10` 을 `1` 로 바꾸고 H10 을 연결합니다. 고관절 각도 · 각속도 · 토크 추정값을 1 kHz 로 보냅니다. 값을 읽기만 하고 토크는 내보내지 않습니다.
7. **변형 2 — 필드 더하기**: 구조체에 멤버를 더하고 `XM_LOG_FIELDS` 에 한 줄 더 적습니다. 크기가 4 의 배수가 되도록 `pad` 크기를 고칩니다.
8. **변형 3 — 등록이 거절되는 경우 보기**: 필드 이름을 16 자 이상으로 바꿔 보세요. 컴파일은 되지만 `Control_Setup()` 의 등록이 거절되어 LED 3 이 빠르게 깜빡입니다. 디버거로 `g_sensor_log_ok` 를 보면 어느 구조체인지 알 수 있습니다.
9. **변형 4 — 카운터 보기**: 디버거 Live Expressions 에 `g_log_stats` 를 넣어 보세요. `rate_gate_suppressed_calls` 는 등록한 주기보다 일찍 부른 횟수라, 매 루프 부르는 이 예제에서는 계속 늘어나는 것이 정상입니다.

---

## 5️⃣ 다음 단계

- PC 에서 받아 저장하고 CSV 로 뽑기: [xm10 도구 안내](../../docs/getting-started/04-pc-data-tool.md)
- 한도와 등록이 거절되는 경우를 자세히: [구조체 로깅](../../docs/api-reference/10-struct-logging.md) · [`xm_api_logging.h` 레퍼런스](../../docs/api-reference/ref/xm-api-logging.md)
- H10 보행 데이터를 USB 메모리에 저장하기: [Ex.34 USB Memory Gait Log](../34_USB_Memory_Gait_Log/)
- 제어 알고리즘의 내부 값을 PC 로 보며 조정하기: [Ex.14 PD Realtime Control](../14_PD_Realtime_Control/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 빌드 오류 `implicit declaration of function` (매크로 줄) | `xm_api_logging_raw.h` include 누락 | `.c` 맨 위에 `#include "xm_api_logging_raw.h"` 를 더합니다 |
| `XM_LOG_DECLARE` 줄에서 `add padding` 오류 | 구조체 크기가 4 의 배수가 아님 | 끝에 `uint8_t pad[]` 를 더해 크기를 맞춥니다(필드로 등록하지 않음) |
| LED 3 이 빠르게 깜빡임 | 등록이 거절됨 — 이름 · 단위가 길거나 영문이 아님, 주기 0, 같은 모듈 ID, 구조체 5 개 이상, 배율이 0 이거나 숫자가 아님, 필드 이름이 겹침, 보내는 양이 너무 많음 | 이름 · 단위 길이와 ID 를 확인하고, 주기를 낮추거나 구조체를 줄입니다 |
| 탭이 생기지 않거나 값이 이상함 | 다른 PC 프로그램이 포트를 같이 열었거나, 예전 릴리스의 `xm10` 도구 | 다른 프로그램을 모두 닫고 SDK 에 들어 있는 `xm10` 도구로 다시 엽니다 |
| 포트를 연 직후 CSV 앞부분에 값 대신 16진수가 보임 | 필드 정보가 데이터보다 늦게 도착했습니다 | 이름과 값이 모두 맞는 CSV 는 `.xmlog` 에서 `export` 로 다시 뽑습니다 ([xm10 도구 안내](../../docs/getting-started/04-pc-data-tool.md)). (보드 확인 전) <!-- board-check --> |
| H10 값이 모두 0 | H10 이 연결되지 않음 (`h10_connected` 가 0) | H10 전원과 CAN-FD 연결을 확인합니다. 연결이 끊기면 마지막 값이 남습니다 |
| 같은 모듈 ID 로 `XM_SendUsbDataWithId()` 가 `false` | 등록한 모듈 ID 는 그 구조체만 씁니다 | 다른 ID(`0xF0` ~ `0xFE` 중 남은 것)를 씁니다 |
| 같은 구조체를 두 번 등록하려 하면 `false` | 같은 토큰은 한 번만 등록할 수 있습니다 | `Control_Setup()` 에서 한 번만 부릅니다 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)

> ℹ️ **아직 보드에서 확인되지 않은 예제입니다** (v2.9.0 시점). 구조체를 등록해 `xm10` 도구에 탭이 생기는 것까지는 아직 보드에서 확인하지 못했습니다. 처음 돌려보실 때 위 4️⃣의 순서로 단계별로 확인해 보시고, 결과를 알려 주시면 다음 사람이 덜 헤맵니다. (보드 확인 전) <!-- board-check -->
