# Ex.42 — EMG Hub Biofeedback (근활성도 바이오피드백)

> 🎯 **학습 목표**:
> - **EMG Hub Module**에서 **허브가 신호처리를 마친** 근활성도 데이터를 받아온다.
> - **MVC 정규화**(개인 최대 수축 기준)로 "지금 얼마나 세게 쓰는가"를 0~100%로 읽는다.
> - LED · **PhAI Studio 스트리밍**으로 실시간 바이오피드백을 준다. **모터 구동 없음(안전).**
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐ | 🟢 **Rev 2.0 전용** (FDCAN2 센서허브 버스)
> 🧰 사전 예제: [Ex.09 CDC Stream](../09_CDC_Stream/) · [Ex.40 EMG Proportional Assist](../40_EMG_Proportional_Assist/)
> 📚 관련 docs: [USB 연결성 (0xF0 스트리밍)](../../docs/api-reference/05-usb-connectivity.md) · [LED/BTN 제어](../../docs/api-reference/03-led-btn-control.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

근육을 쓰는 정도를 실시간으로 **되먹임(biofeedback)** 합니다. 재활·트레이닝에서 "지금 목표 근육을 제대로, 얼마나 쓰고 있는지"를 사용자에게 보여주는 것이 목적입니다. 모터를 쓰지 않아 안전하며, EMG Hub 데이터 파이프라인과 MVC 정규화를 익히는 데 집중합니다.

| 요소 | 동작 |
|---|---|
| 데이터 | 허브가 처리한 `envelope_uv`, `mvc_percent`, `is_active`, 캘리브 상태 |
| 캘리브 | **BTN1** = 이완 offset, **BTN2** = 최대수축(MVC) 캡처 → 이후 0~100% 유효 |
| LED1 | `mvc_percent`에 비례해 빠르게 점멸(셀수록 빠름). 캘리브 전엔 heartbeat |
| LED2 | 근수축 감지(`is_active`, Schmitt) 시 점등 |
| 스트리밍 | `0xF0` 채널 4개: envelope / MVC% / active / calib_valid, 50Hz |

> 🟢 **Rev 2.0 전용**: EMG Hub Module은 **FDCAN2 센서허브 버스**로 연결되며, 이 버스는 Rev 2.0 보드에만 있습니다. Rev 1.1에서는 데이터가 수신되지 않습니다.

---

## 2️⃣ EMG Hub Module이란? — 무엇을 할 수 있는 모듈인가

**EMG Hub Module**은 근전도(sEMG) 센서의 미약한 신호를 받아 **허브 안에서 신호처리까지 끝낸** 근활성도를 XM10으로 넘겨주는 **센서 허브**입니다. XM10은 원신호(raw)와 씨름하지 않고, 허브가 뽑아 준 "근활성도 크기"를 받기만 하면 됩니다.

| 특징 | 내용 |
|---|---|
| **채널** | **1채널** sEMG |
| **on-hub 처리** | 정류 → RMS → **Envelope**(근활성도) → **MVC 정규화**까지 허브가 수행 |
| **출력** | envelope(µV), MVC 대비 %(0~100), 근수축 여부(is_active), 캘리브 유효 플래그 |
| **연결** | XM10과 **FDCAN2**(DOP V3) 센서허브 버스로 통신 |

### 🆚 Ex.40과의 차이 (역할 분리)

| | 신호 소스 | 처리 위치 | 출력 |
|---|---|---|---|
| **Ex.40** Proportional Assist | XM10 외부 ADC로 **raw EMG** 직접 | XM10이 DSP | **모터 보조 토크** |
| **Ex.42** (본 예제) | **EMG Hub Module**이 처리한 결과 | **허브**가 DSP | **바이오피드백**(모터 없음) |

즉 Ex.40은 "센서를 직접 다루는" 예제, Ex.42는 "**완성된 센서 모듈을 붙여 쓰는**" 예제입니다.

### 🔌 연결만 하면 끝 — 자동 데이터 수신

전극을 EMG Hub에 연결하고, 허브를 XM10에 연결하기만 하면 됩니다. 별도 설정 없이 **XM10이 자동으로 근활성도 데이터를 받기 시작**합니다. 사용자 코드는 그저 `XM.status.emg_hub`를 읽으면 됩니다.

### 🛠️ PhEEL Studio 연동 (공개 예정)

EMG 신호 파형의 **실시간 확인과 진단**은 Angel Robotics 자체 개발 도구인 **PhEEL Studio**로 합니다. PhEEL Studio는 EMG Hub에 직접 붙어 원신호·envelope·활성 상태를 파형으로 보여줘, 전극 부착 상태나 신호 품질을 눈으로 점검할 수 있습니다. *(PhEEL Studio는 공개 예정입니다.)*

> 📸 `![PhEEL Studio — EMG Hub 근전도 파형 모니터링](../assets/img/42_pheel_studio_emg_hub.png)` placeholder — PhEEL Studio에서 EMG Hub를 연결해 근전도 파형·활성도를 확인하는 화면

> 💡 이 예제는 "EMG 데이터로 **무엇을 할 수 있는가**"의 대표 예 — **바이오피드백** — 을 보여줍니다.

---

## 3️⃣ 사전 지식 — 시작 전 알아둘 것

- **MVC 정규화란** — Maximum Voluntary Contraction(최대 수의 수축). 사람마다 근력·전극 위치가 달라 절대 µV는 비교가 어렵습니다. 그래서 **개인의 최대 수축을 100%로 잡고** 상대값(%)으로 봅니다.
- **2단계 캘리브** — ① **BTN1**(이완): 근육을 뺀 상태의 offset을 잡습니다(허브가 자동 누적). ② **BTN2**(최대수축): 있는 힘껏 수축한 상태의 RMS를 100% 기준으로 캡처합니다. 이후 `mvc_percent`가 유효(`CALIB_VALID`)해집니다.
- **데이터는 읽기만** — `core_process`가 FDCAN2 수신을 받아 `XM.status.emg_hub`를 자동 갱신합니다. 캘리브 **명령만** `EmgHub_Drv_SendCalCommand()`로 허브에 보냅니다.
- **0xF0 User Custom 스트리밍** — `XM_SetUsbCustomMeta()`로 라벨 등록 + `XM_SendUsbDataWithId()`로 전송. ([Ex.09](../09_CDC_Stream/))
- **is_active** — 허브가 Schmitt 트리거로 판정한 "근수축 중" 플래그. 임계 히스테리시스로 채터링을 막습니다.

---

## 4️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#include "emg_hub_drv.h"   // EmgHub_Drv_SendCalCommand, EMGHUB_CAL_CMD_*, EMGHUB_STATUS_CALIB_VALID

/* ① 캘리브 버튼: BTN1=이완 offset, BTN2=최대수축 MVC → 허브로 명령 전송 */
if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) EmgHub_Drv_SendCalCommand(EMGHUB_CAL_CMD_OFFSET);
if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) EmgHub_Drv_SendCalCommand(EMGHUB_CAL_CMD_MVC);

