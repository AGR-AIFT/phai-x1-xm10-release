# Ex.05 — External IO Analog (고정 ADC 핀 전압 측정)

> 🎯 **학습 목표**:
> - 고정 ADC 핀 (`XM_EXT_ADC_1~4`) 에서 전압을 mV 단위로 직관적으로 읽습니다.
> - 임계치 (Threshold) 기반 LED 제어로 가변저항/센서 신호를 활용합니다.
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.04 Ext IO Basic](../04_Ext_IO_Basic/) | 📚 관련 docs: [External IO](../../docs/api-reference/04-external-io.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

XM 보드의 **고정 아날로그 핀 ADC_1 (PA0) 에 가변저항** 을 연결하고, **출력 전압이 2 V 를 넘으면 LED 1 이 켜집니다.**

> 📸 `![가변저항 연결도](../assets/img/05_pot_to_adc.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **ADC (Analog-to-Digital Converter)** — 0 ~ 3.3 V 의 연속 전압을 디지털 숫자로 변환.
- **고정 ADC 핀 vs DIO→ADC 핀**:
  - 고정 (`XM_EXT_ADC_1~4` = PA0/PA0_C/PA1/PA1_C): 항상 ADC, GPIO 전환 불가
  - 동적 (`XM_EXT_ADC_5~12` = PF3~PF10): 본래 DIO, `XM_SwitchDioToAdc()` 호출 후 ADC 로 전환 ([Ex.05a 참조](../05a_Ext_IO_DIO_to_ADC/))
- **`XM_AnalogReadMillivolts()`** — Resolution 설정과 무관하게 항상 정확한 **밀리볼트** 반환 (raw → mV 자동 변환).

### XM ADC 시리즈 안내 (Ex.05 → 05d 순차 학습 권장)

| 예제 | 핵심 |
|------|------|
| **05** (본 예제) | 고정 ADC 1 핀 (기초) |
| [05a](../05a_Ext_IO_DIO_to_ADC/) | DIO 1핀 → ADC 전환 (FSR 1개) |
| [05b](../05b_Ext_IO_FSR_8ch/) | DIO 8핀 일괄 ADC 전환 + Resolution 설정 |
| [05c](../05c_Ext_IO_Mixed_ADC/) | 고정 ADC + 동적 ADC 혼합 (최대 12채널) |
| [05d](../05d_Ext_IO_DIO_ADC_Hybrid/) | DIO + ADC 동일 포트에서 혼합 (실전 시나리오) |

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);                  // ① TSM 만 등록 — ADC 별도 초기화 X
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
    XM_TSM_AddState(s_tsm, &conf);
}

static void Run_Loop(void)
{
    uint16_t mv = XM_AnalogReadMillivolts(XM_EXT_ADC_1);          // ② 0~3300 mV 반환

    if (mv > 2000) {                                               // ③ 임계치 2 V
        XM_SetLedState(XM_LED_1, XM_ON);
    } else {
        XM_SetLedState(XM_LED_1, XM_OFF);
    }
}
```

전체 코드: [`ext_io_analog.c`](ext_io_analog.c)

> 🧒 ② `XM_AnalogReadMillivolts` 는 결과가 mV 단위라 사람이 바로 이해. 0 V → `0`, 3.3 V → `3300`.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 연결**: 가변저항의 양 끝을 3.3 V 와 GND, 와이퍼 (가운데) 를 PA0 (ADC_1) 핀에 연결.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **가변저항 천천히 돌리기** → ✅ 어떤 지점에서 LED 1 이 ON/OFF 됨
4. **변형 1 — 임계치 변경**: `mv > 2000` 의 `2000` 을 `500` 또는 `3000` 으로 → LED 켜지는 위치가 달라짐.
5. **변형 2 — 다른 핀 사용**: `XM_EXT_ADC_2` (PA0_C) 로 변경 → 동일 동작 (PA0_C 는 항상 사용 가능, 16-bit 네이티브로 더 정밀).
6. **변형 3 — USB 출력**: `XM_SendUsbDebugMessage` 로 mv 값을 1초마다 출력해서 시리얼 터미널에서 모니터링 (sprintf 패턴은 [Ex.08](../08_CDC_Sensor_Print/) 참조).

---

## 5️⃣ 다음 단계

- DIO 를 ADC 로 전환: [Ex.05a DIO to ADC](../05a_Ext_IO_DIO_to_ADC/) — FSR 1개
- 8채널 일괄 전환: [Ex.05b FSR 8ch](../05b_Ext_IO_FSR_8ch/)
- 고정 + 동적 혼합: [Ex.05c Mixed ADC](../05c_Ext_IO_Mixed_ADC/)
- 안전 인터록 + 외부 IO: [Ex.06 Safety Switch](../06_Ext_IO_Safety_Switch/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| mV 값이 항상 0 또는 3300 부근 | 핀이 Floating 또는 단락 | 가변저항 양 끝 3.3V/GND 연결 확인, 와이퍼가 ADC 핀에 닿는지 |
| mV 값이 흔들림 (노이즈) | 짧지 않은 케이블 / 그라운드 차이 | 짧은 점퍼선 사용, 보드 GND 와 센서 GND 공통 |
| Resolution 변경했는데 mV 값이 바뀜 | `XM_AnalogRead` 와 `AnalogReadMillivolts` 혼동 | `mV` API 는 항상 정확. Resolution 영향은 raw 만 |
| ADC_1 + UART4 동시 사용 시 작동 안 함 | PA0 가 UART4_TX 와 shared | Rev1.1 만 해당. Rev2.0 은 별도 UART 포트 |
| 임계치 2000 인데 LED 가 1.5 V 에서 켜짐 | ADC 영점 보정 미고려 또는 ground bounce | 멀티미터로 실제 전압 확인 + Threshold 조정 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
