# Ex.05d — DIO + ADC Hybrid (동일 포트 혼용 + 보호 장치)

> 🎯 **학습 목표**:
> - 같은 DIO 포트에서 일부 핀은 GPIO, 다른 핀은 ADC 로 **동시에 사용**.
> - ADC 전환된 핀에 GPIO API 호출 시 **보호 장치** 가 작동함을 확인.
> - 실전 시나리오: 외부 버튼으로 FSR 측정 모드를 토글.
>
> ⏱️ 권장 시간: 30분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.05c Mixed ADC](../05c_Ext_IO_Mixed_ADC/) | 📚 관련 docs: [External IO](../../docs/api-reference/04-external-io.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

| 핀 | 모드 | 용도 |
|----|------|------|
| DIO 1~4 (PF3~PF6) | ADC | FSR 4개 압력 측정 |
| DIO 5 (PF7) | GPIO Input Pullup | 외부 푸시 버튼 |
| DIO 6 (PF8) | GPIO Output | 상태 표시 LED |

**동작**: 외부 버튼 누름 → 측정 모드 토글. ON 상태에서 FSR 4개 합산 압력 > 6 V (4 × 1.5 V) 이면 내부 LED 1 켜짐.

> 📸 `![Hybrid 회로](../assets/img/05d_hybrid.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **핀 단위 독립 모드** — 같은 DIO 그룹 내에서도 핀별로 다른 모드 가능 (1~4 = ADC, 5~6 = GPIO).
- **보호 장치 (Guard Mechanism)** — ADC 로 전환된 핀에 GPIO API (`XM_SetPinMode`, `XM_DigitalWrite`, `XM_DigitalRead`) 를 호출해도 안전하게 무시. 학생 실수 방지.
- **Edge Detection** — `btn_now && !btn_prev` 패턴으로 "방금 막 눌린 순간" 한 번만 감지 (Leading Edge).

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define BUTTON_PIN  XM_EXT_DIO_5
#define LED_PIN     XM_EXT_DIO_6

static bool s_measuring = false;
static bool s_btn_prev = false;
static uint16_t s_fsr_mv[4];
static uint32_t s_total_pressure_mv;

void User_Setup(void)
{
    XM_SetAnalogReadResolution(12);

    /* ① DIO 1~4 → ADC */
    for (int i = 0; i < 4; i++) XM_SwitchDioToAdc(XM_EXT_DIO_1 + i);

    /* ② DIO 5~6 은 GPIO 유지 */
    XM_SetPinMode(BUTTON_PIN, XM_EXT_DIO_MODE_INPUT_PULLUP);
    XM_SetPinMode(LED_PIN, XM_EXT_DIO_MODE_OUTPUT);
    XM_DigitalWrite(LED_PIN, XM_LOW);

    /* ③ 보호 장치 테스트 — ADC 핀에 GPIO 호출은 무시됨 */
    XM_SetPinMode(XM_EXT_DIO_1, XM_EXT_DIO_MODE_OUTPUT);   // 무시
    XM_DigitalWrite(XM_EXT_DIO_1, XM_HIGH);                  // 무시
    XmLogicLevel_t dummy = XM_DigitalRead(XM_EXT_DIO_1);     // XM_LOW 반환
}

static void Run_Loop(void)
{
    /* ④ Edge Detection: 버튼이 막 눌린 순간 1회 */
    bool btn_now = (XM_DigitalRead(BUTTON_PIN) == XM_LOW);
    bool pressed = (btn_now && !s_btn_prev);
    s_btn_prev = btn_now;

    if (pressed) {
        s_measuring = !s_measuring;                           // ⑤ 모드 토글
        XM_DigitalWrite(LED_PIN, s_measuring ? XM_HIGH : XM_LOW);
    }

    /* ⑥ 측정 모드에서만 FSR 수집 */
    if (s_measuring) {
        s_total_pressure_mv = 0;
        for (int i = 0; i < 4; i++) {
            XmAdcPin_t adc = XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1 + i);
            s_fsr_mv[i] = XM_AnalogReadMillivolts(adc);
            s_total_pressure_mv += s_fsr_mv[i];
        }
        XM_SetLedState(XM_LED_1, s_total_pressure_mv > 6000 ? XM_ON : XM_OFF);
    } else {
        XM_SetLedState(XM_LED_1, XM_OFF);
    }
}
```

전체 코드: [`ext_io_dio_adc_hybrid.c`](ext_io_dio_adc_hybrid.c)

> 🧒 ④의 Edge Detection 은 "버튼 들고 있는 동안 토글 반복" 을 막는 핵심 패턴.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 연결**: FSR 4개 (DIO 1~4), 푸시 버튼 (DIO 5), 외부 LED (DIO 6).
2. **빌드 + 플래시** → ✅ `0 errors`
3. **버튼 짧게 누름** → ✅ 외부 LED ON (측정 모드 진입)
4. **FSR 4개 모두 강하게 누름** → ✅ 내부 LED 1 점등
5. **버튼 다시 누름** → ✅ 외부 LED OFF, 내부 LED 1 도 꺼짐
6. **변형 1 — 보호 장치 직접 확인**: Run_Loop 안에서 `XM_DigitalWrite(XM_EXT_DIO_1, XM_HIGH)` 호출 → FSR 값에 영향 없는지 (ADC 정상 동작 유지) 확인.
7. **변형 2 — 평균 + 표준편차**: 4채널 평균 + 분산 계산 → 분산이 크면 (FSR 들이 비균등 압력) LED 깜빡 효과.
8. **변형 3 — USB 로 모드 + 압력 모니터링**: sprintf 로 모드/총압력 출력 ([Ex.08](../08_CDC_Sensor_Print/) 패턴).

---

## 5️⃣ 다음 단계

- 외부 IO + 안전 인터록 + FSM: [Ex.06 Safety Switch](../06_Ext_IO_Safety_Switch/)
- 12채널 데이터를 PC 로 스트리밍: [Ex.09 CDC Stream](../09_CDC_Stream/)
- 데이터 저장: [Ex.10b MSC Custom Struct](../10b_MSC_Custom_Struct/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 버튼 한 번 누름에 모드가 여러 번 바뀜 | Edge Detection 누락 | `btn_now && !s_btn_prev` 패턴 확인 |
| `s_btn_prev` 가 매 호출 false 로 초기화 | `static` 누락 | `static bool s_btn_prev = false;` |
| ADC 핀에 GPIO 코드 추가했더니 빌드는 됐는데 동작 이상 | 보호 장치가 무시했지만 학생 의도 X | 의도 확인 — ADC 핀은 ADC 만 사용 |
| 외부 LED 가 안 켜짐 | DIO_6 output 설정 누락 | `SetPinMode(LED_PIN, OUTPUT)` 확인 |
| 측정 모드인데 LED 1 안 켜짐 | 임계치 (6000 mV = 6 V) 너무 높음 | FSR 4개 합산 평소 mV 측정 후 임계치 조정 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
