# Ex.29 — Bilateral Coordination (좌·우 협응 — 역위상 대칭 커플링)

> 🎯 **학습 목표**:
> - **역위상 대칭 커플링** — 정상 보행에서 `θ_R ≈ −θ_L` 관계 유도.
> - **비대칭 감지** (`|θ_R + θ_L| > 5°`) → LED 경고 + 편마비 재활 모니터링 활용.
> - **약측 강화 모드** — 한쪽 게인을 더 크게 (`K_strong`) → 약측을 건측 패턴으로 유도.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.20 Impedance](../20_Impedance_Control/) (커플링도 가상 스프링) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Duschau-Wicke, A., et al. (2010). *Path control for patient-cooperative robot-aided gait rehab.* IEEE TNSRE, 18(1).

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

좌·우 다리를 가상 스프링으로 연결 → 한쪽이 신전 (+) 일 때 다른 쪽이 굴곡 (−) 으로 자동 유도:

| 모드 | 동작 |
|------|------|
| **SYMMETRIC** | 좌·우 동일 게인 → 대칭 강조 |
| **ASSIST_RIGHT** | 우측 게인 ↑ (약측 강화) → 좌측은 표준, 우측을 좌측 패턴으로 끌어옴 |
| **ASSIST_LEFT** | 좌측 게인 ↑ |

LED 3 ON → 비대칭 경고 (`|θ_R + θ_L| > 5°`).

USB CDC `BILAT | ΔθR:2.5 K_c:0.3 τR:-0.75 τL:0.75 [ASYM]` 매 500 ms.

> 📸 `![좌·우 역위상 vs 비대칭 비교](../assets/img/29_bilateral_phase.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 정상 보행의 좌·우 위상 관계

- 보행 1주기 동안 좌·우 다리가 **반대 위상** (약 50% 차이)
- 우측 신전 (+10°) → 좌측 굴곡 (−10°) → 합 ≈ 0
- 정상: `θ_R + θ_L ≈ 0` (역위상 균형)

### 비대칭 오차 정의

```
Δθ_R = θ_R − (−θ_L) = θ_R + θ_L         (대칭 기준 편차)
Δθ_L = θ_L + θ_R                          (좌측에서 본 동일 값)
```

### 커플링 토크 (가상 스프링-댐퍼)

```
τ_R = −K_c · Δθ_R − B_c · (θ̇_R + θ̇_L)
τ_L = +K_c · Δθ_R + B_c · (θ̇_R + θ̇_L)
```

부호가 반대 → 두 다리를 **서로 반대 방향으로 밀어냄** → 역위상으로 유도.

### 편마비 재활에서의 응용 (Duschau-Wicke 2010)

- 뇌졸중 환자의 약측 (약 30%) ↔ 건측 (정상)
- 약측 K_c ↑ → 건측 패턴으로 강제 유도 → 신경 가소성 자극

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define K_COUPLING_DEFAULT  0.3f
#define B_COUPLING_DEFAULT  0.01f
#define K_COUPLING_STRONG   0.5f                                    // ① 약측 강화
#define ASYMMETRY_THRESHOLD 5.0f

typedef enum { COORD_SYMMETRIC, COORD_ASSIST_R, COORD_ASSIST_L } CoordMode_t;

static CoordMode_t s_mode = COORD_SYMMETRIC;
static float s_K_c = K_COUPLING_DEFAULT;
static float s_B_c = B_COUPLING_DEFAULT;

static void Active_Loop(void)
{
    /* ② 좌·우 각도 + 속도 */
    float theta_r = XM.status.h10.rightHipAngle;
    float theta_l = XM.status.h10.leftHipAngle;
    float theta_r_dot = (theta_r - s_prev_r) / CONTROL_DT;
    float theta_l_dot = (theta_l - s_prev_l) / CONTROL_DT;
    s_prev_r = theta_r; s_prev_l = theta_l;

    /* ③ 대칭 오차 (역위상 기준) */
    float delta_theta = theta_r + theta_l;                          /* 0 이면 완벽 대칭 */
    float delta_vel   = theta_r_dot + theta_l_dot;

    /* ④ 모드별 게인 */
    float k_r = s_K_c, k_l = s_K_c;
    if (s_mode == COORD_ASSIST_R) k_r = K_COUPLING_STRONG;          /* 우측 강화 */
    if (s_mode == COORD_ASSIST_L) k_l = K_COUPLING_STRONG;

    /* ⑤ 커플링 토크 (반대 부호) */
    float tau_r = -k_r * delta_theta - s_B_c * delta_vel;
    float tau_l = +k_l * delta_theta + s_B_c * delta_vel;

    XM_SetAssistTorqueRH(_ClampFloat(tau_r, -MAX_TORQUE_NM, MAX_TORQUE_NM));
    XM_SetAssistTorqueLH(_ClampFloat(tau_l, -MAX_TORQUE_NM, MAX_TORQUE_NM));

    /* ⑥ 비대칭 LED 경고 */
    if (fabsf(delta_theta) > ASYMMETRY_THRESHOLD)
        XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200);
    else
        XM_SetLedState(XM_LED_3, XM_OFF);
}

static void _HandleButtons(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {              /* BTN1: K_c 순환 */
        kc_idx = (kc_idx + 1) % 4;
        s_K_c = kc_presets[kc_idx]; /* 0.1, 0.2, 0.3, 0.5 */
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {              /* BTN2: B_c */
        bc_idx = (bc_idx + 1) % 3;
        s_B_c = bc_presets[bc_idx];
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK)                /* BTN3: 모드 순환 */
        s_mode = (s_mode + 1) % 3;
}
```

