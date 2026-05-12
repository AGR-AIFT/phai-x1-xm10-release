# Ex.04 — External IO Basic (외부 스위치 + 외부 LED)

> 🎯 **학습 목표**:
> - 외부 확장 포트 (`XM_EXT_DIO_*`) 의 디지털 입출력을 사용합니다.
> - `XM_SetPinMode()` + `XM_DigitalRead/Write()` + 내부 풀업 저항 활용을 익힙니다.
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.01 Button & LED Basic](../01_Button_LED_Basic/) | 📚 관련 docs: [External IO](../../docs/api-reference/04-external-io.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

외부 확장 포트에 연결한 **스위치를 누르면 외부 LED 가 켜지고, 떼면 꺼집니다.**

- 외부 스위치는 DIO 3 (PF5) 에 연결 — 내부 풀업 사용 (풀업 저항 별도 X)
- 외부 LED 는 DIO 4 (PF6) 에 출력 — 3.3 V 또는 0 V

> 📸 `![외부 스위치 + LED 연결도](../assets/img/04_ext_io_basic.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **DIO (Digital IO)** — 외부 신호용 GPIO. 입력은 0 V (LOW) / 3.3 V (HIGH), 출력도 동일.
- **내부 풀업 (Input Pullup)** — MCU 내부 저항으로 핀을 HIGH 로 끌어올림. 스위치 한쪽을 GND 로 두면 누를 때 LOW 가 됨. 외부 풀업 저항 불필요.
- **Active Low 패턴** — 풀업 사용 시 "버튼 눌림 = LOW". 자주 헷갈리는 부분.
- **DIO 핀 매핑**: DIO 1~8 = PF3~PF10 ([api-ref](../../docs/api-reference/04-external-io.md))

> ⚠️ 외부 LED 전류 한계: MCU GPIO 는 ~20 mA. 그 이상 전류 필요한 LED 는 트랜지스터 / MOSFET 으로 스위칭하세요.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void User_Setup(void)
{
    XM_SetPinMode(XM_EXT_DIO_3, XM_EXT_DIO_MODE_INPUT_PULLUP);  // ① 외부 스위치: 풀업 입력
    XM_SetPinMode(XM_EXT_DIO_4, XM_EXT_DIO_MODE_OUTPUT);        // ② 외부 LED: 출력

    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
    XM_TSM_AddState(s_tsm, &conf);
}

static void Run_Loop(void)
{
    XmLogicLevel_t sw = XM_DigitalRead(XM_EXT_DIO_3);            // ③ 스위치 상태 읽기

    if (sw == XM_LOW) {                                           // ④ Active Low 패턴
        XM_DigitalWrite(XM_EXT_DIO_4, XM_HIGH);                    // ⑤ LED ON (3.3V)
    } else {
        XM_DigitalWrite(XM_EXT_DIO_4, XM_LOW);                     // ⑥ LED OFF (0V)
    }
}
```

전체 코드: [`ext_io_basic.c`](ext_io_basic.c)

> 🧒 핵심은 ④ — 풀업 사용 시 **눌림 = LOW**. `XM_HIGH` 와 헷갈리지 마세요.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 연결**: 외부 스위치 (탑재형 푸시 버튼) 한쪽을 DIO_3, 반대쪽을 GND. LED + 전류 제한 저항 (220Ω~1kΩ) 을 DIO_4 와 GND 사이.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **스위치 누름** → ✅ 외부 LED 켜짐
4. **스위치 뗌** → ✅ 외부 LED 꺼짐
5. **변형 1 — Active High**: 풀업 대신 풀다운 (`XM_EXT_DIO_MODE_INPUT_PULLDOWN`) 사용 후 스위치 한쪽을 3.3 V 로 연결 → `if (sw == XM_HIGH)` 로 조건 변경.
6. **변형 2 — 이중 입력**: DIO_5 에 두 번째 스위치 추가 → 두 스위치 모두 눌렀을 때만 LED 켜지도록 (AND 로직).
7. **변형 3 — 토글 동작**: Ex.02 의 이벤트 처리 패턴을 외부 스위치에 적용 — 누를 때마다 LED 토글 (Edge detection 필요).

---

## 5️⃣ 다음 단계

- 아날로그 입력: [Ex.05 Ext IO Analog](../05_Ext_IO_analog/) — 가변저항/센서 전압 읽기
- 안전 인터록 (외부 스위치 + FSM): [Ex.06 Ext IO Safety Switch](../06_Ext_IO_Safety_Switch/)
- USB 통신 (외부 IO 와 PC 데이터 교환): [Ex.07 CDC Basic Print](../07_CDC_Basic_Print/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 스위치 안 눌렀는데 LED 가 켜져있음 | 풀업 미적용 (Floating 입력 → 불안정) | `XM_EXT_DIO_MODE_INPUT_PULLUP` 명시 확인 |
| 스위치 눌렀는데 LED 가 안 켜짐 | Active Low/High 혼동 | 풀업 = LOW 눌림 / 풀다운 = HIGH 눌림 |
| LED 가 흐릿하게 켜짐 | 전류 제한 저항 누락 | 220Ω~1kΩ 직렬 연결 (3.3V / 20mA) |
| 외부 LED 가 동작 안 함 | LED 극성 (anode/cathode) 반대 | 긴 다리 = +, DIO_4 쪽 |
| 스위치 누름에 LED 깜빡거림 (chattering) | 기계식 바운싱 | Ex.02 이벤트 처리 또는 SW 디바운싱 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
