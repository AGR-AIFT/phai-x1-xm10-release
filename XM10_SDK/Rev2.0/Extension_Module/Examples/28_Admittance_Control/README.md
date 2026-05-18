# Ex.28 — Admittance Control (어드미턴스 — 힘 입력 → 위치 출력)

> 🎯 **학습 목표**:
> - **임피던스 vs 어드미턴스 쌍대성** — 임피던스 = 위치→힘, 어드미턴스 = **힘→위치**.
> - 측정 토크 → 가상 M-K-B 동역학 (Forward Euler 적분) → 위치 참조 생성 → 내부 PD 추종.
> - 강성 액추에이터 (stiff actuator) 에서 "유순한" 사용자 따라가기 구현.
>
> ⏱️ 권장 시간: 45분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.20 Impedance Control](../20_Impedance_Control/) (쌍대 비교 필수) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md)
> 📄 논문: Keemink, A. Q. L., et al. (2018). *Admittance control for physical HRI.* Int. J. Robotics Research, 37(11). Hogan, N. (1985).

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

사용자가 다리를 밀면 가상 질량처럼 반응 → 위치가 천천히 이동 → 외력 사라지면 평형점 복귀:

| M_v (가상 관성) | 같은 힘 입력 시 |
|----------------|---------------|
| 0.2 (가벼움) | 빠르게 멀리 이동 |
| 0.5 (default) | 적당히 |
| 2.0 (무거움) | 매우 느린 반응 |

USB CDC `ADM | τext:0.85 θref:5.2 θ:5.1 τcmd:0.06` 매 500 ms.

> 📸 `![어드미턴스 vs 임피던스 블록다이어그램](../assets/img/28_admittance_block.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 임피던스 vs 어드미턴스 — 입출력 방향

```
임피던스 (Ex.20):
  입력 = 위치 편차 (θ_d − θ)
  출력 = 토크 (τ = K·e + B·ė)
  적합: 부드러운 액추에이터 + 위치 제어 부재

어드미턴스 (이 예제):
  입력 = 측정 토크 (τ_ext)
  출력 = 위치 참조 (θ_ref)
  추가: 내부 PD 가 θ_ref 추종 → τ_cmd 송신
  적합: 강성 액추에이터 (위치 제어 강력) + 힘 센서 존재
```

### 가상 동역학 (Forward Euler 적분)

```
M_v · Δθ̈ + B_v · Δθ̇ + K_v · Δθ = τ_ext

이산화 (dt = 1 ms):
  vel[k+1] = vel[k] + dt · (τ_ext − K_v · pos[k] − B_v · vel[k]) / M_v
  pos[k+1] = pos[k] + dt · vel[k+1]

θ_ref = θ_eq + pos
```

### M_v 가 너무 작으면 — 수치 적분 불안정

- Forward Euler 안정 조건: `dt < 2 · M_v / max_eigenvalue`
- M_v < 0.1 일 때 → `MIN_VIRTUAL_MASS` 가드 필수

### 내부 PD 게인

- Kp_pos ≈ 0.3 Nm/deg, Kd_pos ≈ 0.01 Nm·s/deg
- θ_ref 변화율이 느림 (M_v 적분) → 강성 PD 필요 없음

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define M_VIRTUAL_DEFAULT  0.5f                                     // 가상 관성
#define B_VIRTUAL_DEFAULT  2.0f                                     // 가상 감쇠
#define K_VIRTUAL          5.0f                                     // 평형 복원
#define KP_POS             0.3f
#define KD_POS             0.01f
#define MAX_POS_DEV_DEG    20.0f                                    // 편차 한계
#define MIN_VIRTUAL_MASS   0.1f

static float s_pos_dev_deg = 0.0f;                                  // ① 누적 위치 편차
static float s_vel_dev_rad = 0.0f;                                  //    각속도 상태
static float s_M_virtual = M_VIRTUAL_DEFAULT;
static float s_B_virtual = B_VIRTUAL_DEFAULT;
static float s_eq_pos_deg = 0.0f;

static void Active_Loop(void)
{
    /* ② 외력 입력 (측정 토크) */
    float tau_ext = XM.status.h10.rightHipTorque;

    /* ③ 가상 동역학 적분 (Forward Euler) */
    float pos_rad = DEG_TO_RAD(s_pos_dev_deg);
    float vel_dot = (tau_ext - K_VIRTUAL * pos_rad - s_B_virtual * s_vel_dev_rad)
                  / s_M_virtual;

    s_vel_dev_rad += CONTROL_DT * vel_dot;
    s_pos_dev_deg += RAD_TO_DEG(CONTROL_DT * s_vel_dev_rad);

    /* ④ 편차 한계 클램프 (수치 발산 방지) */
    s_pos_dev_deg = _ClampFloat(s_pos_dev_deg, -MAX_POS_DEV_DEG, MAX_POS_DEV_DEG);

    /* ⑤ 위치 참조 생성 */
    float theta_ref_deg = s_eq_pos_deg + s_pos_dev_deg;
    float theta_ref_dot = RAD_TO_DEG(s_vel_dev_rad);

    /* ⑥ 내부 PD 위치 제어기 */
    float theta = XM.status.h10.rightHipAngle;
    float theta_dot = (theta - s_prev_theta) / CONTROL_DT;
    s_prev_theta = theta;

    float tau_cmd = KP_POS * (theta_ref_deg - theta)
                  + KD_POS * (theta_ref_dot - theta_dot);

    tau_cmd = _ClampFloat(tau_cmd, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(tau_cmd);
    XM_SetAssistTorqueLH(tau_cmd);
}

static void _HandleButtons(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {              /* BTN1: M_v 순환 */
        m_idx = (m_idx + 1) % 4;
        s_M_virtual = m_presets[m_idx]; /* 0.2, 0.5, 1.0, 2.0 */
        if (s_M_virtual < MIN_VIRTUAL_MASS) s_M_virtual = MIN_VIRTUAL_MASS;
    }
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {              /* BTN2: B_v */
        b_idx = (b_idx + 1) % 3;
        s_B_virtual = b_presets[b_idx]; /* 1.0, 2.0, 4.0 */
    }
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {              /* BTN3: 리셋 */
        s_pos_dev_deg = 0.0f; s_vel_dev_rad = 0.0f;
    }
}
```

