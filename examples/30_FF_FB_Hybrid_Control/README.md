# Ex.30 — FF+FB Hybrid Control (피드포워드 + 피드백 — 두 접근의 상호보완)

> 🎯 **학습 목표**:
> - **FF (모델 기반) + FB (PD)** 의 역할 분담 — FF 가 예측 가능 동역학, FB 가 불확실성/외란.
> - **Computed Torque** 방식 (Slotine & Li 1991, Ch.6) — 정상 상태 오차 제거.
> - **BTN 3 토글로 직접 비교** — 동일 Kp 에서 FF 있을 때 vs 없을 때의 추종 성능 차이 체감.
>
> ⏱️ 권장 시간: 45분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.14 PD](../14_PD_Realtime_Control/) (FB only) + [Ex.21 Gravity Comp](../21_Gravity_Compensation/) (FF only) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Slotine, J.-J. E., & Li, W. (1991). *Applied Nonlinear Control*, Ch.6.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

정현파 참조 궤도 (`A·sin(2π·f·t)`, A=10°, f=0.5 Hz) 를 추종 — 두 모드 비교:

| 모드 (BTN 3 로 토글) | 토크 구성 | 추종 성능 |
|--------------------|---------|---------|
| **FF + FB** (default) | `τ = Mgl·cos(θ) + B·θ̇ + Kp·e + Kd·de/dt` | 정상 상태 오차 ≈ 0 |
| **FB only** (BTN3 OFF) | `τ = Kp·e + Kd·de/dt` | 중력 영향으로 오차 큼 |

USB CDC `FF_FB | θd:8.5 θ:8.1 τff:1.2 τfb:0.4 τ:1.6` 매 500 ms.

> 📸 `![FF ON vs OFF 추종 비교](../assets/img/30_ff_fb_comparison.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### FF vs FB 각각의 한계

| | Pure FB (Ex.14 PD) | Pure FF (Ex.21 Gravity) |
|--|------------------|------------------------|
| 강점 | 외란 + 불확실성 강건 | 정상 상태 오차 0, 예측적 |
| 약점 | 중력 → SS 오차, 큰 Kp 필요 | 모델 부정확 → 실패 |

### FF+FB 혼합의 시너지

```
τ_total = τ_ff + τ_fb

τ_ff: 알고 있는 동역학 (중력 + 마찰) 사전 보상
τ_fb: 모델 오차 + 외란 보정 (작은 Kp 로 충분)
```

→ 작은 Kp 로 부드러운 보조 + 정상 상태 오차 0

### 피드포워드 — 역동역학 근사 (저속 가정)

```
τ_ff ≈ M · g · L_eff · cos(θ)   +   B_f · θ̇
      └─── 중력 모멘트 ──┘       └─ 점성 마찰 ─┘
```

**주의**: `sin` 이 아닌 `cos` — 고관절 좌표계가 수평을 0° 기준 (Ex.21 의 sin 과 좌표계 다름).

### 참조 궤도

```
θ_d(t) = A · sin(2π · f · t),    A=10°, f=0.5 Hz
θ̇_d(t) = A · 2π·f · cos(2π · f · t)
```

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define M_BODY_KG   70.0f
#define L_EFF_M     0.25f
#define G_ACC       9.81f
#define MGL_EFF     (M_BODY_KG * G_ACC * L_EFF_M)                  // 사전 계산 = 171.7
#define B_FRICTION  0.3f

#define KP_FB_DEFAULT 0.4f
#define KD_FB         0.015f
#define A_REF         10.0f
#define FREQ_HZ       0.5f                                          // 1 초 주기 정현파

static bool s_ff_enabled = true;                                    // ① BTN 3 으로 토글
static float s_Kp = KP_FB_DEFAULT;
static float s_A_ref = A_REF;

static void Active_Loop(void)
{
    float t = (float)XM_GetTick() / 1000.0f;                        /* 초 단위 */

    /* ② 참조 궤도 (정현파) */
    float theta_d     = s_A_ref * sinf(2.0f * M_PI * FREQ_HZ * t);
    float theta_d_dot = s_A_ref * 2.0f * M_PI * FREQ_HZ
                      * cosf(2.0f * M_PI * FREQ_HZ * t);

    /* ③ 현재 상태 */
    float theta     = XM.status.h10.rightHipAngle;
    float theta_dot = (theta - s_prev_theta) / CONTROL_DT;
    s_prev_theta = theta;

    /* ④ 피드포워드 (FF) — 모델 기반 사전 보상 */
    float tau_ff = 0.0f;
    if (s_ff_enabled) {
        tau_ff = MGL_EFF * cosf(DEG_TO_RAD(theta))                 /* 중력 모멘트 */
               + B_FRICTION * theta_dot;                            /* 점성 마찰 */
    }

    /* ⑤ 피드백 (FB) — PD 추종 */
    float e      = theta_d - theta;
    float e_dot  = theta_d_dot - theta_dot;
    float tau_fb = s_Kp * e + KD_FB * e_dot;

    /* ⑥ 혼합 출력 */
    float tau = _ClampFloat(tau_ff + tau_fb, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    XM_SetAssistTorqueRH(tau);
    XM_SetAssistTorqueLH(tau);
}

static void _HandleButtons(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {              /* BTN1: Kp 순환 */
        kp_idx = (kp_idx + 1) % 4;
        s_Kp = kp_presets[kp_idx]; /* 0.1, 0.2, 0.4, 0.6 */
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {              /* BTN2: A 진폭 순환 */
        a_idx = (a_idx + 1) % 4;
        s_A_ref = a_presets[a_idx]; /* 5, 10, 15, 20 */
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK)                /* BTN3: FF 토글 */
        s_ff_enabled = !s_ff_enabled;
}
```

