# Ex.09 — CDC Stream (USB-CDC 실시간 바이너리 스트리밍)

> 🎯 **학습 목표**:
> - System 자동 스트리밍 (Total Data Packet 0x20, 365 B / 1 kHz) 의 존재를 이해합니다.
> - **User Custom 채널** (Module ID `0xF0~0xFE`) 로 알고리즘 디버그 변수를 추가 전송하는 방법.
> - JSON 메타데이터 등록 + `XM_SendUsbDataWithId()` 패턴.
>
> ⏱️ 권장 시간: 35분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.08 Sensor Print](../08_CDC_Sensor_Print/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

---

## ⚠️ USB-CDC 단독 점유

PC 프로그램은 **한 번에 하나만** 연결하세요 (PhAI Studio 또는 `xm10` 도구). PuTTY · TeraTerm · RealTerm 등 시리얼 터미널을 같이 열어두면 COM 포트 충돌로 데이터를 수신하지 못합니다. 텍스트 디버깅이 필요하면 [Ex.07](../07_CDC_Basic_Print/) / [Ex.08](../08_CDC_Sensor_Print/) 만 단독 사용.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

PC 프로그램(PhAI Studio 또는 `xm10` 도구)으로 포트를 열면(Connect) 아래 데이터가 흐릅니다 (USB 를 꽂는 것만으로는 시작되지 않습니다):

| Module ID | 출처 | 주기 | 내용 |
|:---:|------|:---:|------|
| **0x20** | System 자동 | 1 kHz | Total Data Packet 365 B (H10 + GRF + IMU Hub + External IO 전체) |
| **0xEF** | System 자동 | 연결할 때 1회 | User Meta JSON |
| **0xF0** | 본 예제 (Control_Loop) | 가변 | 16 B (H10 연결 / 좌·우 고관절 각도 / 전방 보행 속도) |

→ **0x20** 은 PhAI Studio 가 그래프로 그립니다 (`xm10` 도구는 `.xmlog` 에 저장하고, **Show 0x20 tab** 을 켜면 표로 보여 줍니다).

→ PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요. 이 예제의 **0xF0** 4 변수는 `xm10` 도구 ([안내](../../docs/getting-started/04-pc-data-tool.md)) 의 `0xF0` 탭에 그려집니다.

→ 채널 이름은 연결할 때 한 번 전달됩니다. 이름이 안 보이면 USB 케이블을 다시 꽂고 다시 연결하세요.

> 📸 **xm10 도구 4-channel graph** — 사진·영상 준비 중

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **Total Data Packet (0x20)** — System 이 365 B 의 H10/IMU/External IO 전체 상태를 1 kHz 로 자동 스트리밍. 사용자 코드 0줄.
- **User Custom Channel (0xF0~0xFE)** — 알고리즘 내부 변수 (제어 출력, 추정치 등) 를 PC (`xm10` 도구) 로 내보냅니다. 채널 이름·단위는 메타 JSON 으로 등록하며, 등록은 하나만 유지되어 `XM_SetUsbCustomMeta` 를 다시 부르면 이전 것을 덮어씁니다.
- **`XM_SetUsbCustomMeta(id, json)`** — Setup 단계 1회. JSON 배열로 채널별 `name` + `unit` 등록 → `xm10` 도구가 채널 이름을 그래프 제목·CSV 열 이름으로 씁니다.
- **`XM_SendUsbDataWithId(ptr, size, id)`** — non-blocking 전송. 전송 버퍼가 가득 차거나 구조체가 1020 바이트를 넘으면 `false` 반환 + 해당 tick 드롭.
- **PhAI V2.2 프로토콜** — SOF 0xAA + CRC16-CCITT + STATUS. PC 쪽은 레포 내 `xm10` 도구가 받는다 (`python PythonDecoder/xm10.py recv`, [안내](../../docs/getting-started/04-pc-data-tool.md)).
- **지켜야 할 것** — 구조체는 `float` 만, JSON 항목 수 = 필드 수, ID 는 `0xF0`~`0xFE`. 셋 중 하나만 어긋나도 PC 화면의 열이 밀리거나 값이 이상해진다.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef struct {
    float is_connected;
    float left_hip_angle;
    float right_hip_angle;
    float forward_velocity;
} UserDebugData_t;                                                   // ① 16 bytes

static UserDebugData_t s_debug;

void Control_Setup(void)
{
    /* TSM 등록 생략 */

    /* ② User Custom Meta — 채널 이름 + 단위 등록 (xm10 도구가 사용) */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"H10 Connected\",\"unit\":\"bool\"},"
        "{\"name\":\"Left Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Right Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Forward Velocity\",\"unit\":\"m/s\"}]");
}

