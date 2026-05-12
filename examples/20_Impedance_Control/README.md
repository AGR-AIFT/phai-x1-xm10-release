# Ex.20 — Impedance Control (Hogan 1985 — 가상 스프링-댐퍼 상호작용)

> 🎯 **학습 목표**:
> - **Hogan 임피던스 제어** 의 PD 와의 본질적 차이 — "오차 억제" 가 아닌 "**상호작용 관계 정의**".
> - 가상 강성 K + 가상 감쇠 B + 평형점 θ_d 로 로봇의 **기계적 임피던스** 매개변수화.
> - 강성 / 감쇠 / 평형점 3개를 버튼으로 실시간 조절하며 체감.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.14 PD](../14_PD_Realtime_Control/) (PD 와 비교) + [Ex.15 Inverted Pendulum](../15_Inverted_Pendulum_Control/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Hogan, N. (1985). *Impedance control: An approach to manipulation.* ASME J. Dyn. Sys., 107(1), 1–24.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 가 ASSIST 모드일 때 좌·우 고관절이 **가상의 스프링-댐퍼 시스템** 처럼 동작:

| 강성 K (Nm/deg) | 평형점 θ_d 부근 느낌 |
|----------------|--------------------|
| 0.1 (Soft) | 거의 자유로움, 약한 복원 |
| 0.3 (Medium) | 명확한 "원위치 끌어당김" |
| 0.8 (Stiff) | 단단한 스프링 |

| 감쇠 B (Nm·s/deg) | 움직임 느낌 |
|------------------|----------|
| 0.005 (Light) | 빠른 흔들림, 약간 진동 가능 |
| 0.02 (Medium) | 균형 |
| 0.05 (Heavy) | 끈끈한 액체 속 같은 느릿함 |

USB CDC `IMP | K:0.3 B:0.02 Eq:5.0 θ:3.2 τ:0.56` 매 500 ms + PhAI 0xF0 4ch.

> 📸 `![가상 스프링-댐퍼 + 평형점](../assets/img/20_impedance_phase.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### PD 제어 vs Impedance 제어 — 철학적 차이

| | PD (Ex.14) | Impedance (이 예제) |
|--|-----------|--------------------|
| **목적** | 목표 위치 추종 | 상호작용 관계 정의 |
| **사용자 힘** | "외란" → 억제 대상 | "상호작용" → 가상 스프링으로 반응 |
| **수식** | τ = Kp·e + Kd·de/dt | τ = K·(θ_d − θ) + B·(0 − θ̇) |
| **느낌** | "로봇이 끌어감" | "로봇이 스프링" |

> 🧒 **수학적으로는 유사**. 차이는 **해석 + 게인 의도**. PD 의 Kp 를 "위치 게인", Impedance 의 K 를 "강성" 으로 부르는 것이 본질.

### Hogan (1985) 의 핵심 통찰

1. 접촉 환경에서 **위치와 힘을 동시에 독립 제어 불가** (one degree per port).
2. 대신 위치-힘 사이의 **관계 (=임피던스)** 를 제어.
3. 이 임피던스를 **가상 스프링-댐퍼-관성** 으로 표현 → 안정성 보장.

### 본 예제 게인

- K: 0.1 / 0.3 / 0.8 Nm/deg (Soft / Medium / Stiff)
- B: 0.005 / 0.02 / 0.05 Nm·s/deg (Light / Medium / Heavy)
- θ_d: −25 ~ +25° (BTN3 으로 5° 단위 조정)
- τ 포화: ±5 Nm

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define MAX_TORQUE_NM   5.0f
#define CONTROL_DT      0.001f                                     // 1 kHz

static float s_K_stiffness = 0.3f;                                 // ① 가상 강성
static float s_B_damping   = 0.02f;                                //    가상 감쇠
static float s_eq_pos_deg  = 0.0f;                                 //    평형점

static float s_prev_angle_deg = 0.0f;

static void Active_Loop(void)
{
    float theta = XM.status.h10.rightHipAngle;                      // ② 현재 각도
    /* 후향 차분으로 각속도 추정 — 노이즈 시 LPF 필요 */
    float theta_dot = (theta - s_prev_angle_deg) / CONTROL_DT;      // ③ 이산 미분
    s_prev_angle_deg = theta;

    /* ④ Hogan 임피던스: τ = K(θ_d − θ) + B(0 − θ̇) */
    float tau = s_K_stiffness * (s_eq_pos_deg - theta)
              + s_B_damping   * (-theta_dot);

    tau = _ClampFloat(tau, -MAX_TORQUE_NM, MAX_TORQUE_NM);          // ⑤ 토크 포화
    XM_SetAssistTorqueRH(tau);                                      // ⑥ 좌·우 동일
    XM_SetAssistTorqueLH(tau);
}

static void _HandleButtons(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK)                // BTN1: K 프리셋 순환
        s_K_stiffness = stiffness_presets[(++stiffness_idx) % NUM_STIFFNESS_PRESETS];
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK)                // BTN2: B 프리셋
        s_B_damping = damping_presets[(++damping_idx) % NUM_DAMPING_PRESETS];
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {              // BTN3: 평형점 ±5°
        s_eq_pos_deg += EQUILIBRIUM_STEP_DEG;
        if (s_eq_pos_deg > EQUILIBRIUM_MAX_DEG) s_eq_pos_deg = EQUILIBRIUM_MIN_DEG;
    }
}
```

전체 코드: [`impedance_control.c`](impedance_control.c) (592 줄)

> 🧒 ④ 가 모든 것. K 와 B 의 의미 + 평형점 θ_d 가 핵심. PD 와 수학적으로 같지만 **K=강성** 으로 부르는 순간 해석이 달라짐.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시 + CM ASSIST 진입** → ✅ LED 1 빠른 깜빡 (임피던스 ON)
2. **다리 움직이지 않음** → ✅ 평형점 θ_d=0° 부근으로 천천히 끌어당김 (느낌 약함)
3. **다리를 잡고 흔들어봄** → ✅ 가상 스프링 같은 복원력 + 감쇠 느낌
4. **BTN 1** 3회 클릭 → ✅ K 0.1 → 0.3 → 0.8 → 0.1 순환. 스프링이 단단해짐 체감
5. **BTN 2** 3회 클릭 → ✅ B 0.005 → 0.02 → 0.05. 흔들 때 끈끈함 변화
6. **BTN 3** 클릭 → ✅ 평형점 +5° 이동. 다리가 5° 위치로 끌림
7. **USB CDC** → `IMP | K:0.30 B:0.02 Eq:5.0 θ:4.2 τ:0.24` 매 500 ms
8. **변형 1 — Pure Stiffness (B=0)**: 진동 발생 관찰 (감쇠 없이 스프링만)
9. **변형 2 — Pure Damping (K=0)**: 복원력 없음, 액체 같은 느낌
10. **변형 3 — PD 와 비교**: 같은 K, B 값으로 [Ex.14](../14_PD_Realtime_Control/) 와 응답 비교 — 수학적으로 동일하지만 평형점 개념의 유무
11. **변형 4 — 좌·우 비대칭**: `XM_SetAssistTorqueLH(0)` 으로 우측만 → 보행 비대칭 체감
12. **변형 5 — 추가 강성 프리셋**: `NUM_STIFFNESS_PRESETS` 3 → 5, 더 세밀한 단계

---

## 5️⃣ 다음 단계

- 중력 + 마찰 보상 (투명 모드): [Ex.21 Gravity Compensation](../21_Gravity_Compensation/)
- 어드미턴스 제어 (Impedance 역): [Ex.28 Admittance Control](../28_Admittance_Control/)
- DOB (외란 관측기): [Ex.31 Friction Comp DOB](../31_Friction_Comp_DOB/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 토크가 항상 0 | `XM_SetControlMode(XM_CTRL_TORQUE)` 미호출 | Active_Entry 에 추가 |
| 진동 (oscillation) | B 가 너무 작음 (Light) | B Medium 이상으로 |
| 다리가 평형점으로 너무 빠르게 끌림 | K 가 Stiff | K Soft 또는 Medium |
| `theta_dot` 노이즈 | 후향 차분 + 1 kHz 노이즈 | LPF (factor 0.1~0.2) 추가 또는 B ↓ |
| 평형점 이동 시 갑작스러운 토크 | θ_d 변경이 즉시 적용됨 | LPF 로 θ_d 부드럽게 전환 |
| K 크게 했더니 모터 떨림 | 게인 너무 큼 + 모터 마찰 | MAX_TORQUE_NM 확인 후 단계적 증가 |
| Body Data 미설정인데 동작 OK | 본 예제는 Body Data 불필요 (각도 직접 사용) | 정상 |
| BTN3 평형점이 안 바뀜 | `XM_BTN_CLICK` 이벤트 (LONG_PRESS 아님) 사용 | 짧게 클릭 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
