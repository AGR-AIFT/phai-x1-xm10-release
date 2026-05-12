# API Reference: USB Connectivity

> 📌 **이 페이지를 읽고 나면**: USB-CDC 텍스트/바이너리 송수신 + USB-MSC 로깅 등록을 모두 다룰 수 있습니다.
> ⏱️ 예상 학습 시간: 25분
> 🧰 사전 지식: [Ex.07~09](../../examples/07_CDC_Basic_Print/) CDC + [Ex.10~10c](../../examples/10a_MSC_Basic_Log/) MSC
> 🎯 핵심 함수: `XM_SendUsbDebugMessage` / `XM_SetUsbCustomMeta` / `XM_SendUsbDataWithId` / `XM_SetUsbLogSource` / `XM_StartUsbDataLog`
>
> ⚠️ **USB-CDC 단일 점유**: PhAI Studio 와 시리얼 터미널 (PuTTY/RealTerm 등) 을 같은 COM 포트로 **동시 사용 금지** — COM 포트 충돌로 데이터 손실.

`xm_api_usb.h`에 정의된 **USB 통신 및 데이터 관리 API**에 대한 상세 레퍼런스입니다.
XM10은 USB 포트를 통해 두 가지 강력한 기능을 동시에 제공합니다:

1.  **MSC (Mass Storage Class):** USB 메모리(Flash Drive)에 데이터를 파일(`bin`)로 저장.
2.  **CDC (Communication Device Class):** PC와 가상 시리얼 포트로 연결하여 실시간 데이터 전송 및 디버깅.

이 모듈은 사용자가 **원하는 데이터 구조체**를 등록하면, 시스템이 백그라운드에서 자동으로 저장하고 전송하는 등록 기반(Registration-based) 자동화 시스템을 갖추고 있습니다.

-----

## 1\. 동작 원리 (Operating Principle)

USB 모듈은 사용자의 개입을 최소화하기 위해 **설정(Setup) 후 자동 실행(Automation)** 방식을 따릅니다.

### The Automation Cycle

1.  **등록 (Registration):** 사용자가 `User_Setup()`에서 저장하고 싶은 데이터 구조체(예: `MyData`)의 주소를 시스템에 알려줍니다.
2.  **제어 (Control):** 사용자가 `XM_StartUsbDataLog()`나 시리얼 통신으로 `AGRB MON START`을 호출하여 기능을 켭니다.
3.  **자동 처리 (Processing):** `core_process` 엔진이 2ms마다 `XM_USB_ProcessPeriodic()`을 호출합니다.
      * 이때 시스템은 등록된 구조체의 데이터를 **자동으로 복사**하여 USB 메모리에 쓰거나 PC로 전송합니다.
      * 사용자는 루프마다 `Log()`나 `Send()` 함수를 호출할 필요가 없습니다.

-----

## 2\. 데이터 구조 (Enumerations)

### 2.1. Status Types

#### `XmUsbStatus_t` (권장)

함수 실행 결과를 명확히 알리기 위한 반환 타입입니다.

```c
typedef enum {
    XM_LOG_STATUS_IDLE,      // 중지됨 (초기 상태)
    XM_LOG_STATUS_LOGGING,   // 정상 로깅 중
    XM_LOG_STATUS_WARNING_QUEUE_FULL, // 큐가 90% 참 (f_write 멈춤 발생 중)
    XM_LOG_STATUS_ERROR_STOPPED,    // 큐 오버플로우로 로깅이 강제 중지됨
} XmLogStatus_e;
```

-----

## 3\. 함수 상세 (Function Reference)

### 3.1. Data Source Registration (데이터 등록)

가장 먼저 호출해야 하는 함수들입니다. 등록하지 않으면 기본값(`XM` 전체 구조체)이 사용되거나 아무것도 저장되지 않을 수 있습니다.

#### `XM_SetUsbLogSource`

**[MSC용]** USB 파일에 저장할 데이터의 소스(Source)를 지정합니다.

  * **Syntax**
    ```c
    void XM_SetUsbLogSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: 저장할 사용자 구조체의 주소 (예: `&myData`)
      * `size`: 구조체의 크기 (`sizeof(myData)`)
  * **Example**
    ```c
    typedef struct { uint32_t time; float angle; } MyLog_t;
    MyLog_t myLog;

    void User_Setup() {
        // 이 구조체를 매 주기마다 파일에 저장하겠다고 등록
        XM_SetUsbLogSource(&myLog, sizeof(MyLog_t));
    }
    ```

#### `XM_SetUsbStreamSource`

**[CDC용]** PC로 실시간 전송할 데이터의 소스를 지정합니다. MSC용 소스와 달라도 상관없습니다.

  * **Syntax**
    ```c
    void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: 전송할 구조체의 주소
      * `size`: 구조체의 크기
  * **Note**: 이 함수로 등록된 데이터는 `XM_StartUsbStream()` 호출 시 **바이너리(Binary)** 형태로 PC에 전송됩니다. (Serial Plotter 등에 적합)

-----

### 3.2. MSC Control (데이터 로깅)

USB 메모리에 파일을 생성하고 데이터를 기록하는 기능입니다.

#### `XM_StartUsbDataLog`

데이터 로깅을 시작합니다.

  * **Syntax**
    ```c
    bool XM_StartUsbDataLog(const char* sessionName, const char* metadata);
    ```
  * **Parameters**
      * `sessionName`: 생성할 폴더의 접두어. (예: "Walk" -\> `Walk/data_000_part_000.bin`, `data_001_part_000.bin`...)
      * `metadata`: 파일 헤더(첫 줄)에 기록할 문자열. bin 컬럼명 등을 적기에 좋습니다. (NULL 가능)
  * **Returns**: `true` (시작 성공), `false` (USB 없음 또는 오류)
  * **Example**
    ```c
    // 버튼을 누르면 "Test_xx.csv" 파일을 만들고, 헤더를 "Time,Angle"로 적음
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        XM_StartUsbDataLog("Test", "Time_ms, Angle_deg");
    }
    ```

