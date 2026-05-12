# Ex.27 — MRAC (Model Reference Adaptive Control — MIT Rule 온라인 게인 적응)

> 🎯 **학습 목표**:
> - **참조 모델** (1차 LPF) 정의 + **MIT Rule** 로 게인 θ̂₁, θ̂₂ 온라인 적응.
> - 고정 게인 PD 의 한계 (체중·외란 변화 시 재튜닝 필요) vs MRAC 자동 적응 비교.
> - **파라미터 드리프트 방지** — Projection (범위 제한) 의 필요성.
>
> ⏱️ 권장 시간: 50분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.14 PD](../14_PD_Realtime_Control/) (고정 게인 vs 적응) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Slotine, J.-J. E., & Li, W. (1991). *Applied Nonlinear Control*, Ch.8 (MRAC 교과서). Sharifi, M. (2014). *Nonlinear MRAC for HRI.* Control Eng. Pract., 32.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

참조 모델 (이상적 응답) 을 정의하면 MRAC 가 자동으로 게인을 조정해 추종:

```
참조 모델  : x_m[k+1] = 0.99·x_m + 0.01·r       (1차 LPF, ~100 ms 응답)
적응 입력  : τ = θ̂₁·x + θ̂₂·r                  (선형 파라미터화)
적응 법칙  : θ̂₁ += γ·e·x,   θ̂₂ += γ·e·r       (MIT Rule)
```

| BTN | 동작 |
|-----|------|
| 1 | 참조 r (5° / 10° / 15° / 20° 순환) |
| 2 | 적응 속도 γ (0.001 / 0.005 / 0.01 / 0.05) |
| 3 | θ̂ 리셋 (θ̂₁=0, θ̂₂=0.5) — 처음부터 적응 재시작 |

USB CDC `MRAC | r:10.0 xm:8.5 θ:7.8 e:0.7 θ̂1:0.15 θ̂2:0.62` 매 500 ms.

> 📸 `![게인 θ̂₁/θ̂₂ 적응 궤적](../assets/img/27_mrac_evolution.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 고정 PD vs MRAC

| | PD (Ex.14) | MRAC (이 예제) |
|--|----------|--------------|
| 게인 | 고정 (Kp, Kd) | 온라인 적응 (θ̂₁, θ̂₂) |
| 체중 변화 대응 | 재튜닝 필요 | 자동 적응 |
| 안정성 보장 | 쉬움 | Lyapunov 분석 필요 |
| 본 예제 안정화 장치 | — | Projection (범위 제한) |

### MIT Rule (Gradient Descent)

```
J(θ̂) = 0.5 · e²                              (cost function)
θ̂[k+1] = θ̂[k] − γ · ∂J/∂θ̂
       = θ̂[k] + γ · e · ∂x/∂θ̂              (chain rule)

→ θ̂₁ 의 영향: ∂x/∂θ̂₁ ≈ x   →  θ̂₁ += γ · e · x
→ θ̂₂ 의 영향: ∂x/∂θ̂₂ ≈ r   →  θ̂₂ += γ · e · r
```

### 파라미터 드리프트 (Projection)

- MIT Rule 은 **Lyapunov 안정성 X** — 외란 시 θ̂ 가 무한대로 발산 가능
- 해결: 모든 step 마다 θ̂ 를 [min, max] 범위로 강제 클램프
  - θ̂₁ ∈ [−1.0, +1.0]
  - θ̂₂ ∈ [0.0, +2.0] (입력 게인 양수 유지)

### 1차 참조 모델 의미

```
x_m[k+1] = 0.99 · x_m[k] + 0.01 · r
```
- 극점 0.99 → 시정수 ≈ 100 ms @ 1 kHz
- DC 게인 = 0.01 / (1 − 0.99) = 1 (steady-state x_m = r)

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define AM_REF      0.99f                                           // 참조 모델 극점
#define BM_REF      0.01f                                           // DC unity gain
#define THETA1_MIN -1.0f
#define THETA1_MAX  1.0f
#define THETA2_MIN  0.0f
#define THETA2_MAX  2.0f

static float s_x_m  = 0.0f;                                         // ① 참조 모델 상태
static float s_theta1 = 0.0f;                                       //    상태 피드백 게인 (적응)
static float s_theta2 = 0.5f;                                       //    입력 게인 (적응)
static float s_r_ref  = 10.0f;                                      //    목표 (BTN1)
static float s_gamma  = 0.005f;                                     //    적응 속도

static void Active_Loop(void)
{
    float theta = XM.status.h10.rightHipAngle;                      // ② 실제 각도 x

    /* ③ 참조 모델 업데이트 (1차 LPF) */
    s_x_m = AM_REF * s_x_m + BM_REF * s_r_ref;

    /* ④ 추종 오차 */
    float e = s_x_m - theta;

    /* ⑤ MIT Rule 적응 (gradient descent) */
    s_theta1 += s_gamma * e * theta;
    s_theta2 += s_gamma * e * s_r_ref;

    /* ⑥ Projection (드리프트 방지) */
    s_theta1 = _ClampFloat(s_theta1, THETA1_MIN, THETA1_MAX);
    s_theta2 = _ClampFloat(s_theta2, THETA2_MIN, THETA2_MAX);

    /* ⑦ 적응 제어 입력 */
    float u = s_theta1 * theta + s_theta2 * s_r_ref;
    float tau = _ClampFloat(u, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    XM_SetAssistTorqueRH(tau);
    XM_SetAssistTorqueLH(tau);
}

static void _HandleButtons(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {              /* BTN1: r 순환 */
        s_r_ref += R_REF_STEP;
        if (s_r_ref > R_REF_MAX) s_r_ref = R_REF_MIN;
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {              /* BTN2: γ 순환 */
        gamma_idx = (gamma_idx + 1) % 4;
        s_gamma = gamma_presets[gamma_idx];
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {              /* BTN3: 리셋 */
        s_theta1 = 0.0f; s_theta2 = 0.5f; s_x_m = 0.0f;
    }
}
```

