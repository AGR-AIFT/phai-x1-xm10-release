# Ex.13 — Resistive Mode (H10 내장 저항 운동)

> 🎯 **학습 목표**:
> - H10 내장 저항 제어 로직을 **게인 설정** 만으로 활용 (XM10 에서 토크 직접 계산 X).
> - 슈트 버튼으로 변경되는 **AssistLevel (1~9)** 에 따라 적절한 저항 강도 계산.
> - Event-driven 명령 송신 (값 변경 시에만 전송 → CAN-FD 대역폭 절약).
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.03 FSM](../03_Button_LED_FSM/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 가 **물속에서 걷는 듯한 저항감** 을 제공합니다 (근력 운동용). 사용자가 다리를 빠르게 움직일수록 H10 가 반대 방향으로 더 강한 토크.

| 상태 | 동작 |
|------|------|
| OFF | LED 1 OFF, 저항 게인 = 0 |
| STANDBY | LED 1 Heartbeat (1초). BTN 1 클릭 → OFF, BTN 2 클릭 → ACTIVE |
| ACTIVE | LED 1 ON, 사용자 슈트 AssistLevel (1~9) 에 따라 게인 자동 변경. BTN 2 클릭 → STANDBY |

**제어식 (H10 내부)**:
```
Torque = -Gain × Velocity     (단위: Nm, Gain × rad/s)
```

> 📸 `![저항 운동 데모](../assets/img/13_resistive.gif)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **저항 제어 (Resistive)** — 운동 방향 반대로 토크 생성. 마치 끈끈한 액체 속을 헤엄치는 느낌.
- **`XM_SetResistiveCompGain(node_id, gain)`** — H10 노드 (RH/LH) 에 저항 게인 명령. 음수 = 운동 반대 방향 토크. ([api-ref](../../docs/api-reference/02-h10-control-n-data.md))
- **AssistLevel** — H10 슈트의 사용자 조작 다이얼. 1~9 단계 (1 = 가장 약함, 9 = 가장 강함). `XM.status.h10.h10AssistLevel` 로 읽기.
- **Event-driven 패턴** — 동일한 값 매 cycle 전송하면 CAN-FD 대역폭 낭비. 값 변경 시에만 전송.
- **`s_prev_assist_level = 255`** — 초기값을 0~9 범위 밖으로 잡아 첫 cycle 에서 강제 전송 유도.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define RESISTIVE_GAIN       (-6.0f)   // 기본 게인 (음수 = 저항)
#define VELOCITY_THRESHOLD   (5.0f)    // 노이즈 방지
#define MAX_RESISTIVE_TORQUE (10.0f)   // 안전 한계

static uint8_t s_prev_assist_level = 255;                          // ① 강제 초기 전송 유도

static void Off_Entry(void)
{
    XM_SetResistiveCompGain(SYS_NODE_ID_LH, 0.0f);                  // ② OFF = 게인 0 (안전)
    XM_SetResistiveCompGain(SYS_NODE_ID_RH, 0.0f);
}

static void Active_Entry(void)
{
    s_prev_assist_level = 255;                                       // ③ 재진입 시 강제 업데이트
}

static void Active_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {              // ④ 종료
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    uint8_t current_level = XM.status.h10.h10AssistLevel;            // ⑤ Input: 슈트 레벨

    if (current_level != s_prev_assist_level) {                      // ⑥ 값 변경 시에만
        float gain = (float)current_level * 0.1f * RESISTIVE_GAIN;   //    Level × 0.1 × -6.0
                                                                       //    Level=1 → -0.6 / Level=9 → -5.4
        XM_SetResistiveCompGain(SYS_NODE_ID_LH, gain);               // ⑦ Output: 게인 송신
        XM_SetResistiveCompGain(SYS_NODE_ID_RH, gain);

        char msg[64];
        sprintf(msg, "Level Changed: %d -> Gain: %.2f\r\n", current_level, gain);
        XM_SendUsbDebugMessage(msg);                                  // ⑧ USB 로그

        s_prev_assist_level = current_level;
    }
}

static void Active_Exit(void)
{
    XM_SetResistiveCompGain(SYS_NODE_ID_LH, 0.0f);                  // ⑨ Exit = 게인 0 (안전)
    XM_SetResistiveCompGain(SYS_NODE_ID_RH, 0.0f);
}
```

전체 코드: [`resistive_mode.c`](resistive_mode.c)

> 🧒 ⑥ 의 **Event-driven 패턴** — 매 cycle 전송하지 말고 값 변경 시에만 (CAN-FD 친화).

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW**: KIT H10 ↔ XM10 + 본체 전원
2. **빌드 + 플래시** → ✅ `0 errors`
3. **BTN 1 클릭** (OFF → STANDBY) → ✅ LED 1 Heartbeat
4. **BTN 2 클릭** (STANDBY → ACTIVE) → ✅ LED 1 SOLID
5. **다리 움직임** → ✅ 반대 방향 저항감
6. **슈트 AssistLevel 다이얼** 1 → 9 변경 → ✅ USB 로그에 `Level Changed: ... Gain: ...`
7. **변형 1 — 게인 절대값 변경**: `RESISTIVE_GAIN` -6.0 → -3.0 (약함) 또는 -10.0 (강함).
8. **변형 2 — 비선형 매핑**: `gain = -RESISTIVE_GAIN * sqrtf(current_level)` (Level 차이가 큰 구간에서 더 부드러움).
9. **변형 3 — 최대 토크 제한 추가**: `XM_SetResistiveCompGain` 후 최대 토크 clipping 필요한 경우 H10 PDO 의 `leftHipTorque` 모니터링.

---

## 5️⃣ 다음 단계

- 능동 의도 보조: [Ex.12 Active Assist](../12_Active_Assist_Mode/)
- PD 토크 직접 제어 (학생 알고리즘): [Ex.14 PD Realtime](../14_PD_Realtime_Control/)
- 어드미턴스 제어 (힘 → 위치): [Ex.28 Admittance](../28_Admittance_Control/) (Phase 2D)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 저항감이 안 느껴짐 | OFF 상태에서 시도 (게인 = 0) | BTN 1 → BTN 2 순으로 ACTIVE 진입 |
| 슈트 레벨 변경했는데 게인 미반영 | `s_prev_assist_level` 초기화 255 아님 (실제 레벨로 시작) | Active_Entry 에서 `s_prev_assist_level = 255` 호출 |
| ACTIVE 나갈 때 저항 잔존 | `Active_Exit` 에서 게인 0 reset 누락 | Exit 호출 확인 (안전 핵심) |
| 매 cycle USB 메시지 폭주 | 매 cycle 전송 (Event-driven X) | `if (current_level != prev)` 조건 확인 |
| AssistLevel 가 항상 0 | H10 슈트 미연결 또는 PDO 읽기 실패 | CAN-FD 케이블 + 슈트 전원 |
| 게인이 너무 강해 모터 진동 | `RESISTIVE_GAIN` 절대값 < -8 권장 | 사용자 안전 위해 절대값 작게 시작 |
| 다리 정지 시에도 미세 진동 | `VELOCITY_THRESHOLD` 데드존 무시 (H10 내부 처리 의존) | H10 내장 deadzone 설정 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
