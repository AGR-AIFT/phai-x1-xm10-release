# 외부 IO 제어 API

`xm_api_external_io.h` 의 확장 포트 (Extension Port) 제어 함수들. 아두이노와 비슷한 방식으로 외부 센서 값을 읽거나 디지털 장치를 제어할 수 있어요. DIO 8 + ADC 4 가 기본이고, 필요하면 DIO 를 ADC 로 동적 전환해서 최대 12 채널 ADC 까지 확장됩니다.

> **보드 핀 위치·라벨**은 보드 리비전마다 다를 수 있으니 [하드웨어 핀맵 — Rev 1.1](../hardware/external-gpio-rev1.1.md) / [Rev 2.0](../hardware/external-gpio-rev2.0.md) 을 참고하세요. 이 페이지는 **함수 사용법** 중심입니다.

---

## 핀 한눈 정리

| API 이름 | 종류 | 비고 |
| :--- | :--- | :--- |
| `XM_EXT_DIO_1` ~ `XM_EXT_DIO_8` | 디지털 입출력 | 8 핀 모두 ADC 로 동적 전환 가능 |
| `XM_EXT_ADC_1` ~ `XM_EXT_ADC_4` | 아날로그 입력 (고정) | 0~3.3 V, 10 kHz 샘플링 |
| `XM_EXT_ADC_5` ~ `XM_EXT_ADC_12` | 아날로그 입력 (동적 전환) | `XM_SwitchDioToAdc()` 후 사용 |

> 보드 PCB 라벨, 헤더 위치, 핀 배치는 [하드웨어 핀맵 문서](../hardware/) 참조.

---

## 외부 IMU 사용 시 주의

`XM_EnableExternalImu()` 를 호출해 외부 IMU (XSENS MTi 등) 를 켜면 일부 ADC 핀이 UART 통신용으로 전환됩니다. 어떤 핀이 점유되는지는 보드 리비전마다 다르므로 해당 [핀맵 문서](../hardware/) 의 "외부 IMU 사용 시 주의" 섹션을 보세요.

---

## 데이터 구조

### 핀 식별자

#### `XmDioPin_t`

```c
typedef enum {
    XM_EXT_DIO_1 = 0,
    XM_EXT_DIO_2,
    XM_EXT_DIO_3,
    XM_EXT_DIO_4,
    XM_EXT_DIO_5,
    XM_EXT_DIO_6,
    XM_EXT_DIO_7,
    XM_EXT_DIO_8,
    XM_EXT_DIO_COUNT
} XmDioPin_t;
```

#### `XmAdcPin_t`

```c
typedef enum {
    XM_EXT_ADC_1 = 0,
    XM_EXT_ADC_2,
    XM_EXT_ADC_3,
    XM_EXT_ADC_4,
    /* DIO → ADC 동적 전환 후 사용 (XM_SwitchDioToAdc 호출 필요) */
    XM_EXT_ADC_5,   // DIO 1 → ADC
    XM_EXT_ADC_6,   // DIO 2 → ADC
    XM_EXT_ADC_7,   // DIO 3 → ADC
    XM_EXT_ADC_8,   // DIO 4 → ADC
    XM_EXT_ADC_9,   // DIO 5 → ADC
    XM_EXT_ADC_10,  // DIO 6 → ADC
    XM_EXT_ADC_11,  // DIO 7 → ADC
    XM_EXT_ADC_12,  // DIO 8 → ADC
    XM_EXT_ADC_COUNT
} XmAdcPin_t;
```

### 설정 타입

#### `XmPinMode_t`

핀의 동작 모드를 결정합니다. `XM_SetPinMode` 함수에서 사용됩니다.

```c
typedef enum {
    XM_EXT_DIO_MODE_INPUT,           /**< 디지털 입력 (Floating) */
    XM_EXT_DIO_MODE_INPUT_PULLUP,    /**< 디지털 입력 (내부 Pull-up 저항) */
    XM_EXT_DIO_MODE_INPUT_PULLDOWN,  /**< 디지털 입력 (내부 Pull-down 저항) */
    XM_EXT_DIO_MODE_OUTPUT           /**< 디지털 출력 */
} XmPinMode_t;
```

