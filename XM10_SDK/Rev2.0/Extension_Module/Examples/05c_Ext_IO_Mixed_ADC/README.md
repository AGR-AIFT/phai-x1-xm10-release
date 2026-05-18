# Ex.05c — Mixed ADC (고정 + 동적 혼합, 최대 12채널)

> 🎯 **학습 목표**:
> - 고정 ADC 4핀 + DIO→ADC 8핀 = **최대 12채널** 동시 활용.
> - 네이티브 Resolution 차이 (12-bit vs 16-bit) 이해 + `XM_AnalogReadMillivolts` 가 항상 일관된 mV 제공함을 확인.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.05a/05b](../05a_Ext_IO_DIO_to_ADC/) | 📚 관련 docs: [External IO](../../docs/api-reference/04-external-io.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

**조이스틱 (고정 ADC 2채널) + FSR 4개 (DIO→ADC 4채널)** 을 동시에 읽고, 조이스틱이 중립 위치 (약 1.5~1.8 V) 일 때 LED 1 이 켜집니다.

> 📸 `![Mixed ADC 시스템](../assets/img/05c_mixed_adc.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### XM ADC 핀 맵 (전체 12 채널)

| Facade | 핀 | 네이티브 | 비고 |
|--------|----|---------|------|
| `XM_EXT_ADC_1` | PA0 | 12-bit | (Shared) UART4_TX — Rev1.1 한정 |
| `XM_EXT_ADC_2` | PA0_C | 16-bit | 항상 사용 가능 |
| `XM_EXT_ADC_3` | PA1 | 12-bit | (Shared) UART4_RX — Rev1.1 한정 |
| `XM_EXT_ADC_4` | PA1_C | 16-bit | 항상 사용 가능 |
| `XM_EXT_ADC_5~12` | PF3~PF10 | 16-bit | DIO 1~8 에서 ADC 전환 필요 |

⚠️ **핵심**: ADC_1/3 만 네이티브 12-bit. 나머지는 모두 16-bit.
→ `XM_SetAnalogReadResolution()` 으로 출력만 정규화하면 모든 핀에서 동일 범위.
→ **`XM_AnalogReadMillivolts()` 는 항상 정확한 mV** (네이티브 ↔ mV 변환은 자동).

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define FSR_COUNT 4

typedef struct {
    uint16_t joystick_x_mv;
    uint16_t joystick_y_mv;
    uint16_t fsr_mv[FSR_COUNT];
} SensorData_t;
static SensorData_t s_data;

void User_Setup(void)
{
    /* ① 네이티브 Resolution 확인 (디버깅) */
    uint8_t res1 = XM_GetAnalogResolution(XM_EXT_ADC_1);   // 12
    uint8_t res2 = XM_GetAnalogResolution(XM_EXT_ADC_2);   // 16

    /* ② DIO 1~4 → ADC (FSR 4 채널). 5~8 은 GPIO 유지 */
    for (int i = 0; i < FSR_COUNT; i++) {
        XM_SwitchDioToAdc(XM_EXT_DIO_1 + i);
    }
    /* Resolution 은 기본 16-bit 유지 — mV API 사용하므로 무관 */
}

static void Run_Loop(void)
{
    /* ③ 고정 ADC: 조이스틱 X/Y */
    s_data.joystick_x_mv = XM_AnalogReadMillivolts(XM_EXT_ADC_2);   // PA0_C, 16-bit
    s_data.joystick_y_mv = XM_AnalogReadMillivolts(XM_EXT_ADC_4);   // PA1_C, 16-bit

    /* ④ DIO→ADC: FSR */
    for (int i = 0; i < FSR_COUNT; i++) {
        XmAdcPin_t adc = XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1 + i);        // → ADC_5~8
        s_data.fsr_mv[i] = XM_AnalogReadMillivolts(adc);
    }

    /* ⑤ 조이스틱 중립 감지 (약 1.65V) */
    bool centered = (s_data.joystick_x_mv > 1500 && s_data.joystick_x_mv < 1800)
                 && (s_data.joystick_y_mv > 1500 && s_data.joystick_y_mv < 1800);
    XM_SetLedState(XM_LED_1, centered ? XM_ON : XM_OFF);
}
```

전체 코드: [`ext_io_mixed_adc.c`](ext_io_mixed_adc.c)

> 🧒 핵심 통찰: ③ 고정 ADC 와 ④ 동적 ADC 의 코드가 **차이 없이 동일** — `XM_AnalogReadMillivolts` 가 네이티브 해상도 차이를 추상화.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 연결**: 조이스틱 X → PA0_C (ADC_2), Y → PA1_C (ADC_4). FSR 4개 → DIO 1~4.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **조이스틱 중앙 위치** → ✅ LED 1 켜짐
4. **조이스틱 움직임** → ✅ LED 1 꺼짐
5. **변형 1 — 라드 값 비교**: `XM_AnalogRead(XM_EXT_ADC_1)` 와 `XM_AnalogRead(XM_EXT_ADC_2)` 를 USB 로 출력 → 12-bit (0~4095) vs 16-bit (0~65535) 차이 직접 관찰.
6. **변형 2 — Resolution 변경**: `XM_SetAnalogReadResolution(12)` 추가 → mV API 값은 그대로, raw 값은 모두 0~4095 로 정규화됨을 관찰.
7. **변형 3 — 조이스틱 + FSR 동시 의미**: FSR 1번 누름 + 조이스틱 위 → 특정 LED 패턴 (논리 결합 학습).

---

## 5️⃣ 다음 단계

- 동일 포트에서 DIO + ADC 혼용: [Ex.05d Hybrid](../05d_Ext_IO_DIO_ADC_Hybrid/)
- 안전 인터록 응용: [Ex.06 Safety Switch](../06_Ext_IO_Safety_Switch/)
- 12채널 데이터를 PC 로 실시간 스트리밍: [Ex.09 CDC Stream](../09_CDC_Stream/)
- USB 메모리에 12채널 로깅: [Ex.10c MSC Advanced](../10c_MSC_Advanced_Log/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| ADC_1 사용 시 UART4 가 안 됨 | Rev1.1 의 PA0 핀 공유 | Rev1.1 은 ADC_2/ADC_4 (PA0_C/PA1_C) 우선 사용 |
| mV 값들이 핀마다 미세하게 다름 | 정상 — 네이티브 해상도 + ADC 노이즈 | ±5 mV 정도는 정상 |
| `XM_GetAnalogResolution` 가 잘못된 값 반환 | 핀 인자 오타 (DIO vs ADC) | `XM_EXT_ADC_*` 사용 |
| 조이스틱 중립이 1.65 V 가 아님 | 조이스틱 모델별 다름 | 멀티미터로 실제 중립 전압 측정 후 임계치 조정 |
| FSR 4 채널만 동작, 5~8 무응답 | `SwitchDioToAdc` 가 DIO 1~4 만 호출 | 의도된 동작 — 추가 채널은 루프 범위 확대 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
