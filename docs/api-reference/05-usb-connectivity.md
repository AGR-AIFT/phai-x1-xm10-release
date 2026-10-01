# API Reference: USB Connectivity

> 📌 **이 페이지를 읽고 나면**: USB-CDC 텍스트/바이너리 송수신을 모두 다룰 수 있습니다.
> ⏱️ 예상 학습 시간: 20분
> 🧰 사전 지식: [Ex.07~09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/07_CDC_Basic_Print/) CDC
> 🎯 핵심 함수: `XM_SendUsbDebugMessage` / `XM_SetUsbCustomMeta` / `XM_SendUsbDataWithId`
>
> ⚠️ **USB-CDC 단일 점유**: **PC 프로그램은 한 번에 하나만 연결하세요.** PhAI Studio · `xm10` 도구 · 시리얼 터미널 (PuTTY/RealTerm 등) 을 같은 COM 포트로 동시에 열면 포트 충돌로 데이터가 손실됩니다.

`xm_api_usb.h`에 정의된 **USB-CDC 통신 API**에 대한 상세 레퍼런스입니다.
XM10은 USB 포트를 통해 PC와 가상 시리얼 포트(CDC)로 연결하여 실시간 데이터 전송 및 디버깅을 제공합니다.

> **USB 메모리(MSC) 파일 로깅 기능은 v2.5.0 에서 제거되었습니다.** 데이터 수집은 USB-CDC 실시간 스트리밍으로 합니다 — PhAI Studio, 또는 레포 내 `xm10` 도구(그래프 + 무손실 `.xmlog` 저장 + CSV 내보내기, [안내](../getting-started/04-pc-data-tool.md)). PhAI Studio 는 자동 전송되는 Total Data(0x20)를 보여 줍니다. PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요 (`XM_SendUsbDataWithId` 로 보내는 0xF0~0xFE 채널이 여기에 해당합니다). 온보드 저장(SD카드)은 현재 지원하지 않습니다.

이 모듈은 시스템이 백그라운드에서 Total Data(0x20)를 PC로 자동 전송하고, 사용자는 추가로 보내고 싶은 채널을 `XM_SendUsbDataWithId()` 로 보내는 구조입니다.

-----

## 1\. 동작 원리 (Operating Principle)

USB 모듈은 사용자의 개입을 최소화하기 위해 **설정(Setup) 후 자동 실행(Automation)** 방식을 따릅니다.

### 자동 전송 흐름 (The Automation Cycle)

PC 프로그램이 COM 포트를 열면 Total Data(0x20)가 1ms마다 자동으로 전송됩니다. 더 보낼 값은 `XM_SetUsbCustomMeta()`(Setup) + `XM_SendUsbDataWithId()`(Loop)로 보냅니다 ([Ex.09](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/)).

-----

## 2\. 함수 상세 (Function Reference)

### 2.1. Data Source Registration (데이터 등록)

(Deprecated) 호출하지 않아도 됩니다. 등록하지 않으면 이 경로로는 아무것도 전송되지 않습니다 — `XM` 전체 상태는 별도로 자동 전송되는 Total Data(0x20)에 들어 있습니다.

#### `XM_SetUsbStreamSource`

> ⚠️ **Deprecated** — Total Data(0x20) 자동 전송 / `XM_SendUsbDataWithId` 로 대체되었습니다.

**[CDC용]** PC로 실시간 전송할 데이터의 소스를 지정합니다.

  * **Syntax**
    ```c
    void XM_SetUsbStreamSource(void* data_ptr, uint32_t size);
    ```
  * **Parameters**
      * `data_ptr`: 전송할 구조체의 주소
      * `size`: 구조체의 크기
  * **Note**: 등록한 데이터는 스트리밍이 활성화되면 PhAI 패킷(바이너리, 기본 Module ID 0x10)으로 PC에 전송되므로 Serial Plotter·터미널로는 읽을 수 없습니다.

-----

### 2.2. CDC Control (디버그 및 스트리밍)

PC와 시리얼 통신을 수행합니다.

#### `XM_SendUsbData`

> ⚠️ **Deprecated** — Total Data(0x20) 자동 전송 / `XM_SendUsbDataWithId` 로 대체되었습니다.

데이터 구조체를 PhAI 패킷으로 감싸 PC로 보냅니다(터미널에서는 읽을 수 없습니다). 사용자가 원하는 시점에 직접 호출해서 보냅니다.

  * **Syntax**
    ```c
    bool XM_SendUsbData(const void* data, uint32_t len);
    ```
  * **Parameters**
      * `data`: 전송할 구조체 포인터
      * `len`: 데이터 길이 (바이트 수)

#### `XM_SendUsbDebugMessage`

PC 터미널(TeraTerm 등)로 \*\*문자열(Text)\*\*을 전송합니다. `printf`와 유사하게 디버깅 용도로 사용합니다.

