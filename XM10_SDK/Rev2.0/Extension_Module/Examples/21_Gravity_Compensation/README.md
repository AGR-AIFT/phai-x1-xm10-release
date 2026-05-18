# Ex.21 — Gravity Compensation (투명 모드 — "로봇이 사라진 듯한" 상태)

> 🎯 **학습 목표**:
> - **중력 보상** `τ_grav = M·g·L·sin(θ)` 로 로봇 무게 상쇄 → **투명 모드** 구현.
> - **Coulomb (정마찰) + Viscous (점성마찰)** 보상으로 백드라이버빌리티 (back-drivability) 향상.
> - **α 점진 활성화** (0 → 1, 11단계) 패턴 — 갑작스런 100% 보상 금지 (안전).
>
> ⏱️ 권장 시간: 45분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.15 Inverted Pendulum](../15_Inverted_Pendulum_Control/) (역진자 vs 투명 차이 핵심) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Just, F., et al. (2018). *Human arm weight compensation in rehabilitation robotics.* J. NeuroEng. Rehab., 15(28).

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 가 ASSIST 일 때 **로봇 자체 무게 + 모터 마찰** 을 상쇄 → 착용자가 외골격이 거의 없는 듯한 자유로움 체감.

| α 비율 | 보상 정도 | 체감 |
|-------|---------|------|
| 0.0 | 보상 OFF | 외골격 + 마찰감 정상적 |
| 0.5 | 50% (초기) | 무게 절반 사라짐 |
| 1.0 | 100% | 거의 투명 (이상적) |

USB CDC `GC | α:0.50 Mgl:17.2 τ_grav:1.23 τ_fric:0.15 τ_cmd:0.69` 매 500 ms.

> 📸 `![투명 모드 — α 점진 활성화](../assets/img/21_transparency.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 투명 모드 vs 역진자 안정화 (Ex.15) — 핵심 차이

| | Ex.15 (Inverted Pendulum) | Ex.21 (Transparency) |
|--|-------------------------|---------------------|
| **목적** | 특정 평형점으로 복원 | 모든 로봇 기인 힘 상쇄 |
| **위치 추종** | O (PD 안정화 있음) | X (복원력 없음) |
| **사용자 자유도** | 평형점 부근만 | 전 범위 자유롭게 |

### 중력 토크 모델 (간단 역진자 근사)

```
τ_gravity = M · g · L_eff · sin(θ)
- M     = 체중 (kg)
- g     = 9.81 m/s²
- L_eff = 고관절~대퇴부 CoM 거리 (~0.25 m)
- θ     = 수직 기준 고관절 각도 (rad)
```

수직 (θ=0°) 에서 중력 토크 = 0, 90° 에서 최대 = M·g·L_eff.

### 마찰 보상 (감속기 + 베어링)

```
τ_friction = B_coulomb · sign(θ̇) + B_viscous · θ̇
- Coulomb (정마찰): 속도 부호에 비례 (방향 무관)
- Viscous (점성): 속도 크기에 비례
- Deadzone (|θ̇| < 0.01 rad/s) → 마찰 보상 OFF (정지 시 진동 방지)
```

### α 점진 활성화 패턴

- 갑자기 α=1.0 적용 시 → 큰 토크 점프 → 착용자 비틀거림 위험
- BTN 1 으로 0.1 씩 11 단계 증가 (0.0, 0.1, …, 1.0)
- 50% 에서 시작 권장

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define G_ACC                9.81f
#define M_BODY_DEFAULT_KG    70.0f
#define L_EFF_M              0.25f
#define B_COULOMB_NM         0.3f
#define B_VISCOUS_NMS        0.01f
#define VELOCITY_DEADZONE_RADS  0.01f
#define ALPHA_INIT           0.5f
#define ALPHA_STEP           0.1f

static float s_alpha = ALPHA_INIT;                                  // ① 점진 활성화 비율
static float s_body_mass_kg = M_BODY_DEFAULT_KG;
static bool  s_friction_comp_on = true;

static void Active_Loop(void)
{
    float theta_deg = XM.status.h10.rightHipAngle;
    float theta_rad = DEG_TO_RAD(theta_deg);                        // ② deg → rad

    float theta_dot_rad = ...;  /* 후향 차분 + LPF */

    /* ③ 중력 보상 (역진자 근사) */
    float tau_gravity = s_body_mass_kg * G_ACC * L_EFF_M * sinf(theta_rad);

    /* ④ 마찰 보상 (deadzone 적용) */
    float tau_friction = 0.0f;
    if (s_friction_comp_on && fabsf(theta_dot_rad) > VELOCITY_DEADZONE_RADS) {
        float sign_dot = (theta_dot_rad > 0) ? 1.0f : -1.0f;
        tau_friction = B_COULOMB_NM * sign_dot
                     + B_VISCOUS_NMS * theta_dot_rad;
    }

    /* ⑤ α 점진 적용 + 포화 */
    float tau_raw = s_alpha * (tau_gravity + tau_friction);
    float tau_out = _ClampFloat(tau_raw, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    XM_SetAssistTorqueRH(tau_out);
    XM_SetAssistTorqueLH(tau_out);
}

static void _HandleButtons(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {              // ⑥ BTN1: α 단계 증가
        s_alpha += ALPHA_STEP;
        if (s_alpha > ALPHA_MAX + 0.001f) s_alpha = 0.0f;            //    0~1 래핑
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {              //    BTN2: 체중 프리셋
        s_mass_idx = (s_mass_idx + 1) % NUM_MASS_PRESETS;
        s_body_mass_kg = mass_presets[s_mass_idx];
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK)                //    BTN3: 마찰 ON/OFF
        s_friction_comp_on = !s_friction_comp_on;
}
```

