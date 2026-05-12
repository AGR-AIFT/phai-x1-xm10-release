# Ex.10b — MSC Custom Struct (사용자 구조체 + 수동 타임스탬프)

> 🎯 **학습 목표**:
> - 여러 타입을 혼합한 **구조체 설계** (uint32_t / float / uint8_t / bool).
> - **자동 타임스탬프 비활성화** + 직접 tick 관리.
> - 메타데이터 상세 기술 (Python 디코더가 STRUCT_FMT 자동 생성 가능하도록).
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.10a Basic Log](../10a_MSC_Basic_Log/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

좌·우 고관절 각도 + IMU 3축 가속도 + gait_phase 를 1 ms 주기로 저장. 타임스탬프는 학생이 직접 관리 (구조체에 포함).

```c
typedef struct {
    uint32_t tick_ms;      // 4 B
    float    hip_angle_L;  // 4 B
    float    hip_angle_R;  // 4 B
    float    accel_x;      // 4 B
    float    accel_y;      // 4 B
    float    accel_z;      // 4 B
    uint8_t  gait_phase;   // 1 B
    uint8_t  _pad[3];      // 3 B padding
} SensorSnapshot_t;        // 28 B (4-byte aligned)
```

저장 경로: `/LOGS/SensorCapture/` → 28 B / record × N records.

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **4-byte 정렬 (Alignment)** — float / uint32_t 는 4 바이트, uint8_t / bool 은 1 바이트. 자연 정렬이 깨지면 컴파일러가 padding 자동 삽입 → 디코더 파싱 실패.
- **`__attribute__((packed))` vs natural** — `packed` 는 정렬 무시 (1 B alignment), 자연 설계는 4 B 단위로 배치하면 packed 불필요.
- **수동 타임스탬프** — `XM_SetUsbLogAutoTimestamp(false)` 호출 후 학생 구조체에 `tick_ms` 직접 포함. 정밀 시간 제어 가능.
- **`XM_GetTick()`** — 부팅 이후 ms.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef struct {
    uint32_t tick_ms;
    float    hip_angle_L;
    float    hip_angle_R;
    float    accel_x;
    float    accel_y;
    float    accel_z;
    uint8_t  gait_phase;
    uint8_t  _pad[3];                                                  // ① 정렬용 padding
} SensorSnapshot_t;  /* 28 bytes */

static SensorSnapshot_t s_snap;
static bool s_is_logging = false;

void User_Setup(void)
{
    XM_SetUsbLogSource(&s_snap, sizeof(SensorSnapshot_t));
    XM_SetUsbLogAutoTimestamp(false);                                   // ② 자동 타임스탬프 OFF
}

void User_Loop(void)
{
    if (!s_is_logging && XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogReady()) {
            s_is_logging = XM_StartUsbDataLog(
                "SensorCapture",
                "tick_ms(uint32_t), "                                    // ③ 상세 metadata
                "hip_angle_L(float), hip_angle_R(float), "
                "accel_x(float), accel_y(float), accel_z(float), "
                "gait_phase(uint8_t), _pad(3bytes)");
            if (s_is_logging) XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
        }
    }

    if (s_is_logging && XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_StopUsbDataLog();
        s_is_logging = false;
        XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0);
    }

    if (s_is_logging) {
        s_snap.tick_ms     = XM_GetTick();                              // ④ 직접 갱신
        s_snap.hip_angle_L = XM.status.h10.leftHipAngle;
        s_snap.hip_angle_R = XM.status.h10.rightHipAngle;
        s_snap.accel_x     = XM.status.h10.leftHipImuGlobalAccX;
        s_snap.accel_y     = XM.status.h10.leftHipImuGlobalAccY;
        s_snap.accel_z     = XM.status.h10.leftHipImuGlobalAccZ;
        s_snap.gait_phase  = 0;                                          // 추후 알고리즘 출력
    }
}
```

전체 코드: [`msc_custom_struct.c`](msc_custom_struct.c)

> 🧒 ① `uint8_t _pad[3]` 은 의도적 패딩. uint8_t 뒤에 3 바이트 공백을 명시해 총 28 B (4-byte 배수) 로 정렬.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **FAT32 USB 삽입** + **빌드 + 플래시**
2. **BTN 1** → 로깅 시작, LED 1 깜빡
3. **다리 움직임** (H10 연결 시) → IMU 가속도 변화
4. **BTN 2** → 정지
5. **PC 에서 PythonDecoder/MSC/data_decoder_xm10.py** 로 변환 → CSV
6. **변형 1 — `_pad` 제거**: `uint8_t _pad[3]` 삭제 후 빌드 → `sizeof(SensorSnapshot_t)` 가 변할 수 있음 (`25` 또는 `28`, 컴파일러 의존). 디코더 사용 전 검증.
7. **변형 2 — gait_phase 알고리즘 결합**: Ex.17 의 FSM 출력값 (`s_snap.gait_phase = current_phase`) 사용.
8. **변형 3 — int16_t 추가**: 정수형 센서 raw 값 (예: ADC 값) 도 함께 저장 → metadata 에 `(int16_t)` 표시.

---

## 5️⃣ 다음 단계

- 본격 제품 수준 (TSM + 에러 모니터링): [Ex.10c MSC Advanced Log](../10c_MSC_Advanced_Log/)
- 실시간 PC 스트리밍과 결합: [Ex.09 CDC Stream](../09_CDC_Stream/)
- 보행 분석 데이터 수집 응용: [Ex.34 MSC Gait Log](../34_MSC_GaitAnalysis_Log/) (있는 경우)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 디코더가 "size mismatch" 에러 | 구조체 크기와 metadata 의 합산 불일치 | `printf("%u", sizeof(SensorSnapshot_t))` 로 실제 크기 확인 후 metadata 정정 |
| 매번 `tick_ms` 가 0 | `XM_SetUsbLogAutoTimestamp(false)` 후 직접 갱신 누락 | Run_Loop 에서 `s_snap.tick_ms = XM_GetTick();` 확인 |
| 자동 타임스탬프 비활성화했는데도 8B Header 가 붙음 | Header 는 항상 4B (sync marker). 자동 tick 만 8B 중 4B | metadata 에 헤더 4B 명시 또는 디코더가 자동 처리 |
| float 값이 이상한 숫자 | 구조체 padding 가 디코더 STRUCT_FMT 와 어긋남 | `__attribute__((packed))` 사용 또는 명시적 `_pad` |
| Linux 에서 디코딩 안 됨 | metadata.txt 줄바꿈 `\r\n` 호환 | dos2unix 변환 또는 디코더 옵션 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
