# Ex.09 — CDC Stream (PhAI Studio 실시간 바이너리 스트리밍)

> 🎯 **학습 목표**:
> - System 자동 스트리밍 (Total Data Packet 0x20, 425 B / 1 kHz) 의 존재를 이해합니다.
> - **User Custom 채널** (Module ID `0xF0~0xFE`) 로 알고리즘 디버그 변수를 추가 전송하는 방법.
> - JSON 메타데이터 등록 + `XM_SendUsbDataWithId()` 패턴.
>
> ⏱️ 권장 시간: 35분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.08 Sensor Print](../08_CDC_Sensor_Print/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

---

## ⚠️ USB-CDC 단독 점유 (이 예제는 PhAI Studio 전제)

본 예제는 **PhAI Studio 단독 실행** 을 전제로 합니다. PuTTY · TeraTerm · RealTerm 등 시리얼 터미널을 동시에 열어두면 COM 포트 충돌로 PhAI Studio 가 데이터를 수신하지 못합니다. 텍스트 디버깅이 필요하면 [Ex.07](../07_CDC_Basic_Print/) / [Ex.08](../08_CDC_Sensor_Print/) 만 단독 사용.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

PhAI Studio 를 켜고 XM10 USB 를 연결하면:

| Module ID | 출처 | 주기 | 내용 |
|:---:|------|:---:|------|
| **0x20** | System 자동 | 1 kHz | Total Data Packet 425 B (H10 PDO + GRF + IMU Hub + External IO 전체) |
| **0xEF** | System 자동 | 연결 시 1회 | User Meta JSON |
| **0xF0** | 본 예제 (User_Loop) | 가변 | 16 B (H10 연결 / 좌·우 고관절 각도 / 전방 보행 속도) |

→ PhAI Studio 에서 `0xF0` 채널 선택 시 본 예제가 전송한 4 변수의 실시간 그래프가 그려집니다.

> 📸 `![PhAI Studio 4-channel graph](../assets/img/09_phai_stream.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **Total Data Packet (0x20)** — System 이 425 B 의 H10/IMU/External IO 전체 상태를 1 kHz 로 자동 스트리밍. 학생 코드 0줄.
- **User Custom Channel (0xF0~0xFE)** — 알고리즘 내부 변수 (제어 출력, 추정치 등) 를 PhAI 에 노출. 채널당 별도 메타데이터 JSON 등록 필요.
- **`XM_SetUsbCustomMeta(id, json)`** — Setup 단계 1회. JSON 배열로 채널별 `name` + `unit` 등록 → PhAI 에 자동 표시.
- **`XM_SendUsbDataWithId(ptr, size, id)`** — non-blocking 전송. 버퍼 가득 차면 `false` 반환 + 해당 tick 드롭.
- **PhAI V2.1 프로토콜** — SOF 0xAA + CRC8 + STATUS. PythonDecoder/CDC/ 에 디코더 제공.

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

void User_Setup(void)
{
    /* TSM 등록 생략 */

    /* ② User Custom Meta — PhAI 에 채널 이름 + 단위 등록 */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"H10 Connected\",\"unit\":\"bool\"},"
        "{\"name\":\"Left Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Right Hip Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Forward Velocity\",\"unit\":\"m/s\"}]");
}

static void Run_Loop(void)
{
    /* ③ 데이터 갱신 — H10 PDO 에서 읽기 */
    s_debug.is_connected     = XM.status.h10.is_connected ? 1.0f : 0.0f;
    s_debug.left_hip_angle   = XM.status.h10.leftHipAngle;
    s_debug.right_hip_angle  = XM.status.h10.rightHipAngle;
    s_debug.forward_velocity = XM.status.h10.forwardVelocity;

    /* ④ Module ID 0xF0 으로 송신 — 매 tick (1 kHz) */
    XM_SendUsbDataWithId(&s_debug, sizeof(s_debug), 0xF0);
}
```

전체 코드: [`cdc_stream.c`](cdc_stream.c)

> 🧒 ② 의 JSON 은 한 줄로 작성. PhAI 가 이 메타를 받아 채널 UI 를 자동 구성.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **PhAI Studio 설치 + 실행** — 시리얼 터미널은 모두 종료.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **USB 연결** → PhAI Studio 가 자동으로 XM10 인식, Total Data (0x20) 채널 표시.
4. **0xF0 채널 선택** → ✅ "H10 Connected / Left Hip Angle / Right Hip Angle / Forward Velocity" 4채널 실시간 그래프.
5. **H10 움직임** → ✅ 그래프에 즉시 반영
6. **변형 1 — 채널 추가**: `0xF1` 에 새 구조체 등록 (예: PD gain 출력). `XM_SetUsbCustomMeta(0xF1, ...)` + `XM_SendUsbDataWithId(..., 0xF1)`.
7. **변형 2 — 전송 주기 조절**: Ex.08 의 500ms 타이머 패턴 적용 → 1 kHz 가 아닌 100 Hz 로 전송. 대역폭 절약.
8. **변형 3 — 드롭 모니터링**: `XM_SendUsbDataWithId` 의 반환값 (`bool`) 을 카운트해서 USB 메시지로 출력 → 얼마나 드롭되는지 확인.

---

## 5️⃣ 다음 단계

- 데이터를 USB 메모리에 영구 저장: [Ex.10a MSC Basic Log](../10a_MSC_Basic_Log/)
- Total Data + User Custom 결합 분석: [PythonDecoder/CDC/](../../PythonDecoder/CDC/)
- 실시간 제어 알고리즘 + PhAI 모니터링: [Ex.14 PD Realtime Control](../14_PD_Realtime_Control/)
- 다채널 응용: [Ex.16 TinyAI](../16_TinyAI_Sensor_Fusion/) / [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| PhAI Studio 가 보드 인식 못 함 | 시리얼 터미널이 COM 포트 점유 중 | PuTTY 등 모두 종료 + USB 재연결 |
| Total Data (0x20) 만 보이고 User Custom 안 보임 | `XM_SetUsbCustomMeta` 호출 누락 | Setup 에서 1회 호출 확인 |
| 채널 이름이 안 보이거나 깨짐 | JSON 문법 오류 (escape `\"` 누락 등) | JSON 한 줄로 검증 (예: jsonlint.com) |
| 그래프가 끊기듯 보임 | 버퍼 풀 가득 → 드롭 발생 | 전송 주기 늘리거나 (1 kHz → 100 Hz) 구조체 크기 줄임 |
| H10 Connected 가 항상 0 | KIT H10 미연결 또는 CAN-FD HIGH/LOW 거꾸로 | [01-hardware-setup.md](../../docs/getting-started/01-hardware-setup.md) 핀맵 |
| 코드는 같은데 PhAI 에 메타가 갱신 안 됨 | 이전 메타가 캐시됨 | XM10 reset + PhAI Studio 재연결 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
