# API Reference: USB Connectivity

> 📌 **이 페이지를 읽고 나면**: USB-CDC 텍스트/바이너리 송수신을 모두 다룰 수 있습니다.
> ⏱️ 예상 학습 시간: 20분
> 🧰 사전 지식: [Ex.07~09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) CDC
> 🎯 핵심 함수: `XM_SendUsbDebugMessage` / `XM_SetUsbCustomMeta` / `XM_SendUsbDataWithId` / `XM_SetUsbStreamSource` / `XM_SetUsbAutoStream`
>
> ⚠️ **USB-CDC 단일 점유**: PhAI Studio 와 시리얼 터미널 (PuTTY/RealTerm 등) 을 같은 COM 포트로 **동시 사용 금지** — COM 포트 충돌로 데이터 손실.

`xm_api_usb.h`에 정의된 **USB-CDC 통신 API**에 대한 상세 레퍼런스입니다.
XM10은 USB 포트를 통해 PC와 가상 시리얼 포트(CDC)로 연결하여 실시간 데이터 전송 및 디버깅을 제공합니다.

> **USB 메모리(MSC) 파일 로깅 기능은 v2.5.0 에서 제거되었습니다.** 데이터 수집은 USB-CDC 실시간 스트리밍으로 합니다 — PhAI Studio, 또는 레포 내 `xm10` 도구(그래프 + 무손실 `.xmlog` 저장 + CSV 내보내기, [안내](../getting-started/04-pc-data-tool.md)). 온보드 저장(SD카드)은 향후 HW 리비전에서 지원 예정입니다.

이 모듈은 사용자가 **원하는 데이터 구조체**를 등록하면, 시스템이 백그라운드에서 자동으로 PC로 전송하는 등록 기반(Registration-based) 자동화 시스템을 갖추고 있습니다.

-----

## 1\. 동작 원리 (Operating Principle)

USB 모듈은 사용자의 개입을 최소화하기 위해 **설정(Setup) 후 자동 실행(Automation)** 방식을 따릅니다.

### The Automation Cycle

1.  **등록 (Registration):** 사용자가 `Control_Setup()`에서 전송하고 싶은 데이터 구조체(예: `MyData`)의 주소를 `XM_SetUsbStreamSource()`로 시스템에 알려줍니다.
2.  **활성화 (Control):** `XM_SetUsbAutoStream(true)`를 호출하거나 PC 측에서 스트림을 시작하면 전송이 켜집니다.
3.  **자동 처리 (Processing):** `core_process` 엔진이 2ms마다 `XM_USB_ProcessPeriodic()`을 호출합니다.
      * 이때 시스템은 등록된 구조체의 데이터를 **자동으로 복사**하여 PC로 전송합니다.
      * 사용자는 루프마다 `Send()` 함수를 호출할 필요가 없습니다.

-----

## 2\. 함수 상세 (Function Reference)

### 2.1. Data Source Registration (데이터 등록)

가장 먼저 호출해야 하는 함수입니다. 등록하지 않으면 기본값(`XM` 전체 구조체)이 사용됩니다.

#### `XM_SetUsbStreamSource`

