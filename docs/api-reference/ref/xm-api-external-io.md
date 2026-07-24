# `xm_api_external_io.h` — 외부 확장 포트 GPIO·ADC 제어

> **대상 헤더**: `XM_FW/XM_API/xm_api_external_io.h` (Rev1.1 / Rev2.0 공통 파일 — 핀 매핑·전원 전환 API 등 일부는 Rev2.0 전용, 아래 🟢 뱃지 참고)
> **관련 개념 문서**: [04. 외부 IO 제어 API](../04-external-io.md)
> **관련 예제**: [04_Ext_IO_Basic](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/04_Ext_IO_Basic/) · [05_Ext_IO_analog](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05_Ext_IO_analog/) · [05a_Ext_IO_DIO_to_ADC](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05a_Ext_IO_DIO_to_ADC/) · [05b_Ext_IO_FSR_8ch](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05b_Ext_IO_FSR_8ch/) · [05c_Ext_IO_Mixed_ADC](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05c_Ext_IO_Mixed_ADC/) · [05d_Ext_IO_DIO_ADC_Hybrid](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/05d_Ext_IO_DIO_ADC_Hybrid/) · [06_Ext_IO_Safety_Switch](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/06_Ext_IO_Safety_Switch/) · [40_EMG_Proportional_Assist](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/40_EMG_Proportional_Assist/) (전원 전압 전환)

보드 측면 확장 포트(Extension Port)의 디지털 입출력(DIO) 8핀과 아날로그 입력(ADC) 4핀을 다루는 API 입니다. 필요하면 DIO 핀을 ADC 로 동적 전환해 최대 12채널 아날로그 입력까지 확보할 수 있고, Rev2.0 에서는 여기에 더해 확장 포트 공급 전압 전환(3.3V/5V)도 포함합니다.

---

## 언제 사용하나

보드에 연결한 외부 센서(FSR, 스위치, 전위차계 등)의 값을 읽거나, 외부 LED·릴레이 같은 디지털 장치를 제어하고 싶을 때 사용합니다. 아두이노의 `pinMode`/`digitalWrite`/`analogRead` 와 비슷한 감각으로 호출할 수 있도록 설계되어 있습니다. 동작 원리, 전기적 주의사항, 흔한 실수 목록은 [04. 외부 IO 제어 API](../04-external-io.md) 문서를 먼저 읽는 것을 권장합니다 — 이 페이지는 각 함수의 시그니처·파라미터 상세만 다룹니다.

---

## 함수 목록

