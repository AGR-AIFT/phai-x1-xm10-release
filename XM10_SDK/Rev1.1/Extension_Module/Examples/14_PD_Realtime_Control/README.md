# Ex.14 — PD Realtime Control (이산 PD 토크 제어)

> 🎯 **학습 목표**:
> - **PD (비례+미분)** 피드백 제어를 1 ms 루프에서 직접 구현.
> - 이산 미분 근사 (Backward Difference) + 토크 포화 (Actuator Saturation) + 안전 Exit 패턴.
> - 학생이 처음 짜는 **사용자 정의 제어 알고리즘** — 이후 Ex.15/20+ 의 출발점.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.12 Active Assist](../12_Active_Assist_Mode/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 가 ASSIST 모드일 때 우측 고관절을 **목표 각도** 로 PD 제어합니다.

| 단계 | 동작 |
|------|------|
| OFF | CM 연결 대기 |
| STANDBY | H10 ASSIST 모드 대기 |
| ACTIVE | PD 토크 제어 시작 |
| **BTN 1** (ACTIVE 중) | 목표 각도 부호 반전 (+ ↔ −) |
| **BTN 2** (ACTIVE 중) | 목표 각도 크기 5°↑ (5 → 10 → ... → 25 → 5 래핑) |

USB CDC 500 ms 주기 디버그: `PD | Tgt:10.0 Cur:8.3 Err:1.7 Tau:0.86` + PhAI 0xF0 채널 4축 실시간 그래프.

> 📸 `![PD 응답 곡선](../assets/img/14_pd_step.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### PD 제어식

```
e[k]   = θ_ref − θ[k]                       (위치 오차)
de[k]  = (e[k] − e[k−1]) / Δt               (이산 미분, Backward)
τ[k]   = Kp · e[k] + Kd · de[k]             (PD 제어)
τ_out  = clamp(τ[k], −MAX, +MAX)            (토크 포화)
```

- **Kp** (Nm/deg) — 스프링 강성 비유. 큰 값 = 빠른 복원 (오버슈트 위험).
- **Kd** (Nm·s/deg) — 댐퍼 비유. 큰 값 = 진동 억제 (느린 응답).
- **본 예제 값**: Kp=0.5, Kd=0.02, MAX_TORQUE=5 Nm.

### 안전 핵심

- `XM_SetControlMode(XM_CTRL_TORQUE)` 진입 시 1회 + `XM_CTRL_MONITOR` Exit 시 복귀
- `Active_Exit` 에서 토크 강제 0 reset (잔류 방지)
- 토크 포화 = actuator saturation 모델

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define KP_GAIN  0.5f
#define KD_GAIN  0.02f
#define CONTROL_DT 0.001f       // 1 ms = 1 kHz

static float s_prev_error = 0.0f;       // ① 이산 미분용 (static 필수!)
static float s_target_angle = 10.0f;

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);   // ② 토크 모드
    s_prev_error = 0.0f;                  //    PD state reset
}

static void Active_Loop(void)
{
    /* H10 STANDBY 복귀 시 즉시 종료 */
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    /* ③ PD 제어 연산 */
    float cur   = XM.status.h10.rightHipMotorAngle;
    float error = s_target_angle - cur;                          // e[k]
    float deriv = (error - s_prev_error) / CONTROL_DT;            // de[k]/dt
    float tau   = KP_GAIN * error + KD_GAIN * deriv;             // PD

    /* ④ 토크 포화 + 송신 */
    s_torque_cmd = _ClampFloat(tau, -MAX_TORQUE_NM, +MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(s_torque_cmd);
    XM_SetAssistTorqueLH(s_torque_cmd);     // 좌/우 동일 (대칭)

    s_prev_error = error;                                          // ⑤ 다음 cycle 용
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);              // ⑥ 잔류 토크 강제 0
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
}
```

전체 코드: [`pd_realtime_control.c`](pd_realtime_control.c)

> 🧒 ⑤ `s_prev_error = error` 가 핵심 — 다음 루프의 de/dt 계산을 위해 보존. `static` 누락 시 매 cycle 0 reset 되어 미분 항이 0.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW**: KIT H10 ↔ XM10 + 본체 전원
2. **빌드 + 플래시 + CM 연결 + ASSIST 진입** → ✅ ACTIVE 진입 시 LED 1 빠른 깜빡 + USB `PD | Tgt:10.0 ...`
3. **다리가 +10° 위치로 부드럽게 이동** → ✅ Tgt - Cur 가 점점 0 으로 수렴
4. **BTN 1 클릭** → ✅ 목표 -10° 로 반전 + LED 2 OFF (방향 표시)
5. **BTN 2 클릭** → ✅ 목표 크기 15° → 20° → 25° → 5° 래핑
6. **변형 1 — Kp 증가**: 0.5 → 2.0 → 빠른 복원, 오버슈트 관찰
7. **변형 2 — Kd 변경**: 0.02 → 0.0 (Kd 없음, 진동) vs 0.1 (강한 댐핑, 느림)
8. **변형 3 — Step response 측정**: BTN 2 로 갑작스러운 목표 변경 → PhAI 그래프로 응답 곡선 (Rise time / Overshoot / Settling time) 측정
9. **변형 4 — 좌/우 비대칭**: `XM_SetAssistTorqueLH(0)` 으로 우측만 제어 → 좌/우 동작 차이 관찰

---

## 5️⃣ 다음 단계

- 모델 기반 + 중력 보상: [Ex.15 Inverted Pendulum](../15_Inverted_Pendulum_Control/)
- 보행 단계별 차등 토크: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- 임피던스 제어 (Hogan): [Ex.20 Impedance](../20_Impedance_Control/) (Phase 2D)
- 외란 관측기 (DOB): [Ex.31 Friction Comp DOB](../31_Friction_Comp_DOB/) (Phase 2D)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 매 cycle 미분 = 0 | `s_prev_error` 가 `static` 아님 → 매 호출 0 초기화 | `static float s_prev_error = 0.0f;` |
| 진동 (oscillation) | Kd = 0 또는 너무 작음 | Kd 단계적 증가 (0 → 0.01 → 0.02 → 0.05) |
| 큰 오버슈트 (Overshoot) | Kp 너무 큼 | Kp 절반으로 |
| 토크가 항상 ±5 Nm 포화 | 오차가 항상 큼 → 모터 미응답 (`SetControlMode(TORQUE)` 누락) | Active Entry 에서 모드 설정 확인 |
| H10 STANDBY 복귀 후 잔류 토크 | `Active_Exit` 에서 토크 0 reset 누락 | Exit 함수에서 `SetAssistTorque(0)` + MONITOR 모드 |
| 미분 항 노이즈로 토크 진동 | 1 kHz Backward Difference + 측정 노이즈 | 미분 측에 LPF 추가 또는 Kd ↓ |
| BTN 1 클릭에 LED 2 안 바뀜 | LED 2 상태 갱신 if/else 누락 | `_HandleButtonInput` 의 LED 분기 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
