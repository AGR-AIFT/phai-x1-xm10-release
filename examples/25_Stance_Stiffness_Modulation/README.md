# Ex.25 — Stance Stiffness Modulation (입각/유각 가변 강성 — Collins 2015 Nature)

> 🎯 **학습 목표**:
> - **footContact 기반 위상 감지** — gaitCycle 보다 단순 + 신뢰성 높음.
> - 입각기 = **가상 스프링 강성**, 유각기 = **최소 감쇠** — 두 전략 동적 전환.
> - **LPF blending** (`α=0.05`, ~20 ms 시정수) 으로 불연속 토크 점프 방지.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.20 Impedance](../20_Impedance_Control/) (강성 개념) + [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Collins, S. H., et al. (2015). *Reducing the energy cost of human walking using an unpowered exoskeleton.* Nature, 522(7555), 212–215.

---

## ⚠️ Body Data 전제조건 — 필수

`isRightFootContact` / `isLeftFootContact` 는 H10 의 IMU + 역기구학 추정 결과.
**Body Data 미설정 시 footContact 항상 false → 항시 유각 모드 → 보조 효과 0**.

> [examples/README.md — Body Data 안내](../README.md#part-5)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

좌·우 **독립 FSM** 으로 발 접지 상태에 따라 강성 전략 전환:

| 상태 | 제어 법칙 | 보조 의미 |
|------|---------|---------|
| **입각기** (footContact=true) | τ = K·(θ_eq − θ) − B·θ̇ | 체중 지지 + 평형점 안정 |
| **유각기** (footContact=false) | τ = −B_swing·θ̇ | 자유 운동, 최소 감쇠만 |

좌·우 독립 → 한쪽이 입각기일 때 다른 쪽 유각기 (자연스러운 보행 위상차).

USB CDC `STSF | R:ST K:0.4 τR:1.2 L:SW τL:-0.05` 매 500 ms.

> 📸 `![입각/유각 강성 LPF 전환](../assets/img/25_stance_blending.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 왜 입각기에만 강한 강성을?

- 입각기 = 체중 수용 + 추진 → **무릎/고관절에 큰 부하**
- 유각기 = 다리 들어올림 → 자유로워야 함
- Collins 2015 (Nature): 입각기 탄성 에너지 저장/방출이 보행 대사 비용 감소의 핵심

### LPF Blending 패턴

```
K_active[k+1] = K_active[k] + α · (K_target − K_active[k])
α = 0.05 → 시정수 ≈ 20 ms
```
- 입각/유각 전환 시 K 가 즉시 점프하면 → 큰 토크 점프 → 착용자 비틀거림
- α 작을수록 부드럽지만 반응 느림 (대안: α 0.1 = 10 ms 시정수)

### 입각/유각 비대칭 (좌·우 독립 변수)

- `s_K_active_r`, `s_K_active_l` 분리 — 한쪽 다리만 입각이어도 다른 쪽 토크 무관
- 좌·우가 같은 변수 사용하면 → 양쪽이 동기로 변해 부자연스러움

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define K_STANCE_DEFAULT  0.4f     // 입각 강성 (Nm/deg)
#define B_STANCE_DEFAULT  0.015f
#define B_SWING_DEFAULT   0.005f
#define BLEND_ALPHA       0.05f    // ~20 ms 시정수 @ 1 kHz

static float s_K_target_r = K_STANCE_DEFAULT;
static float s_K_active_r = 0.0f;                                   // ① 좌·우 독립 변수
static float s_K_active_l = 0.0f;
static float s_eq_pos_deg = 0.0f;

static float _ApplyLegControl(bool foot_contact, float theta, float theta_dot,
                              float *p_K_active)
{
    /* ② 입각/유각 목표 강성 결정 */
    float K_target = foot_contact ? s_K_target_r : 0.0f;

    /* ③ LPF 블렌딩 (α = 0.05 → ~20 ms) */
    *p_K_active += BLEND_ALPHA * (K_target - *p_K_active);

    /* ④ 모드별 토크 계산 */
    float tau;
    if (foot_contact) {                                              /* 입각기 */
        tau = (*p_K_active) * (s_eq_pos_deg - theta)
            - B_STANCE_DEFAULT * theta_dot;
    } else {                                                         /* 유각기 */
        tau = -B_SWING_DEFAULT * theta_dot;
    }
    return _ClampFloat(tau, -MAX_TORQUE_NM, MAX_TORQUE_NM);
}

static void Active_Loop(void)
{
    /* ⑤ 우측 다리 */
    bool fc_r = XM.status.h10.isRightFootContact;
    float theta_r = XM.status.h10.rightHipAngle;
    float theta_r_dot = (theta_r - s_prev_theta_r) / CONTROL_DT;
    s_prev_theta_r = theta_r;
    float tau_r = _ApplyLegControl(fc_r, theta_r, theta_r_dot, &s_K_active_r);

    /* ⑥ 좌측 다리 (독립) */
    bool fc_l = XM.status.h10.isLeftFootContact;
    float theta_l = XM.status.h10.leftHipAngle;
    float theta_l_dot = (theta_l - s_prev_theta_l) / CONTROL_DT;
    s_prev_theta_l = theta_l;
    float tau_l = _ApplyLegControl(fc_l, theta_l, theta_l_dot, &s_K_active_l);

    XM_SetAssistTorqueRH(tau_r);
    XM_SetAssistTorqueLH(tau_l);

    /* ⑦ 시각 표시 — LED 2: 우측 입각, LED 3: 좌측 입각 */
    XM_SetLedState(XM_LED_2, fc_r ? XM_ON : XM_OFF);
    XM_SetLedState(XM_LED_3, fc_l ? XM_ON : XM_OFF);
}
```

전체 코드: [`stance_stiffness_modulation.c`](stance_stiffness_modulation.c) (387 줄)

> 🧒 ① 의 **좌·우 독립 변수** 가 핵심. 한 변수로 묶으면 양다리가 동시에 변화 → 부자연.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **Body Data 설정** + 빌드/플래시 + ASSIST
2. **정지 양발 접지** → ✅ LED 2 + LED 3 모두 ON. K_active 둘 다 K_target 으로 수렴
3. **보행 시작** → ✅ LED 2/3 가 보행 주기에 맞춰 교대로 깜빡
4. **USB CDC** → `STSF | R:ST K:0.40 τR:1.20 L:SW τL:-0.05` 매 500 ms
5. **BTN 1** 3회 클릭 → ✅ K_target 0.2 → 0.4 → 0.6 → 0.8 순환. 입각기 지지 강도 변화 체감
6. **BTN 2** 3회 클릭 → ✅ θ_eq −5° → 0° → +5° → +10° 순환. 평형점 이동
7. **BTN 3 클릭** → ✅ 입각 전용 모드 ↔ 항시 모드 전환
8. **변형 1 — LPF 시정수**: `BLEND_ALPHA` 0.05 → 0.1 (10 ms, 빠른 전환). 점프감 비교
9. **변형 2 — LPF 끄기 (α=1.0)**: 즉시 점프 → 토크 충격 체감 (안전 학습)
10. **변형 3 — 입각기 K 매우 크게 (1.5)**: 무릎 부담 감소 효과 (Collins 2015 핵심)
11. **변형 4 — 유각 감쇠 변경**: `B_SWING_DEFAULT` 0.005 → 0.02 → 0.05. 유각 무거워짐
12. **변형 5 — 좌·우 다른 K**: `K_target_r ≠ K_target_l` (편마비 보행 시뮬레이션)

---

## 5️⃣ 다음 단계

- 보행 위상 연속 보조: [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/)
- Hogan 임피던스 (강성 일반화): [Ex.20 Impedance Control](../20_Impedance_Control/)
- 보행 분석 데이터 로깅: [Ex.34 MSC GaitAnalysis Log](../34_MSC_GaitAnalysis_Log/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| LED 2/3 모두 OFF (보행 중에도) | Body Data 미설정 → footContact 항상 false | `XM_SendUserBodyData` 호출 |
| 좌·우 동기로 전환 (자연스럽지 않음) | `K_active` 변수 1개 공유 | 좌·우 독립 변수 (`_r`, `_l`) 분리 |
| 전환 시 토크 큰 점프 | LPF 미적용 (α=1.0) | `BLEND_ALPHA` 0.05 ~ 0.1 |
| 입각기에 토크 없음 | `K_active` 초기값이 0 → LPF 가 채워질 때까지 ~20 ms | 정상. 첫 입각 후 정상 동작 |
| 유각기에도 토크 큰 값 | 모드 분기 누락 — 입각/유각 동일 수식 사용 | `if (foot_contact)` 분기 확인 |
| K 가 너무 작아 효과 미미 | 0.2 → 너무 약함 | K 0.4~0.6 권장 |
| K 가 너무 커 강제됨 | 0.8 이상 시 자연 보행 방해 | 0.6 이하 + 평형점 조정 |
| 평형점 이동 시 즉시 토크 점프 | `s_eq_pos_deg` 변경 즉시 적용 | LPF 적용 또는 BTN3 단계 적게 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
