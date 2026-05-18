# Ex.05b — FSR 8채널 일괄 전환 + Resolution 설정

> 🎯 **학습 목표**:
> - DIO 8개를 한 번에 ADC 로 전환 (`XM_SwitchAllDioToAdc`)
> - 출력 Resolution 변경 (`XM_SetAnalogReadResolution`) 으로 노이즈 자연 감소.
> - 배열 + 루프 패턴으로 다채널 데이터 수집.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.05a DIO→ADC](../05a_Ext_IO_DIO_to_ADC/) | 📚 관련 docs: [External IO](../../docs/api-reference/04-external-io.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

8개의 **FSR 압력 센서** 를 DIO 1~8 에 연결하고, 동시에 눌린 센서 개수에 따라 LED 가 단계별로 켜집니다.

| 눌린 센서 수 | LED 표시 |
|:---:|------|
| 0 | LED 1/2 모두 OFF |
| 1~4 | LED 1 ON |
| 5~8 | LED 1 + LED 2 ON |

> 📸 `![FSR 8ch 배열](../assets/img/05b_fsr_8ch.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **8핀 일괄 전환** — `XM_SwitchAllDioToAdc()` 한 줄로 DIO 1~8 모두 ADC. 루프 작성 불필요.
- **Resolution 옵션** — 출력 raw 값의 범위. 16/12/10/8-bit 선택. 하드웨어 ADC 는 그대로, SW 정규화만 변경.
- **FSR 12-bit 적합 이유** — 16-bit 면 하위 비트가 노이즈로 차서 의미 없음. 12-bit (0~4095) 가 적당.

### Resolution 선택 가이드

| 설정 | 범위 | 용도 |
|:---:|:---:|------|
| 16-bit | 0~65535 | 최대 정밀도 (기본) |
| **12-bit** | **0~4095** | **FSR / 온도센서 일반** |
| 10-bit | 0~1023 | Arduino 호환 |
| 8-bit | 0~255 | 단순 ON/OFF 감지 |

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define FSR_COUNT           8
#define FSR_RESOLUTION      12
#define FSR_PRESS_THRESHOLD 200    // 12-bit 기준 약 0.16 V

static uint16_t s_fsr[FSR_COUNT];
static uint8_t  s_pressed_count;

void User_Setup(void)
{
    XM_SetAnalogReadResolution(FSR_RESOLUTION);                   // ① 12-bit 정규화
    XM_SwitchAllDioToAdc();                                        // ② DIO 1~8 일괄 ADC

    uint8_t cur = XM_GetAnalogReadResolution();                   // ③ 디버깅: 12 확인
    (void)cur;
    /* TSM 등록 생략 */
}

static void Run_Loop(void)
{
    s_pressed_count = 0;

    for (int i = 0; i < FSR_COUNT; i++) {
        s_fsr[i] = XM_AnalogRead(XM_EXT_ADC_5 + i);                // ④ ADC_5~ADC_12 순차

        if (s_fsr[i] > FSR_PRESS_THRESHOLD) {                       // ⑤ 임계치 판단
            s_pressed_count++;
        }
    }

    /* ⑥ 눌린 센서 수에 따라 LED 단계별 점등 */
    if (s_pressed_count == 0) { /* LED1 OFF; LED2 OFF */ }
    else if (s_pressed_count <= 4) { /* LED1 ON; LED2 OFF */ }
    else { /* LED1 ON; LED2 ON */ }
}
```

전체 코드: [`ext_io_fsr_8ch.c`](ext_io_fsr_8ch.c)

> 🧒 ④의 `XM_EXT_ADC_5 + i` — ADC 핀 번호도 enum 이라 정수 덧셈으로 순회 가능.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 연결**: FSR 8개를 DIO 1~8 (PF3~PF10) 에 각각 분압 회로로 연결.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **FSR 하나만 누름** → ✅ LED 1 켜짐
4. **5개 이상 동시 누름** → ✅ LED 1 + LED 2 모두 켜짐
5. **변형 1 — Resolution 변경**: `FSR_RESOLUTION` 을 `16` 으로 → `FSR_PRESS_THRESHOLD` 도 비례해서 (200 × 16 = 3200) 키워야 동일 동작.
6. **변형 2 — 평균 압력**: `s_pressed_count` 대신 `s_fsr[]` 합산해서 평균값 계산 → LED 효과를 그라데이션 (Heartbeat 속도 가변) 으로.
7. **변형 3 — USB 로깅**: 8채널 값을 1초마다 sprintf 로 출력 ([Ex.08](../08_CDC_Sensor_Print/) 참조).

---

## 5️⃣ 다음 단계

- 고정 ADC + 동적 ADC 혼합 (최대 12채널): [Ex.05c Mixed ADC](../05c_Ext_IO_Mixed_ADC/)
- DIO 와 ADC 동일 포트 혼용: [Ex.05d Hybrid](../05d_Ext_IO_DIO_ADC_Hybrid/)
- USB 로 8채널 실시간 스트리밍: [Ex.09 CDC Stream](../09_CDC_Stream/)
- 보행 분석에 8채널 FSR 활용 (응용): [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 모든 FSR 값이 0 | 분압 저항 누락 또는 풀다운 X | 각 FSR 마다 10kΩ → GND 연결 |
| 일부 채널만 0 또는 4095 | 해당 DIO 핀 단선/단락 | 멀티미터로 핀 도통 확인 |
| Resolution 16-bit 인데 값이 0~4095 만 | `XM_SetAnalogReadResolution(12)` 가 호출됨 | Setup 에서 16 으로 변경 |
| `XM_EXT_ADC_5 + i` 가 컴파일 안 됨 | enum + int 연산 경고 (엄격 빌드) | `(XmAdcPin_t)(XM_EXT_ADC_5 + i)` 캐스트 |
| LED 가 깜빡 거리듯 단계 변화 | FSR 임계치 근처에서 노이즈 진동 | Hysteresis (히스테리시스) 추가: 200 ↑ ON, 150 ↓ OFF |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
