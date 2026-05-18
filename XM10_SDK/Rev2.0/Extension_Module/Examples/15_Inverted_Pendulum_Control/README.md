# Ex.15 — Inverted Pendulum Control (중력 보상 + PD, 모델 기반 제어)

> 🎯 **학습 목표**:
> - 인체 보행 시 골반을 **역진자 모델** 로 근사 → 중력 보상 + PD 안정화 토크 결합.
> - **에너지 관점 안정성** (Lyapunov) — 중력 보상으로 위치 E 제거 + Kd 로 운동 E 소산.
> - 4-state TSM (OFF/STANDBY/ACTIVE/ERROR) + Homing → Control 2단계 + 안전 한계 비상 정지.
>
> ⏱️ 권장 시간: 50분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.14 PD Realtime](../14_PD_Realtime_Control/) + [Ex.11 Passive (Homing 패턴)](../11_Passive_Mode/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 가 ASSIST 모드일 때 골반 기울기를 **기준 자세 부근** 으로 안정화합니다.

| 상태 | 동작 |
|------|------|
| OFF / STANDBY | CM 연결 + ASSIST 대기 |
| ACTIVE Homing | 모터 0° 위치 정렬 (Passive Mode 와 동일 패턴) |
| ACTIVE Control | 역진자 제어 시작 (BTN 1: 보조 레벨 0/0.3/0.6/1.0, BTN 2: 기준 각도 reset) |
| ERROR | 골반 ±30° 초과 → 즉시 토크 0 + LED 3-색 깜빡 |

USB CDC `IP | th:0.05 tg:1.23 tp:0.45 tau:1.68` + PhAI 0xF0 채널 5축 (theta, theta_dot, tau_gravity, tau_pd, tau_total).

> 📸 `![역진자 모델 + 토크 분해](../assets/img/15_inverted_pendulum.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 역진자 동역학

```
I · θ̈ = M · g · L · sin(θ)            (비선형, 무중력 시 θ=0 이 안정)
```
- M = 체중 (kg), g = 9.81 m/s², L = 다리 길이 (m), θ = 골반 기울기 (rad)
- I = M · L² (관성 모멘트, 근사)

### 제어 법칙

```
τ_gravity = M · g · L · sin(θ)                          (중력 보상 — 중력 모멘트 상쇄)
τ_pd      = Kp · (θ_ref − θ) + Kd · (0 − θ̇)           (PD 안정화)
τ_total   = gain_scale · (τ_gravity + τ_pd)             (보조 레벨 스케일)
τ_out     = clamp(τ_total, −MAX, +MAX) / 2              (좌/우 50:50 분배)
```

### Lyapunov 안정성 직관

- 중력 보상 → 역진자가 "무중력" → 자유롭게 움직일 수 있는 시스템
- Kp → 기준 자세로 끌어당기는 가상 스프링
- Kd → 운동 에너지 소산 (감쇠) → 안정점 부근 진동 억제

### 본 예제 값

- 체중 70 kg, 다리 0.9 m → 중력 항 최대 = 70 × 9.81 × 0.9 × sin(30°) ≈ 309 Nm (이론값, 실제 좌/우 분배 + MAX 8 Nm 클램프)
- Kp=15 Nm/rad, Kd=3 Nm·s/rad
- 안전 한계: ±30° 초과 → ERROR 전환

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define M_BODY_KG 70.0f
#define L_LEG_M   0.9f
#define G_ACC     9.81f
#define IP_KP_GAIN 15.0f
#define IP_KD_GAIN 3.0f
#define ANGLE_LIMIT_DEG 30.0f                              // 안전 한계

static void _RunInvertedPendulumControl(void)
{
    /* ① 센서 데이터 (deg → rad) */
    float theta_deg = XM.status.h10.pelvicAngle;
    float theta     = DEG_TO_RAD(theta_deg);
    /* 골반 각속도 = 좌/우 IMU GyroY 평균 */
    float pelvic_vel_y = (XM.status.h10.leftHipImuGlobalGyrY
                       + XM.status.h10.rightHipImuGlobalGyrY) * 0.5f;
    float theta_dot = DEG_TO_RAD(pelvic_vel_y);

    /* ② 안전 검사 — 한계 초과 시 즉시 ERROR */
    if (fabsf(theta_deg) > ANGLE_LIMIT_DEG) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ERROR);
        return;
    }

    /* ③ 중력 보상: τ_gravity = M·g·L·sin(θ) */
    s_tau_gravity = M_BODY_KG * G_ACC * L_LEG_M * sinf(theta);

    /* ④ PD 안정화: τ_pd = Kp·(θ_ref − θ) + Kd·(0 − θ̇) */
    float theta_error = s_theta_ref - theta;
    s_tau_pd = IP_KP_GAIN * theta_error + IP_KD_GAIN * (-theta_dot);

    /* ⑤ 보조 레벨 스케일 + 포화 */
    float tau_raw = s_gain_scale * (s_tau_gravity + s_tau_pd);
    s_tau_total = _ClampFloat(tau_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    /* ⑥ 좌/우 대칭 분배 (50:50) */
    XM_SetAssistTorqueRH(s_tau_total * 0.5f);
    XM_SetAssistTorqueLH(s_tau_total * 0.5f);
}

static void Error_Entry(void)
{
    _SafetyShutdown();                                       // ⑦ 토크 + 임피던스 해제
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 100);            // 3-LED 모두 깜빡
    XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 100);
    XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 100);
}
```

전체 코드: [`inverted_pendulum_control.c`](inverted_pendulum_control.c) (720 줄, 큰 예제)

> 🧒 ③④ 의 결합이 핵심 — 중력 보상으로 시스템을 "무중력화" 한 위에 단순 PD 적용.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW + 빌드/플래시** → CM 연결 후 ASSIST 진입
2. **Homing 완료** → ✅ 0° 위치 + Control 단계 진입 (LED 1 빠른 깜빡)
3. **BTN 1 클릭** → ✅ Gain 0 → 0.3 → 0.6 → 1.0 → 0 순환, LED 2/3 표시 변화
4. **착용자가 약간 앞으로 기울임** → ✅ 중력 보상 토크가 자연스럽게 뒤로 끌어당김
5. **BTN 2 클릭** → ✅ 현재 자세를 새 기준점으로 리셋 (USB 로그 `[IP] BTN2: 기준 각도 리셋`)
6. **30° 이상 기울임** → ✅ ERROR 진입, 토크 즉시 0, LED 3개 모두 빠른 깜빡
7. **변형 1 — 체중 변경**: `M_BODY_KG` 50 또는 90 으로 → 중력 보상 비례 변화
8. **변형 2 — Kp/Kd**: Kp 10 (약함) vs 25 (강함) → 자세 복원 속도. Kd 1 (진동) vs 5 (느림) → 안정성
9. **변형 3 — Soft Saturation 추가**: clamp 대신 `tanh(tau/MAX) * MAX` 사용 → 부드러운 포화
10. **변형 4 — 소각 근사 사용**: `sin(theta)` → `theta` 로 변경 (|θ| < 15° 에서 오차 < 1%), 연산 비용 비교

---

## 5️⃣ 다음 단계

- 보행 단계별 차등 제어: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- Hogan 임피던스 제어: [Ex.20 Impedance Control](../20_Impedance_Control/) (Phase 2D)
- DOB (외란 관측기) 투명 모드: [Ex.31 Friction Comp DOB](../31_Friction_Comp_DOB/) (Phase 2D)
- HZD 가상 구속: [Ex.24 Virtual Constraint](../24_Virtual_Constraint/) (Phase 2D)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| Homing 무한 대기 | `isPVectorDone` flag clear 누락 | Wait 단계에서 `ClearPVectorDoneFlag` 호출 |
| 중력 보상이 너무 강해 모터 진동 | M_BODY_KG 가 실제 체중보다 큼 | 정확한 체중 값 적용 |
| ERROR 진입 후 복귀 안 됨 | `Error_Loop` 가 STANDBY 복귀 조건 만족 안 함 | H10 슈트 ASSIST → STANDBY 변경 필요 |
| Gain 1.0 인데 효과 미미 | MAX_TORQUE_NM 클램프 (8 Nm) 가 너무 작음 | 안전 검토 후 단계적 증가 |
| 각도 한계 30° 가 너무 보수적 | 실험 안전 위해 의도된 값 | 임상 환경 외에서는 작게 유지 |
| theta_dot 가 노이즈 | 좌/우 GyroY 평균 + 1 kHz 샘플링 노이즈 | LPF 추가 또는 Kd ↓ |
| 좌/우 비대칭 운동 | 50:50 분배가 균일 분배 | 실제 비대칭 보상 시 분배 비율 조정 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
