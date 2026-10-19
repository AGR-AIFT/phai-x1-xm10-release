# Ex.34 — USB Memory Gait Log (H10 보행 데이터를 USB 메모리에 1 kHz 로 저장하기)

> 🎯 **학습 목표**:
> - H10 이 보내는 보행 데이터를 구조체 하나(96 B)로 묶어 **1 ms 마다** USB 메모리에 저장합니다.
> - 1 kHz 로 저장하면 표본이 일부 빠질 수 있다는 점을 알고, 빠진 위치와 길이를 디코더로 확인합니다.
> - 착용자의 신체 정보(`XM_SendUserBodyData`)가 보행 값의 정확도에 왜 필요한지 이해합니다.
>
> ⏱️ 권장 시간: 60분 | 🔧 난이도: ⭐⭐⭐ | 🟢 **Rev 2.0 전용** (USB 메모리 저장 API)
> 🧰 사전 예제: [Ex.10 USB Memory Log](../10_USB_Memory_Log/) (처음이라면 먼저) | 📚 관련 docs: [USB 메모리 저장](../../docs/api-reference/06-usb-data-logging.md) · [USB 메모리에 저장하기](../../docs/getting-started/05-usb-memory-log.md) · [Body Data 안내](../../docs/api-reference/README.md#body-data--보행-분석-데이터를-쓰려면-먼저-읽어주세요)

> 🟢 **Rev 2.0 전용** — USB 메모리 저장 API (`xm_api_usb_msc.h`) 는 Rev 2.0 SDK 에만 있습니다. Rev 1.1 SDK 에는 이 예제가 들어 있지 않습니다. 본인 보드의 리비전은 [보드 리비전 비교](../../docs/hardware/README.md#보드-리비전-비교)에서 확인하세요.

> ⚠️ **안전** — 기본 설정(`USE_H10_ASSIST 0`)은 기록만 합니다. H10 에 보내는 것은 신체 정보뿐이고, 토크나 제어 명령은 보내지 않습니다.
> `USE_H10_ASSIST` 를 `1` 로 바꾸면 H10 의 기존 보조 알고리즘이 켜져 **착용자에게 보조 토크가 걸립니다**. 착용자와 주변의 안전을 확인한 뒤에만 켜세요.

---

## ⚠️ USB-C 포트는 하나입니다

- USB 메모리를 꽂아 두는 동안에는 PhAI Studio · `xm10` 도구 · USB 로 하는 펌웨어 업로드를 쓸 수 없습니다. 이 예제도 저장하는 동안 PC 로 아무것도 보내지 않습니다.
- 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결하세요. (보드 확인 전) <!-- board-check -->
- 저장하는 동안(LED 2 가 깜빡이는 동안)에는 USB 메모리를 뽑지 마세요.
- PC 프로그램은 **한 번에 하나만** 연결하세요 (PhAI Studio · `xm10` 도구 · 시리얼 터미널).

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 을 연결하고 착용자가 입은 뒤 버튼 1 을 누르면, H10 의 보행 데이터를 구조체 하나(96 B)에 담아 **1 kHz** (1 ms 마다 한 번) 로 USB 메모리에 저장합니다. (보드 확인 전) <!-- board-check -->
보드 전원은 KIT H10 본체에서 들어오므로, H10 본체의 전원은 켜 둡니다.

| 저장하는 값 | 내용 |
|------|------|
| `h10_loop_count` | H10 이 세는 루프 카운터 (디코더가 시간축으로 씁니다) |
| `hip_angle_L` · `hip_angle_R` | 고관절 각도 [deg] |
| `thigh_angle_L` · `thigh_angle_R` | 허벅지 각도 [deg] |
| `knee_angle_L` · `knee_angle_R` | 무릎 각도 (추정) [deg] |
| `pelvic_angle` | 골반 각도 [deg] |
| `forward_velocity` | 전방 보행 속도 [m/s] |
| `hip_torque_L` · `hip_torque_R` | 관절 토크 추정값 [Nm] (읽기만 합니다) |
| `imu_acc_*` · `imu_gyr_*` | 좌우 고관절 IMU 의 가속도 [m/s²] 와 각속도 [deg/s] (x, y, z) |
| `foot_contact_L` · `foot_contact_R` | 발 접지 (0 = 공중, 1 = 접지) |
| `fsm_state` | H10 의 FSM 상태 |
| `h10_connected` | H10 연결 여부 (0 이면 H10 값을 믿지 마세요) |

→ 저장한 폴더는 USB 메모리의 `/LOGS/<날짜_시각>/` 입니다. PC 로 옮겨 XM10 Log Decoder 로 풀면 CSV 가 나오고, 빠진 표본이 있으면 그 위치와 길이를 알려 줍니다. 사용법은 [USB 메모리에 저장하기](../../docs/getting-started/05-usb-memory-log.md) 에 있습니다.

| 버튼 | 동작 |
|:---:|------|
| 버튼 1 (짧게) | 저장 시작 / 정지 |
| 버튼 2 | 마커 1 — 걷기 시작 (예: 출발하는 순간) |
| 버튼 3 | 마커 2 — 이벤트 (예: 표시하고 싶은 순간) |

마커는 디코더가 `events.csv` 로 따로 모아 줍니다.

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

- **1 kHz 저장** — `XM_SetUsbLogSampleRateHz(1000U)` 로 1 ms 마다 구조체 하나를 저장합니다 (정하지 않아도 1000 Hz 입니다). 1000 Hz 는 구조체가 134 B 이하일 때만 시작합니다 (타임스탬프를 켠 기본 설정 기준). 이 구조체는 96 B 입니다. 구조체가 작을수록 내부 버퍼 여유가 늘어나니 꼭 필요한 값만 담으세요. 더 느린 주기가 필요하면 `LOG_SAMPLE_RATE_HZ` 를 100 처럼 바꿉니다 (200 Hz 이하는 구조체가 512 B 까지 시작됩니다).
- **빠진 표본** — 1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다. 빠진 위치와 길이는 디코더가 알려 줍니다. 이 예제는 `XM_GetUsbLogStats()` 를 부르지 않습니다. 통계를 직접 보려면 [Ex.10](../10_USB_Memory_Log/) 의 방법을 쓰세요.
- **신체 정보** — 무릎 각도 · 허벅지 각도 · 발 접지는 H10 이 착용자의 신체 정보로 계산한 값입니다. 이 예제는 `Control_Setup()` 의 `_SetupBodyData()` 에서 `XM_SendUserBodyData()` 로 신체 정보(체중 · 키 · 다리 길이)를 보냅니다. 들어 있는 값(70 kg · 175 cm …)은 **예시**이니 착용자의 값으로 고치세요. 신체 정보가 맞지 않으면 이 값들이 부정확하고, 보내지 않으면 발 접지가 항상 0 으로 남을 수 있습니다.
- **시간축** — 첫 필드 `h10_loop_count` 는 H10 의 1 kHz 루프 카운터입니다. 카운터가 올라가고 있으면 디코더가 이 값으로 1 kHz 시간축을 만들고, 올라가지 않으면(H10 이 연결되지 않았을 때 등) 레코드마다 함께 저장되는 시각으로 만듭니다. 그래서 H10 이 없어도 저장은 진행되고, 이때 H10 값은 연결된 적이 없으면 0 으로 남습니다. (보드 확인 전) <!-- board-check -->
- **H10 보조 상태** — XM10 이 H10 에 연결되면 H10 의 기존 보조 알고리즘은 기본으로 꺼집니다 (`XM_SetH10AssistExistingMode()` 의 기본값). 이 예제도 기본은 기록만 하므로 착용자는 H10 의 보조를 받지 않습니다. 보조를 받으며 걷는 데이터를 저장하려면 `USE_H10_ASSIST` 를 `1` 로 바꾸세요. 위의 ⚠️ **안전** 안내를 먼저 읽으세요. H10 이 보조(Assist) 상태가 아니면 `h10_loop_count` 가 올라가지 않을 수 있습니다. (보드 확인 전) <!-- board-check -->
- **구조체와 메타 줄** — 메타 줄(`LOG_METADATA`)은 구조체의 필드를 선언 순서대로 `이름(타입)` 으로 적은 한 줄입니다. 구조체와 순서 · 타입이 같아야 하고, 필드 크기의 합이 다르면 시작하지 않습니다 (`XM_USB_LOG_START_METADATA_MISMATCH`). 첫 필드 `h10_loop_count` 의 이름은 바꾸지 마세요.
- **지켜야 할 것** — 설정 · 시작 · 정지 · 마커 · 통계 함수는 `Control_Setup()` · `Control_Loop()` 에서만 부릅니다. 다른 태스크나 인터럽트에서 부르면 아무것도 하지 않고 `false` 를 돌려줍니다. 시작은 `XM_IsUsbLogReady()` 와 `XM_IsUsbLogSessionIdle()` 이 모두 `true` 일 때만 부릅니다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef struct __attribute__((packed)) {
    uint32_t h10_loop_count;             // 첫 필드 — 디코더가 시간축으로 씁니다. 이름을 바꾸지 마세요
    float    hip_angle_L;                // 고관절 각도 [deg]
    float    hip_angle_R;
    /* … 허벅지 · 무릎 · 골반 각도, 보행 속도, 토크 추정값, IMU 가속도 · 각속도 … (float 22 개) */
    uint8_t  foot_contact_L;
    uint8_t  foot_contact_R;
    uint8_t  fsm_state;
    uint8_t  h10_connected;
} GaitLogRecord_t;                                                   // ① 96 bytes

static GaitLogRecord_t s_log;            // 저장할 구조체 — 전역 또는 static

void Control_Setup(void)
{
    /* 상태 머신 등록 생략 */

    XM_SetUsbLogSource(&s_log, sizeof(s_log));       // ② 저장할 구조체 등록 (96 B)
    XM_SetUsbLogSampleRateHz(1000U);                 //    1 kHz — 1 ms 마다 구조체 하나
    _SetupBodyData();                                // ③ 신체 정보를 H10 에 보냅니다 (값은 예시 — 착용자 값으로 고치세요)
}

void Control_Loop(void)
{
    const XmH10Data_t* h10 = &XM.status.h10;
    s_log.h10_loop_count = h10->h10AssistModeLoopCnt;   // ④ 값만 채웁니다 — 저장은 시스템이 합니다
    s_log.hip_angle_L    = h10->leftHipAngle;
    s_log.hip_angle_R    = h10->rightHipAngle;
    /* … 나머지 값도 같은 방식으로 … */

    /* 버튼 1: XM_StartUsbDataLog("", LOG_METADATA) 로 시작 / XM_StopUsbDataLog() 로 정지
       버튼 2 · 3: XM_InsertUsbLogMarker(종류, 값) — 저장 중에만 */
}
```

전체 코드: [`usb_memory_gait_log.c`](usb_memory_gait_log.c)

> 🧒 시작 · 정지 · 마커는 [Ex.10](../10_USB_Memory_Log/) 과 같은 흐름(대기 → 저장 → 닫는 중)입니다. 이 예제는 H10 값을 채우는 부분과 신체 정보를 보내는 부분이 다릅니다.
>
> 🧒 `XM.status.h10` 은 루프가 시작될 때 H10 에서 받은 가장 최근 값입니다. 같은 값이 두 번 보이거나 한 번 건너뛸 수 있습니다.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

아래 ✅ 는 코드가 의도한 동작이며, 보드에서는 아직 확인하지 못했습니다. (보드 확인 전) <!-- board-check -->

1. **신체 정보 고치기** — `_SetupBodyData()` 의 체중 · 키 · 다리 길이를 착용자의 값으로 고칩니다.
2. **USB 메모리 준비** — FAT32 (할당 단위 32 KB 권장) 로 포맷합니다. 포맷하면 안의 파일이 모두 지워집니다.
3. **빌드 + 플래시** → ✅ `0 errors`
4. **H10 연결 + 착용** — H10 을 연결하고 착용자가 입은 뒤에 시작하세요. H10 이 없으면 H10 값은 0 으로 저장됩니다. (보드 확인 전) <!-- board-check -->
5. **USB 메모리 꽂기** → ✅ LED 1 켜짐
6. **버튼 1 (짧게)** → ✅ LED 2 가 빠르게 깜빡이다가(파일을 여는 중) 천천히 깜빡입니다(저장 중)
7. **걷기** — 출발하는 순간 버튼 2, 표시하고 싶은 순간 버튼 3 을 누릅니다 → ✅ 마커가 들어갑니다. LED 3 이 짧게 한 번 켜지면 마커를 넣지 못한 것입니다
8. **버튼 1 다시** → ✅ LED 2 가 빠르게 깜빡이는 동안은 파일을 닫는 중이니 기다립니다. LED 2 가 켜진 채로 있으면 저장이 끝난 것이니 USB 메모리를 뽑아도 됩니다. (보드 확인 전) <!-- board-check -->
9. **디코더로 풀기** — USB 메모리의 `/LOGS/<날짜_시각>/` 폴더를 PC 로 옮겨 XM10 Log Decoder 2.2.0 이상으로 풉니다 → ✅ `decoded_output.csv` (1 kHz) · `events.csv` (마커를 넣었다면) · `data_quality_report.txt` 가 만들어집니다. 디코더는 릴리스 페이지의 첨부 파일에서 받습니다.
10. **변형 1 — 저장 주기 바꾸기**: `LOG_SAMPLE_RATE_HZ` 를 `100` 으로 바꿔 저장해 보고, 디코더가 만드는 파일 이름(`decoded_output_100hz.csv`)과 행 수가 1000 Hz 일 때와 어떻게 다른지 보세요. 저장 주기가 다른 로그의 판정은 그대로 비교하지 마세요.
11. **변형 2 — 필드 빼기**: 필요 없는 필드를 빼서 구조체를 줄입니다. 구조체 · 메타 줄 · `_Static_assert` 의 크기를 함께 고칩니다.
12. **변형 3 — 마커 종류 바꾸기**: `MARKER_WALK_START` · `MARKER_EVENT` 의 값(1 ~ 254, 뜻은 직접 정합니다)을 바꿔 봅니다.

---

## 5️⃣ 다음 단계

- 보드만으로 먼저 해 보기: [Ex.10 USB Memory Log](../10_USB_Memory_Log/)
- PC 로 보면서 수집하기: [Ex.09 CDC Stream](../09_CDC_Stream/) · 구조체에 이름 · 단위를 붙여 PC 로 보내기: [Ex.44 CDC Struct Logging](../44_CDC_Struct_Logging/)
- 함수와 한도를 자세히: [USB 메모리 저장](../../docs/api-reference/06-usb-data-logging.md) · [`xm_api_usb_msc.h` 레퍼런스](../../docs/api-reference/ref/xm-api-usb-msc.md)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 무릎 · 허벅지 각도가 부정확하거나 발 접지가 항상 0 | 신체 정보가 예시 값 그대로이거나 보내지 않았습니다 | `_SetupBodyData()` 의 값을 착용자의 값으로 고칩니다 |
| H10 값이 모두 0 | H10 이 연결되지 않았습니다 (`h10_connected` 가 0) | H10 전원과 CAN-FD 연결을 확인합니다. 연결이 끊기면 마지막 값이 남아 있을 수 있으니 `h10_connected` 를 함께 보세요 |
| `h10_loop_count` 가 올라가지 않음 | H10 이 보조(Assist) 상태가 아닙니다 | 보조를 받으며 걷는 데이터가 필요하면 `USE_H10_ASSIST` 를 `1` 로 바꿉니다 — 위의 ⚠️ **안전** 안내를 먼저 읽으세요. 바꾸지 않아도 저장은 계속되고, 디코더는 레코드마다 함께 저장되는 시각으로 시간축을 만듭니다. (보드 확인 전) <!-- board-check --> |
| LED 1 이 안 켜짐 | USB 메모리가 꽂혀 있지 않거나 FAT32 가 아님 (exFAT · NTFS 는 인식하지 않습니다) | FAT32 (할당 단위 32 KB 권장) 로 포맷합니다. 포맷하면 파일이 모두 지워지니 먼저 옮기세요 |
| 버튼 1 을 눌렀는데 LED 3 이 켜짐 | 시작하지 못했습니다 | 디버거 Live Expressions 에 `g_last_start_error` 를 넣어 값을 봅니다 (`XmUsbLogStartError_e`, 이름 앞에 `XM_USB_LOG_START_` 가 붙습니다). 예: `NOT_READY`, `STORAGE`, `TOO_BIG_FOR_RATE` (구조체를 키웠다면 1000 Hz 는 134 B 까지) |
| 폴더에 기록이 하나도 없음 | 시작하자마자 바로 멈췄습니다 | LED 2 가 천천히 깜빡이기 시작한 뒤에 멈춥니다 |
| 저장하는 중에 PhAI Studio · `xm10` 도구가 보드를 못 찾음 | USB-C 포트는 하나라서, USB 메모리를 꽂아 둔 동안에는 PC 와 연결되지 않습니다 | 저장이 끝나면 USB 메모리를 뽑고 PC 를 연결합니다. (보드 확인 전) <!-- board-check --> |
| 저장하는 중에 USB 메모리를 뽑음 | 파일을 정상으로 닫지 못합니다. `summary.txt` 가 만들어지지 않고, 지금까지 쓴 부분만 풀 수 있습니다 | 저장하는 동안(LED 2 가 깜빡이는 동안)에는 뽑지 않습니다. (보드 확인 전) <!-- board-check --> |
| 디코더 보고서에 빠진 표본이 있음 | 1000 Hz 로 저장하면 표본이 일부 빠질 수 있습니다 | 빠진 위치와 길이는 디코더가 알려 줍니다. 판정이 `RE-COLLECT` 이면 USB 메모리를 전체 포맷(FAT32, 할당 단위 32 KB)한 뒤 다시 저장하거나 다른 USB 메모리로 바꿔 봅니다. 계속되면 Angel Robotics(엔젤로보틱스) CS팀에 문의해 주세요 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)

> ℹ️ **아직 보드에서 확인되지 않은 예제입니다** (v2.9.0 시점). H10 과 USB 메모리를 함께 쓰는 동작은 아직 보드에서 확인하지 못했습니다. 처음 돌려보실 때 위 4️⃣의 순서로 단계별로 확인해 보시고, 결과를 알려 주시면 다음 사람이 덜 헤맵니다. (보드 확인 전) <!-- board-check -->