#### `XM_StopUsbDataLog`

로깅을 중단하고 파일을 저장(Close & Sync)합니다.
**[주의]** USB를 뽑기 전에 반드시 이 함수를 호출해야 데이터가 깨지지 않습니다.

  * **Syntax**
    ```c
    void XM_StopUsbDataLog(void);
    ```

#### `XM_IsUsbLogReady`

USB 메모리가 인식되었고 파일 시스템이 준비되었는지 확인합니다.

  * **Syntax**
    ```c
    bool XM_IsUsbLogReady(void);
    ```
  * **Returns**: `true` (준비됨), `false` (연결 안 됨)

#### `XM_GetUsbLogStatus` *(v2.0.0 신규)*

현재 USB 로깅 상태를 세분화된 enum으로 반환합니다.

  * **Syntax**
    ```c
    XmLogStatus_e XM_GetUsbLogStatus(void);
    ```
  * **Returns**: `XM_LOG_STATUS_IDLE`, `XM_LOG_STATUS_LOGGING`, `XM_LOG_STATUS_WARNING_QUEUE_FULL`, `XM_LOG_STATUS_ERROR_STOPPED`

#### `XM_SetUsbLogAutoTimestamp` *(v2.0.0 신규)*

로그 파일에 타임스탬프를 자동 삽입합니다.

  * **Syntax**
    ```c
    void XM_SetUsbLogAutoTimestamp(bool enable);
    ```
  * **Parameters**
      * `enable`: `true`이면 각 샘플에 시스템 시간(ms)을 자동 삽입

#### `XM_SetUsbLogRollingSize` *(v2.0.0 신규)*

로그 파일 롤링(자동 분할) 크기를 설정합니다. 설정된 크기를 초과하면 새 파일이 자동 생성됩니다.

  * **Syntax**
    ```c
    void XM_SetUsbLogRollingSize(uint32_t bytes);
    ```
  * **Parameters**
      * `bytes`: 파일 분할 크기 (바이트). 0이면 롤링 비활성화

-----

### 3.3. CDC Control (디버그 및 스트리밍)

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

### 3.4. System Interface

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

**[시스템 내부용]** USB 로깅 및 스트리밍 로직을 처리하는 함수입니다.
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
| [00_Quick_Start](../../examples/00_Quick_Start/) | 입문 | 디버그 메시지 전송 |
| [07_CDC_Basic_Print](../../examples/07_CDC_Basic_Print/) | 초급 | 텍스트 메시지 |
| [08_CDC_Sensor_Print](../../examples/08_CDC_Sensor_Print/) | 초급 | sprintf 포맷팅 |
| [09_CDC_Stream](../../examples/09_CDC_Stream/) | 중급 | PhAI V2 바이너리 스트리밍 |
| [18_Debug_Monitor](../../examples/18_Debug_Monitor/) | 중급 | Health 대시보드 |

### MSC (데이터 로깅)
| 예제 | 난이도 | MSC 활용 |
|------|--------|---------|
| [10_MSC_Manual_log](../../examples/10_MSC_Manual_log/) | 레거시 | 기본 로깅 (10a 참조) |
| [10a_MSC_Basic_Log](../../examples/10a_MSC_Basic_Log/) | 초급 | 최소 구조체 + 자동 타임스탬프 |
| [10b_MSC_Custom_Struct](../../examples/10b_MSC_Custom_Struct/) | 중급 | 다중 타입 + 수동 타임스탬프 |
| [10c_MSC_Advanced_Log](../../examples/10c_MSC_Advanced_Log/) | 고급 | TSM + 에러 복구 + 파일 롤링 |
| [19_Memory_Aware_Design](../../examples/19_Memory_Aware_Design/) | 중급 | 메모리 효율적 데이터 관리 |

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 시리얼 터미널에 메시지 0줄 | PhAI Studio + 시리얼 터미널 동시 점유 (COM 충돌) | 다른 클라이언트 모두 종료 후 재연결 |
| COM 포트 자체가 안 생김 | 데이터 통신 X (충전 전용) USB-C 케이블 | 데이터 전송 가능 케이블 사용 + Windows 장치 관리자 확인 |
| `XM_SendUsbDataWithId` 가 자주 `false` 반환 | 버퍼 풀 가득 (drop 발생) | 전송 주기 ↓ (1 kHz → 100 Hz) 또는 구조체 크기 ↓ |
| User Custom 채널 이름이 PhAI 에 안 보임 | `XM_SetUsbCustomMeta` 호출 누락 또는 JSON 문법 오류 | Setup 에서 한 줄 JSON + jsonlint 검증 |
| MSC USB 가 인식 안 됨 | exFAT 또는 NTFS 포맷 | **FAT32** + 32 KB cluster 권장 |
| sprintf `%f` 출력이 정수처럼 | newlib-nano (기본) 가 `%f` 미지원 | `Project Properties > MCU Settings > Use float with printf` 체크 |
| 한글 메시지 깨짐 | 터미널 인코딩 UTF-8 아님 | PuTTY: Translation > UTF-8 |
| MSC 로깅 시작 했는데 .bin 0 byte | `XM_SetUsbLogSource` 누락 | Setup 에서 호출 확인 |