전체 코드: [`admittance_control.c`](admittance_control.c) (415 줄)

> 🧒 ③ Forward Euler 가 핵심. 1 ms 단위로 가상 M-K-B 시스템을 시뮬레이션해서 위치를 만들어냄.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드/플래시 + ASSIST** (Body Data 불필요)
2. **다리를 부드럽게 밀어봄** → ✅ 천천히 위치가 이동 (가상 질량 반응)
3. **외력 제거** → ✅ K_V 복원력으로 평형점으로 천천히 복귀
4. **USB CDC** → `ADM | τext:0.85 θref:5.2 θ:5.0 τcmd:0.06` 매 500 ms
5. **BTN 1 → M_v=0.2 (가벼움)** → ✅ 같은 힘으로 더 빠르고 멀리
6. **BTN 1 → M_v=2.0 (무거움)** → ✅ 매우 느린 반응 (관성 큼)
7. **BTN 2 → B_v=4.0** → ✅ 감쇠 강함, 진동 억제
8. **BTN 3 (리셋)** → ✅ θ_ref 가 현재 위치로 초기화
9. **변형 1 — K_V = 0 (순수 어드미턴스)**: 복원력 없음. 힘 주면 계속 한 방향으로 이동
10. **변형 2 — Impedance (Ex.20) 와 동일 K, B 로 비교**: 두 제어기의 외력 반응 차이
11. **변형 3 — M_v < 0.1 (수치 불안정)**: 안정 한계 체험
12. **변형 4 — Backward Euler**: 대안 적분 방법 (안정성 ↑)
13. **변형 5 — 좌·우 독립**: LH 도 별도 가상 동역학 (4 변수 → 8 변수)

---

## 5️⃣ 다음 단계

- 임피던스 제어 (쌍대): [Ex.20 Impedance Control](../20_Impedance_Control/)
- DOB (외란 추정): [Ex.31 Friction Comp DOB](../31_Friction_Comp_DOB/)
- GRF 기반 의도 감지: [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| pos 가 무한대로 발산 | M_v 가 너무 작음 + 큰 외력 | `MIN_VIRTUAL_MASS` 가드 + 편차 클램프 |
| 외력 줘도 위치 안 움직임 | M_v 너무 큼 (관성 압도) | M_v 0.5 이하로 |
| 외력 제거 후 진동 | B_v 작음 | B_v ↑ 또는 K_V ↓ |
| 평형점 안 돌아옴 | K_V = 0 → 순수 적분기 (의도된 경우는 OK) | K_V > 0 설정 |
| `θ_dot` 추정 노이즈 | 후향 차분 + 1 kHz 노이즈 | LPF 추가 |
| 내부 PD 오버슈트 | Kp_pos 너무 큼 | Kp_pos 0.3 이하 + Kd ↑ |
| Body Data 미설정인데 동작 | 본 예제는 측정 토크만 사용 (Body Data 불필요) | 정상 |
| pos 가 음수로 점점 누적 | sensor 토크 bias | 영점 보정 또는 BTN3 리셋 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
