# 예제 05a: DIO→ADC 전환으로 FSR 센서 읽기

본 예제는 **디지털 IO(DIO) 핀을 아날로그 입력(ADC)으로 동적 전환**하여 FSR(Force Sensitive Resistor) 센서를 읽는 방법을 다룹니다. XM 보드의 DIO 1~8번 핀은 런타임에 16-bit ADC3 아날로그 입력으로 전환할 수 있습니다.

## 🎯 학습 목표 (Objective)

* `XM_SwitchDioToAdc()`로 DIO 핀을 ADC 모드로 전환하는 방법을 학습합니다.
* `XM_DIO_TO_ADC_PIN()` 매크로로 DIO 핀 번호를 ADC 핀 번호로 변환하는 방법을 익힙니다.
* `XM_IsDioSwitchedToAdc()`로 전환 상태를 확인하는 방법을 학습합니다.
* `XM_AnalogReadMillivolts()`와 `XM_AnalogRead()` 두 가지 읽기 방법을 비교합니다.

## ⚙️ 동작 원리 (How it Works)

* **핀 전환:** `User_Setup()`에서 `XM_SwitchDioToAdc(XM_EXT_DIO_1)`을 호출하면 PF3 핀이 GPIO에서 ADC3 아날로그 입력으로 전환됩니다. ADC3는 16-bit, 10kHz 샘플링으로 자동 동작합니다.
* **핀 매핑:** DIO 핀 번호와 ADC 핀 번호는 다릅니다. `XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1)`은 `XM_EXT_ADC_5`를 반환합니다.
* **값 읽기:** 전환된 핀은 `XM_AnalogReadMillivolts()`(mV 단위)나 `XM_AnalogRead()`(정규화된 raw)로 읽을 수 있습니다.
* **임계치 판단:** FSR 압력이 0.5V(500mV)를 초과하면 LED를 켭니다.

## 🔌 하드웨어 연결

```
3.3V ─── FSR ──┬── XM_EXT_DIO_1 (PF3)
               └── 10kΩ ─── GND
```

## 📋 DIO → ADC 핀 매핑 표

| DIO 핀 | 물리 핀 | ADC 핀 | 매크로 결과 |
|--------|---------|--------|-------------|
| XM_EXT_DIO_1 | PF3 | XM_EXT_ADC_5 | `XM_DIO_TO_ADC_PIN(XM_EXT_DIO_1)` |
| XM_EXT_DIO_2 | PF4 | XM_EXT_ADC_6 | `XM_DIO_TO_ADC_PIN(XM_EXT_DIO_2)` |
| ... | ... | ... | ... |
| XM_EXT_DIO_8 | PF10 | XM_EXT_ADC_12 | `XM_DIO_TO_ADC_PIN(XM_EXT_DIO_8)` |

## 🚀 실행 방법 (How to Use)

1. **하드웨어 연결:** FSR 센서를 위 회로도대로 DIO 1번 핀에 연결합니다.
2. 코드를 업로드하고 전원을 켭니다.
3. **전환 확인:** LED1이 켜지면 DIO→ADC 전환 성공입니다.
4. **테스트:** FSR 센서를 누르면 LED2가 켜지는 것을 확인합니다.

## 💡 직접 해보기 (Things to Try)

* **다른 핀 사용:** `FSR_PIN`을 `XM_EXT_DIO_3`으로 변경하고, 매핑 매크로가 자동으로 `XM_EXT_ADC_7`을 반환하는지 확인해보세요.
* **임계치 조절:** `FSR_THRESHOLD_MV`를 변경하여 민감도를 조절해보세요.
* 다음 예제 **05b**에서 8채널 FSR을 한 번에 전환하고 Resolution을 설정해봅니다.
