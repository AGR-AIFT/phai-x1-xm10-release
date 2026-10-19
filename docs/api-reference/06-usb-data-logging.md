# API 레퍼런스: USB 메모리 저장

> 📌 **이 페이지를 읽고 나면**: 구조체 하나를 USB 메모리에 주기적으로 저장하고, 저장한 폴더를 PC 에서 풀 수 있습니다.
> ⏱️ 예상 학습 시간: 20분
> 🧰 사전 지식: [Ex.01~03](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/01_Button_LED_Basic/) 버튼 · LED · 상태 머신
> 🎯 핵심 함수: `XM_SetUsbLogSource` / `XM_StartUsbDataLog` / `XM_StopUsbDataLog` / `XM_IsUsbLogSessionIdle`
>
> 🟢 **Rev 2.0 전용**: Rev 2.0 보드와 Rev 2.0 SDK 에서만 쓸 수 있습니다. Rev 1.1(v2.8.1)에는 USB 메모리 저장이 없습니다. Rev 1.1 에서는 [05. USB 시리얼](05-usb-connectivity.md)의 실시간 스트리밍으로 데이터를 받으세요.
>
> ⚠️ **USB-C 포트는 하나입니다**: USB 메모리를 꽂아 두는 동안에는 PC 와 USB 로 연결할 수 없습니다. [아래 설명](#usb-c-port)을 먼저 읽어 주세요.
>
> ⚠️ USB 메모리 저장 전체(저장 · 정지 · 뽑기)와 Ex.10 · Ex.34 는 보드에서 시험하기 전입니다. (보드 확인 전) <!-- board-check -->

`xm_api_usb_msc.h` 에 정의된 **USB 메모리 저장 API** 를 설명하는 페이지입니다.
구조체 하나를 등록해 두면, 저장하는 동안 직접 정한 주기마다 시스템이 그 구조체를 USB 메모리에 기록합니다.
기록 함수를 따로 부르지 않습니다. `Control_Loop()` 에서 구조체의 값만 채우면 됩니다.

저장한 폴더를 PC 로 옮겨 XM10 Log Decoder 로 풀면 CSV 가 나옵니다. 빠진 표본이 있으면 그 위치와 길이를 알려 줍니다.

> v2.5.0 에서 뺐던 USB 메모리 저장을 다시 제공합니다. 이번에는 빠진 표본이 통계 · `summary.txt` · 디코더 판정에 보입니다.

함수마다 정확한 시그니처는 [`xm_api_usb_msc.h` 레퍼런스](ref/xm-api-usb-msc.md)에, 처음부터 따라 하는 순서는 [USB 메모리에 저장하기](../getting-started/05-usb-memory-log.md)에 있습니다.

-----

## 언제 쓰나

| 하고 싶은 것 | 방법 |
|------|------|
| PC 를 연결한 채 실시간으로 보고 저장 | [05. USB 시리얼](05-usb-connectivity.md) — `xm10` 도구로 내 구조체를 봅니다 |
| PC 를 연결하지 않고 저장하고, 나중에 PC 에서 풀기 | 이 페이지 — USB 메모리 저장 |

저장하는 동안 PC 는 필요 없습니다. 보드 전원은 KIT H10 본체에서 들어오므로, H10 본체의 전원은 켜 둡니다.

-----

## USB 메모리 준비

| 항목 | 내용 |
|------|------|
| 파일 시스템 | **FAT32**. exFAT 과 NTFS 는 인식하지 않습니다 |
| 할당 단위 | 32 KB 를 권장합니다 |
| 빈 공간 | 파일을 나누는 크기(기본 10 MB)보다 커야 저장을 시작합니다 |
| 세션 이름 | 폴더 이름이 됩니다. 영문, 숫자, `_`, `-` 만 쓰세요 |

- 포맷하면 USB 메모리 안의 파일이 모두 지워집니다. 필요한 파일은 먼저 옮기세요.
- 32 GB 를 넘는 USB 메모리는 Windows 기본 포맷 창에서 FAT32 를 고를 수 없습니다. 32 GB 이하 메모리를 쓰세요.
- 남은 공간은 `XM_GetUsbDiskFreeMB()` 로 볼 수 있습니다. 꽂은 직후에는 잠깐 0 입니다.
- USB 메모리마다 저장 속도가 달라서 빠지는 표본 수가 다를 수 있습니다. (보드 확인 전) <!-- board-check -->

-----

<a id="usb-c-port"></a>
## USB-C 포트는 하나입니다

XM10 의 USB-C 포트는 하나입니다. PC 와 USB 메모리를 동시에 쓸 수 없습니다.

- USB 메모리를 꽂으면 보드가 USB 메모리에 전원을 공급하고, 읽고 씁니다.
- USB 메모리를 꽂아 두는 동안에는 PhAI Studio, `xm10` 도구, USB 로 하는 펌웨어 업로드를 쓸 수 없습니다.
- 그동안 `XM_IsUsbStreamConnected()` 는 `false` 이고, `XM_SendUsbDebugMessage()` 같은 USB-CDC 송신 함수는 `false` 를 돌려줍니다. 저장하는 동안에는 PC 로 메시지를 보낼 수 없습니다.
- 저장하는 동안의 상태는 보드의 LED 로 봅니다. 예제의 상태 LED 는 LED 1 · 2 · 3 입니다.
- 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요. (보드 확인 전) <!-- board-check -->

-----

## 쓰는 순서

1. `Control_Setup()` 에서 구조체를 등록하고 주기를 정합니다. 구조체는 전역 또는 `static` 이어야 합니다. 주기를 정하지 않으면 1000 Hz 입니다.
2. `Control_Loop()` 에서 구조체의 값을 채웁니다. 저장은 `Control_Loop()` 가 끝난 뒤 시스템이 합니다.
3. `XM_IsUsbLogReady()` 와 `XM_IsUsbLogSessionIdle()` 이 모두 `true` 일 때 `XM_StartUsbDataLog()` 로 시작합니다. `false` 가 돌아오면 `XM_GetUsbLogLastStartError()` 로 이유를 봅니다.
4. `XM_StopUsbDataLog()` 로 멈추고 `XM_IsUsbLogSessionIdle()` 이 `true` 가 될 때까지 기다립니다.
5. `true` 가 되면 파일이 모두 닫힌 것이므로 USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check -->

[Ex.10](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/)의 핵심만 줄인 예입니다. 전체 코드(상태 머신 · LED · 마커 · 통계)는 예제에 있습니다. 예제는 버튼 1 하나로 시작과 정지를 번갈아 하고, 아래 예는 버튼 2 를 정지에 씁니다.

```c
/* 저장할 구조체 — 필드 사이에 빈 바이트가 끼지 않게 packed 로 선언합니다 */
typedef struct __attribute__((packed)) {
    uint32_t loop_count;
    float    sine;
    float    ramp;
    uint8_t  btn1;
    uint8_t  btn2;
    uint8_t  btn3;
    uint8_t  h10_connected;
} LogRecord_t;
_Static_assert(sizeof(LogRecord_t) == 16U, "LogRecord_t 크기가 바뀌었습니다 - 메타 줄도 같이 고치세요");

/* 메타 줄 — 필드를 선언 순서대로 "이름(타입)" 으로 적고 쉼표로 잇습니다 */
#define LOG_METADATA \
    "loop_count(uint32_t), sine(float), ramp(float), " \
    "btn1(uint8_t), btn2(uint8_t), btn3(uint8_t), h10_connected(uint8_t)"

static LogRecord_t s_log;                                    /* 전역 또는 static */
volatile XmUsbLogStartError_e g_last_start_error = XM_USB_LOG_START_OK;

void Control_Setup(void)
{
    XM_SetUsbLogSource(&s_log, sizeof(s_log));               /* 저장할 구조체를 등록합니다 */
    XM_SetUsbLogSampleRateHz(100);                           /* 100 Hz — 생략하면 1000 Hz */
}

void Control_Loop(void)
{
    s_log.loop_count++;                                      /* 값만 채웁니다. 저장은 시스템이 합니다 */

    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {       /* 버튼 1: 시작 */
        if (XM_IsUsbLogReady() && XM_IsUsbLogSessionIdle()) {
            if (!XM_StartUsbDataLog("", LOG_METADATA)) {     /* "" 이면 시계로 폴더 이름을 만듭니다 */
                g_last_start_error = XM_GetUsbLogLastStartError();   /* 시작하지 못한 이유 */
            }
        }
    }

    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {       /* 버튼 2: 정지 요청 */
        XM_StopUsbDataLog();                                 /* 파일은 뒤에서 닫습니다 */
    }
}
```

- 설정 · 시작 · 정지 · 마커 · 통계 함수는 `Control_Setup()` · `Control_Loop()` 에서만 부릅니다. 백그라운드 태스크나 인터럽트에서 부르면 아무것도 하지 않고 `false` 를 돌려줍니다.
- 상태를 읽는 함수(`XM_IsUsbLog…` · `XM_GetUsbLogLastStartError` · `XM_GetUsbDisk…`)는 어디서든 불러도 됩니다.
- `XM_StartUsbDataLog()` 는 기다리지 않고 바로 돌아옵니다. `true` 는 "시작 요청을 받았다" 는 뜻입니다. 파일이 열리면 `XM_IsUsbLogRecording()` 이 `true` 가 됩니다.
- 파일을 열다가 실패하면 `XM_IsUsbLogSessionIdle()` 이 다시 `true` 가 되고, 이유는 `XM_GetUsbLogLastStartError()` 에 남습니다.
- 시작하자마자 바로 멈추면 기록이 없는 빈 폴더가 남을 수 있습니다.
- 저장 중에 표시를 남기려면 `XM_InsertUsbLogMarker(type, value)` 를 부릅니다. `type` 은 1 ~ 254 중에서 뜻을 정해 쓰고, `value` 는 함께 남길 값입니다. 디코더가 표시를 `events.csv` 로 따로 모아 줍니다.

-----

## 저장 주기와 구조체 크기

- 저장 주기는 1000 의 약수입니다: 1000 · 500 · 250 · 200 · 125 · 100 · 50 · 40 · 25 · 20 · 10 · 8 · 5 · 4 · 2 · 1 Hz. 정하지 않으면 1000 Hz 입니다.
- 1000 Hz 미만은 주기마다 한 번만 저장하고, 그 사이 값은 저장하지 않습니다. 이때는 타임스탬프를 켜 두어야 합니다(기본값).
- 주기에 비해 구조체가 크면 시작하지 않고 `XM_USB_LOG_START_TOO_BIG_FOR_RATE` 를 돌려줍니다.
- 구조체를 작게 만들면 내부 버퍼 여유가 늘어납니다. 꼭 필요한 값만 담으세요.
- 1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다. 빠진 수와 가장 긴 공백은 `XM_GetUsbLogStats()` 와 디코더 결과에 나옵니다.
- 디코더 보고서에 "내부 버퍼 여유: 부족" 이 나오면 구조체를 줄이거나 저장 주기를 낮춰 보세요.

타임스탬프를 켠 기본 설정에서 시작할 수 있는 구조체 크기입니다.

| 저장 주기 | 시작할 수 있는 구조체 크기 | 예제 |
|------|------|------|
| 1000 Hz (기본) | 134 B 까지 | Ex.34 (96 B) |
| 200 Hz 이하 | 512 B 까지 | Ex.10 (16 B, 100 Hz) |
| 500 Hz · 250 Hz | 주기에 따라 다릅니다. 너무 크면 시작하지 않습니다 | — |

-----

<a id="meta-line"></a>
## 메타 줄

메타 줄은 구조체의 필드를 선언 순서대로 `이름(타입)` 으로 적고 쉼표로 이은 한 줄입니다. 디코더는 이 줄로 데이터를 풉니다.

```c
"loop_count(uint32_t), hip_L(float), hip_R(float), contact(uint8_t), _pad(3bytes)"
```

- 타입은 `uint8_t` `int8_t` `uint16_t` `int16_t` `uint32_t` `int32_t` `float` `double` `bool` 이고, 소문자 그대로 씁니다.
- 비워 둔 바이트는 `_이름(Nbytes)` 로 적습니다. 예: `_pad(3bytes)`. 이름이 `_` 로 시작해야 채움입니다.
- 이름은 영문 · 숫자 · `_` 만 쓰고, `이름(타입)` 안에는 공백을 넣지 않고, 한 줄로 적습니다.
- 필드 크기의 합이 등록한 구조체 크기와 다르면 시작하지 않고 `XM_USB_LOG_START_METADATA_MISMATCH` 를 돌려줍니다.
- 시스템은 크기의 합만 확인합니다. 이름 · 타입 · 순서가 구조체와 같은지는 확인하지 않으니 직접 맞추세요.
- 구조체는 `__attribute__((packed))` 로 선언하고 `_Static_assert` 로 크기를 고정해 두세요.
- 디코더가 따로 쓰는 이름(`tick_ms` · `t_grid_ms` · `row_origin` · `count` · `h10_loop_count`)은 그 뜻이 아니면 필드 이름으로 쓰지 마세요.
- 메타 줄은 1700 자까지입니다(`XM_USB_LOG_MAX_METADATA_LEN`).

-----

## 파일

저장한 파일은 USB 메모리의 `/LOGS/<세션 이름>/` 폴더에 생깁니다. 세션 이름을 비우면 시계(RTC)로 `YYMMDD_HHMMSS` 를 만듭니다.

```text
/LOGS/<세션 이름>/
    metadata.txt             메타 줄과 저장 설정
    data_000_part_000.bin    저장한 데이터
    data_000_part_001.bin    저장한 데이터의 다음 파트
    summary.txt              저장을 끝낼 때 만들어지는 요약
```

- 파일이 정한 크기(기본 10 MB)를 넘으면 새 파트로 나뉩니다. (보드 확인 전) <!-- board-check -->
- 폴더째로 디코더에 넣으면 파트를 이어서 풉니다.
- 같은 이름의 폴더가 이미 있으면 거기에 이어서 저장합니다. 주기 · 구조체 크기 · 타임스탬프 설정이 같아야 합니다. 다르면 `XM_USB_LOG_START_FOLDER_MISMATCH` 를 돌려줍니다.
- 이어서 저장할 때 `metadata.txt` 는 처음 저장한 것을 그대로 씁니다. 필드 이름이나 타입을 바꿨다면 다른 이름을 쓰세요.
- 시계를 [`XM_RTC_SetDateTime()`](08-rtc-clock.md) 으로 맞추지 않으면 폴더 이름이 실제 날짜와 다르고, 이전 저장과 이름이 겹칠 수 있습니다. 이름을 직접 정해도 됩니다.
- 이름을 비웠는데 시계를 읽지 못하면 시작하지 못하고 `XM_USB_LOG_START_BAD_NAME` 을 돌려줍니다.
- `summary.txt` 는 저장을 끝낼 때 만들어집니다. 저장하는 동안 USB 메모리를 뽑으면 만들어지지 않습니다. (보드 확인 전) <!-- board-check -->
- `summary.txt` 는 먼저 `status` 와 `error_reason` 을 보세요. `error_reason` 이 `NORMAL` 이면 정상으로 끝난 것입니다. 저장 품질은 `slots_empty` · `write_errors` 와 디코더 보고서로 판단하세요. 그 밖의 키는 저장 품질 판단에 쓰지 마세요.

-----

## 안전하게 뽑기

1. `XM_StopUsbDataLog()` 를 부릅니다. `false` 가 돌아오면 조금 뒤 다시 부릅니다.
2. `XM_IsUsbLogSessionIdle()` 이 `true` 가 될 때까지 기다립니다. 그동안 남은 데이터를 쓰고, 파일을 닫고, `summary.txt` 를 씁니다.
3. `true` 가 되면 파일이 모두 닫힌 것이므로 USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check -->

예제는 LED 2 로 보여 줍니다. 빠르게 깜빡이는 동안은 파일을 닫는 중이니 뽑지 마세요. 켜진 채로 있으면 저장이 끝난 것입니다. (보드 확인 전) <!-- board-check -->

저장하는 동안 `XM_IsUsbLogSessionIdle()` 이 스스로 `true` 가 되면 저장이 끝난 것입니다. USB 메모리가 빠졌거나 내부 버퍼가 넘친 경우 등이 있습니다.

- `overflow_records` 가 1 이상이면 그 저장은 거기서 멈춥니다.
- 저장 중에 USB 메모리가 가득 차면 저장이 스스로 멈춥니다. (보드 확인 전) <!-- board-check -->
- 저장 중에 USB 메모리를 뽑으면 파일을 정상으로 닫지 못합니다. 이때 `summary.txt` 는 만들어지지 않고, 지금까지 쓴 부분만 풀 수 있습니다. (보드 확인 전) <!-- board-check -->
- 저장 중에는 USB 메모리를 뽑지 마세요.

-----

## 저장 결과 확인

`XM_GetUsbLogStats()` 로 통계를 읽습니다. 저장 중에는 지금까지의 값이고, 끝나면 그 저장의 마지막 값이 남습니다.

| 필드 | 보는 것 |
|------|------|
| `empty_slots` | 빠진 표본 수. 0 이면 빠진 것이 없습니다 |
| `longest_gap_ms` | 가장 긴 공백 [ms] |
| `overflow_records` | 내부 버퍼가 넘쳐 저장하지 못한 레코드 수. 1 이상이면 그 저장은 거기서 멈춥니다 |
| `write_errors` | USB 메모리 쓰기 실패 횟수 |

저장하는 동안에는 PC 로 통계를 보낼 수 없습니다. 저장이 끝나고 USB 메모리를 뽑은 뒤 PC 를 연결하면 `XM_SendUsbDebugMessage()` 로 보낼 수 있습니다(Ex.10 의 버튼 3). (보드 확인 전) <!-- board-check -->

-----

## PC 에서 풀기

저장한 `LOGS` 폴더를 PC 로 옮겨 **XM10 Log Decoder** 로 풉니다. Windows 에서 쓰는 설치형 프로그램이고, 파이썬은 필요 없습니다. <!-- release-check -->

- 디코더는 릴리스 페이지의 첨부 파일에서 받습니다. <!-- release-check -->
- 디코더는 데이터 표(1000 Hz 는 `decoded_output.csv`, 그 밖의 주기는 `decoded_output_<주기>hz.csv`. 예: 100 Hz 는 `decoded_output_100hz.csv`), 마커가 있으면 `events.csv`, 그리고 `data_quality_report.txt` 를 만듭니다.
- 빠진 표본은 보간으로 메우고 `row_origin` 열에 `1` 로 표시합니다. 보간한 값은 측정값이 아닙니다.
- `data_quality_report.txt` 의 판정이 `GOOD` · `USABLE` · `WARNING` 이면 받아들일 수 있는 저장입니다. `RE-COLLECT` · `INVALID` · `UNVERIFIED` 이면 다시 저장하세요.
- `RE-COLLECT` 가 계속되면 Angel Robotics(엔젤로보틱스) CS팀에 문의해 주세요.

받는 방법, 설치, 판정 읽는 법은 [USB 메모리에 저장하기](../getting-started/05-usb-memory-log.md)에 단계별로 있습니다.

-----

## 예제

| 예제 | 난이도 | 내용 |
|------|--------|------|
| [10_USB_Memory_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/10_USB_Memory_Log/) 🟢 **Rev 2.0 전용** | 중급 | 버튼 1 로 시작 · 정지. 구조체 하나(16 B)를 100 Hz 로 저장 |
| [34_USB_Memory_Gait_Log](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/34_USB_Memory_Gait_Log/) 🟢 **Rev 2.0 전용** | 중급 | H10 보행 데이터를 1 kHz 로 저장(구조체 96 B). 처음이면 Ex.10 먼저 |

- 예제 10 · 34 는 이전 버전에서도 USB 메모리 예제 번호였지만, 지금 예제는 새로 만든 것입니다.
- Ex.10 은 H10 에서 센서 값을 받지 않습니다. 구조체의 `h10_connected` 칸에는 H10 연결 여부(0 또는 1)만 담습니다.
- Ex.10 은 버튼 2 로 저장 중에 표시(마커)를 넣습니다. Ex.34 는 버튼 2 가 마커 1(걷기 시작), 버튼 3 이 마커 2(이벤트)입니다.
- 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결해 시리얼 터미널을 연 뒤 Ex.10 의 버튼 3 을 누르면 마지막 저장의 통계가 나옵니다. (보드 확인 전) <!-- board-check -->
- 저장하는 동안 두 예제는 PC 로 아무것도 보내지 않습니다. 상태는 LED 1 · 2 · 3 으로 봅니다.
- Ex.10 은 일반 터미널용 설정이라 PhAI Studio 의 실시간 화면은 나오지 않습니다. 실시간 그래프는 Ex.09 를 보세요.
- Ex.34 는 H10 을 연결하고 착용자가 입은 뒤 시작하세요. 빌드하기 전에 `_SetupBodyData()` 의 신체 정보(체중 · 키 · 다리 길이)를 착용자의 값으로 고치세요. 예시 값 그대로면 무릎 · 허벅지 각도가 부정확합니다.
- Ex.34 의 기본 설정(`USE_H10_ASSIST 0`)은 기록만 합니다. 토크나 제어 명령을 보내지 않습니다.
- Ex.34 에서 H10 값이 갱신되지 않으면 디코더는 저장 시각(레코드마다 함께 저장되는 시각)으로 시간 기준을 만듭니다. (보드 확인 전) <!-- board-check -->

> ⚠️ **안전**: Ex.34 에서 `USE_H10_ASSIST` 를 1 로 바꾸면 H10 의 기존 보조 알고리즘이 켜져 착용자에게 보조 토크가 걸립니다. 착용자와 주변의 안전을 확인한 뒤에만 켜세요.

-----

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `XM_IsUsbLogReady()` 가 계속 `false` (예제의 LED 1 이 안 켜짐) | USB 메모리가 FAT32 가 아니거나(exFAT · NTFS 는 인식하지 않습니다), 꽂은 직후라 아직 준비 중입니다 | FAT32 로 포맷(할당 단위 32 KB 권장)하고 다시 꽂으세요. 포맷하면 파일이 모두 지워집니다 |
| `XM_StartUsbDataLog()` 가 `false` | 시작 조건이 맞지 않습니다 | `XM_GetUsbLogLastStartError()` 값을 [시작 실패 이유](ref/xm-api-usb-msc.md#xmusblogstarterror_e) 표에서 찾으세요 |
| 시작은 `true` 인데 곧 `XM_IsUsbLogSessionIdle()` 이 `true` 로 돌아옴 | 파일을 여는 중에 실패했습니다(예: 빈 공간 부족 `XM_USB_LOG_START_STORAGE`, 같은 이름의 폴더가 다른 설정 `XM_USB_LOG_START_FOLDER_MISMATCH`) | 같은 방법으로 `XM_GetUsbLogLastStartError()` 를 읽으세요. `XM_StartUsbDataLog()` 를 다시 부르기 전에 읽어야 합니다 |
| `XM_USB_LOG_START_TOO_BIG_FOR_RATE` | 주기에 비해 구조체가 큽니다 | 저장 주기를 낮추거나 구조체를 줄이세요 |
| `XM_USB_LOG_START_METADATA_MISMATCH` | 메타 줄 형식이 틀렸거나, 모르는 타입이 있거나, 필드 크기의 합이 구조체 크기와 다릅니다 | 타입은 소문자로 쓰고, 빈 바이트는 `_pad(3bytes)` 처럼 적고, `_Static_assert` 로 구조체 크기를 고정하세요 |
| `XM_SetUsbLogSampleRateHz()` 같은 설정 함수가 `false` | 저장 세션이 있거나, `Control_Setup()` · `Control_Loop()` 밖에서 불렀거나, 값이 범위 밖입니다 | `XM_IsUsbLogSessionIdle()` 이 `true` 일 때 `Control_Setup()` · `Control_Loop()` 에서 부르세요 |
| 폴더 이름이 실제 날짜와 다르거나 이전 저장과 같은 이름 | 시계를 맞추지 않았을 수 있습니다 | `XM_RTC_SetDateTime()` 으로 시계를 맞추거나 세션 이름을 직접 정하세요 |
| PhAI Studio · `xm10` 도구가 보드를 못 찾음 | USB 메모리가 꽂혀 있습니다 | [USB-C 포트는 하나입니다](#usb-c-port)를 보세요 |
| 디코더 판정이 `RE-COLLECT` | 표본이 많이 빠졌거나 공백이 깁니다 | FAT32(할당 단위 32 KB)로 전체 포맷한 뒤 다시 저장하세요. 그래도 같으면 다른 USB 메모리를 쓰거나 저장 주기를 낮추거나 구조체를 줄이세요 |

-----

## 관련 문서

- [`xm_api_usb_msc.h` 레퍼런스](ref/xm-api-usb-msc.md) — 함수 · 타입 · 상수의 정확한 설명
- [USB 메모리에 저장하기](../getting-started/05-usb-memory-log.md) — 포맷 · 예제 · 디코더까지 단계별로
- [05. USB 시리얼](05-usb-connectivity.md) — PC 에 실시간으로 보내는 방법
- [10. 구조체 로깅](10-struct-logging.md) — 구조체를 PC 로 실시간 전송
- [08. 실시간 시계 (RTC)](08-rtc-clock.md) — 폴더 이름에 쓰는 시계
- [Troubleshooting](../troubleshooting.md)