> ⚠️ PhAI Studio 나 `xm10` 도구가 스트림을 받는 중에 텍스트를 보내면 데이터 패킷 1개가 손실됩니다(메시지 1회당 1개). 텍스트 로그는 일반 터미널로 볼 때 쓰세요. 텍스트만 쓸 때는 `Control_Setup()` 에서 Rev2.0 은 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)`, Rev1.1 은 `XM_SetUsbAutoStream(false)` 를 부르면 깨끗하게 보입니다.

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

USB 장치(CDC)가 준비됐는지 돌려줍니다. 케이블이 꽂히고 USB 초기화가 끝나면 `true` 이며, PC 프로그램이 포트를 열었는지와는 무관합니다. 포트가 열려 스트리밍 중인지는 `XM_IsUsbStreamingActive()` 로 확인하세요 (Rev2.0 에서 `XM_USB_HOST_TERMINAL` 프로파일을 쓰면 `XM_IsUsbStreamingActive()` 는 포트를 열어도 `false` 이고, `XM_IsUsbStreamConnected()` 는 그대로 `true` 입니다).

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

PC 프로그램이 COM 포트를 열면 Total Data(0x20)를 자동으로 스트리밍하는 모드를 설정합니다 (기본 `true`. Rev2.0 에서 `XM_USB_HOST_TERMINAL` 프로파일을 고르면 꺼집니다). USB 케이블을 꽂는 것만으로는 시작되지 않습니다.

  * **Syntax**
    ```c
    void XM_SetUsbAutoStream(bool enabled);
    ```
  * **Parameters**
      * `enabled`: `true`이면 COM 포트가 열릴 때 자동 스트리밍 시작, `false`이면 `AGRB MON START` 명령 대기 (Legacy)

#### `XM_SetUsbStreamModuleId` *(v2.0.0 신규)*

> ⚠️ **Deprecated** — Total Data(0x20) 자동 전송 / `XM_SendUsbDataWithId` 로 대체되었습니다.

예전 스트림(`XM_SetUsbStreamSource`, `XM_SendUsbData`)이 쓰는 Module ID 를 설정합니다(기본 `0x10`). PhAI Studio 연동에는 필요하지 않습니다.

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
  * **Note**: `Control_Loop()` 의 한 틱 안에서 반환값이 0 이 될 때까지 반복해서 읽으세요. 남겨 두면 같은 틱 안에 시스템이 가져가 다시 받을 수 없습니다.

-----

### 2.3. System Interface

#### `XM_SetUsbCustomMeta`

User Custom 채널(Module ID `0xF0`~`0xFE`)의 채널 이름·단위를 JSON 으로 등록합니다. `xm10` 도구가 그래프 제목과 CSV 열 이름으로 씁니다. 채널 이름은 연결할 때 한 번 전달됩니다. 이름이 안 보이면 USB 케이블을 다시 꽂고 다시 연결하세요. JSON 문자열은 512바이트 이하여야 합니다. 넘으면 채널 이름이 전송되지 않습니다(채널이 많거나 이름이 길면 짧게 줄이세요). 연결당 하나만 유지됩니다(마지막 호출이 덮어씀). Rev2.0 에서 `XM_USB_HOST_TERMINAL` 프로파일을 고르면 전송하지 않습니다.

  * **Syntax**
    ```c
    void XM_SetUsbCustomMeta(uint8_t module_id, const char* json_str);
    ```
  * **Parameters**

    | 이름 | 설명 |
    |------|------|
    | `module_id` | Custom Module ID (0xF0~0xFE) |
    | `json_str` | 채널 정의 JSON 배열 문자열 |

  * **Example**
    ```c
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"angle\",\"unit\":\"deg\"},"
        "{\"name\":\"torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"velocity\",\"unit\":\"deg/s\"}]");
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
    | `module_id` | Module ID (사용자 채널은 `0xF0`~`0xFE`) |

  * **Returns**: `true` = 전송 버퍼에 쌓음(PC 가 받았는지와는 무관), `false` = 버퍼가 가득 찼거나, USB 가 준비되지 않았거나, 인자가 잘못됨(NULL 포인터 · 길이 0 · 1020 바이트 초과)

> **Note**: `XM_SendUsbData()`(deprecated)를 대체합니다. Module ID를 명시적으로 지정하여 다중 스트림을 지원합니다.

#### `XM_USB_ProcessPeriodic`

**[시스템 내부용]** USB 스트리밍 로직을 처리하는 함수입니다.
시스템이 자동으로 호출하므로 **End User는 직접 호출할 필요가 없습니다.**

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
| 시리얼 터미널에 메시지 0줄 | PhAI Studio · `xm10` 도구 등 다른 프로그램과 시리얼 터미널 동시 점유 (COM 충돌) | 다른 클라이언트 모두 종료 후 재연결 |
| COM 포트 자체가 안 생김 | 데이터 통신 X (충전 전용) USB-C 케이블 | 데이터 전송 가능 케이블 사용 + Windows 장치 관리자 확인 |
| `XM_SendUsbDataWithId` 가 자주 `false` 반환 | 전송 버퍼 가득 (드롭 발생) — 구조체가 1020 바이트를 넘으면 항상 `false` | 전송 주기 ↓ (1 kHz → 100 Hz) 또는 구조체 크기 ↓ (1020 바이트 이하) |
| PhAI Studio 에서 내 구조체(0xF0~0xFE)가 안 보임 | PhAI Studio 는 자동으로 보내는 Total Data(0x20)를 보여 줍니다 | PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요 |
| `xm10` 에서 채널 이름이 `ch0, ch1…` 로 나옴 | `XM_SetUsbCustomMeta` 호출 누락, JSON 이 올바른 배열이 아님(문법 오류 포함), 또는 JSON 이 512바이트를 넘음 | Setup 에서 한 줄 JSON 배열로 등록 (jsonlint 로 검증, 512바이트 이하로) |
| 이름을 등록했는데도 `xm10` 에서 `ch0, ch1…` 로 나옴 | 연결할 때 채널 이름을 받지 못함 | USB 케이블을 다시 꽂고 `xm10` 으로 다시 연결 (PC 프로그램은 한 번에 하나만 연결하세요) |
| sprintf `%f` 출력이 정수처럼 | newlib-nano (기본) 가 `%f` 미지원 | `Project Properties > MCU Settings > Use float with printf` 체크 |
| 한글 메시지 깨짐 | 터미널 인코딩 UTF-8 아님 | PuTTY: Translation > UTF-8 |