전체 코드: [`gravity_compensation.c`](gravity_compensation.c) (632 줄)

> 🧒 ④ 의 **deadzone 가드** 가 핵심. 정지 시 마찰 보상 끄지 않으면 `sign(0)` 토글로 진동 발생.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시 + CM ASSIST** → ✅ ACTIVE 진입 시 LED 1 빠른 깜빡
2. **α=0.5 (기본) 상태** → ✅ 외골격 무게의 절반이 사라진 느낌
3. **다리를 천천히 움직임** → ✅ 매끄럽게 따라옴 (마찰 보상 효과)
4. **다리를 빠르게 흔듦** → ✅ Viscous 마찰 보상이 속도에 비례해 추가
5. **BTN 1** 5회 클릭 → ✅ α 0.5 → 0.6 → ... → 1.0. 100% 도달 시 완전 투명에 가까움
6. **BTN 1 한 번 더** → ✅ α 0.0 으로 래핑. 보상 OFF — 외골격 무게 다시 느껴짐
7. **BTN 3 클릭 (마찰 OFF)** → ✅ `τ_fric:0.00`. 빠른 움직임 시 모터 마찰 체감
8. **BTN 2 클릭 (체중 50→60→70→80→90)** → ✅ Mgl 비례 변화. 자기 체중 입력 후 비교
9. **변형 1 — α 자동 ramp**: User_Loop 에서 매 100 ms 마다 α 0.001 씩 증가 (10초 만에 1.0)
10. **변형 2 — L_eff 측정**: `L_EFF_M` 0.25 → 본인 고관절~대퇴 중심 측정값
11. **변형 3 — Coulomb 끄고 Viscous 만**: `B_COULOMB_NM` 0 → 점성 효과 분리 관찰
12. **변형 4 — sin(θ) → θ 근사**: 작은 각 (|θ|<15°) 에서 오차 비교
13. **변형 5 — DOB 결합**: 잔류 외란 추정 → [Ex.31 DOB](../31_Friction_Comp_DOB/)

---

## 5️⃣ 다음 단계

- 외란 관측기 (DOB) — 잔류 마찰까지 제거: [Ex.31 Friction Comp DOB](../31_Friction_Comp_DOB/)
- 보행 위상 적응 보조: [Ex.23 Gait Phase Adaptive Torque](../23_Gait_Phase_Adaptive_Torque/)
- MultiLayer Transparent: [Ex.35 MultiLayer Transparent Control](../35_MultiLayer_Transparent_Control/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| α=1.0 인데 외골격 무게가 여전히 느껴짐 | `L_EFF_M` 또는 `M_BODY_KG` 부정확 | 실측값으로 보정 |
| 정지 시 모터 진동 | `sign(θ̇)` deadzone 없음 → 부호 토글 | `VELOCITY_DEADZONE_RADS` 가드 확인 |
| 갑자기 큰 토크로 비틀거림 | α 를 한 번에 0.5 → 1.0 점프 | BTN1 단계적 증가 (0.1 씩) |
| `θ=0°` 에서도 토크 약간 있음 | sin(0)=0 인데 마찰 보상이 미세 속도에 반응 | 정상 (deadzone 이내면 0) |
| Body Data 미설정인데 동작? | 본 예제는 Body Data 불필요 (체중은 `M_BODY_KG` 매크로) | 정상 |
| BTN 2 체중 변경 후 즉시 큰 점프 | M 변경 즉시 적용됨 | LPF 로 M 부드럽게 전환 또는 α 낮춘 후 변경 |
| 빠른 움직임 시 Viscous 가 너무 강함 | `B_VISCOUS_NMS` 0.01 → 0.005 | 점성 계수 ↓ |
| 마찰 OFF (BTN3) 했더니 끊김 | 토크 신호 노이즈 노출 | LPF 또는 단계적 OFF |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