| Mode | Description |
| :--- | :--- |
| **`XM_EXT_DIO_MODE_INPUT`** | **디지털 입력 (기본값).** 핀을 Floating 상태로 둡니다. |
| **`XM_EXT_DIO_MODE_INPUT_PULLUP`** | **풀업 입력.** 내부 저항을 통해 3.3V에 연결합니다. 스위치 연결 시 유용합니다. |
| **`XM_EXT_DIO_MODE_INPUT_PULLDOWN`** | **풀다운 입력.** 내부 저항을 통해 0V에 연결합니다. |
| **`XM_EXT_DIO_MODE_OUTPUT`** | **디지털 출력.** 0V 또는 3.3V를 출력합니다. (하드웨어 변경시 5V 출력 가능) |

#### `XmLogicLevel_t`

디지털 신호의 레벨을 표현합니다.

```c
typedef enum {
    XM_LOW  = 0, /**< 0V (GND) */
    XM_HIGH = 1  /**< 3.3V (VCC) */
} XmLogicLevel_t;
```

-----

## 3\. 함수 상세 (Function Reference)

### 3.1. Configuration Function

#### `XM_SetPinMode`

핀의 동작 모드(입력/출력/아날로그)를 설정합니다. 사용하기 전에 반드시 호출해야 합니다.

  * **Syntax**
    ```c
    bool XM_SetPinMode(XmDioPin_t pin, XmPinMode_t mode);
    ```
  * **Parameters**
      * `pin`: 설정할 핀 번호 (`XM_EXT_DIO_1` \~ `4`)
      * `mode`: 동작 모드 (`XM_EXT_DIO_MODE_INPUT`, `XM_EXT_DIO_MODE_INPUT_PULLUP`, `XM_EXT_DIO_MODE_INPUT_PULLDOWN` 등)
  * **Returns**:
      * `true`: 설정 성공
      * `false`: 실패 (잘못된 핀 번호, 또는 **IMU와 자원 충돌 발생**)
  * **Example**
    ```c
    // 3번 핀을 풀업 입력으로 설정 (스위치 연결용)
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP);

    // 1번 핀을 풀다운 입력으로 설정
    if (!XM_SetPinMode(XM_EXT_DIO_1, XM_EXT_DIO_MODE_INPUT_PULLDOWN)) {
        XM_SendUsbDebugMessage("Error: PIN 1 is busy!\r\n");
    }
    ```

-----

### 3.2. Digital I/O Functions

#### `XM_DigitalWrite`

디지털 핀에 전압(High/Low)을 출력합니다. (`XM_IO_OUTPUT` 모드일 때만 동작)

  * **Syntax**
    ```c
    void XM_DigitalWrite(XmDioPin_t pin, XmLogicLevel_t level);
    ```
  * **Parameters**
      * `pin`: 대상 핀
      * `level`: `XM_HIGH` (3.3V) 또는 `XM_LOW` (0V)
  * **Example**
    ```c
    // 4번 핀에 연결된 LED 켜기
    XM_DigitalWrite(XM_EXT_DIO_4, XM_HIGH);
    ```

#### `XM_DigitalRead`

디지털 핀의 현재 전압 상태를 읽습니다.

  * **Syntax**
    ```c
    XmLogicLevel_t XM_DigitalRead(XmDioPin_t pin);
    ```
  * **Returns**: `XM_HIGH` (입력이 3.3V 근처일 때) 또는 `XM_LOW` (0V 근처일 때)
  * **Example**
    ```c
    // 3번 핀(풀업 스위치)이 눌렸는지 확인 (눌리면 LOW)
    if (XM_DigitalRead(XM_EXT_DIO_3) == XM_LOW) {
        // 스위치 눌림 처리
    }
    ```

