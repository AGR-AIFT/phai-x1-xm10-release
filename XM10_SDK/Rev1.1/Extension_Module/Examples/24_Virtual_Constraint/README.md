# Ex.24 — Virtual Constraint (HZD — Bézier 다항식으로 보행 궤도 정의)

> 🎯 **학습 목표**:
> - **HZD (Hybrid Zero Dynamics)** 핵심 개념 — 시간이 아닌 **보행 위상 s** 를 독립 변수로.
> - **5차 Bézier 다항식** `θ_d(s) = Σ C(5,k)·s^k·(1−s)^(5−k)·α_k` 로 목표 궤도 정의.
> - PD 추종 (`τ = Kp·(θ_d − θ) + Kd·(θ̇_d − θ̇)`) → 위상 기반 궤도 추종.
>
> ⏱️ 권장 시간: 55분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/) (정현파 vs Bézier 비교) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Westervelt, E. R., et al. (2003). *Hybrid zero dynamics of planar biped walkers.* IEEE TAC, 48(1), 42–56.

---

## ⚠️ Body Data 전제조건 — 필수

`s = gaitCycle / 100` 이 Bézier 입력이므로 **gaitCycle 정확도가 모든 것**.
`User_Setup()` 에 반드시 `XM_SendUserBodyData(bodyData)` 호출.