전체 코드: [`bilateral_coordination.c`](bilateral_coordination.c) (370 줄)

> 🧒 ⑤ 의 **부호 반대 토크** 가 핵심. 같은 부호로 했다면 동위상으로 끌려 자연스럽지 않음.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드/플래시 + ASSIST** + 보행 시작 → ✅ 좌·우 자연스러운 역위상
2. **`θ_R + θ_L`** USB 모니터 → 평균 0, 변동 ±2° 이내 (정상 대칭)
3. **한쪽 다리 일부러 멈춤** → ✅ `|θ_R + θ_L| > 5°` → LED 3 깜빡
4. **BTN 1** 3회 클릭 → ✅ K_c 0.1 → 0.2 → 0.3 → 0.5. 커플링 강도 변화
5. **BTN 3 클릭** → ✅ 모드 ASSIST_RIGHT. USB 표시 변경
6. **ASSIST_RIGHT 모드 + 우측 정지 시도** → ✅ K_strong 으로 더 강한 복원
7. **BTN 3 두 번** → ✅ ASSIST_LEFT 로 전환
8. **변형 1 — K_c = 0**: 커플링 OFF → 좌·우 완전 독립 (자연 보행 그대로)
9. **변형 2 — K_c = 1.0+ (강제 대칭)**: 자발적 움직임 제한. 학습 효과 있지만 불편
10. **변형 3 — 비대칭 임계값**: 5° → 3° (LED 자주 켜짐) vs 10° (관대)
11. **변형 4 — Body Data 설정 후 footContact 활용**: gaitCycle 도 함께 모니터링
12. **변형 5 — 좌·우 LPF 시정수**: 즉시 반응 vs 천천히 적용 비교

---

## 5️⃣ 다음 단계

- 모방 학습 (한쪽 → 다른쪽): [Ex.33 Kinesthetic Teaching](../33_Kinesthetic_Teaching/)
- Mirror Therapy 확장: [Ex.36 OnDevice Kinesthetic Learning](../36_OnDevice_Kinesthetic_Learning/)
- ILC 반복 학습 (비대칭 보정): [Ex.26 ILC](../26_Iterative_Learning_Control/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 좌·우 동위상으로 끌림 | 토크 부호 동일 (둘 다 −K·Δθ) | 한쪽 +K, 한쪽 −K 부호 반대 |
| LED 3 항상 OFF | 임계값 너무 큼 또는 보행 매우 균형 잡힘 | 임계값 3° 로 ↓ |
| 한쪽 다리 부담 | ASSIST 모드인데 약측 게인 부정확 | 약측 = K_strong, 건측 = K |
| 강제됨 (자발 보행 X) | K_c 너무 큼 (1.0+) | K_c 0.3~0.5 권장 |
| 정지 시에도 토크 출력 | velocity 미사용 케이스에서 가짜 진동 | B_c ↑ 또는 정지 판정 추가 |
| Body Data 없이 LED 3 부정확 | gaitCycle 부정확 → 비대칭 판정 노이즈 | Body Data 설정 또는 임계값 ↑ |
| Δθ 계산이 (θ_R − θ_L) | 정상 보행에서 큰 값 → 항상 비대칭으로 오판 | `Δθ = θ_R + θ_L` (역위상 기준) |
| 좌·우 게인 비대칭이 효과 미미 | K_strong 과 K 차이가 작음 (0.4 vs 0.5) | K_strong ≥ 2 × K 권장 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