-----

### 3.3. Analog I/O Functions

#### `XM_AnalogRead`

핀의 전압을 16비트 정수 값으로 읽습니다.

  * **Syntax**
    ```c
    uint16_t XM_AnalogRead(XmAdcPin_t pin);
    ```
  * **Returns**: 0 \~ 65535 (0V \~ 3.3V에 대응, 기본 16bit)
  * **Note**: 하드웨어 ADC 해상도가 12비트여도, API는 항상 **16비트로 정규화**된 값을 반환합니다. `XM_SetAnalogReadResolution()`으로 해상도를 변경할 수 있습니다.

#### `XM_AnalogReadMillivolts`

핀의 전압을 밀리볼트(mV) 단위로 읽습니다.

  * **Syntax**
    ```c
    uint32_t XM_AnalogReadMillivolts(XmAdcPin_t pin);
    ```
  * **Returns**: 0 ~ 3300 (mV)

#### `XM_SetAnalogReadResolution`

AnalogRead의 반환값 해상도를 설정합니다.

  * **Syntax**
    ```c
    void XM_SetAnalogReadResolution(uint8_t bits);
    ```
  * **Parameters**
      * `bits`: 원하는 해상도 비트 수 (8, 10, 12, 16)
  * **Example**
    ```c
    XM_SetAnalogReadResolution(12); // 0~4095 범위로 반환
    ```

#### `XM_GetAnalogResolution`

지정된 ADC 핀의 **하드웨어 네이티브** 해상도를 반환합니다.

  * **Syntax**
    ```c
    uint8_t XM_GetAnalogResolution(XmAdcPin_t pin);
    ```
  * **Parameters**: `pin` — 조회할 ADC 핀
  * **Returns**: 해당 ADC 채널의 네이티브 해상도 (bit 수)
  * **Example**
    ```c
    uint8_t res1 = XM_GetAnalogResolution(XM_EXT_ADC_1);  // 12 (ADC1, 12-bit)
    uint8_t res2 = XM_GetAnalogResolution(XM_EXT_ADC_2);  // 16 (ADC2, 16-bit)
    ```

#### `XM_GetAnalogReadResolution`

현재 설정된 AnalogRead **출력** 해상도를 반환합니다. `XM_SetAnalogReadResolution()`으로 설정한 값을 확인합니다.

  * **Syntax**
    ```c
    uint8_t XM_GetAnalogReadResolution(void);
    ```
  * **Returns**: 현재 출력 해상도 비트 수 (기본값 16)

-----

### 3.4. DIO ↔ ADC 동적 전환 함수 *(v2.0.0 신규)*

DIO 핀을 런타임에 ADC 모드로 전환하여, 8개 DIO + 4개 ADC = 최대 12채널 아날로그 입력을 사용할 수 있습니다.

#### `XM_SwitchDioToAdc`

특정 DIO 핀을 ADC 모드로 전환합니다.

  * **Syntax**
    ```c
    bool XM_SwitchDioToAdc(XmDioPin_t dio_pin);
    ```
  * **Parameters**
      * `dio_pin`: ADC로 전환할 DIO 핀
  * **Returns**: `true` (전환 성공), `false` (지원되지 않는 핀)
  * **Example**
    ```c
    // DIO_1을 ADC로 전환 후 읽기
    XM_SwitchDioToAdc(XM_EXT_DIO_1);
    uint16_t val = XM_AnalogRead(XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1));
    ```

#### `XM_SwitchAllDioToAdc`

모든 DIO 핀을 ADC 모드로 일괄 전환합니다.

  * **Syntax**
    ```c
    bool XM_SwitchAllDioToAdc(void);
    ```
  * **Returns**: `true` (전환 성공)

#### `XM_IsDioSwitchedToAdc`