/* ② 활성도 → LED 점멸 주기 (0% 느림 500ms ~ 100% 빠름 100ms). 100% 초과 포화 */
static uint32_t _MvcToBlinkPeriod(uint8_t mvc_percent) {
    uint32_t p = (mvc_percent > 100U) ? 100U : mvc_percent;
    return 500U - (400U * p) / 100U;
}

/* ③ Control_Loop: 캘리브 유효하면 활성도 피드백, LED2=근수축, 0xF0 4채널 스트림 */
bool calib_valid = (emg->status_flags & EMGHUB_STATUS_CALIB_VALID) != 0U;
if (calib_valid) XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, _MvcToBlinkPeriod(emg->mvc_percent));
else             XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);   // 캘리브 전 = 기준 없음
XM_SetLedState(XM_LED_2, emg->is_active ? XM_ON : XM_OFF);
```

전체 코드: [`emg_hub_biofeedback.c`](emg_hub_biofeedback.c)

> 🧒 핵심: 허브가 근활성도까지 다 계산해 줍니다. 여러분은 "그 활성도를 어떻게 사용자에게 되먹일까"를 설계합니다(②의 LED 매핑, §6).

---

## 5️⃣ 실험 — 절차

1. **HW**: EMG Hub Module을 XM10의 FDCAN2 센서허브 포트에 연결. 전극을 목표 근육에 부착.
2. **빌드 + 플래시** → 부팅. 허브 미연결이면 LED1 느린 대기 점멸.
3. **연결 확인** — 허브 연결 시 LED1이 heartbeat(캘리브 전)로 바뀝니다.
4. **이완 캘리브** — 근육을 완전히 뺀 상태에서 **BTN1** 클릭. (디버그 메시지: "Offset 캘리브 시작")
5. **MVC 캘리브** — 목표 근육을 **최대로 수축**한 채 **BTN2** 클릭. 이후 `mvc_percent`가 0~100%로 유효.
6. **피드백 확인** — 근육을 쓰면 LED1이 세기에 비례해 빨라지고, 수축 시 LED2가 켜집니다.
7. **PhAI Studio** — USB Connect → `0xF0` 채널(Envelope / MVC / Active / Calib Valid) 확인.

> ⚠️ 전극 부착·피부 상태에 따라 신호 품질이 달라집니다. 파형이 이상하면 **PhEEL Studio**로 원신호를 먼저 점검하세요.

> 📸 `![PhAI Studio — EMG 근활성도 스트리밍](../assets/img/42_phai_studio_stream.png)` placeholder — envelope·MVC%·활성 상태가 실시간으로 그려지는 화면

---

## 6️⃣ 확장 아이디어 — 여기서 무엇을 더 할 수 있나

이 예제는 **근활성도를 받아 되먹이는 배관**만 제공합니다. EMG 데이터로 할 수 있는 것은 많습니다:

- **A. 목표 구간 피드백** — 목표 %대(예: 40~60%)에 들어오면 초록, 벗어나면 빨강처럼 구간별 피드백.
- **B. 지속시간·반복 카운트** — 임계 이상 수축을 몇 초/몇 회 유지했는지 세어 운동 세트 관리.
- **C. 좌우 대칭 훈련** — EMG Hub 2대(향후)로 좌우 근활성도 균형을 시각화.
- **D. 근피로 추정** — envelope/주파수 변화 추이로 피로 지표 추정.
- **E. 상위 제어 트리거** — 근활성도를 [Ex.40](../40_EMG_Proportional_Assist/)식 보조 제어나 [Ex.16 TinyAI](../16_TinyAI_Sensor_Fusion/)의 입력으로 연결.

> 📝 "어떤 근육을, 어느 강도로, 언제 쓰게 유도할까"가 바이오피드백 설계의 핵심입니다.

---

## 7️⃣ 다음 단계

- User Custom 스트리밍 기초: [Ex.09 CDC Stream](../09_CDC_Stream/)
- EMG → 모터 보조 (외부 ADC): [Ex.40 EMG Proportional Assist](../40_EMG_Proportional_Assist/)
- 온디바이스 AI: [Ex.16 TinyAI Sensor Fusion](../16_TinyAI_Sensor_Fusion/)
- IMU Hub 버전(자세 대시보드): [Ex.41 IMU Hub Dashboard](../41_IMU_Hub_Dashboard/)

---

## ❓ 흔한 실수

| 증상 | 원인 | 해결 |
|---|---|---|
| 데이터 없음 / 반응 없음 | Rev 1.1 보드 (FDCAN2 센서허브 없음) | **Rev 2.0** 보드에서 실행 |
| `is_connected` false | 허브 미연결 / 케이블 / 전원 | FDCAN2 포트·허브 전원 확인 |
| `mvc_percent`가 항상 0 | MVC 캘리브(BTN2) 안 함 | 이완(BTN1) → 최대수축(BTN2) 순서로 캘리브 |
| LED1이 heartbeat에서 안 바뀜 | `CALIB_VALID` 아님 | 캘리브 절차 완료 확인 |
| 값이 튀거나 노이즈 | 전극 부착·피부 접촉 불량 | 전극 재부착, **PhEEL Studio**로 파형 점검 |
| 재연결 후 이상 캘리브 | 미연결 중 버튼 눌림 | 이 예제는 미연결 중 버튼 latch를 비웁니다(재연결 안전) |
| PhAI Studio에 채널 없음 | Connect 안 함 | 포트 선택 → **Connect**, 0xF0 채널 선택 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