static void Run_Loop(void)
{
    /* ③ 데이터 갱신 — H10 데이터에서 읽기 */
    s_debug.is_connected     = XM.status.h10.is_connected ? 1.0f : 0.0f;
    s_debug.left_hip_angle   = XM.status.h10.leftHipAngle;
    s_debug.right_hip_angle  = XM.status.h10.rightHipAngle;
    s_debug.forward_velocity = XM.status.h10.forwardVelocity;

    /* ④ Module ID 0xF0 으로 송신 — 매 tick (1 kHz) */
    XM_SendUsbDataWithId(&s_debug, sizeof(s_debug), 0xF0);
}
```

전체 코드: [`cdc_stream.c`](cdc_stream.c)

> 🧒 ② 의 JSON 은 한 줄로 작성. `xm10` 도구가 이 메타로 채널 이름을 붙입니다.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **PC 프로그램 준비** — 이 예제는 `xm10` 도구([안내](../../docs/getting-started/04-pc-data-tool.md)) 하나로 진행하세요. 0xF0 은 탭의 그래프로, 0x20 은 `.xmlog` / CSV 로 저장됩니다. (0x20 을 그래프로 보고 싶으면 PhAI Studio 를 따로 실행하세요 — PC 프로그램은 한 번에 하나만 연결.) 시리얼 터미널은 모두 종료하세요.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **USB 연결 + 포트 열기** — XM10 을 USB-C 로 PC 에 연결하고, `xm10` 도구에서 포트를 골라 **Connect**. Total Data (0x20) 도 함께 흘러 `.xmlog` 에 저장됩니다.
4. **`xm10` 도구의 0xF0 탭** → ✅ "H10 Connected / Left Hip Angle / Right Hip Angle / Forward Velocity" 4채널 실시간 그래프.
5. **H10 움직임** → ✅ 그래프에 즉시 반영
6. **변형 1 — 채널 추가**: `0xF1` 로 새 구조체를 더 보냅니다 (예: PD gain 출력) — `XM_SendUsbDataWithId(..., 0xF1)`. `xm10` 도구에 `0xF1` 탭이 하나 더 생깁니다. 이름 등록 (`XM_SetUsbCustomMeta`) 은 하나만 유지되어 두 번째로 부르면 첫 번째 이름이 사라지니, 이름이 필요하면 한 Module ID 로 묶으세요.
7. **변형 2 — 전송 주기 조절**: Ex.08 의 500ms 타이머 패턴 적용 → 1 kHz 가 아닌 100 Hz 로 전송. 대역폭 절약.
8. **변형 3 — 드롭 모니터링**: `XM_SendUsbDataWithId` 의 반환값 (`bool`) 을 세어 디버거 Live Expressions 로 보면 얼마나 드롭되는지 알 수 있습니다. (스트림을 받는 중에 `XM_SendUsbDebugMessage` 로 텍스트를 보내면 데이터 패킷 1개가 손실됩니다.)

---

## 5️⃣ 다음 단계

- PC 에서 받아 저장하고 CSV 로 뽑기: [xm10 도구 안내](../../docs/getting-started/04-pc-data-tool.md) — `recv` 로 그래프 + `.xmlog`, `export` 로 Total Data(0x20) 197채널과 이 예제의 0xF0 채널을 나란히 CSV 로
- 실시간 제어 알고리즘 + PC 모니터링 (`xm10` 도구): [Ex.14 PD Realtime Control](../14_PD_Realtime_Control/)
- 다채널 응용: [Ex.16 TinyAI](../16_TinyAI_Sensor_Fusion/) / [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| PhAI Studio 가 보드 인식 못 함 | 시리얼 터미널이 COM 포트 점유 중 | PuTTY 등 모두 종료 + USB 재연결 |
| PhAI Studio 에서 0xF0 채널이 안 보임 | PhAI Studio 는 자동으로 보내는 Total Data(0x20)를 보여 줍니다 | PhAI Studio 는 아직 개발 중이라, 직접 정의한 데이터 구조체(커스텀 구조체)는 우선 `xm10` 도구로 보고 저장하세요 |
| `xm10` 에서 그래프 제목이 `ch0, ch1…` | `XM_SetUsbCustomMeta` 호출 누락, 또는 연결할 때 채널 이름을 받지 못함 | Setup 에서 1회 호출 확인 + USB 케이블을 다시 꽂고 다시 연결 |
| 채널 이름이 안 보이거나 깨짐 | JSON 문법 오류 (escape `\"` 누락 등) | JSON 한 줄로 검증 (예: jsonlint.com) |
| 그래프가 끊기듯 보임 | 전송 버퍼 가득 → 드롭 발생 | 전송 주기 늘리거나 (1 kHz → 100 Hz) 구조체 크기 줄임 |
| H10 Connected 가 항상 0 | KIT H10 미연결 또는 CAN-FD HIGH/LOW 거꾸로 | [01-hardware-setup.md](../../docs/getting-started/01-hardware-setup.md) 핀맵 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
