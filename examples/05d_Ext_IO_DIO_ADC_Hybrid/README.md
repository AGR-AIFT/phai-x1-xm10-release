# 예제 05d: DIO/ADC 혼합 모드 — 버튼 + LED + FSR 실전 시나리오

본 예제는 **동일한 DIO 포트에서 일부 핀은 GPIO(버튼, LED), 일부 핀은 ADC(FSR 센서)로 동시에 사용**하는 실전 시나리오를 다룹니다. ADC로 전환된 핀에 GPIO 함수를 호출해도 **보호 장치가 안전하게 무시**하는 것을 확인합니다.

## 🎯 학습 목표 (Objective)

* 동일한 DIO 포트에서 **GPIO와 ADC를 혼합 사용**하는 방법을 학습합니다.
* ADC 전환된 핀에 GPIO 접근 시 **보호 장치(Guard)**가 작동하여 충돌이 발생하지 않음을 확인합니다.
* `XM_IsDioSwitchedToAdc()`로 핀 모드를 **런타임에 확인**하는 방법을 익힙니다.
* 외부 버튼 **Edge Detection** + FSR 측정 + LED 표시를 조합한 **실전 패턴**을 학습합니다.

## ⚙️ 동작 원리 (How it Works)

* **핀 역할 분배:**
  - DIO 1~4 → ADC 전환 (FSR 센서)
  - DIO 5 → GPIO Input (외부 버튼, 내부 풀업)
  - DIO 6 → GPIO Output (상태 표시 LED)
  - DIO 7~8 → 미사용 (기본 Input Floating)
* **보호 장치:** ADC로 전환된 DIO 1번에 `XM_SetPinMode()`, `XM_DigitalWrite()`, `XM_DigitalRead()`를 호출해도 안전하게 무시됩니다.
* **측정 모드 토글:** 외부 버튼을 누를 때마다 측정 모드가 ON/OFF 토글됩니다. Edge Detection으로 버튼 눌림만 감지합니다.
* **데이터 수집:** 측정 모드일 때만 FSR 4채널의 밀리볼트 값을 수집하고, 총 압력이 임계치를 초과하면 내부 LED로 알림합니다.

## 🔌 하드웨어 연결

```
FSR 센서 (×4):
  3.3V ─── FSR ──┬── DIO 1~4 (PF3~PF6)
                 └── 10kΩ ─── GND

외부 버튼:
  DIO 5 (PF7) ─── 버튼 ─── GND  (내부 풀업 사용)

외부 LED:
  DIO 6 (PF8) ─── 330Ω ─── LED(+) ─── GND
```

| DIO 핀 | 역할 | 모드 |
|--------|------|------|
| DIO 1~4 | FSR 센서 | ADC (전환) |
| DIO 5 | 외부 버튼 | GPIO Input (Pullup) |
| DIO 6 | 상태 LED | GPIO Output |
| DIO 7~8 | 미사용 | GPIO Input (Floating) |

## 🛡️ 보호 장치 동작 확인

코드의 `User_Setup()`에서 보호 장치 테스트를 수행합니다:

```c
// ADC 전환된 DIO 1번에 GPIO 함수 호출 → 안전하게 무시됨
XM_SetPinMode(XM_EXT_DIO_1, XM_EXT_DIO_MODE_OUTPUT);  // 무시됨
XM_DigitalWrite(XM_EXT_DIO_1, XM_HIGH);                // 무시됨
XM_DigitalRead(XM_EXT_DIO_1);                          // XM_LOW 반환
```

## 🚀 실행 방법 (How to Use)

1. **하드웨어 연결:** 위 회로도대로 FSR 4개, 버튼 1개, LED 1개를 연결합니다.
2. 코드를 업로드하고 전원을 켭니다.
3. **대기 상태:** 외부 LED 꺼짐, 내부 LED 꺼짐.
4. **버튼 1회 누름:** 측정 모드 ON → 외부 LED 켜짐, FSR 측정 시작.
5. **FSR 세게 누르기:** 총 압력 > 6V(6000mV)이면 내부 LED1 켜짐.
6. **버튼 다시 누름:** 측정 모드 OFF → 외부 LED 꺼짐, 측정 중지.

## 💡 직접 해보기 (Things to Try)

* **채널 확장:** DIO 5를 버튼 대신 FSR로 사용하고, 내부 버튼(BTN1)으로 측정 모드를 제어해보세요.
* **총 압력 임계치 변경:** `6000`을 `3000`으로 낮춰 민감도를 높여보세요.
* **CDC 스트리밍 결합:** 예제 09(CDC Stream)와 결합하여 FSR 데이터를 PC에서 실시간 그래프로 확인해보세요.
