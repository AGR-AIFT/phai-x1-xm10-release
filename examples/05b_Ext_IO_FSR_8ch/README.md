# 예제 05b: FSR 8채널 일괄 전환 + Resolution 설정

본 예제는 **DIO 8개 핀을 한 번에 ADC로 전환**하고, **출력 Resolution을 변경**하여 8채널 FSR 센서 데이터를 효율적으로 수집하는 방법을 다룹니다.

## 🎯 학습 목표 (Objective)

* `XM_SwitchAllDioToAdc()`로 8개 DIO 핀을 **한 줄로** 일괄 전환하는 방법을 학습합니다.
* `XM_SetAnalogReadResolution()`으로 출력 Resolution을 변경하는 방법을 이해합니다.
* `XM_GetAnalogReadResolution()`으로 현재 설정을 조회하는 방법을 익힙니다.
* 배열과 루프를 사용한 **다채널 데이터 수집 패턴**을 학습합니다.

## ⚙️ 동작 원리 (How it Works)

* **Resolution 설정:** `XM_SetAnalogReadResolution(12)`로 12-bit 모드(0~4095)를 설정합니다. 하드웨어 ADC(16-bit)는 변경되지 않으며, 소프트웨어에서 정규화합니다.
* **일괄 전환:** `XM_SwitchAllDioToAdc()`로 DIO 1~8을 한 번에 ADC3로 전환합니다.
* **배열 읽기:** 루프에서 `XM_AnalogRead(XM_EXT_ADC_5 + i)`로 8채널을 순서대로 읽습니다.
* **반응:** 눌린 센서 개수에 따라 LED 패턴을 변경합니다 (0개: OFF, 1~4개: LED1, 5~8개: LED1+LED2).

## 🔌 하드웨어 연결

```
각 채널 (×8):
  3.3V ─── FSR ──┬── DIO_x (PF3~PF10)
                 └── 10kΩ ─── GND
```

## 📊 Resolution 설정 가이드

| 설정값 | 범위 | 용도 |
|--------|------|------|
| 16-bit | 0~65535 | 최대 정밀도 (기본값) |
| **12-bit** | **0~4095** | **FSR, 온도센서 등 일반 센서 (권장)** |
| 10-bit | 0~1023 | Arduino 호환 |
| 8-bit | 0~255 | 간단한 ON/OFF 감지 |

> **팁:** FSR 센서는 12-bit면 충분합니다. 16-bit로 읽어도 센서 자체의 노이즈가 하위 비트를 채우므로, 12-bit로 설정하면 노이즈가 자연스럽게 제거됩니다.

## 🚀 실행 방법 (How to Use)

1. **하드웨어 연결:** FSR 8개를 DIO 1~8번 핀에 각각 분압 회로로 연결합니다.
2. 코드를 업로드하고 전원을 켭니다.
3. **테스트:** FSR 센서를 누르면 눌린 개수에 따라 LED가 반응합니다.
   - 0개 → LED 모두 OFF
   - 1~4개 → LED1 ON
   - 5~8개 → LED1 + LED2 ON

## 💡 직접 해보기 (Things to Try)

* **Resolution 변경:** `FSR_RESOLUTION`을 `10`으로 바꿔 0~1023 범위로 읽어보세요.
* **임계치 조절:** `FSR_PRESS_THRESHOLD`를 높이거나 낮춰 민감도를 조절해보세요.
* **밀리볼트 비교:** `XM_AnalogRead()`를 `XM_AnalogReadMillivolts()`로 교체하여 Resolution 설정 유무에 따른 차이를 확인해보세요.
* 다음 예제 **05c**에서 고정 ADC 핀과 DIO→ADC 핀을 혼합 사용해봅니다.
