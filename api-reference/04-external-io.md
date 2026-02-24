# API Reference: External I/O Control

`xm_api_external_io.h`에 정의된 **확장 포트(Extension Port) 제어 API**에 대한 상세 레퍼런스입니다.
XM10 보드 측면에 있는 확장 핀을 사용하여 외부 센서(ADC) 값을 읽거나, 다른 장치(GPIO)를 제어할 수 있습니다. 아두이노(Arduino)와 유사한 직관적인 인터페이스를 제공합니다.

-----

## 1\. 하드웨어 인터페이스 (Hardware Interface)

XM10 보드에는 총 8개의 확장 DIO핀과 4개의 ADC input핀이 제공됩니다. 일부 ADC 핀은 **IMU 모듈**사용 시 핀의 설정이 변경되므로 주의가 필요합니다.

### Pin Map (핀 맵)

| Pin Name (API) | Label (PCB) | Features | Note |
| :--- | :--- | :--- | :--- |
| **`XM_EXT_DIO_1`** | EXT\_GPIO_1 | **DIO** | 디지털 입출력 전용 (아날로그 불가 |
| **`XM_EXT_DIO_2`** | EXT\_GPIO_2 | **DIO** | 디지털 입출력 전용 (아날로그 불가 |
| **`XM_EXT_DIO_3`** | EXT\_GPIO_3 | **DIO** | 디지털 입출력 전용 (아날로그 불가 |
| **`XM_EXT_DIO_4`** | EXT\_GPIO_4 | **DIO** | 디지털 입출력 전용 (아날로그 불가) |
| **`XM_EXT_DIO_5`** | EXT\_GPIO_5 | **DIO** | 디지털 입출력 전용 (아날로그 불가 |
| **`XM_EXT_DIO_6`** | EXT\_GPIO_6 | **DIO** | 디지털 입출력 전용 (아날로그 불가 |
| **`XM_EXT_DIO_7`** | EXT\_GPIO_7 | **DIO** | 디지털 입출력 전용 (아날로그 불가 |
| **`XM_EXT_DIO_8`** | EXT\_GPIO_8 | **DIO** | 디지털 입출력 전용 (아날로그 불가) |
| **`XM_EXT_ADC_1`** | EXT\_ADC_1 | **ADC** | 12bit 아날로그 입력 전용 (10kHz Sampling Time) **[주의]** IMU 모듈 사용 시 UART TX로 점유됨 (사용 불가) |
| **`XM_EXT_ADC_2`** | EXT\_ADC_2 | **ADC** | 16bit 아날로그 입력 전용 (10kHz Sampling Time) |
| **`XM_EXT_ADC_3`** | EXT\_ADC_3 | **ADC** | 12bit 아날로그 입력 전용 (10kHz Sampling Time) **[주의]** IMU 모듈 사용 시 UART RX로 점유됨 (사용 불가) |
| **`XM_EXT_ADC_4`** | EXT\_ADC_4 | **ADC** | 16bit 아날로그 입력 전용 (10kHz Sampling Time) |

> **Warning:** `XM_EXT_ADC_1`과 `XM_EXT_ADC_3`는 `EnableExternalImu()`가 활성화되어 있으면 **ADC 설정이 자동으로 UART로 변경**됩니다.

<div align="center">
  <img src="https://github.com/user-attachments/assets/a3c05839-4a08-41ce-86ec-beb0c0863d07" width="90%" />
  <p><b>▲ Figure 1. Board Overview</b></p>
</div>

-----

## 2\. 데이터 구조 (Enumerations)

### 2.1. Identifiers (식별자)

#### `XmDioPin_t`

디지털 입출력을 제어할 핀 번호를 지정합니다.

```c
typedef enum {
    XM_EXT_DIO_1 = 0, // PF3
    XM_EXT_DIO_2,     // PF4
    XM_EXT_DIO_3,     // PF5
    XM_EXT_DIO_4,     // PF6
    XM_EXT_DIO_5,     // PF7
    XM_EXT_DIO_6,     // PF8
    XM_EXT_DIO_7,     // PF9
    XM_EXT_DIO_8,     // PF10
    XM_EXT_DIO_COUNT
} XmDioPin_t;
```

#### `XmAdcPin_t`

아날로그 입력을 제어할 핀 번호를 지정합니다.

```c
typedef enum {
    XM_EXT_ADC_1 = 0, // PA0 [Shared] ADC / UART4_TX (IMU 사용 시 GPIO 불가)
    XM_EXT_ADC_2,     // PA0_C
    XM_EXT_ADC_3,     // PA1 [Shared] ADC / UART4_RX (IMU 사용 시 GPIO 불가)
    XM_EXT_ADC_4,     // PA1_C
    XM_EXT_ADC_COUNT
} XmAdcPin_t;
```

### 2.2. Configuration Types (설정 타입)

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
  * **Returns**: 0 \~ 65535 (0V \~ 3.3V에 대응)
  * **Note**: 하드웨어 ADC 해상도가 12비트여도, API는 항상 **16비트로 정규화**된 값을 반환합니다.
