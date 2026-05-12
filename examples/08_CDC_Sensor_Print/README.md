# Ex.08 — CDC Sensor Print (센서 값 sprintf 모니터링)

> 🎯 **학습 목표**:
> - `sprintf` 로 변수 값을 문자열에 끼워 넣어 PC 로 전송합니다.
> - **논블로킹 타이머 패턴** (`XM_GetTick`) 으로 500 ms 주기 출력.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.07 CDC Basic Print](../07_CDC_Basic_Print/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md) · [H10 Data](../../docs/api-reference/02-h10-control-n-data.md)

---

## ⚠️ USB-CDC 단일 점유 (Ex.07 과 동일 룰)

PhAI Studio 와 시리얼 터미널 (PuTTY 등) 을 같은 COM 포트로 **동시에 열지 마세요** — 충돌로 데이터 손실. 실시간 그래프가 필요하면 [Ex.09 CDC Stream](../09_CDC_Stream/) + PhAI 단독.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

KIT H10 의 **좌/우 고관절 각도** 를 0.5 초마다 시리얼 터미널에 출력:

```
Hip Angles -> RH: 12.34, LH: -8.21
Hip Angles -> RH: 13.45, LH: -7.92
...
```

> 📸 `![센서 모니터링 출력](../assets/img/08_sensor_print.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **`sprintf(buf, "fmt %f", val)`** — C 표준 라이브러리. 문자열에 변수 값을 포맷팅. `%f` 는 float, `%d` 는 int.
- **`%.2f`** — 소수점 둘째 자리까지. 콘솔 가독성을 위해 자주 사용.
- **논블로킹 타이머 (Non-blocking timer)** — `osDelay()` 같은 blocking 호출 대신 `XM_GetTick()` 으로 경과 시간 측정. 500 Hz 사용자 루프를 막지 않음.
- **`XM.status.h10.*`** — System 이 자동으로 PDO 에서 읽어 채워주는 H10 데이터 ([api-ref](../../docs/api-reference/02-h10-control-n-data.md)). 사용자 코드가 직접 접근 가능.
- **`XM_GetTick()`** — 부팅 이후 경과 ms (32-bit, ~49.7일 wrap).

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#include <stdio.h>   // sprintf 사용

static void Run_Loop(void)
{
    static uint32_t last_print_time = 0;                            // ① static 필수 (값 보존)
    uint32_t now = XM_GetTick();

    if (now - last_print_time >= 500) {                              // ② 논블로킹 500ms 타이머
        last_print_time = now;

        float angle_rh = XM.status.h10.rightHipAngle;                 // ③ H10 PDO 읽기
        float angle_lh = XM.status.h10.leftHipAngle;

        char buf[64];                                                  // ④ 출력 버퍼
        sprintf(buf, "Hip Angles -> RH: %.2f, LH: %.2f\r\n",          // ⑤ 변수 포맷팅
                angle_rh, angle_lh);

        XM_SendUsbDebugMessage(buf);                                   // ⑥ 송신
    }
}
```

전체 코드: [`cdc_sensor_print.c`](cdc_sensor_print.c)

> 🧒 ②의 `now - last_print_time >= 500` 은 wrap (49.7일) 안전. 단순 `now > last + 500` 은 wrap 시 버그.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW**: KIT H10 ↔ XM10 CAN-FD 연결 (없으면 각도 값은 0 으로 표시)
2. **빌드 + 플래시 + 시리얼 터미널 열기** (Ex.07 절차)
3. **출력 확인** → ✅ 0.5 초마다 1줄씩 추가
4. **H10 다리 움직임** → ✅ 각도 값 변화
5. **변형 1 — 주기 변경**: `500` 을 `100` (10 Hz) 또는 `2000` (0.5 Hz) 으로.
6. **변형 2 — 다른 센서 추가**: `XM.status.h10.leftKneeAngle`, `rightKneeAngle` 도 함께 출력.
7. **변형 3 — IMU 데이터**: `XM.status.h10.leftHipImuGlobalAccX/Y/Z` 추가.
8. **변형 4 — Tick 직접 표시**: 출력 줄 앞에 `now` 값도 포함 (시간 동기화 확인용).

---

## 5️⃣ 다음 단계

- 고속 바이너리 (텍스트 → 그래프): [Ex.09 CDC Stream](../09_CDC_Stream/)
- 데이터를 USB 메모리에 저장: [Ex.10a MSC Basic Log](../10a_MSC_Basic_Log/)
- 실시간 H10 제어와 결합: [Ex.14 PD Realtime Control](../14_PD_Realtime_Control/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `last_print_time` 이 매번 0 으로 초기화 | `static` 누락 | `static uint32_t last_print_time = 0;` |
| sprintf 결과가 `Hip Angles -> RH: 0.00, LH: 0.00` | H10 미연결 또는 CAN-FD 통신 X | KIT H10 본체 + CAN-FD 케이블 확인 |
| float 출력이 정수처럼 보임 | newlib-nano (기본) 가 `%f` 미지원 | `Project Properties > Tool Settings > MCU Settings` 에서 `Use float with printf` 체크 |
| 버퍼 오버플로 (UTF-8 한글 + 긴 메시지) | `buf[64]` 부족 | 버퍼 크기 늘리거나 `snprintf` 사용 |
| 출력 주기 가변 (500 → 600 ms) | UserTask jitter (다른 무거운 작업) | 정상. 정밀 타이밍은 [Ex.18 Debug Monitor](../18_Debug_Monitor/) 참조 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
