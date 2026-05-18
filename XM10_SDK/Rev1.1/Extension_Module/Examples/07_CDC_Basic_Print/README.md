# Ex.07 — CDC Basic Print (USB 시리얼 텍스트 디버깅)

> 🎯 **학습 목표**:
> - USB-CDC 가상 시리얼 포트로 **텍스트 메시지** 를 PC 터미널에 출력합니다.
> - `XM_SendUsbDebugMessage()` + 버튼 이벤트 결합으로 가장 단순한 `printf` 스타일 디버깅 환경 구축.
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.02 Button Event](../02_Button_LED_Event/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

---

## ⚠️ USB-CDC 단일 점유 (먼저 읽으세요)

USB-CDC 포트는 **한 번에 한 클라이언트만** 점유할 수 있습니다:

- ❌ PhAI Studio + PuTTY (또는 다른 시리얼 터미널) 동시 실행 → COM 포트 충돌 → 메시지 안 보임
- ❌ 터미널 두 개 동시 실행 → 동일 문제
- ✅ 본 예제는 **시리얼 터미널 단독** 사용 (PuTTY · TeraTerm · RealTerm 등)
- ✅ 실시간 그래프가 필요하면 [Ex.09 CDC Stream](../09_CDC_Stream/) + **PhAI Studio 단독** 실행

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

**BTN 1 을 누를 때마다** PC 시리얼 터미널에 `"Hello! Button 1 was clicked."` 메시지가 출력됩니다. 동시에 LED 1 이 oneshot (100 ms) 으로 깜빡여 전송을 시각 확인.

> 📸 `![CDC 터미널 출력 예](../assets/img/07_cdc_terminal.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **USB-CDC (Communication Device Class)** — USB 가상 시리얼 포트. 물리 UART 없이 USB 케이블 하나로 양방향 텍스트/바이너리 통신.
- **`XM_SendUsbDebugMessage()`** — `\r\n` 포함한 C 문자열을 송신. non-blocking. ([api-ref](../../docs/api-reference/05-usb-connectivity.md))
- **시리얼 터미널 설정** — 보드레이트 115200, 8N1, 흐름 제어 None.
- **COM 포트 번호** — Windows 장치 관리자에서 보드 연결 시 새로 생기는 COM 포트 (예: `COM3`).

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Loop };
    XM_TSM_AddState(s_tsm, &conf);
}

static void Run_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {              // ① 클릭 이벤트 1회
        XM_SendUsbDebugMessage("Hello! Button 1 was clicked.\r\n");  // ② 텍스트 송신
        XM_SetLedEffect(XM_LED_1, XM_LED_ONESHOT, 100);               // ③ 송신 시각 확인
    }
}
```

전체 코드: [`cdc_basic_print.c`](cdc_basic_print.c)

> 🧒 ② 는 가장 단순 형태. 다음 단계 ([Ex.08](../08_CDC_Sensor_Print/)) 에서 sprintf 로 변수 값을 끼워 넣는 방법 학습.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시** → ✅ `0 errors`
2. **USB 연결** — XM10 의 USB-C 를 PC 에 연결. Windows 장치 관리자에서 새 COM 포트 (예: `COM3`) 확인.
3. **시리얼 터미널 열기** — PuTTY 등에서 해당 COM 포트, 115200 baud, 8N1 설정.
4. **BTN 1 클릭** → ✅ 터미널에 `"Hello! Button 1 was clicked."` 출력 + LED 1 1회 깜빡
5. **여러 번 클릭** → ✅ 매 클릭마다 1줄씩 추가
6. **변형 1 — 다른 메시지**: 메시지 텍스트를 본인 이름이나 학번으로 변경.
7. **변형 2 — BTN 2 도 추가**: 새 if 블록으로 `XM_BTN_2` 도 처리 → 다른 메시지 출력.
8. **변형 3 — 영문 + 한글**: 한글 메시지 시도 (`"버튼 눌림!\r\n"`) — 터미널 인코딩 UTF-8 설정 필요.

---

## 5️⃣ 다음 단계

- 센서 값 출력 (sprintf): [Ex.08 CDC Sensor Print](../08_CDC_Sensor_Print/)
- 고속 바이너리 스트리밍 (PhAI Studio): [Ex.09 CDC Stream](../09_CDC_Stream/)
- 텍스트 디버깅 대안 (보드에 저장): [Ex.10a MSC Basic Log](../10a_MSC_Basic_Log/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 터미널에 아무것도 안 보임 | COM 포트 다른 프로그램 점유 (PhAI Studio 등) | 다른 시리얼 클라이언트 모두 종료 후 재시도 |
| COM 포트가 안 생김 | USB 케이블 데이터 핀 없음 (전원만 가능 케이블) | 데이터 통신 가능 USB-C 케이블 사용 |
| 메시지가 한 줄에 다 붙음 | `\r\n` 누락 | 항상 `"...\r\n"` 형식으로 종료 |
| 한글이 깨짐 | 터미널 인코딩 UTF-8 아님 | PuTTY: Window > Translation > UTF-8 |
| 클릭 한 번에 메시지 여러 줄 | `XM_GetButtonState` 대신 `XM_GetButtonEvent` 누락 | Event read-clear 패턴 ([Ex.02](../02_Button_LED_Event/)) 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