특정 DIO 핀이 현재 ADC 모드로 전환되어 있는지 확인합니다.

  * **Syntax**
    ```c
    bool XM_IsDioSwitchedToAdc(XmDioPin_t dio_pin);
    ```
  * **Returns**: `true` (ADC 모드), `false` (DIO 모드)

#### `XM_DIO_TO_ADC_PIN` (매크로)

DIO 핀 번호를 ADC 핀 번호로 변환하는 매크로입니다. `XM_SwitchDioToAdc()` 후 `XM_AnalogRead()`에서 사용합니다.

  * **Syntax**
    ```c
    #define XM_DIO_TO_ADC_PIN(dio) // DIO index → ADC index 매핑
    ```

-----

### 3.5. 외부 IMU 제어

#### `XM_EnableExternalImu`

외부 IMU(XSENS MTi-630) 사용을 활성화합니다. 활성화 시 ADC_1/ADC_3 핀이 UART4 TX/RX로 전환됩니다.

  * **Syntax**
    ```c
    void XM_EnableExternalImu(void);
    ```
  * **Note**: 이 함수 호출 후 `XM_EXT_ADC_1`, `XM_EXT_ADC_3`은 ADC로 사용할 수 없습니다.

---

## 관련 예제

| 예제 | 난이도 | 외부 I/O 활용 |
|------|--------|-------------|
| [04_Ext_IO_Basic](../../examples/04_Ext_IO_Basic/) | 초급 | DIO 입출력 |
| [05_Ext_IO_analog](../../examples/05_Ext_IO_analog/) | 초급 | 고정 ADC 전압 읽기 |
| [05a_Ext_IO_DIO_to_ADC](../../examples/05a_Ext_IO_DIO_to_ADC/) | 초급 | DIO→ADC 단일 전환 |
| [05b_Ext_IO_FSR_8ch](../../examples/05b_Ext_IO_FSR_8ch/) | 중급 | 8채널 일괄 전환 + Resolution |
| [05c_Ext_IO_Mixed_ADC](../../examples/05c_Ext_IO_Mixed_ADC/) | 중급 | 고정 + 동적 ADC 혼합 |
| [05d_Ext_IO_DIO_ADC_Hybrid](../../examples/05d_Ext_IO_DIO_ADC_Hybrid/) | 응용 | GPIO + ADC 혼합 모드 |
| [06_Ext_IO_Safety_Switch](../../examples/06_Ext_IO_Safety_Switch/) | 중급 | 안전 스위치 인터록 |

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `XM_EXT_ADC_1` 또는 `_3` 가 0 mV 만 반환 | `XM_EnableExternalImu()` 활성화로 UART 가 핀 점유 | Rev1.1: `XM_EXT_ADC_2/4` (PA0_C/PA1_C) 사용. Rev2.0: 별도 UART 포트라 무관 |
| `SwitchDioToAdc` 후 GPIO 로 복구 안 됨 | ADC 전환은 재부팅 전 영구 (의도된 동작) | 보드 리셋 또는 전원 재인가 |
| `DigitalWrite(ADC 전환된 DIO, ...)` 가 무시됨 | 보호 장치 (Guard Mechanism) — 의도된 동작 | ADC 핀은 ADC API 로만 사용 |
| 8-bit Resolution 인데 raw 값이 256+ | `SetAnalogReadResolution` 호출 누락 또는 mV API 사용 (Resolution 무관) | Setup 에서 `SetAnalogReadResolution(8)` 호출 |
| FSR 분압 회로에서 mV 가 항상 0 또는 3300 | 풀다운 10 kΩ 누락 또는 FSR 단락 | FSR (3.3 V) — DIO 핀 — 10 kΩ — GND 회로 |
| `XM_DIO_TO_ADC_PIN` 컴파일 안 됨 | DIO 1~8 외 핀 사용 (예: DIO_9) | 매크로는 DIO_1~8 에만 유효 |
| ADC 노이즈가 크다 | 짧지 않은 와이어 + GND 차이 | 짧은 점퍼 + 공통 GND, 가능하면 외부 RC 필터 |
