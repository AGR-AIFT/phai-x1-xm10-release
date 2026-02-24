# 예제 05c: 고정 ADC + DIO→ADC 혼합 사용 (최대 12채널)

본 예제는 **고정 ADC 핀(ADC_1~4)**과 **DIO→ADC 전환 핀(ADC_5~12)**을 동시에 사용하여 XM 보드의 아날로그 입력을 최대 12채널까지 활용하는 방법을 다룹니다. 네이티브 Resolution이 다른 핀들을 혼합 사용할 때의 정규화 동작을 이해합니다.

## 🎯 학습 목표 (Objective)

* XM의 **전체 ADC 핀맵**(고정 4핀 + 동적 8핀 = 최대 12핀)을 이해합니다.
* `XM_GetAnalogResolution()`으로 핀별 **네이티브 Resolution 차이**(12-bit vs 16-bit)를 확인합니다.
* `XM_AnalogReadMillivolts()`가 Resolution 설정에 **무관하게 정확한** 것을 확인합니다.
* 고정 ADC 핀(조이스틱)과 DIO→ADC 핀(FSR)을 **동시에 읽는** 패턴을 익힙니다.

## ⚙️ 동작 원리 (How it Works)

* **네이티브 Resolution 확인:** `XM_GetAnalogResolution(XM_EXT_ADC_1)`은 12, `XM_GetAnalogResolution(XM_EXT_ADC_2)`는 16을 반환합니다.
* **선택적 전환:** DIO 1~4만 ADC로 전환하고, DIO 5~8은 GPIO로 유지합니다.
* **밀리볼트 통합 읽기:** 고정 핀(조이스틱)과 전환 핀(FSR) 모두 `XM_AnalogReadMillivolts()`로 읽으면 Resolution 차이와 무관하게 정확한 mV 값을 얻습니다.

## 📋 XM ADC 핀 맵

| Facade 핀 | 물리 핀 | ADC | 네이티브 | 비고 |
|-----------|---------|-----|----------|------|
| XM_ADC_1 | PA0 | ADC1 | 12-bit | [Shared] UART4_TX |
| XM_ADC_2 | PA0_C | ADC2 | 16-bit | 항상 사용 가능 |
| XM_ADC_3 | PA1 | ADC1 | 12-bit | [Shared] UART4_RX |
| XM_ADC_4 | PA1_C | ADC2 | 16-bit | 항상 사용 가능 |
| XM_ADC_5~12 | PF3~PF10 | ADC3 | 16-bit | DIO→ADC 전환 필요 |

> **핵심:** `XM_AnalogRead()`는 설정된 출력 Resolution으로 정규화하여 모든 핀이 동일한 범위를 반환합니다. `XM_AnalogReadMillivolts()`는 네이티브 raw에서 직접 변환하므로 항상 정확합니다.

## 🔌 하드웨어 연결

```
조이스틱:
  X축 출력 ─── XM_EXT_ADC_2 (PA0_C)
  Y축 출력 ─── XM_EXT_ADC_4 (PA1_C)

FSR 센서 (×4):
  3.3V ─── FSR ──┬── DIO_x (PF3~PF6)
                 └── 10kΩ ─── GND
```

## 🚀 실행 방법 (How to Use)

1. **하드웨어 연결:** 조이스틱을 ADC_2/ADC_4에, FSR 4개를 DIO 1~4에 연결합니다.
2. 코드를 업로드하고 전원을 켭니다.
3. **테스트:** 조이스틱을 중립 위치(약 1.65V)에 놓으면 LED1이 켜집니다. 조이스틱을 움직이면 LED1이 꺼집니다.
4. **디버거 확인:** `s_data` 구조체에서 조이스틱 mV와 FSR mV 값을 실시간으로 확인합니다.

## 💡 직접 해보기 (Things to Try)

* **Resolution 테스트:** `XM_SetAnalogReadResolution(12)`를 추가하고, `XM_AnalogRead()`로 고정 ADC 핀(12-bit 네이티브)과 DIO→ADC 핀(16-bit 네이티브)을 읽어 모두 0~4095 범위로 정규화되는지 확인해보세요.
* **12핀 전체 사용:** DIO 5~8도 ADC로 전환하여 12채널 동시 수집을 구현해보세요.
* 다음 예제 **05d**에서 GPIO와 ADC를 혼합하는 실전 시나리오를 다룹니다.