전체 코드: [`ff_fb_hybrid_control.c`](ff_fb_hybrid_control.c) (397 줄)

> 🧒 ⑥ 의 단순한 `τ_ff + τ_fb` 합산이 핵심. 두 토크가 같은 방향이면 시너지, 반대 방향이면 FB 가 FF 모델 오차를 보상.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드/플래시 + ASSIST** → ✅ FF ON 기본
2. **0.5 Hz 정현파 추종 시작** → ✅ θ 가 θ_d 거의 일치
3. **USB CDC** → `FF_FB | θd:8.5 θ:8.3 τff:1.0 τfb:0.2 τ:1.2` — τ_fb 작음 (FF 가 대부분)
4. **BTN 3 클릭 (FF OFF)** → ✅ 동일 Kp 에서 추종 오차 크게 증가 (~3~5°)
5. **BTN 3 다시 (FF ON 복귀)** → ✅ 추종 오차 < 1° 로 복귀
6. **BTN 1** 3회 클릭 → ✅ Kp 0.1 → 0.2 → 0.4 → 0.6. 큰 Kp 에서 FF 없어도 비슷한 성능
7. **BTN 2** 3회 클릭 → ✅ 진폭 5° → 10° → 15° → 20°. 큰 진폭에서 FF 효과 더 커짐
8. **변형 1 — MGL_EFF 부정확**: `M_BODY_KG` 70 → 100. 과보상 효과 (튀어나옴)
9. **변형 2 — Kp = 0 (FF만)**: 매우 부드럽지만 외란 시 복원 X
10. **변형 3 — B_FRICTION = 0**: 마찰 보상 끄고 FF 효과 분리
11. **변형 4 — sin → 톱니파**: `theta_d = fmodf(t * f, 1.0f) * A * 2 - A;` 비교
12. **변형 5 — θ_d 부호 반전**: `−A·sin(...)` 으로 변경. FF/FB 양쪽 반응 비교

---

## 5️⃣ 다음 단계

- DOB (잔류 외란 추정): [Ex.31 Friction Comp DOB](../31_Friction_Comp_DOB/)
- MRAC (FF 게인 자동 적응): [Ex.27 MRAC](../27_MRAC/)
- Bilateral Coordination: [Ex.29 Bilateral](../29_Bilateral_Coordination/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| FF ON 했더니 토크 너무 큼 | MGL_EFF 가 실제보다 큼 (체중 과대) | `M_BODY_KG` 실측치로 |
| FF OFF 했는데 거의 차이 X | Kp 너무 커서 FB 가 다 보정 | Kp ↓ 로 비교 |
| sin 인데 결과가 cos 같음 | 고관절 좌표계 (수평=0°) → cos 가 맞음 | `cos` 사용 (Ex.21 의 sin 과 좌표계 다름) |
| 정현파 한 주기 후 빗나감 | `XM_GetTick` 정밀도 → 위상 누적 오차 | 매 시작 시 위상 기준점 재계산 |
| BTN 3 토글 시 즉시 점프 | FF 0 → MGL_EFF·cos 갑작스럼 | LPF 로 FF 부드럽게 ON/OFF |
| `theta_dot` 노이즈 큼 | 후향 차분 + 노이즈 | LPF + Kd ↓ |
| Body Data 미설정인데 동작 | 본 예제는 매크로 기반 (Body Data 불필요) | 정상 |
| FF 와 FB 가 서로 상쇄 | 부호 오류 (cos vs −cos) | 정상 보행 (다리 앞 흔들 때) τ_ff > 0 인지 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