**[CDC용]** PC로 실시간 전송할 데이터의 소스를 지정합니다.

  * **Syntax**
    ```c
    void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: 전송할 구조체의 주소
      * `size`: 구조체의 크기
  * **Note**: 이 함수로 등록된 데이터는 스트리밍이 활성화되면 (`XM_SetUsbAutoStream(true)` 또는 PC 측 스트림 시작 시) **바이너리(Binary)** 형태로 PC에 전송됩니다. (Serial Plotter 등에 적합)

-----

### 2.2. CDC Control (디버그 및 스트리밍)

PC와 시리얼 통신을 수행합니다.

#### `XM_SendUsbData`

PC 터미널(TeraTerm 등)로 \*\*데이터 구조체\*\*을 전송합니다.
'AGRB MON START'를 사용하지 않고 사용자가 원하는 대로 데이터를 전송하고자 할 때 제공하는 함수입니다.

  * **Syntax**
    ```c
    bool XM_SendUsbData(const void* data, uint32_t len);
    ```
  * **Parameters**
      * `message`: 전송할 문자열 (Null-terminated)
  * **Example**
    ```c
    typedef struct { uint32_t time; float angle; } MyLog_t;
    MyLog_t myLog;
    XM_SendUsbData(&myLog, sizeof(MyLog_t));
    ```

#### `XM_SendUsbDebugMessage`

PC 터미널(TeraTerm 등)로 \*\*문자열(Text)\*\*을 전송합니다. `printf`와 유사하게 디버깅 용도로 사용합니다.

  * **Syntax**
    ```c
    bool XM_SendUsbDebugMessage(const char* message);
    ```
  * **Parameters**
      * `message`: 전송할 문자열 (Null-terminated)
  * **Example**
    ```c
    char buf[64];
    sprintf(buf, "Current State: %d\r\n", current_state);
    XM_SendUsbDebugMessage(buf);
    ```

#### `XM_IsUsbStreamConnected`

USB 케이블이 PC에 연결되어 가상 시리얼 포트가 열렸는지 확인합니다.

  * **Syntax**
    ```c
    bool XM_IsUsbStreamConnected(void);
    ```

#### `XM_IsUsbStreamingActive` *(v2.0.0 신규)*

현재 CDC 스트리밍이 활성 상태인지 확인합니다.

  * **Syntax**
    ```c
    bool XM_IsUsbStreamingActive(void);
    ```
  * **Returns**: `true` (스트리밍 중), `false` (비활성)

#### `XM_SetUsbAutoStream` *(v2.0.0 신규)*

PC 연결 시 등록된 데이터를 자동으로 스트리밍하는 모드를 설정합니다.

  * **Syntax**
    ```c
    void XM_SetUsbAutoStream(bool enable);
    ```
  * **Parameters**
      * `enable`: `true`이면 연결 감지 시 자동 스트리밍 시작

#### `XM_SetUsbStreamModuleId` *(v2.0.0 신규)*

PhAI V2 프로토콜에서 사용하는 모듈 ID를 설정합니다. PhAI Studio와 연동 시 사용합니다.

  * **Syntax**
    ```c
    void XM_SetUsbStreamModuleId(uint8_t module_id);
    ```
  * **Parameters**
      * `module_id`: PhAI 프로토콜 모듈 식별자

#### `XM_GetUsbData`

PC로부터 데이터를 수신합니다. (키보드 입력 등)

  * **Syntax**
    ```c
    uint32_t XM_GetUsbData(void* buffer, uint32_t max_len);
    ```
  * **Returns**: 실제로 읽어온 바이트 수

-----

### 2.3. System Interface

#### `XM_SetUsbCustomMeta`

PhAI Studio Custom 모드 사용 시, Module ID와 JSON 메타데이터를 등록합니다.

  * **Syntax**
    ```c
    void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
    ```
  * **Parameters**

    | 이름 | 설명 |
    |------|------|
    | `module_id` | Custom Module ID (0xF0~0xFE) |
    | `json_str` | 채널 정의 JSON 문자열 (PhAI Studio V2 호환) |

  * **Example**
    ```c
    XM_SetUsbCustomMeta(0xF0, "{\"ch\":[\"angle\",\"torque\",\"velocity\"]}");
    ```

#### `XM_SendUsbDataWithId`

지정된 Module ID로 바이너리 데이터를 USB CDC 스트리밍합니다.

  * **Syntax**
    ```c
    bool XM_SendUsbDataWithId(const void* data, uint32_t len, uint8_t module_id);
    ```
  * **Parameters**

    | 이름 | 설명 |
    |------|------|
    | `data` | 전송할 데이터 포인터 |
    | `len` | 데이터 길이 (바이트) |
    | `module_id` | Module ID (0x10: COMBINED, 0xF0~0xFE: Custom) |

  * **Returns**: `true` 전송 성공, `false` 실패 (연결 없음 등)

> **Note**: `XM_SendUsbData()`(deprecated)를 대체합니다. Module ID를 명시적으로 지정하여 다중 스트림을 지원합니다.

#### `XM_USB_ProcessPeriodic`

**[시스템 내부용]** USB 스트리밍 로직을 처리하는 함수입니다.
`core_process`에 의해 자동으로 호출되므로 **End User는 직접 호출할 필요가 없습니다.**

  * **Syntax**
    ```c
    void XM_USB_ProcessPeriodic(void);
    ```

---

## 관련 예제

### CDC (시리얼 통신)

| 예제 | 난이도 | CDC 활용 |
|------|--------|---------|
| [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) | 입문 | 디버그 메시지 전송 |
| [07_CDC_Basic_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) | 초급 | 텍스트 메시지 |
| [08_CDC_Sensor_Print](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/08_CDC_Sensor_Print/) | 초급 | sprintf 포맷팅 |
| [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) | 중급 | PhAI V2 바이너리 스트리밍 |
| [18_Debug_Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/) | 중급 | Health 대시보드 |

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 시리얼 터미널에 메시지 0줄 | PhAI Studio + 시리얼 터미널 동시 점유 (COM 충돌) | 다른 클라이언트 모두 종료 후 재연결 |
| COM 포트 자체가 안 생김 | 데이터 통신 X (충전 전용) USB-C 케이블 | 데이터 전송 가능 케이블 사용 + Windows 장치 관리자 확인 |
| `XM_SendUsbDataWithId` 가 자주 `false` 반환 | 버퍼 풀 가득 (drop 발생) | 전송 주기 ↓ (1 kHz → 100 Hz) 또는 구조체 크기 ↓ |
| User Custom 채널 이름이 PhAI 에 안 보임 | `XM_SetUsbCustomMeta` 호출 누락 또는 JSON 문법 오류 | Setup 에서 한 줄 JSON + jsonlint 검증 |
| sprintf `%f` 출력이 정수처럼 | newlib-nano (기본) 가 `%f` 미지원 | `Project Properties > MCU Settings > Use float with printf` 체크 |
| 한글 메시지 깨짐 | 터미널 인코딩 UTF-8 아님 | PuTTY: Translation > UTF-8 |