| 함수 / 매크로 | 한 줄 설명 |
|------|-----------|
| [`XM_SetPinMode`](#xm_setpinmode) | DIO 핀의 입력/출력/풀업/풀다운 모드 설정 |
| [`XM_DigitalWrite`](#xm_digitalwrite) | DIO 핀에 HIGH/LOW 출력 |
| [`XM_DigitalRead`](#xm_digitalread) | DIO 핀의 현재 논리 레벨 읽기 |
| [`XM_AnalogRead`](#xm_analogread) | ADC 핀 전압을 정규화된 정수 값으로 읽기 (ADC1/2/3 통합) |
| [`XM_AnalogReadMillivolts`](#xm_analogreadmillivolts) | ADC 핀 전압을 밀리볼트(mV) 단위로 읽기 |
| [`XM_SetAnalogReadResolution`](#xm_setanalogreadresolution) | `XM_AnalogRead` 반환값의 출력 해상도 설정 |
| [`XM_GetAnalogResolution`](#xm_getanalogresolution) | ADC 핀의 하드웨어 네이티브 해상도 조회 |
| [`XM_GetAnalogReadResolution`](#xm_getanalogreadresolution) | 현재 설정된 출력 해상도 조회 |
| [`XM_SwitchDioToAdc`](#xm_switchdiotoadc) | DIO 핀 1개를 고속 ADC3 입력으로 전환 |
| [`XM_SwitchAllDioToAdc`](#xm_switchalldiotoadc) | DIO 8핀 전체를 ADC3 입력으로 일괄 전환 |
| [`XM_IsDioSwitchedToAdc`](#xm_isdioswitchedtoadc) | DIO 핀이 ADC 모드로 전환됐는지 조회 |
| [`XM_DIO_TO_ADC_PIN`](#xm_dio_to_adc_pin-매크로) (매크로) | DIO 핀 번호 → 대응 ADC 핀 번호 변환 |
| [`XM_SetExtPowerVoltage`](#xm_setextpowervoltage) 🟢 Rev 2.0 전용 | 확장 포트 공급 전압을 3.3V/5V 로 전환 |
| [`XM_AttachXsensMTi630`](#xm_attachxsensmti630) | Xsens MTi-630 IMU 를 External UART 에 결합 |
| [`XM_ConfigureXsensMTi630`](#xm_configurexsensmti630) | Xsens MTi-630 Output Configuration 1회 송신 |

---

## 함수 상세

### 1. 디지털 입출력 (DIO)

### `XM_SetPinMode`

```c
void XM_SetPinMode(XmDioPin_t pin, XmPinMode_t mode);
```

DIO 핀의 동작 모드(입력/출력/풀업/풀다운)를 설정합니다. 다른 DIO 함수를 쓰기 전에 반드시 먼저 호출해야 합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmDioPin_t` | 설정할 핀 (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |
| `mode` | `XmPinMode_t` | 목표 모드 (`XM_EXT_DIO_MODE_INPUT`/`_PULLUP`/`_PULLDOWN`/`_OUTPUT`) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: 비실시간 함수입니다. 내부적으로 `HAL_GPIO_Init()` 을 호출하므로 1ms(1kHz) `Control_Loop()` 안에서 호출하지 말고, `Control_Setup()` 에서 한 번만 설정하세요.
> 헤더 표기 참고: Rev2.0 원문 주석은 "2ms 실시간 루프 안에서 호출하지 마십시오"라고 적혀 있고 Rev1.1 원문 주석은 "1ms 실시간 루프"라고 적혀 있어 두 리비전 헤더 표현이 서로 다릅니다. XM10 의 사용자 `Control_Loop()` 주기는 SDK 전역 기준 1ms(1kHz) 이므로([09. 보조 task + 데이터 공유](../09-task-creation.md), [아키텍처 개요](../../architecture/README.md) 참고) 본 문서는 1ms 기준으로 통일해 표기했습니다.

**예제**

```c
void Control_Setup(void)
{
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP);   // 스위치 연결용
    XM_SetPinMode(XM_EXT_DIO_4, XM_EXT_DIO_MODE_OUTPUT);         // LED 출력용
}
```

**참고**: [`XmPinMode_t`](#xmpinmode_t) 전체 모드 목록

---

### `XM_DigitalWrite`

```c
void XM_DigitalWrite(XmDioPin_t pin, XmLogicLevel_t level);
```

DIO 핀에 전압(HIGH/LOW)을 출력합니다. `XM_EXT_DIO_MODE_OUTPUT` 모드로 설정된 핀에서만 동작합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmDioPin_t` | 대상 핀 |
| `level` | `XmLogicLevel_t` | 출력 레벨 (`XM_HIGH` = 3.3V, `XM_LOW` = 0V) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

> 해당 핀이 이미 `XM_SwitchDioToAdc()` 로 ADC 모드로 전환된 상태라면 `XM_DigitalWrite` 호출은 보호 장치(Guard Mechanism)에 의해 무시됩니다 ([04. 외부 IO — 흔한 실수](../04-external-io.md) 참고).

**예제**

```c
XM_DigitalWrite(XM_EXT_DIO_4, XM_HIGH);  // 4번 핀에 연결된 LED 켜기
```

**참고**: [`XM_IsDioSwitchedToAdc`](#xm_isdioswitchedtoadc) — ADC 전환 여부 확인

---

### `XM_DigitalRead`

```c
XmLogicLevel_t XM_DigitalRead(XmDioPin_t pin);
```

DIO 핀의 현재 전압 상태를 읽습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmDioPin_t` | 확인할 핀 |

**반환값**: `XmLogicLevel_t` — `XM_HIGH`(3.3V 근처) 또는 `XM_LOW`(0V 근처)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**예제**

```c
if (XM_DigitalRead(XM_EXT_DIO_2) == XM_HIGH) {
    // 스위치 입력 감지
}
```

---

### 2. 아날로그 입력 (ADC) — 통합 읽기 API

### `XM_AnalogRead`

```c
uint16_t XM_AnalogRead(XmAdcPin_t pin);
```

ADC 핀의 전압을 정규화된 정수 값으로 읽습니다. ADC1/2/3 중 어느 그룹에 속한 핀인지 함수 내부가 자동으로 판별하므로, 사용자는 핀 번호만 지정하면 됩니다. 모든 핀은 동일한 출력 해상도(기본 16-bit)로 정규화되어 반환되므로, 핀마다 하드웨어 해상도가 달라도 신경 쓸 필요가 없습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmAdcPin_t` | 읽을 핀 (`XM_EXT_ADC_1` ~ `XM_EXT_ADC_12`) |

**반환값**: 정규화된 ADC 값 (기본 0~65535, 출력 해상도에 따라 범위 변경). 잘못된 핀이면 `0`.

> `XM_EXT_ADC_5` ~ `_12` (ADC3 그룹) 은 [`XM_SwitchDioToAdc()`](#xm_switchdiotoadc) 로 먼저 전환해야 유효한 값을 반환합니다. 전환 전에 읽으면 `0`이 반환되며(유효한 0V 값과 구분 불가), Rev2.0 에서는 이 상황을 [`g_xm_adc_read_before_switch`](#진단용-전역-변수-🟢-rev-20-전용) 진단 변수로 확인할 수 있습니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준. 읽기 전용 조회이므로 1ms 루프 안에서 반복 호출해도 무방합니다.

**예제**

```c
void Control_Setup(void) {
    XM_SetAnalogReadResolution(12);   // 12-bit 모드 (0~4095)
    XM_SwitchDioToAdc(XM_EXT_DIO_1);
    XM_SwitchDioToAdc(XM_EXT_DIO_2);
}

void Control_Loop(void) {
    uint16_t fsr1 = XM_AnalogRead(XM_EXT_ADC_5);  // 0~4095
    uint16_t fsr2 = XM_AnalogRead(XM_EXT_ADC_6);  // 0~4095
}
```

**참고**: [`XM_AnalogReadMillivolts`](#xm_analogreadmillivolts), [`XM_SetAnalogReadResolution`](#xm_setanalogreadresolution)

---

### `XM_AnalogReadMillivolts`

```c
uint16_t XM_AnalogReadMillivolts(XmAdcPin_t pin);
```

ADC 핀의 전압을 밀리볼트(mV) 단위로 읽습니다. 네이티브 raw 값에서 직접 변환하므로 `XM_SetAnalogReadResolution()` 설정과 무관하게 항상 동일한 정확도를 유지합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmAdcPin_t` | 읽을 핀 (`XM_EXT_ADC_1` ~ `XM_EXT_ADC_12`) |

**반환값**: 밀리볼트 단위 전압 (0 ~ 3300, VREF = 3.3V 기준). 잘못된 핀이면 `0`.

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**예제**

```c
uint16_t mv = XM_AnalogReadMillivolts(XM_EXT_ADC_1);
float voltage = mv / 1000.0f;  // mv == 1650 이면 1.65V
```

---

### `XM_SetAnalogReadResolution`

```c
void XM_SetAnalogReadResolution(uint8_t bits);
```

`XM_AnalogRead()` 반환값의 출력 해상도를 설정합니다 (Arduino `analogReadResolution()` 호환). 하드웨어 ADC 해상도 자체를 바꾸는 것이 아니라, 소프트웨어에서 좌/우 시프트로 정규화하는 방식입니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `bits` | `uint8_t` | 출력 해상도 (8, 10, 12, 14, 16) |

**반환값**: 없음 (`void`). 기본값 16.

**⚠️ 호출 컨텍스트**: 헤더에 별도의 실시간 제약이 명시되어 있지 않습니다. 설정값이 이후의 모든 `XM_AnalogRead()` 호출에 전역으로 적용되므로, 일반적으로 `Control_Setup()` 에서 한 번만 호출하는 것을 권장합니다.

**예제**

```c
XM_SetAnalogReadResolution(12);  // 이후 모든 XM_AnalogRead() → 0~4095
```

---

### `XM_GetAnalogResolution`

```c
uint8_t XM_GetAnalogResolution(XmAdcPin_t pin);
```

지정한 ADC 핀의 **하드웨어 네이티브** 해상도를 조회합니다. 실제 ADC가 몇 bit 로 동작하는지 확인할 때 사용합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmAdcPin_t` | 조회할 ADC 핀 |

**반환값**: 네이티브 해상도(bit 수). 잘못된 핀이면 `0`.

핀별 네이티브 해상도는 리비전마다 다릅니다 — 아래 [`XmAdcPin_t`](#xmadcpin_t) 표 참고.

> ⚠️ **헤더 표기 불일치(플래그)**: Rev2.0 `xm_api_external_io.h` 안의 `@code` 예제 주석은 `XM_EXT_ADC_1`=12-bit, `XM_EXT_ADC_2`=16-bit 라고 적어 두었습니다(Rev1.1 예제를 그대로 복사한 것으로 보입니다). 그러나 같은 파일의 `XmAdcPin_t` enum 주석(`/* ADC1 고정 핀 (Rev2.0: 전부 ADC1 16-bit, 항상 사용 가능) */`)과 [Rev 2.0 하드웨어 핀맵](../../hardware/external-gpio-rev2.0.md) 문서는 모두 `XM_EXT_ADC_1~4` 를 16-bit 로 명시합니다. 본 문서는 후자(enum 주석 + 핀맵 문서)를 따랐습니다. 확실히 하려면 보드에서 `XM_GetAnalogResolution()` 을 직접 호출해 확인하세요.

**예제**

```c
uint8_t res = XM_GetAnalogResolution(XM_EXT_ADC_1);
```

---

### `XM_GetAnalogReadResolution`

```c
uint8_t XM_GetAnalogReadResolution(void);
```

`XM_SetAnalogReadResolution()` 으로 현재 설정된 **출력** 해상도를 조회합니다.

**파라미터**: 없음

**반환값**: 현재 출력 해상도(bit 수, 기본값 16)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**예제**

```c
XM_SetAnalogReadResolution(12);
uint8_t res = XM_GetAnalogReadResolution();  // 12
```

---

### 3. DIO → ADC 동적 전환 (10kHz 고속 아날로그 입력)

### `XM_SwitchDioToAdc`

```c
bool XM_SwitchDioToAdc(XmDioPin_t pin);
```

외부 DIO 핀을 고속 ADC3 입력으로 전환합니다. 기본적으로 DIO 핀(D1~D8)은 디지털 입출력용이며, 이 함수를 호출하면 해당 핀이 16-bit ADC3 아날로그 입력으로 전환됩니다. ADC3 는 최대 8채널 동시 스캔, 10kHz 샘플링을 지원합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmDioPin_t` | 전환할 DIO 핀 (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |

**반환값**: `bool` — `true`(성공), `false`(실패)

**⚠️ 호출 컨텍스트**: **비실시간** 함수입니다. 초기화 단계(`Control_Setup()`)에서만 호출하세요.
> 전환 후 해당 핀은 디지털 GPIO 로 복구할 수 없습니다(재부팅 필요) — 의도된 안전 동작입니다.

**예제**

```c
void Control_Setup(void) {
    XM_SetAnalogReadResolution(12);
    for (int i = 0; i < 8; i++) {
        XM_SwitchDioToAdc(XM_EXT_DIO_1 + i);  // DIO 1~8 → ADC3
    }
}

void Control_Loop(void) {
    uint16_t fsr[8];
    for (int i = 0; i < 8; i++) {
        fsr[i] = XM_AnalogRead(XM_EXT_ADC_5 + i);  // 모두 0~4095
    }
}
```

**참고**: [`XM_SwitchAllDioToAdc`](#xm_switchalldiotoadc) — 8핀 일괄 전환 버전, [`XM_DIO_TO_ADC_PIN`](#xm_dio_to_adc_pin-매크로) — 변환 매크로

---

### `XM_SwitchAllDioToAdc`

```c
bool XM_SwitchAllDioToAdc(void);
```

DIO 핀(D1~D8) 전체를 ADC3 입력으로 한 번에 전환하는 편의 함수입니다. 루프 없이 8채널을 일괄 전환할 때 사용합니다.

**파라미터**: 없음

**반환값**: `bool` — `true`(전체 성공), `false`(하나 이상 실패)

**⚠️ 호출 컨텍스트**: **비실시간** 함수입니다. `XM_SwitchDioToAdc()` 와 동일하게 `Control_Setup()` 에서만 호출하세요.

**예제**

```c
void Control_Setup(void) {
    XM_SetAnalogReadResolution(12);
    XM_SwitchAllDioToAdc();  // DIO 1~8 → ADC3 일괄 전환
}
```

---

### `XM_IsDioSwitchedToAdc`

```c
bool XM_IsDioSwitchedToAdc(XmDioPin_t pin);
```

DIO 핀이 현재 ADC 모드로 전환되어 있는지 확인합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `pin` | `XmDioPin_t` | 확인할 DIO 핀 (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |

**반환값**: `bool` — `true`(ADC 모드), `false`(GPIO 모드 또는 잘못된 핀)

**⚠️ 호출 컨텍스트**: `Control_Setup()` / `Control_Loop()` 컨텍스트 기준.

**예제**

```c
XM_SwitchDioToAdc(XM_EXT_DIO_1);
if (XM_IsDioSwitchedToAdc(XM_EXT_DIO_1)) {
    // ADC 모드 — XM_AnalogRead() 사용 가능
}
```

---

### `XM_DIO_TO_ADC_PIN` (매크로)

```c
#define XM_DIO_TO_ADC_PIN(dio)  ((XmAdcPin_t)((dio) + XM_EXT_ADC_5))
```

DIO 핀 번호를 대응하는 ADC 핀 번호로 변환합니다. `XM_SwitchDioToAdc()` 로 전환한 뒤 `XM_AnalogRead()` 에 넘길 ADC 핀 값을 직관적으로 얻을 때 사용합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `dio` | `XmDioPin_t` | DIO 핀 번호 (`XM_EXT_DIO_1` ~ `XM_EXT_DIO_8`) |

**반환값**: 대응하는 `XmAdcPin_t` 값 (`XM_EXT_ADC_5` ~ `XM_EXT_ADC_12`)

**⚠️ 호출 컨텍스트**: 전처리기 매크로이므로 제약 없음(컴파일 타임 치환).

**예제**

```c
XM_SwitchDioToAdc(XM_EXT_DIO_1);
uint16_t val = XM_AnalogRead(XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1));  // == XM_EXT_ADC_5 읽기
```

---

### 4. 확장 포트 전원 전압 🟢 Rev 2.0 전용

### `XM_SetExtPowerVoltage`

```c
void XM_SetExtPowerVoltage(XmExtPwrVoltage_t voltage);
```

> 🟢 **Rev 2.0 전용** — Rev1.1 헤더에는 이 함수와 `XmExtPwrVoltage_t` 타입이 존재하지 않습니다.

확장 포트의 **센서 공급 전압**을 3.3V 또는 5V 로 전환합니다(보드 신호 `EXT_PWR_SEL_5V`, PE3 GPIO 로 전원 MUX 제어). 기본값은 3.3V 이며, 5V 로 동작하는 외부 센서를 쓸 때 5V 로 전환합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `voltage` | `XmExtPwrVoltage_t` | `XM_EXT_PWR_3V3`(3.3V, 기본값) 또는 `XM_EXT_PWR_5V`(5V) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: 비실시간 함수입니다(헤더 명시). `Control_Setup()` 에서 호출하세요.

> ⚠️ 이 전압은 센서를 **구동(공급)** 하는 전원입니다. **ADC 신호 입력 범위는 공급 전압과 무관하게 항상 0~3.3V** 입니다. 5V 로 구동한 센서의 신호 출력이 3.3V 를 넘으면 ADC 핀이 손상될 수 있으므로, 신호 라인이 0~3.3V 범위인지 확인하고 필요하면 분압하세요.

**예제**

```c
void Control_Setup(void) {
    XM_SetExtPowerVoltage(XM_EXT_PWR_5V);   // 5V 센서 구동
    XM_SwitchDioToAdc(XM_EXT_DIO_1);        // DIO_1 → ADC (XM_EXT_ADC_5)
    // 이후 XM_AnalogReadMillivolts(XM_EXT_ADC_5) 로 센서 전압(mV) 읽기
}
```

**참고**: [`XmExtPwrVoltage_t`](#xmextpwrvoltage_t-🟢-rev-20-전용), [40_EMG_Proportional_Assist 예제](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/40_EMG_Proportional_Assist/)

---

### 5. External UART IMU 결합 (Xsens MTi-630)

### `XM_AttachXsensMTi630`

```c
void XM_AttachXsensMTi630(void);
```

외부 IMU 센서 Xsens MTi-630 을 External UART 포트에 결합합니다. `Control_Setup()` 에서 1회 호출하면 이후 `XM.status.ext_imu.*` 필드로 센서 데이터에 접근할 수 있습니다. 센서가 물리적으로 연결되어 있지 않은 상태에서 호출해도 안전하며, 케이블을 연결하면 자동으로 OPERATIONAL 상태로 전환됩니다.

> ⚠️ **Rev 별 동작 차이** — 함수 시그니처·사용자 코드는 두 리비전에서 동일하지만, 내부적으로 결합되는 하드웨어 포트가 다릅니다.
> - **Rev 2.0**: 전용 USART2 포트(PD5=TX, PD6=RX, 921600bps)에 결합됩니다. 기존에 쓰던 ADC/DIO 핀과 자원 충돌이 없습니다.
> - **Rev 1.1**: PA0/PA1 을 UART4 로 동적 전환해 결합합니다(`ExternalIO_SwitchToUartMode` 내부 호출). 호출 후 `XM_EXT_ADC_1`/`XM_EXT_ADC_3`(PA0/PA1)는 ADC 로 사용할 수 없게 되며, 대신 `XM_EXT_ADC_2`/`XM_EXT_ADC_4`(PA0_C/PA1_C)를 사용해야 합니다. 미호출 시 PA0/PA1 은 그대로 ADC 로 유지됩니다.

**파라미터**: 없음

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서 1회 호출(헤더 명시). `Control_Loop()` 반복 호출용이 아닙니다.

**예제**

```c
void Control_Setup(void)
{
    XM_AttachXsensMTi630();
    // 신품/공장 초기화 센서라면:
    // XM_ConfigureXsensMTi630();
}
```

**참고**: [`XM_ConfigureXsensMTi630`](#xm_configurexsensmti630), [04. 외부 IO — 외부 IMU 사용 시 주의](../04-external-io.md#외부-imu-사용-시-주의)

---

### `XM_ConfigureXsensMTi630`

```c
void XM_ConfigureXsensMTi630(void);
```

Xsens MTi-630 의 Output Configuration(1kHz Quaternion + Acc + Gyro 출력)을 1회 송신합니다. 신품이거나 공장 초기화된 센서에만 필요합니다 — EEPROM 에 설정이 이미 보존된(재사용 중인) 센서라면 호출할 필요가 없습니다.

**파라미터**: 없음

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: 약 1초간 블로킹되므로 1ms `Control_Loop()` 안에서 호출 금지. `Control_Setup()` 또는 이에 준하는 비실시간 시점에서, [`XM_AttachXsensMTi630()`](#xm_attachxsensmti630) 을 먼저 호출한 뒤에만 사용하세요.

**예제**

```c
void Control_Setup(void)
{
    XM_AttachXsensMTi630();
    XM_ConfigureXsensMTi630();  // 신품 센서 1회 설정 (블로킹 ~1초)
}
```

**참고**: [`XM_AttachXsensMTi630`](#xm_attachxsensmti630)

---

## 타입 / 매크로

### `XmDioPin_t`

DIO 핀 8개는 두 리비전에서 동일한 물리 핀에 매핑됩니다.

| 값 | 물리 핀 | 설명 |
|----|--------|------|
| `XM_EXT_DIO_1` = 0 | PF3 | |
| `XM_EXT_DIO_2` | PF4 | |
| `XM_EXT_DIO_3` | PF5 | |
| `XM_EXT_DIO_4` | PF6 | |
| `XM_EXT_DIO_5` | PF7 | |
| `XM_EXT_DIO_6` | PF8 | |
| `XM_EXT_DIO_7` | PF9 | |
| `XM_EXT_DIO_8` | PF10 | |
| `XM_EXT_DIO_COUNT` | — | (내부용) 핀 개수 |

### `XmAdcPin_t`

고정 ADC 4핀(`XM_EXT_ADC_1~4`)의 물리 핀·네이티브 해상도는 **리비전마다 다릅니다**. 동적 전환 ADC3 그룹(`XM_EXT_ADC_5~12`)은 두 리비전에서 동일합니다.

| 값 | Rev 1.1 | Rev 2.0 | 비고 |
|----|---------|---------|------|
| `XM_EXT_ADC_1` = 0 | PA0, **ADC1 12-bit** 네이티브 [Shared: UART4_TX] | PB0 (ADC1_INP9), **ADC1 16-bit** 네이티브 | Rev1.1: IMU 결합 시 UART4 가 점유 |
| `XM_EXT_ADC_2` | PA0_C, **ADC2 16-bit** 네이티브 | PB1 (ADC1_INP5), **ADC1 16-bit** 네이티브 | Rev1.1: IMU 결합 후 ADC_1 대신 사용 |
| `XM_EXT_ADC_3` | PA1, **ADC1 12-bit** 네이티브 [Shared: UART4_RX] | PF11 (ADC1_INP2), **ADC1 16-bit** 네이티브 | Rev1.1: IMU 결합 시 UART4 가 점유 |
| `XM_EXT_ADC_4` | PA1_C, **ADC2 16-bit** 네이티브 | PF12 (ADC1_INP6), **ADC1 16-bit** 네이티브 | Rev1.1: IMU 결합 후 ADC_3 대신 사용 |
| `XM_EXT_ADC_5` | PF3 (DIO 1 → ADC3, 16-bit) | PF3 (DIO 1 → ADC3, 16-bit) | `XM_SwitchDioToAdc()` 호출 필요 |
| `XM_EXT_ADC_6` | PF4 (DIO 2 → ADC3, 16-bit) | PF4 (DIO 2 → ADC3, 16-bit) | 〃 |
| `XM_EXT_ADC_7` | PF5 (DIO 3 → ADC3, 16-bit) | PF5 (DIO 3 → ADC3, 16-bit) | 〃 |
| `XM_EXT_ADC_8` | PF6 (DIO 4 → ADC3, 16-bit) | PF6 (DIO 4 → ADC3, 16-bit) | 〃 |
| `XM_EXT_ADC_9` | PF7 (DIO 5 → ADC3, 16-bit) | PF7 (DIO 5 → ADC3, 16-bit) | 〃 |
| `XM_EXT_ADC_10` | PF8 (DIO 6 → ADC3, 16-bit) | PF8 (DIO 6 → ADC3, 16-bit) | 〃 |
| `XM_EXT_ADC_11` | PF9 (DIO 7 → ADC3, 16-bit) | PF9 (DIO 7 → ADC3, 16-bit) | 〃 |
| `XM_EXT_ADC_12` | PF10 (DIO 8 → ADC3, 16-bit) | PF10 (DIO 8 → ADC3, 16-bit) | 〃 |
| `XM_EXT_ADC_COUNT` | — | — | (내부용) 핀 개수 |

> 표의 Rev2.0 `XM_EXT_ADC_1~4` 해상도는 enum 주석과 [Rev 2.0 하드웨어 핀맵](../../hardware/external-gpio-rev2.0.md) 문서를 근거로 16-bit 로 표기했습니다 — [`XM_GetAnalogResolution`](#xm_getanalogresolution) 항목의 헤더 표기 불일치 설명을 함께 참고하세요.

공통 입력 범위: 0~3.3V, 샘플링 10kHz(고정 핀) / 10kHz(ADC3 그룹). 출력값은 [`XM_SetAnalogReadResolution()`](#xm_setanalogreadresolution) 으로 8/10/12/14/16-bit 중 선택 가능합니다.

### `XmPinMode_t`

| 값 | 설명 |
|----|------|
| `XM_EXT_DIO_MODE_INPUT` | 디지털 입력 (Floating) |
| `XM_EXT_DIO_MODE_INPUT_PULLUP` | 디지털 입력 (내부 Pull-up 저항) |
| `XM_EXT_DIO_MODE_INPUT_PULLDOWN` | 디지털 입력 (내부 Pull-down 저항) |
| `XM_EXT_DIO_MODE_OUTPUT` | 디지털 출력 |

### `XmLogicLevel_t`

| 값 | 설명 |
|----|------|
| `XM_LOW` = 0 | 0V (GND) |
| `XM_HIGH` = 1 | 3.3V (VCC) |

### `XmExtPwrVoltage_t` 🟢 Rev 2.0 전용

> Rev1.1 헤더에는 정의되어 있지 않습니다.

| 값 | 설명 |
|----|------|
| `XM_EXT_PWR_3V3` = 0 | 3.3V 출력 (기본값, Low) |
| `XM_EXT_PWR_5V` = 1 | 5V 출력 (High) |

---

## 진단용 전역 변수 🟢 Rev 2.0 전용

### `g_xm_adc_read_before_switch`

```c
extern volatile uint16_t g_xm_adc_read_before_switch;
```

> 🟢 **Rev 2.0 전용** — Rev1.1 헤더에는 이 변수가 존재하지 않습니다.

ADC3 그룹(`XM_EXT_ADC_5~12`) 핀을 [`XM_SwitchDioToAdc()`](#xm_switchdiotoadc) 전환 없이 [`XM_AnalogRead()`](#xm_analogread)/[`XM_AnalogReadMillivolts()`](#xm_analogreadmillivolts) 로 읽은 이력을 핀별 1비트로 기록하는 진단용 비트마스크입니다. `bit0` = `XM_EXT_ADC_5`(DIO_1) … `bit7` = `XM_EXT_ADC_12`(DIO_8). 전환 전에 ADC3 핀을 읽으면 하위 계층이 `0`을 반환하는데(유효한 0V 값과 구분 불가), 그 상황이 채널별로 이 비트마스크에 기록됩니다. 값이 `0`이면 모든 ADC 읽기가 정상입니다.

**타입**: `volatile uint16_t` (extern) — 헤더는 쓰기 API 를 제공하지 않으므로 읽기 전용으로 사용하세요.

**⚠️ 호출 컨텍스트**: `Control_Loop()`, STM32CubeIDE Live Expressions, 또는 사용자 코드 어디서든 읽을 수 있습니다(USB 미사용·SWD-only 환경에서도 관찰 가능 — 특정 출력 채널에 의존하지 않습니다). `volatile` 이므로 매번 실제 메모리를 읽습니다.

**예제**

```c
// ADC read 값이 계속 0이라 원인을 확인하고 싶을 때
if (g_xm_adc_read_before_switch != 0U) {
    // bit0=ADC_5(DIO1) ... bit7=ADC_12(DIO8) 중 XM_SwitchDioToAdc() 호출 누락된 핀이 있음
}
```

**참고**: [`XM_SwitchDioToAdc`](#xm_switchdiotoadc), [`XM_IsDioSwitchedToAdc`](#xm_isdioswitchedtoadc)

---

## 내부 전용 (호출 금지)

이 헤더에 정의된 함수·타입·변수 중 `[Internal]` 또는 System-only 로 표시된 항목은 없습니다 — 위에서 다룬 14개 함수, 1개 매크로, 5개 타입, 1개 진단 변수 모두 공개(Public) API 입니다.

---

## Rev1.1 / Rev2.0 차이 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| DIO 8핀 기본 제어 (`XM_SetPinMode`/`DigitalWrite`/`DigitalRead`) | ✅ | ✅ |
| ADC 통합 읽기 API (`XM_AnalogRead` 등) | ✅ | ✅ |
| `XM_EXT_ADC_1~4` 물리 핀 / 네이티브 해상도 | PA0/PA0_C/PA1/PA1_C — ADC1 12-bit + ADC2 16-bit 혼재 | PB0/PB1/PF11/PF12 — 전부 ADC1 16-bit |
| DIO → ADC3 동적 전환 (`XM_SwitchDioToAdc` 등) | ✅ | ✅ |
| `XM_AttachXsensMTi630` 내부 결합 방식 | PA0/PA1 → UART4 동적 전환 (`XM_EXT_ADC_1`/`_3` 점유) | 전용 USART2(PD5/PD6), ADC 자원 점유 없음 |
| `XM_SetExtPowerVoltage` / `XmExtPwrVoltage_t` (전원 3.3V/5V 전환) | ❌ 없음 | 🟢 전용 |
| `g_xm_adc_read_before_switch` 진단 변수 | ❌ 없음 | 🟢 전용 |

---

## 관련 문서

- [04. 외부 IO 제어 API (개념)](../04-external-io.md) — 동작 원리, 흔한 실수, 예제 매핑
- [하드웨어 핀맵 — Rev 1.1](../../hardware/external-gpio-rev1.1.md) / [Rev 2.0](../../hardware/external-gpio-rev2.0.md)
- [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교)
