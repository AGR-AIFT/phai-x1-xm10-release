# Ex.05a — DIO → ADC 전환 (FSR 1채널)

> 🎯 **학습 목표**:
> - 기본적으로 디지털인 DIO 핀을 **런타임에 ADC 모드로 전환** 하는 방법을 익힙니다.
> - `XM_SwitchDioToAdc()` + `XM_DIO_TO_ADC_PIN()` 매크로 + 전환 상태 검증.
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.05 Ext IO Analog](../05_Ext_IO_analog/) | 📚 관련 docs: [External IO](../../docs/api-reference/04-external-io.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

DIO 1 (PF3) 에 **FSR (압력 센서)** 를 연결하고, 압력이 일정 수준 이상이면 LED 2 가 켜집니다.

| 단계 | 결과 |
|------|------|
| 부팅 시 | DIO 1 이 ADC 모드로 전환 → LED 1 점등 (전환 성공 표시) |
| FSR 누름 | LED 2 켜짐 (FSR 전압 > 500 mV) |
| FSR 안 누름 | LED 2 꺼짐 |

> 📸 `![FSR 분압 회로](../assets/img/05a_fsr_voltage_divider.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **DIO 와 ADC 핀 번호가 다름** — DIO_1 (PF3) ↔ ADC_5 매핑. (DIO_n ↔ ADC_(n+4))
- **`XM_DIO_TO_ADC_PIN()` 매크로** — DIO 번호에서 ADC 번호로 변환. 학생 친화 헬퍼.
- **전환은 비실시간** — `User_Setup()` 에서 1회만. **재부팅 전까지 GPIO 로 복구 불가**.
- **FSR 분압 회로** — FSR 한쪽 = 3.3 V, 반대쪽 = DIO_1 + 10 kΩ 풀다운 저항 → GND. 누르면 저항이 감소해 분압 증가.

### DIO → ADC 매핑 표

| DIO | 핀 | ADC |
|-----|-----|-----|
| DIO_1 | PF3 | ADC_5 |
| DIO_2 | PF4 | ADC_6 |
| ... | ... | ... |
| DIO_8 | PF10 | ADC_12 |

매크로로 자동 변환: `XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1)` → `XM_EXT_ADC_5`

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define FSR_PIN          XM_EXT_DIO_1
#define FSR_THRESHOLD_MV 500

void User_Setup(void)
{
    XM_SwitchDioToAdc(FSR_PIN);                                   // ① DIO_1 → ADC 전환

    if (XM_IsDioSwitchedToAdc(FSR_PIN)) {                          // ② 전환 성공 확인
        XM_SetLedState(XM_LED_1, XM_ON);                            //    LED 1 = 디버깅용
    }

    /* TSM 설정 (이하 생략) */
}

static void Run_Loop(void)
{
    XmAdcPin_t adc_pin = XM_DIO_TO_ADC_PIN(FSR_PIN);              // ③ DIO_1 → ADC_5 변환
    uint16_t mv = XM_AnalogReadMillivolts(adc_pin);                // ④ mV 단위 읽기

    if (mv > FSR_THRESHOLD_MV) {                                   // ⑤ 임계치 500 mV
        XM_SetLedState(XM_LED_2, XM_ON);
    } else {
        XM_SetLedState(XM_LED_2, XM_OFF);
    }
}
```

전체 코드: [`ext_io_dio_to_adc.c`](ext_io_dio_to_adc.c)

> 🧒 핵심 흐름: ① 전환 → ② 검증 → ③ 매크로로 핀 번호 변환 → ④ 읽기 → ⑤ 판단.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 연결**: FSR 분압 회로 (위 사전 지식 참조), DIO_1 (PF3) 에 연결.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **부팅 직후** → ✅ LED 1 점등 (전환 성공 표시)
4. **FSR 누르기** → ✅ LED 2 켜짐
5. **FSR 떼기** → ✅ LED 2 꺼짐
6. **변형 1 — 임계치 조정**: `FSR_THRESHOLD_MV` 를 `200` 또는 `1000` 으로 → 더 가볍게/세게 눌러야 켜짐.
7. **변형 2 — 다른 DIO**: `FSR_PIN` 을 `XM_EXT_DIO_3` 으로 변경 → 매크로가 자동으로 `XM_EXT_ADC_7` 로 변환됨을 확인.
8. **변형 3 — raw 값과 mV 비교**: `XM_AnalogRead(adc_pin)` 으로 raw 값도 읽어 둘 다 USB 로 출력 → 어떤 차이가 있는지 관찰.

---

## 5️⃣ 다음 단계

- 8채널 일괄 전환: [Ex.05b FSR 8ch](../05b_Ext_IO_FSR_8ch/)
- 고정 ADC + 동적 ADC 혼합: [Ex.05c Mixed ADC](../05c_Ext_IO_Mixed_ADC/)
- DIO + ADC 동일 포트 혼용: [Ex.05d Hybrid](../05d_Ext_IO_DIO_ADC_Hybrid/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 부팅 후 LED 1 안 켜짐 | `XM_SwitchDioToAdc` 호출 누락 또는 핀 번호 오타 | `IsDioSwitchedToAdc` 가 true 인지 확인 |
| mV 값이 항상 0 | 분압 저항 (10 kΩ) 누락 → FSR 단독은 전류 흐를 길 없음 | 분압 회로 그림 다시 확인 |
| mV 값이 항상 3300 | FSR 단락 또는 누르지 않은 상태에서 풀업이 ADC 핀에 영향 | 회로 단절 점검 |
| GPIO 로 복구하려 했더니 안 됨 | ADC 전환은 재부팅 전 영구 | 보드 리셋 (RST 버튼) 또는 전원 재인가 |
| `XM_DIO_TO_ADC_PIN` 결과가 이상함 | DIO 1~8 외 핀 사용 | 매크로는 DIO_1~8 에만 유효 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
