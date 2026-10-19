# Ex.10 — USB Memory Log (USB 메모리에 값 저장하기)

> 🎯 **학습 목표**:
> - 구조체 하나를 USB 메모리에 **100 Hz** 로 저장합니다 (`XM_SetUsbLogSource` · `XM_StartUsbDataLog` · `XM_StopUsbDataLog`).
> - 버튼으로 저장을 시작하고 멈추며, 저장하는 중에 마커(표시)를 넣습니다.
> - 저장이 끝난 뒤 통계와 디코더로 빠진 표본이 없는지 확인합니다.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐ | 🟢 **Rev 2.0 전용** (USB 메모리 저장 API)
> 🧰 사전 예제: [Ex.03 Button & LED FSM](../03_Button_LED_FSM/) · [Ex.07 CDC Basic Print](../07_CDC_Basic_Print/) | 📚 관련 docs: [USB 메모리 저장](../../docs/api-reference/06-usb-data-logging.md) · [USB 메모리에 저장하기](../../docs/getting-started/05-usb-memory-log.md)

> 🟢 **Rev 2.0 전용** — USB 메모리 저장 API (`xm_api_usb_msc.h`) 는 Rev 2.0 SDK 에만 있습니다. Rev 1.1 SDK 에는 이 예제가 들어 있지 않습니다. 본인 보드의 리비전은 [보드 리비전 비교](../../docs/hardware/README.md#보드-리비전-비교)에서 확인하세요.

---

## ⚠️ USB-C 포트는 하나입니다

- USB 메모리를 꽂아 두는 동안에는 PhAI Studio · `xm10` 도구 · USB 로 하는 펌웨어 업로드를 쓸 수 없습니다. 이 예제도 저장하는 동안 PC 로 아무것도 보내지 않습니다.
- 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요. (보드 확인 전) <!-- board-check -->
- 저장하는 동안(LED 2 가 깜빡이는 동안)에는 USB 메모리를 뽑지 마세요.
- PC 프로그램은 **한 번에 하나만** 연결하세요 (PhAI Studio · `xm10` 도구 · 시리얼 터미널).

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

FAT32 로 포맷한 USB 메모리를 보드에 꽂고 버튼 1 을 누르면, 구조체 하나(16 B)를 **100 Hz** (10 ms 마다 한 번) 로 저장합니다. (보드 확인 전) <!-- board-check -->
저장하는 동안 PC 는 필요 없고, H10 에서 값을 받지 않아도 됩니다.
보드 전원은 KIT H10 본체에서 들어오므로, H10 본체의 전원은 켜 둡니다.

| 저장하는 값 | 타입 | 내용 |
|------|:---:|------|
| `loop_count` | `uint32_t` | `Control_Loop()` 를 부른 횟수 (1 ms 마다 +1) |
| `sine` | `float` | 합성 사인파 (주기 2 초, 진폭 1) |
| `ramp` | `float` | 합성 톱니파 (0 → 1, 1 초마다 반복) |
| `btn1` · `btn2` · `btn3` | `uint8_t` | 버튼 상태 (0 = 뗌, 1 = 누름) |
| `h10_connected` | `uint8_t` | H10 연결 여부 (H10 이 없으면 0) |

→ 저장한 폴더는 USB 메모리의 `/LOGS/<날짜_시각>/` 입니다. PC 로 옮겨 XM10 Log Decoder 로 풀면 CSV 가 나오고, 빠진 표본이 있으면 그 위치와 길이를 알려 줍니다. 사용법은 [USB 메모리에 저장하기](../../docs/getting-started/05-usb-memory-log.md) 에 있습니다. <!-- release-check -->

| 버튼 | 동작 |
|:---:|------|
| 버튼 1 (짧게) | 저장 시작 / 정지 |
| 버튼 2 | 저장 중에 마커(표시)를 넣습니다. 디코더가 `events.csv` 로 따로 모아 줍니다 |
| 버튼 3 | 마지막 저장의 통계를 PC 로 보냅니다. USB 메모리를 뽑고 PC 를 연결해 시리얼 터미널을 연 뒤에 누르세요. (보드 확인 전) <!-- board-check --> |

| LED | 뜻 |
|:---:|------|
| LED 1 켜짐 | USB 메모리가 준비되었습니다 |
| LED 2 느린 깜빡임 | 저장 중입니다 |
| LED 2 빠른 깜빡임 | 파일을 열거나 닫는 중입니다. USB 메모리를 뽑지 마세요 |
| LED 2 켜짐 | 저장이 끝났습니다. USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check --> |
| LED 3 켜짐 | 오류입니다. 시작하지 못했거나 저장이 스스로 끝났습니다. 버튼 1 을 다시 누르면 꺼집니다 |
| LED 3 짧게 한 번 | 마커를 넣지 못했습니다. 저장은 계속됩니다 |

> ℹ️ 위 표의 LED · 버튼 동작은 예제 코드가 정한 것입니다. 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **USB 메모리** — FAT32 로 포맷하세요 (할당 단위 32 KB 권장). exFAT · NTFS 는 인식하지 않습니다. 포맷하면 안의 파일이 모두 지워지니 필요한 파일은 먼저 옮기세요. 빈 공간이 파일을 나누는 크기(기본 10 MB)보다 커야 저장을 시작합니다.
- **구조체 등록** — `Control_Setup()` 에서 `XM_SetUsbLogSource(&s_log, sizeof(s_log))` 로 저장할 구조체를 등록합니다. 구조체는 전역 또는 `static` 이어야 합니다. 등록한 뒤에는 기록 함수를 따로 부르지 않습니다. `Control_Loop()` 에서 값만 채우면, 저장하는 동안 정한 주기마다 시스템이 USB 메모리에 기록합니다.
- **저장 주기** — `XM_SetUsbLogSampleRateHz()` 로 정합니다. 1000 의 약수(1000 · 500 · 250 · 200 · 125 · 100 · 50 · 40 · 25 · 20 · 10 · 8 · 5 · 4 · 2 · 1 Hz)이고, 정하지 않으면 1000 Hz 입니다. 이 예제는 100 Hz 입니다.
- **메타 줄** — 구조체의 필드를 선언 순서대로 `이름(타입)` 으로 적고 쉼표로 이은 한 줄입니다. 디코더가 이 줄로 값을 풉니다. 필드 크기의 합이 구조체 크기와 다르면 시작하지 않습니다 (`XM_USB_LOG_START_METADATA_MISMATCH`).
- **시작은 기다리지 않습니다** — `XM_StartUsbDataLog()` 가 `true` 이면 "시작 요청을 받았다" 는 뜻입니다. 파일이 열리면 `XM_IsUsbLogRecording()` 이 `true` 가 됩니다. 파일을 열다가 실패하면 `XM_IsUsbLogSessionIdle()` 이 다시 `true` 가 되고, 이유는 `XM_GetUsbLogLastStartError()` 에 남습니다.
- **폴더 이름** — `XM_StartUsbDataLog()` 의 첫 인자가 `""` 이면 시계(RTC)로 `YYMMDD_HHMMSS` 를 만들어 `/LOGS/` 아래에 폴더를 만듭니다. 시계를 `XM_RTC_SetDateTime()` 으로 맞추지 않았다면 이름을 직접 정하세요 (`LOG_SESSION_NAME`). 같은 이름의 폴더가 있으면 거기에 이어서 저장합니다. 이때 폴더의 메타 줄은 처음 저장한 것을 그대로 쓰니, 필드 이름이나 타입을 바꿨다면 다른 이름을 쓰세요.
- **터미널용 설정** — 이 예제는 `XM_USB_SetHostProfile(XM_USB_HOST_TERMINAL)` 로 일반 시리얼 터미널에 맞춥니다. 그래서 PhAI Studio 의 실시간 화면(Total Data)은 나오지 않습니다. 실시간 그래프는 [Ex.09](../09_CDC_Stream/) 를 보세요.
- **지켜야 할 것** — 설정 · 시작 · 정지 · 마커 · 통계 함수는 `Control_Setup()` · `Control_Loop()` 에서만 부릅니다. 다른 태스크나 인터럽트에서 부르면 아무것도 하지 않고 `false` 를 돌려줍니다. 시작은 `XM_IsUsbLogReady()` 와 `XM_IsUsbLogSessionIdle()` 이 모두 `true` 일 때만 부릅니다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef struct __attribute__((packed)) {
    uint32_t loop_count;
    float    sine;
    float    ramp;
    uint8_t  btn1;
    uint8_t  btn2;
    uint8_t  btn3;
    uint8_t  h10_connected;
} LogRecord_t;                                                       // ① 16 bytes

static LogRecord_t s_log;            // ② 저장할 구조체 — 전역 또는 static

/* ③ 메타 줄 — 구조체의 필드를 선언 순서대로 "이름(타입)" 으로 */
#define LOG_METADATA \
    "loop_count(uint32_t), sine(float), ramp(float), " \
    "btn1(uint8_t), btn2(uint8_t), btn3(uint8_t), h10_connected(uint8_t)"

void Control_Setup(void)
{
    /* 상태 머신 등록 생략 */

    XM_SetUsbLogSource(&s_log, sizeof(s_log));       // ④ 저장할 구조체 등록
    XM_SetUsbLogSampleRateHz(100U);                  //    저장 주기 100 Hz
}

void Control_Loop(void)
{
    s_log.loop_count++;                              // ⑤ 값만 채웁니다 — 저장은 시스템이 합니다
    /* … sine · ramp · 버튼 상태 · h10_connected 도 같은 방식으로 채웁니다 … */

    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogSessionIdle()) {              // 저장 세션이 없으면 시작합니다
            if (XM_IsUsbLogReady()) {
                XM_StartUsbDataLog("", LOG_METADATA);   // ⑥ 시작 — 기다리지 않고 바로 돌아옵니다
            }
        } else {
            XM_StopUsbDataLog();                     // ⑦ 정지 — false 이면 조금 뒤 다시 부릅니다 (실제 예제는 다음 루프에 다시 부릅니다)
        }
    }
}
```

전체 코드: [`usb_memory_log.c`](usb_memory_log.c)

> 🧒 실제 예제는 상태 머신(대기 → 저장 → 닫는 중)으로 짜여 있습니다. 위 코드는 흐름만 보이도록 줄인 것입니다.
>
> 🧒 ⑦ 로 정지한 뒤 `XM_IsUsbLogSessionIdle()` 이 `true` 가 되면 파일이 모두 닫힌 것이니 USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check -->

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

아래 ✅ 는 코드가 의도한 동작이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

1. **USB 메모리 준비** — FAT32 (할당 단위 32 KB 권장) 로 포맷합니다. 포맷하면 안의 파일이 모두 지워집니다.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **USB 메모리 꽂기** → ✅ LED 1 켜짐
4. **버튼 1 (짧게)** → ✅ LED 2 가 빠르게 깜빡이다가(파일을 여는 중) 천천히 깜빡입니다(저장 중)
5. **버튼 2 를 몇 번 누르기** → ✅ 마커가 들어갑니다. LED 3 이 짧게 한 번 켜지면 마커를 넣지 못한 것입니다
6. **버튼 1 다시** → ✅ LED 2 가 빠르게 깜빡이는 동안은 파일을 닫는 중이니 기다립니다. LED 2 가 켜진 채로 있으면 저장이 끝난 것이니 USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check -->
7. **통계 보기 (선택)** — USB 메모리를 뽑고 PC 를 연결해 시리얼 터미널을 연 뒤 버튼 3 → ✅ `[USB Log] records=… empty_slots=… longest_gap_ms=…` 한 줄이 나옵니다. `empty_slots` 는 빠진 표본 수(0 이면 빠진 것이 없습니다), `longest_gap_ms` 는 가장 긴 공백 [ms] 입니다. (보드 확인 전) <!-- board-check -->
8. **디코더로 풀기** — USB 메모리의 `/LOGS/<날짜_시각>/` 폴더를 PC 로 옮겨 XM10 Log Decoder 2.2.0 이상으로 풉니다 → ✅ `decoded_output_100hz.csv` · `events.csv` (마커를 넣었다면) · `data_quality_report.txt` 가 만들어집니다. 디코더는 릴리스 페이지의 첨부 파일에서 받습니다. <!-- release-check -->
9. **변형 1 — 저장 주기 바꾸기**: `LOG_SAMPLE_RATE_HZ` 를 `1000` 으로 바꿔 봅니다. 1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다. 빠진 수와 가장 긴 공백은 버튼 3 통계와 디코더에 나옵니다. 1000 Hz 는 구조체가 134 B 이하일 때만 시작합니다 (타임스탬프를 켠 기본 설정 기준). 200 Hz 이하는 512 B 까지 시작합니다.
10. **변형 2 — 필드 더하기**: 구조체에 멤버를 더하고 메타 줄(`LOG_METADATA`)에도 같은 순서 · 같은 타입으로 적습니다. 필드 크기의 합이 구조체 크기와 다르면 시작하지 않고 LED 3 이 켜집니다. 디버거 Live Expressions 로 `g_last_start_error` 를 보세요.
11. **변형 3 — 폴더 이름 정하기**: `LOG_SESSION_NAME` 을 `"walk01"` 처럼 바꿔 봅니다. 같은 이름의 폴더가 이미 있으면 거기에 이어서 저장합니다. 저장 주기 · 구조체 크기 · 타임스탬프 설정이 같아야 합니다. 폴더의 메타 줄은 처음 저장한 것을 그대로 쓰니, 필드 이름이나 타입을 바꿨다면 다른 이름을 쓰세요.

---

## 5️⃣ 다음 단계

- H10 보행 데이터를 1 kHz 로 저장하기: [Ex.34 USB Memory Gait Log](../34_USB_Memory_Gait_Log/)
- PC 로 보면서 수집하기: [Ex.09 CDC Stream](../09_CDC_Stream/) · 구조체에 이름 · 단위를 붙여 PC 로 보내기: [Ex.44 CDC Struct Logging](../44_CDC_Struct_Logging/)
- 함수와 한도를 자세히: [USB 메모리 저장](../../docs/api-reference/06-usb-data-logging.md) · [`xm_api_usb_msc.h` 레퍼런스](../../docs/api-reference/ref/xm-api-usb-msc.md)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| LED 1 이 안 켜짐 | USB 메모리가 꽂혀 있지 않거나 FAT32 가 아님 (exFAT · NTFS 는 인식하지 않습니다) | FAT32 (할당 단위 32 KB 권장) 로 포맷합니다. 포맷하면 파일이 모두 지워지니 먼저 옮기세요 |
| 버튼 1 을 눌렀는데 LED 3 이 켜짐 | 시작하지 못했습니다 | 디버거 Live Expressions 에 `g_last_start_error` 를 넣어 값을 봅니다 (`XmUsbLogStartError_e`, 이름 앞에 `XM_USB_LOG_START_` 가 붙습니다). 예: `NOT_READY` (USB 메모리 없음 · FAT32 아님), `STORAGE` (빈 공간이 파일을 나누는 크기보다 작음), `FOLDER_MISMATCH` (같은 이름의 폴더가 다른 주기 · 구조체 크기로 이미 있음) |
| 폴더에 기록이 하나도 없음 | 시작하자마자 바로 멈췄습니다 | LED 2 가 천천히 깜빡이기 시작한 뒤에 멈춥니다 |
| 폴더 이름이 오늘 날짜와 다르거나 이전 저장과 같은 이름 | 시계(RTC)를 맞추지 않았습니다 | `XM_RTC_SetDateTime()` 으로 시계를 맞추거나 `LOG_SESSION_NAME` 을 직접 정합니다 ([실시간 시계](../../docs/api-reference/08-rtc-clock.md)) |
| 저장하는 중에 PhAI Studio · `xm10` 도구가 보드를 못 찾음 | USB-C 포트는 하나라서, USB 메모리를 꽂아 둔 동안에는 PC 와 연결되지 않습니다 | 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결합니다. (보드 확인 전) <!-- board-check --> |
| 저장하는 중에 USB 메모리를 뽑음 | 파일을 정상으로 닫지 못합니다. `summary.txt` 가 만들어지지 않고, 지금까지 쓴 부분만 풀 수 있습니다 | 저장하는 동안(LED 2 가 깜빡이는 동안)에는 뽑지 않습니다. (보드 확인 전) <!-- board-check --> |
| 통계의 `empty_slots` 가 0 이 아님 | 빠진 표본이 있습니다 (1000 Hz 에서는 일부 빠질 수 있습니다) | 빠진 위치와 길이는 디코더가 알려 줍니다. 저장 주기를 낮추거나 구조체를 줄입니다 |
| 디코더 보고서의 판정이 `RE-COLLECT` | 빠진 표본이 많거나 공백이 깁니다 | USB 메모리를 전체 포맷(FAT32, 할당 단위 32 KB)한 뒤 다시 저장하거나 다른 USB 메모리로 바꿔 봅니다. 계속되면 Angel Robotics(엔젤로보틱스) CS팀에 문의해 주세요 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)

> ℹ️ **아직 보드에서 확인되지 않은 예제입니다** (v2.9.0 시점). USB 메모리에 실제로 저장하는 동작은 아직 보드에서 확인하지 못했습니다. 처음 돌려보실 때 위 4️⃣의 순서로 단계별로 확인해 보시고, 결과를 알려 주시면 다음 사람이 덜 헤맵니다. (보드 확인 전) <!-- board-check -->