전체 코드: [`mrac_adaptive_control.c`](mrac_adaptive_control.c) (374 줄)

> 🧒 ⑥ 의 **Projection 가드** 가 핵심. 없으면 외란 시 θ̂ 무한 발산. MIT Rule 은 안정성 보장 X 라는 점 명심.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드/플래시 + ASSIST** (Body Data 불필요) → ✅ θ̂₁=0, θ̂₂=0.5 초기 상태
2. **BTN 1 클릭** → ✅ r=10° (기본). USB `MRAC | r:10.0 ...`
3. **수렴 관찰 (5~10 초)** → ✅ θ_actual 이 x_m 추종 시작, θ̂ 값 점진 변화
4. **30 초 후** → ✅ 정상 상태, θ ≈ x_m, e ≈ 0
5. **BTN 1 두 번 더** → ✅ r 15° → 20° → 5° 순환. 새 목표에 다시 적응
6. **BTN 2 클릭** → ✅ γ 0.001 (매우 느림) → 수렴 느려짐
7. **BTN 2 → 0.05** → ✅ 빠른 적응. 가능하면 진동 관찰
8. **BTN 3 클릭 (리셋)** → ✅ θ̂ 초기값 복귀. 다시 적응 시작
9. **변형 1 — γ = 0.1+ (불안정 한계)**: 발산 직접 체험 (Projection 이 막아줌)
10. **변형 2 — Projection 끄기**: `_ClampFloat` 호출 제거 → θ̂ 점점 발산
11. **변형 3 — 참조 모델 시정수**: `AM_REF` 0.99 → 0.95 (빠른 응답 50 ms) vs 0.999 (느림 1초)
12. **변형 4 — PD 와 비교**: [Ex.14](../14_PD_Realtime_Control/) 와 동일 r 로 응답 비교
13. **변형 5 — 외란 추가**: User_Loop 에 인위적 `theta += sinf(t * 0.01f)` → MRAC 대응 관찰

---

## 5️⃣ 다음 단계

- ILC (주기 반복 학습): [Ex.26 ILC](../26_Iterative_Learning_Control/)
- 어드미턴스 (힘→위치 적응): [Ex.28 Admittance Control](../28_Admittance_Control/)
- FF+FB 혼합 (모델 + 적응): [Ex.30 FF+FB Hybrid](../30_FF_FB_Hybrid_Control/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| θ̂ 무한 발산 | Projection 누락 | `_ClampFloat(θ̂, MIN, MAX)` 매 cycle |
| 수렴 매우 느림 | γ = 0.001 | γ 0.005 → 0.01 |
| 진동 (oscillation) | γ 너무 큼 (0.05+) | γ ↓ 또는 참조 모델 시정수 ↑ |
| θ 가 x_m 안 따라감 | θ̂₂ 가 [0, 2] 범위에 묶임 | 부호 확인 + 범위 확장 |
| 정상 상태 오차 큼 | DC 게인 ≠ 1 (`a_m + b_m ≠ 1`) | 0.99 + 0.01 = 1.00 확인 |
| BTN 3 후에도 동일 거동 | s_x_m 만 0 으로 리셋, θ̂ 유지 | θ̂₁=0, θ̂₂=0.5 동시 리셋 |
| 부정확한 모델로 발산 | MIT Rule 은 Lyapunov 보장 X | Projection 의존 — 강건성 한계 인지 |
| 빠른 r 변경 시 transient | LPF 응답 100 ms — 정상 | r 변경 후 0.5 초 기다림 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