> [examples/README.md — Body Data 안내](../README.md#part-5)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 ASSIST 모드에서 보행 위상 s 에 따라 **목표 각도 θ_d(s)** 가 정의된 궤도를 그리며 변화 → PD 추종.

| 프로파일 | α 제어점 (6개 deg) | 보행 특성 |
|---------|------------------|-----------|
| **Natural** (default) | 자연스러운 보행 패턴 | 일반 보행 |
| **Fast** | 빠른 swing 위상 | 빠른 보행 |
| **Minimal** | 작은 진폭 | 최소 보조 |

USB CDC `VC | s:0.45 θd:8.2 θ:7.5 τ:1.40` + PhAI 0xF0 5ch.

> 📸 `![Bézier 곡선 + PD 추종](../assets/img/24_bezier_tracking.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### HZD (Hybrid Zero Dynamics) — Westervelt 2003 핵심 통찰

- 보행은 **연속 동역학 + 이산 임팩트 (heel strike)** 의 hybrid 시스템
- 가상 구속 (Virtual Constraint) 을 적용하면 → 저차원 zero dynamics 로 환원
- 안정성 분석이 가능한 형태로 → 이론적으로 검증된 보행 패턴 합성

### 시간 기반 PD vs 위상 기반 VC

| | PD (Ex.14) | VC (이 예제) |
|--|----------|------------|
| 독립 변수 | t | s = gaitCycle/100 |
| 보행 속도 변화 | 적응 불가 | **자동 적응** |
| 정지 시 | 잘못된 위치로 끌림 | s 고정 |

### 5차 Bézier 다항식

```
θ_d(s) = Σ_{k=0}^{5} C(5,k) · s^k · (1−s)^(5−k) · α_k
C(5,k) = {1, 5, 10, 10, 5, 1}    (이항 계수)
α_k    = 제어점 6개 (deg)
```

**특성**:
- s=0 에서 α_0, s=1 에서 α_5 (양 끝점 보간)
- 중간 α_1~α_4 가 곡선 모양 결정
- C^∞ 부드러움 (모든 도함수 연속)

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define VC_BEZIER_ORDER  5
#define VC_KP            1.0f
#define VC_KD            0.03f

/* ① Bézier 제어점 (Natural 프로파일, 6 개, deg) */
static const float VC_BEZIER_NATURAL[6] = { 20.0f, 25.0f, 10.0f, -15.0f, -20.0f, 15.0f };
static const float VC_BEZIER_FAST[6]    = { 25.0f, 30.0f, 15.0f, -10.0f, -15.0f, 20.0f };

static const float *s_alpha = VC_BEZIER_NATURAL;                    // 활성 프로파일
static const uint16_t C5[6] = { 1, 5, 10, 10, 5, 1 };               // 이항계수

static float _Bezier5(float s)                                       // ② θ_d(s) 계산
{
    float result = 0.0f;
    for (int k = 0; k < 6; k++) {
        float term = (float)C5[k] * powf(s, k) * powf(1.0f - s, 5 - k);
        result += term * s_alpha[k];
    }
    return result;
}

static float _BezierDot(float s)                                     // ③ θ̇_d(s) 도함수
{
    /* d/ds 의 해석적 형태 — Bézier 미분도 4차 Bézier */
    float result = 0.0f;
    for (int k = 0; k < 5; k++) {
        float term = 5.0f * (s_alpha[k+1] - s_alpha[k]);
        float bk = (float)C4[k] * powf(s, k) * powf(1.0f - s, 4 - k);
        result += term * bk;
    }
    return result;
}

static void Active_Loop(void)
{
    /* ④ 위상 s 계산 (Body Data 필수) */
    uint8_t gc = _EstimateGaitCycle();
    float s = (float)gc / 100.0f;

    /* ⑤ 목표 궤도 + 도함수 */
    float theta_d     = _Bezier5(s);
    float theta_d_dot = _BezierDot(s) * s_phase_velocity;   /* s_dot ≈ 1/stride */

    /* ⑥ 현재 상태 + PD 추종 */
    float theta     = XM.status.h10.rightHipAngle;
    float theta_dot = (theta - s_prev_theta) / CONTROL_DT;
    s_prev_theta = theta;

    float tau = VC_KP * (theta_d - theta) + VC_KD * (theta_d_dot - theta_dot);
    tau *= s_strength_scale;                                          /* 보조 강도 0~1 */

    XM_SetAssistTorqueRH(_ClampFloat(tau, -MAX_TORQUE, MAX_TORQUE));
    XM_SetAssistTorqueLH(_ClampFloat(tau, -MAX_TORQUE, MAX_TORQUE));
}
```

전체 코드: [`virtual_constraint.c`](virtual_constraint.c) (469 줄)

> 🧒 ② 의 Bézier 합산이 핵심. 6 개 제어점만으로 매끄러운 보행 궤도 표현 — 비전공자가 GUI 로 튜닝 가능한 형태.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **Body Data 설정** + 빌드/플래시 + ASSIST
2. **정지 상태** → ✅ s=0 고정, 토크 0
3. **보행 시작** → ✅ s 0~1 순환, θ_d 가 Bézier 곡선 그림
4. **PhAI 0xF0 그래프** → ✅ θ_d 와 θ 가 비슷한 모양 (PD 가 추종)
5. **BTN 1 클릭** → ✅ Kp 0.2 → 0.5 → 1.0 → 2.0 순환. 추종 강성 증가
6. **BTN 2 클릭** → ✅ Kd 0.01 → 0.03 → 0.05. 댐핑 변화
7. **BTN 3 클릭** → ✅ 프로파일 Natural / Fast / Minimal 순환
8. **USB CDC** → `VC | s:0.45 θd:8.2 θ:7.5 τ:1.40` 매 200 ms
9. **변형 1 — 제어점 직접 수정**: `VC_BEZIER_NATURAL[2]` 10 → 20 (중간 신전 증가). 보행 궤도 변화 관찰
10. **변형 2 — 7 차 Bézier 로 확장**: 제어점 8 개 + C(7,k) = {1,7,21,35,35,21,7,1}
11. **변형 3 — Kp 너무 크게**: 5.0 → 자연 보행 방해 (강제 끌림). 안전 한계 체험
12. **변형 4 — Bézier 도함수 무시 (Kd 항 = 0)**: 추종 노이즈 비교
13. **변형 5 — 좌·우 다른 프로파일**: LH = Natural, RH = Fast → 비대칭 보행

---

## 5️⃣ 다음 단계

- 위상 기반 정현파 (간단 버전): [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/)
- 입각/유각 가변 강성: [Ex.25 Stance Stiffness](../25_Stance_Stiffness_Modulation/)
- 반복 학습 제어 (Bézier 자동 조정): [Ex.26 ILC](../26_Iterative_Learning_Control/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| s 가 항상 0 | Body Data 미설정 → gaitCycle 0 | `XM_SendUserBodyData` 호출 필수 |
| θ_d 가 보행 패턴과 안 맞음 | Bézier 제어점이 부적절 | Natural 프로파일을 기준으로 시작 |
| 토크 진동 | Kd 가 너무 작음 또는 θ_dot 노이즈 | LPF 추가 또는 Kd ↑ |
| 강제 끌림 (자연 보행 방해) | Kp 너무 큼 | Kp ≤ 1.0 권장, 강도 스케일 0.25 시작 |
| `powf` 가 느림 | 매 cycle 6번 호출 | LUT (lookup table) 또는 de Casteljau 알고리즘 |
| Bézier 도함수 계산 오류 | C4 = {1,4,6,4,1} 사용 누락 | `_BezierDot` 가 4차 Bézier 임을 명심 |
| 정지 → 보행 전환 시 토크 점프 | s 0 → 작은 양수 점프 → θ_d 점프 | s 변화량에 LPF 적용 |
| 좌·우 동일 토크 | LH/RH 분리 안 됨 (의도된 경우 무시) | LH = Bézier(s+0.5) 위상차 적용 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
